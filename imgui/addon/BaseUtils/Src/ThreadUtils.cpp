
#if defined(_WIN32) && !defined(__MINGW64__)
#include <windows.h>
#else
#include <pthread.h>
#endif
#include <sstream>
#include <list>
#include <chrono>
#include "ThreadUtils.h"

using namespace std;
using namespace Logger;

namespace SysUtils
{
void SetThreadName(thread& t, const string& name)
{
#if defined(_WIN32) && !defined(__MINGW64__)
    DWORD threadId = ::GetThreadId(static_cast<HANDLE>(t.native_handle()));
    SetThreadName(threadId, name.c_str());
#elif defined(__APPLE__)
    /*
    // Apple pthread_setname_np only set current thread name and 
    // No other API can set thread name from other thread
    if (name.length() > 15)
    {
        string shortName = name.substr(0, 15);
        pthread_setname_np(shortName.c_str());
    }
    else
    {
        pthread_setname_np(name.c_str());
    }
    */
#else
    auto handle = t.native_handle();
    if (name.length() > 15)
    {
        string shortName = name.substr(0, 15);
        pthread_setname_np(handle, shortName.c_str());
    }
    else
    {
        pthread_setname_np(handle, name.c_str());
    }
#endif
}

bool BaseAsyncTask::SetState(State eState, bool bForce)
{
    lock_guard<mutex> _lk(m_mtxLock);
    if (m_eState == eState)
        return true;

    bool bStateTransAllowed = bForce;
    if (!bForce)
    {
        if (m_eState == WAITING)
        {
            // allowed state transition:
            //   1. WAITING -> PROCESSING
            //   2. WAITING -> CANCELLED
            if (eState == PROCESSING || eState == CANCELLED)
                bStateTransAllowed = true;
        }
        else if (m_eState == PROCESSING)
        {
            // allowed state transition:
            //   1. PROCESSING -> DONE
            //   1. PROCESSING -> FAILED
            //   3. PROCESSING -> CANCELLED
            if (eState == DONE || eState == FAILED || eState == CANCELLED)
                bStateTransAllowed = true;
        }
    }

    if (bStateTransAllowed)
    {
        m_eState = eState;
        return true;
    }
    Log(Error) << "FAILED to set task state from " << (int)m_eState << " to " << (int)eState << "." << endl;
    return false;
}

bool BaseAsyncTask::Cancel()
{
    lock_guard<mutex> _lk(m_mtxLock);
    if (m_eState == CANCELLED)
        return true;
    if (m_eState != WAITING && m_eState != PROCESSING)
        return false;
    m_eState = CANCELLED;
    return true;
}

#define THREAD_IDLE_SLEEP_MILLISEC  2

void BaseAsyncTask::WaitDone()
{
    while (!IsStopped())
        this_thread::sleep_for(chrono::milliseconds(THREAD_IDLE_SLEEP_MILLISEC));
}

bool BaseAsyncTask::WaitState(State eState, int64_t u64TimeOut)
{
    if (u64TimeOut > 0)
    {
        const auto startWaitTime = chrono::steady_clock::now();
        while (m_eState != eState)
        {
            const auto elapsedTime = chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now()-startWaitTime).count();
            if (elapsedTime >= u64TimeOut)
                break;
            this_thread::sleep_for(chrono::milliseconds(THREAD_IDLE_SLEEP_MILLISEC));
        }
    }
    else
    {
        while (m_eState != eState)
            this_thread::sleep_for(chrono::milliseconds(THREAD_IDLE_SLEEP_MILLISEC));
    }
    return m_eState == eState;
}

struct ThreadExecutor
{
    using Holder = shared_ptr<ThreadExecutor>;
    virtual bool EnqueueTask(AsyncTask::Holder hTask) = 0;
    virtual void Terminate(bool bWaitAllTaskDone) = 0;
    virtual bool CanAcceptTask() const = 0;
    virtual bool IsIdle() const = 0;
    virtual bool IsTerminated() const = 0;
};

class ThreadExecutorImpl : public ThreadExecutor
{
public:
    static ThreadExecutor::Holder CreateInstance(const string& name);

    ThreadExecutorImpl(const string& name)
    {
        m_pLogger = GetLogger(name);
        m_thExecutorThread = thread(&ThreadExecutorImpl::_ExecutorProc, this);
    }

    bool EnqueueTask(AsyncTask::Holder hTask) override
    {
        if (m_aTasks.size() >= m_iMaxWaitTaskCnt || m_bTerminating)
            return false;
        m_bIdle = false;
        lock_guard<mutex> lk(m_mtxTasksLock);
        m_aTasks.push_back(hTask);
        // cout << "ThreadExecutor: Added task " << hTask.get() << endl;
        return true;
    }

    void Terminate(bool bWaitAllTaskDone) override
    {
        if (m_bTerminating)
            return;
        m_bTerminating = true;
        if (bWaitAllTaskDone)
        {
            while (!m_aTasks.empty())
                this_thread::sleep_for(chrono::milliseconds(THREAD_IDLE_SLEEP_MILLISEC));
        }
        m_bQuit = true;
        if (m_thExecutorThread.joinable())
            m_thExecutorThread.join();
    }

    bool CanAcceptTask() const override
    {
        return m_aTasks.size() < m_iMaxWaitTaskCnt && !m_bTerminating;
    }

    bool IsIdle() const override
    {
        return m_bIdle && !m_bTerminating;
    }

    bool IsTerminated() const override
    {
        return m_bTerminating;
    }

private:
    void _ExecutorProc()
    {
        while (!m_bQuit)
        {
            bool bIdleLoop = true;

            AsyncTask::Holder hTask;
            {
                if (!m_aTasks.empty())
                {
                    lock_guard<mutex> lk(m_mtxTasksLock);
                    hTask = m_aTasks.front();
                    m_aTasks.pop_front();
                }
            }

            if (hTask && hTask->IsWaiting() && hTask->SetState(AsyncTask::PROCESSING))
            {
                // cout << "ThreadExecutor: Processing task " << hTask.get() << endl;
                (*hTask)();
                // cout << "ThreadExecutor: Done task " << hTask.get() << endl;
                if (hTask->IsProcessing())
                {
                    if (!hTask->SetState(AsyncTask::DONE))
                        m_pLogger->Log(WARN) << "FAILED to set task state as 'DONE' after it's been processed." << endl;
                }
                bIdleLoop = false;
            }
            else if (m_aTasks.empty())
            {
                m_bIdle = true;
            }

            if (bIdleLoop)
            {
                // auto nowtp = chrono::system_clock::now();
                // cout << "[" << chrono::duration_cast<chrono::milliseconds>(nowtp.time_since_epoch()).count() << "] I'm idle" << endl;
                this_thread::sleep_for(chrono::milliseconds(THREAD_IDLE_SLEEP_MILLISEC));
            }
        }
    }

private:
    ALogger* m_pLogger;
    string m_strName;
    list<AsyncTask::Holder> m_aTasks;
    mutex m_mtxTasksLock;
    int m_iMaxWaitTaskCnt{1};
    thread m_thExecutorThread;
    bool m_bIdle{true};
    bool m_bQuit{false}, m_bTerminating{false};
};

ThreadExecutor::Holder ThreadExecutorImpl::CreateInstance(const string& name)
{
    return ThreadExecutor::Holder(new ThreadExecutorImpl(name), [] (ThreadExecutor* p) {
        p->Terminate(false);
        ThreadExecutorImpl* ptr = dynamic_cast<ThreadExecutorImpl*>(p);
        delete ptr;
    });
}

class DefaultThreadPoolExecutorImpl : public ThreadPoolExecutor
{
public:
    DefaultThreadPoolExecutorImpl(const string& name)
    {
        m_strName = name.empty() ? "UnnamedThreadPoolExecutor" : name;
        m_pLogger = GetLogger(m_strName);
        m_thExecutorThread = thread(&DefaultThreadPoolExecutorImpl::_ExecutorProc, this);
    }

    ~DefaultThreadPoolExecutorImpl()
    {
        Terminate(false);
    }

    bool EnqueueTask(AsyncTask::Holder hTask, bool bNonblock) override
    {
        if (m_bTerminating || (bNonblock && m_uMaxWaitingTaskCnt > 0 && m_aTasks.size() >= m_uMaxWaitingTaskCnt))
            return false;
        if (m_uMaxWaitingTaskCnt > 0)
        {
            while (!m_bTerminating && m_aTasks.size() >= m_uMaxWaitingTaskCnt)
                this_thread::sleep_for(chrono::milliseconds(THREAD_IDLE_SLEEP_MILLISEC));
            if (m_bTerminating)
                return false;
        }
        {
            lock_guard<mutex> lk(m_mtxTasksLock);
            m_aTasks.push_back(hTask);
            // cout << "DefaultThreadPoolExecutorImpl: Added task " << hTask.get() << endl;
        }
        return true;
    }

    void SetMaxWaitingTaskCount(uint32_t cnt) override
    {
        m_uMaxWaitingTaskCnt = cnt;
    }

    uint32_t GetWaitingTaskCount() const override
    {
        return m_aTasks.size();
    }

    void SetMaxThreadCount(uint32_t cnt) override
    {
        m_uMaxExecutorCnt = cnt;
    }

    uint32_t GetMaxThreadCount() const override
    {
        return m_uMaxExecutorCnt;
    }

    void SetMinThreadCount(uint32_t cnt) override
    {
        m_uMinExecutorCnt = cnt;
    }

    uint32_t GetMinThreadCount() const override
    {
        return m_uMinExecutorCnt;
    }

    void Terminate(bool bWaitAllTaskDone) override
    {
        if (m_bTerminating)
            return;
        m_bTerminating = true;
        if (bWaitAllTaskDone)
        {
            while (!m_aTasks.empty())
                this_thread::sleep_for(chrono::milliseconds(THREAD_IDLE_SLEEP_MILLISEC));
        }
        m_bQuit = true;
        if (m_thExecutorThread.joinable())
            m_thExecutorThread.join();
        for (auto& hExecutor : m_aExecutors)
            hExecutor->Terminate(bWaitAllTaskDone);
        m_aExecutors.clear();
    }

    void SetLoggerLevel(Level l) override
    {
        m_pLogger->SetShowLevels(l);
    }

private:
    void _ExecutorProc()
    {
        AsyncTask::Holder hTask;
        while (!m_bQuit)
        {
            bool bIdleLoop = true;

            if (!hTask)
            {
                if (!m_aTasks.empty())
                {
                    lock_guard<mutex> lk(m_mtxTasksLock);
                    hTask = m_aTasks.front();
                    m_aTasks.pop_front();
                }
            }

            if (hTask && hTask->IsWaiting())
            {
                ThreadExecutor::Holder hCandidate;
                for (auto& hExecutor : m_aExecutors)
                {
                    if (hExecutor->IsIdle())
                    {
                        hCandidate = hExecutor;
                        break;
                    }
                    if (hExecutor->CanAcceptTask())
                    {
                        hCandidate = hExecutor;
                    }
                }
                if (hCandidate)
                {
                    bIdleLoop = false;
                    if (hCandidate->EnqueueTask(hTask))
                    {
                        // cout << "DefaultThreadPoolExecutorImpl: Dispatched task " << hTask.get() << endl;
                        hTask = nullptr;
                    }
                    else
                    {
                        // cout << "WARNING! FAILED to enqueue task into CANDIDATE EXECUTOR." << endl;
                    }
                }
                if (hTask && (m_uMaxExecutorCnt == 0 || (m_uMaxExecutorCnt > m_uMinExecutorCnt && m_aExecutors.size() < m_uMaxExecutorCnt)))
                {
                    ostringstream oss; oss << m_strName << "-" << m_aExecutors.size();
                    auto hNewExecutor = ThreadExecutorImpl::CreateInstance(oss.str());
                    if (hNewExecutor->EnqueueTask(hTask))
                    {
                        hTask = nullptr;
                        m_aExecutors.push_back(hNewExecutor);
                        bIdleLoop = false;
                    }
                    else
                    {
                        m_pLogger->Log(Error) << "Newly created 'ThreadExecutor' instance can NOT enqueue task!" << endl;
                        hNewExecutor->Terminate(false);
                    }
                }
            }
            else
            {
                hTask = nullptr;
            }

            if (m_uMinExecutorCnt > 0 && m_aExecutors.size() < m_uMinExecutorCnt)
            {
                uint32_t uAddCnt = m_uMinExecutorCnt-m_aExecutors.size();
                for (uint32_t i = 0; i < uAddCnt; i++)
                {
                    ostringstream oss; oss << m_strName << "-" << m_aExecutors.size();
                    m_aExecutors.push_back(ThreadExecutorImpl::CreateInstance(oss.str()));
                }
            }
            else if (m_aTasks.empty() && m_uMaxExecutorCnt > 0 && m_uMaxExecutorCnt >= m_uMinExecutorCnt && m_aExecutors.size() > m_uMaxExecutorCnt)
            {
                uint32_t uRemoveCnt = m_aExecutors.size()-m_uMaxExecutorCnt;
                auto itDel = m_aExecutors.begin();
                while (itDel != m_aExecutors.end() && uRemoveCnt > 0)
                {
                    auto& hExecutor = *itDel;
                    if (hExecutor->IsIdle())
                    {
                        hExecutor->Terminate(true);
                        itDel = m_aExecutors.erase(itDel);
                        uRemoveCnt--;
                    }
                    else
                    {
                        itDel++;
                    }
                }
            }

            if (bIdleLoop)
                this_thread::sleep_for(chrono::milliseconds(THREAD_IDLE_SLEEP_MILLISEC));
        }
    }

private:
    ALogger* m_pLogger;
    string m_strName;
    list<ThreadExecutor::Holder> m_aExecutors;
    uint32_t m_uMaxExecutorCnt{0};
    uint32_t m_uMinExecutorCnt{0};
    list<AsyncTask::Holder> m_aTasks;
    mutex m_mtxTasksLock;
    uint32_t m_uMaxWaitingTaskCnt{0};
    thread m_thExecutorThread;
    bool m_bQuit{false}, m_bTerminating{false};
};

ThreadPoolExecutor::Holder _DEFAULT_THREAD_POOL_EXECUTOR_HOLDER;
mutex _DEFAULT_THREAD_POOL_EXECUTOR_HOLDER_LOCK;

ThreadPoolExecutor::Holder ThreadPoolExecutor::GetDefaultInstance()
{
    if (_DEFAULT_THREAD_POOL_EXECUTOR_HOLDER)
        return _DEFAULT_THREAD_POOL_EXECUTOR_HOLDER;
    lock_guard<mutex> lk(_DEFAULT_THREAD_POOL_EXECUTOR_HOLDER_LOCK);
    if (!_DEFAULT_THREAD_POOL_EXECUTOR_HOLDER)
    {
        _DEFAULT_THREAD_POOL_EXECUTOR_HOLDER = ThreadPoolExecutor::Holder(new DefaultThreadPoolExecutorImpl("DefaultThdPlExtor"), [] (ThreadPoolExecutor* p) {
            p->Terminate(false);
            DefaultThreadPoolExecutorImpl* ptr = dynamic_cast<DefaultThreadPoolExecutorImpl*>(p);
            delete ptr;
        });
        _DEFAULT_THREAD_POOL_EXECUTOR_HOLDER->SetMinThreadCount(8);
        _DEFAULT_THREAD_POOL_EXECUTOR_HOLDER->SetMaxThreadCount(12);
        _DEFAULT_THREAD_POOL_EXECUTOR_HOLDER->SetMaxWaitingTaskCount(12);
    }
    return _DEFAULT_THREAD_POOL_EXECUTOR_HOLDER;
}

void ThreadPoolExecutor::ReleaseDefaultInstance()
{
    lock_guard<mutex> lk(_DEFAULT_THREAD_POOL_EXECUTOR_HOLDER_LOCK);
    if (_DEFAULT_THREAD_POOL_EXECUTOR_HOLDER)
        _DEFAULT_THREAD_POOL_EXECUTOR_HOLDER = nullptr;
}

ThreadPoolExecutor::Holder ThreadPoolExecutor::CreateInstance(const string& name)
{
    return ThreadPoolExecutor::Holder(new DefaultThreadPoolExecutorImpl(name), [] (ThreadPoolExecutor* p) {
        p->Terminate(false);
        DefaultThreadPoolExecutorImpl* ptr = dynamic_cast<DefaultThreadPoolExecutorImpl*>(p);
        delete ptr;
    });
}
}

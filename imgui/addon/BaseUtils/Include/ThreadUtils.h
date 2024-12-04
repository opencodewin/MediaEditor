/*
    Copyright (c) 2023-2024 CodeWin

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once
#include <cstdint>
#include <string>
#include <thread>
#include <mutex>
#include <memory>
#include "BaseUtilsCommon.h"
#include "Logger.h"

namespace SysUtils
{
BASEUTILS_API void SetThreadName(std::thread& t, const std::string& name);

struct AsyncTask
{
    using Holder = std::shared_ptr<AsyncTask>;

    enum State
    {
        WAITING = 0,
        PROCESSING = 1,
        DONE = 2,
        FAILED = 3,
        CANCELLED = 4,
    };

    virtual bool operator() () = 0;
    virtual bool SetState(State eState, bool bForce = false) = 0;
    virtual State GetState() const = 0;
    virtual bool IsWaiting() const = 0;
    virtual bool IsProcessing() const = 0;
    virtual bool IsDone() const = 0;
    virtual bool IsFailed() const = 0;
    virtual bool IsCancelled() const = 0;
    virtual bool IsStopped() const = 0;
    virtual bool Cancel() = 0;
    virtual void WaitDone() = 0;
    virtual bool WaitState(State eState, int64_t u64TimeOut = 0) = 0;
};

class BaseAsyncTask : public AsyncTask
{
public:
    bool operator() () override
    {
        m_bRunning = true;
        if (!_BeforeTaskProc())
        {
            SetState(FAILED);
            m_bRunning = false;
            return false;
        }
        if (!_TaskProc())
        {
            SetState(FAILED);
            m_bRunning = false;
            return false;
        }
        if (!_AfterTaskProc())
        {
            SetState(FAILED);
            m_bRunning = false;
            return false;
        }
        SetState(DONE);
        m_bRunning = false;
        return true;
    }

    bool SetState(State eState, bool bForce = false) override;
    bool Cancel() override;
    State GetState() const override
    { return m_eState; }
    bool IsWaiting() const override
    { return m_eState == WAITING; }
    bool IsProcessing() const override
    { return m_eState == PROCESSING; }
    bool IsDone() const override
    { return m_eState == DONE; }
    bool IsFailed() const override
    { return m_eState == FAILED; }
    bool IsCancelled() const override
    { return m_eState == CANCELLED; }
    bool IsStopped() const override
    { return (IsDone() || IsFailed() || IsCancelled()) && !m_bRunning; }
    void WaitDone() override;
    bool WaitState(State eState, int64_t i64TimeOut = 0) override;

protected:
    virtual bool _BeforeTaskProc() { return true; }
    virtual bool _TaskProc() = 0;
    virtual bool _AfterTaskProc() { return true; }

protected:
    std::mutex m_mtxLock;
    State m_eState{WAITING};
    bool m_bRunning{false};
};

struct ThreadPoolExecutor
{
    using Holder = std::shared_ptr<ThreadPoolExecutor>;
    static Holder GetDefaultInstance();
    static void ReleaseDefaultInstance();
    static Holder CreateInstance(const std::string& name);

    virtual bool EnqueueTask(AsyncTask::Holder hTask, bool bNonblock = false) = 0;
    virtual void SetMaxWaitingTaskCount(uint32_t cnt) = 0;
    virtual uint32_t GetWaitingTaskCount() const = 0;
    virtual void SetMaxThreadCount(uint32_t cnt) = 0;
    virtual uint32_t GetMaxThreadCount() const = 0;
    virtual void SetMinThreadCount(uint32_t cnt) = 0;
    virtual uint32_t GetMinThreadCount() const = 0;
    virtual void Terminate(bool bWaitAllTaskDone) = 0;

    virtual void SetLoggerLevel(Logger::Level l) = 0;
};
}

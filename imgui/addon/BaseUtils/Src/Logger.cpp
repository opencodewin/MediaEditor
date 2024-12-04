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

#include <cstdint>
#include <cstdarg>
#include <memory>
#include <iostream>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <chrono>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <atomic>
#include <thread>
#ifdef _WIN32
#include <Windows.h>
#endif
#include "Logger.h"

using namespace std;

#if defined(_WIN32) && !defined(NDEBUG)
// #define USE_WINOWS_ADDITIONAL_LOG_CONSOLE
#endif

#ifdef USE_WINOWS_ADDITIONAL_LOG_CONSOLE
    static atomic_bool _WIN_CONSOLE_CREATED{false};
    static atomic<HANDLE> _WIN_CONSOLE_OUTPUT_HANDLE(INVALID_HANDLE_VALUE);

    static void _InitializeWindowsDebugConsole()
    {
        bool winConsoleCreated = _WIN_CONSOLE_CREATED.exchange(true);
        if (!winConsoleCreated)
        {
            AllocConsole();
            _WIN_CONSOLE_OUTPUT_HANDLE = GetStdHandle(STD_OUTPUT_HANDLE);
        }
    }
#endif

namespace Logger
{
    uint32_t SINGLE_LOG_MAXSIZE = 4096;

    const unordered_map<Level, const string> LEVEL_NAME = {
        { VERBOSE,  "VERBOSE"   },
        { DEBUG,    "DEBUG"     },
        { INFO,     "INFO"      },
        { WARN,     "WARN"      },
        { Error,    "ERROR"     },
    };

    class NullBuffer : public streambuf
    {
    protected:
        int_type overflow(int_type ch) { return ch; }
    };

    NullBuffer NULL_BUFFER;
    ostream NULL_STREAM(&NULL_BUFFER);

    class BaseLogger : public ALogger
    {
    public:
        BaseLogger(const string& name) : m_name(name) {}

        BaseLogger(const BaseLogger&) = delete;
        BaseLogger(BaseLogger&&) = delete;
        BaseLogger& operator=(const BaseLogger&) = delete;

        virtual ~BaseLogger() {}

        ALogger* SetShowLoggerName(bool show) override
        {
            m_showName = show;
            return this;
        }

        ALogger* SetShowLevels(Level l , int n) override
        {
            m_showLevel = l;
            m_N = n;
            return this;
        }

        ALogger* SetShowLevelName(bool show) override
        {
            m_showLevelName = show;
            return this;
        }

        ALogger* SetShowTime(bool show) override
        {
            m_showTime = show;
            return this;
        }

        string GetName() const override
        {
            return m_name;
        }

        Level GetShowLevels(int& n) const override
        {
            n = m_N;
            return m_showLevel;
        }

        virtual bool CheckShow(Level l) const
        {
            if ((m_N > 0 && l < m_showLevel) || (m_N < 0 && l > m_showLevel) || (m_N == 0 && l != m_showLevel))
                return false;
            return true;
        }

        virtual string GetLogPrefix() const
        {
            ostringstream oss;
            bool empty = true;
            if (m_showTime)
            {
                auto now = chrono::system_clock::now();
                time_t t = chrono::system_clock::to_time_t(now);
                int32_t millisec = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()).count()%1000;
                oss << put_time(localtime(&t), "%H:%M:%S") << "." << setfill('0') << setw(3) << millisec << " ";
                empty = false;
            }
            if (m_showLevelName)
            {
                const string& levelName = LEVEL_NAME.at(m_currLevel);
                oss << "[" << levelName << "]";
                empty = false;
            }
            if (m_showName)
            {
                oss << "[" << m_name << "]";
                empty = false;
            }
            if (!empty) oss << " ";
            return oss.str();
        }

        void Log(Level l, const string fmt, ...) override
        {
            if (!CheckShow(l))
                return;

            va_list ap;
            va_start(ap, fmt);
            Log(l, fmt.c_str(), &ap);
            va_end(ap);
        }

        void Log(Level l, const char *fmt, va_list *ap)
        {
            va_list ap2;
            va_copy(ap2, *ap);
            int size = vsnprintf(nullptr, 0, fmt, *ap);
            if (size > SINGLE_LOG_MAXSIZE-1)
                size = SINGLE_LOG_MAXSIZE-1;
            else if (size < 0)
                return;

            unique_ptr<char> buf(new char[size+1]);
            vsnprintf(buf.get(), size+1, fmt, ap2);
            va_end(ap2);

            GetLogStream(l) << buf.get() << endl;
        }

        ostream& Log(Level l) override
        {
            return GetLogStream(l);
        }

    protected:
        virtual ostream& GetLogStream(Level l) = 0;

    protected:
        Level m_showLevel{INFO};
        int m_N{1};
        bool m_showLevelName{true};
        bool m_showTime{true};
        Level m_currLevel{VERBOSE};
        string m_name;
        bool m_showName{false};
    };

    using LoggerHolder = shared_ptr<BaseLogger>;

    class LogBuffer : public stringbuf
    {
    public:
        LogBuffer(BaseLogger* logger, ostream* os, size_t size)
            : m_logger(logger)
            , m_os(os)
        {
            unique_ptr<stringbuf::char_type[]> buffer(new stringbuf::char_type[size]);
            if (buffer)
            {
                setp(buffer.get(), buffer.get()+size);
                m_buffer = std::move(buffer);
            }
        }

        bool empty() const { return pptr() <= pbase(); }

        void SetLogger(BaseLogger* logger)
        {
            m_logger = logger;
        }

        void SetOStream(ostream* os)
        {
            m_os = os;
        }

    protected:
        int sync() override
        {
            int n = stringbuf::sync();
            char* curr = pptr();
            char* begin = pbase();
            if (curr > begin)
            {
                if (m_os)
                {
                    if (m_logger)
                        *m_os << m_logger->GetLogPrefix();
                    m_os->write(begin, curr-begin);
                    if (m_overflowChars > 0)
                        *m_os << " (" << m_overflowChars << " bytes overflowed)" << endl;
                    else
                        m_os->flush();
                }
                seekpos(0);
                m_overflowChars = 0;
            }
            return n;
        }

        int_type overflow(int_type ch) override
        {
            m_overflowChars++;
            return 0;
        }

    protected:
        BaseLogger* m_logger{nullptr};
        ostream* m_os{nullptr};
        unique_ptr<stringbuf::char_type[]> m_buffer;
        uint32_t m_overflowChars{0};
    };

#ifdef USE_WINOWS_ADDITIONAL_LOG_CONSOLE
    class WinLogBuffer : public LogBuffer
    {
    public:
        WinLogBuffer(BaseLogger* logger, ostream* os, size_t size)
            : LogBuffer(logger, os, size)
        {}

    protected:
        int sync() override
        {
            int n = stringbuf::sync();
            char* curr = pptr();
            char* begin = pbase();
            if (curr > begin)
            {
                ostringstream oss;
                if (m_logger)
                    oss << m_logger->GetLogPrefix();
                oss.write(begin, curr-begin);
                if (m_overflowChars > 0)
                    oss << " (" << m_overflowChars << " bytes overflowed)" << endl;
                string logstr = oss.str();
                const void* lpBuffer = logstr.c_str();
                DWORD nNumberOfCharsToWrite, nNumberOfCharsWritten = 0;
                nNumberOfCharsToWrite = logstr.size();
                WriteConsoleA(_WIN_CONSOLE_OUTPUT_HANDLE, lpBuffer, nNumberOfCharsToWrite, &nNumberOfCharsWritten, NULL);
                seekpos(0);
                m_overflowChars = 0;
            }
            return n;
        }
    };
#endif

    class LogStream : public ostream
    {
    public:
        LogStream(LogBuffer* pBuf) : ostream(pBuf), m_pBuf(pBuf)
        {}

        ~LogStream()
        {
            delete m_pBuf;
        }

        LogStream* SetLogger(BaseLogger* logger)
        {
            m_pBuf->SetLogger(logger);
            return this;
        }

        LogStream* SetOStream(ostream* os)
        {
            m_pBuf->SetOStream(os);
            return this;
        }

    private:
        LogBuffer* m_pBuf;
    };

    static unordered_map<thread::id, unique_ptr<LogStream>> _THREAD_LOGSTREAM_TABLE;
    static mutex _THREAD_LOGSTREAM_TABLE_LOCK;

    LogStream& GetThreadLocalLogStream(BaseLogger* logger, ostream* os = nullptr)
    {
        LogStream* pLogStream;
        auto thid = this_thread::get_id();
        auto iter = _THREAD_LOGSTREAM_TABLE.find(thid);
        if (iter == _THREAD_LOGSTREAM_TABLE.end())
        {
            lock_guard<mutex> lk(_THREAD_LOGSTREAM_TABLE_LOCK);
#ifdef USE_WINOWS_ADDITIONAL_LOG_CONSOLE
            LogBuffer* pBuf = new WinLogBuffer(logger, os, SINGLE_LOG_MAXSIZE);
#else
            LogBuffer* pBuf = new LogBuffer(logger, os, SINGLE_LOG_MAXSIZE);
#endif
            pLogStream = new LogStream(pBuf);
            _THREAD_LOGSTREAM_TABLE[thid] = unique_ptr<LogStream>(pLogStream);
        }
        else
        {
            pLogStream = iter->second.get();
            pLogStream->SetLogger(logger);
            pLogStream->SetOStream(os);
        }
        return *pLogStream;
    }

    class StdoutLogger final : public BaseLogger
    {
    public:
        StdoutLogger(const string& name) : BaseLogger(name) {}

    protected:
        ostream& GetLogStream(Level l) override
        {
            if (CheckShow(l))
            {
                m_currLevel = l;
                return GetThreadLocalLogStream(this, &cout);
            }
            else
                return NULL_STREAM;
        }
    };

#ifdef USE_WINOWS_ADDITIONAL_LOG_CONSOLE
    class WinConsoleLogger final : public BaseLogger
    {
    public:
        WinConsoleLogger(const string& name) : BaseLogger(name) {}

    protected:
        ostream& GetLogStream(Level l) override
        {
            if (CheckShow(l))
            {
                m_currLevel = l;
                return GetThreadLocalLogStream(this);
            }
            else
                return NULL_STREAM;
        }
    };
#endif

    void SetSingleLogMaxSize(uint32_t size)
    {
        SINGLE_LOG_MAXSIZE = size;
    }

    static function<BaseLogger*(const string& name)> STDOUT_LOGGER_CREATOR = [](const string& name) { return new StdoutLogger(name); };
#ifdef USE_WINOWS_ADDITIONAL_LOG_CONSOLE
    static function<BaseLogger*(const string& name)> WINCONSOLE_LOGGER_CREATOR = [](const string& name) { return new WinConsoleLogger(name); };
#endif
    static const string DEFAULT_LOGGER_NAME = "<Default>";
    static function<BaseLogger*(const string& name)> DEFAULT_LOGGER_CREATOR =
#ifdef USE_WINOWS_ADDITIONAL_LOG_CONSOLE
        WINCONSOLE_LOGGER_CREATOR
#else
        STDOUT_LOGGER_CREATOR
#endif
    ;

    bool SetDefaultLoggerType(const string& loggerType)
    {
        if (loggerType == "StdoutLogger")
            DEFAULT_LOGGER_CREATOR = STDOUT_LOGGER_CREATOR;
#ifdef USE_WINOWS_ADDITIONAL_LOG_CONSOLE
        else if (loggerType == "WinConsoleLogger")
            DEFAULT_LOGGER_CREATOR = WINCONSOLE_LOGGER_CREATOR;
#endif
        else
            return false;
        return true;
    }

    static unique_ptr<BaseLogger> DEFAULT_LOGGER;
    static mutex DEFAULT_LOGGER_LOCK;

    static BaseLogger* GetDefaultBaseLogger()
    {
#ifdef USE_WINOWS_ADDITIONAL_LOG_CONSOLE
        _InitializeWindowsDebugConsole();
#endif
        lock_guard<mutex> lk(DEFAULT_LOGGER_LOCK);
        if (!DEFAULT_LOGGER)
            DEFAULT_LOGGER = unique_ptr<BaseLogger>(DEFAULT_LOGGER_CREATOR(DEFAULT_LOGGER_NAME));
        return DEFAULT_LOGGER.get();
    }

    ALogger* GetDefaultLogger()
    {
        return GetDefaultBaseLogger();
    }

    void Log(Level l, const string fmt, ...)
    {
        BaseLogger* logger = GetDefaultBaseLogger();
        if (!logger->CheckShow(l))
            return;
        va_list ap;
        va_start(ap, fmt);
        logger->Log(l, fmt.c_str(), &ap);
        va_end(ap);
    }

    ostream& Log(Level l)
    {
        BaseLogger* logger = GetDefaultBaseLogger();
        return logger->Log(l);
    }

    static unordered_map<string, LoggerHolder> _NAMED_LOGGERS;
    static mutex _NAMED_LOGGERS_LOCK;

    ALogger* GetLogger(const string& name)
    {
#ifdef USE_WINOWS_ADDITIONAL_LOG_CONSOLE
        _InitializeWindowsDebugConsole();
#endif
        ALogger* logger;
        auto iter = _NAMED_LOGGERS.find(name);
        if (iter == _NAMED_LOGGERS.end())
        {
            lock_guard<mutex> lk(_NAMED_LOGGERS_LOCK);
            iter = _NAMED_LOGGERS.find(name);
            if (iter == _NAMED_LOGGERS.end())
            {
                LoggerHolder hLogger = LoggerHolder(DEFAULT_LOGGER_CREATOR(name));
                hLogger->SetShowLoggerName(true);
                _NAMED_LOGGERS[name] = hLogger;
                logger = hLogger.get();
            }
            else
                logger = iter->second.get();
        }
        else
            logger = iter->second.get();
        return logger;
    }
}

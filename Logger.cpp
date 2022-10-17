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
#include "Logger.h"

using namespace std;

namespace Logger
{
    uint32_t SINGLE_LOG_MAXSIZE = 4096;

    unordered_map<Level, const string> LEVEL_NAME = {
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
                time_t t = chrono::system_clock::to_time_t(chrono::system_clock::now());
                oss << put_time(localtime(&t), "%F %T");
                empty = false;
            }
            if (m_showName)
            {
                oss << "[" << m_name << "]";
                empty = false;
            }
            if (m_showLevelName)
            {
                oss << "[" << LEVEL_NAME[m_currLevel] << "]";
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

    private:
        BaseLogger* m_logger{nullptr};
        ostream* m_os{nullptr};
        unique_ptr<stringbuf::char_type[]> m_buffer;
        uint32_t m_overflowChars{0};
    };

    class LogStream : public ostream
    {
    public:
        LogStream(BaseLogger* logger, ostream* os, size_t size)
            : m_logBuffer(logger, os, size)
            , ostream(&m_logBuffer)
        {}

        LogStream* SetLogger(BaseLogger* logger)
        {
            m_logBuffer.SetLogger(logger);
            return this;
        }

        LogStream* SetOStream(ostream* os)
        {
            m_logBuffer.SetOStream(os);
            return this;
        }

    private:
        LogBuffer m_logBuffer;
    };

    thread_local unique_ptr<LogStream> THL_LOG_STREAM;

    LogStream& GetThreadLocalLogStream(BaseLogger* logger, ostream* os = nullptr)
    {
        if (!THL_LOG_STREAM)
            THL_LOG_STREAM = unique_ptr<LogStream>(new LogStream(logger, os, SINGLE_LOG_MAXSIZE));
        else
            THL_LOG_STREAM->SetLogger(logger)->SetOStream(os);
        return *THL_LOG_STREAM;
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

    void SetSingleLogMaxSize(uint32_t size)
    {
        SINGLE_LOG_MAXSIZE = size;
    }

    function<BaseLogger*(const string& name)> STDOUT_LOGGER_CREATOR = [](const string& name) { return new StdoutLogger(name); };

    const string DEFAULT_LOGGER_NAME = "<Default>";
    function<BaseLogger*(const string& name)> DEFAULT_LOGGER_CREATOR = STDOUT_LOGGER_CREATOR;

    bool SetDefaultLoggerType(const string& loggerType)
    {
        if (loggerType == "StdoutLogger")
            DEFAULT_LOGGER_CREATOR = STDOUT_LOGGER_CREATOR;
        else
            return false;
        return true;
    }

    unique_ptr<BaseLogger> DEFAULT_LOGGER;
    mutex DEFAULT_LOGGER_LOCK;

    ALogger* GetDefaultLogger()
    {
        lock_guard<mutex> lk(DEFAULT_LOGGER_LOCK);
        if (!DEFAULT_LOGGER)
            DEFAULT_LOGGER = unique_ptr<BaseLogger>(DEFAULT_LOGGER_CREATOR(DEFAULT_LOGGER_NAME));
        return DEFAULT_LOGGER.get();
    }

    static BaseLogger* GetDefaultBaseLoger()
    {
        lock_guard<mutex> lk(DEFAULT_LOGGER_LOCK);
        if (!DEFAULT_LOGGER)
            DEFAULT_LOGGER = unique_ptr<BaseLogger>(DEFAULT_LOGGER_CREATOR(DEFAULT_LOGGER_NAME));
        return DEFAULT_LOGGER.get();
    }

    void Log(Level l, const string fmt, ...)
    {
        BaseLogger* logger = GetDefaultBaseLoger();
        if (!logger->CheckShow(l))
            return;
        va_list ap;
        va_start(ap, fmt);
        logger->Log(l, fmt.c_str(), &ap);
        va_end(ap);
    }

    ostream& Log(Level l)
    {
        BaseLogger* logger = GetDefaultBaseLoger();
        return logger->Log(l);
    }

    unordered_map<string, LoggerHolder> NAMED_LOGGERS;
    mutex NAMED_LOGGERS_LOCK;

    ALogger* GetLogger(const string& name)
    {
        ALogger* logger;
        auto iter = NAMED_LOGGERS.find(name);
        if (iter == NAMED_LOGGERS.end())
        {
            lock_guard<mutex> lk(NAMED_LOGGERS_LOCK);
            iter = NAMED_LOGGERS.find(name);
            if (iter == NAMED_LOGGERS.end())
            {
                LoggerHolder hLogger = LoggerHolder(new StdoutLogger(name));
                hLogger->SetShowLoggerName(true);
                NAMED_LOGGERS[name] = hLogger;
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

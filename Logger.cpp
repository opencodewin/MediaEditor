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
        { ERROR,    "ERROR"     },
    };

    class NullBuffer : public streambuf
    {
    protected:
        int_type overflow(int_type ch) { return ch; }
    };

    NullBuffer NULL_BUFFER;
    ostream NULL_STREAM(&NULL_BUFFER);

    class LogBuffer : public stringbuf
    {
    public:
        LogBuffer(ostream& os) : m_os(os) {}

        bool empty() const { return pptr() <= pbase(); }

        void SetLevelName(const string* name) { m_lvName = name; }

        void SetShowTime(bool show) { m_showTime = show; }

        void SetBufferSize(size_t size)
        {
            unique_ptr<stringbuf::char_type[]> buffer(new stringbuf::char_type[size]);
            if (buffer)
            {
                setp(buffer.get(), buffer.get()+size);
                m_buffer = move(buffer);
            }
        }

    protected:
        int sync() override
        {
            int n = stringbuf::sync();
            char* curr = pptr();
            char* begin = pbase();
            if (curr > begin)
            {
                if (m_showTime)
                {
                    time_t t = chrono::system_clock::to_time_t(chrono::system_clock::now());
                    m_os << put_time(localtime(&t), "%F %T ");
                }
                if (m_lvName)
                    m_os << "[" << *m_lvName << "] ";
                m_os.write(begin, curr-begin);
                if (m_overflowChars > 0)
                    m_os << " (" << m_overflowChars << " bytes overflowed)" << endl;
                else
                    m_os.flush();
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
        unique_ptr<stringbuf::char_type[]> m_buffer;
        ostream& m_os;
        bool m_showTime{true};
        const string* m_lvName;
        uint32_t m_overflowChars{0};
    };

    class LogStream : public ostream
    {
    public:
        LogStream(LogBuffer* buf) : ostream(buf), m_logBuf(buf) {}

        ostream& AtLevel(Level l)
        {
            if (!m_logBuf->empty())
                flush();
            m_currLevel = l;
            if (m_showLevelName)
                m_logBuf->SetLevelName(&LEVEL_NAME[l]);
            return *this;
        }

        void SetShowLevelName(bool show)
        {
            m_showLevelName = show;
            if (!show)
                m_logBuf->SetLevelName(nullptr);
        }

        void SetShowTime(bool show)
        {
            m_logBuf->SetShowTime(show);
        }

    private:
        LogBuffer* m_logBuf;
        Level m_currLevel{VERBOSE};
        bool m_showLevelName{true};
    };

    class BaseLogger : public Logger
    {
    public:
        virtual ~BaseLogger() {}
        BaseLogger() = default;
        BaseLogger(const BaseLogger&) = delete;
        BaseLogger(BaseLogger&&) = delete;
        BaseLogger& operator=(const BaseLogger&) = delete;

        void SetShowLevels(Level l , int n) override
        {
            m_showLevel = l;
            m_N = n;
        }

        void SetShowLevelName(bool show) override
        {
            m_showLevelName = show;
        }

        void SetShowTime(bool show) override
        {
            m_showTime = show;
        }

        virtual bool CheckShow(Level l) const
        {
            if (m_N > 0 && l < m_showLevel || m_N < 0 && l > m_showLevel || m_N == 0 && l != m_showLevel)
                return false;
            return true;
        }

        virtual void Log(Level l, const char *fmt, va_list *ap) = 0;

    protected:
        Level m_showLevel{INFO};
        int m_N{1};
        bool m_showLevelName{true};
        bool m_showTime;
    };

    class StdoutLogger final : public BaseLogger
    {
    public:
        StdoutLogger()
            : m_logBuf(cout)
            , m_logStream(&m_logBuf)
        {
            m_logBuf.SetBufferSize(SINGLE_LOG_MAXSIZE);
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

        ostream& Log(Level l) override
        {
            if (CheckShow(l))
                return m_logStream.AtLevel(l);
            else
                return NULL_STREAM;
        }

        void SetShowLevelName(bool show) override
        {
            BaseLogger::SetShowLevelName(show);
            m_logStream.SetShowLevelName(show);
        }

        void SetShowTime(bool show) override
        {
            BaseLogger::SetShowTime(show);
            m_logStream.SetShowTime(show);
        }

        void Log(Level l, const char *fmt, va_list *ap) override
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

            if (m_showLevelName)
                cout << "[" << LEVEL_NAME[l] << "] ";
            cout << buf.get() << endl;
        }

    private:
        LogBuffer m_logBuf;
        LogStream m_logStream;
    };

    void SetSingleLogMaxSize(uint32_t size)
    {
        SINGLE_LOG_MAXSIZE = size;
    }

    function<BaseLogger*()> DEFAULT_LOGGER_CREATOR = [] { return new StdoutLogger(); };

    bool SetDefaultLoggerType(const string& loggerType)
    {
        if (loggerType == "StdoutLogger")
            DEFAULT_LOGGER_CREATOR = [] { return new StdoutLogger(); };
        else
            return false;
        return true;
    }

    thread_local unique_ptr<BaseLogger> DEFAULT_LOGGER_PTR;
    Level DEFAULT_LOGGER_LEVEL = INFO;
    int DEFAULT_LOGGER_N = 1;

    void SetDefaultLoggerLevels(Level l , int n)
    {
        DEFAULT_LOGGER_LEVEL = l;
        DEFAULT_LOGGER_N = n;
    }

    void Log(Level l, const string fmt, ...)
    {
        if (!DEFAULT_LOGGER_PTR)
        {
            DEFAULT_LOGGER_PTR = unique_ptr<BaseLogger>(DEFAULT_LOGGER_CREATOR());
            DEFAULT_LOGGER_PTR->SetShowLevels(DEFAULT_LOGGER_LEVEL, DEFAULT_LOGGER_N);
        }
        if (!DEFAULT_LOGGER_PTR->CheckShow(l))
            return;
        va_list ap;
        va_start(ap, fmt);
        DEFAULT_LOGGER_PTR->Log(l, fmt.c_str(), &ap);
        va_end(ap);
    }

    std::ostream& Log(Level l)
    {
        if (!DEFAULT_LOGGER_PTR)
        {
            DEFAULT_LOGGER_PTR = unique_ptr<BaseLogger>(DEFAULT_LOGGER_CREATOR());
            DEFAULT_LOGGER_PTR->SetShowLevels(DEFAULT_LOGGER_LEVEL, DEFAULT_LOGGER_N);
        }
        return (static_cast<Logger*>(DEFAULT_LOGGER_PTR.get()))->Log(l);
    }
}
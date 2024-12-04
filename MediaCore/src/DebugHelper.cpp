#include <list>
#include <unordered_map>
#include <iomanip>
#include <thread>
#include <sstream>
#include "DebugHelper.h"

using namespace std;
using namespace Logger;

namespace MediaCore
{
static const TimePoint _FIRST_TP = SysClock::now();

int64_t GetMillisecFromTimePoint(const TimePoint& tp)
{
    return chrono::duration_cast<chrono::milliseconds>(tp-_FIRST_TP).count();
}

struct _CheckPoint
{
    string name;
    TimePoint tp;
};

static list<_CheckPoint> _DEFAULT_CHECK_POINT_LIST;

void AddCheckPoint(const string& name)
{
    _DEFAULT_CHECK_POINT_LIST.push_back({name, SysClock::now()});
}

void LogCheckPointsTimeInfo(ALogger* logger, Logger::Level loglvl)
{
    if (!logger)
        logger = GetDefaultLogger();
    logger->Log(loglvl) << "Check points: ";
    if (!_DEFAULT_CHECK_POINT_LIST.empty())
    {
        list<_CheckPoint>::iterator it1, it2;
        it1 = _DEFAULT_CHECK_POINT_LIST.end();
        it2 = _DEFAULT_CHECK_POINT_LIST.begin();
        do {
            if (it1 != _DEFAULT_CHECK_POINT_LIST.end())
                logger->Log(loglvl) << " -> " << it2->name << "(" << GetMillisecFromTimePoint(it2->tp)
                    << "), d=" << CountElapsedMillisec(it1->tp, it2->tp);
            else
                logger->Log(loglvl) << it2->name << "(" << GetMillisecFromTimePoint(it2->tp) << ")";
            it1 = it2++;
        } while (it2 != _DEFAULT_CHECK_POINT_LIST.end());
        _DEFAULT_CHECK_POINT_LIST.clear();
    }
    else
    {
        logger->Log(loglvl) << "(EMPTY)";
    }
    logger->Log(loglvl) << endl;
}

ostream& operator<<(ostream& os, const TimeSpan& ts)
{
    auto t1 = SysClock::to_time_t(ts.first);
    int32_t ms1 = chrono::duration_cast<chrono::milliseconds>(ts.first.time_since_epoch()).count()%1000;
    auto t2 = SysClock::to_time_t(ts.second);
    int32_t ms2 = chrono::duration_cast<chrono::milliseconds>(ts.second.time_since_epoch()).count()%1000;
    os << put_time(localtime(&t1), "%H:%M:%S") << "." << setfill('0') << setw(3) << ms1 << "~" << put_time(localtime(&t2), "%H:%M:%S") << "." << ms2;
    return os;
}

class PerformanceAnalyzer_Impl : public PerformanceAnalyzer
{
public:
    PerformanceAnalyzer_Impl(const string& name) : m_name(name)
    {}

    void SetLogInterval(uint32_t millisec) override
    {
        m_logInterval = millisec;
    }

    void Start() override
    {
        auto tpNow = SysClock::now();
        if (m_startTp.time_since_epoch().count() == 0)
            m_startTp = tpNow;
        if (m_currTspan.second.first.time_since_epoch().count() == 0)
            m_currTspan.second.first = tpNow;
    }

    TimeSpan End() override
    {
        auto tpNow = SysClock::now();
        EnrollCurrentTimeSpan(tpNow);
        while (!m_tspanStack.empty())
        {
            m_currTspan = m_tspanStack.back();
            m_tspanStack.pop_back();
            EnrollCurrentTimeSpan(tpNow);
        }
        return {m_startTp, tpNow};
    }

    void Reset() override
    {
        m_isInSleep = false;
        m_prevLogTp = TimePoint();
        m_tspanTable.clear();
        m_sleepTspans.clear();
        m_tspanStack.clear();
        auto tpNow = SysClock::now();
        m_startTp = tpNow;
        m_currTspan.first = "";
        m_currTspan.second.first = tpNow;
        m_currTspan.second.second = TimePoint();
    }

    void SectionStart(const string& name) override
    {
        EnrollCurrentTimeSpan(SysClock::now(), name);
    }

    void SectionEnd() override
    {
        EnrollCurrentTimeSpan(SysClock::now());
    }

    void PushAndSectionStart(const string& name) override
    {
        auto tpNow = SysClock::now();
        if (m_isInSleep)
        {
            m_currTspan.second.second = tpNow;
            m_sleepTspans.push_back(m_currTspan.second);
            m_isInSleep = false;
            m_currTspan.second.first = m_currTspan.second.second;
        }
        m_currTspan.second.second = tpNow;
        if (m_currTspan.first.empty())
        {
            if (m_currTspan.second.second > m_currTspan.second.first)
                EnrollCurrentTimeSpan(tpNow, name);
            else
            {
                m_currTspan.first = name;
                m_currTspan.second.first = m_currTspan.second.second = tpNow;
            }
        }
        else
        {
            m_tspanStack.push_back(m_currTspan);
            m_currTspan.first = name;
            m_currTspan.second.first = m_currTspan.second.second = tpNow;
        }
    }

    void PopSection() override
    {
        EnrollCurrentTimeSpan(SysClock::now());
        if (!m_tspanStack.empty())
        {
            m_currTspan = m_tspanStack.back();
            m_tspanStack.pop_back();
        }
    }

    void EnterSleep() override
    {
        if (!m_isInSleep)
        {
            EnrollCurrentTimeSpan(SysClock::now(), m_currTspan.first);
            m_isInSleep = true;
        }
    }

    void QuitSleep() override
    {
        if (m_isInSleep)
        {
            EnrollCurrentTimeSpan(SysClock::now(), m_currTspan.first);
        }
    }

    TimeSpan LogStatisticsOnInterval(Level l, ALogger* logger) override
    {
        auto tpNow = SysClock::now();
        auto tpBegin = tpNow-chrono::duration_cast<SysClock::duration>(chrono::milliseconds(m_logInterval));
        if (tpBegin < m_prevLogTp)
            return TimeSpan();

        TimeSpan tspan = {tpBegin, tpNow};
        LogStatisticsInTimeSpan(tspan, tpNow, l, logger);
        m_prevLogTp = tpNow;
        return tspan;
    }

    TimeSpan LogAndClearStatistics(Level l, ALogger* logger) override
    {
        auto tpNow = SysClock::now();
        auto tpBegin = m_prevLogTp.time_since_epoch().count() == 0 ? m_startTp : m_prevLogTp;
        TimeSpan tspan = {tpBegin, tpNow};
        LogStatisticsInTimeSpan(tspan, tpNow, l, logger);
        m_tspanTable.clear();
        m_sleepTspans.clear();
        m_prevLogTp = tpNow;
        return tspan;
    }

private:
    void EnrollCurrentTimeSpan(const TimePoint& tpNow, const string& name = "")
    {
        m_currTspan.second.second = tpNow;
        if (m_isInSleep)
        {
            m_sleepTspans.push_back(m_currTspan.second);
            m_isInSleep = false;
        }
        else
        {
            auto iter = m_tspanTable.find(m_currTspan.first);
            if (iter != m_tspanTable.end())
                iter->second.push_back(m_currTspan.second);
            else
                m_tspanTable.insert({m_currTspan.first, {m_currTspan.second}});
        }
        m_currTspan.first = name;
        m_currTspan.second.first = m_currTspan.second.second;
    }

    void LogStatisticsInTimeSpan(const TimeSpan& tspan, const TimePoint& tpNow, Level l, ALogger* logger)
    {
        unordered_map<string, SysClock::duration> timeCostTable;
        for (auto& elem : m_tspanTable)
        {
            SysClock::duration t(0);
            auto& spanList = elem.second;
            auto spanIt = spanList.begin();
            while (spanIt != spanList.end())
            {
                if (spanIt->second <= tspan.first)
                {
                    spanIt = spanList.erase(spanIt);
                }
                else
                {
                    if (spanIt->first < tspan.first)
                        t += spanIt->second-tspan.first;
                    else
                        t += spanIt->second-spanIt->first;
                    spanIt++;
                }
            }
            auto iter = timeCostTable.find(elem.first);
            if (iter != timeCostTable.end())
                iter->second += t;
            else
                timeCostTable[elem.first] = t;
        }
        if (!m_sleepTspans.empty())
        {
            SysClock::duration t(0);
            auto spanIt = m_sleepTspans.begin();
            while (spanIt != m_sleepTspans.end())
            {
                if (spanIt->second <= tspan.first)
                {
                    spanIt = m_sleepTspans.erase(spanIt);
                }
                else
                {
                    if (spanIt->first < tspan.first)
                        t += spanIt->second-tspan.first;
                    else
                        t += spanIt->second-spanIt->first;
                    spanIt++;
                }
            }
            timeCostTable[SLEEP_TIMESPAN_TAG] = t;
        }
        // add time-cost of 'm_currTspan'
        {
            auto t = tspan.second-(m_currTspan.second.first > tspan.first ? m_currTspan.second.first : tspan.first);
            if (t.count() > 0)
            {
                auto iter = timeCostTable.find(m_currTspan.first);
                if (iter != timeCostTable.end())
                    iter->second += t;
                else
                    timeCostTable[m_currTspan.first] = t;
            }
        }

        if (!logger)
            logger = GetDefaultLogger();

        logger->Log(l) << "PerformanceAnalyzer['" << m_name << "' " << tspan << "] : ";
        if (timeCostTable.empty())
            logger->Log(l) << "<EMPTY>";
        else
        {
            auto iter = timeCostTable.begin();
            auto otherIter = timeCostTable.end();
            auto sleepIter = timeCostTable.end();
            int i = 0;
            while (iter != timeCostTable.end())
            {
                if (iter->first.empty())
                {
                    otherIter = iter++;
                    continue;
                }
                else if (iter->first == SLEEP_TIMESPAN_TAG)
                {
                    sleepIter = iter++;
                    continue;
                }

                if (i++ > 0)
                    logger->Log(l) << ", ";
                logger->Log(l) << "'" << iter->first << "'=>" << chrono::duration_cast<chrono::duration<double>>(iter->second).count();
                iter++;
            }
            if (otherIter != timeCostTable.end())
            {
                if (i > 0)
                    logger->Log(l) << ", ";
                logger->Log(l) << "'" << OTHER_TIMESPAN_TAG << "'=>" << chrono::duration_cast<chrono::duration<double>>(otherIter->second).count();
            }
            if (sleepIter != timeCostTable.end())
            {
                if (i > 0)
                    logger->Log(l) << ", ";
                logger->Log(l) << "'" << SLEEP_TIMESPAN_TAG << "'=>" << chrono::duration_cast<chrono::duration<double>>(sleepIter->second).count();
            }
        }
        logger->Log(l) << endl;
    }

    static const string OTHER_TIMESPAN_TAG;
    static const string SLEEP_TIMESPAN_TAG;

private:
    string m_name;
    unordered_map<string, list<TimeSpan>> m_tspanTable;
    list<TimeSpan> m_sleepTspans;
    pair<string, TimeSpan> m_currTspan;
    list<pair<string, TimeSpan>> m_tspanStack;
    bool m_isInSleep{false};
    uint32_t m_logInterval{1000};
    TimePoint m_startTp, m_prevLogTp;
};

const string PerformanceAnalyzer_Impl::OTHER_TIMESPAN_TAG = "<other>";
const string PerformanceAnalyzer_Impl::SLEEP_TIMESPAN_TAG = "<sleep>";

PerformanceAnalyzer::Holder PerformanceAnalyzer::CreateInstance(const string& name)
{
    return PerformanceAnalyzer::Holder(new PerformanceAnalyzer_Impl(name), [] (PerformanceAnalyzer* p) {
        PerformanceAnalyzer_Impl* ptr = dynamic_cast<PerformanceAnalyzer_Impl*>(p);
        delete ptr;
    });
}

static unordered_map<thread::id, PerformanceAnalyzer::Holder> _THREAD_PERFORMANCE_ANALYZER_TABLE;
static mutex _THREAD_PERFORMANCE_ANALYZER_TABLE_LOCK;

PerformanceAnalyzer::Holder PerformanceAnalyzer::GetThreadLocalInstance()
{
    PerformanceAnalyzer::Holder hPa;
    auto thid = this_thread::get_id();
    auto iter = _THREAD_PERFORMANCE_ANALYZER_TABLE.find(thid);
    if (iter == _THREAD_PERFORMANCE_ANALYZER_TABLE.end())
    {
        ostringstream oss;
        oss << "th#" << thid;
        hPa = PerformanceAnalyzer::Holder(new PerformanceAnalyzer_Impl(oss.str()), [] (PerformanceAnalyzer* p) {
            PerformanceAnalyzer_Impl* ptr = dynamic_cast<PerformanceAnalyzer_Impl*>(p);
            delete ptr;
        });
        lock_guard<mutex> lk(_THREAD_PERFORMANCE_ANALYZER_TABLE_LOCK);
        _THREAD_PERFORMANCE_ANALYZER_TABLE[thid] = hPa;
    }
    else
    {
        hPa = _THREAD_PERFORMANCE_ANALYZER_TABLE[thid];
    }
    return hPa;
}

AutoSection::AutoSection(const string& name, PerformanceAnalyzer::Holder hPa)
{
    if (!hPa)
        hPa = PerformanceAnalyzer::GetThreadLocalInstance();
    m_hPa = hPa;
    m_hPa->PushAndSectionStart(name);
}

AutoSection::~AutoSection()
{
    m_hPa->PopSection();
}
}
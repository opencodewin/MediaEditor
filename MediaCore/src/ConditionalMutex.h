#pragma once
#include <mutex>

namespace MediaCore
{
class ConditionalMutex
{
public:
    ConditionalMutex() : m_isOn(true) {}
    ConditionalMutex(bool isOn) : m_isOn(isOn) {}

    void lock() { if (m_isOn) m_mutex.lock(); }
    bool try_lock() { if (m_isOn) return m_mutex.try_lock(); else return true; }
    void unlock() { if (m_isOn) m_mutex.unlock(); }

    bool IsOn() const { return m_isOn; }
    void TurnOn() { m_isOn = true; }
    void TurnOff()
    {
        if (m_isOn)
        {
            // reset mutex state
            m_mutex.try_lock();
            m_mutex.unlock();
            m_isOn = false;
        }
    }

private:
    std::recursive_mutex m_mutex;
    bool m_isOn;
};
}
#ifndef __LOG_H_
#define __LOG_H_

namespace Log
{
    // log
    void Info(const char* fmt, ...);
    void Notify(const char* fmt, ...);
    void Debug(const char* fmt, ...);
    void Warning(const char* fmt, ...);
    void Error(const char* fmt, ...);

    void AddMessage(const int type, const char* fmt, ...);
    // Draw logs
    void ShowLogWindow(bool* p_open = nullptr);

    void Render(bool showWarnings = false, bool show_notifies = false, bool show_errors = true);
}

#endif // __LOG_H_

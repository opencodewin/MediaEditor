#pragma once
#include <cstdint>
#include <string>
#include <memory>
#include <mutex>
#include <list>
#include <vector>
#include <imgui_json.h>
#include <Logger.h>
#include "BackgroundTask.h"

namespace MEC
{
class Project
{
public:
    using Holder = std::shared_ptr<Project>;
    static Holder CreateInstance();
    static std::string GetDefaultProjectBaseDir();

    static uint8_t VER_MAJOR;
    static uint8_t VER_MINOR;

    enum ErrorCode
    {
        OK = 0,
        FAILED,
        PARSE_FAILED,
        FILE_INVALID,
        NOT_OPENED,
        ALREADY_EXISTS,
        MKDIR_FAILED,
        TL_INVALID,
    };

    ErrorCode CreateNew(const std::string& name, const std::string& baseDir);
    ErrorCode Load(const std::string& projFilePath);
    ErrorCode Save();
    ErrorCode Close(bool bSaveBeforeClose = true);
    std::string GetProjectName() const { return m_projName; }
    std::string GetProjectDir() const { return m_projDir; }
    std::string GetProjectFilePath() const { return m_projFilePath; }
    uint32_t GetProjectVersion() const { return m_projVer; }
    uint8_t GetProjectMajorVersion() const { return (uint8_t)(m_projVer>>24); }
    uint8_t GetProjectMinorVersion() const { return (uint8_t)((m_projVer>>16)&0xff); }
    bool IsOpened() const { return m_bOpened; }
    void SetContentJson(const imgui_json::value& jnProjContent) { m_jnProjContent = jnProjContent; }
    const imgui_json::value& GetProjectContentJson() const { return m_jnProjContent; }
    const std::list<BackgroundTask::Holder> GetBackgroundTaskList() const { return m_aBgtasks; }

    void SetLogLevel(Logger::Level l) { m_pLogger->SetShowLevels(l); }

protected:
    Project() {}

private:
    Logger::ALogger* m_pLogger;
    bool m_bOpened{false};
    std::string m_projName;
    std::string m_projDir;
    std::string m_projFilePath;
    uint32_t m_projVer{0};
    imgui_json::value m_jnProjContent;
    std::recursive_mutex m_mtxApiLock;
    std::list<BackgroundTask::Holder> m_aBgtasks;
};
}
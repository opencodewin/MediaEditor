#pragma once
#include <cstdint>
#include <string>
#include <memory>
#include <mutex>
#include <list>
#include <vector>
#include <imgui_json.h>
#include <ThreadUtils.h>
#include <Logger.h>
#include <MediaCore/HwaccelManager.h>
#include "BackgroundTask.h"

namespace MEC
{
class Project : public BackgroundTask::Callbacks
{
public:
    enum ErrorCode
    {
        OK = 0,
        FAILED,
        INVALID_ARG,
        PARSE_FAILED,
        FILE_INVALID,
        NOT_OPENED,
        NOT_READY,
        ALREADY_EXISTS,
        MKDIR_FAILED,
        IO_ERROR,
    };

    using Holder = std::shared_ptr<Project>;
    static Holder CreateNewProject(ErrorCode& ec, const std::string& projName, const std::string& projDir, const std::string& mepFileName, bool overwrite = false);
    static Holder CreateNewProject(ErrorCode& ec, const std::string& projName, const std::string& projDir, bool overwrite = false);
    static Holder CreateNewProjectAtDir(ErrorCode& ec, const std::string& projDir, bool overwrite = false);
    static Holder CreateNewProjectInBaseDir(ErrorCode& ec, const std::string& projName, const std::string& baseDir, bool overwrite = false);
    static Holder CreateUntitledProject(ErrorCode& ec);
    static Holder OpenProjectDir(ErrorCode& ec, const std::string& projDir);
    static Holder OpenProjectFile(ErrorCode& ec, const std::string& mepFilePath);

    static const uint8_t VER_MAJOR;
    static const uint8_t VER_MINOR;
    static const std::string UNTITLED_PROJECT_NAME;

    static Logger::ALogger* GetDefaultLogger();
    static std::string s_PROJ_FILE_EXT;
    static std::string GetDefaultProjectBaseDir();
    static std::string GetCacheDir();
    static ErrorCode SetCacheDir(const std::string& path);

    ErrorCode Move(const std::string& newProjDir, bool overwrite = false);
    ErrorCode Load(const std::string& mepFilePath);
    ErrorCode Save();
    ErrorCode SaveAs(const std::string& newProjName, const std::string& newProjDir, bool overwrite = false);
    ErrorCode SaveTo(const std::string& projFilePath);
    ErrorCode Close(bool bSaveBeforeClose = true);
    ErrorCode Delete();
    void SetBgtaskExecutor(SysUtils::ThreadPoolExecutor::Holder hBgtaskExctor);
    std::string GetProjectName() const { return m_projName; }
    ErrorCode ChangeProjectName(const std::string& newName);
    std::string GetProjectDir() const { return m_projDir; }
    std::string GetProjectFilePath() const { return m_projFilePath; }
    uint32_t GetProjectVersion() const { return m_projVer; }
    uint8_t GetProjectMajorVersion() const { return (uint8_t)(m_projVer>>24); }
    uint8_t GetProjectMinorVersion() const { return (uint8_t)((m_projVer>>16)&0xff); }
    bool IsOpened() const { return m_bOpened; }
    bool IsUntitled() const { return m_bUntitled; }
    void SetContentJson(const imgui_json::value& jnProjContent) { m_jnProjContent = jnProjContent; }
    const imgui_json::value& GetProjectContentJson() const { return m_jnProjContent; }
    Project::ErrorCode EnqueueBackgroundTask(BackgroundTask::Holder hTask);
    std::list<BackgroundTask::Holder> GetBackgroundTaskList();
    Project::ErrorCode RemoveBackgroundTask(BackgroundTask::Holder hTask, bool bRemoveTaskDir = true);
    void SetHwaccelManager(MediaCore::HwaccelManager::Holder hHwMgr) { m_hHwMgr = hHwMgr; }
    MediaCore::HwaccelManager::Holder GetHwaccelManager() const { return m_hHwMgr; }

    bool OnAddMediaItem(MediaCore::MediaParser::Holder hParser) override;
    bool OnCheckMediaItemImported(const std::string& strPath) override;
    bool OnOutputMediaItemMetaData(const std::string& fileUrl, const std::string& metaName, const imgui_json::value& metaValue) override;
    const imgui_json::value& OnCheckMediaItemMetaData(const std::string& fileUrl, const std::string& metaName) override;

    void SetTimelineHandle(void* pHandle) { m_pTlHandle = pHandle; }
    void SetLogLevel(Logger::Level l) { m_pLogger->SetShowLevels(l); }

protected:
    Project() : m_bUntitled(true) {}
    Project(const std::string& projName, const std::string& projDir, const std::string& mepFileName);
    void SetUntitled() { m_bUntitled = true; }

    static std::string s_CACHEDIR;
    static std::string TryCacheDirPath(const std::string& strParentDir, const std::string& strCacheDirName);

private:
    Logger::ALogger* m_pLogger;
    bool m_bOpened{false};
    bool m_bUntitled{false};
    std::string m_projName;
    std::string m_projDir;
    std::string m_projFilePath;
    uint32_t m_projVer{0};
    imgui_json::value m_jnProjContent;
    std::recursive_mutex m_mtxApiLock;
    std::list<BackgroundTask::Holder> m_aBgtasks;
    std::mutex m_mtxBgtaskLock;
    SysUtils::ThreadPoolExecutor::Holder m_hBgtaskExctor;
    MediaCore::HwaccelManager::Holder m_hHwMgr;

    // this ugly reference to the TimeLine instance should be removed after global TimeLine pointer is opted out
    void* m_pTlHandle{nullptr};
};
}
#include <sstream>
#include "imgui_helper.h"
#include "FileSystemUtils.h"
#include "MecProject.h"

using namespace std;
using namespace Logger;

namespace MEC
{
string Project::GetDefaultProjectBaseDir()
{
    auto userVideoDir = ImGuiHelper::getVideoFolder();
    return SysUtils::JoinPath(userVideoDir, "MecProject");
}

Project::Project(SysUtils::ThreadPoolExecutor::Holder hBgtaskExctor)
    : m_hBgtaskExctor(hBgtaskExctor)
{
    m_pLogger = GetLogger("MecProject");
}

Project::ErrorCode Project::CreateNew(const string& name, const string& baseDir)
{
    lock_guard<recursive_mutex> _lk(m_mtxApiLock);
    if (m_bOpened)
    {
        const auto errcode = Save();
        if (errcode != OK)
        {
            m_pLogger->Log(Error) << "FAILED to save current project '" << m_projName << "' before creating new project!" << endl;
            return errcode;
        }
    }
    auto projDir = SysUtils::JoinPath(baseDir, name);
    if (SysUtils::Exists(projDir))
    {
        m_pLogger->Log(Error) << "Project directory path '" << projDir << "' already exists! Can NOT create new project at this location." << endl;
        return ALREADY_EXISTS;
    }
    if (!SysUtils::CreateDirectory(projDir, true))
    {
        m_pLogger->Log(Error) << "FAILED to create project directory at '" << projDir << "'!" << endl;
        return MKDIR_FAILED;
    }
    m_projName = name;
    m_projDir = projDir;
    m_projFilePath = SysUtils::JoinPath(m_projDir, m_projName+".mep");
    m_projVer = (VER_MAJOR<<24) | (VER_MINOR<<16);
    m_bOpened = true;
    return OK;
}

Project::ErrorCode Project::Load(const std::string& projFilePath)
{
    lock_guard<recursive_mutex> _lk(m_mtxApiLock);
    if (m_bOpened)
    {
        const auto errcode = Save();
        if (errcode != OK)
        {
            m_pLogger->Log(Error) << "FAILED to save current project '" << m_projName << "' before loading another project!" << endl;
            return errcode;
        }
    }
    if (!SysUtils::IsFile(projFilePath))
    {
        m_pLogger->Log(Error) << "FAILED to load project from '" << projFilePath << "'! Target is NOT a file." << endl;
        return FILE_INVALID;
    }
    auto res = imgui_json::value::load(projFilePath);
    if (!res.second)
    {
        m_pLogger->Log(Error) << "FAILED to parse project json from '" << projFilePath << "'!" << endl;
        return PARSE_FAILED;
    }
    const auto& jnProj = res.first;
    string attrName = "mec_proj_version";
    if (jnProj.contains(attrName) && jnProj[attrName].is_number())
    {
        m_projVer = (uint32_t)jnProj[attrName].get<imgui_json::number>();
        m_jnProjContent = std::move(jnProj["proj_content"]);
        m_projName = jnProj["proj_name"].get<imgui_json::string>();
        m_projDir = SysUtils::ExtractDirectoryPath(projFilePath);
        attrName = "bg_tasks";
        if (jnProj.contains(attrName) && jnProj[attrName].is_array())
        {
            auto hDummySettings = MediaCore::SharedSettings::CreateInstance();
            hDummySettings->SetHwaccelManager(m_hHwMgr);
            const auto& aTaskSavePaths = jnProj[attrName].get<imgui_json::array>();
            for (const auto& jnItem : aTaskSavePaths)
            {
                if (jnItem.is_string())
                {
                    const string strTaskJsonPath = jnItem.get<imgui_json::string>();
                    const auto tLoadRes = imgui_json::value::load(jnItem.get<imgui_json::string>());
                    if (tLoadRes.second)
                    {
                        auto hTask = BackgroundTask::CreateBackgroundTask(tLoadRes.first, hDummySettings);
                        if (hTask)
                        {
                            hTask->Pause();
                            if (m_hBgtaskExctor)
                                m_hBgtaskExctor->EnqueueTask(hTask);
                            m_aBgtasks.push_back(hTask);
                        }
                        else
                            m_pLogger->Log(Error) << "FAILED to parse background task from json '" << strTaskJsonPath << "'!" << endl;
                    }
                    else
                    {
                        m_pLogger->Log(WARN) << "FAILED to load background task json from '" << strTaskJsonPath << "'." << endl;
                    }
                }
                else
                {
                    m_pLogger->Log(WARN) << "INVALID 'bg_tasks' item in project '" << projFilePath << "'! Non-string item is found." << endl;
                }
            }
        }
    }
    else
    {
        m_jnProjContent = std::move(jnProj);
        m_projName = SysUtils::ExtractFileBaseName(projFilePath);
    }
    m_projFilePath = projFilePath;
    m_bOpened = true;
    return OK;
}

Project::ErrorCode Project::Save()
{
    lock_guard<recursive_mutex> _lk(m_mtxApiLock);
    if (!m_bOpened)
        return NOT_OPENED;
    if (!m_jnProjContent.is_object())
        return TL_INVALID;
    imgui_json::value jnProj;
    jnProj["mec_proj_version"] = imgui_json::number(m_projVer);
    jnProj["proj_name"] = imgui_json::string(m_projName);
    jnProj["proj_content"] = m_jnProjContent;
    imgui_json::array aTaskSavePaths;
    for (auto& hTask : m_aBgtasks)
    {
        const auto strTaskSavePath = hTask->Save();
        aTaskSavePaths.push_back(strTaskSavePath);
    }
    jnProj["bg_tasks"] = aTaskSavePaths;
    if (!jnProj.save(m_projFilePath))
    {
        m_pLogger->Log(Error) << "FAILED to save project json file at '" << m_projFilePath << "'!" << endl;
        return FAILED;
    }
    return OK;
}

Project::ErrorCode Project::Close(bool bSaveBeforeClose)
{
    lock_guard<recursive_mutex> _lk(m_mtxApiLock);
    if (!m_bOpened)
        return OK;
    list<BackgroundTask::Holder> aBgtaskList;
    {
        lock_guard<mutex> _lk2(m_mtxBgtaskLock);
        aBgtaskList = m_aBgtasks;
        m_aBgtasks.clear();
    }
    for (auto& hTask : aBgtaskList)
        hTask->Cancel();
    if (bSaveBeforeClose)
    {
        const auto errcode = Save();
        if (errcode != OK)
        {
            m_pLogger->Log(Error) << "FAILED to save current project '" << m_projName << "' before closing the project!" << endl;
            return errcode;
        }
    }
    m_jnProjContent = nullptr;
    m_projDir.clear();
    m_projName.clear();
    m_projFilePath.clear();
    m_projVer = 0;
    m_bOpened = false;
    return OK;
}

Project::ErrorCode Project::EnqueueBackgroundTask(BackgroundTask::Holder hTask)
{
    lock_guard<recursive_mutex> _lk(m_mtxApiLock);
    if (!m_bOpened)
        return NOT_OPENED;
    if (!m_hBgtaskExctor)
    {
        m_pLogger->Log(Error) << "Current MEC::Project instance has NOT been set with background task executor!" << endl;
        return NOT_READY;
    }
    if (!m_hBgtaskExctor->EnqueueTask(hTask))
    {
        m_pLogger->Log(Error) << "Enqueue background task FAILED!" << endl;
        return FAILED;
    }
    lock_guard<mutex> _lk2(m_mtxBgtaskLock);
    m_aBgtasks.push_back(hTask);
    return OK;
}

list<BackgroundTask::Holder> Project::GetBackgroundTaskList()
{
    lock_guard<mutex> _lk(m_mtxBgtaskLock);
    return list<BackgroundTask::Holder>(m_aBgtasks);
}

Project::ErrorCode Project::RemoveBackgroundTask(BackgroundTask::Holder hTask, bool bRemoveTaskDir)
{
    lock_guard<recursive_mutex> _lk(m_mtxApiLock);
    if (!m_bOpened)
        return NOT_OPENED;
    lock_guard<mutex> _lk2(m_mtxBgtaskLock);
    auto itRem = find(m_aBgtasks.begin(), m_aBgtasks.end(), hTask);
    if (itRem == m_aBgtasks.end())
        return INVALID_ARG;
    m_aBgtasks.erase(itRem);
    if (bRemoveTaskDir)
    {
        const auto strTaskDir = hTask->GetTaskDir();
        if (!strTaskDir.empty())
            SysUtils::DeleteDirectory(strTaskDir);
    }
    return OK;
}

uint8_t Project::VER_MAJOR = 1;
uint8_t Project::VER_MINOR = 0;

Project::Holder Project::CreateInstance(SysUtils::ThreadPoolExecutor::Holder hBgtaskExctor)
{
    return Project::Holder(new Project(hBgtaskExctor));
}
}
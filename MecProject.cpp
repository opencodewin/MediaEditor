#include <sstream>
#include "imgui_helper.h"
#include "FileSystemUtils.h"
#include "MecProject.h"
#include "MediaTimeline.h"

using namespace std;
using namespace Logger;

namespace MEC
{
ALogger* Project::GetDefaultLogger()
{
    return GetLogger("MecProject");
}

string Project::GetDefaultProjectBaseDir()
{
    auto userVideoDir = ImGuiHelper::getVideoFolder();
    return SysUtils::JoinPath(userVideoDir, "MecProjects");
}

Project::Holder Project::CreateNewProject(ErrorCode& ec, const string& _projName, const string& projDir, const string& mepFileName, bool overwrite)
{
    ec = OK;
    auto pLogger = GetDefaultLogger();
    auto projName(_projName);
    if (projName.empty())
    {
        auto tmp = SysUtils::IsPathSeparator(projDir.back()) ? projDir.substr(0, projDir.length()-1) : projDir;
        projName = SysUtils::ExtractFileName(tmp);
    }
    if (projName.empty())
    {
        pLogger->Log(Error) << "FAILED to create new project! 'projName' is empty." << endl;
        ec = INVALID_ARG;
        return nullptr;
    }
    if (projDir.empty())
    {
        pLogger->Log(Error) << "FAILED to create new project! 'projDir' is empty." << endl;
        ec = INVALID_ARG;
        return nullptr;
    }
    if (SysUtils::Exists(projDir))
    {
        if (overwrite && SysUtils::IsDirectory(projDir))
        {
            if (!SysUtils::DeleteDirectoryAt(projDir))
            {
                pLogger->Log(Error) << "FAILED to create new project at '" << projDir << "', CANNOT OVERWRITE the existing directory by deleting it!" << endl;
                ec = IO_ERROR;
                return nullptr;
            }
        }
        else
        {
            pLogger->Log(Error) << "FAILED to create new project at '" << projDir << "', this location is ALREADY OCCUPIED!" << endl;
            ec = ALREADY_EXISTS;
            return nullptr;
        }
    }
    if (!SysUtils::CreateDirectoryAt(projDir, true))
    {
        pLogger->Log(Error) << "FAILED to create new project at '" << projDir << "', CANNOT create the project directory!" << endl;
        ec = MKDIR_FAILED;
        return nullptr;
    }
    return Holder(new Project(projName, projDir, mepFileName.empty() ? projName : mepFileName));
}

Project::Holder Project::CreateNewProject(ErrorCode& ec, const string& projName, const string& projDir, bool overwrite)
{
    return CreateNewProject(ec, projName, projDir, projName, overwrite);
}

Project::Holder Project::CreateNewProjectAtDir(ErrorCode& ec, const string& projDir, bool overwrite)
{
    return CreateNewProject(ec, "", projDir, "", overwrite);
}

Project::Holder Project::CreateNewProjectInBaseDir(ErrorCode& ec, const string& projName, const string& baseDir, bool overwrite)
{
    return CreateNewProject(ec, projName, SysUtils::JoinPath(baseDir, projName), "", overwrite);
}

Project::Holder Project::CreateUntitledProject(ErrorCode& ec)
{
    auto pLogger = GetDefaultLogger();
    const auto strCacheDirPath = GetCacheDir();
    if (strCacheDirPath.empty())
    {
        pLogger->Log(Error) << "FAILED to find a suitable location to store untitled project, NO CACHE DIR available!" << endl;
        ec = FILE_INVALID;
        return nullptr;
    }
    auto hProj = CreateNewProjectInBaseDir(ec, UNTITLED_PROJECT_NAME, strCacheDirPath, true);
    if (ec == OK)
    {
        hProj->SetUntitled();
        hProj->Save();
    }
    return hProj;
}

Project::Holder Project::OpenProjectDir(ErrorCode& ec, const string& projDir)
{
    auto pLogger = GetDefaultLogger();
    if (projDir.empty())
    {
        pLogger->Log(Error) << "FAILED to load mec project! 'projDir' is empty." << endl;
        ec = INVALID_ARG;
        return nullptr;
    }
    if (!SysUtils::IsDirectory(projDir))
    {
        pLogger->Log(Error) << "FAILED to load mec project from directory '" << projDir << "', the path is INVALID!" << endl;
        ec = FILE_INVALID;
        return nullptr;
    }
    string projName;
    {
        const auto tmp = SysUtils::IsPathSeparator(projDir.back()) ? projDir.substr(0, projDir.length()-1) : projDir;
        projName = SysUtils::ExtractFileName(tmp);
    }
    Project::Holder hProj;
    // 1. try to open the project file that has the same name as the project directory
    if (!projName.empty())
    {
        const auto projFilePath = SysUtils::JoinPath(projDir, projName+s_PROJ_FILE_EXT);
        if ((hProj = OpenProjectFile(ec, projFilePath)) != nullptr)
            return hProj;
    }
    // 2. try to find another project file inside the given project directory
    auto hFileIter = SysUtils::FileIterator::CreateInstance(projDir);
    hFileIter->SetCaseSensitive(false);
    hFileIter->SetFilterPattern(".+\\"+s_PROJ_FILE_EXT, true);
    hFileIter->StartParsing();
    const auto aProjFiles = hFileIter->GetAllFilePaths();
    for (const auto& projFilePath : aProjFiles)
    {
        if ((hProj = OpenProjectFile(ec, projFilePath)) != nullptr)
            return hProj;
    }

    // failed to find a valid project file
    pLogger->Log(Error) << "FAILED to open mec project at directory '" << projDir << "'! No project file can be found." << endl;
    ec = NOT_READY;
    return nullptr;
}

Project::Holder Project::OpenProjectFile(ErrorCode& ec, const string& projFilePath)
{
    auto pLogger = GetDefaultLogger();
    if (projFilePath.empty())
    {
        pLogger->Log(Error) << "FAILED to load mec project! 'projFilePath' is empty." << endl;
        ec = INVALID_ARG;
        return nullptr;
    }
    if (!SysUtils::IsFile(projFilePath))
    {
        pLogger->Log(Error) << "FAILED to load mec project from file '" << projFilePath << "', the path is INVALID!" << endl;
        ec = FILE_INVALID;
        return nullptr;
    }
    Holder hProj(new Project());
    const auto eErrCd = hProj->Load(projFilePath);
    if (eErrCd != OK)
    {
        pLogger->Log(Error) << "FAILED to parse mec project from file '" << projFilePath << "'! Error code is " << (int)eErrCd << "." << endl;
        ec = PARSE_FAILED;
        return nullptr;
    }
    return hProj;
}

Project::Project(const string& projName, const string& projDir, const string& mepFileName)
    : m_projName(projName), m_projDir(projDir)
{
    ostringstream loggerNameOss;
    loggerNameOss << "MecProj:" << (projName.length() > 8 ? projName.substr(0, 8) : projName);
    m_pLogger = GetLogger(loggerNameOss.str());
    m_projFilePath = SysUtils::JoinPath(m_projDir, mepFileName+s_PROJ_FILE_EXT);
    m_projVer = (VER_MAJOR<<24) | (VER_MINOR<<16);
    m_bOpened = true;
    if (Save() != OK)
        throw runtime_error("FAILED to save mec project");
}

string Project::GetCacheDir()
{
    if (!s_CACHEDIR.empty())
        return s_CACHEDIR;

    string testDirPath;
    // try to find a suitable path as cache dir
    // 1. user cache dir
    if (testDirPath.empty())
        testDirPath = TryCacheDirPath(ImGuiHelper::getCacheDir(), "MEC");
    // 2. user video dir
    if (testDirPath.empty())
        testDirPath = TryCacheDirPath(ImGuiHelper::getVideoFolder(), ".mec_cache");
    // 3. user home dir
    if (testDirPath.empty())
        testDirPath = TryCacheDirPath(ImGuiHelper::home_path(), ".mec_cache");
    // FAILED to find a suitable location as cache dir
    if (testDirPath.empty())
    {
        auto pLogger = GetLogger("MecProject");
        pLogger->Log(Error) << "FAILED to find a suitable location as MEC cache directory!" << endl;
        return "";
    }
    s_CACHEDIR = testDirPath;
    return s_CACHEDIR;
}

Project::ErrorCode Project::SetCacheDir(const string& path)
{
    if (path.empty())
    {
        s_CACHEDIR.clear();
        return OK;
    }
    if (SysUtils::IsDirectory(path))
    {
        s_CACHEDIR = path;
        return OK;
    }
    if (!SysUtils::CreateDirectoryAt(path, true))
    {
        auto pLogger = GetLogger("MecProject");
        pLogger->Log(Error) << "FAILED to set cache dir as '" << path << "'! cannot create directory at this location." << endl;
        return IO_ERROR;
    }
    s_CACHEDIR = path;
    return OK;
}

Project::ErrorCode Project::Move(const string& newProjDir, bool overwrite)
{
    lock_guard<recursive_mutex> _lk(m_mtxApiLock);
    if (!m_bOpened)
        return NOT_OPENED;
    if (m_projDir == newProjDir)
        return Save();
    if (overwrite && SysUtils::IsDirectory(newProjDir))
    {
        if (!SysUtils::DeleteDirectoryAt(newProjDir))
        {
            m_pLogger->Log(Error) << "FAILED to move project to directory at '" << newProjDir << "'! Target directory already EXISTS and CANNOT be DELETED." << endl;
            return MKDIR_FAILED;
        }
    }
    if (SysUtils::Exists(newProjDir))
    {
        m_pLogger->Log(Error) << "FAILED to move project to directory at '" << newProjDir << "'! Target path is already OCCUPIED." << endl;
        return ALREADY_EXISTS;
    }
    if (!SysUtils::RenameFile(m_projDir, newProjDir))
    {
        m_pLogger->Log(Error) << "FAILED to move project to directory at '" << newProjDir << "'! Move directory failed." << endl;
        return IO_ERROR;
    }
    m_projDir = newProjDir;
    m_projFilePath = SysUtils::JoinPath(m_projDir, m_projName+s_PROJ_FILE_EXT);
    m_bUntitled = false;
    return OK;
}

Project::ErrorCode Project::Load(const string& projFilePath)
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
        attrName = "proj_name";
        if (jnProj.contains(attrName) && jnProj[attrName].is_string())
        {
            m_projName = jnProj["proj_name"].get<imgui_json::string>();
            m_bUntitled = false;
        }
        else
        {
            m_projName = UNTITLED_PROJECT_NAME;
            m_bUntitled = true;
        }
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
                        auto hTask = BackgroundTask::CreateBackgroundTask(tLoadRes.first, hDummySettings, nullptr);
                        if (hTask)
                        {
                            hTask->SetCallbacks(this);
                            hTask->Pause();
                            if (m_hBgtaskExctor)
                                if (!m_hBgtaskExctor->EnqueueTask(hTask))
                                    m_pLogger->Log(Error) << "FAILED to enqueue background task from json '" << strTaskJsonPath << "'!" << endl;
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
    return SaveTo(m_projFilePath);
}

Project::ErrorCode Project::SaveAs(const string& newProjName, const string& newProjDir, bool overwrite)
{
    lock_guard<recursive_mutex> _lk(m_mtxApiLock);
    if (!m_bOpened)
        return NOT_OPENED;
    if (m_projDir == newProjDir && m_projName == newProjName)
        return Save();
    if (overwrite && SysUtils::IsDirectory(newProjDir))
    {
        if (!SysUtils::DeleteDirectoryAt(newProjDir))
        {
            m_pLogger->Log(Error) << "FAILED to move project to directory at '" << newProjDir << "'! Target directory already EXISTS and CANNOT be DELETED." << endl;
            return MKDIR_FAILED;
        }
    }
    if (SysUtils::Exists(newProjDir))
    {
        m_pLogger->Log(Error) << "FAILED to move project to directory at '" << newProjDir << "'! Target path is already OCCUPIED." << endl;
        return ALREADY_EXISTS;
    }
    if (!SysUtils::CreateDirectoryAt(newProjDir, true))
    {
        m_pLogger->Log(Error) << "FAILED to create new project at '" << newProjDir << "', CANNOT create the project directory!" << endl;
        return MKDIR_FAILED;
    }
    m_projName = newProjName;
    m_projDir = newProjDir;
    m_projFilePath = SysUtils::JoinPath(m_projDir, m_projName+s_PROJ_FILE_EXT);
    m_bUntitled = false;
    return Save();
}

Project::ErrorCode Project::SaveTo(const string& projFilePath)
{
    lock_guard<recursive_mutex> _lk(m_mtxApiLock);
    if (!m_bOpened)
        return NOT_OPENED;

    imgui_json::value jnProj;
    jnProj["mec_proj_version"] = imgui_json::number(m_projVer);
    if (!m_bUntitled)
        jnProj["proj_name"] = imgui_json::string(m_projName);

    if (m_pTlHandle)
    {
        MediaTimeline::TimeLine* pTl = (MediaTimeline::TimeLine*)m_pTlHandle;
        // save media items
        imgui_json::array aMediaItems;
        for (auto media : pTl->media_items)
        {
            imgui_json::value item;
            item["id"] = imgui_json::number(media->mID);
            item["name"] = media->mName;
            item["path"] = media->mPath;
            item["type"] = imgui_json::number(media->mMediaType);
            aMediaItems.push_back(item);
        }
        imgui_json::value jnProjContent;
        jnProjContent["MediaBank"] = aMediaItems;
        // save timeline
        imgui_json::value jnTimeLine;
        pTl->Save(jnTimeLine);
        jnProjContent["TimeLine"] = jnTimeLine;
        m_jnProjContent = std::move(jnProjContent);
    }
    jnProj["proj_content"] = m_jnProjContent;

    // save background tasks
    imgui_json::array aTaskSavePaths;
    for (auto& hTask : m_aBgtasks)
    {
        const auto strTaskSavePath = hTask->Save();
        aTaskSavePaths.push_back(strTaskSavePath);
    }
    jnProj["bg_tasks"] = aTaskSavePaths;
    if (!jnProj.save(projFilePath))
    {
        m_pLogger->Log(Error) << "FAILED to save project json file at '" << projFilePath << "'!" << endl;
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
    m_pTlHandle = nullptr;
    return OK;
}

Project::ErrorCode Project::Delete()
{
    lock_guard<recursive_mutex> _lk(m_mtxApiLock);
    if (!m_bOpened)
        return NOT_OPENED;
    const auto projDir = m_projDir;
    Close(false);
    if (SysUtils::IsDirectory(projDir))
    {
        if (!SysUtils::DeleteDirectoryAt(projDir))
        {
            m_pLogger->Log(Error) << "FAILED to delete project at '" << projDir << "'!" << endl;
            return IO_ERROR;
        }
    }
    return OK;
}

void Project::SetBgtaskExecutor(SysUtils::ThreadPoolExecutor::Holder hBgtaskExctor)
{
    lock_guard<recursive_mutex> _lk(m_mtxApiLock);
    m_hBgtaskExctor = hBgtaskExctor;
    if (!hBgtaskExctor)
        return;
    lock_guard<mutex> _lk2(m_mtxBgtaskLock);
    for (auto& hTask : m_aBgtasks)
    {
        if (hTask->IsWaiting())
        {
            if (!hBgtaskExctor->EnqueueTask(hTask))
                m_pLogger->Log(Error) << "Enqueue background task FAILED!" << endl;
        }
    }
}

Project::ErrorCode Project::ChangeProjectName(const string& newName)
{
    lock_guard<recursive_mutex> _lk(m_mtxApiLock);
    if (!m_bOpened)
        return NOT_OPENED;
    if (newName.empty())
        return INVALID_ARG;
    if (m_projName == newName)
        return OK;
    m_projName = newName;
    auto newProjFilePath = SysUtils::JoinPath(m_projDir, newName+s_PROJ_FILE_EXT);
    auto ec = SaveTo(newProjFilePath);
    if (ec != OK)
        return ec;
    m_projFilePath.swap(newProjFilePath);
    if (SysUtils::IsFile(newProjFilePath))
        if (!SysUtils::DeleteFileAt(newProjFilePath))
            m_pLogger->Log(WARN) << "CANNOT delete the old project file at '" << newProjFilePath << "'!" << endl;
    m_bUntitled = false;
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
    hTask->SetCallbacks(this);
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
            SysUtils::DeleteDirectoryAt(strTaskDir);
    }
    return OK;
}

bool Project::OnAddMediaItem(MediaCore::MediaParser::Holder hParser)
{
    if (!m_pTlHandle)
        return false;
    MediaTimeline::TimeLine* pTl = (MediaTimeline::TimeLine*)m_pTlHandle;
    return pTl->AddMediaItem(hParser);
}

bool Project::OnCheckMediaItemImported(const string& strPath)
{
    if (!m_pTlHandle)
        return false;
    MediaTimeline::TimeLine* pTl = (MediaTimeline::TimeLine*)m_pTlHandle;
    return pTl->CheckMediaItemImported(strPath);
}

const uint8_t Project::VER_MAJOR = 1;
const uint8_t Project::VER_MINOR = 1;
const string Project::UNTITLED_PROJECT_NAME = "Untitled";

string Project::s_PROJ_FILE_EXT = ".mep";
string Project::s_CACHEDIR;

string Project::TryCacheDirPath(const string& strParentDir, const string& strCacheDirName)
{
    if (strParentDir.empty())
        return "";
    auto strTestPath = SysUtils::JoinPath(strParentDir, strCacheDirName);
    if (!SysUtils::IsDirectory(strTestPath))
    {
        if (!SysUtils::CreateDirectoryAt(strTestPath, true))
        {
            auto pLogger = GetLogger("MecProject");
            pLogger->Log(WARN) << "FAILED to create MEC cache directory at '" << strTestPath << "'!" << endl;
            strTestPath.clear();
        }
    }
    return std::move(strTestPath);
}
}
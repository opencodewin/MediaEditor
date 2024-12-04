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

#include <sstream>
#include <cstdio>
#include <regex>
#include <vector>
#include <list>
#include <atomic>
#include <thread>
#include <chrono>
#if (defined(__cplusplus) && __cplusplus >= 201703L) || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
#define USE_CPP_FS
#include <filesystem>
namespace fs = std::filesystem;
#elif defined(_WIN32) && !defined(__MINGW64__)
#error "Not supported yet!"
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <ftw.h>
#endif

#include "FileSystemUtils.h"
#include "Logger.h"

using namespace std;
using namespace Logger;

namespace SysUtils
{
#if defined(_WIN32)
static const char _PATH_SEPARATOR = '\\';
#else
static const char _PATH_SEPARATOR = '/';
#endif
static const char _FILE_EXT_SEPARATOR = '.';

bool IsPathSeparator(char c)
{
    return c == _PATH_SEPARATOR;
}

string ExtractFileBaseName(const string& path)
{
    auto lastSlashPos = path.rfind(_PATH_SEPARATOR);
    auto lastDotPos = path.rfind(_FILE_EXT_SEPARATOR);
    if (lastSlashPos == path.length()-1)
    {
        return "";
    }
    else if (lastSlashPos == string::npos)
    {
        if (lastDotPos == string::npos || lastDotPos == 0)
            return path;
        else
            return path.substr(0, lastDotPos);
    }
    else
    {
        if (lastDotPos == string::npos || lastDotPos <= lastSlashPos+1)
            return path.substr(lastSlashPos+1);
        else
            return path.substr(lastSlashPos+1, lastDotPos-lastSlashPos-1);
    }
}

string ExtractFileExtName(const string& path)
{
    auto lastSlashPos = path.rfind(_PATH_SEPARATOR);
    auto lastDotPos = path.rfind(_FILE_EXT_SEPARATOR);
    if (lastSlashPos == path.length()-1)
    {
        return "";
    }
    else if (lastSlashPos == string::npos)
    {
        if (lastDotPos == string::npos || lastDotPos == 0)
            return "";
        else
            return path.substr(lastDotPos);
    }
    else
    {
        if (lastDotPos == string::npos || lastDotPos <= lastSlashPos+1)
            return "";
        else
            return path.substr(lastDotPos);
    }
}

string ExtractFileName(const string& path)
{
    auto lastSlashPos = path.rfind(_PATH_SEPARATOR);
    if (lastSlashPos == path.length()-1)
    {
        return "";
    }
    else if (lastSlashPos == string::npos)
    {
        return path;
    }
    else
    {
        return path.substr(lastSlashPos+1);
    }
}

string ExtractDirectoryPath(const string& path)
{
    auto lastSlashPos = path.rfind(_PATH_SEPARATOR);
    if (lastSlashPos == path.length()-1)
    {
        return path;
    }
    else if (lastSlashPos == string::npos)
    {
        return "";
    }
    else
    {
        return path.substr(0, lastSlashPos+1);
    }
}

string PopLastComponent(const string& path)
{
    if (path.empty())
        return "";
    auto searchPos = path.length()-1;
    while (searchPos >= 0)
    {
        auto lastSlashPos = path.rfind(_PATH_SEPARATOR, searchPos);
        if (lastSlashPos == searchPos)
            searchPos--;
        else if (lastSlashPos == string::npos)
            return path.substr(0, searchPos);
        else
            return path.substr(lastSlashPos+1, searchPos);
    }
    return "";
}

string JoinPath(const string& path1, const string& path2)
{
#ifdef USE_CPP_FS
    const auto result = fs::path(path1)/fs::path(path2);
    return result.string();
#else
    const auto path1_ = !path1.empty() && path1.back() == _PATH_SEPARATOR ? path1.substr(0, path1.size()-1) : path1;
    const auto path2_ = !path2.empty() && path2.front() == _PATH_SEPARATOR ? path2.substr(1) : path2;
    ostringstream oss; oss << path1_ << _PATH_SEPARATOR << path2_;
    return oss.str();
#endif
}

#ifndef USE_CPP_FS
static string GetErrnoStr()
{
    switch (errno)
    {
    case EPERM:
        return "EPERM";
    case ENOENT:
        return "ENOENT";
    case ESRCH:
        return "ESRCH";
    case EINTR:
        return "EINTR";
    case EIO:
        return "EIO";
    case ENXIO:
        return "ENXIO";
    case E2BIG:
        return "E2BIG";
    case ENOEXEC:
        return "ENOEXEC";
    case EBADF:
        return "EBADF";
    case ECHILD:
        return "ECHILD";
    case EAGAIN:
        return "EAGAIN";
    case ENOMEM:
        return "ENOMEM";
    case EACCES:
        return "EACCESS";
    case EFAULT:
        return "EFAULT";
    case ENOTBLK:
        return "ENOTBLK";
    case EBUSY:
        return "EBUSY";
    case EEXIST:
        return "EEXIST";
    case EXDEV:
        return "EXDEV";
    case ENODEV:
        return "ENODEV";
    case ENOTDIR:
        return "ENOTDIR";
    case EISDIR:
        return "EISDIR";
    case EINVAL:
        return "EINVAL";
    case ENFILE:
        return "ENFILE";
    case EMFILE:
        return "EMFILE";
    case ENOTTY:
        return "ENOTTY";
    case ETXTBSY:
        return "ETXTBSY";
    case EFBIG:
        return "EFBIG";
    case ENOSPC:
        return "ENOSPC";
    case ESPIPE:
        return "ESPIPE";
    case EROFS:
        return "EROFS";
    case EMLINK:
        return "EMLINK";
    case EPIPE:
        return "EPIPE";
    case EDOM:
        return"EDOM";
    case ERANGE:
        return "ERANGE";
    default:
        break;
    }
    ostringstream oss; oss << (int)errno << "(unknown error)";
    return oss.str();
}
#endif

bool Exists(const string& path)
{
#ifdef USE_CPP_FS
    return fs::exists(path);
#else
    return access(path.c_str(), F_OK) == 0;
#endif
}

bool IsDirectory(const string& path)
{
#ifdef USE_CPP_FS
    return fs::is_directory(path);
#else
    bool isDir = false;
    auto fd = open(path.c_str(), O_RDONLY);
    if (fd >= 0)
    {
        struct stat st;
        if (fstat(fd, &st) == 0)
            isDir = S_ISDIR(st.st_mode);
        close(fd);
    }
    return isDir;
#endif
}

bool IsFile(const string& path)
{
#ifdef USE_CPP_FS
    return fs::is_regular_file(path);
#else
    bool isFile = false;
    auto fd = open(path.c_str(), O_RDONLY);
    if (fd >= 0)
    {
        struct stat st;
        if (fstat(fd, &st) == 0)
            isFile = S_ISREG(st.st_mode);
        close(fd);
    }
    return isFile;
#endif
}

bool CreateDirectoryAt(const string& _path, bool createParentIfNotExists)
{
    if (_path.empty())
        return false;
#ifdef USE_CPP_FS
    return createParentIfNotExists ? fs::create_directories(_path) : fs::create_directory(_path);
#else
    const auto path = _path.back() == _PATH_SEPARATOR ? _path.substr(0, _path.size()-1) : _path;
    if (createParentIfNotExists)
    {
        const auto parentDir = ExtractDirectoryPath(path);
        if (!parentDir.empty())
        {
            if (!Exists(parentDir))
            {
                if (!CreateDirectoryAt(parentDir, true))
                    return false;
            }
            else if (!IsDirectory(parentDir))
                return false;
        }
    }
#ifdef _WIN32
    auto err = mkdir(path.c_str());
#else
    auto err = mkdir(path.c_str(), S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);
#endif
    return err == 0 || err == EEXIST; 
#endif
}

#ifndef USE_CPP_FS
static int _DeleteDirectory_nftwCb(const char *filename, const struct stat *statptr, int fileflags, struct FTW *pfwt)
{
    int ret;
    if (fileflags == FTW_D || fileflags == FTW_DNR || fileflags == FTW_DP)
        ret = rmdir(filename);
    else
        ret = unlink(filename);
    if (ret != 0)
        Log(Error) << "FAILED to run '_DeleteDirectory_nftwCb()' on '" << filename << "'! Error is '" << GetErrnoStr() << "'." << endl;
    return ret;
}
#endif

bool DeleteDirectoryAt(const string& path, bool recursiveDelete)
{
    if (path.empty())
    {
        Log(Error) << "FAILED to run 'DeleteDirectoryAt()',  argument 'path' is EMPTY!" << endl;
        return false;
    }
    if (!IsDirectory(path))
    {
        Log(Error) << "FAILED to run 'DeleteDirectoryAt()' on '" << path << "'! Target is NOT a DIRECTORY." << endl;
        return false;
    }
#ifdef USE_CPP_FS
    error_code ec;
    if (recursiveDelete)
    {
        const auto cnt = fs::remove_all(path, ec);
        const bool ret = cnt != static_cast<uintmax_t>(-1);
        if (!ret)
            Log(Error) << "FAILED to run 'DeleteDirectoryAt(recursiveDelete=true)' on '" << path << "'! Error is '" << ec << "'." << endl;
        return ret;
    }
    else
    {
        const auto ret = fs::remove(path, ec);
        if (!ret)
            Log(Error) << "FAILED to run 'DeleteDirectoryAt(recursiveDelete=false)' on '" << path << "'! Error is '" << ec << "'." << endl;
        return ret;
    }
#else
    if (recursiveDelete)
    {
        const auto ret = nftw(path.c_str(), _DeleteDirectory_nftwCb, 128, FTW_DEPTH|FTW_PHYS);
        if (ret != 0)
        {
            Log(Error) << "FAILED to run 'DeleteDirectoryAt(recursiveDelete=true)' on '" << path << "'! Error is '" << GetErrnoStr() << "'." << endl;
            return false;
        }
    }
    else
    {
        if (rmdir(path.c_str()) != 0)
        {
            Log(Error) << "FAILED to run 'DeleteDirectoryAt(recursiveDelete=false)' on '" << path << "'! Error is '" << GetErrnoStr() << "'." << endl;
            return false;
        }
    }
    return true;
#endif
}

bool RenameFile(const string& fromPath, const string& toPath)
{
    if (fromPath.empty() || toPath.empty())
    {
        Log(Error) << "'RenameFile()' FAILED, argument 'fromPath' or 'toPath' is EMPTY!" << endl;
        return false;
    }

#ifdef USE_CPP_FS
    error_code ec;
    fs::rename(fromPath, toPath, ec);
    if (ec)
    {
        Log(Error) << "'RenameFile(" << fromPath << ", " << toPath << ")' FAILED! 'std::filesystem::rename()' returns error code '" << ec << "'." << endl;
        return false;
    }
#else
    const auto ret = rename(fromPath.c_str(), toPath.c_str());
    if (ret)
    {
        Log(Error) << "'RenameFile(" << fromPath << ", " << toPath << ")' FAILED! 'errno' is " << errno << "(" << strerror(errno) << ")." << endl;
        return false;
    }
#endif
    return true;
}

bool DeleteFileAt(const string& path)
{
    if (path.empty())
    {
        Log(Error) << "'RenameFile()' FAILED, argument 'fromPath' or 'toPath' is EMPTY!" << endl;
        return false;
    }

#ifdef USE_CPP_FS
    error_code ec;
    const auto ret = fs::remove(path, ec);
    if (!ret)
        Log(Error) << "FAILED to run 'DeleteFileAt()' on '" << path << "'! Error is '" << ec << "'." << endl;
    return ret;
#else
    if (remove(path.c_str()) != 0)
    {
        Log(Error) << "FAILED to run 'DeleteFileAt()' on '" << path << "'! Error is '" << GetErrnoStr() << "'." << endl;
        return false;
    }
    return true;
#endif
}

bool CheckEquivalent(const string& path1, const string& path2)
{
#ifdef USE_CPP_FS
    error_code ec;
    return fs::equivalent(path1, path2, ec);
#else
    stat s1 = stat(path1.c_str());
    stat s2 = stat(path2.c_str());
    return s1.st_dev == s2.st_dev && s1.st_ino == s2.st_ino;
#endif
}

class FileIterator_Impl : public FileIterator
{
public:
    FileIterator_Impl(const string& baseDirPath)
    {
        if (baseDirPath.back() != _PATH_SEPARATOR)
            m_baseDirPath = baseDirPath+_PATH_SEPARATOR;
        else
            m_baseDirPath = baseDirPath;
    }

    virtual ~FileIterator_Impl()
    {
        if (m_parseThread.joinable())
        {
            m_quitThread = true;
            m_parseThread.join();
        }
    }

    Holder Clone() const override
    {
        Holder hNewIns = CreateInstance(m_baseDirPath);
        hNewIns->SetCaseSensitive(m_caseSensitive);
        hNewIns->SetFilterPattern(m_filterPattern, m_isRegexPattern);
        hNewIns->SetRecursive(m_isRecursive);
        if (m_isQuickSampleReady)
        {
            FileIterator_Impl* pFileIter = dynamic_cast<FileIterator_Impl*>(hNewIns.get());
            pFileIter->m_quickSample = m_quickSample;
            pFileIter->m_isQuickSampleReady = true;
        }
        if (m_isParsed && !m_parseFailed)
        {
            FileIterator_Impl* pFileIter = dynamic_cast<FileIterator_Impl*>(hNewIns.get());
            pFileIter->m_paths = m_paths;
            pFileIter->m_isParsed = true;
        }
        else
        {
            hNewIns->StartParsing();
        }
        return hNewIns;
    }

    bool SetFilterPattern(const string& filterPattern, bool isRegexPattern) override
    {
        if (filterPattern == m_filterPattern && isRegexPattern == m_isRegexPattern)
            return true;
        m_filterPattern = filterPattern;
        m_isRegexPattern = isRegexPattern;
        if (isRegexPattern)
        {
            regex::flag_type flags = m_caseSensitive ? regex::optimize : regex::icase|regex::optimize;
            m_filterRegex = regex(filterPattern, flags);
        }
        m_doFileFilter = true;
        return true;
    }

    void SetCaseSensitive(bool sensitive) override
    {
        if (sensitive == m_caseSensitive)
            return;
        m_caseSensitive = sensitive;
        if (m_isRegexPattern)
        {
            regex::flag_type flags = m_caseSensitive ? regex::optimize : regex::icase|regex::optimize;
            m_filterRegex = regex(m_filterPattern, flags);
        }
    }

    void SetRecursive(bool recursive) override
    {
        m_isRecursive = recursive;
    }

    void StartParsing() override
    {
        StartParseThread();
    }

    string GetQuickSample() override
    {
        if (!m_isQuickSampleReady && !m_isParsed)
        {
            StartParseThread();
            while (!m_isQuickSampleReady && !m_isParsed)
                this_thread::sleep_for(chrono::milliseconds(5));
        }
        return m_quickSample;
    }

    string GetBaseDirPath() const override
    {
        return m_baseDirPath;
    }

    string GetCurrFilePath() override
    {
        if (!m_isParsed)
        {
            StartParseThread();
            while (!m_isParsed)
                this_thread::sleep_for(chrono::milliseconds(5));
        }
        if (m_fileIndex >= m_paths.size())
        {
            m_errMsg = "End of path list.";
            return "";
        }
        return m_paths[m_fileIndex];
    }

    string GetNextFilePath() override
    {
        if (!m_isParsed)
        {
            StartParseThread();
            while (!m_isParsed)
                this_thread::sleep_for(chrono::milliseconds(5));
        }
        if (m_fileIndex+1 >= m_paths.size())
        {
            m_errMsg = "End of path list.";
            return "";
        }
        m_fileIndex++;
        return m_paths[m_fileIndex];
    }

    uint32_t GetCurrFileIndex() const override
    {
        return m_fileIndex;
    }

    vector<string> GetAllFilePaths() override
    {
        if (!m_isParsed)
        {
            StartParseThread();
            while (!m_isParsed)
                this_thread::sleep_for(chrono::milliseconds(5));
        }
        return vector<string>(m_paths);
    }

    uint32_t GetValidFileCount(bool refresh) override
    {
        if (!m_isParsed)
        {
            StartParseThread();
            while (!m_isParsed)
                this_thread::sleep_for(chrono::milliseconds(5));
        }
        return m_paths.size();
    }

    bool SeekToValidFile(uint32_t index) override
    {
        if (!m_isParsed)
        {
            StartParseThread();
            while (!m_isParsed)
                this_thread::sleep_for(chrono::milliseconds(5));
        }
        if (index >= m_paths.size())
        {
            m_errMsg = "Arugment 'index' is out of valid range!";
            return false;
        }
        m_fileIndex = index;
        return true;
    }

    string JoinBaseDirPath(const string& relativeFilePath) const override
    {
        ostringstream oss; oss << m_baseDirPath << relativeFilePath;
        return oss.str();
    }

    string JoinBaseDirPath(const char* pPath) const
    {
        ostringstream oss; oss << m_baseDirPath << pPath;
        return oss.str();
    }

    string GetFilterPattern(bool& bIsRegexPattern) const override
    {
        bIsRegexPattern = m_isRegexPattern;
        return m_filterPattern;
    }

    bool IsCaseSensitive() const override
    {
        return m_caseSensitive;
    }

    bool IsRecursive() const override
    {
        return m_isRecursive;
    }

    string GetError() const override
    {
        return m_errMsg;
    }

#ifdef USE_CPP_FS
    bool IsCorrectFileType(const fs::directory_entry& dirEntry)
    {
        if (dirEntry.is_regular_file())
            return true;
        if (dirEntry.is_symlink() && dirEntry.status().type() == fs::file_type::regular)
            return true;
        return false;
    }

    bool IsSubDirectory(const fs::directory_entry& dirEntry)
    {
        if (dirEntry.is_directory())
            return true;
        if (dirEntry.is_symlink() && dirEntry.status().type() == fs::file_type::directory)
            return true;
        return false;
    }
#elif defined(_WIN32) && !defined(__MINGW64__)
#else
    bool IsCorrectFileType(const struct dirent* ent, const string& relativePath)
    {
#ifdef _DIRENT_HAVE_D_TYPE
        if (ent->d_type == DT_REG)
            return true;
#endif
        struct stat fileStat;
        const string fullPath = JoinBaseDirPath(relativePath);
        int ret;
        if ((ret = stat(fullPath.c_str(), &fileStat)) < 0)
        {
            Log(Error) << "FAILED to invoke 'stat' on file '" << relativePath << "' in directory '" << m_baseDirPath << "'! ret=" << ret << "." << endl;
            return false;
        }
        const auto st_mode = fileStat.st_mode;
        return (st_mode&S_IFREG)!=0 && (st_mode&S_IREAD)!=0;
    }

    bool IsSubDirectory(const struct dirent* ent, const string& relativePath)
    {
#ifdef _DIRENT_HAVE_D_TYPE
        if (ent->d_type == DT_DIR)
            return true;
#endif
        struct stat fileStat;
        const string fullPath = JoinBaseDirPath(relativePath);
        int ret;
        if ((ret = stat(fullPath.c_str(), &fileStat)) < 0)
        {
            Log(Error) << "FAILED to invoke 'stat' on file '" << relativePath << "' in directory '" << m_baseDirPath << "'! ret=" << ret << "." << endl;
            return false;
        }
        const auto st_mode = fileStat.st_mode;
        return (st_mode&S_IFDIR)!=0;
    }
#endif

    bool IsMatchPattern(const string& path)
    {
        return IsMatchPattern(path.c_str());
    }

    bool IsMatchPattern(const char* pPath)
    {
        if (m_isRegexPattern)
        {
            return regex_match(pPath, m_filterRegex);
        }
        else
        {
            char buff[256];
            if (sscanf(pPath, m_filterPattern.c_str(), buff) > 0)
                return true;
        }
        return false;
    }

    void StartParseThread()
    {
        bool testVal = false;
        if (!m_parsingStarted.compare_exchange_strong(testVal, true))
            return;
        m_quitThread = false;
        m_parseThread = thread(&FileIterator_Impl::ParseProc, this);
    }

    void ParseProc()
    {
        list<string> pathList;
        if (!ParseOneDir("", pathList))
        {
            m_isParsed = true;
            m_parseFailed = true;
            return;
        }
        pathList.sort();
        m_paths.clear();
        m_paths.reserve(pathList.size());
        while (!pathList.empty())
        {
            m_paths.push_back(std::move(pathList.front()));
            pathList.pop_front();
        }
        m_isParsed = true;
    }

    bool ParseOneDir(const string& subDirPath, list<string>& pathList)
    {
        bool ret = true;
#ifdef USE_CPP_FS
        fs::path dirFullPath = fs::path(m_baseDirPath)/fs::path(subDirPath);
        fs::directory_iterator dirIter(dirFullPath);
        for (auto const& dirEntry : dirIter)
        {
            if (m_quitThread)
                break;
            if (m_isRecursive && IsSubDirectory(dirEntry))
            {
                const fs::path subDirPath2 = fs::path(subDirPath)/dirEntry.path().filename();
                if (!ParseOneDir(subDirPath2.string(), pathList))
                {
                    ret = false;
                    break;
                }
            }
            else if (IsCorrectFileType(dirEntry) && (!m_doFileFilter || IsMatchPattern(dirEntry.path().string())))
            {
                const fs::path filePath = fs::path(subDirPath)/dirEntry.path().filename();
                if (pathList.empty())
                {
                    m_quickSample = filePath.string();
                    m_isQuickSampleReady = true;
                }
                pathList.push_back(filePath.string());
            }
        }
#elif defined(_WIN32) && !defined(__MINGW64__)
        throw runtime_error("Unimplemented!");
#else
        string dirFullPath = JoinBaseDirPath(subDirPath);
        DIR* pSubDir = opendir(dirFullPath.c_str());
        if (!pSubDir)
        {
            ostringstream oss; oss << "FAILED to open directory '" << dirFullPath << "'!";
            m_errMsg = oss.str();
            return false;
        }
        struct dirent *ent;
        while ((ent = readdir(pSubDir)) != NULL)
        {
            if (m_quitThread)
                break;
            const string filename(ent->d_name);
            if (filename == "." || filename == "..")
                continue;
            ostringstream pathOss;
            if (!subDirPath.empty())
                pathOss << subDirPath << _PATH_SEPARATOR;
            pathOss << filename;
            const string relativePath = pathOss.str();
            if (m_isRecursive && IsSubDirectory(ent, relativePath))
            {
                if (!ParseOneDir(relativePath, pathList))
                {
                    ret = false;
                    break;
                }
            }
            else if (IsCorrectFileType(ent, relativePath) && (!m_doFileFilter || IsMatchPattern(ent->d_name)))
            {
                if (pathList.empty())
                {
                    m_quickSample = relativePath;
                    m_isQuickSampleReady = true;
                }
                pathList.push_back(relativePath);
            }
        }
        closedir(pSubDir);
#endif
        return ret;
    }

private:
    string m_baseDirPath;
    bool m_isParsed{false};
    bool m_parseFailed{false};
    bool m_quitThread{false};
    atomic_bool m_parsingStarted{false};
    thread m_parseThread;
    vector<string> m_paths;
    string m_quickSample;
    bool m_isQuickSampleReady{false};
    bool m_isRecursive{false};
    bool m_doFileFilter{false};
    string m_filterPattern;
    bool m_caseSensitive{true};
    bool m_isRegexPattern;
    regex m_filterRegex;
    uint32_t m_fileIndex{0};
    string m_errMsg;
};

static const auto FILE_ITERATOR_HOLDER_DELETER = [] (FileIterator* p) {
    FileIterator_Impl* ptr = dynamic_cast<FileIterator_Impl*>(p);
    delete ptr;
};

FileIterator::Holder FileIterator::CreateInstance(const string& baseDirPath)
{
    if (!IsDirectory(baseDirPath))
        return nullptr;
    return FileIterator::Holder(new FileIterator_Impl(baseDirPath), FILE_ITERATOR_HOLDER_DELETER);
}
}

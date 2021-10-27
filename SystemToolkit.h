#ifndef SYSTEMTOOLKIT_H
#define SYSTEMTOOLKIT_H

#ifdef WIN32
#define PATH_SEP '\\'
#elif defined(LINUX) or defined(APPLE)
#define PATH_SEP '/'
#endif

#include <string>
#include <list>

namespace SystemToolkit
{
    // get fixed length string (17 chars) YYYYMMDDHHmmssiii
    std::string date_time_string();

    // get the OS dependent username
    std::string username();

    // get the OS dependent home path
    std::string home_path();

    // get the OS dependent location
    std::string cwd_path();

    // get the OS dependent path where to store settings
    std::string settings_path();

    // get the OS dependent path where to store temporary files
    std::string temp_path();

    // builds the OS dependent complete file name
    std::string full_filename(const std::string& path, const std::string& filename);

    // extract the filename from a full path / URI (e.g. file:://home/me/toto.mpg -> toto.mpg)
    std::string filename(const std::string& path);

    // extract the base filename from a full path / URI (e.g. file:://home/me/toto.mpg -> toto)
    std::string base_filename(const std::string& path);

    // extract the path of a filename from a full URI (e.g. file:://home/me/toto.mpg -> file:://home/me/)
    std::string path_filename(const std::string& path);

    // extract the extension of a filename
    std::string extension_filename(const std::string& filename);

    // tests if dir is a directory and return its path, empty string otherwise
    std::string path_directory(const std::string& path);

    // list all files of a directory mathing the given filter extension (if any)
    std::list<std::string> list_directory(const std::string& path, const std::list<std::string> &extensions);

    // builds a path relative to 'relativeTo' to reach file at 'absolutePath' (e.g. /a/b/c/d rel to /a/b/e -> ../c/d)
    std::string path_relative_to_path(const std::string& absolutePath, const std::string& relativeTo);

    // builds the absolute path (starting with '/') of relativePath starting from relativeTo (e.g. ../c/d rel to /a/b/e -> /a/b/c/d)
    std::string path_absolute_from_path(const std::string& relativePath, const std::string& relativeTo);

    // true of file exists
    bool file_exists(const std::string& path);

    // create directory and return true on success
    bool create_directory(const std::string& path);

    // remove file and return true if the file does not exist after this call
    bool remove_file(const std::string& path);

    // try to open the file with system
    void open(const std::string& path);

    // try to execute a command
    void execute(const std::string& command);

    // return memory used (in bytes)
    long memory_usage();
    // return maximum memory resident set size used (in bytes)
    long memory_max_usage();

}

#endif // SYSTEMTOOLKIT_H

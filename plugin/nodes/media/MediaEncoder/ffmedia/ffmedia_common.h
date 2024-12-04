#ifndef __FFMEDIA_COMMON_H__
#define __FFMEDIA_COMMON_H__

#include "ffmedia_define.h"
#include <iostream>

#ifdef av_err2str
#undef av_err2str
av_always_inline char* av_err2str(int errnum)
{
    static char str[AV_ERROR_MAX_STRING_SIZE];
    memset(str, 0, sizeof(str));
    return av_make_error_string(str, AV_ERROR_MAX_STRING_SIZE, errnum);
}
#endif
#ifdef _WIN32
#define print_av_err_str(errnum) \
    fprintf(stderr, "[FFMedia]Error: %s @line %d\n",  av_err2str(errnum), __LINE__);
#else
#define print_av_err_str(errnum)                                                      \
    {                                                                                 \
        std::cout << "[FFMedia]Error: " << av_err2str(errnum) << " @line" << __LINE__ \
                  << std::endl;                                                       \
    }
#endif

static inline std::string get_container_string(std::string& name)
{
    std::string result;
    int pos = name.rfind('.');
    if (pos != std::string::npos)
        result = name.substr(pos + 1);
    return result;
}

static inline int get_bit_rate(int dim)
{
    if (dim < 1920)
        return 5 * 1024 * 1024;
    if (dim < 2880)
        return 10 * 1024 * 1024;
    if (dim < 3840)
        return 20 * 1024 * 1024;
    if (dim < 5760)
        return 50 * 1024 * 1024;
    return 100 * 1024 * 1024;
}

#endif  //__FFMEDIA_COMMON_H__

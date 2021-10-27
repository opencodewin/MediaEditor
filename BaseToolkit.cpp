#include "BaseToolkit.h"

#include <chrono>
#include <ctime>
#include <sstream>
#include <list>
#include <iomanip>
#include <algorithm>
#include <climits>
#include <map>

#include <locale>
#include <unicode/ustream.h>
#include <unicode/translit.h>

uint64_t BaseToolkit::uniqueId()
{
    auto duration = std::chrono::high_resolution_clock::now().time_since_epoch();
    // 64-bit int                                                                  18446744073709551615UL
    return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() % 1000000000000000000UL;
}



std::string BaseToolkit::uniqueName(const std::string &basename, std::list<std::string> existingnames)
{
    std::string tentativename = basename;
    int count = 1;
    int max = 100;

    // while tentativename can be found in the list of existingnames
    while ( std::find( existingnames.begin(), existingnames.end(), tentativename ) != existingnames.end() )
    {
        for( auto it = existingnames.cbegin(); it != existingnames.cend(); ++it) {
            if ( it->find(tentativename) != std::string::npos)
                ++count;
        }

        if (count > 1)
            tentativename = basename + "_" + std::to_string( count );
        else
            tentativename += "_";

        if ( --max < 0 ) // for safety only, should never be needed
            break;
    }

    return tentativename;
}

// Using ICU transliteration :
// https://unicode-org.github.io/icu/userguide/transforms/general/#icu-transliterators

std::string BaseToolkit::transliterate(const std::string &input)
{
    // because icu::Transliterator is slow, we keep a dictionnary of already
    // transliterated texts to be faster during repeated calls (update of user interface)
    static std::map<std::string, std::string> dictionnary_;
    std::map<std::string, std::string>::const_iterator existingentry = dictionnary_.find(input);

    if (existingentry == dictionnary_.cend()) {

        auto ucs = icu::UnicodeString::fromUTF8(input);

        UErrorCode status = U_ZERO_ERROR;
        icu::Transliterator *firstTrans = icu::Transliterator::createInstance(
                    "any-NFKD ; [:Nonspacing Mark:] Remove; NFKC; Latin", UTRANS_FORWARD, status);
        firstTrans->transliterate(ucs);
        delete firstTrans;

        icu::Transliterator *secondTrans = icu::Transliterator::createInstance(
                    "any-NFKD ; [:Nonspacing Mark:] Remove; [@!#$*%~] Remove; NFKC", UTRANS_FORWARD, status);
        secondTrans->transliterate(ucs);
        delete secondTrans;

        std::ostringstream output;
        output << ucs;

        // remember for future
        dictionnary_[input] = output.str();
    }

    // return remembered transliterated text
    return dictionnary_[input];
}


std::string BaseToolkit::byte_to_string(long b)
{
    double numbytes = static_cast<double>(b);
    std::ostringstream oss;

    std::list<std::string> list = {" Bytes", " KB", " MB", " GB", " TB"};
    std::list<std::string>::iterator i = list.begin();

    while(numbytes >= 1024.0 && i != list.end())
    {
        ++i;
        numbytes /= 1024.0;
    }
    oss << std::fixed << std::setprecision(2) << numbytes;
    if (i != list.end()) oss << *i;
    return oss.str();
}

std::string BaseToolkit::bits_to_string(long b)
{
    double numbytes = static_cast<double>(b);
    std::ostringstream oss;

    std::list<std::string> list = {" bit", " Kbit", " Mbit", " Gbit", " Tbit"};
    std::list<std::string>::iterator i = list.begin();

    while(numbytes >= 1000.0 && i != list.end())
    {
        ++i;
        numbytes /= 1000.0;
    }
    oss << std::fixed << std::setprecision(2) << numbytes;
    if (i != list.end()) oss << *i;
    return oss.str();
}


std::string BaseToolkit::trunc_string(const std::string& path, int N)
{
    std::string trunc = path;
    int l = path.size();
    if ( l > N ) {
        trunc = std::string("...") + path.substr( l - N + 3 );
    }
    return trunc;
}


std::string BaseToolkit::common_prefix( const std::list<std::string> & allStrings )
{
    if (allStrings.empty())
        return std::string();

    const std::string &s0 = allStrings.front();
    auto _end = s0.cend();
    for (auto it=std::next(allStrings.cbegin()); it != allStrings.cend(); ++it)
    {
        auto _loc = std::mismatch(s0.cbegin(), s0.cend(), it->cbegin(), it->cend());
        if (std::distance(_loc.first, _end) > 0)
            _end = _loc.first;
    }

    return std::string(s0.cbegin(), _end);
}


std::string BaseToolkit::common_suffix(const std::list<std::string> & allStrings)
{
    if (allStrings.empty())
        return std::string();

    const std::string &s0 = allStrings.front();
    auto r_end = s0.crend();
    for (auto it=std::next(allStrings.cbegin()); it != allStrings.cend(); ++it)
    {
        auto r_loc = std::mismatch(s0.crbegin(), s0.crend(), it->crbegin(), it->crend());
        if (std::distance(r_loc.first, r_end) > 0)
            r_end = r_loc.first;
    }

    std::string suffix = std::string(s0.crbegin(), r_end);
    std::reverse(suffix.begin(), suffix.end());
    return suffix;
}


std::string BaseToolkit::common_pattern(const std::list<std::string> &allStrings)
{
    if (allStrings.empty())
        return std::string();

    // find common prefix and suffix
    const std::string &s0 = allStrings.front();
    auto _end = s0.cend();
    auto r_end = s0.crend();
    for (auto it=std::next(allStrings.cbegin()); it != allStrings.cend(); ++it)
    {
        auto _loc = std::mismatch(s0.cbegin(), s0.cend(), it->cbegin(), it->cend());
        if (std::distance(_loc.first, _end) > 0)
            _end = _loc.first;

        auto r_loc = std::mismatch(s0.crbegin(), s0.crend(), it->crbegin(), it->crend());
        if (std::distance(r_loc.first, r_end) > 0)
            r_end = r_loc.first;
    }

    std::string suffix = std::string(s0.crbegin(), r_end);
    std::reverse(suffix.begin(), suffix.end());

    return std::string(s0.cbegin(), _end) + "*" + suffix;
}

std::string BaseToolkit::common_numbered_pattern(const std::list<std::string> &allStrings, int *min, int *max)
{
    if (allStrings.empty())
        return std::string();

    // find common prefix and suffix
    const std::string &s0 = allStrings.front();
    auto _end = s0.cend();
    auto r_end = s0.crend();
    for (auto it=std::next(allStrings.cbegin()); it != allStrings.cend(); ++it)
    {
        auto _loc = std::mismatch(s0.cbegin(), s0.cend(), it->cbegin(), it->cend());
        if (std::distance(_loc.first, _end) > 0)
            _end = _loc.first;

        auto r_loc = std::mismatch(s0.crbegin(), s0.crend(), it->crbegin(), it->crend());
        if (std::distance(r_loc.first, r_end) > 0)
            r_end = r_loc.first;
    }

    // range of middle string, after prefix and before suffix
    size_t pos_prefix = std::distance(s0.cbegin(), _end);
    size_t pos_suffix = s0.size() - pos_prefix - std::distance(s0.crbegin(), r_end);

    int n = -1;
    *max = 0;
    *min = INT_MAX;
    // loop over all strings to verify there are numbers between prefix and suffix
    for (auto it = allStrings.cbegin(); it != allStrings.cend(); ++it)
    {
        // get middle string, after prefix and before suffix
        std::string s = it->substr(pos_prefix, pos_suffix);
        // is this central string ONLY made of digits?
        if (s.end() == std::find_if(s.begin(), s.end(), [](unsigned char c)->bool { return !isdigit(c); })) {
            // yes, validate
            *max = std::max(*max, std::atoi(s.c_str()) );
            *min = std::min(*min, std::atoi(s.c_str()) );
            if (n < 0)
                n = s.size();
            else if ( n != s.size() ) {
                n = 0;
                break;
            }
        }
        else {
            n = 0;
            break;
        }
    }

    if ( n < 1 )
        return std::string();

    std::string suffix = std::string(s0.crbegin(), r_end);
    std::reverse(suffix.begin(), suffix.end());
    std::string pattern = std::string(s0.cbegin(), _end);
    pattern += "%0" + std::to_string(n) + "d";
    pattern += suffix;
    return pattern;
}

#ifndef BASETOOLKIT_H
#define BASETOOLKIT_H

#include <list>
#include <string>

namespace BaseToolkit
{

// get integer with unique id
uint64_t uniqueId();

// proposes a name that is not already in the list
std::string uniqueName(const std::string &basename, std::list<std::string> existingnames);

// get a transliteration to Latin of any string
std::string transliterate(const std::string &input);

// get a string to display memory size with unit KB, MB, GB, TB
std::string byte_to_string(long b);

// get a string to display bit size with unit Kbit, MBit, Gbit, Tbit
std::string bits_to_string(long b);

// Truncate a string to display the right most N characters (e.g. ./home/me/toto.mpg -> ...ome/me/toto.mpg)
std::string trunc_string(const std::string& path, int N);

// find common parts in a list of strings
std::string common_prefix(const std::list<std::string> &allStrings);
std::string common_suffix(const std::list<std::string> &allStrings);

// form a pattern "prefix*suffix" (e.g. file list)
std::string common_pattern(const std::list<std::string> &allStrings);

// form a pattern "prefix%03dsuffix" (e.g. numbered file list)
std::string common_numbered_pattern(const std::list<std::string> &allStrings, int *min, int *max);

}


#endif // BASETOOLKIT_H

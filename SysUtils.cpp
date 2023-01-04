#include "SysUtils.h"

using namespace std;

string GetFileNameFromPath(const string& path)
{
    auto pos = path.rfind("/");
    if (pos == string::npos)
        pos = path.rfind("\\");
    auto fileName = pos==string::npos ? path : path.substr(pos+1);
    return move(fileName);
}
#include <iostream>
#include "HwaccelManager.h"
#include "Logger.h"

using namespace std;
using namespace MediaCore;
using namespace Logger;

int main(int argc, char* argv[])
{
    GetDefaultLogger()->SetShowLevels(DEBUG);
    auto hHwaMgr = HwaccelManager::CreateInstance();
    hHwaMgr->SetLogLevel(DEBUG);
    if (!hHwaMgr->Init())
    {
        Log(Error) << "FAILED to init 'HwaccelManager' instance! Error is '" << hHwaMgr->GetError() << "'." << endl;
        return -1;
    }
    auto devices = hHwaMgr->GetHwaccelTypes();
    return 0;
}
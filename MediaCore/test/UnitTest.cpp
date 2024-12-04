#include <string>
#include <functional>
#include <unordered_map>
#include "DebugHelper.h"
#include "Logger.h"

using namespace std;
using namespace Logger;
using namespace MediaCore;

#include "MediaReader.h"
static void Unit_CreateVideoReaderInstance()
{
    AutoSection _as("CreateVideoInstance");
    auto hVideoReader = MediaReader::CreateVideoInstance();
}

struct TestCase
{
    function<void (void)> testProc;
};

static unordered_map<string, TestCase> g_TestUnits = {
    {"CreateVideoReaderInstance", {Unit_CreateVideoReaderInstance}}
};

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        Log(Error) << "Wrong arguments! Usage: TestUnit <TestCaseName> [loopCount]" << endl;
        return -1;
    }

    string testCaseName;
    int testLoopCount = 1;
    if (argc >= 2)
        testCaseName = string(argv[1]);
    if (argc >= 3)
        testLoopCount = atoi(argv[2]);

    auto testCaseIter = g_TestUnits.find(testCaseName);
    if (testCaseIter == g_TestUnits.end())
    {
        Log(Error) << "No TestCase is named as '" << testCaseName << "'!" << endl;
        return -1;
    }

    int loopCnt = 0;
    while (loopCnt++ < testLoopCount)
    {
        auto hPa = PerformanceAnalyzer::GetThreadLocalInstance();
        hPa->Reset();
        testCaseIter->second.testProc();
        hPa->End();
        hPa->LogAndClearStatistics(INFO);
    }
    return 0;
}
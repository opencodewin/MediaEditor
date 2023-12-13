#include <Logger.h>
#include "BackgroundTask.h"

namespace json = imgui_json;
using namespace Logger;
using namespace std;

namespace MEC
{
BackgroundTask::Holder CreateBgtaskInstanceFrameIntepolation(const json::value& jnTask);

BackgroundTask::Holder BackgroundTask::CreateBackgroundTask(const json::value& jnTask)
{
    if (!jnTask.contains("type"))
    {
        Log(Error) << "FAILED to create 'BackgroundTask'! Json does not have 'type' attribute." << endl;
        return nullptr;
    }
    const string strTaskType = jnTask["type"].get<json::string>();
    if (strTaskType == "FrameInterpolation")
        return CreateBgtaskInstanceFrameIntepolation(jnTask);
    else
    {
        Log(Error) << "FAILED to create 'BackgroundTask'! Unsupported task type '" << strTaskType << "'." << endl;
        return nullptr;
    }
}
}
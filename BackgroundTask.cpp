#include <BaseUtils/Logger.h>
#include "BackgroundTask.h"

namespace json = imgui_json;
using namespace Logger;
using namespace std;

namespace MEC
{
BackgroundTask::Holder CreateBgtask_Vidstab(const json::value& jnTask, MediaCore::SharedSettings::Holder hSettings, RenderUtils::TextureManager::Holder hTxMgr);
BackgroundTask::Holder CreateBgtask_SceneDetect(const json::value& jnTask, MediaCore::SharedSettings::Holder hSettings, RenderUtils::TextureManager::Holder hTxMgr);

BackgroundTask::Holder BackgroundTask::CreateBackgroundTask(const json::value& jnTask, MediaCore::SharedSettings::Holder hSettings, RenderUtils::TextureManager::Holder hTxMgr)
{
    if (!jnTask.contains("type"))
    {
        Log(Error) << "FAILED to create 'BackgroundTask'! Json does not have 'type' attribute." << endl;
        return nullptr;
    }
    const string strTaskType = jnTask["type"].get<json::string>();
    if (strTaskType == "Vidstab")
        return CreateBgtask_Vidstab(jnTask, hSettings, hTxMgr);
    else if (strTaskType == "SceneDetect")
        return CreateBgtask_SceneDetect(jnTask, hSettings, hTxMgr);
    else
    {
        Log(Error) << "FAILED to create 'BackgroundTask'! Unsupported task type '" << strTaskType << "'." << endl;
        return nullptr;
    }
}
}
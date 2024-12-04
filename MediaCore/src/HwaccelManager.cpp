#include <list>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <atomic>
#include <functional>
#include "HwaccelManager.h"

extern "C"
{
    #include "libavutil/hwcontext.h"
}

using namespace std;
using namespace Logger;

namespace MediaCore
{

class HwaccelManager_Impl : public HwaccelManager
{
public:
    HwaccelManager_Impl()
    {
        m_logger = GetLogger("HwaMgr");
    }

    bool Init() override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        list<CheckHwaccelThreadContext> checkTaskContexts;
        AVHWDeviceType hwDevType = AV_HWDEVICE_TYPE_NONE;
        do {
            hwDevType = av_hwdevice_iterate_types(hwDevType);
            if (hwDevType != AV_HWDEVICE_TYPE_NONE)
            {
                checkTaskContexts.emplace_back();
                auto& newTask = checkTaskContexts.back();
                newTask.hwDevType = hwDevType;
                newTask.checkThread = thread(&HwaccelManager_Impl::CheckHwaccelUsableProc, this, &newTask);
            }
        } while (hwDevType != AV_HWDEVICE_TYPE_NONE);
        if (!checkTaskContexts.empty())
        {
            while (true)
            {
                this_thread::sleep_for(chrono::milliseconds(10));
                auto iter = find_if(checkTaskContexts.begin(), checkTaskContexts.end(), [] (auto& ctx) {
                    return !ctx.done;
                });
                if (iter == checkTaskContexts.end())
                    break;
            }
            m_isVaapiUsable = false;
            m_isCudaUsable = false;
            for (auto& ctx : checkTaskContexts)
            {
                if (ctx.devInfo.usable)
                {
                    if (ctx.hwDevType == AV_HWDEVICE_TYPE_VAAPI)
                        m_isVaapiUsable = true;
                    else if (ctx.hwDevType == AV_HWDEVICE_TYPE_CUDA)
                        m_isCudaUsable = true;
                }
                m_devices.emplace_back(ctx.hwDevType, ctx.devInfo, ctx.basePriority);
                if (ctx.checkThread.joinable())
                    ctx.checkThread.join();
            }
            checkTaskContexts.clear();
        }

        m_devices.sort(DEVICE_INFO_COMPARATOR);
        return true;
    }

    vector<const HwaccelTypeInfo*> GetHwaccelTypes() override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        vector<const HwaccelTypeInfo*> result;
        result.reserve(m_devices.size());
        for (auto& devinfo : m_devices)
            result.push_back(&devinfo.commInfo);
        return std::move(result);
    }

    vector<const HwaccelTypeInfo*> GetHwaccelTypesForCodec(const string& _codecName, uint32_t codecTypeFlag) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        auto aHwTypes = GetHwaccelTypes();
        string codecName(_codecName.size(), ' ');
        transform(_codecName.begin(), _codecName.end(), codecName.begin(), [] (auto c) { return tolower(c); });
        if (codecName == "mjpeg" && (codecTypeFlag&VIDEO) > 0 && (codecTypeFlag&DECODER) > 0)
        {
            // disable using 'vaapi' hardware mode to decode 'mjpeg', since the output uv planes are reversed
            auto iter = find_if(aHwTypes.begin(), aHwTypes.end(), [] (auto& pInfo) { return pInfo->name == "vaapi"; });
            if (iter != aHwTypes.end())
                aHwTypes.erase(iter);
        }
        return aHwTypes;
    }

    void IncreaseDecoderInstanceCount(const string& devType) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        auto hwDevType = av_hwdevice_find_type_by_name(devType.c_str());
        if (hwDevType == AV_HWDEVICE_TYPE_NONE)
        {
            m_logger->Log(WARN) << "UNKNOWN hardware device type '" << devType << "'!" << endl;
            return;
        }
        auto iter = find_if(m_devices.begin(), m_devices.end(), [hwDevType] (auto& devInfo) {
            return hwDevType == devInfo.hwDevType;
        });
        if (iter == m_devices.end())
        {
            m_logger->Log(WARN) << "CANNOT find hardware device type '" << devType << "' in the device list!" << endl;
            return;
        }
        iter->decoderCount++;
        iter->UpdateDynamicPriority();
        m_devices.sort(DEVICE_INFO_COMPARATOR);
    }

    void DecreaseDecoderInstanceCount(const std::string& devType) override
    {
        lock_guard<recursive_mutex> lk(m_apiLock);
        auto hwDevType = av_hwdevice_find_type_by_name(devType.c_str());
        if (hwDevType == AV_HWDEVICE_TYPE_NONE)
        {
            m_logger->Log(WARN) << "UNKNOWN hardware device type '" << devType << "'!" << endl;
            return;
        }
        auto iter = find_if(m_devices.begin(), m_devices.end(), [hwDevType] (auto& devInfo) {
            return hwDevType == devInfo.hwDevType;
        });
        if (iter == m_devices.end())
        {
            m_logger->Log(WARN) << "CANNOT find hardware device type '" << devType << "' in the device list!" << endl;
            return;
        }
        iter->decoderCount--;
        iter->UpdateDynamicPriority();
        m_devices.sort(DEVICE_INFO_COMPARATOR);
    }

    void SetLogLevel(Logger::Level l) override
    {
        m_logger->SetShowLevels(l);
    }

    string GetError() const override
    {
        return m_errMsg;
    }

public:
    struct CheckHwaccelThreadContext
    {
        AVHWDeviceType hwDevType;
        HwaccelTypeInfo devInfo;
        int basePriority;
        thread checkThread;
        bool done{false};
    };

    void CheckHwaccelUsableProc(CheckHwaccelThreadContext* ctx)
    {
        int basePriority;
        switch (ctx->hwDevType)
        {
        case AV_HWDEVICE_TYPE_CUDA:
        case AV_HWDEVICE_TYPE_VAAPI:
        case AV_HWDEVICE_TYPE_D3D11VA:
        case AV_HWDEVICE_TYPE_VIDEOTOOLBOX:
            basePriority = 0;
            break;
        case AV_HWDEVICE_TYPE_QSV:
        case AV_HWDEVICE_TYPE_VULKAN:
            basePriority = 1;
            break;
        case AV_HWDEVICE_TYPE_MEDIACODEC:
            basePriority = 2;
            break;
        case AV_HWDEVICE_TYPE_VDPAU:
        case AV_HWDEVICE_TYPE_DXVA2:
            basePriority = 3;
            break;
        case AV_HWDEVICE_TYPE_DRM:
        case AV_HWDEVICE_TYPE_OPENCL:
            basePriority = 9;
            break;
        default:
            basePriority = 16;
            break;
        }
        string typeName = string(av_hwdevice_get_type_name(ctx->hwDevType));
        bool usable = true;
        AVBufferRef* pDevCtx = nullptr;
        int fferr = av_hwdevice_ctx_create(&pDevCtx, ctx->hwDevType, nullptr, nullptr, 0);
        if (fferr == 0)
        {
            m_logger->Log(DEBUG) << "--> " << typeName << " <-- Check SUCCESSFUL!" << endl;
        }
        else
        {
            usable = false;
            m_logger->Log(DEBUG) << "--> " << typeName << " <-- Check FAILED! fferr=" << fferr << endl;
        }
        if (pDevCtx)
            av_buffer_unref(&pDevCtx);
        ctx->devInfo = {typeName, usable};
        ctx->basePriority = basePriority;
        ctx->done = true;
    }

private:
    struct DeviceInfo_Internal
    {
        DeviceInfo_Internal(AVHWDeviceType _hwDevType, HwaccelTypeInfo _commInfo, int _basePriority)
            : hwDevType(_hwDevType), commInfo(_commInfo), basePriority(_basePriority)
        {
            UpdateDynamicPriority();
        }

        void UpdateDynamicPriority()
        {
            dynamicPriority = basePriority*10000+decoderCount;
        }

        AVHWDeviceType hwDevType;
        HwaccelTypeInfo commInfo;
        atomic_int32_t decoderCount{0};
        int basePriority;
        int dynamicPriority;
    };

    static const function<bool(const DeviceInfo_Internal& a, const DeviceInfo_Internal& b)> DEVICE_INFO_COMPARATOR;

private:
    list<DeviceInfo_Internal> m_devices;
    bool m_isVaapiUsable{false};
    bool m_isCudaUsable{false};

    recursive_mutex m_apiLock;
    ALogger* m_logger;
    string m_errMsg;
};

static const auto HWACCEL_MANAGER_DELETER = [] (HwaccelManager* pHwaMgr) {
    HwaccelManager_Impl* pImpl = dynamic_cast<HwaccelManager_Impl*>(pHwaMgr);
    delete pImpl;
};

const function<bool(const HwaccelManager_Impl::DeviceInfo_Internal& a, const HwaccelManager_Impl::DeviceInfo_Internal& b)>
HwaccelManager_Impl::DEVICE_INFO_COMPARATOR = [] (auto& a, auto& b)
{
    return a.dynamicPriority < b.dynamicPriority;
};

HwaccelManager::Holder HwaccelManager::CreateInstance()
{
    return HwaccelManager::Holder(new HwaccelManager_Impl(), HWACCEL_MANAGER_DELETER);
}

static HwaccelManager::Holder _DEFAULT_HWACCEL_MANAGER;
static mutex _DEFAULT_HWACCEL_MANAGER_ACCESS_LOCK;

HwaccelManager::Holder HwaccelManager::GetDefaultInstance()
{
    lock_guard<mutex> lk(_DEFAULT_HWACCEL_MANAGER_ACCESS_LOCK);
    if (!_DEFAULT_HWACCEL_MANAGER)
        _DEFAULT_HWACCEL_MANAGER = HwaccelManager::CreateInstance();
    return _DEFAULT_HWACCEL_MANAGER;
}
}
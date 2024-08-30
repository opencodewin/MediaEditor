#pragma once

#include <cstdint>
#include <memory>
#include <utility>
#include <string>
#include <thread>
#include <ostream>
#include "imgui.h"
#include "immat.h"
#include "MatUtilsVecTypeDef.h"
#include "MediaCore.h"
#include "Logger.h"

namespace RenderUtils
{
struct ManagedTexture
{
    using Holder = std::shared_ptr<ManagedTexture>;

    virtual ImTextureID TextureID() const = 0;
    virtual MatUtils::Rectf GetDisplayRoi() const = 0;
    virtual MatUtils::Size2i GetDisplaySize() const = 0;
    virtual bool IsValid() const = 0;
    virtual void Invalidate() = 0;
    virtual bool RenderMatToTexture(const ImGui::ImMat& vmat) = 0;

    virtual std::string GetError() const = 0;
};

struct TextureManager
{
    using Holder = std::shared_ptr<TextureManager>;
    static MEDIACORE_API Holder CreateInstance();
    static MEDIACORE_API Holder GetDefaultInstance();
    static MEDIACORE_API void ReleaseDefaultInstance();

    virtual ManagedTexture::Holder CreateManagedTextureFromMat(const ImGui::ImMat& vmat, MatUtils::Size2i& textureSize, ImDataType dataType = IM_DT_INT8) = 0;
    virtual bool CreateTexturePool(const std::string& name, const MatUtils::Size2i& textureSize, ImDataType dataType, uint32_t minPoolSize, uint32_t maxPoolSize = 0) = 0;
    virtual ManagedTexture::Holder GetTextureFromPool(const std::string& poolName) = 0;
    virtual bool CreateGridTexturePool(const std::string& name, const MatUtils::Size2i& textureSize, ImDataType dataType, const MatUtils::Size2i& gridSize, uint32_t minPoolSize, uint32_t maxPoolSize = 0) = 0;
    virtual ManagedTexture::Holder GetGridTextureFromPool(const std::string& poolName) = 0;
    virtual bool ReleaseTexturePool(const std::string& name) = 0;

    virtual void SetUiThread(const std::thread::id& threadId) = 0;
    virtual bool UpdateTextureState() = 0;  // run this method in UI thread
    virtual void Release() = 0;
    virtual bool IsTextureFrom(const std::string& poolName, ManagedTexture::Holder hTx) = 0;

    struct TexturePoolAttributes
    {
        MatUtils::Size2i tTxSize;
        ImDataType eTxDtype;
        bool bKeepAspectRatio;
    };
    virtual bool GetTexturePoolAttributes(const std::string& poolName, TexturePoolAttributes& tTxPoolAttrs) = 0;
    virtual bool SetTexturePoolAttributes(const std::string& poolName, const TexturePoolAttributes& tTxPoolAttrs) = 0;

    virtual std::string GetError() const = 0;
    virtual void SetLogLevel(Logger::Level l) = 0;

};
MEDIACORE_API std::ostream& operator<<(std::ostream& os, const TextureManager* pTxMgr);
}
#include <cstdlib>
#include <cstring>
#include <list>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <iomanip>
#include <functional>
#include <algorithm>
#include <mutex>
#include <atomic>
#include <typeinfo>
#include <cassert>
#include "TextureManager.h"
#if IMGUI_VULKAN_SHADER
#include "Resize_vulkan.h"
#include "warpAffine_vulkan.h"
#endif
#include "imgui_helper.h"

using namespace std;
using namespace MatUtils;
using namespace Logger;

namespace RenderUtils
{
struct _TextureContainer
{
    using Holder = shared_ptr<_TextureContainer>;
    virtual const string& GetName() const = 0;
    virtual void Release() = 0;
    virtual bool NeedRelease() const = 0;
    virtual ManagedTexture::Holder GetFreeTexture() = 0;
    virtual void UpdateTextureState() = 0;
    virtual bool RequestTextureID(ManagedTexture* pMtx) = 0;
    virtual TextureManager::TexturePoolAttributes GetAttributes() const = 0;
    virtual bool SetAttributes(const TextureManager::TexturePoolAttributes& tTxPoolAttrs) = 0;
    virtual bool HasTexture(ManagedTexture::Holder hTx) = 0;
};

class TextureManager_Impl : public TextureManager
{
private:
    struct ManagedTexture_Impl : public ManagedTexture
    {
        ManagedTexture_Impl(TextureManager_Impl* owner, _TextureContainer* container, const Size2i& textureSize, const Size2i& roiSize, ImDataType dataType, bool bKeepAspectRatio)
            : m_owner(owner), m_container(container), m_textureSize(textureSize), m_roiSize(roiSize), m_roiRect({0,0}, textureSize), m_dataType(dataType), m_bKeepAspectRatio(bKeepAspectRatio)
        {}
        virtual ~ManagedTexture_Impl() {}

        ImTextureID TextureID() const override { return m_tid; }

        Rectf GetDisplayRoi() const override
        {
            Vec2<float> xRoi = m_textureSize.x > 0 ? Vec2<float>(m_roiRect.leftTop.x, m_roiRect.size.x)/(float)m_textureSize.x : Vec2<float>(0, 1);
            Vec2<float> yRoi = m_textureSize.y > 0 ? Vec2<float>(m_roiRect.leftTop.y, m_roiRect.size.y)/(float)m_textureSize.y : Vec2<float>(0, 1);
            return Rectf(xRoi.x, yRoi.x, xRoi.y, yRoi.y);
        }

        Size2i GetDisplaySize() const override { return m_roiSize; }

        bool IsValid() const override { return m_valid; }

        bool IsDiscarded() const { return m_discarded; }

        void Discard()
        {
            if (!m_discarded)
            {
                Invalidate();
                m_renderMat.release();
                m_discarded = true;
            }
        }

        void Reuse() { m_discarded = false; }

        void Invalidate() override
        {
            if (m_valid)
            {
                m_valid = false;
                m_owner->m_validTxCount--;
            }
        }

        bool ReleaseTexture()
        {
            if (m_valid)
            {
                m_valid = false;
                m_owner->m_validTxCount--;
            }
            bool txDestroyed = false;
            if (m_tid)
            {
                if (m_ownTx)
                {
                    ImGui::ImDestroyTexture(&m_tid);
                    m_owner->m_txCount--;
                    m_owner->m_logger->Log(VERBOSE) << "Destroyed texture in container '" << m_container->GetName() << "'." << endl;
                    m_ownTx = false;
                }
                txDestroyed = true;
            }
            return txDestroyed;
        }

        bool RenderMatToTexture(const ImGui::ImMat& vmat) override
        {
            if (vmat.empty())
            {
                m_owner->m_errMsg = "Input 'vmat' is empty!";
                return false;
            }
            if (vmat.color_format != IM_CF_ABGR)
            {
                m_owner->m_errMsg = "Can only support 'vmat' with color format as 'ABGR'!";
                return false;
            }

            ImGui::ImMat renderMat = vmat;
#if IMGUI_VULKAN_SHADER
            const auto& roiSize = m_roiSize;
            if ((roiSize.x > 0 && roiSize.x != vmat.w) || (roiSize.y > 0 && roiSize.y != vmat.h) || vmat.type != m_dataType)
            {
                if (m_bKeepAspectRatio)
                {
                    if (vmat.w != m_iPrevWarpInputW || vmat.h != m_iPrevWarpInputH)
                    {
                        m_bNeedWarpAffine = CalcWarpAffineMatrix({vmat.w, vmat.h}, roiSize, m_tWarpMatrix);
                        m_iPrevWarpInputW = vmat.w;
                        m_iPrevWarpInputH = vmat.h;
                    }
                    if (m_bNeedWarpAffine)
                    {
                        ImGui::ImMat tWarppedMat;
                        tWarppedMat.type = m_dataType;
                        tWarppedMat.w = roiSize.x; tWarppedMat.h = roiSize.y;
                        m_owner->m_warpAffine.warp(vmat, tWarppedMat, m_tWarpMatrix, IM_INTERPOLATE_NEAREST, ImPixel(0.f, 0.f, 0.f, 1.f));
                        renderMat = tWarppedMat;
                    }
                }
                else
                {
                    ImGui::ImMat rszMat;
                    rszMat.type = m_dataType;
                    ImInterpolateMode interpMode = IM_INTERPOLATE_BICUBIC;
                    if (roiSize.x*roiSize.y < (int32_t)vmat.w*(int32_t)vmat.h)
                        interpMode = IM_INTERPOLATE_AREA;
                    rszMat.w = roiSize.x; rszMat.h = roiSize.y;
                    m_owner->m_scaler.Resize(vmat, rszMat, 0, 0, interpMode);
                    if (rszMat.empty())
                    {
                        ostringstream oss; oss << "FAILED to resize input 'vmat'(" << vmat.w << "x" << vmat.h << ") to texture size(" << roiSize.x << "," << roiSize.y << ")!";
                        m_owner->m_errMsg = oss.str();
                        return false;
                    }
                    renderMat = rszMat;
                }
            }
#endif
            m_roiSize = {renderMat.w, renderMat.h};

            if (this_thread::get_id() != m_owner->m_uiThreadId)
            {
                if (m_valid)
                {
                    m_valid = false;
                    m_owner->m_validTxCount--;
                }
                m_renderMat = renderMat;
                return true;
            }

            m_renderMat = renderMat;
            return DoRender();
        }

        // this method is invoked from the ui thread
        bool DoRender()
        {
            bool createNewTx = !m_tid;
            bool ownTx = true;
            if (createNewTx)
            {
                if (!m_container->RequestTextureID(this))
                {
                    m_owner->m_logger->Log(Error) << "FAILED to invoke 'RequestTextureID()'! In container '" << m_container->GetName() << "'." << endl;
                    return false;
                }
                if (m_tid)
                    ownTx = false;
            }
            static const Size2i _ORIGIN_POIN(0, 0);
            const int w = m_renderMat.w, h = m_renderMat.h, c = m_renderMat.c;
            if (m_roiRect.leftTop == _ORIGIN_POIN && m_roiRect.size == m_textureSize)
            {
                ImGui::ImGenerateOrUpdateTexture(m_tid, w, h, c, reinterpret_cast<const unsigned char*>(&m_renderMat), true);
                if (!m_tid)
                {
                    m_owner->m_errMsg = "FAILED to render ImMat to texture by 'ImGenerateOrUpdateTexture()'!";
                    if (m_valid)
                    {
                        m_valid = false;
                        m_owner->m_validTxCount--;
                    }
                    return false;
                }
            }
            else
            {
                if (!m_tid)
                {
                    m_owner->m_errMsg = "No texture id prepared for 'ImCopyToTexture()'!";
                    if (m_valid)
                    {
                        m_valid = false;
                        m_owner->m_validTxCount--;
                    }
                    return false;
                }
                // render mat to the roi rectangle, in this case the actual texture is created somewhere else in previous
                ImGui::ImCopyToTexture(m_tid, reinterpret_cast<unsigned char*>(&m_renderMat), w, h, c, m_roiRect.leftTop.x, m_roiRect.leftTop.y, true);
            }
            m_renderMat.release();
            if (createNewTx)
            {
                m_owner->m_logger->Log(VERBOSE) << "Created new texture of size (" << w << "x" << h << "x" << c << "), resided in container '" << m_container->GetName() << "'." << endl;
                m_ownTx = ownTx;
                if (ownTx)
                    m_owner->m_txCount++;
                m_owner->m_logicTxCount++;
            }
            if (!m_valid)
            {
                m_valid = true;
                m_owner->m_validTxCount++;
            }
            return true;
        }

        bool CalcWarpAffineMatrix(const MatUtils::Size2i& tInputSize, const MatUtils::Size2i& tTxSize, ImGui::ImMat& tWarpMatrix)
        {
            if (tInputSize.x == tTxSize.x && tInputSize.y == tTxSize.y)
                return false;

            int renderWidth, renderHeight;
            if (tTxSize.x*tInputSize.y > tTxSize.y*tInputSize.x)
            {
                renderWidth = tTxSize.y*tInputSize.x/tInputSize.y;
                renderHeight = tTxSize.y;
            }
            else
            {
                renderWidth = tTxSize.x;
                renderHeight = tTxSize.x*tInputSize.y/tInputSize.x;
            }
            const float fScaleH = (float)renderWidth/tInputSize.x;
            const float fScaleV = (float)renderHeight/tInputSize.y;
            const float _x_scale = 1.f / (fScaleH + FLT_EPSILON);
            const float _y_scale = 1.f / (fScaleV + FLT_EPSILON);
            const float _angle = 0.f;
            const float alpha_00 = _x_scale;
            const float alpha_11 = _y_scale;
            const float beta_01 = 0.f;
            const float beta_10 = 0.f;
            const float x_diff = (float)tTxSize.x - (float)tInputSize.x;
            const float y_diff = (float)tTxSize.y - (float)tInputSize.y;
            const float _x_offset = x_diff / 2.f;
            const float _y_offset = y_diff / 2.f;
            const int center_x = tInputSize.x / 2.f + _x_offset;
            const int center_y = tInputSize.y / 2.f + _y_offset;

            tWarpMatrix.create_type(3, 2, IM_DT_FLOAT32);
            memset(tWarpMatrix.data, 0, tWarpMatrix.elemsize*tWarpMatrix.total());
            tWarpMatrix.at<float>(0, 0) =  alpha_00;
            tWarpMatrix.at<float>(1, 0) = beta_01;
            tWarpMatrix.at<float>(2, 0) = (1 - alpha_00) * center_x - beta_01 * center_y - _x_offset;
            tWarpMatrix.at<float>(0, 1) = -beta_10;
            tWarpMatrix.at<float>(1, 1) = alpha_11;
            tWarpMatrix.at<float>(2, 1) = beta_10 * center_x + (1 - alpha_11) * center_y - _y_offset;
            return true;
        }

        string GetError() const override { return m_owner->GetError(); }

        TextureManager_Impl* m_owner;
        _TextureContainer* m_container;
        ImTextureID m_tid{nullptr};
        bool m_ownTx{false};
        bool m_valid{false};
        bool m_discarded{false};
        Size2i m_textureSize;
        Size2i m_roiSize;
        Recti m_roiRect;
        ImDataType m_dataType;
        bool m_bKeepAspectRatio;
        bool m_bNeedWarpAffine;
        int m_iPrevWarpInputW{0}, m_iPrevWarpInputH{0};
        ImGui::ImMat m_tWarpMatrix;
        ImGui::ImMat m_renderMat;
    };
    static const function<void(ManagedTexture*)> MANAGED_TEXTURE_DELETER;

    // SingleTextureContainer
    struct SingleTextureContainer : public _TextureContainer
    {
        SingleTextureContainer(TextureManager_Impl* owner, ManagedTexture::Holder hTx)
            : m_owner(owner), m_hTx(hTx)
        {
            m_pTx = dynamic_cast<ManagedTexture_Impl*>(m_hTx.get());
            ostringstream oss; oss << "0x" << setw(16) << setfill('0') << hex << m_pTx;
            m_name = oss.str();
        }

        virtual ~SingleTextureContainer() {}

        const string& GetName() const override { return m_name; }

        void Release() override
        {
            if (m_hTx)
            {
                if (m_pTx->ReleaseTexture())
                    m_owner->m_logicTxCount--;
                m_pTx = nullptr;
                m_hTx = nullptr;
            }
        }

        bool NeedRelease() const override { return m_needRelease; }

        ManagedTexture::Holder GetFreeTexture() override
        {
            ManagedTexture::Holder hTx;
            {
                lock_guard<mutex> lk(m_txLock);
                hTx = m_hTx;
            }
            return hTx;
        }

        void UpdateTextureState() override
        {
            {
                lock_guard<mutex> lk(m_txLock);
                if (m_hTx.use_count() == 1)
                {
                    if (m_pTx->ReleaseTexture())
                        m_owner->m_logicTxCount--;
                    m_hTx = nullptr;
                    m_pTx = nullptr;
                    m_needRelease = true;
                }
            }
            if (!m_needRelease)
            {
                ManagedTexture_Impl* const pTx = m_pTx;
                if (!pTx->m_renderMat.empty())
                    pTx->DoRender();
            }
        }

        bool RequestTextureID(ManagedTexture* pMtx) override { return true; }
        TexturePoolAttributes GetAttributes() const override { return { m_hTx->GetDisplaySize(), IM_DT_UNDEFINED, false }; }
        bool SetAttributes(const TexturePoolAttributes& tTxPoolAttrs) override { return false; }
        bool HasTexture(ManagedTexture::Holder hTx) override { return m_hTx == hTx; }

        TextureManager_Impl* m_owner;
        string m_name;
        ManagedTexture::Holder m_hTx;
        ManagedTexture_Impl* m_pTx;
        mutex m_txLock;
        bool m_needRelease{false};
    };
    static const function<void(_TextureContainer*)> SINGLE_TEXTURE_CONTAINER_DELETER;

    // TexturePoolContainer
    struct TexturePoolContainer : public _TextureContainer
    {
        TexturePoolContainer(TextureManager_Impl* owner, const string& name, const Size2i& textureSize, ImDataType dataType, uint32_t minPoolSize, uint32_t maxPoolSize)
            : m_owner(owner), m_name(name), m_textureSize(textureSize), m_dataType(dataType), m_minPoolSize(minPoolSize), m_maxPoolSize(maxPoolSize)
        {}

        virtual ~TexturePoolContainer() {}

        const string& GetName() const override { return m_name; }

        void Release() override
        {
            for (auto& hTx : m_txPool)
            {
                ManagedTexture_Impl* pTx = dynamic_cast<ManagedTexture_Impl*>(hTx.get());
                if (pTx->ReleaseTexture())
                    m_owner->m_logicTxCount--;
            }
            m_txPool.clear();
        }

        bool NeedRelease() const override { return m_needRelease; }

        ManagedTexture::Holder GetFreeTexture() override
        {
            if (m_needRelease) return nullptr;
            lock_guard<mutex> lk(m_txPoolLock);
            auto iter = find_if(m_txPool.begin(), m_txPool.end(), [] (auto& hTx) {
                ManagedTexture_Impl* pTx = dynamic_cast<ManagedTexture_Impl*>(hTx.get());
                return pTx->IsDiscarded();
            });
            if (iter != m_txPool.end())
            {
                ManagedTexture_Impl* pTx = dynamic_cast<ManagedTexture_Impl*>(iter->get());
                pTx->Reuse();
                return *iter;  // found a invalidated texture in the pool
            }
            if (m_maxPoolSize > 0 && m_txPool.size() >= (size_t)m_maxPoolSize) // pool is full
            {
                m_owner->m_logger->Log(WARN) << "! The count of pooled textures has reached the limitation " << m_maxPoolSize << "!" << endl;
                return nullptr;
            }

            // create new ManagedTexture
            ManagedTexture_Impl* pTx = new ManagedTexture_Impl(m_owner, this, m_textureSize, m_textureSize, m_dataType, m_bKeepAspectRatio);
            ManagedTexture::Holder hTx(pTx, MANAGED_TEXTURE_DELETER);
            m_txPool.push_back(hTx);
            return hTx;
        }

        void UpdateTextureState() override
        {
            list<ManagedTexture::Holder> releaseList;
            list<ManagedTexture::Holder> renderList;
            {
                lock_guard<mutex> lk(m_txPoolLock);
                // remove over min-threshold unused textures
                uint32_t removeCap = m_txPool.size()>m_minPoolSize ? m_txPool.size()-m_minPoolSize : 0;
                auto iter = m_txPool.begin();
                while (iter != m_txPool.end())
                {
                    auto& hTx = *iter;
                    ManagedTexture_Impl* pTx = dynamic_cast<ManagedTexture_Impl*>(hTx.get());
                    if (hTx.use_count() == 1)
                    {
                        if (removeCap > 0)
                        {
                            releaseList.push_back(hTx);
                            iter = m_txPool.erase(iter);
                            removeCap--;
                            continue;
                        }
                        else
                        {
                            pTx->Discard();
                            iter++;
                            continue;
                        }
                    }

                    if (!pTx->m_renderMat.empty())
                        renderList.push_back(hTx);
                    iter++;
                }
            }
            // release textures
            for (auto& hTx : releaseList)
            {
                ManagedTexture_Impl* pTx = dynamic_cast<ManagedTexture_Impl*>(hTx.get());
                if (pTx->ReleaseTexture())
                    m_owner->m_logicTxCount--;
            }
            // render textures
            for (auto& hTx : renderList)
            {
                ManagedTexture_Impl* pTx = dynamic_cast<ManagedTexture_Impl*>(hTx.get());
                pTx->DoRender();
            }
        }

        bool RequestTextureID(ManagedTexture* pMtx) override { return true; }
        TexturePoolAttributes GetAttributes() const override { return { m_textureSize, m_dataType, m_bKeepAspectRatio }; }

        bool SetAttributes(const TexturePoolAttributes& tTxPoolAttrs) override
        {
            m_textureSize = tTxPoolAttrs.tTxSize;
            m_dataType = tTxPoolAttrs.eTxDtype;
            m_bKeepAspectRatio = tTxPoolAttrs.bKeepAspectRatio;
            return true;
        }

        bool HasTexture(ManagedTexture::Holder hTx) override
        {
            bool found = false;
            lock_guard<mutex> lk(m_txPoolLock);
            auto iter = find(m_txPool.begin(), m_txPool.end(), hTx);
            return iter != m_txPool.end();
        }

        TextureManager_Impl* m_owner;
        string m_name;
        Size2i m_textureSize;
        ImDataType m_dataType;
        bool m_bKeepAspectRatio{false};
        list<ManagedTexture::Holder> m_txPool;
        mutex m_txPoolLock;
        uint32_t m_minPoolSize, m_maxPoolSize;
        bool m_needRelease{false};
    };
    static const function<void(_TextureContainer*)> TEXTURE_POOL_CONTAINER_DELETER;

    // GridTexturePoolContainer
    struct GridTexturePoolContainer : public _TextureContainer
    {
        struct GridTexture
        {
            GridTexture(const Size2i& textureSize, int32_t channel, int32_t bitDepth, int32_t gridCap)
            {
                auto bytesPerPixel = channel*bitDepth/8;
                size_t buffSize = textureSize.x*textureSize.y*bytesPerPixel;
                void* pBuff = malloc(buffSize);
                memset(pBuff, 0, buffSize);
                m_tid = ImGui::ImCreateTexture(pBuff, textureSize.x, textureSize.y, channel, NAN, bitDepth);
                free(pBuff);
                if (m_tid)
                {
                    m_txs.reserve(gridCap);
                }
            }

            bool Release()
            {
                bool textureDestroyed = false;
                if (m_tid)
                {
                    ImGui::ImDestroyTexture(&m_tid);
                    textureDestroyed = true;
                }
                return textureDestroyed;
            }

            ImTextureID m_tid{nullptr};
            vector<ManagedTexture::Holder> m_txs;
        };

        GridTexturePoolContainer(TextureManager_Impl* owner, const string& name, const Size2i& textureSize, ImDataType dataType, const Size2i& gridSize, uint32_t minPoolSize, uint32_t maxPoolSize)
            : m_owner(owner), m_name(name), m_textureSize(textureSize), m_dataType(dataType), m_gridSize(gridSize), m_minPoolSize(minPoolSize), m_maxPoolSize(maxPoolSize)
        {
            m_bitDepth = dataType==IM_DT_INT8 ? 8 : 32;  // either IM_DT_INT8 or IM_DT_FLOAT32
            m_gridTxSize = { textureSize.x*gridSize.x, textureSize.y*gridSize.y };
            m_gridCap = gridSize.x*gridSize.y;
            m_maxTxCnt = m_gridCap*m_maxPoolSize;
        }

        virtual ~GridTexturePoolContainer() {}

        const string& GetName() const override { return m_name; }

        void Release() override
        {
            for (auto& hTx : m_txPool)
            {
                ManagedTexture_Impl* pTx = dynamic_cast<ManagedTexture_Impl*>(hTx.get());
                if (pTx->ReleaseTexture())
                    m_owner->m_logicTxCount--;
            }
            m_txPool.clear();
            for (auto pGtx : m_gridTxPool)
            {
                if (pGtx->Release())
                {
                    m_owner->m_txCount--;
                    m_owner->m_logger->Log(VERBOSE) << "Destroyed texture in container '" << m_name << "'." << endl;
                }
                delete pGtx;
            }
            m_gridTxPool.clear();
        }

        bool NeedRelease() const override { return m_needRelease; }

        // this api may not be invoked from the ui thread
        ManagedTexture::Holder GetFreeTexture() override
        {
            if (m_needRelease) return nullptr;
            lock_guard<mutex> lk(m_txPoolLock);
            auto iter = find_if(m_txPool.begin(), m_txPool.end(), [] (auto& hTx) {
                ManagedTexture_Impl* pTx = dynamic_cast<ManagedTexture_Impl*>(hTx.get());
                return pTx->IsDiscarded();
            });
            if (iter != m_txPool.end())
            {
                ManagedTexture_Impl* pTx = dynamic_cast<ManagedTexture_Impl*>(iter->get());
                pTx->Reuse();
                return *iter;  // found a invalidated texture in the pool
            }
            if (m_maxTxCnt > 0 && m_gridTxPool.size() >= (size_t)m_maxTxCnt) // pool is full
            {
                m_owner->m_logger->Log(WARN) << "! The count of pooled grid textures has reached the limitation " << m_maxTxCnt << "="
                        << m_maxPoolSize << "(maxPoolSize)x(" << m_gridSize.x << "x" << m_gridSize.y << ")(gridSize)!" << endl;
                return nullptr;
            }

            // create new ManagedTexture
            ManagedTexture_Impl* pTx = new ManagedTexture_Impl(m_owner, this, m_gridTxSize, m_textureSize, m_dataType, m_bKeepAspectRatio);
            ManagedTexture::Holder hTx(pTx, MANAGED_TEXTURE_DELETER);
            m_txPool.push_back(hTx);
            return hTx;
        }

        void UpdateTextureState() override
        {
            // release over min-threshold count grid textures
            if (m_gridTxPool.size() > m_minPoolSize)
            {
                uint32_t delCap = m_gridTxPool.size()-m_minPoolSize;
                auto delIter = m_gridTxPool.begin();
                while (delIter != m_gridTxPool.end() && delCap > 0)
                {
                    auto pGtx = *delIter;
                    bool unused = true;
                    for (auto& hTx : pGtx->m_txs)
                    {
                        ManagedTexture_Impl* pTx = dynamic_cast<ManagedTexture_Impl*>(hTx.get());
                        if (hTx.use_count() > 2)
                            unused = false;
                        else if (!pTx->IsDiscarded())
                        {
                            pTx->Discard();
                        }
                    }
                    if (unused)
                    {
                        // remove GridTexture::m_txs from m_txPool
                        {
                            lock_guard<mutex> lk(m_txPoolLock);
                            for (auto& hTx : pGtx->m_txs)
                            {
                                m_txPool.remove(hTx);
                                ManagedTexture_Impl* pTx = dynamic_cast<ManagedTexture_Impl*>(hTx.get());
                                if (pTx->ReleaseTexture())
                                    m_owner->m_logicTxCount--;
                            }
                        }
                        pGtx->m_txs.clear();
                        if (pGtx->Release())
                        {
                            m_owner->m_txCount--;
                            m_owner->m_logger->Log(VERBOSE) << "Destroyed texture in container '" << m_name << "'." << endl;
                        }
                        delete pGtx;
                        delIter = m_gridTxPool.erase(delIter);
                        delCap--;
                    }
                    else
                    {
                        delIter++;
                    }
                }
            }

            list<ManagedTexture::Holder> renderList;
            {
                lock_guard<mutex> lk(m_txPoolLock);
                auto iter = m_txPool.begin();
                while (iter != m_txPool.end())
                {
                    auto& hTx = *iter++;
                    ManagedTexture_Impl* pTx = dynamic_cast<ManagedTexture_Impl*>(hTx.get());
                    if (!pTx->m_renderMat.empty())
                        renderList.push_back(hTx);
                }
            }
            // render textures
            for (auto& hTx : renderList)
            {
                ManagedTexture_Impl* pTx = dynamic_cast<ManagedTexture_Impl*>(hTx.get());
                pTx->DoRender();
            }
        }

        bool RequestTextureID(ManagedTexture* pMtx) override
        {
            ManagedTexture_Impl* pTx = dynamic_cast<ManagedTexture_Impl*>(pMtx);
            auto searchIter = find_if(m_txPool.begin(), m_txPool.end(), [pTx] (auto& hTx) {
                return hTx.get() == pTx;
            });
            if (searchIter == m_txPool.end())
                return false;
            ManagedTexture::Holder hTx = *searchIter;
            GridTexture* pNonFullGtx = nullptr;
            auto iter = find_if(m_gridTxPool.begin(), m_gridTxPool.end(), [this] (auto pGtx) {
                return pGtx->m_txs.size() < m_gridCap;
            });
            if (iter == m_gridTxPool.end())
            {
                GridTexture* pNewGtx = new GridTexture(m_gridTxSize, 4, m_bitDepth, m_gridCap);
                if (pNewGtx->m_tid)
                    m_owner->m_txCount++;
                else
                {
                    delete pNewGtx;
                    return false;
                }
                m_gridTxPool.push_back(pNewGtx);
                pNonFullGtx = pNewGtx;
            }
            else
            {
                pNonFullGtx = *iter;
            }

            int32_t gridIndex = pNonFullGtx->m_txs.size();
            int32_t gridX = gridIndex%m_gridSize.x;
            int32_t gridY = gridIndex/m_gridSize.y;
            pTx->m_roiRect.leftTop = { gridX*m_textureSize.x, gridY*m_textureSize.y };
            pTx->m_roiRect.size = m_textureSize;
            pTx->m_tid = pNonFullGtx->m_tid;
            pNonFullGtx->m_txs.push_back(hTx);
            return true;
        }

        TexturePoolAttributes GetAttributes() const override { return {m_textureSize, m_dataType, m_bKeepAspectRatio}; }

        bool SetAttributes(const TexturePoolAttributes& tTxPoolAttrs) override
        {
            if (m_textureSize != tTxPoolAttrs.tTxSize || m_dataType != tTxPoolAttrs.eTxDtype)
                return false;
            m_bKeepAspectRatio = tTxPoolAttrs.bKeepAspectRatio;
            return true;
        }

        bool HasTexture(ManagedTexture::Holder hTx) override
        {
            bool found = false;
            lock_guard<mutex> lk(m_txPoolLock);
            auto iter = find(m_txPool.begin(), m_txPool.end(), hTx);
            return iter != m_txPool.end();
        }

        TextureManager_Impl* m_owner;
        string m_name;
        Size2i m_textureSize;
        ImDataType m_dataType;
        bool m_bKeepAspectRatio{false};
        int32_t m_bitDepth;
        Size2i m_gridSize;
        int32_t m_gridCap;
        Size2i m_gridTxSize;
        list<ManagedTexture::Holder> m_txPool;
        mutex m_txPoolLock;
        list<GridTexture*> m_gridTxPool;
        uint32_t m_minPoolSize, m_maxPoolSize;
        uint32_t m_maxTxCnt;
        bool m_needRelease{false};
    };
    static const function<void(_TextureContainer*)> GRID_TEXTURE_POOL_CONTAINER_DELETER;

public:
    TextureManager_Impl()
    {
        m_logger = GetLogger("TxMgr");
        m_uiThreadId = this_thread::get_id();
    }

    virtual ~TextureManager_Impl()
    {
        m_containers.clear();
    }

    ManagedTexture::Holder CreateManagedTextureFromMat(const ImGui::ImMat& vmat, Size2i& textureSize, ImDataType dataType) override
    {
        if (vmat.empty())
        {
            m_errMsg = "Input 'vmat' is empty!";
            return nullptr;
        }
        if (vmat.color_format != IM_CF_ABGR)
        {
            m_errMsg = "Can only support 'vmat' with color format as 'ABGR'!";
            return nullptr;
        }
        if (dataType != IM_DT_INT8 && dataType != IM_DT_FLOAT32)
        {
            m_errMsg = "Only support 'vmat' with data type as 'INT8' or 'FLOAT32'!";
            return nullptr;
        }
        if (textureSize.x <= 0) textureSize.x = vmat.w;
        if (textureSize.y <= 0) textureSize.y = vmat.h;

        ManagedTexture_Impl* pTx = new ManagedTexture_Impl(this, nullptr, textureSize, textureSize, dataType, false);
        ManagedTexture::Holder hTx(pTx, MANAGED_TEXTURE_DELETER);
        SingleTextureContainer* pCont = new SingleTextureContainer(this, hTx);
        pTx->m_container = static_cast<_TextureContainer*>(pCont);
        _TextureContainer::Holder hCont(pCont, SINGLE_TEXTURE_CONTAINER_DELETER);
        {
            lock_guard<mutex> lk(m_containersLock);
            m_containers[pCont->GetName()] = hCont;
        }

        if (!pTx->RenderMatToTexture(vmat))
            return nullptr;
        return hTx;
    }

    bool CreateTexturePool(const string& name, const Size2i& textureSize, ImDataType dataType, uint32_t minPoolSize, uint32_t maxPoolSize) override
    {
        if (name.empty())
        {
            m_errMsg = "INVALID argument 'name', it can not be empty string!";
            return false;
        }
        if (textureSize.x < 0 || textureSize.y < 0)
        {
            m_errMsg = "INVALID argument 'textureSize', 'x' and 'y' CANNOT be negative value!";
            return false;
        }
        if (dataType != IM_DT_INT8 && dataType != IM_DT_FLOAT32)
        {
            m_errMsg = "Only support 'ImMat' instance with data type as 'INT8' or 'FLOAT32'!";
            return false;
        }
        if (maxPoolSize > 0 && minPoolSize > maxPoolSize)
        {
            m_errMsg = "INVALID argument, 'minPoolSize' CANNOT be larger than 'maxPoolSize'!";
            return false;
        }
        lock_guard<mutex> lk(m_containersLock);
        auto iter = m_containers.find(name);
        if (iter != m_containers.end())
        {
            ostringstream oss; oss << "There is already an existing container with name '" << name << "'!";
            m_errMsg = oss.str();
            return false;
        }

        _TextureContainer::Holder hCont(new TexturePoolContainer(this, name, textureSize, dataType, minPoolSize, maxPoolSize), TEXTURE_POOL_CONTAINER_DELETER);
        m_containers[name] = hCont;
        return true;
    }

    ManagedTexture::Holder GetTextureFromPool(const string& poolName) override
    {
        lock_guard<mutex> lk(m_containersLock);
        auto iter = m_containers.find(poolName);
        TexturePoolContainer* pCont = nullptr;
        if (iter != m_containers.end())
            pCont = dynamic_cast<TexturePoolContainer*>(iter->second.get());
        if (!pCont)
        {
            ostringstream oss; oss << "CANNOT find any texture pool with name '" << poolName << "'!";
            m_errMsg = oss.str();
            return nullptr;
        }
        return iter->second->GetFreeTexture();
    }

    bool CreateGridTexturePool(const string& name, const Size2i& textureSize, ImDataType dataType, const Size2i& gridSize, uint32_t minPoolSize, uint32_t maxPoolSize) override
    {
        if (name.empty())
        {
            m_errMsg = "INVALID argument 'name', it can not be empty string!";
            return false;
        }
        if (textureSize.x <= 0 || textureSize.y <= 0)
        {
            m_errMsg = "INVALID argument 'textureSize', 'x' and 'y' CANNOT be non-positive value!";
            return false;
        }
        if (dataType != IM_DT_INT8 && dataType != IM_DT_FLOAT32)
        {
            m_errMsg = "Only support 'ImMat' instance with data type as 'INT8' or 'FLOAT32'!";
            return false;
        }
        if (gridSize.x <= 0 || gridSize.y <= 0)
        {
            m_errMsg = "INVALID argument 'gridSize', 'x' and 'y' CANNOT be non-positive value!";
            return false;
        }
        if (maxPoolSize > 0 && minPoolSize > maxPoolSize)
        {
            m_errMsg = "INVALID argument, 'minPoolSize' CANNOT be larger than 'maxPoolSize'!";
            return false;
        }
        lock_guard<mutex> lk(m_containersLock);
        auto iter = m_containers.find(name);
        if (iter != m_containers.end())
        {
            ostringstream oss; oss << "There is already an existing container with name '" << name << "'!";
            m_errMsg = oss.str();
            return false;
        }

        _TextureContainer::Holder hCont(new GridTexturePoolContainer(this, name, textureSize, dataType, gridSize, minPoolSize, maxPoolSize), GRID_TEXTURE_POOL_CONTAINER_DELETER);
        m_containers[name] = hCont;
        return true;
    }

    ManagedTexture::Holder GetGridTextureFromPool(const string& poolName) override
    {
        lock_guard<mutex> lk(m_containersLock);
        auto iter = m_containers.find(poolName);
        GridTexturePoolContainer* pCont = nullptr;
        if (iter != m_containers.end())
            pCont = dynamic_cast<GridTexturePoolContainer*>(iter->second.get());
        if (!pCont)
        {
            ostringstream oss; oss << "CANNOT find any grid texture pool with name '" << poolName << "'!";
            m_errMsg = oss.str();
            return nullptr;
        }
        return pCont->GetFreeTexture();
    }

    bool ReleaseTexturePool(const std::string& name) override
    {
        if (name.empty())
        {
            m_errMsg = "INVALID argument 'name', it can not be empty string!";
            return false;
        }
        lock_guard<mutex> lk(m_containersLock);
        auto iter = m_containers.find(name);
        if (iter == m_containers.end())
        {
            ostringstream oss; oss << "There is no container with name '" << name << "'!";
            m_errMsg = oss.str();
            return false;
        }

        auto& hCont = iter->second;
        hCont->Release();
        m_containers.erase(iter);
        return true;
    }

    bool GetTexturePoolAttributes(const string& poolName, TexturePoolAttributes& tTxPoolAttrs) override
    {
        lock_guard<mutex> lk(m_containersLock);
        auto iter = m_containers.find(poolName);
        if (iter == m_containers.end())
        {
            ostringstream oss; oss << "CANNOT find any grid texture pool with name '" << poolName << "'!";
            m_errMsg = oss.str();
            return false;
        }
        auto& hCont = iter->second;
        tTxPoolAttrs = hCont->GetAttributes();
        return true;
    }

    bool SetTexturePoolAttributes(const string& poolName, const TexturePoolAttributes& tTxPoolAttrs) override
    {
        lock_guard<mutex> lk(m_containersLock);
        auto iter = m_containers.find(poolName);
        if (iter == m_containers.end())
        {
            ostringstream oss; oss << "CANNOT find any grid texture pool with name '" << poolName << "'!";
            m_errMsg = oss.str();
            return false;
        }
        auto& hCont = iter->second;
        return hCont->SetAttributes(tTxPoolAttrs);
        return true;
    }

    bool IsTextureFrom(const std::string& poolName, ManagedTexture::Holder hTx) override
    {
        lock_guard<mutex> lk(m_containersLock);
        if (!poolName.empty())
        {
            auto iter = m_containers.find(poolName);
            if (iter == m_containers.end())
            {
                ostringstream oss; oss << "CANNOT find any texture container with name '" << poolName << "'!";
                m_errMsg = oss.str();
                return false;
            }
            return iter->second->HasTexture(hTx);
        }
        else
        {
            bool found = false;
            for (auto& elem : m_containers)
            {
                if (elem.second->HasTexture(hTx))
                {
                    found = true;
                    break;
                }
            }
            return found;
        }
    }

    void SetUiThread(const thread::id& threadId) override { m_uiThreadId = threadId; }

    bool UpdateTextureState() override
    {
        unordered_map<string, _TextureContainer::Holder> containers;
        {
            lock_guard<mutex> lk(m_containersLock);
            auto iter = m_containers.begin();
            while (iter != m_containers.end())
            {
                auto& hCont = iter->second;
                if (hCont->NeedRelease())
                {
                    hCont->Release();
                    iter = m_containers.erase(iter);
                }
                else
                {
                    iter++;
                }
            }
            containers = m_containers;
        }
        for (auto& elem : containers)
        {
            auto& hCont = elem.second;
            hCont->UpdateTextureState();
        }
        return true;
    }

    void Release() override
    {
        lock_guard<mutex> lk(m_containersLock);
        auto iter = m_containers.begin();
        while (iter != m_containers.end())
        {
            auto& hCont = iter->second;
            hCont->Release();
            iter = m_containers.erase(iter);
        }
    }

    string GetError() const override { return m_errMsg; }
    void SetLogLevel(Logger::Level l) override { m_logger->SetShowLevels(l); }

public:
    static const function<void(TextureManager*)> TEXTURE_MANAGER_DELETER;

private:
    unordered_map<string, _TextureContainer::Holder> m_containers;
    mutex m_containersLock;
    atomic_int32_t m_txCount{0};
    atomic_int32_t m_logicTxCount{0};
    atomic_int32_t m_validTxCount{0};
    thread::id m_uiThreadId;
#if IMGUI_VULKAN_SHADER
    ImGui::Resize_vulkan m_scaler;
    ImGui::warpAffine_vulkan m_warpAffine;
#endif
    string m_errMsg;
    ALogger* m_logger;

    friend ostream& operator<<(ostream& os, const TextureManager* pTxMgr);
};

const function<void(ManagedTexture*)> TextureManager_Impl::MANAGED_TEXTURE_DELETER = [] (ManagedTexture* p) {
    ManagedTexture_Impl* ptr = dynamic_cast<ManagedTexture_Impl*>(p);
    delete ptr;
};
const function<void(_TextureContainer*)> TextureManager_Impl::SINGLE_TEXTURE_CONTAINER_DELETER = [] (_TextureContainer* p) {
    TextureManager_Impl::SingleTextureContainer* ptr = dynamic_cast<TextureManager_Impl::SingleTextureContainer*>(p);
    delete ptr;
};
const function<void(_TextureContainer*)> TextureManager_Impl::TEXTURE_POOL_CONTAINER_DELETER = [] (_TextureContainer* p) {
    TextureManager_Impl::TexturePoolContainer* ptr = dynamic_cast<TextureManager_Impl::TexturePoolContainer*>(p);
    delete ptr;
};
const function<void(_TextureContainer*)> TextureManager_Impl::GRID_TEXTURE_POOL_CONTAINER_DELETER = [] (_TextureContainer* p) {
    TextureManager_Impl::GridTexturePoolContainer* ptr = dynamic_cast<TextureManager_Impl::GridTexturePoolContainer*>(p);
    delete ptr;
};
const function<void(TextureManager*)> TextureManager_Impl::TEXTURE_MANAGER_DELETER = [] (TextureManager* p) {
    TextureManager_Impl* ptr = dynamic_cast<TextureManager_Impl*>(p);
    ptr->Release();
    delete ptr;
};

TextureManager::Holder TextureManager::CreateInstance()
{
    return TextureManager::Holder(new TextureManager_Impl(), TextureManager_Impl::TEXTURE_MANAGER_DELETER);
}

ostream& operator<<(ostream& os, const TextureManager* pTxMgr)
{
    const TextureManager_Impl* pTxMgrImpl = dynamic_cast<const TextureManager_Impl*>(pTxMgr);
    if (pTxMgrImpl)
    {
        os << "Total tx: " << pTxMgrImpl->m_txCount << ", logic tx: " << pTxMgrImpl->m_logicTxCount << ", valid tx: " << pTxMgrImpl->m_validTxCount << ".";
    }
    else
    {
        os << "(not a 'TextureManager_Impl' instance)";
    }
    return os;
}

static TextureManager::Holder g_defaultTxMgr;
static mutex g_defaultTxMgrLock;
TextureManager::Holder TextureManager::GetDefaultInstance()
{
    lock_guard<mutex> lk(g_defaultTxMgrLock);
    if (!g_defaultTxMgr)
        g_defaultTxMgr = CreateInstance();
    return g_defaultTxMgr;
}

void TextureManager::ReleaseDefaultInstance()
{
    lock_guard<mutex> lk(g_defaultTxMgrLock);
    if (g_defaultTxMgr)
    {
        g_defaultTxMgr->Release();
        g_defaultTxMgr = nullptr;
    }
}
}

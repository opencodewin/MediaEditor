#pragma once
#include <imgui.h>
#include <imgui_internal.h>
#include <immat.h>

namespace ImGui {

IMGUI_API void ImGenerateOrUpdateTexture(ImTextureID& imtexid, int width, int height, int channels, const unsigned char* pixels, bool useMipmapsIfPossible, bool wraps, bool wrapt, bool minFilterNearest = false, bool magFilterNearest=false, bool is_immat=false);
IMGUI_API void ImGenerateOrUpdateTexture(ImTextureID& imtexid,int width, int height, int channels, const unsigned char* pixels, bool is_immat = false);
IMGUI_API ImTextureID ImCreateTexture(const void* data, int width, int height, int channels, double time_stamp = NAN, int bit_depth = 8);
IMGUI_API ImTextureID ImLoadTexture(const char* path);
IMGUI_API ImTextureID ImLoadTexture(const unsigned int * data, size_t size);
IMGUI_API void ImLoadImageToMat(const char* path, ImMat& mat, bool gray = false);
IMGUI_API void ImDestroyTexture(ImTextureID* texture_ptr);
IMGUI_API int ImGetTextureWidth(ImTextureID texture);
IMGUI_API int ImGetTextureHeight(ImTextureID texture);
IMGUI_API int ImGetTextureData(ImTextureID texture, void* data);
IMGUI_API ImPixel ImGetTexturePixel(ImTextureID texture, float x, float y);
IMGUI_API double ImGetTextureTimeStamp(ImTextureID texture);
IMGUI_API bool ImTextureToFile(ImTextureID texture, std::string path);
IMGUI_API bool ImMatToFile(const ImMat& mat, std::string path);
IMGUI_API void ImMatToTexture(const ImMat& mat, ImTextureID& texture);
IMGUI_API void ImTextureToMat(ImTextureID texture, ImMat& mat, ImVec2 offset = {}, ImVec2 size = {});
IMGUI_API void ImCopyToTexture(ImTextureID& imtexid, unsigned char* pixels, int width, int height, int channels, int offset_x, int offset_y, bool is_immat=false);
#if IMGUI_RENDERING_VULKAN && IMGUI_VULKAN_SHADER
//IMGUI_API ImTextureID ImCreateTexture(VkImageMat & image, double time_stamp = NAN);
#endif
IMGUI_API void ImUpdateTextures(); // update internal textures, check need destroy texture and destroy it if we can
IMGUI_API void ImDestroyTextures(); // clean internal textures
IMGUI_API size_t ImGetTextureCount();

IMGUI_API void ImShowVideoWindow(ImDrawList *draw_list, ImTextureID texture, ImVec2 pos, ImVec2 size, float zoom_size = 256.f, ImU32 back_color = IM_COL32_BLACK, int short_key = 0, float* offset_x = nullptr, float* offset_y = nullptr, float* tf_x = nullptr, float* tf_y = nullptr, bool bLandscape = true, bool out_border = false, const ImVec2& uvMin = ImVec2(0, 0), const ImVec2& uvMax = ImVec2(1, 1));
IMGUI_API void ImShowVideoWindowCompare(ImDrawList *draw_list, ImTextureID texture1, ImTextureID texture2, ImVec2 pos, ImVec2 size, float& split, bool horizontal = true, float zoom_size = 256.f, float* offset_x = nullptr, float* offset_y = nullptr, float* tf_x = nullptr, float* tf_y = nullptr, bool bLandscape = true, bool out_border = false);
} // namespace ImGui
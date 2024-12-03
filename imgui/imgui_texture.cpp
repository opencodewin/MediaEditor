#include "imgui_texture.h"
#include "imgui_helper.h"
#include <thread>
//#if IMGUI_VULKAN_SHADER
//#include "ImVulkanShader.h"
//#endif
#if IMGUI_TIFF
#include <tiffio.h>
#endif

#if IMGUI_RENDERING_MATAL
#ifdef IMGUI_OPENGL
#undef IMGUI_OPENGL
#define IMGUI_OPENGL 0
#endif
#endif

#if IMGUI_OPENGL
#if defined(IMGUI_IMPL_OPENGL_ES2) || defined(__EMSCRIPTEN__)
#ifndef IMGUI_IMPL_OPENGL_ES2
#define IMGUI_IMPL_OPENGL_ES2
#endif
#include <GLES2/gl2.h>
#if defined(__EMSCRIPTEN__)
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif
#include <GLES2/gl2ext.h>
#endif
// About Desktop OpenGL function loaders:
//  Modern desktop OpenGL doesn't have a standard portable header file to load OpenGL function pointers.
//  Helper libraries are often used for this purpose! Here we are supporting a few common ones (gl3w, glew, glad).
//  You may use another loader/header of your choice (glext, glLoadGen, etc.), or chose to manually implement your own.
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
#include <GL/gl3w.h>            // Initialize with gl3wInit()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
#include <GL/glew.h>            // Initialize with glewInit()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
#include <glad/glad.h>          // Initialize with gladLoadGL()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD2)
#include <glad/gl.h>            // Initialize with gladLoadGL(...) or gladLoaderLoadGL()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING2)
#define GLFW_INCLUDE_NONE       // GLFW including OpenGL headers causes ambiguity or multiple definition errors.
#include <glbinding/Binding.h>  // Initialize with glbinding::Binding::initialize()
#include <glbinding/gl/gl.h>
using namespace gl;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING3)
#define GLFW_INCLUDE_NONE       // GLFW including OpenGL headers causes ambiguity or multiple definition errors.
#include <glbinding/glbinding.h>// Initialize with glbinding::initialize()
#include <glbinding/gl/gl.h>
using namespace gl;
#else
#include IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#endif
#if IMGUI_RENDERING_GL3
#include <imgui_impl_opengl3.h>
#elif IMGUI_RENDERING_GL2
#include <imgui_impl_opengl2.h>
#endif
#endif

#if IMGUI_RENDERING_VULKAN
#include <imgui_impl_vulkan.h>
#endif

#if IMGUI_RENDERING_VULKAN
struct ImTexture
{
    ImTextureVk TextureID = nullptr;
    int     Width     = 0;
    int     Height    = 0;
    double  TimeStamp = NAN;
    std::thread::id CreateThread;
    bool NeedDestroy  = false;
};
#elif IMGUI_RENDERING_DX11
#include <imgui_impl_dx11.h>
struct ImTexture
{
    ID3D11ShaderResourceView * TextureID = nullptr;
    int    Width     = 0;
    int    Height    = 0;
    double  TimeStamp = NAN;
    std::thread::id CreateThread;
    bool NeedDestroy  = false;
};
#elif IMGUI_RENDERING_DX9
#include <imgui_impl_dx9.h>
struct ImTexture
{
    LPDIRECT3DTEXTURE9 TextureID = nullptr;
    int    Width     = 0;
    int    Height    = 0;
    double  TimeStamp = NAN;
    std::thread::id CreateThread;
    bool NeedDestroy  = false;
};
#elif IMGUI_OPENGL
struct ImTexture
{
    ImTextureGl TextureID = nullptr;
    int    Width     = 0;
    int    Height    = 0;
    double  TimeStamp = NAN;
    std::thread::id CreateThread;
    bool NeedDestroy  = false;
};
#else
struct ImTexture
{
    int    TextureID = -1;
    int    Width     = 0;
    int    Height    = 0;
    double  TimeStamp = NAN;
    std::thread::id CreateThread;
    bool NeedDestroy  = false;
};
#endif

namespace ImGui {
static std::vector<ImTexture> g_Textures;
std::mutex g_tex_mutex;

void ImGenerateOrUpdateTexture(ImTextureID& imtexid,int width,int height,int channels,const unsigned char* pixels,bool useMipmapsIfPossible,bool wraps,bool wrapt,bool minFilterNearest,bool magFilterNearest,bool is_immat)
{
    IM_ASSERT(pixels);
    IM_ASSERT(channels>0 && channels<=4);
    unsigned char* data = nullptr;
#if IMGUI_RENDERING_VULKAN
    VkBuffer buffer {nullptr};
    size_t offset {0};
    bool is_vulkan = false;
    int bit_depth = 8;
    if (is_immat)
    {
        ImGui::ImMat* mat = (ImGui::ImMat*)pixels;
        if (mat->empty())
            return;
        bit_depth = mat->depth;
#if IMGUI_VULKAN_SHADER
        if (mat->device == IM_DD_VULKAN)
        {
            ImGui::VkMat* vkmat = (ImGui::VkMat*)mat;
            buffer = vkmat->buffer();
            offset = vkmat->buffer_offset();
            if (!buffer)
                return;
            is_vulkan = true;
        }
        else 
#endif
        if (mat->device == IM_DD_CPU)
        {
            data = (unsigned char *)mat->data;
            is_vulkan = false;
        }
    }
    else
        data = (unsigned char *)pixels;
    if (!is_vulkan && !data)
        return;
    if (imtexid == 0)
    {
        // TODO::Dicky Need deal with 3 channels Image(link RGB / BGR) and 1 channel (Gray)
        g_tex_mutex.lock();
        g_Textures.resize(g_Textures.size() + 1);
        ImTexture& texture = g_Textures.back();
        if (is_vulkan)
            texture.TextureID = (ImTextureVk)ImGui_ImplVulkan_CreateTexture(buffer, offset, width, height, channels, bit_depth);
        else
            texture.TextureID = (ImTextureVk)ImGui_ImplVulkan_CreateTexture(data, width, height, channels, bit_depth);
        if (!texture.TextureID)
        {
            g_Textures.pop_back();
            g_tex_mutex.unlock();
            return;
        }
        texture.CreateThread = std::this_thread::get_id();
        texture.NeedDestroy = false;
        texture.Width  = width;
        texture.Height = height;
        imtexid = texture.TextureID;
        g_tex_mutex.unlock();
        return;
    }
#if IMGUI_VULKAN_SHADER
    if (is_vulkan)
        ImGui_ImplVulkan_UpdateTexture(imtexid, buffer, offset, width, height, channels, bit_depth);
    else
#endif
        ImGui_ImplVulkan_UpdateTexture(imtexid, data, width, height, channels, bit_depth);
#elif IMGUI_RENDERING_DX11
    auto textureID = (ID3D11ShaderResourceView *)imtexid;
    if (textureID)
    {
        textureID->Release();
        textureID = nullptr;
    }
    imtexid = ImCreateTexture(pixels, width, height, channels);
#elif IMGUI_RENDERING_DX9
    LPDIRECT3DDEVICE9 pd3dDevice = (LPDIRECT3DDEVICE9)ImGui_ImplDX9_GetDevice();
    if (!pd3dDevice) return;
    LPDIRECT3DTEXTURE9& texid = reinterpret_cast<LPDIRECT3DTEXTURE9&>(imtexid);
    if (texid==0 && pd3dDevice->CreateTexture(width, height, useMipmapsIfPossible ? 0 : 1, 0, channels==1 ? D3DFMT_A8 : channels==2 ? D3DFMT_A8L8 : channels==3 ? D3DFMT_R8G8B8 : D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texid, NULL) < 0) return;

    D3DLOCKED_RECT tex_locked_rect;
    if (texid->LockRect(0, &tex_locked_rect, NULL, 0) != D3D_OK) {texid->Release();texid=0;return;}
    if (channels==3 || channels==4) {
        unsigned char* pw;
        const unsigned char* ppxl = pixels;
        for (int y = 0; y < height; y++)    {
            pw = (unsigned char *)tex_locked_rect.pBits + tex_locked_rect.Pitch * y;  // each row has Pitch bytes
            ppxl = &pixels[y*width*channels];
            for( int x = 0; x < width; x++ )
            {
                *pw++ = ppxl[2];
                *pw++ = ppxl[1];
                *pw++ = ppxl[0];
                if (channels==4) *pw++ = ppxl[3];
                ppxl+=channels;
            }
        }
    }
    else {
        for (int y = 0; y < height; y++)    {
            memcpy((unsigned char *)tex_locked_rect.pBits + tex_locked_rect.Pitch * y, pixels + (width * channels) * y, (width * channels));
        }
    }
    texid->UnlockRect(0);
#elif IMGUI_OPENGL
    glEnable(GL_TEXTURE_2D);
    GLint last_texture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);

    if (imtexid == 0)
    {
        g_tex_mutex.lock();
        g_Textures.resize(g_Textures.size() + 1);
        ImTexture& texture = g_Textures.back();
        texture.TextureID = new ImTextureGL("GLTexture");
        glGenTextures(1, &texture.TextureID->gID);
        texture.CreateThread = std::this_thread::get_id();
        texture.NeedDestroy = false;
        texture.Width  = width;
        texture.Height = height;
        imtexid = texture.TextureID;
        g_tex_mutex.unlock();
    }

    auto textureID = (ImTextureGL *)imtexid;

    glBindTexture(GL_TEXTURE_2D, textureID->gID);

    GLenum clampEnum = 0x2900;    // 0x2900 -> GL_CLAMP; 0x812F -> GL_CLAMP_TO_EDGE
#   ifndef GL_CLAMP
#       ifdef GL_CLAMP_TO_EDGE
        clampEnum = GL_CLAMP_TO_EDGE;
#       else //GL_CLAMP_TO_EDGE
        clampEnum = 0x812F;
#       endif // GL_CLAMP_TO_EDGE
#   else //GL_CLAMP
    clampEnum = GL_CLAMP;
#   endif //GL_CLAMP

    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,wraps ? GL_REPEAT : clampEnum);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,wrapt ? GL_REPEAT : clampEnum);
    //const GLfloat borderColor[]={0.f,0.f,0.f,1.f};glTexParameterfv(GL_TEXTURE_2D,GL_TEXTURE_BORDER_COLOR,borderColor);
    if (magFilterNearest) glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    else glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    if (useMipmapsIfPossible)   {
#       ifdef NO_IMGUI_OPENGL_GLGENERATEMIPMAP
#           ifndef GL_GENERATE_MIPMAP
#               define GL_GENERATE_MIPMAP 0x8191
#           endif //GL_GENERATE_MIPMAP
        // I guess this is compilable, even if it's not supported:
        glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);    // This call must be done before glTexImage2D(...) // GL_GENERATE_MIPMAP can't be used with NPOT if there are not supported by the hardware of GL_ARB_texture_non_power_of_two.
#       endif //NO_IMGUI_OPENGL_GLGENERATEMIPMAP
    }
    if (minFilterNearest) glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, useMipmapsIfPossible ? GL_LINEAR_MIPMAP_NEAREST : GL_NEAREST);
    else glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, useMipmapsIfPossible ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);

    //glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    //glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    GLenum luminanceAlphaEnum = 0x190A; // 0x190A -> GL_LUMINANCE_ALPHA [Note that we're FORCING this definition even if when it's not defined! What should we use for 2 channels?]
    GLenum compressedLuminanceAlphaEnum = 0x84EB; // 0x84EB -> GL_COMPRESSED_LUMINANCE_ALPHA [Note that we're FORCING this definition even if when it's not defined! What should we use for 2 channels?]
#   ifdef GL_LUMINANCE_ALPHA
    luminanceAlphaEnum = GL_LUMINANCE_ALPHA;
#   endif //GL_LUMINANCE_ALPHA
#   ifdef GL_COMPRESSED_LUMINANCE_ALPHA
    compressedLuminanceAlphaEnum = GL_COMPRESSED_LUMINANCE_ALPHA;
#   endif //GL_COMPRESSED_LUMINANCE_ALPHA

#   ifdef IMIMPL_USE_ARB_TEXTURE_SWIZZLE_TO_SAVE_FONT_TEXTURE_MEMORY
    if (&imtexid==&gImImplPrivateParams.fontTex && channels==1) {
        GLint swizzleMask[] = {GL_ONE, GL_ONE, GL_ONE, GL_ALPHA};
        glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
        //printf("IMIMPL_USE_ARB_TEXTURE_SWIZZLE_TO_SAVE_FONT_TEXTURE_MEMORY used.\n");
    }
#   endif //IMIMPL_USE_ARB_TEXTURE_SWIZZLE_TO_SAVE_FONT_TEXTURE_MEMORY
# ifdef __APPLE__
    GLenum grayFormat = GL_RED;
#else
    GLenum grayFormat = GL_LUMINANCE;
#endif
    GLenum fmt = channels==1 ? grayFormat : channels==2 ? luminanceAlphaEnum : channels==3 ? GL_RGB : GL_RGBA;  // channels == 1 could be GL_LUMINANCE, GL_ALPHA, GL_RED ...
    GLenum ifmt = GL_RGBA;//fmt;
#   ifdef IMIMPL_USE_ARB_TEXTURE_COMPRESSION_TO_COMPRESS_FONT_TEXTURE
    if (&imtexid==&gImImplPrivateParams.fontTex)    {
        ifmt = channels==1 ? GL_COMPRESSED_ALPHA : channels==2 ? compressedLuminanceAlphaEnum : channels==3 ? GL_COMPRESSED_RGB : GL_COMPRESSED_RGBA;  // channels == 1 could be GL_COMPRESSED_LUMINANCE, GL_COMPRESSED_ALPHA, GL_COMPRESSED_RED ...
    }
#   endif //IMIMPL_USE_ARB_TEXTURE_COMPRESSION_TO_COMPRESS_FONT_TEXTURE

    if (is_immat)
    {
        ImGui::ImMat *mat = (ImGui::ImMat*)pixels;
//#if IMGUI_VULKAN_SHADER
//        if (mat->device == IM_DD_VULKAN)
//        {
//            ImGui::VkMat * vkmat = (ImGui::VkMat*)mat;
//            if (!vkmat->empty())
//            {
//                auto data = ImGui::ImVulkanVkMatMapping(*vkmat);
//                if (data) glTexImage2D(GL_TEXTURE_2D, 0, ifmt, width, height, 0, fmt, mat->type == IM_DT_FLOAT32 ? GL_FLOAT : mat->type == IM_DT_INT16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_BYTE, data);
//            }
//        }
//        else
//#endif
        if (mat->device == IM_DD_CPU)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, ifmt, width, height, 0, fmt, mat->type == IM_DT_FLOAT32 ? GL_FLOAT : mat->type == IM_DT_INT16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_BYTE, mat->data);
        }
    }
    else
        glTexImage2D(GL_TEXTURE_2D, 0, ifmt, width, height, 0, fmt, GL_UNSIGNED_BYTE, pixels);

#   ifdef IMIMPL_USE_ARB_TEXTURE_COMPRESSION_TO_COMPRESS_FONT_TEXTURE
    if (&imtexid==&gImImplPrivateParams.fontTex)    {
        GLint compressed = GL_FALSE;
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED, &compressed);
        if (compressed==GL_FALSE)
            printf("Font texture compressed = %s\n",compressed==GL_TRUE?"true":"false");
    }
#   endif //IMIMPL_USE_ARB_TEXTURE_COMPRESSION_TO_COMPRESS_FONT_TEXTURE

#   ifndef NO_IMGUI_OPENGL_GLGENERATEMIPMAP
    if (useMipmapsIfPossible) glGenerateMipmap(GL_TEXTURE_2D);
#   endif //NO_IMGUI_OPENGL_GLGENERATEMIPMAP
    glBindTexture(GL_TEXTURE_2D, last_texture);
#endif
    //fprintf(stderr, "[ImTexture]:%lu\n", g_Textures.size());
}

void ImGenerateOrUpdateTexture(ImTextureID& imtexid,int width, int height, int channels, const unsigned char* pixels, bool is_immat)
{
    ImGenerateOrUpdateTexture(imtexid, width, height, channels, pixels,false,false,false,false,false,is_immat);
}

void ImCopyToTexture(ImTextureID& imtexid, unsigned char* pixels, int width, int height, int channels, int offset_x, int offset_y, bool is_immat)
{
    IM_ASSERT(imtexid);
    IM_ASSERT(pixels);
    IM_ASSERT(channels>0 && channels<=4);
    auto texture_width = ImGetTextureWidth(imtexid);
    auto texture_height = ImGetTextureHeight(imtexid);
    if (offset_x < 0 || offset_y < 0 ||
        offset_x + width > texture_width ||
        offset_y + height > texture_height)
        return;
#if IMGUI_RENDERING_VULKAN

    bool is_vulkan = false;
    int bit_depth = 8;
    VkBuffer buffer {nullptr};
    size_t offset {0};
    unsigned char* data = nullptr;
    if (is_immat)
    {
        ImGui::ImMat* mat = (ImGui::ImMat*)pixels;
        if (mat->empty())
            return;
        bit_depth = mat->depth;
#if IMGUI_VULKAN_SHADER
        if (mat->device == IM_DD_VULKAN)
        {
            ImGui::VkMat* vkmat = (ImGui::VkMat*)mat;
            buffer = vkmat->buffer();
            offset = vkmat->buffer_offset();
            if (!buffer)
                return;
            is_vulkan = true;
        }
        else 
#endif
        if (mat->device == IM_DD_CPU)
        {
            data = (unsigned char *)mat->data;
            is_vulkan = false;
        }
    }
    if (!is_vulkan && !data)
        return;
#if IMGUI_VULKAN_SHADER
    if (is_vulkan)
        ImGui_ImplVulkan_UpdateTexture(imtexid, buffer, offset, width, height, channels, bit_depth, offset_x, offset_y);
    else
#endif
        ImGui_ImplVulkan_UpdateTexture(imtexid, data, width, height, channels, bit_depth, offset_x, offset_y);
#elif IMGUI_RENDERING_DX11
#elif IMGUI_RENDERING_DX9
#elif IMGUI_OPENGL
    glEnable(GL_TEXTURE_2D);
    GLint last_texture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);

    auto textureID = (ImTextureGL *)imtexid;
    glBindTexture(GL_TEXTURE_2D, textureID->gID);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);

    GLenum luminanceAlphaEnum = 0x190A; // 0x190A -> GL_LUMINANCE_ALPHA [Note that we're FORCING this definition even if when it's not defined! What should we use for 2 channels?]
    GLenum compressedLuminanceAlphaEnum = 0x84EB; // 0x84EB -> GL_COMPRESSED_LUMINANCE_ALPHA [Note that we're FORCING this definition even if when it's not defined! What should we use for 2 channels?]
#   ifdef GL_LUMINANCE_ALPHA
    luminanceAlphaEnum = GL_LUMINANCE_ALPHA;
#   endif //GL_LUMINANCE_ALPHA
#   ifdef GL_COMPRESSED_LUMINANCE_ALPHA
    compressedLuminanceAlphaEnum = GL_COMPRESSED_LUMINANCE_ALPHA;
#   endif //GL_COMPRESSED_LUMINANCE_ALPHA

#   ifdef IMIMPL_USE_ARB_TEXTURE_SWIZZLE_TO_SAVE_FONT_TEXTURE_MEMORY
    if (&imtexid==&gImImplPrivateParams.fontTex && channels==1) {
        GLint swizzleMask[] = {GL_ONE, GL_ONE, GL_ONE, GL_ALPHA};
        glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
        //printf("IMIMPL_USE_ARB_TEXTURE_SWIZZLE_TO_SAVE_FONT_TEXTURE_MEMORY used.\n");
    }
#   endif //IMIMPL_USE_ARB_TEXTURE_SWIZZLE_TO_SAVE_FONT_TEXTURE_MEMORY
    GLenum fmt = channels==1 ? GL_ALPHA : channels==2 ? luminanceAlphaEnum : channels==3 ? GL_RGB : GL_RGBA;  // channels == 1 could be GL_LUMINANCE, GL_ALPHA, GL_RED ...
    GLenum ifmt = GL_RGBA;

    if (is_immat)
    {
        ImGui::ImMat *mat = (ImGui::ImMat*)pixels;
        auto src_format = mat->type == IM_DT_FLOAT32 ? GL_FLOAT : GL_UNSIGNED_BYTE;
//#if IMGUI_VULKAN_SHADER
//        if (mat->device == IM_DD_VULKAN)
//        {
//            ImGui::VkMat * vkmat = (ImGui::VkMat*)mat;
//            if (!vkmat->empty())
//            {
//                auto data = ImGui::ImVulkanVkMatMapping(*vkmat);
//                if (data) glTexSubImage2D(GL_TEXTURE_2D, 0, offset_x, offset_y, width, height, fmt, src_format, data);
//            }
//        }
//        else
//#endif
        if (mat->device == IM_DD_CPU)
        {
            glTexSubImage2D(GL_TEXTURE_2D, 0, offset_x, offset_y, width, height, fmt, src_format, mat->data);
        }
    }
    else
        glTexSubImage2D(GL_TEXTURE_2D, 0, offset_x, offset_y, width, height, fmt, GL_UNSIGNED_BYTE, pixels);

    glBindTexture(GL_TEXTURE_2D, last_texture);
#endif
}

ImTextureID ImCreateTexture(const void* data, int width, int height, int channels, double time_stamp, int bit_depth)
{
#if IMGUI_RENDERING_VULKAN
    g_tex_mutex.lock();
    g_Textures.resize(g_Textures.size() + 1);
    ImTexture& texture = g_Textures.back();
    texture.TextureID = (ImTextureVk)ImGui_ImplVulkan_CreateTexture(data, width, height, channels, bit_depth);
    if (!texture.TextureID)
    {
        g_Textures.pop_back();
        g_tex_mutex.unlock();
        return (ImTextureID)nullptr;
    }
    texture.CreateThread = std::this_thread::get_id();
    texture.NeedDestroy = false;
    texture.Width  = width;
    texture.Height = height;
    texture.TimeStamp = time_stamp;
    g_tex_mutex.unlock();
    return (ImTextureID)texture.TextureID;
#elif IMGUI_RENDERING_DX11
    ID3D11Device* pd3dDevice = (ID3D11Device*)ImGui_ImplDX11_GetDevice();
    if (!pd3dDevice)
        return nullptr;
    g_tex_mutex.lock();
    g_Textures.resize(g_Textures.size() + 1);
    ImTexture& texture = g_Textures.back();

    // Create texture
    D3D11_TEXTURE2D_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;

    ID3D11Texture2D *pTexture = NULL;
    D3D11_SUBRESOURCE_DATA subResource;
    subResource.pSysMem = data;
    subResource.SysMemPitch = desc.Width * 4;
    subResource.SysMemSlicePitch = 0;
    pd3dDevice->CreateTexture2D(&desc, &subResource, &pTexture);

    // Create texture view
    //ID3D11ShaderResourceView * texture = nullptr;
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    ZeroMemory(&srvDesc, sizeof(srvDesc));
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = desc.MipLevels;
    srvDesc.Texture2D.MostDetailedMip = 0;
    pd3dDevice->CreateShaderResourceView(pTexture, &srvDesc, &texture.TextureID);
    pTexture->Release();
    texture.CreateThread = std::this_thread::get_id();
    texture.NeedDestroy = false;
    texture.Width  = width;
    texture.Height = height;
    texture.TimeStamp = time_stamp;
    g_tex_mutex.unlock();
    return (ImTextureID)texture.TextureID;
#elif IMGUI_RENDERING_DX9
    LPDIRECT3DDEVICE9 pd3dDevice = (LPDIRECT3DDEVICE9)ImGui_ImplDX9_GetDevice();
    if (!pd3dDevice)
        return nullptr;
    g_tex_mutex.lock();
    g_Textures.resize(g_Textures.size() + 1);
    ImTexture& texture = g_Textures.back();
    if (pd3dDevice->CreateTexture(width, height, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &texture.TextureID, NULL) < 0)
    {
        g_tex_mutex.unlock();
        return nullptr;
    }
    D3DLOCKED_RECT tex_locked_rect;
    int bytes_per_pixel = 4;
    if (texture.TextureID->LockRect(0, &tex_locked_rect, NULL, 0) != D3D_OK)
    {
        g_tex_mutex.unlock();
        return nullptr;
    }
    for (int y = 0; y < height; y++)
        memcpy((unsigned char*)tex_locked_rect.pBits + tex_locked_rect.Pitch * y, (unsigned char* )data + (width * bytes_per_pixel) * y, (width * bytes_per_pixel));
    texture.TextureID->UnlockRect(0);
    texture.CreateThread = std::this_thread::get_id();
    texture.NeedDestroy = false;
    texture.Width  = width;
    texture.Height = height;
    texture.TimeStamp = time_stamp;
    g_tex_mutex.unlock();
    return (ImTextureID)texture.TextureID;
#elif IMGUI_OPENGL
    g_tex_mutex.lock();
    g_Textures.resize(g_Textures.size() + 1);
    ImTexture& texture = g_Textures.back();
    texture.TextureID = new ImTextureGL("GLTexture");
    // Upload texture to graphics system
    GLint last_texture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
    glGenTextures(1, &texture.TextureID->gID);
    glBindTexture(GL_TEXTURE_2D, texture.TextureID->gID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glBindTexture(GL_TEXTURE_2D, last_texture);

    texture.CreateThread = std::this_thread::get_id();
    texture.NeedDestroy = false;
    texture.Width  = width;
    texture.Height = height;
    texture.TimeStamp = time_stamp;
    g_tex_mutex.unlock();
    return (ImTextureID)texture.TextureID;
#else
    return nullptr;
#endif
}

static std::vector<ImTexture>::iterator ImFindTexture(ImTextureID texture)
{
#if IMGUI_RENDERING_VULKAN
    auto textureID = reinterpret_cast<ImTextureVk>(texture);
#elif IMGUI_RENDERING_DX11
    auto textureID = (ID3D11ShaderResourceView *)texture;
#elif IMGUI_RENDERING_DX9
    auto textureID = reinterpret_cast<LPDIRECT3DTEXTURE9>(texture);
#elif IMGUI_OPENGL
    auto textureID = reinterpret_cast<ImTextureGl>(texture);
#else
    int textureID = -1;
#endif
    return std::find_if(g_Textures.begin(), g_Textures.end(), [textureID](ImTexture& texture)
    {
        return texture.TextureID == textureID;
    });
}

static void destroy_texture(ImTexture* tex)
{
#if IMGUI_RENDERING_VULKAN
    if (tex->TextureID)
    {
        ImGui_ImplVulkan_DestroyTexture(&tex->TextureID);
        tex->TextureID = nullptr;
    }
#elif IMGUI_RENDERING_DX11
    if (tex->TextureID)
    {
        tex->TextureID->Release();
        tex->TextureID = nullptr;
    }
#elif IMGUI_RENDERING_DX9
    if (tex->TextureID)
    {
        tex->TextureID->Release();
        tex->TextureID = nullptr;
    }
#elif IMGUI_OPENGL
    if (tex->TextureID)
    {
        glDeleteTextures(1, &tex->TextureID->gID);
        delete tex->TextureID;
        tex->TextureID = nullptr;
    }
#endif
}

void ImDestroyTexture(ImTextureID* texture_ptr)
{
    //fprintf(stderr, "[Destroy ImTexture]:%lu\n", g_Textures.size());
    if (!texture_ptr || !*texture_ptr) return;
    g_tex_mutex.lock();
    auto textureIt = ImFindTexture(*texture_ptr);
    if (textureIt == g_Textures.end())
    {
        g_tex_mutex.unlock();
        return;
    }
    if (textureIt->CreateThread != std::this_thread::get_id())
    {
        textureIt->NeedDestroy = true;
        g_tex_mutex.unlock();
        return;
    }
    destroy_texture(&(*textureIt));
    g_Textures.erase(textureIt);
    g_tex_mutex.unlock();
    *texture_ptr = nullptr;
}

void ImDestroyTextures()
{
    //fprintf(stderr, "[remain ImTexture]:%lu\n", g_Textures.size());
    g_tex_mutex.lock();
    for (auto iter = g_Textures.begin(); iter != g_Textures.end(); iter++)
    {
        destroy_texture(&(*iter));
    }
    g_Textures.clear();
    g_tex_mutex.unlock();
}

void ImUpdateTextures()
{
    g_tex_mutex.lock();
    for (auto iter = g_Textures.begin(); iter != g_Textures.end();)
    {
        if (!iter->NeedDestroy)
        {
            iter++;
        }
        else if (iter->CreateThread != std::this_thread::get_id())
        {
            iter++;
        }
        else
        {
            //fprintf(stderr, "[Update ImTexture delete]:%lu\n", g_Textures.size());
            destroy_texture(&(*iter));
            iter = g_Textures.erase(iter);
        }
    }
    g_tex_mutex.unlock();
    //fprintf(stderr, "[Update ImTexture]:%lu\n", g_Textures.size());
}

size_t ImGetTextureCount()
{
    return g_Textures.size();
}

int ImGetTextureWidth(ImTextureID texture)
{
    auto textureIt = ImFindTexture(texture);
    if (textureIt != g_Textures.end())
        return textureIt->Width;
    return 0;
}

int ImGetTextureHeight(ImTextureID texture)
{
    auto textureIt = ImFindTexture(texture);
    if (textureIt != g_Textures.end())
        return textureIt->Height;
    return 0;
}

double ImGetTextureTimeStamp(ImTextureID texture)
{
    auto textureIt = ImFindTexture(texture);
    if (textureIt != g_Textures.end())
        return textureIt->TimeStamp;
    return NAN;
}

ImTextureID ImLoadTexture(const char* path)
{
    int width = 0, height = 0, component = 0;
    if (auto data = stbi_load(path, &width, &height, &component, 4))
    {
        auto texture = ImCreateTexture(data, width, height, 4 /*component*/);
        stbi_image_free(data);
        return texture;
    }
    else
        return nullptr;
}

ImTextureID ImLoadTexture(const unsigned int * data, size_t size)
{
    int width = 0, height = 0, component = 0;
    if (auto img_data = stbi_load_from_memory((const stbi_uc *)data, size, &width, &height, &component, 4))
    {
        auto texture = ImCreateTexture(img_data, width, height, 4 /*component*/);
        stbi_image_free(img_data);
        return texture;
    }
    else
        return nullptr;
}

void ImLoadImageToMat(const char* path, ImMat& mat, bool gray)
{
    auto file_suffix = ImGuiHelper::path_filename_suffix(path);
    if (file_suffix.compare(".tiff") == 0 || file_suffix.compare(".TIFF") == 0)
    {
#if IMGUI_TIFF
        TIFF* tif = TIFFOpen(path, "r");
        if (tif)
        {
            uint32_t w, h, c, imagelength;
            uint16_t depth, config;
            TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
            TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
            TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &c);
            TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &depth);
            TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &imagelength);
            TIFFGetField(tif, TIFFTAG_PLANARCONFIG, &config);
            mat.create_type(w, h, c, depth == 8 ? IM_DT_INT8 : depth == 16 ? IM_DT_INT16 : IM_DT_FLOAT32);
            if (config == PLANARCONFIG_CONTIG)
            {
                mat.elempack = c;
                for (int row = 0; row < imagelength; row++)
                {
                    void* mat_row = (unsigned char*)mat.data + (size_t)mat.w * row * mat.elemsize * mat.elempack;
                    TIFFReadScanline(tif, mat_row, row);
                }
            }
            else if (config == PLANARCONFIG_SEPARATE)
            {
                // TODO::Dicky
            }
            TIFFClose(tif);
        }
#endif
        return;
    }

    int width = 0, height = 0, component = 0;
    if (auto data = stbi_load(path, &width, &height, &component, gray ? 1 : 4))
    {
        ImMat tmp;
        tmp.create_type(width, height, gray ? 1 : 4, data, IM_DT_INT8);
        tmp.elempack = gray ? 1 : 4;
        mat = tmp.clone();
        stbi_image_free(data);
    }
    else if (auto data = stbi_load_16(path, &width, &height, &component, gray ? 1 : 4))
    {
        ImMat tmp;
        tmp.create_type(width, height, gray ? 1 : 4, data, IM_DT_INT16);
        tmp.elempack = gray ? 1 : 4;
        mat = tmp.clone();
        stbi_image_free(data);
    }
}

int ImGetTextureData(ImTextureID texture, void* data)
{
    int ret = -1;
    auto textureIt = ImFindTexture(texture);
    if (textureIt == g_Textures.end())
        return -1;
    if (!textureIt->TextureID || !data)
        return -1;

    int width = ImGui::ImGetTextureWidth(texture);
    int height = ImGui::ImGetTextureHeight(texture);
    int channels = 4; // TODO::Dicky need check

    if (width <= 0 || height <= 0 || channels <= 0)
        return -1;

#if IMGUI_RENDERING_VULKAN
    ret = ImGui_ImplVulkan_GetTextureData(textureIt->TextureID, data, width, height, channels);
#elif !IMGUI_EMSCRIPTEN && (IMGUI_RENDERING_GL3 || IMGUI_RENDERING_GL2)
    glEnable(GL_TEXTURE_2D);
    GLint last_texture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
    glBindTexture(GL_TEXTURE_2D, textureIt->TextureID->gID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glBindTexture(GL_TEXTURE_2D, last_texture);
    ret = 0;
#else
    return -1;
#endif
    if (ret != 0)
        return -1;
    return 0;
}

ImPixel ImGetTexturePixel(ImTextureID texture, float x, float y)
{
    ImPixel pixel = {};
    auto textureIt = ImFindTexture(texture);
    if (textureIt == g_Textures.end())
        return pixel;
    if (!textureIt->TextureID)
        return pixel;

    int width = ImGui::ImGetTextureWidth(texture);
    int height = ImGui::ImGetTextureHeight(texture);
    int channels = 4; // TODO::Dicky need check

    if (width <= 0 || height <= 0 || channels <= 0)
        return pixel;

    if (x < 0 || y < 0 || x > width || y > height)
        return pixel;

#if IMGUI_RENDERING_VULKAN
    auto color = ImGui_ImplVulkan_GetTexturePixel(textureIt->TextureID, x, y);
    pixel = {color.x, color.y, color.z, color.w};
#elif !IMGUI_EMSCRIPTEN && (IMGUI_RENDERING_GL3 || IMGUI_RENDERING_GL2)
    // ulgy using full texture data to pick one pixel
    // if GlVersion is greater then 4.5, maybe we can using glGetTextureSubImage
    void* data = IM_ALLOC(width * height * channels);
    int ret = ImGetTextureData(texture, data);
    if (ret == 0)
    {
        unsigned char * pixels = (unsigned char *)data;
        pixel.r = *(pixels + ((int)y * width + (int)x) * channels + 0) / 255.f;
        pixel.g = *(pixels + ((int)y * width + (int)x) * channels + 1) / 255.f;
        pixel.b = *(pixels + ((int)y * width + (int)x) * channels + 2) / 255.f;
        pixel.a = *(pixels + ((int)y * width + (int)x) * channels + 3) / 255.f;
    }
    if (data) IM_FREE(data);
#endif
    return pixel;
}

bool ImTextureToFile(ImTextureID texture, std::string path)
{
    int ret = -1;
    
    int width = ImGui::ImGetTextureWidth(texture);
    int height = ImGui::ImGetTextureHeight(texture);
    int channels = 4; // TODO::Dicky need check
    
    if (!width || !height || !channels)
    {
        return false;
    }

    void* data = IM_ALLOC(width * height * channels);
    ret = ImGetTextureData(texture, data);
    if (ret != 0)
    {
        IM_FREE(data);
        return false;
    }

    auto file_suffix = ImGuiHelper::path_filename_suffix(path);
    if (!file_suffix.empty())
    {
        if (file_suffix.compare(".png") == 0 || file_suffix.compare(".PNG") == 0)
            stbi_write_png(path.c_str(), width, height, channels, data, width * channels);
        else if (file_suffix.compare(".jpg") == 0 || file_suffix.compare(".JPG") == 0 ||
                file_suffix.compare(".jpeg") == 0 || file_suffix.compare(".JPEG") == 0)
            stbi_write_jpg(path.c_str(), width, height, channels, data, width * channels);
        else if (file_suffix.compare(".bmp") == 0 || file_suffix.compare(".BMP") == 0)
            stbi_write_bmp(path.c_str(), width, height, channels, data);
        else if (file_suffix.compare(".tga") == 0 || file_suffix.compare(".TGA") == 0)
            stbi_write_tga(path.c_str(), width, height, channels, data);
    }
    else
    {
        path += ".png";
        stbi_write_png(path.c_str(), width, height, channels, data, width * channels);
    }
    if (data) IM_FREE(data);
    return true;
}

bool ImMatToFile(const ImMat& mat, std::string path)
{
    int ret = -1;
    if (mat.empty())
    {
        return false;
    }
    auto file_suffix = ImGuiHelper::path_filename_suffix(path);
    if (!file_suffix.empty())
    {
        if (file_suffix.compare(".png") == 0 || file_suffix.compare(".PNG") == 0)
            stbi_write_png(path.c_str(), mat.w, mat.h, mat.c, mat.data, mat.w * mat.c);
        else if (file_suffix.compare(".jpg") == 0 || file_suffix.compare(".JPG") == 0 ||
                file_suffix.compare(".jpeg") == 0 || file_suffix.compare(".JPEG") == 0)
            stbi_write_jpg(path.c_str(), mat.w, mat.h, mat.c, mat.data, mat.w * mat.c);
        else if (file_suffix.compare(".bmp") == 0 || file_suffix.compare(".BMP") == 0)
            stbi_write_bmp(path.c_str(), mat.w, mat.h, mat.c, mat.data);
        else if (file_suffix.compare(".tga") == 0 || file_suffix.compare(".TGA") == 0)
            stbi_write_tga(path.c_str(), mat.w, mat.h, mat.c, mat.data);
#if IMGUI_TIFF
        else if (file_suffix.compare(".tiff") == 0 || file_suffix.compare(".TIFF") == 0)
        {
            // TODO::Dicky
            TIFF* tif = TIFFOpen(path.c_str(), "w");
            if (tif)
            {
                const char* copyright_str = "CodeWin"; 
                const char* app_str = "CodeWin"; 
                TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, mat.w);
                TIFFSetField(tif, TIFFTAG_IMAGELENGTH, mat.h);
                TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, mat.c);
                TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, mat.h);
                TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, mat.depth);
                if (mat.c == 1)
                    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
                else
                    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
                //TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_DEFLATE);
                TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT, RESUNIT_INCH);
                TIFFSetField(tif, TIFFTAG_XRESOLUTION, 72.0);
                TIFFSetField(tif, TIFFTAG_YRESOLUTION, 72.0);
                TIFFSetField(tif, TIFFTAG_COPYRIGHT, copyright_str);
                TIFFSetField(tif, TIFFTAG_SOFTWARE, app_str);
                TIFFSetField(tif, TIFFTAG_PLANARCONFIG, mat.elempack > 1 ? PLANARCONFIG_CONTIG : PLANARCONFIG_SEPARATE);
                if (mat.type == IM_DT_FLOAT32)
                    TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_IEEEFP);
                else if (mat.type == IM_DT_INT16 || mat.type == IM_DT_INT32)
                    TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
                if (mat.elempack > 1)
                {
                    for (int row = 0; row < mat.h; row++)
                    {
                        void* mat_row = (unsigned char*)mat.data + (size_t)mat.w * row * mat.elemsize * mat.elempack;
                        TIFFWriteScanline(tif, mat_row, row, 0);
                    } 
                }
                else
                {
                    for (int c = 0; c < mat.c; c++)
                    {
                        auto channel = mat.channel(c);
                        for (int row = 0; row < channel.h; row++)
                        {
                            void* channel_row = (unsigned char*)channel.data + (size_t)channel.w * row * channel.elemsize;
                            TIFFWriteScanline(tif, channel_row, row, c);
                        }
                    }
                }

                TIFFClose(tif);
            }
        }
#endif
    }
    else
    {
        path += ".png";
        stbi_write_png(path.c_str(), mat.w, mat.h, mat.c, mat.data, mat.w * mat.c);
    }
    return true;
}

void ImMatToTexture(const ImGui::ImMat& mat, ImTextureID& texture)
{
    if (mat.empty())
        return;
    if (texture)
    {
        int image_width = ImGui::ImGetTextureWidth(texture);
        int image_height = ImGui::ImGetTextureHeight(texture);
        if (mat.w != image_width || mat.h != image_height)
        {
            // mat changed
            ImGui::ImDestroyTexture(&texture);
        }
    }
    ImGui::ImGenerateOrUpdateTexture(texture, mat.w, mat.h, mat.c, (const unsigned char *)&mat, true);
}

void ImTextureToMat(ImTextureID texture, ImGui::ImMat& mat, ImVec2 offset, ImVec2 size)
{
    int ret = -1;
    if (!texture) return;
    int width = ImGui::ImGetTextureWidth(texture);
    int height = ImGui::ImGetTextureHeight(texture);
    int channels = 4; // TODO::Dicky need check
    
    if (!width || !height || !channels)
    {
        return;
    }

    if (offset.x == 0 && offset.y == 0 && (size.x == 0 || size.y == 0))
    {
        mat.create(width, height, channels, (size_t)1, 4);
        ret = ImGetTextureData(texture, mat.data);
        if (ret != 0) mat.release();
    }
    else
    {
        auto _size_x = ImMin((float)width - offset.x, size.x);
        auto _size_y = ImMin((float)height - offset.y, size.y);
        void* data = IM_ALLOC(width * height * channels);
        ret = ImGetTextureData(texture, data);
        if (ret != 0)
        {
            IM_FREE(data);
            return;
        }
        mat.create(_size_x, _size_y, channels, (size_t)1, 4);
        int line_size = _size_x * 4;
        for (int i = 0; i < _size_y; i++)
        {
            char * src_ptr = (char *)data + (int)((offset.y + i) * width * 4 + offset.x * 4);
            char * dst_ptr = (char *)mat.data + i * line_size;
            memcpy(dst_ptr, src_ptr, line_size);
        }
        IM_FREE(data);
    }
}

void ImShowVideoWindowCompare(ImDrawList *draw_list, ImTextureID texture1, ImTextureID texture2, ImVec2 pos, ImVec2 size, float& split, bool horizontal, float zoom_size, float* offset_x, float* offset_y, float* tf_x, float* tf_y, bool bLandscape, bool out_border)
{
    ImGuiIO& io = ImGui::GetIO();
    float img_split = split;
    static bool drag_handle = false;
    if (!texture1 && !texture2)
    {
        return;
    }
    ImRect video_rc(pos, pos + size);
    std::string dialog_id = "##TextureFileDlgKey" + std::to_string((long long)(texture1 ? texture1 : texture2));
    float texture_width = texture1 ? ImGui::ImGetTextureWidth(texture1) : size.x;
    float texture_height = texture1 ? ImGui::ImGetTextureHeight(texture1) : size.y;
    float aspectRatioTexture = texture_width / texture_height;
    float aspectRatioView = size.x / size.y;
    bool bTextureisLandscape = aspectRatioTexture > 1.f ? true : false;
    bool bViewisLandscape = aspectRatioView > 1.f ? true : false;
    float adj_w = 0, adj_h = 0;
    if ((bViewisLandscape && bTextureisLandscape) || (!bViewisLandscape && !bTextureisLandscape))
    {
        if (aspectRatioTexture >= aspectRatioView)
        {
            adj_w = size.x;
            adj_h = adj_w / aspectRatioTexture;
        }
        else
        {
            adj_h = size.y;
            adj_w = adj_h * aspectRatioTexture;
        }
    }
    else if (bViewisLandscape && !bTextureisLandscape)
    {
        adj_h = size.y;
        adj_w = adj_h * aspectRatioTexture;
    }
    else if (!bViewisLandscape && bTextureisLandscape)
    {
        adj_w = size.x;
        adj_h = adj_w / aspectRatioTexture;
    }
    float _tf_x = (size.x - adj_w) / 2.0;
    float _tf_y = (size.y - adj_h) / 2.0;
    float _offset_x = pos.x + _tf_x;
    float _offset_y = pos.y + _tf_y;
    if (!texture1 || !texture2)
    {
        if (!texture1) img_split = 0.f;
        else img_split = 1.0f;
    }
    if (horizontal)
    {
        if (texture1)
            draw_list->AddImage(
                            texture1,
                            ImVec2(_offset_x, _offset_y),
                            ImVec2(_offset_x + adj_w * img_split, _offset_y + adj_h),
                            ImVec2(0, 0),
                            ImVec2(img_split, 1)
                            );
        if (texture2)
            draw_list->AddImage(
                            texture2,
                            ImVec2(_offset_x + adj_w * img_split, _offset_y),
                            ImVec2(_offset_x + adj_w, _offset_y + adj_h),
                            ImVec2(img_split, 0),
                            ImVec2(1, 1)
                            );
        if (texture1 && texture2)
        {
            draw_list->AddLine(ImVec2(_offset_x + adj_w * img_split, _offset_y), ImVec2(_offset_x + adj_w * img_split, _offset_y + adj_h / 2 - 32), IM_COL32_ALPHA(IM_COL32_WHITE, 192));
            draw_list->AddLine(ImVec2(_offset_x + adj_w * img_split, _offset_y + adj_h / 2 + 32), ImVec2(_offset_x + adj_w * img_split, _offset_y + adj_h), IM_COL32_ALPHA(IM_COL32_WHITE, 192));
            ImRect handle(ImVec2(_offset_x + adj_w * img_split - 6, _offset_y + adj_h / 2 - 32), ImVec2(_offset_x + adj_w * img_split + 6, _offset_y + adj_h / 2 + 32));
            draw_list->AddRectFilled(handle.Min, handle.Max, IM_COL32_ALPHA(IM_COL32_WHITE, 96), 4);
            for (int i = 2; i < 10; i+=2)
            {
                draw_list->AddLine(handle.Min + ImVec2(i+1, 4), handle.Min + ImVec2(i+1, 60), IM_COL32_ALPHA(IM_COL32_WHITE, 224));
                draw_list->AddLine(handle.Min + ImVec2(i+2, 4), handle.Min + ImVec2(i+2, 60), IM_COL32_ALPHA(IM_COL32_BLACK, 192));
            }
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) || handle.Contains(io.MousePos))
            {
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            }
            if (drag_handle || (video_rc.Contains(io.MousePos) && (handle.Contains(io.MousePos) && ImGui::IsMouseDown(ImGuiMouseButton_Left))))
            {
                auto _split = (io.MousePos.x - _offset_x) / adj_w;
                img_split = ImClamp(_split, 0.f, 1.f);
                split = img_split;
                drag_handle = true;
            }
        }
    }
    else
    {
        if (texture1)
            draw_list->AddImage(
                            texture1,
                            ImVec2(_offset_x, _offset_y),
                            ImVec2(_offset_x + adj_w, _offset_y + adj_h * img_split),
                            ImVec2(0, 0),
                            ImVec2(1, img_split)
                            );
        if (texture2)
            draw_list->AddImage(
                            texture2,
                            ImVec2(_offset_x, _offset_y + adj_h * img_split),
                            ImVec2(_offset_x + adj_w, _offset_y + adj_h),
                            ImVec2(0, img_split),
                            ImVec2(1, 1)
                            );
        if (texture1 && texture2)
        {
            draw_list->AddLine(ImVec2(_offset_x, _offset_y + adj_h * img_split), ImVec2(_offset_x + adj_w / 2 - 32, _offset_y + adj_h * img_split), IM_COL32_ALPHA(IM_COL32_WHITE, 192));
            draw_list->AddLine(ImVec2(_offset_x + adj_w / 2 + 32, _offset_y + adj_h * img_split), ImVec2(_offset_x + adj_w, _offset_y + adj_h * img_split), IM_COL32_ALPHA(IM_COL32_WHITE, 192));
            ImRect handle(ImVec2(_offset_x + adj_w / 2 - 32, _offset_y + adj_h *img_split - 6), ImVec2(_offset_x + adj_w / 2 + 32, _offset_y + adj_h * img_split + 6));
            draw_list->AddRectFilled(handle.Min, handle.Max, IM_COL32_ALPHA(IM_COL32_WHITE, 96), 4);
            for (int i = 2; i < 10; i+=2)
            {
                draw_list->AddLine(handle.Min + ImVec2(4, i+1), handle.Min + ImVec2(60, i+1), IM_COL32_ALPHA(IM_COL32_WHITE, 224));
                draw_list->AddLine(handle.Min + ImVec2(4, i+2), handle.Min + ImVec2(60, i+2), IM_COL32_ALPHA(IM_COL32_BLACK, 192));
            }
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) || handle.Contains(io.MousePos))
            {
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
            }
            if (drag_handle || (video_rc.Contains(io.MousePos) && (handle.Contains(io.MousePos) && ImGui::IsMouseDown(ImGuiMouseButton_Left))))
            {
                auto _split = (io.MousePos.y - _offset_y) / adj_h;
                img_split = ImClamp(_split, 0.f, 1.f);
                split = img_split;
                drag_handle = true;
            }
        }
    }

    if (offset_x) *offset_x = _offset_x;
    if (offset_y) *offset_y = _offset_y;

    _tf_x = _offset_x + adj_w;
    _tf_y = _offset_y + adj_h;

    if (tf_x) *tf_x = _tf_x;
    if (tf_y) *tf_y = _tf_y;

    ImVec2 scale_range = ImVec2(1.0, 8.0);
    static float texture_zoom = scale_range.x;
    ImGui::SetCursorScreenPos(pos);
    ImGui::InvisibleButton(("##video_window" + std::to_string((long long)texture1)).c_str(), size);
    bool zoom = ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift);
    if (zoom && ImGui::IsItemHovered())
    {
        float region_sz = zoom_size / texture_zoom;
        float scale_w = texture_width / (_tf_x - _offset_x);
        float scale_h = texture_height / (_tf_y - _offset_y);
        float pos_x = (io.MousePos.x - _offset_x) * scale_w;
        float pos_y = (io.MousePos.y - _offset_y) * scale_h;
        float region_x = pos_x - region_sz * 0.5f;
        float region_y = pos_y - region_sz * 0.5f;
        if (region_x < 0.0f) { region_x = 0.0f; }
        else if (region_x > texture_width - region_sz) { region_x = texture_width - region_sz; }
        if (region_y < 0.0f) { region_y = 0.0f; }
        else if (region_y > texture_height - region_sz) { region_y = texture_height - region_sz; }
        ImGui::SetNextWindowBgAlpha(1.0);
        if (ImGui::BeginTooltip())
        {
            ImGui::Text("(%.2fx)", texture_zoom);
            ImVec2 uv0 = ImVec2((region_x) / texture_width, (region_y) / texture_height);
            ImVec2 uv1 = ImVec2((region_x + region_sz) / texture_width, (region_y + region_sz) / texture_height);
            if (texture1) ImGui::Image(texture1, ImVec2(region_sz * texture_zoom, region_sz * texture_zoom), uv0, uv1);
            ImGui::SameLine();
            if (texture2) ImGui::Image(texture2, ImVec2(region_sz * texture_zoom, region_sz * texture_zoom), uv0, uv1);
            ImGui::EndTooltip();
        }
        if (io.MouseWheel < -FLT_EPSILON)
        {
            texture_zoom *= 0.9;
            if (texture_zoom < scale_range.x) texture_zoom = scale_range.x;
        }
        else if (io.MouseWheel > FLT_EPSILON)
        {
            texture_zoom *= 1.1;
            if (texture_zoom > scale_range.y) texture_zoom = scale_range.y;
        }
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        drag_handle = false;
    }
}

void ImShowVideoWindow(ImDrawList *draw_list, ImTextureID texture, ImVec2 pos, ImVec2 size, float zoom_size, ImU32 back_color, int short_key, float* offset_x, float* offset_y, float* tf_x, float* tf_y, bool bLandscape, bool out_border, const ImVec2& uvMin, const ImVec2& uvMax)
{
    // draw background
    draw_list->AddRectFilled(pos, pos + size, back_color);
    if (texture)
    {
        ImGuiIO& io = ImGui::GetIO();
        float _tf_x, _tf_y, _offset_x, _offset_y;
        float texture_width = ImGui::ImGetTextureWidth(texture);
        float texture_height = ImGui::ImGetTextureHeight(texture);
        float aspectRatioTexture = texture_width / texture_height;
        float aspectRatioView = size.x / size.y;
        bool bTextureisLandscape = aspectRatioTexture > 1.f ? true : false;
        bool bViewisLandscape = aspectRatioView > 1.f ? true : false;
        float adj_w = 0, adj_h = 0;
        if ((bViewisLandscape && bTextureisLandscape) || (!bViewisLandscape && !bTextureisLandscape))
        {
            if (aspectRatioTexture >= aspectRatioView)
            {
                adj_w = size.x;
                adj_h = adj_w / aspectRatioTexture;
            }
            else
            {
                adj_h = size.y;
                adj_w = adj_h * aspectRatioTexture;
            }
        }
        else if (bViewisLandscape && !bTextureisLandscape)
        {
            adj_h = size.y;
            adj_w = adj_h * aspectRatioTexture;
        }
        else if (!bViewisLandscape && bTextureisLandscape)
        {
            adj_w = size.x;
            adj_h = adj_w / aspectRatioTexture;
        }
        _tf_x = (size.x - adj_w) / 2.0;
        _tf_y = (size.y - adj_h) / 2.0;
        _offset_x = pos.x + _tf_x;
        _offset_y = pos.y + _tf_y;
        
        draw_list->AddImage(
            texture,
            ImVec2(_offset_x, _offset_y),
            ImVec2(_offset_x + adj_w, _offset_y + adj_h),
            uvMin,
            uvMax
        );
        
        _tf_x = _offset_x + adj_w;
        _tf_y = _offset_y + adj_h;

        if (tf_x) *tf_x = _tf_x;
        if (tf_y) *tf_y = _tf_y;

        ImVec2 scale_range = ImVec2(2.0 , 8.0);
        static float texture_zoom = scale_range.x;
        ImGui::SetCursorScreenPos(pos);
        ImGui::InvisibleButton(("##video_window" + std::to_string((long long)texture)).c_str(), size);
        bool zoom = short_key == 0 ? ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift) : 
                    short_key == 1 ? ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl) : 
                    short_key == 2 ? ImGui::IsKeyDown(ImGuiKey_LeftAlt) || ImGui::IsKeyDown(ImGuiKey_RightAlt) : 
                                    ImGui::IsKeyDown(ImGuiKey_LeftSuper) || ImGui::IsKeyDown(ImGuiKey_RightSuper);
        if (zoom && ImGui::IsItemHovered())
        {
            ImVec4 tint_col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);   // No tint
            ImVec4 border_col = ImVec4(1.0f, 1.0f, 1.0f, 0.5f); // 50% opaque white
            float region_sz = zoom_size / texture_zoom;
            float scale_w = texture_width / (_tf_x - _offset_x);
            float scale_h = texture_height / (_tf_y - _offset_y);
            float pos_x = (io.MousePos.x - _offset_x) * scale_w;
            float pos_y = (io.MousePos.y - _offset_y) * scale_h;
            float region_x = pos_x - region_sz * 0.5f;
            float region_y = pos_y - region_sz * 0.5f;
            if (region_x < 0.0f) { region_x = 0.0f; }
            else if (region_x > texture_width - region_sz) { region_x = texture_width - region_sz; }
            if (region_y < 0.0f) { region_y = 0.0f; }
            else if (region_y > texture_height - region_sz) { region_y = texture_height - region_sz; }
            ImGui::SetNextWindowBgAlpha(1.0);
            if (ImGui::BeginTooltip())
            {
                ImGui::Text("(%.2fx)", texture_zoom);
                ImVec2 uv0 = ImVec2((region_x) / texture_width, (region_y) / texture_height);
                ImVec2 uv1 = ImVec2((region_x + region_sz) / texture_width, (region_y + region_sz) / texture_height);
                ImGui::Image(texture, ImVec2(region_sz * texture_zoom, region_sz * texture_zoom), uv0, uv1, tint_col, border_col);
                //ImVec2 window_pos = ImGui::GetWindowPos();
                //ImVec2 window_size = ImGui::GetWindowSize();
                //ImVec2 window_center = ImVec2(window_pos.x + window_size.x * 0.5f, window_pos.y + window_size.y * 0.5f);
                //ImGui::GetForegroundDrawList()->AddCircle(window_center, 4, IM_COL32(0, 255, 0, 128), 0, 3);
                ImGui::EndTooltip();
            }
            if (io.MouseWheel < -FLT_EPSILON)
            {
                texture_zoom *= 0.9;
                if (texture_zoom < scale_range.x) texture_zoom = scale_range.x;
            }
            else if (io.MouseWheel > FLT_EPSILON)
            {
                texture_zoom *= 1.1;
                if (texture_zoom > scale_range.y) texture_zoom = scale_range.y;
            }
        }
        if (offset_x) *offset_x = _offset_x;
        if (offset_y) *offset_y = _offset_y;
    }
}
} // namespace ImGui
/*
    Copyright (c) 2023 CodeWin

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <application.h>
#include <imgui.h>
#include <imgui_helper.h>
#include <imgui_extra_widget.h>
#include <imgui_json.h>
#include <implot.h>
#include <ImGuiFileDialog.h>
#include <portable-file-dialogs.h>
#include <ImGuiTabWindow.h>
#if IMGUI_VULKAN_SHADER
#include <Histogram_vulkan.h>
#include <Waveform_vulkan.h>
#include <CIE_vulkan.h>
#include <Vector_vulkan.h>
#endif
#include "MediaTimeline.h"
#include "MediaEncoder.h"
#include "FFUtils.h"
#include "FontManager.h"
#include "Logger.h"
#include <sstream>
#include <iomanip>

#define DEFAULT_MAIN_VIEW_WIDTH     1680
#define DEFAULT_MAIN_VIEW_HEIGHT    1024

#ifdef __APPLE__
#define DEFAULT_FONT_NAME ""
#elif defined(_WIN32)
#define DEFAULT_FONT_NAME ""
#elif defined(__linux__)
#define DEFAULT_FONT_NAME ""
#else
#define DEFAULT_FONT_NAME ""
#endif

using namespace MediaTimeline;

static const std::map<float, float> audio_bar_seg = {
    {66.f, 0.4f},
    {86.f, 0.2f},
    {96.f, 0.f}
};

typedef struct _output_format
{
    std::string name;
    std::string suffix;
} output_format;

static const output_format OutFormats[] = 
{
    {"QuickTime", "mov"},
    {"MP4 (MPEG-4 Part 14)", "mp4"},
    {"Matroska", "mkv"},
    {"Material eXchange Format", "mxf"},
    {"MPEG-2 Transport Stream", "ts"},
    {"WebM", "webm"},
    //{"PNG", "png"},
    //{"TIFF", "tiff"}
};

typedef struct _output_codec
{
    std::string name;
    std::string codec;
} output_codec;

static const output_codec OutputVideoCodec[] = 
{
    {"H.264 / AVC / MPEG-4 AVC / MPEG-4 part 10", "h264"},
    {"H.265 / HEVC", "hevc"},
    {"Apple ProRes", "prores"},
    {"VP9", "vp9"},
    {"DPX (Digital Picture Exchange) image", "dpx"},
    {"OpenEXR image", "exr"},
    {"VC3/DNxHD", "dnxhd"},
    {"Uncompressed", ""},
};

static const output_codec OutputVideoCodecUncompressed[] = 
{
    {"RGB 10-bit", "r210"},
    {"YUV packed 4:2:0", "yuv4"},
    {"YUV Packed 4:4:4", "v308"},
    {"YUV packed QT 4:4:4:4", "v408"},
    {"YUV 4:2:2 10-bit", "v210"},
    {"YUV 4:4:4 10-bit", "v410"},
    {"YUV 4:1:1 12-bit", "y41p"},
    {"Packed MS 4:4:4:4", "ayuv"},
};

static const output_codec OutputAudioCodec[] = 
{
    {"Advanced Audio Coding AAC", "aac"},
    {"ATSC A/52A AC-3", "ac3"},
    {"ATSC A/52 E-AC-3", "eac3"},
    {"MPEG audio layer 3", "mp3"},
    {"MPEG audio layer 2", "mp2"},
    {"Apple Lossless Audio Codec", "alac"},
    {"PCM", ""}
};

static const output_codec OutputAudioCodecPCM[] = 
{
    {"PCM signed 16-bit little-endian", "pcm_s16le"},
    {"PCM signed 16-bit big-endian", "pcm_s16be"},
    {"PCM signed 24-bit little-endian", "pcm_s24le"},
    {"PCM signed 24-bit big-endian", "pcm_s24be"},
    {"PCM signed 32-bit little-endian", "pcm_s32le"},
    {"PCM signed 32-bit big-endian", "pcm_s32be"},
    {"PCM 32-bit floating point little-endian", "pcm_f32le"},
    {"PCM 32-bit floating point big-endian", "pcm_f32be"},
};

typedef struct _output_color
{
    std::string name;
    std::string desc;
    int tag;
} output_color;

static const output_color ColorSpace[] = 
{
    {"sRGB", "RGB / IEC 61966-2-1 / YZX / ST 428-1", AVCOL_SPC_RGB},
    {"BT 709", "ITU-R BT1361 / xvYCC709", AVCOL_SPC_BT709},
    //{"None", "", AVCOL_SPC_UNSPECIFIED},
    //{"Reserved", "", AVCOL_SPC_RESERVED},
    {"FCC", "Federal Regulations 73.682", AVCOL_SPC_FCC},
    {"BT 470 BG", "BT601-6 625 / BT1358 625 / BT1700 625 PAL & SECAM / xvYCC601", AVCOL_SPC_BT470BG},
    {"SMPTE 170M", "BT601-6 525 / BT1358 525 / BT1700 NTSC", AVCOL_SPC_SMPTE170M},
    {"SMPTE 240M", "SMPTE170M and D65 white point", AVCOL_SPC_SMPTE240M},
    {"YCGCO", "Dirac / VC-2 and H.264 FRext / ITU-T SG16", AVCOL_SPC_YCGCO},
    {"BT 2020 NCL", "ITU-R BT2020 non-constant luminance", AVCOL_SPC_BT2020_NCL},
    {"BT 2020 CL", "ITU-R BT2020 constant luminance", AVCOL_SPC_BT2020_CL},
    {"SMPTE 2085", "SMPTE 2085, Y'D'zD'x", AVCOL_SPC_SMPTE2085},
    {"Chroma derived NCL", "Chromaticity-derived non-constant luminance", AVCOL_SPC_CHROMA_DERIVED_NCL},
    {"Chroma derived CL", "Chromaticity-derived constant luminance", AVCOL_SPC_CHROMA_DERIVED_CL},
    {"ICTCP", "ITU-R BT.2100-0, ICtCp", AVCOL_SPC_ICTCP},
};

static const output_color ColorTransfer[] = 
{
    //{"Reserved0", "", AVCOL_TRC_RESERVED0},
    {"BT 709", "ITU-R BT709 / BT1361", AVCOL_TRC_BT709},
    //{"None", "", AVCOL_TRC_UNSPECIFIED},
    //{"Reserved1", "", AVCOL_TRC_RESERVED},
    {"Gamma 22", "ITU-R BT470M / ITU-R BT1700 625 PAL & SECAM", AVCOL_TRC_GAMMA22},
    {"Gamma 28", "ITU-R BT470BG", AVCOL_TRC_GAMMA28},
    {"SMPTE 170M", "ITU-R BT601-6 525 or 625/BT1358 525 or 625/BT1700 NTSC", AVCOL_TRC_SMPTE170M},
    {"SMPTE 240M", "SMPTE170M and D65 white point", AVCOL_TRC_SMPTE240M},
    {"Linear", "Linear transfer characteristics", AVCOL_TRC_LINEAR},
    {"Log", "Logarithmic transfer characteristic (100:1)", AVCOL_TRC_LOG},
    {"Log sqrt", "Logarithmic transfer characteristic (316 : 1)", AVCOL_TRC_LOG_SQRT},
    {"IEC 61966", "IEC 61966-2-4", AVCOL_TRC_IEC61966_2_4},
    {"BT1361 ECG", "ITU-R BT1361 Extended Colour Gamut", AVCOL_TRC_BT1361_ECG},
    {"IEC 61966-2-1", "IEC 61966-2-1 sRGB or sYCC", AVCOL_TRC_IEC61966_2_1},
    {"BT 2020 10", "ITU-R BT2020 10-bit system", AVCOL_TRC_BT2020_10},
    {"BT 2020 12", "ITU-R BT2020 12-bit system", AVCOL_TRC_BT2020_12},
    {"SMPTE 2084", "SMPTE ST 2084 10/12/14/16 bit systems", AVCOL_TRC_SMPTE2084},
    {"SMPTE 428", "SMPTE ST 428-1", AVCOL_TRC_SMPTE428},
    {"ARIB STD B67", "ARIB STD-B67/Hybrid log-gamma", AVCOL_TRC_ARIB_STD_B67},
};

static const char* x264_profile[] = { "baseline", "main", "high", "high10", "high422", "high444" };
static const char* x264_preset[] = { "ultrafast", "superfast", "veryfast", "faster", "fast", "medium", "slow", "slower", "veryslow", "placebo" };
static const char* x264_tune[] = { "film", "animation", "grain", "stillimage", "psnr", "ssim", "fastdecode", "zerolatency" };
static const char* x265_profile[] = { "main", "main10", "mainstillpicture" };
static const char* x265_preset[] = { "ultrafast", "superfast", "veryfast", "faster", "fast", "medium", "slow", "slower", "veryslow", "placebo" };
static const char* x265_tune[] = { "psnr", "ssim", "grain", "zerolatency", "fastdecode" };
static const char* v264_profile[] = { "auto", "baseline", "main", "high", "extended" };
static const char* v265_profile[] = { "auto", "main", "main10" };

static const char* resolution_items[] = { "Custom", "720x480 NTSC", "720x576 PAL", "1280x720 HD", "1920x1080 HD", "3840x2160 UHD", "7680x3420 8K UHD"};
static const char* pixel_aspect_items[] = { "Custom", "Square", "16:9", "4:3", "Cinemascope", "Academy Standard", "Academy Flat" }; // Cinemascope=2.35:1 Academy Standard=1.37:1 Academy Flat=1.85:1
static const char* frame_rate_items[] = { "Custom", "23.976", "24", "25", "29.97", "30", "50", "59.94", "60", "100", "120" };
static const char* audio_sample_rate_items[] = { "8k", "16k", "32k", "44.1k", "48k", "96k" };
static const char* audio_channels_items[] = { "Mono", "Stereo", "Surround Stereo 5.1", "Surround Stereo 7.1", "Surround Stereo 10.1", "Surround Stereo 12.1"};
static const char* audio_format_items[] = { "16bit Short", "32bit Float", "64bit Double" };

static const char* color_system_items[] = { "NTSC", "EBU", "SMPTE", "SMPTE 240M", "APPLE", "wRGB", "CIE1931", "Rec709", "Rec2020", "DCIP3" };
static const char* cie_system_items[] = { "XYY", "UCS", "LUV" };

static const char* audio_vector_mode_items[] = { "Lissajous", "Lissajous XY", "Polar"};

static const char * font_bold_list[] = {
    "Regular",
    "Light",
    "Bold" 
};

static const char * font_italic_list[] = {
    "None",
    "Italic",
    //"Oblique" 
};

static const char* ConfigureTabNames[] = {
    "System",
    "Timeline"
};

static const char* ControlPanelTabNames[] = {
    ICON_MEDIA_BANK " Media",
    ICON_MEDIA_FILTERS " Filters",
    ICON_MEDIA_TRANS " Fusions",
    ICON_MEDIA_OUTPUT " Output"
};

static const char* ControlPanelTabTooltips[] = 
{
    "Media Bank",
    "Filters Bank",
    "Fusion Bank",
    "Media Output"
};

static const char* MainWindowTabNames[] = {
    ICON_MEDIA_PREVIEW " Preview",
    ICON_MEDIA_VIDEO " Video",
    ICON_MUSIC " Audio",
    ICON_MEDIA_TEXT " Text"
};

static const char* MainWindowTabTooltips[] = 
{
    "Media Preview",
    "Video Editor",
    "Audio Editor",
    "Text Editor",
};

#define SCOPE_VIDEO_HISTOGRAM   (1<<0)
#define SCOPE_VIDEO_WAVEFORM    (1<<1)
#define SCOPE_VIDEO_CIE         (1<<2)
#define SCOPE_VIDEO_VECTOR      (1<<3)
#define SCOPE_AUDIO_WAVE        (1<<4)
#define SCOPE_AUDIO_VECTOR      (1<<5)
#define SCOPE_AUDIO_FFT         (1<<6)
#define SCOPE_AUDIO_DB          (1<<7)
#define SCOPE_AUDIO_DB_LEVEL    (1<<8)
#define SCOPE_AUDIO_SPECTROGRAM (1<<9)

static const char* ScopeWindowTabNames[] = {
    ICON_HISTOGRAM " Video Histogram",
    ICON_WAVEFORM " Video Waveform",
    ICON_CIE " Video CIE",
    ICON_VECTOR " Video Vector",
    ICON_WAVE " Audio Wave",
    ICON_AUDIOVECTOR " Audio Vector",
    ICON_FFT " Audio FFT",
    ICON_DB " Audio dB",
    ICON_DB_LEVEL " Audio dB Level",
    ICON_SPECTROGRAM " Audio Spectrogram"
};

static const char* VideoEditorTabNames[] = {
    ICON_BLUE_PRINT,
    ICON_TRANS,
    ICON_CROP,
    //ICON_ROTATE
};

static const char* VideoEditorTabTooltips[] = {
    "Video Filter",
    "Video Fusion",
    "Video Attribute",
    //"Video Rotate"
};

static const char* AudioEditorTabNames[] = {
    ICON_BLUE_PRINT,
    ICON_TRANS,
    ICON_AUDIO_MIXING,
};

static const char* AudioEditorTabTooltips[] = {
    "Audio Filter",
    "Audio Fusion",
    "Audio Mixing",
};

static const char* TextEditorTabNames[] = {
    "Clip Style",
    "Track Style",
};

static const char* VideoAttributeScaleType[] = {
    "Fit",
    "Crop",
    "Fill",
    "Stretch",
};

static const char* VideoPreviewScale[] = {
    "1:1",
    "1/2",
    "1/4",
    "1/8",
};

const std::string video_file_dis = "*.mp4 *.mov *.mkv *.mxf *.avi *.webm *.ts";
const std::string video_file_suffix = ".mp4,.mov,.mkv,.mxf,.avi,.webm,.ts";
const std::string audio_file_dis = "*.wav *.mp3 *.aac *.ogg *.ac3 *.dts";
const std::string audio_file_suffix = ".wav,.mp3,.aac,.ogg,.ac3,.dts";
const std::string image_file_dis = "*.png *.gif *.jpg *.jpeg *.tiff *.webp";
const std::string image_file_suffix = ".png,.gif,.jpg,.jpeg,.tiff,.webp";
const std::string text_file_dis = "*.txt *.srt *.ass *.stl *.lrc *.xml";
const std::string text_file_suffix = ".txt,.srt,.ass,.stl,.lrc,.xml";
const std::string video_filter = "Video files (" + video_file_dis + "){" + video_file_suffix + "}";
const std::string audio_filter = "Audio files (" + audio_file_dis + "){" + audio_file_suffix + "}";
const std::string image_filter = "Image files (" + image_file_dis + "){" + image_file_suffix + "}";
const std::string text_filter = "SubTitle files (" + text_file_dis + "){" + text_file_suffix + "}";
const std::string ffilters = "All Support Files (" + video_file_dis + " " + audio_file_dis + " " + image_file_dis + " " + text_file_dis + ")" + "{" +
                                                    video_file_suffix + "," + audio_file_suffix + "," + image_file_suffix + "," + text_file_suffix + "}" + "," +
                                                    video_filter + "," +
                                                    audio_filter + "," +
                                                    image_filter + "," +
                                                    text_filter + "," +
                                                    ".*";
                                                    
struct MediaEditorSettings
{
    std::string UILanguage {"Default"};     // UI Language
    float TopViewHeight {0.6};              // Top view height percentage
    float BottomViewHeight {0.4};           // Bottom view height percentage
    float ControlPanelWidth {0.3};          // Control panel view width percentage
    float MainViewWidth {0.7};              // Main view width percentage
    bool BottomViewExpanded {true};         // Timeline/Scope view expended
    bool VideoFilterCurveExpanded {true};   // Video filter curve view expended
    bool VideoFusionCurveExpanded {true};   // Video fusion curve view expended
    bool AudioFilterCurveExpanded {true};   // Audio filter curve view expended
    bool AudioFusionCurveExpanded {true};   // audio fusion curve view expended
    bool TextCurveExpanded {true};          // Text curve view expended
    float OldBottomViewHeight {0.4};        // Old Bottom view height, recorde at non-expended
    bool showMeters {true};                 // show fps/GPU usage at top right of windows

    bool HardwareCodec {true};              // try HW codec
    int VideoWidth  {1920};                 // timeline Media Width
    int VideoHeight {1080};                 // timeline Media Height
    float PreviewScale {0.5};               // timeline Media Video Preview scale
    MediaInfo::Ratio VideoFrameRate {25000, 1000};// timeline frame rate
    MediaInfo::Ratio PixelAspectRatio {1, 1}; // timeline pixel aspect ratio
    int ColorSpaceIndex {1};                // timeline color space default is bt 709
    int ColorTransferIndex {0};             // timeline color transfer default is bt 709
    int VideoFrameCacheSize {10};           // timeline video cache size
    int AudioChannels {2};                  // timeline audio channels
    int AudioSampleRate {44100};            // timeline audio sample rate
    int AudioFormat {2};                    // timeline audio format 0=unknown 1=s16 2=f32
    std::string project_path;               // Editor Recently project file path
    int BankViewStyle {0};                  // Bank view style type, 0 = icons, 1 = tree vide, and ... 
    bool ShowHelpTooltips {false};          // Show UI help tool tips

    bool ExpandScope {false};
    // Histogram Scope tools
    bool HistogramLog {false};
    bool HistogramSplited {true};
    bool HistogramYRGB  {true};
    float HistogramScale {0.001};

    // Waveform Scope tools
    bool WaveformMirror {true};
    bool WaveformSeparate {false};
    bool WaveformShowY {false};
    float WaveformIntensity {2.0};
#if IMGUI_VULKAN_SHADER
    // CIE Scope tools
    int CIEColorSystem {ImGui::Rec709system};
    int CIEMode {ImGui::XYY};
    int CIEGamuts {ImGui::Rec2020system};
#else
    int CIEColorSystem {0};
    int CIEMode {0};
    int CIEGamuts {0};
#endif
    float CIEContrast {0.75};
    float CIEIntensity {0.5};
    bool CIECorrectGamma {false};
    bool CIEShowColor {true};

    // Vector Scope tools
    float VectorIntensity {0.5};

    // Audio Wave Scale setting
    float AudioWaveScale    {1.0};

    // Audio Vector Setting
    float AudioVectorScale  {1.0};
    int AudioVectorMode {LISSAJOUS};

    // Audio FFT Scale setting
    float AudioFFTScale    {1.0};

    // Audio dB Scale setting
    float AudioDBScale    {1.0};

    // Audio dB Level setting
    int AudioDBLevelShort   {1};

    // Audio Spectrogram setting
    float AudioSpectrogramOffset {0.0};
    float AudioSpectrogramLight {1.0};

    // Scope view
    int ScopeWindowIndex {0};           // default video histogram
    int ScopeWindowExpandIndex {4};     // default audio waveform

    // Text configure
    std::string FontName {DEFAULT_FONT_NAME};
    bool FontScaleLink {true};
    float FontScaleX {1.0f};
    float FontScaleY {1.0f};
    float FontSpacing {1.0f};
    float FontAngle {0.0f};
    float FontOutlineWidth {1.0f};
    int FontAlignment {2};
    int FontItalic {0};
    int FontBold {0};
    bool FontUnderLine {false};
    bool FontStrikeOut {false};
    float FontPosOffsetX {0};
    float FontPosOffsetY {0};
    int FontBorderType {1};
    float FontShadowDepth {0.0f};
    ImVec4 FontPrimaryColor {1, 1, 1, 1};
    ImVec4 FontOutlineColor {0, 0, 0, 1};
    ImVec4 FontBackColor {0, 0, 0, 1};

    // Output configure
    int OutputFormatIndex {0};
    // Output video configure
    int OutputVideoCodecIndex {0};
    int OutputVideoCodecTypeIndex {0};
    int OutputVideoCodecProfileIndex {-1};
    int OutputVideoCodecPresetIndex {-1};
    int OutputVideoCodecTuneIndex {-1};
    int OutputVideoCodecCompressionIndex {-1};   // image format compression
    bool OutputVideoSettingAsTimeline {true};
    int OutputVideoResolutionIndex {-1};
    int OutputVideoResolutionWidth {-1};         // custom setting
    int OutputVideoResolutionHeight {-1};        // custom setting
    int OutputVideoPixelAspectRatioIndex {-1};
    MediaInfo::Ratio OutputVideoPixelAspectRatio {1, 1};// custom setting
    int OutputVideoFrameRateIndex {-1};
    MediaInfo::Ratio OutputVideoFrameRate {25000, 1000};// custom setting
    int OutputColorSpaceIndex {-1};
    int OutputColorTransferIndex {-1};
    int OutputVideoBitrateStrategyindex {0};            // 0=cbr 1:vbr default cbr
    int OutputVideoBitrate {-1};
    int OutputVideoGOPSize {-1};
    int OutputVideoBFrames {0};
    // Output audio configure
    int OutputAudioCodecIndex {0};
    int OutputAudioCodecTypeIndex {0};
    bool OutputAudioSettingAsTimeline {true};
    int OutputAudioSampleRateIndex {3};                 // 44100
    int OutputAudioSampleRate {44100};                  // custom setting
    int OutputAudioChannelsIndex {1};
    int OutputAudioChannels {2};                        // custom setting

    MediaEditorSettings() {}
};

static std::string ini_file = "Media_Editor.ini";
static std::string icon_file;
static std::vector<std::string> import_url;    // import file url from system drag
static TimeLine * timeline = nullptr;
static ImTextureID codewin_texture = nullptr;
static ImTextureID logo_texture = nullptr;
static std::thread * g_loading_thread {nullptr};
static bool g_project_loading {false};
static float g_project_loading_percentage {0};
static ImGui::TabLabelStyle * tab_style = &ImGui::TabLabelStyle::Get();
static MediaEditorSettings g_media_editor_settings;
static MediaEditorSettings g_new_setting;
static imgui_json::value g_project;
static bool g_vidEncSelChanged = true;
static std::vector<MediaEncoder::EncoderDescription> g_currVidEncDescList;
static bool g_audEncSelChanged = true;
static std::vector<MediaEncoder::EncoderDescription> g_currAudEncDescList;
static std::string g_encoderConfigErrorMessage;
static bool quit_save_confirm = true;
static bool project_need_save = false;
static bool mouse_hold = false;
static uint32_t scope_flags = 0xFFFFFFFF;
static bool set_context_in_splash = false;

static int ConfigureIndex = 0;              // default timeline setting
static int ControlPanelIndex = 0;           // default Media Bank window
static int MainWindowIndex = 0;             // default Media Preview window
static int BottomWindowIndex = 0;           // default Media Timeline window, no other view so far
static int VideoEditorWindowIndex = 0;      // default Video Filter window
static int AudioEditorWindowIndex = 0;      // default Audio Filter window
static int LastMainWindowIndex = 0;
static int LastVideoEditorWindowIndex = 0;
static int LastAudioEditorWindowIndex = 0;

static int MonitorIndexPreviewVideo = -1;
static int MonitorIndexVideoFilterOrg = -1;
static int MonitorIndexVideoFiltered = -1;
static int MonitorIndexScope = -1;
static bool MonitorIndexChanged = false;

static float ui_breathing = 1.0f;
static float ui_breathing_step = 0.01;
static float ui_breathing_min = 0.5;
static float ui_breathing_max = 1.0;

#if IMGUI_VULKAN_SHADER
static ImGui::Histogram_vulkan *    m_histogram {nullptr};
static ImGui::Waveform_vulkan *     m_waveform {nullptr};
static ImGui::CIE_vulkan *          m_cie {nullptr};
static ImGui::Vector_vulkan *       m_vector {nullptr};
#endif

static bool need_update_scope {false};
static bool need_update_preview {false};

static ImGui::ImMat mat_histogram;

static ImGui::ImMat mat_waveform;
static ImTextureID waveform_texture {nullptr};

static ImGui::ImMat mat_cie;
static ImTextureID cie_texture {nullptr};

static ImGui::ImMat mat_vector;
static ImTextureID vector_texture {nullptr};

static std::unordered_map<std::string, std::vector<FM::FontDescriptorHolder>> fontTable;
static std::vector<string> fontFamilies;     // system fonts

static void GetVersion(int& major, int& minor, int& patch, int& build)
{
    major = MEDIAEDITOR_VERSION_MAJOR;
    minor = MEDIAEDITOR_VERSION_MINOR;
    patch = MEDIAEDITOR_VERSION_PATCH;
    build = MEDIAEDITOR_VERSION_BUILD;
}

static void UpdateBreathing()
{
    ui_breathing -= ui_breathing_step;
    if (ui_breathing <= ui_breathing_min)
    {
        ui_breathing = ui_breathing_min;
        ui_breathing_step = -ui_breathing_step;
    }
    else if (ui_breathing >= ui_breathing_max)
    {
        ui_breathing = ui_breathing_max;
        ui_breathing_step = -ui_breathing_step;
    }
}
static bool UIPageChanged()
{
    bool updated = false;
    if (LastMainWindowIndex == 0 && MainWindowIndex != 0)
    {
        // we leave video preview windows, stop preview play
        Logger::Log(Logger::DEBUG) << "[Changed page] leaving video preview page!!!" << std::endl;
        need_update_scope = true;
        if (timeline)
            timeline->Play(false);
    }
    if (LastMainWindowIndex == 1 && LastVideoEditorWindowIndex == 0 && (
        MainWindowIndex != 1 || VideoEditorWindowIndex != 0))
    {
        // we leave video filter windows, stop filter play, check unsaved bp
        Logger::Log(Logger::DEBUG) << "[Changed page] leaving video filter page!!!" << std::endl;
        if (timeline && timeline->mVidFilterClip)
        {
            timeline->mVidFilterClipLock.lock();
            timeline->mVidFilterClip->Save();
            timeline->mVidFilterClipLock.unlock();
            updated = true;
            need_update_scope = true;
        }
    }
    if (LastMainWindowIndex == 1 && LastVideoEditorWindowIndex == 1 && (
        MainWindowIndex != 1 || VideoEditorWindowIndex != 1))
    {
        // we leave video fusion windows, stop fusion play, check unsaved bp
        Logger::Log(Logger::DEBUG) << "[Changed page] leaving video fusion page!!!" << std::endl;
        if (timeline && timeline->mVidOverlap)
        {
            timeline->mVidFusionLock.lock();
            timeline->mVidOverlap->Save();
            timeline->mVidFusionLock.unlock();
            updated = true;
            need_update_scope = true;
        }
    }
    if (LastMainWindowIndex == 1 && LastVideoEditorWindowIndex == 2 && (
        MainWindowIndex != 1 || VideoEditorWindowIndex != 2))
    {
        // we leave video attribute windows
        Logger::Log(Logger::DEBUG) << "[Changed page] leaving video attribute page!!!" << std::endl;
        if (timeline && timeline->mVidFilterClip)
        {
            timeline->mVidFilterClipLock.lock();
            timeline->mVidFilterClip->Save();
            timeline->mVidFilterClipLock.unlock();
            updated = true;
            need_update_scope = true;
        }
    }
    if (LastMainWindowIndex == 2 && LastAudioEditorWindowIndex == 0 && (
        MainWindowIndex != 2 || AudioEditorWindowIndex != 0))
    {
        // we leave audio filter windows, stop filter play, check unsaved bp
        Logger::Log(Logger::DEBUG) << "[Changed page] leaving audio filter page!!!" << std::endl;
        if (timeline && timeline->mAudFilterClip)
        {
            timeline->mAudFilterClipLock.lock();
            timeline->mAudFilterClip->Save();
            timeline->mAudFilterClipLock.unlock();
        }
        updated = true;
    }
    if (LastMainWindowIndex == 2 && LastAudioEditorWindowIndex == 1 && (
        MainWindowIndex != 2 || AudioEditorWindowIndex != 1))
    {
        // we leave audio fusion windows, stop fusion play, check unsaved bp
        Logger::Log(Logger::DEBUG) << "[Changed page] leaving audio fusion page!!!" << std::endl;
    }

    if (LastMainWindowIndex == 3 && MainWindowIndex != 3)
    {
        // we leave text editor windows
        Logger::Log(Logger::DEBUG) << "[Changed page] leaving Text editor page!!!" << std::endl;
    }

    if (MainWindowIndex == 1 && VideoEditorWindowIndex == 0 && (
        LastMainWindowIndex != 1 || LastVideoEditorWindowIndex != 0))
    {
        // we enter video filter windows
        Logger::Log(Logger::DEBUG) << "[Changed page] Enter video filter page!!!" << std::endl;
    }

    if (MainWindowIndex == 1 && VideoEditorWindowIndex == 1 && (
        LastMainWindowIndex != 1 || LastVideoEditorWindowIndex != 1))
    {
        // we enter video fusion windows
        Logger::Log(Logger::DEBUG) << "[Changed page] Enter video fusion page!!!" << std::endl;
    }

    if (MainWindowIndex == 1 && VideoEditorWindowIndex == 2 && (
        LastMainWindowIndex != 1 || LastVideoEditorWindowIndex != 2))
    {
        // we enter video attribute windows
        Logger::Log(Logger::DEBUG) << "[Changed page] Enter video attribute page!!!" << std::endl;
    }

    if (MainWindowIndex == 2 && AudioEditorWindowIndex == 0 && (
        LastMainWindowIndex != 2 || LastAudioEditorWindowIndex != 0))
    {
        // we enter audio filter windows
        Logger::Log(Logger::DEBUG) << "[Changed page] Enter audio filter page!!!" << std::endl;
    }

    if (MainWindowIndex == 2 && AudioEditorWindowIndex == 1 && (
        LastMainWindowIndex != 2 || LastAudioEditorWindowIndex != 1))
    {
        // we enter audio fusion windows
        Logger::Log(Logger::DEBUG) << "[Changed page] Enter audio fusion page!!!" << std::endl;
    }

    if (MainWindowIndex == 3 && LastMainWindowIndex != 3)
    {
        // we enter text editor windows
        Logger::Log(Logger::DEBUG) << "[Changed page] Enter text editor page!!!" << std::endl;
    }
    
    LastMainWindowIndex = MainWindowIndex;
    LastVideoEditorWindowIndex = VideoEditorWindowIndex;
    LastAudioEditorWindowIndex = AudioEditorWindowIndex;
    return updated;
}

static int EditingClipAttribute(int type, void* handle)
{
    if (IS_VIDEO(type))
    {
        MainWindowIndex = 1;
        VideoEditorWindowIndex = 2;
    }
    else if (IS_AUDIO(type))
    {
        MainWindowIndex = 2;
        AudioEditorWindowIndex = 0; // ？
    }
    else if (IS_TEXT(type))
    {
        MainWindowIndex = 3;
    }
    auto updated = UIPageChanged();
    return updated ? 1 : 0;
}

static int EditingClipFilter(int type, void* handle)
{
    if (IS_VIDEO(type))
    {
        MainWindowIndex = 1;
        VideoEditorWindowIndex = 0;
    }
    else if (IS_AUDIO(type))
    {
        MainWindowIndex = 2;
        AudioEditorWindowIndex = 0;
    }
    else if (IS_TEXT(type))
    {
        MainWindowIndex = 3;
    }
    auto updated = UIPageChanged();
    return updated ? 1 : 0;
}

static int EditingOverlap(int type, void* handle)
{
    if (IS_VIDEO(type))
    {
        MainWindowIndex = 1;
        VideoEditorWindowIndex = 1;
    }
    else if (IS_AUDIO(type))
    {
        MainWindowIndex = 2;
        AudioEditorWindowIndex = 1;
    }
    auto updated = UIPageChanged();
    return updated ? 1 : 0;
}

// Utils functions
static bool ExpendButton(ImDrawList *draw_list, ImVec2 pos, bool expand = true)
{
    ImGuiIO &io = ImGui::GetIO();
    ImRect delRect(pos, ImVec2(pos.x + 16, pos.y + 16));
    bool overDel = delRect.Contains(io.MousePos);
    ImU32 delColor = IM_COL32_WHITE;
    float midy = pos.y + 16 / 2 - 0.5f;
    float midx = pos.x + 16 / 2 - 0.5f;
    draw_list->AddRect(delRect.Min, delRect.Max, delColor, 4);
    draw_list->AddLine(ImVec2(delRect.Min.x + 3, midy), ImVec2(delRect.Max.x - 4, midy), delColor, 2);
    if (expand) draw_list->AddLine(ImVec2(midx, delRect.Min.y + 3), ImVec2(midx, delRect.Max.y - 4), delColor, 2);
    return overDel;
}

static void ShowVideoWindow(ImDrawList *draw_list, ImTextureID texture, ImVec2 pos, ImVec2 size, float& offset_x, float& offset_y, float& tf_x, float& tf_y, bool bLandscape = true)
{
    if (texture)
    {
        ImGui::SetCursorScreenPos(pos);
        ImGui::InvisibleButton(("##video_window" + std::to_string((long long)texture)).c_str(), size);
        
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
        tf_x = (size.x - adj_w) / 2.0;
        tf_y = (size.y - adj_h) / 2.0;
        offset_x = pos.x + tf_x;
        offset_y = pos.y + tf_y;
        draw_list->AddRectFilled(ImVec2(offset_x, offset_y), ImVec2(offset_x + adj_w, offset_y + adj_h), IM_COL32_BLACK);
        draw_list->AddImage(
            texture,
            ImVec2(offset_x, offset_y),
            ImVec2(offset_x + adj_w, offset_y + adj_h),
            ImVec2(0, 0),
            ImVec2(1, 1)
        );
        tf_x = offset_x + adj_w;
        tf_y = offset_y + adj_h;
    }
}

static void ShowVideoWindow(ImTextureID texture, ImVec2 pos, ImVec2 size)
{
    float offset_x = 0, offset_y = 0;
    float tf_x = 0, tf_y = 0;
    ShowVideoWindow(ImGui::GetWindowDrawList(), texture, pos, size, offset_x, offset_y, tf_x, tf_y);
}

static void CalculateVideoScope(ImGui::ImMat& mat)
{
#if IMGUI_VULKAN_SHADER
    if (m_histogram && (scope_flags & SCOPE_VIDEO_HISTOGRAM)) m_histogram->scope(mat, mat_histogram, 256, g_media_editor_settings.HistogramScale, g_media_editor_settings.HistogramLog);
    if (m_waveform && (scope_flags & SCOPE_VIDEO_WAVEFORM)) m_waveform->scope(mat, mat_waveform, 256, g_media_editor_settings.WaveformIntensity, g_media_editor_settings.WaveformSeparate, g_media_editor_settings.WaveformShowY);
    if (m_cie && (scope_flags & SCOPE_VIDEO_CIE)) m_cie->scope(mat, mat_cie, g_media_editor_settings.CIEIntensity, g_media_editor_settings.CIEShowColor);
    if (m_vector && (scope_flags & SCOPE_VIDEO_VECTOR)) m_vector->scope(mat, mat_vector, g_media_editor_settings.VectorIntensity);
#endif
    need_update_scope = false;
}

static bool MonitorButton(const char * label, ImVec2 pos, int& monitor_index, std::vector<int> disabled_index, bool vertical = false, bool show_main = true, bool check_change = false)
{
    static std::string monitor_icons[] = {ICON_ONE, ICON_TWO, ICON_THREE, ICON_FOUR, ICON_FIVE, ICON_SIX, ICON_SEVEN, ICON_EIGHT, ICON_NINE};
    auto platform_io = ImGui::GetPlatformIO();
    ImGuiViewportP* viewport = (ImGuiViewportP*)ImGui::GetWindowViewport();
    auto current_monitor = viewport->PlatformMonitor;
    int org_index = monitor_index;
    ImGui::SetCursorScreenPos(pos);
    for (int monitor_n = 0; monitor_n < platform_io.Monitors.Size; monitor_n++)
    {
        bool disable = false;
        for (auto disabled : disabled_index)
        {
            if (disabled != -1 && disabled == monitor_n)
                disable = true;
        }
        ImGui::BeginDisabled(disable);
        bool selected = monitor_index == monitor_n;
        bool is_current_monitor = monitor_index == -1 && monitor_n == current_monitor;
        if (show_main) selected |= is_current_monitor;
        std::string icon_str = (is_current_monitor && !show_main) ? g_media_editor_settings.ExpandScope ? ICON_DRAWING_PIN : ICON_EXPANMD : monitor_icons[monitor_n];
        std::string monitor_label = icon_str + "##monitor_index" + std::string(label);
        if (ImGui::CheckButton(monitor_label.c_str(), &selected, ImVec4(0.2, 0.5, 0.2, 1.0)))
        {
            if (monitor_n == current_monitor)
            { 
                monitor_index = -1;
            }
            else
            {
                monitor_index = monitor_n;
            }
            if (check_change) MonitorIndexChanged = true;
        }
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered())
        {
            ImGuiPlatformMonitor& mon = platform_io.Monitors[monitor_n];
            ImGui::SetNextWindowViewport(viewport->ID);
            if (ImGui::BeginTooltip())
            {
                ImGui::BulletText("Monitor #%d:", monitor_n);
                ImGui::Text("DPI %.0f", mon.DpiScale * 100.0f);
                ImGui::Text("MainSize (%.0f,%.0f)", mon.MainSize.x, mon.MainSize.y);
                ImGui::Text("WorkSize (%.0f,%.0f)", mon.WorkSize.x, mon.WorkSize.y);
                ImGui::Text("MainMin (%.0f,%.0f)",  mon.MainPos.x,  mon.MainPos.y);
                ImGui::Text("MainMax (%.0f,%.0f)",  mon.MainPos.x + mon.MainSize.x, mon.MainPos.y + mon.MainSize.y);
                ImGui::Text("WorkMin (%.0f,%.0f)",  mon.WorkPos.x,  mon.WorkPos.y);
                ImGui::Text("WorkMax (%.0f,%.0f)",  mon.WorkPos.x + mon.WorkSize.x, mon.WorkPos.y + mon.WorkSize.y);
                ImGui::EndTooltip();
            }
        }
        auto icon_height = 32;//ImGui::GetTextLineHeight();
        if (!vertical) ImGui::SameLine();
        else ImGui::SetCursorScreenPos(pos + ImVec2(0, icon_height * (monitor_n + 1)));
    }
    return monitor_index != org_index;
}

// System view
static bool Show_Version(ImDrawList* draw_list, int32_t start_time)
{
    bool title_finished = true;
    int32_t current_time = ImGui::get_current_time_msec();  
    ImVec2 window_pos = ImGui::GetWindowPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    if (!logo_texture && !icon_file.empty()) logo_texture = ImGui::ImLoadTexture(icon_file.c_str());
    if (!codewin_texture) codewin_texture = ImGui::ImCreateTexture(codewin::codewin_pixels, codewin::codewin_width, codewin::codewin_height);
    draw_list->AddRectFilled(window_pos, window_pos + window_size, COL_DEEP_DARK);
    if (logo_texture || codewin_texture)
    {
        float logo_alpha = ImMin((float)(current_time - start_time) / 2000.f, 1.f);
        if (logo_texture)
        {
            auto texture_pos =  window_pos + ImVec2(32, (window_size.y - 256 - 32) / 2);
            draw_list->AddImage(logo_texture, texture_pos, texture_pos + ImVec2(256, 256), ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, logo_alpha * 255));
        }
        if (codewin_texture && logo_alpha >= 1.0)
        {
            float codewin_alpha = ImMin((float)(current_time - start_time - 2000) / 500.f, 1.f);
            ImVec2 codewin_pos = window_pos + ImVec2(32 + 256 - 70, (window_size.y - 256 - 32) / 2 + 256 - 66);
            codewin_pos += ImVec2(32.f * codewin_alpha, 32.f * codewin_alpha);
            ImVec2 codewin_size = ImVec2(64, 64) - ImVec2(32.f * codewin_alpha, 32.f * codewin_alpha);
            draw_list->AddImage(codewin_texture, codewin_pos, codewin_pos + codewin_size, ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, codewin_alpha * 255));
        }
    }
    {
        float title_alpha = ImMin((float)(current_time - start_time) / 5000.f, 1.f);
        ImGui::SetWindowFontScale(4.0);
        ImGui::PushStyleVar(ImGuiStyleVar_TexGlyphShadowOffset, ImVec2(4, 4));
        ImGui::PushStyleColor(ImGuiCol_TexGlyphShadow, ImVec4(0.0, 0.0, 0.0, 1.0));
        std::string str = "Media Editor";
        auto mark_size = ImGui::CalcTextSize(str.c_str());
        float xoft = (logo_texture ? 32 + 256 : 0) + (window_size.x - mark_size.x - (logo_texture ? 256 : 0)) / 2;
        float yoft = (window_size.y - mark_size.y - 32) / 2 - 32;
        ImGui::GetWindowDrawList()->AddText(window_pos + ImVec2(xoft, yoft), IM_COL32(255, 255, 255, title_alpha * 255), str.c_str());
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        ImGui::SetWindowFontScale(2.0);
        std::string cstr = "Community";
        auto csize = ImGui::CalcTextSize(cstr.c_str());
        float cxoft = xoft + mark_size.x - csize.x + 32;
        float cyoft = yoft + mark_size.y - 16;
        ImGui::GetWindowDrawList()->AddText(window_pos + ImVec2(cxoft, cyoft), IM_COL32(128, 255, 128, title_alpha * 255), cstr.c_str());

        int ver_major = 0, ver_minor = 0, ver_patch = 0, ver_build = 0;
        // Media Editor Version
        ImGui::SetWindowFontScale(1.2);
        GetVersion(ver_major, ver_minor, ver_patch, ver_build);
        std::string version_string = "Version: " + std::to_string(ver_major) + "." + std::to_string(ver_minor) + "." + std::to_string(ver_patch) + "." + std::to_string(ver_build);
        auto version_size = ImGui::CalcTextSize(version_string.c_str());
        float vxoft = xoft;
        float vyoft = yoft + mark_size.y + 8;
        ImGui::GetWindowDrawList()->AddText(window_pos + ImVec2(vxoft, vyoft), IM_COL32(255, 255, 255, title_alpha * 255), version_string.c_str());
        ImGui::SetWindowFontScale(1.0);
        // imgui version
        ImGui::GetVersion(ver_major, ver_minor, ver_patch, ver_build);
        version_string = "ImGui: " + std::to_string(ver_major) + "." + std::to_string(ver_minor) + "." + std::to_string(ver_patch) + "." + std::to_string(ver_build);
        version_size = ImGui::CalcTextSize(version_string.c_str());
        vxoft = window_size.x - version_size.x - 32;
        vyoft = window_size.y - 18 * 4 - 32 - 32;
        ImGui::GetWindowDrawList()->AddText(window_pos + ImVec2(vxoft, vyoft), IM_COL32(255, 255, 255, title_alpha * 255), version_string.c_str());
        // MediaCore version
        MediaCore::GetVersion(ver_major, ver_minor, ver_patch, ver_build);
        version_string = "MediaCore: " + std::to_string(ver_major) + "." + std::to_string(ver_minor) + "." + std::to_string(ver_patch) + "." + std::to_string(ver_build);
        version_size = ImGui::CalcTextSize(version_string.c_str());
        vxoft = window_size.x - version_size.x - 32;
        vyoft = window_size.y - 18 * 3 - 32 - 32;
        ImGui::GetWindowDrawList()->AddText(window_pos + ImVec2(vxoft, vyoft), IM_COL32(255, 255, 255, title_alpha * 255), version_string.c_str());
        // Blurprint SDK version
        BluePrint::GetVersion(ver_major, ver_minor, ver_patch, ver_build);
        version_string = "BluePrint: " + std::to_string(ver_major) + "." + std::to_string(ver_minor) + "." + std::to_string(ver_patch) + "." + std::to_string(ver_build);
        version_size = ImGui::CalcTextSize(version_string.c_str());
        vxoft = window_size.x - version_size.x - 32;
        vyoft = window_size.y - 18 * 2 - 32 - 32;
        ImGui::GetWindowDrawList()->AddText(window_pos + ImVec2(vxoft, vyoft), IM_COL32(255, 255, 255, title_alpha * 255), version_string.c_str());
#if IMGUI_VULKAN_SHADER
        // vkshader version
        ImGui::ImVulkanGetVersion(ver_major, ver_minor, ver_patch, ver_build);
        version_string = "VkShader: " + std::to_string(ver_major) + "." + std::to_string(ver_minor) + "." + std::to_string(ver_patch) + "." + std::to_string(ver_build);
        version_size = ImGui::CalcTextSize(version_string.c_str());
        vxoft = window_size.x - version_size.x - 32;
        vyoft = window_size.y - 18 * 1 - 32 - 32;
        ImGui::GetWindowDrawList()->AddText(window_pos + ImVec2(vxoft, vyoft), IM_COL32(255, 255, 255, title_alpha * 255), version_string.c_str());
#endif
        // copyright
        std::string copy_str = "Copyright(c) 2023 OpenCodeWin Team";
        auto copy_size = ImGui::CalcTextSize(copy_str.c_str());
        ImGui::GetWindowDrawList()->AddText(window_pos + ImVec2(window_size.x - copy_size.x - 16, window_size.y - 32 - 24), IM_COL32(128, 128, 255, title_alpha * 255), copy_str.c_str());
        if (title_alpha < 1.0) title_finished = false;
        else title_finished = true;
    }
    return title_finished;
}

static void ShowAbout(int32_t about_start_time)
{
    ImGui::BeginChild("MediaEditor Version", ImVec2(800, 400));
    Show_Version(ImGui::GetWindowDrawList(), about_start_time);
    ImGui::EndChild();
    ImGui::Separator();
    ImGui::ShowImGuiInfo();
    ImGui::Separator();
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", ImGui::GetIO().DeltaTime * 1000.f, ImGui::GetIO().Framerate);
    ImGui::Text("Frames since last input: %d", ImGui::GetIO().FrameCountSinceLastInput);
#if IMGUI_VULKAN_SHADER
    ImGui::Separator();
    int device_count = ImGui::get_gpu_count();
    for (int i = 0; i < device_count; i++)
    {
        ImGui::VulkanDevice* vkdev = ImGui::get_gpu_device(i);
        uint32_t driver_version = vkdev->info.driver_version();
        uint32_t api_version = vkdev->info.api_version();
        int device_type = vkdev->info.type();
        std::string driver_ver = std::to_string(VK_VERSION_MAJOR(driver_version)) + "." + 
                                std::to_string(VK_VERSION_MINOR(driver_version)) + "." +
                                std::to_string(VK_VERSION_PATCH(driver_version));
        std::string api_ver =   std::to_string(VK_VERSION_MAJOR(api_version)) + "." + 
                                std::to_string(VK_VERSION_MINOR(api_version)) + "." +
                                std::to_string(VK_VERSION_PATCH(api_version));
        std::string device_name = vkdev->info.device_name();
        uint32_t gpu_memory_budget = vkdev->get_heap_budget();
        uint32_t gpu_memory_usage = vkdev->get_heap_usage();
        ImGui::Text("Device[%d]:%s", i, device_name.c_str());
        ImGui::Text("Driver:%s", driver_ver.c_str());
        ImGui::Text("   API:%s", api_ver.c_str());
        ImGui::Text("Memory:%uMB(%uMB)", gpu_memory_budget, gpu_memory_usage);
        ImGui::Text("Device Type:%s", device_type == 0 ? "Discrete" : device_type == 1 ? "Integrated" : device_type == 2 ? "Virtual" : "CPU");
    }
#endif
}

static int GetResolutionIndex(int width, int height)
{
    if (width == 720 && height == 480)
        return 1;
    else if (width == 720 && height == 576)
        return 2;
    else if (width == 1280 && height == 720)
        return 3;
    else if (width == 1920 && height == 1080)
        return 4;
    else if (width == 3840 && height == 2160)
        return 5;
    else if (width == 7680 && height == 3420)
        return 6;
    return 0;
}

static void SetResolution(int& width, int& height, int index)
{
    switch (index)
    {
        case 1: width = 720;  height = 480; break;
        case 2: width = 720;  height = 576; break;
        case 3: width = 1280; height = 720; break;
        case 4: width = 1920; height = 1080; break;
        case 5: width = 3840; height = 2160; break;
        case 6: width = 7680; height = 3420; break;
        default: break;
    }
}

static int GetPixelAspectRatioIndex(MediaInfo::Ratio ratio)
{
    if (ratio.num == 1 && ratio.den == 1)
        return 1;
    else if (ratio.num == 16 && ratio.den == 9)
        return 2;
    else if (ratio.num == 4 && ratio.den == 3)
        return 3;
    else if (ratio.num == 235 && ratio.den == 100)
        return 4;
    else if (ratio.num == 137 && ratio.den == 100)
        return 5;
    else if (ratio.num == 185 && ratio.den == 100)
        return 6;
    return 0;
}

static void SetPixelAspectRatio(MediaInfo::Ratio& ratio, int index)
{
    switch (index)
    {
        case 1: ratio.num = 1;   ratio.den = 1; break;
        case 2: ratio.num = 16;  ratio.den = 9; break;
        case 3: ratio.num = 4;   ratio.den = 3; break;
        case 4: ratio.num = 235; ratio.den = 100; break;
        case 5: ratio.num = 137; ratio.den = 100; break;
        case 6: ratio.num = 185; ratio.den = 100; break;
        default: break;
    }
}

static int GetVideoFrameIndex(MediaInfo::Ratio fps)
{
    if (fps.num == 24000 && fps.den == 1001)
        return 1;
    else if (fps.num == 24000 && fps.den == 1000)
        return 2;
    else if (fps.num == 25000 && fps.den == 1000)
        return 3;
    else if (fps.num == 30000 && fps.den == 1001)
        return 4;
    else if (fps.num == 30000 && fps.den == 1000)
        return 5;
    else if (fps.num == 50000 && fps.den == 1000)
        return 6;
    else if (fps.num == 60000 && fps.den == 1001)
        return 7;
    else if (fps.num == 60000 && fps.den == 1000)
        return 8;
    else if (fps.num == 100000 && fps.den == 1000)
        return 9;
    else if (fps.num == 120000 && fps.den == 1000)
        return 10;
    return 0;
}

static void SetVideoFrameRate(MediaInfo::Ratio & rate, int index)
{
    switch (index)
    {
        case  1: rate.num = 24000;  rate.den = 1001; break;
        case  2: rate.num = 24000;  rate.den = 1000; break;
        case  3: rate.num = 25000;  rate.den = 1000; break;
        case  4: rate.num = 30000;  rate.den = 1001; break;
        case  5: rate.num = 30000;  rate.den = 1000; break;
        case  6: rate.num = 50000;  rate.den = 1000; break;
        case  7: rate.num = 60000;  rate.den = 1001; break;
        case  8: rate.num = 60000;  rate.den = 1000; break;
        case  9: rate.num = 100000; rate.den = 1000; break;
        case 10: rate.num = 120000; rate.den = 1000; break;
        default: break;
    }
}

static int GetSampleRateIndex(int sample_rate)
{
    switch (sample_rate)
    {
        case 8000:  return 0;
        case 16000: return 1;
        case 32000: return 2;
        case 44100: return 3;
        case 48000: return 4;
        case 96000: return 5;
        default: return 3;
    }
}

static void SetSampleRate(int& sample_rate, int index)
{
    switch (index)
    {
        case 0: sample_rate =  8000; break;
        case 1: sample_rate = 16000; break;
        case 2: sample_rate = 32000; break;
        case 3: sample_rate = 44100; break;
        case 4: sample_rate = 48000; break;
        case 5: sample_rate = 96000; break;
        default:sample_rate = 44100; break;
    }
}

static int GetChannelIndex(int channels)
{
    switch (channels)
    {
        case  1: return 0;
        case  2: return 1;
        case  6: return 2;
        case  8: return 3;
        case 11: return 4;
        case 13: return 5;
        default: return 1;
    }
}

static void SetAudioChannel(int& channels, int index)
{
    switch (index)
    {
        case 0: channels =  1; break;
        case 1: channels =  2; break;
        case 2: channels =  6; break;
        case 3: channels =  8; break;
        case 4: channels = 11; break;
        case 5: channels = 13; break;
        default:channels =  2; break;
    }
}

static int GetAudioFormatIndex(int format)
{
    switch (format)
    {
        case  1: return 0;
        case  2: return 1;
        case  3: return 2;
        default: return 1;
    }
}

static void SetAudioFormat(MediaEditorSettings & config, int index)
{
    switch (index)
    {
        case 0: config.AudioFormat =  1; break;
        case 1: config.AudioFormat =  2; break;
        case 2: config.AudioFormat =  3; break;
        default:config.AudioFormat =  2; break;
    }
}

static std::string GetFrequencyTag(uint32_t freq)
{
    char tag[16]= {0};
    if (freq < 1000)
        snprintf(tag, sizeof(tag)-1, "%u", freq);
    else if (freq%1000 > 0)
        snprintf(tag, sizeof(tag)-1, "%.1fK", (float)(freq/1000));
    else
        snprintf(tag, sizeof(tag)-1, "%uK", freq/1000);
    return std::string(tag);
}

static int GetPreviewScaleIndex(float scale)
{
    if (scale >= 1) return 0;
    if (scale < 1 && scale >= 0.5) return 1;
    if (scale < 0.5 && scale >= 0.25) return 2;
    if (scale < 0.25 && scale >= 0.125) return 3;
    return 1;
}

static void SetPreviewScale(MediaEditorSettings & config, int index)
{
    switch (index)
    {
        case 0: config.PreviewScale = 1.0; break;
        case 1: config.PreviewScale = 0.5; break;
        case 2: config.PreviewScale = 0.25; break;
        case 3: config.PreviewScale = 0.125; break;
        default: config.PreviewScale = 0.5; break;
    }
}

static void ShowConfigure(MediaEditorSettings & config)
{
    ImGuiIO &io = ImGui::GetIO();
    static int resolution_index = GetResolutionIndex(config.VideoWidth, config.VideoHeight);
    static int preview_scale_index = GetPreviewScaleIndex(config.PreviewScale);
    static int pixel_aspect_index = GetPixelAspectRatioIndex(config.PixelAspectRatio);
    static int frame_rate_index = GetVideoFrameIndex(config.VideoFrameRate);
    static int sample_rate_index = GetSampleRateIndex(config.AudioSampleRate);
    static int channels_index = GetChannelIndex(config.AudioChannels);
    static int format_index = GetAudioFormatIndex(config.AudioFormat);

    static char buf_cache_size[64] = {0}; snprintf(buf_cache_size, 64, "%d", config.VideoFrameCacheSize);
    static char buf_res_x[64] = {0}; snprintf(buf_res_x, 64, "%d", config.VideoWidth);
    static char buf_res_y[64] = {0}; snprintf(buf_res_y, 64, "%d", config.VideoHeight);
    static char buf_par_x[64] = {0}; snprintf(buf_par_x, 64, "%d", config.PixelAspectRatio.num);
    static char buf_par_y[64] = {0}; snprintf(buf_par_y, 64, "%d", config.PixelAspectRatio.den);
    static char buf_fmr_x[64] = {0}; snprintf(buf_fmr_x, 64, "%d", config.VideoFrameRate.num);
    static char buf_fmr_y[64] = {0}; snprintf(buf_fmr_y, 64, "%d", config.VideoFrameRate.den);

    static const int numConfigureTabs = sizeof(ConfigureTabNames)/sizeof(ConfigureTabNames[0]);
    if (ImGui::BeginChild("##ConfigureView", ImVec2(800, 600), false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::TabLabels(numConfigureTabs, ConfigureTabNames, ConfigureIndex, nullptr , false, false, nullptr, nullptr, false, false, nullptr, nullptr);
        switch (ConfigureIndex)
        {
            case 0:
                // system setting
            {
                ImGuiContext& g = *GImGui;
                if (g.LanguagesLoaded && !g.StringMap.empty())
                {
                    const char* language_name = config.UILanguage.c_str();
                    ImGui::TextUnformatted("UI Language");
                    if (ImGui::BeginCombo("##system_setting_language", language_name))
                    {
                        for (auto it = g.StringMap.begin(); it != g.StringMap.end(); ++it)
                        {
                            bool is_selected = it->first == std::string(language_name);
                            if (ImGui::Selectable(it->first.c_str(), is_selected))
                            {
                                config.UILanguage = it->first;
                            }
                            if (is_selected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                }

                ImGui::TextUnformatted("Show UI Help Tips");
                ImGui::ToggleButton("##show_ui_help_tooltips", &config.ShowHelpTooltips);
                ImGui::Separator();
                ImGui::TextUnformatted("Show UI Meters");
                ImGui::ToggleButton("##show_ui_meters", &config.showMeters);
                ImGui::Separator();
                ImGui::TextUnformatted("Bank View Style");
                ImGui::RadioButton("Icons",  (int *)&config.BankViewStyle, 0); ImGui::SameLine();
                ImGui::RadioButton("Tree",  (int *)&config.BankViewStyle, 1);
                ImGui::TextUnformatted("Video Frame Cache Size");
                ImGui::PushItemWidth(60);
                ImGui::InputText("##Video_cache_size", buf_cache_size, 64, ImGuiInputTextFlags_CharsDecimal);
                config.VideoFrameCacheSize = atoi(buf_cache_size);
            }
            break;
            case 1:
            {
                // timeline setting
                ImGui::BulletText(ICON_MEDIA_VIDEO " Video");
                if (ImGui::Combo("Resolution", &resolution_index, resolution_items, IM_ARRAYSIZE(resolution_items)))
                {
                    SetResolution(config.VideoWidth, config.VideoHeight, resolution_index);
                }
                ImGui::BeginDisabled(resolution_index != 0);
                ImGui::PushItemWidth(60);
                ImGui::InputText("##Resolution_x", buf_res_x, 64, ImGuiInputTextFlags_CharsDecimal);
                ImGui::SameLine();
                ImGui::TextUnformatted("X");
                ImGui::SameLine();
                ImGui::InputText("##Resolution_y", buf_res_y, 64, ImGuiInputTextFlags_CharsDecimal);
                ImGui::PopItemWidth();
                ImGui::EndDisabled();
                if (resolution_index == 0)
                {
                    config.VideoWidth = atoi(buf_res_x);
                    config.VideoHeight = atoi(buf_res_y);
                }
                if (ImGui::Combo("Preview Scale", &preview_scale_index, VideoPreviewScale, IM_ARRAYSIZE(VideoPreviewScale)))
                {
                    SetPreviewScale(config, preview_scale_index);
                }
                if (ImGui::Combo("Pixel Aspect Ratio", &pixel_aspect_index, pixel_aspect_items, IM_ARRAYSIZE(pixel_aspect_items)))
                {
                    SetPixelAspectRatio(config.PixelAspectRatio, pixel_aspect_index);
                }
                ImGui::BeginDisabled(pixel_aspect_index != 0);
                ImGui::PushItemWidth(60);
                ImGui::InputText("##PixelAspectRatio_x", buf_par_x, 64, ImGuiInputTextFlags_CharsDecimal);
                ImGui::SameLine();
                ImGui::TextUnformatted(":");
                ImGui::SameLine();
                ImGui::InputText("##PixelAspectRatio_y", buf_par_y, 64, ImGuiInputTextFlags_CharsDecimal);
                ImGui::PopItemWidth();
                ImGui::EndDisabled();
                if (pixel_aspect_index == 0)
                {
                    config.PixelAspectRatio.num = atoi(buf_par_x);
                    config.PixelAspectRatio.den = atoi(buf_par_y); // TODO::Dicky need check den != 0
                }

                if (ImGui::Combo("Video Frame Rate", &frame_rate_index, frame_rate_items, IM_ARRAYSIZE(frame_rate_items)))
                {
                    SetVideoFrameRate(config.VideoFrameRate, frame_rate_index);
                }
                ImGui::BeginDisabled(frame_rate_index != 0);
                ImGui::PushItemWidth(60);
                ImGui::InputText("##VideoFrameRate_x", buf_fmr_x, 64, ImGuiInputTextFlags_CharsDecimal);
                ImGui::SameLine();
                ImGui::TextUnformatted(":");
                ImGui::SameLine();
                ImGui::InputText("##VideoFrameRate_y", buf_fmr_y, 64, ImGuiInputTextFlags_CharsDecimal);
                ImGui::PopItemWidth();
                ImGui::EndDisabled();
                if (frame_rate_index == 0)
                {
                    config.VideoFrameRate.num = atoi(buf_fmr_x);
                    config.VideoFrameRate.den = atoi(buf_fmr_y); // TODO::Dicky need check den != 0
                }

                auto color_getter = [](void* data, int idx, const char** out_text){
                    output_color * color = (output_color *)data;
                    *out_text = color[idx].name.c_str();
                    return true;
                };
                ImGui::Combo("Color Space", &config.ColorSpaceIndex, color_getter, (void *)ColorSpace, IM_ARRAYSIZE(ColorSpace));
                ImGui::Combo("Color Transfer", &config.ColorTransferIndex, color_getter, (void *)ColorTransfer, IM_ARRAYSIZE(ColorTransfer));
                ImGui::Checkbox("HW codec if available", &config.HardwareCodec); ImGui::SameLine(); ImGui::TextUnformatted("(Restart Application required)");
                ImGui::Separator();
                ImGui::BulletText(ICON_MEDIA_AUDIO " Audio");
                if (ImGui::Combo("Audio Sample Rate", &sample_rate_index, audio_sample_rate_items, IM_ARRAYSIZE(audio_sample_rate_items)))
                {
                    SetSampleRate(config.AudioSampleRate, sample_rate_index);
                }
                if (ImGui::Combo("Audio Channels", &channels_index, audio_channels_items, IM_ARRAYSIZE(audio_channels_items)))
                {
                    SetAudioChannel(config.AudioChannels, channels_index);
                }
                if (ImGui::Combo("Audio Format", &format_index, audio_format_items, IM_ARRAYSIZE(audio_format_items)))
                {
                    SetAudioFormat(config, format_index);
                }
                ImGui::Separator();
                ImGui::BulletText(ICON_MEDIA_TEXT " Text");
                const char* familyValue = config.FontName.c_str();
                if (ImGui::BeginCombo("Font family##system_setting", familyValue))
                {
                    for (int i = 0; i < fontFamilies.size(); i++)
                    {
                        bool is_selected = fontFamilies[i] == config.FontName;
                        if (ImGui::Selectable(fontFamilies[i].c_str(), is_selected))
                        {
                            config.FontName = fontFamilies[i];
                        }
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                ImGui::Combo("Font Bold", &config.FontBold, font_bold_list, IM_ARRAYSIZE(font_bold_list));
                ImGui::Combo("Font Italic", &config.FontItalic, font_italic_list, IM_ARRAYSIZE(font_italic_list));
                float scale_x = config.FontScaleX;
                if (ImGui::SliderFloat("Font scale X", &scale_x, 0.2, 10, "%.1f"))
                {
                    float scale_ratio = scale_x / config.FontScaleX;
                    if (config.FontScaleLink ) config.FontScaleY *= scale_ratio;
                    config.FontScaleX = scale_x;
                }
                // link button for scalex/scaley
                auto size = ImGui::GetWindowSize();
                auto item_width = ImGui::CalcItemWidth();
                auto current_pos = ImGui::GetCursorScreenPos();
                auto link_button_pos = current_pos + ImVec2(size.x - 128, - 8);
                ImRect link_button_rect(link_button_pos, link_button_pos + ImVec2(16, 16));
                std::string link_button_text = std::string(config.FontScaleLink ? ICON_SETTING_LINK : ICON_SETTING_UNLINK);
                auto  link_button_color = config.FontScaleLink ? IM_COL32(192, 192, 192, 255) : IM_COL32(128, 128, 128, 255);
                if (link_button_rect.Contains(io.MousePos))
                {
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                        config.FontScaleLink = !config.FontScaleLink;
                    link_button_color = IM_COL32_WHITE;
                }
                ImGui::GetWindowDrawList()->AddText(link_button_pos, link_button_color, link_button_text.c_str());
                float scale_y = config.FontScaleY;
                if (ImGui::SliderFloat("Font scale Y", &scale_y, 0.2, 10, "%.1f"))
                {
                    float scale_ratio = scale_y / config.FontScaleY;
                    if (config.FontScaleLink) config.FontScaleX *= scale_ratio;
                    config.FontScaleY = scale_y;
                }
                ImGui::SliderFloat("Font position X", &config.FontPosOffsetX, -2.f, 2.f, "%.2f");
                ImGui::SliderFloat("Font position Y", &config.FontPosOffsetY, -2.f, 2.f, "%.2f");
                ImGui::SliderFloat("Font spacing", &config.FontSpacing, 0.5, 5, "%.1f");
                ImGui::SliderFloat("Font angle", &config.FontAngle, 0, 360, "%.1f");
                ImGui::SliderFloat("Font outline width", &config.FontOutlineWidth, 0, 5, "%.0f");
                ImGui::Checkbox(ICON_FONT_UNDERLINE "##font_underLine", &config.FontUnderLine);
                ImGui::SameLine();
                ImGui::Checkbox(ICON_FONT_STRIKEOUT "##font_strike_out", &config.FontStrikeOut);
                ImGui::SameLine(item_width); ImGui::TextUnformatted("Font attribute");
                ImGui::RadioButton(ICON_FA_ALIGN_LEFT "##font_alignment", &config.FontAlignment, 1); ImGui::SameLine();
                ImGui::RadioButton(ICON_FA_ALIGN_CENTER "##font_alignment", &config.FontAlignment, 2); ImGui::SameLine();
                ImGui::RadioButton(ICON_FA_ALIGN_RIGHT "##font_alignment", &config.FontAlignment, 3);
                ImGui::SameLine(item_width); ImGui::TextUnformatted("Font alignment");
                ImGui::RadioButton("Drop##font_border_type", &config.FontBorderType, 1); ImGui::SameLine();
                ImGui::RadioButton("Box##font_border_type", &config.FontBorderType, 3);
                ImGui::SameLine(item_width); ImGui::TextUnformatted("Font Border Type");
                ImGui::SliderFloat("Font shadow depth", &config.FontShadowDepth, -20.f, 20.f, "%.1f");
                ImGui::ColorEdit4("FontColor##Primary", (float*)&config.FontPrimaryColor, ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_AlphaBar);
                ImGui::SameLine(item_width); ImGui::TextUnformatted("Font primary color");
                ImGui::ColorEdit4("FontColor##Outline", (float*)&config.FontOutlineColor, ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_AlphaBar);
                ImGui::SameLine(item_width); ImGui::TextUnformatted("Font outline color");
                ImGui::ColorEdit4("FontColor##Back", (float*)&config.FontBackColor, ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_AlphaBar);
                ImGui::SameLine(item_width); ImGui::TextUnformatted("Font shadow color");
            }

            break;
            default: break;
        }
    }
    ImGui::EndChild();
    ImGui::Separator();
}

// Document Framework
static void NewTimeline()
{
    timeline = new TimeLine();
    if (timeline)
    {
        timeline->mHardwareCodec = g_media_editor_settings.HardwareCodec;
        timeline->mWidth = g_media_editor_settings.VideoWidth;
        timeline->mHeight = g_media_editor_settings.VideoHeight;
        timeline->mPreviewScale = g_media_editor_settings.PreviewScale;
        timeline->mFrameRate = g_media_editor_settings.VideoFrameRate;
        timeline->mMaxCachedVideoFrame = g_media_editor_settings.VideoFrameCacheSize > 0 ? g_media_editor_settings.VideoFrameCacheSize : MAX_VIDEO_CACHE_FRAMES;
        timeline->mAudioSampleRate = g_media_editor_settings.AudioSampleRate;
        timeline->mAudioChannels = g_media_editor_settings.AudioChannels;
        timeline->mAudioFormat = (MediaCore::AudioRender::PcmFormat)g_media_editor_settings.AudioFormat;
        timeline->mShowHelpTooltips = g_media_editor_settings.ShowHelpTooltips;
        timeline->mAudioAttribute.mAudioSpectrogramLight = g_media_editor_settings.AudioSpectrogramLight;
        timeline->mAudioAttribute.mAudioSpectrogramOffset = g_media_editor_settings.AudioSpectrogramOffset;
        timeline->mAudioAttribute.mAudioVectorScale = g_media_editor_settings.AudioVectorScale;
        timeline->mAudioAttribute.mAudioVectorMode = g_media_editor_settings.AudioVectorMode;
        timeline->mFontName = g_media_editor_settings.FontName;

        // init callbacks
        timeline->m_CallBacks.EditingClipAttribute = EditingClipAttribute;
        timeline->m_CallBacks.EditingClipFilter = EditingClipFilter;
        timeline->m_CallBacks.EditingOverlap = EditingOverlap;

        // set global variables
        MediaCore::VideoClip::USE_HWACCEL = timeline->mHardwareCodec;
    }
}

static void CleanProject()
{
    if (timeline)
    {
        delete timeline;
        timeline = nullptr;
    }
    NewTimeline();
    g_project = imgui_json::value();
}

static void NewProject()
{
    Logger::Log(Logger::DEBUG) << "[Project] Create new project!!!" << std::endl;
    CleanProject();
    g_media_editor_settings.project_path = "";
    quit_save_confirm = true;
    project_need_save = true;
}

static void LoadThread(std::string path, bool in_splash)
{
    g_project_loading = true;
    g_project_loading_percentage = 0;
    Logger::Log(Logger::DEBUG) << "[Project] Load project from file!!!" << std::endl;
    g_project_loading_percentage = 0.1;
    auto loadResult = imgui_json::value::load(path);
    if (!loadResult.second)
    {
        g_project_loading_percentage = 1.0f;
        g_project_loading = false;
        return;
    }
    g_project_loading_percentage = 0.2;
    timeline->m_in_threads = true;
    auto project = loadResult.first;
    const imgui_json::array* mediaBankArray = nullptr;
    if (imgui_json::GetPtrTo(project, "MediaBank", mediaBankArray))
    {
        float percentage = mediaBankArray->size() > 0 ?  0.6 / mediaBankArray->size() : 0;
        for (auto& media : *mediaBankArray)
        {
            MediaItem * item = nullptr;
            int64_t id = -1;
            std::string name;
            std::string path;
            uint32_t type = MEDIA_UNKNOWN;
            if (media.contains("id"))
            {
                auto& val = media["id"];
                if (val.is_number())
                {
                    id = val.get<imgui_json::number>();
                }
            }
            if (media.contains("name"))
            {
                auto& val = media["name"];
                if (val.is_string())
                {
                    name = val.get<imgui_json::string>();
                }
            }
            if (media.contains("path"))
            {
                auto& val = media["path"];
                if (val.is_string())
                {
                    path = val.get<imgui_json::string>();
                }
            }
            if (media.contains("type"))
            {
                auto& val = media["type"];
                if (val.is_number())
                {
                    type = val.get<imgui_json::number>();
                }
            }
            
            item = new MediaItem(name, path, type, timeline);
            if (id != -1) item->mID = id;
            if (!item->mValid)
            {
                if (media.contains("start"))
                {
                    auto& val = media["start"];
                    if (val.is_number())
                    {
                        item->mStart = val.get<imgui_json::number>();
                    }
                }
                if (media.contains("end"))
                {
                    auto& val = media["end"];
                    if (val.is_number())
                    {
                        item->mEnd = val.get<imgui_json::number>();
                    }
                }
            }
            timeline->media_items.push_back(item);
            g_project_loading_percentage += percentage;
        }
    }

    g_project_loading_percentage = 0.8;

    // second load TimeLine
    if (timeline && project.contains("TimeLine"))
    {
        auto& val = project["TimeLine"];
        timeline->Load(val);
    }

    g_media_editor_settings.project_path = path;
    quit_save_confirm = false;
    project_need_save = true;
    g_project_loading_percentage = 1.0;
    g_project_loading = false;
    timeline->m_in_threads = false;
}

static void SaveProject(std::string path)
{
    if (!timeline || path.empty())
        return;

    Logger::Log(Logger::DEBUG) << "[Project] Save project to file!!!" << std::endl;

    timeline->Play(false, true);
    // check current editing clip, if it has bp then save it to clip
    Clip * editing_clip = timeline->FindEditingClip();
    if (editing_clip)
    {
        if (IS_VIDEO(editing_clip->mType))
        {
            if (timeline->mVidFilterClip) timeline->mVidFilterClip->Save();
        }
        else if (IS_AUDIO(editing_clip->mType))
        {
            if (timeline->mAudFilterClip) timeline->mAudFilterClip->Save();
        }
    }

    // check current editing overlap, if it has bp then save it to overlap
    Overlap * editing_overlap = timeline->FindEditingOverlap();
    if (editing_overlap)
    {
        if (IS_VIDEO(editing_overlap->mType))
        {
            if (timeline->mVidOverlap) timeline->mVidOverlap->Save();
        }
        else if (IS_AUDIO(editing_overlap->mType))
        {
            if (timeline->mAudOverlap) timeline->mAudOverlap->Save();
        }
    }

    // first save media bank info
    imgui_json::value media_bank;
    for (auto media : timeline->media_items)
    {
        imgui_json::value item;
        item["id"] = imgui_json::number(media->mID);
        item["name"] = media->mName;
        item["path"] = media->mPath;
        item["type"] = imgui_json::number(media->mMediaType);
        item["start"] = imgui_json::number(media->mStart);
        item["end"] = imgui_json::number(media->mEnd);
        media_bank.push_back(item);
    }
    g_project["MediaBank"] = media_bank;

    // second save Timeline
    imgui_json::value timeline_val;
    timeline->Save(timeline_val);
    g_project["TimeLine"] = timeline_val;

    g_project.save(path);
    g_media_editor_settings.project_path = path;
    //quit_save_confirm = false;
}

/****************************************************************************************
 * 
 * Media Bank window
 *
 ***************************************************************************************/
static inline std::string GetVideoIcon(int width, int height)
{
    if (width == 320 && height == 240) return "QVGA";
    else if (width == 176 && height == 144) return "QCIF";
    else if (width == 352 && height == 288) return "CIF";
    else if ((width == 720 && height == 576) || (width == 704 && height == 576)) return "D1";
    else if (width == 640 && height == 480) return "VGA";
    else if (width == 1280 && height == 720) return ICON_1K;
    else if (height >= 1080 && height <= 1088) return ICON_2K;
    else if (height == 1836) return ICON_3K;
    else if (height == 2160) return ICON_4K_PLUS;
    else if (height == 2700) return ICON_5K;
    else if (height == 3240) return ICON_6K;
    else if (height == 3780) return ICON_7K;
    else if (height == 4320) return ICON_8K;
    else if (height == 4860) return ICON_9K;
    else if (height == 5400) return ICON_10K;
    else 
    {
        if (height > 720  && height < 1080) return ICON_1K_PLUS;
        if (height > 1088  && height < 1836) return ICON_2K_PLUS;
        if (height > 1836  && height < 2160) return ICON_3K_PLUS;
        if (height > 2160  && height < 2700) return ICON_4K_PLUS;
        if (height > 2700  && height < 3240) return ICON_5K_PLUS;
        if (height > 3240  && height < 3780) return ICON_6K_PLUS;
        if (height > 3780  && height < 4320) return ICON_7K_PLUS;
        if (height > 4320  && height < 4860) return ICON_8K_PLUS;
        if (height > 4860  && height < 5400) return ICON_9K_PLUS;
    }
    return ICON_MEDIA_VIDEO;
}

static inline std::string GetAudioChannelName(int channels)
{
    if (channels < 2) return "Mono";
    else if (channels == 2) return "Stereo";
    else if (channels == 6) return "Surround 5.1";
    else if (channels == 8) return "Surround 7.1";
    else if (channels == 10) return "Surround 9.1";
    else if (channels == 13) return "Surround 12.1";
    else return "Channels " + std::to_string(channels);
}

static uint32_t CheckMediaType(std::string file_suffix)
{
    uint32_t type = MEDIA_UNKNOWN;
    if (!file_suffix.empty())
    {
        if ((file_suffix.compare(".mp4") == 0) ||
            (file_suffix.compare(".mov") == 0) ||
            (file_suffix.compare(".mkv") == 0) ||
            (file_suffix.compare(".mxf") == 0) ||
            (file_suffix.compare(".avi") == 0) ||
            (file_suffix.compare(".webm") == 0) ||
            (file_suffix.compare(".ts") == 0))
            type = MEDIA_VIDEO;
        else 
            if ((file_suffix.compare(".wav") == 0) ||
                (file_suffix.compare(".mp3") == 0) ||
                (file_suffix.compare(".aac") == 0) ||
                (file_suffix.compare(".ac3") == 0) ||
                (file_suffix.compare(".dts") == 0) ||
                (file_suffix.compare(".ogg") == 0))
            type = MEDIA_AUDIO;
        else
            if ((file_suffix.compare(".mid") == 0) ||
                (file_suffix.compare(".midi") == 0))
            type = MEDIA_SUBTYPE_AUDIO_MIDI;
        else 
            if ((file_suffix.compare(".jpg") == 0) ||
                (file_suffix.compare(".jpeg") == 0) ||
                (file_suffix.compare(".png") == 0) ||
                (file_suffix.compare(".gif") == 0) ||
                (file_suffix.compare(".tiff") == 0) ||
                (file_suffix.compare(".webp") == 0))
            type = MEDIA_SUBTYPE_VIDEO_IMAGE;
        else
            if ((file_suffix.compare(".txt") == 0) ||
                (file_suffix.compare(".srt") == 0) ||
                (file_suffix.compare(".ass") == 0) ||
                (file_suffix.compare(".stl") == 0) ||
                (file_suffix.compare(".lrc") == 0) ||
                (file_suffix.compare(".xml") == 0))
            type = MEDIA_SUBTYPE_TEXT_SUBTITLE;
    }
    return type;
}

static bool InsertMedia(const std::string path)
{
    auto file_suffix = ImGuiHelper::path_suffix(path);
    auto name = ImGuiHelper::path_filename(path);
    auto type = CheckMediaType(file_suffix);
    if (timeline)
    {
        // check media is already in bank
        auto iter = std::find_if(timeline->media_items.begin(), timeline->media_items.end(), [name, path, type](const MediaItem* item)
        {
            return  name.compare(item->mName) == 0 &&
                    path.compare(item->mPath) == 0 &&
                    type == item->mMediaType;
        });
        if (iter == timeline->media_items.end() && type != MEDIA_UNKNOWN)
        {
            MediaItem * item = new MediaItem(name, path, type, timeline);
            timeline->media_items.push_back(item);
            project_need_save = true;
            return project_need_save;
        }
    }
    return false;
}

static bool ReloadMedia(std::string path, MediaItem* item)
{
    bool updated = true;
    if (!timeline)
        return false;
    // first update media item
    auto file_suffix = ImGuiHelper::path_suffix(path);
    auto name = ImGuiHelper::path_filename(path);
    // check type match
    auto type = CheckMediaType(file_suffix);
    if (type != item->mMediaType)
        return false;
    auto old_name = item->mName;
    auto old_path = item->mPath;
    auto old_start = item->mStart;
    auto old_end = item->mEnd;
    item->UpdateItem(name, path, timeline);
    if (item->mValid)
    {
        // need update timeline clip which is using current Media
        for (auto clip : timeline->m_Clips)
        {
            if (clip->mMediaID == item->mID && IS_DUMMY(clip->mType))
            {
                // reload clip from new item
                if (IS_IMAGE(clip->mType))
                {
                    VideoClip * new_clip = (VideoClip *)clip;
                    new_clip->UpdateClip(item->mMediaOverview);
                }
                else if (IS_VIDEO(clip->mType))
                {
                    VideoClip * new_clip = (VideoClip *)clip;
                    SnapshotGenerator::ViewerHolder hViewer;
                    SnapshotGeneratorHolder hSsGen = timeline->GetSnapshotGenerator(item->mID);
                    if (hSsGen)
                    {
                        hViewer = hSsGen->CreateViewer();
                        new_clip->UpdateClip(item->mMediaOverview->GetMediaParser(), hViewer, item->mEnd - item->mStart);
                    }
                    // update video snapshot
                    if (!IS_DUMMY(new_clip->mType))
                        new_clip->CalcDisplayParams();
                }
                else if (IS_AUDIO(clip->mType))
                {
                    AudioClip * new_clip = (AudioClip *)clip;
                    new_clip->UpdateClip(item->mMediaOverview, item->mEnd - item->mStart);
                }
                else if (IS_TEXT(clip->mType))
                {
                    // Text clip don't need update clip
                }
                auto track = timeline->FindTrackByClipID(clip->mID);
                if (track && !IS_DUMMY(clip->mType))
                {
                    // build data layer multi-track media reader
                    if (IS_VIDEO(clip->mType))
                    {
                        MediaCore::VideoTrackHolder vidTrack = timeline->mMtvReader->GetTrackById(track->mID);
                        if (vidTrack)
                        {
                            MediaCore::VideoClipHolder hVidClip;
                            if (IS_IMAGE(clip->mType))
                                hVidClip = vidTrack->AddNewClip(clip->mID, clip->mMediaParser, clip->mStart, clip->mEnd-clip->mStart, 0, 0);
                            else
                                hVidClip = vidTrack->AddNewClip(clip->mID, clip->mMediaParser, clip->mStart, clip->mStartOffset, clip->mEndOffset, timeline->currentTime - clip->mStart);
                        
                            BluePrintVideoFilter* bpvf = new BluePrintVideoFilter(timeline);
                            bpvf->SetBluePrintFromJson(clip->mFilterBP);
                            bpvf->SetKeyPoint(clip->mFilterKeyPoints);
                            MediaCore::VideoFilterHolder hFilter(bpvf);
                            hVidClip->SetFilter(hFilter);
                            auto attribute = hVidClip->GetTransformFilter();
                            if (attribute)
                            {
                                VideoClip * vidclip = (VideoClip *)clip;
                                attribute->SetScaleType(vidclip->mScaleType);
                                attribute->SetScaleH(vidclip->mScaleH);
                                attribute->SetScaleV(vidclip->mScaleV);
                                attribute->SetPositionOffsetH(vidclip->mPositionOffsetH);
                                attribute->SetPositionOffsetV(vidclip->mPositionOffsetV);
                                attribute->SetRotationAngle(vidclip->mRotationAngle);
                                attribute->SetCropMarginL(vidclip->mCropMarginL);
                                attribute->SetCropMarginT(vidclip->mCropMarginT);
                                attribute->SetCropMarginR(vidclip->mCropMarginR);
                                attribute->SetCropMarginB(vidclip->mCropMarginB);
                                attribute->SetKeyPoint(vidclip->mAttributeKeyPoints);
                            }
                        }
                        clip->SetViewWindowStart(timeline->firstTime);
                    }
                    else if (IS_AUDIO(clip->mType))
                    {
                        MediaCore::AudioTrackHolder audTrack = timeline->mMtaReader->GetTrackById(track->mID);
                        if (audTrack)
                        {
                            MediaCore::AudioClipHolder hAudClip = audTrack->AddNewClip(clip->mID, clip->mMediaParser, clip->mStart, clip->mStartOffset, clip->mEndOffset);
                            BluePrintAudioFilter* bpaf = new BluePrintAudioFilter(timeline);
                            bpaf->SetBluePrintFromJson(clip->mFilterBP);
                            bpaf->SetKeyPoint(clip->mFilterKeyPoints);
                            MediaCore::AudioFilterHolder hFilter(bpaf);
                            hAudClip->SetFilter(hFilter);
                            // audio attribute
                            auto aeFilter = audTrack->GetAudioEffectFilter();
                            // gain
                            auto volParams = aeFilter->GetVolumeParams();
                            volParams.volume = track->mAudioTrackAttribute.mAudioGain;
                            aeFilter->SetVolumeParams(&volParams);
                        }
                    }
                }
                if (IS_DUMMY(clip->mType))
                {
                    updated = false;
                    break;
                }
            }
        }
        if (!updated)
        {
            item->ReleaseItem();
            item->mName = old_name;
            item->mPath = old_path;
            item->mStart = old_start;
            item->mEnd = old_end;
        }
        // update preview
        timeline->UpdatePreview();
    }
    else
    {
        item->ReleaseItem();
        item->mName = old_name;
        item->mPath = old_path;
        item->mStart = old_start;
        item->mEnd = old_end;
        return false;
    }
    return updated;
}

static bool InsertMediaAddIcon(ImDrawList *draw_list, ImVec2 icon_pos, float media_icon_size)
{
    bool ret = false;
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25, 0.25, 0.4, 1.0));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
    ImVec2 icon_size = ImVec2(media_icon_size, media_icon_size);
    draw_list->AddRectFilled(icon_pos + ImVec2(6, 6), icon_pos + ImVec2(6, 6) + icon_size, IM_COL32(16, 16, 16, 255), 8, ImDrawFlags_RoundCornersAll);
    draw_list->AddRectFilled(icon_pos + ImVec2(4, 4), icon_pos + ImVec2(4, 4) + icon_size, IM_COL32(32, 32, 48, 255), 8, ImDrawFlags_RoundCornersAll);
    draw_list->AddRectFilled(icon_pos + ImVec2(2, 2), icon_pos + ImVec2(2, 2) + icon_size, IM_COL32(0, 0, 0, 255), 8, ImDrawFlags_RoundCornersAll);
    ImGui::SetCursorScreenPos(icon_pos);
    ImGui::SetWindowFontScale(2.0);
    if (ImGui::Button(ICON_IGFD_ADD "##AddMedia", icon_size))
    {
        ret = true;
        ImGuiFileDialog::Instance()->OpenDialog("##MediaEditFileDlgKey", ICON_IGFD_FOLDER_OPEN " Choose Media File", 
                                                    ffilters.c_str(),
                                                    ".",
                                                    1, 
                                                    IGFDUserDatas("Media Source"), 
                                                    ImGuiFileDialogFlags_ShowBookmark | ImGuiFileDialogFlags_CaseInsensitiveExtention | ImGuiFileDialogFlags_DisableCreateDirectoryButton | ImGuiFileDialogFlags_Modal);
    }
    ImGui::SetWindowFontScale(1.0);
    ImGui::ShowTooltipOnHover("Add new media into bank");
    ImGui::SetCursorScreenPos(icon_pos);
    ImGui::PopStyleColor(3);
    return ret;
}

static std::vector<MediaItem *>::iterator InsertMediaIcon(std::vector<MediaItem *>::iterator item, ImDrawList *draw_list, ImVec2 icon_pos, float media_icon_size)
{
    ImTextureID texture = nullptr;
    ImGuiIO& io = ImGui::GetIO();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    (*item)->UpdateThumbnail();
    ImVec2 icon_size = ImVec2(media_icon_size, media_icon_size);
    // Draw Shadow for Icon
    draw_list->AddRectFilled(icon_pos + ImVec2(6, 6), icon_pos + ImVec2(6, 6) + icon_size, IM_COL32(16, 16, 16, 255), 8, ImDrawFlags_RoundCornersAll);
    draw_list->AddRectFilled(icon_pos + ImVec2(4, 4), icon_pos + ImVec2(4, 4) + icon_size, IM_COL32(32, 32, 48, 255), 8, ImDrawFlags_RoundCornersAll);
    draw_list->AddRectFilled(icon_pos + ImVec2(2, 2), icon_pos + ImVec2(2, 2) + icon_size, IM_COL32(64, 64, 96, 255), 8, ImDrawFlags_RoundCornersAll);
    ImGui::SetCursorScreenPos(icon_pos);
    ImGui::InvisibleButton((*item)->mPath.c_str(), icon_size);
    
    if ((*item)->mValid)
    {
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
        {
            ImGui::SetDragDropPayload("Media_drag_drop", *item, sizeof(MediaItem));
            ImGui::TextUnformatted((*item)->mName.c_str());
            if (!(*item)->mMediaThumbnail.empty() && (*item)->mMediaThumbnail[0])
            {
                auto tex_w = ImGui::ImGetTextureWidth((*item)->mMediaThumbnail[0]);
                auto tex_h = ImGui::ImGetTextureHeight((*item)->mMediaThumbnail[0]);
                float aspectRatio = (float)tex_w / (float)tex_h;
                ImGui::Image((*item)->mMediaThumbnail[0], ImVec2(icon_size.x, icon_size.y / aspectRatio));
            }
            ImGui::EndDragDropSource();
        }
        if (ImGui::IsItemHovered())
        {
            float pos_x = io.MousePos.x - icon_pos.x;
            float percent = pos_x / icon_size.x;
            ImClamp(percent, 0.0f, 1.0f);
            int texture_index = (*item)->mMediaThumbnail.size() * percent;
            if (IS_IMAGE((*item)->mMediaType))
                texture_index = 0;
            if (!(*item)->mMediaThumbnail.empty())
            {
                texture = (*item)->mMediaThumbnail[texture_index];
            }

            // Show help tooltip
            if (timeline->mShowHelpTooltips && ImGui::BeginTooltip())
            {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
                ImGui::TextUnformatted("Help:");
                ImGui::TextUnformatted("    Slider mouse to overview");
                ImGui::TextUnformatted("    Drag media to timeline");
                ImGui::PopStyleVar();
                ImGui::EndTooltip();
            }
        }
        else if (!(*item)->mMediaThumbnail.empty())
        {
            if ((*item)->mMediaThumbnail.size() > 1)
                texture = (*item)->mMediaThumbnail[1];
            else
                texture = (*item)->mMediaThumbnail[0];
        }

        ImGui::SetCursorScreenPos(icon_pos);
        if (texture)
        {
            ShowVideoWindow(texture, icon_pos + ImVec2(4, 16), icon_size - ImVec2(4, 32));
        }
        else
        {
            if (IS_AUDIO((*item)->mMediaType) && (*item)->mMediaOverview)
            {
                auto wavefrom = (*item)->mMediaOverview->GetWaveform();
                if (wavefrom && wavefrom->pcm.size() > 0)
                {
                    ImVec2 wave_pos = icon_pos + ImVec2(4, 28);
                    ImVec2 wave_size = icon_size - ImVec2(8, 48);
                    ImGui::SetCursorScreenPos(icon_pos);
                    float wave_range = fmax(fabs(wavefrom->minSample), fabs(wavefrom->maxSample));
                    int channels = wavefrom->pcm.size();
                    if (channels > 2) channels = 2;
                    int channel_height = wave_size.y / channels;
                    ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
                    ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.f, 1.f, 0.f, 1.0f));
                    for (int i = 0; i < channels; i++)
                    {
                        ImGui::SetCursorScreenPos(wave_pos + ImVec2(0, i * channel_height));
                        ImVec2 plot_size(wave_size.x, channel_height);
                        int sampleSize = wavefrom->pcm[i].size();
                        std::string id_string = "##BankWaveform@" + std::to_string((*item)->mID) + "@" + std::to_string(i);
                        ImGui::PlotLinesEx(id_string.c_str(), &wavefrom->pcm[i][0], sampleSize, 0, nullptr, -wave_range / 2, wave_range / 2, plot_size, sizeof(float), false);
                    }
                    ImGui::PopStyleColor(2);
                }
                else
                {
                    std::string lable = std::string(ICON_MEDIA_WAVE) + "##" + (*item)->mName + "@" + std::to_string((*item)->mID);
                    ImGui::SetWindowFontScale(2.5);
                    ImGui::Button(lable.c_str(), ImVec2(media_icon_size, media_icon_size));
                    ImGui::SetWindowFontScale(1.0);
                }
            }
            else if (IS_TEXT((*item)->mMediaType))
            {
                ImGui::Button(ICON_MEDIA_TEXT " Text", ImVec2(media_icon_size, media_icon_size));
            }
            else
                ImGui::Button((*item)->mName.c_str(), ImVec2(media_icon_size, media_icon_size));
        }

        if ((*item)->mMediaOverview && (*item)->mMediaOverview->IsOpened())
        {
            auto has_video = (*item)->mMediaOverview->HasVideo();
            auto has_audio = (*item)->mMediaOverview->HasAudio();
            auto media_length = (*item)->mMediaOverview->GetMediaParser()->GetMediaInfo()->duration;
            ImGui::SetCursorScreenPos(icon_pos + ImVec2(4, 4));
            std::string type_string = "? ";
            if (IS_VIDEO((*item)->mMediaType))
            {
                if (IS_IMAGE((*item)->mMediaType)) type_string = std::string(ICON_FA_FILE_IMAGE) + " ";
                else type_string = std::string(ICON_FA_FILE_VIDEO) + " ";
            }
            else if (IS_AUDIO((*item)->mMediaType))
            {
                if (IS_MIDI((*item)->mMediaType)) type_string = std::string(ICON_FA_FILE_WAVEFORM) + " ";
                else type_string = std::string(ICON_FA_FILE_AUDIO) + " ";
            }
            if (!IS_IMAGE((*item)->mMediaType))
                type_string += ImGuiHelper::MillisecToString(media_length * 1000, 2);
            ImGui::SetWindowFontScale(0.7);
            ImGui::TextUnformatted(type_string.c_str());
            ImGui::SetWindowFontScale(1.0);
            ImGui::ShowTooltipOnHover("%s", (*item)->mPath.c_str());
            ImGui::SetCursorScreenPos(icon_pos + ImVec2(0, media_icon_size - 20));

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
            if (has_video)
            {
                auto stream = (*item)->mMediaOverview->GetVideoStream();
                if (stream)
                {
                    auto video_icon = GetVideoIcon(stream->width, stream->height);
                    ImGui::SetWindowFontScale(1.2);
                    ImGui::Button(video_icon.c_str(), ImVec2(24, 24));
                    if (ImGui::IsItemHovered() && ImGui::BeginTooltip())
                    {
                        std::string bitrate_str = stream->bitRate >= 1000000 ? std::to_string((float)stream->bitRate / 1000000) + "Mbps" :
                                                stream->bitRate >= 1000 ? std::to_string((float)stream->bitRate / 1000) + "Kbps" :
                                                std::to_string(stream->bitRate) + "bps";
                        ImGui::Text("S: %d x %d", stream->width, stream->height);
                        ImGui::Text("B: %s", bitrate_str.c_str());
                        ImGui::Text("F: %.3ffps", stream->avgFrameRate.den > 0 ? stream->avgFrameRate.num / stream->avgFrameRate.den : 0.0);
                        ImGui::EndTooltip();
                    }
                    ImGui::SameLine(0, 0);
                    if (stream->isHdr)
                    {
                        ImGui::Button(ICON_HDR, ImVec2(24, 24));
                        ImGui::SameLine(0, 0);
                    }
                    ImGui::SetWindowFontScale(0.6);
                    ImGui::Button((std::to_string(stream->bitDepth) + "bit").c_str(), ImVec2(24, 24));
                    ImGui::SetWindowFontScale(1.0);
                    ImGui::SameLine(0, 0);
                }
            }
            if (has_audio)
            {
                auto stream = (*item)->mMediaOverview->GetAudioStream();
                if (stream)
                {
                    auto audio_channels = stream->channels;
                    auto audio_sample_rate = stream->sampleRate;
                    std::string audio_icon = audio_channels >= 2 ? ICON_STEREO : ICON_MONO;
                    ImGui::Button(audio_icon.c_str(), ImVec2(24, 24));
                    ImGui::ShowTooltipOnHover("%d %s", audio_sample_rate, GetAudioChannelName(audio_channels).c_str());
                    ImGui::SameLine(0 ,0);
                }
            }
            ImGui::PopStyleColor(3);
        }
        else if (IS_TEXT((*item)->mMediaType) && ImGuiHelper::file_exists((*item)->mPath))
        {
            ImGui::SetCursorScreenPos(icon_pos + ImVec2(4, 4));
            std::string type_string;
            if (IS_SUBTITLE((*item)->mMediaType)) type_string = std::string(ICON_FA_FILE_CODE);
            else type_string = std::string(ICON_FA_FILE_LINES);
            ImGui::SetWindowFontScale(0.7);
            ImGui::TextUnformatted(type_string.c_str());
            ImGui::SetWindowFontScale(1.0);
            ImGui::ShowTooltipOnHover("%s", (*item)->mPath.c_str());
            ImGui::SetCursorScreenPos(icon_pos + ImVec2(0, media_icon_size - 20));
        }
    }
    else
    {
        // Media lost or path isn't right
        auto error_area_min = icon_pos + ImVec2(4, 16);
        auto error_area_max = error_area_min + icon_size - ImVec2(4, 32);
        draw_list->AddRectFilled(error_area_min, error_area_max, COL_ERROR_MEDIA);
        draw_list->AddLine(error_area_min, error_area_max, IM_COL32_BLACK);
        draw_list->AddLine(error_area_min + ImVec2(0, icon_size.y - 32), error_area_max + ImVec2(0, - icon_size.y + 32), IM_COL32_BLACK);
        ImGui::SetWindowFontScale(2.0);
        std::string error_mark = std::string(ICON_ERROR_MEDIA);
        auto error_mark_size = ImGui::CalcTextSize(error_mark.c_str());
        float _xoft = (error_area_max.x - error_area_min.x - error_mark_size.x) / 2;
        float _yoft = (error_area_max.y - error_area_min.y - error_mark_size.y) / 2;
        draw_list->AddText(error_area_min + ImVec2(_xoft, _yoft), IM_COL32_WHITE, ICON_ERROR_MEDIA);
        ImGui::SetWindowFontScale(1.0);

        ImGui::SetCursorScreenPos(icon_pos + ImVec2(4, 4));
        std::string type_string = "? ";
        if (IS_VIDEO((*item)->mMediaType))
        {
            if (IS_IMAGE((*item)->mMediaType)) type_string = std::string(ICON_FA_FILE_IMAGE);
            else type_string = std::string(ICON_FA_FILE_VIDEO);
        }
        else if (IS_AUDIO((*item)->mMediaType))
        {
            if (IS_MIDI((*item)->mMediaType)) type_string = std::string(ICON_FA_FILE_WAVEFORM);
            else type_string = std::string(ICON_FA_FILE_AUDIO);
        }
        else if (IS_TEXT((*item)->mMediaType))
        {
            if (IS_SUBTITLE((*item)->mMediaType)) type_string = std::string(ICON_FA_FILE_CODE);
            else type_string = std::string(ICON_FA_FILE_LINES);
        }
        ImGui::SetWindowFontScale(0.7);
        ImGui::TextUnformatted(type_string.c_str());
        ImGui::ShowTooltipOnHover("%s", (*item)->mPath.c_str());
        ImGui::SetWindowFontScale(0.8);
        std::string err_str = "*Media Missing*";
        auto str_size = ImGui::CalcTextSize(err_str.c_str());
        float x_oft = (media_icon_size - str_size.x) / 2;
        float y_oft = (16 - str_size.y) / 2;
        ImGui::SetCursorScreenPos(icon_pos + ImVec2(x_oft, media_icon_size - y_oft - str_size.y));
        ImGui::TextUnformatted(err_str.c_str());
        ImGui::SetWindowFontScale(1.0);

        ImGui::SetCursorScreenPos(icon_pos + ImVec2(media_icon_size - 36, 0));
        ImGui::SetWindowFontScale(0.8);
        ImGui::Button((std::string(ICON_ZOOM "##relocate_media") + (*item)->mPath).c_str(), ImVec2(16, 16));
        ImGui::SetWindowFontScale(1.0);
        ImRect _rect(icon_pos + ImVec2(media_icon_size - 36, 0), icon_pos + ImVec2(media_icon_size - 36, 0) + ImVec2(16, 16));
        bool _overButton = _rect.Contains(io.MousePos);
        if (_overButton && io.MouseClicked[0])
        {
            std::string filter;
            if (IS_IMAGE((*item)->mMediaType)) filter = image_filter;
            else if (IS_VIDEO((*item)->mMediaType)) filter = video_filter;
            else if (IS_AUDIO((*item)->mMediaType)) filter = audio_filter;
            else if (IS_TEXT((*item)->mMediaType)) filter = text_filter;
            else filter = ".*";
            ImGuiFileDialog::Instance()->OpenDialog("##MediaEditReloadDlgKey", ICON_IGFD_FOLDER_OPEN " Choose Media File", 
                                                    filter.c_str(),
                                                    ".",
                                                    1, 
                                                    IGFDUserDatas((*item)), 
                                                    ImGuiFileDialogFlags_ShowBookmark | ImGuiFileDialogFlags_CaseInsensitiveExtention | ImGuiFileDialogFlags_DisableCreateDirectoryButton | ImGuiFileDialogFlags_Modal);

        }
        ImGui::ShowTooltipOnHover("Reload Media");
    }

    ImGui::SetCursorScreenPos(icon_pos + ImVec2(media_icon_size - 16, 0));
    ImGui::SetWindowFontScale(0.8);
    ImGui::Button((std::string(ICON_TRASH "##delete_media") + (*item)->mPath).c_str(), ImVec2(16, 16));
    ImGui::SetWindowFontScale(1.0);
    ImRect button_rect(icon_pos + ImVec2(media_icon_size - 16, 0), icon_pos + ImVec2(media_icon_size - 16, 0) + ImVec2(16, 16));
    bool overButton = button_rect.Contains(io.MousePos);
    if (overButton && io.MouseClicked[0])
    {
        // TODO::Dicky need delete it from timeline list ?
        MediaItem * it = *item;
        delete it;
        item = timeline->media_items.erase(item);
    }
    else
        item++;
    ImGui::ShowTooltipOnHover("Delete Media");

    ImGui::PopStyleColor();
    return item;
}

static void ShowMediaBankWindow(ImDrawList *draw_list, float media_icon_size)
{
    ImGuiIO& io = ImGui::GetIO();
    bool multiviewport = io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    static std::vector<std::string> failed_items;
    ImVec2 window_pos = ImGui::GetWindowPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    ImVec2 contant_size = ImGui::GetContentRegionAvail();
    ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
    ImGui::SetWindowFontScale(2.5);
    ImGui::Indent(20);
    ImGui::PushStyleVar(ImGuiStyleVar_TexGlyphOutlineWidth, 0.5f);
    ImGui::PushStyleColor(ImGuiCol_TexGlyphOutline, ImVec4(0.2, 0.2, 0.2, 0.7));
    draw_list->AddText(window_pos + ImVec2(8, 0), COL_GRAY_TEXT, "Media Bank");
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::SetWindowFontScale(1.0);

    if (!timeline)
        return;

    if (timeline->media_items.empty())
    {
        ImGui::SetWindowFontScale(2.0);
        //ImGui::Indent(20);
        ImGui::PushStyleVar(ImGuiStyleVar_TexGlyphOutlineWidth, 0.5f);
        ImGui::PushStyleColor(ImGuiCol_TexGlyphOutline, ImVec4(0.2, 0.2, 0.2, 0.7));
        ImU32 text_color = IM_COL32(ui_breathing * 255, ui_breathing * 255, ui_breathing * 255, 255);
        draw_list->AddText(window_pos + ImVec2(128,  48), COL_GRAY_TEXT, "Please Click");
        draw_list->AddText(window_pos + ImVec2(128,  80), text_color, "<-- Here");
        draw_list->AddText(window_pos + ImVec2(128, 112), COL_GRAY_TEXT, "To Add Media");
        draw_list->AddText(window_pos + ImVec2( 10, 144), COL_GRAY_TEXT, "Or Drag Files From Brower");
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        ImGui::SetWindowFontScale(1.0);
    }
    // Show Media Icons
    ImGui::SetCursorPos({20, 20});
    int icon_number_pre_row = window_size.x / (media_icon_size + 24);
    // insert empty icon for add media
    auto icon_pos = ImGui::GetCursorScreenPos() + ImVec2(0, 24);
    InsertMediaAddIcon(draw_list, icon_pos, media_icon_size);
    bool media_add_icon = true;
    for (auto item = timeline->media_items.begin(); item != timeline->media_items.end();)
    {
        if (!media_add_icon) icon_pos = ImGui::GetCursorScreenPos() + ImVec2(0, 24);
        for (int i = media_add_icon ? 1 : 0; i < icon_number_pre_row; i++)
        {
            auto row_icon_pos = icon_pos + ImVec2(i * (media_icon_size + 24), 0);
            item = InsertMediaIcon(item, draw_list, row_icon_pos, media_icon_size);
            if (item == timeline->media_items.end())
                break;
        }
        media_add_icon = false;
        if (item == timeline->media_items.end())
            break;
        ImGui::SetCursorScreenPos(icon_pos + ImVec2(0, media_icon_size));
    }

    ImGui::Dummy(ImVec2(0, 24));

    // Handle drag drop from system
    ImGui::SetCursorScreenPos(window_pos);
    ImGui::InvisibleButton("media_bank_view", contant_size);
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FILES"))
        {
            if (timeline)
            {
                for (auto path : import_url)
                {
                    auto ret = InsertMedia(path);
                    if (!ret)
                    {
                        auto filename = ImGuiHelper::path_filename(path);
                        failed_items.push_back(filename);
                    }
                    else
                    {
                        pfd::notify("Import File Succeed", path, pfd::icon::info);
                    }
                }
                import_url.clear();
                if (!failed_items.empty())
                {
                    ImGui::OpenPopup("Failed loading media", ImGuiPopupFlags_AnyPopup);
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    if (multiviewport)
        ImGui::SetNextWindowViewport(viewport->ID);
    if (ImGui::BeginPopupModal("Failed loading media", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::TextUnformatted("Can't load following Media:");
        for (auto name : failed_items)
        {
            ImGui::Text("%s", name.c_str());
        }
        if (ImGui::Button("OK", ImVec2(60, 0)))
        {
            failed_items.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

/****************************************************************************************
 * 
 * Transition Bank window
 *
 ***************************************************************************************/
static void ShowFusionBankIconWindow(ImDrawList *draw_list)
{
    ImVec2 fusion_icon_size{96, 54};
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 window_pos = ImGui::GetWindowPos();
    ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
    ImGui::SetWindowFontScale(2.5);
    ImGui::Indent(20);
    ImGui::PushStyleVar(ImGuiStyleVar_TexGlyphOutlineWidth, 0.5f);
    ImGui::PushStyleColor(ImGuiCol_TexGlyphOutline, ImVec4(0.2, 0.2, 0.2, 0.7));
    draw_list->AddText(window_pos + ImVec2(8, 0), COL_GRAY_TEXT, "Fusion Bank");
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::SetWindowFontScale(1.0);

    if (!timeline)
        return;
    // Show Fusion Icons
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::SetCursorPos({20, 20});
    if (timeline->m_BP_UI.m_Document)
    {
        auto &bp = timeline->m_BP_UI.m_Document->m_Blueprint;
        auto node_reg = bp.GetNodeRegistry();
        //for (auto type : node_reg->GetTypes())
        for (auto node : node_reg->GetNodes())
        {
            auto catalog = BluePrint::GetCatalogInfo(node->GetCatalog());
            if (catalog.size() < 2 || catalog[0].compare("Fusion") != 0)
                continue;
            auto type = node->GetTypeInfo();
            std::string drag_type = "Fusion_drag_drop_" + catalog[1];
            ImGui::Dummy(ImVec2(0, 16));
            auto icon_pos = ImGui::GetCursorScreenPos();
            ImVec2 icon_size = fusion_icon_size;
            // Draw Shadow for Icon
            draw_list->AddRectFilled(icon_pos + ImVec2(6, 6), icon_pos + ImVec2(6, 6) + icon_size, IM_COL32(32, 32, 32, 255));
            draw_list->AddRectFilled(icon_pos + ImVec2(4, 4), icon_pos + ImVec2(4, 4) + icon_size, IM_COL32(48, 48, 72, 255));
            draw_list->AddRectFilled(icon_pos + ImVec2(2, 2), icon_pos + ImVec2(2, 2) + icon_size, IM_COL32(64, 64, 96, 255));
            draw_list->AddRectFilled(icon_pos, icon_pos + icon_size, COL_BLACK_DARK);
            ImGui::InvisibleButton(type.m_Name.c_str(), icon_size);
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
            {
                ImGui::SetDragDropPayload(drag_type.c_str(), node, sizeof(BluePrint::Node));
                ImGui::TextUnformatted(ICON_BANK " Add Fusion");
                ImGui::TextUnformatted(type.m_Name.c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::IsItemHovered())
            {
                // Show help tooltip
                if (timeline->mShowHelpTooltips && !ImGui::IsDragDropActive() && ImGui::BeginTooltip())
                {
                    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
                    ImGui::TextUnformatted("Help:");
                    ImGui::TextUnformatted("    Drag fusion to blue print");
                    ImGui::PopStyleVar();
                    ImGui::EndTooltip();                }
            }
            ImGui::SetCursorScreenPos(icon_pos + ImVec2(2, 2));
            node->DrawNodeLogo(ImGui::GetCurrentContext(), fusion_icon_size); 
            float gap = (icon_size.y - ImGui::GetFontSize()) / 2.0f;
            ImGui::SetCursorScreenPos(icon_pos + ImVec2(icon_size.x + 8, gap));
            ImGui::Button(type.m_Name.c_str(), ImVec2(0, 32));
            ImGui::Spacing();
        }
    }
    ImGui::PopStyleColor();
}

static void ShowFusionBankTreeWindow(ImDrawList *draw_list)
{
    ImVec2 window_pos = ImGui::GetWindowPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
    const ImVec2 item_size(window_size.x, 32);
    ImGui::SetWindowFontScale(2.5);
    ImGui::PushStyleVar(ImGuiStyleVar_TexGlyphOutlineWidth, 0.5f);
    ImGui::PushStyleColor(ImGuiCol_TexGlyphOutline, ImVec4(0.2, 0.2, 0.2, 0.7));
    draw_list->AddText(window_pos + ImVec2(8, 0), COL_GRAY_TEXT, "Fusion Bank");
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::SetWindowFontScale(1.0);
    
    // Show Fusion Tree
    if (timeline->m_BP_UI.m_Document)
    {
        std::vector<const BluePrint::Node*> fusions;
        auto &bp = timeline->m_BP_UI.m_Document->m_Blueprint;
        auto node_reg = bp.GetNodeRegistry();
        // find all fusions
        for (auto node : node_reg->GetNodes())
        {
            auto catalog = BluePrint::GetCatalogInfo(node->GetCatalog());
            if (!catalog.size() || catalog[0].compare("Fusion") != 0)
                continue;
            fusions.push_back(node);
        }

        // make fusion type as tree
        ImGui::ImTree fusion_tree;
        fusion_tree.name = "Fusion";
        for (auto node : fusions)
        {
            auto catalog = BluePrint::GetCatalogInfo(node->GetCatalog());
            if (!catalog.size())
                continue;
            auto type = node->GetTypeInfo();
            if (catalog.size() > 1)
            {
                auto children = fusion_tree.FindChildren(catalog[1]);
                if (!children)
                {
                    ImGui::ImTree subtree(catalog[1]);
                    if (catalog.size() > 2)
                    {
                        ImGui::ImTree sub_sub_tree(catalog[2]);
                        ImGui::ImTree end_sub(type.m_Name, (void *)node);
                        sub_sub_tree.childrens.push_back(end_sub);
                        subtree.childrens.push_back(sub_sub_tree);
                    }
                    else
                    {
                        ImGui::ImTree end_sub(type.m_Name, (void *)node);
                        subtree.childrens.push_back(end_sub);
                    }

                    fusion_tree.childrens.push_back(subtree);
                }
                else
                {
                    if (catalog.size() > 2)
                    {
                        auto sub_children = children->FindChildren(catalog[2]);
                        if (!sub_children)
                        {
                            ImGui::ImTree subtree(catalog[2]);
                            ImGui::ImTree end_sub(type.m_Name, (void *)node);
                            subtree.childrens.push_back(end_sub);
                            children->childrens.push_back(subtree);
                        }
                        else
                        {
                            ImGui::ImTree end_sub(type.m_Name, (void *)node);
                            sub_children->childrens.push_back(end_sub);
                        }
                    }
                    else
                    {
                        ImGui::ImTree end_sub(type.m_Name, (void *)node);
                        children->childrens.push_back(end_sub);
                    }
                }
            }
            else
            {
                ImGui::ImTree end_sub(type.m_Name, (void *)node);
                fusion_tree.childrens.push_back(end_sub);
            }
        }

        auto AddFusion = [&](void* data)
        {
            const BluePrint::Node* node = (const BluePrint::Node*)data;
            if (!node) return;
            auto type = node->GetTypeInfo();
            auto catalog = BluePrint::GetCatalogInfo(type.m_Catalog);
            if (catalog.size() < 2 || catalog[0].compare("Fusion") != 0)
                return;
            std::string drag_type = "Fusion_drag_drop_" + catalog[1];
            auto icon_pos = ImGui::GetCursorScreenPos();
            ImVec2 icon_size = ImVec2(56, 32);
            ImGui::InvisibleButton(type.m_Name.c_str(), icon_size);
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
            {
                ImGui::SetDragDropPayload(drag_type.c_str(), node, sizeof(BluePrint::Node));
                ImGui::TextUnformatted(ICON_BANK " Add Fusion");
                ImGui::TextUnformatted(type.m_Name.c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::IsItemHovered())
            {
                // Show help tooltip
                if (timeline->mShowHelpTooltips && !ImGui::IsDragDropActive() && ImGui::BeginTooltip())
                {
                    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
                    ImGui::TextUnformatted("Help:");
                    ImGui::TextUnformatted("    Drag fusion to blue print");
                    ImGui::PopStyleVar();
                    ImGui::EndTooltip();
                }
            }
            ImGui::SetCursorScreenPos(icon_pos);
            node->DrawNodeLogo(ImGui::GetCurrentContext(), icon_size);
            ImGui::SameLine();
            ImGui::Button(type.m_Name.c_str(), ImVec2(0, 32));
        };

        // draw fusion tree
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        for (auto sub : fusion_tree.childrens)
        {
            ImGui::SetNextItemOpen(true, ImGuiCond_Once);
            if (sub.data)
            {
                AddFusion(sub.data);
            }
            else if (ImGui::TreeNode(sub.name.c_str()))
            {
                for (auto sub_sub : sub.childrens)
                {
                    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
                    if (sub_sub.data)
                    {
                        AddFusion(sub_sub.data);
                    }
                    else if (ImGui::TreeNode(sub_sub.name.c_str()))
                    {
                        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
                        for (auto end : sub_sub.childrens)
                        {
                            if (!end.data)
                                continue;
                            else
                            {
                                AddFusion(end.data);
                            }
                        }   
                        ImGui::TreePop();
                    }
                }
                ImGui::TreePop();
            }
        }
        ImGui::PopStyleColor();
    }
}

/****************************************************************************************
 * 
 * Filters Bank window
 *
 ***************************************************************************************/
static void ShowFilterBankIconWindow(ImDrawList *draw_list)
{
    float filter_icon_size = 48;
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 window_pos = ImGui::GetWindowPos();
    ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
    ImGui::SetWindowFontScale(2.5);
    ImGui::Indent(20);
    ImGui::PushStyleVar(ImGuiStyleVar_TexGlyphOutlineWidth, 0.5f);
    ImGui::PushStyleColor(ImGuiCol_TexGlyphOutline, ImVec4(0.2, 0.2, 0.2, 0.7));
    draw_list->AddText(window_pos + ImVec2(8, 0), COL_GRAY_TEXT, "Filters Bank");
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::SetWindowFontScale(1.0);

    if (!timeline)
        return;
    // Show Filter Icons
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::SetCursorPos({20, 20});
    if (timeline->m_BP_UI.m_Document)
    {
        auto &bp = timeline->m_BP_UI.m_Document->m_Blueprint;
        auto node_reg = bp.GetNodeRegistry();
        for (auto node : node_reg->GetNodes())
        {
            auto catalog = BluePrint::GetCatalogInfo(node->GetCatalog());
            if (catalog.size() < 2 || catalog[0].compare("Filter") != 0)
                continue;
            auto type = node->GetTypeInfo();
            std::string drag_type = "Filter_drag_drop_" + catalog[1];
            ImGui::Dummy(ImVec2(0, 16));
            auto icon_pos = ImGui::GetCursorScreenPos();
            ImVec2 icon_size = ImVec2(filter_icon_size, filter_icon_size);
            // Draw Shadow for Icon
            draw_list->AddRectFilled(icon_pos + ImVec2(6, 6), icon_pos + ImVec2(6, 6) + icon_size, IM_COL32(32, 32, 32, 255));
            draw_list->AddRectFilled(icon_pos + ImVec2(4, 4), icon_pos + ImVec2(4, 4) + icon_size, IM_COL32(48, 48, 72, 255));
            draw_list->AddRectFilled(icon_pos + ImVec2(2, 2), icon_pos + ImVec2(2, 2) + icon_size, IM_COL32(64, 64, 96, 255));
            draw_list->AddRectFilled(icon_pos, icon_pos + icon_size, COL_BLACK_DARK);
            ImGui::InvisibleButton(type.m_Name.c_str(), icon_size);
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
            {
                ImGui::SetDragDropPayload(drag_type.c_str(), node, sizeof(BluePrint::Node));
                ImGui::TextUnformatted(ICON_BANK " Add Filter");
                ImGui::TextUnformatted(type.m_Name.c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::IsItemHovered())
            {
                // Show help tooltip
                if (timeline->mShowHelpTooltips && !ImGui::IsDragDropActive() && ImGui::BeginTooltip())
                {
                    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
                    ImGui::TextUnformatted("Help:");
                    ImGui::TextUnformatted("    Drag filter to blue print");
                    ImGui::PopStyleVar();
                    ImGui::EndTooltip();
                }
            }
            ImGui::SetCursorScreenPos(icon_pos + ImVec2(2, 2));
            node->DrawNodeLogo(ImGui::GetCurrentContext(), ImVec2(filter_icon_size, filter_icon_size)); 
            float gap = (icon_size.y - ImGui::GetFontSize()) / 2.0f;
            ImGui::SetCursorScreenPos(icon_pos + ImVec2(icon_size.x + 8, gap));
            ImGui::Button(type.m_Name.c_str(), ImVec2(0, 32));
            ImGui::Spacing();
        }
    }
    ImGui::PopStyleColor();
}

static void ShowFilterBankTreeWindow(ImDrawList *draw_list)
{
    ImVec2 window_pos = ImGui::GetWindowPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
    const ImVec2 item_size(window_size.x, 32);
    ImGui::SetWindowFontScale(2.5);
    ImGui::PushStyleVar(ImGuiStyleVar_TexGlyphOutlineWidth, 0.5f);
    ImGui::PushStyleColor(ImGuiCol_TexGlyphOutline, ImVec4(0.2, 0.2, 0.2, 0.7));
    draw_list->AddText(window_pos + ImVec2(8, 0), COL_GRAY_TEXT, "Filters Bank");
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::SetWindowFontScale(1.0);

    // Show Filter Tree
    if (timeline->m_BP_UI.m_Document)
    {
        std::vector<const BluePrint::Node*> filters;
        auto bp = timeline->m_BP_UI.m_Document->m_Blueprint;
        auto node_reg = bp.GetNodeRegistry();
        // find all filters
        for (auto node : node_reg->GetNodes())
        {
            auto catalog = BluePrint::GetCatalogInfo(node->GetCatalog());
            if (!catalog.size() || catalog[0].compare("Filter") != 0)
                continue;
            filters.push_back(node);
        }

        // make filter type as tree
        ImGui::ImTree filter_tree;
        filter_tree.name = "Filters";
        for (auto node : filters)
        {
            auto catalog = BluePrint::GetCatalogInfo(node->GetCatalog());
            if (!catalog.size())
                continue;
            auto type = node->GetTypeInfo();
            if (catalog.size() > 1)
            {
                auto children = filter_tree.FindChildren(catalog[1]);
                if (!children)
                {
                    ImGui::ImTree subtree(catalog[1]);
                    if (catalog.size() > 2)
                    {
                        ImGui::ImTree sub_sub_tree(catalog[2]);
                        ImGui::ImTree end_sub(type.m_Name, (void *)node);
                        sub_sub_tree.childrens.push_back(end_sub);
                        subtree.childrens.push_back(sub_sub_tree);
                    }
                    else
                    {
                        ImGui::ImTree end_sub(type.m_Name, (void *)node);
                        subtree.childrens.push_back(end_sub);
                    }

                    filter_tree.childrens.push_back(subtree);
                }
                else
                {
                    if (catalog.size() > 2)
                    {
                        auto sub_children = children->FindChildren(catalog[2]);
                        if (!sub_children)
                        {
                            ImGui::ImTree subtree(catalog[2]);
                            ImGui::ImTree end_sub(type.m_Name, (void *)node);
                            subtree.childrens.push_back(end_sub);
                            children->childrens.push_back(subtree);
                        }
                        else
                        {
                            ImGui::ImTree end_sub(type.m_Name, (void *)node);
                            sub_children->childrens.push_back(end_sub);
                        }
                    }
                    else
                    {
                        ImGui::ImTree end_sub(type.m_Name, (void *)node);
                        children->childrens.push_back(end_sub);
                    }
                }
            }
            else
            {
                ImGui::ImTree end_sub(type.m_Name, (void *)node);
                filter_tree.childrens.push_back(end_sub);
            }
        }

        auto AddFilter = [&](void* data)
        {
            const BluePrint::Node* node = (const BluePrint::Node*)data;
            if (!node) return;
            auto type = node->GetTypeInfo();
            auto catalog = BluePrint::GetCatalogInfo(type.m_Catalog);
            if (catalog.size() < 2 || catalog[0].compare("Filter") != 0)
                return;
            std::string drag_type = "Filter_drag_drop_" + catalog[1];
            auto icon_pos = ImGui::GetCursorScreenPos();
            ImVec2 icon_size = ImVec2(32, 32);
            ImGui::InvisibleButton(type.m_Name.c_str(), icon_size);
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
            {
                ImGui::SetDragDropPayload(drag_type.c_str(), node, sizeof(BluePrint::Node));
                ImGui::TextUnformatted(ICON_BANK " Add Filter");
                ImGui::TextUnformatted(type.m_Name.c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::IsItemHovered())
            {
                // Show help tooltip
                if (timeline->mShowHelpTooltips && !ImGui::IsDragDropActive() && ImGui::BeginTooltip())
                {
                    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
                    ImGui::TextUnformatted("Help:");
                    ImGui::TextUnformatted("    Drag filter to blue print");
                    ImGui::PopStyleVar();
                    ImGui::EndTooltip();
                }
            }
            ImGui::SetCursorScreenPos(icon_pos);
            node->DrawNodeLogo(ImGui::GetCurrentContext(), icon_size);
            ImGui::SameLine();
            ImGui::Button(type.m_Name.c_str(), ImVec2(0, 32));
        };

        // draw filter tree
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        for (auto sub : filter_tree.childrens)
        {
            ImGui::SetNextItemOpen(true, ImGuiCond_Once);
            if (sub.data)
            {
                AddFilter(sub.data);
            }
            else if (ImGui::TreeNode(sub.name.c_str()))
            {
                for (auto sub_sub : sub.childrens)
                {
                    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
                    if (sub_sub.data)
                    {
                        AddFilter(sub_sub.data);
                    }
                    else if (ImGui::TreeNode(sub_sub.name.c_str()))
                    {
                        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
                        for (auto end : sub_sub.childrens)
                        {
                            if (!end.data)
                                continue;
                            else
                            {
                                AddFilter(end.data);
                            }
                        }   
                        ImGui::TreePop();
                    }
                }
                ImGui::TreePop();
            }
        }
        ImGui::PopStyleColor();
    }
}

/****************************************************************************************
 * 
 * Media Output window
 *
 ***************************************************************************************/
static void ShowMediaOutputWindow(ImDrawList *draw_list)
{
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    bool multiviewport = io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 window_pos = ImGui::GetWindowPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
    const ImVec2 item_size(window_size.x, 32);
    ImGui::SetWindowFontScale(2.5);
    ImGui::PushStyleVar(ImGuiStyleVar_TexGlyphOutlineWidth, 0.5f);
    ImGui::PushStyleColor(ImGuiCol_TexGlyphOutline, ImVec4(0.2, 0.2, 0.2, 0.7));
    draw_list->AddText(window_pos + ImVec2(8, 0), COL_GRAY_TEXT, "Media Output");
    static int encoder_stage = 0; // 0:unknown 1:encoding 2:finished
    static double encoder_start = -1, encoder_end = -1, encode_duration = -1;
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::SetWindowFontScale(1.0);
    if (!timeline)
        return;

    ImGui::SetCursorPos({20, 50});
    ImGui::SetWindowFontScale(1.2);
    if (ImGui::ColoredButton(ICON_MAKE_VIDEO " Make Video", ImVec2(window_size.x - 40, 48.f), IM_COL32(255, 255, 255, 255), IM_COL32(50, 220, 60, 255), IM_COL32(69, 150, 70, 255),10.0f))
    {
        g_encoderConfigErrorMessage.clear();
        encoder_stage = 0;
        encoder_end = encoder_start = encode_duration = -1;
        ImGui::OpenPopup("Make Media##MakeVideoDlyKey", ImGuiPopupFlags_AnyPopup);
    }
    ImGui::SetWindowFontScale(1.0);

    if (ImGui::BeginChild("##Subcp01"))
    {
        ImGui::Dummy(ImVec2(0, 20));
        string value = timeline->mOutputName;
        if (ImGui::InputText("File Name##output_file_name_string_value", (char*)value.data(), value.size() + 1, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackResize, [](ImGuiInputTextCallbackData* data) -> int
        {
            if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
            {
                auto& stringValue = *static_cast<string*>(data->UserData);
                ImVector<char>* my_str = (ImVector<char>*)data->UserData;
                IM_ASSERT(stringValue.data() == data->Buf);
                stringValue.resize(data->BufSize);
                data->Buf = (char*)stringValue.data();
            }
            else if (data->EventFlag == ImGuiInputTextFlags_CallbackEdit)
            {
                auto& stringValue = *static_cast<string*>(data->UserData);
                stringValue = std::string(data->Buf);
            }
            return 0;
        }, &value))
        {
            value.resize(strlen(value.c_str()));
            if (timeline->mOutputName.compare(value) != 0)
            {
                timeline->mOutputName = value;
            }
        }
        value = timeline->mOutputPath;
        if (ImGui::InputText("File Path##output_file_path_string_value", (char*)value.data(), value.size() + 1, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackResize, [](ImGuiInputTextCallbackData* data) -> int
        {
            if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
            {
                auto& stringValue = *static_cast<string*>(data->UserData);
                ImVector<char>* my_str = (ImVector<char>*)data->UserData;
                IM_ASSERT(stringValue.data() == data->Buf);
                stringValue.resize(data->BufSize);
                data->Buf = (char*)stringValue.data();
            }
            else if (data->EventFlag == ImGuiInputTextFlags_CallbackEdit)
            {
                auto& stringValue = *static_cast<string*>(data->UserData);
                stringValue = std::string(data->Buf);
            }
            return 0;
        }, &value))
        {
            value.resize(strlen(value.c_str()));
            if (timeline->mOutputPath.compare(value) != 0)
            {
                timeline->mOutputPath = value;
            }
        }
        if (ImGui::IsItemHovered() && !timeline->mOutputPath.empty() && ImGui::BeginTooltip())
        {
            ImGui::TextUnformatted(timeline->mOutputPath.c_str());
            ImGui::EndTooltip();
        }
        ImGui::SameLine();
        if (ImGui::Button("...##output_path_browse"))
        {
            ImGuiFileDialog::Instance()->OpenDialog("##MediaEditOutputPathDlgKey", ICON_IGFD_FOLDER_OPEN " Output Path", 
                                                    nullptr,
                                                    timeline->mOutputPath.empty() ? "." : timeline->mOutputPath,
                                                    1, 
                                                    IGFDUserDatas("OutputPath"), 
                                                    ImGuiFileDialogFlags_ShowBookmark | 
                                                    ImGuiFileDialogFlags_CaseInsensitiveExtention |
                                                    ImGuiFileDialogFlags_Modal);
        }

        // Format Setting
        auto format_getter = [](void* data, int idx, const char** out_text){
            output_format * formats = (output_format *)data;
            *out_text = formats[idx].name.c_str();
            return true;
        };
        ImGui::Combo("File Format##file_format", &g_media_editor_settings.OutputFormatIndex, format_getter, (void *)OutFormats, IM_ARRAYSIZE(OutFormats));

        // Video Setting
        ImGui::Dummy(ImVec2(0, 20));
        ImGui::Checkbox("Export Video##export_video", &timeline->bExportVideo);
        ImGui::Separator();
        if (timeline->bExportVideo) ImGui::BeginDisabled(false); else ImGui::BeginDisabled(true);
        bool has_bit_rate = false;
        bool has_gop_size = false;
        bool has_b_frame = false;
        auto codec_getter = [](void* data, int idx, const char** out_text){
            output_codec * codecs = (output_codec *)data;
            *out_text = codecs[idx].name.c_str();
            return true;
        };
        auto codec_type_getter = [](void* data, int idx, const char** out_text){
            std::vector<MediaEncoder::EncoderDescription> * codecs = (std::vector<MediaEncoder::EncoderDescription>*)data;
            *out_text = codecs->at(idx).longName.c_str();
            return true;
        };
        auto codec_option_getter = [](void* data, int idx, const char** out_text){
            std::vector<MediaEncoder::Option::EnumValue> * profiles = (std::vector<MediaEncoder::Option::EnumValue>*)data;
            *out_text = profiles->at(idx).name.c_str();
            return true;
        };
        auto color_getter = [](void* data, int idx, const char** out_text){
            output_color * color = (output_color *)data;
            *out_text = color[idx].name.c_str();
            return true;
        };
        // video codec select
        if (ImGui::Combo("Codec##video_codec", &g_media_editor_settings.OutputVideoCodecIndex, codec_getter, (void *)OutputVideoCodec, IM_ARRAYSIZE(OutputVideoCodec)))
        {
            g_vidEncSelChanged = true;
            g_media_editor_settings.OutputVideoCodecTypeIndex = 0;  // reset codec type if we change codec
            g_media_editor_settings.OutputVideoCodecProfileIndex = -1;
            g_media_editor_settings.OutputVideoCodecPresetIndex = -1;
            g_media_editor_settings.OutputVideoCodecTuneIndex = -1;
            g_media_editor_settings.OutputVideoCodecCompressionIndex = -1;
        }

        // video codec type select
        if (OutputVideoCodec[g_media_editor_settings.OutputVideoCodecIndex].name.compare("Uncompressed") == 0)
        {
            ImGui::Combo("Codec Type##uncompressed_video_codec", &g_media_editor_settings.OutputVideoCodecTypeIndex, codec_getter, (void *)OutputVideoCodecUncompressed, IM_ARRAYSIZE(OutputVideoCodecUncompressed));
        }
        else
        {
            if (g_vidEncSelChanged)
            {
                string codecHint = OutputVideoCodec[g_media_editor_settings.OutputVideoCodecIndex].codec;
                if (!MediaEncoder::FindEncoder(codecHint, g_currVidEncDescList))
                {
                    g_currVidEncDescList.clear();
                }
                g_vidEncSelChanged = false;
            }
            if (ImGui::Combo("Codec Type##video_codec_type", &g_media_editor_settings.OutputVideoCodecTypeIndex, codec_type_getter, (void *)&g_currVidEncDescList, g_currVidEncDescList.size()))
            {
                g_media_editor_settings.OutputVideoCodecProfileIndex = -1;
                g_media_editor_settings.OutputVideoCodecPresetIndex = -1;
                g_media_editor_settings.OutputVideoCodecTuneIndex = -1;
                g_media_editor_settings.OutputVideoCodecCompressionIndex = -1;
            }

            if (!g_currVidEncDescList.empty())
            {
                if (g_currVidEncDescList[g_media_editor_settings.OutputVideoCodecTypeIndex].codecName.compare("libx264") == 0 ||
                    g_currVidEncDescList[g_media_editor_settings.OutputVideoCodecTypeIndex].codecName.compare("libx264rgb") == 0)
                {
                    // libx264 setting
                    if (g_media_editor_settings.OutputVideoCodecProfileIndex == -1) g_media_editor_settings.OutputVideoCodecProfileIndex = 1;
                    if (g_media_editor_settings.OutputVideoCodecPresetIndex == -1) g_media_editor_settings.OutputVideoCodecPresetIndex = 5;
                    if (g_media_editor_settings.OutputVideoCodecTuneIndex == -1) g_media_editor_settings.OutputVideoCodecTuneIndex = 0;
                    ImGui::Combo("Codec Profile##x264_profile", &g_media_editor_settings.OutputVideoCodecProfileIndex, x264_profile, IM_ARRAYSIZE(x264_profile));
                    ImGui::Combo("Codec Preset##x264_preset", &g_media_editor_settings.OutputVideoCodecPresetIndex, x264_preset, IM_ARRAYSIZE(x264_preset));
                    ImGui::Combo("Codec Tune##x264_Tune", &g_media_editor_settings.OutputVideoCodecTuneIndex, x264_tune, IM_ARRAYSIZE(x264_tune));
                    has_bit_rate = has_gop_size = has_b_frame = true;
                }
                else if (g_currVidEncDescList[g_media_editor_settings.OutputVideoCodecTypeIndex].codecName.compare("libx265") == 0)
                {
                    // libx265 setting
                    if (g_media_editor_settings.OutputVideoCodecProfileIndex == -1) g_media_editor_settings.OutputVideoCodecProfileIndex = 0;
                    if (g_media_editor_settings.OutputVideoCodecPresetIndex == -1) g_media_editor_settings.OutputVideoCodecPresetIndex = 5;
                    if (g_media_editor_settings.OutputVideoCodecTuneIndex == -1) g_media_editor_settings.OutputVideoCodecTuneIndex = 4;
                    ImGui::Combo("Codec Profile##x265_profile", &g_media_editor_settings.OutputVideoCodecProfileIndex, x265_profile, IM_ARRAYSIZE(x265_profile));
                    ImGui::Combo("Codec Preset##x265_preset", &g_media_editor_settings.OutputVideoCodecPresetIndex, x265_preset, IM_ARRAYSIZE(x265_preset));
                    ImGui::Combo("Codec Tune##x265_Tune", &g_media_editor_settings.OutputVideoCodecTuneIndex, x265_tune, IM_ARRAYSIZE(x265_tune));
                    has_bit_rate = has_gop_size = has_b_frame = true;
                }
                else if (g_currVidEncDescList[g_media_editor_settings.OutputVideoCodecTypeIndex].codecName.compare("h264_videotoolbox") == 0)
                {
                    if (g_media_editor_settings.OutputVideoCodecProfileIndex == -1) g_media_editor_settings.OutputVideoCodecProfileIndex = 0;
                    ImGui::Combo("Codec Profile##v264_profile", &g_media_editor_settings.OutputVideoCodecProfileIndex, v264_profile, IM_ARRAYSIZE(v264_profile));
                    has_bit_rate = has_gop_size = has_b_frame = true;
                }
                else if (g_currVidEncDescList[g_media_editor_settings.OutputVideoCodecTypeIndex].codecName.compare("hevc_videotoolbox") == 0)
                {
                    if (g_media_editor_settings.OutputVideoCodecProfileIndex == -1) g_media_editor_settings.OutputVideoCodecProfileIndex = 0;
                    ImGui::Combo("Codec Profile##v265_profile", &g_media_editor_settings.OutputVideoCodecProfileIndex, v265_profile, IM_ARRAYSIZE(v265_profile));
                    has_bit_rate = has_gop_size = has_b_frame = true;
                }
                else
                {
                    for (auto opt : g_currVidEncDescList[g_media_editor_settings.OutputVideoCodecTypeIndex].optDescList)
                    {
                        if (opt.name.compare("profile") == 0)
                        {
                            if (g_media_editor_settings.OutputVideoCodecProfileIndex == -1)
                            {
                                for (int i = 0; i < opt.enumValues.size(); i++)
                                {
                                    if (opt.defaultValue.numval.i64 == opt.enumValues[i].value)
                                    {
                                        g_media_editor_settings.OutputVideoCodecProfileIndex = i;
                                        break;
                                    }
                                }
                            }
                            ImGui::Combo("Codec Profile##video_codec_profile", &g_media_editor_settings.OutputVideoCodecProfileIndex, codec_option_getter, (void *)&opt.enumValues, opt.enumValues.size());
                        }
                        if (opt.name.compare("tune") == 0)
                        {
                            if (g_media_editor_settings.OutputVideoCodecTuneIndex == -1)
                            {
                                for (int i = 0; i < opt.enumValues.size(); i++)
                                {
                                    if (opt.defaultValue.numval.i64 == opt.enumValues[i].value)
                                    {
                                        g_media_editor_settings.OutputVideoCodecTuneIndex = i;
                                        break;
                                    }
                                }
                            }
                            ImGui::Combo("Codec Tune##video_codec_tune", &g_media_editor_settings.OutputVideoCodecTuneIndex, codec_option_getter, (void *)&opt.enumValues, opt.enumValues.size());
                        }
                        if (opt.name.compare("preset") == 0 || opt.name.compare("usage") == 0)
                        {
                            if (g_media_editor_settings.OutputVideoCodecPresetIndex == -1)
                            {
                                for (int i = 0; i < opt.enumValues.size(); i++)
                                {
                                    if (opt.defaultValue.numval.i64 == opt.enumValues[i].value)
                                    {
                                        g_media_editor_settings.OutputVideoCodecPresetIndex = i;
                                        break;
                                    }
                                }
                            }
                            ImGui::Combo("Codec Preset##video_codec_preset", &g_media_editor_settings.OutputVideoCodecPresetIndex, codec_option_getter, (void *)&opt.enumValues, opt.enumValues.size());
                        }
                        if (opt.name.compare("compression") == 0 || opt.name.compare("compression_algo") == 0)
                        {
                            if (g_media_editor_settings.OutputVideoCodecCompressionIndex == -1)
                            {
                                for (int i = 0; i < opt.enumValues.size(); i++)
                                {
                                    if (opt.defaultValue.numval.i64 == opt.enumValues[i].value)
                                    {
                                        g_media_editor_settings.OutputVideoCodecCompressionIndex = i;
                                        break;
                                    }
                                }
                            }
                            ImGui::Combo("Codec Compression##video_codec_compression", &g_media_editor_settings.OutputVideoCodecCompressionIndex, codec_option_getter, (void *)&opt.enumValues, opt.enumValues.size());
                        }
                        if (opt.tag.compare("gop size") == 0) has_gop_size = true;
                        if (opt.tag.compare("b frames") == 0) has_b_frame = true;
                        if (has_gop_size || has_b_frame)
                        {
                            has_bit_rate = true;
                        }
                    }
                }
            }
        }

        // Video codec global
        ImGui::TextUnformatted("Video Setting: "); ImGui::SameLine(0.f, 0.f);
        static char buf_res_x[64] = {0}; snprintf(buf_res_x, 64, "%d", g_media_editor_settings.OutputVideoResolutionWidth);
        static char buf_res_y[64] = {0}; snprintf(buf_res_y, 64, "%d", g_media_editor_settings.OutputVideoResolutionHeight);
        static char buf_par_x[64] = {0}; snprintf(buf_par_x, 64, "%d", g_media_editor_settings.OutputVideoPixelAspectRatio.num);
        static char buf_par_y[64] = {0}; snprintf(buf_par_y, 64, "%d", g_media_editor_settings.OutputVideoPixelAspectRatio.den);
        static char buf_fmr_x[64] = {0}; snprintf(buf_fmr_x, 64, "%d", g_media_editor_settings.OutputVideoFrameRate.num);
        static char buf_fmr_y[64] = {0}; snprintf(buf_fmr_y, 64, "%d", g_media_editor_settings.OutputVideoFrameRate.den);

        ImGui::Checkbox("as Timeline##video_setting", &g_media_editor_settings.OutputVideoSettingAsTimeline);
        if (g_media_editor_settings.OutputVideoSettingAsTimeline)
        {
            g_media_editor_settings.OutputVideoResolutionIndex = GetResolutionIndex(g_media_editor_settings.VideoWidth, g_media_editor_settings.VideoHeight);
            g_media_editor_settings.OutputVideoResolutionWidth = g_media_editor_settings.VideoWidth;
            g_media_editor_settings.OutputVideoResolutionHeight = g_media_editor_settings.VideoHeight;
            g_media_editor_settings.OutputVideoPixelAspectRatioIndex = GetPixelAspectRatioIndex(g_media_editor_settings.PixelAspectRatio);
            g_media_editor_settings.OutputVideoPixelAspectRatio = g_media_editor_settings.PixelAspectRatio;
            g_media_editor_settings.OutputVideoFrameRateIndex = GetVideoFrameIndex(g_media_editor_settings.VideoFrameRate);
            g_media_editor_settings.OutputVideoFrameRate = g_media_editor_settings.VideoFrameRate;
            g_media_editor_settings.OutputColorSpaceIndex = g_media_editor_settings.ColorSpaceIndex;
            g_media_editor_settings.OutputColorTransferIndex = g_media_editor_settings.ColorTransferIndex;
        }
        ImGui::BeginDisabled(g_media_editor_settings.OutputVideoSettingAsTimeline);
            if (ImGui::Combo("Resolution", &g_media_editor_settings.OutputVideoResolutionIndex, resolution_items, IM_ARRAYSIZE(resolution_items)))
            {
                SetResolution(g_media_editor_settings.OutputVideoResolutionWidth, g_media_editor_settings.OutputVideoResolutionHeight, g_media_editor_settings.OutputVideoResolutionIndex);
            }
            ImGui::BeginDisabled(g_media_editor_settings.OutputVideoResolutionIndex != 0);
                ImGui::PushItemWidth(60);
                ImGui::InputText("##Output_Resolution_x", buf_res_x, 64, ImGuiInputTextFlags_CharsDecimal);
                ImGui::SameLine();
                ImGui::TextUnformatted("X");
                ImGui::SameLine();
                ImGui::InputText("##Output_Resolution_y", buf_res_y, 64, ImGuiInputTextFlags_CharsDecimal);
                ImGui::PopItemWidth();
            ImGui::EndDisabled(); // disable if resolution not custom
            if (g_media_editor_settings.OutputVideoResolutionIndex == 0)
            {
                g_media_editor_settings.OutputVideoResolutionWidth = atoi(buf_res_x);
                g_media_editor_settings.OutputVideoResolutionHeight = atoi(buf_res_y);
            }

            if (ImGui::Combo("Pixel Aspect Ratio", &g_media_editor_settings.OutputVideoPixelAspectRatioIndex, pixel_aspect_items, IM_ARRAYSIZE(pixel_aspect_items)))
            {
                SetPixelAspectRatio(g_media_editor_settings.OutputVideoPixelAspectRatio, g_media_editor_settings.OutputVideoPixelAspectRatioIndex);
            }
            ImGui::BeginDisabled(g_media_editor_settings.OutputVideoPixelAspectRatioIndex != 0);
                ImGui::PushItemWidth(60);
                ImGui::InputText("##OutputPixelAspectRatio_x", buf_par_x, 64, ImGuiInputTextFlags_CharsDecimal);
                ImGui::SameLine();
                ImGui::TextUnformatted(":");
                ImGui::SameLine();
                ImGui::InputText("##OutputPixelAspectRatio_y", buf_par_y, 64, ImGuiInputTextFlags_CharsDecimal);
                ImGui::PopItemWidth();
            ImGui::EndDisabled(); // disable if pixel aspact ratio is not custom
            if (g_media_editor_settings.OutputVideoPixelAspectRatioIndex == 0)
            {
                g_media_editor_settings.OutputVideoPixelAspectRatio.num = atoi(buf_par_x);
                g_media_editor_settings.OutputVideoPixelAspectRatio.den = atoi(buf_par_y);
            }

            if (ImGui::Combo("Video Frame Rate", &g_media_editor_settings.OutputVideoFrameRateIndex, frame_rate_items, IM_ARRAYSIZE(frame_rate_items)))
            {
                SetVideoFrameRate(g_media_editor_settings.OutputVideoFrameRate, g_media_editor_settings.OutputVideoFrameRateIndex);
            }
            ImGui::BeginDisabled(g_media_editor_settings.OutputVideoFrameRateIndex != 0);
                ImGui::PushItemWidth(60);
                ImGui::InputText("##OutputVideoFrameRate_x", buf_fmr_x, 64, ImGuiInputTextFlags_CharsDecimal);
                ImGui::SameLine();
                ImGui::TextUnformatted(":");
                ImGui::SameLine();
                ImGui::InputText("##OutputVideoFrameRate_y", buf_fmr_y, 64, ImGuiInputTextFlags_CharsDecimal);
                ImGui::PopItemWidth();
            ImGui::EndDisabled(); // disable if frame rate is not custom
            if (g_media_editor_settings.OutputVideoFrameRateIndex == 0)
            {
                g_media_editor_settings.OutputVideoFrameRate.num = atoi(buf_fmr_x);
                g_media_editor_settings.OutputVideoFrameRate.den = atoi(buf_fmr_y);
            }
            ImGui::Combo("Color Space", &g_media_editor_settings.OutputColorSpaceIndex, color_getter, (void *)&ColorSpace ,IM_ARRAYSIZE(ColorSpace));
            ImGui::Combo("Color Transfer", &g_media_editor_settings.OutputColorTransferIndex, color_getter, (void *)&ColorTransfer ,IM_ARRAYSIZE(ColorTransfer));
        ImGui::EndDisabled(); // disable if param as timline

        if (has_bit_rate)
        {
            if (g_media_editor_settings.OutputVideoBitrate == -1)
            {
                g_media_editor_settings.OutputVideoBitrate = 
                    (int64_t)g_media_editor_settings.OutputVideoResolutionWidth * (int64_t)g_media_editor_settings.OutputVideoResolutionHeight *
                    (int64_t)g_media_editor_settings.OutputVideoFrameRate.num / (int64_t)g_media_editor_settings.OutputVideoFrameRate.den / 10;
            }

            ImGui::InputInt("Bitrate##video", &g_media_editor_settings.OutputVideoBitrate, 1000, 1000000, ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::Combo("Bitrate Strategy##video", &g_media_editor_settings.OutputVideoBitrateStrategyindex, "CBR\0VBR\0");
        }
        else
            g_media_editor_settings.OutputVideoBitrate = -1;
        if (has_gop_size)
        {
            if (g_media_editor_settings.OutputVideoGOPSize == -1)
                g_media_editor_settings.OutputVideoGOPSize = 12;
            ImGui::InputInt("GOP Size##video", &g_media_editor_settings.OutputVideoGOPSize, 1, 1, ImGuiInputTextFlags_EnterReturnsTrue);
        }
        else
            g_media_editor_settings.OutputVideoGOPSize = -1;
        if (has_b_frame)
        {
            if (g_media_editor_settings.OutputVideoBFrames == 0)
                g_media_editor_settings.OutputVideoBFrames = 2;
            ImGui::InputInt("B Frames##video", &g_media_editor_settings.OutputVideoBFrames, 1, 1, ImGuiInputTextFlags_EnterReturnsTrue);
        }
        else
            g_media_editor_settings.OutputVideoBFrames = 0;
        ImGui::EndDisabled(); // disable if disable video
        ImGui::Separator();

        // Audio Setting
        ImGui::Dummy(ImVec2(0, 20));
        ImGui::Checkbox("Export Audio##export_audio", &timeline->bExportAudio);
        ImGui::Separator();
        if (timeline->bExportAudio) ImGui::BeginDisabled(false); else ImGui::BeginDisabled(true);
        
        // audio codec select
        if (ImGui::Combo("Codec##audio_codec", &g_media_editor_settings.OutputAudioCodecIndex, codec_getter, (void *)OutputAudioCodec, IM_ARRAYSIZE(OutputAudioCodec)))
        {
            g_audEncSelChanged = true;
            g_media_editor_settings.OutputAudioCodecTypeIndex = 0;  // reset codec type if we change codec
        }
        // audio codec type select
        if (OutputAudioCodec[g_media_editor_settings.OutputAudioCodecIndex].name.compare("PCM") == 0)
        {
            ImGui::Combo("Codec Type##pcm_audio_codec", &g_media_editor_settings.OutputAudioCodecTypeIndex, codec_getter, (void *)OutputAudioCodecPCM, IM_ARRAYSIZE(OutputAudioCodecPCM));
        }
        else
        {
            if (g_audEncSelChanged)
            {
                std::string codecHint = OutputAudioCodec[g_media_editor_settings.OutputAudioCodecIndex].codec;
                if (!MediaEncoder::FindEncoder(codecHint, g_currAudEncDescList))
                {
                    g_currAudEncDescList.clear();
                }
                g_audEncSelChanged = false;
            }

            ImGui::Combo("Codec Type##audio_codec_type", &g_media_editor_settings.OutputAudioCodecTypeIndex, codec_type_getter, (void *)&g_currAudEncDescList, g_currAudEncDescList.size());
        }
        // Audio codec global
        ImGui::TextUnformatted("Audio Setting: "); ImGui::SameLine(0.f, 0.f);
        ImGui::Checkbox("as Timeline##audio_setting", &g_media_editor_settings.OutputAudioSettingAsTimeline);
        if (g_media_editor_settings.OutputAudioSettingAsTimeline)
        {
            g_media_editor_settings.OutputAudioSampleRateIndex = GetSampleRateIndex(g_media_editor_settings.AudioSampleRate);
            g_media_editor_settings.OutputAudioSampleRate = g_media_editor_settings.AudioSampleRate;
            g_media_editor_settings.OutputAudioChannelsIndex = GetChannelIndex(g_media_editor_settings.AudioChannels);
            g_media_editor_settings.OutputAudioChannels = g_media_editor_settings.AudioChannels;
        }
        ImGui::BeginDisabled(g_media_editor_settings.OutputAudioSettingAsTimeline);
        if (ImGui::Combo("Audio Sample Rate", &g_media_editor_settings.OutputAudioSampleRateIndex, audio_sample_rate_items, IM_ARRAYSIZE(audio_sample_rate_items)))
        {
            SetSampleRate(g_media_editor_settings.OutputAudioSampleRate, g_media_editor_settings.OutputAudioSampleRateIndex);
        }
        if (ImGui::Combo("Audio Channels", &g_media_editor_settings.OutputAudioChannelsIndex, audio_channels_items, IM_ARRAYSIZE(audio_channels_items)))
        {
            SetAudioChannel(g_media_editor_settings.OutputAudioChannels, g_media_editor_settings.OutputAudioChannelsIndex);
        }
        ImGui::EndDisabled(); // disable if param as timline
        ImGui::EndDisabled(); // disable if no audio
        ImGui::Separator();
    }
    ImGui::EndChild();

    // Make Media dialog
    if (multiviewport)
        ImGui::SetNextWindowViewport(viewport->ID);
    if (ImGui::BeginPopupModal("Make Media##MakeVideoDlyKey", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::TextUnformatted("Output path:"); ImGui::SameLine(0, 10);
        std::string fullpath = timeline->mOutputPath+"/"+timeline->mOutputName
            +"."+OutFormats[g_media_editor_settings.OutputFormatIndex].suffix;
        ImGui::Text("%s", fullpath.c_str());
        ImVec2 preview_size = ImVec2(640, 360);
        ImVec2 preview_pos = ImGui::GetCursorScreenPos();
        float pos_x = 0, pos_y = 0;
        if (timeline->mIsEncoding)
        {
            ImGui::ImMat encMatV;
            {
                std::lock_guard<std::mutex> lk(timeline->mEncodingMutex);
                encMatV = timeline->mEncodingVFrame;
            }
            if (!encMatV.empty())
            {
                ImGui::ImMatToTexture(encMatV, timeline->mEncodingPreviewTexture);
            }
            if (timeline->mEncodingPreviewTexture)
            {
                ShowVideoWindow(timeline->mEncodingPreviewTexture, preview_pos, preview_size);
            }
            else
            {
                ImGui::Dummy(preview_size);
            }
        }
        else if (timeline->mEncodingPreviewTexture)
        {
            ShowVideoWindow(timeline->mEncodingPreviewTexture, preview_pos, preview_size);
        }
        else if (timeline->mMainPreviewTexture)
        {
            ShowVideoWindow(timeline->mMainPreviewTexture, preview_pos, preview_size);
        }
        else
        {
            ImGui::Dummy(preview_size);
        }
        if (timeline->mIsEncoding)
            ImGui::SpinnerDnaDots("SpinnerEncoding", 12, 3, ImColor(255, 255, 255), 8, 8, 0.25f, true);
        else
            ImGui::Dummy(ImVec2(32, 32));
        ImGui::SameLine();
        ImGui::ProgressBar("##encoding_progress",timeline->mEncodingProgress, 0.f, 1.f, "%1.1f%%", ImVec2(540, 16), 
                                ImVec4(1.f, 1.f, 1.f, 1.f), ImVec4(0.f, 0.f, 0.f, 1.f), ImVec4(1.f, 1.f, 1.f, 1.f));

        auto valid_duration = timeline->ValidDuration();

        if (encoder_stage == 2 && encode_duration > 0)
        {
            ImGui::TextUnformatted("Encoding finished. Spend Time:"); ImGui::SameLine();
            ImGui::Text("%s", ImGuiHelper::MillisecToString(encode_duration * 1000, 3).c_str());
            ImGui::SameLine();
            ImGui::TextUnformatted("Speed:"); ImGui::SameLine();
            float encoding_speed = timeline->mEncodingDuration / (encode_duration + FLT_EPSILON);
            ImGui::Text("%.2fx", encoding_speed);
        }
        else if (timeline->mIsEncoding)
        {
            encoder_end = ImGui::get_current_time();
            encode_duration = encoder_end - encoder_start;
            float encoding_time = timeline->mEncodingProgress * timeline->mEncodingDuration;
            float encoding_speed = encoding_time / (encode_duration + FLT_EPSILON);
            float estimated_time = (1.f - timeline->mEncodingProgress) * timeline->mEncodingDuration / (encoding_speed + FLT_EPSILON);
            ImGui::TextUnformatted("Speed:"); ImGui::SameLine(); ImGui::Text("%.2fx", encoding_speed); ImGui::SameLine();
            ImGui::TextUnformatted("Estimated:"); ImGui::SameLine(); ImGui::Text("%s", ImGuiHelper::MillisecToString(estimated_time * 1000, 1).c_str());
        }
        else
        {
            ImGui::TextUnformatted("Output duration:");
            ImGui::SameLine();
            ImGui::Text("%s", ImGuiHelper::MillisecToString(valid_duration, 2).c_str());
        }

        const ImVec2 btnPaddingSize { 30, 14 };
        std::string btnText;
        ImVec2 btnTxtSize;
        btnText = "Ok";
        btnTxtSize = ImGui::CalcTextSize(btnText.c_str());
        ImGui::BeginDisabled(timeline->mIsEncoding);
        if (ImGui::Button(btnText.c_str(), btnTxtSize + btnPaddingSize))
        {
            if (timeline->mEncodingPreviewTexture) { ImGui::ImDestroyTexture(timeline->mEncodingPreviewTexture); timeline->mEncodingPreviewTexture = nullptr; }
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        
        btnText = timeline->mIsEncoding ? "Stop encoding" : "Start encoding";
        btnTxtSize = ImGui::CalcTextSize(btnText.c_str());
        if (encoder_stage != 2 && ImGui::Button(btnText.c_str(), btnTxtSize + btnPaddingSize))
        {
            if (timeline->mIsEncoding)
            {
                timeline->StopEncoding();
                encoder_stage = 2;
            }
            else if (valid_duration > 0)
            {
                // config encoders
                TimeLine::VideoEncoderParams vidEncParams;
                vidEncParams.codecName = g_currVidEncDescList[g_media_editor_settings.OutputVideoCodecTypeIndex].codecName;
                vidEncParams.width = g_media_editor_settings.OutputVideoResolutionWidth;
                vidEncParams.height = g_media_editor_settings.OutputVideoResolutionHeight;
                vidEncParams.frameRate = g_media_editor_settings.OutputVideoFrameRate;
                vidEncParams.bitRate = g_media_editor_settings.OutputVideoBitrate;
                TimeLine::AudioEncoderParams audEncParams;
                audEncParams.codecName = g_currAudEncDescList[g_media_editor_settings.OutputAudioCodecTypeIndex].codecName;
                audEncParams.channels = g_media_editor_settings.OutputAudioChannels;
                audEncParams.sampleRate = g_media_editor_settings.OutputAudioSampleRate;
                audEncParams.bitRate = 128000;
                if (timeline->ConfigEncoder(fullpath, vidEncParams, audEncParams, g_encoderConfigErrorMessage))
                {
                    timeline->StartEncoding();
                    encode_duration = -1;
                    encoder_start = ImGui::get_current_time();
                    encoder_stage = 1;
                }
            }
        }
        if (timeline->mark_in != -1 && timeline->mark_out != -1 && encoder_stage != 2)
        {
            ImGui::SameLine();
            ImGui::BeginDisabled(timeline->mIsEncoding);
            ImGui::Checkbox("Encoding in mark range", &timeline->mEncodingInRange);
            ImGui::EndDisabled();
        }
        else
        {
            timeline->mEncodingInRange = false;
        }
        if (!g_encoderConfigErrorMessage.empty())
        {
            ImGui::TextColored({1., 0.2, 0.2, 1.}, "%s", g_encoderConfigErrorMessage.c_str());
        }

        if (!timeline->mEncodeProcErrMsg.empty())
        {
            ImGui::TextColored({1., 0.5, 0.5, 1.}, "%s", timeline->mEncodeProcErrMsg.c_str());
        }

        if (!timeline->mIsEncoding && encoder_start > 0)
        {
            encoder_end = encoder_start = -1;
            encoder_stage = 2;
        }
        ImGui::EndPopup();
    }

    // File dialog
    ImVec2 minSize = ImVec2(600, 600);
	ImVec2 maxSize = ImVec2(FLT_MAX, FLT_MAX);
    if (multiviewport)
        ImGui::SetNextWindowViewport(viewport->ID);
    if (ImGuiFileDialog::Instance()->Display("##MediaEditOutputPathDlgKey", ImGuiWindowFlags_NoCollapse, minSize, maxSize))
    {
        if (ImGuiFileDialog::Instance()->IsOk())
        {
            auto file_path = ImGuiFileDialog::Instance()->GetFilePathName();
            timeline->mOutputPath = file_path;
        }
        ImGuiFileDialog::Instance()->Close();
    }
}

/****************************************************************************************
 * 
 * Media Preview window
 *
 ***************************************************************************************/
static void ShowMediaPreviewWindow(ImDrawList *draw_list, std::string title, ImRect& video_rect, int64_t start = -1, int64_t end = -1, bool audio_bar = true, bool monitors = true, bool force_update = false, bool small = false, bool zoom_button = true, bool loop_button = true)
{
    // preview control pannel
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 window_pos = ImGui::GetCursorScreenPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    draw_list->AddRectFilled(window_pos, window_pos + window_size, COL_DEEP_DARK);
    ImVec2 PanelBarPos = window_pos + window_size - ImVec2(window_size.x, 48);
    ImVec2 PanelBarSize = ImVec2(window_size.x, 48);
    draw_list->AddRectFilled(PanelBarPos, PanelBarPos + PanelBarSize, COL_DARK_PANEL);

    // Preview buttons Stop button is center of Panel bar
    auto PanelCenterX = PanelBarPos.x + window_size.x / 2;
    auto PanelButtonY = PanelBarPos.y + 8;

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0.5));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2, 0.2, 0.2, 1.0));

    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX - 16 - (32 + 8) * 3, PanelButtonY));
    if (ImGui::Button(ICON_TO_START "##preview_tostart", ImVec2(32, 32)))
    {
        if (timeline)
        {
            if (start < 0) timeline->ToStart();
            else timeline->Seek(start);
        }            
    }
    ImGui::ShowTooltipOnHover("To Start");

    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX - 16 - (32 + 8) * 2, PanelButtonY));
    if (ImGui::Button(ICON_STEP_BACKWARD "##preview_step_backward", ImVec2(32, 32)))
    {
        if (timeline)
        {
            if (start < 0) timeline->Step(false);
            else if (timeline->currentTime > start) timeline->Step(false);
        }
    }
    ImGui::ShowTooltipOnHover("Step Prev");

    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX - 16 - (32 + 8) * 1, PanelButtonY));
    if (ImGui::RotateButton(ICON_PLAY_BACKWARD "##preview_reverse", ImVec2(32, 32), 180))
    {
        if (timeline)
        {
            if (start < 0) timeline->Play(true, false);
            else if (timeline->currentTime > start) timeline->Play(true, false);
        }
    }
    ImGui::ShowTooltipOnHover("Reverse");

    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX - 16, PanelButtonY));
    if (ImGui::Button(ICON_STOP "##preview_stop", ImVec2(32, 32)))
    {
        if (timeline) timeline->Play(false, true);
    }
    ImGui::ShowTooltipOnHover("Stop");

    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX + 16 + 8, PanelButtonY));
    if (ImGui::Button(ICON_PLAY_FORWARD "##preview_play", ImVec2(32, 32)))
    {
        if (timeline)
        {
            if (start < 0 || end < 0) timeline->Play(true, true);
            else if (timeline->currentTime < end) timeline->Play(true, true);
        }
    }
    ImGui::ShowTooltipOnHover("Play");

    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX + 16 + 8 + (32 + 8) * 1, PanelButtonY));
    if (ImGui::Button(ICON_STEP_FORWARD "##preview_step_forward", ImVec2(32, 32)))
    {
        if (timeline)
        {
            if (end < 0) timeline->Step(true);
            else if (timeline->currentTime < end) timeline->Step(true);
        }
    }
    ImGui::ShowTooltipOnHover("Step Next");

    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX + 16 + 8 + (32 + 8) * 2, PanelButtonY));
    if (ImGui::Button(ICON_TO_END "##preview_toend", ImVec2(32, 32)))
    {
        if (timeline)
        {
            if (end < 0) timeline->ToEnd();
            else timeline->Seek(end);
        }
    }
    ImGui::ShowTooltipOnHover("To End");

    if (loop_button)
    {
        bool loop = timeline ? timeline->bLoop : false;
        ImGui::SetCursorScreenPos(ImVec2(PanelCenterX + 16 + 8 + (32 + 8) * 3, PanelButtonY + 4));
        if (ImGui::Button(loop ? ICON_LOOP : ICON_LOOP_ONE "##preview_loop", ImVec2(32, 32)))
        {
            if (timeline)
            {
                loop = !loop;
                timeline->Loop(loop);
            }
        }
        ImGui::ShowTooltipOnHover("Loop");
    }

    if (zoom_button)
    {
        bool zoom = timeline ? timeline->bPreviewZoom : false;
        ImGui::SetCursorScreenPos(ImVec2(PanelCenterX + 16 + 8 + (32 + 8) * 4, PanelButtonY + 8));
        if (ImGui::CheckButton(ICON_ZOOM "##preview_zoom", &zoom, ImVec4(0.5, 0.5, 0.0, 1.0)))
        {
            timeline->bPreviewZoom = zoom;
        }
        ImGui::ShowTooltipOnHover("Magnifying");
    }


    // Time stamp on left of control panel
    auto PanelRightX = PanelBarPos.x + window_size.x - (small ? 60 : 150);
    auto PanelRightY = PanelBarPos.y + (small ? 16 : 8);
    auto time_str = ImGuiHelper::MillisecToString(timeline->currentTime, 3);
    ImGui::SetWindowFontScale(small ? 1.0 : 1.5);
    draw_list->AddText(ImVec2(PanelRightX, PanelRightY), timeline->mIsPreviewPlaying ? COL_MARK : COL_MARK_HALF, time_str.c_str());
    ImGui::SetWindowFontScale(1.0);

    // audio meters
    if (audio_bar)
    {
        ImVec2 AudioMeterPos;
        ImVec2 AudioMeterSize;
        AudioMeterPos = window_pos + ImVec2(window_size.x - 70, 16);
        AudioMeterSize = ImVec2(32, window_size.y - 48 - 16 - 8);
        ImVec2 AudioUVLeftPos = AudioMeterPos + ImVec2(36, 0);
        ImVec2 AudioUVLeftSize = ImVec2(12, AudioMeterSize.y);
        ImVec2 AudioUVRightPos = AudioMeterPos + ImVec2(36 + 16, 0);
        ImVec2 AudioUVRightSize = AudioUVLeftSize;

        draw_list->AddRectFilled(AudioMeterPos - ImVec2(0, 16), AudioMeterPos + ImVec2(70, AudioMeterSize.y + 8), COL_DARK_TWO);

        for (int i = 0; i <= 96; i+= 5)
        {
            float mark_step = AudioMeterSize.y / 96.0f;
            ImVec2 MarkPos = AudioMeterPos + ImVec2(0, i * mark_step);
            if (i % 10 == 0)
            {
                std::string mark_str = i == 0 ? "  0" : "-" + std::to_string(i);
                draw_list->AddLine(MarkPos + ImVec2(20, 8), MarkPos + ImVec2(30, 8), COL_MARK_HALF, 1);
                ImGui::SetWindowFontScale(0.75);
                draw_list->AddText(MarkPos + ImVec2(0, 2), COL_MARK_HALF, mark_str.c_str());
                ImGui::SetWindowFontScale(1.0);
            }
            else
            {
                draw_list->AddLine(MarkPos + ImVec2(25, 8), MarkPos + ImVec2(30, 8), COL_MARK_HALF, 1);
            }
        }

        int l_level = timeline->GetAudioLevel(0);
        int r_level = timeline->GetAudioLevel(1);
        ImGui::SetCursorScreenPos(AudioUVLeftPos);
        ImGui::UvMeter("##luv", AudioUVLeftSize, &l_level, 0, 96, AudioUVLeftSize.y / 4, &timeline->mAudioAttribute.left_stack, &timeline->mAudioAttribute.left_count, 0.2, audio_bar_seg);
        ImGui::SetCursorScreenPos(AudioUVRightPos);
        ImGui::UvMeter("##ruv", AudioUVRightSize, &r_level, 0, 96, AudioUVRightSize.y / 4, &timeline->mAudioAttribute.right_stack, &timeline->mAudioAttribute.right_count, 0.2, audio_bar_seg);
    }

    // video texture area
    ImVec2 PreviewPos;
    ImVec2 PreviewSize;
    PreviewPos = window_pos + ImVec2(8, 8);
    PreviewSize = window_size - ImVec2(16 + (audio_bar ? 64 : 0), 16 + 48);
    auto frames = timeline->GetPreviewFrame();
    ImGui::ImMat frame;
    if (!frames.empty())
        frame = frames[0].frame;
    if ((timeline->mIsPreviewNeedUpdate || timeline->mLastFrameTime == -1 || timeline->mLastFrameTime != (int64_t)(frame.time_stamp * 1000) || need_update_scope || force_update))
    {
        if (!frame.empty())
        {
            CalculateVideoScope(frame);
            ImGui::ImMatToTexture(frame, timeline->mMainPreviewTexture);
            timeline->mLastFrameTime = frame.time_stamp * 1000;
            timeline->mIsPreviewNeedUpdate = false;
        }
    }

    if (start > 0 && end > 0)
    {
        if (timeline->mIsPreviewPlaying && (timeline->currentTime < start || timeline->currentTime > end))
        {
            // reach clip border
            if (timeline->currentTime < start) { timeline->Play(false, false); timeline->Seek(start); }
            if (timeline->currentTime > end) { timeline->Play(false, true); timeline->Seek(end); }
        }
    }

    float pos_x = 0, pos_y = 0;
    float offset_x = 0, offset_y = 0;
    float tf_x = 0, tf_y = 0;
    ImVec2 scale_range = ImVec2(2.0 / timeline->mPreviewScale, 8.0 / timeline->mPreviewScale);
    static float texture_zoom = scale_range.x;
    ShowVideoWindow(draw_list, timeline->mMainPreviewTexture, PreviewPos, PreviewSize, offset_x, offset_y, tf_x, tf_y);
    if (ImGui::IsItemHovered() && timeline->bPreviewZoom && timeline->mMainPreviewTexture)
    {
        ImVec4 tint_col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);   // No tint
        ImVec4 border_col = ImVec4(1.0f, 1.0f, 1.0f, 0.5f); // 50% opaque white
        float region_sz = 480.0f / texture_zoom;
        float image_width = ImGui::ImGetTextureWidth(timeline->mMainPreviewTexture);
        float image_height = ImGui::ImGetTextureHeight(timeline->mMainPreviewTexture);
        float scale_w = image_width / (tf_x - offset_x);
        float scale_h = image_height / (tf_y - offset_y);
        pos_x = (io.MousePos.x - offset_x) * scale_w;
        pos_y = (io.MousePos.y - offset_y) * scale_h;
        float region_x = pos_x - region_sz * 0.5f;
        float region_y = pos_y - region_sz * 0.5f;
        if (region_x < 0.0f) { region_x = 0.0f; }
        else if (region_x > image_width - region_sz) { region_x = image_width - region_sz; }
        if (region_y < 0.0f) { region_y = 0.0f; }
        else if (region_y > image_height - region_sz) { region_y = image_height - region_sz; }
        ImGui::SetNextWindowBgAlpha(1.0);
        if (ImGui::BeginTooltip())
        {
            ImVec2 uv0 = ImVec2((region_x) / image_width, (region_y) / image_height);
            ImVec2 uv1 = ImVec2((region_x + region_sz) / image_width, (region_y + region_sz) / image_height);
            ImGui::Image(timeline->mMainPreviewTexture, ImVec2(region_sz * texture_zoom, region_sz * texture_zoom), uv0, uv1, tint_col, border_col);
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
    video_rect.Min = ImVec2(offset_x, offset_y);
    video_rect.Max = ImVec2(tf_x, tf_y);
    if (monitors)
    {
        // Show monitors
        std::vector<int> disabled_monitor;
        MonitorButton("preview_monitor_select", ImVec2(PanelBarPos.x + 20, PanelBarPos.y + 16), MonitorIndexPreviewVideo, disabled_monitor, false, true);
    }

    ImGui::PopStyleColor(3);

    ImGui::SetCursorScreenPos(window_pos + ImVec2(40, 30));
    ImGui::TextComplex(title.c_str(), 2.0f, ImVec4(0.8, 0.8, 0.8, 0.2),
                        0.1f, ImVec4(0.8, 0.8, 0.8, 0.3),
                        ImVec2(4, 4), ImVec4(0.0, 0.0, 0.0, 0.5));
}

/****************************************************************************************
 * 
 * Media Filter Preview window
 *
 ***************************************************************************************/
static void ShowVideoFilterPreviewWindow(ImDrawList *draw_list, int64_t start, int64_t end, bool attribute = false)
{
    // preview control pannel
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 window_pos = ImGui::GetCursorScreenPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    draw_list->AddRectFilled(window_pos, window_pos + window_size, COL_DEEP_DARK);
    ImVec2 PanelBarPos = window_pos + window_size - ImVec2(window_size.x, 48);
    ImVec2 PanelBarSize = ImVec2(window_size.x, 48);
    draw_list->AddRectFilled(PanelBarPos, PanelBarPos + PanelBarSize, COL_DARK_PANEL);

    // Preview buttons Stop button is center of Panel bar
    auto PanelCenterX = PanelBarPos.x + window_size.x / 2;
    auto PanelButtonY = PanelBarPos.y + 8;

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0.5));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2, 0.2, 0.2, 1.0));

    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX - 16 - (32 + 8) * 3, PanelButtonY));
    if (ImGui::Button(ICON_TO_START "##preview_tostart", ImVec2(32, 32)))
    {
        if (timeline && timeline->mVidFilterClip)
        {
            timeline->Seek(start);
        }
    }
    ImGui::ShowTooltipOnHover("To Start");

    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX - 16 - (32 + 8) * 2, PanelButtonY));
    if (ImGui::Button(ICON_STEP_BACKWARD "##preview_step_backward", ImVec2(32, 32)))
    {
        if (timeline && timeline->mVidFilterClip)
        {
            if (timeline->currentTime > start)
                timeline->Step(false);
        }
    }
    ImGui::ShowTooltipOnHover("Step Prev");

    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX - 16 - (32 + 8) * 1, PanelButtonY));
    if (ImGui::RotateButton(ICON_PLAY_BACKWARD "##preview_reverse", ImVec2(32, 32), 180))
    {
        if (timeline && timeline->mVidFilterClip)
        {
            if (timeline->currentTime > start)
                timeline->Play(true, false);
        }
    }
    ImGui::ShowTooltipOnHover("Reverse");

    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX - 16 - (32 + 8) * 0, PanelButtonY));
    if (ImGui::Button(ICON_STOP "##preview_stop", ImVec2(32, 32)))
    {
        if (timeline)
            timeline->Play(false, true);
    }
    ImGui::ShowTooltipOnHover("Stop");

    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX + 16 + 8 + (32 + 8) * 0, PanelButtonY));
    if (ImGui::Button(ICON_PLAY_FORWARD "##preview_play", ImVec2(32, 32)))
    {
        if (timeline && timeline->mVidFilterClip)
        {
            if (timeline->currentTime < end)
                timeline->Play(true, true);
        }
    }
    ImGui::ShowTooltipOnHover("Play");

    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX + 16 + 8 + (32 + 8) * 1, PanelButtonY));
    if (ImGui::Button(ICON_STEP_FORWARD "##preview_step_forward", ImVec2(32, 32)))
    {
        if (timeline && timeline->mVidFilterClip)
        {
            if (timeline->currentTime < end)
                timeline->Step(true);
        }
    }
    ImGui::ShowTooltipOnHover("Step Next");

    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX + 16 + 8 + (32 + 8) * 2, PanelButtonY));
    if (ImGui::Button(ICON_TO_END "##preview_toend", ImVec2(32, 32)))
    {
        if (timeline && timeline->mVidFilterClip)
        {
            if (timeline->currentTime < end)
                timeline->Seek(end - 40);
        }
    }
    ImGui::ShowTooltipOnHover("To End");

    if (attribute)
    {
        ImGui::SetCursorScreenPos(ImVec2(PanelCenterX + 16 + 8 + (32 + 8) * 4, PanelButtonY + 6));
        if (ImGui::CheckButton(timeline->bAttributeOutputPreview ? ICON_MEDIA_PREVIEW : ICON_FILTER "##video_filter_output_preview", &timeline->bAttributeOutputPreview, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive)))
        {
            timeline->UpdatePreview();
        }
        ImGui::ShowTooltipOnHover(timeline->bAttributeOutputPreview ? "Attribute Out" : "Preview Out");
    }
    else
    {
        ImGui::SetCursorScreenPos(ImVec2(PanelCenterX + 16 + 8 + (32 + 8) * 4, PanelButtonY + 6));
        if (ImGui::CheckButton(timeline->bFilterOutputPreview ? ICON_MEDIA_PREVIEW : ICON_FILTER "##video_filter_output_preview", &timeline->bFilterOutputPreview, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive)))
        {
            timeline->UpdatePreview();
        }
        ImGui::ShowTooltipOnHover(timeline->bFilterOutputPreview ? "Filter Out" : "Preview Out");
    }

    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX + 16 + 8 + (32 + 8) * 5, PanelButtonY + 6));
    ImGui::CheckButton(ICON_COMPARE "##video_filter_compare", &timeline->bCompare, ImVec4(0.5, 0.5, 0.0, 1.0));
    ImGui::ShowTooltipOnHover("Zoom Compare");

    // Time stamp on left of control panel
    auto PanelRightX = PanelBarPos.x + window_size.x - 300;
    auto PanelRightY = PanelBarPos.y + 8;
    auto time_str = ImGuiHelper::MillisecToString(timeline->currentTime, 3);
    ImGui::SetWindowFontScale(1.5);
    draw_list->AddText(ImVec2(PanelRightX, PanelRightY), timeline->mIsPreviewPlaying ? COL_MARK : COL_MARK_HALF, time_str.c_str());
    ImGui::SetWindowFontScale(1.0);

    // filter input texture area
    ImVec2 InputVideoPos = window_pos + ImVec2(4, 4);
    ImVec2 InputVideoSize = ImVec2(window_size.x / 2 - 8, window_size.y - PanelBarSize.y - 8);
    ImVec2 OutputVideoPos = window_pos + ImVec2(window_size.x / 2 + 4, 4);
    ImVec2 OutputVideoSize = ImVec2(window_size.x / 2 - 8, window_size.y - PanelBarSize.y - 8);
    ImRect InputVideoRect(InputVideoPos, InputVideoPos + InputVideoSize);
    ImRect OutVideoRect(OutputVideoPos, OutputVideoPos + OutputVideoSize);
    ImVec2 VideoZoomPos = window_pos + ImVec2(0, window_size.y - PanelBarSize.y + 4);
    if (timeline->mVidFilterClip)
    {
        ImVec2 scale_range = ImVec2(0.5 / timeline->mPreviewScale, 4.0 / timeline->mPreviewScale);
        static float texture_zoom = scale_range.x;
        if (InputVideoRect.Contains(io.MousePos) || OutVideoRect.Contains(io.MousePos))
        {
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
        float region_sz = 360.0f / texture_zoom;
        std::pair<ImGui::ImMat, ImGui::ImMat> pair;
        bool ret = false;
        if (attribute)
            ret = timeline->mVidFilterClip->GetFrame(pair, timeline->bAttributeOutputPreview, attribute);
        else
            ret = timeline->mVidFilterClip->GetFrame(pair, timeline->bFilterOutputPreview);
        int64_t output_timestamp = pair.second.time_stamp * 1000;
        if (ret && 
            (timeline->mIsPreviewNeedUpdate || timeline->mLastFrameTime == -1 || timeline->mLastFrameTime != output_timestamp || need_update_scope))
        {
            CalculateVideoScope(pair.second);
            ImGui::ImMatToTexture(pair.first, timeline->mVideoFilterInputTexture);
            ImGui::ImMatToTexture(pair.second, timeline->mVideoFilterOutputTexture);
            timeline->mLastFrameTime = output_timestamp;
            timeline->mIsPreviewNeedUpdate = false;
        }
        if (timeline->mIsPreviewPlaying && (timeline->currentTime < start || timeline->currentTime > end))
        {
            // reach clip border
            if (timeline->currentTime < start) { timeline->Play(false, false); timeline->Seek(start); }
            if (timeline->currentTime > end) { timeline->Play(false, true); timeline->Seek(end); }
        }
        float pos_x = 0, pos_y = 0;
        bool draw_compare = false;
        ImVec4 tint_col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);   // No tint
        ImVec4 border_col = ImVec4(1.0f, 1.0f, 1.0f, 0.5f); // 50% opaque white
        float offset_x = 0, offset_y = 0;
        float tf_x = 0, tf_y = 0;
        // filter input texture area
        ShowVideoWindow(draw_list, timeline->mVideoFilterInputTexture, InputVideoPos, InputVideoSize, offset_x, offset_y, tf_x, tf_y);
        draw_list->AddRect(ImVec2(offset_x, offset_y), ImVec2(tf_x, tf_y), IM_COL32(128,128,128,128), 0, 0, 1.0);
        if (ImGui::IsItemHovered() && timeline->mVideoFilterInputTexture)
        {
            float image_width = ImGui::ImGetTextureWidth(timeline->mVideoFilterInputTexture);
            float image_height = ImGui::ImGetTextureHeight(timeline->mVideoFilterInputTexture);
            float scale_w = image_width / (tf_x - offset_x);
            float scale_h = image_height / (tf_y - offset_y);
            pos_x = (io.MousePos.x - offset_x) * scale_w;
            pos_y = (io.MousePos.y - offset_y) * scale_h;
            if (io.MouseType == 1)
            {
                ImGui::RenderMouseCursor(ICON_STRAW, ImVec2(2, 12));
                draw_list->AddRect(io.MousePos - ImVec2(2, 2), io.MousePos + ImVec2(2, 2), IM_COL32(255,0, 0,255));

                auto pixel = ImGui::ImGetTexturePixel(timeline->mVideoFilterInputTexture, pos_x, pos_y);
                if (ImGui::BeginTooltip())
                {
                    ImGui::ColorButton("##straw_color", ImVec4(pixel.r, pixel.g, pixel.b, pixel.a), 0, ImVec2(64,64));
                    ImGui::Text("x:%d y:%d", (int)pos_x, (int)pos_y);
                    ImGui::Text("R:%d G:%d B:%d A:%d", (int)(pixel.r * 255), (int)(pixel.g * 255), (int)(pixel.b * 255), (int)(pixel.a * 255));
                    ImGui::EndTooltip();
                }
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                {
                    io.MouseStrawed = true;
                    io.MouseStrawValue = ImVec4(pixel.r, pixel.g, pixel.b, pixel.a);
                }
            }
            else
            {
                draw_compare = true;
            }
        }
        // filter output texture area
        ShowVideoWindow(draw_list, timeline->mVideoFilterOutputTexture, OutputVideoPos, OutputVideoSize, offset_x, offset_y, tf_x, tf_y);
        draw_list->AddRect(ImVec2(offset_x, offset_y), ImVec2(tf_x, tf_y), IM_COL32(128,128,128,128), 0, 0, 1.0);
        if (ImGui::IsItemHovered() && timeline->mVideoFilterOutputTexture)
        {
            float image_width = ImGui::ImGetTextureWidth(timeline->mVideoFilterOutputTexture);
            float image_height = ImGui::ImGetTextureHeight(timeline->mVideoFilterOutputTexture);
            float scale_w = image_width / (tf_x - offset_x);
            float scale_h = image_height / (tf_y - offset_y);
            pos_x = (io.MousePos.x - offset_x) * scale_w;
            pos_y = (io.MousePos.y - offset_y) * scale_h;
            draw_compare = true;
        }
        if (timeline->bCompare && draw_compare)
        {
            if (timeline->mVideoFilterInputTexture)
            {
                float image_width = ImGui::ImGetTextureWidth(timeline->mVideoFilterInputTexture);
                float image_height = ImGui::ImGetTextureHeight(timeline->mVideoFilterInputTexture);
                float region_x = pos_x - region_sz * 0.5f;
                float region_y = pos_y - region_sz * 0.5f;
                if (region_x < 0.0f) { region_x = 0.0f; }
                else if (region_x > image_width - region_sz) { region_x = image_width - region_sz; }
                if (region_y < 0.0f) { region_y = 0.0f; }
                else if (region_y > image_height - region_sz) { region_y = image_height - region_sz; }
                ImGui::SetNextWindowPos(VideoZoomPos);
                ImGui::SetNextWindowBgAlpha(1.0);
                if (ImGui::BeginTooltip())
                {
                    ImVec2 uv0 = ImVec2((region_x) / image_width, (region_y) / image_height);
                    ImVec2 uv1 = ImVec2((region_x + region_sz) / image_width, (region_y + region_sz) / image_height);
                    ImGui::Image(timeline->mVideoFilterInputTexture, ImVec2(region_sz * texture_zoom, region_sz * texture_zoom), uv0, uv1, tint_col, border_col);
                    ImGui::EndTooltip();
                }
            }
            if (timeline->mVideoFilterOutputTexture)
            {
                float image_width = ImGui::ImGetTextureWidth(timeline->mVideoFilterOutputTexture);
                float image_height = ImGui::ImGetTextureHeight(timeline->mVideoFilterOutputTexture);
                float region_x = pos_x - region_sz * 0.5f;
                float region_y = pos_y - region_sz * 0.5f;
                if (region_x < 0.0f) { region_x = 0.0f; }
                else if (region_x > image_width - region_sz) { region_x = image_width - region_sz; }
                if (region_y < 0.0f) { region_y = 0.0f; }
                else if (region_y > image_height - region_sz) { region_y = image_height - region_sz; }
                ImGui::SetNextWindowBgAlpha(1.0);
                if (ImGui::BeginTooltip())
                {
                    ImGui::SameLine();
                    ImVec2 uv0 = ImVec2((region_x) / image_width, (region_y) / image_height);
                    ImVec2 uv1 = ImVec2((region_x + region_sz) / image_width, (region_y + region_sz) / image_height);
                    ImGui::Image(timeline->mVideoFilterOutputTexture, ImVec2(region_sz * texture_zoom, region_sz * texture_zoom), uv0, uv1, tint_col, border_col);
                    ImGui::EndTooltip();
                }
            }
        }
    }

    // Show monitors
    std::vector<int> org_disabled_monitor = {MonitorIndexVideoFiltered};
    MonitorButton("video_filter_org_monitor_select", ImVec2(PanelBarPos.x + 20, PanelBarPos.y + 8), MonitorIndexVideoFilterOrg, org_disabled_monitor, false, true);
    std::vector<int> filter_disabled_monitor = {MonitorIndexVideoFilterOrg};
    MonitorButton("video_filter_monitor_select", ImVec2(PanelBarPos.x + PanelBarSize.x - 80, PanelBarPos.y + 8), MonitorIndexVideoFiltered, filter_disabled_monitor, false, true);

    ImGui::PopStyleColor(3);

    ImGui::SetCursorScreenPos(window_pos + ImVec2(20, 10));
    if (attribute)
        ImGui::TextComplex("Video Attribute", 2.0f, ImVec4(0.8, 0.8, 0.8, 0.2),
                        0.1f, ImVec4(0.8, 0.8, 0.8, 0.3),
                        ImVec2(2, 2), ImVec4(0.0, 0.0, 0.0, 0.5));
    else
        ImGui::TextComplex("Video Filter", 2.0f, ImVec4(0.8, 0.8, 0.8, 0.2),
                        0.1f, ImVec4(0.8, 0.8, 0.8, 0.3),
                        ImVec2(2, 2), ImVec4(0.0, 0.0, 0.0, 0.5));
}
/****************************************************************************************
 * 
 * Video Editor windows
 *
 ***************************************************************************************/
static void ShowVideoAttributeWindow(ImDrawList *draw_list)
{
    /*
    ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
    ┃                                 ┃                                  ┃
    ┃                                 ┃                                  ┃
    ┃       preview before            ┃          preview after           ┃ 
    ┃                                 ┃                                  ┃
    ┃                                 ┃                                  ┃ 
    ┃                                 ┃                                  ┃ 
    ┃                                 ┃                                  ┃ 
    ┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
    ┃                          |<  <  []  >  >|                          ┃
    ┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━┫
    ┃             timeline                       ┃                       ┃
    ┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫    attribute edit     ┃
    ┃              curves                        ┃                       ┃
    ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━┛
    */
    ImGuiIO &io = ImGui::GetIO();
    ImVec2 window_pos = ImGui::GetCursorScreenPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    draw_list->AddRectFilled(window_pos, window_pos + window_size, COL_DEEP_DARK);
    if (!timeline)
        return;
    
    MediaCore::VideoTransformFilterHolder attribute;
    Clip * editing_clip = timeline->FindEditingClip();
    if (editing_clip && !IS_VIDEO(editing_clip->mType))
    {
        editing_clip = nullptr;
    }
    if (editing_clip)
    {
        if (timeline->mVidFilterClip)
        {
            timeline->mVidFilterClip->UpdateClipRange(editing_clip);
        }
        else
        {
            timeline->mVidFilterClip = new EditingVideoClip((VideoClip*)editing_clip);
        }
        attribute = timeline->mVidFilterClip->mAttribute;
    }

    float clip_timeline_height = 30 + 50 + 12;

    float clip_keypoint_height = window_size.y / 3 - clip_timeline_height;
    ImVec2 video_preview_pos = window_pos;
    float video_preview_height = window_size.y - clip_timeline_height - clip_keypoint_height;
    float clip_setting_width = 400;
    float clip_setting_height = window_size.y - video_preview_height;
    ImVec2 clip_setting_pos = video_preview_pos + ImVec2(window_size.x - clip_setting_width, video_preview_height);
    ImVec2 clip_setting_size(clip_setting_width, clip_setting_height);
    float video_preview_width = window_size.x;
    if (window_size.x / video_preview_height > 3.f)
    {
        video_preview_width = window_size.x - clip_setting_width;
        clip_setting_height = window_size.y;
        clip_setting_pos = video_preview_pos + ImVec2(video_preview_width, 0);
        clip_setting_size = ImVec2(clip_setting_width, clip_setting_height);
    }
    ImVec2 video_preview_size(video_preview_width, video_preview_height);
    ImVec2 clip_timeline_pos = video_preview_pos + ImVec2(0, video_preview_height);
    ImVec2 clip_timeline_size(window_size.x - clip_setting_width, clip_timeline_height);
    ImVec2 clip_keypoint_pos = clip_timeline_pos + ImVec2(0, clip_timeline_height);
    ImVec2 clip_keypoint_size(window_size.x - clip_setting_width, clip_keypoint_height);

    ImGuiWindowFlags child_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings;
    ImGuiWindowFlags setting_child_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
    // draw video attribute preview
    ImGui::SetCursorScreenPos(video_preview_pos);
    if (ImGui::BeginChild("##video_attribute_preview", video_preview_size, false, child_flags))
    {
        ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
        ImVec2 sub_window_size = ImGui::GetWindowSize();
        draw_list->AddRectFilled(sub_window_pos, sub_window_pos + sub_window_size, COL_DEEP_DARK);
        if (editing_clip) ShowVideoFilterPreviewWindow(draw_list, editing_clip->mStart, editing_clip->mEnd, true);
        else ShowVideoFilterPreviewWindow(draw_list, timeline->GetStart(), timeline->GetEnd(), true);
    }
    ImGui::EndChild();

    // draw clip timeline
    ImGui::SetCursorScreenPos(clip_timeline_pos);
    if (ImGui::BeginChild("##video_attribute_timeline", clip_timeline_size + ImVec2(0, clip_keypoint_height), false, child_flags))
    {
        ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
        ImVec2 sub_window_size = ImGui::GetWindowSize();
        //draw_list->AddRectFilled(sub_window_pos, sub_window_pos + sub_window_size, COL_DARK_TWO);
        mouse_hold |= DrawClipTimeLine(timeline, timeline->mVidFilterClip, timeline->currentTime, 30, 50, clip_keypoint_height, attribute ? attribute->GetKeyPoint() : nullptr);
    }
    ImGui::EndChild();

    // draw clip attribute setting
    ImGui::SetCursorScreenPos(clip_setting_pos);
    if (ImGui::BeginChild("##video_attribute_setting", clip_setting_size, false, setting_child_flags))
    {
        char ** curve_type_list = nullptr;
        auto curve_type_count = ImGui::ImCurveEdit::GetCurveTypeName(curve_type_list);
        // Add Curve
        auto addCurve = [&](std::string name, float _min, float _max, float _default)
        {
            if (attribute)
            {
                auto attribute_keypoint = attribute->GetKeyPoint();
                auto found = attribute_keypoint ? attribute_keypoint->GetCurveIndex(name) : -1;
                if (found == -1)
                {
                    ImU32 color; ImGui::RandomColor(color, 1.f);
                    auto curve_index = attribute_keypoint->AddCurve(name, ImGui::ImCurveEdit::Smooth, color, true, _min, _max, _default);
                    attribute_keypoint->AddPoint(curve_index, ImVec2(0, _min), ImGui::ImCurveEdit::Smooth);
                    attribute_keypoint->AddPoint(curve_index, ImVec2(editing_clip->mEnd - editing_clip->mStart, _max), ImGui::ImCurveEdit::Smooth);
                    attribute_keypoint->SetCurvePointDefault(curve_index, 0);
                    attribute_keypoint->SetCurvePointDefault(curve_index, 1);
                }
            }
        };
        // Editor Curve
        auto EditCurve = [&](std::string name) 
        {
            if (attribute)
            {
                auto attribute_keypoint = attribute->GetKeyPoint();
                int index = attribute_keypoint ? attribute_keypoint->GetCurveIndex(name) : -1;
                if (index != -1)
                {
                    ImGui::Separator();
                    bool break_loop = false;
                    ImGui::PushID(ImGui::GetID(name.c_str()));
                    auto pCount = attribute_keypoint->GetCurvePointCount(index);
                    std::string lable_id = std::string(ICON_CURVE) + " " + name + " (" + std::to_string(pCount) + " keys)" + "##video_attribute_curve";
                    if (ImGui::TreeNodeEx(lable_id.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
                        float value = attribute_keypoint->GetValue(index, timeline->currentTime);
                        ImGui::BracketSquare(true); ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0, 1.0, 0.0, 1.0)); ImGui::Text("%.2f", value); ImGui::PopStyleColor();
                        
                        ImGui::PushItemWidth(60);
                        float curve_min = attribute_keypoint->GetCurveMin(index);
                        float curve_max = attribute_keypoint->GetCurveMax(index);
                        ImGui::BeginDisabled(true);
                        ImGui::DragFloat("##curve_video_filter_min", &curve_min, 0.1f, -FLT_MAX, curve_max, "%.1f"); ImGui::ShowTooltipOnHover("Min");
                        ImGui::SameLine(0, 8);
                        ImGui::DragFloat("##curve_video_filter_max", &curve_max, 0.1f, curve_min, FLT_MAX, "%.1f"); ImGui::ShowTooltipOnHover("Max");
                        ImGui::SameLine(0, 8);
                        ImGui::EndDisabled();
                        float curve_default = attribute_keypoint->GetCurveDefault(index);
                        if (ImGui::DragFloat("##curve_video_attribute_default", &curve_default, 0.1f, curve_min, curve_max, "%.1f"))
                        {
                            attribute_keypoint->SetCurveDefault(index, curve_default);
                            timeline->UpdatePreview();
                        } ImGui::ShowTooltipOnHover("Default");
                        ImGui::PopItemWidth();

                        ImGui::SameLine(0, 8);
                        ImGui::SetWindowFontScale(0.75);
                        auto curve_color = ImGui::ColorConvertU32ToFloat4(attribute_keypoint->GetCurveColor(index));
                        if (ImGui::ColorEdit4("##curve_video_attribute_color", (float*)&curve_color, ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar))
                        {
                            attribute_keypoint->SetCurveColor(index, ImGui::ColorConvertFloat4ToU32(curve_color));
                        } ImGui::ShowTooltipOnHover("Curve Color");
                        ImGui::SetWindowFontScale(1.0);
                        ImGui::SameLine(0, 4);
                        bool is_visiable = attribute_keypoint->IsVisible(index);
                        if (ImGui::Button(is_visiable ? ICON_WATCH : ICON_UNWATCH "##curve_video_attribute_visiable"))
                        {
                            is_visiable = !is_visiable;
                            attribute_keypoint->SetCurveVisible(index, is_visiable);
                        } ImGui::ShowTooltipOnHover(is_visiable ? "Hide" : "Show");
                        ImGui::SameLine(0, 4);
                        if (ImGui::Button(ICON_DELETE "##curve_video_attribute_delete"))
                        {
                            attribute_keypoint->DeleteCurve(index);
                            timeline->UpdatePreview();
                            break_loop = true;
                        } ImGui::ShowTooltipOnHover("Delete");
                        ImGui::SameLine(0, 4);
                        if (ImGui::Button(ICON_RETURN_DEFAULT "##curve_video_attribute_reset"))
                        {
                            for (int p = 0; p < pCount; p++)
                            {
                                attribute_keypoint->SetCurvePointDefault(index, p);
                            }
                            timeline->UpdatePreview();
                        } ImGui::ShowTooltipOnHover("Reset");

                        if (!break_loop)
                        {
                            // list points
                            for (int p = 0; p < pCount; p++)
                            {
                                bool is_disabled = false;
                                ImGui::PushID(p);
                                ImGui::PushItemWidth(96);
                                auto point = attribute_keypoint->GetPoint(index, p);
                                ImGui::Diamond(false);
                                if (p == 0 || p == pCount - 1)
                                    is_disabled = true;
                                ImGui::BeginDisabled(is_disabled);
                                if (ImGui::DragTimeMS("##curve_video_attribute_point_x", &point.point.x, attribute_keypoint->GetMax().x / 1000.f, attribute_keypoint->GetMin().x, attribute_keypoint->GetMax().x, 2))
                                {
                                    attribute_keypoint->EditPoint(index, p, point.point, point.type);
                                    timeline->UpdatePreview();
                                }
                                ImGui::EndDisabled();
                                ImGui::SameLine();
                                auto speed = fabs(attribute_keypoint->GetCurveMax(index) - attribute_keypoint->GetCurveMin(index)) / 500;
                                if (ImGui::DragFloat("##curve_video_attribute_point_y", &point.point.y, speed, attribute_keypoint->GetCurveMin(index), attribute_keypoint->GetCurveMax(index), "%.2f"))
                                {
                                    attribute_keypoint->EditPoint(index, p, point.point, point.type);
                                    timeline->UpdatePreview();
                                }
                                ImGui::SameLine();
                                if (ImGui::Combo("##curve_video_attribute_type", (int*)&point.type, curve_type_list, curve_type_count))
                                {
                                    attribute_keypoint->EditPoint(index, p, point.point, point.type);
                                    timeline->UpdatePreview();
                                }
                                ImGui::PopItemWidth();
                                ImGui::PopID();
                            }
                        }

                        ImGui::PopStyleColor();
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                    ImGui::Separator();
                }
            }
        };
        ImVec2 sub_window_pos = ImGui::GetWindowPos(); // we need draw background with scroll view
        ImVec2 sub_window_size = ImGui::GetWindowSize();
        draw_list->AddRectFilled(sub_window_pos, sub_window_pos + sub_window_size, COL_BLACK_DARK);
        if (timeline->mVidFilterClip && attribute)
        {
            ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(255,255,255,255));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5, 0.5, 0.5, 0.5));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0, 1.0, 1.0, 1.0));
            ImGui::PushItemWidth(240);
            auto attribute_keypoint = attribute->GetKeyPoint();
            // Attribute Crop setting
            if (ImGui::TreeNodeEx("Crop Setting##video_attribute", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::ImCurveEdit::keys margin_key; margin_key.m_id = timeline->mVidFilterClip->mID;
                // Crop Margin Left
                int curve_margin_l_index = attribute_keypoint ? attribute_keypoint->GetCurveIndex("CropMarginL") : -1;
                bool has_curve_margin_l = attribute_keypoint ? curve_margin_l_index != -1 : false;
                float margin_l = has_curve_margin_l ? attribute_keypoint->GetValue(curve_margin_l_index, timeline->currentTime) : attribute->GetCropMarginLScale();
                ImGui::BeginDisabled(has_curve_margin_l);
                if (ImGui::SliderFloat("Crop Left", &margin_l, 0.f, 1.f))
                {
                    attribute->SetCropMarginL(margin_l);
                    timeline->UpdatePreview();
                }
                ImGui::SameLine(sub_window_size.x - 66); if (ImGui::Button(ICON_RETURN_DEFAULT "##crop_marhin_l_default")) { attribute->SetCropMarginL(0.f); timeline->UpdatePreview(); }
                ImGui::EndDisabled();
                if (ImGui::ImCurveCheckEditKey("##add_curve_margin_l##video_attribute", &margin_key, has_curve_margin_l, "margin_l##video_attribute", 0.f, 1.f, 0.f, 360))
                {
                    if (has_curve_margin_l) addCurve("CropMarginL", margin_key.m_min, margin_key.m_max, margin_key.m_default);
                    else if (attribute_keypoint) attribute_keypoint->DeleteCurve("CropMarginL");
                    timeline->UpdatePreview();
                }
                if (has_curve_margin_l) EditCurve("CropMarginL");

                // Crop Margin Top
                int curve_margin_t_index = attribute_keypoint ? attribute_keypoint->GetCurveIndex("CropMarginT") : -1;
                bool has_curve_margin_t = attribute_keypoint ? curve_margin_t_index != -1 : false;
                float margin_t = has_curve_margin_t ? attribute_keypoint->GetValue(curve_margin_t_index, timeline->currentTime) : attribute->GetCropMarginTScale();
                ImGui::BeginDisabled(has_curve_margin_t);
                if (ImGui::SliderFloat("Crop Top", &margin_t, 0.f, 1.f))
                {
                    attribute->SetCropMarginT(margin_t);
                    timeline->UpdatePreview();
                }
                ImGui::SameLine(sub_window_size.x - 66); if (ImGui::Button(ICON_RETURN_DEFAULT "##crop_marhin_t_default")) { attribute->SetCropMarginT(0.f); timeline->UpdatePreview(); }
                ImGui::EndDisabled();
                if (ImGui::ImCurveCheckEditKey("##add_curve_margin_t##video_attribute", &margin_key, has_curve_margin_t, "margin_t##video_attribute", 0.f, 1.f, 0.f, 360))
                {
                    if (has_curve_margin_t) addCurve("CropMarginT", margin_key.m_min, margin_key.m_max, margin_key.m_default);
                    else if (attribute_keypoint) attribute_keypoint->DeleteCurve("CropMarginT");
                    timeline->UpdatePreview();
                }
                if (has_curve_margin_t) EditCurve("CropMarginT");

                // Crop Margin Right
                int curve_margin_r_index = attribute_keypoint ? attribute_keypoint->GetCurveIndex("CropMarginR") : -1;
                bool has_curve_margin_r = attribute_keypoint ? curve_margin_r_index != -1 : false;
                float margin_r = has_curve_margin_r ? attribute_keypoint->GetValue(curve_margin_r_index, timeline->currentTime) : attribute->GetCropMarginRScale();
                ImGui::BeginDisabled(has_curve_margin_r);
                if (ImGui::SliderFloat("Crop Right", &margin_r, 0.f, 1.f))
                {
                    attribute->SetCropMarginR(margin_r);
                    timeline->UpdatePreview();
                }
                ImGui::SameLine(sub_window_size.x - 66); if (ImGui::Button(ICON_RETURN_DEFAULT "##crop_marhin_r_default")) { attribute->SetCropMarginR(0.f); timeline->UpdatePreview(); }
                ImGui::EndDisabled();
                if (ImGui::ImCurveCheckEditKey("##add_curve_margin_r##video_attribute", &margin_key, has_curve_margin_r, "margin_r##video_attribute", 0.f, 1.f, 0.f, 360))
                {
                    if (has_curve_margin_r) addCurve("CropMarginR", margin_key.m_min, margin_key.m_max, margin_key.m_default);
                    else if (attribute_keypoint) attribute_keypoint->DeleteCurve("CropMarginR");
                    timeline->UpdatePreview();
                }
                if (has_curve_margin_r) EditCurve("CropMarginR");

                // Crop Margin Bottom
                int curve_margin_b_index = attribute_keypoint ? attribute_keypoint->GetCurveIndex("CropMarginB") : -1;
                bool has_curve_margin_b = attribute_keypoint ? curve_margin_b_index != -1 : false;
                float margin_b = has_curve_margin_b ? attribute_keypoint->GetValue(curve_margin_b_index, timeline->currentTime) : attribute->GetCropMarginBScale();
                ImGui::BeginDisabled(has_curve_margin_b);
                if (ImGui::SliderFloat("Crop Bottom", &margin_b, 0.f, 1.f))
                {
                    attribute->SetCropMarginB(margin_b);
                    timeline->UpdatePreview();
                }
                ImGui::SameLine(sub_window_size.x - 66); if (ImGui::Button(ICON_RETURN_DEFAULT "##crop_marhin_b_default")) { attribute->SetCropMarginB(0.f); timeline->UpdatePreview(); }
                ImGui::EndDisabled();
                if (ImGui::ImCurveCheckEditKey("##add_curve_margin_b##video_attribute", &margin_key, has_curve_margin_b, "margin_b##video_attribute", 0.f, 1.f, 0.f, 360))
                {
                    if (has_curve_margin_b) addCurve("CropMarginB", margin_key.m_min, margin_key.m_max, margin_key.m_default);
                    else if (attribute_keypoint) attribute_keypoint->DeleteCurve("CropMarginB");
                    timeline->UpdatePreview();
                }
                if (has_curve_margin_b) EditCurve("CropMarginB");

                ImGui::TreePop();
            }
            // Attribute Position setting
            if (ImGui::TreeNodeEx("Position Setting##video_attribute", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::ImCurveEdit::keys margin_key; margin_key.m_id = timeline->mVidFilterClip->mID;
                // Position offset H
                int curve_position_h_index = attribute_keypoint ? attribute_keypoint->GetCurveIndex("PositionOffsetH") : -1;
                bool has_curve_position_h = attribute_keypoint ? curve_position_h_index != -1 : false;
                float position_h = has_curve_position_h ? attribute_keypoint->GetValue(curve_position_h_index, timeline->currentTime) : attribute->GetPositionOffsetHScale();
                ImGui::BeginDisabled(has_curve_position_h);
                if (ImGui::SliderFloat("Position H", &position_h, -1.f, 1.f))
                {
                    attribute->SetPositionOffsetH(position_h);
                    timeline->UpdatePreview();
                }
                ImGui::SameLine(sub_window_size.x - 66); if (ImGui::Button(ICON_RETURN_DEFAULT "##position_h_default")) { attribute->SetPositionOffsetH(0.f); timeline->UpdatePreview(); }
                ImGui::EndDisabled();
                if (ImGui::ImCurveCheckEditKey("##add_curve_position_h##video_attribute", &margin_key, has_curve_position_h, "position_h##video_attribute", -1.f, 1.f, 0.f, 360))
                {
                    if (has_curve_position_h) addCurve("PositionOffsetH", margin_key.m_min, margin_key.m_max, margin_key.m_default);
                    else if (attribute_keypoint) attribute_keypoint->DeleteCurve("PositionOffsetH");
                    timeline->UpdatePreview();
                }
                if (has_curve_position_h) EditCurve("PositionOffsetH");

                // Position offset V
                int curve_position_v_index = attribute_keypoint ? attribute_keypoint->GetCurveIndex("PositionOffsetV") : -1;
                bool has_curve_position_v = attribute_keypoint ? curve_position_v_index != -1 : false;
                float position_v = has_curve_position_v ? attribute_keypoint->GetValue(curve_position_v_index, timeline->currentTime) : attribute->GetPositionOffsetVScale();
                ImGui::BeginDisabled(has_curve_position_v);
                if (ImGui::SliderFloat("Position V", &position_v, -1.f, 1.f))
                {
                    attribute->SetPositionOffsetV(position_v);
                    timeline->UpdatePreview();
                }
                ImGui::SameLine(sub_window_size.x - 66); if (ImGui::Button(ICON_RETURN_DEFAULT "##position_v_default")) { attribute->SetPositionOffsetV(0.f); timeline->UpdatePreview(); }
                ImGui::EndDisabled();
                if (ImGui::ImCurveCheckEditKey("##add_curve_position_v##video_attribute", &margin_key, has_curve_position_v, "position_v##video_attribute", -1.f, 1.f, 0.f, 360))
                {
                    if (has_curve_position_v) addCurve("PositionOffsetV", margin_key.m_min, margin_key.m_max, margin_key.m_default);
                    else if (attribute_keypoint) attribute_keypoint->DeleteCurve("PositionOffsetV");
                    timeline->UpdatePreview();
                }
                if (has_curve_position_v) EditCurve("PositionOffsetV");

                ImGui::TreePop();
            }
            // Attribute Scale setting
            if (ImGui::TreeNodeEx("Scale Setting##video_attribute", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::ImCurveEdit::keys margin_key; margin_key.m_id = timeline->mVidFilterClip->mID;
                // ScaleType as scale method
                MediaCore::ScaleType scale_type = attribute->GetScaleType();
                ImGui::PushItemWidth(100);
                if (ImGui::Combo("Scale Type##curve_video_attribute_scale_type", (int*)&scale_type, VideoAttributeScaleType, IM_ARRAYSIZE(VideoAttributeScaleType)))
                {
                    attribute->SetScaleType(scale_type);
                    timeline->UpdatePreview();
                }
                ImGui::SameLine();
                bool keep_aspect_ratio = editing_clip ? ((VideoClip*)editing_clip)->mKeepAspectRatio : false;
                if (ImGui::Checkbox("Keep Ratio", &keep_aspect_ratio))
                {
                    ((VideoClip*)editing_clip)->mKeepAspectRatio = keep_aspect_ratio;
                    if (keep_aspect_ratio)
                    {
                        if (attribute_keypoint) attribute_keypoint->DeleteCurve("ScaleH");
                        if (attribute_keypoint) attribute_keypoint->DeleteCurve("ScaleV");
                    }
                    else
                    {
                        if (attribute_keypoint) attribute_keypoint->DeleteCurve("Scale");
                    }
                }
                ImGui::PopItemWidth();

                if (keep_aspect_ratio)
                {
                    int curve_scale_index = attribute_keypoint ? attribute_keypoint->GetCurveIndex("Scale") : -1;
                    bool has_curve_scale = attribute_keypoint ? curve_scale_index != -1 : false;
                    float scale = has_curve_scale ? attribute_keypoint->GetValue(curve_scale_index, timeline->currentTime) : (attribute->GetScaleH() + attribute->GetScaleV()) / 2;
                    ImGui::BeginDisabled(has_curve_scale);
                    if (ImGui::SliderFloat("Scale", &scale, 0, 8.f, "%.1f"))
                    {
                        attribute->SetScaleH(scale);
                        attribute->SetScaleV(scale);
                        timeline->UpdatePreview();
                    }
                    ImGui::SameLine(sub_window_size.x - 66); if (ImGui::Button(ICON_RETURN_DEFAULT "##scale_default")) { attribute->SetScaleH(1.0); attribute->SetScaleV(1.0); timeline->UpdatePreview(); }
                    ImGui::EndDisabled();
                    if (ImGui::ImCurveCheckEditKey("##add_curve_scale##video_attribute", &margin_key, has_curve_scale, "scale##video_attribute", 0, 8.f, 1.f, 360))
                    {
                        if (has_curve_scale) addCurve("Scale", margin_key.m_min, margin_key.m_max, margin_key.m_default);
                        else if (attribute_keypoint) attribute_keypoint->DeleteCurve("Scale");
                        timeline->UpdatePreview();
                    }
                    if (has_curve_scale) EditCurve("Scale");
                }
                else
                {
                    // Scale H
                    int curve_scale_h_index = attribute_keypoint ? attribute_keypoint->GetCurveIndex("ScaleH") : -1;
                    bool has_curve_scale_h = attribute_keypoint ? curve_scale_h_index != -1 : false;
                    float scale_h = has_curve_scale_h ? attribute_keypoint->GetValue(curve_scale_h_index, timeline->currentTime) : attribute->GetScaleH();
                    ImGui::BeginDisabled(has_curve_scale_h);
                    if (ImGui::SliderFloat("Scale H", &scale_h, 0, 8.f, "%.1f"))
                    {
                        attribute->SetScaleH(scale_h);
                        timeline->UpdatePreview();
                    }
                    ImGui::SameLine(sub_window_size.x - 66); if (ImGui::Button(ICON_RETURN_DEFAULT "##scale_h_default")) { attribute->SetScaleH(1.0); timeline->UpdatePreview(); }
                    ImGui::EndDisabled();
                    if (ImGui::ImCurveCheckEditKey("##add_curve_scale_h##video_attribute", &margin_key, has_curve_scale_h, "scale_h##video_attribute", 0, 8.f, 1.f, 360))
                    {
                        if (has_curve_scale_h) addCurve("ScaleH", margin_key.m_min, margin_key.m_max, margin_key.m_default);
                        else if (attribute_keypoint) attribute_keypoint->DeleteCurve("ScaleH");
                        timeline->UpdatePreview();
                    }
                    if (has_curve_scale_h) EditCurve("ScaleH");

                    // Scale V
                    int curve_scale_v_index = attribute_keypoint ? attribute_keypoint->GetCurveIndex("ScaleV") : -1;
                    bool has_curve_scale_v = attribute_keypoint ? curve_scale_v_index != -1 : false;
                    float scale_v = has_curve_scale_v ? attribute_keypoint->GetValue(curve_scale_v_index, timeline->currentTime) : attribute->GetScaleV();
                    ImGui::BeginDisabled(has_curve_scale_v);
                    if (ImGui::SliderFloat("Scale V", &scale_v, 0, 8.f, "%.1f"))
                    {
                        attribute->SetScaleV(scale_v);
                        timeline->UpdatePreview();
                    }
                    ImGui::SameLine(sub_window_size.x - 66); if (ImGui::Button(ICON_RETURN_DEFAULT "##scale_v_default")) { attribute->SetScaleV(1.0); timeline->UpdatePreview(); }
                    ImGui::EndDisabled();
                    if (ImGui::ImCurveCheckEditKey("##add_curve_scale_v##video_attribute", &margin_key, has_curve_scale_v, "scale_v##video_attribute", 0, 8.f, 1.f, 360))
                    {
                        if (has_curve_scale_v) addCurve("ScaleV", margin_key.m_min, margin_key.m_max, margin_key.m_default);
                        else if (attribute_keypoint) attribute_keypoint->DeleteCurve("ScaleV");
                        timeline->UpdatePreview();
                    }
                    if (has_curve_scale_v) EditCurve("ScaleV");
                }

                ImGui::TreePop();
            }
            // Attribute Angle setting
            if (ImGui::TreeNodeEx("Angle Setting##video_attribute", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::ImCurveEdit::keys margin_key; margin_key.m_id = timeline->mVidFilterClip->mID;

                // Rotate angle
                int curve_angle_index = attribute_keypoint ? attribute_keypoint->GetCurveIndex("RotateAngle") : -1;
                bool has_curve_angle = attribute_keypoint ? curve_angle_index != -1 : false;
                float angle = has_curve_angle ? attribute_keypoint->GetValue(curve_angle_index, timeline->currentTime) : attribute->GetRotationAngle();
                ImGui::BeginDisabled(has_curve_angle);
                if (ImGui::SliderFloat("Rotate Angle", &angle, -360.f, 360.f, "%.0f"))
                {
                    attribute->SetRotationAngle(angle);
                    timeline->UpdatePreview();
                }
                ImGui::SameLine(sub_window_size.x - 66); if (ImGui::Button(ICON_RETURN_DEFAULT "##angle_default")) { attribute->SetRotationAngle(0.0); timeline->UpdatePreview(); }
                ImGui::EndDisabled();
                if (ImGui::ImCurveCheckEditKey("##add_curve_angle##video_attribute", &margin_key, has_curve_angle, "angle##video_attribute", -360.f, 360.f, 0.f, 360))
                {
                    if (has_curve_angle) addCurve("RotateAngle", margin_key.m_min, margin_key.m_max, margin_key.m_default);
                    else if (attribute_keypoint) attribute_keypoint->DeleteCurve("RotateAngle");
                    timeline->UpdatePreview();
                }
                if (has_curve_angle) EditCurve("RotateAngle");

                ImGui::TreePop();
            }
            ImGui::PopItemWidth();
            ImGui::PopStyleColor(4);
        }
    }
    ImGui::EndChild();
}

/****************************************************************************************
 * 
 * Video Filter window
 *
 ***************************************************************************************/
static void ShowVideoFilterBluePrintWindow(ImDrawList *draw_list, Clip * clip)
{
    if (timeline && timeline->mVidFilterClip && timeline->mVidFilterClip->mFilter && timeline->mVidFilterClip->mFilter->mBp)
    {
        if (clip && !timeline->mVidFilterClip->mFilter->mBp->m_Document->m_Blueprint.IsOpened())
        {
            auto track = timeline->FindTrackByClipID(clip->mID);
            if (track)
                track->SelectEditingClip(clip, true);
            timeline->mVidFilterClip->mFilter->mBp->View_ZoomToContent();
        }
        ImVec2 window_pos = ImGui::GetCursorScreenPos();
        ImVec2 window_size = ImGui::GetWindowSize();
        ImGui::SetCursorScreenPos(window_pos + ImVec2(3, 3));
        ImGui::InvisibleButton("video_editor_blueprint_back_view", window_size - ImVec2(6, 6));
        if (ImGui::BeginDragDropTarget() && timeline->mVidFilterClip->mFilter->mBp->Blueprint_IsValid())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Filter_drag_drop_Video"))
            {
                const BluePrint::Node * node = (const BluePrint::Node *)payload->Data;
                if (node)
                {
                    timeline->mVidFilterClip->mFilter->mBp->Edit_Insert(node->GetTypeID());
                }
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::SetCursorScreenPos(window_pos + ImVec2(1, 1));
        if (ImGui::BeginChild("##video_editor_blueprint", window_size - ImVec2(2, 2), false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings))
        {
            timeline->mVidFilterClip->mFilter->mBp->Frame(true, true, clip != nullptr, BluePrint::BluePrintFlag::BluePrintFlag_Filter);
        }
        ImGui::EndChild();
    }
}

static void ShowVideoFilterWindow(ImDrawList *draw_list)
{
    /*
    ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
    ┃                                 ┃                                  ┃
    ┃                                 ┃                                  ┃
    ┃       preview before            ┃          preview after           ┃ 
    ┃                                 ┃                                  ┃
    ┃                                 ┃                                  ┃ 
    ┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫
    ┃                          |<  <  []  >  >|                          ┃
    ┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━┫
    ┃          blueprint                         ┃                       ┃ 
    ┃                                            ┃                       ┃ 
    ┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫    filter edit        ┃ 
    ┃             timeline                       ┃                       ┃
    ┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫                       ┃
    ┃              curves                        ┃                       ┃
    ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━┛
    */

    ImVec2 window_pos = ImGui::GetCursorScreenPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    draw_list->AddRectFilled(window_pos, window_pos + window_size, COL_DEEP_DARK);
    if (!timeline)
        return;
    
    BluePrintVideoFilter * filter = nullptr;
    BluePrint::BluePrintUI* blueprint = nullptr;

    Clip * editing_clip = timeline->FindEditingClip();
    if (editing_clip && !IS_VIDEO(editing_clip->mType))
    {
        editing_clip = nullptr;
    }

    if (editing_clip)
    {
        if (timeline->mVidFilterClip)
        {
            timeline->mVidFilterClip->UpdateClipRange(editing_clip);
        }
        else
        {
            timeline->mVidFilterClip = new EditingVideoClip((VideoClip*)editing_clip);
        }
        filter = timeline->mVidFilterClip->mFilter;
        blueprint = filter ? filter->mBp : nullptr;
    }
    
    float clip_timeline_height = 30 + 50 + 12;
    float clip_keypoint_height = g_media_editor_settings.VideoFilterCurveExpanded ? 80 : 0;
    ImVec2 video_preview_pos = window_pos;
    float video_preview_height = (window_size.y - clip_timeline_height - clip_keypoint_height) * 2 / 3;
    float video_bluepoint_height = (window_size.y - clip_timeline_height - clip_keypoint_height) - video_preview_height;
    float clip_setting_width = 400;
    float clip_setting_height = window_size.y - video_preview_height;
    ImVec2 clip_setting_pos = video_preview_pos + ImVec2(window_size.x - clip_setting_width, video_preview_height);
    ImVec2 clip_setting_size(clip_setting_width, clip_setting_height);
    float video_preview_width = window_size.x;
    if (window_size.x / video_preview_height > 4.f)
    {
        video_preview_width = window_size.x - clip_setting_width;
        clip_setting_height = window_size.y;
        clip_setting_pos = video_preview_pos + ImVec2(video_preview_width, 0);
        clip_setting_size = ImVec2(clip_setting_width, clip_setting_height);
    }
    
    ImVec2 video_preview_size(video_preview_width, video_preview_height);
    ImVec2 video_bluepoint_pos = video_preview_pos + ImVec2(0, video_preview_height);
    ImVec2 video_bluepoint_size(window_size.x - clip_setting_width, video_bluepoint_height);
    ImVec2 clip_timeline_pos = video_bluepoint_pos + ImVec2(0, video_bluepoint_height);
    ImVec2 clip_timeline_size(window_size.x - clip_setting_width, clip_timeline_height);
    ImVec2 clip_keypoint_pos = g_media_editor_settings.VideoFilterCurveExpanded ? clip_timeline_pos + ImVec2(0, clip_timeline_height) : clip_timeline_pos + ImVec2(0, clip_timeline_height - 16);
    ImVec2 clip_keypoint_size(window_size.x - clip_setting_width, clip_keypoint_height);

    ImGuiWindowFlags child_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings;
    ImGuiWindowFlags setting_child_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
    
    // draw video filter preview
    ImGui::SetCursorScreenPos(video_preview_pos);
    if (ImGui::BeginChild("##video_filter_preview", video_preview_size, false, child_flags))
    {
        ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
        ImVec2 sub_window_size = ImGui::GetWindowSize();
        draw_list->AddRectFilled(sub_window_pos, sub_window_pos + sub_window_size, COL_DEEP_DARK);
        if (editing_clip) ShowVideoFilterPreviewWindow(draw_list, editing_clip->mStart, editing_clip->mEnd);
        else ShowVideoFilterPreviewWindow(draw_list, timeline->GetStart(), timeline->GetEnd());
    }
    ImGui::EndChild();

    // draw filter blueprint
    ImGui::SetCursorScreenPos(video_bluepoint_pos);
    if (ImGui::BeginChild("##video_filter_blueprint", video_bluepoint_size, false, child_flags))
    {
        ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
        ImVec2 sub_window_size = ImGui::GetWindowSize();
        draw_list->AddRectFilled(sub_window_pos, sub_window_pos + sub_window_size, COL_DARK_ONE);
        ShowVideoFilterBluePrintWindow(draw_list, editing_clip);
    }
    ImGui::EndChild();

    // draw filter timeline
    ImGui::SetCursorScreenPos(clip_timeline_pos);
    if (ImGui::BeginChild("##video_filter_timeline", clip_timeline_size + ImVec2(0, clip_keypoint_height), false, child_flags))
    {
        ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
        ImVec2 sub_window_size = ImGui::GetWindowSize();
        //draw_list->AddRectFilled(sub_window_pos, sub_window_pos + sub_window_size, COL_DARK_TWO);
        // Draw Clip TimeLine
        mouse_hold |= DrawClipTimeLine(timeline, timeline->mVidFilterClip, timeline->currentTime, 30, 50, clip_keypoint_height, filter ? &filter->mKeyPoints : nullptr);
    }
    ImGui::EndChild();

    // draw keypoint hidden button
    ImVec2 hidden_button_pos = clip_keypoint_pos - ImVec2(16, 0);
    ImRect hidden_button_rect = ImRect(hidden_button_pos, hidden_button_pos + ImVec2(16, 16));
    ImGui::SetWindowFontScale(0.75);
    if (hidden_button_rect.Contains(ImGui::GetMousePos()))
    {
        draw_list->AddRectFilled(hidden_button_rect.Min, hidden_button_rect.Max, IM_COL32(64,64,64,255));
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            g_media_editor_settings.VideoFilterCurveExpanded = !g_media_editor_settings.VideoFilterCurveExpanded;
        }
        if (ImGui::BeginTooltip())
        {
            ImGui::TextUnformatted(g_media_editor_settings.VideoFilterCurveExpanded ? "Hide Curve View" : "Show Curve View");
            ImGui::EndTooltip();
        }
    }
    draw_list->AddText(hidden_button_pos, IM_COL32_WHITE, ICON_FA_BEZIER_CURVE);
    ImGui::SetWindowFontScale(1.0);

    // draw filter setting
    ImGui::SetCursorScreenPos(clip_setting_pos);
    if (ImGui::BeginChild("##video_filter_setting", clip_setting_size, false, setting_child_flags))
    {
        auto addCurve = [&](std::string name, float _min, float _max, float _default)
        {
            if (filter)
            {
                auto found = filter->mKeyPoints.GetCurveIndex(name);
                if (found == -1)
                {
                    ImU32 color; ImGui::RandomColor(color, 1.f);
                    auto curve_index = filter->mKeyPoints.AddCurve(name, ImGui::ImCurveEdit::Smooth, color, true, _min, _max, _default);
                    filter->mKeyPoints.AddPoint(curve_index, ImVec2(/*editing_clip->mStart*/ 0, _min), ImGui::ImCurveEdit::Smooth);
                    filter->mKeyPoints.AddPoint(curve_index, ImVec2(editing_clip->mEnd - editing_clip->mStart, _max), ImGui::ImCurveEdit::Smooth);
                    filter->mKeyPoints.SetCurvePointDefault(curve_index, 0);
                    filter->mKeyPoints.SetCurvePointDefault(curve_index, 1);
                    if (blueprint)
                    {
                        auto entry_node = blueprint->FindEntryPointNode();
                        if (entry_node) entry_node->InsertOutputPin(BluePrint::PinType::Float, name);
                        timeline->UpdatePreview();
                    }
                }
            }
        };
        
        ImVec2 sub_window_pos = ImGui::GetWindowPos(); // we need draw background with scroll view
        ImVec2 sub_window_size = ImGui::GetWindowSize();
        draw_list->AddRectFilled(sub_window_pos, sub_window_pos + sub_window_size, COL_BLACK_DARK);
        if (timeline->mVidFilterClip && filter)
        {
            // Filter curve setting
            if (ImGui::TreeNodeEx("Curve Setting##video_filter", ImGuiTreeNodeFlags_DefaultOpen))
            {
                char ** curve_type_list = nullptr;
                auto curve_type_count = ImGui::ImCurveEdit::GetCurveTypeName(curve_type_list);
                static std::string curve_name = "";
                std::string value = curve_name;
                bool name_input_empty = curve_name.empty();
                if (ImGui::InputTextWithHint("##new_curve_name_video_filter", "Input curve name", (char*)value.data(), value.size() + 1, ImGuiInputTextFlags_CallbackEdit | ImGuiInputTextFlags_CallbackResize, [](ImGuiInputTextCallbackData* data) -> int
                {
                    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
                    {
                        auto& stringValue = *static_cast<string*>(data->UserData);
                        ImVector<char>* my_str = (ImVector<char>*)data->UserData;
                        //IM_ASSERT(stringValue.data() == data->Buf);
                        stringValue.resize(data->BufSize);
                        data->Buf = (char*)stringValue.data();
                    }
                    else if (data->EventFlag == ImGuiInputTextFlags_CallbackEdit)
                    {
                        auto& stringValue = *static_cast<string*>(data->UserData);
                        stringValue = std::string(data->Buf);
                    }
                    return 0;
                }, &value))
                {
                    value.resize(strlen(value.c_str()));
                    curve_name = value;
                    name_input_empty = curve_name.empty();
                }

                ImGui::BeginDisabled(name_input_empty);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
                ImGui::SameLine();
                if (ImGui::Button(ICON_ADD "##insert_curve_video_filter"))
                {
                    addCurve(curve_name, 0.f, 1.f, 0.5);
                }
                ImGui::ShowTooltipOnHover("Add custom curve");
                ImGui::PopStyleVar();
                ImGui::EndDisabled();

                // list curves
                for (int i = 0; i < filter->mKeyPoints.GetCurveCount(); i++)
                {
                    bool break_loop = false;
                    ImGui::PushID(i);
                    auto pCount = filter->mKeyPoints.GetCurvePointCount(i);
                    std::string lable_id = std::string(ICON_CURVE) + " " + filter->mKeyPoints.GetCurveName(i) + " (" + std::to_string(pCount) + " keys)" + "##video_filter_curve";
                    if (ImGui::TreeNodeEx(lable_id.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
                        float value = filter->mKeyPoints.GetValue(i, timeline->currentTime - timeline->mVidFilterClip->mStart);
                        ImGui::BracketSquare(true); ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0, 1.0, 0.0, 1.0)); ImGui::Text("%.2f", value); ImGui::PopStyleColor();
                        ImGui::PushItemWidth(60);
                        float curve_min = filter->mKeyPoints.GetCurveMin(i);
                        float curve_max = filter->mKeyPoints.GetCurveMax(i);
                        if (ImGui::DragFloat("##curve_video_filter_min", &curve_min, 0.1f, -FLT_MAX, curve_max, "%.1f"))
                        {
                            filter->mKeyPoints.SetCurveMin(i, curve_min);
                            timeline->UpdatePreview();
                        } ImGui::ShowTooltipOnHover("Min");
                        ImGui::SameLine(0, 8);
                        if (ImGui::DragFloat("##curve_video_filter_max", &curve_max, 0.1f, curve_min, FLT_MAX, "%.1f"))
                        {
                            filter->mKeyPoints.SetCurveMax(i, curve_max);
                            timeline->UpdatePreview();
                        } ImGui::ShowTooltipOnHover("Max");
                        ImGui::SameLine(0, 8);
                        float curve_default = filter->mKeyPoints.GetCurveDefault(i);
                        if (ImGui::DragFloat("##curve_video_filter_default", &curve_default, 0.1f, curve_min, curve_max, "%.1f"))
                        {
                            filter->mKeyPoints.SetCurveDefault(i, curve_default);
                            timeline->UpdatePreview();
                        } ImGui::ShowTooltipOnHover("Default");
                        ImGui::PopItemWidth();
                        
                        ImGui::SameLine(0, 8);
                        ImGui::SetWindowFontScale(0.75);
                        auto curve_color = ImGui::ColorConvertU32ToFloat4(filter->mKeyPoints.GetCurveColor(i));
                        if (ImGui::ColorEdit4("##curve_video_filter_color", (float*)&curve_color, ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar))
                        {
                            filter->mKeyPoints.SetCurveColor(i, ImGui::ColorConvertFloat4ToU32(curve_color));
                        } ImGui::ShowTooltipOnHover("Curve Color");
                        ImGui::SetWindowFontScale(1.0);
                        ImGui::SameLine(0, 4);
                        bool is_visiable = filter->mKeyPoints.IsVisible(i);
                        if (ImGui::Button(is_visiable ? ICON_WATCH : ICON_UNWATCH "##curve_video_filter_visiable"))
                        {
                            is_visiable = !is_visiable;
                            filter->mKeyPoints.SetCurveVisible(i, is_visiable);
                        } ImGui::ShowTooltipOnHover( is_visiable ? "Hide" : "Show");
                        ImGui::SameLine(0, 4);
                        if (ImGui::Button(ICON_DELETE "##curve_video_filter_delete"))
                        {
                            // delete blueprint entry node pin
                            auto pin_name = filter->mKeyPoints.GetCurveName(i);
                            if (blueprint)
                            {
                                auto entry_node = blueprint->FindEntryPointNode();
                                if (entry_node) entry_node->DeleteOutputPin(pin_name);
                                timeline->UpdatePreview();
                            }
                            filter->mKeyPoints.DeleteCurve(i);
                            break_loop = true;
                        } ImGui::ShowTooltipOnHover("Delete");
                        ImGui::SameLine(0, 4);
                        if (ImGui::Button(ICON_RETURN_DEFAULT "##curve_video_filter_reset"))
                        {
                            for (int p = 0; p < pCount; p++)
                            {
                                filter->mKeyPoints.SetCurvePointDefault(i, p);
                            }
                            timeline->UpdatePreview();
                        } ImGui::ShowTooltipOnHover("Reset");

                        if (!break_loop)
                        {
                            // list points
                            for (int p = 0; p < pCount; p++)
                            {
                                bool is_disabled = false;
                                ImGui::PushID(p);
                                ImGui::PushItemWidth(96);
                                auto point = filter->mKeyPoints.GetPoint(i, p);
                                ImGui::Diamond(false);
                                if (p == 0 || p == pCount - 1)
                                    is_disabled = true;
                                ImGui::BeginDisabled(is_disabled);
                                if (ImGui::DragTimeMS("##curve_video_filter_point_x", &point.point.x, filter->mKeyPoints.GetMax().x / 1000.f, filter->mKeyPoints.GetMin().x, filter->mKeyPoints.GetMax().x, 2))
                                {
                                    filter->mKeyPoints.EditPoint(i, p, point.point, point.type);
                                    timeline->UpdatePreview();
                                }
                                ImGui::EndDisabled();
                                ImGui::SameLine();
                                auto speed = fabs(filter->mKeyPoints.GetCurveMax(i) - filter->mKeyPoints.GetCurveMin(i)) / 500;
                                if (ImGui::DragFloat("##curve_video_filter_point_y", &point.point.y, speed, filter->mKeyPoints.GetCurveMin(i), filter->mKeyPoints.GetCurveMax(i), "%.2f"))
                                {
                                    filter->mKeyPoints.EditPoint(i, p, point.point, point.type);
                                    timeline->UpdatePreview();
                                }
                                ImGui::SameLine();
                                if (ImGui::Combo("##curve_video_filter_type", (int*)&point.type, curve_type_list, curve_type_count))
                                {
                                    filter->mKeyPoints.EditPoint(i, p, point.point, point.type);
                                    timeline->UpdatePreview();
                                }
                                ImGui::PopItemWidth();
                                ImGui::PopID();
                            }
                        }
                        ImGui::PopStyleColor();
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                    if (break_loop) break;
                }

                ImGui::TreePop();
            }
            // Filter Node setting
            if (blueprint && blueprint->Blueprint_IsValid())
            {
                if (ImGui::TreeNodeEx("Node Configure##video_filter", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto nodes = blueprint->m_Document->m_Blueprint.GetNodes();
                    for (auto node : nodes)
                    {
                        auto type = node->GetTypeInfo().m_Type;
                        if (type == BluePrint::NodeType::EntryPoint || type == BluePrint::NodeType::ExitPoint)
                            continue;
                        if (!node->CustomLayout())
                            continue;
                        auto label_name = node->m_Name;
                        std::string lable_id = label_name + "##video_filter_node" + "@" + std::to_string(node->m_ID);
                        node->DrawNodeLogo(ImGui::GetCurrentContext(), ImVec2(28, 28));
                        ImGui::SameLine(40);
                        if (ImGui::TreeNodeEx(lable_id.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            ImGui::ImCurveEdit::keys key;
                            key.m_id = node->m_ID;
                            if (node->DrawCustomLayout(ImGui::GetCurrentContext(), 1.0, ImVec2(0, 0), &key))
                            {
                                node->m_NeedUpdate = true;
                                timeline->UpdatePreview();
                            }
                            if (!key.name.empty())
                            {
                                addCurve(key.name, key.m_min, key.m_max, key.m_default);
                            }
                            ImGui::TreePop();
                        }
                    }
                    ImGui::TreePop();
                }
            }
        }
    }
    ImGui::EndChild();
}

/****************************************************************************************
 * 
 * Video Fusion window
 *
 ***************************************************************************************/
static void ShowVideoFusionBluePrintWindow(ImDrawList *draw_list, Overlap * overlap)
{
    if (timeline && timeline->mVidOverlap && timeline->mVidOverlap->mFusion && timeline->mVidOverlap->mFusion->mBp)
    {
        if (overlap && !timeline->mVidOverlap->mFusion->mBp->m_Document->m_Blueprint.IsOpened())
        {
            auto track = timeline->FindTrackByClipID(overlap->m_Clip.first);
            if (track)
                track->SelectEditingOverlap(overlap);
            timeline->mVidOverlap->mFusion->mBp->View_ZoomToContent();
        }
        ImVec2 window_pos = ImGui::GetCursorScreenPos();
        ImVec2 window_size = ImGui::GetWindowSize();
        ImGui::SetCursorScreenPos(window_pos + ImVec2(3, 3));
        ImGui::InvisibleButton("video_fusion_blueprint_back_view", window_size - ImVec2(6, 6));
        if (ImGui::BeginDragDropTarget() && timeline->mVidOverlap->mFusion->mBp->Blueprint_IsValid())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Fusion_drag_drop_Video"))
            {
                const BluePrint::Node * node = (const BluePrint::Node *)payload->Data;
                if (node)
                {
                    timeline->mVidOverlap->mFusion->mBp->Edit_Insert(node->GetTypeID());
                }
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::SetCursorScreenPos(window_pos + ImVec2(1, 1));
        if (ImGui::BeginChild("##fusion_edit_blueprint", window_size - ImVec2(2, 2), false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings))
        {
            timeline->mVidOverlap->mFusion->mBp->Frame(true, true, overlap != nullptr, BluePrint::BluePrintFlag::BluePrintFlag_Fusion);
        }
        ImGui::EndChild();
    }
}

static void ShowVideoFusionPreviewWindow(ImDrawList *draw_list)
{
    // Draw Video Fusion Play control bar
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 window_pos = ImGui::GetCursorScreenPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    draw_list->AddRectFilled(window_pos, window_pos + window_size, COL_DEEP_DARK);
    ImVec2 PanelBarPos = window_pos + ImVec2(0, (window_size.y - 36));
    ImVec2 PanelBarSize = ImVec2(window_size.x, 36);
    draw_list->AddRectFilled(PanelBarPos, PanelBarPos + PanelBarSize, COL_DARK_PANEL);
    
    // Preview buttons Stop button is center of Panel bar
    auto PanelCenterX = PanelBarPos.x + window_size.x / 2;
    auto PanelButtonY = PanelBarPos.y + 2;
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0.5));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2, 0.2, 0.2, 1.0));
    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX - 16 - (32 + 8) * 3, PanelButtonY));
    if (ImGui::Button(ICON_TO_START "##video_fusion_tostart", ImVec2(32, 32)))
    {
        if (timeline && timeline->mVidOverlap)
            timeline->mVidOverlap->Seek(timeline->mVidOverlap->mStart);
    } ImGui::ShowTooltipOnHover("To Start");
    
    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX - 16 - (32 + 8) * 2, PanelButtonY));
    if (ImGui::Button(ICON_STEP_BACKWARD "##video_fusion_step_backward", ImVec2(32, 32)))
    {
        if (timeline && timeline->mVidOverlap)
        {
            if (timeline->currentTime > timeline->mVidOverlap->mStart)
                timeline->Step(false);
        }
    } ImGui::ShowTooltipOnHover("Step Prev");
    
    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX - 16 - (32 + 8) * 1, PanelButtonY));
    if (ImGui::RotateButton(ICON_PLAY_BACKWARD "##video_fusion_reverse", ImVec2(32, 32), 180))
    {
        if (timeline && timeline->mVidOverlap)
        {
            if (timeline->currentTime > timeline->mVidOverlap->mStart)
                timeline->Play(true, false);
        }
    } ImGui::ShowTooltipOnHover("Reverse");
    
    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX - 16, PanelButtonY));
    if (ImGui::Button(ICON_STOP "##video_fusion_stop", ImVec2(32, 32)))
    {
        if (timeline)
            timeline->Play(false, true);
    } ImGui::ShowTooltipOnHover("Stop");
    
    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX + 16 + 8, PanelButtonY));
    if (ImGui::Button(ICON_PLAY_FORWARD "##video_fusion_play", ImVec2(32, 32)))
    {
        if (timeline && timeline->mVidOverlap)
        {
            if (timeline->currentTime < timeline->mVidOverlap->mEnd)
                timeline->Play(true, true);
        }
    } ImGui::ShowTooltipOnHover("Play");
    
    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX + 16 + 8 + (32 + 8) * 1, PanelButtonY));
    if (ImGui::Button(ICON_STEP_FORWARD "##video_fusion_step_forward", ImVec2(32, 32)))
    {
        if (timeline && timeline->mVidOverlap)
        {
            if (timeline->currentTime < timeline->mVidOverlap->mEnd)
                timeline->Step(true);
        }
    } ImGui::ShowTooltipOnHover("Step Next");
    
    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX + 16 + 8 + (32 + 8) * 2, PanelButtonY));
    if (ImGui::Button(ICON_TO_END "##video_fusion_toend", ImVec2(32, 32)))
    {
        if (timeline && timeline->mVidOverlap)
            timeline->mVidOverlap->Seek(timeline->mVidOverlap->mEnd - 40);
    } ImGui::ShowTooltipOnHover("To End");

    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX + 16 + 8 + (32 + 8) * 4, PanelButtonY + 6));
    if (ImGui::CheckButton(timeline->bFusionOutputPreview ? ICON_MEDIA_PREVIEW : ICON_TRANS "##video_fusion_output_preview", &timeline->bFusionOutputPreview, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive)))
    {
        timeline->UpdatePreview();
    }
    ImGui::ShowTooltipOnHover(timeline->bFusionOutputPreview ? "Fusion Out" : "Preview Out");

    // Time stamp on left of control panel
    auto PanelRightX = PanelBarPos.x + window_size.x - 300;
    auto PanelRightY = PanelBarPos.y + 8;
    auto time_str = ImGuiHelper::MillisecToString(timeline->currentTime, 3);
    ImGui::SetWindowFontScale(1.5);
    draw_list->AddText(ImVec2(PanelRightX, PanelRightY), timeline->mIsPreviewPlaying ? COL_MARK : COL_MARK_HALF, time_str.c_str());
    ImGui::SetWindowFontScale(1.0);

    // fusion texture area
    ImVec2 InputFirstVideoPos = window_pos + ImVec2(4, 4);
    ImVec2 InputFirstVideoSize = ImVec2(window_size.x / 4 - 8, window_size.y - PanelBarSize.y - 8);
    ImVec2 OutputVideoPos = window_pos + ImVec2(window_size.x / 4 + 8, 4);
    ImVec2 OutputVideoSize = ImVec2(window_size.x / 2 - 32, window_size.y - PanelBarSize.y - 8);
    ImVec2 InputSecondVideoPos = window_pos + ImVec2(window_size.x * 3 / 4 - 8, 4);
    ImVec2 InputSecondVideoSize = InputFirstVideoSize;
    
    ImRect InputFirstVideoRect(InputFirstVideoPos, InputFirstVideoPos + InputFirstVideoSize);
    ImRect InputSecondVideoRect(InputSecondVideoPos, InputSecondVideoPos + InputSecondVideoSize);
    ImRect OutVideoRect(OutputVideoPos, OutputVideoPos + OutputVideoSize);
    
    if (timeline->mVidOverlap)
    {
        std::pair<std::pair<ImGui::ImMat, ImGui::ImMat>, ImGui::ImMat> pair;
        auto ret = timeline->mVidOverlap->GetFrame(pair, timeline->bFusionOutputPreview);
        if (ret && 
            (timeline->mIsPreviewNeedUpdate || timeline->mLastFrameTime == -1 || timeline->mLastFrameTime != (int64_t)(pair.first.first.time_stamp * 1000) || need_update_scope))
        {
            CalculateVideoScope(pair.second);
            ImGui::ImMatToTexture(pair.first.first, timeline->mVideoFusionInputFirstTexture);
            ImGui::ImMatToTexture(pair.first.second, timeline->mVideoFusionInputSecondTexture);
            ImGui::ImMatToTexture(pair.second, timeline->mVideoFusionOutputTexture);
            timeline->mLastFrameTime = pair.first.first.time_stamp * 1000;
            timeline->mIsPreviewNeedUpdate = false;
        }
        ImVec4 tint_col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);   // No tint
        ImVec4 border_col = ImVec4(1.0f, 1.0f, 1.0f, 0.5f); // 50% opaque white
        float offset_x = 0, offset_y = 0;
        float tf_x = 0, tf_y = 0;
        // fusion first input texture area
        ShowVideoWindow(draw_list, timeline->mVideoFusionInputFirstTexture, InputFirstVideoPos, InputFirstVideoSize, offset_x, offset_y, tf_x, tf_y);
        draw_list->AddRect(ImVec2(offset_x, offset_y), ImVec2(tf_x, tf_y), IM_COL32(128,128,128,128), 0, 0, 1.0);
        // fusion second input texture area
        ShowVideoWindow(draw_list, timeline->mVideoFusionInputSecondTexture, InputSecondVideoPos, InputSecondVideoSize, offset_x, offset_y, tf_x, tf_y);
        draw_list->AddRect(ImVec2(offset_x, offset_y), ImVec2(tf_x, tf_y), IM_COL32(128,128,128,128), 0, 0, 1.0);
        // filter output texture area
        ShowVideoWindow(draw_list, timeline->mVideoFusionOutputTexture, OutputVideoPos, OutputVideoSize, offset_x, offset_y, tf_x, tf_y);
        draw_list->AddRect(ImVec2(offset_x, offset_y), ImVec2(tf_x, tf_y), IM_COL32(192, 192, 192, 128), 0, 0, 2.0);
        if (timeline->mIsPreviewPlaying && (timeline->currentTime < timeline->mVidOverlap->mStart || timeline->currentTime > timeline->mVidOverlap->mEnd))
        {
            // reach clip border
            if (timeline->currentTime < timeline->mVidOverlap->mStart) { timeline->Play(false, false); timeline->Seek(timeline->mVidOverlap->mStart); }
            if (timeline->currentTime > timeline->mVidOverlap->mEnd) { timeline->Play(false, true); timeline->Seek(timeline->mVidOverlap->mEnd); }
        }
    }
    
    ImGui::PopStyleColor(3);
}

static void ShowVideoFusionWindow(ImDrawList *draw_list)
{
    /*
    ┏━━━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━┓
    ┃                   ┃                          ┃                     ┃
    ┃                   ┃                          ┃                     ┃
    ┃      first        ┃          preview         ┃       second        ┃
    ┃                   ┃                          ┃                     ┃
    ┃                   ┃                          ┃                     ┃
    ┣━━━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━┫
    ┃                          |<  <  []  >  >|                          ┃
    ┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━┫
    ┃          blueprint                         ┃                       ┃ 
    ┃                                            ┃                       ┃ 
    ┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫    fusion edit        ┃ 
    ┃             timeline                       ┃                       ┃
    ┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫                       ┃
    ┃              curves                        ┃                       ┃
    ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━┛
    */

    ImVec2 window_pos = ImGui::GetCursorScreenPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    draw_list->AddRectFilled(window_pos, window_pos + window_size, COL_DEEP_DARK);
    if (!timeline)
        return;
    
    Overlap * editing_overlap = timeline->FindEditingOverlap();

    if (editing_overlap)
    {
        auto clip_first = timeline->FindClipByID(editing_overlap->m_Clip.first);
        auto clip_second = timeline->FindClipByID(editing_overlap->m_Clip.second);
        if (!clip_first || !clip_second || 
            !IS_VIDEO(clip_first->mType) || !IS_VIDEO(clip_second->mType))
        {
            editing_overlap = nullptr;
        }
    }

    BluePrint::BluePrintUI* blueprint = nullptr;
    BluePrintVideoTransition * fusion = nullptr;

    if (editing_overlap)
    {
        if (!timeline->mVidOverlap)
        {
            timeline->mVidOverlap = new EditingVideoOverlap(editing_overlap);
            fusion = timeline->mVidOverlap->mFusion;
        }
        else if (timeline->mVidOverlap->mStart != editing_overlap->mStart || timeline->mVidOverlap->mEnd != editing_overlap->mEnd)
        {
            fusion = timeline->mVidOverlap->mFusion;
            // effect range changed for timeline->mVidOverlap
            timeline->mVidOverlap->mStart = editing_overlap->mStart;
            timeline->mVidOverlap->mEnd = editing_overlap->mEnd;
            timeline->mVidOverlap->mDuration = timeline->mVidOverlap->mEnd - timeline->mVidOverlap->mStart;
            if (fusion) fusion->mKeyPoints.SetMax(ImVec2(editing_overlap->mEnd - editing_overlap->mStart, 1.0f), true);
        }
        else
        {
            fusion = timeline->mVidOverlap->mFusion;
        }
        blueprint = fusion ? fusion->mBp : nullptr;
    }

    float clip_header_height = 30;
    float clip_channel_height = 50;
    float clip_timeline_height = clip_header_height + clip_channel_height * 2;
    float clip_keypoint_height = g_media_editor_settings.VideoFusionCurveExpanded ? 80 : 0;
    ImVec2 video_preview_pos = window_pos;
    float video_preview_height = (window_size.y - clip_timeline_height - clip_keypoint_height) * 2 / 3;
    float video_bluepoint_height = (window_size.y - clip_timeline_height - clip_keypoint_height) - video_preview_height;
    float clip_setting_width = 400;
    float clip_setting_height = window_size.y - video_preview_height;
    ImVec2 clip_setting_pos = video_preview_pos + ImVec2(window_size.x - clip_setting_width, video_preview_height);
    ImVec2 clip_setting_size(clip_setting_width, clip_setting_height);
    float video_preview_width = window_size.x;

    ImVec2 video_preview_size(video_preview_width, video_preview_height);
    ImVec2 video_bluepoint_pos = video_preview_pos + ImVec2(0, video_preview_height);
    ImVec2 video_bluepoint_size(window_size.x - clip_setting_width, video_bluepoint_height);
    ImVec2 clip_timeline_pos = video_bluepoint_pos + ImVec2(0, video_bluepoint_height);
    ImVec2 clip_timeline_size(window_size.x - clip_setting_width, clip_timeline_height);
    ImVec2 clip_keypoint_pos = g_media_editor_settings.VideoFusionCurveExpanded ? clip_timeline_pos + ImVec2(0, clip_timeline_height) : clip_timeline_pos + ImVec2(0, clip_timeline_height - 16);
    ImVec2 clip_keypoint_size(window_size.x - clip_setting_width, clip_keypoint_height);

    ImGuiWindowFlags child_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings;
    ImGuiWindowFlags setting_child_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;

    // draw video preview
    ImGui::SetCursorScreenPos(video_preview_pos);
    if (ImGui::BeginChild("##video_fusion_preview", video_preview_size, false, child_flags))
    {
        ImRect video_rect;
        ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
        ImVec2 sub_window_size = ImGui::GetWindowSize();
        draw_list->AddRectFilled(sub_window_pos, sub_window_pos + sub_window_size, COL_DEEP_DARK);
        ShowVideoFusionPreviewWindow(draw_list);
    }
    ImGui::EndChild();

    // draw overlap blueprint
    ImGui::SetCursorScreenPos(video_bluepoint_pos);
    if (ImGui::BeginChild("##video_fusion_blueprint", video_bluepoint_size, false, child_flags))
    {
        ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
        ImVec2 sub_window_size = ImGui::GetWindowSize();
        draw_list->AddRectFilled(sub_window_pos, sub_window_pos + sub_window_size, COL_DEEP_DARK);
        ShowVideoFusionBluePrintWindow(draw_list, editing_overlap);
    }
    ImGui::EndChild();

    // draw overlap timeline
    ImGui::SetCursorScreenPos(clip_timeline_pos);
    if (ImGui::BeginChild("##video_fusion_timeline", clip_timeline_size, false, child_flags))
    {
        ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
        ImVec2 sub_window_size = ImGui::GetWindowSize();
        draw_list->AddRectFilled(sub_window_pos, sub_window_pos + sub_window_size, COL_DARK_TWO);
        // Draw Clip TimeLine
        DrawOverlapTimeLine(timeline->mVidOverlap, timeline->currentTime - (timeline->mVidOverlap ? timeline->mVidOverlap->mStart : 0), clip_header_height, clip_channel_height);
    }
    ImGui::EndChild();

    // draw keypoint hidden button
    ImVec2 hidden_button_pos = clip_keypoint_pos - ImVec2(16, 0);
    ImRect hidden_button_rect = ImRect(hidden_button_pos, hidden_button_pos + ImVec2(16, 16));
    ImGui::SetWindowFontScale(0.75);
    if (hidden_button_rect.Contains(ImGui::GetMousePos()))
    {
        draw_list->AddRectFilled(hidden_button_rect.Min, hidden_button_rect.Max, IM_COL32(64,64,64,255));
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            g_media_editor_settings.VideoFusionCurveExpanded = !g_media_editor_settings.VideoFusionCurveExpanded;
        }
        if (ImGui::BeginTooltip())
        {
            ImGui::TextUnformatted(g_media_editor_settings.VideoFusionCurveExpanded ? "Hide Curve View" : "Show Curve View");
            ImGui::EndTooltip();
        }
    }
    draw_list->AddText(hidden_button_pos, IM_COL32_WHITE, ICON_FA_BEZIER_CURVE);
    ImGui::SetWindowFontScale(1.0);

    // draw fusion curve editor
    if (g_media_editor_settings.VideoFusionCurveExpanded)
    {
        ImGui::SetCursorScreenPos(clip_keypoint_pos);
        if (ImGui::BeginChild("##video_fusion_keypoint", clip_keypoint_size, false, child_flags))
        {
            ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
            ImVec2 sub_window_size = ImGui::GetWindowSize();
            draw_list->AddRectFilled(sub_window_pos, sub_window_pos + sub_window_size, COL_DARK_ONE);
            if (timeline->mVidOverlap && fusion)
            {
                bool _changed = false;
                float current_time = timeline->currentTime - timeline->mVidOverlap->mStart;
                mouse_hold |= ImGui::ImCurveEdit::Edit( nullptr,
                                                        &fusion->mKeyPoints, 
                                                        sub_window_size, 
                                                        ImGui::GetID("##video_fusion_keypoint_editor"),
                                                        true,
                                                        current_time,
                                                        CURVE_EDIT_FLAG_VALUE_LIMITED | CURVE_EDIT_FLAG_MOVE_CURVE | CURVE_EDIT_FLAG_KEEP_BEGIN_END | CURVE_EDIT_FLAG_DOCK_BEGIN_END, 
                                                        nullptr, // clippingRect
                                                        &_changed
                                                        );
                if (_changed) timeline->UpdatePreview();
            }
            // draw cursor line after curve draw
            if (timeline && timeline->mVidOverlap)
            {
                static const float cursorWidth = 2.f;
                float cursorOffset = sub_window_pos.x + (timeline->currentTime - timeline->mVidOverlap->mStart) * timeline->mVidOverlap->msPixelWidth - 0.5f;
                draw_list->AddLine(ImVec2(cursorOffset, sub_window_pos.y), ImVec2(cursorOffset, sub_window_pos.y + sub_window_size.y), COL_CURSOR_LINE_R, cursorWidth);
            }
        }
        ImGui::EndChild();
    }

    // draw overlap setting
    ImGui::SetCursorScreenPos(clip_setting_pos);
    if (ImGui::BeginChild("##video_fusion_setting", clip_setting_size, false, setting_child_flags))
    {
        auto addCurve = [&](std::string name, float _min, float _max, float _default)
        {
            if (fusion)
            {
                auto found = fusion->mKeyPoints.GetCurveIndex(name);
                if (found == -1)
                {
                    ImU32 color; ImGui::RandomColor(color, 1.f);
                    auto curve_index = fusion->mKeyPoints.AddCurve(name, ImGui::ImCurveEdit::Linear, color, true, _min, _max, _default);
                    fusion->mKeyPoints.AddPoint(curve_index, ImVec2(0.f, _min), ImGui::ImCurveEdit::Linear);
                    fusion->mKeyPoints.AddPoint(curve_index, ImVec2(timeline->mVidOverlap->mEnd - timeline->mVidOverlap->mStart, _max), ImGui::ImCurveEdit::Linear);
                    // insert curve pin for blueprint entry node
                    if (blueprint)
                    {
                        auto entry_node = blueprint->FindEntryPointNode();
                        if (entry_node) entry_node->InsertOutputPin(BluePrint::PinType::Float, name);
                        timeline->UpdatePreview();
                    }
                }
            }
        };
        ImVec2 sub_window_pos = ImGui::GetWindowPos(); // we need draw background with scroll view
        ImVec2 sub_window_size = ImGui::GetWindowSize();
        draw_list->AddRectFilled(sub_window_pos, sub_window_pos + sub_window_size, COL_BLACK_DARK);
        if (timeline->mVidOverlap && fusion)
        {
            // Overlap curve setting
            if (ImGui::TreeNodeEx("Curve Setting##video_fusion", ImGuiTreeNodeFlags_DefaultOpen))
            {
                char ** curve_type_list = nullptr;
                auto curve_type_count = ImGui::ImCurveEdit::GetCurveTypeName(curve_type_list);
                static std::string curve_name = "";
                std::string value = curve_name;
                bool name_input_empty = curve_name.empty();
                if (ImGui::InputTextWithHint("##new_curve_name_video_fusion", "Input curve name", (char*)value.data(), value.size() + 1, ImGuiInputTextFlags_CallbackEdit | ImGuiInputTextFlags_CallbackResize, [](ImGuiInputTextCallbackData* data) -> int
                {
                    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
                    {
                        auto& stringValue = *static_cast<string*>(data->UserData);
                        ImVector<char>* my_str = (ImVector<char>*)data->UserData;
                        //IM_ASSERT(stringValue.data() == data->Buf);
                        stringValue.resize(data->BufSize);
                        data->Buf = (char*)stringValue.data();
                    }
                    else if (data->EventFlag == ImGuiInputTextFlags_CallbackEdit)
                    {
                        auto& stringValue = *static_cast<string*>(data->UserData);
                        stringValue = std::string(data->Buf);
                    }
                    return 0;
                }, &value))
                {
                    value.resize(strlen(value.c_str()));
                    curve_name = value;
                    name_input_empty = curve_name.empty();
                }

                ImGui::BeginDisabled(name_input_empty);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
                ImGui::SameLine();
                if (ImGui::Button(ICON_ADD "##insert_curve_video_fusion"))
                {
                    addCurve(curve_name, 0.f, 1.f, 1.f);
                }
                ImGui::ShowTooltipOnHover("Add custom curve");
                ImGui::PopStyleVar();
                ImGui::EndDisabled();

                // list curves
                for (int i = 0; i < fusion->mKeyPoints.GetCurveCount(); i++)
                {
                    bool break_loop = false;
                    ImGui::PushID(i);
                    auto pCount = fusion->mKeyPoints.GetCurvePointCount(i);
                    std::string lable_id = std::string(ICON_CURVE) + " " + fusion->mKeyPoints.GetCurveName(i) + " (" + std::to_string(pCount) + " keys)" + "##video_fusion_curve";
                    if (ImGui::TreeNodeEx(lable_id.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
                        float value = fusion->mKeyPoints.GetValue(i, timeline->currentTime - timeline->mVidOverlap->mStart);
                        ImGui::BracketSquare(true); ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0, 1.0, 0.0, 1.0)); ImGui::Text("%.2f", value); ImGui::PopStyleColor();
                        ImGui::PushItemWidth(60);
                        float curve_min = fusion->mKeyPoints.GetCurveMin(i);
                        float curve_max = fusion->mKeyPoints.GetCurveMax(i);
                        if (ImGui::DragFloat("##curve_video_fusion_min", &curve_min, 0.1f, -FLT_MAX, curve_max, "%.1f"))
                        {
                            fusion->mKeyPoints.SetCurveMin(i, curve_min);
                            timeline->UpdatePreview();
                        } ImGui::ShowTooltipOnHover("Min");
                        ImGui::SameLine(0, 8);
                        if (ImGui::DragFloat("##curve_video_fusion_max", &curve_max, 0.1f, curve_min, FLT_MAX, "%.1f"))
                        {
                            fusion->mKeyPoints.SetCurveMax(i, curve_max);
                            timeline->UpdatePreview();
                        } ImGui::ShowTooltipOnHover("Max");
                        ImGui::SameLine(0, 8);
                        float curve_default = fusion->mKeyPoints.GetCurveDefault(i);
                        if (ImGui::DragFloat("##curve_video_fusion_default", &curve_default, 0.1f, curve_min, curve_max, "%.1f"))
                        {
                            fusion->mKeyPoints.SetCurveDefault(i, curve_default);
                            timeline->UpdatePreview();
                        } ImGui::ShowTooltipOnHover("Default");
                        ImGui::PopItemWidth();

                        ImGui::SameLine(0, 8);
                        ImGui::SetWindowFontScale(0.75);
                        auto curve_color = ImGui::ColorConvertU32ToFloat4(fusion->mKeyPoints.GetCurveColor(i));
                        if (ImGui::ColorEdit4("##curve_video_fusion_color", (float*)&curve_color, ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar))
                        {
                            fusion->mKeyPoints.SetCurveColor(i, ImGui::ColorConvertFloat4ToU32(curve_color));
                        } ImGui::ShowTooltipOnHover("Curve Color");
                        ImGui::SetWindowFontScale(1.0);
                        ImGui::SameLine(0, 4);
                        bool is_visiable = fusion->mKeyPoints.IsVisible(i);
                        if (ImGui::Button(is_visiable ? ICON_WATCH : ICON_UNWATCH "##curve_video_fusion_visiable"))
                        {
                            is_visiable = !is_visiable;
                            fusion->mKeyPoints.SetCurveVisible(i, is_visiable);
                        } ImGui::ShowTooltipOnHover( is_visiable ? "Hide" : "Show");
                        ImGui::SameLine(0, 4);
                        if (ImGui::Button(ICON_DELETE "##curve_video_fusion_delete"))
                        {
                            // delete blueprint entry node pin
                            auto pin_name = fusion->mKeyPoints.GetCurveName(i);
                            if (blueprint)
                            {
                                auto entry_node = blueprint->FindEntryPointNode();
                                if (entry_node) entry_node->DeleteOutputPin(pin_name);
                                timeline->UpdatePreview();
                            }
                            fusion->mKeyPoints.DeleteCurve(i);
                            break_loop = true;
                        } ImGui::ShowTooltipOnHover("Delete");
                        ImGui::SameLine(0, 4);
                        if (ImGui::Button(ICON_RETURN_DEFAULT "##curve_video_fusion_reset"))
                        {
                            for (int p = 0; p < pCount; p++)
                            {
                                fusion->mKeyPoints.SetCurvePointDefault(i, p);
                            }
                            timeline->UpdatePreview();
                        } ImGui::ShowTooltipOnHover("Reset");

                        if (!break_loop)
                        {
                            // list points
                            for (int p = 0; p < pCount; p++)
                            {
                                bool is_disabled = false;
                                ImGui::PushID(p);
                                ImGui::PushItemWidth(96);
                                auto point = fusion->mKeyPoints.GetPoint(i, p);
                                ImGui::Diamond(false);
                                if (p == 0 || p == pCount - 1)
                                    is_disabled = true;
                                ImGui::BeginDisabled(is_disabled);
                                if (ImGui::DragTimeMS("##curve_video_fusion_point_x", &point.point.x, fusion->mKeyPoints.GetMax().x / 1000.f, fusion->mKeyPoints.GetMin().x, fusion->mKeyPoints.GetMax().x, 2))
                                {
                                    fusion->mKeyPoints.EditPoint(i, p, point.point, point.type);
                                    timeline->UpdatePreview();
                                }
                                ImGui::EndDisabled();
                                ImGui::SameLine();
                                if (ImGui::DragFloat("##curve_video_fusion_point_y", &point.point.y, 0.01f, fusion->mKeyPoints.GetCurveMin(i), fusion->mKeyPoints.GetCurveMax(i), "%.2f"))
                                {
                                    fusion->mKeyPoints.EditPoint(i, p, point.point, point.type);
                                    timeline->UpdatePreview();
                                }
                                ImGui::SameLine();
                                if (ImGui::Combo("##curve_video_fusion_type", (int*)&point.type, curve_type_list, curve_type_count))
                                {
                                    fusion->mKeyPoints.EditPoint(i, p, point.point, point.type);
                                    timeline->UpdatePreview();
                                }
                                ImGui::PopItemWidth();
                                ImGui::PopID();
                            }
                        }
                        ImGui::PopStyleColor();
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                    if (break_loop) break;
                }

                ImGui::TreePop();
            }
            // Overlap Node setting
            if (blueprint && blueprint->Blueprint_IsValid())
            {
                if (ImGui::TreeNodeEx("Node Configure##video_fusion", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto nodes = blueprint->m_Document->m_Blueprint.GetNodes();
                    for (auto node : nodes)
                    {
                        auto type = node->GetTypeInfo().m_Type;
                        if (type == BluePrint::NodeType::EntryPoint || type == BluePrint::NodeType::ExitPoint)
                            continue;
                        if (!node->CustomLayout())
                            continue;
                        auto label_name = node->m_Name;
                        std::string lable_id = label_name + "##video_fusion_node" + "@" + std::to_string(node->m_ID);
                        node->DrawNodeLogo(ImGui::GetCurrentContext(), ImVec2(50, 28));
                        ImGui::SameLine(70);
                        if (ImGui::TreeNodeEx(lable_id.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            ImGui::ImCurveEdit::keys key;
                            key.m_id = node->m_ID;
                            if (node->DrawCustomLayout(ImGui::GetCurrentContext(), 1.0, ImVec2(0, 0), &key))
                            {
                                node->m_NeedUpdate = true;
                                timeline->UpdatePreview();
                            }
                            if (!key.name.empty())
                            {
                                addCurve(key.name, key.m_min, key.m_max, key.m_default);
                            }
                            ImGui::TreePop();
                        }
                    }
                    ImGui::TreePop();
                }
            }
        }
    }
    ImGui::EndChild();
}

static void ShowVideoEditorWindow(ImDrawList *draw_list)
{
    float labelWidth = ImGui::CalcVerticalTabLabelsWidth() + 4;
    ImVec2 clip_window_pos = ImGui::GetCursorScreenPos();
    ImVec2 clip_window_size = ImGui::GetWindowSize();
    static const int numTabs = sizeof(VideoEditorTabNames)/sizeof(VideoEditorTabNames[0]);
    if (ImGui::TabLabelsVertical(false, numTabs, VideoEditorTabNames, VideoEditorWindowIndex, VideoEditorTabTooltips, true, nullptr, nullptr, false, false, nullptr, nullptr))
    {
        UIPageChanged();
    }
    ImGui::SetCursorScreenPos(clip_window_pos + ImVec2(labelWidth, 0));
    if (ImGui::BeginChild("##video_editor_views", ImVec2(clip_window_size.x - labelWidth, clip_window_size.y), false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings /*| ImGuiWindowFlags_NoScrollWithMouse*/))
    {
        ImVec2 editor_view_window_pos = ImGui::GetCursorScreenPos();
        ImVec2 editor_view_window_size = ImGui::GetWindowSize();
        draw_list->AddRectFilled(editor_view_window_pos, editor_view_window_pos + editor_view_window_size, COL_DARK_ONE);
        switch (VideoEditorWindowIndex)
        {
            case 0: ShowVideoFilterWindow(draw_list); break;
            case 1: ShowVideoFusionWindow(draw_list); break;
            case 2: ShowVideoAttributeWindow(draw_list); break;
            default: break;
        }
    }
    ImGui::EndChild();
}
/****************************************************************************************
 * 
 * Audio Editor windows
 *
 ***************************************************************************************/
static void ShowAudioFilterBluePrintWindow(ImDrawList *draw_list, Clip * clip)
{
    if (timeline && timeline->mAudFilterClip && timeline->mAudFilterClip->mFilter && timeline->mAudFilterClip->mFilter->mBp)
    {
        if (clip && !timeline->mAudFilterClip->mFilter->mBp->m_Document->m_Blueprint.IsOpened())
        {
            auto track = timeline->FindTrackByClipID(clip->mID);
            if (track)
                track->SelectEditingClip(clip, true);
            timeline->mAudFilterClip->mFilter->mBp->View_ZoomToContent();
        }
        ImVec2 window_pos = ImGui::GetCursorScreenPos();
        ImVec2 window_size = ImGui::GetWindowSize();
        ImGui::SetCursorScreenPos(window_pos + ImVec2(3, 3));
        ImGui::InvisibleButton("audio_editor_blueprint_back_view", window_size - ImVec2(6, 6));
        if (ImGui::BeginDragDropTarget() && timeline->mAudFilterClip->mFilter->mBp->Blueprint_IsValid())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Filter_drag_drop_Audio"))
            {
                const BluePrint::Node * node = (const BluePrint::Node *)payload->Data;
                if (node)
                {
                    timeline->mAudFilterClip->mFilter->mBp->Edit_Insert(node->GetTypeID());
                }
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::SetCursorScreenPos(window_pos + ImVec2(1, 1));
        if (ImGui::BeginChild("##audio_editor_blueprint", window_size - ImVec2(2, 2), false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings))
        {
            timeline->mAudFilterClip->mFilter->mBp->Frame(true, true, clip != nullptr, BluePrint::BluePrintFlag::BluePrintFlag_Filter);
        }
        ImGui::EndChild();
    }
}

static void ShowAudioFilterWindow(ImDrawList *draw_list)
{
    /*
    ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━┓
    ┃                                            ┃                       ┃
    ┃                                            ┃   blueprint area      ┃
    ┃                                            ┃                       ┃
    ┃               preview                      ┃                       ┃
    ┃                                            ┃                       ┃ 
    ┃                                            ┃                       ┃ 
    ┃                                            ┣━━━━━━━━━━━━━━━━━━━━━━━┫ 
    ┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫                       ┃ 
    ┃             |<  <  []  >  >|               ┃    filter edit        ┃ 
    ┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫                       ┃
    ┃             timeline                       ┃                       ┃
    ┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫                       ┃
    ┃              curves                        ┃                       ┃
    ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━┛
    */

    ImVec2 window_pos = ImGui::GetCursorScreenPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    draw_list->AddRectFilled(window_pos, window_pos + window_size, COL_DEEP_DARK);
    float audio_view_width = window_size.x * 2 / 3;
    float audio_editor_width = window_size.x - audio_view_width;
    if (!timeline)
        return;

    BluePrintAudioFilter * filter = nullptr;
    BluePrint::BluePrintUI* blueprint = nullptr;

    Clip * editing_clip = timeline->FindEditingClip();
    if (editing_clip && editing_clip->mType != MEDIA_AUDIO)
    {
        editing_clip = nullptr;
    }

    if (editing_clip)
    {
        if (timeline->mAudFilterClipLock.try_lock())
        {
            if (timeline->mAudFilterClip)
                timeline->mAudFilterClip->UpdateClipRange(editing_clip);
            else
                timeline->mAudFilterClip = new EditingAudioClip((AudioClip*)editing_clip);
            timeline->mAudFilterClipLock.unlock();
        }
        filter = timeline->mAudFilterClip->mFilter;
        blueprint = filter ? filter->mBp : nullptr;
    }

    float clip_header_height = 30 + 12;
    float clip_channel_height = 50;
    float clip_timeline_height = clip_header_height;
    float clip_keypoint_height = g_media_editor_settings.AudioFilterCurveExpanded ? 100 : 0;
    ImVec2 preview_pos = window_pos;
    float preview_width = audio_view_width;

    if (editing_clip)
    {
        int channels = ((AudioClip *)editing_clip)->mAudioChannels;
        clip_timeline_height += channels * clip_channel_height;
    }
    float preview_height = window_size.y - clip_timeline_height - clip_keypoint_height - 4;
    float audio_blueprint_height = window_size.y / 2;
    float clip_timeline_width = audio_view_width;
    ImVec2 clip_timeline_pos = window_pos + ImVec2(0, preview_height);
    ImVec2 clip_timeline_size(clip_timeline_width, clip_timeline_height);
    ImVec2 clip_keypoint_pos = g_media_editor_settings.AudioFilterCurveExpanded ? clip_timeline_pos + ImVec2(0, clip_timeline_height) : clip_timeline_pos + ImVec2(0, clip_timeline_height - 16);
    ImVec2 clip_keypoint_size(audio_view_width, clip_keypoint_height);
    ImVec2 clip_setting_pos = window_pos + ImVec2(audio_view_width, window_size.y / 2);
    ImVec2 clip_setting_size(audio_editor_width, window_size.y / 2);

    ImGuiWindowFlags child_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings;
    ImGuiWindowFlags setting_child_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;

    if (ImGui::BeginChild("##audio_filter_preview", ImVec2(preview_width, preview_height), false, child_flags))
    {
        ImRect video_rect;
        ImVec2 audio_view_window_pos = ImGui::GetCursorScreenPos();
        ImVec2 audio_view_window_size = ImGui::GetWindowSize();
        draw_list->AddRectFilled(audio_view_window_pos, audio_view_window_pos + audio_view_window_size, COL_DEEP_DARK);
        ShowMediaPreviewWindow(draw_list, "Audio Filter", video_rect, editing_clip ? editing_clip->mStart : -1, editing_clip ? editing_clip->mEnd : -1, true, false, false);
    }
    ImGui::EndChild();

    ImGui::SetCursorScreenPos(clip_timeline_pos);
    if (ImGui::BeginChild("##audio_filter_timeline", clip_timeline_size + ImVec2(0, clip_keypoint_height), false, child_flags))
    {
        ImVec2 clip_timeline_window_pos = ImGui::GetCursorScreenPos();
        ImVec2 clip_timeline_window_size = ImGui::GetWindowSize();
        draw_list->AddRectFilled(clip_timeline_window_pos, clip_timeline_window_pos + clip_timeline_window_size, COL_DARK_TWO);
        mouse_hold |= DrawClipTimeLine(timeline, timeline->mAudFilterClip, timeline->currentTime, clip_header_height, clip_timeline_height - clip_header_height - 12, clip_keypoint_height, filter ? &filter->mKeyPoints : nullptr);
    }
    ImGui::EndChild();

    // draw keypoint hidden button
    ImVec2 hidden_button_pos = clip_keypoint_pos - ImVec2(16, 0);
    ImRect hidden_button_rect = ImRect(hidden_button_pos, hidden_button_pos + ImVec2(16, 16));
    ImGui::SetWindowFontScale(0.75);
    if (hidden_button_rect.Contains(ImGui::GetMousePos()))
    {
        draw_list->AddRectFilled(hidden_button_rect.Min, hidden_button_rect.Max, IM_COL32(64,64,64,255));
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            g_media_editor_settings.AudioFilterCurveExpanded = !g_media_editor_settings.AudioFilterCurveExpanded;
        }
        if (ImGui::BeginTooltip())
        {
            ImGui::TextUnformatted(g_media_editor_settings.AudioFilterCurveExpanded ? "Hide Curve View" : "Show Curve View");
            ImGui::EndTooltip();
        }
    }
    draw_list->AddText(hidden_button_pos, IM_COL32_WHITE, ICON_FA_BEZIER_CURVE);
    ImGui::SetWindowFontScale(1.0);

    ImGui::SetCursorScreenPos(window_pos + ImVec2(audio_view_width, 0));
    if (ImGui::BeginChild("##audio_filter_blueprint", ImVec2(audio_editor_width, audio_blueprint_height), false, child_flags))
    {
        ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
        ImVec2 sub_window_size = ImGui::GetWindowSize();
        draw_list->AddRectFilled(sub_window_pos, sub_window_pos + sub_window_size, COL_DARK_ONE);
        ShowAudioFilterBluePrintWindow(draw_list, editing_clip);
    }
    ImGui::EndChild();

    // Draw Audio filter setting
    ImGui::SetCursorScreenPos(clip_setting_pos);
    if (ImGui::BeginChild("##audio_filter_setting", clip_setting_size, false, setting_child_flags))
    {
        auto addCurve = [&](std::string name, float _min, float _max, float _default)
        {
            if (filter)
            {
                auto found = filter->mKeyPoints.GetCurveIndex(name);
                if (found == -1)
                {
                    ImU32 color; ImGui::RandomColor(color, 1.f);
                    auto curve_index = filter->mKeyPoints.AddCurve(name, ImGui::ImCurveEdit::Smooth, color, true, _min, _max, _default);
                    filter->mKeyPoints.AddPoint(curve_index, ImVec2(0, _min), ImGui::ImCurveEdit::Smooth);
                    filter->mKeyPoints.AddPoint(curve_index, ImVec2(editing_clip->mEnd - editing_clip->mStart, _max), ImGui::ImCurveEdit::Smooth);
                    filter->mKeyPoints.SetCurvePointDefault(curve_index, 0);
                    filter->mKeyPoints.SetCurvePointDefault(curve_index, 1);
                    if (blueprint)
                    {
                        auto entry_node = blueprint->FindEntryPointNode();
                        if (entry_node) entry_node->InsertOutputPin(BluePrint::PinType::Float, name);
                        timeline->UpdatePreview();
                    }
                }
            }
        };
        ImVec2 sub_window_pos = ImGui::GetWindowPos(); // we need draw background with scroll view
        ImVec2 sub_window_size = ImGui::GetWindowSize();
        draw_list->AddRectFilled(sub_window_pos, sub_window_pos + sub_window_size, COL_BLACK_DARK);
        if (timeline->mAudFilterClip && filter)
        {
            // Filter curve setting
            if (ImGui::TreeNodeEx("Curve Setting##audio_filter", ImGuiTreeNodeFlags_DefaultOpen))
            {
                char ** curve_type_list = nullptr;
                auto curve_type_count = ImGui::ImCurveEdit::GetCurveTypeName(curve_type_list);
                static std::string curve_name = "";
                std::string value = curve_name;
                bool name_input_empty = curve_name.empty();
                if (ImGui::InputTextWithHint("##new_curve_name_audio_filter", "Input curve name", (char*)value.data(), value.size() + 1, ImGuiInputTextFlags_CallbackEdit | ImGuiInputTextFlags_CallbackResize, [](ImGuiInputTextCallbackData* data) -> int
                {
                    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
                    {
                        auto& stringValue = *static_cast<string*>(data->UserData);
                        ImVector<char>* my_str = (ImVector<char>*)data->UserData;
                        //IM_ASSERT(stringValue.data() == data->Buf);
                        stringValue.resize(data->BufSize);
                        data->Buf = (char*)stringValue.data();
                    }
                    else if (data->EventFlag == ImGuiInputTextFlags_CallbackEdit)
                    {
                        auto& stringValue = *static_cast<string*>(data->UserData);
                        stringValue = std::string(data->Buf);
                    }
                    return 0;
                }, &value))
                {
                    value.resize(strlen(value.c_str()));
                    curve_name = value;
                    name_input_empty = curve_name.empty();
                }

                ImGui::BeginDisabled(name_input_empty);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
                ImGui::SameLine();
                if (ImGui::Button(ICON_ADD "##insert_curve_audio_filter"))
                {
                    addCurve(curve_name, 0.f, 1.f, 0.5);
                }
                ImGui::ShowTooltipOnHover("Add custom curve");
                ImGui::PopStyleVar();
                ImGui::EndDisabled();

                // list curves
                for (int i = 0; i < filter->mKeyPoints.GetCurveCount(); i++)
                {
                    bool break_loop = false;
                    ImGui::PushID(i);
                    auto pCount = filter->mKeyPoints.GetCurvePointCount(i);
                    std::string lable_id = std::string(ICON_CURVE) + " " + filter->mKeyPoints.GetCurveName(i) + " (" + std::to_string(pCount) + " keys)" + "##audio_filter_curve";
                    if (ImGui::TreeNodeEx(lable_id.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
                        float value = filter->mKeyPoints.GetValue(i, timeline->currentTime - timeline->mAudFilterClip->mStart);
                        ImGui::BracketSquare(true); ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0, 1.0, 0.0, 1.0)); ImGui::Text("%.2f", value); ImGui::PopStyleColor();
                        ImGui::PushItemWidth(60);
                        float curve_min = filter->mKeyPoints.GetCurveMin(i);
                        float curve_max = filter->mKeyPoints.GetCurveMax(i);
                        if (ImGui::DragFloat("##curve_audio_filter_min", &curve_min, 0.1f, -FLT_MAX, curve_max, "%.1f"))
                        {
                            filter->mKeyPoints.SetCurveMin(i, curve_min);
                            timeline->UpdatePreview();
                        } ImGui::ShowTooltipOnHover("Min");
                        ImGui::SameLine(0, 8);
                        if (ImGui::DragFloat("##curve_audio_filter_max", &curve_max, 0.1f, curve_min, FLT_MAX, "%.1f"))
                        {
                            filter->mKeyPoints.SetCurveMax(i, curve_max);
                            timeline->UpdatePreview();
                        } ImGui::ShowTooltipOnHover("Max");
                        ImGui::SameLine(0, 8);
                        float curve_default = filter->mKeyPoints.GetCurveDefault(i);
                        if (ImGui::DragFloat("##curve_audio_filter_default", &curve_default, 0.1f, curve_min, curve_max, "%.1f"))
                        {
                            filter->mKeyPoints.SetCurveDefault(i, curve_default);
                            timeline->UpdatePreview();
                        } ImGui::ShowTooltipOnHover("Default");
                        ImGui::PopItemWidth();
                        
                        ImGui::SameLine(0, 8);
                        ImGui::SetWindowFontScale(0.75);
                        auto curve_color = ImGui::ColorConvertU32ToFloat4(filter->mKeyPoints.GetCurveColor(i));
                        if (ImGui::ColorEdit4("##curve_audio_filter_color", (float*)&curve_color, ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar))
                        {
                            filter->mKeyPoints.SetCurveColor(i, ImGui::ColorConvertFloat4ToU32(curve_color));
                        } ImGui::ShowTooltipOnHover("Curve Color");
                        ImGui::SetWindowFontScale(1.0);
                        ImGui::SameLine(0, 4);
                        bool is_visiable = filter->mKeyPoints.IsVisible(i);
                        if (ImGui::Button(is_visiable ? ICON_WATCH : ICON_UNWATCH "##curve_audio_filter_visiable"))
                        {
                            is_visiable = !is_visiable;
                            filter->mKeyPoints.SetCurveVisible(i, is_visiable);
                        } ImGui::ShowTooltipOnHover( is_visiable ? "Hide" : "Show");
                        ImGui::SameLine(0, 4);
                        if (ImGui::Button(ICON_DELETE "##curve_audio_filter_delete"))
                        {
                            // delete blueprint entry node pin
                            auto pin_name = filter->mKeyPoints.GetCurveName(i);
                            if (blueprint)
                            {
                                auto entry_node = blueprint->FindEntryPointNode();
                                if (entry_node) entry_node->DeleteOutputPin(pin_name);
                                timeline->UpdatePreview();
                            }
                            filter->mKeyPoints.DeleteCurve(i);
                            break_loop = true;
                        } ImGui::ShowTooltipOnHover("Delete");
                        ImGui::SameLine(0, 4);
                        if (ImGui::Button(ICON_RETURN_DEFAULT "##curve_audio_filter_reset"))
                        {
                            for (int p = 0; p < pCount; p++)
                            {
                                filter->mKeyPoints.SetCurvePointDefault(i, p);
                            }
                            timeline->UpdatePreview();
                        } ImGui::ShowTooltipOnHover("Reset");

                        if (!break_loop)
                        {
                            // list points
                            for (int p = 0; p < pCount; p++)
                            {
                                bool is_disabled = false;
                                ImGui::PushID(p);
                                ImGui::PushItemWidth(96);
                                auto point = filter->mKeyPoints.GetPoint(i, p);
                                ImGui::Diamond(false);
                                if (p == 0 || p == pCount - 1)
                                    is_disabled = true;
                                ImGui::BeginDisabled(is_disabled);
                                if (ImGui::DragTimeMS("##curve_audio_filter_point_x", &point.point.x, filter->mKeyPoints.GetMax().x / 1000.f, filter->mKeyPoints.GetMin().x, filter->mKeyPoints.GetMax().x, 2))
                                {
                                    filter->mKeyPoints.EditPoint(i, p, point.point, point.type);
                                    timeline->UpdatePreview();
                                }
                                ImGui::EndDisabled();
                                ImGui::SameLine();
                                auto speed = fabs(filter->mKeyPoints.GetCurveMax(i) - filter->mKeyPoints.GetCurveMin(i)) / 500;
                                if (ImGui::DragFloat("##curve_audio_filter_point_y", &point.point.y, speed, filter->mKeyPoints.GetCurveMin(i), filter->mKeyPoints.GetCurveMax(i), "%.2f"))
                                {
                                    filter->mKeyPoints.EditPoint(i, p, point.point, point.type);
                                    timeline->UpdatePreview();
                                }
                                ImGui::SameLine();
                                if (ImGui::Combo("##curve_audio_filter_type", (int*)&point.type, curve_type_list, curve_type_count))
                                {
                                    filter->mKeyPoints.EditPoint(i, p, point.point, point.type);
                                    timeline->UpdatePreview();
                                }
                                ImGui::PopItemWidth();
                                ImGui::PopID();
                            }
                        }
                        ImGui::PopStyleColor();
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                    if (break_loop) break;
                }

                ImGui::TreePop();
            }
            // Filter Node setting
            if (blueprint && blueprint->Blueprint_IsValid())
            {
                if (ImGui::TreeNodeEx("Node Configure##audio_filter", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto nodes = blueprint->m_Document->m_Blueprint.GetNodes();
                    for (auto node : nodes)
                    {
                        auto type = node->GetTypeInfo().m_Type;
                        if (type == BluePrint::NodeType::EntryPoint || type == BluePrint::NodeType::ExitPoint)
                            continue;
                        if (!node->CustomLayout())
                            continue;
                        auto label_name = node->m_Name;
                        std::string lable_id = label_name + "##audio_filter_node" + "@" + std::to_string(node->m_ID);
                        node->DrawNodeLogo(ImGui::GetCurrentContext(), ImVec2(28, 28));
                        ImGui::SameLine(40);
                        if (ImGui::TreeNodeEx(lable_id.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            ImGui::ImCurveEdit::keys key;
                            key.m_id = node->m_ID;
                            if (node->DrawCustomLayout(ImGui::GetCurrentContext(), 1.0, ImVec2(0, 0), &key))
                            {
                                node->m_NeedUpdate = true;
                                timeline->UpdatePreview();
                            }
                            if (!key.name.empty())
                            {
                                addCurve(key.name, key.m_min, key.m_max, key.m_default);
                            }
                            ImGui::TreePop();
                        }
                    }
                    ImGui::TreePop();
                }
            }
        }
    }
    ImGui::EndChild();
}

static void ShowAudioFusionBluePrintWindow(ImDrawList *draw_list, Overlap * overlap)
{
    if (timeline && timeline->mAudOverlap && timeline->mAudOverlap->mFusion && timeline->mAudOverlap->mFusion->mBp)
    {
        if (overlap && !timeline->mAudOverlap->mFusion->mBp->m_Document->m_Blueprint.IsOpened())
        {
            auto track = timeline->FindTrackByClipID(overlap->m_Clip.first);
            if (track)
                track->SelectEditingOverlap(overlap);
            timeline->mAudOverlap->mFusion->mBp->View_ZoomToContent();
        }
        ImVec2 window_pos = ImGui::GetCursorScreenPos();
        ImVec2 window_size = ImGui::GetWindowSize();
        ImGui::SetCursorScreenPos(window_pos + ImVec2(3, 3));
        ImGui::InvisibleButton("audio_fusion_blueprint_back_view", window_size - ImVec2(6, 6));
        if (ImGui::BeginDragDropTarget() && timeline->mAudOverlap->mFusion->mBp->Blueprint_IsValid())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Fusion_drag_drop_Audio"))
            {
                const BluePrint::Node * node = (const BluePrint::Node *)payload->Data;
                if (node)
                {
                    timeline->mAudOverlap->mFusion->mBp->Edit_Insert(node->GetTypeID());
                }
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::SetCursorScreenPos(window_pos + ImVec2(1, 1));
        if (ImGui::BeginChild("##audio_fusion_blueprint", window_size - ImVec2(2, 2), false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings))
        {
            timeline->mAudOverlap->mFusion->mBp->Frame(true, true, overlap != nullptr, BluePrint::BluePrintFlag::BluePrintFlag_Fusion);
        }
        ImGui::EndChild();
    }
}

static void ShowAudioFusionWindow(ImDrawList *draw_list)
{
    /*
    ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━┓
    ┃                                            ┃                       ┃
    ┃                                            ┃   blueprint area      ┃
    ┃                                            ┃                       ┃
    ┃               preview                      ┃                       ┃
    ┃                                            ┃                       ┃ 
    ┃                                            ┣━━━━━━━━━━━━━━━━━━━━━━━┫ 
    ┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫                       ┃ 
    ┃             |<  <  []  >  >|               ┃    filter edit        ┃ 
    ┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫                       ┃
    ┃             timeline one                   ┃                       ┃
    ┃             timeline two                   ┃                       ┃
    ┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫                       ┃
    ┃              curves                        ┃                       ┃
    ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━┛
    */
    ImVec2 window_pos = ImGui::GetCursorScreenPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    draw_list->AddRectFilled(window_pos, window_pos + window_size, COL_DEEP_DARK);
    float audio_view_width = window_size.x * 2 / 3;
    float audio_editor_width = window_size.x - audio_view_width;
    if (!timeline)
        return;

    Overlap * editing_overlap = timeline->FindEditingOverlap();

    if (editing_overlap)
    {
        auto clip_first = timeline->FindClipByID(editing_overlap->m_Clip.first);
        auto clip_second = timeline->FindClipByID(editing_overlap->m_Clip.second);
        if (!clip_first || !clip_second || 
            !IS_AUDIO(clip_first->mType) || !IS_AUDIO(clip_second->mType))
        {
            editing_overlap = nullptr;
        }
    }

    BluePrint::BluePrintUI* blueprint = nullptr;
    BluePrintAudioTransition * fusion = nullptr;

    if (editing_overlap)
    {
        if (!timeline->mAudOverlap)
        {
            timeline->mAudOverlap = new EditingAudioOverlap(editing_overlap);
            fusion = timeline->mAudOverlap->mFusion;
        }
        else if (timeline->mAudOverlap->mStart != editing_overlap->mStart || timeline->mAudOverlap->mEnd != editing_overlap->mEnd)
        {
            fusion = timeline->mAudOverlap->mFusion;
            // effect range changed for timeline->mAudOverlap
            timeline->mAudOverlap->mStart = editing_overlap->mStart;
            timeline->mAudOverlap->mEnd = editing_overlap->mEnd;
            timeline->mAudOverlap->mDuration = timeline->mAudOverlap->mEnd - timeline->mAudOverlap->mStart;
            if (fusion) fusion->mKeyPoints.SetMax(ImVec2(editing_overlap->mEnd - editing_overlap->mStart, 1.0f), true);
        }
        else
        {
            fusion = timeline->mAudOverlap->mFusion;
        }
        blueprint = fusion ? fusion->mBp : nullptr;
    }

    float clip_header_height = 30;
    float clip_channel_height = 60;
    float clip_timeline_height = clip_header_height + clip_channel_height * 2;
    float clip_keypoint_height = g_media_editor_settings.AudioFusionCurveExpanded ? 120 : 0;
    ImVec2 preview_pos = window_pos;
    float preview_width = audio_view_width;

    float preview_height = window_size.y - clip_timeline_height - clip_keypoint_height - 4;
    float audio_blueprint_height = window_size.y / 2;
    float clip_timeline_width = audio_view_width;
    ImVec2 clip_timeline_pos = window_pos + ImVec2(0, preview_height);
    ImVec2 clip_timeline_size(clip_timeline_width, clip_timeline_height);
    ImVec2 clip_keypoint_pos = g_media_editor_settings.AudioFusionCurveExpanded ? clip_timeline_pos + ImVec2(0, clip_timeline_height) : clip_timeline_pos + ImVec2(0, clip_timeline_height - 16);
    ImVec2 clip_keypoint_size(audio_view_width, clip_keypoint_height);
    ImVec2 clip_setting_pos = window_pos + ImVec2(audio_view_width, window_size.y / 2);
    ImVec2 clip_setting_size(audio_editor_width, window_size.y / 2);

    ImGuiWindowFlags child_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings;
    ImGuiWindowFlags setting_child_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;

    if (ImGui::BeginChild("##audio_fusion_preview", ImVec2(preview_width, preview_height), false, child_flags))
    {
        ImRect video_rect;
        ImVec2 audio_view_window_pos = ImGui::GetCursorScreenPos();
        ImVec2 audio_view_window_size = ImGui::GetWindowSize();
        draw_list->AddRectFilled(audio_view_window_pos, audio_view_window_pos + audio_view_window_size, COL_DEEP_DARK);
        ShowMediaPreviewWindow(draw_list, "Audio Fusion", video_rect, editing_overlap ? editing_overlap->mStart : -1, editing_overlap ? editing_overlap->mEnd : -1, true, false, false);
    }
    ImGui::EndChild();

    ImGui::SetCursorScreenPos(clip_timeline_pos);
    if (ImGui::BeginChild("##audio_fusion_timeline", clip_timeline_size, false, child_flags))
    {
        ImVec2 clip_timeline_window_pos = ImGui::GetCursorScreenPos();
        ImVec2 clip_timeline_window_size = ImGui::GetWindowSize();
        draw_list->AddRectFilled(clip_timeline_window_pos, clip_timeline_window_pos + clip_timeline_window_size, COL_DARK_TWO);
        DrawOverlapTimeLine(timeline->mAudOverlap, timeline->currentTime - (timeline->mAudOverlap ? timeline->mAudOverlap->mStart : 0), clip_header_height, clip_channel_height);
    }
    ImGui::EndChild();

    // draw keypoint hidden button
    ImVec2 hidden_button_pos = window_pos + ImVec2(0, preview_height - 16);
    ImRect hidden_button_rect = ImRect(hidden_button_pos, hidden_button_pos + ImVec2(16, 16));
    ImGui::SetWindowFontScale(0.75);
    if (hidden_button_rect.Contains(ImGui::GetMousePos()))
    {
        draw_list->AddRectFilled(hidden_button_rect.Min, hidden_button_rect.Max, IM_COL32(64,64,64,255));
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            g_media_editor_settings.AudioFusionCurveExpanded = !g_media_editor_settings.AudioFusionCurveExpanded;
        }
        if (ImGui::BeginTooltip())
        {
            ImGui::TextUnformatted(g_media_editor_settings.TextCurveExpanded ? "Hide Curve View" : "Show Curve View");
            ImGui::EndTooltip();
        }
    }
    draw_list->AddText(hidden_button_pos, IM_COL32_WHITE, ICON_FA_BEZIER_CURVE);
    ImGui::SetWindowFontScale(1.0);

    // draw fusion curve editor
    if (g_media_editor_settings.AudioFusionCurveExpanded)
    {
        ImGui::SetCursorScreenPos(clip_keypoint_pos);
        if (ImGui::BeginChild("##audio_fusion_keypoint", clip_keypoint_size, false, child_flags))
        {
            ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
            ImVec2 sub_window_size = ImGui::GetWindowSize();
            draw_list->AddRectFilled(sub_window_pos, sub_window_pos + sub_window_size, COL_DARK_ONE);
            if (timeline->mAudOverlap && fusion)
            {
                bool _changed = false;
                float current_time = timeline->currentTime - timeline->mAudOverlap->mStart;
                mouse_hold |= ImGui::ImCurveEdit::Edit( nullptr,
                                                        &fusion->mKeyPoints,
                                                        sub_window_size, 
                                                        ImGui::GetID("##audio_fusion_keypoint_editor"), 
                                                        true,
                                                        current_time,
                                                        CURVE_EDIT_FLAG_VALUE_LIMITED | CURVE_EDIT_FLAG_MOVE_CURVE | CURVE_EDIT_FLAG_KEEP_BEGIN_END | CURVE_EDIT_FLAG_DOCK_BEGIN_END, 
                                                        nullptr, // clippingRect
                                                        &_changed
                                                        );
                if (_changed) timeline->UpdatePreview();
            }
            // draw cursor line after curve draw
            if (timeline && timeline->mAudOverlap)
            {
                static const float cursorWidth = 2.f;
                float cursorOffset = sub_window_pos.x + (timeline->currentTime - timeline->mAudOverlap->mStart) * timeline->mAudOverlap->msPixelWidth - 0.5f;
                draw_list->AddLine(ImVec2(cursorOffset, sub_window_pos.y), ImVec2(cursorOffset, sub_window_pos.y + sub_window_size.y), COL_CURSOR_LINE_R, cursorWidth);
            }
        }
        ImGui::EndChild();
    }

    ImGui::SetCursorScreenPos(window_pos + ImVec2(audio_view_width, 0));
    if (ImGui::BeginChild("##audio_fusion_blueprint", ImVec2(audio_editor_width, audio_blueprint_height), false, child_flags))
    {
        ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
        ImVec2 sub_window_size = ImGui::GetWindowSize();
        draw_list->AddRectFilled(sub_window_pos, sub_window_pos + sub_window_size, COL_DARK_ONE);
        ShowAudioFusionBluePrintWindow(draw_list, editing_overlap);
    }
    ImGui::EndChild();

    // Draw Audio fusion setting
    ImGui::SetCursorScreenPos(clip_setting_pos);
    if (ImGui::BeginChild("##audio_fusion_setting", clip_setting_size, false, setting_child_flags))
    {
        auto addCurve = [&](std::string name, float _min, float _max, float _default)
        {
            if (fusion)
            {
                auto found = fusion->mKeyPoints.GetCurveIndex(name);
                if (found == -1)
                {
                    ImU32 color; ImGui::RandomColor(color, 1.f);
                    auto curve_index = fusion->mKeyPoints.AddCurve(name, ImGui::ImCurveEdit::Linear, color, true, _min, _max, _default);
                    fusion->mKeyPoints.AddPoint(curve_index, ImVec2(0.f, _min), ImGui::ImCurveEdit::Linear);
                    fusion->mKeyPoints.AddPoint(curve_index, ImVec2(timeline->mAudOverlap->mEnd - timeline->mAudOverlap->mStart, _max), ImGui::ImCurveEdit::Linear);
                    if (blueprint)
                    {
                        auto entry_node = blueprint->FindEntryPointNode();
                        if (entry_node) entry_node->InsertOutputPin(BluePrint::PinType::Float, name);
                        timeline->UpdatePreview();
                    }
                }
            }
        };
        ImVec2 sub_window_pos = ImGui::GetWindowPos(); // we need draw background with scroll view
        ImVec2 sub_window_size = ImGui::GetWindowSize();
        draw_list->AddRectFilled(sub_window_pos, sub_window_pos + sub_window_size, COL_BLACK_DARK);
        if (timeline->mAudOverlap && fusion)
        {
            // Fusion curve setting
            if (ImGui::TreeNodeEx("Curve Setting##audio_fusion", ImGuiTreeNodeFlags_DefaultOpen))
            {
                char ** curve_type_list = nullptr;
                auto curve_type_count = ImGui::ImCurveEdit::GetCurveTypeName(curve_type_list);
                static std::string curve_name = "";
                std::string value = curve_name;
                bool name_input_empty = curve_name.empty();
                if (ImGui::InputTextWithHint("##new_curve_name_audio_fusion", "Input curve name", (char*)value.data(), value.size() + 1, ImGuiInputTextFlags_CallbackEdit | ImGuiInputTextFlags_CallbackResize, [](ImGuiInputTextCallbackData* data) -> int
                {
                    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
                    {
                        auto& stringValue = *static_cast<string*>(data->UserData);
                        ImVector<char>* my_str = (ImVector<char>*)data->UserData;
                        //IM_ASSERT(stringValue.data() == data->Buf);
                        stringValue.resize(data->BufSize);
                        data->Buf = (char*)stringValue.data();
                    }
                    else if (data->EventFlag == ImGuiInputTextFlags_CallbackEdit)
                    {
                        auto& stringValue = *static_cast<string*>(data->UserData);
                        stringValue = std::string(data->Buf);
                    }
                    return 0;
                }, &value))
                {
                    value.resize(strlen(value.c_str()));
                    curve_name = value;
                    name_input_empty = curve_name.empty();
                }

                ImGui::BeginDisabled(name_input_empty);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
                ImGui::SameLine();
                if (ImGui::Button(ICON_ADD "##insert_curve_audio_fusion"))
                {
                    addCurve(curve_name, 0.f, 1.f, 1.0);
                }
                ImGui::ShowTooltipOnHover("Add custom curve");
                ImGui::PopStyleVar();
                ImGui::EndDisabled();

                // list curves
                for (int i = 0; i < fusion->mKeyPoints.GetCurveCount(); i++)
                {
                    bool break_loop = false;
                    ImGui::PushID(i);
                    auto pCount = fusion->mKeyPoints.GetCurvePointCount(i);
                    std::string lable_id = std::string(ICON_CURVE) + " " + fusion->mKeyPoints.GetCurveName(i) + " (" + std::to_string(pCount) + " keys)" + "##audio_fusion_curve";
                    if (ImGui::TreeNodeEx(lable_id.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
                        float value = fusion->mKeyPoints.GetValue(i, timeline->currentTime - timeline->mAudOverlap->mStart);
                        ImGui::BracketSquare(true); ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0, 1.0, 0.0, 1.0)); ImGui::Text("%.2f", value); ImGui::PopStyleColor();
                        ImGui::PushItemWidth(60);
                        float curve_min = fusion->mKeyPoints.GetCurveMin(i);
                        float curve_max = fusion->mKeyPoints.GetCurveMax(i);
                        if (ImGui::DragFloat("##curve_audio_fusion_min", &curve_min, 0.1f, -FLT_MAX, curve_max, "%.1f"))
                        {
                            fusion->mKeyPoints.SetCurveMin(i, curve_min);
                            timeline->UpdatePreview();
                        } ImGui::ShowTooltipOnHover("Min");
                        ImGui::SameLine(0, 8);
                        if (ImGui::DragFloat("##curve_audio_fusion_max", &curve_max, 0.1f, curve_min, FLT_MAX, "%.1f"))
                        {
                            fusion->mKeyPoints.SetCurveMax(i, curve_max);
                            timeline->UpdatePreview();
                        } ImGui::ShowTooltipOnHover("Max");
                        ImGui::SameLine(0, 8);
                        float curve_default = fusion->mKeyPoints.GetCurveDefault(i);
                        if (ImGui::DragFloat("##curve_audio_fusion_default", &curve_default, 0.1f, curve_min, curve_max, "%.1f"))
                        {
                            fusion->mKeyPoints.SetCurveDefault(i, curve_default);
                            timeline->UpdatePreview();
                        } ImGui::ShowTooltipOnHover("Default");
                        ImGui::PopItemWidth();
                        
                        ImGui::SameLine(0, 8);
                        ImGui::SetWindowFontScale(0.75);
                        auto curve_color = ImGui::ColorConvertU32ToFloat4(fusion->mKeyPoints.GetCurveColor(i));
                        if (ImGui::ColorEdit4("##curve_audio_fusion_color", (float*)&curve_color, ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar))
                        {
                            fusion->mKeyPoints.SetCurveColor(i, ImGui::ColorConvertFloat4ToU32(curve_color));
                        } ImGui::ShowTooltipOnHover("Curve Color");
                        ImGui::SetWindowFontScale(1.0);
                        ImGui::SameLine(0, 4);
                        bool is_visiable = fusion->mKeyPoints.IsVisible(i);
                        if (ImGui::Button(is_visiable ? ICON_WATCH : ICON_UNWATCH "##curve_audio_fusion_visiable"))
                        {
                            is_visiable = !is_visiable;
                            fusion->mKeyPoints.SetCurveVisible(i, is_visiable);
                        } ImGui::ShowTooltipOnHover( is_visiable ? "Hide" : "Show");
                        ImGui::SameLine(0, 4);
                        if (ImGui::Button(ICON_DELETE "##curve_audio_fusion_delete"))
                        {
                            // delete blueprint entry node pin
                            auto pin_name = fusion->mKeyPoints.GetCurveName(i);
                            if (blueprint)
                            {
                                auto entry_node = blueprint->FindEntryPointNode();
                                if (entry_node) entry_node->DeleteOutputPin(pin_name);
                                timeline->UpdatePreview();
                            }
                            fusion->mKeyPoints.DeleteCurve(i);
                            break_loop = true;
                        } ImGui::ShowTooltipOnHover("Delete");
                        ImGui::SameLine(0, 4);
                        if (ImGui::Button(ICON_RETURN_DEFAULT "##curve_audio_fusion_reset"))
                        {
                            for (int p = 0; p < pCount; p++)
                            {
                                fusion->mKeyPoints.SetCurvePointDefault(i, p);
                            }
                            timeline->UpdatePreview();
                        } ImGui::ShowTooltipOnHover("Reset");

                        if (!break_loop)
                        {
                            // list points
                            for (int p = 0; p < pCount; p++)
                            {
                                bool is_disabled = false;
                                ImGui::PushID(p);
                                ImGui::PushItemWidth(96);
                                auto point = fusion->mKeyPoints.GetPoint(i, p);
                                ImGui::Diamond(false);
                                if (p == 0 || p == pCount - 1)
                                    is_disabled = true;
                                ImGui::BeginDisabled(is_disabled);
                                if (ImGui::DragTimeMS("##curve_audio_fusion_point_x", &point.point.x, fusion->mKeyPoints.GetMax().x / 1000.f, fusion->mKeyPoints.GetMin().x, fusion->mKeyPoints.GetMax().x, 2))
                                {
                                    fusion->mKeyPoints.EditPoint(i, p, point.point, point.type);
                                    timeline->UpdatePreview();
                                }
                                ImGui::EndDisabled();
                                ImGui::SameLine();
                                auto speed = fabs(fusion->mKeyPoints.GetCurveMax(i) - fusion->mKeyPoints.GetCurveMin(i)) / 500;
                                if (ImGui::DragFloat("##curve_audio_fusion_point_y", &point.point.y, speed, fusion->mKeyPoints.GetCurveMin(i), fusion->mKeyPoints.GetCurveMax(i), "%.2f"))
                                {
                                    fusion->mKeyPoints.EditPoint(i, p, point.point, point.type);
                                    timeline->UpdatePreview();
                                }
                                ImGui::SameLine();
                                if (ImGui::Combo("##curve_audio_fusion_type", (int*)&point.type, curve_type_list, curve_type_count))
                                {
                                    fusion->mKeyPoints.EditPoint(i, p, point.point, point.type);
                                    timeline->UpdatePreview();
                                }
                                ImGui::PopItemWidth();
                                ImGui::PopID();
                            }
                        }
                        ImGui::PopStyleColor();
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                    if (break_loop) break;
                }

                ImGui::TreePop();
            }
            // Fusion Node setting
            if (blueprint && blueprint->Blueprint_IsValid())
            {
                if (ImGui::TreeNodeEx("Node Configure##audio_fusion", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto nodes = blueprint->m_Document->m_Blueprint.GetNodes();
                    for (auto node : nodes)
                    {
                        auto type = node->GetTypeInfo().m_Type;
                        if (type == BluePrint::NodeType::EntryPoint || type == BluePrint::NodeType::ExitPoint)
                            continue;
                        if (!node->CustomLayout())
                            continue;
                        auto label_name = node->m_Name;
                        std::string lable_id = label_name + "##audio_fusion_node" + "@" + std::to_string(node->m_ID);
                        node->DrawNodeLogo(ImGui::GetCurrentContext(), ImVec2(28, 28));
                        ImGui::SameLine(40);
                        if (ImGui::TreeNodeEx(lable_id.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            ImGui::ImCurveEdit::keys key;
                            key.m_id = node->m_ID;
                            if (node->DrawCustomLayout(ImGui::GetCurrentContext(), 1.0, ImVec2(0, 0), &key))
                            {
                                node->m_NeedUpdate = true;
                                timeline->UpdatePreview();
                            }
                            if (!key.name.empty())
                            {
                                addCurve(key.name, key.m_min, key.m_max, key.m_default);
                            }
                            ImGui::TreePop();
                        }
                    }
                    ImGui::TreePop();
                }
            }
        }
    }
    ImGui::EndChild();
}

static void ShowAudioMixingWindow(ImDrawList *draw_list)
{
    /*
    ┏━━━━━━━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━┓
    ┃                       ┃                  ┃                       ┃
    ┃                       ┃                  ┃                       ┃
    ┃        mixing         ┃    meter         ┃       preview         ┃
    ┃                       ┃                  ┃                       ┃
    ┃                       ┃                  ┃                       ┃
    ┣━━━━━━━━━━━━━━┳━━━━━━━━┻━━━━┳━━━━━━━━━━┳━━┻━━━━━━━┳━━━━━━━━━━━━━━━┫
    ┃              ┃             ┃          ┃          ┃               ┃
    ┃              ┃             ┃          ┃          ┃               ┃
    ┃    pan       ┃  equalizer  ┃   gate   ┃ limiter  ┃   compressor  ┃
    ┃              ┃             ┃          ┃          ┃               ┃
    ┃              ┃             ┃          ┃          ┃               ┃
    ┗━━━━━━━━━━━━━━┻━━━━━━━━━━━━━┻━━━━━━━━━━┻━━━━━━━━━━┻━━━━━━━━━━━━━━━┛
    */
    ImVec2 window_pos = ImGui::GetCursorScreenPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    draw_list->AddRectFilled(window_pos, window_pos + window_size, COL_DEEP_DARK);
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(1.f, 1.f, 1.f, 0.5f));

    ImGui::ColorSet circle_color = {{0.5f, 0.5f, 0.5f, 1.f}, {0.6f, 0.6f, 0.6f, 1.f}, {0.8f, 0.8f, 0.8f, 1.f}};
    ImGui::ColorSet wiper_color = {{1.f, 1.f, 1.f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {1.f, 1.f, 1.f, 1.f}};
    ImGui::ColorSet track_color = {{0.2f, 0.2f, 0.2f, 1.f}, {0.3f, 0.3f, 0.3f, 1.f}, {0.4f, 0.4f, 0.4f, 1.f}};
    ImGui::ColorSet tick_color = {{1.f, 1.f, 1.f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {1.f, 1.f, 1.f, 1.f}};

    ImVec2 top_view_pos = window_pos;
    ImVec2 top_view_size = ImVec2(window_size.x, window_size.y * 2 / 5);
    ImVec2 bottom_view_pos = top_view_pos + ImVec2(0, top_view_size.y);
    ImVec2 bottom_view_size = ImVec2(window_size.x, window_size.y * 3 / 5);
    ImVec2 mixing_pos = top_view_pos;
    ImVec2 mixing_size = ImVec2(top_view_size.x / 3,  top_view_size.y);
    ImVec2 meter_pos = top_view_pos + ImVec2(mixing_size.x, 0);
    ImVec2 meter_size = ImVec2(top_view_size.x / 3,  top_view_size.y);
    ImVec2 preview_pos = top_view_pos + ImVec2(mixing_size.x + meter_size.x, 0);
    ImVec2 preview_size = ImVec2(top_view_size.x / 3,  top_view_size.y);
    ImVec2 pan_pos = bottom_view_pos;
    ImVec2 pan_size = ImVec2(bottom_view_size.x / 5, bottom_view_size.y);
    ImVec2 equalizer_pos = pan_pos + ImVec2(pan_size.x, 0);
    ImVec2 equalizer_size = ImVec2(240 + 96, bottom_view_size.y);
    ImVec2 gate_pos = equalizer_pos + ImVec2(equalizer_size.x, 0);
    ImVec2 gate_size = ImVec2(bottom_view_size.x / 5, bottom_view_size.y);
    ImVec2 limiter_pos = gate_pos + ImVec2(gate_size.x, 0);
    ImVec2 limiter_size = ImVec2(bottom_view_size.x / 8, bottom_view_size.y);
    ImVec2 compressor_pos = limiter_pos + ImVec2(limiter_size.x, 0);
    ImVec2 compressor_size = bottom_view_size - ImVec2(equalizer_size.x + pan_size.x + gate_size.x + limiter_size.x, 0);

    ImGuiWindowFlags setting_child_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
    auto amFilter = timeline->mMtaReader->GetAudioEffectFilter();
    // draw mixing UI
    ImGui::SetCursorScreenPos(mixing_pos);
    if (ImGui::BeginChild("##audio_mixing", mixing_size, false, setting_child_flags))
    {
        char value_str[64] = {0};
        ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
        ImVec2 sub_window_size = ImGui::GetWindowSize();
        draw_list->AddRectFilled(sub_window_pos, sub_window_pos + sub_window_size, COL_DEEP_DARK);
        ImGui::TextUnformatted("Audio Mixer");
        ImGui::Separator();
        auto current_pos = ImGui::GetCursorScreenPos();
        auto header_height = current_pos.y - sub_window_pos.y;
        auto slider_size = ImVec2(24, sub_window_size.y - header_height - 48);
        // first is draw output gain setting 
        auto str_size = ImGui::CalcTextSize("Master");
        auto str_offset = str_size.x < 48 ? (48 - str_size.x) / 2 : 0;
        ImGui::SetCursorScreenPos(current_pos + ImVec2(sub_window_size.x - 60 + str_offset, 0));
        ImGui::TextColored({ 0.9, 0.3, 0.3, 1.0 }, "Master");
        ImGui::SetCursorScreenPos(current_pos + ImVec2(sub_window_size.x - 48, 16));
        auto volMaster = amFilter->GetVolumeParams();
        timeline->mAudioAttribute.mAudioGain = volMaster.volume;
        float vol = (timeline->mAudioAttribute.mAudioGain - 1.f) * 96.f;
        if (ImGui::VSliderFloat("##master_gain", slider_size, &vol, -96.f, 32.f, "", ImGuiSliderFlags_Mark))
        {
            timeline->mAudioAttribute.mAudioGain = (vol / 96.f) + 1.f;
            volMaster.volume = timeline->mAudioAttribute.mAudioGain;
            amFilter->SetVolumeParams(&volMaster);
        }
        snprintf(value_str, 64, "%.1fdB", vol);
        auto vol_str_size = ImGui::CalcTextSize(value_str);
        auto vol_str_offset = vol_str_size.x < 64 ? (64 - vol_str_size.x) / 2 : 0;
        ImGui::SetCursorScreenPos(current_pos + ImVec2(sub_window_size.x - 64 + vol_str_offset, sub_window_size.y - header_height - 32));
        ImGui::TextColored({ 0.9, 0.7, 0.7, 1.0 }, "%s", value_str);
        // draw track gain
        int count = 0;
        for (auto track : timeline->m_Tracks)
        {
            if (IS_AUDIO(track->mType))
            {
                ImGui::BeginGroup();
                ImGui::PushID(count);
                auto name_str_size = ImGui::CalcTextSize(track->mName.c_str());
                auto name_str_offset = name_str_size.x < 48 ? (48 - name_str_size.x) / 2 : 0;
                ImGui::SetCursorScreenPos(current_pos + ImVec2(count * 48 + name_str_offset, 0));
                ImGui::TextColored({ 0.7, 0.9, 0.7, 1.0 }, "%s", track->mName.c_str());
                ImGui::SetCursorScreenPos(current_pos + ImVec2(count * 48 + 12, 16));
                auto atHolder = timeline->mMtaReader->GetTrackById(track->mID);
                if (atHolder)
                {
                    auto aeFilter = atHolder->GetAudioEffectFilter();
                    auto volParams = aeFilter->GetVolumeParams();
                    track->mAudioTrackAttribute.mAudioGain = volParams.volume;
                    float volTrack = (track->mAudioTrackAttribute.mAudioGain - 1.f) * 96.f;
                    if (ImGui::VSliderFloat("##track_gain", slider_size, &volTrack, -96.f, 32.f, "", ImGuiSliderFlags_Mark))
                    {
                        track->mAudioTrackAttribute.mAudioGain = (volTrack / 96.f) + 1.f;
                        volParams.volume = track->mAudioTrackAttribute.mAudioGain;
                        aeFilter->SetVolumeParams(&volParams);
                    }
                }
                ImGui::PopID();
                snprintf(value_str, 64, "%.1fdB", (track->mAudioTrackAttribute.mAudioGain - 1.f) * 96.f);
                auto value_str_size = ImGui::CalcTextSize(value_str);
                auto value_str_offset = value_str_size.x < 48 ? (48 - value_str_size.x) / 2 : 0;
                ImGui::SetCursorScreenPos(current_pos + ImVec2(count * 48 + value_str_offset, sub_window_size.y - header_height - 32));
                ImGui::TextColored({ 0.5, 0.9, 0.5, 1.0 }, "%s", value_str);
                count++;
                ImGui::EndGroup();
            }
        }
    }
    ImGui::EndChild();

    // draw meter UI
    ImGui::SetCursorScreenPos(meter_pos);
    if (ImGui::BeginChild("##audio_meter", meter_size, false, setting_child_flags))
    {
        ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
        ImVec2 sub_window_size = ImGui::GetWindowSize();
        draw_list->AddRectFilled(sub_window_pos, sub_window_pos + sub_window_size, COL_BLACK_DARK);
        ImGui::TextUnformatted("Audio Meters");
        ImGui::Separator();
        auto current_pos = ImGui::GetCursorScreenPos();
        auto header_height = current_pos.y - sub_window_pos.y;
        auto meter_size = ImVec2(24, sub_window_size.y - header_height - 32);
        // first is draw output meter
        auto str_size = ImGui::CalcTextSize("Master");
        auto str_offset = str_size.x < 48 ? (48 - str_size.x) / 2 : 0;
        ImGui::SetCursorScreenPos(current_pos + ImVec2(sub_window_size.x - 76 + str_offset, 0));
        ImGui::TextColored({ 0.9, 0.3, 0.3, 1.0 }, "Master");
        timeline->mAudioAttribute.audio_mutex.lock();
        int l_level = timeline->GetAudioLevel(0);
        int r_level = timeline->GetAudioLevel(1);
        timeline->mAudioAttribute.audio_mutex.unlock();
        auto AudioMeterPos = current_pos + ImVec2(sub_window_size.x - 30, 16);
        ImGui::SetCursorScreenPos(current_pos + ImVec2(sub_window_size.x - 60, 16));
        ImGui::UvMeter("##luv", ImVec2(meter_size.x / 2, meter_size.y), &l_level, 0, 96, meter_size.y / 4, &timeline->mAudioAttribute.left_stack, &timeline->mAudioAttribute.left_count, 0.2, audio_bar_seg);
        ImGui::SetCursorScreenPos(current_pos + ImVec2(sub_window_size.x - 44, 16));
        ImGui::UvMeter("##ruv", ImVec2(meter_size.x / 2, meter_size.y), &r_level, 0, 96, meter_size.y / 4, &timeline->mAudioAttribute.right_stack, &timeline->mAudioAttribute.right_count, 0.2, audio_bar_seg);
        // draw main mark
        for (int i = 0; i <= 96; i+= 5)
        {
            float mark_step = meter_size.y / 96.0f;
            ImVec2 MarkPos = AudioMeterPos + ImVec2(0, i * mark_step);
            if (i % 10 == 0)
            {
                std::string mark_str = i == 0 ? "  0" : "-" + std::to_string(i);
                draw_list->AddLine(MarkPos + ImVec2(20, 8), MarkPos + ImVec2(30, 8), COL_MARK_HALF, 1);
                ImGui::SetWindowFontScale(0.75);
                draw_list->AddText(MarkPos + ImVec2(0, 2), COL_MARK_HALF, mark_str.c_str());
                ImGui::SetWindowFontScale(1.0);
            }
            else
            {
                draw_list->AddLine(MarkPos + ImVec2(25, 8), MarkPos + ImVec2(30, 8), COL_MARK_HALF, 1);
            }
        }
        // draw channels meter
        int count = 0;
        for (auto track : timeline->m_Tracks)
        {
            if (IS_AUDIO(track->mType))
            {
                ImGui::BeginGroup();
                ImGui::PushID(count);
                auto name_str_size = ImGui::CalcTextSize(track->mName.c_str());
                auto name_str_offset = name_str_size.x < 48 ? (48 - name_str_size.x) / 2 : 0;
                ImGui::SetCursorScreenPos(current_pos + ImVec2(count * 48 + name_str_offset, 0));
                ImGui::TextColored({ 0.7, 0.9, 0.7, 1.0 }, "%s", track->mName.c_str());
                ImGui::SetCursorScreenPos(current_pos + ImVec2(count * 48 + 12, 16));
                auto channel_meter_pos = current_pos + ImVec2(count * 48 + 12, 16);
                ImGui::SetCursorScreenPos(channel_meter_pos);
                track->mAudioTrackAttribute.audio_mutex.lock();
                int tl_level = track->GetAudioLevel(0);
                int tr_level = track->GetAudioLevel(1);
                track->mAudioTrackAttribute.audio_mutex.unlock();
                ImGui::UvMeter("##tluv", ImVec2(meter_size.x / 2, meter_size.y), &tl_level, 0, 96, meter_size.y / 4, &track->mAudioTrackAttribute.left_stack, &track->mAudioTrackAttribute.left_count, 0.2, audio_bar_seg);
                ImGui::SetCursorScreenPos(channel_meter_pos + ImVec2(14, 0));
                ImGui::UvMeter("##truv", ImVec2(meter_size.x / 2, meter_size.y), &tr_level, 0, 96, meter_size.y / 4, &track->mAudioTrackAttribute.right_stack, &track->mAudioTrackAttribute.right_count, 0.2, audio_bar_seg);
                // draw channel mark
                for (int i = 0; i <= 96; i+= 5)
                {
                    float mark_step = meter_size.y / 96.0f;
                    ImVec2 MarkPos = channel_meter_pos + ImVec2(8, i * mark_step);
                    if (i % 10 == 0)
                    {
                        draw_list->AddLine(MarkPos + ImVec2(20, 8), MarkPos + ImVec2(30, 8), COL_MARK_HALF, 1);
                    }
                    else
                    {
                        draw_list->AddLine(MarkPos + ImVec2(20, 8), MarkPos + ImVec2(25, 8), COL_MARK_HALF, 1);
                    }
                }
                ImGui::PopID();
                count++;
                ImGui::EndGroup();
            }
        }
    }
    ImGui::EndChild();

    // draw preview UI
    ImGui::SetCursorScreenPos(preview_pos);
    if (ImGui::BeginChild("##audio_preview", preview_size, false, setting_child_flags))
    {
        ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
        ImVec2 sub_window_size = ImGui::GetWindowSize();
        draw_list->AddRectFilled(sub_window_pos, sub_window_pos + sub_window_size, COL_DEEP_DARK);
        ImRect video_rect;
        ShowMediaPreviewWindow(draw_list, "Preview", video_rect, timeline->mStart, timeline->mEnd, false, false, false, true, false, false);
    }
    ImGui::EndChild();

    // bottom area
    // draw pan UI
    ImGui::SetCursorScreenPos(pan_pos);
    ImGui::BeginGroup();
    bool pan_changed = false;
    ImGui::TextUnformatted("Audio Pan");
    ImGui::SameLine();
    pan_changed |= ImGui::ToggleButton("##audio_pan_enabe", &timeline->mAudioAttribute.bPan);
    ImGui::Separator();
    if (ImGui::BeginChild("##audio_pan", pan_size - ImVec2(0, 32), false, setting_child_flags))
    {
        ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
        ImVec2 sub_window_size = ImGui::GetWindowSize();
        float scroll_y = ImGui::GetScrollY();
        draw_list->AddRectFilled(sub_window_pos + ImVec2(0, scroll_y), sub_window_pos + ImVec2(0, scroll_y) + sub_window_size, COL_BLACK_DARK);
        ImGui::PushItemWidth(sub_window_size.x - 32);
        float const w = ImGui::CalcItemWidth();
        float pos_offset = sub_window_size.x <= w ? 0 : (sub_window_size.x - w) / 2;
        ImGui::SetCursorScreenPos(sub_window_pos + ImVec2(pos_offset, 32));
        ImGui::BeginDisabled(!timeline->mAudioAttribute.bPan);
        auto audio_pan = timeline->mAudioAttribute.audio_pan - ImVec2(0.5, 0.5);
        audio_pan.y = -audio_pan.y;
        pan_changed |= ImGui::InputVec2("##audio_pan_input", &audio_pan, ImVec2(-0.5f, -0.5f), ImVec2(0.5f, 0.5f), 1.0, false, false);
        ImGui::PopItemWidth();
        ImGui::Separator();
        auto knob_pos = ImGui::GetCursorScreenPos() + ImVec2(0, 40);
        auto knob_offset_x = (sub_window_size.x - 160) / 3;
        ImGui::SetCursorScreenPos(knob_pos + ImVec2(knob_offset_x, 0));
        pan_changed |= ImGui::Knob("Left/Right", &audio_pan.x, -0.5f, 0.5f, NAN, 0.f, 80, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_STEPPED_DOT, "%.2f", 10);
        ImGui::SetCursorScreenPos(knob_pos + ImVec2(knob_offset_x * 2 + 80, 0));
        pan_changed |= ImGui::Knob("Front/Back", &audio_pan.y, -0.5f, 0.5f, NAN, 0.f, 80, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_STEPPED_DOT, "%.2f", 10);
        if (pan_changed)
        {
            audio_pan.y = -audio_pan.y;
            audio_pan += ImVec2(0.5, 0.5);
            timeline->mAudioAttribute.audio_pan = audio_pan;
            auto panParams = amFilter->GetPanParams();
            panParams.x = timeline->mAudioAttribute.bPan ? timeline->mAudioAttribute.audio_pan.x : 0.5f;
            panParams.y = timeline->mAudioAttribute.bPan ? timeline->mAudioAttribute.audio_pan.y : 0.5f;
            amFilter->SetPanParams(&panParams);
        }
        ImGui::EndDisabled();
    }
    ImGui::EndChild();
    ImGui::EndGroup();

    // draw equalizer UI
    ImGui::SetCursorScreenPos(equalizer_pos);
    ImGui::BeginGroup();
    bool equalizer_changed = false;
    ImGui::TextUnformatted("Equalizer");
    ImGui::SameLine();
    equalizer_changed |= ImGui::ToggleButton("##audio_equalizer_enabe", &timeline->mAudioAttribute.bEqualizer);
    ImGui::Separator();
    if (ImGui::BeginChild("##audio_equalizer", equalizer_size - ImVec2(0, 32), false, setting_child_flags))
    {
        ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
        ImVec2 sub_window_size = ImGui::GetWindowSize();
        draw_list->AddRectFilled(sub_window_pos, sub_window_pos + sub_window_size, COL_DEEP_DARK);
        ImGui::SetCursorScreenPos(sub_window_pos);
        ImGui::BeginDisabled(!timeline->mAudioAttribute.bEqualizer);
        for (int i = 0; i < 10; i++)
        {
            if (i > 0) ImGui::SameLine();
            ImGui::BeginGroup();
            std::string cfTag = GetFrequencyTag(timeline->mAudioAttribute.mBandCfg[i].centerFreq);
            ImGui::TextUnformatted(cfTag.c_str());
            ImGui::PushID(i);
            equalizer_changed |= ImGui::VSliderInt("##band_gain", ImVec2(24, sub_window_size.y - 48), &timeline->mAudioAttribute.mBandCfg[i].gain, MIN_GAIN, MAX_GAIN, "", ImGuiSliderFlags_Mark);
            ImGui::PopID();
            ImGui::Text("%d", timeline->mAudioAttribute.mBandCfg[i].gain);
            ImGui::EndGroup();
        }
        ImGui::EndDisabled();
        ImGui::SetCursorScreenPos(sub_window_pos + ImVec2(240 + 80, 0));
        ImGui::TextColored({ 0.9, 0.4, 0.4, 1.0 }, "Hz");
        ImGui::SetCursorScreenPos(sub_window_pos + ImVec2(240 + 80, sub_window_size.y - 24));
        ImGui::TextColored({ 0.4, 0.4, 0.9, 1.0 }, "dB");
    }
    ImGui::EndChild();
    if (equalizer_changed)
    {
        for (int i = 0; i < 10; i++)
        {
            auto equalizerParams = amFilter->GetEqualizerParamsByIndex(i);
            equalizerParams.gain = timeline->mAudioAttribute.bEqualizer ? timeline->mAudioAttribute.mBandCfg[i].gain : 0.0f;
            amFilter->SetEqualizerParamsByIndex(&equalizerParams, i);
        }
    }
    ImGui::EndGroup();

    // draw gate UI
    ImGui::SetCursorScreenPos(gate_pos);
    ImGui::BeginGroup();
    bool gate_changed = false;
    ImGui::TextUnformatted("Gate");
    ImGui::SameLine();
    gate_changed |= ImGui::ToggleButton("##audio_gate_enabe", &timeline->mAudioAttribute.bGate);
    ImGui::Separator();
    if (ImGui::BeginChild("##audio_gate", gate_size - ImVec2(0, 32), false, setting_child_flags))
    {
        ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
        ImVec2 sub_window_size = ImGui::GetWindowSize();
        float scroll_y = ImGui::GetScrollY();
        draw_list->AddRectFilled(sub_window_pos + ImVec2(0, scroll_y), sub_window_pos + ImVec2(0, scroll_y) + sub_window_size, COL_BLACK_DARK);
        ImGui::BeginDisabled(!timeline->mAudioAttribute.bGate);
        auto knob_pos = ImGui::GetCursorScreenPos();
        auto knob_offset_x = (sub_window_size.x - 240) / 4;
        ImGui::SetCursorScreenPos(knob_pos + ImVec2(knob_offset_x, 4));
        gate_changed |= ImGui::Knob("Threshold##gate", &timeline->mAudioAttribute.gate_thd, 0.f, 1.0f, NAN, 0.125f, 80, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_STEPPED_DOT, "%.3f", 10);
        ImGui::SetCursorScreenPos(knob_pos + ImVec2(knob_offset_x * 2 + 80, 4));
        gate_changed |= ImGui::Knob("Range##gate", &timeline->mAudioAttribute.gate_range, 0.f, 1.0f, NAN, 0.06125f, 80, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_STEPPED_DOT, "%.3f", 10);
        ImGui::SetCursorScreenPos(knob_pos + ImVec2(knob_offset_x * 3 + 160, 4));
        gate_changed |= ImGui::Knob("Ratio##gate", &timeline->mAudioAttribute.gate_ratio, 1.f, 9000.0f, NAN, 2.f, 80, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_STEPPED_DOT, "%.0f", 10);
        auto knob_time_offset_x = (sub_window_size.x - 100) / 3;
        ImGui::SetCursorScreenPos(knob_pos + ImVec2(knob_time_offset_x, 140));
        gate_changed |= ImGui::Knob("Attack##gate", &timeline->mAudioAttribute.gate_attack, 0.01f, 9000.0f, NAN, 20.f, 50, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_STEPPED_DOT, "%.0fms", 10);
        ImGui::SetCursorScreenPos(knob_pos + ImVec2(knob_time_offset_x * 2 + 50, 140));
        gate_changed |= ImGui::Knob("Release##gate", &timeline->mAudioAttribute.gate_release, 0.01f, 9000.0f, NAN, 250.f, 50, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_STEPPED_DOT, "%.0fms", 10);
        auto knob_level_offset_x = (sub_window_size.x - 160) / 3;
        ImGui::SetCursorScreenPos(knob_pos + ImVec2(knob_level_offset_x, 240));
        gate_changed |= ImGui::Knob("Make Up##gate", &timeline->mAudioAttribute.gate_makeup, 1.f, 64.0f, NAN, 1.f, 80, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_STEPPED_DOT, "%.0f", 10);
        ImGui::SetCursorScreenPos(knob_pos + ImVec2(knob_level_offset_x * 2 + 80, 240));
        gate_changed |= ImGui::Knob("Knee##gate", &timeline->mAudioAttribute.gate_knee, 1.0f, 8.0f, NAN, 2.82843f, 80, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_STEPPED_DOT, "%.3f", 10);
        if (gate_changed)
        {
            auto gateParams = amFilter->GetGateParams();
            gateParams.threshold = timeline->mAudioAttribute.bGate ? timeline->mAudioAttribute.gate_thd : 0.f;
            gateParams.range = timeline->mAudioAttribute.bGate ? timeline->mAudioAttribute.gate_range : 0.f;
            gateParams.ratio = timeline->mAudioAttribute.bGate ? timeline->mAudioAttribute.gate_ratio : 2.f;
            gateParams.attack = timeline->mAudioAttribute.bGate ? timeline->mAudioAttribute.gate_attack : 20.f;
            gateParams.release = timeline->mAudioAttribute.bGate ? timeline->mAudioAttribute.gate_release : 250.f;
            gateParams.makeup = timeline->mAudioAttribute.bGate ? timeline->mAudioAttribute.gate_makeup : 1.f;
            gateParams.knee = timeline->mAudioAttribute.bGate ? timeline->mAudioAttribute.gate_knee : 2.82843f;
            amFilter->SetGateParams(&gateParams);
        }
        ImGui::EndDisabled();
    }
    ImGui::EndChild();
    ImGui::EndGroup();

    // draw limiter UI
    ImGui::SetCursorScreenPos(limiter_pos);
    ImGui::BeginGroup();
    bool limiter_changed = false;
    ImGui::TextUnformatted("Limiter");
    ImGui::SameLine();
    limiter_changed |= ImGui::ToggleButton("##audio_limiter_enabe", &timeline->mAudioAttribute.bLimiter);
    ImGui::Separator();
    if (ImGui::BeginChild("##audio_limiter", limiter_size - ImVec2(0, 32), false, setting_child_flags))
    {
        ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
        ImVec2 sub_window_size = ImGui::GetWindowSize();
        float scroll_y = ImGui::GetScrollY();
        draw_list->AddRectFilled(sub_window_pos + ImVec2(0, scroll_y), sub_window_pos + ImVec2(0, scroll_y) + sub_window_size, COL_DEEP_DARK);
        ImGui::BeginDisabled(!timeline->mAudioAttribute.bLimiter);
        auto knob_pos = ImGui::GetCursorScreenPos();
        auto knob_offset_x = (sub_window_size.x - 80) / 2;
        ImGui::SetCursorScreenPos(knob_pos + ImVec2(knob_offset_x, 4));
        limiter_changed |= ImGui::Knob("Limit", &timeline->mAudioAttribute.limit, 0.0625f, 1.0f, NAN, 1.f, 80, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_STEPPED_DOT, "%.3f", 10);
        auto knob_time_offset_x = (sub_window_size.x - 50) / 2;
        ImGui::SetCursorScreenPos(knob_pos + ImVec2(knob_time_offset_x, 140));
        limiter_changed |= ImGui::Knob("Attack##limiter", &timeline->mAudioAttribute.limiter_attack, 0.1f, 80.0f, NAN, 5.f, 50, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_STEPPED_DOT, "%.1fms", 10);
        auto knob_level_offset_x = (sub_window_size.x - 50) / 2;
        ImGui::SetCursorScreenPos(knob_pos + ImVec2(knob_level_offset_x, 240));
        limiter_changed |= ImGui::Knob("Release##limiter", &timeline->mAudioAttribute.limiter_release, 1.f, 8000.0f, NAN, 50.f, 50, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_STEPPED_DOT, "%.0fms", 10);
        if (limiter_changed)
        {
            auto limiterParams = amFilter->GetLimiterParams();
            limiterParams.limit = timeline->mAudioAttribute.bLimiter ? timeline->mAudioAttribute.limit : 1.0;
            limiterParams.attack = timeline->mAudioAttribute.bLimiter ? timeline->mAudioAttribute.limiter_attack : 5;
            limiterParams.release = timeline->mAudioAttribute.bLimiter ? timeline->mAudioAttribute.limiter_release : 50;
            amFilter->SetLimiterParams(&limiterParams);
        }
        ImGui::EndDisabled();
    }
    ImGui::EndChild();
    ImGui::EndGroup();

    // draw compressor UI
    ImGui::SetCursorScreenPos(compressor_pos);
    ImGui::BeginGroup();
    bool compressor_changed = false;
    ImGui::TextUnformatted("Compressor");
    ImGui::SameLine();
    compressor_changed |= ImGui::ToggleButton("##audio_compressor_enabe", &timeline->mAudioAttribute.bCompressor);
    ImGui::Separator();
    if (ImGui::BeginChild("##audio_compressor", compressor_size - ImVec2(0, 32), false, setting_child_flags))
    {
        ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
        ImVec2 sub_window_size = ImGui::GetWindowSize();
        float scroll_y = ImGui::GetScrollY();
        draw_list->AddRectFilled(sub_window_pos + ImVec2(0, scroll_y), sub_window_pos + ImVec2(0, scroll_y) + sub_window_size, COL_BLACK_DARK);
        ImGui::BeginDisabled(!timeline->mAudioAttribute.bCompressor);
        auto knob_pos = ImGui::GetCursorScreenPos();
        auto knob_offset_x = (sub_window_size.x - 320) / 5;
        ImGui::SetCursorScreenPos(knob_pos + ImVec2(knob_offset_x, 4));
        compressor_changed |= ImGui::Knob("Threshold##compressor", &timeline->mAudioAttribute.compressor_thd, 0.001f, 1.0f, NAN, 0.125f, 80, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_STEPPED_DOT, "%.3f", 10);
        ImGui::SetCursorScreenPos(knob_pos + ImVec2(knob_offset_x * 2 + 80, 4));
        compressor_changed |= ImGui::Knob("Ratio##compressor", &timeline->mAudioAttribute.compressor_ratio, 1.f, 20.0f, NAN, 2.f, 80, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_STEPPED_DOT, "%.0f", 10);
        ImGui::SetCursorScreenPos(knob_pos + ImVec2(knob_offset_x * 3 + 160, 4));
        compressor_changed |= ImGui::Knob("Knee##compressor", &timeline->mAudioAttribute.compressor_knee, 1.f, 8.0f, NAN, 2.82843f, 80, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_STEPPED_DOT, "%.3f", 10);
        ImGui::SetCursorScreenPos(knob_pos + ImVec2(knob_offset_x * 4 + 240, 4));
        compressor_changed |= ImGui::Knob("Mix##compressor", &timeline->mAudioAttribute.compressor_mix, 0.f, 1.0f, NAN, 1.0f, 80, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_STEPPED_DOT, "%.3f", 10);
        auto knob_time_offset_x = (sub_window_size.x - 100) / 3;
        ImGui::SetCursorScreenPos(knob_pos + ImVec2(knob_time_offset_x, 140));
        compressor_changed |= ImGui::Knob("Attack##compressor", &timeline->mAudioAttribute.compressor_attack, 0.01f, 2000.0f, NAN, 20.f, 50, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_STEPPED_DOT, "%.0fms", 10);
        ImGui::SetCursorScreenPos(knob_pos + ImVec2(knob_time_offset_x * 2 + 50, 140));
        compressor_changed |= ImGui::Knob("Release##compressor", &timeline->mAudioAttribute.compressor_release, 0.01f, 9000.0f, NAN, 250.f, 50, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_STEPPED_DOT, "%.0fms", 10);
        auto knob_level_offset_x = (sub_window_size.x - 160) / 3;
        ImGui::SetCursorScreenPos(knob_pos + ImVec2(knob_level_offset_x, 240));
        compressor_changed |= ImGui::Knob("Make Up##compressor", &timeline->mAudioAttribute.compressor_makeup, 1.f, 64.0f, NAN, 1.f, 80, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_STEPPED_DOT, "%.0f", 10);
        ImGui::SetCursorScreenPos(knob_pos + ImVec2(knob_level_offset_x * 2 + 80, 240));
        compressor_changed |= ImGui::Knob("Level SC##compressor", &timeline->mAudioAttribute.compressor_level_sc, 0.015f, 64.0f, NAN, 1.f, 80, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_STEPPED_DOT, "%.3f", 10);
        if (compressor_changed)
        {
            auto compressorParams = amFilter->GetCompressorParams();
            compressorParams.threshold = timeline->mAudioAttribute.bCompressor ? timeline->mAudioAttribute.compressor_thd : 1.f;
            compressorParams.ratio = timeline->mAudioAttribute.bCompressor ? timeline->mAudioAttribute.compressor_ratio : 2.f;
            compressorParams.knee = timeline->mAudioAttribute.bCompressor ? timeline->mAudioAttribute.compressor_knee : 2.82843f;
            compressorParams.mix = timeline->mAudioAttribute.bCompressor ? timeline->mAudioAttribute.compressor_mix : 1.f;
            compressorParams.attack = timeline->mAudioAttribute.bCompressor ? timeline->mAudioAttribute.compressor_attack : 20.f;
            compressorParams.release = timeline->mAudioAttribute.bCompressor ? timeline->mAudioAttribute.compressor_release : 250.f;
            compressorParams.makeup = timeline->mAudioAttribute.bCompressor ? timeline->mAudioAttribute.compressor_makeup : 1.f;
            compressorParams.levelIn = timeline->mAudioAttribute.bCompressor ? timeline->mAudioAttribute.compressor_level_sc : 1.f;
            amFilter->SetCompressorParams(&compressorParams);
        }
        ImGui::EndDisabled();
    }
    ImGui::EndChild();
    ImGui::EndGroup();

    ImGui::PopStyleColor();
}

static void ShowAudioEditorWindow(ImDrawList *draw_list)
{
    float labelWidth = ImGui::CalcVerticalTabLabelsWidth() + 4;
    ImVec2 clip_window_pos = ImGui::GetCursorScreenPos();
    ImVec2 clip_window_size = ImGui::GetWindowSize();
    static const int numTabs = sizeof(AudioEditorTabNames)/sizeof(AudioEditorTabNames[0]);
    if (ImGui::TabLabelsVertical(false, numTabs, AudioEditorTabNames, AudioEditorWindowIndex, AudioEditorTabTooltips, true, nullptr, nullptr, false, false, nullptr, nullptr))
    {
        UIPageChanged();
    }
    ImGui::SetCursorScreenPos(clip_window_pos + ImVec2(labelWidth, 0));
    if (ImGui::BeginChild("##audio_editor_views", ImVec2(clip_window_size.x - labelWidth, clip_window_size.y), false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings))
    {
        ImVec2 editor_view_window_pos = ImGui::GetCursorScreenPos();
        ImVec2 editor_view_window_size = ImGui::GetWindowSize();
        draw_list->AddRectFilled(editor_view_window_pos, editor_view_window_pos + editor_view_window_size, COL_DARK_ONE);
        switch (AudioEditorWindowIndex)
        {
            case 0: ShowAudioFilterWindow(draw_list); break;
            case 1: ShowAudioFusionWindow(draw_list); break;
            case 2: ShowAudioMixingWindow(draw_list); break;
            default: break;
        }
    }
    ImGui::EndChild();
}

static bool edit_text_clip_style(ImDrawList *draw_list, TextClip * clip, ImVec2 size, ImVec2 default_size)
{
    if (!clip || !clip->mClipHolder)
        return false;;
    MediaTrack * track = (MediaTrack * )clip->mTrack;
    if (!track || !track->mMttReader)
        return false;
    bool update_preview = false;
    // Add Curve
    auto key_point = &clip->mAttributeKeyPoints;
    char ** curve_type_list = nullptr;
    auto curve_type_count = ImGui::ImCurveEdit::GetCurveTypeName(curve_type_list);
    auto addCurve = [&](std::string name, float _min, float _max, float _default)
    {
        auto found = key_point->GetCurveIndex(name);
        if (found == -1)
        {
            ImU32 color; ImGui::RandomColor(color, 1.f);
            auto curve_index = key_point->AddCurve(name, ImGui::ImCurveEdit::Smooth, color, true, _min, _max, _default);
            key_point->AddPoint(curve_index, ImVec2(key_point->GetMin().x, _min), ImGui::ImCurveEdit::Smooth);
            key_point->AddPoint(curve_index, ImVec2(key_point->GetMax().x, _max), ImGui::ImCurveEdit::Smooth);
            key_point->SetCurvePointDefault(curve_index, 0);
            key_point->SetCurvePointDefault(curve_index, 1);
        }
    };
    
    // Editor Curve
    auto EditCurve = [&](std::string name) 
    {
        int index = key_point->GetCurveIndex(name);
        if (index != -1)
        {
            ImGui::Separator();
            bool break_loop = false;
            ImGui::PushID(ImGui::GetID(name.c_str()));
            auto pCount = key_point->GetCurvePointCount(index);
            std::string lable_id = std::string(ICON_CURVE) + " " + name + " (" + std::to_string(pCount) + " keys)" + "##text_clip_curve";
            if (ImGui::TreeNodeEx(lable_id.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
                float value = key_point->GetValue(index, timeline->currentTime - clip->mStart);
                ImGui::BracketSquare(true); ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0, 1.0, 0.0, 1.0)); ImGui::Text("%.2f", value); ImGui::PopStyleColor();
                
                ImGui::PushItemWidth(60);
                float curve_min = key_point->GetCurveMin(index);
                float curve_max = key_point->GetCurveMax(index);
                ImGui::BeginDisabled(true);
                ImGui::DragFloat("##curve_text_clip_min", &curve_min, 0.1f, -FLT_MAX, curve_max, "%.1f"); ImGui::ShowTooltipOnHover("Min");
                ImGui::SameLine(0, 8);
                ImGui::DragFloat("##curve_text_clip_max", &curve_max, 0.1f, curve_min, FLT_MAX, "%.1f"); ImGui::ShowTooltipOnHover("Max");
                ImGui::SameLine(0, 8);
                ImGui::EndDisabled();
                float curve_default = key_point->GetCurveDefault(index);
                if (ImGui::DragFloat("##curve_text_clip_default", &curve_default, 0.1f, curve_min, curve_max, "%.1f"))
                {
                    key_point->SetCurveDefault(index, curve_default);
                    update_preview = true;
                } ImGui::ShowTooltipOnHover("Default");
                ImGui::PopItemWidth();

                ImGui::SameLine(0, 8);
                ImGui::SetWindowFontScale(0.75);
                auto curve_color = ImGui::ColorConvertU32ToFloat4(key_point->GetCurveColor(index));
                if (ImGui::ColorEdit4("##curve_text_clip_color", (float*)&curve_color, ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar))
                {
                    key_point->SetCurveColor(index, ImGui::ColorConvertFloat4ToU32(curve_color));
                } ImGui::ShowTooltipOnHover("Curve Color");
                ImGui::SetWindowFontScale(1.0);
                ImGui::SameLine(0, 4);
                bool is_visiable = key_point->IsVisible(index);
                if (ImGui::Button(is_visiable ? ICON_WATCH : ICON_UNWATCH "##curve_text_clip_visiable"))
                {
                    is_visiable = !is_visiable;
                    key_point->SetCurveVisible(index, is_visiable);
                } ImGui::ShowTooltipOnHover(is_visiable ? "Hide" : "Show");
                ImGui::SameLine(0, 4);
                if (ImGui::Button(ICON_DELETE "##curve_text_clip_delete"))
                {
                    key_point->DeleteCurve(index);
                    update_preview = true;
                    break_loop = true;
                } ImGui::ShowTooltipOnHover("Delete");
                ImGui::SameLine(0, 4);
                if (ImGui::Button(ICON_RETURN_DEFAULT "##curve_text_clip_reset"))
                {
                    for (int p = 0; p < pCount; p++)
                    {
                        key_point->SetCurvePointDefault(index, p);
                    }
                    update_preview = true;
                } ImGui::ShowTooltipOnHover("Reset");

                if (!break_loop)
                {
                    // list points
                    for (int p = 0; p < pCount; p++)
                    {
                        bool is_disabled = false;
                        ImGui::PushID(p);
                        ImGui::PushItemWidth(96);
                        auto point = key_point->GetPoint(index, p);
                        ImGui::Diamond(false);
                        if (p == 0 || p == pCount - 1)
                            is_disabled = true;
                        ImGui::BeginDisabled(is_disabled);
                        if (ImGui::DragTimeMS("##curve_text_clip_point_x", &point.point.x, key_point->GetMax().x / 1000.f, key_point->GetMin().x, key_point->GetMax().x, 2))
                        {
                            key_point->EditPoint(index, p, point.point, point.type);
                            update_preview = true;
                        }
                        ImGui::EndDisabled();
                        ImGui::SameLine();
                        auto speed = fabs(key_point->GetCurveMax(index) - key_point->GetCurveMin(index)) / 500;
                        if (ImGui::DragFloat("##curve_text_clip_point_y", &point.point.y, speed, key_point->GetCurveMin(index), key_point->GetCurveMax(index), "%.2f"))
                        {
                            key_point->EditPoint(index, p, point.point, point.type);
                            update_preview = true;
                        }
                        ImGui::SameLine();
                        if (ImGui::Combo("##curve_text_clip_type", (int*)&point.type, curve_type_list, curve_type_count))
                        {
                            key_point->EditPoint(index, p, point.point, point.type);
                            update_preview = true;
                        }
                        ImGui::PopItemWidth();
                        ImGui::PopID();
                    }
                }

                ImGui::PopStyleColor();
                ImGui::TreePop();
            }
            ImGui::PopID();
            ImGui::Separator();
        }
    };
    ImGuiIO &io = ImGui::GetIO();
    auto& style = track->mMttReader->DefaultStyle();
    ImGui::PushItemWidth(240);
    const float reset_button_offset = size.x - 64;
    const float curve_button_offset = size.x - 36;
    auto item_width = ImGui::CalcItemWidth();
    const char* familyValue = clip->mFontName.c_str();
    if (ImGui::BeginCombo("Font family##text_clip_editor", familyValue))
    {
        for (int i = 0; i < fontFamilies.size(); i++)
        {
            bool is_selected = fontFamilies[i] == clip->mFontName;
            if (ImGui::Selectable(fontFamilies[i].c_str(), is_selected))
            {
                clip->mFontName = fontFamilies[i];
                clip->mClipHolder->SetFont(clip->mFontName);
                update_preview = true;
            }
            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    } ImGui::SameLine(reset_button_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##clip_font_family_default")) { clip->mFontName = style.Font(); clip->mClipHolder->SetFont(clip->mFontName); update_preview = true; }
    
    ImGui::ImCurveEdit::keys text_key; text_key.m_id = clip->mID;
    float pos_x = clip->mFontPosX;
    int curve_pos_x_index = key_point->GetCurveIndex("OffsetH");
    bool has_curve_pos_x = curve_pos_x_index != -1;
    ImGui::BeginDisabled(has_curve_pos_x);
    if (ImGui::SliderFloat("Font position X", &pos_x, - default_size.x, 1.f, "%.2f"))
    {
        float offset_x = pos_x - clip->mFontPosX;
        clip->mFontOffsetH += offset_x;
        clip->mClipHolder->SetOffsetH(clip->mFontOffsetH);
        clip->mFontPosX = pos_x;
        update_preview = true;
    } ImGui::SameLine(reset_button_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##posx_default")) { clip->mFontOffsetH = 0; clip->mClipHolder->SetOffsetH(0.f); update_preview = true; }
    ImGui::EndDisabled();
    if (ImGui::ImCurveCheckEditKey("##add_curve_text_pos_x##text_clip_ediror", &text_key, has_curve_pos_x, "text_pos_x##text_clip_ediror",  - (float)default_size.x , 1.f, pos_x, curve_button_offset))
    {
        if (has_curve_pos_x) addCurve("OffsetH", text_key.m_min, text_key.m_max, text_key.m_default);
        else { key_point->DeleteCurve("OffsetH"); clip->mClipHolder->SetOffsetH(clip->mFontOffsetH); }
        update_preview = true;
    }
    if (has_curve_pos_x) EditCurve("OffsetH");
    
    float pos_y = clip->mFontPosY;
    int curve_pos_y_index = key_point->GetCurveIndex("OffsetV");
    bool has_curve_pos_y = curve_pos_y_index != -1;
    ImGui::BeginDisabled(has_curve_pos_y);
    if (ImGui::SliderFloat("Font position Y", &pos_y, - default_size.y, 1.0, "%.2f"))
    {
        float offset_y = pos_y - clip->mFontPosY;
        clip->mFontOffsetV += offset_y;
        clip->mClipHolder->SetOffsetV(clip->mFontOffsetV);
        clip->mFontPosY = pos_y;
        update_preview = true;
    } ImGui::SameLine(reset_button_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##posy_default")) { clip->mFontOffsetV = 0; clip->mClipHolder->SetOffsetV(0.f); update_preview = true; }
    ImGui::EndDisabled();
    if (ImGui::ImCurveCheckEditKey("##add_curve_text_pos_y##text_clip_ediror", &text_key, has_curve_pos_y, "text_pos_y##text_clip_ediror",  - (float)default_size.y , 1.f, pos_y, curve_button_offset))
    {
        if (has_curve_pos_y) addCurve("OffsetV", text_key.m_min, text_key.m_max, text_key.m_default);
        else { key_point->DeleteCurve("OffsetV"); clip->mClipHolder->SetOffsetV(clip->mFontOffsetV); }
        update_preview = true;
    }
    if (has_curve_pos_y) EditCurve("OffsetV");
    
    float scale_x = clip->mFontScaleX;
    int curve_scale_x_index = key_point->GetCurveIndex("ScaleX");
    bool has_curve_scale_x = curve_scale_x_index != -1;
    ImGui::BeginDisabled(has_curve_scale_x);
    if (ImGui::SliderFloat("Font scale X", &scale_x, 0.2, 10, "%.1f"))
    {
        float scale_ratio = scale_x / clip->mFontScaleX;
        if (clip->mScaleSettingLink) { clip->mFontScaleY *= scale_ratio; clip->mClipHolder->SetScaleY(clip->mFontScaleY); }
        clip->mFontScaleX = scale_x;
        clip->mClipHolder->SetScaleX(clip->mFontScaleX);
        update_preview = true;
    } ImGui::SameLine(reset_button_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##scalex_default")) { clip->mFontScaleX = style.ScaleX(); clip->mClipHolder->SetScaleX(style.ScaleX()); update_preview = true; }
    ImGui::EndDisabled();
    if (ImGui::ImCurveCheckEditKey("##add_curve_text_scale_x##text_clip_ediror", &text_key, has_curve_scale_x, "text_pscale_x##text_clip_ediror",  0.2f , 10.f, style.ScaleX(), curve_button_offset))
    {
        if (has_curve_scale_x) addCurve("ScaleX", text_key.m_min, text_key.m_max, text_key.m_default);
        else { key_point->DeleteCurve("ScaleX"); clip->mClipHolder->SetScaleX(clip->mFontScaleX); }
        update_preview = true;
    }
    if (has_curve_scale_x) EditCurve("ScaleX");
    
    // link button for scalex/scaley
    auto was_disable = (ImGui::GetItemFlags() & ImGuiItemFlags_Disabled) == ImGuiItemFlags_Disabled;
    auto current_pos = ImGui::GetCursorScreenPos();
    auto link_button_pos = current_pos + ImVec2(size.x - 96, - 8);
    ImRect link_button_rect(link_button_pos, link_button_pos + ImVec2(16, 16));
    std::string link_button_text = std::string(clip->mScaleSettingLink ? ICON_SETTING_LINK : ICON_SETTING_UNLINK);
    auto  link_button_color = clip->mScaleSettingLink ? IM_COL32(192, 192, 192, 255) : IM_COL32(128, 128, 128, 255);
    if (was_disable) link_button_color = IM_COL32(128, 128, 128, 128);
    if (!was_disable && link_button_rect.Contains(io.MousePos))
    {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            clip->mScaleSettingLink = !clip->mScaleSettingLink;
        link_button_color = IM_COL32_WHITE;
    }
    draw_list->AddText(link_button_pos, link_button_color, link_button_text.c_str());

    float scale_y = clip->mFontScaleY;
    int curve_scale_y_index = key_point->GetCurveIndex("ScaleY");
    bool has_curve_scale_y = curve_scale_y_index != -1;
    ImGui::BeginDisabled(has_curve_scale_y);
    if (ImGui::SliderFloat("Font scale Y", &scale_y, 0.2, 10, "%.1f"))
    {
        float scale_ratio = scale_y / clip->mFontScaleY;
        if (clip->mScaleSettingLink) { clip->mFontScaleX *= scale_ratio; clip->mClipHolder->SetScaleX(clip->mFontScaleX); }
        clip->mFontScaleY = scale_y;
        clip->mClipHolder->SetScaleY(clip->mFontScaleY);
        update_preview = true;
    } ImGui::SameLine(reset_button_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##scaley_default")) { clip->mFontScaleY = style.ScaleY(); clip->mClipHolder->SetScaleY(style.ScaleY()); update_preview = true; }
    ImGui::EndDisabled();
    if (ImGui::ImCurveCheckEditKey("##add_curve_text_scale_y##text_clip_ediror", &text_key, has_curve_scale_y, "text_scale_y##text_clip_ediror",  0.2f , 10.f, style.ScaleY(), curve_button_offset))
    {
        if (has_curve_scale_y) addCurve("ScaleY", text_key.m_min, text_key.m_max, text_key.m_default);
        else { key_point->DeleteCurve("ScaleY"); clip->mClipHolder->SetScaleY(clip->mFontScaleY); }
        update_preview = true;
    }
    if (has_curve_scale_y) EditCurve("ScaleY");
    
    float spacing = clip->mFontSpacing;
    int curve_spacing_index = key_point->GetCurveIndex("Spacing");
    bool has_curve_spacing = curve_spacing_index != -1;
    ImGui::BeginDisabled(has_curve_spacing);
    if (ImGui::SliderFloat("Font spacing", &spacing, 0.5, 5, "%.1f"))
    {
        clip->mFontSpacing = spacing;
        clip->mClipHolder->SetSpacing(clip->mFontSpacing);
        update_preview = true;
    } ImGui::SameLine(reset_button_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##spacing_default")) { clip->mFontSpacing = style.Spacing(); clip->mClipHolder->SetSpacing(style.Spacing()); update_preview = true; }
    ImGui::EndDisabled();
    if (ImGui::ImCurveCheckEditKey("##add_curve_text_spacing##text_clip_ediror", &text_key, has_curve_spacing, "text_spacing##text_clip_ediror",  0.5f , 5.f, style.Spacing(), curve_button_offset))
    {
        if (has_curve_spacing) addCurve("Spacing", text_key.m_min, text_key.m_max, text_key.m_default);
        else { key_point->DeleteCurve("Spacing"); clip->mClipHolder->SetSpacing(clip->mFontSpacing); }
        update_preview = true;
    }
    if (has_curve_spacing) EditCurve("Spacing");
    
    float anglex = clip->mFontAngleX;
    int curve_anglex_index = key_point->GetCurveIndex("AngleX");
    bool has_curve_anglex = curve_anglex_index != -1;
    ImGui::BeginDisabled(has_curve_anglex);
    if (ImGui::SliderFloat("Font angle X", &anglex, 0, 360, "%.1f"))
    {
        clip->mFontAngleX = anglex;
        clip->mClipHolder->SetRotationX(clip->mFontAngleX);
        update_preview = true;
    } ImGui::SameLine(reset_button_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##anglex_default")) { clip->mFontAngleX = style.Angle(); clip->mClipHolder->SetRotationX( style.Angle()); update_preview = true; }
    ImGui::EndDisabled();
    if (ImGui::ImCurveCheckEditKey("##add_curve_text_anglex##text_clip_ediror", &text_key, has_curve_anglex, "text_anglex##text_clip_ediror",  0.f , 360.f, style.Angle(), curve_button_offset))
    {
        if (has_curve_anglex) addCurve("AngleX", text_key.m_min, text_key.m_max, text_key.m_default);
        else { key_point->DeleteCurve("AngleX"); clip->mClipHolder->SetRotationX(clip->mFontAngleX); }
        update_preview = true;
    }
    if (has_curve_anglex) EditCurve("AngleX");
    
    float angley = clip->mFontAngleY;
    int curve_angley_index = key_point->GetCurveIndex("AngleY");
    bool has_curve_angley = curve_angley_index != -1;
    ImGui::BeginDisabled(has_curve_angley);
    if (ImGui::SliderFloat("Font angle Y", &angley, 0, 360, "%.1f"))
    {
        clip->mFontAngleY = angley;
        clip->mClipHolder->SetRotationY(clip->mFontAngleY);
        update_preview = true;
    } ImGui::SameLine(reset_button_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##angley_default")) { clip->mFontAngleY = style.Angle(); clip->mClipHolder->SetRotationY( style.Angle()); update_preview = true; }
    ImGui::EndDisabled();
    if (ImGui::ImCurveCheckEditKey("##add_curve_text_angley##text_clip_ediror", &text_key, has_curve_angley, "text_angley##text_clip_ediror",  0.f , 360.f, style.Angle(), curve_button_offset))
    {
        if (has_curve_angley) addCurve("AngleY", text_key.m_min, text_key.m_max, text_key.m_default);
        else { key_point->DeleteCurve("AngleY"); clip->mClipHolder->SetRotationY(clip->mFontAngleY); }
        update_preview = true;
    }
    if (has_curve_angley) EditCurve("AngleY");
    
    float anglez = clip->mFontAngleZ;
    int curve_anglez_index = key_point->GetCurveIndex("AngleZ");
    bool has_curve_anglez = curve_anglez_index != -1;
    ImGui::BeginDisabled(has_curve_anglez);
    if (ImGui::SliderFloat("Font angle Z", &anglez, 0, 360, "%.1f"))
    {
        clip->mFontAngleZ = anglez;
        clip->mClipHolder->SetRotationZ(clip->mFontAngleZ);
        update_preview = true;
    } ImGui::SameLine(reset_button_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##anglez_default")) { clip->mFontAngleZ = style.Angle(); clip->mClipHolder->SetRotationZ( style.Angle()); update_preview = true; }
    ImGui::EndDisabled();
    if (ImGui::ImCurveCheckEditKey("##add_curve_text_anglez##text_clip_ediror", &text_key, has_curve_anglez, "text_anglez##text_clip_ediror",  0.f , 360.f, style.Angle(), curve_button_offset))
    {
        if (has_curve_anglez) addCurve("AngleZ", text_key.m_min, text_key.m_max, text_key.m_default);
        else { key_point->DeleteCurve("AngleZ"); clip->mClipHolder->SetRotationZ(clip->mFontAngleZ); }
        update_preview = true;
    }
    if (has_curve_anglez) EditCurve("AngleZ");

    float outline_width = clip->mFontOutlineWidth;
    int curve_outline_width_index = key_point->GetCurveIndex("OutlineWidth");
    bool has_curve_outline_width = curve_outline_width_index != -1;
    ImGui::BeginDisabled(has_curve_outline_width);
    if (ImGui::SliderFloat("Font outline width", &outline_width, 0, 5, "%.0f"))
    {
        clip->mFontOutlineWidth = outline_width;
        clip->mClipHolder->SetBorderWidth(clip->mFontOutlineWidth);
        update_preview = true;
    } ImGui::SameLine(reset_button_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##outline_default")) { clip->mFontOutlineWidth = style.OutlineWidth(); clip->mClipHolder->SetBorderWidth(style.OutlineWidth()); update_preview = true; }
    ImGui::EndDisabled();
    if (ImGui::ImCurveCheckEditKey("##add_curve_text_outline_width##text_clip_ediror", &text_key, has_curve_outline_width, "text_outline_width##text_clip_ediror",  0.f , 5.f, style.OutlineWidth(), curve_button_offset))
    {
        if (has_curve_outline_width) addCurve("OutlineWidth", text_key.m_min, text_key.m_max, text_key.m_default);
        else { key_point->DeleteCurve("OutlineWidth"); clip->mClipHolder->SetBorderWidth(clip->mFontOutlineWidth); }
        update_preview = true;
    }
    if (has_curve_outline_width) EditCurve("OutlineWidth");
    
    float shadow_depth = clip->mFontShadowDepth;
    int curve_shadow_depth_index = key_point->GetCurveIndex("ShadowDepth");
    bool has_curve_shadow_depth = curve_shadow_depth_index != -1;
    ImGui::BeginDisabled(has_curve_shadow_depth);
    if (ImGui::SliderFloat("Font shadow depth", &shadow_depth, 0.f, 20.f, "%.1f"))
    {
        clip->mFontShadowDepth = shadow_depth;
        clip->mClipHolder->SetShadowDepth(clip->mFontShadowDepth);
        update_preview = true;
    } ImGui::SameLine(reset_button_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##shadow_default")) { clip->mFontShadowDepth = fabs(style.ShadowDepth()); clip->mClipHolder->SetShadowDepth(clip->mFontShadowDepth); update_preview = true; }
    ImGui::EndDisabled();
    if (ImGui::ImCurveCheckEditKey("##add_curve_text_shadow_depth##text_clip_ediror", &text_key, has_curve_shadow_depth, "text_shadow_depth##text_clip_ediror",  0.f , 20.f, style.ShadowDepth(), curve_button_offset))
    {
        if (has_curve_shadow_depth) addCurve("ShadowDepth", text_key.m_min, text_key.m_max, text_key.m_default);
        else { key_point->DeleteCurve("ShadowDepth"); clip->mClipHolder->SetShadowDepth(clip->mFontShadowDepth); }
        update_preview = true;
    }
    if (has_curve_shadow_depth) EditCurve("ShadowDepth");
    
    if (ImGui::Checkbox(ICON_FONT_BOLD "##font_bold", &clip->mFontBold))
    {
        clip->mClipHolder->SetBold(clip->mFontBold);
        update_preview = true;
    }
    ImGui::SameLine();
    if (ImGui::Checkbox(ICON_FONT_ITALIC "##font_italic", &clip->mFontItalic))
    {
        clip->mClipHolder->SetItalic(clip->mFontItalic);
        update_preview = true;
    }
    ImGui::SameLine();
    if (ImGui::Checkbox(ICON_FONT_UNDERLINE "##font_underLine", &clip->mFontUnderLine))
    {
        clip->mClipHolder->SetUnderLine(clip->mFontUnderLine);
        update_preview = true;
    }
    ImGui::SameLine();
    if (ImGui::Checkbox(ICON_FONT_STRIKEOUT "##font_strike_out", &clip->mFontStrikeOut))
    {
        clip->mClipHolder->SetStrikeOut(clip->mFontStrikeOut);
        update_preview = true;
    }
    ImGui::SameLine(item_width); ImGui::TextUnformatted("Font attribute");
    ImGui::SameLine(reset_button_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##attribute_default"))
    { 
        clip->mFontBold = style.Bold() > 0; 
        clip->mFontItalic = style.Italic() > 0;
        clip->mFontUnderLine = style.UnderLine();
        clip->mFontStrikeOut = style.StrikeOut();
        clip->mClipHolder->SetBold(clip->mFontBold);
        clip->mClipHolder->SetItalic(clip->mFontItalic);
        clip->mClipHolder->SetUnderLine(clip->mFontUnderLine);
        clip->mClipHolder->SetStrikeOut(clip->mFontStrikeOut);
        update_preview = true;
    }

    int alignment = clip->mFontAlignment;
    ImGui::RadioButton(ICON_FA_ALIGN_LEFT "##font_alignment", (int *)&alignment, 1); ImGui::SameLine();
    ImGui::RadioButton(ICON_FA_ALIGN_CENTER "##font_alignment", (int *)&alignment, 2); ImGui::SameLine();
    ImGui::RadioButton(ICON_FA_ALIGN_RIGHT "##font_alignment", (int *)&alignment, 3);
    ImGui::SameLine(item_width); ImGui::TextUnformatted("Font alignment");
    if (alignment != clip->mFontAlignment)
    {
        clip->mFontAlignment = alignment;
        clip->mClipHolder->SetAlignment(alignment);
        update_preview = true;
    }
    ImGui::SameLine(reset_button_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##alignment_default")) { clip->mFontAlignment = style.Alignment(); clip->mClipHolder->SetAlignment(style.Alignment()); update_preview = true; }

    if (ImGui::ColorEdit4("FontColor##Primary", (float*)&clip->mFontPrimaryColor, ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_AlphaBar))
    {
        clip->mClipHolder->SetPrimaryColor(clip->mFontPrimaryColor);
        update_preview = true;
    }
    ImGui::SameLine(item_width); ImGui::TextUnformatted("Font primary color");
    ImGui::SameLine(reset_button_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##primary_color_default")) { clip->mFontPrimaryColor = style.PrimaryColor().ToImVec4(); clip->mClipHolder->SetPrimaryColor(style.PrimaryColor()); update_preview = true; }
    if (ImGui::ColorEdit4("FontColor##Outline", (float*)&clip->mFontOutlineColor, ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_AlphaBar))
    {
        clip->mClipHolder->SetOutlineColor(clip->mFontOutlineColor);
        update_preview = true;
    }
    ImGui::SameLine(item_width); ImGui::TextUnformatted("Font outline color");
    ImGui::SameLine(reset_button_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##outline_color_default")) { clip->mFontOutlineColor = style.OutlineColor().ToImVec4(); clip->mClipHolder->SetOutlineColor(style.OutlineColor()); update_preview = true; }
    if (ImGui::ColorEdit4("FontColor##Back", (float*)&clip->mFontBackColor, ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_AlphaBar))
    {
        clip->mClipHolder->SetBackColor(clip->mFontBackColor);
        update_preview = true;
    }
    ImGui::SameLine(item_width); ImGui::TextUnformatted("Font shadow color");
    ImGui::SameLine(reset_button_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##back_color_default")) { clip->mFontBackColor = style.BackColor().ToImVec4(); clip->mClipHolder->SetBackColor(style.BackColor()); update_preview = true; }

    ImGui::PopItemWidth();

    if (update_preview && clip->mClipHolder) clip->mClipHolder->SetKeyPoints(clip->mAttributeKeyPoints); 
    return update_preview;
}

static bool edit_text_track_style(ImDrawList *draw_list, MediaTrack * track, ImVec2 size)
{
    if (!track || !track->mMttReader)
        return false;
    bool update_preview = false;
    // Add Curve
    auto keyPointsPtr = track->mMttReader->GetKeyPoints();
    char ** curve_type_list = nullptr;
    auto curve_type_count = ImGui::ImCurveEdit::GetCurveTypeName(curve_type_list);
    auto addCurve = [&](std::string name, float _min, float _max, float _default)
    {
        auto found = keyPointsPtr->GetCurveIndex(name);
        if (found == -1)
        {
            ImU32 color; ImGui::RandomColor(color, 1.f);
            auto curve_index = keyPointsPtr->AddCurve(name, ImGui::ImCurveEdit::Smooth, color, true, _min, _max, _default);
            keyPointsPtr->AddPoint(curve_index, ImVec2(keyPointsPtr->GetMin().x, _min), ImGui::ImCurveEdit::Smooth);
            keyPointsPtr->AddPoint(curve_index, ImVec2(keyPointsPtr->GetMax().x, _max), ImGui::ImCurveEdit::Smooth);
            keyPointsPtr->SetCurvePointDefault(curve_index, 0);
            keyPointsPtr->SetCurvePointDefault(curve_index, 1);
            update_preview = true;
        }
    };
    // Editor Curve
    auto EditCurve = [&](std::string name) 
    {
        int index = keyPointsPtr->GetCurveIndex(name);
        if (index != -1)
        {
            ImGui::Separator();
            bool break_loop = false;
            ImGui::PushID(ImGui::GetID(name.c_str()));
            auto pCount = keyPointsPtr->GetCurvePointCount(index);
            std::string lable_id = std::string(ICON_CURVE) + " " + name + " (" + std::to_string(pCount) + " keys)" + "##text_track_curve";
            if (ImGui::TreeNodeEx(lable_id.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
                float value = keyPointsPtr->GetValue(index, timeline->currentTime);
                ImGui::BracketSquare(true); ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0, 1.0, 0.0, 1.0)); ImGui::Text("%.2f", value); ImGui::PopStyleColor();
                
                ImGui::PushItemWidth(60);
                float curve_min = keyPointsPtr->GetCurveMin(index);
                float curve_max = keyPointsPtr->GetCurveMax(index);
                ImGui::BeginDisabled(true);
                ImGui::DragFloat("##curve_text_track_min", &curve_min, 0.1f, -FLT_MAX, curve_max, "%.1f"); ImGui::ShowTooltipOnHover("Min");
                ImGui::SameLine(0, 8);
                ImGui::DragFloat("##curve_text_track_max", &curve_max, 0.1f, curve_min, FLT_MAX, "%.1f"); ImGui::ShowTooltipOnHover("Max");
                ImGui::SameLine(0, 8);
                ImGui::EndDisabled();
                float curve_default = keyPointsPtr->GetCurveDefault(index);
                if (ImGui::DragFloat("##curve_text_track_default", &curve_default, 0.1f, curve_min, curve_max, "%.1f"))
                {
                    keyPointsPtr->SetCurveDefault(index, curve_default);
                    update_preview = true;
                } ImGui::ShowTooltipOnHover("Default");
                ImGui::PopItemWidth();

                ImGui::SameLine(0, 8);
                ImGui::SetWindowFontScale(0.75);
                auto curve_color = ImGui::ColorConvertU32ToFloat4(keyPointsPtr->GetCurveColor(index));
                if (ImGui::ColorEdit4("##curve_text_track_color", (float*)&curve_color, ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar))
                {
                    keyPointsPtr->SetCurveColor(index, ImGui::ColorConvertFloat4ToU32(curve_color));
                } ImGui::ShowTooltipOnHover("Curve Color");
                ImGui::SetWindowFontScale(1.0);
                ImGui::SameLine(0, 4);
                bool is_visiable = keyPointsPtr->IsVisible(index);
                if (ImGui::Button(is_visiable ? ICON_WATCH : ICON_UNWATCH "##curve_text_track_visiable"))
                {
                    is_visiable = !is_visiable;
                    keyPointsPtr->SetCurveVisible(index, is_visiable);
                } ImGui::ShowTooltipOnHover(is_visiable ? "Hide" : "Show");
                ImGui::SameLine(0, 4);
                if (ImGui::Button(ICON_DELETE "##curve_text_track_delete"))
                {
                    keyPointsPtr->DeleteCurve(index);
                    update_preview = true;
                    break_loop = true;
                } ImGui::ShowTooltipOnHover("Delete");
                ImGui::SameLine(0, 4);
                if (ImGui::Button(ICON_RETURN_DEFAULT "##curve_text_track_reset"))
                {
                    for (int p = 0; p < pCount; p++)
                    {
                        keyPointsPtr->SetCurvePointDefault(index, p);
                    }
                    update_preview = true;
                } ImGui::ShowTooltipOnHover("Reset");

                if (!break_loop)
                {
                    // list points
                    for (int p = 0; p < pCount; p++)
                    {
                        bool is_disabled = false;
                        ImGui::PushID(p);
                        ImGui::PushItemWidth(96);
                        auto point = keyPointsPtr->GetPoint(index, p);
                        ImGui::Diamond(false);
                        if (p == 0 || p == pCount - 1)
                            is_disabled = true;
                        ImGui::BeginDisabled(is_disabled);
                        if (ImGui::DragTimeMS("##curve_text_track_point_x", &point.point.x, keyPointsPtr->GetMax().x / 1000.f, keyPointsPtr->GetMin().x, keyPointsPtr->GetMax().x, 2))
                        {
                            keyPointsPtr->EditPoint(index, p, point.point, point.type);
                            update_preview = true;
                        }
                        ImGui::EndDisabled();
                        ImGui::SameLine();
                        auto speed = fabs(keyPointsPtr->GetCurveMax(index) - keyPointsPtr->GetCurveMin(index)) / 500;
                        if (ImGui::DragFloat("##curve_text_track_point_y", &point.point.y, speed, keyPointsPtr->GetCurveMin(index), keyPointsPtr->GetCurveMax(index), "%.2f"))
                        {
                            keyPointsPtr->EditPoint(index, p, point.point, point.type);
                            update_preview = true;
                        }
                        ImGui::SameLine();
                        if (ImGui::Combo("##curve_text_track_type", (int*)&point.type, curve_type_list, curve_type_count))
                        {
                            keyPointsPtr->EditPoint(index, p, point.point, point.type);
                            update_preview = true;
                        }
                        ImGui::PopItemWidth();
                        ImGui::PopID();
                    }
                }

                ImGui::PopStyleColor();
                ImGui::TreePop();
            }
            ImGui::PopID();
            ImGui::Separator();
        }
    };
    ImGuiIO &io = ImGui::GetIO();
    ImGui::PushItemWidth(240);
    const float reset_button_offset = size.x - 64;
    const float curve_button_offset = size.x - 36;
    auto item_width = ImGui::CalcItemWidth();
    auto& style = track->mMttReader->DefaultStyle();
    auto familyName = style.Font();
    const char* familyValue = familyName.c_str();
    ImGui::ImCurveEdit::keys text_key; text_key.m_id = track->mID;
    if (ImGui::BeginCombo("Font family##text_track_editor", familyValue))
    {
        for (int i = 0; i < fontFamilies.size(); i++)
        {
            bool is_selected = fontFamilies[i] == style.Font();
            if (ImGui::Selectable(fontFamilies[i].c_str(), is_selected))
            {
                track->mMttReader->SetFont(fontFamilies[i]);
                update_preview = true;
            }
            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    } ImGui::SameLine(reset_button_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##track_font_family_default")) { track->mMttReader->SetFont(g_media_editor_settings.FontName); update_preview = true; }
    
    float offset_x = style.OffsetHScale();
    int curve_pos_x_index = keyPointsPtr->GetCurveIndex("OffsetH");
    bool has_curve_pos_x = curve_pos_x_index != -1;
    ImGui::BeginDisabled(has_curve_pos_x);
    if (ImGui::SliderFloat("Font position X", &offset_x, -1.f , 1.f, "%.2f"))
    {
        track->mMttReader->SetOffsetH(offset_x);
        update_preview = true;
    } ImGui::SameLine(reset_button_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##track_font_offsetx_default")) { track->mMttReader->SetOffsetH(g_media_editor_settings.FontPosOffsetX); update_preview = true; }
    ImGui::EndDisabled();
    if (ImGui::ImCurveCheckEditKey("##add_curve_text_pos_x##text_track_ediror", &text_key, has_curve_pos_x, "text_pos_x##text_track_ediror",  -1.f , 1.f, g_media_editor_settings.FontPosOffsetX, curve_button_offset))
    {
        if (has_curve_pos_x) addCurve("OffsetH", text_key.m_min, text_key.m_max, text_key.m_default);
        else keyPointsPtr->DeleteCurve("OffsetH");
        update_preview = true;
    }
    if (has_curve_pos_x) EditCurve("OffsetH");

    float offset_y = style.OffsetVScale();
    int curve_pos_y_index = keyPointsPtr->GetCurveIndex("OffsetV");
    bool has_curve_pos_y = curve_pos_y_index != -1;
    ImGui::BeginDisabled(has_curve_pos_y);
    if (ImGui::SliderFloat("Font position Y", &offset_y, -1.f, 1.f, "%.2f"))
    {
        track->mMttReader->SetOffsetV(offset_y);
        update_preview = true;
    } ImGui::SameLine(reset_button_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##track_font_offsety_default")) { track->mMttReader->SetOffsetV(g_media_editor_settings.FontPosOffsetY); update_preview = true; }
    ImGui::EndDisabled();
    if (ImGui::ImCurveCheckEditKey("##add_curve_text_pos_y##text_track_ediror", &text_key, has_curve_pos_y, "text_pos_y##text_track_ediror",  -1.f , 1.f, g_media_editor_settings.FontPosOffsetY, curve_button_offset))
    {
        if (has_curve_pos_y) addCurve("OffsetV", text_key.m_min, text_key.m_max, text_key.m_default);
        else keyPointsPtr->DeleteCurve("OffsetV");
        update_preview = true;
    }
    if (has_curve_pos_y) EditCurve("OffsetV");
    
    int bold = style.Bold();
    if (ImGui::Combo("Font Bold", &bold, font_bold_list, IM_ARRAYSIZE(font_bold_list)))
    {
        track->mMttReader->SetBold(bold);
        update_preview = true;
    } ImGui::SameLine(reset_button_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##track_font_bold_default")) { track->mMttReader->SetBold(g_media_editor_settings.FontBold); update_preview = true; }
    
    int italic = style.Italic();
    if (ImGui::Combo("Font Italic", &italic, font_italic_list, IM_ARRAYSIZE(font_italic_list)))
    {
        track->mMttReader->SetItalic(italic);
        update_preview = true;
    } ImGui::SameLine(reset_button_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##track_font_italic_default")) { track->mMttReader->SetItalic(g_media_editor_settings.FontItalic); update_preview = true; }
    
    float scale_x = style.ScaleX();
    int curve_scale_x_index = keyPointsPtr->GetCurveIndex("ScaleX");
    bool has_curve_scale_x = curve_scale_x_index != -1;
    ImGui::BeginDisabled(has_curve_scale_x);
    if (ImGui::SliderFloat("Font scale X", &scale_x, 0.2, 10, "%.1f"))
    {
        float scale_ratio = scale_x / style.ScaleX();
        if (track->mTextTrackScaleLink) track->mMttReader->SetScaleY(style.ScaleY() * scale_ratio);
        track->mMttReader->SetScaleX(scale_x);
        update_preview = true;
    } ImGui::SameLine(reset_button_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##track_scalex_default")) { track->mMttReader->SetScaleX(g_media_editor_settings.FontScaleX); update_preview = true; }
    ImGui::EndDisabled();
    if (ImGui::ImCurveCheckEditKey("##add_curve_text_scale_x##text_track_ediror", &text_key, has_curve_scale_x, "text_scale_x##text_track_ediror",  0.2f, 10.f, g_media_editor_settings.FontScaleX, curve_button_offset))
    {
        if (has_curve_scale_x) addCurve("ScaleX", text_key.m_min, text_key.m_max, text_key.m_default);
        else keyPointsPtr->DeleteCurve("ScaleX");
        update_preview = true;
    }
    if (has_curve_scale_x) EditCurve("ScaleX");

    // link button for scalex/scaley
    auto current_pos = ImGui::GetCursorScreenPos();
    auto link_button_pos = current_pos + ImVec2(size.x - 96, - 8);
    ImRect link_button_rect(link_button_pos, link_button_pos + ImVec2(16, 16));
    std::string link_button_text = std::string(track->mTextTrackScaleLink ? ICON_SETTING_LINK : ICON_SETTING_UNLINK);
    auto  link_button_color = track->mTextTrackScaleLink ? IM_COL32(192, 192, 192, 255) : IM_COL32(128, 128, 128, 255);
    if (link_button_rect.Contains(io.MousePos))
    {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            track->mTextTrackScaleLink = !track->mTextTrackScaleLink;
        link_button_color = IM_COL32_WHITE;
    }
    draw_list->AddText(link_button_pos, link_button_color, link_button_text.c_str());
    
    float scale_y = style.ScaleY();
    int curve_scale_y_index = keyPointsPtr->GetCurveIndex("ScaleY");
    bool has_curve_scale_y = curve_scale_y_index != -1;
    ImGui::BeginDisabled(has_curve_scale_y);
    if (ImGui::SliderFloat("Font scale Y", &scale_y, 0.2, 10, "%.1f"))
    {
        float scale_ratio = scale_y / style.ScaleY();
        if (track->mTextTrackScaleLink) track->mMttReader->SetScaleX(style.ScaleX() * scale_ratio);
        track->mMttReader->SetScaleY(scale_y);
        update_preview = true;
    } ImGui::SameLine(reset_button_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##track_scaley_default")) { track->mMttReader->SetScaleY(g_media_editor_settings.FontScaleY); update_preview = true; }
    ImGui::EndDisabled();
    if (ImGui::ImCurveCheckEditKey("##add_curve_text_scale_y##text_track_ediror", &text_key, has_curve_scale_y, "text_scale_y##text_track_ediror",  0.2f, 10.f, g_media_editor_settings.FontScaleY, curve_button_offset))
    {
        if (has_curve_scale_y) addCurve("ScaleY", text_key.m_min, text_key.m_max, text_key.m_default);
        else keyPointsPtr->DeleteCurve("ScaleY");
        update_preview = true;
    }
    if (has_curve_scale_y) EditCurve("ScaleY");
    
    float spacing = style.Spacing();
    int curve_spacing_index = keyPointsPtr->GetCurveIndex("Spacing");
    bool has_curve_spacing = curve_spacing_index != -1;
    ImGui::BeginDisabled(has_curve_spacing);
    if (ImGui::SliderFloat("Font spacing", &spacing, 0.5, 5, "%.1f"))
    {
        track->mMttReader->SetSpacing(spacing);
        update_preview = true;
    } ImGui::SameLine(reset_button_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##track_spacing_default")) { track->mMttReader->SetSpacing(g_media_editor_settings.FontSpacing); update_preview = true; }
    ImGui::EndDisabled();
    if (ImGui::ImCurveCheckEditKey("##add_curve_text_spacing##text_track_ediror", &text_key, has_curve_spacing, "text_spacing##text_track_ediror",  0.5f, 5.f, g_media_editor_settings.FontSpacing, curve_button_offset))
    {
        if (has_curve_spacing) addCurve("Spacing", text_key.m_min, text_key.m_max, text_key.m_default);
        else keyPointsPtr->DeleteCurve("Spacing");
        update_preview = true;
    }
    if (has_curve_spacing) EditCurve("Spacing");
    
    float angle = style.Angle();
    int curve_angle_index = keyPointsPtr->GetCurveIndex("Angle");
    bool has_curve_angle = curve_angle_index != -1;
    ImGui::BeginDisabled(has_curve_angle);
    if (ImGui::SliderFloat("Font angle", &angle, 0, 360, "%.1f"))
    {
        track->mMttReader->SetAngle(angle);
        update_preview = true;
    } ImGui::SameLine(reset_button_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##track_angle_default")) { track->mMttReader->SetAngle(g_media_editor_settings.FontAngle);update_preview = true;  }
    ImGui::EndDisabled();
    if (ImGui::ImCurveCheckEditKey("##add_curve_text_angle##text_track_ediror", &text_key, has_curve_angle, "text_angle##text_track_ediror",  0.f, 360.f, g_media_editor_settings.FontAngle, curve_button_offset))
    {
        if (has_curve_angle) addCurve("Angle", text_key.m_min, text_key.m_max, text_key.m_default);
        else keyPointsPtr->DeleteCurve("Angle");
        update_preview = true;
    }
    if (has_curve_angle) EditCurve("Angle");
    
    float outline_width = style.OutlineWidth();
    int curve_outline_width_index = keyPointsPtr->GetCurveIndex("OutlineWidth");
    bool has_curve_outline_width = curve_outline_width_index != -1;
    ImGui::BeginDisabled(has_curve_outline_width);
    if (ImGui::SliderFloat("Font outline width", &outline_width, 0, 5, "%.0f"))
    {
        track->mMttReader->SetOutlineWidth(outline_width);
        update_preview = true;
    } ImGui::SameLine(reset_button_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##track_outline_default")) { track->mMttReader->SetOutlineWidth(g_media_editor_settings.FontOutlineWidth); update_preview = true; }
    ImGui::EndDisabled();
    if (ImGui::ImCurveCheckEditKey("##add_curve_text_outline_width##text_track_ediror", &text_key, has_curve_outline_width, "text_outline_width##text_track_ediror",  0.f, 5.f, g_media_editor_settings.FontOutlineWidth, curve_button_offset))
    {
        if (has_curve_outline_width) addCurve("OutlineWidth", text_key.m_min, text_key.m_max, text_key.m_default);
        else keyPointsPtr->DeleteCurve("OutlineWidth");
        update_preview = true;
    }
    if (has_curve_outline_width) EditCurve("OutlineWidth");
    
    bool underLine = style.UnderLine();
    if (ImGui::Checkbox(ICON_FONT_UNDERLINE "##font_underLine", &underLine))
    {
        track->mMttReader->SetUnderLine(underLine);
        update_preview = true;
    }
    ImGui::SameLine();
    bool strike_out = style.StrikeOut();
    if (ImGui::Checkbox(ICON_FONT_STRIKEOUT "##font_strike_out", &strike_out))
    {
        track->mMttReader->SetStrikeOut(strike_out);
        update_preview = true;
    }
    ImGui::SameLine(item_width); ImGui::TextUnformatted("Font attribute");
    ImGui::SameLine(reset_button_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##track_attribute_default")) 
    {
        track->mMttReader->SetUnderLine(g_media_editor_settings.FontUnderLine);
        track->mMttReader->SetStrikeOut(g_media_editor_settings.FontStrikeOut);
        update_preview = true;
    }

    int aligment = style.Alignment();
    ImGui::RadioButton(ICON_FA_ALIGN_LEFT "##font_alignment", &aligment, 1); ImGui::SameLine();
    ImGui::RadioButton(ICON_FA_ALIGN_CENTER "##font_alignment", &aligment, 2); ImGui::SameLine();
    ImGui::RadioButton(ICON_FA_ALIGN_RIGHT "##font_alignment", &aligment, 3);
    ImGui::SameLine(item_width); ImGui::TextUnformatted("Font alignment");
    if (aligment != style.Alignment())
    {
        track->mMttReader->SetAlignment(aligment);
        update_preview = true;
    }
    ImGui::SameLine(reset_button_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##track_alignment_default")) { track->mMttReader->SetAlignment(g_media_editor_settings.FontAlignment); update_preview = true; }

    int border_type = style.BorderStyle();
    ImGui::RadioButton("Drop##font_border_type", &border_type, 1); ImGui::SameLine();
    ImGui::RadioButton("Box##font_border_type", &border_type, 3);
    ImGui::SameLine(item_width); ImGui::TextUnformatted("Font Border Type");
    if (border_type != style.BorderStyle())
    {
        track->mMttReader->SetBorderStyle(border_type);
        update_preview = true;
    }
    ImGui::SameLine(reset_button_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##track_bordertype_default")) { track->mMttReader->SetBorderStyle(g_media_editor_settings.FontBorderType); update_preview = true; }

    float shadow_depth = style.ShadowDepth();
    int curve_outline_shadow_depth = keyPointsPtr->GetCurveIndex("ShadowDepth");
    bool has_curve_shadow_depth = curve_outline_shadow_depth != -1;
    ImGui::BeginDisabled(has_curve_shadow_depth);
    ImGui::SliderFloat("Font shadow depth", &shadow_depth, -20.f, 20.f, "%.1f");
    if (shadow_depth != style.ShadowDepth())
    {
        track->mMttReader->SetShadowDepth(shadow_depth);
        update_preview = true;
    }
    ImGui::SameLine(reset_button_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##track_shadowdepth_default")) { track->mMttReader->SetShadowDepth(g_media_editor_settings.FontShadowDepth); update_preview = true; }
    ImGui::EndDisabled();
    if (ImGui::ImCurveCheckEditKey("##add_curve_text_shadow_depth##text_track_ediror", &text_key, has_curve_shadow_depth, "text_shadow_depth##text_track_ediror",  -20.f, 20.f, g_media_editor_settings.FontShadowDepth, curve_button_offset))
    {
        if (has_curve_shadow_depth) addCurve("ShadowDepth", text_key.m_min, text_key.m_max, text_key.m_default);
        else keyPointsPtr->DeleteCurve("ShadowDepth");
        update_preview = true;
    }
    if (has_curve_shadow_depth) EditCurve("ShadowDepth");

    auto primary_color = style.PrimaryColor().ToImVec4();
    if (ImGui::ColorEdit4("FontColor##Primary", (float*)&primary_color, ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_AlphaBar))
    {
        track->mMttReader->SetPrimaryColor(primary_color);
        update_preview = true;
    }
    ImGui::SameLine(item_width); ImGui::TextUnformatted("Font primary color");
    ImGui::SameLine(reset_button_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##track_primary_color_default")) { track->mMttReader->SetPrimaryColor(g_media_editor_settings.FontPrimaryColor); update_preview = true; }
    auto outline_color = style.OutlineColor().ToImVec4();
    if (ImGui::ColorEdit4("FontColor##Outline", (float*)&outline_color, ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_AlphaBar))
    {
        track->mMttReader->SetOutlineColor(outline_color);
        update_preview = true;
    }
    ImGui::SameLine(item_width); ImGui::TextUnformatted("Font outline color");
    ImGui::SameLine(reset_button_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##track_outline_color_default")) { track->mMttReader->SetOutlineColor(g_media_editor_settings.FontOutlineColor); update_preview = true; }
    auto back_color = style.BackColor().ToImVec4();
    if (ImGui::ColorEdit4("FontColor##Back", (float*)&back_color, ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_AlphaBar))
    {
        track->mMttReader->SetBackColor(back_color);
        update_preview = true;
    }
    ImGui::SameLine(item_width); ImGui::TextUnformatted("Font shadow color");
    ImGui::SameLine(reset_button_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##track_back_color_default")) { track->mMttReader->SetBackColor(g_media_editor_settings.FontBackColor); update_preview = true; }

    ImGui::PopItemWidth();

    if (update_preview)
        track->mMttReader->Refresh();
    return update_preview;
}

static void ShowTextEditorWindow(ImDrawList *draw_list)
{
    /*
    ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━┓
    ┃                                            ┃                       ┃
    ┃                                            ┃   text input area     ┃
    ┃                                            ┣━━━━━━━━━━━┳━━━━━━━━━━━┫ 
    ┃               preview                      ┃  clip     ┃  track    ┃
    ┃                                            ┣━━━━━━━━━━━┻━━━━━━━━━━━┫ 
    ┃                                            ┃                       ┃ 
    ┃                                            ┃                       ┃ 
    ┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫                       ┃ 
    ┃             |<  <  []  >  >|               ┃    attribute edit     ┃ 
    ┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫                       ┃
    ┃             timeline                       ┃                       ┃
    ┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫                       ┃
    ┃              curves                        ┃                       ┃
    ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━┛
    */
    static int StyleWindowIndex = 0; 
    ImVec2 window_pos = ImGui::GetCursorScreenPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    draw_list->AddRectFilled(window_pos, window_pos + window_size, COL_DEEP_DARK);
    float preview_view_width = window_size.x * 2 / 3;
    float style_editor_width = window_size.x - preview_view_width;
    float text_keypoint_height = g_media_editor_settings.TextCurveExpanded ? 100 + 30 : 0;
    float preview_view_height = window_size.y - text_keypoint_height;
    ImVec2 text_keypoint_pos = window_pos + ImVec2(0, preview_view_height);
    ImVec2 text_keypoint_size(window_size.x - style_editor_width, text_keypoint_height);
    if (!timeline)
        return;
    bool force_update_preview = false;
    ImGuiIO &io = ImGui::GetIO();
    ImVec2 default_size(0, 0);
    MediaCore::SubtitleImage current_image;
    TextClip * editing_clip = dynamic_cast<TextClip*>(timeline->FindEditingClip());
    MediaTrack * editing_track = nullptr;
    if (editing_clip && !IS_TEXT(editing_clip->mType))
    {
        editing_clip = nullptr;
    }
    else if (editing_clip && editing_clip->mClipHolder)
    {
        editing_track = (MediaTrack *)editing_clip->mTrack;
        current_image = editing_clip->mClipHolder->Image(timeline->currentTime-editing_clip->mStart);
        default_size = ImVec2((float)current_image.Area().w / (float)timeline->GetPreviewWidth(), (float)current_image.Area().h / (float)timeline->GetPreviewHeight());
        editing_clip->mFontPosX = (float)current_image.Area().x / (float)timeline->GetPreviewWidth();
        editing_clip->mFontPosY = (float)current_image.Area().y / (float)timeline->GetPreviewHeight();
    }

    ImGuiWindowFlags child_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings;
    ImGuiWindowFlags setting_child_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
    ImGui::SetCursorScreenPos(window_pos + ImVec2(preview_view_width, 0));
    if (ImGui::BeginChild("##text_editor_style", ImVec2(style_editor_width, window_size.y), false, setting_child_flags))
    {
        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(1,1,1,1));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5, 0.5, 0.5, 0.5));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0, 1.0, 1.0, 1.0));
        ImVec2 style_window_pos = ImGui::GetCursorScreenPos();
        ImVec2 style_window_size = ImGui::GetWindowSize();
        draw_list->AddRectFilled(style_window_pos, style_window_pos + style_window_size, COL_BLACK_DARK);
        ImGui::SetCursorScreenPos(style_window_pos + ImVec2(10, 30));
        ImGui::TextComplex("Text Style", 2.0f, ImVec4(0.8, 0.8, 0.8, 0.2),
                            0.1f, ImVec4(0.8, 0.8, 0.8, 0.3),
                            ImVec2(4, 4), ImVec4(0.0, 0.0, 0.0, 0.5));
    
        ImGui::Separator();
        // add text style
        if (editing_clip)
        {
            // show clip time
            auto start_time_str = std::string(ICON_CLIP_START) + " " + ImGuiHelper::MillisecToString(editing_clip->mStart, 3);
            auto end_time_str = ImGuiHelper::MillisecToString(editing_clip->mEnd, 3) + " " + std::string(ICON_CLIP_END);
            auto start_time_str_size = ImGui::CalcTextSize(start_time_str.c_str());
            auto end_time_str_size = ImGui::CalcTextSize(end_time_str.c_str());
            float time_str_offset = (style_window_size.x - start_time_str_size.x - end_time_str_size.x - 30) / 2;
            ImGui::SetCursorScreenPos(style_window_pos + ImVec2(time_str_offset, 80));
            ImGui::TextUnformatted(start_time_str.c_str());
            ImGui::SameLine(0, 30);
            ImGui::TextUnformatted(end_time_str.c_str());
            // show clip text
            std::string value = editing_clip->mText;
            if (ImGui::InputTextEx("##text_clip_string", "Please input text here", (char*)value.data(), value.size() + 1, ImVec2(style_window_size.x, 64), ImGuiInputTextFlags_CallbackEdit | ImGuiInputTextFlags_CallbackResize | ImGuiInputTextFlags_Multiline, [](ImGuiInputTextCallbackData* data) -> int
            {
                if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
                {
                    auto& stringValue = *static_cast<string*>(data->UserData);
                    ImVector<char>* my_str = (ImVector<char>*)data->UserData;
                    //IM_ASSERT(stringValue.data() == data->Buf);
                    stringValue.resize(data->BufSize);
                    data->Buf = (char*)stringValue.data();
                }
                else if (data->EventFlag == ImGuiInputTextFlags_CallbackEdit)
                {
                    auto& stringValue = *static_cast<string*>(data->UserData);
                    stringValue = std::string(data->Buf);
                }
                return 0;
            }, &value))
            {
                value.resize(strlen(value.c_str()));
                if (editing_clip->mText.compare(value) != 0)
                {
                    editing_clip->mText = value;
                    editing_clip->mClipHolder->SetText(editing_clip->mText);
                    force_update_preview = true;
                }
            }
            // show style control
            bool useTrackStyle = editing_clip->mTrackStyle;
            if (ImGui::Checkbox("Using track style", &useTrackStyle))
            {
                editing_clip->EnableUsingTrackStyle(useTrackStyle);
                force_update_preview = true;
            }
            ImGui::Separator();
            static const int numTabs = sizeof(TextEditorTabNames)/sizeof(TextEditorTabNames[0]);
            ImVec2 style_view_pos = ImGui::GetCursorPos();
            ImVec2 style_view_size(style_window_size.x, window_size.y - style_view_pos.y);
            if (ImGui::BeginChild("##text_sytle_window", style_view_size - ImVec2(8, 0), false, child_flags))
            {
                if (ImGui::TabLabels(numTabs, TextEditorTabNames, StyleWindowIndex, nullptr , false, true, nullptr, nullptr, false, false, nullptr, nullptr))
                {
                }

                if (ImGui::BeginChild("##style_Window_content", style_view_size - ImVec2(16, 32), false, setting_child_flags))
                {
                    ImVec2 style_setting_window_pos = ImGui::GetCursorScreenPos();
                    ImVec2 style_setting_window_size = ImGui::GetWindowSize();
                    if (StyleWindowIndex == 0)
                    {
                        // clip style
                        bool bEnabled = timeline->currentTime >= editing_clip->mStart && timeline->currentTime <= editing_clip->mEnd;
                        ImGui::BeginDisabled(editing_clip->mTrackStyle || !bEnabled);
                        force_update_preview |= edit_text_clip_style(draw_list, editing_clip, style_setting_window_size, default_size);
                        ImGui::EndDisabled();
                    }
                    else
                    {
                        // track style
                        force_update_preview |= edit_text_track_style(draw_list, editing_track, style_setting_window_size);
                    }
                }
                ImGui::EndChild();
            }
            ImGui::EndChild();
        }
        ImGui::PopStyleColor(4);
    }
    ImGui::EndChild();

    ImGui::SetCursorScreenPos(window_pos);
    ImRect video_rect;
    static bool MovingTextPos = false;
    static bool mouse_is_dragging = false;
    if (ImGui::BeginChild("##text_editor_preview", ImVec2(preview_view_width, preview_view_height), false, child_flags))
    {
        const float resize_handel_radius = 2;
        const ImVec2 handle_size(resize_handel_radius, resize_handel_radius);
        ShowMediaPreviewWindow(draw_list, "Text Preview", video_rect, editing_clip ? editing_clip->mStart : -1, editing_clip ? editing_clip->mEnd : -1, false, false, force_update_preview || MovingTextPos);
        // show test rect on preview view and add UI editor
        draw_list->PushClipRect(video_rect.Min, video_rect.Max);
        if (editing_clip && current_image.Valid() && timeline->currentTime >= editing_clip->mStart && timeline->currentTime <= editing_clip->mEnd)
        {
            ImVec2 text_pos_min = ImVec2(editing_clip->mFontPosX * video_rect.GetWidth(), editing_clip->mFontPosY * video_rect.GetHeight());
            ImVec2 text_pos_max = text_pos_min + ImVec2(default_size.x * video_rect.GetWidth(), default_size.y * video_rect.GetHeight());
            ImRect text_rect(video_rect.Min + text_pos_min, video_rect.Min + text_pos_max);
            ImRect text_lt_rect(text_rect.Min - handle_size, text_rect.Min + handle_size);
            ImRect text_rt_rect(text_rect.Min + ImVec2(text_rect.GetWidth(), 0) - handle_size, text_rect.Min + ImVec2(text_rect.GetWidth(), 0) + handle_size);
            ImRect text_tm_rect(text_rect.Min + ImVec2(text_rect.GetWidth() / 2, 0) - handle_size, text_rect.Min + ImVec2(text_rect.GetWidth() / 2, 0) + handle_size);
            ImRect text_lb_rect(text_rect.Min + ImVec2(0, text_rect.GetHeight()) - handle_size, text_rect.Min + ImVec2(0, text_rect.GetHeight()) + handle_size);
            ImRect text_rb_rect(text_rect.Max - handle_size, text_rect.Max + handle_size);
            ImRect text_bm_rect(text_rect.Min + ImVec2(text_rect.GetWidth() / 2, text_rect.GetHeight()) - handle_size, text_rect.Min + ImVec2(text_rect.GetWidth() / 2, text_rect.GetHeight()) + handle_size);
            ImRect text_lm_rect(text_rect.Min + ImVec2(0, text_rect.GetHeight() / 2) - handle_size, text_rect.Min + ImVec2(0, text_rect.GetHeight() / 2) + handle_size);
            ImRect text_rm_rect(text_rect.Min + ImVec2(text_rect.GetWidth(), text_rect.GetHeight() / 2) - handle_size, text_rect.Min + ImVec2(text_rect.GetWidth(), text_rect.GetHeight() / 2) + handle_size);
            if (!editing_clip->mTrackStyle)
            {
                draw_list->AddRect(text_rect.Min - ImVec2(1, 1), text_rect.Max + ImVec2(1, 1), IM_COL32_WHITE);
                draw_list->AddRect(text_lt_rect.Min, text_lt_rect.Max, IM_COL32_WHITE);
                draw_list->AddRect(text_rt_rect.Min, text_rt_rect.Max, IM_COL32_WHITE);
                draw_list->AddRect(text_tm_rect.Min, text_tm_rect.Max, IM_COL32_WHITE);
                draw_list->AddRect(text_lb_rect.Min, text_lb_rect.Max, IM_COL32_WHITE);
                draw_list->AddRect(text_rb_rect.Min, text_rb_rect.Max, IM_COL32_WHITE);
                draw_list->AddRect(text_bm_rect.Min, text_bm_rect.Max, IM_COL32_WHITE);
                draw_list->AddRect(text_lm_rect.Min, text_lm_rect.Max, IM_COL32_WHITE);
                draw_list->AddRect(text_rm_rect.Min, text_rm_rect.Max, IM_COL32_WHITE);
                if (text_rect.Contains(io.MousePos) || (mouse_is_dragging && MovingTextPos == 0))
                {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_None);
                    ImGui::RenderMouseCursor(io.MousePos, 0.5, ImGuiMouseCursor_ResizeAll, IM_COL32_WHITE, IM_COL32_BLACK, IM_COL32_DISABLE);
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !MovingTextPos)
                    {
                        MovingTextPos = true;
                    }
                }
                if (MovingTextPos && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                {
                    ImGui::CaptureMouseFromApp();
                    mouse_is_dragging = true;
                    editing_clip->mFontOffsetH += io.MouseDelta.x / video_rect.GetWidth();
                    editing_clip->mClipHolder->SetOffsetH(editing_clip->mFontOffsetH);
                    editing_clip->mFontOffsetV += io.MouseDelta.y / video_rect.GetHeight();
                    editing_clip->mClipHolder->SetOffsetV(editing_clip->mFontOffsetV);

                    // draw meters on video
                    draw_list->AddLine(video_rect.Min + ImVec2(video_rect.GetWidth() / 2, 0), video_rect.Min + ImVec2(video_rect.GetWidth() / 2, video_rect.GetHeight()), IM_COL32(128, 128, 128, 128));
                    draw_list->AddLine(video_rect.Min + ImVec2(0, video_rect.GetHeight() / 2), video_rect.Min + ImVec2(video_rect.GetWidth(), video_rect.GetHeight() / 2), IM_COL32(128, 128, 128, 128));
                }
            }
            else
            {
                draw_list->AddRect(text_rect.Min - ImVec2(1, 1), text_rect.Max + ImVec2(1, 1), IM_COL32(192,192,192,192));
            }
        }
        draw_list->PopClipRect();
    }
    ImGui::EndChild();

    // draw keypoint hidden button
    ImVec2 hidden_button_pos = window_pos + ImVec2(0, preview_view_height - 16);
    ImRect hidden_button_rect = ImRect(hidden_button_pos, hidden_button_pos + ImVec2(16, 16));
    ImGui::SetWindowFontScale(0.75);
    if (hidden_button_rect.Contains(ImGui::GetMousePos()))
    {
        draw_list->AddRectFilled(hidden_button_rect.Min, hidden_button_rect.Max, IM_COL32(64,64,64,255));
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            g_media_editor_settings.TextCurveExpanded = !g_media_editor_settings.TextCurveExpanded;
        }
        if (ImGui::BeginTooltip())
        {
            ImGui::TextUnformatted(g_media_editor_settings.TextCurveExpanded ? "Hide Curve View" : "Show Curve View");
            ImGui::EndTooltip();
        }
    }
    draw_list->AddText(hidden_button_pos, IM_COL32_WHITE, ICON_FA_BEZIER_CURVE);
    ImGui::SetWindowFontScale(1.0);

    // draw filter curve editor
    if (g_media_editor_settings.TextCurveExpanded)
    {
        ImGui::SetCursorScreenPos(text_keypoint_pos);
        if (ImGui::BeginChild("##text_keypoint", text_keypoint_size, false, child_flags))
        {
            ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
            ImVec2 sub_window_size = ImGui::GetWindowSize();
            draw_list->AddRectFilled(sub_window_pos, sub_window_pos + sub_window_size, COL_DARK_ONE);
            if (StyleWindowIndex == 0 && editing_clip)
            {
                bool _changed = false;
                float current_time = timeline->currentTime - editing_clip->mStart;
                auto keyPointsPtr = &editing_clip->mAttributeKeyPoints;
                mouse_hold |= ImGui::ImCurveEdit::Edit( nullptr,
                                                        keyPointsPtr,
                                                        sub_window_size, 
                                                        ImGui::GetID("##text_clip_keypoint_editor"), 
                                                        true,
                                                        current_time,
                                                        CURVE_EDIT_FLAG_VALUE_LIMITED | CURVE_EDIT_FLAG_MOVE_CURVE | CURVE_EDIT_FLAG_KEEP_BEGIN_END | CURVE_EDIT_FLAG_DOCK_BEGIN_END | CURVE_EDIT_FLAG_DRAW_TIMELINE, 
                                                        nullptr, // clippingRect
                                                        &_changed
                                                        );
                current_time += editing_clip->mStart;
                if ((int64_t)current_time != timeline->currentTime) { timeline->Seek(current_time); }
                if (_changed && editing_clip->mClipHolder) { editing_clip->mClipHolder->SetKeyPoints(editing_clip->mAttributeKeyPoints); timeline->UpdatePreview(); }
            }
            else if (StyleWindowIndex == 1 && editing_track)
            {
                bool _changed = false;
                float current_time = timeline->currentTime;
                auto keyPointsPtr = editing_track->mMttReader->GetKeyPoints();
                mouse_hold |= ImGui::ImCurveEdit::Edit( nullptr,
                                                        keyPointsPtr,
                                                        sub_window_size, 
                                                        ImGui::GetID("##text_track_keypoint_editor"), 
                                                        true,
                                                        current_time,
                                                        CURVE_EDIT_FLAG_VALUE_LIMITED | CURVE_EDIT_FLAG_MOVE_CURVE | CURVE_EDIT_FLAG_KEEP_BEGIN_END | CURVE_EDIT_FLAG_DOCK_BEGIN_END | CURVE_EDIT_FLAG_DRAW_TIMELINE, 
                                                        nullptr, // clippingRect
                                                        &_changed
                                                        );
                if ((int64_t)current_time != timeline->currentTime) { timeline->Seek(current_time); }
                if (_changed)
                {
                    timeline->UpdatePreview();
                    editing_track->mMttReader->Refresh();
                }
            }
        }
        ImGui::EndChild();
    }

    if (!editing_clip || ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        MovingTextPos = false;
        mouse_is_dragging = false;
        ImGui::CaptureMouseFromApp(false);
    }
}
/****************************************************************************************
 * 
 * Media Analyse windows
 *
 ***************************************************************************************/
static void ShowMediaScopeSetting(int index, bool show_tooltips = true)
{
    ImGui::BeginGroup();
    ImGui::PushItemWidth(200);
    switch (index)
    {
        case 0:
        {
            // histogram setting
            ImGui::TextUnformatted("Log:"); ImGui::SameLine();
            ImGui::ToggleButton("##histogram_logview", &g_media_editor_settings.HistogramLog);
            ImGui::TextUnformatted("Splited:"); ImGui::SameLine();
            ImGui::ToggleButton("##histogram_splited", &g_media_editor_settings.HistogramSplited);
            ImGui::TextUnformatted("YRGB:"); ImGui::SameLine();
            ImGui::ToggleButton("##histogram_yrgb", &g_media_editor_settings.HistogramYRGB);
            if (ImGui::DragFloat("Scale##histogram_scale", &g_media_editor_settings.HistogramScale, 0.01f, 0.002f, 4.f, "%.3f"))
                need_update_scope = true;
            if (show_tooltips)
            {
                ImGui::TextDisabled("%s", "Mouse wheel up/down on scope view also");
                ImGui::TextDisabled("%s", "Mouse left double click return default");
            }
        }
        break;
        case 1:
        {
            // waveform setting
            ImGui::TextUnformatted("Mirror:"); ImGui::SameLine();
            if (ImGui::ToggleButton("##waveform_mirror", &g_media_editor_settings.WaveformMirror))
                need_update_scope = true;
            ImGui::TextUnformatted("Separate:"); ImGui::SameLine();
            if (ImGui::ToggleButton("##waveform_separate", &g_media_editor_settings.WaveformSeparate))
                need_update_scope = true;
            ImGui::TextUnformatted("Show Y:"); ImGui::SameLine();
            if (ImGui::ToggleButton("##waveform_separate", &g_media_editor_settings.WaveformShowY))
                need_update_scope = true;
            if (ImGui::DragFloat("Intensity##WaveformIntensity", &g_media_editor_settings.WaveformIntensity, 0.05f, 0.f, 4.f, "%.1f"))
                need_update_scope = true;
            if (show_tooltips)
            {
                ImGui::TextDisabled("%s", "Mouse wheel up/down on scope view also");
                ImGui::TextDisabled("%s", "Mouse left double click return default");
            }
        }
        break;
        case 2:
        {
            // cie setting
            bool cie_setting_changed = false;
            ImGui::TextUnformatted("Show Color:"); ImGui::SameLine();
            if (ImGui::ToggleButton("##cie_show_color", &g_media_editor_settings.CIEShowColor))
            {
                need_update_scope = true;
            }
            ImGui::SameLine();
            ImGui::TextUnformatted("CorrectGamma:"); ImGui::SameLine();
            if (ImGui::ToggleButton("##cie_correct_gamma", &g_media_editor_settings.CIECorrectGamma))
            {
                cie_setting_changed = true;
            }
            if (ImGui::Combo("Color System", (int *)&g_media_editor_settings.CIEColorSystem, color_system_items, IM_ARRAYSIZE(color_system_items)))
            {
                cie_setting_changed = true;
            }
            if (ImGui::Combo("Cie System", (int *)&g_media_editor_settings.CIEMode, cie_system_items, IM_ARRAYSIZE(cie_system_items)))
            {
                cie_setting_changed = true;
            }
            if (ImGui::Combo("Show Gamut", (int *)&g_media_editor_settings.CIEGamuts, color_system_items, IM_ARRAYSIZE(color_system_items)))
            {
                cie_setting_changed = true;
            }
            if (ImGui::DragFloat("Contrast##cie_contrast", &g_media_editor_settings.CIEContrast, 0.01f, 0.f, 1.f, "%.2f"))
            {
                cie_setting_changed = true;
            }
#if IMGUI_VULKAN_SHADER
            if (cie_setting_changed && m_cie)
            {
                need_update_scope = true;
                m_cie->SetParam(g_media_editor_settings.CIEColorSystem, 
                                g_media_editor_settings.CIEMode, 512, 
                                g_media_editor_settings.CIEGamuts, 
                                g_media_editor_settings.CIEContrast, 
                                g_media_editor_settings.CIECorrectGamma);
            }
#endif
            if (ImGui::DragFloat("Intensity##CIEIntensity", &g_media_editor_settings.CIEIntensity, 0.01f, 0.f, 1.f, "%.2f"))
                need_update_scope = true;
            if (show_tooltips)
            {
                ImGui::TextDisabled("%s", "Mouse wheel up/down on scope view also");
                ImGui::TextDisabled("%s", "Mouse left double click return default");
            }
        }
        break;
        case 3:
        {
            // vector setting
            if (ImGui::DragFloat("Intensity##VectorIntensity", &g_media_editor_settings.VectorIntensity, 0.01f, 0.f, 1.f, "%.2f"))
                need_update_scope = true;
            if (show_tooltips)
            {
                ImGui::TextDisabled("%s", "Mouse wheel up/down on scope view also");
                ImGui::TextDisabled("%s", "Mouse left double click return default");
            }
        }
        break;
        case 4:
        {
            // audio wave setting
            ImGui::DragFloat("Scale##AudioWaveScale", &g_media_editor_settings.AudioWaveScale, 0.05f, 0.1f, 4.f, "%.1f");
            if (show_tooltips)
            {
                ImGui::TextDisabled("%s", "Mouse wheel up/down on scope view also");
                ImGui::TextDisabled("%s", "Mouse left double click return default");
            }
        }
        break;
        case 5:
        {
            // audio vector setting
            if (ImGui::DragFloat("Scale##AudioVectorScale", &g_media_editor_settings.AudioVectorScale, 0.05f, 0.5f, 4.f, "%.1f"))
            {
                timeline->mAudioAttribute.mAudioVectorScale = g_media_editor_settings.AudioVectorScale;
            }
            if (show_tooltips)
            {
                ImGui::TextDisabled("%s", "Mouse wheel up/down on scope view also");
                ImGui::TextDisabled("%s", "Mouse left double click return default");
            }
            if (ImGui::Combo("Vector Mode", (int *)&g_media_editor_settings.AudioVectorMode, audio_vector_mode_items, IM_ARRAYSIZE(audio_vector_mode_items)))
            {
                timeline->mAudioAttribute.mAudioVectorMode = g_media_editor_settings.AudioVectorMode;
            }
        }
        break;
        case 6:
        {
            // audio fft setting
            ImGui::DragFloat("Scale##AudioFFTScale", &g_media_editor_settings.AudioFFTScale, 0.05f, 0.1f, 4.f, "%.1f");
            if (show_tooltips)
            {
                ImGui::TextDisabled("%s", "Mouse wheel up/down on scope view also");
                ImGui::TextDisabled("%s", "Mouse left double click return default");
            }
        }
        break;
        case 7:
        {
            // audio dB setting
            ImGui::DragFloat("Scale##AudioDBScale", &g_media_editor_settings.AudioDBScale, 0.05f, 0.1f, 4.f, "%.1f");
            if (show_tooltips)
            {
                ImGui::TextDisabled("%s", "Mouse wheel up/down on scope view also");
                ImGui::TextDisabled("%s", "Mouse left double click return default");
            }
        }
        break;
        case 8:
        {
            // audio dB level setting
            ImGui::RadioButton("dB 20 Band##AudioDbLevelShort", &g_media_editor_settings.AudioDBLevelShort, 1);
            ImGui::RadioButton("dB 76 Band##AudioDbLevelLong", &g_media_editor_settings.AudioDBLevelShort, 0);
        }
        break;
        case 9:
        {
            // audio spectrogram setting
            if (ImGui::DragFloat("Offset##AudioSpectrogramOffset", &g_media_editor_settings.AudioSpectrogramOffset, 5.f, -96.f, 96.f, "%.1f"))
            {
                timeline->mAudioAttribute.mAudioSpectrogramOffset = g_media_editor_settings.AudioSpectrogramOffset;
            }
            if (ImGui::DragFloat("Lighting##AudioSpectrogramLight", &g_media_editor_settings.AudioSpectrogramLight, 0.01f, 0.f, 1.f, "%.2f"))
            {
                timeline->mAudioAttribute.mAudioSpectrogramLight = g_media_editor_settings.AudioSpectrogramLight;
            }
            if (show_tooltips)
            {
                ImGui::TextDisabled("%s", "Mouse wheel up/down on scope view for light");
                ImGui::TextDisabled("%s", "Mouse wheel left/right on scope view for offset");
                ImGui::TextDisabled("%s", "Mouse left double click return default");
            }
        }
        break;
        default: break;
    }
    ImGui::PopItemWidth();
    ImGui::EndGroup();
}

static void ShowMediaScopeView(int index, ImVec2 pos, ImVec2 size)
{
    ImGuiIO &io = ImGui::GetIO();
    ImGui::SetCursorScreenPos(pos);
    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    ImRect scrop_rect = ImRect(pos, pos + size);
    switch (index)
    {
        case 0:
        {
            // histogram view
            ImGui::BeginGroup();
            ImGui::InvisibleButton("##histogram_view", size);
            if (ImGui::IsItemHovered())
            {
                if (io.MouseWheel < -FLT_EPSILON)
                {
                    g_media_editor_settings.HistogramScale *= 0.9f;
                    if (g_media_editor_settings.HistogramScale < 0.002)
                        g_media_editor_settings.HistogramScale = 0.002;
                    need_update_scope = true;
                    if (ImGui::BeginTooltip())
                    {
                        ImGui::Text("Scale:%f", g_media_editor_settings.HistogramScale);
                        ImGui::EndTooltip();
                    }
                }
                else if (io.MouseWheel > FLT_EPSILON)
                {
                    g_media_editor_settings.HistogramScale *= 1.1f;
                    if (g_media_editor_settings.HistogramScale > 4.0f)
                        g_media_editor_settings.HistogramScale = 4.0;
                    need_update_scope = true;
                    if (ImGui::BeginTooltip())
                    {
                        ImGui::Text("Scale:%f", g_media_editor_settings.HistogramScale);
                        ImGui::EndTooltip();
                    }
                }
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    g_media_editor_settings.HistogramScale = g_media_editor_settings.HistogramLog ? 0.1 : 0.002f;
                    need_update_scope = true;
                }
            }
            if (!mat_histogram.empty())
            {

                ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
                ImGui::SetCursorScreenPos(pos);
                float height_scale = g_media_editor_settings.HistogramSplited ? g_media_editor_settings.HistogramYRGB ? 4.f : 3.f : 1.f;
                float height_offset = g_media_editor_settings.HistogramSplited ? g_media_editor_settings.HistogramYRGB ? size.y / 4.f : size.y / 3.f : 0;
                auto rmat = mat_histogram.channel(0);
                auto gmat = mat_histogram.channel(1);
                auto bmat = mat_histogram.channel(2);
                auto ymat = mat_histogram.channel(3);
                ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(1.f, 0.f, 0.f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(1.f, 0.f, 0.f, 0.3f));
                ImGui::PlotLinesEx("##rh", &((float *)rmat.data)[1], mat_histogram.w - 1, 0, nullptr, 0, g_media_editor_settings.HistogramLog ? 10 : 1000, ImVec2(size.x, size.y / height_scale), 4, false, true);
                ImGui::PopStyleColor(2);
                ImGui::SetCursorScreenPos(pos + ImVec2(0, height_offset));
                ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.f, 1.f, 0.f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.f, 1.f, 0.f, 0.3f));
                ImGui::PlotLinesEx("##gh", &((float *)gmat.data)[1], mat_histogram.w - 1, 0, nullptr, 0, g_media_editor_settings.HistogramLog ? 10 : 1000, ImVec2(size.x, size.y / height_scale), 4, false, true);
                ImGui::PopStyleColor(2);
                ImGui::SetCursorScreenPos(pos + ImVec2(0, height_offset * 2));
                ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.f, 0.f, 1.f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.f, 0.f, 1.f, 0.3f));
                ImGui::PlotLinesEx("##bh", &((float *)bmat.data)[1], mat_histogram.w - 1, 0, nullptr, 0, g_media_editor_settings.HistogramLog ? 10 : 1000, ImVec2(size.x, size.y / height_scale), 4, false, true);
                ImGui::PopStyleColor(2);
                if (g_media_editor_settings.HistogramYRGB)
                {
                    ImGui::SetCursorScreenPos(pos + ImVec2(0, height_offset * 3));
                    ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(1.f, 1.f, 1.f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(1.f, 1.f, 1.f, 0.3f));
                    ImGui::PlotLinesEx("##yh", &((float *)ymat.data)[1], mat_histogram.w - 1, 0, nullptr, 0, g_media_editor_settings.HistogramLog ? 10 : 1000, ImVec2(size.x, size.y / height_scale), 4, false, true);
                    ImGui::PopStyleColor(2);
                }
                ImGui::PopStyleColor();
            }
            draw_list->AddRect(scrop_rect.Min, scrop_rect.Max, COL_SLIDER_HANDLE, 0);
            // draw graticule line
            draw_list->PushClipRect(scrop_rect.Min, scrop_rect.Max);
            float graticule_scale = g_media_editor_settings.HistogramSplited ? g_media_editor_settings.HistogramYRGB ? 4.0f : 3.f : 1.f;
            auto histogram_step = size.x / 10;
            auto histogram_sub_vstep = size.x / 50;
            auto histogram_vstep = size.y * g_media_editor_settings.HistogramScale * 10 / graticule_scale;
            auto histogram_seg = size.y / histogram_vstep / graticule_scale;
            for (int i = 1; i <= 10; i++)
            {
                ImVec2 p0 = scrop_rect.Min + ImVec2(i * histogram_step, 0);
                ImVec2 p1 = scrop_rect.Min + ImVec2(i * histogram_step, scrop_rect.Max.y);
                draw_list->AddLine(p0, p1, COL_GRATICULE_DARK, 1);
            }
            for (int i = 0; i < histogram_seg; i++)
            {
                ImVec2 pr0 = scrop_rect.Min + ImVec2(0, (size.y / graticule_scale) - i * histogram_vstep);
                ImVec2 pr1 = scrop_rect.Min + ImVec2(scrop_rect.Max.x, (size.y / graticule_scale) - i * histogram_vstep);
                draw_list->AddLine(pr0, pr1, g_media_editor_settings.HistogramSplited ? IM_COL32(255, 128, 0, 32) : COL_GRATICULE_DARK, 1);
                if (g_media_editor_settings.HistogramSplited)
                {
                    ImVec2 pg0 = scrop_rect.Min + ImVec2(0, size.y / graticule_scale) + ImVec2(0, (size.y / graticule_scale) - i * histogram_vstep);
                    ImVec2 pg1 = scrop_rect.Min + ImVec2(0, size.y / graticule_scale) + ImVec2(scrop_rect.Max.x, (size.y / graticule_scale) - i * histogram_vstep);
                    draw_list->AddLine(pg0, pg1, IM_COL32(128, 255, 0, 32), 1);
                    ImVec2 pb0 = scrop_rect.Min + ImVec2(0, size.y * 2 / graticule_scale) + ImVec2(0, (size.y / graticule_scale) - i * histogram_vstep);
                    ImVec2 pb1 = scrop_rect.Min + ImVec2(0, size.y * 2 / graticule_scale) + ImVec2(scrop_rect.Max.x, (size.y / graticule_scale) - i * histogram_vstep);
                    draw_list->AddLine(pb0, pb1, IM_COL32(128, 128, 255, 32), 1);
                }
            }
            for (int i = 0; i < 50; i++)
            {
                ImVec2 p0 = scrop_rect.Min + ImVec2(i * histogram_sub_vstep, 0);
                ImVec2 p1 = scrop_rect.Min + ImVec2(i * histogram_sub_vstep, 5);
                draw_list->AddLine(p0, p1, COL_GRATICULE, 1);
            }
            draw_list->PopClipRect();
            ImGui::EndGroup();
        }
        break;
        case 1:
        {
            // waveform view
            ImGui::BeginGroup();
            ImGui::InvisibleButton("##waveform_view", size);
            if (ImGui::IsItemHovered())
            {
                if (io.MouseWheel < -FLT_EPSILON)
                {
                    g_media_editor_settings.WaveformIntensity *= 0.9f;
                    if (g_media_editor_settings.WaveformIntensity < 0.1)
                        g_media_editor_settings.WaveformIntensity = 0.1;
                    need_update_scope = true;
                    if (ImGui::BeginTooltip())
                    {
                        ImGui::Text("Intensity:%f", g_media_editor_settings.WaveformIntensity);
                        ImGui::EndTooltip();
                    }
                }
                else if (io.MouseWheel > FLT_EPSILON)
                {
                    g_media_editor_settings.WaveformIntensity *= 1.1f;
                    if (g_media_editor_settings.WaveformIntensity > 4.0f)
                        g_media_editor_settings.WaveformIntensity = 4.0;
                    need_update_scope = true;
                    if (ImGui::BeginTooltip())
                    {
                        ImGui::Text("Intensity:%f", g_media_editor_settings.WaveformIntensity);
                        ImGui::EndTooltip();
                    }
                }
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    g_media_editor_settings.WaveformIntensity = 2.0;
                    need_update_scope = true;
                }
            }
            if (!mat_waveform.empty())
            {
                ImGui::ImMatToTexture(mat_waveform, waveform_texture);
                draw_list->AddImage(waveform_texture, scrop_rect.Min, scrop_rect.Max, g_media_editor_settings.WaveformMirror ? ImVec2(0, 1) : ImVec2(0, 0), g_media_editor_settings.WaveformMirror ? ImVec2(1, 0) : ImVec2(1, 1));
            }
            draw_list->AddRect(scrop_rect.Min, scrop_rect.Max, COL_SLIDER_HANDLE, 0);
            // draw graticule line
            draw_list->PushClipRect(scrop_rect.Min, scrop_rect.Max);
            auto waveform_step = size.y / 10;
            auto waveform_vstep = size.x / 10;
            auto waveform_sub_step = size.y / 50;
            auto waveform_sub_vstep = size.x / 100;
            for (int i = 0; i < 10; i++)
            {
                ImVec2 p0 = scrop_rect.Min + ImVec2(0, i * waveform_step);
                ImVec2 p1 = scrop_rect.Min + ImVec2(scrop_rect.Max.x, i * waveform_step);
                if (i != 5)
                    draw_list->AddLine(p0, p1, COL_GRATICULE_DARK, 1);
                else
                {
                    ImGui::ImDrawListAddLineDashed(draw_list, p0, p1, COL_GRATICULE_DARK, 1, 100);
                }
                ImVec2 vp0 = scrop_rect.Min + ImVec2(i * waveform_vstep, 0);
                ImVec2 vp1 = scrop_rect.Min + ImVec2(i * waveform_vstep, 10);
                draw_list->AddLine(vp0, vp1, COL_GRATICULE, 1);
            }
            for (int i = 0; i < 50; i++)
            {
                float l = i == 0 || i % 10 == 0 ? 10 : 5;
                ImVec2 p0 = scrop_rect.Min + ImVec2(0, i * waveform_sub_step);
                ImVec2 p1 = scrop_rect.Min + ImVec2(l, i * waveform_sub_step);
                draw_list->AddLine(p0, p1, COL_GRATICULE, 1);
            }
            for (int i = 0; i < 100; i++)
            {
                ImVec2 p0 = scrop_rect.Min + ImVec2(i * waveform_sub_vstep, 0);
                ImVec2 p1 = scrop_rect.Min + ImVec2(i * waveform_sub_vstep, 5);
                draw_list->AddLine(p0, p1, COL_GRATICULE, 1);
            }
            draw_list->PopClipRect();
            ImGui::EndGroup();
        }
        break;
        case 2:
        {
            // cie view
#if IMGUI_VULKAN_SHADER
            ImGui::BeginGroup();
            ImGui::InvisibleButton("##cie_view", size);
            if (ImGui::IsItemHovered())
            {
                if (io.MouseWheel < -FLT_EPSILON)
                {
                    g_media_editor_settings.CIEIntensity *= 0.9f;
                    if (g_media_editor_settings.CIEIntensity < 0.01)
                        g_media_editor_settings.CIEIntensity = 0.01;
                    need_update_scope = true;
                    if (ImGui::BeginTooltip())
                    {
                        ImGui::Text("Intensity:%f", g_media_editor_settings.CIEIntensity);
                        ImGui::EndTooltip();
                    }
                }
                else if (io.MouseWheel > FLT_EPSILON)
                {
                    g_media_editor_settings.CIEIntensity *= 1.1f;
                    if (g_media_editor_settings.CIEIntensity > 1.0f)
                        g_media_editor_settings.CIEIntensity = 1.0;
                    need_update_scope = true;
                    if (ImGui::BeginTooltip())
                    {
                        ImGui::Text("Intensity:%f", g_media_editor_settings.CIEIntensity);
                        ImGui::EndTooltip();
                    }
                }
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    g_media_editor_settings.CIEIntensity = 0.5f;
                    need_update_scope = true;
                }
            }
            if (!mat_cie.empty())
            {
                ImGui::ImMatToTexture(mat_cie, cie_texture);
                draw_list->AddImage(cie_texture, scrop_rect.Min, scrop_rect.Max, ImVec2(0, 0), ImVec2(1, 1));
            }
            draw_list->AddRect(scrop_rect.Min, scrop_rect.Max, COL_SLIDER_HANDLE, 0);
            // draw graticule line
            draw_list->PushClipRect(scrop_rect.Min, scrop_rect.Max);
            auto cie_step = size.y / 10;
            auto cie_vstep = size.x / 10;
            auto cie_sub_step = size.y / 50;
            auto cie_sub_vstep = size.x / 50;
            for (int i = 1; i <= 10; i++)
            {
                ImVec2 hp0 = scrop_rect.Min + ImVec2(0, i * cie_step);
                ImVec2 hp1 = scrop_rect.Min + ImVec2(scrop_rect.Max.x, i * cie_step);
                draw_list->AddLine(hp0, hp1, COL_GRATICULE_DARK, 1);
                ImVec2 vp0 = scrop_rect.Min + ImVec2(i * cie_vstep, 0);
                ImVec2 vp1 = scrop_rect.Min + ImVec2(i * cie_vstep, scrop_rect.Max.y);
                draw_list->AddLine(vp0, vp1, COL_GRATICULE_DARK, 1);
            }
            for (int i = 0; i < 50; i++)
            {
                ImVec2 hp0 = scrop_rect.Min + ImVec2(size.x - 3, i * cie_sub_step);
                ImVec2 hp1 = scrop_rect.Min + ImVec2(size.x, i * cie_sub_step);
                draw_list->AddLine(hp0, hp1, COL_GRATICULE_HALF, 1);
                ImVec2 vp0 = scrop_rect.Min + ImVec2(i * cie_sub_vstep, 0);
                ImVec2 vp1 = scrop_rect.Min + ImVec2(i * cie_sub_vstep, 3);
                draw_list->AddLine(vp0, vp1, COL_GRATICULE_HALF, 1);
            }
            std::string X_str = "X";
            std::string Y_str = "Y";
            if (g_media_editor_settings.CIEMode == ImGui::UCS)
            {
                X_str = "U"; Y_str = "C";
            }
            else if (g_media_editor_settings.CIEMode == ImGui::LUV)
            {
                X_str = "U"; Y_str = "V";
            }
            draw_list->AddText(scrop_rect.Min + ImVec2(2, 2), COL_GRATICULE, X_str.c_str());
            draw_list->AddText(scrop_rect.Min + ImVec2(size.x - 12, size.y - 18), COL_GRATICULE, Y_str.c_str());
            ImGui::SetWindowFontScale(0.7);
            for (int i = 0; i < 10; i++)
            {
                if (i == 0) continue;
                char mark[32] = {0};
                ImFormatString(mark, IM_ARRAYSIZE(mark), "%.1f", i / 10.f);
                draw_list->AddText(scrop_rect.Min + ImVec2(i * cie_vstep - 8, 2), COL_GRATICULE, mark);
                draw_list->AddText(scrop_rect.Min + ImVec2(size.x - 18, size.y - i * cie_step - 6), COL_GRATICULE, mark);
            }
            ImGui::SetWindowFontScale(1.0);
            ImGui::PushStyleVar(ImGuiStyleVar_TexGlyphShadowOffset, ImVec2(1, 1));
            if (m_cie)
            {
                ImVec2 white_point;
                m_cie->GetWhitePoint((ImGui::ColorsSystems)g_media_editor_settings.CIEColorSystem, size.x, size.y, &white_point.x, &white_point.y);
                draw_list->AddCircle(scrop_rect.Min + white_point, 3, IM_COL32_WHITE, 0, 2);
                draw_list->AddCircle(scrop_rect.Min + white_point, 2, IM_COL32_BLACK, 0, 1);
                ImVec2 green_point_system;
                m_cie->GetGreenPoint((ImGui::ColorsSystems)g_media_editor_settings.CIEColorSystem, size.x, size.y, &green_point_system.x, &green_point_system.y);
                draw_list->AddText(scrop_rect.Min + green_point_system, COL_GRATICULE, color_system_items[g_media_editor_settings.CIEColorSystem]);
                ImVec2 green_point_gamuts;
                m_cie->GetGreenPoint((ImGui::ColorsSystems)g_media_editor_settings.CIEGamuts, size.x, size.y, &green_point_gamuts.x, &green_point_gamuts.y);
                draw_list->AddText(scrop_rect.Min + green_point_gamuts, COL_GRATICULE, color_system_items[g_media_editor_settings.CIEGamuts]);
            }
            ImGui::PopStyleVar();
            draw_list->PopClipRect();
            ImGui::EndGroup();
#endif
        }
        break;
        case 3:
        {
            // vector view
            ImGui::BeginGroup();
            ImGui::InvisibleButton("##vector_view", size);
            if (ImGui::IsItemHovered())
            {
                if (io.MouseWheel < -FLT_EPSILON)
                {
                    g_media_editor_settings.VectorIntensity *= 0.9f;
                    if (g_media_editor_settings.VectorIntensity < 0.01)
                        g_media_editor_settings.VectorIntensity = 0.01;
                    need_update_scope = true;
                    if (ImGui::BeginTooltip())
                    {
                        ImGui::Text("Intensity:%f", g_media_editor_settings.VectorIntensity);
                        ImGui::EndTooltip();
                    }
                }
                else if (io.MouseWheel > FLT_EPSILON)
                {
                    g_media_editor_settings.VectorIntensity *= 1.1f;
                    if (g_media_editor_settings.VectorIntensity > 1.0f)
                        g_media_editor_settings.VectorIntensity = 1.0;
                    need_update_scope = true;
                    if (ImGui::BeginTooltip())
                    {
                        ImGui::Text("Intensity:%f", g_media_editor_settings.VectorIntensity);
                        ImGui::EndTooltip();
                    }
                }
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    g_media_editor_settings.VectorIntensity = 0.5f;
                    need_update_scope = true;
                }
            }
            if (!mat_vector.empty())
            {
                ImGui::ImMatToTexture(mat_vector, vector_texture);
                draw_list->AddImage(vector_texture, scrop_rect.Min, scrop_rect.Max, ImVec2(0, 0), ImVec2(1, 1));
            }
            draw_list->AddRect(scrop_rect.Min, scrop_rect.Max, COL_SLIDER_HANDLE, 0);
            // draw graticule line
            ImVec2 center_point = ImVec2(scrop_rect.Min + size / 2);
            float radius = size.x / 2;
            draw_list->PushClipRect(scrop_rect.Min, scrop_rect.Max);
            draw_list->AddCircle(center_point, radius, COL_GRATICULE_DARK, 0, 1);
            draw_list->AddLine(scrop_rect.Min + ImVec2(0, size.y / 2), scrop_rect.Max - ImVec2(0, size.y / 2), COL_GRATICULE_DARK);
            draw_list->AddLine(scrop_rect.Min + ImVec2(size.x / 2, 0), scrop_rect.Max - ImVec2(size.x / 2, 0), COL_GRATICULE_DARK);

            auto AngleToCoordinate = [&](float angle, float length)
            {
                ImVec2 point(0, 0);
                float hAngle = angle * M_PI / 180.f;
                if (angle == 0.f)
                    point = ImVec2(length, 0);  // positive x axis
                else if (angle == 180.f)
                    point = ImVec2(-length, 0); // negative x axis
                else if (angle == 90.f)
                    point = ImVec2(0, length); // positive y axis
                else if (angle == 270.f)
                    point = ImVec2(0, -length);  // negative y axis
                else
                    point = ImVec2(length * cos(hAngle), length * sin(hAngle));
                return point;
            };
            auto AngleToPoint = [&](float angle, float length)
            {
                ImVec2 point = AngleToCoordinate(angle, length);
                point = ImVec2(point.x * radius, -point.y * radius);
                return point;
            };
            auto ColorToPoint = [&](float r, float g, float b)
            {
                float angle, length, v;
                ImGui::ColorConvertRGBtoHSV(r, g, b, angle, length, v);
                angle = angle * 360;
                auto point = AngleToCoordinate(angle, v);
                point = ImVec2(point.x * radius, -point.y * radius);
                return point;
            };

            for (int i = 0; i < 360; i+= 5)
            {
                float l = 0.95;
                if (i == 0 || i % 10 == 0)
                    l = 0.9;
                auto p0 = AngleToPoint(i, 1.0);
                auto p1 = AngleToPoint(i, l);
                draw_list->AddLine(center_point + p0, center_point + p1, COL_GRATICULE_DARK);
            }

              auto draw_mark = [&](ImVec2 point, const char * mark)
            {
                float rect_size = 12;
                auto p0 = center_point + point - ImVec2(rect_size, rect_size);
                auto p1 = center_point + point + ImVec2(rect_size, rect_size);
                auto p2 = p0 + ImVec2(rect_size * 2, 0);
                auto p3 = p0 + ImVec2(0, rect_size * 2);
                auto text_size = ImGui::CalcTextSize(mark);
                draw_list->AddText(p0 + ImVec2(rect_size - text_size.x / 2, rect_size - text_size.y / 2), COL_GRATICULE, mark);
                draw_list->AddLine(p0, p0 + ImVec2(5, 0),   COL_GRATICULE, 2);
                draw_list->AddLine(p0, p0 + ImVec2(0, 5),   COL_GRATICULE, 2);
                draw_list->AddLine(p1, p1 + ImVec2(-5, 0),  COL_GRATICULE, 2);
                draw_list->AddLine(p1, p1 + ImVec2(0, -5),  COL_GRATICULE, 2);
                draw_list->AddLine(p2, p2 + ImVec2(-5, 0),  COL_GRATICULE, 2);
                draw_list->AddLine(p2, p2 + ImVec2(0, 5),   COL_GRATICULE, 2);
                draw_list->AddLine(p3, p3 + ImVec2(5, 0),   COL_GRATICULE, 2);
                draw_list->AddLine(p3, p3 + ImVec2(0, -5),  COL_GRATICULE, 2);
            };

            draw_mark(ColorToPoint(0.75, 0, 0), "R");
            draw_mark(ColorToPoint(0, 0.75, 0), "G");
            draw_mark(ColorToPoint(0, 0, 0.75), "B");
            draw_mark(ColorToPoint(0.75, 0.75, 0), "Y");
            draw_mark(ColorToPoint(0.75, 0, 0.75), "M");
            draw_mark(ColorToPoint(0, 0.75, 0.75), "C");

            draw_list->PopClipRect();
            ImGui::EndGroup();
        }
        break;
        case 4:
        {
            // wave view
            timeline->mAudioAttribute.audio_mutex.lock();
            ImGui::BeginGroup();
            ImGui::InvisibleButton("##audio_wave_view", size);
            if (ImGui::IsItemHovered())
            {
                if (io.MouseWheel < -FLT_EPSILON)
                {
                    g_media_editor_settings.AudioWaveScale *= 0.9f;
                    if (g_media_editor_settings.AudioWaveScale < 0.1)
                        g_media_editor_settings.AudioWaveScale = 0.1;
                    if (ImGui::BeginTooltip())
                    {
                        ImGui::Text("Scale:%f", g_media_editor_settings.AudioWaveScale);
                        ImGui::EndTooltip();
                    }
                }
                else if (io.MouseWheel > FLT_EPSILON)
                {
                    g_media_editor_settings.AudioWaveScale *= 1.1f;
                    if (g_media_editor_settings.AudioWaveScale > 4.0f)
                        g_media_editor_settings.AudioWaveScale = 4.0;
                    if (ImGui::BeginTooltip())
                    {
                        ImGui::Text("Scale:%f", g_media_editor_settings.AudioWaveScale);
                        ImGui::EndTooltip();
                    }
                }
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    g_media_editor_settings.AudioWaveScale = 1.f;
            }
            draw_list->AddRect(scrop_rect.Min, scrop_rect.Max, COL_SLIDER_HANDLE, 0);
            draw_list->PushClipRect(scrop_rect.Min, scrop_rect.Max);
            ImVec2 channel_view_size = ImVec2(size.x, size.y / timeline->mAudioAttribute.channel_data.size());
            ImGui::SetCursorScreenPos(pos);
            ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.f, 1.f, 0.f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            for (int i = 0; i < timeline->mAudioAttribute.channel_data.size(); i++)
            {
                ImVec2 channel_min = pos + ImVec2(0, channel_view_size.y * i);
                ImVec2 channel_max = pos + ImVec2(channel_view_size.x, channel_view_size.y * i);
                // draw graticule line
                // draw middle line
                ImVec2 p1 = ImVec2(pos.x, pos.y + channel_view_size.y * i + channel_view_size.y / 2);
                ImVec2 p2 = p1 + ImVec2(channel_view_size.x, 0);
                draw_list->AddLine(p1, p2, IM_COL32(128, 128, 128, 128));
                // draw grid
                auto grid_number = floor(10 / g_media_editor_settings.AudioWaveScale);
                auto grid_height = channel_view_size.y / 2 / grid_number;
                if (grid_number > 10) grid_number = 10;
                for (int x = 0; x < grid_number; x++)
                {
                    ImVec2 gp1 = p1 - ImVec2(0, grid_height * x);
                    ImVec2 gp2 = gp1 + ImVec2(channel_view_size.x, 0);
                    draw_list->AddLine(gp1, gp2, COL_GRAY_GRATICULE);
                    ImVec2 gp3 = p1 + ImVec2(0, grid_height * x);
                    ImVec2 gp4 = gp3 + ImVec2(channel_view_size.x, 0);
                    draw_list->AddLine(gp3, gp4, COL_GRAY_GRATICULE);
                }
                auto vgrid_number = channel_view_size.x / grid_height;
                for (int x = 0; x < vgrid_number; x++)
                {
                    ImVec2 gp1 = p1 + ImVec2(grid_height * x, 0);
                    ImVec2 gp2 = gp1 - ImVec2(0, grid_height * (grid_number - 1));
                    draw_list->AddLine(gp1, gp2, COL_GRAY_GRATICULE);
                    ImVec2 gp3 = gp1 + ImVec2(0, grid_height * (grid_number - 1));
                    draw_list->AddLine(gp1, gp3, COL_GRAY_GRATICULE);
                }
                if (!timeline->mAudioAttribute.channel_data[i].m_wave.empty())
                {
                    ImGui::PushID(i);
                    ImGui::PlotLinesEx("##wave", (float *)timeline->mAudioAttribute.channel_data[i].m_wave.data, timeline->mAudioAttribute.channel_data[i].m_wave.w, 0, nullptr, -1.0 / g_media_editor_settings.AudioWaveScale , 1.0 / g_media_editor_settings.AudioWaveScale, channel_view_size, 4, false, false);
                    ImGui::PopID();
                }
                draw_list->AddRect(channel_min, channel_max, COL_SLIDER_HANDLE, 0);
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
            draw_list->PopClipRect();
            ImGui::EndGroup();
            timeline->mAudioAttribute.audio_mutex.unlock();
        }
        break;
        case 5:
        {
            char mark[32] = {0};
            // audio vector
            timeline->mAudioAttribute.audio_mutex.lock();
            ImGui::BeginGroup();
            ImGui::InvisibleButton("##audio_vector_view", size);
            if (ImGui::IsItemHovered())
            {
                if (io.MouseWheel < -FLT_EPSILON)
                {
                    g_media_editor_settings.AudioVectorScale *= 0.9f;
                    if (g_media_editor_settings.AudioVectorScale < 0.5)
                        g_media_editor_settings.AudioVectorScale = 0.5;
                    timeline->mAudioAttribute.mAudioVectorScale = g_media_editor_settings.AudioVectorScale;
                    if (ImGui::BeginTooltip())
                    {
                        ImGui::Text("Zoom:%f", g_media_editor_settings.AudioVectorScale);
                        ImGui::EndTooltip();
                    }
                }
                else if (io.MouseWheel > FLT_EPSILON)
                {
                    g_media_editor_settings.AudioVectorScale *= 1.1f;
                    if (g_media_editor_settings.AudioVectorScale > 4.0f)
                        g_media_editor_settings.AudioVectorScale = 4.0;
                    timeline->mAudioAttribute.mAudioVectorScale = g_media_editor_settings.AudioVectorScale;
                    if (ImGui::BeginTooltip())
                    {
                        ImGui::Text("Zoom:%f", g_media_editor_settings.AudioVectorScale);
                        ImGui::EndTooltip();
                    }
                }
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    g_media_editor_settings.AudioVectorScale = 1.f;
                    timeline->mAudioAttribute.mAudioVectorScale = g_media_editor_settings.AudioVectorScale;
                }
            }
            draw_list->AddRect(scrop_rect.Min, scrop_rect.Max, COL_SLIDER_HANDLE, 0);
            draw_list->PushClipRect(scrop_rect.Min, scrop_rect.Max);
            if (!timeline->mAudioAttribute.m_audio_vector.empty())
            {
                ImGui::ImMatToTexture(timeline->mAudioAttribute.m_audio_vector, timeline->mAudioAttribute.m_audio_vector_texture);
                draw_list->AddImage(timeline->mAudioAttribute.m_audio_vector_texture, scrop_rect.Min, scrop_rect.Max, ImVec2(0, 0), ImVec2(1, 1));
            }
            // draw graticule line
            ImVec2 center_point = ImVec2(scrop_rect.Min + size / 2);
            float radius = size.x / 2;
            draw_list->AddCircle(center_point, radius, COL_GRATICULE_DARK, 0, 1);
            draw_list->AddLine(scrop_rect.Min + ImVec2(0, size.y / 2), scrop_rect.Max - ImVec2(0, size.y / 2), COL_GRATICULE_DARK);
            draw_list->AddLine(scrop_rect.Min + ImVec2(size.x / 2, 0), scrop_rect.Max - ImVec2(size.x / 2, 0), COL_GRATICULE_DARK);
            ImGui::SetWindowFontScale(0.6);
            float mark_point = size.x / 2 * g_media_editor_settings.AudioVectorScale;
            float mark_val = 1.0;
            ImVec2 mark_pos = center_point;
            while (mark_point >= size.x / 2) { mark_point /= 2; mark_val /= 2; }
            mark_pos = center_point + ImVec2(- mark_point, 0);
            ImFormatString(mark, IM_ARRAYSIZE(mark), mark_val >= 0.5 ? "%-4.1f" : mark_val >= 0.25 ? "%-4.2f" : "%-4.3f", mark_val);
            draw_list->AddLine(mark_pos + ImVec2(0, -4), mark_pos + ImVec2(0, 4), COL_GRATICULE_DARK, 2);
            draw_list->AddText(mark_pos + ImVec2(-8, -12), COL_GRATICULE, mark);
            mark_pos = center_point + ImVec2(mark_point, 0);
            draw_list->AddLine(mark_pos + ImVec2(0, -4), mark_pos + ImVec2(0, 4), COL_GRATICULE_DARK, 2);
            draw_list->AddText(mark_pos + ImVec2(-8, -12), COL_GRATICULE, mark);
            mark_pos = center_point + ImVec2(0, -mark_point);
            draw_list->AddLine(mark_pos + ImVec2(-4, 0), mark_pos + ImVec2(4, 0), COL_GRATICULE_DARK, 2);
            draw_list->AddText(mark_pos + ImVec2(-8, -10), COL_GRATICULE, mark);
            mark_pos = center_point + ImVec2(0, mark_point);
            draw_list->AddLine(mark_pos + ImVec2(-4, 0), mark_pos + ImVec2(4, 0), COL_GRATICULE_DARK, 2);
            draw_list->AddText(mark_pos + ImVec2(-8, 0), COL_GRATICULE, mark);
            mark_point /= 2;
            mark_val /= 2;
            ImFormatString(mark, IM_ARRAYSIZE(mark), mark_val >= 0.5 ? "%-4.1f" : mark_val >= 0.25 ? "%-4.2f" : "%-4.3f", mark_val);
            mark_pos = center_point + ImVec2(- mark_point, 0);
            draw_list->AddLine(mark_pos + ImVec2(0, -4), mark_pos + ImVec2(0, 4), COL_GRATICULE_DARK, 2);
            draw_list->AddText(mark_pos + ImVec2(-8, -12), COL_GRATICULE, mark);
            mark_pos = center_point + ImVec2(mark_point, 0);
            draw_list->AddLine(mark_pos + ImVec2(0, -4), mark_pos + ImVec2(0, 4), COL_GRATICULE_DARK, 2);
            draw_list->AddText(mark_pos + ImVec2(-8, -12), COL_GRATICULE, mark);
            mark_pos = center_point + ImVec2(0, -mark_point);
            draw_list->AddLine(mark_pos + ImVec2(-4, 0), mark_pos + ImVec2(4, 0), COL_GRATICULE_DARK, 2);
            draw_list->AddText(mark_pos + ImVec2(-8, -10), COL_GRATICULE, mark);
            mark_pos = center_point + ImVec2(0, mark_point);
            draw_list->AddLine(mark_pos + ImVec2(-4, 0), mark_pos + ImVec2(4, 0), COL_GRATICULE_DARK, 2);
            draw_list->AddText(mark_pos + ImVec2(-8, 0), COL_GRATICULE, mark);
            ImGui::SetWindowFontScale(1.0);
            draw_list->PopClipRect();
            ImGui::EndGroup();
            timeline->mAudioAttribute.audio_mutex.unlock();
        }
        break;
        case 6:
        {
            // fft view
            timeline->mAudioAttribute.audio_mutex.lock();
            ImGui::BeginGroup();
            ImGui::InvisibleButton("##audio_fft_view", size);
            if (ImGui::IsItemHovered())
            {
                if (io.MouseWheel < -FLT_EPSILON)
                {
                    g_media_editor_settings.AudioFFTScale *= 0.9f;
                    if (g_media_editor_settings.AudioFFTScale < 0.1)
                        g_media_editor_settings.AudioFFTScale = 0.1;
                    if (ImGui::BeginTooltip())
                    {
                        ImGui::Text("Scale:%f", g_media_editor_settings.AudioFFTScale);
                        ImGui::EndTooltip();
                    }
                }
                else if (io.MouseWheel > FLT_EPSILON)
                {
                    g_media_editor_settings.AudioFFTScale *= 1.1f;
                    if (g_media_editor_settings.AudioFFTScale > 4.0f)
                        g_media_editor_settings.AudioFFTScale = 4.0;
                    if (ImGui::BeginTooltip())
                    {
                        ImGui::Text("Scale:%f", g_media_editor_settings.AudioFFTScale);
                        ImGui::EndTooltip();
                    }
                }
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    g_media_editor_settings.AudioFFTScale = 1.f;
            }
            draw_list->AddRect(scrop_rect.Min, scrop_rect.Max, COL_SLIDER_HANDLE, 0);
            draw_list->PushClipRect(scrop_rect.Min, scrop_rect.Max);
            ImVec2 channel_view_size = ImVec2(size.x, size.y / timeline->mAudioAttribute.channel_data.size());
            ImGui::SetCursorScreenPos(pos);
            ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(1.f, 1.f, 1.f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            for (int i = 0; i < timeline->mAudioAttribute.channel_data.size(); i++)
            {
                ImVec2 channel_min = pos + ImVec2(0, channel_view_size.y * i);
                ImVec2 channel_max = pos + ImVec2(channel_view_size.x, channel_view_size.y * i);
                // draw graticule line
                ImVec2 p1 = ImVec2(pos.x, pos.y + channel_view_size.y * i + channel_view_size.y);
                auto grid_number = floor(10 / g_media_editor_settings.AudioFFTScale);
                auto grid_height = channel_view_size.y / grid_number;
                if (grid_number > 20) grid_number = 20;
                for (int x = 0; x < grid_number; x++)
                {
                    ImVec2 gp1 = p1 - ImVec2(0, grid_height * x);
                    ImVec2 gp2 = gp1 + ImVec2(channel_view_size.x, 0);
                    draw_list->AddLine(gp1, gp2, COL_GRAY_GRATICULE);
                }
                auto vgrid_number = channel_view_size.x / grid_height;
                for (int x = 0; x < vgrid_number; x++)
                {
                    ImVec2 gp1 = p1 + ImVec2(grid_height * x, 0);
                    ImVec2 gp2 = gp1 - ImVec2(0, grid_height * (grid_number - 1));
                    draw_list->AddLine(gp1, gp2, COL_GRAY_GRATICULE);
                }
                if (!timeline->mAudioAttribute.channel_data[i].m_fft.empty())
                {
                    ImGui::PushID(i);
                    ImGui::PlotLinesEx("##fft", (float *)timeline->mAudioAttribute.channel_data[i].m_fft.data, timeline->mAudioAttribute.channel_data[i].m_fft.w, 0, nullptr, 0.0, 1.0 / g_media_editor_settings.AudioFFTScale, channel_view_size, 4, false, true);
                    ImGui::PopID();
                }
                draw_list->AddRect(channel_min, channel_max, COL_SLIDER_HANDLE, 0);
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);
            draw_list->PopClipRect();
            ImGui::EndGroup();
            timeline->mAudioAttribute.audio_mutex.unlock();
        }
        break;
        case 7:
        {
            // db view
            timeline->mAudioAttribute.audio_mutex.lock();
            ImGui::BeginGroup();
            ImGui::InvisibleButton("##audio_db_view", size);
            if (ImGui::IsItemHovered())
            {
                if (io.MouseWheel < -FLT_EPSILON)
                {
                    g_media_editor_settings.AudioDBScale *= 0.9f;
                    if (g_media_editor_settings.AudioDBScale < 0.1)
                        g_media_editor_settings.AudioDBScale = 0.1;
                    if (ImGui::BeginTooltip())
                    {
                        ImGui::Text("Scale:%f", g_media_editor_settings.AudioDBScale);
                        ImGui::EndTooltip();
                    }
                }
                else if (io.MouseWheel > FLT_EPSILON)
                {
                    g_media_editor_settings.AudioDBScale *= 1.1f;
                    if (g_media_editor_settings.AudioDBScale > 4.0f)
                        g_media_editor_settings.AudioDBScale = 4.0;
                    if (ImGui::BeginTooltip())
                    {
                        ImGui::Text("Scale:%f", g_media_editor_settings.AudioDBScale);
                        ImGui::EndTooltip();
                    }
                }
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    g_media_editor_settings.AudioDBScale = 1.f;
            }
            draw_list->AddRect(scrop_rect.Min, scrop_rect.Max, COL_SLIDER_HANDLE, 0);
            draw_list->PushClipRect(scrop_rect.Min, scrop_rect.Max);
            ImVec2 channel_view_size = ImVec2(size.x, size.y / timeline->mAudioAttribute.channel_data.size());
            ImGui::SetCursorScreenPos(pos);
            ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(1.f, 1.f, 1.f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            for (int i = 0; i < timeline->mAudioAttribute.channel_data.size(); i++)
            {
                ImVec2 channel_min = pos + ImVec2(0, channel_view_size.y * i);
                ImVec2 channel_max = pos + ImVec2(channel_view_size.x, channel_view_size.y * i);
                // draw graticule line
                ImVec2 p1 = ImVec2(pos.x, pos.y + channel_view_size.y * i + channel_view_size.y);
                auto grid_number = floor(10 / g_media_editor_settings.AudioDBScale);
                auto grid_height = channel_view_size.y / grid_number;
                if (grid_number > 20) grid_number = 20;
                for (int x = 0; x < grid_number; x++)
                {
                    ImVec2 gp1 = p1 - ImVec2(0, grid_height * x);
                    ImVec2 gp2 = gp1 + ImVec2(channel_view_size.x, 0);
                    draw_list->AddLine(gp1, gp2, COL_GRAY_GRATICULE);
                }
                auto vgrid_number = channel_view_size.x / grid_height;
                for (int x = 0; x < vgrid_number; x++)
                {
                    ImVec2 gp1 = p1 + ImVec2(grid_height * x, 0);
                    ImVec2 gp2 = gp1 - ImVec2(0, grid_height * (grid_number - 1));
                    draw_list->AddLine(gp1, gp2, COL_GRAY_GRATICULE);
                }
                if (!timeline->mAudioAttribute.channel_data[i].m_db.empty())
                {
                    ImGui::PushID(i);
                    ImGui::ImMat db_mat_inv = timeline->mAudioAttribute.channel_data[i].m_db.clone();
                    db_mat_inv += 90.f;
                    ImGui::PlotLinesEx("##db", (float *)db_mat_inv.data,db_mat_inv.w, 0, nullptr, 0.f, 90.f / g_media_editor_settings.AudioDBScale, channel_view_size, 4, false, true);
                    ImGui::PopID();
                }
                draw_list->AddRect(channel_min, channel_max, COL_SLIDER_HANDLE, 0);
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);
            draw_list->PopClipRect();
            ImGui::EndGroup();
            timeline->mAudioAttribute.audio_mutex.unlock();
        }
        break;
        case 8:
        {
            // db level view
            timeline->mAudioAttribute.audio_mutex.lock();
            ImGui::BeginGroup();
            draw_list->AddRect(scrop_rect.Min, scrop_rect.Max, COL_SLIDER_HANDLE, 0);
            draw_list->PushClipRect(scrop_rect.Min, scrop_rect.Max);
            ImVec2 channel_view_size = ImVec2(size.x, size.y / timeline->mAudioAttribute.channel_data.size());
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(1.f, 1.f, 1.f, 0.5f));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            for (int i = 0; i < timeline->mAudioAttribute.channel_data.size(); i++)
            {
                ImVec2 channel_min = pos + ImVec2(0, channel_view_size.y * i);
                ImVec2 channel_max = pos + ImVec2(channel_view_size.x, channel_view_size.y * i);
                ImGui::PushID(i);
                if (!timeline->mAudioAttribute.channel_data[i].m_DBShort.empty() && g_media_editor_settings.AudioDBLevelShort == 1)
                    ImGui::PlotHistogram("##db_level", (float *)timeline->mAudioAttribute.channel_data[i].m_DBShort.data, timeline->mAudioAttribute.channel_data[i].m_DBShort.w, 0, nullptr, 0, 100, channel_view_size, 4);
                else if (!timeline->mAudioAttribute.channel_data[i].m_DBLong.empty() && g_media_editor_settings.AudioDBLevelShort == 0)
                    ImGui::PlotHistogram("##db_level", (float *)timeline->mAudioAttribute.channel_data[i].m_DBLong.data, timeline->mAudioAttribute.channel_data[i].m_DBLong.w, 0, nullptr, 0, 100, channel_view_size, 4);
                ImGui::PopID();
                draw_list->AddRect(channel_min, channel_max, COL_SLIDER_HANDLE, 0);
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
            draw_list->PopClipRect();
            ImGui::EndGroup();
            timeline->mAudioAttribute.audio_mutex.unlock();
        }
        break;
        case 9:
        {
            // spectrogram view
            timeline->mAudioAttribute.audio_mutex.lock();
            ImGui::BeginGroup();
            ImGui::InvisibleButton("##audio_spectrogram_view", size);
            if (ImGui::IsItemHovered())
            {
                if (io.MouseWheel < -FLT_EPSILON)
                {
                    g_media_editor_settings.AudioSpectrogramLight *= 0.9f;
                    if (g_media_editor_settings.AudioSpectrogramLight < 0.1)
                        g_media_editor_settings.AudioSpectrogramLight = 0.1;
                    timeline->mAudioAttribute.mAudioSpectrogramLight = g_media_editor_settings.AudioSpectrogramLight;
                    if (ImGui::BeginTooltip())
                    {
                        ImGui::Text("Light:%f", g_media_editor_settings.AudioSpectrogramLight);
                        ImGui::EndTooltip();
                    }
                }
                else if (io.MouseWheel > FLT_EPSILON)
                {
                    g_media_editor_settings.AudioSpectrogramLight *= 1.1f;
                    if (g_media_editor_settings.AudioSpectrogramLight > 1.0f)
                        g_media_editor_settings.AudioSpectrogramLight = 1.0;
                    timeline->mAudioAttribute.mAudioSpectrogramLight = g_media_editor_settings.AudioSpectrogramLight;
                    if (ImGui::BeginTooltip())
                    {
                        ImGui::Text("Light:%f", g_media_editor_settings.AudioSpectrogramLight);
                        ImGui::EndTooltip();
                    }
                }
                else if (io.MouseWheelH < -FLT_EPSILON)
                {
                    g_media_editor_settings.AudioSpectrogramOffset -= 5;
                    if (g_media_editor_settings.AudioSpectrogramOffset < -96.0)
                        g_media_editor_settings.AudioSpectrogramOffset = -96.0;
                    timeline->mAudioAttribute.mAudioSpectrogramOffset = g_media_editor_settings.AudioSpectrogramOffset;
                    if (ImGui::BeginTooltip())
                    {
                        ImGui::Text("Offset:%f", g_media_editor_settings.AudioSpectrogramOffset);
                        ImGui::EndTooltip();
                    }
                }
                else if (io.MouseWheelH > FLT_EPSILON)
                {
                    g_media_editor_settings.AudioSpectrogramOffset += 5;
                    if (g_media_editor_settings.AudioSpectrogramOffset > 96.0)
                        g_media_editor_settings.AudioSpectrogramOffset = 96.0;
                    timeline->mAudioAttribute.mAudioSpectrogramOffset = g_media_editor_settings.AudioSpectrogramOffset;
                    if (ImGui::BeginTooltip())
                    {
                        ImGui::Text("Offset:%f", g_media_editor_settings.AudioSpectrogramOffset);
                        ImGui::EndTooltip();
                    }
                }
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    g_media_editor_settings.AudioSpectrogramLight = 1.f;
                    g_media_editor_settings.AudioSpectrogramOffset = 0.f;
                    timeline->mAudioAttribute.mAudioSpectrogramLight = g_media_editor_settings.AudioSpectrogramLight;
                    timeline->mAudioAttribute.mAudioSpectrogramOffset = g_media_editor_settings.AudioSpectrogramOffset;
                }
            }
            draw_list->AddRect(scrop_rect.Min, scrop_rect.Max, COL_SLIDER_HANDLE, 0);
            draw_list->PushClipRect(scrop_rect.Min, scrop_rect.Max);
            ImVec2 channel_view_size = ImVec2(size.x - 80, size.y / timeline->mAudioAttribute.channel_data.size());
            ImGui::SetCursorScreenPos(pos);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            for (int i = 0; i < timeline->mAudioAttribute.channel_data.size(); i++)
            {
                ImVec2 channel_min = pos + ImVec2(32, channel_view_size.y * i);
                ImVec2 channel_max = pos + ImVec2(channel_view_size.x + 32, channel_view_size.y * i);
                ImVec2 center = channel_min + channel_view_size / 2;

                  // draw graticule line
                auto hz_step = channel_view_size.y / 11;
                ImGui::SetWindowFontScale(0.6);
                for (int i = 0; i < 11; i++)
                {
                    std::string str = std::to_string(i * 2) + "kHz";
                    ImVec2 p0 = channel_min + ImVec2(-32, channel_view_size.y - i * hz_step);
                    ImVec2 p1 = channel_min + ImVec2(-24, channel_view_size.y - i * hz_step);
                    draw_list->AddLine(p0, p1, COL_GRATICULE_DARK, 1);
                    draw_list->AddText(p1 + ImVec2(2, -9), IM_COL32_WHITE, str.c_str());
                }
                ImGui::SetWindowFontScale(1.0);
                
                if (!timeline->mAudioAttribute.channel_data[i].m_Spectrogram.empty())
                {
                    ImVec2 texture_pos = center - ImVec2(channel_view_size.y / 2, channel_view_size.x / 2);
                    ImGui::ImMatToTexture(timeline->mAudioAttribute.channel_data[i].m_Spectrogram, timeline->mAudioAttribute.channel_data[i].texture_spectrogram);
                    ImGui::ImDrawListAddImageRotate(draw_list, timeline->mAudioAttribute.channel_data[i].texture_spectrogram, texture_pos, ImVec2(channel_view_size.y, channel_view_size.x), -90.0);
                }
            }
            // draw bar mark
            for (int i = 0; i < size.y; i++)
            {
                float step = 128.0 / size.y;
                int mark_step = size.y / 9;
                float value = i * step;
                float light = value / 127.0f;
                float hue = ((int)(value + 170) % 255) / 255.f;
                auto color = ImColor::HSV(hue, 1.0, light * timeline->mAudioAttribute.mAudioSpectrogramLight);
                ImVec2 p0 = ImVec2(scrop_rect.Max.x - 44, scrop_rect.Max.y - i);
                ImVec2 p1 = ImVec2(scrop_rect.Max.x - 32, scrop_rect.Max.y - i);
                draw_list->AddLine(p0, p1, color);
                ImGui::SetWindowFontScale(0.6);
                if ((i % mark_step) == 0)
                {
                    int db_value = 90 - (i / mark_step) * 10;
                    std::string str = std::to_string(-db_value) + "dB";
                    ImVec2 p2 = ImVec2(scrop_rect.Max.x - 8, scrop_rect.Max.y - i);
                    ImVec2 p3 = ImVec2(scrop_rect.Max.x, scrop_rect.Max.y - i);
                    draw_list->AddLine(p2, p3, COL_GRATICULE_DARK, 1);
                    draw_list->AddText(p1 + ImVec2(2, db_value == 90 ? -9 : -4), IM_COL32_WHITE, str.c_str());
                }
                ImGui::SetWindowFontScale(1.0);
            }

            ImGui::PopStyleVar();
            draw_list->PopClipRect();
            ImGui::EndGroup();
            timeline->mAudioAttribute.audio_mutex.unlock();
        }
        break;
        default: break;
    }
}

static void ShowMediaAnalyseWindow(TimeLine *timeline, bool *expanded)
{
    ImGuiIO &io = ImGui::GetIO();
    bool multiviewport = io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    ImVec2 window_pos = ImGui::GetCursorScreenPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    ImVec2 scope_view_size = ImVec2(256, 256);
    // control bar
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5, 0.5, 0.5, 0.5));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2, 0.2, 0.2, 1.0));

    // global control bar
    auto pos = window_pos + ImVec2(window_size.x - 64, 0);
    std::vector<int> disabled_monitor;
    if (MainWindowIndex == 0)
        disabled_monitor.push_back(MonitorIndexPreviewVideo);
    else if (MainWindowIndex == 1 && VideoEditorWindowIndex == 0)
    {
        disabled_monitor.push_back(MonitorIndexVideoFilterOrg);
        disabled_monitor.push_back(MonitorIndexVideoFiltered);
    }
    MonitorButton("scope_monitor", pos, MonitorIndexScope, disabled_monitor, false, false, true);
    if (MonitorIndexChanged)
    {
        g_media_editor_settings.ExpandScope = true;
    }

    ImVec2 main_scope_pos = window_pos;
    if (expanded && !*expanded)
    {
        main_scope_pos += ImVec2((window_size.x - scope_view_size.x) / 2, 0);
    }

    ImGui::SetCursorScreenPos(main_scope_pos);
    if (ImGui::Button(ICON_MORE "##select_scope#main"))
    {
        ImGui::OpenPopup("##select_scope_view#main");
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_SETTING "##setting_scope#main"))
    {
        ImGui::OpenPopup("##setting_scope_view#main");
    }
    if (multiviewport)
        ImGui::SetNextWindowViewport(viewport->ID);
    
    if (ImGui::BeginPopup("##select_scope_view#main"))
    {
        for (int i = 0; i < IM_ARRAYSIZE(ScopeWindowTabNames); i++)
            if (ImGui::Selectable(ScopeWindowTabNames[i]))
            {
                g_media_editor_settings.ScopeWindowIndex = i;
                need_update_scope = true;
            }
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("##setting_scope_view#main"))
    {
        ShowMediaScopeSetting(g_media_editor_settings.ScopeWindowIndex);
        ImGui::EndPopup();
    }

    ShowMediaScopeView(g_media_editor_settings.ScopeWindowIndex, main_scope_pos + ImVec2(0, 16 + 8), scope_view_size);

    if (expanded && *expanded)
    {
        ImVec2 expand_scope_pos = window_pos + ImVec2(scope_view_size.x + 16, 0);
        ImGui::SetCursorScreenPos(expand_scope_pos);
        if (ImGui::Button(ICON_MORE "##select_scope#expand"))
        {
            ImGui::OpenPopup("##select_scope_view#expand");
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_SETTING "##setting_scope#expand"))
        {
            ImGui::OpenPopup("##setting_scope_view#expand");
        }
        if (multiviewport)
            ImGui::SetNextWindowViewport(viewport->ID);
        if (ImGui::BeginPopup("##select_scope_view#expand"))
        {
            for (int i = 0; i < IM_ARRAYSIZE(ScopeWindowTabNames); i++)
                if (ImGui::Selectable(ScopeWindowTabNames[i]))
                {
                    g_media_editor_settings.ScopeWindowExpandIndex = i;
                    need_update_scope = true;
                }
            ImGui::EndPopup();
        }
        if (ImGui::BeginPopup("##setting_scope_view#expand"))
        {
            ShowMediaScopeSetting(g_media_editor_settings.ScopeWindowExpandIndex);
            ImGui::EndPopup();
        }
        ShowMediaScopeView(g_media_editor_settings.ScopeWindowExpandIndex, expand_scope_pos + ImVec2(0, 16 + 8), scope_view_size);
    }

    ImGui::PopStyleColor(3);
}

static void ShowMediaAnalyseWindow(TimeLine *timeline)
{
    ImGuiIO &io = ImGui::GetIO();
    auto platform_io = ImGui::GetPlatformIO();
    bool multiviewport = io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable;
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
    ImVec2 pos = ImVec2(100, 100);
    if (MonitorIndexChanged)
    {
        if (multiviewport && MonitorIndexScope != -1 && MonitorIndexScope < platform_io.Monitors.Size)
        {
            auto mon = platform_io.Monitors[MonitorIndexScope];
            pos += mon.MainPos;
        }
        ImGui::SetNextWindowPos(pos);
        MonitorIndexChanged = false;
    }
    ImGui::SetNextWindowSize(ImVec2(1600, 800), ImGuiCond_None);
    ImGui::Begin("ScopeView", nullptr, flags);
    ImVec2 window_pos = ImGui::GetCursorScreenPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    // add left tool bar
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5, 0.5, 0.5, 0.5));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2, 0.2, 0.2, 1.0));
    if (ImGui::Button(ICON_DRAWING_PIN "##unexpand_scope"))
    {
        g_media_editor_settings.ExpandScope = false;
        MonitorIndexScope = -1;
    }
    if (multiviewport)
    {
        auto pos = ImGui::GetCursorScreenPos();
        std::vector<int> disabled_monitor;
        if (MainWindowIndex == 0)
            disabled_monitor.push_back(MonitorIndexPreviewVideo);
        else if (MainWindowIndex == 1 && VideoEditorWindowIndex == 0)
        {
            disabled_monitor.push_back(MonitorIndexVideoFilterOrg);
            disabled_monitor.push_back(MonitorIndexVideoFiltered);
        }
        MonitorButton("scope_monitor", pos, MonitorIndexScope, disabled_monitor, true, true, true);
    }

    // add scope UI layout
    ImVec2 view_pos = window_pos + ImVec2(32, 0);
    ImVec2 view_size = window_size - ImVec2(32, 0);
    ImVec2 scope_view_size = ImVec2(256, 256);
    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    // add video scope + audio wave
    for (int i = 0; i <= 4; i++)
    {
        ShowMediaScopeView(i, view_pos + ImVec2(i * (256 + 48), 0), scope_view_size);
        ImGui::SetCursorScreenPos(view_pos + ImVec2(i * (256 + 48), 256));
        ShowMediaScopeSetting(i, false);
        ImGui::SetWindowFontScale(2.0);
        ImGui::ImAddTextVertical(draw_list, view_pos + ImVec2(i * (256 + 48) + 256, 0), IM_COL32_WHITE, ScopeWindowTabNames[i]);
        ImGui::SetWindowFontScale(1.0);
    }
    // add audio scope
    for (int i = 5; i < 10; i++)
    {
        ShowMediaScopeView(i, view_pos + ImVec2((i - 5) * (256 + 48), 420), scope_view_size);
        ImGui::SetCursorScreenPos(view_pos + ImVec2((i - 5) * (256 + 48), 420 + 256));
        ShowMediaScopeSetting(i, false);
        ImGui::SetWindowFontScale(2.0);
        ImGui::ImAddTextVertical(draw_list, view_pos + ImVec2((i - 5) * (256 + 48) + 256, 420), IM_COL32_WHITE, ScopeWindowTabNames[i]);
        ImGui::SetWindowFontScale(1.0);
    }

    ImGui::PopStyleColor(3);
    ImGui::End();
}

#if 0
/****************************************************************************************
 * 
 * Media AI windows
 *
 ***************************************************************************************/
static void ShowMediaAIWindow(ImDrawList *draw_list)
{
    ImGui::SetWindowFontScale(1.2);
    ImGui::Indent(20);
    ImGui::PushStyleVar(ImGuiStyleVar_TexGlyphOutlineWidth, 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4, 0.4, 0.8, 0.8));
    ImGui::TextUnformatted("Media AI");
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::SetWindowFontScale(1.0);
}
#endif

/****************************************************************************************
 * 
 * Application Framework
 *
 ***************************************************************************************/
static void MediaEditor_SetupContext(ImGuiContext* ctx, bool in_splash)
{
    if (!ctx)
        return;
    set_context_in_splash = in_splash;
    // Setup MediaEditorSetting
    ImGuiSettingsHandler setting_ini_handler;
    setting_ini_handler.TypeName = "MediaEditorSetting";
    setting_ini_handler.TypeHash = ImHashStr("MediaEditorSetting");
    setting_ini_handler.ReadOpenFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler, const char* name) -> void*
    {
        return &g_media_editor_settings;
    };
    setting_ini_handler.ReadLineFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler, void* entry, const char* line) -> void
    {
        MediaEditorSettings * setting = (MediaEditorSettings*)entry;
        int val_int = 0;
        int64_t val_int64 = 0;
        float val_float = 0;
        char val_path[1024] = {0};
        ImVec4 val_vec4 = {0, 0, 0, 0};
        if (sscanf(line, "ProjectPath=%[^|\n]", val_path) == 1) { setting->project_path = std::string(val_path); }
        else if (sscanf(line, "UILanguage=%[^|\n]", val_path) == 1) { setting->UILanguage = std::string(val_path); }
        else if (sscanf(line, "BottomViewExpanded=%d", &val_int) == 1) { setting->BottomViewExpanded = val_int == 1; }
        else if (sscanf(line, "VideoFilterCurveExpanded=%d", &val_int) == 1) { setting->VideoFilterCurveExpanded = val_int == 1; }
        else if (sscanf(line, "VideoFusionCurveExpanded=%d", &val_int) == 1) { setting->VideoFusionCurveExpanded = val_int == 1; }
        else if (sscanf(line, "AudioFilterCurveExpanded=%d", &val_int) == 1) { setting->AudioFilterCurveExpanded = val_int == 1; }
        else if (sscanf(line, "AudioFusionCurveExpanded=%d", &val_int) == 1) { setting->AudioFusionCurveExpanded = val_int == 1; }
        else if (sscanf(line, "TextCurveExpanded=%d", &val_int) == 1) { setting->TextCurveExpanded = val_int == 1; }
        else if (sscanf(line, "TopViewHeight=%f", &val_float) == 1) { setting->TopViewHeight = isnan(val_float) ?  0.6f : val_float; }
        else if (sscanf(line, "BottomViewHeight=%f", &val_float) == 1) { setting->BottomViewHeight = isnan(val_float) ? 0.4f : val_float; }
        else if (sscanf(line, "OldBottomViewHeight=%f", &val_float) == 1) { setting->OldBottomViewHeight = val_float; }
        else if (sscanf(line, "ShowMeters=%d", &val_int) == 1) { setting->showMeters = val_int == 1; }
        else if (sscanf(line, "ControlPanelWidth=%f", &val_float) == 1) { setting->ControlPanelWidth = val_float; }
        else if (sscanf(line, "MainViewWidth=%f", &val_float) == 1) { setting->MainViewWidth = val_float; }
        else if (sscanf(line, "HWCodec=%d", &val_int) == 1) { setting->HardwareCodec = val_int == 1; }
        else if (sscanf(line, "VideoWidth=%d", &val_int) == 1) { setting->VideoWidth = val_int; }
        else if (sscanf(line, "VideoHeight=%d", &val_int) == 1) { setting->VideoHeight = val_int; }
        else if (sscanf(line, "PreviewScale=%f", &val_float) == 1) { setting->PreviewScale = val_float; }
        else if (sscanf(line, "VideoFrameRateNum=%d", &val_int) == 1) { setting->VideoFrameRate.num = val_int; }
        else if (sscanf(line, "VideoFrameRateDen=%d", &val_int) == 1) { setting->VideoFrameRate.den = val_int; }
        else if (sscanf(line, "PixelAspectRatioNum=%d", &val_int) == 1) { setting->PixelAspectRatio.num = val_int; }
        else if (sscanf(line, "PixelAspectRatioDen=%d", &val_int) == 1) { setting->PixelAspectRatio.den = val_int; }
        else if (sscanf(line, "ColorSpaceIndex=%d", &val_int) == 1) { setting->ColorSpaceIndex = val_int; }
        else if (sscanf(line, "ColorTransferIndex=%d", &val_int) == 1) { setting->ColorTransferIndex = val_int; }
        else if (sscanf(line, "VideoFrameCache=%d", &val_int) == 1) { setting->VideoFrameCacheSize = val_int; }
        else if (sscanf(line, "AudioChannels=%d", &val_int) == 1) { setting->AudioChannels = val_int; }
        else if (sscanf(line, "AudioSampleRate=%d", &val_int) == 1) { setting->AudioSampleRate = val_int; }
        else if (sscanf(line, "AudioFormat=%d", &val_int) == 1) { setting->AudioFormat = val_int; }
        else if (sscanf(line, "BankViewStyle=%d", &val_int) == 1) { setting->BankViewStyle = val_int; }
        else if (sscanf(line, "ShowHelpTips=%d", &val_int) == 1) { setting->ShowHelpTooltips = val_int == 1; }
        else if (sscanf(line, "ExpandScope=%d", &val_int) == 1) { setting->ExpandScope = val_int == 1; }
        else if (sscanf(line, "HistogramLogView=%d", &val_int) == 1) { setting->HistogramLog = val_int == 1; }
        else if (sscanf(line, "HistogramSplited=%d", &val_int) == 1) { setting->HistogramSplited = val_int == 1; }
        else if (sscanf(line, "HistogramYRGB=%d", &val_int) == 1) { setting->HistogramYRGB = val_int == 1; }
        else if (sscanf(line, "HistogramScale=%f", &val_float) == 1) { setting->HistogramScale = val_float; }
        else if (sscanf(line, "WaveformMirror=%d", &val_int) == 1) { setting->WaveformMirror = val_int == 1; }
        else if (sscanf(line, "WaveformSeparate=%d", &val_int) == 1) { setting->WaveformSeparate = val_int == 1; }
        else if (sscanf(line, "WaveformShowY=%d", &val_int) == 1) { setting->WaveformShowY = val_int == 1; }
        else if (sscanf(line, "WaveformIntensity=%f", &val_float) == 1) { setting->WaveformIntensity = val_float; }
        else if (sscanf(line, "CIECorrectGamma=%d", &val_int) == 1) { setting->CIECorrectGamma = val_int == 1; }
        else if (sscanf(line, "CIEShowColor=%d", &val_int) == 1) { setting->CIEShowColor = val_int == 1; }
        else if (sscanf(line, "CIEContrast=%f", &val_float) == 1) { setting->CIEContrast = val_float; }
        else if (sscanf(line, "CIEIntensity=%f", &val_float) == 1) { setting->CIEIntensity = val_float; }
        else if (sscanf(line, "CIEColorSystem=%d", &val_int) == 1) { setting->CIEColorSystem = val_int; }
        else if (sscanf(line, "CIEMode=%d", &val_int) == 1) { setting->CIEMode = val_int; }
        else if (sscanf(line, "CIEGamuts=%d", &val_int) == 1) { setting->CIEGamuts = val_int; }
        else if (sscanf(line, "VectorIntensity=%f", &val_float) == 1) { setting->VectorIntensity = val_float; }
        else if (sscanf(line, "AudioWaveScale=%f", &val_float) == 1) { setting->AudioWaveScale = val_float; }
        else if (sscanf(line, "AudioVectorScale=%f", &val_float) == 1) { setting->AudioVectorScale = val_float; }
        else if (sscanf(line, "AudioVectorMode=%d", &val_int) == 1) { setting->AudioVectorMode = val_int; }
        else if (sscanf(line, "AudioFFTScale=%f", &val_float) == 1) { setting->AudioFFTScale = val_float; }
        else if (sscanf(line, "AudioDBScale=%f", &val_float) == 1) { setting->AudioDBScale = val_float; }
        else if (sscanf(line, "AudioDBLevelShort=%d", &val_int) == 1) { setting->AudioDBLevelShort = val_int == 1; }
        else if (sscanf(line, "AudioSpectrogramOffset=%f", &val_float) == 1) { setting->AudioSpectrogramOffset = val_float; }
        else if (sscanf(line, "AudioSpectrogramLight=%f", &val_float) == 1) { setting->AudioSpectrogramLight = val_float; }
        else if (sscanf(line, "ScopeWindowIndex=%d", &val_int) == 1) { setting->ScopeWindowIndex = val_int; }
        else if (sscanf(line, "ScopeWindowExpandIndex=%d", &val_int) == 1) { setting->ScopeWindowExpandIndex = val_int; }
        else if (sscanf(line, "FontName=%[^|\n]", val_path) == 1) { setting->FontName = std::string(val_path); }
        else if (sscanf(line, "FontScaleLink=%d", &val_int) == 1) { setting->FontScaleLink = val_int == 1; }
        else if (sscanf(line, "FontScaleX=%f", &val_float) == 1) { setting->FontScaleX = val_float; }
        else if (sscanf(line, "FontScaleY=%f", &val_float) == 1) { setting->FontScaleY = val_float; }
        else if (sscanf(line, "FontSpacing=%f", &val_float) == 1) { setting->FontSpacing = val_float; }
        else if (sscanf(line, "FontAngle=%f", &val_float) == 1) { setting->FontAngle = val_float; }
        else if (sscanf(line, "FontOutlineWidth=%f", &val_float) == 1) { setting->FontOutlineWidth = val_float; }
        else if (sscanf(line, "FontAlignment=%d", &val_int) == 1) { setting->FontAlignment = val_int; }
        else if (sscanf(line, "FontItalic=%d", &val_int) == 1) { setting->FontItalic = val_int; }
        else if (sscanf(line, "FontBold=%d", &val_int) == 1) { setting->FontBold = val_int; }
        else if (sscanf(line, "FontUnderLine=%d", &val_int) == 1) { setting->FontUnderLine = val_int == 1; }
        else if (sscanf(line, "FontStrikeOut=%d", &val_int) == 1) { setting->FontStrikeOut = val_int == 1; }
        else if (sscanf(line, "FontPosOffsetX=%f", &val_float) == 1) { setting->FontPosOffsetX = val_float; }
        else if (sscanf(line, "FontPosOffsetY=%f", &val_float) == 1) { setting->FontPosOffsetY = val_float; }
        else if (sscanf(line, "FontBorderType=%d", &val_int) == 1) { setting->FontBorderType = val_int; }
        else if (sscanf(line, "FontShadowDepth=%f", &val_float) == 1) { setting->FontShadowDepth = val_float; }
        else if (sscanf(line, "FontPrimaryColor=%f,%f,%f,%f", &val_vec4.x, &val_vec4.y, &val_vec4.z, &val_vec4.w) == 1) { setting->FontPrimaryColor = val_vec4; }
        else if (sscanf(line, "FontOutlineColor=%f,%f,%f,%f", &val_vec4.x, &val_vec4.y, &val_vec4.z, &val_vec4.w) == 1) { setting->FontOutlineColor = val_vec4; }
        else if (sscanf(line, "FontBackColor=%f,%f,%f,%f", &val_vec4.x, &val_vec4.y, &val_vec4.z, &val_vec4.w) == 1) { setting->FontBackColor = val_vec4; }
        else if (sscanf(line, "OutputFormatIndex=%d", &val_int) == 1) { setting->OutputFormatIndex = val_int; }
        else if (sscanf(line, "OutputVideoCodecIndex=%d", &val_int) == 1) { setting->OutputVideoCodecIndex = val_int; }
        else if (sscanf(line, "OutputVideoCodecTypeIndex=%d", &val_int) == 1) { setting->OutputVideoCodecTypeIndex = val_int; }
        else if (sscanf(line, "OutputVideoCodecProfileIndex=%d", &val_int) == 1) { setting->OutputVideoCodecProfileIndex = val_int; }
        else if (sscanf(line, "OutputVideoCodecPresetIndex=%d", &val_int) == 1) { setting->OutputVideoCodecPresetIndex = val_int; }
        else if (sscanf(line, "OutputVideoCodecTuneIndex=%d", &val_int) == 1) { setting->OutputVideoCodecTuneIndex = val_int; }
        else if (sscanf(line, "OutputVideoCodecCompressionIndex=%d", &val_int) == 1) { setting->OutputVideoCodecCompressionIndex = val_int; }
        else if (sscanf(line, "OutputVideoSettingAsTimeline=%d", &val_int) == 1) { setting->OutputVideoSettingAsTimeline = val_int == 1; }
        else if (sscanf(line, "OutputVideoResolutionIndex=%d", &val_int) == 1) { setting->OutputVideoResolutionIndex = val_int; }
        else if (sscanf(line, "OutputVideoResolutionWidth=%d", &val_int) == 1) { setting->OutputVideoResolutionWidth = val_int; }
        else if (sscanf(line, "OutputVideoResolutionHeight=%d", &val_int) == 1) { setting->OutputVideoResolutionHeight = val_int; }
        else if (sscanf(line, "OutputVideoPixelAspectRatioIndex=%d", &val_int) == 1) { setting->OutputVideoPixelAspectRatioIndex = val_int; }
        else if (sscanf(line, "OutputVideoPixelAspectRatioNum=%d", &val_int) == 1) { setting->OutputVideoPixelAspectRatio.num = val_int; }
        else if (sscanf(line, "OutputVideoPixelAspectRatioDen=%d", &val_int) == 1) { setting->OutputVideoPixelAspectRatio.den = val_int; }
        else if (sscanf(line, "OutputVideoFrameRateIndex=%d", &val_int) == 1) { setting->OutputVideoFrameRateIndex = val_int; }
        else if (sscanf(line, "OutputVideoFrameRateNum=%d", &val_int) == 1) { setting->OutputVideoFrameRate.num = val_int; }
        else if (sscanf(line, "OutputVideoFrameRateDen=%d", &val_int) == 1) { setting->OutputVideoFrameRate.den = val_int; }
        else if (sscanf(line, "OutputColorSpaceIndex=%d", &val_int) == 1) { setting->OutputColorSpaceIndex = val_int; }
        else if (sscanf(line, "OutputColorTransferIndex=%d", &val_int) == 1) { setting->OutputColorTransferIndex = val_int; }
        else if (sscanf(line, "OutputVideoBitrateStrategyindex=%d", &val_int) == 1) { setting->OutputVideoBitrateStrategyindex = val_int; }
        else if (sscanf(line, "OutputVideoBitrate=%d", &val_int) == 1) { setting->OutputVideoBitrate = val_int; }
        else if (sscanf(line, "OutputVideoGOPSize=%d", &val_int) == 1) { setting->OutputVideoGOPSize = val_int; }
        else if (sscanf(line, "OutputVideoBFrames=%d", &val_int) == 1) { setting->OutputVideoBFrames = val_int; }
        else if (sscanf(line, "OutputAudioCodecIndex=%d", &val_int) == 1) { setting->OutputAudioCodecIndex = val_int; }
        else if (sscanf(line, "OutputAudioCodecTypeIndex=%d", &val_int) == 1) { setting->OutputAudioCodecTypeIndex = val_int; }
        else if (sscanf(line, "OutputAudioSettingAsTimeline=%d", &val_int) == 1) { setting->OutputAudioSettingAsTimeline = val_int == 1; }
        else if (sscanf(line, "OutputAudioSampleRateIndex=%d", &val_int) == 1) { setting->OutputAudioSampleRateIndex = val_int; }
        else if (sscanf(line, "OutputAudioSampleRate=%d", &val_int) == 1) { setting->OutputAudioSampleRate = val_int; }
        else if (sscanf(line, "OutputAudioChannelsIndex=%d", &val_int) == 1) { setting->OutputAudioChannelsIndex = val_int; }
        else if (sscanf(line, "OutputAudioChannels=%d", &val_int) == 1) { setting->OutputAudioChannels = val_int; }
        g_new_setting = g_media_editor_settings;
    };
    setting_ini_handler.WriteAllFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* out_buf)
    {
        ImGuiContext& g = *ctx;
        out_buf->reserve(out_buf->size() + g.SettingsWindows.size() * 6); // ballpark reserve
        out_buf->appendf("[%s][##MediaEditorSetting]\n", handler->TypeName);
        out_buf->appendf("ProjectPath=%s\n", g_media_editor_settings.project_path.c_str());
        out_buf->appendf("UILanguage=%s\n", g_media_editor_settings.UILanguage.c_str());
        out_buf->appendf("BottomViewExpanded=%d\n", g_media_editor_settings.BottomViewExpanded ? 1 : 0);
        out_buf->appendf("VideoFilterCurveExpanded=%d\n", g_media_editor_settings.VideoFilterCurveExpanded ? 1 : 0);
        out_buf->appendf("VideoFusionCurveExpanded=%d\n", g_media_editor_settings.VideoFusionCurveExpanded ? 1 : 0);
        out_buf->appendf("AudioFilterCurveExpanded=%d\n", g_media_editor_settings.AudioFilterCurveExpanded ? 1 : 0);
        out_buf->appendf("AudioFusionCurveExpanded=%d\n", g_media_editor_settings.AudioFusionCurveExpanded ? 1 : 0);
        out_buf->appendf("TextCurveExpanded=%d\n", g_media_editor_settings.TextCurveExpanded ? 1 : 0);
        out_buf->appendf("TopViewHeight=%f\n", g_media_editor_settings.TopViewHeight);
        out_buf->appendf("BottomViewHeight=%f\n", g_media_editor_settings.BottomViewHeight);
        out_buf->appendf("OldBottomViewHeight=%f\n", g_media_editor_settings.OldBottomViewHeight);
        out_buf->appendf("ShowMeters=%d\n", g_media_editor_settings.showMeters ? 1 : 0);
        out_buf->appendf("ControlPanelWidth=%f\n", g_media_editor_settings.ControlPanelWidth);
        out_buf->appendf("MainViewWidth=%f\n", g_media_editor_settings.MainViewWidth);
        out_buf->appendf("HWCodec=%d\n", g_media_editor_settings.HardwareCodec ? 1 : 0);
        out_buf->appendf("VideoWidth=%d\n", g_media_editor_settings.VideoWidth);
        out_buf->appendf("VideoHeight=%d\n", g_media_editor_settings.VideoHeight);
        out_buf->appendf("PreviewScale=%f\n", g_media_editor_settings.PreviewScale);
        out_buf->appendf("VideoFrameRateNum=%d\n", g_media_editor_settings.VideoFrameRate.num);
        out_buf->appendf("VideoFrameRateDen=%d\n", g_media_editor_settings.VideoFrameRate.den);
        out_buf->appendf("PixelAspectRatioNum=%d\n", g_media_editor_settings.PixelAspectRatio.num);
        out_buf->appendf("PixelAspectRatioDen=%d\n", g_media_editor_settings.PixelAspectRatio.den);
        out_buf->appendf("ColorSpaceIndex=%d\n", g_media_editor_settings.ColorSpaceIndex);
        out_buf->appendf("ColorTransferIndex=%d\n", g_media_editor_settings.ColorTransferIndex);
        out_buf->appendf("VideoFrameCache=%d\n", g_media_editor_settings.VideoFrameCacheSize);
        out_buf->appendf("AudioChannels=%d\n", g_media_editor_settings.AudioChannels);
        out_buf->appendf("AudioSampleRate=%d\n", g_media_editor_settings.AudioSampleRate);
        out_buf->appendf("AudioFormat=%d\n", g_media_editor_settings.AudioFormat);
        out_buf->appendf("BankViewStyle=%d\n", g_media_editor_settings.BankViewStyle);
        out_buf->appendf("ShowHelpTips=%d\n", g_media_editor_settings.ShowHelpTooltips ? 1 : 0);
        out_buf->appendf("ExpandScope=%d\n", g_media_editor_settings.ExpandScope ? 1 : 0);
        out_buf->appendf("HistogramLogView=%d\n", g_media_editor_settings.HistogramLog ? 1 : 0);
        out_buf->appendf("HistogramSplited=%d\n", g_media_editor_settings.HistogramSplited ? 1 : 0);
        out_buf->appendf("HistogramYRGB=%d\n", g_media_editor_settings.HistogramYRGB ? 1 : 0);
        out_buf->appendf("HistogramScale=%f\n", g_media_editor_settings.HistogramScale);
        out_buf->appendf("WaveformMirror=%d\n", g_media_editor_settings.WaveformMirror ? 1 : 0);
        out_buf->appendf("WaveformSeparate=%d\n", g_media_editor_settings.WaveformSeparate ? 1 : 0);
        out_buf->appendf("WaveformShowY=%d\n", g_media_editor_settings.WaveformShowY ? 1 : 0);
        out_buf->appendf("WaveformIntensity=%f\n", g_media_editor_settings.WaveformIntensity);
        out_buf->appendf("CIECorrectGamma=%d\n", g_media_editor_settings.CIECorrectGamma ? 1 : 0);
        out_buf->appendf("CIEShowColor=%d\n", g_media_editor_settings.CIEShowColor ? 1 : 0);
        out_buf->appendf("CIEContrast=%f\n", g_media_editor_settings.CIEContrast);
        out_buf->appendf("CIEIntensity=%f\n", g_media_editor_settings.CIEIntensity);
        out_buf->appendf("CIEColorSystem=%d\n", g_media_editor_settings.CIEColorSystem);
        out_buf->appendf("CIEMode=%d\n", g_media_editor_settings.CIEMode);
        out_buf->appendf("CIEGamuts=%d\n", g_media_editor_settings.CIEGamuts);
        out_buf->appendf("VectorIntensity=%f\n", g_media_editor_settings.VectorIntensity);
        out_buf->appendf("AudioWaveScale=%f\n", g_media_editor_settings.AudioWaveScale);
        out_buf->appendf("AudioVectorScale=%f\n", g_media_editor_settings.AudioVectorScale);
        out_buf->appendf("AudioVectorMode=%d\n", g_media_editor_settings.AudioVectorMode);
        out_buf->appendf("AudioFFTScale=%f\n", g_media_editor_settings.AudioFFTScale);
        out_buf->appendf("AudioDBScale=%f\n", g_media_editor_settings.AudioDBScale);
        out_buf->appendf("AudioDBLevelShort=%d\n", g_media_editor_settings.AudioDBLevelShort ? 1 : 0);
        out_buf->appendf("AudioSpectrogramOffset=%f\n", g_media_editor_settings.AudioSpectrogramOffset);
        out_buf->appendf("AudioSpectrogramLight=%f\n", g_media_editor_settings.AudioSpectrogramLight);
        out_buf->appendf("ScopeWindowIndex=%d\n", g_media_editor_settings.ScopeWindowIndex);
        out_buf->appendf("ScopeWindowExpandIndex=%d\n", g_media_editor_settings.ScopeWindowExpandIndex);
        out_buf->appendf("FontName=%s\n", g_media_editor_settings.FontName.c_str());
        out_buf->appendf("FontScaleLink=%d\n", g_media_editor_settings.FontScaleLink ? 1 : 0);
        out_buf->appendf("FontScaleX=%f\n", g_media_editor_settings.FontScaleX);
        out_buf->appendf("FontScaleY=%f\n", g_media_editor_settings.FontScaleY);
        out_buf->appendf("FontSpacing=%f\n", g_media_editor_settings.FontSpacing);
        out_buf->appendf("FontAngle=%f\n", g_media_editor_settings.FontAngle);
        out_buf->appendf("FontOutlineWidth=%f\n", g_media_editor_settings.FontOutlineWidth);
        out_buf->appendf("FontAlignment=%d\n", g_media_editor_settings.FontAlignment);
        out_buf->appendf("FontItalic=%d\n", g_media_editor_settings.FontItalic);
        out_buf->appendf("FontBold=%d\n", g_media_editor_settings.FontBold);
        out_buf->appendf("FontUnderLine=%d\n", g_media_editor_settings.FontUnderLine ? 1 : 0);
        out_buf->appendf("FontStrikeOut=%d\n", g_media_editor_settings.FontStrikeOut ? 1 : 0);
        out_buf->appendf("FontPosOffsetX=%f\n", g_media_editor_settings.FontPosOffsetX);
        out_buf->appendf("FontPosOffsetY=%f\n", g_media_editor_settings.FontPosOffsetY);
        out_buf->appendf("FontBorderType=%d\n", g_media_editor_settings.FontBorderType);
        out_buf->appendf("FontShadowDepth=%f\n", g_media_editor_settings.FontShadowDepth);
        out_buf->appendf("FontPrimaryColor=%f,%f,%f,%f\n", g_media_editor_settings.FontPrimaryColor.x, g_media_editor_settings.FontPrimaryColor.y, g_media_editor_settings.FontPrimaryColor.z, g_media_editor_settings.FontPrimaryColor.w);
        out_buf->appendf("FontOutlineColor=%f,%f,%f,%f\n", g_media_editor_settings.FontOutlineColor.x, g_media_editor_settings.FontOutlineColor.y, g_media_editor_settings.FontOutlineColor.z, g_media_editor_settings.FontOutlineColor.w);
        out_buf->appendf("FontBackColor=%f,%f,%f,%f\n", g_media_editor_settings.FontBackColor.x, g_media_editor_settings.FontBackColor.y, g_media_editor_settings.FontBackColor.z, g_media_editor_settings.FontBackColor.w);
        out_buf->appendf("OutputFormatIndex=%d\n", g_media_editor_settings.OutputFormatIndex);
        out_buf->appendf("OutputVideoCodecIndex=%d\n", g_media_editor_settings.OutputVideoCodecIndex);
        out_buf->appendf("OutputVideoCodecTypeIndex=%d\n", g_media_editor_settings.OutputVideoCodecTypeIndex);
        out_buf->appendf("OutputVideoCodecProfileIndex=%d\n", g_media_editor_settings.OutputVideoCodecProfileIndex);
        out_buf->appendf("OutputVideoCodecPresetIndex=%d\n", g_media_editor_settings.OutputVideoCodecPresetIndex);
        out_buf->appendf("OutputVideoCodecTuneIndex=%d\n", g_media_editor_settings.OutputVideoCodecTuneIndex);
        out_buf->appendf("OutputVideoCodecCompressionIndex=%d\n", g_media_editor_settings.OutputVideoCodecCompressionIndex);
        out_buf->appendf("OutputVideoSettingAsTimeline=%d\n", g_media_editor_settings.OutputVideoSettingAsTimeline ? 1 : 0);
        out_buf->appendf("OutputVideoResolutionIndex=%d\n", g_media_editor_settings.OutputVideoResolutionIndex);
        out_buf->appendf("OutputVideoResolutionWidth=%d\n", g_media_editor_settings.OutputVideoResolutionWidth);
        out_buf->appendf("OutputVideoResolutionHeight=%d\n", g_media_editor_settings.OutputVideoResolutionHeight);
        out_buf->appendf("OutputVideoPixelAspectRatioIndex=%d\n", g_media_editor_settings.OutputVideoPixelAspectRatioIndex);
        out_buf->appendf("OutputVideoPixelAspectRatioNum=%d\n", g_media_editor_settings.OutputVideoPixelAspectRatio.num);
        out_buf->appendf("OutputVideoPixelAspectRatioDen=%d\n", g_media_editor_settings.OutputVideoPixelAspectRatio.den);
        out_buf->appendf("OutputVideoFrameRateIndex=%d\n", g_media_editor_settings.OutputVideoFrameRateIndex);
        out_buf->appendf("OutputVideoFrameRateNum=%d\n", g_media_editor_settings.OutputVideoFrameRate.num);
        out_buf->appendf("OutputVideoFrameRateDen=%d\n", g_media_editor_settings.OutputVideoFrameRate.den);
        out_buf->appendf("OutputColorSpaceIndex=%d\n", g_media_editor_settings.OutputColorSpaceIndex);
        out_buf->appendf("OutputColorTransferIndex=%d\n", g_media_editor_settings.OutputColorTransferIndex);
        out_buf->appendf("OutputVideoBitrateStrategyindex=%d\n", g_media_editor_settings.OutputVideoBitrateStrategyindex);
        out_buf->appendf("OutputVideoBitrate=%d\n", g_media_editor_settings.OutputVideoBitrate);
        out_buf->appendf("OutputVideoGOPSize=%d\n", g_media_editor_settings.OutputVideoGOPSize);
        out_buf->appendf("OutputVideoBFrames=%d\n", g_media_editor_settings.OutputVideoBFrames);
        out_buf->appendf("OutputAudioCodecIndex=%d\n", g_media_editor_settings.OutputAudioCodecIndex);
        out_buf->appendf("OutputAudioCodecTypeIndex=%d\n", g_media_editor_settings.OutputAudioCodecTypeIndex);
        out_buf->appendf("OutputAudioSettingAsTimeline=%d\n", g_media_editor_settings.OutputAudioSettingAsTimeline ? 1 : 0);
        out_buf->appendf("OutputAudioSampleRateIndex=%d\n", g_media_editor_settings.OutputAudioSampleRateIndex);
        out_buf->appendf("OutputAudioSampleRate=%d\n", g_media_editor_settings.OutputAudioSampleRate);
        out_buf->appendf("OutputAudioChannelsIndex=%d\n", g_media_editor_settings.OutputAudioChannelsIndex);
        out_buf->appendf("OutputAudioChannels=%d\n", g_media_editor_settings.OutputAudioChannels);
        out_buf->append("\n");
    };
    setting_ini_handler.ApplyAllFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler)
    {
        // handle project after all setting is loaded 
        if (!g_media_editor_settings.project_path.empty())
        {
            CleanProject();
            g_loading_thread = new std::thread(LoadThread, g_media_editor_settings.project_path, set_context_in_splash);
        }
        else
        {
            NewTimeline();
        }
#if IMGUI_VULKAN_SHADER
        if (m_cie) 
            m_cie->SetParam(g_media_editor_settings.CIEColorSystem, 
                            g_media_editor_settings.CIEMode, 512, 
                            g_media_editor_settings.CIEGamuts, 
                            g_media_editor_settings.CIEContrast, 
                            g_media_editor_settings.CIECorrectGamma);
#endif
    };
    ctx->SettingsHandlers.push_back(setting_ini_handler);

#ifdef USE_BOOKMARK
    ImGuiSettingsHandler bookmark_ini_handler;
    bookmark_ini_handler.TypeName = "BookMark";
    bookmark_ini_handler.TypeHash = ImHashStr("BookMark");
    bookmark_ini_handler.ReadOpenFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler, const char* name) -> void*
    {
        return ImGuiFileDialog::Instance();
    };
    bookmark_ini_handler.ReadLineFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler, void* entry, const char* line) -> void
    {
        IGFD::FileDialog * dialog = (IGFD::FileDialog *)entry;
        dialog->DeserializeBookmarks(line);
    };
    bookmark_ini_handler.WriteAllFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* out_buf)
    {
        ImGuiContext& g = *ctx;
        out_buf->reserve(out_buf->size() + g.SettingsWindows.size() * 6); // ballpark reserve
        auto bookmark = ImGuiFileDialog::Instance()->SerializeBookmarks();
        out_buf->appendf("[%s][##%s]\n", handler->TypeName, handler->TypeName);
        out_buf->appendf("%s\n", bookmark.c_str());
        out_buf->append("\n");
    };
    ctx->SettingsHandlers.push_back(bookmark_ini_handler);
#endif
}

static void MediaEditor_Initialize(void** handle)
{
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImFontAtlas* atlas = io.Fonts;
    ImFont* font = atlas->Fonts[0];
    io.FontDefault = font;
    if (io.ConfigFlags & ImGuiConfigFlags_EnableLowRefreshMode)
    {
        ImGui::SetTableLabelBreathingSpeed(0.01, 0.5);
        ui_breathing_step = 0.02;
    }
    else
    {
        ImGui::SetTableLabelBreathingSpeed(0.005, 0.5);
        ui_breathing_step = 0.01;
    }
    ImGui::ResetTabLabelStyle(ImGui::ImGuiTabLabelStyle_Dark, *tab_style);

#if defined(NDEBUG)
    av_log_set_level(AV_LOG_FATAL);
#else
    Logger::GetDefaultLogger()->SetShowLevels(Logger::DEBUG);
    // GetMultiTrackVideoReaderLogger()->SetShowLevels(Logger::VERBOSE);
    // GetMediaReaderLogger()->SetShowLevels(Logger::DEBUG);
    // GetSnapshotGeneratorLogger()->SetShowLevels(Logger::DEBUG);
    // GetMediaEncoderLogger()->SetShowLevels(Logger::DEBUG);
    GetSubtitleTrackLogger()->SetShowLevels(Logger::DEBUG);
    // GetMediaOverviewLogger()->SetShowLevels(Logger::DEBUG);
#endif

    if (!MediaCore::InitializeSubtitleLibrary())
        std::cout << "FAILED to initialize the subtitle library!" << std::endl;
    else
    {
        fontTable = FM::GroupFontsByFamily(FM::GetAvailableFonts());
        std::list<std::string> fonts;
        for (auto& item : fontTable)
            fonts.push_back(item.first);
        fonts.sort();
        for (auto& item : fonts)
            fontFamilies.push_back(item);
    }
#if IMGUI_VULKAN_SHADER
    int gpu = ImGui::get_default_gpu_index();
    m_histogram = new ImGui::Histogram_vulkan(gpu);
    m_waveform = new ImGui::Waveform_vulkan(gpu);
    m_cie = new ImGui::CIE_vulkan(gpu);
    m_vector = new ImGui::Vector_vulkan(gpu);
#endif
    if (!ImGuiHelper::file_exists(io.IniFilename) && !timeline)  NewTimeline();
}

static void MediaEditor_Finalize(void** handle)
{
    if (timeline) { delete timeline; timeline = nullptr; }
#if IMGUI_VULKAN_SHADER
    if (m_histogram) { delete m_histogram; m_histogram = nullptr; }
    if (m_waveform) { delete m_waveform; m_waveform = nullptr; }
    if (m_cie) { delete m_cie; m_cie = nullptr; }
    if (m_vector) {delete m_vector; m_vector = nullptr; }
#endif
    if (waveform_texture) { ImGui::ImDestroyTexture(waveform_texture); waveform_texture = nullptr; }
    if (cie_texture) { ImGui::ImDestroyTexture(cie_texture); cie_texture = nullptr; }
    if (vector_texture) { ImGui::ImDestroyTexture(vector_texture); vector_texture = nullptr; }
    if (logo_texture) { ImGui::ImDestroyTexture(logo_texture); logo_texture = nullptr; }
    if (codewin_texture) { ImGui::ImDestroyTexture(codewin_texture); codewin_texture = nullptr; }

    ImPlot::DestroyContext();
    MediaCore::ReleaseSubtitleLibrary();
}

static void MediaEditor_DropFromSystem(std::vector<std::string>& drops)
{
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceExtern | ImGuiDragDropFlags_SourceNoPreviewTooltip | ImGuiDragDropFlags_AcceptPeekOnly))
	{
        if (timeline)
        {
            import_url.clear();
            for (auto path : drops) import_url.push_back(path);
            ImGui::SetDragDropPayload("FILES", timeline, sizeof(timeline));
        }
		ImGui::EndDragDropSource();
	}
}

static bool MediaEditor_Frame(void * handle, bool app_will_quit)
{
    static bool app_done = false;
    const float media_icon_size = 96; 
    const float scope_height = 256;
    const float tool_icon_size = 32;
    static bool show_about = false;
    static bool show_configure = false;
    static bool show_debug = false;
    static bool show_file_dialog = false;
    auto platform_io = ImGui::GetPlatformIO();
    bool is_splitter_hold = false;
    if (!timeline) return app_will_quit;
    ImGuiContext& g = *GImGui;
    if (!g_media_editor_settings.UILanguage.empty() && g.LanguageName != g_media_editor_settings.UILanguage)
        g.LanguageName = g_media_editor_settings.UILanguage;
    const ImGuiFileDialogFlags fflags = ImGuiFileDialogFlags_ShowBookmark | ImGuiFileDialogFlags_CaseInsensitiveExtention | ImGuiFileDialogFlags_DisableCreateDirectoryButton | ImGuiFileDialogFlags_Modal;
    const ImGuiFileDialogFlags pflags = ImGuiFileDialogFlags_ShowBookmark | ImGuiFileDialogFlags_CaseInsensitiveExtention | ImGuiFileDialogFlags_ConfirmOverwrite | ImGuiFileDialogFlags_Modal;
    const std::string pfilters = "Project files (*.mep){.mep},.*";
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    static const int numControlPanelTabs = sizeof(ControlPanelTabNames)/sizeof(ControlPanelTabNames[0]);
    static const int numMainWindowTabs = sizeof(MainWindowTabNames)/sizeof(MainWindowTabNames[0]);
    bool multiviewport = io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | 
                        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoDocking;
    if (multiviewport)
    {
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    }
    else
    {
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_None);
    }
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 12.0f);
    UpdateBreathing();
    ImGui::Begin("Main Editor", nullptr, flags);
    // for debug
    //if (show_debug) ImGui::ShowMetricsWindow(&show_debug);
    // for debug end

    if (!set_context_in_splash)
    {
        if (g_project_loading)
        {
            ImGui::OpenPopup("Project Loading", ImGuiPopupFlags_AnyPopup);
        }

        if (multiviewport)
            ImGui::SetNextWindowViewport(viewport->ID);
        if (ImGui::BeginPopupModal("Project Loading", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings))
        {
            ImGui::Text("Project Loading...");
            ImGui::Separator();
            ImGui::BufferingBar("##project_loading_bar", g_project_loading_percentage, ImVec2(400, 6), 0.75, ImGui::GetColorU32(ImGuiCol_Button), ImGui::GetColorU32(ImGuiCol_ButtonHovered));
            if (!g_project_loading) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }

    if (g_project_loading)
    {
        if (multiviewport) ImGui::PopStyleVar(2);
        ImGui::PopStyleVar(2);
        ImGui::End();
        return app_will_quit;
    }

    if (show_about)
    {
        ImGui::OpenPopup(ICON_FA_CIRCLE_INFO " About", ImGuiPopupFlags_AnyPopup);
    }

    if (multiviewport)
        ImGui::SetNextWindowViewport(viewport->ID);
    if (ImGui::BeginPopupModal(ICON_FA_CIRCLE_INFO " About", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings))
    {
        static int32_t about_start_time = 0;
        if (about_start_time == 0) about_start_time = ImGui::get_current_time_msec();
        ShowAbout(about_start_time);
        int i = ImGui::GetCurrentWindow()->ContentSize.x;
        ImGui::Indent((i - 40.0f) * 0.5f);
        if (ImGui::Button("OK", ImVec2(40, 0))) { show_about = false; about_start_time = 0; ImGui::CloseCurrentPopup(); }
        ImGui::SetItemDefaultFocus();
        ImGui::EndPopup();
    }

    if (show_configure)
    {
        ImGui::OpenPopup(ICON_FA_WHMCS " Configure", ImGuiPopupFlags_AnyPopup);
    }
    if (multiviewport)
        ImGui::SetNextWindowViewport(viewport->ID);
    if (ImGui::BeginPopupModal(ICON_FA_WHMCS " Configure", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings))
    {
        ShowConfigure(g_new_setting);
        int i = ImGui::GetCurrentWindow()->ContentSize.x;
        ImGui::Indent((i - 140.0f) * 0.5f);
        if (ImGui::Button("OK", ImVec2(60, 0)))
        {
            show_configure = false;
            g_media_editor_settings = g_new_setting;
            if (timeline)
            {
                timeline->mHardwareCodec = g_media_editor_settings.HardwareCodec;
                timeline->mWidth = g_media_editor_settings.VideoWidth;
                timeline->mHeight = g_media_editor_settings.VideoHeight;
                timeline->mPreviewScale = g_media_editor_settings.PreviewScale;
                timeline->mFrameRate = g_media_editor_settings.VideoFrameRate;
                timeline->mMaxCachedVideoFrame = g_media_editor_settings.VideoFrameCacheSize > 0 ? g_media_editor_settings.VideoFrameCacheSize : MAX_VIDEO_CACHE_FRAMES;
                timeline->mAudioSampleRate = g_media_editor_settings.AudioSampleRate;
                timeline->mAudioChannels = g_media_editor_settings.AudioChannels;
                timeline->mAudioFormat = (MediaCore::AudioRender::PcmFormat)g_media_editor_settings.AudioFormat;
                timeline->mShowHelpTooltips = g_media_editor_settings.ShowHelpTooltips;
                timeline->mFontName = g_media_editor_settings.FontName;
            }
            ImGui::CloseCurrentPopup(); 
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(60, 0)))
        {
            show_configure = false;
            g_new_setting = g_media_editor_settings;
            ImGui::CloseCurrentPopup(); 
        }
        ImGui::EndPopup();
    }
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.f, 0.f, 0.f, 0.f));
    ImVec2 window_pos = ImGui::GetWindowPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    if (multiviewport)
    {
        window_pos = viewport->WorkPos;
        window_size = viewport->WorkSize;
    }

    ImGui::PushID("##Main_Timeline");
    float main_height = g_media_editor_settings.TopViewHeight * window_size.y;
    float timeline_height = g_media_editor_settings.BottomViewHeight * window_size.y;
    is_splitter_hold |= ImGui::Splitter(false, 4.0f, &main_height, &timeline_height, window_size.y / 2, scope_height + 16);
    g_media_editor_settings.TopViewHeight = main_height / window_size.y;
    g_media_editor_settings.BottomViewHeight = timeline_height / window_size.y;
    ImGui::PopID();
    ImVec2 main_pos = window_pos + ImVec2(4, 0);
    ImVec2 main_size(window_size.x, main_height + 4);
    ImGui::SetNextWindowPos(main_pos, ImGuiCond_Always);
    if (ImGui::BeginChild("##Top_Panel", main_size, false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings))
    {
        ImVec2 main_window_size = ImGui::GetWindowSize();
        ImGui::PushID("##Control_Panel_Main");
        float control_pane_width = g_media_editor_settings.ControlPanelWidth * main_window_size.x;
        float main_width = g_media_editor_settings.MainViewWidth * main_window_size.x;
        is_splitter_hold |= ImGui::Splitter(true, 4.0f, &control_pane_width, &main_width, media_icon_size + 256, main_window_size.x * 0.65);
        g_media_editor_settings.ControlPanelWidth = control_pane_width / main_window_size.x;
        g_media_editor_settings.MainViewWidth = main_width / main_window_size.x;
        ImGui::PopID();
        
        // add left tool bar
        ImGui::SetCursorPos(ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5, 0.5, 0.5, 0.5));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2, 0.2, 0.2, 1.0));
        if (ImGui::Button(ICON_FA_CIRCLE_INFO "##About", ImVec2(tool_icon_size, tool_icon_size)))
        {
            // Show About
            show_about = true;
        }
        ImGui::ShowTooltipOnHover("About Media Editor");
        if (ImGui::Button(ICON_OPEN_PROJECT "##OpenProject", ImVec2(tool_icon_size, tool_icon_size)))
        {
            // Open Project
            show_file_dialog = true;
            ImGuiFileDialog::Instance()->OpenDialog("##MediaEditFileDlgKey", ICON_IGFD_FOLDER_OPEN " Open Project File", 
                                                    pfilters.c_str(),
                                                    ".",
                                                    1, 
                                                    IGFDUserDatas("ProjectOpen"), 
                                                    fflags);
        }
        ImGui::ShowTooltipOnHover("Open Project ...");
        if (ImGui::Button(ICON_NEW_PROJECT "##NewProject", ImVec2(tool_icon_size, tool_icon_size)))
        {
            // New Project
            if (!project_need_save)
                NewProject();
            else if (g_media_editor_settings.project_path.empty())
            {
                show_file_dialog = true;
                ImGuiFileDialog::Instance()->OpenDialog("##MediaEditFileDlgKey", ICON_IGFD_FOLDER_OPEN " Save Project File", 
                                                    pfilters.c_str(),
                                                    ".",
                                                    1, 
                                                    IGFDUserDatas("ProjectSaveAndNew"), 
                                                    pflags);
            }
            else
            {
                // conform save to project file
                ImGui::OpenPopup(ICON_FA_CIRCLE_INFO " Save Project File", ImGuiPopupFlags_AnyPopup);
            }
        }
        ImGui::ShowTooltipOnHover("New Project");
        if (ImGui::Button(ICON_SAVE_PROJECT "##SaveProject", ImVec2(tool_icon_size, tool_icon_size)))
        {
            // Save Project
            show_file_dialog = true;
            ImGuiFileDialog::Instance()->OpenDialog("##MediaEditFileDlgKey", ICON_IGFD_FOLDER_OPEN " Save Project File", 
                                                    pfilters.c_str(),
                                                    ".",
                                                    1, 
                                                    IGFDUserDatas("ProjectSave"), 
                                                    pflags);
        }
        ImGui::ShowTooltipOnHover("Save Project As...");

        if (ImGui::Button(ICON_FA_WHMCS "##Configure", ImVec2(tool_icon_size, tool_icon_size)))
        {
            // Show Setting
            show_configure = true;
        }
        ImGui::ShowTooltipOnHover("Configure");
/*
        if (ImGui::Button(ICON_UI_DEBUG "##UIDebug", ImVec2(tool_icon_size, tool_icon_size)))
        {
            // open debug window
            show_debug = !show_debug;
        }
        ImGui::ShowTooltipOnHover("UI Debug");
*/

        if (ImGui::Button(ICON_FA_POWER_OFF "##Quit", ImVec2(tool_icon_size, tool_icon_size)))
        {
            // Mark to quit
            app_will_quit = true;
        }
        ImGui::ShowTooltipOnHover("Quit");
        ImGui::PopStyleColor(3);

        if (ImGui::BeginPopupModal(ICON_FA_CIRCLE_INFO " Save Project File", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings))
        {
            ImGui::TextUnformatted("Project need to save");
            ImGui::TextUnformatted("Do you need save current project?");
            if (ImGui::Button("OK", ImVec2(40, 0)))
            {
                SaveProject(g_media_editor_settings.project_path);
                ImGui::CloseCurrentPopup();
                NewProject();
            }
            ImGui::SameLine();
            if (ImGui::Button("NO", ImVec2(40, 0)))
            {
                ImGui::CloseCurrentPopup();
                NewProject();
            }
            ImGui::SetItemDefaultFocus();
            ImGui::EndPopup();
        }

        // add banks window
        ImVec2 bank_pos = window_pos + ImVec2(4 + tool_icon_size, 0);
        ImVec2 bank_size(control_pane_width - 4 - tool_icon_size, main_window_size.y - 4);
        if (!g_media_editor_settings.ExpandScope)
            bank_size -= ImVec2(0, scope_height + 32);
        ImGui::SetNextWindowPos(bank_pos, ImGuiCond_Always);
        if (ImGui::BeginChild("##Control_Panel_Window", bank_size, false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings))
        {
            ImVec2 bank_window_size = ImGui::GetWindowSize();
            ImGui::TabLabels(numControlPanelTabs, ControlPanelTabNames, ControlPanelIndex, ControlPanelTabTooltips , false, true, nullptr, nullptr, false, false, nullptr, nullptr);

            // make control panel area
            ImVec2 area_pos = window_pos + ImVec2(tool_icon_size + 4, 32);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, COL_DARK_ONE);
            ImGui::SetNextWindowPos(area_pos, ImGuiCond_Always);
            if (ImGui::BeginChild("##Control_Panel_content", bank_window_size - ImVec2(4, 32), false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings))
            {
                ImDrawList *draw_list = ImGui::GetWindowDrawList();
                switch (ControlPanelIndex)
                {
                    case 0: ShowMediaBankWindow(draw_list, media_icon_size); break;
                    case 1: 
                        switch (g_media_editor_settings.BankViewStyle)
                        {
                            case 0: ShowFilterBankIconWindow(draw_list); break;
                            case 1: ShowFilterBankTreeWindow(draw_list); break;
                            default: break;
                        }
                    break;
                    case 2: 
                        switch (g_media_editor_settings.BankViewStyle)
                        {
                            case 0: ShowFusionBankIconWindow(draw_list);; break;
                            case 1: ShowFusionBankTreeWindow(draw_list); break;
                            default: break;
                        }
                    break;
                    case 3: ShowMediaOutputWindow(draw_list); break;
                    default: break;
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();
        if (!g_media_editor_settings.ExpandScope)
        {
            bool _expanded = bank_size.x >= scope_height * 2 + 32;
            ImVec2 scope_pos = bank_pos + ImVec2(0, bank_size.y);
            ImVec2 scope_size = ImVec2(bank_size.x, scope_height + 32);
            ImGui::SetNextWindowPos(scope_pos, ImGuiCond_Always);
            if (ImGui::BeginChild("##Scope_View", scope_size, false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings))
            {
                ShowMediaAnalyseWindow(timeline, &_expanded);
                scope_flags = 1 << g_media_editor_settings.ScopeWindowIndex;
                if (_expanded)
                    scope_flags |= 1 << g_media_editor_settings.ScopeWindowExpandIndex;
            }
            ImGui::EndChild();
        }
        else
        {
            scope_flags = 0xFFFFFFFF;
            ShowMediaAnalyseWindow(timeline);
        }

        ImVec2 main_sub_pos = window_pos + ImVec2(control_pane_width + 8, 0);
        ImVec2 main_sub_size(main_width - 8, main_window_size.y - 4);
        ImGui::SetNextWindowPos(main_sub_pos, ImGuiCond_Always);
        if (ImGui::BeginChild("##Main_Window", main_sub_size, false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar))
        {
            // full background
            ImDrawList *draw_list = ImGui::GetWindowDrawList();
            if (ImGui::TabLabels(numMainWindowTabs, MainWindowTabNames, MainWindowIndex, MainWindowTabTooltips , false, true, nullptr, nullptr, false, false, nullptr, nullptr))
            {
                UIPageChanged();
            }
            // show meters
            if (g_media_editor_settings.showMeters)
            {
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(2) << ImGui::GetIO().DeltaTime * 1000.f << "ms/frame ";
                oss << ImGui::GetIO().Framerate << "FPS";
#if IMGUI_VULKAN_SHADER
                int device_count = ImGui::get_gpu_count();
                for (int i = 0; i < device_count; i++)
                {
                    ImGui::VulkanDevice* vkdev = ImGui::get_gpu_device(i);
                    std::string device_name = vkdev->info.device_name();
                    uint32_t gpu_memory_budget = vkdev->get_heap_budget();
                    uint32_t gpu_memory_usage = vkdev->get_heap_usage();
                    oss << " GPU[" << i << "]:" << device_name << " VRAM(" << gpu_memory_usage << "MB/" << gpu_memory_budget << "MB)";
                }
#endif
                std::string meters = oss.str();
                auto str_size = ImGui::CalcTextSize(meters.c_str());
                auto spos = main_sub_pos + ImVec2(main_sub_size.x - str_size.x - 8, 0);
                draw_list->AddText(spos, IM_COL32_WHITE, meters.c_str());
            }

            ImRect video_rect;
            auto wmin = main_sub_pos + ImVec2(0, 32);
            auto wmax = wmin + ImGui::GetContentRegionAvail() - ImVec2(8, 0);
            draw_list->AddRectFilled(wmin, wmax, IM_COL32_BLACK, 8.0, ImDrawFlags_RoundCornersAll);
            if (ImGui::BeginChild("##Main_Window_content", wmax - wmin, false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings))
            {
                switch (MainWindowIndex)
                {
                    case 0: ShowMediaPreviewWindow(draw_list, "Preview", video_rect); break;
                    case 1: ShowVideoEditorWindow(draw_list); break;
                    case 2: ShowAudioEditorWindow(draw_list); break;
                    case 3: ShowTextEditorWindow(draw_list); break;
                    default: break;
                }
            }
            ImGui::EndChild();
        }
        ImGui::EndChild();
    }
    ImGui::EndChild();
    
    bool _expanded = g_media_editor_settings.BottomViewExpanded;
    ImVec2 panel_pos = window_pos + ImVec2(4, g_media_editor_settings.TopViewHeight * window_size.y + 12);
    ImVec2 panel_size(window_size.x - 4, g_media_editor_settings.BottomViewHeight * window_size.y - 12);
    ImGui::SetNextWindowPos(panel_pos, ImGuiCond_Always);
    if (ImGui::BeginChild("##Timeline", panel_size, false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings))
    {
        ImDrawList *draw_list = ImGui::GetWindowDrawList();
        bool overExpanded = ExpendButton(draw_list, ImVec2(panel_pos.x + 2, panel_pos.y + 2), !_expanded);
        if (overExpanded && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            _expanded = !_expanded;
        ImGui::SetCursorScreenPos(panel_pos + ImVec2(32, 0));
        bool changed = DrawTimeLine(timeline,  &_expanded, !is_splitter_hold && !mouse_hold && !show_configure && !show_about && !show_file_dialog);
        project_need_save |= changed;
        if (g_media_editor_settings.BottomViewExpanded != _expanded)
        {
            if (!_expanded)
            {
                g_media_editor_settings.OldBottomViewHeight = g_media_editor_settings.BottomViewHeight;
                g_media_editor_settings.BottomViewHeight = 60.0f / window_size.y;
                g_media_editor_settings.TopViewHeight = 1 - g_media_editor_settings.BottomViewHeight;
            }
            else
            {
                g_media_editor_settings.BottomViewHeight = g_media_editor_settings.OldBottomViewHeight;
                g_media_editor_settings.TopViewHeight = 1.0f - g_media_editor_settings.BottomViewHeight;
            }
            g_media_editor_settings.BottomViewExpanded = _expanded;
        }
    }
    ImGui::EndChild();

    ImGui::PopStyleColor();
    ImGui::End();

    if (MainWindowIndex == 0)
    {
        // preview view
        if (MonitorIndexPreviewVideo != -1 && MonitorIndexPreviewVideo < platform_io.Monitors.Size)
        {
            std::string preview_window_lable = "Preview_view_windows" + std::to_string(MonitorIndexPreviewVideo);
            auto mon = platform_io.Monitors[MonitorIndexPreviewVideo];
            ImGui::SetNextWindowPos(mon.MainPos);
            ImGui::SetNextWindowSize(mon.MainSize);
            ImGui::Begin(preview_window_lable.c_str(), nullptr, flags | ImGuiWindowFlags_FullScreen);
            ShowVideoWindow(timeline->mMainPreviewTexture, mon.MainPos, mon.MainSize);
            ImGui::SetCursorScreenPos(mon.MainPos + ImVec2(80, 60));
            ImGui::TextComplex("Preview", 3.0f, ImVec4(0.8, 0.8, 0.8, 0.2),
                                0.1f, ImVec4(0.8, 0.8, 0.8, 0.3),
                                ImVec2(4, 4), ImVec4(0.0, 0.0, 0.0, 0.5));
            ImGui::End();
        }
    }
    else if (MainWindowIndex == 1 && VideoEditorWindowIndex == 0)
    {
        // video filter
        if (MonitorIndexVideoFilterOrg != -1 && MonitorIndexVideoFilterOrg < platform_io.Monitors.Size)
        {
            std::string view_window_lable = "video_filter_org_windows" + std::to_string(MonitorIndexVideoFilterOrg);
            auto mon = platform_io.Monitors[MonitorIndexVideoFilterOrg];
            ImGui::SetNextWindowPos(mon.MainPos);
            ImGui::SetNextWindowSize(mon.MainSize);
            ImGui::Begin(view_window_lable.c_str(), nullptr, flags | ImGuiWindowFlags_FullScreen);
            ShowVideoWindow(timeline->mVideoFilterInputTexture, mon.MainPos, mon.MainSize);
            ImGui::SetCursorScreenPos(mon.MainPos + ImVec2(80, 60));
            ImGui::TextComplex("Filter Input", 3.0f, ImVec4(0.8, 0.8, 0.8, 0.2),
                                0.1f, ImVec4(0.8, 0.8, 0.8, 0.3),
                                ImVec2(4, 4), ImVec4(0.0, 0.0, 0.0, 0.5));
            ImGui::End();
        }
        if (MonitorIndexVideoFiltered != -1 && MonitorIndexVideoFiltered < platform_io.Monitors.Size)
        {
            std::string view_window_lable = "video_filter_output_windows" + std::to_string(MonitorIndexVideoFiltered);
            auto mon = platform_io.Monitors[MonitorIndexVideoFiltered];
            ImGui::SetNextWindowPos(mon.MainPos);
            ImGui::SetNextWindowSize(mon.MainSize);
            ImGui::Begin(view_window_lable.c_str(), nullptr, flags | ImGuiWindowFlags_FullScreen);
            ShowVideoWindow(timeline->mVideoFilterOutputTexture, mon.MainPos, mon.MainSize);
            ImGui::SetCursorScreenPos(mon.MainPos + ImVec2(80, 60));
            ImGui::TextComplex(timeline->bFilterOutputPreview ? "Preview Output" : "Filter Output", 3.0f, ImVec4(0.8, 0.8, 0.8, 0.2),
                                0.1f, ImVec4(0.8, 0.8, 0.8, 0.3),
                                ImVec2(4, 4), ImVec4(0.0, 0.0, 0.0, 0.5));
            ImGui::End();
        }
    }

    if (multiviewport)
    {
        ImGui::PopStyleVar(2);
    }
    ImGui::PopStyleVar(2);
    // check save stage if app will quit
    if (app_will_quit && timeline)
    {
        if (project_need_save) // TODO::Dicky Check timeline changed later
        {
            if (quit_save_confirm || g_media_editor_settings.project_path.empty())
            {
                show_file_dialog = true;
                ImGuiFileDialog::Instance()->OpenDialog("##MediaEditFileDlgKey", ICON_IGFD_FOLDER_OPEN " Save Project File", 
                                                        pfilters.c_str(),
                                                        ".",
                                                        1, 
                                                        IGFDUserDatas("ProjectSaveQuit"), 
                                                        pflags);
            }
            else
            {
                SaveProject(g_media_editor_settings.project_path);
                app_done = app_will_quit;
            }
        }
        else
        {
            quit_save_confirm = false;
        }
    }
    // File Dialog
    ImVec2 minSize = ImVec2(600, 600);
	ImVec2 maxSize = ImVec2(FLT_MAX, FLT_MAX);
    if (multiviewport) ImGui::SetNextWindowViewport(viewport->ID);
    if (ImGuiFileDialog::Instance()->Display("##MediaEditReloadDlgKey", ImGuiWindowFlags_NoCollapse, minSize, maxSize))
    {
        if (ImGuiFileDialog::Instance()->IsOk())
        {
            auto file_path = ImGuiFileDialog::Instance()->GetFilePathName();
            auto userDatas = (MediaItem*)ImGuiFileDialog::Instance()->GetUserDatas();
            if (!ReloadMedia(file_path, userDatas))
            {
                pfd::message("Reload Failed", "Can't reload media, maybe it isn't the same file?",
                                    pfd::choice::ok,
                                    pfd::icon::error);
            }
            show_file_dialog = false;
        }
        ImGuiFileDialog::Instance()->Close();
    }
    if (multiviewport) ImGui::SetNextWindowViewport(viewport->ID);
    if (ImGuiFileDialog::Instance()->Display("##MediaEditFileDlgKey", ImGuiWindowFlags_NoCollapse, minSize, maxSize))
    {
        if (ImGuiFileDialog::Instance()->IsOk())
        {
            auto file_path = ImGuiFileDialog::Instance()->GetFilePathName();
            auto file_name = ImGuiFileDialog::Instance()->GetCurrentFileName();
            auto file_suffix = ImGuiFileDialog::Instance()->GetCurrentFileSuffix();
            auto userDatas = std::string((const char*)ImGuiFileDialog::Instance()->GetUserDatas());
            if (userDatas.compare("Media Source") == 0)
            {
                InsertMedia(file_path);
            }
            if (userDatas.compare("ProjectOpen") == 0)
            {
                if (!g_media_editor_settings.project_path.empty())
                {
                    SaveProject(g_media_editor_settings.project_path);
                }
                if (g_project_loading)
                {
                    if (g_loading_thread && g_loading_thread->joinable())
                        g_loading_thread->join();
                    g_project_loading = false;
                    g_loading_thread = nullptr;
                }
                CleanProject();
                set_context_in_splash = false;
                g_loading_thread = new std::thread(LoadThread, file_path, set_context_in_splash);
            }
            if (userDatas.compare("ProjectSave") == 0)
            {
                if (file_suffix.empty())
                    file_path += ".mep";
                SaveProject(file_path);
            }
            if (userDatas.compare("ProjectSaveAndNew") == 0)
            {
                if (file_suffix.empty())
                    file_path += ".mep";
                SaveProject(file_path);
                NewProject();
            }
            if (userDatas.compare("ProjectSaveQuit") == 0)
            {
                if (file_suffix.empty())
                    file_path += ".mep";
                SaveProject(file_path);
                app_done = true;
            }
            show_file_dialog = false;
        }
        else
        {
            auto userDatas = std::string((const char*)ImGuiFileDialog::Instance()->GetUserDatas());
            if (userDatas.compare("ProjectSaveQuit") == 0)
                app_done = true;
            show_file_dialog = false;
        }
        ImGuiFileDialog::Instance()->Close();
    }
    else if (!quit_save_confirm)
    {
        app_done = app_will_quit;
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        mouse_hold = false;
    }
    return app_done;
}

bool MediaEditor_Splash_Screen(void* handle, bool app_will_quit)
{
    static int32_t splash_start_time = ImGui::get_current_time_msec();
    auto& io = ImGui::GetIO();
    ImGuiContext& g = *GImGui;
    if (!g_media_editor_settings.UILanguage.empty() && g.LanguageName != g_media_editor_settings.UILanguage)
        g.LanguageName = g_media_editor_settings.UILanguage;
    ImGuiCond cond = ImGuiCond_None;
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | 
                            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize, cond);
    ImGui::Begin("MediaEditor Splash", nullptr, flags);
    auto draw_list = ImGui::GetWindowDrawList();
    bool title_finished = Show_Version(draw_list, splash_start_time);
    if (g_project_loading)
    {
        std::string load_str = "Project Loading...";
        auto loading_size = ImGui::CalcTextSize(load_str.c_str());
        float xoft = (io.DisplaySize.x - loading_size.x) / 2;
        float yoft = io.DisplaySize.y - loading_size.y - 32 - 8;
        draw_list->AddText(ImVec2(xoft, yoft), IM_COL32(255, 255, 255, 255), load_str.c_str());
        ImGui::SetCursorPos(ImVec2(4, io.DisplaySize.y - 32));
        ImGui::ProgressBar("##splash_progress", g_project_loading_percentage, 0.f, 1.f, "", ImVec2(io.DisplaySize.x - 16, 8), 
                                ImVec4(0.3f, 0.3f, 0.8f, 1.f), ImVec4(0.1f, 0.1f, 0.2f, 1.f), ImVec4(0.f, 0.f, 0.8f, 1.f));
    }

    ImGui::End();
    bool should_finished = title_finished && !g_project_loading;
    if (should_finished)
    {
        if (logo_texture) { ImGui::ImDestroyTexture(logo_texture); logo_texture = nullptr; }
        if (codewin_texture) { ImGui::ImDestroyTexture(codewin_texture); codewin_texture = nullptr; }
    }
    return should_finished;
}

void Application_Setup(ApplicationWindowProperty& property)
{
    auto exec_path = ImGuiHelper::exec_path();
    // add language
    property.language_path = 
#if defined(__APPLE__)
        exec_path + "../Resources/";
#elif defined(_WIN32)
        exec_path + "../languages/";
#elif defined(__linux__)
        exec_path + "../languages/";
#else
        std::string();
#endif
    icon_file = 
    property.icon_path =  
#if defined(__APPLE__)
        exec_path + "../Resources/mec_logo.png";
#elif defined(__linux__)
        //exec_path + "mec.png";
        exec_path + "../resources/mec_logo.png";
#elif defined(_WIN32)
        exec_path + "../resources/mec_logo.png";
#else
        std::string();
#endif

    property.name = APP_NAME;
    //property.viewport = false;
    property.docking = false;
    property.auto_merge = false;
    property.internationalize = true;
    //property.using_setting_path = false;
    //property.power_save = false;
    property.font_scale = 2.0f;
#if 1
    property.resizable = false;
    property.full_size = true;
    //property.full_screen = true;
#else
    property.width = DEFAULT_MAIN_VIEW_WIDTH;
    property.height = DEFAULT_MAIN_VIEW_HEIGHT;
#endif
    property.splash_screen_width = 800;
    property.splash_screen_height = 400;
    property.splash_screen_alpha = 0.95;
    property.application.Application_SetupContext = MediaEditor_SetupContext;
    property.application.Application_Initialize = MediaEditor_Initialize;
    property.application.Application_Finalize = MediaEditor_Finalize;
    property.application.Application_DropFromSystem = MediaEditor_DropFromSystem;
    property.application.Application_SplashScreen = MediaEditor_Splash_Screen;
    property.application.Application_Frame = MediaEditor_Frame;
}

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
#include <ImMaskCreator.h>
#if IMGUI_VULKAN_SHADER
#include <Histogram_vulkan.h>
#include <Waveform_vulkan.h>
#include <CIE_vulkan.h>
#include <Vector_vulkan.h>
#endif
#include "MediaTimeline.h"
#include "EventStackFilter.h"
#include "MediaEncoder.h"
#include "HwaccelManager.h"
#include "TextureManager.h"
#include "FFUtils.h"
#include "MatUtils.h"
#include "FontManager.h"
#include "Logger.h"
#include "DebugHelper.h"
#include <sstream>
#include <iomanip>
#include <getopt.h>

//#define DEBUG_IMGUI
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

static const std::vector<std::string> ConfigureTabNames = {
    "System",
    "Timeline",
    "Text"
};

static const std::vector<std::string> ControlPanelTabNames = {
    ICON_MEDIA_BANK " Media",
    ICON_MEDIA_FILTERS " Filters",
    ICON_MEDIA_TRANS " Transitions",
    ICON_MEDIA_OUTPUT " Output"
};

static const std::vector<std::string> ControlPanelTabTooltips = 
{
    "Media Bank",
    "Filter Bank",
    "Transition Bank",
    "Media Output"
};

static const std::vector<std::string> MediaBankTabNames = {
    ICON_MD_AUTO_AWESOME_MOTION " Bank",
    ICON_MD_FOLDER_OPEN " Browser"
};

static const std::vector<std::string> MediaBankTabTooltips = {
    "Bank",
    "File Browser"
};

static const std::vector<std::string> MainWindowTabNames = {
    ICON_MEDIA_PREVIEW " Preview",
    ICON_AUDIO_MIXING " Mixing"
};

static const std::vector<std::string> MainWindowTabTooltips = 
{
    "Media Preview",
    "Audio Mixing",
};

enum MainPage : int
{
    MAIN_PAGE_PREVIEW = 0,
    MAIN_PAGE_MIXING,
    MAIN_PAGE_CLIP_EDITOR,
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

static const char* ScopeWindowTabIcon[] = {
    ICON_HISTOGRAM,
    ICON_WAVEFORM,
    ICON_CIE,
    ICON_VECTOR,
    ICON_WAVE,
    ICON_AUDIOVECTOR,
    ICON_FFT,
    ICON_DB,
    ICON_DB_LEVEL,
    ICON_SPECTROGRAM,
};

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

static const std::vector<std::string> TextEditorTabNames = {
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

// Add by Jimmy, Start
typedef struct _method_property
{
    std::string name;
    std::string icon;
    uint32_t meaning; 
} method_property;

static const method_property SortMethodItems[] = {
    {"Import Date", ICON_SORT_ID, 0},
    {"Media Type", ICON_SORT_TYPE, 0},
    {"Media Name", ICON_SORT_NAME, 0}
};

static const method_property FilterMethodItems[] = {
    {"All", ICON_FILTER_NONE, 0},
    {"Video", ICON_MEDIA_VIDEO, MEDIA_VIDEO},
    {"Audio", ICON_MEDIA_AUDIO, MEDIA_AUDIO},
    {"Image", ICON_MEDIA_IMAGE, MEDIA_SUBTYPE_VIDEO_IMAGE},
    {"Text", ICON_MEDIA_TEXT, MEDIA_TEXT}
};
// Add by Jimmy, End

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
const std::string abbr_ffilters = "All Support Files{" + video_file_suffix + "," + audio_file_suffix + "," + image_file_suffix + "}";
const std::string pfilters = "Project files (*.mep){.mep},.*";

struct ImageSequenceSetting
{
    bool bPng {true};
    bool bJpg {true};
    bool bBmp {true};
    bool bTif {true};
    bool bTga {false};
    bool bWebP {false};
    // more...
    MediaCore::Ratio frame_rate {25000, 1000};
};

struct MediaEditorSettings
{
    std::string UILanguage {"Default"};     // UI Language
    float TopViewHeight {0.6};              // Top view height percentage
    float BottomViewHeight {0.4};           // Bottom view height percentage
    float ControlPanelWidth {0.3};          // Control panel view width percentage
    float MainViewWidth {0.7};              // Main view width percentage
    bool BottomViewExpanded {true};         // Timeline view expanded
    bool VideoFilterCurveExpanded {true};   // Video filter curve view expanded
    bool VideoTransitionCurveExpanded {true};   // Video Transition curve view expanded
    bool AudioFilterCurveExpanded {true};   // Audio filter curve view expanded
    bool AudioTransitionCurveExpanded {true};   // audio Transition curve view expanded
    bool TextCurveExpanded {true};          // Text curve view expanded
    float OldBottomViewHeight {0.4};        // Old Bottom view height, recorde at non-expanded
    bool showMeters {true};                 // show fps/GPU usage at top right of windows
    bool powerSaving {false};               // power saving mode of imgui in low refresh rate
    int  MediaBankViewType {0};             // Media bank view type, 0 = Media bank, 1 = embedded browser

    bool HardwareCodec {true};              // try HW codec
    int VideoWidth  {1920};                 // timeline Media Width
    int VideoHeight {1080};                 // timeline Media Height
    float PreviewScale {0.5};               // timeline Media Video Preview scale
    MediaCore::Ratio VideoFrameRate {25000, 1000};// timeline frame rate
    MediaCore::Ratio PixelAspectRatio {1, 1}; // timeline pixel aspect ratio
    int ColorSpaceIndex {1};                // timeline color space default is bt 709
    int ColorTransferIndex {0};             // timeline color transfer default is bt 709
    int VideoFrameCacheSize {10};           // timeline video cache size
    int AudioChannels {2};                  // timeline audio channels
    int AudioSampleRate {44100};            // timeline audio sample rate
    int AudioFormat {2};                    // timeline audio format 0=unknown 1=s16 2=f32
    std::string project_path;               // Editor Recently project file path
    int BankViewStyle {0};                  // Bank view style type, 0 = icons, 1 = tree vide, and ... 
    bool ShowHelpTooltips {false};          // Show UI help tool tips

    // clip filter editor layout
    float video_clip_timeline_height {0.5}; // video clip filter view timelime height
    float video_clip_timeline_width {0.5};  // video clip filter view timeline width

    // clip filter editor layout
    float audio_clip_timeline_height {0.5}; // audio clip filter view timelime height
    float audio_clip_timeline_width {0.5};  // audio clip filter view timeline width

    bool ExpandScope {true};
    bool SeparateScope {false};
    // Histogram Scope tools
    bool HistogramLog {false};
    bool HistogramSplited {true};
    bool HistogramYRGB  {true};
    float HistogramScale {0.05};

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

    // Image sequence
    ImageSequenceSetting image_sequence;

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
    MediaCore::Ratio OutputVideoPixelAspectRatio {1, 1};// custom setting
    int OutputVideoFrameRateIndex {-1};
    MediaCore::Ratio OutputVideoFrameRate {25000, 1000};// custom setting
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

    void SyncSettingsFromTimeline(TimeLine* tl)
    {
        auto& hMediaSettings = tl->mhMediaSettings;

        VideoWidth = hMediaSettings->VideoOutWidth();
        VideoHeight = hMediaSettings->VideoOutHeight();
        VideoFrameRate = hMediaSettings->VideoOutFrameRate();
        PreviewScale = tl->mPreviewScale;

        AudioChannels = hMediaSettings->AudioOutChannels();
        AudioSampleRate = hMediaSettings->AudioOutSampleRate();
        auto renderPcmFormat = MatUtils::ImDataType2PcmFormat(hMediaSettings->AudioOutDataType());
        if (renderPcmFormat == MediaCore::AudioRender::PcmFormat::UNKNOWN)
            throw std::runtime_error("Audio output data type is NOT SUPPORTED as render pcm format!");
        AudioFormat = (int)renderPcmFormat;
    }
};

static std::string ini_file = "Media_Editor.ini";
static std::string icon_file;
static std::vector<std::string> import_url;    // import file url from system drag
static short main_mon = 0;
static TimeLine * timeline = nullptr;
static ImTextureID codewin_texture = nullptr;
static ImTextureID logo_texture = nullptr;
static std::thread * g_loading_project_thread {nullptr};
static std::thread * g_loading_plugin_thread {nullptr};
static std::thread * g_env_scan_thread {nullptr};
static float g_project_loading_percentage {0};
static bool g_plugin_loading {false};
static bool g_plugin_loaded {false};
static bool g_project_loading {false};
static float g_plugin_loading_percentage {0};
static int g_plugin_loading_current_index {0};
static std::string g_plugin_loading_message;
static bool g_env_scanned = false;
static bool g_env_scanning = false;
static ImGui::TabLabelStyle * tab_style = &ImGui::TabLabelStyle::Get();
static MediaEditorSettings g_media_editor_settings;
static MediaEditorSettings g_new_setting;
static imgui_json::value g_project;
static bool g_vidEncSelChanged = true;
static std::vector<MediaCore::MediaEncoder::Description> g_currVidEncDescList;
static bool g_audEncSelChanged = true;
static std::vector<MediaCore::MediaEncoder::Description> g_currAudEncDescList;
static std::string g_encoderConfigErrorMessage;
static bool quit_save_confirm = true;
static bool project_need_save = false;
static bool project_changed = false;
static bool mouse_hold = false;
static uint32_t scope_flags = 0xFFFFFFFF;
static bool set_context_in_splash = false;
static ImGuiFileDialog embedded_filedialog; // Media Finder, embedded filedialog

static int ConfigureIndex = 0;              // default timeline setting
static int ControlPanelIndex = 0;           // default Media Bank window
static int BottomWindowIndex = 0;           // default Media Timeline window, no other view so far
static int MainWindowIndex = MAIN_PAGE_PREVIEW;             // default Media Preview window
static int LastMainWindowIndex = MAIN_PAGE_PREVIEW;
static int LastEditingWindowIndex = -1;

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

#define MATVIEW_WIDTH   256
#define MATVIEW_HEIGHT  256

static bool need_update_scope {false};
static bool need_update_preview {false};

static ImGui::ImMat mat_histogram;
static ImGui::ImMat histogram_mat;
static ImTextureID histogram_texture {nullptr};

static ImGui::ImMat mat_video_waveform;
static ImTextureID video_waveform_texture {nullptr};

static ImGui::ImMat mat_cie;
static ImTextureID cie_texture {nullptr};

static ImGui::ImMat mat_vector;
static ImTextureID vector_texture {nullptr};

static ImGui::ImMat wave_mat;
static ImTextureID wave_texture {nullptr};
static ImGui::ImMat fft_mat;
static ImTextureID fft_texture {nullptr};
static ImGui::ImMat db_mat;
static ImTextureID db_texture {nullptr};

static std::unordered_map<std::string, std::vector<FM::FontDescriptorHolder>> fontTable;
static std::vector<string> fontFamilies;     // system fonts

static std::string g_plugin_path = "";
static std::string g_language_path = "";
static std::string g_resource_path = "";

static void GetVersion(int& major, int& minor, int& patch, int& build)
{
    major = MEDIAEDITOR_VERSION_MAJOR;
    minor = MEDIAEDITOR_VERSION_MINOR;
    patch = MEDIAEDITOR_VERSION_PATCH;
    build = MEDIAEDITOR_VERSION_BUILD;
}

// static bool TimelineConfChanged(MediaEditorSettings &old_setting, MediaEditorSettings &new_setting)
// {
//     if (old_setting.VideoHeight != new_setting.VideoHeight || old_setting.VideoWidth != new_setting.VideoWidth ||
//         old_setting.PreviewScale != new_setting.PreviewScale || old_setting.PixelAspectRatio.den != new_setting.PixelAspectRatio.den ||
//         old_setting.PixelAspectRatio.num != new_setting.PixelAspectRatio.num || old_setting.VideoFrameRate.den != new_setting.VideoFrameRate.den ||
//         old_setting.VideoFrameRate.num != new_setting.VideoFrameRate.num || old_setting.ColorSpaceIndex != new_setting.ColorSpaceIndex ||
//         old_setting.ColorTransferIndex != new_setting.ColorTransferIndex || old_setting.HardwareCodec != new_setting.HardwareCodec ||
//         old_setting.AudioSampleRate != new_setting.AudioSampleRate || old_setting.AudioChannels != new_setting.AudioChannels ||old_setting.AudioFormat != new_setting.AudioFormat)
//         return true;
//     return false;
// }

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
    if (LastMainWindowIndex == MAIN_PAGE_PREVIEW && MainWindowIndex != MAIN_PAGE_PREVIEW)
    {
        // we leave video preview windows, stop preview play
        Logger::Log(Logger::DEBUG) << "[Changed page] leaving video preview page!!!" << std::endl;
        need_update_scope = true;
        if (timeline)
        {
            timeline->bPreviewing = false;
            timeline->Play(false);
        }
    }
    if (LastMainWindowIndex == MAIN_PAGE_CLIP_EDITOR && MainWindowIndex != MAIN_PAGE_CLIP_EDITOR)
    {
        // we leave editing windows
        Logger::Log(Logger::DEBUG) << "[Changed page] leaving editing page!!!" << std::endl;
        LastEditingWindowIndex = -1;
        timeline->Seek(timeline->mCurrentTime);
        timeline->UpdatePreview();
    }

    if (LastMainWindowIndex == MAIN_PAGE_MIXING && MainWindowIndex != MAIN_PAGE_MIXING)
    {
        // we leave audio mixing editor windows
        Logger::Log(Logger::DEBUG) << "[Changed page] leaving audio mixing editor page!!!" << std::endl;
    }

    if ((MainWindowIndex == MAIN_PAGE_CLIP_EDITOR && LastMainWindowIndex != MAIN_PAGE_CLIP_EDITOR) || 
        (MainWindowIndex == MAIN_PAGE_CLIP_EDITOR && LastMainWindowIndex == MAIN_PAGE_CLIP_EDITOR && LastEditingWindowIndex == -1))
    {
        // we enter editing windows
        Logger::Log(Logger::DEBUG) << "[Changed page] Enter editing page!!!" << std::endl;
        LastEditingWindowIndex = timeline->mSelectedItem;
    }
    else if (MainWindowIndex == MAIN_PAGE_CLIP_EDITOR && LastMainWindowIndex == MAIN_PAGE_CLIP_EDITOR && LastEditingWindowIndex != timeline->mSelectedItem)
    {
        // we changed editing page
        Logger::Log(Logger::DEBUG) << "[Changed page] Change editing page!!!" << std::endl;
        LastEditingWindowIndex = timeline->mSelectedItem;
        if (timeline->mSelectedItem != -1)
        {
            auto item = timeline->mEditingItems[timeline->mSelectedItem];
            if (item)
            {
                int64_t seek_time = -1;
                if (item->mEditorType == EDITING_CLIP && item->mEditingClip)
                {
                    seek_time = item->mEditingClip->mCurrentTime != -1 ? item->mEditingClip->mStart + item->mEditingClip->mCurrentTime : -1;
                }
                else if (item->mEditorType == EDITING_TRANSITION && item->mEditingOverlap)
                {
                    seek_time = item->mEditingOverlap->mCurrentTime != -1 ? item->mEditingOverlap->mStart + item->mEditingOverlap->mCurrentTime : -1;
                }
                if (seek_time != -1)
                {
                    timeline->Seek(seek_time);
                }
            }
        }
    }
    else if (MainWindowIndex == MAIN_PAGE_CLIP_EDITOR && timeline->mSelectedItem == -1)
    {
        MainWindowIndex = MAIN_PAGE_PREVIEW;
        LastEditingWindowIndex = -1;
        timeline->Seek(timeline->mCurrentTime);
        timeline->UpdatePreview();
    }

    if (MainWindowIndex == MAIN_PAGE_MIXING && LastMainWindowIndex != MAIN_PAGE_MIXING)
    {
        // we enter audio mixing editor windows
        Logger::Log(Logger::DEBUG) << "[Changed page] Enter audio mixing editor page!!!" << std::endl;
    }
    
    LastMainWindowIndex = MainWindowIndex;
    return updated;
}

static int EditingClip(int type, void* handle)
{
    if (timeline && timeline->mSelectedItem != -1)
    {
        MainWindowIndex = MAIN_PAGE_CLIP_EDITOR;
    }
    else
    {
        MainWindowIndex = MAIN_PAGE_PREVIEW;
    }
    auto updated = UIPageChanged();
    return updated ? 1 : 0;
}

static int EditingOverlap(int type, void* handle)
{
    if (timeline && timeline->mSelectedItem != -1)
    {
        MainWindowIndex = MAIN_PAGE_CLIP_EDITOR;
    }
    else
    {
        MainWindowIndex = MAIN_PAGE_PREVIEW;
    }
    auto updated = UIPageChanged();
    return updated ? 1 : 0;
}

// Utils functions
static bool ExpandButton(ImDrawList *draw_list, ImVec2 pos, bool expand = true, bool icon_mirror = false)
{
    ImGuiIO &io = ImGui::GetIO();
    ImRect delRect(pos, ImVec2(pos.x + 16, pos.y + 16));
    bool overDel = delRect.Contains(io.MousePos);
    ImU32 delColor = IM_COL32_WHITE;
    //        a ----- c
    //        | \   / |
    //        |   e   |
    //        | /   \ |
    //        b ----- d
    ImVec2 tra = pos + ImVec2(0, 0);
    ImVec2 trb = pos + ImVec2(0, 16);
    ImVec2 trc = pos + ImVec2(16, 0);
    ImVec2 trd = pos + ImVec2(16, 16);
    ImVec2 tre = pos + ImVec2(8, 8);
    if (!expand) icon_mirror ? draw_list->AddTriangleFilled(trc, trd, tre, delColor) : draw_list->AddTriangleFilled(tra, trb, tre, delColor);
    else draw_list->AddTriangleFilled(tra, tre, trc, delColor);
    return overDel;
}

static void ShowVideoWindow(ImDrawList *draw_list, ImTextureID texture, ImVec2 pos, ImVec2 size, std::string title, float title_size, float& offset_x, float& offset_y, float& tf_x, float& tf_y, bool bLandscape = true, bool out_border = false, const ImVec2& uvMin = ImVec2(0, 0), const ImVec2& uvMax = ImVec2(1, 1))
{
    if (texture)
    {        
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
            uvMin,
            uvMax
        );
        
        tf_x = offset_x + adj_w;
        tf_y = offset_y + adj_h;
        if (!title.empty() && title_size > 0)
        {
            draw_list->AddTextComplex(ImVec2(offset_x, offset_y) + ImVec2(20, 10),
                                title.c_str(), title_size, IM_COL32(224, 224, 224, 128),
                                0.25f, IM_COL32(128, 128, 128, 255),
                                ImVec2(title_size * 1.5, title_size * 1.5), IM_COL32(32, 32, 32, 255));
        }
        if (out_border)
        {
            draw_list->AddTextComplex(ImVec2(offset_x, offset_y + adj_h - 48) + ImVec2(20, 10),
                                "Out of range", title_size, IM_COL32(224, 0, 0, 224),
                                0.25f, IM_COL32(128, 128, 128, 255),
                                ImVec2(title_size * 1.5, title_size * 1.5), IM_COL32(32, 32, 32, 255));
        }
        ImGui::SetCursorScreenPos(pos);
        ImGui::InvisibleButton(("##video_window" + std::to_string((long long)texture)).c_str(), size);
    }
}

static void ShowVideoWindow(ImTextureID texture, ImVec2 pos, ImVec2 size, std::string title = std::string(), float title_size = 0.f, bool out_border = false, const ImVec2& uvMin = ImVec2(0, 0), const ImVec2& uvMax = ImVec2(1, 1))
{
    float offset_x = 0, offset_y = 0;
    float tf_x = 0, tf_y = 0;
    ShowVideoWindow(ImGui::GetWindowDrawList(), texture, pos, size, title, title_size, offset_x, offset_y, tf_x, tf_y, true, out_border, uvMin, uvMax);
}

static void CalculateVideoScope(ImGui::ImMat& mat)
{
#if IMGUI_VULKAN_SHADER
    if (m_histogram && (scope_flags & SCOPE_VIDEO_HISTOGRAM | need_update_scope)) m_histogram->scope(mat, mat_histogram, 256, g_media_editor_settings.HistogramScale, g_media_editor_settings.HistogramLog);
    if (m_waveform && (scope_flags & SCOPE_VIDEO_WAVEFORM | need_update_scope)) m_waveform->scope(mat, mat_video_waveform, 256, g_media_editor_settings.WaveformIntensity, g_media_editor_settings.WaveformSeparate, g_media_editor_settings.WaveformShowY);
    if (m_cie && (scope_flags & SCOPE_VIDEO_CIE | need_update_scope)) m_cie->scope(mat, mat_cie, g_media_editor_settings.CIEIntensity, g_media_editor_settings.CIEShowColor);
    if (m_vector && (scope_flags & SCOPE_VIDEO_VECTOR | need_update_scope)) m_vector->scope(mat, mat_vector, g_media_editor_settings.VectorIntensity);
#endif
    need_update_scope = false;
}

static bool MonitorButton(const char * label, ImVec2 pos, int& monitor_index, std::vector<int> disabled_index)
{
    static std::string monitor_icons[] = {ICON_ONE, ICON_TWO, ICON_THREE, ICON_FOUR, ICON_FIVE, ICON_SIX, ICON_SEVEN, ICON_EIGHT, ICON_NINE};
    auto platform_io = ImGui::GetPlatformIO();
    ImGuiViewportP* viewport = (ImGuiViewportP*)ImGui::GetWindowViewport();
    auto current_monitor = viewport->PlatformMonitor;
    int org_index = monitor_index;
    ImGui::SetCursorScreenPos(pos);
    auto show_monitor_tooltips = [&](int index)
    {
        ImGuiPlatformMonitor& mon = index >= 0 ? platform_io.Monitors[index] : platform_io.Monitors[current_monitor];
        ImGui::SetNextWindowViewport(viewport->ID);
        if (ImGui::BeginTooltip())
        {
            ImGui::BulletText("Monitor #%d:", index + 1);
            ImGui::Text("DPI %.0f", mon.DpiScale * 100.0f);
            ImGui::Text("MainSize (%.0f,%.0f)", mon.MainSize.x, mon.MainSize.y);
            ImGui::Text("WorkSize (%.0f,%.0f)", mon.WorkSize.x, mon.WorkSize.y);
            ImGui::Text("MainMin (%.0f,%.0f)",  mon.MainPos.x,  mon.MainPos.y);
            ImGui::Text("MainMax (%.0f,%.0f)",  mon.MainPos.x + mon.MainSize.x, mon.MainPos.y + mon.MainSize.y);
            ImGui::Text("WorkMin (%.0f,%.0f)",  mon.WorkPos.x,  mon.WorkPos.y);
            ImGui::Text("WorkMax (%.0f,%.0f)",  mon.WorkPos.x + mon.WorkSize.x, mon.WorkPos.y + mon.WorkSize.y);
            ImGui::EndTooltip();
        }
    };
    ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.2, 0.7, 0.2, 1.0));
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2, 0.5, 0.2, 1.0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.2, 0.7, 0.2, 1.0));
    ImGui::PushItemWidth(24);
    std::string selected_label = monitor_icons[monitor_index < 0 ? current_monitor : monitor_index];
    if (ImGui::BeginCombo(label, selected_label.c_str(), ImGuiComboFlags_NoArrowButton))
    {
        for (int i = 0; i < platform_io.Monitors.Size; i++)
        {
            bool is_selected = monitor_icons[i] == selected_label;
            bool is_disable = false;
            for (auto disabled : disabled_index)
            {
                if (disabled != -1 && disabled == i)
                    is_disable = true;
            }
            if (g_media_editor_settings.SeparateScope && current_monitor == i)
                is_disable = true;
            
            //if (is_disable) continue;
            ImGui::BeginDisabled(is_disable);
            if (ImGui::Selectable(monitor_icons[i].c_str(), is_selected))
            {
                if (i == current_monitor)
                    monitor_index = -1;
                else
                    monitor_index = i;
                MonitorIndexChanged = true;
            }
            ImGui::EndDisabled();

            if (is_selected)
            {
                ImGui::SetItemDefaultFocus();
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            {
                show_monitor_tooltips(i);
            }
        }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();
    ImGui::PopStyleColor(4);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    {
        show_monitor_tooltips(monitor_index);
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
        std::string str = "Media Editor";
        auto mark_size = ImGui::CalcTextSize(str.c_str());
        float xoft = (logo_texture ? 32 + 256 : 0) + (window_size.x - mark_size.x - (logo_texture ? 256 : 0)) / 2;
        float yoft = (window_size.y - mark_size.y - 32) / 2 - 32;
        draw_list->AddTextComplex(window_pos + ImVec2(xoft, yoft), str.c_str(), 4.f, IM_COL32(255, 255, 255, title_alpha * 255), 0.f, 0, ImVec2(4, 4), IM_COL32(0, 0, 0, 255));
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
    ImGui::Text("Frames since last update: %d", ImGui::GetIO().FrameCountSinceLastUpdate);
    ImGui::Text("Frames since last event: %d", ImGui::GetIO().FrameCountSinceLastEvent);
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

static int GetPixelAspectRatioIndex(MediaCore::Ratio ratio)
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

static void SetPixelAspectRatio(MediaCore::Ratio& ratio, int index)
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

static int GetVideoFrameIndex(MediaCore::Ratio fps)
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

static void SetVideoFrameRate(MediaCore::Ratio & rate, int index)
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
    static int resolution_index, preview_scale_index, pixel_aspect_index, frame_rate_index, sample_rate_index, channels_index, format_index;
    resolution_index = GetResolutionIndex(config.VideoWidth, config.VideoHeight);
    preview_scale_index = GetPreviewScaleIndex(config.PreviewScale);
    pixel_aspect_index = GetPixelAspectRatioIndex(config.PixelAspectRatio);
    frame_rate_index = GetVideoFrameIndex(config.VideoFrameRate);
    sample_rate_index = GetSampleRateIndex(config.AudioSampleRate);
    channels_index = GetChannelIndex(config.AudioChannels);
    format_index = GetAudioFormatIndex(config.AudioFormat);

    static char buf_cache_size[64] = {0}; snprintf(buf_cache_size, 64, "%d", config.VideoFrameCacheSize);
    static char buf_res_x[64] = {0}; snprintf(buf_res_x, 64, "%d", config.VideoWidth);
    static char buf_res_y[64] = {0}; snprintf(buf_res_y, 64, "%d", config.VideoHeight);
    static char buf_par_x[64] = {0}; snprintf(buf_par_x, 64, "%d", config.PixelAspectRatio.num);
    static char buf_par_y[64] = {0}; snprintf(buf_par_y, 64, "%d", config.PixelAspectRatio.den);
    static char buf_fmr_x[64] = {0}; snprintf(buf_fmr_x, 64, "%d", config.VideoFrameRate.num);
    static char buf_fmr_y[64] = {0}; snprintf(buf_fmr_y, 64, "%d", config.VideoFrameRate.den);

    if (ImGui::BeginChild("##ConfigureView", ImVec2(800, 600), false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings))
    {
        ImVec2 table_size;
        ImGui::TabLabels(ConfigureTabNames, ConfigureIndex, table_size, std::vector<std::string>() , false, false, nullptr, nullptr, false, false, nullptr, nullptr);
        switch (ConfigureIndex)
        {
            case 0:
            // system setting
            {
                ImGuiContext& g = *GImGui;
                if (g.LanguagesLoaded && !g.StringMap.empty())
                {
                    const char* language_name = config.UILanguage.c_str();
                    ImGui::BulletText("UI Language");
                    // ImGui::TextUnformatted("UI Language");
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

                ImGui::BulletText("Show UI Help Tips");
                // ImGui::TextUnformatted("Show UI Help Tips");
                ImGui::ToggleButton("##show_ui_help_tooltips", &config.ShowHelpTooltips);
                ImGui::Separator();
                ImGui::BulletText("Show UI Meters");
                // ImGui::TextUnformatted("Show UI Meters");
                ImGui::ToggleButton("##show_ui_meters", &config.showMeters);
                ImGui::Separator();
                ImGui::BulletText("UI PowerSaving");
                ImGui::ToggleButton("##ui_power_saving", &config.powerSaving);
                ImGui::Separator();
                ImGui::BulletText("Bank View Style");
                // ImGui::TextUnformatted("Bank View Style");
                ImGui::RadioButton("Icons",  (int *)&config.BankViewStyle, 0); ImGui::SameLine();
                ImGui::RadioButton("Tree",  (int *)&config.BankViewStyle, 1);
                ImGui::BulletText("Video Frame Cache Size");
                // ImGui::TextUnformatted("Video Frame Cache Size");
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
                    config.PixelAspectRatio.den = atoi(buf_par_y);
                    if (config.PixelAspectRatio.den) config.PixelAspectRatio.den = 1;
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
                    config.VideoFrameRate.den = atoi(buf_fmr_y);
                    if (config.VideoFrameRate.den == 0) config.VideoFrameRate.den = 1;
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
            }
            break;
            case 2:
            {
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

static inline void ImgSeuqPane(const char* vFilter, const char* currentPath, IGFDUserDatas vUserDatas, bool* vCantContinue)
{
    auto file_suffix = ImGuiHelper::path_filename_suffix(std::string(currentPath));
    bool isDirectory = file_suffix.empty();
    ImageSequenceSetting* setting = &g_media_editor_settings.image_sequence;
    if (setting)
    {
        ImGui::BeginDisabled(!isDirectory);
        static int frame_rate_index = GetVideoFrameIndex(setting->frame_rate);
        static char buf_fmr_x[64] = {0}; snprintf(buf_fmr_x, 64, "%d", setting->frame_rate.num);
        static char buf_fmr_y[64] = {0}; snprintf(buf_fmr_y, 64, "%d", setting->frame_rate.den);
        ImGui::TextUnformatted("Image Sequence Setting:");
        ImGui::Checkbox("png", &setting->bPng);
        ImGui::Checkbox("jpg", &setting->bJpg);
        ImGui::Checkbox("bmp", &setting->bBmp);
        ImGui::Checkbox("tif", &setting->bTif);
        ImGui::Checkbox("tga", &setting->bTga);
        ImGui::Checkbox("webp", &setting->bWebP);
        ImGui::TextUnformatted("Image Sequence Frame Rate:");
        if (ImGui::Combo("##Image Frame Rate", &frame_rate_index, frame_rate_items, IM_ARRAYSIZE(frame_rate_items)))
        {
            SetVideoFrameRate(setting->frame_rate, frame_rate_index);
        }
        ImGui::BeginDisabled(frame_rate_index != 0);
        ImGui::PushItemWidth(60);
        ImGui::InputText("##ImageFrameRate_x", buf_fmr_x, 64, ImGuiInputTextFlags_CharsDecimal);
        ImGui::SameLine();
        ImGui::TextUnformatted(":");
        ImGui::SameLine();
        ImGui::InputText("##ImageFrameRate_y", buf_fmr_y, 64, ImGuiInputTextFlags_CharsDecimal);
        ImGui::PopItemWidth();
        ImGui::EndDisabled();
        if (frame_rate_index == 0)
        {
            setting->frame_rate.num = atoi(buf_fmr_x);
            setting->frame_rate.den = atoi(buf_fmr_y);
            if (setting->frame_rate.den == 0) setting->frame_rate.den = 1;
        }
        ImGui::EndDisabled();
        if (isDirectory)
        {
            std::vector<std::string> files, file_names;
            std::vector<std::string> filter;
            if (setting->bPng) filter.push_back("png");
            if (setting->bJpg) { filter.push_back("jpg"); filter.push_back("jpeg"); }
            if (setting->bBmp) filter.push_back("bmp");
            if (setting->bTif) { filter.push_back("tif"); filter.push_back("tiff"); }
            if (setting->bTga) filter.push_back("tga");
            if (setting->bWebP) filter.push_back("webp");
            if (filter.empty())
            {
                *vCantContinue = false;
                return;
            }
            int ret = DIR_Iterate(currentPath, files, file_names, filter, false, false, false, true);
            if (ret == -2)
                *vCantContinue = true;
            else
                *vCantContinue = false;
        }
    }
}

// Document Framework
static void NewTimeline()
{
    timeline = new TimeLine(g_plugin_path);
    if (timeline)
    {
        g_media_editor_settings.SyncSettingsFromTimeline(timeline);
        timeline->mHardwareCodec = g_media_editor_settings.HardwareCodec;
        timeline->mMaxCachedVideoFrame = g_media_editor_settings.VideoFrameCacheSize > 0 ? g_media_editor_settings.VideoFrameCacheSize : MAX_VIDEO_CACHE_FRAMES;
        timeline->mShowHelpTooltips = g_media_editor_settings.ShowHelpTooltips;
        timeline->mAudioAttribute.mAudioSpectrogramLight = g_media_editor_settings.AudioSpectrogramLight;
        timeline->mAudioAttribute.mAudioSpectrogramOffset = g_media_editor_settings.AudioSpectrogramOffset;
        timeline->mAudioAttribute.mAudioVectorScale = g_media_editor_settings.AudioVectorScale;
        timeline->mAudioAttribute.mAudioVectorMode = g_media_editor_settings.AudioVectorMode;
        timeline->mFontName = g_media_editor_settings.FontName;

        // init callbacks
        timeline->m_CallBacks.EditingClip = EditingClip;
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
    project_changed = false;
}

static void LoadProjectThread(std::string path, bool in_splash)
{
    // waiting plugin loading
    while (g_plugin_loading || !g_plugin_loaded || g_env_scanning || !g_env_scanned)
    {
        ImGui::sleep(100);
    }
    if (g_env_scan_thread && g_env_scan_thread->joinable())
        g_env_scan_thread->join();
    g_project_loading = true;
    g_project_loading_percentage = 0;
    Logger::Log(Logger::DEBUG) << "[MEC] Load project from '" << path << "'." << std::endl;
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
        g_media_editor_settings.SyncSettingsFromTimeline(timeline);
    }

    g_media_editor_settings.project_path = path;
    quit_save_confirm = false;
    project_need_save = true;
    project_changed = false;
    g_project_loading_percentage = 1.0;
    g_project_loading = false;
    timeline->m_in_threads = false;
}

static void SaveProject(const std::string& path)
{
    if (!timeline || path.empty())
        return;

    Logger::Log(Logger::DEBUG) << "[Project] Save project to file!!!" << std::endl;

    timeline->Play(false, true);

    // first save media bank info
    imgui_json::value media_bank;
    for (auto media : timeline->media_items)
    {
        imgui_json::value item;
        item["id"] = imgui_json::number(media->mID);
        item["name"] = media->mName;
        item["path"] = media->mPath;
        item["type"] = imgui_json::number(media->mMediaType);
        media_bank.push_back(item);
    }
    g_project["MediaBank"] = media_bank;

    // second save Timeline
    imgui_json::value timeline_val;
    timeline->Save(timeline_val);
    g_project["TimeLine"] = timeline_val;

    g_project.save(path);
    g_media_editor_settings.project_path = path;
    project_need_save = false;
    project_changed = false;
    timeline->mIsBluePrintChanged = false;
}

static void OpenProject(const std::string& projectPath)
{
    if (!g_media_editor_settings.project_path.empty())
    {
        SaveProject(g_media_editor_settings.project_path);
    }
    if (g_project_loading)
    {
        if (g_loading_project_thread && g_loading_project_thread->joinable())
            g_loading_project_thread->join();
        g_project_loading = false;
        g_loading_project_thread = nullptr;
    }
    CleanProject();
    set_context_in_splash = false;
    g_loading_project_thread = new std::thread(LoadProjectThread, projectPath, set_context_in_splash);
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

static bool InsertMedia(const std::string path, bool bIsImageSeq = false)
{
    auto file_suffix = ImGuiHelper::path_filename_suffix(path);
    auto name = ImGuiHelper::path_filename(path);
    auto type = bIsImageSeq ? MEDIA_SUBTYPE_VIDEO_IMAGE_SEQUENCE : EstimateMediaType(file_suffix);
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
    auto file_suffix = ImGuiHelper::path_filename_suffix(path);
    auto name = ImGuiHelper::path_filename(path);
    // check type match
    auto type = EstimateMediaType(file_suffix);
    if (type != item->mMediaType)
        return false;
    auto old_name = item->mName;
    auto old_path = item->mPath;
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
                    MediaCore::Snapshot::Viewer::Holder hViewer;
                    MediaCore::Snapshot::Generator::Holder hSsGen = timeline->GetSnapshotGenerator(item->mID);
                    if (hSsGen)
                    {
                        hViewer = hSsGen->CreateViewer();
                        new_clip->UpdateClip(item->mMediaOverview->GetMediaParser(), hViewer, item->mSrcLength);
                    }
                    // update video snapshot
                    if (!IS_DUMMY(new_clip->mType))
                        new_clip->CalcDisplayParams();
                }
                else if (IS_AUDIO(clip->mType))
                {
                    AudioClip * new_clip = (AudioClip *)clip;
                    new_clip->UpdateClip(item->mMediaOverview, item->mSrcLength);
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
                        MediaCore::VideoTrack::Holder vidTrack = timeline->mMtvReader->GetTrackById(track->mID);
                        if (vidTrack)
                        {
                            MediaCore::VideoClip::Holder hVidClip;
                            if (IS_IMAGE(clip->mType))
                                hVidClip = vidTrack->AddImageClip(clip->mID, clip->mMediaParser, clip->Start(), clip->Length());
                            else
                                hVidClip = vidTrack->AddVideoClip(clip->mID, clip->mMediaParser, clip->Start(), clip->End(), clip->StartOffset(), clip->EndOffset(), timeline->mCurrentTime - clip->Start());
                            VideoClip* vclip = dynamic_cast<VideoClip*>(clip);
                            vclip->SyncFilterWithDataLayer(hVidClip);
                            vclip->SyncAttributesWithDataLayer(hVidClip);
                        }
                        clip->SetViewWindowStart(timeline->firstTime);
                    }
                    else if (IS_AUDIO(clip->mType))
                    {
                        MediaCore::AudioTrack::Holder audTrack = timeline->mMtaReader->GetTrackById(track->mID);
                        if (audTrack)
                        {
                            MediaCore::AudioClip::Holder hAudClip = audTrack->AddNewClip(clip->mID, clip->mMediaParser, clip->Start(), clip->End(), clip->StartOffset(), clip->EndOffset());
                            AudioClip* aclip = dynamic_cast<AudioClip*>(clip);
                            aclip->SyncFilterWithDataLayer(hAudClip);
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
        }
        // update preview
        timeline->UpdatePreview();
    }
    else
    {
        item->ReleaseItem();
        item->mName = old_name;
        item->mPath = old_path;
        return false;
    }
    return updated;
}

static void StopTimelineMediaPlay()
{
    if (timeline)
    {
        for (auto media : timeline->media_items) media->mSelected = false;
        timeline->mMediaPlayer->Close();
    }
}

static void ShowMediaPlayWindow(bool &show)
{
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList * draw_list = ImGui::GetWindowDrawList();
    ImVec2 window_pos = ImGui::GetWindowPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    ImVec2 video_size = window_size - ImVec2(4, 60);
    auto& player = timeline->mMediaPlayer;
    bool isFileOpened = player->IsOpened();
    static bool isFullscreen = false;
    float playPos = 0;
    float mediaDur = 0;
    std::string media_url = isFileOpened ? player->GetUrl() : "";
    if (player->HasVideo())
        mediaDur = player->GetVideoDuration();
    else if (player->HasAudio())
        mediaDur = player->GetAudioDuration();

    playPos = player->GetCurrentPos();
    if (playPos < 0) playPos = 0;
    if (playPos > mediaDur)
    {
        playPos = mediaDur;
        player->Pause();
    }

    if (!isFullscreen)
    {
        draw_list->AddRectFilled(window_pos, ImVec2(window_pos.x + window_size.x - 4, window_pos.y + window_size.y - 60), IM_COL32(0, 0, 0, 255));
        ImTextureID tid = player->GetFrame(playPos);
        if (tid)
        {
            ImGui::ImShowVideoWindow(draw_list, tid, window_pos, video_size);
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                ImGui::OpenPopup(ICON_MEDIA_PREVIEW " MediaPlay FullScreen", ImGuiPopupFlags_AnyPopup);
                isFullscreen = true;
            }
        }
        else
            ImGui::Dummy(video_size);
        // player controller
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::BeginDisabled(!isFileOpened);
        std::string playBtnLabel = player->IsPlaying() ? ICON_STOP : ICON_PLAY_FORWARD;
        ImGui::SetCursorScreenPos(ImVec2(window_pos.x + window_size.x / 2 - 15, window_pos.y + window_size.y - 56));
        if (ImGui::Button(playBtnLabel.c_str()))
        {
            if (player->IsPlaying())
                player->Pause();
            else
                player->Play();
        }

        ImGui::Spacing();
        ImGui::SetNextItemWidth(window_size.x);
        if (ImGui::SliderFloat("##TimePosition", &playPos, 0, mediaDur, ""))
            player->Seek(playPos, true);
        if (player->IsSeeking() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            player->Seek(playPos, false);

        // Time stamp on left of control panel
        ImGui::SetCursorScreenPos(ImVec2(window_pos.x + 48, window_pos.y + window_size.y - 48));
        auto time_str = ImGuiHelper::MillisecToString(playPos * 1000, 3);
        auto dur_str = ImGuiHelper::MillisecToString(mediaDur * 1000, 3);
        auto show_text = time_str + "/" + dur_str;
        ImGui::TextUnformatted(show_text.c_str());
        ImGui::EndDisabled();

        std::string media_status_icon;
        bool could_be_added = false;
        bool is_in_timeline = false;
        if (media_url.empty())
        {
            media_status_icon = ICON_FA_PLUG_CIRCLE_XMARK;
            could_be_added = true;
        }
        else if (timeline->isURLInTimeline(media_url))
        {
            media_status_icon = ICON_FA_PLUG_CIRCLE_BOLT;
            is_in_timeline = true;
        }
        else if (timeline->isURLInMediaBank(media_url))
        {
            media_status_icon = ICON_FA_PLUG_CIRCLE_CHECK;
        }
        else
        {
            media_status_icon = ICON_FA_PLUG_CIRCLE_XMARK;
            could_be_added = true;
        }

        ImGui::SetCursorScreenPos(ImVec2(window_pos.x + 8, window_pos.y + window_size.y - 56));
        if (could_be_added)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0, 0.0, 0.0, 1.0));
        }
        else
        {
            if (is_in_timeline) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0, 1.0, 0.0, 1.0));
            else ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0, 1.0, 0.0, 1.0));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
        }
        if (ImGui::Button(media_status_icon.c_str()))
        {
            if (!media_url.empty() && could_be_added)
            {
                auto name = ImGuiHelper::path_filename(media_url);
                auto path = std::string(media_url);
                auto file_suffix = ImGuiHelper::path_filename_suffix(path);
                auto type = EstimateMediaType(file_suffix);
                MediaItem * item = new MediaItem(name, path, type, timeline);
                timeline->media_items.push_back(item);
            }
        }
        if (could_be_added)
            ImGui::PopStyleColor();
        else
            ImGui::PopStyleColor(4);

        if (player->HasVideo())
        {
            ImGui::SetCursorScreenPos(ImVec2(window_pos.x + window_size.x - 8 - 16, window_pos.y + window_size.y - 56));
            if (ImGui::Button(ICON_MD_SETTINGS_OVERSCAN "##mediaplay_fullscreen"))
            {
                ImGui::OpenPopup(ICON_MEDIA_PREVIEW " MediaPlay FullScreen", ImGuiPopupFlags_AnyPopup);
                isFullscreen = true;
            }
        }

        // close button
        ImVec2 close_pos = ImVec2(window_pos.x + window_size.x - 32, window_pos.y + 4);
        ImVec2 close_size = ImVec2(16, 16);
        ImRect close_rect(close_pos, close_pos + close_size);
        ImGui::SetCursorScreenPos(close_pos);
        ImGui::Button(ICON_MD_CLEAR "##close_player");
        if (close_rect.Contains(io.MousePos) && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            StopTimelineMediaPlay();
            show = false;
        }
        ImGui::PopStyleColor();
    }
    else
    {
        draw_list->AddRectFilled(window_pos, ImVec2(window_pos.x + window_size.x, window_pos.y + window_size.y), IM_COL32(0, 0, 0, 255));
        std::string center_str = "Full screen playing...";
        auto string_width = ImGui::CalcTextSize(center_str.c_str());
        auto pos_center = window_pos + window_size / 2;
        auto str_pos = pos_center - string_width / 2;
        ImGui::SetCursorScreenPos(str_pos);
        ImGui::TextUnformatted(center_str.c_str());
    }

    if (ImGui::BeginPopupModal(ICON_MEDIA_PREVIEW " MediaPlay FullScreen", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_FullScreen | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings))
    {
        static bool show_ctrlbar = true;
        static int ctrlbar_hide_count = 0;
        static int ctrlbar_alpha = 255;
        auto viewport = ImGui::GetWindowViewport();
        ImDrawList * popup_draw_list = ImGui::GetWindowDrawList();
        ImVec2 popup_window_pos = ImGui::GetWindowPos();
        ImVec2 popup_window_size = ImGui::GetWindowSize();
        // handle key event
        bool no_ctrl_key = !io.KeyCtrl && !io.KeyShift && !io.KeyAlt;
        if (no_ctrl_key && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Space), false))
        {
            if (player->IsPlaying())
                player->Pause();
            else
                player->Play();
        }
        if (no_ctrl_key && !player->IsPlaying() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_LeftArrow), false))
        {
            player->Step(false);
        }
        if (no_ctrl_key && !player->IsPlaying() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_RightArrow), false))
        {
            player->Step(true);
        }
        if (no_ctrl_key && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape), false))
        {
            isFullscreen = false;
            ImGui::CloseCurrentPopup();
        }
        
        // Show PlayControl panel
        if (show_ctrlbar && io.FrameCountSinceLastEvent)
        {
            ctrlbar_hide_count++;
            if (ctrlbar_hide_count >= 100)
            {
                ctrlbar_alpha-=4;
            }
            if (ctrlbar_alpha <= 0)
            {
                ctrlbar_hide_count = 0;
                ctrlbar_alpha = 0;
                show_ctrlbar = false;
            }
        }
        
        if (io.KeyShift)
        {
            // we may using shift to tiggle video zoom
            ctrlbar_hide_count = 0;
            ctrlbar_alpha = 0;
            show_ctrlbar = false;
        }
        else if (io.FrameCountSinceLastEvent == 0)
        {
            ctrlbar_hide_count = 0;
            ctrlbar_alpha = 255;
            show_ctrlbar = true;
        }

        ImTextureID tid = player->GetFrame(playPos);
        if (tid)
            ImGui::ImShowVideoWindow(popup_draw_list, tid, popup_window_pos, popup_window_size, 512);

        if (show_ctrlbar)
        {
            ImVec2 center(20.f, io.DisplaySize.y * 0.85f);
            ImVec2 panel_size(io.DisplaySize.x - 40.0, 120);
            ImGui::SetNextWindowPos(center, ImGuiCond_Always);
            ImGui::SetNextWindowBgAlpha(0.f);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.25, 0.25, 0.25, 0.5));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.75, 0.75, 0.75, 1.0));
            ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.75, 0.75, 0.75, 1));
            ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(1, 1, 1, 1));
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, (float)ctrlbar_alpha / 255.f);
            if (ImGui::BeginChild("MediaPlayControlPanel", panel_size, 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize))
            {
                ImDrawList * ctrl_draw_list = ImGui::GetWindowDrawList();
                ImVec2 ctrl_window_pos = ImGui::GetWindowPos();
                ImVec2 ctrl_window_size = ImGui::GetWindowSize();
                ctrl_draw_list->AddRectFilled(ctrl_window_pos, ctrl_window_pos + ctrl_window_size, IM_COL32(64, 64, 64, ctrlbar_alpha / 2), 16);
                
                ImGui::SetWindowFontScale(2.0);

                ImGui::BeginDisabled(player->IsPlaying());
                ImGui::SetCursorScreenPos(ImVec2(ctrl_window_pos.x + ctrl_window_size.x / 2 - 15 - 32 - 16, ctrl_window_pos.y + 8));
                if (ImGui::Button(ICON_STEP_BACKWARD "##media_play_back_step"))
                {
                    player->Step(false);
                }
                ImGui::ShowTooltipOnHover("Step Prev (<-)");
                ImGui::EndDisabled();

                std::string playBtnLabel = (player->IsPlaying() ? std::string(ICON_STOP) : std::string(ICON_PLAY_FORWARD)) + "##media_play_pause";
                ImGui::SetCursorScreenPos(ImVec2(ctrl_window_pos.x + ctrl_window_size.x / 2 - 15, ctrl_window_pos.y + 8));
                if (ImGui::Button(playBtnLabel.c_str()))
                {
                    if (player->IsPlaying())
                        player->Pause();
                    else
                        player->Play();
                }
                ImGui::ShowTooltipOnHover("Play/Pause (Space)");

                ImGui::BeginDisabled(player->IsPlaying());
                ImGui::SetCursorScreenPos(ImVec2(ctrl_window_pos.x + ctrl_window_size.x / 2 + 15 + 16, ctrl_window_pos.y + 8));
                if (ImGui::Button(ICON_STEP_FORWARD "##media_play_next_step"))
                {
                    player->Step(true);
                }
                ImGui::ShowTooltipOnHover("Step Next (->)");
                ImGui::EndDisabled();

                ImGui::SetCursorScreenPos(ImVec2(ctrl_window_pos.x + ctrl_window_size.x - 64, ctrl_window_pos.y + 8));
                if (ImGui::Button(ICON_MD_ZOOM_IN_MAP "##exit_fullscreen"))
                {
                    isFullscreen = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::ShowTooltipOnHover("Exit FullScreen");

                ImGui::SetWindowFontScale(1.0);
                ImGui::Spacing();
                ImGui::SetNextItemWidth(ctrl_window_size.x);
                if (ImGui::SliderFloat("##TimePosition", &playPos, 0, mediaDur, ""))
                    player->Seek(playPos, true);
                if (player->IsSeeking() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
                    player->Seek(playPos, false);
                // Time stamp on left of control panel
                ImRect TimeStampRect = ImRect(ctrl_window_pos.x + 32, ctrl_window_pos.y + 12, 
                                            ctrl_window_pos.x + ctrl_window_size.x / 2, ctrl_window_pos.y + 64);
                ctrl_draw_list->PushClipRect(TimeStampRect.Min, TimeStampRect.Max, true);
                ImGui::SetWindowFontScale(1.5);
                ImGui::ShowDigitalTimeDuration(ctrl_draw_list, playPos * 1000, mediaDur * 1000, 3, TimeStampRect.Min, IM_COL32(255, 255, 255, ctrlbar_alpha));
                ImGui::SetWindowFontScale(1.0);
                ctrl_draw_list->PopClipRect();
            }
            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(5);
        }
        
        ImGui::EndPopup();
    }
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
        ImGuiFileDialog::Instance()->OpenDialogWithPane("##MediaEditFileDlgKey", ICON_IGFD_FOLDER_OPEN " Choose Media File", 
                                                        ffilters.c_str(), ".", 
                                                        std::bind(&ImgSeuqPane, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4),
                                                        200, 1, IGFDUserDatas("Media Source"),
                                                        ImGuiFileDialogFlags_ShowBookmark | 
                                                        ImGuiFileDialogFlags_CaseInsensitiveExtention | 
                                                        ImGuiFileDialogFlags_DisableCreateDirectoryButton | 
                                                        ImGuiFileDialogFlags_Modal | 
                                                        ImGuiFileDialogFlags_AllowDirectorySelect);
    }
    ImGui::SetWindowFontScale(1.0);
    ImGui::ShowTooltipOnHover("Add new media into bank");
    ImGui::SetCursorScreenPos(icon_pos);
    ImGui::PopStyleColor(3);
    return ret;
}

// Tips by Jimmy: add 'bool& filtered' param, 'bool& searched'
// 'true' indicates that the media_items is filtered
// 'true' indicates that the media_items is fsearched
static std::vector<MediaItem *>::iterator InsertMediaIcon(std::vector<MediaItem *>::iterator item, ImDrawList *draw_list, ImVec2 icon_pos, float media_icon_size, bool& changed, bool& filtered, bool& searched)
{
    ImGuiIO& io = ImGui::GetIO();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    (*item)->UpdateThumbnail();
    ImVec2 icon_size = ImVec2(media_icon_size, media_icon_size);
    // Draw Shadow for Icon
    draw_list->AddRectFilled(icon_pos + ImVec2(6, 6), icon_pos + ImVec2(6, 6) + icon_size, IM_COL32(16, 16, 16, 255), 8, ImDrawFlags_RoundCornersAll);
    draw_list->AddRectFilled(icon_pos + ImVec2(4, 4), icon_pos + ImVec2(4, 4) + icon_size, IM_COL32(32, 32, 48, 255), 8, ImDrawFlags_RoundCornersAll);
    draw_list->AddRectFilled(icon_pos + ImVec2(2, 2), icon_pos + ImVec2(2, 2) + icon_size, IM_COL32(64, 64, 96, 255), 8, ImDrawFlags_RoundCornersAll);
    if ((*item)->mSelected)
        draw_list->AddRect(icon_pos + ImVec2(-1, -1), icon_pos + ImVec2(2, 2) + icon_size, IM_COL32(255, 255, 0, 255), 8, ImDrawFlags_RoundCornersAll);
    ImGui::SetCursorScreenPos(icon_pos);
    ImGui::InvisibleButton((*item)->mPath.c_str(), icon_size);
    
    if ((*item)->mValid)
    {
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_PreviewTooltipKeepViewport))
        {
            ImGui::SetDragDropPayload("Media_drag_drop", *item, sizeof(MediaItem));
            ImGui::TextUnformatted((*item)->mName.c_str());
            if (!(*item)->mMediaThumbnail.empty() && (*item)->mMediaThumbnail[0])
            {
                const auto vidstm = (*item)->mMediaOverview->GetVideoStream();
                float aspectRatio = (float)vidstm->width / (float)vidstm->height;
                auto hTx = (*item)->mMediaThumbnail[0];
                auto tid = hTx->TextureID();
                auto roiRect = hTx->GetDisplayRoi();
                ImGui::Image(tid, ImVec2(icon_size.x, icon_size.y / aspectRatio), roiRect.lt, roiRect.rb);
            }
            ImGui::EndDragDropSource();
        }
        RenderUtils::ManagedTexture::Holder hTx;
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            float pos_x = io.MousePos.x - icon_pos.x;
            float percent = pos_x / icon_size.x;
            ImClamp(percent, 0.0f, 1.0f);
            int texture_index = (*item)->mMediaThumbnail.size() * percent;
            if (IS_IMAGE((*item)->mMediaType))
                texture_index = 0;
            if (!(*item)->mMediaThumbnail.empty())
            {
                hTx = (*item)->mMediaThumbnail[texture_index];
            }

            if (timeline && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                // clean all item selected flags
                for (auto media : timeline->media_items) media->mSelected = false;
                (*item)->mSelected = true;
                // set timeline player
                timeline->mMediaPlayer->Close();
                timeline->mMediaPlayer->Open((*item)->mPath);
                timeline->mMediaPlayer->Play();
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
                hTx = (*item)->mMediaThumbnail[1];
            else
                hTx = (*item)->mMediaThumbnail[0];
        }

        ImGui::SetCursorScreenPos(icon_pos);
        if (hTx)
        {
            auto roiRect = hTx->GetDisplayRoi();
            ShowVideoWindow(hTx->TextureID(), icon_pos + ImVec2(4, 16), icon_size - ImVec2(4, 32), "", 0.f, false, roiRect.lt, roiRect.rb);
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
                        ImVec2 plot_size(wave_size.x, channel_height);
                        int sampleSize = wavefrom->pcm[i].size();
#if PLOT_TEXTURE
                        ImGui::ImMat plot_mat;
                        ImTextureID texture = (*item)->mWaveformTextures.size() > i ? (*item)->mWaveformTextures[i] : nullptr;
                        if (!texture)
                        {
                            ImGui::PlotMat(plot_mat, &wavefrom->pcm[i][0], sampleSize, 0, -wave_range / 2, wave_range / 2, plot_size, sizeof(float), false, 1.0);
                            ImMatToTexture(plot_mat, texture);
                            (*item)->mWaveformTextures.push_back(texture);
                        }
                        else if (!wavefrom->parseDone)
                        {
                            ImGui::PlotMat(plot_mat, &wavefrom->pcm[i][0], sampleSize, 0, -wave_range / 2, wave_range / 2, plot_size, sizeof(float), false, 1.0);
                            ImMatToTexture(plot_mat, texture);
                        }

                        if (texture)
                        {
                            draw_list->AddImage(texture, wave_pos + ImVec2(0, i * channel_height), wave_pos + ImVec2(0, i * channel_height) + plot_size, ImVec2(0, 0), ImVec2(1, 1));
                        }
#else
                        ImGui::SetCursorScreenPos(wave_pos + ImVec2(0, i * channel_height));
                        std::string id_string = "##BankWaveform@" + std::to_string((*item)->mID) + "@" + std::to_string(i);
                        ImGui::PlotLinesEx(id_string.c_str(), &wavefrom->pcm[i][0], sampleSize, 0, nullptr, -wave_range / 2, wave_range / 2, plot_size, sizeof(float), false);
#endif
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
                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && ImGui::BeginTooltip())
                    {
                        std::string bitrate_str = stream->bitRate >= 1000000 ? "Mbps" :
                                                stream->bitRate >= 1000 ? "Kbps" : "bps";
                        float bitrate = stream->bitRate >= 1000000 ? (float)stream->bitRate / 1000000 : stream->bitRate >= 1000 ? (float)stream->bitRate / 1000 : stream->bitRate;
                        ImGui::TextUnformatted("   size:"); ImGui::SameLine(); ImGui::Text("%d x %d", stream->width, stream->height);
                        ImGui::TextUnformatted("bitrate:"); ImGui::SameLine(); ImGui::Text("%.3f %s", bitrate, bitrate_str.c_str());
                        ImGui::TextUnformatted("    fps:"); ImGui::SameLine(); ImGui::Text("%.3f fps", stream->avgFrameRate.den > 0 ? stream->avgFrameRate.num / stream->avgFrameRate.den : 
                                                                                                    stream->realFrameRate.den > 0 ? stream->realFrameRate.num / stream->realFrameRate.den : 0.0f);
                        ImGui::TextUnformatted("  codec:"); ImGui::SameLine(); ImGui::Text("%s", stream->codec.c_str());
                        ImGui::TextUnformatted(" format:"); ImGui::SameLine(); ImGui::Text("%s", stream->format.c_str());
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
                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && ImGui::BeginTooltip())
                    {
                        ImGui::TextUnformatted("sample rate:"); ImGui::SameLine(); ImGui::Text("%d", audio_sample_rate);
                        ImGui::TextUnformatted("   channels:"); ImGui::SameLine(); ImGui::Text("%s", GetAudioChannelName(audio_channels).c_str());
                        ImGui::TextUnformatted("      depth:"); ImGui::SameLine(); ImGui::Text("%d", stream->bitDepth);
                        ImGui::TextUnformatted("      codec:"); ImGui::SameLine(); ImGui::Text("%s", stream->codec.c_str());
                        ImGui::TextUnformatted("     format:"); ImGui::SameLine(); ImGui::Text("%s", stream->format.c_str());
                        ImGui::EndTooltip();
                    }
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
    bool is_item_in_timeline = timeline->isURLInTimeline((*item)->mPath);
    ImGui::SetWindowFontScale(0.8);
    ImGui::Button(((is_item_in_timeline ? std::string(ICON_FA_PLUG_CIRCLE_BOLT) : std::string(ICON_TRASH)) + "##delete_media" + (*item)->mPath).c_str(), ImVec2(16, 16));
    ImGui::SetWindowFontScale(1.0);
    ImRect button_rect(icon_pos + ImVec2(media_icon_size - 16, 0), icon_pos + ImVec2(media_icon_size - 16, 0) + ImVec2(16, 16));
    bool overButton = button_rect.Contains(io.MousePos);
    if (overButton && io.MouseClicked[0] && !is_item_in_timeline)
    {
        MediaItem * it = *item;
        // Modify by Jimmy, Begin
        if (searched)
        {
            auto m_iter = std::find_if(timeline->media_items.begin(), timeline->media_items.end(), [it](const MediaItem* lit)
            {
                return it->mID == lit->mID;
            });
            if (m_iter != timeline->media_items.end())
                timeline->media_items.erase(m_iter); // first, delete this media_item from timeline->media_items

            auto f_iter = std::find_if(timeline->filter_media_items.begin(), timeline->filter_media_items.end(), [it](const MediaItem* lit)
            {
                return it->mID == lit->mID;
            });
            if (f_iter != timeline->filter_media_items.end())
                timeline->filter_media_items.erase(f_iter); // then, delete this media_item from timeline->filter_media_items

            item = timeline->search_media_items.erase(item); // final, delete this media_item from timeline->filter_media_items
        }
        else if (filtered)
        {
            auto iter = std::find_if(timeline->media_items.begin(), timeline->media_items.end(), [it](const MediaItem* lit)
            {
                return it->mID == lit->mID;
            });
            if (iter != timeline->media_items.end())
                timeline->media_items.erase(iter); // first, delete this media_item from timeline->media_items

            item = timeline->filter_media_items.erase(item); // then, delete this media_item from timeline->filter_media_items
        }
        else
        {
            item = timeline->media_items.erase(item);
        }
        delete it;
        // Modify by Jimmy, End
        changed = true;
    }
    else
        item++;
    ImGui::ShowTooltipOnHover(is_item_in_timeline ? "Media in Timeline" : "Delete Media");

    ImGui::PopStyleColor();
    return item;
}

static void ShowMediaBankWindow(ImDrawList *_draw_list, float media_icon_size)
{
    ImGuiIO& io = ImGui::GetIO();
    bool multiviewport = io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    static std::vector<std::string> failed_items;
    static bool show_player = false;
    bool changed = false;
    bool filtered = false; // 'true' indicates that the media_items is filtered
    bool searched = false; // 'true' indicates that the media_items is searched
    if (!timeline)
        return;

    if (g_media_editor_settings.ExpandScope)
    {
        StopTimelineMediaPlay();
        show_player = false;
    }

    ImVec2 contain_size = ImGui::GetWindowSize() - ImVec2(4, 4);
    ImVec2 bank_window_size = contain_size - ImVec2(20, 0);
    if (show_player) bank_window_size = ImVec2(contain_size.x, contain_size.y  * 2 / 3) - ImVec2(20, 0);
    ImGuiWindowFlags bank_window_flags = ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar | 
                                            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
    ImGuiWindowFlags child_window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::BeginChild("##Media_bank_content", bank_window_size, false, bank_window_flags))
    {
        if (g_media_editor_settings.MediaBankViewType == 0)
        {
            ImVec2 menu_window_size = ImVec2(ImGui::GetWindowSize().x, 40);
            ImGui::SetNextWindowPos(ImGui::GetWindowPos() + ImVec2(4, 0));
            if (ImGui::BeginChild("##Media_bank_content_menu", menu_window_size, false, child_window_flags))
            {
                ImDrawList * draw_list = ImGui::GetWindowDrawList();
                draw_list->AddTextComplex(ImGui::GetWindowPos() + ImVec2(4, 0),
                                        "Media Bank", 2.5, COL_GRAY_TEXT,
                                        0.5f, IM_COL32(56, 56, 56, 192));
                // Modify by Jimmy, Begin
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5, 0.5, 0.5, 0.5));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7, 0.7, 0.7, 1.0));

                // Sorted, timeline->media_items
                ImGui::SetCursorPos(ImVec2(ImGui::GetWindowSize().x - media_icon_size, 10));
                ImGui::SetNextItemWidth(media_icon_size / 2);
                uint32_t curr_sort_method = timeline->mSortMethod;
                if (ImGui::BeginCombo("##sort_media_item", SortMethodItems[timeline->mSortMethod].icon.c_str()))
                {
                    for (int n = 0; n < IM_ARRAYSIZE(SortMethodItems); n++)
                    {
                        const bool is_selected = (timeline->mSortMethod == n);
                        if (ImGui::Selectable(SortMethodItems[n].icon.c_str(), is_selected))
                            timeline->mSortMethod = n;

                        // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                        ImGui::ShowTooltipOnHover("%s", std::string("Sorted by " + SortMethodItems[n].name).c_str());
                    }
                    ImGui::EndCombo();
                }
                ImGui::ShowTooltipOnHover("%s", std::string("Sorted by " + SortMethodItems[timeline->mSortMethod].name).c_str());
                if (curr_sort_method != timeline->mSortMethod || timeline->mCurrViewCount != timeline->media_items.size()) // 1. mSortMethod changed; 2. media_items.size() changed
                    timeline->mSortMethod ? (timeline->mSortMethod-1 ? timeline->SortMediaItemByName() : timeline->SortMediaItemByType()) : timeline->SortMediaItemByID();

                // filtered, timeline->fliter_media_items
                ImGui::SetCursorPos(ImVec2(ImGui::GetWindowSize().x - media_icon_size / 2, 10));
                ImGui::SetNextItemWidth(media_icon_size / 2);
                uint32_t curr_filter_method = timeline->mFilterMethod;
                if (ImGui::BeginCombo("##filter_media_item", FilterMethodItems[timeline->mFilterMethod].icon.c_str()))
                {
                    for (int n = 0; n < IM_ARRAYSIZE(FilterMethodItems); n++)
                    {
                        const bool is_selected = (timeline->mFilterMethod == n);
                        if (ImGui::Selectable(FilterMethodItems[n].icon.c_str(), is_selected))
                            timeline->mFilterMethod = n;

                        // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                        ImGui::ShowTooltipOnHover("%s", std::string("Filtered by " + FilterMethodItems[n].name).c_str());
                    }
                    ImGui::EndCombo();
                }
                ImGui::ShowTooltipOnHover("%s", std::string("Filtered by " + FilterMethodItems[timeline->mFilterMethod].name).c_str());
                // 1. mFilterMethod changed 2. media_items.size() changed; 3. mSortMethod changed;
                if (curr_filter_method != timeline->mFilterMethod || timeline->mCurrViewCount != timeline->media_items.size() || curr_sort_method != timeline->mSortMethod)
                {
                    timeline->filter_media_items.clear();
                    timeline->FilterMediaItemByType(FilterMethodItems[timeline->mFilterMethod].meaning);
                }

                // Searched, timeline->fliter_media_items
                ImGui::SetCursorPos(ImVec2(ImGui::GetWindowPos().x + media_icon_size + 50, 10));
                ImGui::SetNextItemWidth(ImGui::GetWindowSize().x - media_icon_size*3 - 20);
                timeline->mTextSearchFilter.Draw(ICON_ZOOM "##text_filter");

                timeline->mCurrViewCount = timeline->media_items.size(); // Update timeline->media_items count
                ImGui::PopStyleColor(3);
                // Modify by Jimmy, End
            }
            ImGui::EndChild();
            ImVec2 main_window_size = ImVec2(ImGui::GetWindowSize().x, ImGui::GetWindowSize().y - 40);
            ImGui::SetNextWindowPos(ImGui::GetWindowPos() + ImVec2(4, 44));
            if (ImGui::BeginChild("##Media_bank_content_main", main_window_size, false, child_window_flags))
            {
                ImDrawList * draw_list = ImGui::GetWindowDrawList();
                ImVec2 window_pos = ImGui::GetWindowPos();
                ImVec2 window_size = ImGui::GetWindowSize();
                ImVec2 contant_size = ImGui::GetContentRegionAvail();
                ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
                ImGui::Indent(20);
                if (timeline->media_items.empty())
                {
                    ImU32 text_color = IM_COL32(ui_breathing * 255, ui_breathing * 255, ui_breathing * 255, 255);
                    draw_list->AddTextComplex(window_pos + ImVec2(128,  48), "Please Click", 2.0f, COL_GRAY_TEXT, 0.5f, IM_COL32(56, 56, 56, 192));
                    draw_list->AddTextComplex(window_pos + ImVec2(128,  80), "<-- Here", 2.0f, text_color, 0.5f, IM_COL32(56, 56, 56, 192));
                    draw_list->AddTextComplex(window_pos + ImVec2(128, 112), "To Add Media", 2.0f, COL_GRAY_TEXT, 0.5f, IM_COL32(56, 56, 56, 192));
                    draw_list->AddTextComplex(window_pos + ImVec2( 10, 144), "Or Drag Files From Brower", 2.f, COL_GRAY_TEXT, 0.5f, IM_COL32(56, 56, 56, 192));
                }
                // Show Media Icons
                int icon_number_pre_row = window_size.x / (media_icon_size + 24);
                // insert empty icon for add media
                int media_add_icon_cnt = 1;
                const auto icon_area_pos = ImGui::GetCursorScreenPos() + ImVec2(0, 24);
                auto icon_pos = icon_area_pos;
                InsertMediaAddIcon(draw_list, icon_pos, media_icon_size);
                int icon_row_idx = 0;
                // Modify by Jimmy, Start
                if (timeline->mTextSearchFilter.IsActive())// op in timeline->filter_media_items
                {
                    searched = true;
                    timeline->search_media_items.clear();
                    if (timeline->mFilterMethod == 0)
                    {
                        for (auto media_item : timeline->media_items)
                        {
                            if (timeline->mTextSearchFilter.PassFilter(media_item->mName.c_str()))
                                timeline->search_media_items.push_back(media_item);
                        }
                    }
                    else
                    {
                        for (auto media_item : timeline->filter_media_items)
                        {
                            if (timeline->mTextSearchFilter.PassFilter(media_item->mName.c_str()))
                                timeline->search_media_items.push_back(media_item);
                        }
                    }

                    for (auto item = timeline->search_media_items.begin(); item != timeline->search_media_items.end();)
                    {
                        int icon_col_idx = icon_row_idx > 0 ? 0 : media_add_icon_cnt;
                        for (; icon_col_idx < icon_number_pre_row; icon_col_idx++)
                        {
                            auto row_icon_pos = icon_area_pos + ImVec2(icon_col_idx * (media_icon_size + 24), icon_row_idx * (media_icon_size + 24));
                            auto next_item = InsertMediaIcon(item, draw_list, row_icon_pos, media_icon_size, changed, filtered, searched);

                            if (next_item == timeline->search_media_items.end())
                            {
                                item = next_item;
                                break;
                            }
                            if ((*item)->mSelected)
                            {
                                if (g_media_editor_settings.ExpandScope) g_media_editor_settings.ExpandScope = false;
                                show_player = true;
                            }
                            item = next_item;
                        }
                        if (item == timeline->search_media_items.end())
                            break;
                        icon_row_idx++;
                    }
                }
                else if (timeline->mFilterMethod != 0) // op in timeline->filter_media_items
                {
                    filtered = true;
                    for (auto item = timeline->filter_media_items.begin(); item != timeline->filter_media_items.end();)
                    {
                        int icon_col_idx = icon_row_idx > 0 ? 0 : media_add_icon_cnt;
                        for (; icon_col_idx < icon_number_pre_row; icon_col_idx++)
                        {
                            auto row_icon_pos = icon_pos + ImVec2(icon_col_idx * (media_icon_size + 24), icon_row_idx * (media_icon_size + 24));
                            auto next_item = InsertMediaIcon(item, draw_list, row_icon_pos, media_icon_size, changed, filtered, searched);

                            if (next_item == timeline->filter_media_items.end())
                            {
                                item = next_item;
                                break;
                            }
                            if ((*item)->mSelected)
                            {
                                if (g_media_editor_settings.ExpandScope) g_media_editor_settings.ExpandScope = false;
                                show_player = true;
                            }
                            item = next_item;
                        }
                        if (item == timeline->filter_media_items.end())
                            break;
                        icon_row_idx++;
                    }
                }
                else
                {
                    for (auto item = timeline->media_items.begin(); item != timeline->media_items.end();)
                    {
                        int icon_col_idx = icon_row_idx > 0 ? 0 : media_add_icon_cnt;
                        for (; icon_col_idx < icon_number_pre_row; icon_col_idx++)
                        {
                            auto row_icon_pos = icon_area_pos + ImVec2(icon_col_idx * (media_icon_size + 24), icon_row_idx * (media_icon_size + 24));
                            auto next_item = InsertMediaIcon(item, draw_list, row_icon_pos, media_icon_size, changed, filtered, searched);

                            if ((*item)->mSelected)
                            {
                                if (g_media_editor_settings.ExpandScope) g_media_editor_settings.ExpandScope = false;
                                show_player = true;
                            }

                            if (next_item == timeline->media_items.end())
                            {
                                item = next_item;
                                break;
                            }
                            
                            item = next_item;
                        }
                        if (item == timeline->media_items.end())
                            break;
                        icon_row_idx++;
                    }
                }
                // Modify by Jimmy, End
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
                                    changed = true;
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
            ImGui::EndChild();
        }
        else
        {
            static std::string finder_filename = "";
            ImVec2 finder_size = bank_window_size - ImVec2(0, 60);
            if (ImGui::BeginChild("##Media_finder_content", bank_window_size, false, child_window_flags))
            {
                ImDrawList * draw_list = ImGui::GetWindowDrawList();
                draw_list->AddTextComplex(ImGui::GetWindowPos() + ImVec2(4, 0),
                                        "Media Finder", 2.5, COL_GRAY_TEXT,
                                        0.5f, IM_COL32(56, 56, 56, 192));
                ImGui::SetCursorPos(ImVec2(0, 50));
                ImGui::PushStyleColor(ImGuiCol_Separator, COL_MARK);
                ImGui::Separator();
                embedded_filedialog.OpenDialog("##MediaEmbeddedFileDlgKey", "Select File",
                                                abbr_ffilters.c_str(),
                                                finder_filename,
                                                -1,
                                                nullptr, 
                                                ImGuiFileDialogFlags_NoDialog |
                                                ImGuiFileDialogFlags_NoButton |
                                                ImGuiFileDialogFlags_DontShowHiddenFiles |
                                                //ImGuiFileDialogFlags_PathDecompositionShort |
                                                //ImGuiFileDialogFlags_DisableBookmarkMode | 
                                                ImGuiFileDialogFlags_ShowBookmark |
                                                ImGuiFileDialogFlags_ReadOnlyFileNameField |
                                                ImGuiFileDialogFlags_CaseInsensitiveExtention);

                if (embedded_filedialog.Display("##MediaEmbeddedFileDlgKey", ImGuiWindowFlags_NoCollapse, ImVec2(0,0), finder_size))
                {
                    if (embedded_filedialog.IsOk())
                    {
                        finder_filename = embedded_filedialog.GetFilePathName();
                        timeline->mMediaPlayer->Close();
                        timeline->mMediaPlayer->Open(finder_filename);
                        timeline->mMediaPlayer->Play();
                        show_player = true;
                    }
                    embedded_filedialog.Close();
                }
                ImGui::PopStyleColor();
            }
            ImGui::EndChild();
        }
    }
    ImGui::EndChild();
    ImGui::SameLine(0,0);
    if (ImGui::TabLabelsVertical(MediaBankTabNames,g_media_editor_settings.MediaBankViewType,MediaBankTabTooltips, false, nullptr, nullptr, false, false, nullptr, nullptr, true, false))
    {
        StopTimelineMediaPlay();
        show_player = false;
    }
    if (show_player)
    {
        ImVec2 play_window_size = ImVec2(contain_size.x, contain_size.y / 3 - 4);
        ImGui::SetNextWindowPos(ImGui::GetCursorScreenPos() + ImVec2(4, 4));
        if (ImGui::BeginChild("##Media_finder_content_player", play_window_size, false, child_window_flags))
        {
            ShowMediaPlayWindow(show_player);
        }
        ImGui::EndChild();
    }
    if (!g_project_loading) project_changed |= changed;
}

/***************************************************************************************
 * 
 * Transition Bank window
 *
 ***************************************************************************************/
static void ShowTransitionBankIconWindow(ImDrawList *_draw_list)
{
    ImGuiIO& io = ImGui::GetIO();
    bool multiviewport = io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 transition_icon_size{96, 54};

    if (!timeline)
        return;

    if (ImGui::BeginChild("##Transition_bank_icon_content", ImGui::GetWindowSize() - ImVec2(4, 4), false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings))
    {
        ImVec2 window_pos = ImGui::GetWindowPos();
        ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
        ImDrawList * draw_list = ImGui::GetWindowDrawList();
        draw_list->AddTextComplex(window_pos + ImVec2(8, 0), "Transition Bank", 2.5f, COL_GRAY_TEXT, 0.5f, IM_COL32(56, 56, 56, 192));
        ImGui::Indent(20);
        // Show Transition Icons
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
                if (catalog.size() < 2 || catalog[0].compare("Transition") != 0)
                    continue;
                auto type = node->GetTypeInfo();
                std::string drag_type = "Transition_drag_drop_" + catalog[1];
                ImGui::Dummy(ImVec2(0, 16));
                auto icon_pos = ImGui::GetCursorScreenPos();
                ImVec2 icon_size = transition_icon_size;
                // Draw Shadow for Icon
                draw_list->AddRectFilled(icon_pos + ImVec2(6, 6), icon_pos + ImVec2(6, 6) + icon_size, IM_COL32(32, 32, 32, 255));
                draw_list->AddRectFilled(icon_pos + ImVec2(4, 4), icon_pos + ImVec2(4, 4) + icon_size, IM_COL32(48, 48, 72, 255));
                draw_list->AddRectFilled(icon_pos + ImVec2(2, 2), icon_pos + ImVec2(2, 2) + icon_size, IM_COL32(64, 64, 96, 255));
                draw_list->AddRectFilled(icon_pos, icon_pos + icon_size, COL_BLACK_DARK);
                ImGui::InvisibleButton(type.m_Name.c_str(), icon_size);
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
                {
                    ImGui::SetDragDropPayload(drag_type.c_str(), node, sizeof(BluePrint::Node));
                    ImGui::TextUnformatted(ICON_BANK " Add Transition");
                    ImGui::TextUnformatted(type.m_Name.c_str());
                    ImGui::EndDragDropSource();
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                {
                    // Show help tooltip
                    if (timeline->mShowHelpTooltips && !ImGui::IsDragDropActive() && ImGui::BeginTooltip())
                    {
                        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
                        ImGui::TextUnformatted("Help:");
                        ImGui::TextUnformatted("    Drag transition to blue print");
                        ImGui::PopStyleVar();
                        ImGui::EndTooltip();                }
                }
                ImGui::SetCursorScreenPos(icon_pos + ImVec2(2, 2));
                node->DrawNodeLogo(ImGui::GetCurrentContext(), transition_icon_size); 
                float gap = (icon_size.y - ImGui::GetFontSize()) / 2.0f;
                ImGui::SetCursorScreenPos(icon_pos + ImVec2(icon_size.x + 8, gap));
                ImGui::Button(type.m_Name.c_str(), ImVec2(0, 32));
                ImGui::Spacing();
            }
        }
        ImGui::PopStyleColor();
    }
    ImGui::EndChild();
}

static void ShowTransitionBankTreeWindow(ImDrawList *_draw_list)
{
    ImGuiIO& io = ImGui::GetIO();
    bool multiviewport = io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();

    if (!timeline)
        return;
    
    if (ImGui::BeginChild("##Transition_bank_tree_content", ImGui::GetWindowSize() - ImVec2(4, 4), false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings))
    {
        ImVec2 window_pos = ImGui::GetWindowPos();
        ImVec2 window_size = ImGui::GetWindowSize();
        ImDrawList * draw_list = ImGui::GetWindowDrawList();

        ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
        const ImVec2 item_size(window_size.x, 32);
        draw_list->AddTextComplex(window_pos + ImVec2(8, 0), "Transition Bank", 2.5f, COL_GRAY_TEXT, 0.5f, IM_COL32(56, 56, 56, 192));
        // Show Transition Tree
        if (timeline->m_BP_UI.m_Document)
        {
            std::vector<const BluePrint::Node*> transitions;
            auto &bp = timeline->m_BP_UI.m_Document->m_Blueprint;
            auto node_reg = bp.GetNodeRegistry();
            // find all transitions
            for (auto node : node_reg->GetNodes())
            {
                auto catalog = BluePrint::GetCatalogInfo(node->GetCatalog());
                if (!catalog.size() || catalog[0].compare("Transition") != 0)
                    continue;
                transitions.push_back(node);
            }

            // make transition type as tree
            ImGui::ImTree transition_tree;
            transition_tree.name = "Transition";
            for (auto node : transitions)
            {
                auto catalog = BluePrint::GetCatalogInfo(node->GetCatalog());
                if (!catalog.size())
                    continue;
                auto type = node->GetTypeInfo();
                if (catalog.size() > 1)
                {
                    auto children = transition_tree.FindChildren(catalog[1]);
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

                        transition_tree.childrens.push_back(subtree);
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
                    transition_tree.childrens.push_back(end_sub);
                }
            }

            auto AddTransition = [&](void* data)
            {
                const BluePrint::Node* node = (const BluePrint::Node*)data;
                if (!node) return;
                auto type = node->GetTypeInfo();
                auto catalog = BluePrint::GetCatalogInfo(type.m_Catalog);
                if (catalog.size() < 2 || catalog[0].compare("Transition") != 0)
                    return;
                std::string drag_type = "Transition_drag_drop_" + catalog[1];
                auto icon_pos = ImGui::GetCursorScreenPos();
                ImVec2 icon_size = ImVec2(56, 32);
                ImGui::InvisibleButton(type.m_Name.c_str(), icon_size);
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
                {
                    ImGui::SetDragDropPayload(drag_type.c_str(), node, sizeof(BluePrint::Node));
                    ImGui::TextUnformatted(ICON_BANK " Add Transition");
                    ImGui::TextUnformatted(type.m_Name.c_str());
                    ImGui::EndDragDropSource();
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                {
                    // Show help tooltip
                    if (timeline->mShowHelpTooltips && !ImGui::IsDragDropActive() && ImGui::BeginTooltip())
                    {
                        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
                        ImGui::TextUnformatted("Help:");
                        ImGui::TextUnformatted("    Drag transition to blue print");
                        ImGui::PopStyleVar();
                        ImGui::EndTooltip();
                    }
                }
                ImGui::SetCursorScreenPos(icon_pos);
                node->DrawNodeLogo(ImGui::GetCurrentContext(), icon_size);
                ImGui::SameLine();
                ImGui::Button(type.m_Name.c_str(), ImVec2(0, 32));
            };

            // draw transition tree
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            for (auto sub : transition_tree.childrens)
            {
                ImGui::SetNextItemOpen(true, ImGuiCond_Once);
                if (sub.data)
                {
                    AddTransition(sub.data);
                }
                else if (ImGui::TreeNode(sub.name.c_str()))
                {
                    for (auto sub_sub : sub.childrens)
                    {
                        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
                        if (sub_sub.data)
                        {
                            AddTransition(sub_sub.data);
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
                                    AddTransition(end.data);
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
    ImGui::EndChild();
}

/****************************************************************************************
 * 
 * Filters Bank window
 *
 ***************************************************************************************/
static void ShowFilterBankIconWindow(ImDrawList *_draw_list)
{
    ImGuiIO& io = ImGui::GetIO();
    bool multiviewport = io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    float filter_icon_size = 48;

    if (!timeline)
        return;

    if (ImGui::BeginChild("##Filter_bank_icon_content", ImGui::GetWindowSize() - ImVec2(4, 4), false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings))
    {
        ImVec2 window_pos = ImGui::GetWindowPos();
        ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
        ImDrawList * draw_list = ImGui::GetWindowDrawList();
        draw_list->AddTextComplex(window_pos + ImVec2(8, 0), "Filter Bank", 2.5f, COL_GRAY_TEXT, 0.5f, IM_COL32(56, 56, 56, 192));
        ImGui::Indent(20);
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
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
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
    ImGui::EndChild();
}

static void ShowFilterBankTreeWindow(ImDrawList *_draw_list)
{
    ImGuiIO& io = ImGui::GetIO();
    bool multiviewport = io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();

    if (!timeline)
        return;

    if (ImGui::BeginChild("##Filter_bank_tree_content", ImGui::GetWindowSize() - ImVec2(4, 4), false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings))
    {
        ImVec2 window_pos = ImGui::GetWindowPos();
        ImVec2 window_size = ImGui::GetWindowSize();
        ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
        ImDrawList * draw_list = ImGui::GetWindowDrawList();
        const ImVec2 item_size(window_size.x, 32);
        draw_list->AddTextComplex(window_pos + ImVec2(8, 0), "Filter Bank", 2.5f, COL_GRAY_TEXT, 0.5f, IM_COL32(56, 56, 56, 192));

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
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
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
    ImGui::EndChild();
}

/****************************************************************************************
 * 
 * Media Output window
 *
 ***************************************************************************************/
static void ShowMediaOutputWindow(ImDrawList *_draw_list)
{
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    bool multiviewport = io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    static int encoder_stage = 0; // 0:unknown 1:encoding 2:finished
    static double encoder_start = -1, encoder_end = -1, encode_duration = -1;

    if (!timeline)
        return;

    if (ImGui::BeginChild("##Media_output_content", ImGui::GetWindowSize() - ImVec2(4, 4), false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings))
    {
        ImVec2 window_pos = ImGui::GetWindowPos();
        ImVec2 window_size = ImGui::GetWindowSize();
        ImDrawList * draw_list = ImGui::GetWindowDrawList();
        ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
        const ImVec2 item_size(window_size.x, 32);
        draw_list->AddTextComplex(window_pos + ImVec2(8, 0), "Media Output", 2.5f, COL_GRAY_TEXT, 0.5f, IM_COL32(56, 56, 56, 192));

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
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && !timeline->mOutputPath.empty() && ImGui::BeginTooltip())
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
                std::vector<MediaCore::MediaEncoder::Description> * codecs = (std::vector<MediaCore::MediaEncoder::Description>*)data;
                *out_text = codecs->at(idx).longName.c_str();
                return true;
            };
            auto codec_option_getter = [](void* data, int idx, const char** out_text){
                std::vector<MediaCore::MediaEncoder::Option::EnumValue> * profiles = (std::vector<MediaCore::MediaEncoder::Option::EnumValue>*)data;
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
                    if (!MediaCore::MediaEncoder::FindEncoder(codecHint, g_currVidEncDescList))
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
                    if (!MediaCore::MediaEncoder::FindEncoder(codecHint, g_currAudEncDescList))
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
                    ShowVideoWindow(timeline->mEncodingPreviewTexture, preview_pos, preview_size, "Encoding...", 1.5f);
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
                timeline->StopEncoding();
                if (timeline->mEncodingPreviewTexture) { ImGui::ImDestroyTexture(timeline->mEncodingPreviewTexture); timeline->mEncodingPreviewTexture = nullptr; }
                timeline->mEncoder = nullptr;
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
                    auto outColorspaceValue = ColorSpace[g_media_editor_settings.OutputColorSpaceIndex].tag;
                    switch (outColorspaceValue)
                    {
                    case AVCOL_SPC_BT709:
                        vidEncParams.extraOpts.push_back({"color_primaries", MediaCore::Value((int)AVCOL_PRI_BT709)});
                        break;
                    case AVCOL_SPC_FCC:
                        vidEncParams.extraOpts.push_back({"color_primaries", MediaCore::Value((int)AVCOL_PRI_BT470M)});
                        break;
                    case AVCOL_SPC_BT470BG:
                        vidEncParams.extraOpts.push_back({"color_primaries", MediaCore::Value((int)AVCOL_PRI_BT470BG)});
                        break;
                    case AVCOL_SPC_SMPTE170M:
                        vidEncParams.extraOpts.push_back({"color_primaries", MediaCore::Value((int)AVCOL_PRI_SMPTE170M)});
                        break;
                    case AVCOL_SPC_SMPTE240M:
                        vidEncParams.extraOpts.push_back({"color_primaries", MediaCore::Value((int)AVCOL_PRI_SMPTE240M)});
                        break;
                    case AVCOL_SPC_BT2020_NCL:
                    case AVCOL_SPC_BT2020_CL:
                        vidEncParams.extraOpts.push_back({"color_primaries", MediaCore::Value((int)AVCOL_PRI_BT2020)});
                        break;
                    default:
                        vidEncParams.extraOpts.push_back({"color_primaries", MediaCore::Value((int)AVCOL_PRI_UNSPECIFIED)});
                    }
                    vidEncParams.extraOpts.push_back({"colorspace", MediaCore::Value((int)outColorspaceValue)});
                    vidEncParams.extraOpts.push_back({"color_trc", MediaCore::Value((int)(ColorTransfer[g_media_editor_settings.OutputColorTransferIndex].tag))});
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
    }
    ImGui::EndChild();
    
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
static void ShowMediaPreviewWindow(ImDrawList *draw_list, std::string title, float title_size, ImRect& video_rect, int64_t start = -1, int64_t end = -1, bool audio_bar = true, bool monitors = true, bool force_update = false, bool zoom_button = true, bool loop_button = true, bool control_only = false)
{
    // preview control pannel
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 window_pos = ImGui::GetCursorScreenPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    draw_list->AddRectFilled(window_pos, window_pos + window_size, COL_DEEP_DARK);
    bool is_small_window = window_size.x < 600;
    int bar_height = is_small_window ? 32 : 48;
    int bar_y_offset = is_small_window ? 4 : 8;
    ImVec2 PanelBarPos = window_pos + (control_only ? ImVec2(0, 0) : window_size - ImVec2(window_size.x, bar_height));
    ImVec2 PanelBarSize = ImVec2(window_size.x, bar_height);
    draw_list->AddRectFilled(PanelBarPos, PanelBarPos + PanelBarSize, COL_DARK_PANEL);
    bool out_of_border = false;

    // Preview buttons Stop button is center of Panel bar
    auto PanelCenterX = PanelBarPos.x + window_size.x / 2;
    auto PanelButtonY = PanelBarPos.y + bar_y_offset;

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0.5));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2, 0.2, 0.2, 1.0));
    const int b_size = is_small_window ? 24 : 32;
    const int b_gap = window_size.x < 600 ? 0 : window_size.x < 800 ? 2 : 8;
    const int button_gap = b_size + b_gap;
    const ImVec2 button_size = ImVec2(b_size, b_size);

    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX - b_size / 2 - button_gap * 3, PanelButtonY));
    if (ImGui::Button(ICON_TO_START "##preview_tostart", button_size))
    {
        if (timeline)
        {
            if (start < 0) timeline->ToStart();
            else timeline->Seek(start);
        }            
    }
    ImGui::ShowTooltipOnHover("To Start");

    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX - b_size / 2 - button_gap * 2, PanelButtonY));
    if (ImGui::Button(ICON_STEP_BACKWARD "##preview_step_backward", button_size))
    {
        if (timeline)
        {
            if (start < 0) timeline->Step(false);
            else if (timeline->mCurrentTime > start) timeline->Step(false);
        }
    }
    ImGui::ShowTooltipOnHover("Step Prev");

    bool isForwordPlaying = timeline ? (timeline->mIsPreviewPlaying && timeline->mIsPreviewForward) : false;
    bool isBackwardPlaying = (timeline && !isForwordPlaying) ? (timeline->mIsPreviewPlaying && !timeline->mIsPreviewForward) : false;
    
    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX - b_size / 2 - button_gap * 1, PanelButtonY));
    if (ImGui::RotateCheckButton(ICON_PLAY_BACKWARD "##preview_reverse", &isBackwardPlaying, ImVec4(0.5, 0.5, 0.0, 1.0), 180, button_size))
    {
        if (timeline)
        {
            if (start < 0) timeline->Play(true, false);
            else if (timeline->mCurrentTime > start) timeline->Play(true, false);
        }
    }
    ImGui::ShowTooltipOnHover("Reverse");

    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX - b_size / 2 - button_gap * 0, PanelButtonY));
    if (ImGui::Button(ICON_STOP "##preview_stop", button_size))
    {
        if (timeline) timeline->Play(false, true);
        isForwordPlaying = false;
    }
    ImGui::ShowTooltipOnHover("Stop");

    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX + b_size / 2  + b_gap + button_gap * 0, PanelButtonY));
    if (ImGui::RotateCheckButton(ICON_PLAY_FORWARD "##preview_play", &isForwordPlaying, ImVec4(0.5, 0.5, 0.0, 1.0), 0, button_size))
    {
        if (timeline)
        {
            if (start < 0 || end < 0) timeline->Play(true, true);
            else if (timeline->mCurrentTime < end) timeline->Play(true, true);
        }
    }
    ImGui::ShowTooltipOnHover("Play");

    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX + b_size / 2 + b_gap + button_gap * 1, PanelButtonY));
    if (ImGui::Button(ICON_STEP_FORWARD "##preview_step_forward", button_size))
    {
        if (timeline)
        {
            if (end < 0) timeline->Step(true);
            else if (timeline->mCurrentTime < end) timeline->Step(true);
        }
    }
    ImGui::ShowTooltipOnHover("Step Next");

    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX + b_size / 2  + b_gap + button_gap * 2, PanelButtonY));
    if (ImGui::Button(ICON_TO_END "##preview_toend", button_size))
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
        ImGui::SetCursorScreenPos(ImVec2(PanelBarPos.x + PanelBarSize.x - button_gap * 2 - 64, PanelButtonY));
        if (ImGui::RotateCheckButton(ICON_LOOP_ONE "##preview_loop", &loop, ImVec4(0.5, 0.5, 0.0, 1.0), 0, button_size))
        {
            if (timeline)
            {
                timeline->Loop(loop);
            }
        }
        ImGui::ShowTooltipOnHover("Loop");
    }

    if (zoom_button)
    {
        bool zoom = timeline ? timeline->bPreviewZoom : false;
        ImGui::SetCursorScreenPos(ImVec2(PanelBarPos.x + PanelBarSize.x - button_gap * 1 - 64, PanelButtonY));
        if (ImGui::RotateCheckButton(ICON_ZOOM "##preview_zoom", &zoom, ImVec4(0.5, 0.5, 0.0, 1.0), 0, button_size))
        {
            timeline->bPreviewZoom = zoom;
        }
        ImGui::ShowTooltipOnHover("Magnifying");
    }

    // Time stamp on right of control panel
    ImRect TimeStampRect = ImRect(PanelBarPos.x + (monitors ? 48 : 16), PanelBarPos.y + (is_small_window ? 8 : 12), 
                                PanelCenterX - b_size / 2 - button_gap * 3, PanelBarPos.y + bar_height);
    draw_list->PushClipRect(TimeStampRect.Min, TimeStampRect.Max, true);
    ImGui::SetWindowFontScale(is_small_window ? 1.0 : 1.5);
    ImGui::ShowDigitalTime(draw_list, timeline->mCurrentTime, 3, TimeStampRect.Min, timeline->mIsPreviewPlaying ? IM_COL32(255, 255, 0, 255) : COL_MARK);
    ImGui::SetWindowFontScale(1.0);
    draw_list->PopClipRect();

    // audio meters
    if (audio_bar && !control_only)
    {
        ImVec2 AudioMeterPos;
        ImVec2 AudioMeterSize;
        AudioMeterPos = window_pos + ImVec2(window_size.x - 70, 16);
        AudioMeterSize = ImVec2(32, window_size.y - bar_height - 16 - 8);
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
    PreviewSize = window_size - ImVec2(16 + (audio_bar ? 64 : 0), 16 + bar_height);
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

    if (start >= 0 && end >= 0)
    {
        if (timeline->mCurrentTime < start || timeline->mCurrentTime > end)
        {
            out_of_border = true;
            // reach clip border
            if (timeline->mIsPreviewPlaying)
            {
                if (timeline->mCurrentTime < start) { timeline->Play(false, false); timeline->Seek(start); }
                if (timeline->mCurrentTime > end) { timeline->Play(false, true); timeline->Seek(end); }
            }
        }
    }

    if (!control_only)
    {
        float pos_x = 0, pos_y = 0;
        float offset_x = 0, offset_y = 0;
        float tf_x = 0, tf_y = 0;
        ImVec2 scale_range = ImVec2(2.0 / timeline->mPreviewScale, 8.0 / timeline->mPreviewScale);
        static float texture_zoom = scale_range.x;
        ShowVideoWindow(draw_list, timeline->mMainPreviewTexture, PreviewPos, PreviewSize, title, title_size, offset_x, offset_y, tf_x, tf_y, true, out_of_border);
        if (!out_of_border && ImGui::IsItemHovered() && timeline->bPreviewZoom && timeline->mMainPreviewTexture)
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
                ImGui::TextUnformatted("Magnifying: Mouse wheel up to zoom in, down to zoom out");
                ImGui::SameLine(); ImGui::Text("(%.2fx)", texture_zoom);
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
    }
    if (monitors)
    {
        // Show monitors
        std::vector<int> disabled_monitor = {MonitorIndexScope};
        MonitorButton("##preview_monitor_select", ImVec2(PanelBarPos.x + 8, PanelBarPos.y + (is_small_window ? 8 : 16)), MonitorIndexPreviewVideo, disabled_monitor);
    }

    ImGui::PopStyleColor(3);
}

/****************************************************************************************
 * 
 * Media Preview window
 *
 ***************************************************************************************/
static void ShowVideoPreviewWindow(ImDrawList *draw_list, EditingVideoClip* editing_clip, bool vertical = false)
{
    auto clip = editing_clip->GetClip();
    auto start = clip->Start();
    auto end = clip->End();

    // preview control pannel
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 window_pos = ImGui::GetCursorScreenPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    draw_list->AddRectFilled(window_pos, window_pos + window_size, COL_BLACK_DARK);
    bool is_small_window = window_size.x < 600;
    int bar_height = is_small_window ? 32 : 48;
    int bar_y_offset = is_small_window ? 4 : 8;
    ImVec2 PanelBarPos = window_pos + window_size - ImVec2(window_size.x, bar_height);
    ImVec2 PanelBarSize = ImVec2(window_size.x, bar_height);
    draw_list->AddRectFilled(PanelBarPos, PanelBarPos + PanelBarSize, COL_DARK_PANEL);

    // Preview buttons Stop button is center of Panel bar
    auto PanelCenterX = PanelBarPos.x + window_size.x / 2 + (is_small_window ? 16 : 0);
    auto PanelButtonY = PanelBarPos.y + bar_y_offset;
    bool out_of_border = false;

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0.5));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2, 0.2, 0.2, 1.0));
    const int b_size = is_small_window ? 24 : 32;
    const int b_gap = window_size.x < 600 ? 0 : window_size.x < 800 ? 2 : 8;
    const int button_gap = b_size + b_gap;
    const ImVec2 button_size = ImVec2(b_size, b_size);

    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX - b_size / 2 - button_gap * 3, PanelButtonY));
    if (ImGui::Button(ICON_TO_START "##preview_tostart", button_size))
    {
        if (timeline && editing_clip)
        {
            timeline->Seek(start);
        }
    }
    ImGui::ShowTooltipOnHover("To Start");

    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX - b_size / 2 - button_gap * 2, PanelButtonY));
    if (ImGui::Button(ICON_STEP_BACKWARD "##preview_step_backward", button_size))
    {
        if (timeline && editing_clip)
        {
            if (timeline->mCurrentTime > start)
                timeline->Step(false);
        }
    }
    ImGui::ShowTooltipOnHover("Step Prev");

    bool isForwordPlaying = timeline ? (timeline->mIsPreviewPlaying && timeline->mIsPreviewForward) : false;
    bool isBackwardPlaying = (timeline && !isForwordPlaying) ? (timeline->mIsPreviewPlaying && !timeline->mIsPreviewForward) : false;
    
    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX - b_size / 2 - button_gap * 1, PanelButtonY));
    if (ImGui::RotateCheckButton(ICON_PLAY_BACKWARD "##preview_reverse", &isBackwardPlaying, ImVec4(0.5, 0.5, 0.0, 1.0), 180, button_size))
    {
        if (timeline && editing_clip)
        {
            if (timeline->mCurrentTime > start)
                timeline->Play(true, false);
        }
    }
    ImGui::ShowTooltipOnHover("Reverse");

    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX - b_size / 2 - button_gap * 0, PanelButtonY));
    if (ImGui::Button(ICON_STOP "##preview_stop", button_size))
    {
        if (timeline)
            timeline->Play(false, true);
        isForwordPlaying = false;
    }
    ImGui::ShowTooltipOnHover("Stop");

    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX + b_size / 2 + b_gap + button_gap * 0, PanelButtonY));
    if (ImGui::RotateCheckButton(ICON_PLAY_FORWARD "##preview_play", &isForwordPlaying, ImVec4(0.5, 0.5, 0.0, 1.0), 0, button_size))
    {
        if (timeline && editing_clip)
        {
            if (timeline->mCurrentTime < end)
                timeline->Play(true, true);
        }
    }
    ImGui::ShowTooltipOnHover("Play");

    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX + b_size / 2 + b_gap + button_gap * 1, PanelButtonY));
    if (ImGui::Button(ICON_STEP_FORWARD "##preview_step_forward", button_size))
    {
        if (timeline && editing_clip)
        {
            if (timeline->mCurrentTime < end)
                timeline->Step(true);
        }
    }
    ImGui::ShowTooltipOnHover("Step Next");

    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX + b_size / 2 + b_gap + button_gap * 2, PanelButtonY));
    if (ImGui::Button(ICON_TO_END "##preview_toend", button_size))
    {
        if (timeline && editing_clip)
        {
            if (timeline->mCurrentTime < end)
                timeline->Seek(end - 40);
        }
    }
    ImGui::ShowTooltipOnHover("To End");

/*
    if (attribute)
    {
        ImGui::SetCursorScreenPos(ImVec2(PanelBarPos.x + PanelBarSize.x - button_gap * 2 - 64, PanelButtonY));
        if (ImGui::RotateCheckButton(timeline->bAttributeOutputPreview ? ICON_MEDIA_PREVIEW : ICON_FILTER "##video_filter_output_preview", &timeline->bAttributeOutputPreview, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive), 0, button_size))
        {
            timeline->UpdatePreview();
        }
        ImGui::ShowTooltipOnHover(timeline->bAttributeOutputPreview ? "Attribute Output" : "Preview Output");
    }
    else
    {
        ImGui::SetCursorScreenPos(ImVec2(PanelBarPos.x + PanelBarSize.x - button_gap * 2 - 64, PanelButtonY));
        if (ImGui::RotateCheckButton(timeline->bFilterOutputPreview ? ICON_MEDIA_PREVIEW : ICON_FILTER "##video_filter_output_preview", &timeline->bFilterOutputPreview, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive), 0, button_size))
        {
            timeline->UpdatePreview();
        }
        ImGui::ShowTooltipOnHover(timeline->bFilterOutputPreview ? "Filter Output" : "Preview Output");
    }
*/

    ImGui::SetCursorScreenPos(ImVec2(PanelBarPos.x + PanelBarSize.x - button_gap * 2 - 64, PanelButtonY));
    if (ImGui::RotateCheckButton(timeline->bFilterOutputPreview ? ICON_MEDIA_PREVIEW : ICON_FILTER "##video_filter_output_preview", &timeline->bFilterOutputPreview, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive), 0, button_size))
    {
        timeline->UpdatePreview();
    }
    ImGui::ShowTooltipOnHover(timeline->bFilterOutputPreview ? "Filter Output" : "Preview Output");

    ImGui::SetCursorScreenPos(ImVec2(PanelBarPos.x + PanelBarSize.x - button_gap * 1 - 64, PanelButtonY));
    ImGui::RotateCheckButton(ICON_COMPARE "##video_filter_compare", &timeline->bCompare, ImVec4(0.5, 0.5, 0.0, 1.0), 0, button_size);
    ImGui::ShowTooltipOnHover("Zoom Compare");

    // Time stamp on right of control panel
    ImRect TimeStampRect = ImRect(PanelBarPos.x + 32, PanelBarPos.y + (is_small_window ? 8 : 12), 
                                PanelCenterX - b_size / 2 - button_gap * 3, PanelBarPos.y + bar_height);
    draw_list->PushClipRect(TimeStampRect.Min, TimeStampRect.Max, true);
    ImGui::SetWindowFontScale(is_small_window ? 1.0 : 1.5);
    ImGui::ShowDigitalTime(draw_list, timeline->mCurrentTime, 3, TimeStampRect.Min, timeline->mIsPreviewPlaying ? IM_COL32(255, 255, 0, 255) : COL_MARK);
    ImGui::SetWindowFontScale(1.0);
    draw_list->PopClipRect();

    // Show monitors
    std::vector<int> org_disabled_monitor = {MonitorIndexVideoFiltered, MonitorIndexScope};
    MonitorButton("##video_filter_org_monitor_select", ImVec2(PanelBarPos.x + 8, PanelBarPos.y + (is_small_window ? 8 : 16)), MonitorIndexVideoFilterOrg, org_disabled_monitor);
    std::vector<int> filter_disabled_monitor = {MonitorIndexVideoFilterOrg, MonitorIndexScope};
    MonitorButton("##video_filter_monitor_select", ImVec2(PanelBarPos.x + PanelBarSize.x - 8 - b_size, PanelBarPos.y + (is_small_window ? 8 : 16)), MonitorIndexVideoFiltered, filter_disabled_monitor);

    int show_video_number = 0;
    if (MonitorIndexVideoFilterOrg == -1) show_video_number++;
    if (MonitorIndexVideoFiltered == -1) show_video_number++;

    // filter input texture area
    ImVec2 InputVideoPos = window_pos + ImVec2(4, 4);
    ImVec2 InputVideoSize = show_video_number > 0 ? (ImVec2(window_size.x / show_video_number - 8, window_size.y - PanelBarSize.y - 8)) : ImVec2(0, 0);
    ImVec2 OutputVideoPos = window_pos + ImVec2((show_video_number > 1 ? window_size.x / show_video_number : 0) + 4, 4);
    ImVec2 OutputVideoSize = show_video_number > 0 ? (ImVec2(window_size.x / show_video_number - 8, window_size.y - PanelBarSize.y - 8)) : ImVec2(0, 0);
    if (vertical)
    {
        InputVideoSize = show_video_number > 0 ?  (ImVec2(window_size.x - 8, (window_size.y - PanelBarSize.y) / show_video_number - 8)) : ImVec2(0, 0);
        OutputVideoPos = window_pos + ImVec2(4, (show_video_number > 1 ? (window_size.y - PanelBarSize.y) / show_video_number : 0) + 4);
        OutputVideoSize = show_video_number > 0 ? (ImVec2(window_size.x - 8, (window_size.y - PanelBarSize.y) / show_video_number - 8)) : ImVec2(0, 0);
    }
    ImRect InputVideoRect(InputVideoPos, InputVideoPos + InputVideoSize);
    ImRect OutVideoRect(OutputVideoPos, OutputVideoPos + OutputVideoSize);

    ImVec2 VideoZoomPos = window_pos + ImVec2(window_size.x - 740.f, window_size.y - PanelBarSize.y + 4);
    if (editing_clip)
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
        
        std::pair<ImGui::ImMat, ImGui::ImMat> pair;
        bool ret = false;
        bool is_preview_image = timeline->bFilterOutputPreview;
        ret = editing_clip->GetFrame(pair, timeline->bFilterOutputPreview);
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

        if (timeline->mIsPreviewPlaying)
        {
            // reach clip border
            if (timeline->mIsPreviewForward && timeline->mCurrentTime >= end)
            {
                timeline->Play(false, true);
                timeline->Seek(end);
                //out_of_border = true;
            }
            else if (!timeline->mIsPreviewForward && timeline->mCurrentTime <= start)
            {
                timeline->Play(false, false);
                timeline->Seek(start);
                //out_of_border = true;
            }
        }
        else if (timeline->mCurrentTime < start || timeline->mCurrentTime > end)
        {
            out_of_border = true;
        }

        float region_sz = 360.0f / texture_zoom;
        float pos_x = 0, pos_y = 0;
        bool draw_compare = false;
        ImVec4 tint_col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);   // No tint
        ImVec4 border_col = ImVec4(1.0f, 1.0f, 1.0f, 0.5f); // 50% opaque white
        float offset_x = 0, offset_y = 0;
        float tf_x = 0, tf_y = 0;

        if (MonitorIndexVideoFilterOrg == -1)
        {
            // filter input texture area
            ShowVideoWindow(draw_list, timeline->mVideoFilterInputTexture, InputVideoPos, InputVideoSize, "Original", 1.5f, offset_x, offset_y, tf_x, tf_y, true, out_of_border);
            draw_list->AddRect(ImVec2(offset_x, offset_y), ImVec2(tf_x, tf_y), IM_COL32(128,128,128,128), 0, 0, 1.0);
            if (!out_of_border && timeline->mVideoFilterInputTexture && ImGui::IsItemHovered())
            {
                float image_width = ImGui::ImGetTextureWidth(timeline->mVideoFilterInputTexture);
                float image_height = ImGui::ImGetTextureHeight(timeline->mVideoFilterInputTexture);
                float scale_w = image_width / (tf_x - offset_x);
                float scale_h = image_height / (tf_y - offset_y);
                pos_x = (io.MousePos.x - offset_x) * scale_w;
                pos_y = (io.MousePos.y - offset_y) * scale_h;
                if (io.MouseStrawed)
                {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Straw);
                    draw_list->AddRect(io.MousePos - ImVec2(2, 2), io.MousePos + ImVec2(2, 2), IM_COL32(255,0, 0,255));
                    auto pixel = ImGui::ImGetTexturePixel(timeline->mVideoFilterInputTexture, pos_x, pos_y);
                    if (ImGui::BeginTooltip())
                    {
                        ImGui::ColorButton("##straw_color", ImVec4(pixel.r, pixel.g, pixel.b, pixel.a), 0, ImVec2(64,64));
                        ImGui::Text("x:%d y:%d", (int)pos_x, (int)pos_y);
                        ImGui::Text("R:%d G:%d B:%d A:%d", (int)(pixel.r * 255), (int)(pixel.g * 255), (int)(pixel.b * 255), (int)(pixel.a * 255));
                        ImGui::EndTooltip();
                    }
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                    {
                        io.MouseStrawValue = ImVec4(pixel.r, pixel.g, pixel.b, pixel.a);
                    }
                }
                else
                {
                    draw_compare = true;
                }
            }
        }

        if (MonitorIndexVideoFiltered == -1)
        {
            // filter output texture area
            ShowVideoWindow(draw_list, timeline->mVideoFilterOutputTexture, OutputVideoPos, OutputVideoSize, is_preview_image ? "Preview Output" : "Filter Output", 1.5f, offset_x, offset_y, tf_x, tf_y, true, out_of_border);
            draw_list->AddRect(ImVec2(offset_x, offset_y), ImVec2(tf_x, tf_y), IM_COL32(128,128,128,128), 0, 0, 1.0);
            if (!out_of_border && timeline->mVideoFilterOutputTexture)
            {
                bool bInMaskEventRange = timeline->mCurrentTime >= start+editing_clip->mMaskEventStart && timeline->mCurrentTime < start+editing_clip->mMaskEventEnd;
                if (editing_clip->mhMaskCreator && bInMaskEventRange)
                {
                    int64_t i64Tick = timeline->mCurrentTime-(start+editing_clip->mMaskEventStart);
                    if (editing_clip->mhMaskCreator->DrawContent({offset_x, offset_y}, {tf_x-offset_x, tf_y-offset_y}, true, i64Tick))
                    {
                        auto pTrack = timeline->FindTrackByClipID(editing_clip->mID);
                        timeline->RefreshTrackView({ pTrack->mID });
                    }
                }
                else if (ImGui::IsItemHovered())
                {
                    float image_width = ImGui::ImGetTextureWidth(timeline->mVideoFilterOutputTexture);
                    float image_height = ImGui::ImGetTextureHeight(timeline->mVideoFilterOutputTexture);
                    float scale_w = image_width / (tf_x - offset_x);
                    float scale_h = image_height / (tf_y - offset_y);
                    pos_x = (io.MousePos.x - offset_x) * scale_w;
                    pos_y = (io.MousePos.y - offset_y) * scale_h;
                    draw_compare = true;
                }
            }
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
                //ImGui::SetNextWindowPos(VideoZoomPos);
                ImGui::SetNextWindowBgAlpha(1.0);
                if (ImGui::BeginTooltip())
                {
                    ImGui::TextUnformatted("Compare View: Mouse wheel up to zoom in, down to zoom out");
                    ImGui::SameLine(); ImGui::Text("(%.2fx)", texture_zoom);
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
    ImGui::PopStyleColor(3);
}
/****************************************************************************************
 * 
 * Filter Blueprint windows
 *
 ***************************************************************************************/
static void ShowClipBluePrintWindow(ImDrawList *draw_list, BaseEditingClip * editing)
{
    if (!editing) return;
    auto clip = editing->GetClip();
    bool is_audio_clip = IS_AUDIO(clip->mType);
    bool is_video_clip = IS_VIDEO(clip->mType);
    if (!timeline || (!is_audio_clip && !is_video_clip))
        return;
    BluePrint::BluePrintUI* pBp = nullptr;
    MEC::Event::Holder hTargetEvent;

    if (is_audio_clip)
    {
        EditingAudioClip * editing_clip = (EditingAudioClip *)editing;
        MediaCore::AudioFilter* pFilter = editing_clip->mFilter;
        auto filterName = pFilter->GetFilterName();
        if (filterName == "EventStackFilter")
        {
            MEC::AudioEventStackFilter* pEsf = dynamic_cast<MEC::AudioEventStackFilter*>(pFilter);
            hTargetEvent = pEsf->GetEditingEvent();
            if (hTargetEvent)
                pBp = hTargetEvent->GetBp();
            else
            {
                hTargetEvent = clip->FindSelectedEvent();
                if (hTargetEvent)
                {
                    pEsf->SetEditingEvent(hTargetEvent->Id());
                    pBp = hTargetEvent->GetBp();
                }
            }
        }
    }
    if (is_video_clip)
    {
        EditingVideoClip * editing_clip = (EditingVideoClip *)editing;
        MediaCore::VideoFilter* pFilter = editing_clip->mFilter;
        auto filterName = pFilter->GetFilterName();
        if (filterName == "EventStackFilter")
        {
            MEC::VideoEventStackFilter* pEsf = dynamic_cast<MEC::VideoEventStackFilter*>(pFilter);
            hTargetEvent = pEsf->GetEditingEvent();
            if (hTargetEvent)
                pBp = hTargetEvent->GetBp();
            else
            {
                hTargetEvent = clip->FindSelectedEvent();
                if (hTargetEvent)
                {
                    pEsf->SetEditingEvent(hTargetEvent->Id());
                    pBp = hTargetEvent->GetBp();
                }
            }
        }
    }
    if (pBp)
    {
        if (clip && !pBp->m_Document->m_Blueprint.IsOpened())
        {
            auto track = timeline->FindTrackByClipID(clip->mID);
            if (track)
                track->SelectEditingClip(clip);
            pBp->View_ZoomToContent();
        }
        ImVec2 window_pos = ImGui::GetCursorScreenPos();
        ImVec2 window_size = ImGui::GetWindowSize();
        ImGui::SetCursorScreenPos(window_pos + ImVec2(3, 3));
        ImGui::InvisibleButton("editor_blueprint_back_view", window_size - ImVec2(6, 6));
        if (ImGui::BeginDragDropTarget() && pBp->Blueprint_IsValid())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(is_audio_clip ? "Filter_drag_drop_Audio" : "Filter_drag_drop_Video"))
            {
                if (payload->Data)
                {
                    clip->AppendEvent(hTargetEvent, payload->Data);
                    project_changed = true;
                }
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::SetCursorScreenPos(window_pos + ImVec2(1, 1));
        if (ImGui::BeginChild("##filter_editor_blueprint", window_size - ImVec2(2, 2), false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings))
        {
            pBp->Frame(true, true, clip != nullptr, BluePrint::BluePrintFlag::BluePrintFlag_Filter);
        }
        ImGui::EndChild();
        if (timeline->mIsBluePrintChanged) { project_changed = true; project_need_save = true; }
    }
    else
    {
        ImVec2 window_pos = ImGui::GetCursorScreenPos();
        ImVec2 window_size = ImGui::GetWindowSize();
        ImGui::SetWindowFontScale(2);
        auto pos_center = window_pos + window_size / 2;
        std::string tips_string = "Please Select event from event Timeline";
        auto string_width = ImGui::CalcTextSize(tips_string.c_str());
        auto tips_pos = pos_center - string_width / 2;
        ImGui::SetWindowFontScale(1);
        draw_list->AddTextComplex(tips_pos, tips_string.c_str(), 2.f, IM_COL32(255, 255, 255, 128), 0.5f, IM_COL32(56, 56, 56, 192));
    }
}

static void DrawClipBlueprintWindow(ImDrawList *draw_list, BaseEditingClip * editing)
{
    ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
    ImVec2 sub_window_size = ImGui::GetWindowSize();
    draw_list->AddRectFilled(sub_window_pos, sub_window_pos + sub_window_size, COL_DARK_ONE);
    ShowClipBluePrintWindow(draw_list, editing);
}

static void DrawClipEventWindow(ImDrawList *draw_list, BaseEditingClip * editing)
{
    bool changed = false;
    if (!editing) return;
    auto editing_clip = editing->GetClip();
    if (!editing_clip || !editing_clip->mEventStack)
        return;
    bool is_audio_clip = IS_AUDIO(editing_clip->mType);
    bool is_video_clip = IS_VIDEO(editing_clip->mType);
    if (!timeline || (!is_audio_clip && !is_video_clip))
        return;

    ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
    ImVec2 sub_window_size = ImGui::GetWindowSize();
    EditingVideoClip* pEdtVidClip = nullptr;
    if (is_video_clip)
        pEdtVidClip = dynamic_cast<EditingVideoClip*>(editing);

    static const char* buttons[] = { "Delete", "Cancel", NULL };
    static ImGui::MsgBox msgbox_event;
    msgbox_event.Init("Delete Event?", ICON_MD_WARNING, "Are you really sure you want to delete event?", buttons, false);

    static ImGui::MsgBox msgbox_node;
    msgbox_node.Init("Delete Node?", ICON_MD_WARNING, "Are you really sure you want to delete node?", buttons, false);

    int64_t trackId = -1;
    auto track = timeline->FindTrackByClipID(editing_clip->mID);
    if (track) trackId = track->mID;

    char ** curve_type_list = nullptr;
    auto curve_type_count = ImGui::ImCurveEdit::GetCurveTypeName(curve_type_list);

    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(1.0,1.0,1.0,1.0));

    // attribute setting
    if (is_video_clip)
    {
        EditingVideoClip * vclip = (EditingVideoClip *)editing;
        auto attribute = vclip->mAttribute;
        if (attribute)
        {
            auto attribute_keypoint = attribute->GetKeyPoint();
            if (attribute_keypoint)
            {
                attribute_keypoint->SetMin(ImVec4(0, 0, 0, 0));
                attribute_keypoint->SetMax(ImVec4(1, 1, 1, editing_clip->Length()), true);
            }
        }
         // reflush timeline
        auto Reflush = [&]()
        {
            timeline->RefreshTrackView({trackId});
            project_changed = true;
        };
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
                    auto curve_index = attribute_keypoint->AddCurveByDim(name, ImGui::ImCurveEdit::Smooth, color, true, ImGui::ImCurveEdit::DIM_X, _min, _max, _default);
                    attribute_keypoint->AddPointByDim(curve_index, ImVec2(0, _default), ImGui::ImCurveEdit::Smooth, ImGui::ImCurveEdit::DIM_X, true);
                    attribute_keypoint->AddPointByDim(curve_index, ImVec2(editing_clip->Length(), _default), ImGui::ImCurveEdit::Smooth, ImGui::ImCurveEdit::DIM_X, true);
                    project_changed = true;
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
                        float value = attribute_keypoint->GetValueByDim(index, editing->mCurrentTime, ImGui::ImCurveEdit::DIM_X);
                        ImGui::BracketSquare(true); ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0, 1.0, 0.0, 1.0)); ImGui::Text("%.2f", value); ImGui::PopStyleColor();
                        
                        ImGui::PushItemWidth(60);
                        float curve_min = attribute_keypoint->GetCurveMinByDim(index, ImGui::ImCurveEdit::DIM_X);
                        float curve_max = attribute_keypoint->GetCurveMaxByDim(index, ImGui::ImCurveEdit::DIM_X);
                        ImGui::BeginDisabled(true);
                        ImGui::DragFloat("##curve_video_filter_min", &curve_min, 0.1f, -FLT_MAX, curve_max, "%.1f"); ImGui::ShowTooltipOnHover("Min");
                        ImGui::SameLine(0, 8);
                        ImGui::DragFloat("##curve_video_filter_max", &curve_max, 0.1f, curve_min, FLT_MAX, "%.1f"); ImGui::ShowTooltipOnHover("Max");
                        ImGui::SameLine(0, 8);
                        ImGui::EndDisabled();
                        float curve_default = attribute_keypoint->GetCurveDefaultByDim(index, ImGui::ImCurveEdit::DIM_X);
                        if (ImGui::DragFloat("##curve_video_attribute_default", &curve_default, 0.1f, curve_min, curve_max, "%.1f"))
                        {
                            attribute_keypoint->SetCurveDefaultByDim(index, curve_default, ImGui::ImCurveEdit::DIM_X);
                            Reflush();
                        } ImGui::ShowTooltipOnHover("Default");
                        ImGui::PopItemWidth();

                        ImGui::SameLine(0, 8);
                        ImGui::SetWindowFontScale(0.75);
                        auto curve_color = ImGui::ColorConvertU32ToFloat4(attribute_keypoint->GetCurveColor(index));
                        if (ImGui::ColorEdit4("##curve_video_attribute_color", (float*)&curve_color, ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar))
                        {
                            attribute_keypoint->SetCurveColor(index, ImGui::ColorConvertFloat4ToU32(curve_color));
                            project_changed = true;
                        } ImGui::ShowTooltipOnHover("Curve Color");
                        ImGui::SetWindowFontScale(1.0);
                        ImGui::SameLine(0, 4);
                        bool is_visiable = attribute_keypoint->IsVisible(index);
                        if (ImGui::Button(is_visiable ? ICON_WATCH : ICON_UNWATCH "##curve_video_attribute_visiable"))
                        {
                            is_visiable = !is_visiable;
                            attribute_keypoint->SetCurveVisible(index, is_visiable);
                            project_changed = true;
                        } ImGui::ShowTooltipOnHover(is_visiable ? "Hide" : "Show");
                        ImGui::SameLine(0, 4);
                        if (ImGui::Button(ICON_DELETE "##curve_video_attribute_delete"))
                        {
                            attribute_keypoint->DeleteCurve(index);
                            Reflush();
                            break_loop = true;
                        } ImGui::ShowTooltipOnHover("Delete");
                        ImGui::SameLine(0, 4);
                        if (ImGui::Button(ICON_RETURN_DEFAULT "##curve_video_attribute_reset"))
                        {
                            for (int p = 0; p < pCount; p++)
                            {
                                attribute_keypoint->SetCurvePointDefault(index, p);
                            }
                            Reflush();
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
                                if (ImGui::DragTimeMS("##curve_video_attribute_point_time", &point.t, attribute_keypoint->GetMax().w / 1000.f, attribute_keypoint->GetMin().w, attribute_keypoint->GetMax().w, 2))
                                {
                                    attribute_keypoint->EditPoint(index, p, point.val, point.type);
                                    Reflush();
                                }
                                ImGui::EndDisabled();
                                ImGui::SameLine();
                                const auto curveMin = attribute_keypoint->GetCurveMinByDim(index, ImGui::ImCurveEdit::DIM_T);
                                const auto curveMax = attribute_keypoint->GetCurveMaxByDim(index, ImGui::ImCurveEdit::DIM_T);
                                auto speed = fabs(curveMax - curveMin) / 500;
                                if (ImGui::DragFloat("##curve_video_attribute_point_time", &point.t, speed, curveMin, curveMax, "%.2f"))
                                {
                                    attribute_keypoint->EditPoint(index, p, point.val, point.type);
                                    Reflush();
                                }
                                ImGui::SameLine();
                                if (ImGui::Combo("##curve_video_attribute_type", (int*)&point.type, curve_type_list, curve_type_count))
                                {
                                    attribute_keypoint->EditPoint(index, p, point.val, point.type);
                                    Reflush();
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
        if (attribute && ImGui::TreeNodeEx("Video Attribute", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (editing_clip->bAttributeScrolling)
            {
                ImGui::ScrollToItem(ImGuiScrollFlags_KeepVisibleCenterY);
                editing_clip->bAttributeScrolling = false;
            }
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushItemWidth(200);
            float setting_offset = sub_window_size.x - 80;
            static ImGuiSliderFlags flags = ImGuiSliderFlags_AlwaysClamp; // ImGuiSliderFlags_NoInput
            auto attribute_keypoint = attribute->GetKeyPoint();
            // Attribute Crop setting
            if (ImGui::TreeNodeEx("Crop Setting##video_attribute", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::ImCurveEdit::Curve margin_key; margin_key.m_id = editing_clip->mID;
                // Crop Margin Left
                int curve_margin_l_index = attribute_keypoint ? attribute_keypoint->GetCurveIndex("CropMarginL") : -1;
                bool has_curve_margin_l = attribute_keypoint ? curve_margin_l_index != -1 : false;
                float margin_l = has_curve_margin_l ? attribute_keypoint->GetValueByDim(curve_margin_l_index, editing->mCurrentTime, ImGui::ImCurveEdit::DIM_X) : attribute->GetCropMarginLScale();
                ImGui::BeginDisabled(has_curve_margin_l);
                if (ImGui::SliderFloat("Crop Left", &margin_l, 0.f, 1.f, "%.3f", flags))
                {
                    attribute->SetCropMarginL(margin_l);
                    Reflush();
                }
                ImGui::SameLine(setting_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##crop_marhin_l_default")) { attribute->SetCropMarginL(0.f); Reflush(); }
                ImGui::ShowTooltipOnHover("Reset");
                ImGui::EndDisabled();
                if (ImGui::ImCurveCheckEditKeyByDim("##add_curve_margin_l##video_attribute", &margin_key, ImGui::ImCurveEdit::DIM_X, has_curve_margin_l, "margin_l##video_attribute", 0.f, 1.f, 0.f))
                {
                    if (has_curve_margin_l) addCurve("CropMarginL",
                            ImGui::ImCurveEdit::GetDimVal(margin_key.m_min, ImGui::ImCurveEdit::DIM_X),
                            ImGui::ImCurveEdit::GetDimVal(margin_key.m_max, ImGui::ImCurveEdit::DIM_X),
                            ImGui::ImCurveEdit::GetDimVal(margin_key.m_default, ImGui::ImCurveEdit::DIM_X));
                    else if (attribute_keypoint) attribute_keypoint->DeleteCurve("CropMarginL");
                    Reflush();
                }
                if (has_curve_margin_l) EditCurve("CropMarginL");

                // Crop Margin Top
                int curve_margin_t_index = attribute_keypoint ? attribute_keypoint->GetCurveIndex("CropMarginT") : -1;
                bool has_curve_margin_t = attribute_keypoint ? curve_margin_t_index != -1 : false;
                float margin_t = has_curve_margin_t ? attribute_keypoint->GetValueByDim(curve_margin_t_index, editing->mCurrentTime, ImGui::ImCurveEdit::DIM_X) : attribute->GetCropMarginTScale();
                ImGui::BeginDisabled(has_curve_margin_t);
                if (ImGui::SliderFloat("Crop Top", &margin_t, 0.f, 1.f))
                {
                    attribute->SetCropMarginT(margin_t);
                    Reflush();
                }
                ImGui::SameLine(setting_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##crop_marhin_t_default")) { attribute->SetCropMarginT(0.f); Reflush(); }
                ImGui::ShowTooltipOnHover("Reset");
                ImGui::EndDisabled();
                if (ImGui::ImCurveCheckEditKeyByDim("##add_curve_margin_t##video_attribute", &margin_key, ImGui::ImCurveEdit::DIM_X, has_curve_margin_t, "margin_t##video_attribute", 0.f, 1.f, 0.f))
                {
                    if (has_curve_margin_t) addCurve("CropMarginT",
                            ImGui::ImCurveEdit::GetDimVal(margin_key.m_min, ImGui::ImCurveEdit::DIM_X),
                            ImGui::ImCurveEdit::GetDimVal(margin_key.m_max, ImGui::ImCurveEdit::DIM_X),
                            ImGui::ImCurveEdit::GetDimVal(margin_key.m_default, ImGui::ImCurveEdit::DIM_X));
                    else if (attribute_keypoint) attribute_keypoint->DeleteCurve("CropMarginT");
                    Reflush();
                }
                if (has_curve_margin_t) EditCurve("CropMarginT");

                // Crop Margin Right
                int curve_margin_r_index = attribute_keypoint ? attribute_keypoint->GetCurveIndex("CropMarginR") : -1;
                bool has_curve_margin_r = attribute_keypoint ? curve_margin_r_index != -1 : false;
                float margin_r = has_curve_margin_r ? attribute_keypoint->GetValueByDim(curve_margin_r_index, editing->mCurrentTime, ImGui::ImCurveEdit::DIM_X) : attribute->GetCropMarginRScale();
                ImGui::BeginDisabled(has_curve_margin_r);
                if (ImGui::SliderFloat("Crop Right", &margin_r, 0.f, 1.f))
                {
                    attribute->SetCropMarginR(margin_r);
                    Reflush();
                }
                ImGui::SameLine(setting_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##crop_marhin_r_default")) { attribute->SetCropMarginR(0.f); Reflush(); }
                ImGui::ShowTooltipOnHover("Reset");
                ImGui::EndDisabled();
                if (ImGui::ImCurveCheckEditKeyByDim("##add_curve_margin_r##video_attribute", &margin_key, ImGui::ImCurveEdit::DIM_X, has_curve_margin_r, "margin_r##video_attribute", 0.f, 1.f, 0.f))
                {
                    if (has_curve_margin_r) addCurve("CropMarginR",
                            ImGui::ImCurveEdit::GetDimVal(margin_key.m_min, ImGui::ImCurveEdit::DIM_X),
                            ImGui::ImCurveEdit::GetDimVal(margin_key.m_max, ImGui::ImCurveEdit::DIM_X),
                            ImGui::ImCurveEdit::GetDimVal(margin_key.m_default, ImGui::ImCurveEdit::DIM_X));
                    else if (attribute_keypoint) attribute_keypoint->DeleteCurve("CropMarginR");
                    Reflush();
                }
                if (has_curve_margin_r) EditCurve("CropMarginR");

                // Crop Margin Bottom
                int curve_margin_b_index = attribute_keypoint ? attribute_keypoint->GetCurveIndex("CropMarginB") : -1;
                bool has_curve_margin_b = attribute_keypoint ? curve_margin_b_index != -1 : false;
                float margin_b = has_curve_margin_b ? attribute_keypoint->GetValueByDim(curve_margin_b_index, editing->mCurrentTime, ImGui::ImCurveEdit::DIM_X) : attribute->GetCropMarginBScale();
                ImGui::BeginDisabled(has_curve_margin_b);
                if (ImGui::SliderFloat("Crop Bottom", &margin_b, 0.f, 1.f))
                {
                    attribute->SetCropMarginB(margin_b);
                    Reflush();
                }
                ImGui::SameLine(setting_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##crop_marhin_b_default")) { attribute->SetCropMarginB(0.f); Reflush(); }
                ImGui::ShowTooltipOnHover("Reset");
                ImGui::EndDisabled();
                if (ImGui::ImCurveCheckEditKeyByDim("##add_curve_margin_b##video_attribute", &margin_key, ImGui::ImCurveEdit::DIM_X, has_curve_margin_b, "margin_b##video_attribute", 0.f, 1.f, 0.f))
                {
                    if (has_curve_margin_b) addCurve("CropMarginB",
                            ImGui::ImCurveEdit::GetDimVal(margin_key.m_min, ImGui::ImCurveEdit::DIM_X),
                            ImGui::ImCurveEdit::GetDimVal(margin_key.m_max, ImGui::ImCurveEdit::DIM_X),
                            ImGui::ImCurveEdit::GetDimVal(margin_key.m_default, ImGui::ImCurveEdit::DIM_X));
                    else if (attribute_keypoint) attribute_keypoint->DeleteCurve("CropMarginB");
                    Reflush();
                }
                if (has_curve_margin_b) EditCurve("CropMarginB");
                ImGui::TreePop();
            }
            
            // Attribute Position setting
            if (ImGui::TreeNodeEx("Position Setting##video_attribute", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::ImCurveEdit::Curve margin_key; margin_key.m_id = editing_clip->mID;
                // Position offset H
                int curve_position_h_index = attribute_keypoint ? attribute_keypoint->GetCurveIndex("PositionOffsetH") : -1;
                bool has_curve_position_h = attribute_keypoint ? curve_position_h_index != -1 : false;
                float position_h = has_curve_position_h ? attribute_keypoint->GetValueByDim(curve_position_h_index, editing->mCurrentTime, ImGui::ImCurveEdit::DIM_X) : attribute->GetPositionOffsetHScale();
                ImGui::BeginDisabled(has_curve_position_h);
                if (ImGui::SliderFloat("Position H", &position_h, -1.f, 1.f))
                {
                    attribute->SetPositionOffsetH(position_h);
                    Reflush();
                }
                ImGui::SameLine(setting_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##position_h_default")) { attribute->SetPositionOffsetH(0.f); Reflush(); }
                ImGui::ShowTooltipOnHover("Reset");
                ImGui::EndDisabled();
                if (ImGui::ImCurveCheckEditKeyByDim("##add_curve_position_h##video_attribute", &margin_key, ImGui::ImCurveEdit::DIM_X, has_curve_position_h, "position_h##video_attribute", -1.f, 1.f, 0.f))
                {
                    if (has_curve_position_h) addCurve("PositionOffsetH",
                            ImGui::ImCurveEdit::GetDimVal(margin_key.m_min, ImGui::ImCurveEdit::DIM_X),
                            ImGui::ImCurveEdit::GetDimVal(margin_key.m_max, ImGui::ImCurveEdit::DIM_X),
                            ImGui::ImCurveEdit::GetDimVal(margin_key.m_default, ImGui::ImCurveEdit::DIM_X));
                    else if (attribute_keypoint) attribute_keypoint->DeleteCurve("PositionOffsetH");
                    Reflush();
                }
                if (has_curve_position_h) EditCurve("PositionOffsetH");

                // Position offset V
                int curve_position_v_index = attribute_keypoint ? attribute_keypoint->GetCurveIndex("PositionOffsetV") : -1;
                bool has_curve_position_v = attribute_keypoint ? curve_position_v_index != -1 : false;
                float position_v = has_curve_position_v ? attribute_keypoint->GetValueByDim(curve_position_v_index, editing->mCurrentTime, ImGui::ImCurveEdit::DIM_X) : attribute->GetPositionOffsetVScale();
                ImGui::BeginDisabled(has_curve_position_v);
                if (ImGui::SliderFloat("Position V", &position_v, -1.f, 1.f))
                {
                    attribute->SetPositionOffsetV(position_v);
                    Reflush();
                }
                ImGui::SameLine(setting_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##position_v_default")) { attribute->SetPositionOffsetV(0.f); Reflush(); }
                ImGui::ShowTooltipOnHover("Reset");
                ImGui::EndDisabled();
                if (ImGui::ImCurveCheckEditKeyByDim("##add_curve_position_v##video_attribute", &margin_key, ImGui::ImCurveEdit::DIM_X, has_curve_position_v, "position_v##video_attribute", -1.f, 1.f, 0.f))
                {
                    if (has_curve_position_v) addCurve("PositionOffsetV",
                            ImGui::ImCurveEdit::GetDimVal(margin_key.m_min, ImGui::ImCurveEdit::DIM_X),
                            ImGui::ImCurveEdit::GetDimVal(margin_key.m_max, ImGui::ImCurveEdit::DIM_X),
                            ImGui::ImCurveEdit::GetDimVal(margin_key.m_default, ImGui::ImCurveEdit::DIM_X));
                    else if (attribute_keypoint) attribute_keypoint->DeleteCurve("PositionOffsetV");
                    Reflush();
                }
                if (has_curve_position_v) EditCurve("PositionOffsetV");
                ImGui::TreePop();
            }

            // Attribute Scale setting
            if (ImGui::TreeNodeEx("Scale Setting##video_attribute", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::ImCurveEdit::Curve margin_key; margin_key.m_id = editing_clip->mID;
                // ScaleType as scale method
                MediaCore::ScaleType scale_type = attribute->GetScaleType();
                ImGui::PushItemWidth(100);
                if (ImGui::Combo("Scale Type##curve_video_attribute_scale_type", (int*)&scale_type, VideoAttributeScaleType, IM_ARRAYSIZE(VideoAttributeScaleType)))
                {
                    attribute->SetScaleType(scale_type);
                    Reflush();
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
                    float scale = has_curve_scale ? attribute_keypoint->GetValueByDim(curve_scale_index, editing->mCurrentTime, ImGui::ImCurveEdit::DIM_X) : (attribute->GetScaleH() + attribute->GetScaleV()) / 2;
                    ImGui::BeginDisabled(has_curve_scale);
                    if (ImGui::SliderFloat("Scale", &scale, 0, 8.f, "%.1f"))
                    {
                        attribute->SetScaleH(scale);
                        attribute->SetScaleV(scale);
                        Reflush();
                    }
                    ImGui::SameLine(setting_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##scale_default")) { attribute->SetScaleH(1.0); attribute->SetScaleV(1.0); Reflush(); }
                    ImGui::ShowTooltipOnHover("Reset");
                    ImGui::EndDisabled();
                    if (ImGui::ImCurveCheckEditKeyByDim("##add_curve_scale##video_attribute", &margin_key, ImGui::ImCurveEdit::DIM_X, has_curve_scale, "scale##video_attribute", 0, 8.f, 1.f))
                    {
                        if (has_curve_scale) addCurve("Scale",
                                ImGui::ImCurveEdit::GetDimVal(margin_key.m_min, ImGui::ImCurveEdit::DIM_X),
                                ImGui::ImCurveEdit::GetDimVal(margin_key.m_max, ImGui::ImCurveEdit::DIM_X),
                                ImGui::ImCurveEdit::GetDimVal(margin_key.m_default, ImGui::ImCurveEdit::DIM_X));
                        else if (attribute_keypoint) attribute_keypoint->DeleteCurve("Scale");
                        Reflush();
                    }
                    if (has_curve_scale) EditCurve("Scale");
                }
                else
                {
                    // Scale H
                    int curve_scale_h_index = attribute_keypoint ? attribute_keypoint->GetCurveIndex("ScaleH") : -1;
                    bool has_curve_scale_h = attribute_keypoint ? curve_scale_h_index != -1 : false;
                    float scale_h = has_curve_scale_h ? attribute_keypoint->GetValueByDim(curve_scale_h_index, editing->mCurrentTime, ImGui::ImCurveEdit::DIM_X) : attribute->GetScaleH();
                    ImGui::BeginDisabled(has_curve_scale_h);
                    if (ImGui::SliderFloat("Scale H", &scale_h, 0, 8.f, "%.1f"))
                    {
                        attribute->SetScaleH(scale_h);
                        Reflush();
                    }
                    ImGui::SameLine(setting_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##scale_h_default")) { attribute->SetScaleH(1.0); Reflush(); }
                    ImGui::ShowTooltipOnHover("Reset");
                    ImGui::EndDisabled();
                    if (ImGui::ImCurveCheckEditKeyByDim("##add_curve_scale_h##video_attribute", &margin_key, ImGui::ImCurveEdit::DIM_X, has_curve_scale_h, "scale_h##video_attribute", 0, 8.f, 1.f))
                    {
                        if (has_curve_scale_h) addCurve("ScaleH",
                                ImGui::ImCurveEdit::GetDimVal(margin_key.m_min, ImGui::ImCurveEdit::DIM_X),
                                ImGui::ImCurveEdit::GetDimVal(margin_key.m_max, ImGui::ImCurveEdit::DIM_X),
                                ImGui::ImCurveEdit::GetDimVal(margin_key.m_default, ImGui::ImCurveEdit::DIM_X));
                        else if (attribute_keypoint) attribute_keypoint->DeleteCurve("ScaleH");
                        Reflush();
                    }
                    if (has_curve_scale_h) EditCurve("ScaleH");

                    // Scale V
                    int curve_scale_v_index = attribute_keypoint ? attribute_keypoint->GetCurveIndex("ScaleV") : -1;
                    bool has_curve_scale_v = attribute_keypoint ? curve_scale_v_index != -1 : false;
                    float scale_v = has_curve_scale_v ? attribute_keypoint->GetValueByDim(curve_scale_v_index, editing->mCurrentTime, ImGui::ImCurveEdit::DIM_X) : attribute->GetScaleV();
                    ImGui::BeginDisabled(has_curve_scale_v);
                    if (ImGui::SliderFloat("Scale V", &scale_v, 0, 8.f, "%.1f"))
                    {
                        attribute->SetScaleV(scale_v);
                        Reflush();
                    }
                    ImGui::SameLine(setting_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##scale_v_default")) { attribute->SetScaleV(1.0); Reflush(); }
                    ImGui::ShowTooltipOnHover("Reset");
                    ImGui::EndDisabled();
                    if (ImGui::ImCurveCheckEditKeyByDim("##add_curve_scale_v##video_attribute", &margin_key, ImGui::ImCurveEdit::DIM_X, has_curve_scale_v, "scale_v##video_attribute", 0, 8.f, 1.f))
                    {
                        if (has_curve_scale_v) addCurve("ScaleV",
                                ImGui::ImCurveEdit::GetDimVal(margin_key.m_min, ImGui::ImCurveEdit::DIM_X),
                                ImGui::ImCurveEdit::GetDimVal(margin_key.m_max, ImGui::ImCurveEdit::DIM_X),
                                ImGui::ImCurveEdit::GetDimVal(margin_key.m_default, ImGui::ImCurveEdit::DIM_X));
                        else if (attribute_keypoint) attribute_keypoint->DeleteCurve("ScaleV");
                        Reflush();
                    }
                    if (has_curve_scale_v) EditCurve("ScaleV");
                }
                ImGui::TreePop();
            }

            // Attribute Angle setting
            if (ImGui::TreeNodeEx("Angle Setting##video_attribute", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::ImCurveEdit::Curve margin_key; margin_key.m_id = editing_clip->mID;

                // Rotate angle
                int curve_angle_index = attribute_keypoint ? attribute_keypoint->GetCurveIndex("RotateAngle") : -1;
                bool has_curve_angle = attribute_keypoint ? curve_angle_index != -1 : false;
                float angle = has_curve_angle ? attribute_keypoint->GetValueByDim(curve_angle_index, editing->mCurrentTime, ImGui::ImCurveEdit::DIM_X) : attribute->GetRotationAngle();
                ImGui::BeginDisabled(has_curve_angle);
                if (ImGui::SliderFloat("Rotate Angle", &angle, -360.f, 360.f, "%.0f"))
                {
                    attribute->SetRotationAngle(angle);
                    Reflush();
                }
                ImGui::SameLine(setting_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##angle_default")) { attribute->SetRotationAngle(0.0); Reflush(); }
                ImGui::ShowTooltipOnHover("Reset");
                ImGui::EndDisabled();
                if (ImGui::ImCurveCheckEditKeyByDim("##add_curve_angle##video_attribute", &margin_key, ImGui::ImCurveEdit::DIM_X, has_curve_angle, "angle##video_attribute", -360.f, 360.f, 0.f))
                {
                    if (has_curve_angle) addCurve("RotateAngle",
                            ImGui::ImCurveEdit::GetDimVal(margin_key.m_min, ImGui::ImCurveEdit::DIM_X),
                            ImGui::ImCurveEdit::GetDimVal(margin_key.m_max, ImGui::ImCurveEdit::DIM_X),
                            ImGui::ImCurveEdit::GetDimVal(margin_key.m_default, ImGui::ImCurveEdit::DIM_X));
                    else if (attribute_keypoint) attribute_keypoint->DeleteCurve("RotateAngle");
                    Reflush();
                }
                if (has_curve_angle) EditCurve("RotateAngle");
                ImGui::TreePop();
            }

            ImGui::PopItemWidth();
            ImGui::PopStyleColor();
            ImGui::TreePop();
        }
    }

    ImGui::Separator();
    ImGui::PopStyleColor();
    // event editing
    auto event_list = editing_clip->mEventStack->GetEventList();
    if (event_list.empty())
    {
        //ImGui::SetWindowFontScale(2);
        //auto pos_center = sub_window_pos + sub_window_size / 2;
        //std::string tips_string = "Please add event first";
        //auto string_width = ImGui::CalcTextSize(tips_string.c_str());
        //auto tips_pos = pos_center - string_width / 2;
        //ImGui::SetWindowFontScale(1);
        //draw_list->AddTextComplex(tips_pos, tips_string.c_str(), 2.f, IM_COL32(255, 255, 255, 128), 0.5f, IM_COL32(56, 56, 56, 192));
        ImGui::TextUnformatted("No Event");
        return;
    }

    auto update_track = [&](BluePrint::BluePrintUI* pBP, BluePrint::Node* node)
    {
        auto track = timeline->FindTrackByClipID(editing_clip->mID);
        if (track) timeline->RefreshTrackView({track->mID});
        pBP->Blueprint_UpdateNode(node->m_ID);
        changed = true;
    };
    auto Reflush = [&](MediaTimeline::MediaTrack* track)
    {
        if (track) timeline->RefreshTrackView({track->mID});
        changed = true;
    };
    for (auto event : event_list)
    {

        bool is_selected = event->Status() & EVENT_SELECTED;
        bool is_scrolling = event->Status() & EVENT_SCROLLING;
        bool is_in_range = event->End() > 0 && event->Start() < editing_clip->Length();
        std::string event_label = ImGuiHelper::MillisecToString(event->Start(), 3) + " -> " + ImGuiHelper::MillisecToString(event->End(), 3) + "##clip_event##" + std::to_string(event->Id());
        std::string event_drag_drop_label = "##event_tree##" + std::to_string(event->Id());
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        if (is_selected)
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.5, 0.5, 0.0, 0.5));

        if (!is_in_range)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0, 0.0, 0.0, 1.0));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.3, 0.0, 0.0, 0.5));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.3, 0.0, 0.0, 0.5));
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0, 1.0, 1.0, 1.0));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.4, 0.4, 1.0, 0.5));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.2, 0.2, 1.0, 0.5));
        }
        auto tree_pos = ImGui::GetCursorScreenPos();
        ImGui::Circle(is_selected);
        ImGui::Dummy(ImVec2(0, 30));
        ImGui::SameLine(15);
        bool event_tree_open = ImGui::TreeNodeEx(event_label.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_AllowOverlap | (is_selected ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None));
        int iReserveWidth = is_video_clip ? 80 : 40;
        
        if (is_selected && is_scrolling)
        {
            ImGui::ScrollToItem(ImGuiScrollFlags_KeepVisibleCenterY);
            event->SetStatus(EVENT_NEED_SCROLL, 0);
        }
        
        ImGui::SetCursorScreenPos(ImVec2(sub_window_pos.x + sub_window_size.x - iReserveWidth, tree_pos.y));
        if (is_video_clip)
        {
            if (ImGui::Button(ICON_MASK "##add_event_mask"))
            {
                auto pVidEvt = dynamic_cast<MEC::VideoEvent*>(event.get());
                auto newMaskIdx = pVidEvt->GetMaskCount();
                std::ostringstream oss; oss << "Mask " << newMaskIdx;
                std::string maskName = oss.str();
                const MatUtils::Size2i vidPreviewSize(timeline->mhPreviewSettings->VideoOutWidth(), timeline->mhPreviewSettings->VideoOutHeight());
                auto hMaskCreator = pVidEvt->CreateNewMask(maskName, vidPreviewSize);
                assert(hMaskCreator);
                if (is_selected)
                    pEdtVidClip->SelectEditingMask(event, -1, newMaskIdx, hMaskCreator);
            }
            ImGui::ShowTooltipOnHover("Add new mask");
            ImGui::SameLine();
            if (pEdtVidClip->mMaskEventId == event->Id() && !is_selected)
                pEdtVidClip->UnselectEditingMask();
        }
        if (ImGui::Button(ICON_DELETE "##event_list_editor_delete_event"))
        {
            ImGui::OpenPopup("Delete Event?");
        }
        ImGui::ShowTooltipOnHover("Delete Event");
        ImGui::PopStyleColor(3);
        if (event_tree_open)
        {
            ImGui::Unindent(ImGui::GetTreeNodeToLabelSpacing());
            // draw event mask list
            if (is_video_clip)
            {
                ImGui::Indent(30);
                MEC::VideoEvent* pVidEvt = dynamic_cast<MEC::VideoEvent*>(event.get());
                const auto iMaskCount = pVidEvt->GetMaskCount();
                int iToDelIdx = -1;
                auto currPos = ImGui::GetCursorScreenPos();
                const float iconWidth = 30;
                const int iconCnt = 2;
                auto rightIconPosX = sub_window_pos.x+sub_window_size.x-iconWidth*iconCnt-60;
                const ImVec2 selectableItemSize = { rightIconPosX-currPos.x-20, 20 };
                for (int idx = 0; idx < iMaskCount; idx++)
                {
                    auto hMaskCreator = pVidEvt->GetMaskCreator(idx);
                    if (!hMaskCreator)
                    {
                        Logger::Log(Logger::WARN) << pVidEvt->GetError() << std::endl;
                        continue;
                    }
                    std::string maskName = hMaskCreator->GetName();
                    std::ostringstream oss; oss << maskName << "##event" << event->Id() << "_mask@" << idx;
                    std::string label = oss.str();
                    bool bMaskSelected = pEdtVidClip->mMaskEventId == event->Id() && pEdtVidClip->mMaskNodeId == -1 && pEdtVidClip->mMaskIndex == idx;
                    if (ImGui::Selectable(label.c_str(), bMaskSelected, 0, selectableItemSize) && !bMaskSelected)
                    {
                        if (!is_selected) editing_clip->SelectEvent(event);
                        pEdtVidClip->SelectEditingMask(event, -1, idx, hMaskCreator);
                    }
                    ImGui::SameLine();
                    currPos = ImGui::GetCursorScreenPos();
                    int iconIdx = 0;
                    ImGui::SetCursorScreenPos({rightIconPosX+(iconIdx++)*iconWidth, currPos.y});
                    oss.str(""); oss << ICON_FA_CLOCK << "##enable_keyframe@event" << event->Id() << "_index" << idx;
                    label = oss.str();
                    bool bKeyFrameEnabled = hMaskCreator->IsKeyFrameEnabled();
                    const ImColor tChkbtnColor(100, 100, 100);
                    bool bKeyFrameEnabled_ = !bKeyFrameEnabled;
                    ImGui::BeginDisabled(!bMaskSelected);
                    if (ImGui::CheckButton(label.c_str(), &bKeyFrameEnabled_, tChkbtnColor))
                    {
                        bKeyFrameEnabled = !bKeyFrameEnabled_;
                        pEdtVidClip->mhMaskCreator->EnableKeyFrames(bKeyFrameEnabled);
                    }
                    ImGui::EndDisabled();
                    ImGui::SetCursorScreenPos({rightIconPosX+(iconIdx++)*iconWidth, currPos.y});
                    oss.str(""); oss << ICON_DELETE << "##delete_mask@event" << event->Id() << "_index" << idx;
                    label = oss.str();
                    if (ImGui::Button(label.c_str()))
                        iToDelIdx = idx;
                    ImGui::ShowTooltipOnHover("Delete Mask");

                    // draw mask key-frame timeline
                    if (bMaskSelected && bKeyFrameEnabled)
                    {
                        currPos = ImGui::GetCursorScreenPos();
                        auto wdgWidth = sub_window_pos.x+sub_window_size.x-currPos.x-60;
                        const int64_t i64Tick = timeline->mCurrentTime-(pEdtVidClip->mStart+pEdtVidClip->mMaskEventStart);
                        int64_t i64Tick_ = i64Tick;
                        pEdtVidClip->mhMaskCreator->DrawContourPointKeyFrames(i64Tick_, nullptr, wdgWidth);
                        if (i64Tick != i64Tick_)
                            timeline->Seek(i64Tick_+pEdtVidClip->mStart+pEdtVidClip->mMaskEventStart);
                    }
                }
                if (iToDelIdx >= 0)
                {
                    if (pEdtVidClip->mMaskEventId == event->Id() && pEdtVidClip->mMaskNodeId == -1 && pEdtVidClip->mMaskIndex == iToDelIdx)
                        pEdtVidClip->UnselectEditingMask();
                    pVidEvt->RemoveMask(iToDelIdx);
                }
                ImGui::Unindent(30);
            }
            auto pBP = event->GetBp();
            auto pKP = event->GetKeyPoint();
            auto addCurve = [&](BluePrint::Node* node, std::string name, float _min, float _max, float _default, int64_t pin_id)
            {
                if (pKP)
                {
                    auto found = pKP->GetCurveIndex(name);
                    if (found == -1)
                    {
                        ImU32 color; ImGui::RandomColor(color, 1.f);
                        auto curve_index = pKP->AddCurveByDim(name, ImGui::ImCurveEdit::Smooth, color, true, ImGui::ImCurveEdit::DIM_X, _min, _max, _default, node->m_ID, pin_id);
                        pKP->AddPointByDim(curve_index, ImVec2(0, _default), ImGui::ImCurveEdit::Smooth, ImGui::ImCurveEdit::DIM_X, true);
                        pKP->AddPointByDim(curve_index, ImVec2(event->End()-event->Start(), _default), ImGui::ImCurveEdit::Smooth, ImGui::ImCurveEdit::DIM_X, true);
                        if (pBP)
                        {
                            auto entry_node = pBP->FindEntryPointNode();
                            if (entry_node)
                            {
                                auto pin = entry_node->InsertOutputPin(BluePrint::PinType::Float, name);
                                if (pin && pin_id != -1)
                                {
                                    auto link_pin = pBP->m_Document->m_Blueprint.FindPin(pin_id);
                                    if (link_pin) link_pin->LinkTo(*pin);
                                }
                            }
                            update_track(pBP, node);
                        }
                    }
                }
            };
            auto delCurve = [&](BluePrint::Node* node, std::string name, int64_t pin_id)
            {
                if (pKP)
                {
                    auto found = pKP->GetCurveIndex(name);
                    if (found != -1)
                    {
                        pKP->DeleteCurve(found);
                        if (pBP && pin_id != -1)
                        {
                            auto link_pin = pBP->m_Document->m_Blueprint.FindPin(pin_id);
                            if (link_pin) link_pin->Unlink();
                            auto entry_node = pBP->FindEntryPointNode();
                            if (entry_node)
                            {
                                auto pin = entry_node->FindPin(name);
                                if (pin && pin->m_LinkFrom.empty())
                                {
                                    entry_node->DeleteOutputPin(name);
                                }
                            }
                            update_track(pBP, node);
                        }
                    }
                }
            };
            auto editCurve = [&](ImGui::KeyPointEditor* keypoint, BluePrint::Node* node)
            {
                if (!keypoint || !node) return;
                if (keypoint->GetCurveCount() <= 0) return;
                ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.2,0.2,1.0,0.5));
                ImGui::Separator();
                MediaTimeline::MediaTrack * track = node ? timeline->FindTrackByClipID(node->m_ID) : nullptr;
                for (int i = 0; i < keypoint->GetCurveCount(); i++)
                {
                    bool break_loop = false;
                    if (keypoint->GetCurveID(i) != node->m_ID)
                        continue;
                    ImGui::PushID(i);
                    auto pCount = keypoint->GetCurvePointCount(i);
                    std::string lable_id = std::string(ICON_CURVE) + " " + keypoint->GetCurveName(i) + " (" + std::to_string(pCount) + " keys)" + "##event_curve";
                    if (ImGui::TreeNodeEx(lable_id.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        float curve_min = keypoint->GetCurveMinByDim(i, ImGui::ImCurveEdit::DIM_X);
                        float curve_max = keypoint->GetCurveMaxByDim(i, ImGui::ImCurveEdit::DIM_X);
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
                        const auto start_time = editing->mStart;
                        const auto curve_time = timeline->mCurrentTime - start_time - event->Start();
                        const auto curve_value = keypoint->GetValueByDim(i, curve_time, ImGui::ImCurveEdit::DIM_X);
                        bool in_range = curve_time >= keypoint->GetMin().w && 
                                        curve_time <= keypoint->GetMax().w;
                        ImGui::BracketSquare(true); 
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0, 1.0, 0.0, 1.0)); 
                        if (in_range) ImGui::Text("%.2f", curve_value); 
                        else ImGui::TextUnformatted("--.--");
                        ImGui::PopStyleColor();
                        ImGui::ShowTooltipOnHover("Current value");
                        ImGui::SameLine();
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3, 0.3, 1.0, 1.0)); 
                        if (in_range) ImGui::Text("%s", ImGuiHelper::MillisecToString(curve_time + event->Start(), 3).c_str()); 
                        else ImGui::TextUnformatted("--:--.--");
                        ImGui::PopStyleColor();
                        ImGui::ShowTooltipOnHover("Clip time");
                        ImGui::SameLine();
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0, 1.0, 0.0, 1.0)); 
                        ImGui::Text("%s", ImGuiHelper::MillisecToString(timeline->mCurrentTime, 3).c_str());
                        ImGui::PopStyleColor();
                        ImGui::ShowTooltipOnHover("Main time");
                        ImGui::SameLine();
                        ImGui::BeginDisabled(!in_range);
                        if (ImGui::Button(ICON_MD_ADS_CLICK))
                        {
                            keypoint->AddPointByDim(i, ImVec2(curve_time, curve_value), ImGui::ImCurveEdit::Smooth, ImGui::ImCurveEdit::DIM_X, true);
                            changed = true;
                        }
                        ImGui::EndDisabled();
                        ImGui::ShowTooltipOnHover("Add key at current");

                        ImGui::PushItemWidth(60);
                        if (ImGui::DragFloat("##curve_filter_min", &curve_min, 0.1f, -FLT_MAX, curve_max, "%.1f"))
                        {
                            keypoint->SetCurveMinByDim(i, curve_min, ImGui::ImCurveEdit::DIM_X);
                            Reflush(track);
                        } ImGui::ShowTooltipOnHover("Min");
                        ImGui::SameLine(0, 8);
                        if (ImGui::DragFloat("##curve_filter_max", &curve_max, 0.1f, curve_min, FLT_MAX, "%.1f"))
                        {
                            keypoint->SetCurveMaxByDim(i, curve_max, ImGui::ImCurveEdit::DIM_X);
                            Reflush(track);
                        } ImGui::ShowTooltipOnHover("Max");
                        ImGui::SameLine(0, 8);
                        float curve_default = keypoint->GetCurveDefaultByDim(i, ImGui::ImCurveEdit::DIM_X);
                        if (ImGui::DragFloat("##curve_filter_default", &curve_default, 0.1f, curve_min, curve_max, "%.1f"))
                        {
                            keypoint->SetCurveDefaultByDim(i, curve_default, ImGui::ImCurveEdit::DIM_X);
                            Reflush(track);
                        } ImGui::ShowTooltipOnHover("Default");
                        ImGui::PopItemWidth();

                        ImGui::SameLine(0, 8);
                        ImGui::SetWindowFontScale(0.75);
                        auto curve_color = ImGui::ColorConvertU32ToFloat4(keypoint->GetCurveColor(i));
                        if (ImGui::ColorEdit4("##curve_filter_color", (float*)&curve_color, ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar))
                        {
                            keypoint->SetCurveColor(i, ImGui::ColorConvertFloat4ToU32(curve_color));
                            changed = true;
                        } ImGui::ShowTooltipOnHover("Curve Color");
                        ImGui::SetWindowFontScale(1.0);
                        ImGui::SameLine(0, 4);
                        bool is_visiable = keypoint->IsVisible(i);
                        if (ImGui::Button(is_visiable ? ICON_WATCH : ICON_UNWATCH "##curve_filter_visiable"))
                        {
                            is_visiable = !is_visiable;
                            keypoint->SetCurveVisible(i, is_visiable);
                        } ImGui::ShowTooltipOnHover( is_visiable ? "Hide" : "Show");
                        ImGui::SameLine(0, 4);
                        if (ImGui::Button(ICON_MD_ROTATE_90_DEGREES_CCW "##curve_filter_reset"))
                        {
                            for (int p = 0; p < pCount; p++)
                            {
                                keypoint->SetCurvePointDefault(i, p);
                            }
                            Reflush(track);
                        } ImGui::ShowTooltipOnHover("Reset");
                        if (!break_loop)
                        {
                            // list points
                            for (int p = 0; p < pCount; p++)
                            {
                                bool is_disabled = false;
                                ImGui::PushID(p);
                                ImGui::PushItemWidth(96);
                                auto point = keypoint->GetPoint(i, p);
                                ImGui::Diamond(true, true);
                                if (p == 0 || p == pCount - 1)
                                    is_disabled = true;
                                ImGui::BeginDisabled(is_disabled);
                                float time_start = keypoint->GetMin().w + event->Start();
                                float time_end = keypoint->GetMax().w + event->Start();
                                float time_current = point.t + event->Start();
                                if (ImGui::DragTimeMS("##curve_filter_point_time", &time_current, time_end / 1000.f, time_start, time_end, 2))
                                {
                                    point.t = time_current - event->Start();
                                    keypoint->EditPoint(i, p, point.val, point.type);
                                    Reflush(track);
                                }
                                ImGui::EndDisabled();
                                ImGui::SameLine();
                                auto speed = fabs(curve_max - curve_min) / 500;
                                if (ImGui::DragFloat("##curve_filter_point_x", &point.x, speed, curve_min, curve_max, "%.2f"))
                                {
                                    keypoint->EditPoint(i, p, point.val, point.type);
                                    Reflush(track);
                                }
                                ImGui::SameLine();
                                if (ImGui::Combo("##curve_filter_type", (int*)&point.type, curve_type_list, curve_type_count))
                                {
                                    keypoint->EditPoint(i, p, point.val, point.type);
                                    Reflush(track);
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
                ImGui::Separator();
                ImGui::PopStyleColor();
            };
            if (pBP)
            {                
                auto nodes = pBP->m_Document->m_Blueprint.GetNodes();
                bool need_redraw = false;
                for (auto node : nodes)
                {
                    if (need_redraw)
                        break;
                    auto type = node->GetTypeInfo().m_Type;
                    if (type == BluePrint::NodeType::EntryPoint || type == BluePrint::NodeType::ExitPoint)
                        continue;
                    auto label_name = node->m_Name;
                    std::string lable_id = label_name + "##filter_node" + "@" + std::to_string(node->m_ID);
                    auto node_pos = ImGui::GetCursorScreenPos();
                    node->DrawNodeLogo(ImGui::GetCurrentContext(), ImVec2(60, 30));
                    ImGui::SameLine(30);
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0, 1.0, 1.0, 1.0));
                    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.2, 0.5, 0.2, 0.5));
                    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.1, 0.5, 0.1, 0.5));
                    bool tree_open = ImGui::TreeNodeEx(lable_id.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_AllowOverlap);
                    ImGui::PopStyleColor(3);
                    if (ImGui::BeginDragDropSource())
                    {
                        ImGui::SetDragDropPayload(event_drag_drop_label.c_str(), node, sizeof(BluePrint::Node));
                        ImGui::TextUnformatted(node->m_Name.c_str());
                        ImGui::EndDragDropSource();
                    }
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(event_drag_drop_label.c_str()))
                        {
                            BluePrint::Node* src_node = (BluePrint::Node*)payload->Data;
                            if (src_node)
                            {
                                pBP->Blueprint_SwapNode(src_node->m_ID, node->m_ID);
                                need_redraw = true;
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }
                    ImGui::SetCursorScreenPos(ImVec2(sub_window_pos.x + sub_window_size.x - 80, node_pos.y));
                    if (ImGui::Button(node->m_Enabled ? ICON_VIEW : ICON_VIEW_DISABLE "##event_list_editor_disable_node"))
                    {
                        node->m_Enabled = !node->m_Enabled;
                        update_track(pBP, node);
                    }
                    ImGui::ShowTooltipOnHover(node->m_Enabled ? "Disable Node" : "Enable Node");
                    ImGui::SameLine();
                    if (ImGui::Button(ICON_DELETE "##event_list_editor_delete_node"))
                    {
                        ImGui::OpenPopup("Delete Node?");
                    }
                    ImGui::ShowTooltipOnHover("Delete Node");
                    if (tree_open && !need_redraw)
                    {
                        ImGui::ImCurveEdit::Curve key;
                        key.m_id = node->m_ID;
                        ImGui::Indent(20);
                        if (node->DrawCustomLayout(ImGui::GetCurrentContext(), 1.0, ImVec2(0, 0), &key, false))
                        {
                            update_track(pBP, node);
                        }
                        ImGui::Indent(-20);
                        if (!key.name.empty())
                        {
                            if (key.checked)
                                addCurve(node, key.name,
                                        ImGui::ImCurveEdit::GetDimVal(key.m_min, ImGui::ImCurveEdit::DIM_X),
                                        ImGui::ImCurveEdit::GetDimVal(key.m_max, ImGui::ImCurveEdit::DIM_X),
                                        ImGui::ImCurveEdit::GetDimVal(key.m_default, ImGui::ImCurveEdit::DIM_X),
                                        key.m_sub_id);
                            else
                                delCurve(node, key.name, key.m_sub_id);
                        }
                        // list curve
                        editCurve(pKP, node);
                    }

                    // Handle node delete
                    if (msgbox_node.Draw() == 1)
                    {
                        if (pKP)
                        {
                            int found = -1;
                            while ((found = pKP->GetCurveIndex(node->m_ID)) != -1)
                            {
                                auto curve = pKP->GetCurve(found);
                                if (curve) delCurve(node, curve->name, curve->m_sub_id);
                            }
                        }
                        auto track = timeline->FindTrackByClipID(node->m_ID);
                        pBP->Blueprint_DeleteNode(node->m_ID);
                        Reflush(track);
                        need_redraw = true;
                    }
                    if (tree_open) ImGui::TreePop();
                }
            }
            // Handle event delete
            if (msgbox_event.Draw() == 1)
            {
                editing_clip->DeleteEvent(event, &timeline->mUiActions);
                auto track = timeline->FindTrackByClipID(editing_clip->mID);
                Reflush(track);
            }
            ImGui::Indent(ImGui::GetTreeNodeToLabelSpacing());
            ImGui::TreePop();
        }
        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(1.0,1.0,1.0,0.75));
        ImGui::Separator();
        ImGui::PopStyleColor();
        if (is_selected)
            ImGui::PopStyleColor();
        ImGui::PopStyleColor();
    }
    if (!g_project_loading) project_changed |= changed;
}

/****************************************************************************************
 * 
 * Video Clip window
 *
 ***************************************************************************************/
static void DrawVideoClipPreviewWindow(ImDrawList *draw_list, EditingVideoClip * editing_clip)
{
    ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
    ImVec2 sub_window_size = ImGui::GetWindowSize();
    bool is_vertical = sub_window_size.y > sub_window_size.x;
    ShowVideoPreviewWindow(draw_list, editing_clip, is_vertical);
}

static bool DrawVideoClipTimelineWindow(bool& show_BP, EditingVideoClip * editing_clip)
{
    ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
    ImVec2 sub_window_size = ImGui::GetWindowSize();
    bool timeline_changed = false;
    auto mouse_hold = DrawClipTimeLine(timeline, editing_clip, timeline->mCurrentTime, 30, 50, show_BP, timeline_changed);
    if (!g_project_loading) project_changed |= timeline_changed;
    return mouse_hold;
}

static void ShowVideoClipWindow(ImDrawList *draw_list, ImRect title_rect, EditingVideoClip* editing)
{
    /*                1. with preview(2 Splitters)                                                   2. without preview(1 Splitter)
    ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓      ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━┓ 
    ┃                                 ┃                                  ┃      ┃           |<  <  []  >  >|                 ║                       ┃
    ┃                                 ┃                                  ┃      ┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━╢                       ┃
    ┃       preview before            ┃          preview after           ┃      ┃              timeline                      ║   > event             ┃  
    ┃                                 ┃                                  ┃      ┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━╢    |> filter          ┃
    ┃                                 ┃                                  ┃      ┃              event track                   ║      > filter edit    ┃ 
    ┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫      ┃               [event]                      ║        .              ┃ 
    ┃                          |<  <  []  >  >|                          ┃      ┃               [curve]                      ║        .              ┃ 
    ┣════════════════════════════════════════════╦═══════════════════════┫      ┃                  .                         ║      > effect edit    ┃ 
    ┃             timeline                       ║   > event             ┃      ┃                  .                         ║        .              ┃ 
    ┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━╢    |> filter          ┃      ┃                  .                         ║        .              ┃ 
    ┃              event track                   ║      > filter edit    ┃      ┃              event track                   ║      .                ┃ 
    ┃              [ event   ]                   ║        .              ┃      ┃                  .                         ║      .                ┃
    ┃              [ curve   ]                   ║        .              ┃      ┃                                            ║                       ┃
    ┃                  .                         ║      .                ┃      ┃                                            ║                       ┃
    ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━┛      ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━┛
                    3. with blueprint edit(3 Splitters)                                                 4. with single preview(2 Splitters)
    ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━┓      ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓ 
    ┃    []--[]--                                ║                       ┃      ┃                                 ║  > event                         ┃ 
    ┃             \                              ║       preview         ┃      ┃                                 ║   |> filter                      ┃
    ┃               \- []                        ║                       ┃      ┃            preview              ║     > filter edit                ┃
    ┃                                            ║                       ┃      ┃                                 ║        .                         ┃
    ┃                                            ╟━━━━━━━━━━━━━━━━━━━━━━━┫      ┃                                 ║        .                         ┃
    ┃                                            ║                       ┃      ┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━╢        .                         ┃
    ┃                                            ║       preview         ┃      ┃        |<  <  []  >  >|         ║     > effect edit                ┃
    ┃                                            ║                       ┃      ┣═════════════════════════════════╢        .                         ┃
    ┃                                            ╟━━━━━━━━━━━━━━━━━━━━━━━┫      ┃             timeline            ║        .                         ┃
    ┃                                            ║    |<  <  []  >  >|   ┃      ┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━╢      .                           ┃
    ┣════════════════════════════════════════════╬═══════════════════════┫      ┃            event track          ║      .                           ┃
    ┃             timeline                       ║   > event             ┃      ┃           [ event   ]           ║                                  ┃
    ┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━╢    |> filter          ┃      ┃           [ curve   ]           ║                                  ┃
    ┃              [ event ]                     ║      > filter edit    ┃      ┃               .                 ║                                  ┃
    ┃              [ curve ]                     ║        .              ┃      ┃               .                 ║                                  ┃
    ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━┛      ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛
                    5. with blueprint edit no preview (2 Splitters)                           
    ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓ 
    ┃    []--[]--                                                        ┃ 
    ┃             \                                                      ┃ 
    ┃               \- []                                                ┃ 
    ┃                                                                    ┃ 
    ┃                                                                    ┃ 
    ┃                                                                    ┃ 
    ┃                                                                    ┃ 
    ┃                                                                    ┃ 
    ┣════════════════════════════════════════════╦═══════════════════════┫ 
    ┃           |<  <  []  >  >|                 ║  > event              ┃ 
    ┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━╢   |> filter           ┃ 
    ┃             timeline                       ║    > filter edit      ┃ 
    ┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━╢       .               ┃ 
    ┃              [ event ]                     ║       .               ┃ 
    ┃              [ curve ]                     ║                       ┃ 
    ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━┛ 
    */

    // draw page title
    ImGui::SetWindowFontScale(1.8);
    auto title_size = ImGui::CalcTextSize("Video Clip");
    float str_offset = title_rect.Max.x - title_size.x - 16;
    ImGui::SetWindowFontScale(1.0);
    draw_list->AddTextComplex(ImVec2(str_offset, title_rect.Min.y), "Video Clip", 1.8f, COL_TITLE_COLOR, 0.5f, COL_TITLE_OUTLINE);

    ImVec2 window_pos = ImGui::GetCursorScreenPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    draw_list->AddRectFilled(window_pos, window_pos + window_size, COL_DEEP_DARK);
    if (!timeline)
        return;
    
    BluePrint::BluePrintUI* blueprint = nullptr;
    ImGui::KeyPointEditor* keypoint = nullptr;

    int64_t trackId = -1;
    Clip * editing_clip = editing->GetClip();
    blueprint = editing->mFilterBp;
    keypoint = editing->mFilterKp;
    auto track = timeline->FindTrackByClipID(editing_clip->mID);
    if (track) trackId = track->mID;
    editing->UpdateClipRange(editing_clip);

    // every type for area rect
    // preview_rect: could be (0, 0, 0, 0)
    // preview_control_rect: if preview_rect is exist, width shoule equ preview_rect.width, otherwise, alway on top and same width with window
    // timeline_rect: always bottom left of window, include event_track, timeline_height = 30(head height) + 50(clip height) + 12
    // event_list_rect: always bottom right of window
    // blueprint_rect: only has when we show event's bp, and aways on left top of window, same width with timeline
    ImRect preview_rect, preview_control_rect, timeline_rect, event_list_rect, blueprint_rect;

    const float event_min_width = 440;
    bool is_splitter_hold = false;
    static bool show_blueprint = false;
    int preview_count = 0;
    if (MonitorIndexVideoFilterOrg == -1) preview_count ++;
    if (MonitorIndexVideoFiltered == -1)  preview_count ++;

    if (!show_blueprint)
    {
        if (preview_count == 0)
        {
            // 1 Splitter vertically only(chart 2)
            float timeline_width = window_size.x * g_media_editor_settings.video_clip_timeline_width;
            float event_list_width = window_size.x - timeline_width;
            is_splitter_hold |= ImGui::Splitter(true, 4.0f, &timeline_width, &event_list_width, window_size.x * 0.5, event_min_width/*window_size.x * 0.2*/);
            g_media_editor_settings.video_clip_timeline_width = timeline_width / window_size.x;
            if (ImGui::BeginChild("video_timeline&preview_tool_bar", ImVec2(timeline_width - 4, window_size.y), false))
            {
                // show preview window with tool bar only
                if (ImGui::BeginChild("video_preview_tool_bar", ImVec2(timeline_width - 4, 48), false))
                {
                    DrawVideoClipPreviewWindow(draw_list, editing);
                }
                ImGui::EndChild();
                if (ImGui::BeginChild("video_timeline", ImVec2(timeline_width - 4, window_size.y - 48), false))
                {
                    mouse_hold |= DrawVideoClipTimelineWindow(show_blueprint, editing);
                }
                ImGui::EndChild();
            }
            ImGui::EndChild();
            ImGui::SameLine();
            if (ImGui::BeginChild("video_event_list", ImVec2(event_list_width - 4, window_size.y), false))
            {
                DrawClipEventWindow(draw_list, editing);
            }
            ImGui::EndChild();
        }
        else if (preview_count < 2 || g_media_editor_settings.video_clip_timeline_height > 0.5)
        {
            // 2 Splitters, vertically first, then horizontal(chart 4)
            float timeline_width = window_size.x * g_media_editor_settings.video_clip_timeline_width;
            float event_list_width = window_size.x - timeline_width;
            is_splitter_hold |= ImGui::Splitter(true, 4.0f, &timeline_width, &event_list_width, window_size.x * 0.5, event_min_width/*window_size.x * 0.2*/);
            g_media_editor_settings.video_clip_timeline_width = timeline_width / window_size.x;
            if (ImGui::BeginChild("video_timeline&preview", ImVec2(timeline_width - 4, window_size.y), false))
            {
                float timeline_height = window_size.y * g_media_editor_settings.video_clip_timeline_height;
                float preview_height = window_size.y - timeline_height;
                is_splitter_hold |= ImGui::Splitter(false, 4.0f, &preview_height, &timeline_height, window_size.y * 0.3, window_size.y * 0.3);
                g_media_editor_settings.video_clip_timeline_height = timeline_height / window_size.y;
                if (ImGui::BeginChild("video_preview", ImVec2(timeline_width - 4, preview_height - 4), false))
                {
                    DrawVideoClipPreviewWindow(draw_list, editing);
                }
                ImGui::EndChild();
                ImGui::Dummy(ImVec2(0, 4));
                if (ImGui::BeginChild("video_timeline", ImVec2(timeline_width - 4, timeline_height - 8), false))
                {
                    mouse_hold |= DrawVideoClipTimelineWindow(show_blueprint, editing);
                }
                ImGui::EndChild();
            }
            ImGui::EndChild();
            ImGui::SameLine();
            if (ImGui::BeginChild("video_event_list", ImVec2(event_list_width - 8, window_size.y), false))
            {
                DrawClipEventWindow(draw_list, editing);
            }
            ImGui::EndChild();
        }
        else
        {
            // 2 Splitters, horizontal first, then vertically(chart 1)
            float timeline_height = window_size.y * g_media_editor_settings.video_clip_timeline_height;
            float preview_height = window_size.y - timeline_height;
            is_splitter_hold |= ImGui::Splitter(false, 4.0f, &preview_height, &timeline_height, window_size.y * 0.3, window_size.y * 0.3);
            g_media_editor_settings.video_clip_timeline_height = timeline_height / window_size.y;
            if (ImGui::BeginChild("video_preview", ImVec2(window_size.x, preview_height - 4), false))
            {
                DrawVideoClipPreviewWindow(draw_list, editing);
            }
            ImGui::EndChild();
            float timeline_width = window_size.x * g_media_editor_settings.video_clip_timeline_width;
            float event_list_width = window_size.x - timeline_width;
            is_splitter_hold |= ImGui::Splitter(true, 4.0f, &timeline_width, &event_list_width, window_size.x * 0.5, event_min_width/*window_size.x * 0.2*/);
            g_media_editor_settings.video_clip_timeline_width = timeline_width / window_size.x;
            ImGui::Dummy(ImVec2(0, 4));
            if (ImGui::BeginChild("video_timeline", ImVec2(timeline_width - 4, timeline_height - 8), false))
            {
                mouse_hold |= DrawVideoClipTimelineWindow(show_blueprint, editing);
            }
            ImGui::EndChild();
            ImGui::SameLine();
            if (ImGui::BeginChild("video_event_list", ImVec2(event_list_width - 4, timeline_height - 8), false))
            {
                DrawClipEventWindow(draw_list, editing);
            }
            ImGui::EndChild();
        }
    }
    else
    {
        if (preview_count == 0)
        {
            // 2 Splitters, horizontal first, then vertically(chart 5)
            float timeline_height = window_size.y * g_media_editor_settings.video_clip_timeline_height;
            float preview_height = window_size.y - timeline_height;
            is_splitter_hold |= ImGui::Splitter(false, 4.0f, &preview_height, &timeline_height, window_size.y * 0.5, window_size.y * 0.3);
            g_media_editor_settings.video_clip_timeline_height = timeline_height / window_size.y;
            if (ImGui::BeginChild("video_blue_print", ImVec2(window_size.x, preview_height - 4), false))
            {
                DrawClipBlueprintWindow(draw_list, editing);
            }
            ImGui::EndChild();
            ImGui::Dummy(ImVec2(0, 4));
            if (ImGui::BeginChild("video_timeline&event_list", ImVec2(window_size.x, timeline_height - 8), false))
            {
                float timeline_width = window_size.x * g_media_editor_settings.video_clip_timeline_width;
                float event_list_width = window_size.x - timeline_width;
                is_splitter_hold |= ImGui::Splitter(true, 4.0f, &timeline_width, &event_list_width, window_size.x * 0.5, event_min_width/*window_size.x * 0.2*/);
                g_media_editor_settings.video_clip_timeline_width = timeline_width / window_size.x;
                if (ImGui::BeginChild("timeline", ImVec2(timeline_width - 4, timeline_height - 8), false))
                {
                    mouse_hold |= DrawVideoClipTimelineWindow(show_blueprint, editing);
                }
                ImGui::EndChild();
                ImGui::SameLine();
                if (ImGui::BeginChild("event list", ImVec2(event_list_width - 4, timeline_height - 8), false))
                {
                    DrawClipEventWindow(draw_list, editing);
                }
                ImGui::EndChild();
                }
            ImGui::EndChild();
        }
        else
        {
            // 3 Splitters, horizontal first, then 2 vertically(chart 3)
            float timeline_height = window_size.y * g_media_editor_settings.video_clip_timeline_height;
            float preview_height = window_size.y - timeline_height;
            is_splitter_hold |= ImGui::Splitter(false, 4.0f, &preview_height, &timeline_height, window_size.y * 0.5, window_size.y * 0.3);
            g_media_editor_settings.video_clip_timeline_height = timeline_height / window_size.y;
            if (ImGui::BeginChild("blue print&preview", ImVec2(window_size.x, preview_height - 4), false))
            {
                float blue_width = window_size.x * g_media_editor_settings.video_clip_timeline_width;
                float preview_width = window_size.x - blue_width;
                is_splitter_hold |= ImGui::Splitter(true, 4.0f, &blue_width, &preview_width, window_size.x * 0.5, event_min_width /*window_size.x * 0.2*/);
                g_media_editor_settings.video_clip_timeline_width = blue_width / window_size.x;
                if (ImGui::BeginChild("blue print", ImVec2(blue_width - 4, preview_height - 4), false))
                {
                    DrawClipBlueprintWindow(draw_list, editing);
                }
                ImGui::EndChild();
                ImGui::SameLine();
                if (ImGui::BeginChild("preview", ImVec2(preview_width - 4, preview_height - 4), false))
                {
                    DrawVideoClipPreviewWindow(draw_list, editing);
                }
                ImGui::EndChild();
            }
            ImGui::EndChild();
            ImGui::Dummy(ImVec2(0, 4));
            if (ImGui::BeginChild("timeline&event_list", ImVec2(window_size.x, timeline_height - 8), false))
            {
                float timeline_width = window_size.x * g_media_editor_settings.video_clip_timeline_width;
                float event_list_width = window_size.x - timeline_width;
                is_splitter_hold |= ImGui::Splitter(true, 4.0f, &timeline_width, &event_list_width, window_size.x * 0.5, event_min_width/*window_size.x * 0.2*/);
                g_media_editor_settings.video_clip_timeline_width = timeline_width / window_size.x;
                if (ImGui::BeginChild("timeline", ImVec2(timeline_width - 4, timeline_height - 8), false))
                {
                    mouse_hold |= DrawVideoClipTimelineWindow(show_blueprint, editing);
                }
                ImGui::EndChild();
                ImGui::SameLine();
                if (ImGui::BeginChild("event list", ImVec2(event_list_width - 4, timeline_height - 8), false))
                {
                    DrawClipEventWindow(draw_list, editing);
                }
                ImGui::EndChild();
            }
            ImGui::EndChild();
        }
    }
    if (!timeline->mNeedUpdateTrackIds.empty())
    {
        timeline->RefreshTrackView(timeline->mNeedUpdateTrackIds);
        timeline->mNeedUpdateTrackIds.clear();
    }
}

/****************************************************************************************
 * 
 * Video Transition window
 *
 ***************************************************************************************/
static void ShowVideoTransitionBluePrintWindow(ImDrawList *draw_list, EditingVideoOverlap * editing)
{
    Overlap * overlap = timeline && editing ? timeline->FindOverlapByID(editing->mID) : nullptr;
    if (timeline && editing && editing->mTransition && editing->mTransition->mBp)
    {
        if (overlap && !editing->mTransition->mBp->m_Document->m_Blueprint.IsOpened())
        {
            auto track = timeline->FindTrackByClipID(overlap->m_Clip.first);
            if (track)
                track->SelectEditingOverlap(overlap);
            editing->mTransition->mBp->View_ZoomToContent();
        }
        ImVec2 window_pos = ImGui::GetCursorScreenPos();
        ImVec2 window_size = ImGui::GetWindowSize();
        ImGui::SetCursorScreenPos(window_pos + ImVec2(3, 3));
        ImGui::InvisibleButton("video_transition_blueprint_back_view", window_size - ImVec2(6, 6));
        if (ImGui::BeginDragDropTarget() && editing->mTransition->mBp->Blueprint_IsValid())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Transition_drag_drop_Video"))
            {
                const BluePrint::Node * node = (const BluePrint::Node *)payload->Data;
                if (node)
                {
                    editing->mTransition->mBp->Edit_Insert(node->GetTypeID());
                }
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::SetCursorScreenPos(window_pos + ImVec2(1, 1));
        if (ImGui::BeginChild("##transition_edit_blueprint", window_size - ImVec2(2, 2), false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings))
        {
            editing->mTransition->mBp->Frame(true, true, overlap != nullptr, BluePrint::BluePrintFlag::BluePrintFlag_Transition);
        }
        ImGui::EndChild();
        if (timeline->mIsBluePrintChanged) { project_changed = true; project_need_save = true; }
    }
}

static void ShowVideoTransitionPreviewWindow(ImDrawList *draw_list, EditingVideoOverlap * editing)
{
    // Draw Video Transition Play control bar
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 window_pos = ImGui::GetCursorScreenPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    draw_list->AddRectFilled(window_pos, window_pos + window_size, COL_DEEP_DARK);
    ImVec2 PanelBarPos = window_pos + ImVec2(0, (window_size.y - 36));
    ImVec2 PanelBarSize = ImVec2(window_size.x, 36);
    draw_list->AddRectFilled(PanelBarPos, PanelBarPos + PanelBarSize, COL_DARK_PANEL);
    bool out_of_border = false;
    // Preview buttons Stop button is center of Panel bar
    auto PanelCenterX = PanelBarPos.x + window_size.x / 2;
    auto PanelButtonY = PanelBarPos.y + 2;
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0.5));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2, 0.2, 0.2, 1.0));
    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX - 16 - (32 + 8) * 3, PanelButtonY));
    if (ImGui::Button(ICON_TO_START "##video_transition_tostart", ImVec2(32, 32)))
    {
        if (timeline && editing)
            editing->Seek(editing->mStart, false);
    } ImGui::ShowTooltipOnHover("To Start");
    
    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX - 16 - (32 + 8) * 2, PanelButtonY));
    if (ImGui::Button(ICON_STEP_BACKWARD "##video_transition_step_backward", ImVec2(32, 32)))
    {
        if (timeline && editing)
        {
            if (timeline->mCurrentTime > editing->mStart)
                timeline->Step(false);
        }
    } ImGui::ShowTooltipOnHover("Step Prev");
    
    bool isForwordPlaying = timeline ? (timeline->mIsPreviewPlaying && timeline->mIsPreviewForward) : false;
    bool isBackwardPlaying = (timeline && !isForwordPlaying) ? (timeline->mIsPreviewPlaying && !timeline->mIsPreviewForward) : false;
    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX - 16 - (32 + 8) * 1 + 6, PanelButtonY + 5));
    if (ImGui::RotateCheckButton(ICON_PLAY_BACKWARD "##video_transition_reverse", &isBackwardPlaying, ImVec4(0.5, 0.5, 0.0, 1.0), 180))
    {
        if (timeline && editing)
        {
            if (timeline->mCurrentTime > editing->mStart)
                timeline->Play(true, false);
        }
    } ImGui::ShowTooltipOnHover("Reverse");
    
    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX - 16, PanelButtonY));
    if (ImGui::Button(ICON_STOP "##video_transition_stop", ImVec2(32, 32)))
    {
        if (timeline)
            timeline->Play(false, true);
        isForwordPlaying = false;
    } ImGui::ShowTooltipOnHover("Stop");
    
    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX + 16 + 8 + 6, PanelButtonY + 5));
    if (ImGui::CheckButton(ICON_PLAY_FORWARD "##video_transition_play", &isForwordPlaying, ImVec4(0.5, 0.5, 0.0, 1.0)))
    {
        if (timeline && editing)
        {
            if (timeline->mCurrentTime < editing->mEnd)
                timeline->Play(true, true);
        }
    } ImGui::ShowTooltipOnHover("Play");
    
    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX + 16 + 8 + (32 + 8) * 1, PanelButtonY));
    if (ImGui::Button(ICON_STEP_FORWARD "##video_transition_step_forward", ImVec2(32, 32)))
    {
        if (timeline && editing)
        {
            if (timeline->mCurrentTime < editing->mEnd)
                timeline->Step(true);
        }
    } ImGui::ShowTooltipOnHover("Step Next");
    
    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX + 16 + 8 + (32 + 8) * 2, PanelButtonY));
    if (ImGui::Button(ICON_TO_END "##video_transition_toend", ImVec2(32, 32)))
    {
        if (timeline && editing)
            editing->Seek(editing->mEnd - 40, false);
    } ImGui::ShowTooltipOnHover("To End");

    ImGui::SetCursorScreenPos(ImVec2(PanelCenterX + 16 + 8 + (32 + 8) * 4, PanelButtonY + 6));
    if (ImGui::CheckButton(timeline->bTransitionOutputPreview ? ICON_MEDIA_PREVIEW : ICON_TRANS "##video_transition_output_preview", &timeline->bTransitionOutputPreview, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive)))
    {
        timeline->UpdatePreview();
    }
    ImGui::ShowTooltipOnHover(timeline->bTransitionOutputPreview ? "Transition Output" : "Preview Output");

    // Time stamp on right of control panel
    ImRect TimeStampRect = ImRect(PanelBarPos.x + 64, PanelBarPos.y + 6, 
                                PanelBarPos.x + window_size.x, PanelBarPos.y + PanelBarSize.y);
    draw_list->PushClipRect(TimeStampRect.Min, TimeStampRect.Max, true);
    ImGui::SetWindowFontScale(1.5);
    ImGui::ShowDigitalTime(draw_list, timeline->mCurrentTime, 3, TimeStampRect.Min, timeline->mIsPreviewPlaying ? IM_COL32(255, 255, 0, 255) : COL_MARK);
    ImGui::SetWindowFontScale(1.0);
    draw_list->PopClipRect();

    // transition texture area
    ImVec2 InputFirstVideoPos = window_pos + ImVec2(4, 4);
    ImVec2 InputFirstVideoSize = ImVec2(window_size.x / 4 - 8, window_size.y - PanelBarSize.y - 8);
    ImVec2 OutputVideoPos = window_pos + ImVec2(window_size.x / 4 + 8, 4);
    ImVec2 OutputVideoSize = ImVec2(window_size.x / 2 - 32, window_size.y - PanelBarSize.y - 8);
    ImVec2 InputSecondVideoPos = window_pos + ImVec2(window_size.x * 3 / 4 - 8, 4);
    ImVec2 InputSecondVideoSize = InputFirstVideoSize;
    
    ImRect InputFirstVideoRect(InputFirstVideoPos, InputFirstVideoPos + InputFirstVideoSize);
    ImRect InputSecondVideoRect(InputSecondVideoPos, InputSecondVideoPos + InputSecondVideoSize);
    ImRect OutVideoRect(OutputVideoPos, OutputVideoPos + OutputVideoSize);
    if (editing)
    {
        std::pair<std::pair<ImGui::ImMat, ImGui::ImMat>, ImGui::ImMat> pair;
        auto ret = editing->GetFrame(pair, timeline->bTransitionOutputPreview);
        if (ret && 
            (timeline->mIsPreviewNeedUpdate || timeline->mLastFrameTime == -1 || timeline->mLastFrameTime != (int64_t)(pair.first.first.time_stamp * 1000) || need_update_scope))
        {
            CalculateVideoScope(pair.second);
            ImGui::ImMatToTexture(pair.first.first, timeline->mVideoTransitionInputFirstTexture);
            ImGui::ImMatToTexture(pair.first.second, timeline->mVideoTransitionInputSecondTexture);
            ImGui::ImMatToTexture(pair.second, timeline->mVideoTransitionOutputTexture);
            timeline->mLastFrameTime = pair.first.first.time_stamp * 1000;
            timeline->mIsPreviewNeedUpdate = false;
        }
        ImVec4 tint_col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);   // No tint
        ImVec4 border_col = ImVec4(1.0f, 1.0f, 1.0f, 0.5f); // 50% opaque white
        float offset_x = 0, offset_y = 0;
        float tf_x = 0, tf_y = 0;
        if (timeline->mIsPreviewPlaying)
        {
            // reach clip border
            if (timeline->mIsPreviewForward && timeline->mCurrentTime >= editing->mEnd)
            {
                timeline->Play(false, true);
                timeline->Seek(editing->mEnd);
                //out_of_border = true;
            }
            else if (!timeline->mIsPreviewForward && timeline->mCurrentTime <= editing->mStart)
            {
                timeline->Play(false, false);
                timeline->Seek(editing->mStart);
                //out_of_border = true;
            }
        }
        else if (timeline->mCurrentTime < editing->mStart || timeline->mCurrentTime > editing->mEnd)
        {
            out_of_border = true;
        }
        // transition first input texture area
        ShowVideoWindow(draw_list, timeline->mVideoTransitionInputFirstTexture, InputFirstVideoPos, InputFirstVideoSize, "1", 1.2f, offset_x, offset_y, tf_x, tf_y, true, out_of_border);
        draw_list->AddRect(ImVec2(offset_x, offset_y), ImVec2(tf_x, tf_y), IM_COL32(128,128,128,128), 0, 0, 1.0);
        // transition second input texture area
        ShowVideoWindow(draw_list, timeline->mVideoTransitionInputSecondTexture, InputSecondVideoPos, InputSecondVideoSize, "2", 1.2f, offset_x, offset_y, tf_x, tf_y, true, out_of_border);
        draw_list->AddRect(ImVec2(offset_x, offset_y), ImVec2(tf_x, tf_y), IM_COL32(128,128,128,128), 0, 0, 1.0);
        // filter output texture area
        ShowVideoWindow(draw_list, timeline->mVideoTransitionOutputTexture, OutputVideoPos, OutputVideoSize, timeline->bTransitionOutputPreview ? "Transition Output" : "Preview Output", 1.5f, offset_x, offset_y, tf_x, tf_y, true, out_of_border);
        draw_list->AddRect(ImVec2(offset_x, offset_y), ImVec2(tf_x, tf_y), IM_COL32(192, 192, 192, 128), 0, 0, 2.0);
        
    }
    ImGui::PopStyleColor(3);
}

static void ShowVideoTransitionWindow(ImDrawList *draw_list, ImRect title_rect, EditingVideoOverlap* editing)
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
    ┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫    transition edit    ┃ 
    ┃             timeline                       ┃                       ┃
    ┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫                       ┃
    ┃              curves                        ┃                       ┃
    ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━┛
    */

    // draw page title
    ImGui::SetWindowFontScale(1.8);
    auto title_size = ImGui::CalcTextSize("Video Transition");
    float str_offset = title_rect.Max.x - title_size.x - 16;
    ImGui::SetWindowFontScale(1.0);
    draw_list->AddTextComplex(ImVec2(str_offset, title_rect.Min.y), "Video Transition", 1.8f, COL_TITLE_COLOR, 0.5f, COL_TITLE_OUTLINE);

    ImVec2 window_pos = ImGui::GetCursorScreenPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    draw_list->AddRectFilled(window_pos, window_pos + window_size, COL_DEEP_DARK);
    if (!timeline)
        return;

    BluePrint::BluePrintUI* blueprint = nullptr;
    BluePrintVideoTransition * transition = nullptr;
    Overlap * editing_overlap = timeline->FindOverlapByID(editing->mID);

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

    if (editing_overlap)
    {
        if (editing)
        {
            transition = editing->mTransition;
            if (editing->mStart != editing_overlap->mStart || editing->mEnd != editing_overlap->mEnd)
            {
                editing->mStart = editing_overlap->mStart;
                editing->mEnd = editing_overlap->mEnd;
                editing->mDuration = editing->mEnd - editing->mStart;
                if (transition) transition->mKeyPoints.SetMax(ImVec4(1.f, 1.f, 1.f, editing_overlap->mEnd-editing_overlap->mStart), true);
            }
        }
        blueprint = transition ? transition->mBp : nullptr;
    }

    float clip_header_height = 30;
    float clip_channel_height = 50;
    float clip_timeline_height = clip_header_height + clip_channel_height * 2;
    float clip_keypoint_height = g_media_editor_settings.VideoTransitionCurveExpanded ? 80 : 0;
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
    ImVec2 clip_keypoint_pos = g_media_editor_settings.VideoTransitionCurveExpanded ? clip_timeline_pos + ImVec2(0, clip_timeline_height) : clip_timeline_pos + ImVec2(0, clip_timeline_height - 16);
    ImVec2 clip_keypoint_size(window_size.x - clip_setting_width, clip_keypoint_height);

    ImGuiWindowFlags child_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings;
    ImGuiWindowFlags setting_child_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;

    // draw video preview
    ImGui::SetCursorScreenPos(video_preview_pos);
    if (ImGui::BeginChild("##video_transition_preview", video_preview_size, false, child_flags))
    {
        ImRect video_rect;
        ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
        ImVec2 sub_window_size = ImGui::GetWindowSize();
        draw_list->AddRectFilled(sub_window_pos, sub_window_pos + sub_window_size, COL_DEEP_DARK);
        ShowVideoTransitionPreviewWindow(draw_list, editing);
    }
    ImGui::EndChild();

    // draw overlap blueprint
    ImGui::SetCursorScreenPos(video_bluepoint_pos);
    if (ImGui::BeginChild("##video_transition_blueprint", video_bluepoint_size, false, child_flags))
    {
        ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
        ImVec2 sub_window_size = ImGui::GetWindowSize();
        draw_list->AddRectFilled(sub_window_pos, sub_window_pos + sub_window_size, COL_DEEP_DARK);
        ShowVideoTransitionBluePrintWindow(draw_list, editing);
    }
    ImGui::EndChild();

    // draw overlap timeline
    ImGui::SetCursorScreenPos(clip_timeline_pos);
    if (ImGui::BeginChild("##video_transition_timeline", clip_timeline_size, false, child_flags))
    {
        ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
        ImVec2 sub_window_size = ImGui::GetWindowSize();
        draw_list->AddRectFilled(sub_window_pos, sub_window_pos + sub_window_size, COL_DARK_TWO);
        // Draw Clip TimeLine
        DrawOverlapTimeLine(editing, timeline->mCurrentTime - (editing ? editing->mStart : 0), clip_header_height, clip_channel_height);
    }
    ImGui::EndChild();

    // draw keypoint hidden button
    ImVec2 hidden_button_pos = clip_timeline_pos - ImVec2(0, 16);
    ImRect hidden_button_rect = ImRect(hidden_button_pos, hidden_button_pos + ImVec2(16, 16));
    ImGui::SetWindowFontScale(0.75);
    if (hidden_button_rect.Contains(ImGui::GetMousePos()))
    {
        draw_list->AddRectFilled(hidden_button_rect.Min, hidden_button_rect.Max, IM_COL32(64,64,64,255));
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            g_media_editor_settings.VideoTransitionCurveExpanded = !g_media_editor_settings.VideoTransitionCurveExpanded;
        }
        if (ImGui::BeginTooltip())
        {
            ImGui::TextUnformatted(g_media_editor_settings.VideoTransitionCurveExpanded ? "Hide Curve View" : "Show Curve View");
            ImGui::EndTooltip();
        }
    }
    draw_list->AddText(hidden_button_pos, IM_COL32_WHITE, ICON_FA_BEZIER_CURVE);
    ImGui::SetWindowFontScale(1.0);

    // draw transition curve editor
    if (g_media_editor_settings.VideoTransitionCurveExpanded)
    {
        ImGui::SetCursorScreenPos(clip_keypoint_pos);
        if (ImGui::BeginChild("##video_transition_keypoint", clip_keypoint_size, false, child_flags))
        {
            ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
            ImVec2 sub_window_size = ImGui::GetWindowSize();
            draw_list->AddRectFilled(sub_window_pos, sub_window_pos + sub_window_size, COL_DARK_ONE);
            if (editing && transition)
            {
                bool _changed = false;
                float current_time = timeline->mCurrentTime - editing->mStart;
                mouse_hold |= ImGui::ImCurveEdit::Edit( nullptr,
                                                        &transition->mKeyPoints, 
                                                        sub_window_size, 
                                                        ImGui::GetID("##video_transition_keypoint_editor"),
                                                        true,
                                                        current_time,
                                                        CURVE_EDIT_FLAG_VALUE_LIMITED | CURVE_EDIT_FLAG_MOVE_CURVE | CURVE_EDIT_FLAG_KEEP_BEGIN_END | CURVE_EDIT_FLAG_DOCK_BEGIN_END, 
                                                        nullptr, // clippingRect
                                                        &_changed
                                                        );
                if (_changed) timeline->UpdatePreview();
            }
            // draw cursor line after curve draw
            if (timeline && editing)
            {
                static const float cursorWidth = 2.f;
                float cursorOffset = sub_window_pos.x + (timeline->mCurrentTime - editing->mStart) * editing->msPixelWidth - 0.5f;
                draw_list->AddLine(ImVec2(cursorOffset, sub_window_pos.y), ImVec2(cursorOffset, sub_window_pos.y + sub_window_size.y), COL_CURSOR_LINE_R, cursorWidth);
            }
        }
        ImGui::EndChild();
    }

    // draw overlap setting
    ImGui::SetCursorScreenPos(clip_setting_pos);
    if (ImGui::BeginChild("##video_transition_setting", clip_setting_size, false, setting_child_flags))
    {
        auto addCurve = [&](std::string name, float _min, float _max, float _default)
        {
            if (transition)
            {
                auto found = transition->mKeyPoints.GetCurveIndex(name);
                if (found == -1)
                {
                    ImU32 color; ImGui::RandomColor(color, 1.f);
                    auto curve_index = transition->mKeyPoints.AddCurveByDim(name, ImGui::ImCurveEdit::Linear, color, true, ImGui::ImCurveEdit::DIM_X, _min, _max, _default);
                    transition->mKeyPoints.AddPointByDim(curve_index, ImVec2(0.f, _min), ImGui::ImCurveEdit::Linear, ImGui::ImCurveEdit::DIM_X, true);
                    transition->mKeyPoints.AddPointByDim(curve_index, ImVec2(editing->mEnd-editing->mStart, _max), ImGui::ImCurveEdit::Linear, ImGui::ImCurveEdit::DIM_X, true);
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
        if (editing && transition)
        {
            // Overlap curve setting
            if (ImGui::TreeNodeEx("Curve Setting##video_transition", ImGuiTreeNodeFlags_DefaultOpen))
            {
                char ** curve_type_list = nullptr;
                auto curve_type_count = ImGui::ImCurveEdit::GetCurveTypeName(curve_type_list);
                static std::string curve_name = "";
                std::string value = curve_name;
                bool name_input_empty = curve_name.empty();
                if (ImGui::InputTextWithHint("##new_curve_name_video_transition", "Input curve name", (char*)value.data(), value.size() + 1, ImGuiInputTextFlags_CallbackEdit | ImGuiInputTextFlags_CallbackResize, [](ImGuiInputTextCallbackData* data) -> int
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
                if (ImGui::Button(ICON_ADD "##insert_curve_video_transition"))
                {
                    addCurve(curve_name, 0.f, 1.f, 1.f);
                }
                ImGui::ShowTooltipOnHover("Add custom curve");
                ImGui::PopStyleVar();
                ImGui::EndDisabled();

                // list curves
                for (int i = 0; i < transition->mKeyPoints.GetCurveCount(); i++)
                {
                    bool break_loop = false;
                    ImGui::PushID(i);
                    auto pCount = transition->mKeyPoints.GetCurvePointCount(i);
                    std::string lable_id = std::string(ICON_CURVE) + " " + transition->mKeyPoints.GetCurveName(i) + " (" + std::to_string(pCount) + " keys)" + "##video_transition_curve";
                    if (ImGui::TreeNodeEx(lable_id.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        float curve_min = transition->mKeyPoints.GetCurveMinByDim(i, ImGui::ImCurveEdit::DIM_X);
                        float curve_max = transition->mKeyPoints.GetCurveMaxByDim(i, ImGui::ImCurveEdit::DIM_X);
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
                        const auto curve_time = timeline->mCurrentTime - editing->mStart;
                        const auto curve_value = transition->mKeyPoints.GetValueByDim(i, curve_time, ImGui::ImCurveEdit::DIM_X);
                        ImGui::BracketSquare(true); 
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0, 1.0, 0.0, 1.0)); 
                        ImGui::Text("%.2f", curve_value); 
                        ImGui::PopStyleColor();
                                                ImGui::SameLine();
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0, 1.0, 0.0, 1.0)); 
                        ImGui::Text("%s", ImGuiHelper::MillisecToString(curve_time, 3).c_str()); 
                        ImGui::PopStyleColor();
                        ImGui::SameLine();
                        bool in_range = curve_time >= transition->mKeyPoints.GetMin().w && 
                                        curve_time <= transition->mKeyPoints.GetMax().w;
                        ImGui::BeginDisabled(!in_range);
                        if (ImGui::Button(ICON_MD_ADS_CLICK))
                        {
                            transition->mKeyPoints.AddPointByDim(i, ImVec2(curve_time, curve_value), ImGui::ImCurveEdit::Smooth, ImGui::ImCurveEdit::DIM_X, true);
                        }
                        ImGui::EndDisabled();
                        ImGui::ShowTooltipOnHover("Add key at current");

                        ImGui::PushItemWidth(60);
                        if (ImGui::DragFloat("##curve_video_transition_min", &curve_min, 0.1f, -FLT_MAX, curve_max, "%.1f"))
                        {
                            transition->mKeyPoints.SetCurveMinByDim(i, curve_min, ImGui::ImCurveEdit::DIM_X);
                            timeline->UpdatePreview();
                        } ImGui::ShowTooltipOnHover("Min");
                        ImGui::SameLine(0, 8);
                        if (ImGui::DragFloat("##curve_video_transition_max", &curve_max, 0.1f, curve_min, FLT_MAX, "%.1f"))
                        {
                            transition->mKeyPoints.SetCurveMaxByDim(i, curve_max, ImGui::ImCurveEdit::DIM_X);
                            timeline->UpdatePreview();
                        } ImGui::ShowTooltipOnHover("Max");
                        ImGui::SameLine(0, 8);
                        float curve_default = transition->mKeyPoints.GetCurveDefaultByDim(i, ImGui::ImCurveEdit::DIM_X);
                        if (ImGui::DragFloat("##curve_video_transition_default", &curve_default, 0.1f, curve_min, curve_max, "%.1f"))
                        {
                            transition->mKeyPoints.SetCurveDefaultByDim(i, curve_default, ImGui::ImCurveEdit::DIM_X);
                            timeline->UpdatePreview();
                        } ImGui::ShowTooltipOnHover("Default");
                        ImGui::PopItemWidth();

                        ImGui::SameLine(0, 8);
                        ImGui::SetWindowFontScale(0.75);
                        auto curve_color = ImGui::ColorConvertU32ToFloat4(transition->mKeyPoints.GetCurveColor(i));
                        if (ImGui::ColorEdit4("##curve_video_transition_color", (float*)&curve_color, ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar))
                        {
                            transition->mKeyPoints.SetCurveColor(i, ImGui::ColorConvertFloat4ToU32(curve_color));
                        } ImGui::ShowTooltipOnHover("Curve Color");
                        ImGui::SetWindowFontScale(1.0);
                        ImGui::SameLine(0, 4);
                        bool is_visiable = transition->mKeyPoints.IsVisible(i);
                        if (ImGui::Button(is_visiable ? ICON_WATCH : ICON_UNWATCH "##curve_video_transition_visiable"))
                        {
                            is_visiable = !is_visiable;
                            transition->mKeyPoints.SetCurveVisible(i, is_visiable);
                        } ImGui::ShowTooltipOnHover( is_visiable ? "Hide" : "Show");
                        ImGui::SameLine(0, 4);
                        if (ImGui::Button(ICON_DELETE "##curve_video_transition_delete"))
                        {
                            // delete blueprint entry node pin
                            auto pin_name = transition->mKeyPoints.GetCurveName(i);
                            if (blueprint)
                            {
                                auto entry_node = blueprint->FindEntryPointNode();
                                if (entry_node) entry_node->DeleteOutputPin(pin_name);
                                timeline->UpdatePreview();
                            }
                            transition->mKeyPoints.DeleteCurve(i);
                            break_loop = true;
                        } ImGui::ShowTooltipOnHover("Delete");
                        ImGui::SameLine(0, 4);
                        if (ImGui::Button(ICON_MD_ROTATE_90_DEGREES_CCW "##curve_video_transition_reset"))
                        {
                            for (int p = 0; p < pCount; p++)
                            {
                                transition->mKeyPoints.SetCurvePointDefault(i, p);
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
                                auto point = transition->mKeyPoints.GetPoint(i, p);
                                ImGui::Diamond(false);
                                if (p == 0 || p == pCount - 1)
                                    is_disabled = true;
                                ImGui::BeginDisabled(is_disabled);
                                if (ImGui::DragTimeMS("##curve_video_transition_point_time", &point.t, transition->mKeyPoints.GetMax().w / 1000.f, transition->mKeyPoints.GetMin().w, transition->mKeyPoints.GetMax().w, 2))
                                {
                                    transition->mKeyPoints.EditPoint(i, p, point.val, point.type);
                                    timeline->UpdatePreview();
                                }
                                ImGui::EndDisabled();
                                ImGui::SameLine();
                                if (ImGui::DragFloat("##curve_video_transition_point_x", &point.x, 0.01f, curve_min, curve_max, "%.2f"))
                                {
                                    transition->mKeyPoints.EditPoint(i, p, point.val, point.type);
                                    timeline->UpdatePreview();
                                }
                                ImGui::SameLine();
                                if (ImGui::Combo("##curve_video_transition_type", (int*)&point.type, curve_type_list, curve_type_count))
                                {
                                    transition->mKeyPoints.EditPoint(i, p, point.val, point.type);
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
                if (ImGui::TreeNodeEx("Node Configure##video_transition", ImGuiTreeNodeFlags_DefaultOpen))
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
                        std::string lable_id = label_name + "##video_transition_node" + "@" + std::to_string(node->m_ID);
                        node->DrawNodeLogo(ImGui::GetCurrentContext(), ImVec2(50, 28));
                        ImGui::SameLine(70);
                        if (ImGui::TreeNodeEx(lable_id.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            ImGui::ImCurveEdit::Curve curve;
                            curve.m_id = node->m_ID;
                            if (node->DrawCustomLayout(ImGui::GetCurrentContext(), 1.0, ImVec2(0, 0), &curve, false))
                            {
                                timeline->UpdatePreview();
                                blueprint->Blueprint_UpdateNode(node->m_ID);
                            }
                            if (!curve.name.empty())
                            {
                                addCurve(curve.name,
                                        ImGui::ImCurveEdit::GetDimVal(curve.m_min, ImGui::ImCurveEdit::DIM_X),
                                        ImGui::ImCurveEdit::GetDimVal(curve.m_max, ImGui::ImCurveEdit::DIM_X),
                                        ImGui::ImCurveEdit::GetDimVal(curve.m_default, ImGui::ImCurveEdit::DIM_X));
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
 * Audio Editor windows
 *
 ***************************************************************************************/
static void DrawAudioClipPreviewWindow(ImDrawList *draw_list, EditingAudioClip * editing_clip, bool control_only = false)
{
    ImRect video_rect;
    ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
    ImVec2 sub_window_size = ImGui::GetWindowSize();
    draw_list->AddRectFilled(sub_window_pos, sub_window_pos + sub_window_size, COL_DEEP_DARK);
    ShowMediaPreviewWindow(draw_list, "Audio Clip", 1.5f, video_rect, editing_clip ? editing_clip->mStart : -1, editing_clip ? editing_clip->mEnd : -1, true, true, false, false, false, control_only);
}

static bool DrawAudioClipTimelineWindow(bool& show_BP, EditingAudioClip * editing_clip)
{
    ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
    ImVec2 sub_window_size = ImGui::GetWindowSize();
    bool timeline_changed = false;
    auto mouse_hild = DrawClipTimeLine(timeline, editing_clip, timeline->mCurrentTime, 30, 50, show_BP, timeline_changed);
    if (!g_project_loading) project_changed |= timeline_changed;
    return mouse_hild;
}

static void ShowAudioClipWindow(ImDrawList *draw_list, ImRect title_rect, EditingAudioClip* editing)
{
    /*
                1. with blueprint edit(3 Splitters)                                       2. with blueprint edit without preview(2 Splitters)         
    ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━┓    ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━┓      
    ┃    []--[]--                                ║                       ┃    ┃    []--[]--                                ║    |<  <  []  >  >|   ┃      
    ┃             \                              ║                       ┃    ┃             \                              ╟━━━━━━━━━━━━━━━━━━━━━━━┫      
    ┃               \- []                        ║                       ┃    ┃               \- []                        ║   > event             ┃      
    ┃                                            ║                       ┃    ┃                                            ║     |> filter         ┃      
    ┃                                            ║       preview         ┃    ┃                                            ║      > filter edit    ┃      
    ┃                                            ║                       ┃    ┃                                            ║        .              ┃      
    ┃                                            ║                       ┃    ┃                                            ║        .              ┃      
    ┃                                            ║                       ┃    ┃                                            ║      > effect edit    ┃      
    ┃                                            ╟━━━━━━━━━━━━━━━━━━━━━━━┫    ┃                                            ║        .              ┃      
    ┃                                            ║    |<  <  []  >  >|   ┃    ┃                                            ║        .              ┃      
    ┣════════════════════════════════════════════╬═══════════════════════┫    ┣════════════════════════════════════════════╢      .                ┃      
    ┃             timeline                       ║   > event             ┃    ┃             timeline                       ║      .                ┃      
    ┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━╢    |> filter          ┃    ┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━╢     |> filter         ┃      
    ┃              [ event ]                     ║      > filter edit    ┃    ┃              [ event ]                     ║       > filter edit   ┃      
    ┃              [ curve ]                     ║        .              ┃    ┃              [ curve ]                     ║        .              ┃      
    ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━┛    ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━┛  
                3. without blueprint(2 Splitter)                                          4. without preview and blueprint(1 Splitter)  
    ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━┓    ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━┓
    ┃              timeline                      ║                       ┃    ┃              timeline                      ║    |<  <  []  >  >|   ┃
    ┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━╢                       ┃    ┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━╟━━━━━━━━━━━━━━━━━━━━━━━┫
    ┃              event track                   ║                       ┃    ┃              event track                   ║   > event             ┃
    ┃               [event]                      ║                       ┃    ┃                  .                         ║    |> filter          ┃
    ┃              event track                   ║       preview         ┃    ┃              event track                   ║      > filter edit    ┃
    ┃               [event]                      ║                       ┃    ┃               [event]                      ║        .              ┃
    ┃               [curve]                      ╟━━━━━━━━━━━━━━━━━━━━━━━┫    ┃               [curve]                      ║        .              ┃
    ┃                  .                         ║    |<  <  []  >  >|   ┃    ┃                  .                         ║      > effect edit    ┃
    ┃                  .                         ╟═══════════════════════┫    ┃                  .                         ║        .              ┃
    ┃                  .                         ║    > event            ┃    ┃                  .                         ║        .              ┃
    ┃              event track                   ║      |> filter        ┃    ┃              event track                   ║      .                ┃
    ┃                  .                         ║       > filter edit   ┃    ┃                  .                         ║      .                ┃
    ┃                                            ║        .              ┃    ┃                                            ║                       ┃
    ┃                                            ║        .              ┃    ┃                                            ║                       ┃
    ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━┛    ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━┛ 
    */
    // draw page title
    ImGui::SetWindowFontScale(1.8);
    auto title_size = ImGui::CalcTextSize("Audio Clip");
    float str_offset = title_rect.Max.x - title_size.x - 16;
    ImGui::SetWindowFontScale(1.0);
    draw_list->AddTextComplex(ImVec2(str_offset, title_rect.Min.y), "Audio Clip", 1.8f, COL_TITLE_COLOR, 0.5f, COL_TITLE_OUTLINE);

    ImVec2 window_pos = ImGui::GetCursorScreenPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    draw_list->AddRectFilled(window_pos, window_pos + window_size, COL_DEEP_DARK);
    float audio_view_width = window_size.x * 2 / 3;
    float audio_editor_width = window_size.x - audio_view_width;
    if (!timeline)
        return;

    BluePrint::BluePrintUI* blueprint = nullptr;
    ImGui::KeyPointEditor* keypoint = nullptr;
    Clip * editing_clip = editing->GetClip();
    blueprint = editing->mFilterBp;
    keypoint = editing->mFilterKp;
    editing->UpdateClipRange(editing_clip);

    // every type for area rect
    // preview_rect: right top of event window
    // preview_control_rect: if preview_rect is exist, width shoule equ preview_rect.width, otherwise, alway on top and same width with window
    // timeline_rect: always bottom left of window, include event_track, timeline_height = 30(head height) + 50(clip height) + 12
    // event_list_rect: always bottom right of window
    // blueprint_rect: only has when we show event's bp, and aways on left top of window, same width with timeline
    ImRect preview_rect, preview_control_rect, timeline_rect, event_list_rect, blueprint_rect;
    const float event_min_width = 440;
    bool is_splitter_hold = false;
    static bool show_blueprint = false;
    int preview_count = 0;
    if (MonitorIndexPreviewVideo == -1) preview_count ++;
    if (!show_blueprint)
    {
        if (preview_count == 0)
        {
            // 1 Splitter without preview and blueprint vertically only(chart 4)
            float timeline_width = window_size.x * g_media_editor_settings.audio_clip_timeline_width;
            float event_list_width = window_size.x - timeline_width;
            is_splitter_hold |= ImGui::Splitter(true, 4.0f, &timeline_width, &event_list_width, window_size.x * 0.5, event_min_width/*window_size.x * 0.2*/);
            g_media_editor_settings.audio_clip_timeline_width = timeline_width / window_size.x;
            if (ImGui::BeginChild("audio_timeline", ImVec2(timeline_width - 4, window_size.y), false))
            {
                mouse_hold |= DrawAudioClipTimelineWindow(show_blueprint, editing);
            }
            ImGui::EndChild();
            ImGui::SameLine();
            if (ImGui::BeginChild("audio_event_list&preview_tool_bar", ImVec2(event_list_width - 4, window_size.y), false))
            {
                // show preview window with tool bar only
                if (ImGui::BeginChild("audio_preview_tool_bar", ImVec2(event_list_width - 4, 48), false))
                {
                    DrawAudioClipPreviewWindow(draw_list, editing, true);
                }
                ImGui::EndChild();
                ImGui::Dummy(ImVec2(0, 4));
                if (ImGui::BeginChild("audio_event_list", ImVec2(event_list_width - 4, window_size.y - 48), false))
                {
                    DrawClipEventWindow(draw_list, editing);
                }
                ImGui::EndChild();
            }
            ImGui::EndChild();
        }
        else
        {
            // 2 Splitter preview without blueprint vertically first (chart 3)
            float timeline_width = window_size.x * g_media_editor_settings.audio_clip_timeline_width;
            float event_list_width = window_size.x - timeline_width;
            is_splitter_hold |= ImGui::Splitter(true, 4.0f, &timeline_width, &event_list_width, window_size.x * 0.5, event_min_width/*window_size.x * 0.2*/);
            g_media_editor_settings.audio_clip_timeline_width = timeline_width / window_size.x;
            if (ImGui::BeginChild("audio_timeline", ImVec2(timeline_width - 4, window_size.y), false))
            {
                mouse_hold |= DrawAudioClipTimelineWindow(show_blueprint, editing);
            }
            ImGui::EndChild();
            ImGui::SameLine();
            if (ImGui::BeginChild("audio_preview&event_list", ImVec2(event_list_width - 8, window_size.y), false))
            {
                float timeline_height = window_size.y * g_media_editor_settings.audio_clip_timeline_height;
                float preview_height = window_size.y - timeline_height;
                is_splitter_hold |= ImGui::Splitter(false, 4.0f, &preview_height, &timeline_height, window_size.y * 0.3, window_size.y * 0.3);
                g_media_editor_settings.audio_clip_timeline_height = timeline_height / window_size.y;
                if (ImGui::BeginChild("audio_preview", ImVec2(event_list_width - 4, preview_height - 4), false))
                {
                    DrawAudioClipPreviewWindow(draw_list, editing);
                }
                ImGui::EndChild();
                ImGui::Dummy(ImVec2(0, 4));
                if (ImGui::BeginChild("audio_event_list", ImVec2(event_list_width - 4, timeline_height - 8), false))
                {
                    DrawClipEventWindow(draw_list, editing);
                }
                ImGui::EndChild();
            }
            ImGui::EndChild();
        }
    }
    else
    {
        if (preview_count == 0)
        {
            // 2 Splitters with blueprint edit without preview vertically first (chart 2)
            float timeline_width = window_size.x * g_media_editor_settings.audio_clip_timeline_width;
            float event_list_width = window_size.x - timeline_width;
            is_splitter_hold |= ImGui::Splitter(true, 4.0f, &timeline_width, &event_list_width, window_size.x * 0.5, event_min_width/*window_size.x * 0.2*/);
            g_media_editor_settings.audio_clip_timeline_width = timeline_width / window_size.x;
            if (ImGui::BeginChild("audio_blue_print&timeline", ImVec2(timeline_width - 4, window_size.y), false))
            {
                float timeline_height = window_size.y * g_media_editor_settings.audio_clip_timeline_height;
                float blueprint_height = window_size.y - timeline_height;
                is_splitter_hold |= ImGui::Splitter(false, 4.0f, &blueprint_height, &timeline_height, window_size.y * 0.3, window_size.y * 0.3);
                g_media_editor_settings.audio_clip_timeline_height = timeline_height / window_size.y;
                if (ImGui::BeginChild("audio_blue_print", ImVec2(timeline_width - 4, blueprint_height - 4), false))
                {
                    DrawClipBlueprintWindow(draw_list, editing);
                }
                ImGui::EndChild();
                if (ImGui::BeginChild("audio_timeline", ImVec2(timeline_width - 4, timeline_height - 8), false))
                {
                    mouse_hold |= DrawAudioClipTimelineWindow(show_blueprint, editing);
                }
                ImGui::EndChild();
            }
            ImGui::EndChild();
            ImGui::SameLine();
            if (ImGui::BeginChild("audio_preview_var&event_list", ImVec2(event_list_width - 4, window_size.y - 8), false))
            {
                // show preview window with tool bar only
                if (ImGui::BeginChild("audio_preview_tool_bar", ImVec2(event_list_width - 4, 48), false))
                {
                    DrawAudioClipPreviewWindow(draw_list, editing, true);
                }
                ImGui::EndChild();
                ImGui::Dummy(ImVec2(0, 4));
                if (ImGui::BeginChild("audio_event_list", ImVec2(event_list_width - 4, window_size.y - 48), false))
                {
                    DrawClipEventWindow(draw_list, editing);
                }
                ImGui::EndChild();
            }
            ImGui::EndChild();
        }
        else
        {
            // 3 Splitters with blueprint edit vertically first (chart 1)
            float timeline_width = window_size.x * g_media_editor_settings.audio_clip_timeline_width;
            float event_list_width = window_size.x - timeline_width;
            is_splitter_hold |= ImGui::Splitter(true, 4.0f, &timeline_width, &event_list_width, window_size.x * 0.5, event_min_width/*window_size.x * 0.2*/);
            g_media_editor_settings.audio_clip_timeline_width = timeline_width / window_size.x;
            if (ImGui::BeginChild("audio_blue_print&timeline", ImVec2(timeline_width - 4, window_size.y), false))
            {
                float timeline_height = window_size.y * g_media_editor_settings.audio_clip_timeline_height;
                float blueprint_height = window_size.y - timeline_height;
                is_splitter_hold |= ImGui::Splitter(false, 4.0f, &blueprint_height, &timeline_height, window_size.y * 0.3, window_size.y * 0.3);
                g_media_editor_settings.audio_clip_timeline_height = timeline_height / window_size.y;
                if (ImGui::BeginChild("audio_blue_print", ImVec2(timeline_width - 4, blueprint_height - 4), false))
                {
                    DrawClipBlueprintWindow(draw_list, editing);
                }
                ImGui::EndChild();
                if (ImGui::BeginChild("audio_timeline", ImVec2(timeline_width - 4, timeline_height - 8), false))
                {
                    mouse_hold |= DrawAudioClipTimelineWindow(show_blueprint, editing);
                }
                ImGui::EndChild();
            }
            ImGui::EndChild();
            ImGui::SameLine();
            if (ImGui::BeginChild("audio_preview&event_list", ImVec2(event_list_width - 4, window_size.y), false))
            {
                float timeline_height = window_size.y * g_media_editor_settings.audio_clip_timeline_height;
                float preview_height = window_size.y - timeline_height;
                is_splitter_hold |= ImGui::Splitter(false, 4.0f, &preview_height, &timeline_height, window_size.y * 0.3, window_size.y * 0.3);
                g_media_editor_settings.audio_clip_timeline_height = timeline_height / window_size.y;
                if (ImGui::BeginChild("audio_preview", ImVec2(event_list_width - 4, preview_height - 4), false))
                {
                    DrawAudioClipPreviewWindow(draw_list, editing);
                }
                ImGui::EndChild();
                ImGui::Dummy(ImVec2(0, 4));
                if (ImGui::BeginChild("audio_event_list", ImVec2(event_list_width - 4, timeline_height - 48), false))
                {
                    DrawClipEventWindow(draw_list, editing);
                }
                ImGui::EndChild();
            }
            ImGui::EndChild();
        }
    }
}

static void ShowAudioTransitionBluePrintWindow(ImDrawList *draw_list, EditingAudioOverlap * editing)
{
    Overlap * overlap = timeline && editing ? timeline->FindOverlapByID(editing->mID) : nullptr;
    if (timeline && editing && editing->mTransition && editing->mTransition->mBp)
    {
        if (overlap && !editing->mTransition->mBp->m_Document->m_Blueprint.IsOpened())
        {
            auto track = timeline->FindTrackByClipID(overlap->m_Clip.first);
            if (track)
                track->SelectEditingOverlap(overlap);
            editing->mTransition->mBp->View_ZoomToContent();
        }
        ImVec2 window_pos = ImGui::GetCursorScreenPos();
        ImVec2 window_size = ImGui::GetWindowSize();
        ImGui::SetCursorScreenPos(window_pos + ImVec2(3, 3));
        ImGui::InvisibleButton("audio_transition_blueprint_back_view", window_size - ImVec2(6, 6));
        if (ImGui::BeginDragDropTarget() && editing->mTransition->mBp->Blueprint_IsValid())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Transition_drag_drop_Audio"))
            {
                const BluePrint::Node * node = (const BluePrint::Node *)payload->Data;
                if (node)
                {
                    editing->mTransition->mBp->Edit_Insert(node->GetTypeID());
                }
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::SetCursorScreenPos(window_pos + ImVec2(1, 1));
        if (ImGui::BeginChild("##audio_transition_blueprint", window_size - ImVec2(2, 2), false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings))
        {
            editing->mTransition->mBp->Frame(true, true, overlap != nullptr, BluePrint::BluePrintFlag::BluePrintFlag_Transition);
        }
        ImGui::EndChild();
        if (timeline->mIsBluePrintChanged) { project_changed = true; project_need_save = true; }
    }
}

static void ShowAudioTransitionWindow(ImDrawList *draw_list, ImRect title_rect, EditingAudioOverlap* editing)
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
    // draw page title
    ImGui::SetWindowFontScale(1.8);
    auto title_size = ImGui::CalcTextSize("Audio Transition");
    float str_offset = title_rect.Max.x - title_size.x - 16;
    ImGui::SetWindowFontScale(1.0);
    draw_list->AddTextComplex(ImVec2(str_offset, title_rect.Min.y), "Audio Transition", 1.8f, COL_TITLE_COLOR, 0.5f, COL_TITLE_OUTLINE);

    ImVec2 window_pos = ImGui::GetCursorScreenPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    draw_list->AddRectFilled(window_pos, window_pos + window_size, COL_DEEP_DARK);
    float audio_view_width = window_size.x * 2 / 3;
    float audio_editor_width = window_size.x - audio_view_width;
    if (!timeline)
        return;

    BluePrint::BluePrintUI* blueprint = nullptr;
    BluePrintAudioTransition * transition = nullptr;

    Overlap * editing_overlap = timeline->FindOverlapByID(editing->mID);

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

    if (editing_overlap)
    {
        if (editing)
        {
            transition = editing->mTransition;
            if (editing->mStart != editing_overlap->mStart || editing->mEnd != editing_overlap->mEnd)
            {
                editing->mStart = editing_overlap->mStart;
                editing->mEnd = editing_overlap->mEnd;
                editing->mDuration = editing->mEnd - editing->mStart;
                if (transition) transition->mKeyPoints.SetMax(ImVec4(1.f, 1.f, 1.f, editing_overlap->mEnd-editing_overlap->mStart), true);
            }
        }
        blueprint = transition ? transition->mBp : nullptr;
    }

    float clip_header_height = 30;
    float clip_channel_height = 60;
    float clip_timeline_height = clip_header_height + clip_channel_height * 2;
    float clip_keypoint_height = g_media_editor_settings.AudioTransitionCurveExpanded ? 120 : 0;
    ImVec2 preview_pos = window_pos;
    float preview_width = audio_view_width;

    float preview_height = window_size.y - clip_timeline_height - clip_keypoint_height - 4;
    float audio_blueprint_height = window_size.y / 2;
    float clip_timeline_width = audio_view_width;
    ImVec2 clip_timeline_pos = window_pos + ImVec2(0, preview_height);
    ImVec2 clip_timeline_size(clip_timeline_width, clip_timeline_height);
    ImVec2 clip_keypoint_pos = g_media_editor_settings.AudioTransitionCurveExpanded ? clip_timeline_pos + ImVec2(0, clip_timeline_height) : clip_timeline_pos + ImVec2(0, clip_timeline_height - 16);
    ImVec2 clip_keypoint_size(audio_view_width, clip_keypoint_height);
    ImVec2 clip_setting_pos = window_pos + ImVec2(audio_view_width, window_size.y / 2);
    ImVec2 clip_setting_size(audio_editor_width, window_size.y / 2);

    ImGuiWindowFlags child_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings;
    ImGuiWindowFlags setting_child_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;

    if (ImGui::BeginChild("##audio_transition_preview", ImVec2(preview_width, preview_height), false, child_flags))
    {
        ImRect video_rect;
        ImVec2 audio_view_window_pos = ImGui::GetCursorScreenPos();
        ImVec2 audio_view_window_size = ImGui::GetWindowSize();
        draw_list->AddRectFilled(audio_view_window_pos, audio_view_window_pos + audio_view_window_size, COL_DEEP_DARK);
        ShowMediaPreviewWindow(draw_list, "Audio Transition", 1.5f, video_rect, editing_overlap ? editing_overlap->mStart : -1, editing_overlap ? editing_overlap->mEnd : -1, true, false, false);
    }
    ImGui::EndChild();

    ImGui::SetCursorScreenPos(clip_timeline_pos);
    if (ImGui::BeginChild("##audio_transition_timeline", clip_timeline_size, false, child_flags))
    {
        ImVec2 clip_timeline_window_pos = ImGui::GetCursorScreenPos();
        ImVec2 clip_timeline_window_size = ImGui::GetWindowSize();
        draw_list->AddRectFilled(clip_timeline_window_pos, clip_timeline_window_pos + clip_timeline_window_size, COL_DARK_TWO);
        DrawOverlapTimeLine(editing, timeline->mCurrentTime - (editing ? editing->mStart : 0), clip_header_height, clip_channel_height);
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
            g_media_editor_settings.AudioTransitionCurveExpanded = !g_media_editor_settings.AudioTransitionCurveExpanded;
        }
        if (ImGui::BeginTooltip())
        {
            ImGui::TextUnformatted(g_media_editor_settings.TextCurveExpanded ? "Hide Curve View" : "Show Curve View");
            ImGui::EndTooltip();
        }
    }
    draw_list->AddText(hidden_button_pos, IM_COL32_WHITE, ICON_FA_BEZIER_CURVE);
    ImGui::SetWindowFontScale(1.0);

    // draw transition curve editor
    if (g_media_editor_settings.AudioTransitionCurveExpanded)
    {
        ImGui::SetCursorScreenPos(clip_keypoint_pos);
        if (ImGui::BeginChild("##audio_transition_keypoint", clip_keypoint_size, false, child_flags))
        {
            ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
            ImVec2 sub_window_size = ImGui::GetWindowSize();
            draw_list->AddRectFilled(sub_window_pos, sub_window_pos + sub_window_size, COL_DARK_ONE);
            if (editing && transition)
            {
                bool _changed = false;
                float current_time = timeline->mCurrentTime - editing->mStart;
                mouse_hold |= ImGui::ImCurveEdit::Edit( nullptr,
                                                        &transition->mKeyPoints,
                                                        sub_window_size, 
                                                        ImGui::GetID("##audio_transition_keypoint_editor"), 
                                                        true,
                                                        current_time,
                                                        CURVE_EDIT_FLAG_VALUE_LIMITED | CURVE_EDIT_FLAG_MOVE_CURVE | CURVE_EDIT_FLAG_KEEP_BEGIN_END | CURVE_EDIT_FLAG_DOCK_BEGIN_END, 
                                                        nullptr, // clippingRect
                                                        &_changed
                                                        );
                if (_changed) timeline->UpdatePreview();
            }
            // draw cursor line after curve draw
            if (timeline && editing)
            {
                static const float cursorWidth = 2.f;
                float cursorOffset = sub_window_pos.x + (timeline->mCurrentTime - editing->mStart) * editing->msPixelWidth - 0.5f;
                draw_list->AddLine(ImVec2(cursorOffset, sub_window_pos.y), ImVec2(cursorOffset, sub_window_pos.y + sub_window_size.y), COL_CURSOR_LINE_R, cursorWidth);
            }
        }
        ImGui::EndChild();
    }

    ImGui::SetCursorScreenPos(window_pos + ImVec2(audio_view_width, 0));
    if (ImGui::BeginChild("##audio_transition_blueprint", ImVec2(audio_editor_width, audio_blueprint_height), false, child_flags))
    {
        ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
        ImVec2 sub_window_size = ImGui::GetWindowSize();
        draw_list->AddRectFilled(sub_window_pos, sub_window_pos + sub_window_size, COL_DARK_ONE);
        ShowAudioTransitionBluePrintWindow(draw_list, editing);
    }
    ImGui::EndChild();

    // Draw Audio transition setting
    ImGui::SetCursorScreenPos(clip_setting_pos);
    if (ImGui::BeginChild("##audio_transition_setting", clip_setting_size, false, setting_child_flags))
    {
        auto addCurve = [&](std::string name, float _min, float _max, float _default)
        {
            if (transition)
            {
                auto found = transition->mKeyPoints.GetCurveIndex(name);
                if (found == -1)
                {
                    ImU32 color; ImGui::RandomColor(color, 1.f);
                    auto curve_index = transition->mKeyPoints.AddCurveByDim(name, ImGui::ImCurveEdit::Linear, color, true, ImGui::ImCurveEdit::DIM_X, _min, _max, _default);
                    transition->mKeyPoints.AddPointByDim(curve_index, ImVec2(0.f, _min), ImGui::ImCurveEdit::Linear, ImGui::ImCurveEdit::DIM_X, true);
                    transition->mKeyPoints.AddPointByDim(curve_index, ImVec2(editing->mEnd-editing->mStart, _max), ImGui::ImCurveEdit::Linear, ImGui::ImCurveEdit::DIM_X, true);
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
        if (editing && transition)
        {
            // Transition curve setting
            if (ImGui::TreeNodeEx("Curve Setting##audio_transition", ImGuiTreeNodeFlags_DefaultOpen))
            {
                char ** curve_type_list = nullptr;
                auto curve_type_count = ImGui::ImCurveEdit::GetCurveTypeName(curve_type_list);
                static std::string curve_name = "";
                std::string value = curve_name;
                bool name_input_empty = curve_name.empty();
                if (ImGui::InputTextWithHint("##new_curve_name_audio_transition", "Input curve name", (char*)value.data(), value.size() + 1, ImGuiInputTextFlags_CallbackEdit | ImGuiInputTextFlags_CallbackResize, [](ImGuiInputTextCallbackData* data) -> int
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
                if (ImGui::Button(ICON_ADD "##insert_curve_audio_transition"))
                {
                    addCurve(curve_name, 0.f, 1.f, 1.0);
                }
                ImGui::ShowTooltipOnHover("Add custom curve");
                ImGui::PopStyleVar();
                ImGui::EndDisabled();

                // list curves
                for (int i = 0; i < transition->mKeyPoints.GetCurveCount(); i++)
                {
                    bool break_loop = false;
                    ImGui::PushID(i);
                    auto pCount = transition->mKeyPoints.GetCurvePointCount(i);
                    std::string lable_id = std::string(ICON_CURVE) + " " + transition->mKeyPoints.GetCurveName(i) + " (" + std::to_string(pCount) + " keys)" + "##audio_transition_curve";
                    if (ImGui::TreeNodeEx(lable_id.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        float curve_min = transition->mKeyPoints.GetCurveMinByDim(i, ImGui::ImCurveEdit::DIM_X);
                        float curve_max = transition->mKeyPoints.GetCurveMinByDim(i, ImGui::ImCurveEdit::DIM_X);
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
                        const auto curve_time = timeline->mCurrentTime - editing->mStart;
                        const float curve_value = transition->mKeyPoints.GetValueByDim(i, curve_time, ImGui::ImCurveEdit::DIM_X);
                        ImGui::BracketSquare(true); 
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0, 1.0, 0.0, 1.0)); 
                        ImGui::Text("%.2f", curve_value); 
                        ImGui::PopStyleColor();
                        ImGui::SameLine();
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0, 1.0, 0.0, 1.0)); 
                        ImGui::Text("%s", ImGuiHelper::MillisecToString(curve_time, 3).c_str()); 
                        ImGui::PopStyleColor();
                        ImGui::SameLine();
                        bool in_range = curve_time >= transition->mKeyPoints.GetMin().w && 
                                        curve_time <= transition->mKeyPoints.GetMax().w;
                        ImGui::BeginDisabled(!in_range);
                        if (ImGui::Button(ICON_MD_ADS_CLICK))
                        {
                            transition->mKeyPoints.AddPointByDim(i, ImVec2(curve_time, curve_value), ImGui::ImCurveEdit::Smooth, ImGui::ImCurveEdit::DIM_X, true);
                        }
                        ImGui::EndDisabled();
                        ImGui::ShowTooltipOnHover("Add key at current");
                        
                        ImGui::PushItemWidth(60);
                        if (ImGui::DragFloat("##curve_audio_transition_min", &curve_min, 0.1f, -FLT_MAX, curve_max, "%.1f"))
                        {
                            transition->mKeyPoints.SetCurveMinByDim(i, curve_min, ImGui::ImCurveEdit::DIM_X);
                            timeline->UpdatePreview();
                        } ImGui::ShowTooltipOnHover("Min");
                        ImGui::SameLine(0, 8);
                        if (ImGui::DragFloat("##curve_audio_transition_max", &curve_max, 0.1f, curve_min, FLT_MAX, "%.1f"))
                        {
                            transition->mKeyPoints.SetCurveMaxByDim(i, curve_max, ImGui::ImCurveEdit::DIM_X);
                            timeline->UpdatePreview();
                        } ImGui::ShowTooltipOnHover("Max");
                        ImGui::SameLine(0, 8);
                        float curve_default = transition->mKeyPoints.GetCurveDefaultByDim(i, ImGui::ImCurveEdit::DIM_X);
                        if (ImGui::DragFloat("##curve_audio_transition_default", &curve_default, 0.1f, curve_min, curve_max, "%.1f"))
                        {
                            transition->mKeyPoints.SetCurveDefaultByDim(i, curve_default, ImGui::ImCurveEdit::DIM_X);
                            timeline->UpdatePreview();
                        } ImGui::ShowTooltipOnHover("Default");
                        ImGui::PopItemWidth();
                        
                        ImGui::SameLine(0, 8);
                        ImGui::SetWindowFontScale(0.75);
                        auto curve_color = ImGui::ColorConvertU32ToFloat4(transition->mKeyPoints.GetCurveColor(i));
                        if (ImGui::ColorEdit4("##curve_audio_transition_color", (float*)&curve_color, ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar))
                        {
                            transition->mKeyPoints.SetCurveColor(i, ImGui::ColorConvertFloat4ToU32(curve_color));
                        } ImGui::ShowTooltipOnHover("Curve Color");
                        ImGui::SetWindowFontScale(1.0);
                        ImGui::SameLine(0, 4);
                        bool is_visiable = transition->mKeyPoints.IsVisible(i);
                        if (ImGui::Button(is_visiable ? ICON_WATCH : ICON_UNWATCH "##curve_audio_transition_visiable"))
                        {
                            is_visiable = !is_visiable;
                            transition->mKeyPoints.SetCurveVisible(i, is_visiable);
                        } ImGui::ShowTooltipOnHover( is_visiable ? "Hide" : "Show");
                        ImGui::SameLine(0, 4);
                        if (ImGui::Button(ICON_DELETE "##curve_audio_transition_delete"))
                        {
                            // delete blueprint entry node pin
                            auto pin_name = transition->mKeyPoints.GetCurveName(i);
                            if (blueprint)
                            {
                                auto entry_node = blueprint->FindEntryPointNode();
                                if (entry_node) entry_node->DeleteOutputPin(pin_name);
                                timeline->UpdatePreview();
                            }
                            transition->mKeyPoints.DeleteCurve(i);
                            break_loop = true;
                        } ImGui::ShowTooltipOnHover("Delete");
                        ImGui::SameLine(0, 4);
                        if (ImGui::Button(ICON_MD_ROTATE_90_DEGREES_CCW "##curve_audio_transition_reset"))
                        {
                            for (int p = 0; p < pCount; p++)
                            {
                                transition->mKeyPoints.SetCurvePointDefault(i, p);
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
                                auto point = transition->mKeyPoints.GetPoint(i, p);
                                ImGui::Diamond(false);
                                if (p == 0 || p == pCount - 1)
                                    is_disabled = true;
                                ImGui::BeginDisabled(is_disabled);
                                if (ImGui::DragTimeMS("##curve_audio_transition_point_time", &point.t, transition->mKeyPoints.GetMax().w / 1000.f, transition->mKeyPoints.GetMin().w, transition->mKeyPoints.GetMax().w, 2))
                                {
                                    transition->mKeyPoints.EditPoint(i, p, point.val, point.type);
                                    timeline->UpdatePreview();
                                }
                                ImGui::EndDisabled();
                                ImGui::SameLine();
                                auto speed = fabs(curve_max - curve_min) / 500;
                                if (ImGui::DragFloat("##curve_audio_transition_point_x", &point.x, speed, curve_min, curve_max, "%.2f"))
                                {
                                    transition->mKeyPoints.EditPoint(i, p, point.val, point.type);
                                    timeline->UpdatePreview();
                                }
                                ImGui::SameLine();
                                if (ImGui::Combo("##curve_audio_transition_type", (int*)&point.type, curve_type_list, curve_type_count))
                                {
                                    transition->mKeyPoints.EditPoint(i, p, point.val, point.type);
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
            // Transition Node setting
            if (blueprint && blueprint->Blueprint_IsValid())
            {
                if (ImGui::TreeNodeEx("Node Configure##audio_transition", ImGuiTreeNodeFlags_DefaultOpen))
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
                        std::string lable_id = label_name + "##audio_transition_node" + "@" + std::to_string(node->m_ID);
                        node->DrawNodeLogo(ImGui::GetCurrentContext(), ImVec2(28, 28));
                        ImGui::SameLine(40);
                        if (ImGui::TreeNodeEx(lable_id.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            ImGui::ImCurveEdit::Curve curve;
                            curve.m_id = node->m_ID;
                            if (node->DrawCustomLayout(ImGui::GetCurrentContext(), 1.0, ImVec2(0, 0), &curve, false))
                            {
                                timeline->UpdatePreview();
                                blueprint->Blueprint_UpdateNode(node->m_ID);
                            }
                            if (!curve.name.empty())
                            {
                                addCurve(curve.name,
                                        ImGui::ImCurveEdit::GetDimVal(curve.m_min, ImGui::ImCurveEdit::DIM_X),
                                        ImGui::ImCurveEdit::GetDimVal(curve.m_max, ImGui::ImCurveEdit::DIM_X),
                                        ImGui::ImCurveEdit::GetDimVal(curve.m_default, ImGui::ImCurveEdit::DIM_X));
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

static void ShowAudioMixingWindow(ImDrawList *draw_list, ImRect title_rect)
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
    // draw page title
    bool changed = false;
    ImGui::SetWindowFontScale(1.8);
    auto title_size = ImGui::CalcTextSize("Audio Mixer");
    float str_offset = title_rect.Max.x - title_size.x - 16;
    ImGui::SetWindowFontScale(1.0);
    draw_list->AddTextComplex(ImVec2(str_offset, title_rect.Min.y), "Audio Mixer", 1.8f, COL_TITLE_COLOR, 0.5f, COL_TITLE_OUTLINE);
    
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
    ImVec2 equalizer_size = ImVec2(240 + 112, bottom_view_size.y);
    ImVec2 gate_pos = equalizer_pos + ImVec2(equalizer_size.x, 0);
    ImVec2 gate_size = ImVec2(bottom_view_size.x / 5, bottom_view_size.y);
    ImVec2 limiter_pos = gate_pos + ImVec2(gate_size.x, 0);
    ImVec2 limiter_size = ImVec2(bottom_view_size.x / 8, bottom_view_size.y);
    ImVec2 compressor_pos = limiter_pos + ImVec2(limiter_size.x, 0);
    ImVec2 compressor_size = bottom_view_size - ImVec2(equalizer_size.x + pan_size.x + gate_size.x + limiter_size.x, 0);

    auto draw_separator_line = [&](ImVec2 pos, ImVec2 size)
    {
        draw_list->AddLine(pos + ImVec2(16, 0), pos + ImVec2(size.x - 12, 0), COL_MIXING_BORDER_LOW, 2);
    };
    ImGuiWindowFlags setting_child_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
    auto amFilter = timeline->mMtaReader->GetAudioEffectFilter();
    // draw mixing UI
    ImGui::SetCursorScreenPos(mixing_pos);
    if (ImGui::BeginChild("##audio_mixing", mixing_size, false, setting_child_flags))
    {
        char value_str[64] = {0};
        ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
        ImVec2 sub_window_size = ImGui::GetWindowSize();
        //draw_list->AddRectFilled(sub_window_pos, sub_window_pos + sub_window_size, COL_MIXING_BG, 8);
        draw_list->AddRectFilledMultiColor(sub_window_pos, sub_window_pos + sub_window_size, COL_MIXING_BG_HIGH, COL_MIXING_BG_MID, COL_MIXING_BG, COL_MIXING_BG_MID);
        draw_list->AddRect(sub_window_pos, sub_window_pos + sub_window_size, COL_MIXING_BORDER, 8);
        ImGui::Dummy({0, 2});
        ImGui::Indent(16);
        ImGui::SetWindowFontScale(1.2);
        ImGui::TextUnformatted("Audio Mixer");
        ImGui::SetWindowFontScale(1);
        ImGui::Unindent();
        draw_separator_line(ImGui::GetCursorScreenPos(), sub_window_size);
        auto current_pos = ImGui::GetCursorScreenPos() + ImVec2(4, 8);
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
            changed = true;
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
                        changed = true;
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
        //draw_list->AddRectFilled(sub_window_pos, sub_window_pos + sub_window_size, COL_MIXING_BG, 8);
        draw_list->AddRectFilledMultiColor(sub_window_pos, sub_window_pos + sub_window_size, COL_MIXING_BG_HIGH, COL_MIXING_BG_MID, COL_MIXING_BG, COL_MIXING_BG_MID);
        draw_list->AddRect(sub_window_pos, sub_window_pos + sub_window_size, COL_MIXING_BORDER, 8);
        ImGui::Dummy({0, 2});
        ImGui::Indent(16);
        ImGui::SetWindowFontScale(1.2);
        ImGui::TextUnformatted("Audio Meters");
        ImGui::SetWindowFontScale(1);
        ImGui::Unindent();
        draw_separator_line(ImGui::GetCursorScreenPos(), sub_window_size);
        auto current_pos = ImGui::GetCursorScreenPos() + ImVec2(4, 8);
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
    if (ImGui::BeginChild("##audio_mixing_preview", preview_size, false, setting_child_flags))
    {
        ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
        ImVec2 sub_window_size = ImGui::GetWindowSize();
        draw_list->AddRectFilled(sub_window_pos, sub_window_pos + sub_window_size, COL_DEEP_DARK);
        ImRect video_rect;
        ShowMediaPreviewWindow(draw_list, "Audio Mixing", 1.5f, video_rect, timeline->mStart, timeline->mEnd, false, false, false, false, false);
    }
    ImGui::EndChild();

    // bottom area
    // draw pan UI
    ImGui::SetCursorScreenPos(pan_pos);
    ImGui::BeginGroup();
    //draw_list->AddRectFilled(pan_pos, pan_pos + pan_size, COL_MIXING_BG, 8);
    draw_list->AddRectFilledMultiColor(pan_pos, pan_pos + pan_size, COL_MIXING_BG_HIGH, COL_MIXING_BG_MID, COL_MIXING_BG, COL_MIXING_BG_MID);
    draw_list->AddRect(pan_pos, pan_pos + pan_size, COL_MIXING_BORDER, 8);
    ImGui::Dummy({0, 2});
    ImGui::Indent(16);
    ImGui::SetWindowFontScale(1.2);
    bool pan_changed = false;
    ImGui::TextUnformatted("Audio Pan");
    ImGui::SetWindowFontScale(1);
    ImGui::Unindent();
    ImGui::SameLine();
    pan_changed |= ImGui::ToggleButton("##audio_pan_enabe", &timeline->mAudioAttribute.bPan);
    if (ImGui::BeginChild("##audio_pan", pan_size - ImVec2(0, 32), false, setting_child_flags))
    {
        ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
        ImVec2 sub_window_size = ImGui::GetWindowSize();
        draw_separator_line(ImGui::GetCursorScreenPos(), sub_window_size);
        float scroll_y = ImGui::GetScrollY();
        ImGui::PushItemWidth(sub_window_size.x - 48);
        float const w = ImGui::CalcItemWidth();
        float pos_offset = sub_window_size.x <= w ? 0 : (sub_window_size.x - w) / 2 + 8;
        ImGui::SetCursorScreenPos(sub_window_pos + ImVec2(pos_offset, 16));
        ImGui::BeginDisabled(!timeline->mAudioAttribute.bPan);
        auto audio_pan = timeline->mAudioAttribute.audio_pan - ImVec2(0.5, 0.5);
        audio_pan.y = -audio_pan.y;
        pan_changed |= ImGui::InputVec2("##audio_pan_input", &audio_pan, ImVec2(-0.5f, -0.5f), ImVec2(0.5f, 0.5f), 1.0, false, false);
        ImGui::PopItemWidth();
        ImGui::Dummy(ImVec2(0, 16));
        draw_separator_line(ImGui::GetCursorScreenPos(), sub_window_size);
        auto knob_pos = ImGui::GetCursorScreenPos() + ImVec2(0, 32);
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
            changed = true;
        }
        ImGui::EndDisabled();
    }
    ImGui::EndChild();
    ImGui::EndGroup();

    // draw equalizer UI
    ImGui::SetCursorScreenPos(equalizer_pos);
    ImGui::BeginGroup();
    bool equalizer_changed = false;
    //draw_list->AddRectFilled(equalizer_pos, equalizer_pos + equalizer_size, COL_MIXING_BG, 8);
    draw_list->AddRectFilledMultiColor(equalizer_pos, equalizer_pos + equalizer_size, COL_MIXING_BG_HIGH, COL_MIXING_BG_MID, COL_MIXING_BG, COL_MIXING_BG_MID);
    draw_list->AddRect(equalizer_pos, equalizer_pos + equalizer_size, COL_MIXING_BORDER, 8);
    ImGui::Dummy({0, 2});
    ImGui::Indent(16);
    ImGui::SetWindowFontScale(1.2);
    ImGui::TextUnformatted("Equalizer");
    ImGui::SetWindowFontScale(1);
    ImGui::Unindent();
    ImGui::SameLine();
    equalizer_changed |= ImGui::ToggleButton("##audio_equalizer_enabe", &timeline->mAudioAttribute.bEqualizer);
    if (ImGui::BeginChild("##audio_equalizer", equalizer_size - ImVec2(0, 32), false, setting_child_flags))
    {
        ImVec2 sub_window_pos = ImGui::GetCursorScreenPos() + ImVec2(16, 8);
        ImVec2 sub_window_size = ImGui::GetWindowSize();
        draw_separator_line(ImGui::GetCursorScreenPos(), sub_window_size);
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
        changed = true;
    }
    ImGui::EndGroup();

    // draw gate UI
    ImGui::SetCursorScreenPos(gate_pos);
    ImGui::BeginGroup();
    bool gate_changed = false;
    //draw_list->AddRectFilled(gate_pos, gate_pos + gate_size, COL_MIXING_BG, 8);
    draw_list->AddRectFilledMultiColor(gate_pos, gate_pos + gate_size, COL_MIXING_BG_HIGH, COL_MIXING_BG_MID, COL_MIXING_BG, COL_MIXING_BG_MID);
    draw_list->AddRect(gate_pos, gate_pos + gate_size, COL_MIXING_BORDER, 8);
    ImGui::Dummy({0, 2});
    ImGui::Indent(16);
    ImGui::SetWindowFontScale(1.2);
    ImGui::TextUnformatted("Gate");
    ImGui::SetWindowFontScale(1);
    ImGui::Unindent();
    ImGui::SameLine();
    gate_changed |= ImGui::ToggleButton("##audio_gate_enabe", &timeline->mAudioAttribute.bGate);
    if (ImGui::BeginChild("##audio_gate", gate_size - ImVec2(0, 32), false, setting_child_flags))
    {
        ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
        ImVec2 sub_window_size = ImGui::GetWindowSize();
        draw_separator_line(ImGui::GetCursorScreenPos(), sub_window_size);
        float scroll_y = ImGui::GetScrollY();
        ImGui::BeginDisabled(!timeline->mAudioAttribute.bGate);
        auto knob_pos = ImGui::GetCursorScreenPos();
        auto knob_offset_x = (sub_window_size.x - 240) / 4;
        ImGui::SetCursorScreenPos(knob_pos + ImVec2(knob_offset_x, 4));
        gate_changed |= ImGui::Knob("Threshold##gate", &timeline->mAudioAttribute.gate_thd, 0.f, 1.0f, NAN, 0.125f, 80, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_STEPPED_DOT, "%.3f", 10);
        ImGui::SetCursorScreenPos(knob_pos + ImVec2(knob_offset_x * 2 + 80, 4));
        gate_changed |= ImGui::Knob("Range##gate", &timeline->mAudioAttribute.gate_range, 0.f, 1.0f, NAN, 0.06125f, 80, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_STEPPED_DOT, "%.3f", 10);
        ImGui::SetCursorScreenPos(knob_pos + ImVec2(knob_offset_x * 3 + 160, 4));
        gate_changed |= ImGui::Knob("Ratio##gate", &timeline->mAudioAttribute.gate_ratio, 1.f, 9000.0f, NAN, 2.f, 80, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_STEPPED_DOT, "%.0f", 10);
        ImGui::Dummy(ImVec2(0, 4));
        draw_separator_line(ImGui::GetCursorScreenPos(), sub_window_size);
        auto knob_time_offset_x = (sub_window_size.x - 100) / 3;
        ImGui::SetCursorScreenPos(knob_pos + ImVec2(knob_time_offset_x, 148));
        gate_changed |= ImGui::Knob("Attack##gate", &timeline->mAudioAttribute.gate_attack, 0.01f, 9000.0f, NAN, 20.f, 50, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_STEPPED_DOT, "%.0fms", 10);
        ImGui::SetCursorScreenPos(knob_pos + ImVec2(knob_time_offset_x * 2 + 50, 148));
        gate_changed |= ImGui::Knob("Release##gate", &timeline->mAudioAttribute.gate_release, 0.01f, 9000.0f, NAN, 250.f, 50, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_STEPPED_DOT, "%.0fms", 10);
        ImGui::Dummy(ImVec2(0, 4));
        draw_separator_line(ImGui::GetCursorScreenPos(), sub_window_size);
        auto knob_level_offset_x = (sub_window_size.x - 160) / 3;
        ImGui::SetCursorScreenPos(knob_pos + ImVec2(knob_level_offset_x, 264));
        gate_changed |= ImGui::Knob("Make Up##gate", &timeline->mAudioAttribute.gate_makeup, 1.f, 64.0f, NAN, 1.f, 80, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_STEPPED_DOT, "%.0f", 10);
        ImGui::SetCursorScreenPos(knob_pos + ImVec2(knob_level_offset_x * 2 + 80, 264));
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
            changed = true;
        }
        ImGui::EndDisabled();
    }
    ImGui::EndChild();
    ImGui::EndGroup();

    // draw limiter UI
    ImGui::SetCursorScreenPos(limiter_pos);
    ImGui::BeginGroup();
    bool limiter_changed = false;
    //draw_list->AddRectFilled(limiter_pos, limiter_pos + limiter_size, COL_MIXING_BG, 8);
    draw_list->AddRectFilledMultiColor(limiter_pos, limiter_pos + limiter_size, COL_MIXING_BG_HIGH, COL_MIXING_BG_MID, COL_MIXING_BG, COL_MIXING_BG_MID);
    draw_list->AddRect(limiter_pos, limiter_pos + limiter_size, COL_MIXING_BORDER, 8);
    ImGui::Dummy({0, 2});
    ImGui::Indent(16);
    ImGui::SetWindowFontScale(1.2);
    ImGui::TextUnformatted("Limiter");
    ImGui::SetWindowFontScale(1);
    ImGui::Unindent();
    ImGui::SameLine();
    limiter_changed |= ImGui::ToggleButton("##audio_limiter_enabe", &timeline->mAudioAttribute.bLimiter);
    if (ImGui::BeginChild("##audio_limiter", limiter_size - ImVec2(0, 32), false, setting_child_flags))
    {
        ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
        ImVec2 sub_window_size = ImGui::GetWindowSize();
        draw_separator_line(ImGui::GetCursorScreenPos(), sub_window_size);
        float scroll_y = ImGui::GetScrollY();
        ImGui::BeginDisabled(!timeline->mAudioAttribute.bLimiter);
        auto knob_pos = ImGui::GetCursorScreenPos();
        auto knob_offset_x = (sub_window_size.x - 80) / 2;
        ImGui::SetCursorScreenPos(knob_pos + ImVec2(knob_offset_x, 4));
        limiter_changed |= ImGui::Knob("Limit", &timeline->mAudioAttribute.limit, 0.0625f, 1.0f, NAN, 1.f, 80, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_STEPPED_DOT, "%.3f", 10);
        ImGui::Dummy(ImVec2(0, 4));
        draw_separator_line(ImGui::GetCursorScreenPos(), sub_window_size);
        auto knob_time_offset_x = (sub_window_size.x - 50) / 2;
        ImGui::SetCursorScreenPos(knob_pos + ImVec2(knob_time_offset_x, 148));
        limiter_changed |= ImGui::Knob("Attack##limiter", &timeline->mAudioAttribute.limiter_attack, 0.1f, 80.0f, NAN, 5.f, 50, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_STEPPED_DOT, "%.1fms", 10);
        ImGui::Dummy(ImVec2(0, 4));
        draw_separator_line(ImGui::GetCursorScreenPos(), sub_window_size);
        auto knob_level_offset_x = (sub_window_size.x - 50) / 2;
        ImGui::SetCursorScreenPos(knob_pos + ImVec2(knob_level_offset_x, 264));
        limiter_changed |= ImGui::Knob("Release##limiter", &timeline->mAudioAttribute.limiter_release, 1.f, 8000.0f, NAN, 50.f, 50, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_STEPPED_DOT, "%.0fms", 10);
        if (limiter_changed)
        {
            auto limiterParams = amFilter->GetLimiterParams();
            limiterParams.limit = timeline->mAudioAttribute.bLimiter ? timeline->mAudioAttribute.limit : 1.0;
            limiterParams.attack = timeline->mAudioAttribute.bLimiter ? timeline->mAudioAttribute.limiter_attack : 5;
            limiterParams.release = timeline->mAudioAttribute.bLimiter ? timeline->mAudioAttribute.limiter_release : 50;
            amFilter->SetLimiterParams(&limiterParams);
            changed = true;
        }
        ImGui::EndDisabled();
    }
    ImGui::EndChild();
    ImGui::EndGroup();

    // draw compressor UI
    ImGui::SetCursorScreenPos(compressor_pos);
    ImGui::BeginGroup();
    bool compressor_changed = false;
    //draw_list->AddRectFilled(compressor_pos, compressor_pos + compressor_size, COL_MIXING_BG, 8);
    draw_list->AddRectFilledMultiColor(compressor_pos, compressor_pos + compressor_size, COL_MIXING_BG_HIGH, COL_MIXING_BG_MID, COL_MIXING_BG, COL_MIXING_BG_MID);
    draw_list->AddRect(compressor_pos, compressor_pos + compressor_size, COL_MIXING_BORDER, 8);
    ImGui::Dummy({0, 2});
    ImGui::Indent(16);
    ImGui::SetWindowFontScale(1.2);
    ImGui::TextUnformatted("Compressor");
    ImGui::SetWindowFontScale(1);
    ImGui::Unindent();
    ImGui::SameLine();
    compressor_changed |= ImGui::ToggleButton("##audio_compressor_enabe", &timeline->mAudioAttribute.bCompressor);
    if (ImGui::BeginChild("##audio_compressor", compressor_size - ImVec2(0, 32), false, setting_child_flags))
    {
        ImVec2 sub_window_pos = ImGui::GetCursorScreenPos();
        ImVec2 sub_window_size = ImGui::GetWindowSize();
        draw_separator_line(ImGui::GetCursorScreenPos(), sub_window_size);
        float scroll_y = ImGui::GetScrollY();
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
        ImGui::Dummy(ImVec2(0, 4));
        draw_separator_line(ImGui::GetCursorScreenPos(), sub_window_size);
        auto knob_time_offset_x = (sub_window_size.x - 100) / 3;
        ImGui::SetCursorScreenPos(knob_pos + ImVec2(knob_time_offset_x, 148));
        compressor_changed |= ImGui::Knob("Attack##compressor", &timeline->mAudioAttribute.compressor_attack, 0.01f, 2000.0f, NAN, 20.f, 50, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_STEPPED_DOT, "%.0fms", 10);
        ImGui::SetCursorScreenPos(knob_pos + ImVec2(knob_time_offset_x * 2 + 50, 148));
        compressor_changed |= ImGui::Knob("Release##compressor", &timeline->mAudioAttribute.compressor_release, 0.01f, 9000.0f, NAN, 250.f, 50, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_STEPPED_DOT, "%.0fms", 10);
        ImGui::Dummy(ImVec2(0, 4));
        draw_separator_line(ImGui::GetCursorScreenPos(), sub_window_size);
        auto knob_level_offset_x = (sub_window_size.x - 160) / 3;
        ImGui::SetCursorScreenPos(knob_pos + ImVec2(knob_level_offset_x, 264));
        compressor_changed |= ImGui::Knob("Make Up##compressor", &timeline->mAudioAttribute.compressor_makeup, 1.f, 64.0f, NAN, 1.f, 80, circle_color,  wiper_color, track_color, tick_color, ImGui::ImGuiKnobType::IMKNOB_STEPPED_DOT, "%.0f", 10);
        ImGui::SetCursorScreenPos(knob_pos + ImVec2(knob_level_offset_x * 2 + 80, 264));
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
            changed = true;
        }
        ImGui::EndDisabled();
    }
    ImGui::EndChild();
    ImGui::EndGroup();

    ImGui::PopStyleColor();
    if (!g_project_loading) project_changed |= changed;
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
            auto curve_index = key_point->AddCurveByDim(name, ImGui::ImCurveEdit::Smooth, color, true, ImGui::ImCurveEdit::DIM_X, _min, _max, _default);
            key_point->AddPointByDim(curve_index, ImVec2(key_point->GetMin().w, _default), ImGui::ImCurveEdit::Smooth, ImGui::ImCurveEdit::DIM_X, true);
            key_point->AddPointByDim(curve_index, ImVec2(key_point->GetMax().w, _default), ImGui::ImCurveEdit::Smooth, ImGui::ImCurveEdit::DIM_X, true);
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
                float value = key_point->GetValueByDim(index, timeline->mCurrentTime - clip->Start(), ImGui::ImCurveEdit::DIM_X);
                ImGui::BracketSquare(true); ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0, 1.0, 0.0, 1.0)); ImGui::Text("%.2f", value); ImGui::PopStyleColor();
                
                ImGui::PushItemWidth(60);
                float curve_min = key_point->GetCurveMinByDim(index, ImGui::ImCurveEdit::DIM_X);
                float curve_max = key_point->GetCurveMaxByDim(index, ImGui::ImCurveEdit::DIM_X);
                ImGui::BeginDisabled(true);
                ImGui::DragFloat("##curve_text_clip_min", &curve_min, 0.1f, -FLT_MAX, curve_max, "%.1f"); ImGui::ShowTooltipOnHover("Min");
                ImGui::SameLine(0, 8);
                ImGui::DragFloat("##curve_text_clip_max", &curve_max, 0.1f, curve_min, FLT_MAX, "%.1f"); ImGui::ShowTooltipOnHover("Max");
                ImGui::SameLine(0, 8);
                ImGui::EndDisabled();
                float curve_default = key_point->GetCurveDefaultByDim(index, ImGui::ImCurveEdit::DIM_X);
                if (ImGui::DragFloat("##curve_text_clip_default", &curve_default, 0.1f, curve_min, curve_max, "%.1f"))
                {
                    key_point->SetCurveDefaultByDim(index, curve_default, ImGui::ImCurveEdit::DIM_X);
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
                        if (ImGui::DragTimeMS("##curve_text_clip_point_time", &point.t, key_point->GetMax().w / 1000.f, key_point->GetMin().w, key_point->GetMax().w, 2))
                        {
                            key_point->EditPoint(index, p, point.val, point.type);
                            update_preview = true;
                        }
                        ImGui::EndDisabled();
                        ImGui::SameLine();
                        auto speed = fabs(curve_max - curve_min) / 500;
                        if (ImGui::DragFloat("##curve_text_clip_point_x", &point.x, speed, curve_min, curve_max, "%.2f"))
                        {
                            key_point->EditPoint(index, p, point.val, point.type);
                            update_preview = true;
                        }
                        ImGui::SameLine();
                        if (ImGui::Combo("##curve_text_clip_type", (int*)&point.type, curve_type_list, curve_type_count))
                        {
                            key_point->EditPoint(index, p, point.val, point.type);
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
    ImGui::PushItemWidth(200);
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
    ImGui::ShowTooltipOnHover("Reset");
    ImGui::ImCurveEdit::Curve text_key; text_key.m_id = clip->mID;
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
    ImGui::ShowTooltipOnHover("Reset");
    ImGui::EndDisabled();
    if (ImGui::ImCurveCheckEditKeyByDim("##add_curve_text_pos_x##text_clip_ediror", &text_key, ImGui::ImCurveEdit::DIM_X, has_curve_pos_x, "text_pos_x##text_clip_ediror",  - (float)default_size.x , 1.f, pos_x, curve_button_offset))
    {
        if (has_curve_pos_x) addCurve("OffsetH",
                ImGui::ImCurveEdit::GetDimVal(text_key.m_min, ImGui::ImCurveEdit::DIM_X),
                ImGui::ImCurveEdit::GetDimVal(text_key.m_max, ImGui::ImCurveEdit::DIM_X),
                ImGui::ImCurveEdit::GetDimVal(text_key.m_default, ImGui::ImCurveEdit::DIM_X));
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
    ImGui::ShowTooltipOnHover("Reset");
    ImGui::EndDisabled();
    if (ImGui::ImCurveCheckEditKeyByDim("##add_curve_text_pos_y##text_clip_ediror", &text_key, ImGui::ImCurveEdit::DIM_X, has_curve_pos_y, "text_pos_y##text_clip_ediror",  - (float)default_size.y , 1.f, pos_y, curve_button_offset))
    {
        if (has_curve_pos_y) addCurve("OffsetV",
                ImGui::ImCurveEdit::GetDimVal(text_key.m_min, ImGui::ImCurveEdit::DIM_X),
                ImGui::ImCurveEdit::GetDimVal(text_key.m_max, ImGui::ImCurveEdit::DIM_X),
                ImGui::ImCurveEdit::GetDimVal(text_key.m_default, ImGui::ImCurveEdit::DIM_X));
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
    ImGui::ShowTooltipOnHover("Reset");
    ImGui::EndDisabled();
    if (ImGui::ImCurveCheckEditKeyByDim("##add_curve_text_scale_x##text_clip_ediror", &text_key, ImGui::ImCurveEdit::DIM_X, has_curve_scale_x, "text_pscale_x##text_clip_ediror",  0.2f , 10.f, style.ScaleX(), curve_button_offset))
    {
        if (has_curve_scale_x) addCurve("ScaleX",
                ImGui::ImCurveEdit::GetDimVal(text_key.m_min, ImGui::ImCurveEdit::DIM_X),
                ImGui::ImCurveEdit::GetDimVal(text_key.m_max, ImGui::ImCurveEdit::DIM_X),
                ImGui::ImCurveEdit::GetDimVal(text_key.m_default, ImGui::ImCurveEdit::DIM_X));
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
    ImGui::ShowTooltipOnHover("Reset");
    ImGui::EndDisabled();
    if (ImGui::ImCurveCheckEditKeyByDim("##add_curve_text_scale_y##text_clip_ediror", &text_key, ImGui::ImCurveEdit::DIM_X, has_curve_scale_y, "text_scale_y##text_clip_ediror",  0.2f , 10.f, style.ScaleY(), curve_button_offset))
    {
        if (has_curve_scale_y) addCurve("ScaleY",
                ImGui::ImCurveEdit::GetDimVal(text_key.m_min, ImGui::ImCurveEdit::DIM_X),
                ImGui::ImCurveEdit::GetDimVal(text_key.m_max, ImGui::ImCurveEdit::DIM_X),
                ImGui::ImCurveEdit::GetDimVal(text_key.m_default, ImGui::ImCurveEdit::DIM_X));
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
    ImGui::ShowTooltipOnHover("Reset");
    ImGui::EndDisabled();
    if (ImGui::ImCurveCheckEditKeyByDim("##add_curve_text_spacing##text_clip_ediror", &text_key, ImGui::ImCurveEdit::DIM_X, has_curve_spacing, "text_spacing##text_clip_ediror",  0.5f , 5.f, style.Spacing(), curve_button_offset))
    {
        if (has_curve_spacing) addCurve("Spacing",
                ImGui::ImCurveEdit::GetDimVal(text_key.m_min, ImGui::ImCurveEdit::DIM_X),
                ImGui::ImCurveEdit::GetDimVal(text_key.m_max, ImGui::ImCurveEdit::DIM_X),
                ImGui::ImCurveEdit::GetDimVal(text_key.m_default, ImGui::ImCurveEdit::DIM_X));
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
    ImGui::ShowTooltipOnHover("Reset");
    ImGui::EndDisabled();
    if (ImGui::ImCurveCheckEditKeyByDim("##add_curve_text_anglex##text_clip_ediror", &text_key, ImGui::ImCurveEdit::DIM_X, has_curve_anglex, "text_anglex##text_clip_ediror",  0.f , 360.f, style.Angle(), curve_button_offset))
    {
        if (has_curve_anglex) addCurve("AngleX",
                ImGui::ImCurveEdit::GetDimVal(text_key.m_min, ImGui::ImCurveEdit::DIM_X),
                ImGui::ImCurveEdit::GetDimVal(text_key.m_max, ImGui::ImCurveEdit::DIM_X),
                ImGui::ImCurveEdit::GetDimVal(text_key.m_default, ImGui::ImCurveEdit::DIM_X));
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
    ImGui::ShowTooltipOnHover("Reset");
    ImGui::EndDisabled();
    if (ImGui::ImCurveCheckEditKeyByDim("##add_curve_text_angley##text_clip_ediror", &text_key, ImGui::ImCurveEdit::DIM_X, has_curve_angley, "text_angley##text_clip_ediror",  0.f , 360.f, style.Angle(), curve_button_offset))
    {
        if (has_curve_angley) addCurve("AngleY",
                ImGui::ImCurveEdit::GetDimVal(text_key.m_min, ImGui::ImCurveEdit::DIM_X),
                ImGui::ImCurveEdit::GetDimVal(text_key.m_max, ImGui::ImCurveEdit::DIM_X),
                ImGui::ImCurveEdit::GetDimVal(text_key.m_default, ImGui::ImCurveEdit::DIM_X));
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
    ImGui::ShowTooltipOnHover("Reset");
    ImGui::EndDisabled();
    if (ImGui::ImCurveCheckEditKeyByDim("##add_curve_text_anglez##text_clip_ediror", &text_key, ImGui::ImCurveEdit::DIM_X, has_curve_anglez, "text_anglez##text_clip_ediror",  0.f , 360.f, style.Angle(), curve_button_offset))
    {
        if (has_curve_anglez) addCurve("AngleZ",
                ImGui::ImCurveEdit::GetDimVal(text_key.m_min, ImGui::ImCurveEdit::DIM_X),
                ImGui::ImCurveEdit::GetDimVal(text_key.m_max, ImGui::ImCurveEdit::DIM_X),
                ImGui::ImCurveEdit::GetDimVal(text_key.m_default, ImGui::ImCurveEdit::DIM_X));
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
    ImGui::ShowTooltipOnHover("Reset");
    ImGui::EndDisabled();
    if (ImGui::ImCurveCheckEditKeyByDim("##add_curve_text_outline_width##text_clip_ediror", &text_key, ImGui::ImCurveEdit::DIM_X, has_curve_outline_width, "text_outline_width##text_clip_ediror",  0.f , 5.f, style.OutlineWidth(), curve_button_offset))
    {
        if (has_curve_outline_width) addCurve("OutlineWidth",
                ImGui::ImCurveEdit::GetDimVal(text_key.m_min, ImGui::ImCurveEdit::DIM_X),
                ImGui::ImCurveEdit::GetDimVal(text_key.m_max, ImGui::ImCurveEdit::DIM_X),
                ImGui::ImCurveEdit::GetDimVal(text_key.m_default, ImGui::ImCurveEdit::DIM_X));
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
    ImGui::ShowTooltipOnHover("Reset");
    ImGui::EndDisabled();
    if (ImGui::ImCurveCheckEditKeyByDim("##add_curve_text_shadow_depth##text_clip_ediror", &text_key, ImGui::ImCurveEdit::DIM_X, has_curve_shadow_depth, "text_shadow_depth##text_clip_ediror",  0.f , 20.f, style.ShadowDepth(), curve_button_offset))
    {
        if (has_curve_shadow_depth) addCurve("ShadowDepth",
                ImGui::ImCurveEdit::GetDimVal(text_key.m_min, ImGui::ImCurveEdit::DIM_X),
                ImGui::ImCurveEdit::GetDimVal(text_key.m_max, ImGui::ImCurveEdit::DIM_X),
                ImGui::ImCurveEdit::GetDimVal(text_key.m_default, ImGui::ImCurveEdit::DIM_X));
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
    ImGui::ShowTooltipOnHover("Reset");

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
    ImGui::ShowTooltipOnHover("Reset");
    if (ImGui::ColorEdit4("FontColor##Primary", (float*)&clip->mFontPrimaryColor, ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_AlphaBar))
    {
        clip->mClipHolder->SetPrimaryColor(clip->mFontPrimaryColor);
        update_preview = true;
    }
    ImGui::SameLine(item_width); ImGui::TextUnformatted("Font primary color");
    ImGui::SameLine(reset_button_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##primary_color_default")) { clip->mFontPrimaryColor = style.PrimaryColor().ToImVec4(); clip->mClipHolder->SetPrimaryColor(style.PrimaryColor()); update_preview = true; }
    ImGui::ShowTooltipOnHover("Reset");
    if (ImGui::ColorEdit4("FontColor##Outline", (float*)&clip->mFontOutlineColor, ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_AlphaBar))
    {
        clip->mClipHolder->SetOutlineColor(clip->mFontOutlineColor);
        update_preview = true;
    }
    ImGui::SameLine(item_width); ImGui::TextUnformatted("Font outline color");
    ImGui::SameLine(reset_button_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##outline_color_default")) { clip->mFontOutlineColor = style.OutlineColor().ToImVec4(); clip->mClipHolder->SetOutlineColor(style.OutlineColor()); update_preview = true; }
    ImGui::ShowTooltipOnHover("Reset");
    if (ImGui::ColorEdit4("FontColor##Back", (float*)&clip->mFontBackColor, ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_AlphaBar))
    {
        clip->mClipHolder->SetBackColor(clip->mFontBackColor);
        update_preview = true;
    }
    ImGui::SameLine(item_width); ImGui::TextUnformatted("Font shadow color");
    ImGui::SameLine(reset_button_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##back_color_default")) { clip->mFontBackColor = style.BackColor().ToImVec4(); clip->mClipHolder->SetBackColor(style.BackColor()); update_preview = true; }
    ImGui::ShowTooltipOnHover("Reset");
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
            auto curve_index = keyPointsPtr->AddCurveByDim(name, ImGui::ImCurveEdit::Smooth, color, true, ImGui::ImCurveEdit::DIM_X, _min, _max, _default);
            keyPointsPtr->AddPointByDim(curve_index, ImVec2(keyPointsPtr->GetMin().w, _default), ImGui::ImCurveEdit::Smooth, ImGui::ImCurveEdit::DIM_X, true);
            keyPointsPtr->AddPointByDim(curve_index, ImVec2(keyPointsPtr->GetMax().w, _default), ImGui::ImCurveEdit::Smooth, ImGui::ImCurveEdit::DIM_X, true);
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
                float value = keyPointsPtr->GetValueByDim(index, timeline->mCurrentTime, ImGui::ImCurveEdit::DIM_X);
                ImGui::BracketSquare(true); ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0, 1.0, 0.0, 1.0)); ImGui::Text("%.2f", value); ImGui::PopStyleColor();
                
                ImGui::PushItemWidth(60);
                float curve_min = keyPointsPtr->GetCurveMinByDim(index, ImGui::ImCurveEdit::DIM_X);
                float curve_max = keyPointsPtr->GetCurveMaxByDim(index, ImGui::ImCurveEdit::DIM_X);
                ImGui::BeginDisabled(true);
                ImGui::DragFloat("##curve_text_track_min", &curve_min, 0.1f, -FLT_MAX, curve_max, "%.1f"); ImGui::ShowTooltipOnHover("Min");
                ImGui::SameLine(0, 8);
                ImGui::DragFloat("##curve_text_track_max", &curve_max, 0.1f, curve_min, FLT_MAX, "%.1f"); ImGui::ShowTooltipOnHover("Max");
                ImGui::SameLine(0, 8);
                ImGui::EndDisabled();
                float curve_default = keyPointsPtr->GetCurveDefaultByDim(index, ImGui::ImCurveEdit::DIM_X);
                if (ImGui::DragFloat("##curve_text_track_default", &curve_default, 0.1f, curve_min, curve_max, "%.1f"))
                {
                    keyPointsPtr->SetCurveDefaultByDim(index, curve_default, ImGui::ImCurveEdit::DIM_X);
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
                        if (ImGui::DragTimeMS("##curve_text_track_point_time", &point.t, keyPointsPtr->GetMax().w / 1000.f, keyPointsPtr->GetMin().w, keyPointsPtr->GetMax().w, 2))
                        {
                            keyPointsPtr->EditPoint(index, p, point.val, point.type);
                            update_preview = true;
                        }
                        ImGui::EndDisabled();
                        ImGui::SameLine();
                        auto speed = fabs(curve_max - curve_min) / 500;
                        if (ImGui::DragFloat("##curve_text_track_point_x", &point.x, speed, curve_min, curve_max, "%.2f"))
                        {
                            keyPointsPtr->EditPoint(index, p, point.val, point.type);
                            update_preview = true;
                        }
                        ImGui::SameLine();
                        if (ImGui::Combo("##curve_text_track_type", (int*)&point.type, curve_type_list, curve_type_count))
                        {
                            keyPointsPtr->EditPoint(index, p, point.val, point.type);
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
    ImGui::PushItemWidth(200);
    const float reset_button_offset = size.x - 64;
    const float curve_button_offset = size.x - 36;
    auto item_width = ImGui::CalcItemWidth();
    auto& style = track->mMttReader->DefaultStyle();
    auto familyName = style.Font();
    const char* familyValue = familyName.c_str();
    ImGui::ImCurveEdit::Curve text_key; text_key.m_id = track->mID;
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
    ImGui::ShowTooltipOnHover("Reset");
    float offset_x = style.OffsetHScale();
    int curve_pos_x_index = keyPointsPtr->GetCurveIndex("OffsetH");
    bool has_curve_pos_x = curve_pos_x_index != -1;
    ImGui::BeginDisabled(has_curve_pos_x);
    if (ImGui::SliderFloat("Font position X", &offset_x, -1.f , 1.f, "%.2f"))
    {
        track->mMttReader->SetOffsetH(offset_x);
        update_preview = true;
    } ImGui::SameLine(reset_button_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##track_font_offsetx_default")) { track->mMttReader->SetOffsetH(g_media_editor_settings.FontPosOffsetX); update_preview = true; }
    ImGui::ShowTooltipOnHover("Reset");
    ImGui::EndDisabled();
    if (ImGui::ImCurveCheckEditKeyByDim("##add_curve_text_pos_x##text_track_ediror", &text_key, ImGui::ImCurveEdit::DIM_X, has_curve_pos_x, "text_pos_x##text_track_ediror",  -1.f , 1.f, g_media_editor_settings.FontPosOffsetX, curve_button_offset))
    {
        if (has_curve_pos_x) addCurve("OffsetH",
                ImGui::ImCurveEdit::GetDimVal(text_key.m_min, ImGui::ImCurveEdit::DIM_X),
                ImGui::ImCurveEdit::GetDimVal(text_key.m_max, ImGui::ImCurveEdit::DIM_X),
                ImGui::ImCurveEdit::GetDimVal(text_key.m_default, ImGui::ImCurveEdit::DIM_X));
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
    ImGui::ShowTooltipOnHover("Reset");
    ImGui::EndDisabled();
    if (ImGui::ImCurveCheckEditKeyByDim("##add_curve_text_pos_y##text_track_ediror", &text_key, ImGui::ImCurveEdit::DIM_X, has_curve_pos_y, "text_pos_y##text_track_ediror",  -1.f , 1.f, g_media_editor_settings.FontPosOffsetY, curve_button_offset))
    {
        if (has_curve_pos_y) addCurve("OffsetV",
                ImGui::ImCurveEdit::GetDimVal(text_key.m_min, ImGui::ImCurveEdit::DIM_X),
                ImGui::ImCurveEdit::GetDimVal(text_key.m_max, ImGui::ImCurveEdit::DIM_X),
                ImGui::ImCurveEdit::GetDimVal(text_key.m_default, ImGui::ImCurveEdit::DIM_X));
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
    ImGui::ShowTooltipOnHover("Reset");
    int italic = style.Italic();
    if (ImGui::Combo("Font Italic", &italic, font_italic_list, IM_ARRAYSIZE(font_italic_list)))
    {
        track->mMttReader->SetItalic(italic);
        update_preview = true;
    } ImGui::SameLine(reset_button_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##track_font_italic_default")) { track->mMttReader->SetItalic(g_media_editor_settings.FontItalic); update_preview = true; }
    ImGui::ShowTooltipOnHover("Reset");
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
    ImGui::ShowTooltipOnHover("Reset");
    ImGui::EndDisabled();
    if (ImGui::ImCurveCheckEditKeyByDim("##add_curve_text_scale_x##text_track_ediror", &text_key, ImGui::ImCurveEdit::DIM_X, has_curve_scale_x, "text_scale_x##text_track_ediror",  0.2f, 10.f, g_media_editor_settings.FontScaleX, curve_button_offset))
    {
        if (has_curve_scale_x) addCurve("ScaleX",
                ImGui::ImCurveEdit::GetDimVal(text_key.m_min, ImGui::ImCurveEdit::DIM_X),
                ImGui::ImCurveEdit::GetDimVal(text_key.m_max, ImGui::ImCurveEdit::DIM_X),
                ImGui::ImCurveEdit::GetDimVal(text_key.m_default, ImGui::ImCurveEdit::DIM_X));
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
    ImGui::ShowTooltipOnHover("Reset");
    ImGui::EndDisabled();
    if (ImGui::ImCurveCheckEditKeyByDim("##add_curve_text_scale_y##text_track_ediror", &text_key, ImGui::ImCurveEdit::DIM_X, has_curve_scale_y, "text_scale_y##text_track_ediror",  0.2f, 10.f, g_media_editor_settings.FontScaleY, curve_button_offset))
    {
        if (has_curve_scale_y) addCurve("ScaleY",
                ImGui::ImCurveEdit::GetDimVal(text_key.m_min, ImGui::ImCurveEdit::DIM_X),
                ImGui::ImCurveEdit::GetDimVal(text_key.m_max, ImGui::ImCurveEdit::DIM_X),
                ImGui::ImCurveEdit::GetDimVal(text_key.m_default, ImGui::ImCurveEdit::DIM_X));
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
    ImGui::ShowTooltipOnHover("Reset");
    ImGui::EndDisabled();
    if (ImGui::ImCurveCheckEditKeyByDim("##add_curve_text_spacing##text_track_ediror", &text_key, ImGui::ImCurveEdit::DIM_X, has_curve_spacing, "text_spacing##text_track_ediror",  0.5f, 5.f, g_media_editor_settings.FontSpacing, curve_button_offset))
    {
        if (has_curve_spacing) addCurve("Spacing",
                ImGui::ImCurveEdit::GetDimVal(text_key.m_min, ImGui::ImCurveEdit::DIM_X),
                ImGui::ImCurveEdit::GetDimVal(text_key.m_max, ImGui::ImCurveEdit::DIM_X),
                ImGui::ImCurveEdit::GetDimVal(text_key.m_default, ImGui::ImCurveEdit::DIM_X));
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
    ImGui::ShowTooltipOnHover("Reset");
    ImGui::EndDisabled();
    if (ImGui::ImCurveCheckEditKeyByDim("##add_curve_text_angle##text_track_ediror", &text_key, ImGui::ImCurveEdit::DIM_X, has_curve_angle, "text_angle##text_track_ediror",  0.f, 360.f, g_media_editor_settings.FontAngle, curve_button_offset))
    {
        if (has_curve_angle) addCurve("Angle",
                ImGui::ImCurveEdit::GetDimVal(text_key.m_min, ImGui::ImCurveEdit::DIM_X),
                ImGui::ImCurveEdit::GetDimVal(text_key.m_max, ImGui::ImCurveEdit::DIM_X),
                ImGui::ImCurveEdit::GetDimVal(text_key.m_default, ImGui::ImCurveEdit::DIM_X));
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
    ImGui::ShowTooltipOnHover("Reset");
    ImGui::EndDisabled();
    if (ImGui::ImCurveCheckEditKeyByDim("##add_curve_text_outline_width##text_track_ediror", &text_key, ImGui::ImCurveEdit::DIM_X, has_curve_outline_width, "text_outline_width##text_track_ediror",  0.f, 5.f, g_media_editor_settings.FontOutlineWidth, curve_button_offset))
    {
        if (has_curve_outline_width) addCurve("OutlineWidth",
                ImGui::ImCurveEdit::GetDimVal(text_key.m_min, ImGui::ImCurveEdit::DIM_X),
                ImGui::ImCurveEdit::GetDimVal(text_key.m_max, ImGui::ImCurveEdit::DIM_X),
                ImGui::ImCurveEdit::GetDimVal(text_key.m_default, ImGui::ImCurveEdit::DIM_X));
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
    ImGui::ShowTooltipOnHover("Reset");
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
    ImGui::ShowTooltipOnHover("Reset");
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
    ImGui::ShowTooltipOnHover("Reset");
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
    ImGui::ShowTooltipOnHover("Reset");
    ImGui::EndDisabled();
    if (ImGui::ImCurveCheckEditKeyByDim("##add_curve_text_shadow_depth##text_track_ediror", &text_key, ImGui::ImCurveEdit::DIM_X, has_curve_shadow_depth, "text_shadow_depth##text_track_ediror",  -20.f, 20.f, g_media_editor_settings.FontShadowDepth, curve_button_offset))
    {
        if (has_curve_shadow_depth) addCurve("ShadowDepth",
                ImGui::ImCurveEdit::GetDimVal(text_key.m_min, ImGui::ImCurveEdit::DIM_X),
                ImGui::ImCurveEdit::GetDimVal(text_key.m_max, ImGui::ImCurveEdit::DIM_X),
                ImGui::ImCurveEdit::GetDimVal(text_key.m_default, ImGui::ImCurveEdit::DIM_X));
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
    ImGui::ShowTooltipOnHover("Reset");
    auto outline_color = style.OutlineColor().ToImVec4();
    if (ImGui::ColorEdit4("FontColor##Outline", (float*)&outline_color, ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_AlphaBar))
    {
        track->mMttReader->SetOutlineColor(outline_color);
        update_preview = true;
    }
    ImGui::SameLine(item_width); ImGui::TextUnformatted("Font outline color");
    ImGui::SameLine(reset_button_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##track_outline_color_default")) { track->mMttReader->SetOutlineColor(g_media_editor_settings.FontOutlineColor); update_preview = true; }
    ImGui::ShowTooltipOnHover("Reset");
    auto back_color = style.BackColor().ToImVec4();
    if (ImGui::ColorEdit4("FontColor##Back", (float*)&back_color, ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_AlphaBar))
    {
        track->mMttReader->SetBackColor(back_color);
        update_preview = true;
    }
    ImGui::SameLine(item_width); ImGui::TextUnformatted("Font shadow color");
    ImGui::SameLine(reset_button_offset); if (ImGui::Button(ICON_RETURN_DEFAULT "##track_back_color_default")) { track->mMttReader->SetBackColor(g_media_editor_settings.FontBackColor); update_preview = true; }
    ImGui::ShowTooltipOnHover("Reset");
    ImGui::PopItemWidth();

    if (update_preview)
        track->mMttReader->Refresh();
    return update_preview;
}

static void ShowTextEditorWindow(ImDrawList *draw_list, ImRect title_rect, EditingTextClip* editing)
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
    bool changed = false;
    // draw page title
    ImGui::SetWindowFontScale(1.8);
    auto title_size = ImGui::CalcTextSize("Text Style");
    float str_offset = title_rect.Max.x - title_size.x - 16;
    ImGui::SetWindowFontScale(1.0);
    draw_list->AddTextComplex(ImVec2(str_offset, title_rect.Min.y), "Text Style", 1.8f, COL_TITLE_COLOR, 0.5f, COL_TITLE_OUTLINE);

    static int StyleWindowIndex = 0; 
    ImVec2 window_pos = ImGui::GetCursorScreenPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    draw_list->AddRectFilled(window_pos, window_pos + window_size, COL_DEEP_DARK);
    float preview_view_width = window_size.x * 15 / 24;
    float style_editor_width = window_size.x - preview_view_width;
    float text_keypoint_height = g_media_editor_settings.TextCurveExpanded ? 100 + 30 : 0;
    float preview_view_height = window_size.y - text_keypoint_height;
    ImVec2 text_keypoint_pos = window_pos + ImVec2(0, preview_view_height);
    ImVec2 text_keypoint_size(window_size.x - style_editor_width, text_keypoint_height);
    if (!timeline)
        return;
    bool force_update_preview = false;
    ImGuiIO &io = ImGui::GetIO();
    bool power_saving_mode = io.ConfigFlags & ImGuiConfigFlags_EnablePowerSavingMode;
    ImVec2 default_size(0, 0);
    MediaCore::SubtitleImage current_image;
    TextClip * editing_clip = dynamic_cast<TextClip*>(editing->GetClip());
    MediaTrack * editing_track = nullptr;
    if (editing_clip && !IS_TEXT(editing_clip->mType))
    {
        editing_clip = nullptr;
    }
    else if (editing_clip && editing_clip->mClipHolder)
    {
        editing_track = (MediaTrack *)editing_clip->mTrack;
        current_image = editing_clip->mClipHolder->Image(timeline->mCurrentTime-editing_clip->Start());
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
        // add text style
        if (editing_clip)
        {
            // show clip time
            auto start_time_str = std::string(ICON_CLIP_START) + " " + ImGuiHelper::MillisecToString(editing_clip->Start(), 3);
            auto end_time_str = ImGuiHelper::MillisecToString(editing_clip->End(), 3) + " " + std::string(ICON_CLIP_END);
            auto start_time_str_size = ImGui::CalcTextSize(start_time_str.c_str());
            auto end_time_str_size = ImGui::CalcTextSize(end_time_str.c_str());
            float time_str_offset = (style_window_size.x - start_time_str_size.x - end_time_str_size.x - 30) / 2;
            ImGui::SetCursorScreenPos(style_window_pos + ImVec2(time_str_offset, 20));
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
                    auto found = timeline->FindEditingItem(EDITING_CLIP, editing_clip->mID);
                    if (found != -1) timeline->mEditingItems[found]->mTooltip = value;
                    force_update_preview = true;
                    changed = true;
                }
            }
            // show style control
            bool useTrackStyle = editing_clip->mTrackStyle;
            if (ImGui::Checkbox("Using track style", &useTrackStyle))
            {
                editing_clip->EnableUsingTrackStyle(useTrackStyle);
                force_update_preview = true;
                changed = true;
            }
            ImGui::Separator();
            //static const int numTabs = sizeof(TextEditorTabNames)/sizeof(TextEditorTabNames[0]);
            ImVec2 style_view_pos = ImGui::GetCursorPos();
            ImVec2 style_view_size(style_window_size.x, window_size.y - style_view_pos.y);
            if (ImGui::BeginChild("##text_sytle_window", style_view_size - ImVec2(8, 0), false, child_flags))
            {
                ImVec2 table_size;
                if (ImGui::TabLabels(TextEditorTabNames, StyleWindowIndex, table_size, std::vector<std::string>() , false, !power_saving_mode, nullptr, nullptr, false, false, nullptr, nullptr))
                {
                }

                if (ImGui::BeginChild("##style_Window_content", style_view_size - ImVec2(16, 32), false, setting_child_flags))
                {
                    ImVec2 style_setting_window_pos = ImGui::GetCursorScreenPos();
                    ImVec2 style_setting_window_size = ImGui::GetWindowSize();
                    if (StyleWindowIndex == 0)
                    {
                        // clip style
                        bool bEnabled = editing_clip->IsInClipRange(timeline->mCurrentTime);
                        ImGui::BeginDisabled(editing_clip->mTrackStyle || !bEnabled);
                        force_update_preview |= edit_text_clip_style(draw_list, editing_clip, style_setting_window_size, default_size);
                        changed |= force_update_preview;
                        ImGui::EndDisabled();
                    }
                    else
                    {
                        // track style
                        force_update_preview |= edit_text_track_style(draw_list, editing_track, style_setting_window_size);
                        changed |= force_update_preview;
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
        ShowMediaPreviewWindow(draw_list, "Text Preview", 2.f, video_rect, editing_clip ? editing_clip->Start() : -1, editing_clip ? editing_clip->End() : -1, false, false, force_update_preview || MovingTextPos);
        // show test rect on preview view and add UI editor
        draw_list->PushClipRect(video_rect.Min, video_rect.Max);
        if (editing_clip && current_image.Valid() && editing_clip->IsInClipRange(timeline->mCurrentTime))
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
                    changed = true;
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
                float current_time = timeline->mCurrentTime - editing_clip->Start();
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
                current_time += editing_clip->Start();
                if ((int64_t)current_time != timeline->mCurrentTime) { timeline->Seek(current_time); }
                if (_changed && editing_clip->mClipHolder) { editing_clip->mClipHolder->SetKeyPoints(editing_clip->mAttributeKeyPoints); timeline->UpdatePreview(); }
                changed |= _changed;
            }
            else if (StyleWindowIndex == 1 && editing_track)
            {
                bool _changed = false;
                float current_time = timeline->mCurrentTime;
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
                if ((int64_t)current_time != timeline->mCurrentTime) { timeline->Seek(current_time); }
                if (_changed)
                {
                    timeline->UpdatePreview();
                    editing_track->mMttReader->Refresh();
                    changed = true;
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
    if (!g_project_loading) project_changed |= changed;
}
/****************************************************************************************
 * 
 * Media Analyse windows
 *
 ***************************************************************************************/
static void ShowMediaScopeSetting(int index, bool show_tooltips = true)
{
    ImGui::BeginGroup();
    const auto viewport = ImGui::GetWindowViewport();
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
            ImGui::SetNextWindowViewport(viewport->ID);
            if (ImGui::Combo("Color System", (int *)&g_media_editor_settings.CIEColorSystem, color_system_items, IM_ARRAYSIZE(color_system_items)))
            {
                cie_setting_changed = true;
            }
            ImGui::SetNextWindowViewport(viewport->ID);
            if (ImGui::Combo("Cie System", (int *)&g_media_editor_settings.CIEMode, cie_system_items, IM_ARRAYSIZE(cie_system_items)))
            {
                cie_setting_changed = true;
            }
            ImGui::SetNextWindowViewport(viewport->ID);
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
            ImGui::SetNextWindowViewport(viewport->ID);
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
                    g_media_editor_settings.HistogramScale = g_media_editor_settings.HistogramLog ? 0.1 : 0.05f;
                    need_update_scope = true;
                }
            }

            if (histogram_mat.empty())
                histogram_mat.create(MATVIEW_WIDTH, MATVIEW_HEIGHT, 4, (size_t)1, 4);
            else
                histogram_mat.fill((int8_t)0);
            if (!mat_histogram.empty())
            {

                ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
                ImGui::SetCursorScreenPos(pos);
                float height_scale = g_media_editor_settings.HistogramSplited ? g_media_editor_settings.HistogramYRGB ? 4.f : 3.f : 1.f;
                float height_offset = g_media_editor_settings.HistogramSplited ? g_media_editor_settings.HistogramYRGB ? MATVIEW_HEIGHT / 4.f : MATVIEW_HEIGHT / 3.f : 0;
                auto rmat = mat_histogram.channel(0);
                auto gmat = mat_histogram.channel(1);
                auto bmat = mat_histogram.channel(2);
                auto ymat = mat_histogram.channel(3);
                //if (!histogram_texture)
                {
                    ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(1.f, 0.f, 0.f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(1.f, 0.f, 0.f, 0.5f));
                    ImGui::PlotMat(histogram_mat, &((float *)rmat.data)[1], mat_histogram.w - 1, 0, 0, g_media_editor_settings.HistogramLog ? 10 : 1000, ImVec2(MATVIEW_WIDTH, MATVIEW_HEIGHT / height_scale), sizeof(float), true, true);
                    ImGui::PopStyleColor(2);
                    ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.f, 1.f, 0.f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.f, 1.f, 0.f, 0.5f));
                    ImGui::PlotMat(histogram_mat, ImVec2(0, height_offset), &((float *)gmat.data)[1], mat_histogram.w - 1, 0, 0, g_media_editor_settings.HistogramLog ? 10 : 1000, ImVec2(MATVIEW_WIDTH, MATVIEW_HEIGHT / height_scale), sizeof(float), true, true);
                    ImGui::PopStyleColor(2);
                    ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.f, 0.f, 1.f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.f, 0.f, 1.f, 0.5f));
                    ImGui::PlotMat(histogram_mat, ImVec2(0, height_offset * 2), &((float *)bmat.data)[1], mat_histogram.w - 1, 0, 0, g_media_editor_settings.HistogramLog ? 10 : 1000, ImVec2(MATVIEW_WIDTH, MATVIEW_HEIGHT / height_scale), sizeof(float), true, true);
                    ImGui::PopStyleColor(2);
                    if (g_media_editor_settings.HistogramYRGB)
                    {
                        ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(1.f, 1.f, 1.f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(1.f, 1.f, 1.f, 0.5f));
                        ImGui::PlotMat(histogram_mat, ImVec2(0, height_offset * 3), &((float *)ymat.data)[1], mat_histogram.w - 1, 0, 0, g_media_editor_settings.HistogramLog ? 10 : 1000, ImVec2(MATVIEW_WIDTH, MATVIEW_HEIGHT / height_scale), sizeof(float), true, true);
                        ImGui::PopStyleColor(2);
                    }
                }

                ImGui::PopStyleColor();
            }
            // draw graticule line
            draw_list->PushClipRect(scrop_rect.Min, scrop_rect.Max);
            float graticule_scale = g_media_editor_settings.HistogramSplited ? g_media_editor_settings.HistogramYRGB ? 4.0f : 3.f : 1.f;
            auto histogram_step = size.x / 10;
            auto histogram_sub_vstep = size.x / 50;
            auto histogram_vstep = size.y * g_media_editor_settings.HistogramScale * 10 / graticule_scale;
            auto histogram_seg = size.y / histogram_vstep / graticule_scale;
            if (histogram_seg <= 1.0) { histogram_vstep /= 10.f; histogram_seg *= 10.f; }
            else if (histogram_seg > 10.f) { histogram_vstep *= 10.f; histogram_seg /= 10.f; }
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
                    if (g_media_editor_settings.HistogramYRGB)
                    {
                        ImVec2 pw0 = scrop_rect.Min + ImVec2(0, size.y * 3 / graticule_scale) + ImVec2(0, (size.y / graticule_scale) - i * histogram_vstep);
                        ImVec2 pw1 = scrop_rect.Min + ImVec2(0, size.y * 3 / graticule_scale) + ImVec2(scrop_rect.Max.x, (size.y / graticule_scale) - i * histogram_vstep);
                        draw_list->AddLine(pw0, pw1, IM_COL32(128, 128, 255, 32), 1);
                    }
                }
            }
            for (int i = 0; i < 50; i++)
            {
                ImVec2 p0 = scrop_rect.Min + ImVec2(i * histogram_sub_vstep, 0);
                ImVec2 p1 = scrop_rect.Min + ImVec2(i * histogram_sub_vstep, 5);
                draw_list->AddLine(p0, p1, COL_GRATICULE_HALF, 1);
            }
            draw_list->PopClipRect();
            ImMatToTexture(histogram_mat, histogram_texture);
            if (histogram_texture) draw_list->AddImage(histogram_texture, pos, pos + size, ImVec2(0, 0), ImVec2(1, 1));
            draw_list->AddRect(scrop_rect.Min, scrop_rect.Max, COL_SLIDER_HANDLE, 8);
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
            if (!mat_video_waveform.empty())
            {
                if (mat_video_waveform.flags & IM_MAT_FLAGS_CUSTOM_UPDATED) { ImGui::ImMatToTexture(mat_video_waveform, video_waveform_texture); mat_video_waveform.flags &= ~IM_MAT_FLAGS_CUSTOM_UPDATED; }
                draw_list->AddImage(video_waveform_texture, scrop_rect.Min, scrop_rect.Max, g_media_editor_settings.WaveformMirror ? ImVec2(0, 1) : ImVec2(0, 0), g_media_editor_settings.WaveformMirror ? ImVec2(1, 0) : ImVec2(1, 1));
            }
            draw_list->AddRect(scrop_rect.Min, scrop_rect.Max, COL_SLIDER_HANDLE, 8);
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
                    ImGui::AddLineDashed(draw_list, p0, p1, COL_GRATICULE_DARK, 1, 100);
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
                if (mat_cie.flags & IM_MAT_FLAGS_CUSTOM_UPDATED) { ImGui::ImMatToTexture(mat_cie, cie_texture); mat_cie.flags &= ~IM_MAT_FLAGS_CUSTOM_UPDATED; }
                draw_list->AddImage(cie_texture, scrop_rect.Min, scrop_rect.Max, ImVec2(0, 0), ImVec2(1, 1));
            }
            draw_list->AddRect(scrop_rect.Min, scrop_rect.Max, COL_SLIDER_HANDLE, 8);
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
            ImGui::PushStyleColor(ImGuiCol_TexGlyphShadow, ImGui::ColorConvertU32ToFloat4(IM_COL32_BLACK));
            if (m_cie)
            {
                ImVec2 white_point;
                m_cie->GetWhitePoint((ImGui::ColorsSystems)g_media_editor_settings.CIEColorSystem, size.x, size.y, &white_point.x, &white_point.y);
                draw_list->AddCircle(scrop_rect.Min + white_point, 3, IM_COL32_WHITE, 0, 2);
                draw_list->AddCircle(scrop_rect.Min + white_point, 2, IM_COL32_BLACK, 0, 1);
                ImVec2 green_point_system;
                m_cie->GetGreenPoint((ImGui::ColorsSystems)g_media_editor_settings.CIEColorSystem, size.x, size.y, &green_point_system.x, &green_point_system.y);
                draw_list->AddText(scrop_rect.Min + green_point_system, IM_COL32_WHITE, color_system_items[g_media_editor_settings.CIEColorSystem]);
                ImVec2 green_point_gamuts;
                m_cie->GetGreenPoint((ImGui::ColorsSystems)g_media_editor_settings.CIEGamuts, size.x, size.y, &green_point_gamuts.x, &green_point_gamuts.y);
                draw_list->AddText(scrop_rect.Min + green_point_gamuts, IM_COL32_WHITE, color_system_items[g_media_editor_settings.CIEGamuts]);
            }
            ImGui::PopStyleColor();
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
                if (mat_vector.flags & IM_MAT_FLAGS_CUSTOM_UPDATED) { ImGui::ImMatToTexture(mat_vector, vector_texture); mat_vector.flags &= ~IM_MAT_FLAGS_CUSTOM_UPDATED; }
                draw_list->AddImage(vector_texture, scrop_rect.Min, scrop_rect.Max, ImVec2(0, 0), ImVec2(1, 1));
            }
            draw_list->AddRect(scrop_rect.Min, scrop_rect.Max, COL_SLIDER_HANDLE, 8);
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
            draw_list->AddRect(scrop_rect.Min, scrop_rect.Max, COL_SLIDER_HANDLE, 8);
            draw_list->PushClipRect(scrop_rect.Min, scrop_rect.Max);
            ImVec2 channel_view_size = ImVec2(size.x, size.y / timeline->mAudioAttribute.channel_data.size());
            ImVec2 mat_channel_view_size = ImVec2(MATVIEW_WIDTH, MATVIEW_HEIGHT / timeline->mAudioAttribute.channel_data.size());
            ImGui::SetCursorScreenPos(pos);
            if (wave_mat.empty())
                wave_mat.create(MATVIEW_WIDTH, MATVIEW_HEIGHT, 4, (size_t)1, 4);
            else
                wave_mat.fill((int8_t)0);
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
                    ImGui::PlotMat(wave_mat, ImVec2(0, mat_channel_view_size.y * i), (float *)timeline->mAudioAttribute.channel_data[i].m_wave.data, timeline->mAudioAttribute.channel_data[i].m_wave.w, 0, -1.0 / g_media_editor_settings.AudioWaveScale , 1.0 / g_media_editor_settings.AudioWaveScale, mat_channel_view_size, sizeof(float), false);
                }
                draw_list->AddRect(channel_min, channel_max, COL_SLIDER_HANDLE, 0);
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
            ImMatToTexture(wave_mat, wave_texture);
            if (wave_texture) draw_list->AddImage(wave_texture, pos, pos + size, ImVec2(0, 0), ImVec2(1, 1));
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
            draw_list->AddRect(scrop_rect.Min, scrop_rect.Max, COL_SLIDER_HANDLE, 8);
            draw_list->PushClipRect(scrop_rect.Min, scrop_rect.Max);
            if (!timeline->mAudioAttribute.m_audio_vector.empty())
            {
                if (timeline->mAudioAttribute.m_audio_vector.flags & IM_MAT_FLAGS_CUSTOM_UPDATED) 
                {
                    ImGui::ImMatToTexture(timeline->mAudioAttribute.m_audio_vector, timeline->mAudioAttribute.m_audio_vector_texture);
                    timeline->mAudioAttribute.m_audio_vector.flags &= ~IM_MAT_FLAGS_CUSTOM_UPDATED;
                }
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
            draw_list->AddRect(scrop_rect.Min, scrop_rect.Max, COL_SLIDER_HANDLE, 8);
            draw_list->PushClipRect(scrop_rect.Min, scrop_rect.Max);
            ImVec2 channel_view_size = ImVec2(size.x, size.y / timeline->mAudioAttribute.channel_data.size());
            ImVec2 mat_channel_view_size = ImVec2(MATVIEW_WIDTH, MATVIEW_HEIGHT / timeline->mAudioAttribute.channel_data.size());
            ImGui::SetCursorScreenPos(pos);
            if (fft_mat.empty())
                fft_mat.create(MATVIEW_WIDTH, MATVIEW_HEIGHT, 4, (size_t)1, 4);
            else
                fft_mat.fill((int8_t)0);
            ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(1.f, 1.f, 1.f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(1.0f, 1.0f, 1.0f, 0.5f));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            for (int i = 0; i < timeline->mAudioAttribute.channel_data.size(); i++)
            {
                ImVec2 channel_min = pos + ImVec2(0, channel_view_size.y * i);
                ImVec2 channel_max = pos + ImVec2(channel_view_size.x, channel_view_size.y * i);
                // draw graticule line
                ImVec2 p1 = ImVec2(0, mat_channel_view_size.y * i + mat_channel_view_size.y);
                auto grid_number = floor(10 / g_media_editor_settings.AudioFFTScale);
                auto grid_height = mat_channel_view_size.y / grid_number;
                if (grid_number > 20) grid_number = 20;
                for (int x = 0; x < grid_number; x++)
                {
                    ImVec2 gp1 = p1 - ImVec2(0, grid_height * x);
                    ImVec2 gp2 = gp1 + ImVec2(mat_channel_view_size.x, 0);
                    fft_mat.draw_line(Vec2Point(gp1), Vec2Point(gp2), U32Color(COL_GRAY_GRATICULE));
                }
                auto vgrid_number = mat_channel_view_size.x / grid_height;
                for (int x = 0; x < vgrid_number; x++)
                {
                    ImVec2 gp1 = p1 + ImVec2(grid_height * x, 0);
                    ImVec2 gp2 = gp1 - ImVec2(0, grid_height * (grid_number - 1));
                    fft_mat.draw_line(Vec2Point(gp1), Vec2Point(gp2), U32Color(COL_GRAY_GRATICULE));
                }
                if (!timeline->mAudioAttribute.channel_data[i].m_fft.empty())
                {
                    ImGui::PlotMat(fft_mat, ImVec2(0, mat_channel_view_size.y * i), (float *)timeline->mAudioAttribute.channel_data[i].m_fft.data, timeline->mAudioAttribute.channel_data[i].m_fft.w, 0, 0.0, 1.0 / g_media_editor_settings.AudioFFTScale, mat_channel_view_size, sizeof(float), true);
                }
                draw_list->AddRect(channel_min, channel_max, COL_SLIDER_HANDLE, 0);
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);
            ImMatToTexture(fft_mat, fft_texture);
            if (fft_texture) draw_list->AddImage(fft_texture, pos, pos + size, ImVec2(0, 0), ImVec2(1, 1));
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
            draw_list->AddRect(scrop_rect.Min, scrop_rect.Max, COL_SLIDER_HANDLE, 8);
            draw_list->PushClipRect(scrop_rect.Min, scrop_rect.Max);
            ImVec2 channel_view_size = ImVec2(size.x, size.y / timeline->mAudioAttribute.channel_data.size());
            ImVec2 mat_channel_view_size = ImVec2(MATVIEW_WIDTH, MATVIEW_HEIGHT / timeline->mAudioAttribute.channel_data.size());
            ImGui::SetCursorScreenPos(pos);
            if (db_mat.empty())
                db_mat.create(MATVIEW_WIDTH, MATVIEW_HEIGHT, 4, (size_t)1, 4);
            else
                db_mat.fill((int8_t)0);
            ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(1.f, 1.f, 1.f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(1.0f, 1.0f, 1.0f, 0.5f));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            for (int i = 0; i < timeline->mAudioAttribute.channel_data.size(); i++)
            {
                ImVec2 channel_min = pos + ImVec2(0, channel_view_size.y * i);
                ImVec2 channel_max = pos + ImVec2(channel_view_size.x, channel_view_size.y * i);
                // draw graticule line
                ImVec2 p1 = ImVec2(0, mat_channel_view_size.y * i + mat_channel_view_size.y);
                auto grid_number = floor(10 / g_media_editor_settings.AudioDBScale);
                auto grid_height = mat_channel_view_size.y / grid_number;
                if (grid_number > 20) grid_number = 20;
                for (int x = 0; x < grid_number; x++)
                {
                    ImVec2 gp1 = p1 - ImVec2(0, grid_height * x);
                    ImVec2 gp2 = gp1 + ImVec2(mat_channel_view_size.x, 0);
                    db_mat.draw_line(Vec2Point(gp1), Vec2Point(gp2), U32Color(COL_GRAY_GRATICULE));
                }
                auto vgrid_number = mat_channel_view_size.x / grid_height;
                for (int x = 0; x < vgrid_number; x++)
                {
                    ImVec2 gp1 = p1 + ImVec2(grid_height * x, 0);
                    ImVec2 gp2 = gp1 - ImVec2(0, grid_height * (grid_number - 1));
                    db_mat.draw_line(Vec2Point(gp1), Vec2Point(gp2), U32Color(COL_GRAY_GRATICULE));
                }
                if (!timeline->mAudioAttribute.channel_data[i].m_db.empty())
                {
                    ImGui::ImMat db_mat_inv = timeline->mAudioAttribute.channel_data[i].m_db.clone();
                    db_mat_inv += 90.f;
                    ImGui::PlotMat(db_mat, ImVec2(0, mat_channel_view_size.y * i), (float *)db_mat_inv.data, db_mat_inv.w, 0, 0.f, 90.f / g_media_editor_settings.AudioDBScale, mat_channel_view_size, sizeof(float), false);
                }
                draw_list->AddRect(channel_min, channel_max, COL_SLIDER_HANDLE, 0);
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);
            ImMatToTexture(db_mat, db_texture);
            if (db_texture) draw_list->AddImage(db_texture, pos, pos + size, ImVec2(0, 0), ImVec2(1, 1));
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
            draw_list->AddRect(scrop_rect.Min, scrop_rect.Max, COL_SLIDER_HANDLE, 8);
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
            draw_list->AddRect(scrop_rect.Min, scrop_rect.Max, COL_SLIDER_HANDLE, 8);
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
                    if (timeline->mAudioAttribute.channel_data[i].m_Spectrogram.flags & IM_MAT_FLAGS_CUSTOM_UPDATED)
                    {
                        ImGui::ImMatToTexture(timeline->mAudioAttribute.channel_data[i].m_Spectrogram, timeline->mAudioAttribute.channel_data[i].texture_spectrogram);
                        timeline->mAudioAttribute.channel_data[i].m_Spectrogram.flags &= ~IM_MAT_FLAGS_CUSTOM_UPDATED;
                    }
                    ImGui::AddImageRotate(draw_list, timeline->mAudioAttribute.channel_data[i].texture_spectrogram, texture_pos, ImVec2(channel_view_size.y, channel_view_size.x), -90.0);
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

static void ShowMediaAnalyseWindow(TimeLine *timeline, bool *expand, bool spread)
{
    ImGuiIO &io = ImGui::GetIO();
    static bool last_spread = false;
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

    if (last_spread != spread)
    {
        if (expand && timeline)
            timeline->UpdatePreview();
        last_spread = spread;
        need_update_scope = true;
    }

    ImGui::SetCursorScreenPos(window_pos + ImVec2(window_size.x - 32, 0));
    if (ImGui::Button(ICON_EXPANMD "##scope_expand"))
    {
        g_media_editor_settings.SeparateScope = true;
        need_update_scope = true;
    }

    if (expand && !*expand)
    {
        draw_list->AddText(nullptr, 24.f, window_pos, IM_COL32_WHITE, ScopeWindowTabIcon[g_media_editor_settings.ScopeWindowIndex]);
        if (spread)
        {
            draw_list->AddText(nullptr, 24.f, window_pos + ImVec2(window_size.x / 2, 0), IM_COL32_WHITE, ScopeWindowTabIcon[g_media_editor_settings.ScopeWindowExpandIndex]);
        }
    }
    else
    {
        
        ImVec2 main_scope_pos = window_pos;
        if (!spread)
        {
            main_scope_pos += ImVec2((window_size.x - scope_view_size.x) / 2, 0);
        }

        draw_list->AddText(nullptr, 24.f, main_scope_pos, IM_COL32_WHITE, ScopeWindowTabIcon[g_media_editor_settings.ScopeWindowIndex]);
        
        ImGui::SetCursorScreenPos(main_scope_pos + ImVec2(32, 0));

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

        if (spread)
        {
            ImVec2 expand_scope_pos = window_pos + ImVec2(scope_view_size.x + 16, 0);
            draw_list->AddText(nullptr, 24.f, expand_scope_pos, IM_COL32_WHITE, ScopeWindowTabIcon[g_media_editor_settings.ScopeWindowExpandIndex]);
            ImGui::SetCursorScreenPos(expand_scope_pos + ImVec2(32, 0));
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
    }
    ImGui::PopStyleColor(3);
}

static void ShowMediaAnalyseWindow(TimeLine *timeline)
{
    ImGuiIO &io = ImGui::GetIO();
    auto platform_io = ImGui::GetPlatformIO();
    bool multiviewport = io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable;
    ImVec2 pos = ImVec2(100, 100);
    ImVec2 size = ImVec2(1600, 800);
    static ImVec2 full_size = size;
    static ImVec2 full_pos = pos;
    static bool is_full_size = false;
    static bool change_to_full_size = false;
    if (MonitorIndexChanged)
    {
        if (multiviewport && MonitorIndexScope != -1 && MonitorIndexScope != main_mon && MonitorIndexScope < platform_io.Monitors.Size)
        {
            auto mon = platform_io.Monitors[MonitorIndexScope];
            full_pos = mon.WorkPos;
            full_size = mon.WorkSize;
            is_full_size = true;
            change_to_full_size = true;
        }
        else
        {
            is_full_size = false;
        }
        ImGui::SetNextWindowPos(pos);
        MonitorIndexChanged = false;
        need_update_scope = true;
    }
    if (change_to_full_size)
    {
        ImGui::SetNextWindowSize(size, ImGuiCond_None);
        change_to_full_size = false;
    }
    else
    {
        ImGui::SetNextWindowSize(is_full_size ? full_size : size, ImGuiCond_None);
    }
    if (is_full_size) ImGui::SetNextWindowPos(full_pos);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;
    if (is_full_size) flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove;
    if (!ImGui::Begin("ScopeView", nullptr, flags))
    {
        ImGui::End();
        return;
    }
    ImVec2 window_pos = ImGui::GetCursorScreenPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    float scope_gap = is_full_size ? 100 : 48;
    float scope_size = is_full_size ? (window_size.x - 32) / 5 - scope_gap : 256;
    float col_second = window_size.y / 2 + 20;
    ImVec2 scope_view_size = ImVec2(scope_size, scope_size);
    // add left tool bar
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5, 0.5, 0.5, 0.5));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2, 0.2, 0.2, 1.0));

    if (ImGui::Button(ICON_DRAWING_PIN "##unexpand_scope"))
    {
        g_media_editor_settings.SeparateScope = false;
        MonitorIndexScope = -1;
    }
    if (multiviewport)
    {
        auto pos = ImGui::GetCursorScreenPos();
        std::vector<int> disabled_monitor;
        if (MainWindowIndex == MAIN_PAGE_PREVIEW)
            disabled_monitor.push_back(MonitorIndexPreviewVideo);
#if 0 // TODO::Dicky editing item monitors support
        else if (MainWindowIndex == MAIN_PAGE_VIDEO && VideoEditorWindowIndex == VIDEO_PAGE_FILTER)
        {
            disabled_monitor.push_back(MonitorIndexVideoFilterOrg);
            disabled_monitor.push_back(MonitorIndexVideoFiltered);
        }
#endif
        MonitorButton("##scope_monitor", pos, MonitorIndexScope, disabled_monitor);
    }

    // add scope UI layout
    ImVec2 view_pos = window_pos + ImVec2(32, 0);
    ImVec2 view_size = window_size - ImVec2(32, 0);
    ImDrawList *draw_list = ImGui::GetWindowDrawList();

    // add video scope + audio wave
    for (int i = 0; i <= 4; i++)
    {
        ShowMediaScopeView(i, view_pos + ImVec2(i * (scope_size + scope_gap), 0), scope_view_size);
        ImGui::SetCursorScreenPos(view_pos + ImVec2(i * (scope_size + scope_gap), scope_size));
        ShowMediaScopeSetting(i, false);
        ImGui::SetWindowFontScale(2.0);
        ImGui::AddTextVertical(draw_list, view_pos + ImVec2(i * (scope_size + scope_gap) + scope_size, 0), IM_COL32_WHITE, ScopeWindowTabNames[i]);
        ImGui::SetWindowFontScale(1.0);
    }

    // add audio scope
    for (int i = 5; i < 10; i++)
    {
        ShowMediaScopeView(i, view_pos + ImVec2((i - 5) * (scope_size + scope_gap), col_second), scope_view_size);
        ImGui::SetCursorScreenPos(view_pos + ImVec2((i - 5) * (scope_size + scope_gap), col_second + scope_size));
        ShowMediaScopeSetting(i, false);
        ImGui::SetWindowFontScale(2.0);
        ImGui::AddTextVertical(draw_list, view_pos + ImVec2((i - 5) * (scope_size + scope_gap) + scope_size, col_second), IM_COL32_WHITE, ScopeWindowTabNames[i]);
        ImGui::SetWindowFontScale(1.0);
    }

    ImGui::PopStyleColor(3);

    ImGui::End();
}

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
        if (!setting) return;
        int val_int = 0;
        int64_t val_int64 = 0;
        float val_float = 0;
        char val_path[1024] = {0};
        ImVec4 val_vec4 = {0, 0, 0, 0};
        if (sscanf(line, "ProjectPath=%[^|\n]", val_path) == 1) { setting->project_path = std::string(val_path); }
        else if (sscanf(line, "UILanguage=%[^|\n]", val_path) == 1) { setting->UILanguage = std::string(val_path); }
        else if (sscanf(line, "BottomViewExpanded=%d", &val_int) == 1) { setting->BottomViewExpanded = val_int == 1; }
        else if (sscanf(line, "VideoFilterCurveExpanded=%d", &val_int) == 1) { setting->VideoFilterCurveExpanded = val_int == 1; }
        else if (sscanf(line, "VideoTransitionCurveExpanded=%d", &val_int) == 1) { setting->VideoTransitionCurveExpanded = val_int == 1; }
        else if (sscanf(line, "AudioFilterCurveExpanded=%d", &val_int) == 1) { setting->AudioFilterCurveExpanded = val_int == 1; }
        else if (sscanf(line, "AudioTransitionCurveExpanded=%d", &val_int) == 1) { setting->AudioTransitionCurveExpanded = val_int == 1; }
        else if (sscanf(line, "TextCurveExpanded=%d", &val_int) == 1) { setting->TextCurveExpanded = val_int == 1; }
        else if (sscanf(line, "TopViewHeight=%f", &val_float) == 1) { setting->TopViewHeight = isnan(val_float) ?  0.6f : val_float; }
        else if (sscanf(line, "BottomViewHeight=%f", &val_float) == 1) { setting->BottomViewHeight = isnan(val_float) ? 0.4f : val_float; }
        else if (sscanf(line, "OldBottomViewHeight=%f", &val_float) == 1) { setting->OldBottomViewHeight = val_float; }
        else if (sscanf(line, "ShowMeters=%d", &val_int) == 1) { setting->showMeters = val_int == 1; }
        else if (sscanf(line, "PowerSaving=%d", &val_int) == 1) { setting->powerSaving = val_int == 1; }
        else if (sscanf(line, "MediaBankView=%d", &val_int) == 1) { setting->MediaBankViewType = val_int; }
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
        else if (sscanf(line, "VideoClipTimelineHeight=%f", &val_float) == 1) { setting->video_clip_timeline_height = val_float; }
        else if (sscanf(line, "VideoClipTimelineWidth=%f", &val_float) == 1) { setting->video_clip_timeline_width = val_float; }
        else if (sscanf(line, "AudioClipTimelineHeight=%f", &val_float) == 1) { setting->audio_clip_timeline_height = val_float; }
        else if (sscanf(line, "AudioClipTimelineWidth=%f", &val_float) == 1) { setting->audio_clip_timeline_width = val_float; }
        else if (sscanf(line, "ExpandScope=%d", &val_int) == 1) { setting->ExpandScope = val_int == 1; }
        else if (sscanf(line, "SeparateScope=%d", &val_int) == 1) { setting->SeparateScope = val_int == 1; }
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
        else if (sscanf(line, "ImageSequSetting_png=%d", &val_int) == 1) { setting->image_sequence.bPng = val_int == 1; }
        else if (sscanf(line, "ImageSequSetting_jpg=%d", &val_int) == 1) { setting->image_sequence.bJpg = val_int == 1; }
        else if (sscanf(line, "ImageSequSetting_bmp=%d", &val_int) == 1) { setting->image_sequence.bBmp = val_int == 1; }
        else if (sscanf(line, "ImageSequSetting_tif=%d", &val_int) == 1) { setting->image_sequence.bTif = val_int == 1; }
        else if (sscanf(line, "ImageSequSetting_tga=%d", &val_int) == 1) { setting->image_sequence.bTga = val_int == 1; }
        else if (sscanf(line, "ImageSequSetting_webp=%d", &val_int) == 1) { setting->image_sequence.bWebP = val_int == 1; }
        else if (sscanf(line, "ImageSequSetting_rate_den=%d", &val_int) == 1) { setting->image_sequence.frame_rate.den = val_int; }
        else if (sscanf(line, "ImageSequSetting_rate_num=%d", &val_int) == 1) { setting->image_sequence.frame_rate.num = val_int; }
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
        out_buf->appendf("VideoTransitionCurveExpanded=%d\n", g_media_editor_settings.VideoTransitionCurveExpanded ? 1 : 0);
        out_buf->appendf("AudioFilterCurveExpanded=%d\n", g_media_editor_settings.AudioFilterCurveExpanded ? 1 : 0);
        out_buf->appendf("AudioTransitionCurveExpanded=%d\n", g_media_editor_settings.AudioTransitionCurveExpanded ? 1 : 0);
        out_buf->appendf("TextCurveExpanded=%d\n", g_media_editor_settings.TextCurveExpanded ? 1 : 0);
        out_buf->appendf("TopViewHeight=%f\n", g_media_editor_settings.TopViewHeight);
        out_buf->appendf("BottomViewHeight=%f\n", g_media_editor_settings.BottomViewHeight);
        out_buf->appendf("OldBottomViewHeight=%f\n", g_media_editor_settings.OldBottomViewHeight);
        out_buf->appendf("ShowMeters=%d\n", g_media_editor_settings.showMeters ? 1 : 0);
        out_buf->appendf("PowerSaving=%d\n", g_media_editor_settings.powerSaving ? 1 : 0);
        out_buf->appendf("MediaBankView=%d\n", g_media_editor_settings.MediaBankViewType);
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
        out_buf->appendf("VideoClipTimelineHeight=%f\n", g_media_editor_settings.video_clip_timeline_height);
        out_buf->appendf("VideoClipTimelineWidth=%f\n", g_media_editor_settings.video_clip_timeline_width);
        out_buf->appendf("AudioClipTimelineHeight=%f\n", g_media_editor_settings.audio_clip_timeline_height);
        out_buf->appendf("AudioClipTimelineWidth=%f\n", g_media_editor_settings.audio_clip_timeline_width);
        out_buf->appendf("ExpandScope=%d\n", g_media_editor_settings.ExpandScope ? 1 : 0);
        out_buf->appendf("SeparateScope=%d\n", g_media_editor_settings.SeparateScope ? 1 : 0);
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
        out_buf->appendf("ImageSequSetting_png=%d\n", g_media_editor_settings.image_sequence.bPng ? 1 : 0);
        out_buf->appendf("ImageSequSetting_jpg=%d\n", g_media_editor_settings.image_sequence.bJpg ? 1 : 0);
        out_buf->appendf("ImageSequSetting_bmp=%d\n", g_media_editor_settings.image_sequence.bBmp ? 1 : 0);
        out_buf->appendf("ImageSequSetting_tif=%d\n", g_media_editor_settings.image_sequence.bTif ? 1 : 0);
        out_buf->appendf("ImageSequSetting_tga=%d\n", g_media_editor_settings.image_sequence.bTga ? 1 : 0);
        out_buf->appendf("ImageSequSetting_webp=%d\n", g_media_editor_settings.image_sequence.bWebP ? 1 : 0);
        out_buf->appendf("ImageSequSetting_rate_den=%d\n", g_media_editor_settings.image_sequence.frame_rate.den);
        out_buf->appendf("ImageSequSetting_rate_num=%d\n", g_media_editor_settings.image_sequence.frame_rate.num);
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
            g_loading_project_thread = new std::thread(LoadProjectThread, g_media_editor_settings.project_path, set_context_in_splash);
        }
        else
        {
            g_project_loading = false;
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
        if (dialog) dialog->DeserializeBookmarks(line);
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

    ImGuiSettingsHandler bookmark_ini_handler_media_bank;
    bookmark_ini_handler_media_bank.TypeName = "BookMark_Bank";
    bookmark_ini_handler_media_bank.TypeHash = ImHashStr("BookMark_Bank");
    bookmark_ini_handler_media_bank.ReadLineFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler, void* entry, const char* line) -> void
    {
        embedded_filedialog.DeserializeBookmarks(line);
    };
    bookmark_ini_handler_media_bank.WriteAllFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* out_buf)
    {
        ImGuiContext& g = *ctx;
        out_buf->reserve(out_buf->size() + g.SettingsWindows.size() * 6); // ballpark reserve
        auto bookmark = embedded_filedialog.SerializeBookmarks();
        out_buf->appendf("[%s][##%s]\n", handler->TypeName, handler->TypeName);
        out_buf->appendf("%s\n", bookmark.c_str());
        out_buf->append("\n");
    };
    ctx->SettingsHandlers.push_back(bookmark_ini_handler_media_bank);
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
    // MediaCore::MultiTrackVideoReader::GetLogger()->SetShowLevels(Logger::DEBUG);
    // MediaCore::MultiTrackAudioReader::GetLogger()->SetShowLevels(Logger::DEBUG);
    // MediaCore::MediaReader::GetLogger()->SetShowLevels(Logger::DEBUG);
    // MediaCore::Snapshot::GetLogger()->SetShowLevels(Logger::DEBUG);
    // MediaCore::MediaEncoder::GetLogger()->SetShowLevels(Logger::DEBUG);
    // MediaCore::Overview::GetLogger()->SetShowLevels(Logger::DEBUG);
    // GetSubtitleTrackLogger()->SetShowLevels(Logger::DEBUG);
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
    g_project_loading = true;
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
    if (histogram_texture) { ImGui::ImDestroyTexture(histogram_texture); histogram_texture = nullptr; }
    if (video_waveform_texture) { ImGui::ImDestroyTexture(video_waveform_texture); video_waveform_texture = nullptr; }
    if (cie_texture) { ImGui::ImDestroyTexture(cie_texture); cie_texture = nullptr; }
    if (vector_texture) { ImGui::ImDestroyTexture(vector_texture); vector_texture = nullptr; }
    if (wave_texture) { ImGui::ImDestroyTexture(wave_texture); wave_texture = nullptr; }
    if (fft_texture) { ImGui::ImDestroyTexture(fft_texture); fft_texture = nullptr; }
    if (db_texture) { ImGui::ImDestroyTexture(db_texture); db_texture = nullptr; }
    if (logo_texture) { ImGui::ImDestroyTexture(logo_texture); logo_texture = nullptr; }
    if (codewin_texture) { ImGui::ImDestroyTexture(codewin_texture); codewin_texture = nullptr; }

    ImPlot::DestroyContext();
    MediaCore::ReleaseSubtitleLibrary();
    RenderUtils::TextureManager::ReleaseDefaultInstance();
}

/****************************************************************************************
 * 
 * Editing Item window
 *
 ***************************************************************************************/
static void ShowEditingItemWindow(ImDrawList *draw_list, ImRect title_rect)
{   
    if (timeline->mSelectedItem == -1 || timeline->mEditingItems.empty() || timeline->mSelectedItem >= timeline->mEditingItems.size())
        return;
    auto item = timeline->mEditingItems[timeline->mSelectedItem];
    if (!item)
        return;

    if (IS_VIDEO(item->mMediaType))
    {
        if (item->mEditorType == EDITING_CLIP)
        {
            ShowVideoClipWindow(draw_list, title_rect, (EditingVideoClip*)item->mEditingClip);
        }
        else if (item->mEditorType == EDITING_TRANSITION)
        {
            ShowVideoTransitionWindow(draw_list, title_rect, (EditingVideoOverlap*)item->mEditingOverlap);
        }
    }
    else if (IS_AUDIO(item->mMediaType))
    {
        if (item->mEditorType == EDITING_CLIP)
        {
            ShowAudioClipWindow(draw_list, title_rect, (EditingAudioClip*)item->mEditingClip);
        }
        else if (item->mEditorType == EDITING_TRANSITION)
        {
            ShowAudioTransitionWindow(draw_list, title_rect, (EditingAudioOverlap*)item->mEditingOverlap);
        }
    }
    else if (IS_TEXT(item->mMediaType))
    {
        if (item->mEditorType == EDITING_CLIP)
        {
            ShowTextEditorWindow(draw_list, title_rect, (EditingTextClip*)item->mEditingClip);
        }
    }
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
#if UI_PERFORMANCE_ANALYSIS
    MediaCore::AutoSection _as("MEFrm");
#endif
    //static bool first_display = true;
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
    static std::string project_name = g_media_editor_settings.project_path.empty() ? "Untitled" : ImGuiHelper::path_filename_prefix(g_media_editor_settings.project_path);
    const ImGuiFileDialogFlags fflags = ImGuiFileDialogFlags_ShowBookmark | ImGuiFileDialogFlags_CaseInsensitiveExtention | ImGuiFileDialogFlags_DisableCreateDirectoryButton | ImGuiFileDialogFlags_Modal;
    const ImGuiFileDialogFlags pflags = ImGuiFileDialogFlags_ShowBookmark | ImGuiFileDialogFlags_CaseInsensitiveExtention | ImGuiFileDialogFlags_ConfirmOverwrite | ImGuiFileDialogFlags_Modal;
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    bool power_saving_mode = io.ConfigFlags & ImGuiConfigFlags_EnablePowerSavingMode;
    bool multiviewport = io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable;
    const ImGuiViewportP* viewport = (ImGuiViewportP*) ImGui::GetMainViewport();
    main_mon = viewport->PlatformMonitor;
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
    //if (first_display) { ImGui::SetNextWindowFocus(); first_display = false; }
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 12.0f);
    if (power_saving_mode) UpdateBreathing();
    if (g_project_loading || 
        (timeline && timeline->mIsPreviewPlaying) || 
        (timeline && timeline->mMediaPlayer && timeline->mMediaPlayer->IsPlaying()))
    {
        ImGui::UpdateData();
    }
    ImGui::Begin("Main Editor", nullptr, flags);
#ifdef DEBUG_IMGUI
    if (show_debug) ImGui::ShowMetricsWindow(&show_debug);
#endif

    if (!logo_texture && !icon_file.empty()) logo_texture = ImGui::ImLoadTexture(icon_file.c_str());
    if (!codewin_texture) codewin_texture = ImGui::ImCreateTexture(codewin::codewin_pixels, codewin::codewin_width, codewin::codewin_height);
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
            ImGui::Text("%s ", project_name.c_str()); ImGui::SameLine();
            ImGui::TextUnformatted("Project Loading...");
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
                bool needReloadProject = false;
                if (timeline->mHardwareCodec != g_media_editor_settings.HardwareCodec)
                {
                    timeline->mHardwareCodec = g_media_editor_settings.HardwareCodec;
                    MediaCore::VideoClip::USE_HWACCEL = g_media_editor_settings.HardwareCodec;
                    needReloadProject = true;
                }
                timeline->mMaxCachedVideoFrame = g_media_editor_settings.VideoFrameCacheSize > 0 ? g_media_editor_settings.VideoFrameCacheSize : MAX_VIDEO_CACHE_FRAMES;
                timeline->mShowHelpTooltips = g_media_editor_settings.ShowHelpTooltips;
                timeline->mFontName = g_media_editor_settings.FontName;

                MediaCore::SharedSettings::Holder hNewSettings = MediaCore::SharedSettings::CreateInstance();
                hNewSettings->SetVideoOutWidth(g_media_editor_settings.VideoWidth);
                hNewSettings->SetVideoOutHeight(g_media_editor_settings.VideoHeight);
                hNewSettings->SetVideoOutFrameRate(g_media_editor_settings.VideoFrameRate);
                hNewSettings->SetVideoOutColorFormat(timeline->mhPreviewSettings->VideoOutColorFormat());
                hNewSettings->SetVideoOutDataType(timeline->mhPreviewSettings->VideoOutDataType());
                hNewSettings->SetAudioOutChannels(g_media_editor_settings.AudioChannels);
                hNewSettings->SetAudioOutSampleRate(g_media_editor_settings.AudioSampleRate);
                auto pcmFormat = (MediaCore::AudioRender::PcmFormat)g_media_editor_settings.AudioFormat;;
                auto pcmDataType = MatUtils::PcmFormat2ImDataType(pcmFormat);
                if (pcmDataType == IM_DT_UNDEFINED)
                    throw std::runtime_error("UNSUPPORTED audio render pcm format!");
                hNewSettings->SetAudioOutDataType(pcmDataType);

                if (hNewSettings->VideoOutFrameRate() != timeline->mhMediaSettings->VideoOutFrameRate())
                    needReloadProject = true;
                if (needReloadProject)
                {
                    timeline->mhMediaSettings->SyncVideoSettingsFrom(hNewSettings.get());
                    timeline->mPreviewScale = g_media_editor_settings.PreviewScale;
                    timeline->mhMediaSettings->SyncAudioSettingsFrom(hNewSettings.get());
                    timeline->mAudioRenderFormat = pcmFormat;
                    OpenProject(g_media_editor_settings.project_path);
                }
                else
                {
                    timeline->UpdateVideoSettings(hNewSettings, g_media_editor_settings.PreviewScale);
                    timeline->UpdateAudioSettings(hNewSettings, pcmFormat);
                }
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

    // title bar, show project info
    ImVec2 title_pos = window_pos;
    ImVec2 title_size = ImVec2(window_size.x, 32);
    ImGui::GetWindowDrawList()->AddRectFilled(title_pos, title_pos + title_size, COL_DARK_TWO);
    std::string title_str = "Project:" + project_name;
    ImGui::SetWindowFontScale(1.2);
    ImGui::SetCursorScreenPos(title_pos + ImVec2(16, 4));
    if (logo_texture) { ImGui::Image(logo_texture, ImVec2(24, 24)); ImGui::SameLine(); }
    else if (codewin_texture) { ImGui::Image(codewin_texture, ImVec2(24, 24)); ImGui::SameLine(); }
    ImGui::TextUnformatted("Project:"); ImGui::SameLine();
    ImGui::Text("%s", project_name.c_str());
    if (project_changed) { ImGui::SameLine(); ImGui::TextUnformatted("*"); }
    ImGui::SetWindowFontScale(1.0);
    // power saving mode check
    if (g_media_editor_settings.powerSaving)
    {
        io.ConfigFlags |= ImGuiConfigFlags_EnablePowerSavingMode;
    }
    else
    {
        io.ConfigFlags &= ~ImGuiConfigFlags_EnablePowerSavingMode;
    }
    // show meters
    if (g_media_editor_settings.showMeters)
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << ImGui::GetIO().DeltaTime * 1000.f << "ms/frame ";
        oss << "(" << ImGui::GetIO().FrameCountSinceLastUpdate << "/" << ImGui::GetIO().FrameCountSinceLastEvent <<  ") ";
        oss << ImGui::GetIO().Framerate << "FPS";
#ifdef SHOW_GPU_INFO
        int device_count = ImGui::get_gpu_count();
        for (int i = 0; i < device_count; i++)
        {
            ImGui::VulkanDevice* vkdev = ImGui::get_gpu_device(i);
            oss << " GPU[" << i << "]";
            std::string device_name = vkdev->info.device_name();
            oss << ":" << device_name;
            uint32_t gpu_memory_usage = vkdev->get_heap_usage();
            oss << " VRAM(" << gpu_memory_usage << "MB";
            uint32_t gpu_memory_budget = vkdev->get_heap_budget();
            oss << "/" << gpu_memory_budget << "MB)";
        }
#endif
        oss << " T:" << ImGui::ImGetTextureCount();
        oss << " V:" << io.MetricsRenderVertices;
        oss << " I:" << io.MetricsRenderIndices;
        std::string meters = oss.str();
        auto str_size = ImGui::CalcTextSize(meters.c_str());
        auto spos = title_pos + ImVec2(title_size.x - str_size.x - 8, 8);
        ImGui::GetWindowDrawList()->AddText(spos, IM_COL32(255,255,255,128), meters.c_str());
    }

    // main window
    ImVec2 main_pos = window_pos + ImVec2(4, 32);
    ImVec2 main_size(window_size.x, main_height + 4); // if we need add toolbar, should add here
    ImGui::SetNextWindowPos(main_pos, ImGuiCond_Always);
    if (ImGui::BeginChild("##Top_Panel", main_size, false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings))
    {
        ImVec2 main_window_size = ImGui::GetWindowSize();
        ImGui::PushID("##Control_Panel_Main");
        float control_pane_width = g_media_editor_settings.ControlPanelWidth * main_window_size.x;
        float main_width = g_media_editor_settings.MainViewWidth * main_window_size.x;
        is_splitter_hold |= ImGui::Splitter(true, 4.0f, &control_pane_width, &main_width, media_icon_size*2 + 256, main_window_size.x * 0.65);
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
            std::string project_path = g_media_editor_settings.project_path.empty() ? "." : ImGuiHelper::path_url(g_media_editor_settings.project_path);
            ImGuiFileDialog::Instance()->OpenDialog("##MediaEditFileDlgKey", ICON_IGFD_FOLDER_OPEN " Open Project File", 
                                                    pfilters.c_str(),
                                                    project_path.c_str(),
                                                    1, 
                                                    IGFDUserDatas("ProjectOpen"), 
                                                    fflags);
        }
        ImGui::ShowTooltipOnHover("Open Project ...");
        if (ImGui::Button(ICON_NEW_PROJECT "##NewProject", ImVec2(tool_icon_size, tool_icon_size)))
        {
            // New Project
            if (!project_need_save)
            {
                NewProject();
                project_name = "Untitled";
            }
            else if (g_media_editor_settings.project_path.empty())
            {
                show_file_dialog = true;
                ImGuiFileDialog::Instance()->OpenDialog("##MediaEditFileDlgKey", ICON_IGFD_FOLDER_OPEN " Save Project File", 
                                                    pfilters.c_str(),
                                                    "Untitled.mep",
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
        if (ImGui::Button(ICON_MD_SAVE "##SaveProject", ImVec2(tool_icon_size, tool_icon_size)))
        {
            if (!g_media_editor_settings.project_path.empty())
            {
                SaveProject(g_media_editor_settings.project_path);
            }
            else
            {
                show_file_dialog = true;
                ImGuiFileDialog::Instance()->OpenDialog("##MediaEditFileDlgKey", ICON_IGFD_FOLDER_OPEN " Save Project File", 
                                                    pfilters.c_str(),
                                                    "Untitled.mep",
                                                    1, 
                                                    IGFDUserDatas("ProjectSave"), 
                                                    pflags);
            }
        }
        ImGui::ShowTooltipOnHover("Save Project");
        if (ImGui::Button(ICON_MD_SAVE_AS "##SaveProjectAs", ImVec2(tool_icon_size, tool_icon_size)))
        {
            // Save Project
            show_file_dialog = true;
            std::string project_path = g_media_editor_settings.project_path.empty() ? "Untitled.mep" : g_media_editor_settings.project_path;
            ImGuiFileDialog::Instance()->OpenDialog("##MediaEditFileDlgKey", ICON_IGFD_FOLDER_OPEN " Save Project File", 
                                                    pfilters.c_str(),
                                                    project_path.c_str(),
                                                    1, 
                                                    IGFDUserDatas("ProjectSave"), 
                                                    pflags);
        }
        ImGui::ShowTooltipOnHover("Save Project As...");

        if (ImGui::Button(ICON_FA_WHMCS "##Configure", ImVec2(tool_icon_size, tool_icon_size)))
        {
            // Show Setting
            g_new_setting = g_media_editor_settings; // update current setting
            show_configure = true;
        }
        ImGui::ShowTooltipOnHover("Configure");
#ifdef DEBUG_IMGUI
        if (ImGui::Button(ICON_UI_DEBUG "##UIDebug", ImVec2(tool_icon_size, tool_icon_size)))
        {
            // open debug window
            show_debug = !show_debug;
        }
        ImGui::ShowTooltipOnHover("UI Debug");
#endif

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
                project_name = "Untitled";
            }
            ImGui::SameLine();
            if (ImGui::Button("NO", ImVec2(40, 0)))
            {
                ImGui::CloseCurrentPopup();
                NewProject();
                project_name = "Untitled";
            }
            ImGui::SetItemDefaultFocus();
            ImGui::EndPopup();
        }

        // add banks window
        ImVec2 bank_pos = window_pos + ImVec2(4 + tool_icon_size, 32);
        ImVec2 bank_size(control_pane_width - 4 - tool_icon_size, main_window_size.y - 4 - 32);
        if (!g_media_editor_settings.SeparateScope)
            bank_size -= g_media_editor_settings.ExpandScope ? ImVec2(0, scope_height + 32) : ImVec2(0, 16);
        ImGui::SetNextWindowPos(bank_pos, ImGuiCond_Always);
        if (ImGui::BeginChild("##Control_Panel_Window", bank_size, false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings))
        {
            ImVec2 bank_window_size = ImGui::GetWindowSize();
            ImVec2 table_size;
            if (ImGui::TabLabels(ControlPanelTabNames, ControlPanelIndex, table_size, ControlPanelTabTooltips , false, !power_saving_mode, nullptr, nullptr, false, false, nullptr, nullptr))
            {
            }

            // make control panel area
            ImVec2 area_pos = window_pos + ImVec2(tool_icon_size + 4, 32 + 32);
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
                            case 0: ShowTransitionBankIconWindow(draw_list);; break;
                            case 1: ShowTransitionBankTreeWindow(draw_list); break;
                            default: break;
                        }
                        ImGui::UpdateData(); // for animation icon effect
                    break;
                    case 3: ShowMediaOutputWindow(draw_list); break;
                    default: break;
                }
            }
            ImGui::EndChild();

            if(ControlPanelIndex != 0) // switch ControlPanel page to stop play media file
            {
                StopTimelineMediaPlay();
            }
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();
        if (!g_media_editor_settings.SeparateScope)
        {
            bool spread = bank_size.x >= scope_height * 2 + 32;
            ImVec2 scope_pos = bank_pos + ImVec2(0, bank_size.y);
            ImVec2 scope_size = ImVec2(bank_size.x, scope_height + 32);
            ImDrawList *draw_list = ImGui::GetWindowDrawList();
            ImGui::SetNextWindowPos(scope_pos, ImGuiCond_Always);
            if (ImGui::BeginChild("##Scope_View", scope_size, false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings))
            {
                bool overExpanded = ExpandButton(draw_list, ImVec2(scope_pos.x - 24, scope_pos.y + 2), g_media_editor_settings.ExpandScope);
                if (overExpanded && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
                {
                    g_media_editor_settings.ExpandScope = !g_media_editor_settings.ExpandScope;
                    if (g_media_editor_settings.ExpandScope) need_update_scope = true;
                }
                ShowMediaAnalyseWindow(timeline, &g_media_editor_settings.ExpandScope, spread);
                if (!g_media_editor_settings.ExpandScope)
                {
                    scope_flags = 0;
                }
                else
                {
                    scope_flags = 1 << g_media_editor_settings.ScopeWindowIndex;
                    if (spread) scope_flags |= 1 << g_media_editor_settings.ScopeWindowExpandIndex;
                }
            }
            ImGui::EndChild();
        }
        else
        {
            scope_flags = 0xFFFFFFFF;
            ShowMediaAnalyseWindow(timeline);
        }

        ImVec2 main_sub_pos = window_pos + ImVec2(control_pane_width + 8, 32);
        ImVec2 main_sub_size(main_width - 8, main_window_size.y - 4 - 32);
        ImGui::SetNextWindowPos(main_sub_pos, ImGuiCond_Always);
        if (ImGui::BeginChild("##Main_Window", main_sub_size, false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar))
        {
            // full background
            ImDrawList *draw_list = ImGui::GetWindowDrawList();
            ImVec2 table_size;
            int window_index = MainWindowIndex;
            if (ImGui::TabLabels(MainWindowTabNames, window_index, table_size, MainWindowTabTooltips, false, !power_saving_mode, nullptr, nullptr, false, false, nullptr, nullptr))
            {
                MainWindowIndex = window_index;
                timeline->mSelectedItem = -1;
                UIPageChanged();
            }
            
            ImRect title_rect(main_sub_pos + ImVec2(table_size.x, 0), main_sub_pos + ImVec2(main_sub_size.x, table_size.y));
            //draw_list->AddRectFilled(title_rect.Min, title_rect.Max, IM_COL32_WHITE);

            ImRect video_rect;
            auto wmin = main_sub_pos + ImVec2(0, 32);
            auto wmax = wmin + ImGui::GetContentRegionAvail() - ImVec2(8, 0);
            if (timeline->mEditingItems.size() > 0) wmax -= ImVec2(0, 64); // image tab label has height 36
            draw_list->AddRectFilled(wmin, wmax, IM_COL32_BLACK, 8.0, ImDrawFlags_RoundCornersAll);
            if (ImGui::BeginChild("##Main_Window_content", wmax - wmin, false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings))
            {
                switch (MainWindowIndex)
                {
                    case MAIN_PAGE_PREVIEW: ShowMediaPreviewWindow(draw_list, "Preview", 3.f, video_rect); break;
                    case MAIN_PAGE_MIXING: ShowAudioMixingWindow(draw_list, title_rect); break;
                    case MAIN_PAGE_CLIP_EDITOR : ShowEditingItemWindow(draw_list, title_rect); break;
                    default: break;
                }
            }
            ImGui::EndChild();

            if (timeline->mEditingItems.size() > 0)
            {
                static int optionalHoveredTab = 0;
                ImVec2 clip_table_size;
                std::vector<std::string> tab_names;
                std::vector<std::string> tab_tooltips;
                std::vector<ImTextureID> tab_textures;
                std::vector<int> tab_index;
                int justClosedTabIndex = -1;
                int justClosedTabIndexInsideTabItemOrdering = -1;
                //int oldSelectedTab = timeline->mSelectedItem;
                for (auto item : timeline->mEditingItems)
                {
                    tab_names.push_back(item->mName);
                    tab_tooltips.push_back(item->mTooltip);
                    tab_textures.push_back(item->mTexture);
                    tab_index.push_back(item->mIndex);
                }
                ImGui::SetCursorScreenPos(ImGui::GetCursorScreenPos() - ImVec2(0, 4));
                if (ImGui::TabImageLabels(tab_names, timeline->mSelectedItem, clip_table_size, tab_tooltips, tab_textures, ImVec2(64,36), false, false, &optionalHoveredTab, tab_index.data(), true, true, &justClosedTabIndex, &justClosedTabIndexInsideTabItemOrdering, true))
                {
                    MainWindowIndex = MAIN_PAGE_CLIP_EDITOR;
                    UIPageChanged();
                }
                
                if (justClosedTabIndex != -1)
                {
                    //fprintf(stderr, "just closed index:%d %d\n", justClosedTabIndex, justClosedTabIndexInsideTabItemOrdering);
                    for (auto item : timeline->mEditingItems)
                    {
                        if (item->mIndex > justClosedTabIndexInsideTabItemOrdering) item->mIndex--;
                    }
                    auto iter = timeline->mEditingItems.begin() + justClosedTabIndex;
                    auto item = *iter;
                    timeline->mEditingItems.erase(iter);
                    delete item;
                    if (timeline->mEditingItems.empty())
                    {
                        MainWindowIndex = MAIN_PAGE_PREVIEW;
                        UIPageChanged();
                    }
                    else
                    {
                        if (timeline->mSelectedItem < 0)
                        {
                            MainWindowIndex = MAIN_PAGE_PREVIEW;
                        }
                        else if (timeline->mSelectedItem >= timeline->mEditingItems.size())
                        {
                            MainWindowIndex = MAIN_PAGE_PREVIEW;
                            timeline->mSelectedItem = -1;
                        }
                        UIPageChanged();
                    }
                    //for (auto item : timeline->mEditingItems) fprintf(stderr, "%d\n", item->mIndex);
                }
                else
                {
                    for (int i = 0; i < timeline->mEditingItems.size(); i++)
                    {
                        timeline->mEditingItems[i]->mIndex = tab_index[i];
                    }
                }
            }
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
        bool overExpanded = ExpandButton(draw_list, ImVec2(panel_pos.x + 8, panel_pos.y + 2), _expanded);
        if (overExpanded && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            _expanded = !_expanded;
        ImGui::SetCursorScreenPos(panel_pos + ImVec2(32, 0));
        bool timeline_need_save = false;
        auto timeline_changed = DrawTimeLine(timeline,  &_expanded, timeline_need_save, !is_splitter_hold && !mouse_hold && !show_configure && !show_about && !show_file_dialog);
        if (!g_project_loading) project_changed |= timeline_changed;
        project_need_save |= timeline_changed | timeline_need_save;
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

    if (MainWindowIndex == MAIN_PAGE_PREVIEW)
    {
        // preview view
        if (MonitorIndexPreviewVideo != -1 && MonitorIndexPreviewVideo < platform_io.Monitors.Size)
        {
            std::string preview_window_lable = "Preview_view_windows" + std::to_string(MonitorIndexPreviewVideo);
            auto mon = platform_io.Monitors[MonitorIndexPreviewVideo];
            ImGui::SetNextWindowPos(mon.MainPos);
            ImGui::SetNextWindowSize(mon.MainSize);
            if (ImGui::Begin(preview_window_lable.c_str(), nullptr, flags | ImGuiWindowFlags_FullScreen))
                ShowVideoWindow(timeline->mMainPreviewTexture, mon.MainPos, mon.MainSize, "Preview", 3.f);
            ImGui::End();
        }
    }
#if 0 // TODO::Dicky editing item monitors support
    else if (MainWindowIndex == MAIN_PAGE_VIDEO && VideoEditorWindowIndex == VIDEO_PAGE_FILTER)
    {
        // video filter
        if (MonitorIndexVideoFilterOrg != -1 && MonitorIndexVideoFilterOrg < platform_io.Monitors.Size)
        {
            std::string view_window_lable = "video_filter_org_windows" + std::to_string(MonitorIndexVideoFilterOrg);
            auto mon = platform_io.Monitors[MonitorIndexVideoFilterOrg];
            ImGui::SetNextWindowPos(mon.MainPos);
            ImGui::SetNextWindowSize(mon.MainSize);
            if (ImGui::Begin(view_window_lable.c_str(), nullptr, flags | ImGuiWindowFlags_FullScreen))
                ShowVideoWindow(timeline->mVideoFilterInputTexture, mon.MainPos, mon.MainSize, "Original", 3.f);
            ImGui::End();
        }
        if (MonitorIndexVideoFiltered != -1 && MonitorIndexVideoFiltered < platform_io.Monitors.Size)
        {
            std::string view_window_lable = "video_filter_output_windows" + std::to_string(MonitorIndexVideoFiltered);
            auto mon = platform_io.Monitors[MonitorIndexVideoFiltered];
            ImGui::SetNextWindowPos(mon.MainPos);
            ImGui::SetNextWindowSize(mon.MainSize);
            if (ImGui::Begin(view_window_lable.c_str(), nullptr, flags | ImGuiWindowFlags_FullScreen))
                ShowVideoWindow(timeline->mVideoFilterOutputTexture, mon.MainPos, mon.MainSize, timeline->bFilterOutputPreview ? "Preview Output" : "Filter Output", 3.f);
            ImGui::End();
        }
    }
    else if (MainWindowIndex == MAIN_PAGE_VIDEO && VideoEditorWindowIndex == VIDEO_PAGE_TRANSITION)
    {
        // video attribute
        if (MonitorIndexVideoFilterOrg != -1 && MonitorIndexVideoFilterOrg < platform_io.Monitors.Size)
        {
            std::string view_window_lable = "video_attribute_org_windows" + std::to_string(MonitorIndexVideoFilterOrg);
            auto mon = platform_io.Monitors[MonitorIndexVideoFilterOrg];
            ImGui::SetNextWindowPos(mon.MainPos);
            ImGui::SetNextWindowSize(mon.MainSize);
            if (ImGui::Begin(view_window_lable.c_str(), nullptr, flags | ImGuiWindowFlags_FullScreen))
                ShowVideoWindow(timeline->mVideoFilterInputTexture, mon.MainPos, mon.MainSize, "Original", 3.f);
            ImGui::End();
        }
        if (MonitorIndexVideoFiltered != -1 && MonitorIndexVideoFiltered < platform_io.Monitors.Size)
        {
            std::string view_window_lable = "video_attribute_output_windows" + std::to_string(MonitorIndexVideoFiltered);
            auto mon = platform_io.Monitors[MonitorIndexVideoFiltered];
            ImGui::SetNextWindowPos(mon.MainPos);
            ImGui::SetNextWindowSize(mon.MainSize);
            if (ImGui::Begin(view_window_lable.c_str(), nullptr, flags | ImGuiWindowFlags_FullScreen))
                ShowVideoWindow(timeline->mVideoFilterOutputTexture, mon.MainPos, mon.MainSize, timeline->bFilterOutputPreview ? "Preview Output" : "Attribute Output", 3.f);
            ImGui::End();
        }
    }
#endif
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
                bool image_sequence = file_suffix.empty() || file_suffix == ".";
                InsertMedia(file_path, image_sequence);
                project_changed = true;
            }
            else if (userDatas.compare("ProjectOpen") == 0)
            {
                OpenProject(file_path);
                project_name = ImGuiHelper::path_filename_prefix(file_path);
            }
            else if (userDatas.compare("ProjectSave") == 0)
            {
                if (file_suffix.empty())
                    file_path += ".mep";
                SaveProject(file_path);
                project_name = ImGuiHelper::path_filename_prefix(file_path);
            }
            else if (userDatas.compare("ProjectSaveAndNew") == 0)
            {
                if (file_suffix.empty())
                    file_path += ".mep";
                SaveProject(file_path);
                NewProject();
                project_name = "Untitled";
            }
            else if (userDatas.compare("ProjectSaveQuit") == 0)
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

    auto txmgr = RenderUtils::TextureManager::GetDefaultInstance();
    txmgr->UpdateTextureState();
    // Logger::Log(Logger::DEBUG) << txmgr.get() << std::endl;
    return app_done;
}

#if UI_PERFORMANCE_ANALYSIS
static bool MediaEditor_Frame_wrapper(void * handle, bool app_will_quit)
{
    auto hPa = MediaCore::PerformanceAnalyzer::GetThreadLocalInstance();
    hPa->Reset();
    auto ret = MediaEditor_Frame(handle, app_will_quit);
    auto tspan = hPa->End();
    // if 'Application_Frame' takes more than 33 millisec, the refresh rate will drop below 30fps
    if (MediaCore::CountElapsedMillisec(tspan.first, tspan.second) > 33)
        hPa->LogAndClearStatistics(Logger::INFO);
    return ret;
}
#endif

static void LoadPluginThread()
{
    std::vector<std::string> plugin_paths;
    plugin_paths.push_back(g_plugin_path);
    int plugins = BluePrint::BluePrintUI::CheckPlugins(plugin_paths);
    BluePrint::BluePrintUI::LoadPlugins(plugin_paths, g_plugin_loading_current_index, g_plugin_loading_message, g_plugin_loading_percentage, plugins);
    g_plugin_loading_message = "Plugin load finished!!!";
    g_plugin_loading = false;
}

static void EnvScanThread()
{
    auto hHwaMgr = MediaCore::HwaccelManager::GetDefaultInstance();
    if (!hHwaMgr->Init())
        Logger::Log(Logger::Error) << "FAILED to init 'HwaccelManager' instance! Error is '" << hHwaMgr->GetError() << "'." << std::endl;
    g_env_scanning = false;
}

static bool MediaEditor_Splash_Screen(void* handle, bool app_will_quit)
{
    static int32_t splash_start_time = ImGui::get_current_time_msec();
    auto& io = ImGui::GetIO();
    ImGuiContext& g = *GImGui;
    if (!g_media_editor_settings.UILanguage.empty() && g.LanguageName != g_media_editor_settings.UILanguage)
        g.LanguageName = g_media_editor_settings.UILanguage;
    std::string project_name = g_media_editor_settings.project_path.empty() ? "Untitled" : ImGuiHelper::path_filename_prefix(g_media_editor_settings.project_path);
    ImGuiCond cond = ImGuiCond_None;
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | 
                            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize, cond);
    ImGui::Begin("MediaEditor Splash", nullptr, flags);
    auto draw_list = ImGui::GetWindowDrawList();
    bool title_finished = Show_Version(draw_list, splash_start_time);
    if (!g_plugin_loaded)
    {
        g_plugin_loading = true;
        g_loading_plugin_thread = new std::thread(LoadPluginThread);
        g_plugin_loaded = true;
    }
    if (!g_env_scanned)
    {
        g_env_scanning = true;
        g_env_scan_thread = new std::thread(EnvScanThread);
        g_env_scanned = true;
    }
    std::string load_str;
    if (g_plugin_loading)
    {
        load_str = "Loading Plugin:" + g_plugin_loading_message;
        auto loading_size = ImGui::CalcTextSize(load_str.c_str());
        float xoft = 4;
        float yoft = io.DisplaySize.y - loading_size.y - 32 - 8;
        ImGui::SetCursorPos(ImVec2(xoft, yoft));
        ImGui::TextUnformatted("Loading Plugin:"); ImGui::SameLine();
        ImGui::Text("%s", g_plugin_loading_message.c_str());
    }
    else if (g_project_loading)
    {
        load_str = project_name + " Project Loading...";
        auto loading_size = ImGui::CalcTextSize(load_str.c_str());
        float xoft = (io.DisplaySize.x - loading_size.x) / 2;
        float yoft = io.DisplaySize.y - loading_size.y - 32 - 8;
        ImGui::SetCursorPos(ImVec2(xoft, yoft));
        ImGui::Text("%s ", project_name.c_str()); ImGui::SameLine();
        ImGui::TextUnformatted("Project Loading...");
    }

    float percentage = std::min(g_plugin_loading_percentage * 0.5 + g_project_loading_percentage * 0.5, 1.0);
    ImGui::SetCursorPos(ImVec2(4, io.DisplaySize.y - 32));
    ImGui::ProgressBar("##splash_progress", percentage, 0.f, 1.f, "", ImVec2(io.DisplaySize.x - 16, 8), 
                        ImVec4(0.3f, 0.3f, 0.8f, 1.f), ImVec4(0.1f, 0.1f, 0.2f, 1.f), ImVec4(0.f, 0.f, 0.8f, 1.f));
    ImGui::UpdateData();
    ImGui::End();
    return title_finished && !g_project_loading && !g_plugin_loading;
}

static void MediaEditor_Splash_Finalize(void** handle)
{
    if (logo_texture) { ImGui::ImDestroyTexture(logo_texture); logo_texture = nullptr; }
    if (codewin_texture) { ImGui::ImDestroyTexture(codewin_texture); codewin_texture = nullptr; }
}

void Application_Setup(ApplicationWindowProperty& property)
{
    // param commandline args
    static struct option long_options[] = {
        { "plugin_dir", required_argument, NULL, 'p' },
        { "language_dir", required_argument, NULL, 'l' },
        { "resuorce_dir", required_argument, NULL, 'r' },
        { 0, 0, 0, 0 }
    };
    if (property.argc > 1 && property.argv)
    {
        int o = -1;
        int option_index = 0;
        while ((o = getopt_long(property.argc, property.argv, "p:l:r:", long_options, &option_index)) != -1)
        {
            if (o == -1)
                break;
            switch (o)
            {
                case 'p': g_plugin_path = std::string(optarg); break;
                case 'l': g_language_path = std::string(optarg); break;
                case 'r': g_resource_path = std::string(optarg); break;
                default: break;
            }
        }
    }

    auto exec_path = ImGuiHelper::exec_path();
    // add language
    property.language_path = !g_language_path.empty() ? g_language_path : 
#if defined(__APPLE__)
        exec_path + "../Resources/languages/";
#elif defined(_WIN32)
        exec_path + "../languages/";
#elif defined(__linux__)
        exec_path + "../languages/";
#else
        std::string();
#endif
    icon_file = 
    property.icon_path =  !g_resource_path.empty() ? g_resource_path + "/mec_logo.png" : 
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
    property.navigator = false;
    //property.using_setting_path = false;
    property.low_reflash = true;
    property.font_scale = 2.0f;
#if 1
    //property.resizable = false;
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
    property.application.Application_SplashFinalize = MediaEditor_Splash_Finalize;
#if UI_PERFORMANCE_ANALYSIS
    property.application.Application_Frame = MediaEditor_Frame_wrapper;
#else
    property.application.Application_Frame = MediaEditor_Frame;
#endif

    if (g_plugin_path.empty())
        g_plugin_path = ImGuiHelper::path_parent(exec_path) + "plugins";
}

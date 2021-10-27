#include <application.h>
#include <imgui_helper.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/app/app.h>

// GstVideoTestSrcPattern
static const char *video_test_patterns[] = {
  "SMPTE color bars",
  "Random (television snow)",
  "100% Black",
  "100% White",
  "Red",
  "Green",
  "Blue",
  "Checkers 1px",
  "Checkers 2px",
  "Checkers 4px",
  "Checkers 8px",
  "Circular",
  "Blink",
  "SMPTE 75% color bars",
  "Zone plate",
  "Gamut checkers",
  "Chroma zone plate",
  "Solid color",
  "Moving ball",
  "SMPTE 100% color bars",
  "Bar",
  "Pinwheel",
  "Spokes",
  "Gradient",
  "Colors"
};

#define VIDEO_WIDTH     1920
#define VIDEO_HEIGHT    1080
static GstElement *pipeline = nullptr;
static GstElement *videosrc = nullptr;
static GstElement *appsink = nullptr;
static ImTextureID video_texture = nullptr;

void Application_GetWindowProperties(ApplicationWindowProperty& property)
{
    property.name = "Media Editor";
    property.viewport = false;
    property.docking = false;
    property.auto_merge = false;
    property.width = 1280;
    property.height = 720;
}

void Application_Initialize(void** handle)
{
    gst_init(nullptr, nullptr);
    pipeline = gst_pipeline_new("pipeline");
    videosrc = gst_element_factory_make("videotestsrc", "videosrc0");
    appsink = gst_element_factory_make("appsink", "videosink0");
    gst_app_sink_set_max_buffers(GST_APP_SINK(appsink), 1);
    gst_app_sink_set_drop(GST_APP_SINK(appsink), true);
    gst_bin_add_many(GST_BIN(pipeline), videosrc, appsink, NULL);
    GstCaps *caps = gst_caps_new_simple(
        "video/x-raw",
        "format", G_TYPE_STRING, "RGBA",
        "width", G_TYPE_INT, VIDEO_WIDTH,
        "height", G_TYPE_INT, VIDEO_HEIGHT,
        "framerate", GST_TYPE_FRACTION, 30, 1,
        NULL);
    gboolean link_ok = gst_element_link_filtered(videosrc, appsink, caps);
    g_assert(link_ok);
    gst_caps_unref(caps);
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    ImGuiIO& io = ImGui::GetIO(); (void)io;
}

void Application_Finalize(void** handle)
{
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    gst_deinit();
    if (video_texture) { ImGui::ImDestroyTexture(video_texture); video_texture = nullptr; }
}

bool Application_Frame(void * handle)
{
    if (video_texture) { ImGui::ImDestroyTexture(video_texture); video_texture = nullptr; }

    GstSample *videosample = gst_app_sink_try_pull_sample(GST_APP_SINK(appsink), 10 * GST_MSECOND);
    if (videosample) 
    {
        GstBuffer *videobuf = gst_sample_get_buffer(videosample);
        GstMapInfo map;

        gst_buffer_map(videobuf, &map, GST_MAP_READ);

        video_texture = ImGui::ImCreateTexture(map.data, VIDEO_WIDTH, VIDEO_HEIGHT);

        gst_buffer_unmap(videobuf, &map);
        gst_sample_unref(videosample);
    }

     ImGui::GetBackgroundDrawList ()->AddImage (
        (void *) (guintptr) video_texture,
        ImVec2 (0, 0),
        ImVec2 (1280, 720),
        ImVec2 (0, 0),
        ImVec2 (1, 1)
        );

    static ImGuiComboFlags combo_flags = 0;
    static const char *item_current = video_test_patterns[0];
    if (ImGui::BeginCombo("Video Test Patterns", item_current, combo_flags))
    {
        for (int i = 0; i < IM_ARRAYSIZE(video_test_patterns); i++)
        {
            bool is_selected = (item_current == video_test_patterns[i]);
            if (ImGui::Selectable(video_test_patterns[i], is_selected)) 
            {
                item_current = video_test_patterns[i];
                g_object_set(videosrc, "pattern", i, NULL);
		        GST_INFO("selected %d=%s", i, video_test_patterns[i]);
            }
            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo ();
    }
    return false;
}
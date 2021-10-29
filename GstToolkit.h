#ifndef __GSTGUI_TOOLKIT_H_
#define __GSTGUI_TOOLKIT_H_

#include <gst/gst.h>

#include <string>
#include <list>

namespace GstToolkit
{

typedef enum {
    TIME_STRING_FIXED = 0,
    TIME_STRING_ADJUSTED,
    TIME_STRING_MINIMAL,
    TIME_STRING_READABLE
} time_string_mode;

std::string time_to_string(guint64 t, time_string_mode m = TIME_STRING_ADJUSTED);
std::string filename_to_uri(std::string filename);

std::string gst_version();

std::list<std::string> all_plugins();
std::list<std::string> enable_gpu_decoding_plugins(bool enable = true);
std::string used_gpu_decoding_plugins(GstElement *gstbin);
std::string used_decoding_plugins(GstElement *gstbin);

std::list<std::string> all_plugin_features(std::string pluginname);
bool has_feature (std::string name);
bool enable_feature (std::string name, bool enable);

}

#endif // __GSTGUI_TOOLKIT_H_

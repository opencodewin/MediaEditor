# - Try to find gst-plugins-base
# Once done this will define
#
#  GSTREAMER_PLUGINS_BASE_FOUND - system has gst-plugins-base
#
#  And for all the plugin libraries specified in the COMPONENTS
#  of find_package, this module will define:
#
#  GSTREAMER_<plugin_lib>_LIBRARY_FOUND - system has <plugin_lib>
#  GSTREAMER_<plugin_lib>_LIBRARY - the <plugin_lib> library
#  GSTREAMER_<plugin_lib>_INCLUDE_DIR - the <plugin_lib> include directory
#
# Copyright (c) 2010, Collabora Ltd.
#   @author George Kiagiadakis <george.kiagiadakis@collabora.co.uk>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

set(GSTREAMER_ABI_VERSION "1.0")


# Find the pkg-config file for doing the version check
find_package(PkgConfig)

if (PKG_CONFIG_FOUND)
    pkg_check_modules(PKG_GSTREAMER_PLUGINS_BAD gstreamer-plugins-bad-${GSTREAMER_ABI_VERSION})
endif()


# Find the plugin libraries
include(MacroFindGStreamerLibrary)

macro(_find_gst_plugins_bad_component _name _header)
    find_gstreamer_library(${_name} ${_header} ${GSTREAMER_ABI_VERSION})
    set(_GSTREAMER_PLUGINS_BAD_EXTRA_VARIABLES ${_GSTREAMER_PLUGINS_BAD_EXTRA_VARIABLES}
                                        GSTREAMER_${_name}_LIBRARY GSTREAMER_${_name}_INCLUDE_DIR)
endmacro()

foreach(_component ${GStreamerPluginsBad_FIND_COMPONENTS})
    if (${_component} STREQUAL "player")
        _find_gst_plugins_bad_component(PLAYER gstplayer.h)
    elseif (${_component} STREQUAL "webrtc")
        _find_gst_plugins_bad_component(WEBRTC webrtc.h)
    elseif (${_component} STREQUAL "mpegts")
        _find_gst_plugins_bad_component(MPEGTS mpegts.h)
    else()
        message (AUTHOR_WARNING "FindGStreamerPluginBad.cmake: Invalid component \"${_component}\" was specified")
    endif()
endforeach()

get_filename_component(_GSTREAMER_BAD_LIB_DIR ${GSTREAMER_PLAYER_LIBRARY} PATH)
set(PKG_GSTREAMER_BAD_PLUGIN_DIR ${_GSTREAMER_BAD_LIB_DIR}/gstreamer-${GSTREAMER_ABI_VERSION})

# Version check
if (GStreamerPluginsBad_FIND_VERSION)
    if (PKG_GSTREAMER_PLUGINS_BAD_FOUND)
        if("${PKG_GSTREAMER_PLUGINS_BAD_VERSION}" VERSION_LESS "${GStreamerPluginsBad_FIND_VERSION}")
            message(STATUS "Found gst-plugins-base version ${PKG_GSTREAMER_PLUGINS_BAD_VERSION}, but at least version ${GStreamerPluginsBad_FIND_VERSION} is required")
            set(GSTREAMER_PLUGINS_BAD_VERSION_COMPATIBLE FALSE)
        else()
            set(GSTREAMER_PLUGINS_BAD_VERSION_COMPATIBLE TRUE)
        endif()
    else()
        # We can't make any version checks without pkg-config, just assume version is compatible and hope...
        set(GSTREAMER_PLUGINS_BAD_VERSION_COMPATIBLE TRUE)
    endif()
else()
    # No version constrain was specified, thus we consider the version compatible
    set(GSTREAMER_PLUGINS_BAD_VERSION_COMPATIBLE TRUE)
endif()


include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GStreamerPluginsBad DEFAULT_MSG
                                  GSTREAMER_PLUGINS_BAD_VERSION_COMPATIBLE
                                  ${_GSTREAMER_PLUGINS_BAD_EXTRA_VARIABLES})

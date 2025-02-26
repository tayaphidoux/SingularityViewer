# -*- cmake -*-
include(Prebuilt)
  include(FindPkgConfig)
  pkg_check_modules(GSTREAMER010 REQUIRED gstreamer-1.0)
  pkg_check_modules(GSTREAMER010_PLUGINS_BASE REQUIRED gstreamer-plugins-base-1.0)


if (WINDOWS)
  # We don't need to explicitly link against gstreamer itself, because
  # LLMediaImplGStreamer probes for the system's copy at runtime.
    set(GSTREAMER010_LIBRARIES
         libgstvideo
         libgstaudio
         libgstbase-1.0
         libgstreamer-1.0
         gobject-2.0
         gmodule-2.0
         gthread-2.0
         glib-2.0
         )
else (WINDOWS)
  # We don't need to explicitly link against gstreamer itself, because
  # LLMediaImplGStreamer probes for the system's copy at runtime.
    set(GSTREAMER010_LIBRARIES
         gstvideo-1.0
         gstaudio-1.0
         gstbase-1.0
         gstreamer-1.0
         gobject-2.0
         gmodule-2.0
         dl
         gthread-2.0
         rt
         glib-2.0
         )


endif (WINDOWS)


if (GSTREAMER010_FOUND AND GSTREAMER010_PLUGINS_BASE_FOUND)
  set(GSTREAMER010 ON CACHE BOOL "Build with GStreamer-1.0 streaming media support.")
  add_definitions(-DLL_GSTREAMER010_ENABLED=1)
endif (GSTREAMER010_FOUND AND GSTREAMER010_PLUGINS_BASE_FOUND)

  



# -*- cmake -*-
if (WINDOWS)
   configure_file(
       ${CMAKE_SOURCE_DIR}/newview/res/viewerRes.rc.in
       ${CMAKE_SOURCE_DIR}/newview/res/viewerRes.rc
   )
endif (WINDOWS)

if (DARWIN)
   configure_file(
       ${CMAKE_SOURCE_DIR}/newview/English.lproj/InfoPlist.strings.in
       ${CMAKE_SOURCE_DIR}/newview/English.lproj/InfoPlist.strings
   )
endif (DARWIN)

if (LINUX)
   configure_file(
       ${CMAKE_SOURCE_DIR}/newview/linux_tools/wrapper.sh.in
       ${CMAKE_SOURCE_DIR}/newview/linux_tools/wrapper.sh
       @ONLY
   )
   configure_file(
       ${CMAKE_SOURCE_DIR}/newview/linux_tools/handle_secondlifeprotocol.sh.in
       ${CMAKE_SOURCE_DIR}/newview/linux_tools/handle_secondlifeprotocol.sh
       @ONLY
   )
   configure_file(
       ${CMAKE_SOURCE_DIR}/newview/linux_tools/install.sh.in
       ${CMAKE_SOURCE_DIR}/newview/linux_tools/install.sh
       @ONLY
   )
   configure_file(
       ${CMAKE_SOURCE_DIR}/newview/linux_tools/refresh_desktop_app_entry.sh.in
       ${CMAKE_SOURCE_DIR}/newview/linux_tools/refresh_desktop_app_entry.sh
       @ONLY
   )
endif (LINUX)

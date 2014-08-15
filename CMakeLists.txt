project(akonadiclient)

###### packages ######

find_package(KDE4 4.7.0 REQUIRED)
include(KDE4Defaults)
include(MacroLibrary)
include(CheckIncludeFiles)
include(MacroBoolTo01)
find_package(KdepimLibs 4.7.0 REQUIRED)

set(VERSION "0.1")

###### version tracking ######

add_custom_target(vcsversion
  ALL
  COMMENT "Checking VCS type and version"
  VERBATIM
  COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/genversion.sh ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} ${VERSION}
)

###### targets ######

add_subdirectory(src)
add_subdirectory(doc)

###### information ######

macro_display_feature_log()

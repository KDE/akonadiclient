project(akonadiclient)

###### packages ######

find_package(KDE4 4.7.0 REQUIRED)
include(KDE4Defaults)
include(MacroLibrary)
include(CheckIncludeFiles)
include(MacroBoolTo01)
find_package(KdepimLibs 4.7.0 REQUIRED)

set(VERSION "0.1")

###### targets ######

add_subdirectory(src)

###### information ######

macro_display_feature_log()

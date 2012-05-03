project(akonadiclient)

###### packages ######

find_package(KDE4 4.7.0 REQUIRED)
include(KDE4Defaults)
include(MacroLibrary)
include(CheckIncludeFiles)
include(MacroBoolTo01)
find_package(KdepimLibs 4.7.0 REQUIRED)

include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} ${KDEPIMLIBS_INCLUDE_DIRS} ${KDE4_INCLUDES} ${QT_INCLUDE_DIR})

###### target ######

set(akonadiclient_SRCS
    main.cpp
)

add_executable(akonadiclient ${akonadiclient_SRCS})

target_link_libraries(akonadiclient ${QT_QTCORE_LIBRARY})

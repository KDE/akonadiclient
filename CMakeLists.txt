project(akonadiclient)

###### packages ######

find_package(KDE4 4.7.0 REQUIRED)
include(KDE4Defaults)
include(MacroLibrary)
include(CheckIncludeFiles)
include(MacroBoolTo01)
find_package(KdepimLibs 4.7.0 REQUIRED)

include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} ${KDEPIMLIBS_INCLUDE_DIRS} ${KDE4_INCLUDES} ${QT_INCLUDE_DIR})

set(CMAKE_AUTOMOC TRUE)

###### target ######

add_definitions(-DPROGRAM_VERSION='"0.1"')

set(akonadiclient_SRCS
    abstractcommand.cpp
    addcommand.cpp
    collectionresolvejob.cpp
    commandfactory.cpp
    commandrunner.cpp
    listcommand.cpp
    main.cpp
)

add_executable(akonadiclient ${akonadiclient_SRCS})

target_link_libraries(akonadiclient ${KDEPIMLIBS_AKONADI_LIBS} ${KDE4_KDECORE_LIBS} ${QT_QTCORE_LIBRARY})

install(TARGETS akonadiclient ${INSTALL_TARGETS_DEFAULT_ARGS})

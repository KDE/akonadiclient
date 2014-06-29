project(akonadiclient)

###### setup ######

include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} ${KDEPIMLIBS_INCLUDE_DIRS} ${KDE4_INCLUDES} ${QT_INCLUDE_DIR})

add_definitions(-DVERSION=\\"${VERSION}\\")

set(CMAKE_AUTOMOC TRUE)

###### target ######

set(akonadiclient_SRCS
    abstractcommand.cpp
    addcommand.cpp
    collectionresolvejob.cpp
    collectionpathjob.cpp
    commandfactory.cpp
    commandrunner.cpp
    editcommand.cpp
    listcommand.cpp
    infocommand.cpp
    showcommand.cpp
    createcommand.cpp
    copycommand.cpp
    movecommand.cpp
    expandcommand.cpp
    main.cpp
    errorreporter.cpp
)

kde4_add_executable(akonadiclient ${akonadiclient_SRCS})
target_link_libraries(akonadiclient ${KDEPIMLIBS_AKONADI_LIBS} ${KDE4_KABC_LIBS} ${KDE4_KDECORE_LIBS} ${QT_QTCORE_LIBRARY})

###### version tracking ######

add_dependencies(akonadiclient vcsversion)

###### install ######

install(TARGETS akonadiclient ${INSTALL_TARGETS_DEFAULT_ARGS})

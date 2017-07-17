project(akonadiclient)

###### setup ######

option(AKONADICLIENT_BUILD_TESTS "Whether to build the automatic tests" ${KDE4_BUILD_TESTS})

include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} ${KDEPIMLIBS_INCLUDE_DIRS} ${KDE4_INCLUDES} ${QT_INCLUDE_DIR})

add_definitions(-DVERSION=\\"${VERSION}\\")

set(CMAKE_AUTOMOC TRUE)

###### target ######

set(libakonadiclient_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/abstractcommand.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/addcommand.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/collectionresolvejob.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/collectionpathjob.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/commandfactory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/commandrunner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/listcommand.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/infocommand.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/showcommand.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/createcommand.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/copycommand.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/movecommand.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/groupcommand.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/errorreporter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/editcommand.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/deletecommand.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/renamecommand.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/updatecommand.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tagscommand.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/agentscommand.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/exportcommand.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/importcommand.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/commandshell.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dumpcommand.cpp
)

set(akonadiclient_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)
kde4_add_library(libakonadiclient STATIC ${libakonadiclient_SRCS})
target_link_libraries(libakonadiclient ${KDEPIMLIBS_AKONADI_LIBS} ${KDEPIMLIBS_AKONADI_XML_LIBS} ${KDEPIMLIBS_AKONADI_CONTACT_LIBS})
target_link_libraries(libakonadiclient ${KDEPIMLIBS_KPIMUTILS_LIBS} ${KDE4_KABC_LIBS})
target_link_libraries(libakonadiclient ${KDE4_KDECORE_LIBS})
target_link_libraries(libakonadiclient ${QT_QTCORE_LIBRARY})

set_target_properties(libakonadiclient PROPERTIES ENABLE_EXPORTS TRUE)

kde4_add_executable(akonadiclient ${akonadiclient_SRCS})
target_link_libraries(akonadiclient libakonadiclient)

###### version tracking ######

add_dependencies(akonadiclient vcsversion)

###### install ######

install(TARGETS akonadiclient ${INSTALL_TARGETS_DEFAULT_ARGS})

if (AKONADICLIENT_BUILD_TESTS)
  add_subdirectory(tests)
endif (AKONADICLIENT_BUILD_TESTS)

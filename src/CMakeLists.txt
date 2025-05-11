if (HAVE_TERMINFO)
  add_definitions("-DHAVE_TERMINFO")
endif ()

###### library ######

set(libakonadiclient_SRCS
    abstractcommand.cpp
    addcommand.cpp
    collectionlistcommand.cpp
    collectionresolvejob.cpp
    collectionpathjob.cpp
    commandfactory.cpp
    commandrunner.cpp
    listcommand.cpp
    infocommand.cpp
    showcommand.cpp
    createcommand.cpp
    copycommand.cpp
    movecommand.cpp
    groupcommand.cpp
    errorreporter.cpp
    editcommand.cpp
    deletecommand.cpp
    renamecommand.cpp
    updatecommand.cpp
    tagscommand.cpp
    agentscommand.cpp
    exportcommand.cpp
    importcommand.cpp
    commandshell.cpp
    dumpcommand.cpp
    attributescommand.cpp
    terminalcolour.cpp
    folderscommand.cpp
    jsonformatter.cpp
)

add_library(libakonadiclient STATIC)
target_sources(libakonadiclient PRIVATE ${libakonadiclient_SRCS})
target_link_libraries(libakonadiclient KPim6::AkonadiCore KPim6::AkonadiXml KPim6::AkonadiContactCore KPim6::Mime)
target_link_libraries(libakonadiclient KF6::I18n KF6::Codecs KF6::ConfigCore)
target_link_libraries(libakonadiclient Qt::Core)
if (HAVE_TERMINFO)
  target_link_libraries(libakonadiclient ${CURSES_LIBRARIES})
endif ()
set_target_properties(libakonadiclient PROPERTIES ENABLE_EXPORTS TRUE)

###### executable ######

set(akonadiclient_SRCS main.cpp)
add_executable(akonadiclient ${akonadiclient_SRCS})
target_link_libraries(akonadiclient libakonadiclient)

###### install ######

install(TARGETS akonadiclient ${INSTALL_TARGETS_DEFAULT_ARGS})

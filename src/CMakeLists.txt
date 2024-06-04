
###### target ######

set(libakonadiclient_SRCS
    abstractcommand.cpp
    addcommand.cpp
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

    abstractcommand.h
    addcommand.h
    collectionresolvejob.h
    collectionpathjob.h
    commandfactory.h
    commandrunner.h
    listcommand.h
    infocommand.h
    showcommand.h
    createcommand.h
    copycommand.h
    movecommand.h
    groupcommand.h
    errorreporter.h
    editcommand.h
    deletecommand.h
    renamecommand.h
    updatecommand.h
    tagscommand.h
    agentscommand.h
    exportcommand.h
    importcommand.h
    commandshell.h
    dumpcommand.h
)

set(akonadiclient_SRCS main.cpp)
add_library(libakonadiclient STATIC)
target_sources(libakonadiclient PRIVATE ${libakonadiclient_SRCS})
target_link_libraries(libakonadiclient KPim6::AkonadiCore KPim6::AkonadiXml KPim6::AkonadiContactCore KPim6::Mime)
target_link_libraries(libakonadiclient KF6::I18n KF6::Codecs)
target_link_libraries(libakonadiclient Qt::Core)

set_target_properties(libakonadiclient PROPERTIES ENABLE_EXPORTS TRUE)

add_executable(akonadiclient ${akonadiclient_SRCS})
target_link_libraries(akonadiclient libakonadiclient)

###### version tracking ######

add_dependencies(akonadiclient vcsversion)

###### install ######

install(TARGETS akonadiclient ${INSTALL_TARGETS_DEFAULT_ARGS})

project(akonadiclient)

#include_directories("../")
#get_filename_component(PARENT_DIR ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)
#list(REMOVE_ITEM akonadiclient_SRCS ${PARENT_DIR}/main.cpp)

macro(add_akonadi_isolated_test _source)
set(_test abstractcommandtest.cpp ${_source})
get_filename_component(_name ${_source} NAME_WE)
kde4_add_executable(${_name} TEST ${_test})
target_link_libraries(${_name}
libakonadiclient akonadi-kde akonadi-kmime ${QT_QTTEST_LIBRARY} 
${KDE4_KDECORE_LIBS} ${KDE4_KABC_LIBS}
)

# based on kde4_add_unit_test
if (WIN32)
    get_target_property( _loc ${_name} LOCATION )
    set(_executable ${_loc}.bat)
else (WIN32)
    set(_executable ${CMAKE_CURRENT_BINARY_DIR}/${_name})
endif (WIN32)

if (UNIX)
    set(_executable ${_executable}.shell)
endif (UNIX)

find_program(_testrunner akonaditest)
    add_test( akonadiclient-${_name}
        ${_testrunner} -c
        ${CMAKE_CURRENT_SOURCE_DIR}/unittestenv/config.xml
        ${_executable}
    )
endmacro(add_akonadi_isolated_test)

add_akonadi_isolated_test(copycommandtest.cpp)
add_akonadi_isolated_test(createcommandtest.cpp)
add_akonadi_isolated_test(addcommandtest.cpp)
add_akonadi_isolated_test(movecommandtest.cpp)
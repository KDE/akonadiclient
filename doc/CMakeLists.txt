set(AKONADICLIENTDOC_MANS
	akonadiclient.1
)

find_program(RST2MAN_EXECUTABLE NAMES rst2man)
find_program(GZIP_EXECUTABLE NAMES gzip)

foreach(manpage ${AKONADICLIENTDOC_MANS})
	execute_process(COMMAND ${RST2MAN_EXECUTABLE}
		${CMAKE_CURRENT_SOURCE_DIR}/${manpage}.rst
		${CMAKE_CURRENT_BINARY_DIR}/${manpage}
	)

	execute_process(COMMAND ${GZIP_EXECUTABLE} -f ${CMAKE_CURRENT_BINARY_DIR}/${manpage})

endforeach()

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/akonadiclient.1.gz DESTINATION ${MAN_INSTALL_DIR}/man1/)

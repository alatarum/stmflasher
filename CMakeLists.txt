cmake_minimum_required (VERSION 2.6)

set (PROJECT stmflasher)

project (${PROJECT})
set (EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})
set (BIN_INSTALL_DIR bin)

include_directories (
	./
	./parsers
)


set (HEADERS
	./serial.h
	./stm32.h
	./utils.h
	./parsers/parser.h
	./parsers/binary.h
	./parsers/hex.h
)

set (SOURCES 
	./main.c
	./utils.c
	./stm32.c
	./serial_common.c
	./parsers/binary.c
	./parsers/hex.c
)

IF(WIN32)
	SET (SOURCES
		${SOURCES}
		./serial_w32.c
	)
ELSE(WIN32)
	SET (SOURCES
		${SOURCES}
		./serial_posix.c
	)
ENDIF(WIN32)


source_group ("Header Files" FILES ${HEADERS})
source_group ("Source Files" FILES ${SOURCES})

add_executable (${PROJECT} ${HEADERS} ${SOURCES})
install(TARGETS ${PROJECT} DESTINATION ${BIN_INSTALL_DIR})

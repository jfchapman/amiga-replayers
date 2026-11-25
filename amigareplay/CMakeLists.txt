# The replayers are built in, so that this one library is all that a program needs.
set(SOURCES)
foreach(REPLAYER hippelreplay soundmonreplay tfmxreplay)
	get_target_property(FILES ${REPLAYER} SOURCES)
	foreach(FILE ${FILES})
		get_filename_component(FILE ${PROJECT_SOURCE_DIR}/${REPLAYER}/${FILE} ABSOLUTE)
		list(APPEND SOURCES ${FILE})
	endforeach()
endforeach()
list(REMOVE_DUPLICATES SOURCES)

replay_library(amigareplay PREFIX AMIGAREPLAY
	SOURCES
		src/amigareplay.cpp
		src/format_hippel.cpp
		src/format_soundmon.cpp
		src/format_tfmx.cpp
		${SOURCES}
)
target_include_directories(amigareplay PRIVATE ../hippelreplay/include ../soundmonreplay/include ../tfmxreplay/include)

if(AMIGA_REPLAYERS_TOOLS)
	add_executable(amigarender tools/amigarender.c)
	target_link_libraries(amigarender PRIVATE amigareplay)
	if(MSVC)
		target_compile_definitions(amigarender PRIVATE _CRT_SECURE_NO_WARNINGS)
	endif()
	install(TARGETS amigarender RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

	# TFMXreplay's self-contained tests, for ctest. They need Python, so they're left out without it.
	find_package(Python3 COMPONENTS Interpreter QUIET)
	if(Python3_Interpreter_FOUND)
		add_test(NAME tfmx-selftest COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tfmxreplay/tests/selftest.py
			--tool $<TARGET_FILE:tfmxtrace> --render $<TARGET_FILE:amigarender>)
	endif()
endif()

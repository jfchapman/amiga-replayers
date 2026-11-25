# replay_library(<name> PREFIX <prefix> TOOLS <prefix of the tools' names> SOURCES <files>)
# One replayer's library, and its render and trace tools, from the folder's include, src and tools.
function(replay_library NAME)
	cmake_parse_arguments(ARG "" "PREFIX;TOOLS" "SOURCES" ${ARGN})
	if(AMIGA_REPLAYERS_SHARED)
		add_library(${NAME} SHARED ${ARG_SOURCES})
		target_compile_definitions(${NAME} PUBLIC ${ARG_PREFIX}_SHARED)  # its API file defines ${ARG_PREFIX}_BUILDING itself
		set_target_properties(${NAME} PROPERTIES CXX_VISIBILITY_PRESET hidden VERSION ${PROJECT_VERSION} SOVERSION 0)
	else()
		add_library(${NAME} STATIC ${ARG_SOURCES})
	endif()
	add_library(AmigaReplayers::${NAME} ALIAS ${NAME})

	target_include_directories(${NAME}
		PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/common>
			$<INSTALL_INTERFACE:include>
		PRIVATE src)
	set_target_properties(${NAME} PROPERTIES PUBLIC_HEADER include/${NAME}.h)

	# Link the C runtime statically by default, like most audio codec builds for Windows plugins.
	set(RUNTIME "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	if(MSVC)
		target_compile_options(${NAME} PRIVATE /W4)
		set_property(TARGET ${NAME} PROPERTY MSVC_RUNTIME_LIBRARY ${RUNTIME})
	else()
		target_compile_options(${NAME} PRIVATE -Wall -Wextra)
	endif()
	install(TARGETS ${NAME} EXPORT AmigaReplayersTargets
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
		PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

	if(AMIGA_REPLAYERS_TOOLS)
		set(RENDER ${ARG_TOOLS}render)
		set(TRACE ${ARG_TOOLS}trace)
		add_executable(${RENDER} tools/${RENDER}.c)
		target_link_libraries(${RENDER} PRIVATE ${NAME})
		# The trace tool uses the library's internal classes, which a shared library doesn't export, so it builds
		# the sources itself in that case.
		if(AMIGA_REPLAYERS_SHARED)
			add_executable(${TRACE} tools/${TRACE}.cpp ${ARG_SOURCES})
			target_include_directories(${TRACE} PRIVATE include ${PROJECT_SOURCE_DIR}/common)
		else()
			add_executable(${TRACE} tools/${TRACE}.cpp)
			target_link_libraries(${TRACE} PRIVATE ${NAME})
		endif()
		target_include_directories(${TRACE} PRIVATE src)
		if(MSVC)
			set_property(TARGET ${RENDER} ${TRACE} PROPERTY MSVC_RUNTIME_LIBRARY ${RUNTIME})
			target_compile_definitions(${RENDER} PRIVATE _CRT_SECURE_NO_WARNINGS)
			target_compile_definitions(${TRACE} PRIVATE _CRT_SECURE_NO_WARNINGS)
		endif()
		install(TARGETS ${RENDER} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
	endif()
endfunction()

cmake_minimum_required(VERSION 3.16)
project(HippelReplay VERSION 0.1.0 LANGUAGES C CXX)

option(HIPPEL_BUILD_SHARED "Build a shared library (DLL / .so / .dylib) instead of a static one" OFF)
option(HIPPEL_BUILD_TOOLS "Build hippelrender and hippeltrace" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 99)

set(HIPPEL_SOURCES
	src/mixer.cpp
	src/module.cpp
	src/paula.cpp
	src/replayer.cpp
	src/hippelreplay.cpp
)

if(HIPPEL_BUILD_SHARED)
	add_library(hippelreplay SHARED ${HIPPEL_SOURCES})
	target_compile_definitions(hippelreplay PUBLIC HIPPEL_SHARED)  # hippelreplay.cpp defines HIPPEL_BUILDING itself
	set_target_properties(hippelreplay PROPERTIES CXX_VISIBILITY_PRESET hidden VERSION ${PROJECT_VERSION} SOVERSION 0)
else()
	add_library(hippelreplay STATIC ${HIPPEL_SOURCES})
endif()
add_library(HippelReplay::hippelreplay ALIAS hippelreplay)

target_include_directories(hippelreplay
	PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>
	PRIVATE src)
set_target_properties(hippelreplay PROPERTIES PUBLIC_HEADER include/hippelreplay.h)

if(MSVC)
	target_compile_options(hippelreplay PRIVATE /W4)
	set_property(TARGET hippelreplay PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
	target_compile_options(hippelreplay PRIVATE -Wall -Wextra)
endif()

if(HIPPEL_BUILD_TOOLS)
	add_executable(hippelrender tools/hippelrender.c)
	target_link_libraries(hippelrender PRIVATE hippelreplay)
	# hippeltrace uses the library's internal classes, which a shared library doesn't export, so
	# it builds the sources itself in that case.
	if(HIPPEL_BUILD_SHARED)
		add_executable(hippeltrace tools/hippeltrace.cpp ${HIPPEL_SOURCES})
		target_include_directories(hippeltrace PRIVATE include src)
	else()
		add_executable(hippeltrace tools/hippeltrace.cpp)
		target_link_libraries(hippeltrace PRIVATE hippelreplay)
		target_include_directories(hippeltrace PRIVATE src)
	endif()
	if(MSVC)
		set_property(TARGET hippelrender hippeltrace PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
		target_compile_definitions(hippelrender PRIVATE _CRT_SECURE_NO_WARNINGS)
		target_compile_definitions(hippeltrace PRIVATE _CRT_SECURE_NO_WARNINGS)
	endif()
endif()

include(GNUInstallDirs)
install(TARGETS hippelreplay EXPORT HippelReplayTargets
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT HippelReplayTargets NAMESPACE HippelReplay:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/HippelReplay)
include(CMakePackageConfigHelpers)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/HippelReplayConfig.cmake
	"include(\"\${CMAKE_CURRENT_LIST_DIR}/HippelReplayTargets.cmake\")\n")
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/HippelReplayConfigVersion.cmake COMPATIBILITY SameMinorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/HippelReplayConfig.cmake ${CMAKE_CURRENT_BINARY_DIR}/HippelReplayConfigVersion.cmake
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/HippelReplay)
if(HIPPEL_BUILD_TOOLS)
	install(TARGETS hippelrender RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

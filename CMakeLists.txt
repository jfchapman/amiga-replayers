cmake_minimum_required(VERSION 3.16)

file(READ common/amiga_replayers_version.h VERSION_HEADER)
foreach(PART MAJOR MINOR PATCH)
	string(REGEX MATCH "AMIGA_REPLAYERS_VERSION_${PART} ([0-9]+)" _ "${VERSION_HEADER}")
	set(VERSION_${PART} ${CMAKE_MATCH_1})
endforeach()
project(AmigaReplayers VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH} LANGUAGES C CXX)

option(AMIGA_REPLAYERS_SHARED "Build shared libraries (DLL / .so / .dylib) instead of static ones" OFF)
option(AMIGA_REPLAYERS_TOOLS "Build the render and trace tools" ON)
option(AMIGA_REPLAYERS_TFMX "Build TFMXreplay" ON)
option(AMIGA_REPLAYERS_SOUNDMON "Build SoundMonReplay" ON)
option(AMIGA_REPLAYERS_HIPPEL "Build HippelReplay" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 99)
# Everything goes in one folder, where the test scripts look for the tools.
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(common/Library.cmake)
enable_testing()

if(AMIGA_REPLAYERS_TFMX)
	add_subdirectory(tfmxreplay)
endif()
if(AMIGA_REPLAYERS_SOUNDMON)
	add_subdirectory(soundmonreplay)
endif()
if(AMIGA_REPLAYERS_HIPPEL)
	add_subdirectory(hippelreplay)
endif()

install(FILES common/amiga_replayers_version.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT AmigaReplayersTargets NAMESPACE AmigaReplayers:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AmigaReplayers)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/AmigaReplayersConfig.cmake
	"include(\"\${CMAKE_CURRENT_LIST_DIR}/AmigaReplayersTargets.cmake\")\n")
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/AmigaReplayersConfigVersion.cmake COMPATIBILITY SameMinorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/AmigaReplayersConfig.cmake ${CMAKE_CURRENT_BINARY_DIR}/AmigaReplayersConfigVersion.cmake
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AmigaReplayers)

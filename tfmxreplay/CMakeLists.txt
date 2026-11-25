replay_library(tfmxreplay PREFIX TFMX TOOLS tfmx
	SOURCES
		../common/output.cpp
		src/module.cpp
		src/tfmx_paula.cpp
		src/replayer.cpp
		src/tfmxreplay.cpp
		src/variant.cpp
)

# The self-contained tests, for ctest. They need Python, so they're left out without it.
if(AMIGA_REPLAYERS_TOOLS)
	find_package(Python3 COMPONENTS Interpreter QUIET)
	if(Python3_Interpreter_FOUND)
		add_test(NAME tfmx-selftest COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/tests/selftest.py
			--tool $<TARGET_FILE:tfmxtrace> --render $<TARGET_FILE:tfmxrender>)
	endif()
endif()

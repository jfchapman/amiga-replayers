@echo off
rem Builds build\wasm\amigareplay.wasm with Emscripten. Run it from the repository's root, with em++ on the path
rem (emsdk_env.bat sets that up). It's a single file that needs no JavaScript from Emscripten.
setlocal
if not exist build\wasm mkdir build\wasm
set SOURCES=common/output.cpp common/paula.cpp
set SOURCES=%SOURCES% tfmxreplay/src/module.cpp tfmxreplay/src/tfmx_paula.cpp tfmxreplay/src/replayer.cpp tfmxreplay/src/tfmxreplay.cpp tfmxreplay/src/variant.cpp
set SOURCES=%SOURCES% soundmonreplay/src/module.cpp soundmonreplay/src/replayer.cpp soundmonreplay/src/soundmonreplay.cpp
set SOURCES=%SOURCES% hippelreplay/src/mixer.cpp hippelreplay/src/module.cpp hippelreplay/src/replayer.cpp hippelreplay/src/hippelreplay.cpp
set SOURCES=%SOURCES% amigareplay/src/amigareplay.cpp amigareplay/src/format_hippel.cpp amigareplay/src/format_soundmon.cpp amigareplay/src/format_tfmx.cpp amigareplay/wasm/amigareplay_wasm.cpp
call em++ -O3 -std=c++17 -Icommon -Itfmxreplay/include -Isoundmonreplay/include -Ihippelreplay/include -Iamigareplay/include %SOURCES% ^
	--no-entry -sSTANDALONE_WASM -sFILESYSTEM=0 -sALLOW_MEMORY_GROWTH=1 ^
	-sEXPORTED_FUNCTIONS=_malloc,_free,_amigareplay_version,_amigareplay_wasm_open,_amigareplay_wasm_configure,_amigareplay_close,_amigareplay_subsong_count,_amigareplay_subsong_from_id,_amigareplay_select_subsong,_amigareplay_render_float,_amigareplay_seek ^
	-o build/wasm/amigareplay.wasm || exit /b 1
echo Built build\wasm\amigareplay.wasm
exit /b 0

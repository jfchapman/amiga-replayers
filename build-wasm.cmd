@echo off
rem Builds build\wasm\*.wasm, one per library, with Emscripten. Run it from the repository's root, with em++ on the path
rem (emsdk_env.bat sets that up). Each is a single file that needs no JavaScript from Emscripten.
setlocal
if not exist build\wasm mkdir build\wasm
call :build tfmxreplay "common/output.cpp tfmxreplay/src/module.cpp tfmxreplay/src/tfmx_paula.cpp tfmxreplay/src/replayer.cpp tfmxreplay/src/tfmxreplay.cpp tfmxreplay/src/variant.cpp" "_tfmx_version,_tfmx_wasm_open,_tfmx_wasm_configure,_tfmx_close,_tfmx_subsong_index,_tfmx_select_subsong,_tfmx_render_float,_tfmx_seek" || exit /b 1
call :build soundmonreplay "soundmonreplay/src/module.cpp common/output.cpp common/paula.cpp soundmonreplay/src/replayer.cpp soundmonreplay/src/soundmonreplay.cpp" "_smon_version,_smon_wasm_open,_smon_wasm_configure,_smon_close,_smon_subsong_count,_smon_select_subsong,_smon_render_float,_smon_seek" || exit /b 1
call :build hippelreplay "hippelreplay/src/mixer.cpp hippelreplay/src/module.cpp common/output.cpp common/paula.cpp hippelreplay/src/replayer.cpp hippelreplay/src/hippelreplay.cpp" "_hippel_version,_hippel_wasm_open,_hippel_wasm_configure,_hippel_close,_hippel_subsong_count,_hippel_select_subsong,_hippel_render_float,_hippel_seek" || exit /b 1
exit /b 0

rem :build <library> "<sources>" "<exported functions>"
:build
call em++ -O3 -std=c++17 -Icommon -I%1/include -I%1/src %~2 %1/wasm/%1_wasm.cpp ^
	--no-entry -sSTANDALONE_WASM -sFILESYSTEM=0 -sALLOW_MEMORY_GROWTH=1 ^
	-sEXPORTED_FUNCTIONS=_malloc,_free,%~3 ^
	-o build/wasm/%1.wasm || exit /b 1
echo Built build\wasm\%1.wasm
exit /b 0

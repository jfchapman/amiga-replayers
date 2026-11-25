@echo off
rem Builds build\wasm\hippelreplay.wasm with Emscripten. Run it from the repository's root, with em++ on the path
rem (emsdk_env.bat sets that up). The result is a single file that needs no JavaScript from Emscripten.
setlocal
if not exist build\wasm mkdir build\wasm
call em++ -O3 -std=c++17 -Iinclude -Isrc ^
	src/mixer.cpp src/module.cpp src/paula.cpp src/replayer.cpp src/hippelreplay.cpp wasm/hippelreplay_wasm.cpp ^
	--no-entry -sSTANDALONE_WASM -sFILESYSTEM=0 -sALLOW_MEMORY_GROWTH=1 ^
	-sEXPORTED_FUNCTIONS=_malloc,_free,_hippel_version,_hippel_wasm_open,_hippel_wasm_configure,_hippel_close,_hippel_subsong_count,_hippel_select_subsong,_hippel_render_float,_hippel_seek ^
	-o build/wasm/hippelreplay.wasm || exit /b 1
echo Built build\wasm\hippelreplay.wasm

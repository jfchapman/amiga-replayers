# amiga-replayers

Libraries for playing Amiga music formats of the late 1980s and early 1990s:

| Library | Format | Games |
|---|---|---|
| [TFMXreplay](tfmxreplay/README.md) | TFMX (Chris Hülsbeck and others) | *Turrican*, *Turrican 2*, *Apidya* |
| [SoundMonReplay](soundmonreplay/README.md) | BP SoundMon 1, 2 and 3 (Brian Postma) | *Alien Breed*, *Plexu*, *Fantasy World Dizzy* |
| [HippelReplay](hippelreplay/README.md) | Hippel, Hippel 7V and Hippel COSO (Jochen Hippel) | *Wings of Death*, *Lethal Xcess*, *Enchanted Land* |

Each format is described in its library's `docs` folder, in enough detail to write a player from.

[amigareplay](amigareplay/README.md) is one library for them all: it finds out what kind of song it's given, and plays
it. Use that, or a single one of the others if it's all you need. They have a lot in common:

- A plain C API (`include/*.h`), static or shared library, no dependencies.
- Subsongs, with lengths and loop points, and seeking. They tell songs that loop from ones that stop.
- Output options: sample rate, stereo separation, interpolation (none, linear, cubic), Amiga output filters (A500,
  A1200 or none, plus the LED filter), anti-click volume ramping, loop count, fade-out, gain.
- Clean-room replayers: written from descriptions of the formats, and checked tick by tick against the original
  replayers running in an Amiga emulator, without reading any replayer's code. Each README says how closely they match.

For an example web-based player that uses them, please visit [modplayer](https://mods.chapzero.com).

## Building

With CMake (tested with MSVC, x64 and 32-bit):

```
cmake -S . -B build
cmake --build build --config Release
```

Options:
- `-DAMIGA_REPLAYERS_SHARED=ON` builds shared libraries (DLL / .so / .dylib).
- `-DAMIGA_REPLAYERS_TOOLS=OFF` leaves out the tools.
- `-DAMIGA_REPLAYERS_TFMX=OFF`, `-DAMIGA_REPLAYERS_SOUNDMON=OFF` and `-DAMIGA_REPLAYERS_HIPPEL=OFF` leave out a library.
  amigareplay is only built with all three.

For a 32-bit Windows build, add `-A Win32` with a Visual Studio generator. `cmake --install build` installs the headers,
libraries and a CMake package, so another project can use `find_package(AmigaReplayers)` and link
`AmigaReplayers::amigareplay`, which includes the others, or `AmigaReplayers::tfmxreplay`,
`AmigaReplayers::soundmonreplay` or `AmigaReplayers::hippelreplay`.

Without CMake, with GNU make and gcc or clang: `make` (static libraries and tools) or `make shared`.

To compile a library's sources in your own project, add its `src` files and `common/*.cpp` (for amigareplay, all the
libraries'), with its `include` folder (for amigareplay, all of them) and `common` on the include path.

For the web, `build-wasm.cmd` builds `build\wasm\amigareplay.wasm` with [Emscripten](https://emscripten.org/) (run it
from the repository's root, with `em++` on the path). It needs no JavaScript from Emscripten: instantiate it, call its
`_initialize` export, and give it a do-nothing `env.emscripten_notify_memory_growth` import. `amigareplay/wasm` adds
functions that take the settings as plain numbers.

## Rendering songs

`amigarender` renders a song of any of the formats to a WAV file. It goes by what's in the file, so the name doesn't
matter:

```
amigarender "mdat.turrican 2 level 1-desert"     # finds smpl.* itself
amigarender -a -o out song.hip                   # every subsong: out-0.wav, out-1.wav, ...
amigarender -l 1 -F 10 -o out.wav bp13.mysong    # plays it twice, then fades out
amigarender -n song.hip                          # only lists the subsongs and their lengths
```

Run it without arguments for all the options. It's also the example of using amigareplay: it uses only the public API.

## Tests

Each library's `tests` folder has its own. Most need the original replayers running from game memory, which can't be
distributed as they come from the games themselves. `ctest` runs the ones that don't (it needs Python).

`tests/wav` has tools for comparing renders with recordings from an emulator, run with Node.js (no packages needed):
`locate.js` finds a song in a recording and measures how well a render fits it, `voicefit.js` fits each voice separately,
`specfit.js` compares songs tick by tick by level and spectrum (so sub-millisecond timing differences don't count),
`transfer.js` shows the match by frequency, `cut.js` copies and filters stretches of WAV files, and `splice.js` makes a
blind listening test: random stretches of two or more versions joined into one file, with an answer key. Each prints its
usage when run without arguments.

## AI Disclaimer

This project uses AI development tools (Claude Code by Anthropic).

## Licence

MIT: see [LICENSE](LICENSE).

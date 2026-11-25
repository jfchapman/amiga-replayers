# amigareplay

One library for all the formats here: it finds out what kind of song it's given, and plays it with
[TFMXreplay](../tfmxreplay/README.md), [SoundMonReplay](../soundmonreplay/README.md) or
[HippelReplay](../hippelreplay/README.md). It includes them, so it's the only one a program needs, and the program
doesn't have to know about the formats: later versions can add more without changing the API.

Building it is described in [the main README](../README.md).

## Using the library

```c
#include "amigareplay.h"

amigareplay_settings settings;
amigareplay_default_settings(&settings, sizeof settings);
settings.sample_rate = 48000;

amigareplay_error error;
amigareplay_module *module = amigareplay_open_memory(data, size, &settings, &error);
if (!module) { /* amigareplay_error_string(error) */ }

int16_t buffer[2 * 1024];
size_t frames;
while ((frames = amigareplay_render_s16(module, buffer, 1024)) > 0) {
	/* 'frames' stereo frames */
}
amigareplay_close(module);
```

The header documents the rest. [amigarender](tools/amigarender.c) is a complete example.

- **Songs of several files:** TFMX songs have their samples in a file of their own. `amigareplay_companion_file` gives
  the names of a song's other files, and `amigareplay_open` takes them all. A song of one file can be opened this way
  too, so a program can treat every song the same.
- **Detection:** `amigareplay_probe` needs only the start of a file.
- **Settings** are what every format has. **Options** are what only some do (such as the replayer's build), as names and
  values. `amigareplay_option_name` and the rest list them, so a program can offer them all without knowing any.
- **Subsongs** go by index. Each also has an `id` to store, for finding it again with a later version.

## Adding a format

A format is a file in `src`, `format_<name>.cpp`: a `Player` for an open song, and a `Format` that detects and opens
them (`format.h`). Add it to the list in `amigareplay.cpp`, and its sources to `CMakeLists.txt`, `Makefile` and
`build-wasm.cmd`.

## Tests

`python tests/api.py <songs>` tests the API through the DLL (`-DAMIGA_REPLAYERS_SHARED=ON`, 64-bit).

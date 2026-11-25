# HippelReplay

A library for playing Jochen Hippel's Amiga music: the format he wrote for games such as *Wings of
Death*, *Lethal Xcess*, *Enchanted Land*, *A Prehistoric Tale*, *Dragonflight* and *Leavin' Teramis*.

- All of the Amiga forms: `.hip` and `.hip7` files, with or without their replayer in front, and COSO
  (`.hipc`) files. Songs with 4 voices, or with 7, four of which are mixed in software as the original
  does.
- The different replayer generations, detected from the song or chosen.
- Subsongs, with lengths and loop points, seeking, and sample names. Finds subsongs that repeat another's
  music, and tells songs that loop from ones that stop.
- Output options: sample rate, stereo separation, interpolation (none, linear, cubic),
  Amiga output filters (A500, A1200 or none, plus the LED filter), anti-click volume ramping,
  loop count, fade-out, gain.
- A clean-room replayer: written from a description of the format, and checked tick by tick
  against the original replayers running in an Amiga emulator, without reading any replayer's
  code. See Status.

The Atari ST versions aren't supported: they drive a different sound chip.

The format is described in [docs/Hippel-format.md](docs/Hippel-format.md). Building it, and what it shares with the other
libraries here, is in [the main README](../README.md).

## Using it

```c
#include "hippelreplay.h"

hippel_settings settings;
hippel_default_settings(&settings);
settings.sample_rate = 48000;

hippel_error error;
hippel_module *module = hippel_open(data, size, &settings, &error);
if (!module) { /* hippel_error_string(error) */ }

int16_t buffer[2 * 1024];
size_t frames;
while ((frames = hippel_render_s16(module, buffer, 1024)) > 0) {
	/* 'frames' stereo frames */
}
hippel_close(module);
```

The header documents the rest:
- **Subsongs:** a file can hold several. `hippel_subsong_count` and `hippel_get_subsong_info` give each
  one's number, length and loop, and whether it only repeats music in another (`repeat_of`).
  `hippel_select_subsong` picks one. Entries that play nothing are left out.
- **Settings:** loops and fade-out, interpolation, stereo separation, the Amiga's output filters, and
  the replayer's rules.
- **Playback control:** seeking, and muting channels.

### Replayers

Hippel's replayers differ, mostly in their vibrato and slides, so the same data sounds different on the
wrong one. A `.hip` file has its replayer in front, and the library goes by its length. A COSO file
doesn't, so the library goes by what the song uses (section 10 of the format document).
`hippel_settings.replayer` overrides the choice.

| Rules | Where they come from |
|---|---|
| `older` | The early .hip builds, and *Astaroth* |
| `newer` | The later .hip builds (*Leavin' Teramis*) |
| `2708` | The *Wings of Death* build |
| `COSO` | The later COSO replayers (*Enchanted Land*, *A Prehistoric Tale*, *Lethal Xcess*), and the 7-voice ones |

## Tools

- `amigarender` renders songs to WAV files ([the main README](../README.md) describes it).
- `hippeltrace song [subsong] [ticks] [replayer]` prints each voice's Paula state per tick, for
  comparison with the original replayers.

## Tests

`tests/compare.py` compares the library with the original replayers, tick by tick. It needs the original
replayers running from the songs' files and game memory, and the test rig for that, which can't be
distributed as they come from the games themselves. The format document's section 12 describes the method.

## Status

Compared against the original replayers:

- **.hip files:** every subsong of modland's 62 files with the standard layout (139) matches its own
  replayer exactly over 1,500 ticks (30 seconds), and 134 over 6,000. Of the other five, three read past
  the replayer's period table into its own memory, and two are broken entries of the subsong table.
- **COSO files:** 13 savestates of games match exactly: *A Prehistoric Tale*, *Ghost Battle*, *Astaroth*,
  *Return of Medusa*, *Enchanted Land* and *Lethal Xcess*.
- **7 voices:** *Amberstar*'s and *Lethal Xcess*'s intros match their games: the three hardware voices
  exactly over 8,000 ticks, and the software mix apart from single samples now and then.
- **Recordings:** WinUAE recordings of 13 songs from 7 games, with every set of rules, match in level and
  tone, and a blind listening test, splicing recordings and the library's output together, found no
  audible difference in the five songs tried.

Known differences from the Amiga:
- All voices are played, at the volumes in the song. Some games play only some of them, to leave channels
  for sound effects, or scale the music's volume.
- 4-voice songs play at the PAL frame rate (49.92 Hz). A few games used a timer instead (49.54 Hz in
  *Ghost Battle*), which the song doesn't say.
- The speech 2:16 into *Wings of Death*'s title song plays here. In the game only its loop is heard, because
  the replayer restarts the channel too quickly for Paula to notice.
- In *Amberstar*'s title, the size of the random drop in some drums' volume differs.

`astaroth.hip` has a layout of its own and isn't supported (its songs are in modland as
`astaroth123.hipc` and `astaroth4.hipc`), nor are four of modland's *Cryptoburners* files, which are
damaged.

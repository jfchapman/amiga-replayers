# The Jochen Hippel music format

Jochen Hippel ("Mad Max" of The Exceptions) wrote music and replayers for the Atari ST and the Amiga from the late 1980s to the early 1990s, for Thalion and others. This document describes his Amiga format, used in games such as *Wings of Death*, *Lethal Xcess*, *Enchanted Land*, *A Prehistoric Tale*, *Dragonflight* and *Leavin' Teramis*. Songs are sequences of patterns for four voices, or for seven, four of which are mixed in software. Every instrument is a pair of small scripts, one for volume and one for pitch and samples. There was never a public specification, and each game carried its own build of the replayer, so "the format" is a family of closely related behaviours.

This document describes the format as far as it's known: the file layouts, what every command does, and where the replayers differ. It's detailed enough to write a player from, and [HippelReplay](../README.md) was written from it. Section 10 sets out how the replayers differ, section 12 explains how the behaviour was measured, and section 13 lists what's still unknown.

**Conventions.** All values are big-endian. "Word" means 16 bits and "long" 32 bits. `$` means hexadecimal. Voices are numbered from 1. Counts of files refer to the modland collection: 63 files in `Hippel` (`.hip`), 6 in `Hippel 7V` (`.hip7`) and 92 in `Hippel COSO` (`.hipc`). The Atari ST versions (`Hippel ST`, `Hippel ST COSO`), which drive a different sound chip, aren't covered.

## Contents

1. [Concepts](#1-concepts)
2. [Files](#2-files)
3. [Layouts](#3-layouts)
4. [Timing and start state](#4-timing-and-start-state)
5. [Track rows and patterns](#5-track-rows-and-patterns)
6. [Instruments](#6-instruments)
7. [Periods, vibrato and slide](#7-periods-vibrato-and-slide)
8. [Seven voices](#8-seven-voices)
9. [The tick, in order](#9-the-tick-in-order)
10. [Replayers](#10-replayers)
11. [Subsongs, ends and loops](#11-subsongs-ends-and-loops)
12. [How this was measured](#12-how-this-was-measured)
13. [Open questions](#13-open-questions)

---

## 1. Concepts

- **Track row:** one line of the song's sequence. For each voice it names a pattern, a transpose for its notes and one for its instruments (the *sound transpose*).
- **Pattern:** 32 rows, each with an optional note and an info byte.
- **Row:** one line of a pattern. A row lasts *speed* ticks.
- **Tick:** one call of the replayer, normally once a frame.
- **Volume sequence:** a script of volumes. It also names the frequency sequence to play, and sets the vibrato, so an *instrument* is a volume sequence.
- **Frequency sequence:** a script of pitches, one per tick, with commands that start and change samples.
- **Sample:** a piece of 8-bit sample data, with an optional loop. Short synthetic waveforms and digitised sounds are kept in one list.
- **Subsong:** a range of track rows with a speed.

Songs of one form can play by several sets of rules, depending on the replayer they were written for. This document calls the sets **older**, **newer**, **2708** and **COSO** (§10). Where nothing is said, a rule holds for all of them.

---

## 2. Files

| Form | Files | Content |
|---|---|---|
| `.hip` | 63 | Usually the replayer's 68000 code (2.2–2.8 KB), followed by the song, which starts with the tag `TFMX` |
| `.hip7` | 6 | The 7-voice form. Four have about 32 KB of replayer code in front, two are the song only |
| `.hipc` | 92 | COSO ("COmpressed SOng"): the same song in a more compact layout, with no code. Five are 7-voice songs |

The code in front isn't part of the format: the songs were copied from memory with their replayer. Despite the tag, this is not Chris Hülsbeck's TFMX format.

**Recognising a song.** Find `TFMX` at an even offset in the file's first 64 KB, with bytes 20–31 from the tag all zero and an even pattern size (bytes 12–13) from 2 to 256. Hülsbeck's files start with the same four letters but don't fit. If the 32 bytes before the tag start with `COSO`, it's a COSO song.

**The same song in two forms.** Twenty `.hip` and `.hip7` songs also exist as `.hipc`, some of them revised between the two.

---

## 3. Layouts

### 3.1 The header

Both forms share it (offsets from `TFMX`):

| Offset | Size | Content |
|---|---|---|
| 0 | 4 | `TFMX` |
| 4 | 2 | c0: number of frequency sequences − 1 |
| 6 | 2 | c1: number of volume sequences − 1 |
| 8 | 2 | c2: number of patterns − 1 |
| 10 | 2 | c3: number of track rows − 1 |
| 12 | 2 | c4: pattern size in bytes: 64 in every song |
| 14 | 2 | c5: unknown, with no effect seen. 0 in the `.hip7` songs and in 29 others, 2–9 in the rest |
| 16 | 2 | c6: number of subsongs |
| 18 | 2 | c7: number of samples |
| 20 | 12 | Zero |

### 3.2 `.hip` and `.hip7`

The sections follow the header in this order:

| Size | Content |
|---|---|
| (c0 + 1) × 64 | Frequency sequences |
| (c1 + 1) × 64 | Volume sequences |
| (c2 + 1) × c4 | Patterns: 32 rows of 2 bytes, note and info |
| (c3 + 1) × 12 (7 voices: × 28) | Track rows (§5.1, §8.1) |
| (c6 + 1) × 6 (7 voices: × 8) | Subsongs (§3.4), ending with a zero entry |
| c7 × 30 | Sample headers |
| | Sample data |

Nothing in the header says how many voices there are. The sections' sizes do: with the wrong number, the sample headers don't make sense (their offsets and lengths run far past the end of the file).

**Sample header (30 bytes):**

| Offset | Size | Field |
|---|---|---|
| 0 | 18 | Name, padded with zeros |
| 18 | 4 | Offset from the start of the sample data, in bytes |
| 22 | 2 | Length in words |
| 24 | 2 | No effect: 0, or $0040 in some samples |
| 26 | 2 | Loop start in bytes, from the start of the sample |
| 28 | 2 | Loop length in words. 1 means no loop |

### 3.3 COSO

Offsets are from `COSO`, and so are all the pointers in the file:

| Offset | Size | Content |
|---|---|---|
| 0 | 4 | `COSO` |
| 4 | 28 | Seven long offsets: frequency sequences, volume sequences, patterns, track rows, subsongs, sample headers, sample data |
| 32 | 32 | The header (§3.1) |
| 64 | | The sections |

- **Sequences:** a table of c0 + 1 (or c1 + 1) words, the offset of each sequence, then the sequences: the same bytes as in the `.hip` form without the padding to 64.
- **Patterns:** a table of c2 + 1 words, the offset of each pattern, then the patterns as lists of events (below).
- **Track rows and subsongs:** as in the `.hip` form.
- **Sample headers:** 10 bytes, a `.hip` sample header without its name, its length following its offset directly: offset (long, bytes), length (words), loop start (bytes), loop length (words). There's normally one header more than c7.
- **Seven voices:** the track rows section is at least 28 × (c3 + 1) bytes long.

**Pattern events.** Each event takes up a row, plus a gap of empty rows after it:

| Bytes | Meaning |
|---|---|
| `note info` | An event. If the info byte has bit $20 or $40 set, a third byte, *P*, follows (§5.2) |
| `FE n` | The gap after each following event is *n* rows |
| `FD n` | An empty event, and the gap is *n* from here on (so *n* + 1 empty rows) |
| `FF` | The end. Before row 32, it's a pattern break (§5.2) |

For example `FE 03 | 0C 00 | 0C 01 | FE 01 | 16 20 7F | 98 00 | FF` puts events on rows 0, 4, 8 and 10. An empty pattern is `FD 1F FF`.

### 3.4 Subsongs

| Offset | Size | 4 voices | 7 voices |
|---|---|---|---|
| 0 | 2 | First track row | First track row |
| 2 | 2 | Last track row | Last track row |
| 4 | 2 | Speed: ticks per row | Unknown: the last row again, or 1 |
| 6 | 2 | | Speed |

The 2708 build steps through the table 4 bytes at a time, so it misreads every subsong after the first. The other replayers, and the games' own use of the subsongs, show that 6 bytes is right.

---

## 4. Timing and start state

- **Ticks:** 4-voice songs were normally played once per vertical blank. On a PAL Amiga a frame is 313 lines of 227 colour clocks, so a tick is 20.03 ms (49.92 Hz). Some games used a CIA timer instead: *Ghost Battle* and *Return of Medusa* run at 49.54 Hz. Nothing in the song says which, and only the tempo depends on it. 7-voice songs set their own rate (§8.2).
- **First row:** the older rules play row 0 on tick *speed* − 1, counting from 0; the others on tick 0.
- **Voices** are silent until their first note. Under the COSO rules each voice starts with frequency sequence 0 current, which matters if its first note keeps the sound (§6.1).

---

## 5. Track rows and patterns

### 5.1 Track rows

Three bytes per voice, voice 1 first (voices 1 and 4 play on the left, 2 and 3 on the right):

| Byte | Field |
|---|---|
| 0 | Pattern |
| 1 | Transpose (signed), added to the note |
| 2 | Sound transpose, added to the instrument number (kept to 8 bits) |

**Transpose** applies on every tick, so a note still playing when a new track row starts takes the new row's transpose.

**Sound transpose $80** on any voice ends the song: from that track row's second tick every voice is silent, and nothing more is played. This is how a subsong plays only once.

**COSO rules: sound transposes above $80** are commands, and the voice keeps the sound transpose it had. They're read on every row:

| Value | Meaning |
|---|---|
| $E1–$EF | Speed: the low nibble is the number of ticks per row, from the next row on. It stays in force when the song loops |
| $F0–$FF | Level: the low nibble *n* scales the voice's volume from here on, until another level. 0 is full volume; for 1–15, volume × (16 − *n*) × 3 / 50, rounded down |

Levels are used for fades written into the track rows (FA F8 F5 F3 F1 F0 in, F2 F4 F6 F8 FA FC FF out). Values $81–$E0 don't occur. Under the other rules, values above $80 are ordinary transposes.

### 5.2 Patterns

| Note | Meaning |
|---|---|
| 0 | Nothing. If the info byte isn't 0, it's a parameter for the next row |
| 1 | On voice 1, a **pattern break**: the next track row starts on this row, for every voice. Elsewhere, nothing |
| $02–$7F | A note |
| $80–$FF | **Legato:** the pitch changes to the note (bits 0–6), and the sample and both sequences carry on |

**Info byte**, on a row with a note: bits 0–4 are the instrument (the sound transpose is added), and bits 5 and 6 ask for the parameter *P*:

| Bit | Meaning |
|---|---|
| $20 | **Slide** by *P* (§7.3). A legato note with it starts a slide, and one without it stops any slide |
| $40 | Play the instrument's volume sequence with frequency sequence *P* instead of its own. The older rules ignore it, and so does a legato note |

***P*** is, in the `.hip` form, the info byte of the row before in the same pattern (row 0 takes row 31's). In COSO it's the event's third byte.

After row 31, or a break, comes the next track row.

---

## 6. Instruments

A note's instrument number, info bits 0–4 plus the sound transpose, is the number of a volume sequence. Both kinds of sequence are positions in one block of data rather than separate buffers: one that has no end command runs on into the one after it.

### 6.1 Volume sequences

| Byte | Field |
|---|---|
| 0 | Speed: ticks per step (0 = 256) |
| 1 | Frequency sequence. COSO rules: bit 7 = **keep the sound** |
| 2 | Vibrato speed |
| 3 | Vibrato depth |
| 4 | Vibrato delay in ticks |
| 5… | Steps: volumes, written to Paula as they are |

| Command | Meaning |
|---|---|
| `E0 n` | Go to byte *n* of the sequence, on the same tick |
| `E1` | End: the volume stays |
| `E8 n` | Hold the volume for *n* + *speed* − 1 ticks, counting this one |

A note starts the sequence at byte 5, and its first step is played on the note's tick.

**Keep the sound** (byte 1 bit 7, in 36 COSO files): the note restarts only the volume sequence and the vibrato. The frequency sequence, and so the sample, carries on. The first volume step then comes *speed* ticks after the note, not at once.

### 6.2 Frequency sequences

One pitch per tick. Commands are carried out on the way to it, on the same tick:

| Bytes | Meaning |
|---|---|
| $00–$7F | Pitch, added to the note and transpose |
| $80–$DF | Fixed pitch: the low 7 bits are the period table index (§7.1), whatever the note |
| `E0 n` | Go to byte *n* of the sequence |
| `E1` | End: the pitch stays |
| `E2 s` | Start sample *s* |
| `E3 a b` | Vibrato speed *a* and depth *b* (§7.2) |
| `E4 s` | Switch to sample *s* without a restart: its loop follows when the current block ends |
| `E5 s A B C D` | Start a scan of sample *s* (§6.4). 2708 and COSO rules |
| `E6 B C D` | Change the scan. 2708 and COSO rules |
| `E7 s` | Older rules: **go to frequency sequence** *s*. Others: start sample *s*, unless it's already playing (below) |
| `E8 n` | Hold the pitch for *n* ticks, counting this one |
| `E9 s n` | Start part *n* of sample *s* (§6.5). All but the older rules |
| `EA n` | Vary the volume at random (§8.4). 7 voices |

**Starting a sample** turns the channel's DMA off and on (LC and LEN = the whole sample), sets its loop, restarts the volume sequence at byte 5, and ends any scan. The vibrato carries on.

**`E7 s`** remembers the sample it started, per voice, and doesn't restart the same one again; it still ends a running scan. `E2` clears the memory. `E5` counts as having started its sample under the 2708 rules, and leaves the memory as it was under the COSO rules.

### 6.3 Loops

When a sample starts, its loop (LC and LEN) is written straight after under the newer, 2708 and COSO rules, and on the following tick under the older ones (except build 2262, which does it at once). Paula takes it when the first pass ends, so the difference only matters for a sample shorter than a tick. A loop of 1 word is as good as silence.

### 6.4 Sample scans

`E5 s A B C D` plays a moving window of a sample: *A*, *B* and *C* are words (*C* signed) and *D* a byte. The sample starts at word *A* with a loop of *B* words there, and every *D* + 1 ticks the window moves by *C* words. If the next position would put the window outside the sample, the window is written once more where it is, and the scan stops.

`E6 B C D` gives a running scan a new window length, step and delay. It carries on from where it is, with its first step *D* + 1 ticks later. Without a running scan it does nothing.

### 6.5 Samples in parts

`E9 s n` plays part *n* of a sample that starts with a header, once, with a one-word loop:

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | `SSMP` |
| 4 | 2 | Number of parts |
| 6 | 2 | 0 |
| 8 | 24 each | Per part: start and end (longs, in bytes from the end of the header), and a name (16 bytes) |

---

## 7. Periods, vibrato and slide

### 7.1 Periods

The table index is (note + transpose + pitch) and $7F, or a fixed pitch's low 7 bits:

```
 0  1712 1616 1524 1440 1356 1280 1208 1140 1076 1016  960  906
12   856  808  762  720  678  640  604  570  538  508  480  453
24   428  404  381  360  339  320  302  285  269  254  240  226
36   214  202  190  180  170  160  151  143  135  127  120  113
48   113 (12 times)
60  3424 3232 3048 2880 2712 2560 2416 2280 2152 2032 1920 1812
```

| Builds | Entries |
|---|---|
| 2402, 2320, 2366 | 54 |
| 2708 | 69 |
| 2470 | 84: the low octave twice |
| The rest | 72 |

Nothing is clamped. Past its table a replayer reads its own memory: the words 0, 1, 1, an address, and then the song from its tag. Only a handful of songs get there audibly, for a few ticks.

### 7.2 Vibrato

A position *p* moves between 0 and a top, from the note on. It starts at *depth* and goes down. Each tick, once the delay has run out, it moves by *speed*; below 0 it stops at 0 and turns, and at the top it turns the other way. While the delay runs, the offset is 0.

| Rules | Top | Turns at the top when | Period offset |
|---|---|---|---|
| Older, newer | 2 × depth | *p* ≥ top | (*p* − depth) × unit, where unit is 16, 8, 4, 2 for index octaves 0–3 and 1 above |
| 2708 | 2 × depth | *p* > top | *p* − depth |
| COSO | depth | *p* > top | ((*p* − depth / 2) × period) >> 10, rounded down, on the table's period; none if depth is 0 |

So with the older and newer rules the offset stays at its lowest for two ticks but turns at its highest at once, and with the others it stays two ticks at both ends.

Under the 2708 rules, bit 7 of the speed means half rate: the position moves on every other tick only, the first one skipped. `E3` changes only the speed and the depth, not the position or the direction.

### 7.3 Slide

*t* counts the ticks of the slide, 1 on the first. *P* is signed, and both results are truncated toward zero:

| Rules | Change to the period |
|---|---|
| Older, newer, 2708 | −*P* × *t* / 32 |
| COSO | −*P* × *t* × period / 1024, where the period already includes the vibrato |

---

## 8. Seven voices

Voices 1–3 play on Paula channels 1–3 as in a 4-voice song, and voices 4–7 are mixed into one buffer that channel 4 plays. Everything else follows the COSO rules, apart from what's below.

### 8.1 Track rows

28 bytes: four per voice, the first three as in §5.1, and a **level** byte:

| Level byte | Meaning |
|---|---|
| $F0–$FF | The voice's level, as the sound transposes $F0–$FF of §5.1 |
| $D0–$DF | Tempo (§8.2): the new value is the level byte of the *next* entry (the next voice's, or the next track row's first). Only *Amberstar*'s replayer does it, and only one song has it |
| Others | Nothing |

### 8.2 Rate and tempo

Track row 0 is a header and isn't played; subsongs start at row 1. Its byte 1 is the mixing rate in kHz (0 = 18, at most 23), its byte 2 the **tempo** (signed), and the rest is often text. *Amberstar*'s replayer always mixes at 18 kHz, whatever the song says.

A tick is one mix buffer of 10 × kHz × (256 + tempo) / 256 words, rounded down, played at period 3579545 / (1000 × kHz), rounded to the nearest. So a tempo of 0 is 50 Hz, and −48 is 61 Hz. The number of samples mixed is 20 × kHz × (256 + tempo) / 256, rounded down: when it's odd, the last one isn't played. The direct voices' register writes for a tick are made when its buffer starts to play.

### 8.3 The mix

For each of the four voices and each output sample:
- The position steps by 2048 × mix period / period (rounded down) in 1/2048ths of a sample, with no interpolation. A note starts at position 2047/2048.
- The sample is scaled by volume / 63, truncated toward zero, with volumes above 63 counting as 63.
- When the end of the sample is reached, the loop follows at once, keeping the fraction. A loop of 1 word is silence.

The four are added and limited to −128…127. Channel 4 plays the result at volume 64.

### 8.4 Random volume (`EA n`)

The volume of the tick it's read on becomes volume × (*L* − *r*) / 256, where *L* is 256 at full level or 246 × (16 − *n*ₗ) / 16 at level *n*ₗ, and *r* is *n* × (a random number from 0 to 166) / 64, the range as observed. Only *Amberstar*'s title uses it, on its drums.

---

## 9. The tick, in order

1. Under the older rules, loops left over from the last tick's sample starts are written.
2. If the song has ended (§5.1), every voice gets volume 0 and DMA off, and that's all.
3. The row counter steps. When it runs out it's reloaded with the speed, and a row is read:
   - if voice 1 has a note 1 there, play moves to the next track row's row 0;
   - then, for each voice in turn: the transpose, the sound transpose or its command, and the note.
4. For each voice that has played a note:
   - the frequency sequence;
   - the scan, unless a sample started on this tick;
   - the volume sequence;
   - the period: table, vibrato, slide.

---

## 10. Replayers

| Rules | Differences |
|---|---|
| Older | `E7` goes to a sequence; no `E5`, `E6`, `E9` or info bit $40; the first row comes *speed* − 1 ticks in; loops a tick late |
| Newer | `E7` starts samples; `E9`; info bit $40 |
| 2708 | As newer, plus scans, and its own vibrato |
| COSO | As newer, plus scans; vibrato and slide in proportion to the period; kept sounds; levels and speed changes in the track rows |

**`.hip` files** carry their replayer, and the length of the code in front tells which it is: 2708 is the 2708 build (22 files), and 2262, 2278, 2320, 2366, 2402, 2446, 2470, 2636 and 2640 are older ones. 2604 (20 files) and 2818 are newer.

**COSO files** don't say which rules they were written for, and there are at least three sets: *Astaroth*'s plays by the older rules, *Wings of Death*'s by the 2708 ones, and the later games' (*Enchanted Land*, *A Prehistoric Tale*, *Lethal Xcess*, *Ghost Battle*, *Return of Medusa*) by the COSO rules. The song's own features tell them apart on every game checked:
- **COSO:** vibrato depths around 20 (the others' are 1–6), kept sounds, sound transposes above $80, or `E5`, `E6` or `E9` in a frequency sequence;
- **older:** `E3` or `E8` commands, or a second spare sample header;
- **2708:** everything else.

**`.hip7` files and 7-voice COSO:** the COSO rules.

**Music voices.** Some in-game builds play only the first two or three voices, leaving the rest to sound effects. Some games also scale the music's volume. These are the games' choices: a player should play every voice, at the volumes in the song.

**Restarting Paula.** The replayers turn a channel's DMA off and on again within a tick to start a sample. Paula notices that DMA is off only when it finishes the word it's playing, and after a very low note that can take longer than the replayer waits. The sample then doesn't start, and only its loop is heard. *Wings of Death*'s title has one such note, the speech 2:16 into the song. A player can simply play the sample.

---

## 11. Subsongs, ends and loops

A subsong plays its track rows from first to last and goes back to the first. It plays once only if it reaches a sound transpose of $80 (§5.1). The end test is that the next track row equals last + 1, so entries whose last row is before their first never loop and run on through whatever follows: they aren't real subsongs. Speed 0 stops the song where it starts, so those entries are silence.

Some entries only play part of another subsong, such as *Wings of Death*'s title song's last track row. HippelReplay tells them by the rows they play and each row's speed, which the track rows' speed commands can change on the way.

Because rows run in order, a song's loop point is simply the first row that's read a second time.

---

## 12. How this was measured

**Method.** The original replayers were run in a 68000 emulator: those carried by the `.hip` and `.hip7` files, loaded into an empty machine, and the games' own, from WinUAE savestates. Every Paula write was logged, tick by tick, along with the 7-voice mix buffers. The replayers were run, not disassembled, and no other player's source was read. Test songs that use one feature at a time established each behaviour, and whole songs were then compared with HippelReplay (`tests/compare.py`). The savestates and songs belong to the games and aren't distributed, so the comparisons can't be rerun without them.

A match there means that HippelReplay sends Paula what the original does. For the sound itself, **WinUAE recordings** of the games (no filter, no interpolation) were compared with renders, tick by tick, by level and spectrum in third-octave bands from 25 Hz to 5 kHz. Note starts differ by up to a few milliseconds, depending on how busy the game is, so a plain subtraction isn't meaningful.

**Results.**

| Reference | Result |
|---|---|
| The `.hip` files' own replayers | All 139 subsongs exact over 1,500 ticks (30 seconds): period, volume, sample starts, loops and DMA. 134 are exact over 6,000 ticks. Of the others, three read past the period table and two are broken subsong entries |
| Game savestates, 4 voices | 13 savestates exact over 3,000 ticks, in periods and volumes: *A Prehistoric Tale* (4), *Ghost Battle* (3), *Astaroth* (2), *Return of Medusa* (2), *Enchanted Land*, *Lethal Xcess* |
| 7 voices, direct | The three direct voices exact: *Amberstar*'s and *Lethal Xcess*'s intros from their games over 8,000 ticks, and two other rips over 10,000 or more |
| 7 voices, mix | Sample for sample apart from single samples now and then (§13), and from notes already playing when one of the savestates was taken |
| Recordings | 13 songs from 7 games, 4 and 7 voices, each set of rules. Level within 0.02 dB, median band difference 0.05–0.8 dB |

**Listening.** Blind tests were made from five songs of *A Prehistoric Tale*, *Amberstar* and *Lethal Xcess*: one file each, made of random 3–9 second stretches taken in turn from the recording and from HippelReplay, joined with 20 ms crossfades. No joins could be heard.

---

## 13. Open questions

- **c5** of the header, and the third word of a 7-voice subsong entry.
- **The mix position above the mixing rate:** it gains 1/2048 of a sample now and then, which changes single samples of the mix.
- **`EA`'s random numbers:** their range is measured, not how they're made.
- **`E5` and `E7`'s memory in 4-voice COSO songs** (§6.2): measured with 7 voices only.
- **COSO vibrato speeds with bit 7 set,** and 4-voice sound transposes $81–$E0.
- **COSO songs of games that haven't been checked:** their rules are chosen from the song's features (§10).
- ***Australian Pioneers*** (a game that was never released) and *Ghost Battle*'s 7-voice "gameover test" have no replayer to compare with. The game itself plays a different, 4-voice song there.
- **`astaroth.hip`** holds its replayer in front of a COSO song in a layout of its own. Its music is also in modland as `astaroth123.hipc` and `astaroth4.hipc`.

---

This documentation was generated using AI development tools (Claude Code by Anthropic).

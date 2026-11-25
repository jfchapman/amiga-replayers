/*
 * amigareplay: one API for all the replayers here (TFMXreplay, SoundMonReplay and HippelReplay). It finds out
 * what kind of song it's given and plays it with the right one.
 *
 * Copyright (c) 2026 James Chapman. MIT licence: see LICENSE.
 *
 * A plain C API. Typical use:
 *
 *     amigareplay_settings settings;
 *     amigareplay_default_settings(&settings, sizeof settings);
 *     settings.sample_rate = 48000;
 *
 *     amigareplay_error error;
 *     amigareplay_module *module = amigareplay_open_memory(data, size, &settings, &error);
 *     if (!module) { ... amigareplay_error_string(error) ... }
 *
 *     float buffer[2 * 1024];
 *     size_t frames;
 *     while ((frames = amigareplay_render_float(module, buffer, 1024)) > 0) {
 *         ... write 'frames' stereo frames ...
 *     }
 *     amigareplay_close(module);
 *
 * Songs of more than one file (TFMX's mdat and smpl) are opened with amigareplay_open.
 *
 * Strings are UTF-8. Those returned belong to the library: they stay valid until the module is closed, or for ever
 * if they don't come from one. They're never NULL, but may be empty.
 *
 * Thread safety: a module may be used from one thread at a time. Different modules are independent.
 */
#ifndef AMIGAREPLAY_H
#define AMIGAREPLAY_H

#include <stddef.h>
#include <stdint.h>

#include "amiga_replayers_version.h"

#if defined(_WIN32) && defined(AMIGAREPLAY_SHARED)
#  if defined(AMIGAREPLAY_BUILDING)
#    define AMIGAREPLAY_API __declspec(dllexport)
#  else
#    define AMIGAREPLAY_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__) && defined(AMIGAREPLAY_SHARED)
#  define AMIGAREPLAY_API __attribute__((visibility("default")))
#else
#  define AMIGAREPLAY_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Returns the library version as "major.minor.patch". */
AMIGAREPLAY_API const char *amigareplay_version(void);

typedef struct amigareplay_module amigareplay_module;

typedef enum amigareplay_error {
	AMIGAREPLAY_OK = 0,
	AMIGAREPLAY_ERROR_ARGUMENT,    /* a null pointer or an invalid setting */
	AMIGAREPLAY_ERROR_OPTION,      /* an unknown option, or a value it doesn't have */
	AMIGAREPLAY_ERROR_FORMAT,      /* not a song any of the replayers knows, or damaged */
	AMIGAREPLAY_ERROR_FILES,       /* the song has other files, which weren't given */
	AMIGAREPLAY_ERROR_UNSUPPORTED, /* a song of a known kind that can't be played */
	AMIGAREPLAY_ERROR_MEMORY       /* out of memory */
} amigareplay_error;

AMIGAREPLAY_API const char *amigareplay_error_string(amigareplay_error error);


/* --- settings: what every format has --- */

typedef enum amigareplay_interpolation {
	AMIGAREPLAY_INTERPOLATION_NONE = 0, /* nearest sample, like the Amiga (bright, with aliasing): default */
	AMIGAREPLAY_INTERPOLATION_LINEAR,
	AMIGAREPLAY_INTERPOLATION_CUBIC
} amigareplay_interpolation;

/*
 * The fixed low-pass filter on the Amiga's audio output, always in the path whatever the power LED shows. The
 * separate LED filter is amigareplay_settings.led_filter.
 */
typedef enum amigareplay_amiga_model {
	AMIGAREPLAY_AMIGA_A500 = 0, /* 6 dB/octave at about 4.9 kHz: default */
	AMIGAREPLAY_AMIGA_A1200,    /* 6 dB/octave at about 29 kHz: close to flat */
	AMIGAREPLAY_AMIGA_NONE      /* no fixed filter */
} amigareplay_amiga_model;

/* The "LED" filter (12 dB/octave at about 3.3 kHz). */
typedef enum amigareplay_led_filter {
	AMIGAREPLAY_LED_SONG = 0, /* as the song switches it: default. Only SoundMon songs do; it's off in the rest */
	AMIGAREPLAY_LED_OFF,
	AMIGAREPLAY_LED_ON
} amigareplay_led_filter;

typedef struct amigareplay_settings {
	size_t size;                             /* sizeof(amigareplay_settings), so that later versions can add to it.
	                                            amigareplay_default_settings sets it */
	int sample_rate;                         /* output rate in Hz, 8000-192000 (default 44100) */
	int stereo_separation;                   /* 0 = mono, 100 = hard Amiga panning (default 100) */
	amigareplay_interpolation interpolation; /* default AMIGAREPLAY_INTERPOLATION_NONE */
	int volume_ramping;                      /* anti-click ramp in microseconds, 0 = off (default 2000) */
	int loop_count;                          /* extra times to play a looped song: 0 = play once (default), -1 = forever */
	float fade_seconds;                      /* after its loops, a looped song plays on, fading out over this many
	                                            seconds. 0 = stop at the loop point (default) */
	float gain;                              /* output gain, 1.0 = default */
	amigareplay_amiga_model amiga_model;     /* the fixed output filter, default AMIGAREPLAY_AMIGA_A500 */
	amigareplay_led_filter led_filter;       /* default AMIGAREPLAY_LED_SONG */
} amigareplay_settings;

/* Fills settings with the defaults. size: sizeof(amigareplay_settings). */
AMIGAREPLAY_API void amigareplay_default_settings(amigareplay_settings *settings, size_t size);


/*
 * --- options: what only some formats have ---
 *
 * Options are names and values, both text, in the way of libopenmpt's ctls: new ones can be added without changing
 * the API. Each belongs to a format, and songs of other formats ignore it, so the same options can be given for any
 * song:
 *
 *   tfmx.variant     auto (default), pro, 1.5, masterblazer, xout, rocknroll, gemx, turrican2, t3small, hexuma,
 *                    logical, dangerfreak: the replayer's build
 *   tfmx.mix_setting auto (default), 8 to 22: the quality of 7-voice songs' software mix (about 16 kHz x setting / 18)
 *   hippel.replayer  auto (default), older, newer, 2708, coso: the replayer's rules. 7-voice songs always use
 *                    coso
 */

/* An option and its value, for amigareplay_open. */
typedef struct amigareplay_option {
	const char *name;
	const char *value;
} amigareplay_option;

/* The options there are: their number, and one's name, its values separated by spaces (a range as "8-22"), and its
   default. NULL for an index out of range. */
AMIGAREPLAY_API int amigareplay_option_count(void);
AMIGAREPLAY_API const char *amigareplay_option_name(int index);
AMIGAREPLAY_API const char *amigareplay_option_values(int index);
AMIGAREPLAY_API const char *amigareplay_option_default(int index);

/* The value an open song uses: for "auto", the one chosen if that's known. "" for another format's option or an
   unknown name. */
AMIGAREPLAY_API const char *amigareplay_get_option(const amigareplay_module *module, const char *name);


/* --- opening --- */

/* A file of a song. Most songs are one file, but some keep their samples in another, and a few in several. */
typedef struct amigareplay_file {
	const char *name; /* the file's name, which may have folders ("/") before it. Needed for a song of several files;
	                     otherwise it may be NULL */
	const void *data;
	size_t size;
} amigareplay_file;

typedef enum amigareplay_probe_result {
	AMIGAREPLAY_PROBE_NO = 0,
	AMIGAREPLAY_PROBE_YES,
	AMIGAREPLAY_PROBE_YES_FILES /* a song with other files: see amigareplay_companion_file */
} amigareplay_probe_result;

/* How much of a file's start amigareplay_probe looks at. */
#define AMIGAREPLAY_PROBE_SIZE 0x10040

/* Returns whether a file, of which at least the start is given, looks like a song that can be played. */
AMIGAREPLAY_API amigareplay_probe_result amigareplay_probe(const amigareplay_file *file);

/*
 * The other files of a song: writes the name of number 'index' (from 0) into 'buffer', as it would be relative to the
 * song's own file, in the same folder or one below it. The letter case of the actual file may differ. 'file' is the
 * song's main file, with its name.
 *
 * Returns 1 for a file that's needed, 2 for one that isn't but improves the song, and 0 when there are no more (or the
 * buffer is too small).
 *
 * For example, for "mdat.turrican" there's one, "smpl.turrican".
 */
AMIGAREPLAY_API int amigareplay_companion_file(const amigareplay_file *file, int index, char *buffer, size_t buffer_size);

/*
 * The reverse, for leaving out of a list of songs the files that only belong to one: writes the name of number 'index'
 * (from 0) of the main files that the file named 'name' might belong to. If there's a file of that name in the same
 * folder, it does. It goes by the name alone, so it only finds companion files that are named after their song.
 *
 * Returns 1, or 0 when there are no more (or the buffer is too small).
 *
 * For example, for "smpl.turrican" there's one, "mdat.turrican".
 */
AMIGAREPLAY_API int amigareplay_main_file(const char *name, int index, char *buffer, size_t buffer_size);

/*
 * Opens a song. The data is copied, so the caller may free its buffers afterwards.
 *
 * files, count: the song's main file, then its other files, if it has any, in any order.
 * settings:     NULL for the defaults.
 * options:      an array ending with a NULL name, or NULL for none. An unknown option, or a value it doesn't have, is
 *               an error.
 * error:        may be NULL.
 *
 * Subsong 0 is selected.
 *
 * Returns NULL if it fails.
 */
AMIGAREPLAY_API amigareplay_module *amigareplay_open(const amigareplay_file *files, int count,
                                                     const amigareplay_settings *settings,
                                                     const amigareplay_option *options, amigareplay_error *error);

/* The same for a song of a single file, with no options. */
AMIGAREPLAY_API amigareplay_module *amigareplay_open_memory(const void *data, size_t size,
                                                            const amigareplay_settings *settings,
                                                            amigareplay_error *error);

AMIGAREPLAY_API void amigareplay_close(amigareplay_module *module);


/* --- song information --- */

/*
 * Information by name:
 *
 *   format   the kind of song: "tfmx", "soundmon" or "hippel"
 *   type     the same for display, with its version: "TFMX Pro", "BP SoundMon 2 (V.2)", "Hippel 7V COSO"
 *   title    the song's title, from inside the file
 *   message  text in the file: TFMX songs' text block, lines separated by "\n"
 *
 * "" if the song doesn't have it, or for an unknown name.
 */
AMIGAREPLAY_API const char *amigareplay_get_metadata(const amigareplay_module *module, const char *name);

/* Number of voices in the song: 4, or 7 or 8 when some are mixed in software. */
AMIGAREPLAY_API int amigareplay_voice_count(const amigareplay_module *module);

/* Number of samples and instruments, and the names of number 0 and up. Formats have one or the other, or neither (0);
   names can be empty. */
AMIGAREPLAY_API int amigareplay_sample_count(const amigareplay_module *module);
AMIGAREPLAY_API const char *amigareplay_sample_name(const amigareplay_module *module, int index);
AMIGAREPLAY_API int amigareplay_instrument_count(const amigareplay_module *module);
AMIGAREPLAY_API const char *amigareplay_instrument_name(const amigareplay_module *module, int index);


/* --- subsongs --- */

/* Subsongs are numbered from 0 to amigareplay_subsong_count - 1 (their index). */
AMIGAREPLAY_API int amigareplay_subsong_count(const amigareplay_module *module);

/* The subsong to play when only one will do: the main song. For TFMX songs the first call takes a little while (it
   plays through every subsong without sound), as does amigareplay_get_subsong_info's. */
AMIGAREPLAY_API int amigareplay_default_subsong(amigareplay_module *module);

typedef struct amigareplay_subsong_info {
	size_t size;        /* sizeof(amigareplay_subsong_info), so that later versions can add to it: set it first */
	int id;             /* a number to store, to find the subsong again (amigareplay_subsong_from_id) in later versions
	                       of the library too. TFMX songs' own subsong numbers; otherwise the index */
	int number;         /* for display: the subsong's number in the song itself, as the format counts them */
	double duration;    /* one play-through in seconds, up to the song's end or the point where it loops; -1 if no end
	                       was found */
	double loop_start;  /* where the song continues from at the end of a play-through, in seconds; -1 if it doesn't */
	double loop_length; /* seconds from loop_start to the end of the play-through, 0 if it doesn't loop */
	int looped;         /* 1 if the song is heard to loop. Other songs stop at the end of a play-through, whatever
	                       loop_count and fade_seconds say */
	int audible;        /* 0 if it plays nothing but silence */
	int repeat_of;      /* the index of a subsong that plays this one's music too, or -1 */
} amigareplay_subsong_info;

/* Fills info for a subsong. Returns 0 on success. */
AMIGAREPLAY_API int amigareplay_get_subsong_info(amigareplay_module *module, int index, amigareplay_subsong_info *info);

/* The index of the subsong with this id, or -1. */
AMIGAREPLAY_API int amigareplay_subsong_from_id(const amigareplay_module *module, int id);

/*
 * Playing length of a subsong in seconds with the current settings: one play-through, plus loop_count extra loops and
 * fade_seconds if it's a looped song. -1 if no end was found, or it plays for ever.
 */
AMIGAREPLAY_API double amigareplay_subsong_duration(amigareplay_module *module, int index);

/* Selects and restarts a subsong. Returns 0 on success. */
AMIGAREPLAY_API int amigareplay_select_subsong(amigareplay_module *module, int index);
AMIGAREPLAY_API int amigareplay_selected_subsong(const amigareplay_module *module);


/* --- playback --- */

/*
 * Renders up to 'frames' stereo frames (interleaved left, right). Returns the number rendered, which is less than
 * 'frames' only at the end of the song (0 once it has ended).
 */
AMIGAREPLAY_API size_t amigareplay_render_float(amigareplay_module *module, float *buffer, size_t frames);
AMIGAREPLAY_API size_t amigareplay_render_s16(amigareplay_module *module, int16_t *buffer, size_t frames);

/*
 * Seeks by time played since the start of the subsong, in seconds. Returns the new position. It plays from the start
 * without rendering audio, so the sound is exact.
 */
AMIGAREPLAY_API double amigareplay_seek(amigareplay_module *module, double seconds);

/* Time played since the start of the subsong, in seconds: it keeps rising through loops. */
AMIGAREPLAY_API double amigareplay_position(const amigareplay_module *module);

/*
 * Changes the settings of an open song, at once. sample_rate must be the one it was opened with, and loop_count and
 * fade_seconds may wait for the next amigareplay_select_subsong or amigareplay_seek. Returns 0 on success.
 */
AMIGAREPLAY_API int amigareplay_set_settings(amigareplay_module *module, const amigareplay_settings *settings);

/*
 * Chooses which voices are heard, bit n for voice n (all by default). Bits 0-3 are Paula's channels. When some voices
 * are mixed in software onto channel 3, TFMX songs can mute each of those (bits 4-7), and Hippel songs all of them
 * together (bit 3). Muted voices still play.
 */
AMIGAREPLAY_API int amigareplay_set_voice_mask(amigareplay_module *module, unsigned mask);

#ifdef __cplusplus
}
#endif

#endif

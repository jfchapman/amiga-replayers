/*
 * HippelReplay: a renderer for Jochen Hippel's Amiga music: 4- and 7-voice songs in the .hip (with or
 * without their replayer in front) and COSO (.hipc) forms.
 *
 * Copyright (c) 2026 James Chapman. MIT licence: see LICENSE.
 *
 * A plain C API. Typical use:
 *
 *     hippel_settings settings;
 *     hippel_default_settings(&settings);
 *     settings.sample_rate = 48000;
 *
 *     hippel_error error;
 *     hippel_module *module = hippel_open(data, size, &settings, &error);
 *     if (!module) { ... hippel_error_string(error) ... }
 *
 *     int16_t buffer[2 * 1024];
 *     size_t frames;
 *     while ((frames = hippel_render_s16(module, buffer, 1024)) > 0) {
 *         ... write 'frames' stereo frames ...
 *     }
 *     hippel_close(module);
 *
 * Thread safety: a module may be used from one thread at a time. Different modules are independent.
 */
#ifndef HIPPELREPLAY_H
#define HIPPELREPLAY_H

#include <stddef.h>
#include <stdint.h>

#include "amiga_replayers_version.h"

#if defined(_WIN32) && defined(HIPPEL_SHARED)
#  if defined(HIPPEL_BUILDING)
#    define HIPPEL_API __declspec(dllexport)
#  else
#    define HIPPEL_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__) && defined(HIPPEL_SHARED)
#  define HIPPEL_API __attribute__((visibility("default")))
#else
#  define HIPPEL_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define HIPPEL_VERSION_MAJOR AMIGA_REPLAYERS_VERSION_MAJOR
#define HIPPEL_VERSION_MINOR AMIGA_REPLAYERS_VERSION_MINOR
#define HIPPEL_VERSION_PATCH AMIGA_REPLAYERS_VERSION_PATCH

/* Returns the library version as "major.minor.patch". */
HIPPEL_API const char *hippel_version(void);

typedef struct hippel_module hippel_module;

typedef enum hippel_error {
	HIPPEL_OK = 0,
	HIPPEL_ERROR_ARGUMENT, /* a null pointer or an invalid setting */
	HIPPEL_ERROR_FORMAT,   /* not a Hippel song, or damaged */
	HIPPEL_ERROR_UNSUPPORTED, /* a Hippel song this library doesn't play */
	HIPPEL_ERROR_MEMORY    /* out of memory */
} hippel_error;

HIPPEL_API const char *hippel_error_string(hippel_error error);

typedef enum hippel_interpolation {
	HIPPEL_INTERPOLATION_NONE = 0, /* nearest sample, like the Amiga (bright, with aliasing): default */
	HIPPEL_INTERPOLATION_LINEAR,
	HIPPEL_INTERPOLATION_CUBIC
} hippel_interpolation;

/*
 * The fixed low-pass filter on the Amiga's audio output, always in the path whatever the power
 * LED shows. The separate LED filter is hippel_settings.led_filter.
 */
typedef enum hippel_amiga_model {
	HIPPEL_AMIGA_A500 = 0, /* 6 dB/octave at about 4.9 kHz: default */
	HIPPEL_AMIGA_A1200,    /* 6 dB/octave at about 29 kHz: close to flat */
	HIPPEL_AMIGA_NONE      /* no fixed filter */
} hippel_amiga_model;

/* The "LED" filter (12 dB/octave at about 3.3 kHz). Hippel songs don't switch it. */
typedef enum hippel_led_filter {
	HIPPEL_LED_OFF = 0, /* default */
	HIPPEL_LED_ON
} hippel_led_filter;

/*
 * The replayer whose rules a song is played by: Hippel's replayers changed over the years, mostly
 * in their vibrato and slides. A .hip file has its own replayer in front, which tells. A COSO file
 * doesn't: AUTO goes by the features the song uses.
 */
typedef enum hippel_replayer {
	HIPPEL_REPLAYER_AUTO = 0, /* default */
	HIPPEL_REPLAYER_OLDER,    /* the early .hip builds, and Astaroth's COSO replayer */
	HIPPEL_REPLAYER_NEWER,    /* the later .hip builds (Leavin' Teramis) */
	HIPPEL_REPLAYER_2708,     /* the Wings of Death build */
	HIPPEL_REPLAYER_COSO      /* the later COSO replayers: Enchanted Land, A Prehistoric Tale, Lethal Xcess.
                                 7-voice songs always use these */
} hippel_replayer;

typedef struct hippel_settings {
	int sample_rate;                  /* output rate in Hz, 8000-192000 (default 44100) */
	int stereo_separation;            /* 0 = mono, 100 = hard Amiga panning (default 100) */
	hippel_interpolation interpolation; /* default HIPPEL_INTERPOLATION_NONE */
	int volume_ramping;               /* anti-click ramp in microseconds, 0 = off (default 2000) */
	int loop_count;                   /* extra times to play a looped song: 0 = play once (default), -1 = forever */
	float gain;                       /* output gain, 1.0 = default */
	hippel_amiga_model amiga_model;     /* the fixed output filter, default HIPPEL_AMIGA_A500 */
	hippel_led_filter led_filter;       /* default HIPPEL_LED_OFF */
	hippel_replayer replayer;         /* default HIPPEL_REPLAYER_AUTO; only hippel_open reads it */
	float fade_seconds;               /* after its loops, a looped song plays on, fading out over this
	                                     many seconds. 0 = stop at the loop point (default) */
} hippel_settings;

/* Fills settings with the defaults. */
HIPPEL_API void hippel_default_settings(hippel_settings *settings);

/*
 * Opens a song. The data is copied, so the caller may free its buffer afterwards. settings may
 * be NULL for the defaults, and error may be NULL. Subsong 0 is selected.
 */
HIPPEL_API hippel_module *hippel_open(const void *data, size_t size, const hippel_settings *settings, hippel_error *error);

/* Returns 1 if the data looks like a Hippel song. Only the first 64 KB are needed. */
HIPPEL_API int hippel_probe(const void *data, size_t size);

HIPPEL_API void hippel_close(hippel_module *module);

/* --- song information --- */

/*
 * The form of the file, "Hippel", "Hippel COSO", "Hippel 7V" or "Hippel 7V COSO", and the replayer rules in
 * use: "older", "newer", "2708", "COSO" or "7V".
 */
HIPPEL_API const char *hippel_format(const hippel_module *module);
HIPPEL_API const char *hippel_replayer_name(const hippel_module *module);

/* Number of voices in the song: 4, or 7 (four of them mixed in software). */
HIPPEL_API int hippel_voice_count(const hippel_module *module);

/* Number of samples, and the name of sample 0 and up: "" for COSO songs (which have no names) or a number out of range. */
HIPPEL_API int hippel_sample_count(const hippel_module *module);
HIPPEL_API const char *hippel_sample_name(const hippel_module *module, int sample);

/* Number of subsongs. Entries of the song's table that play nothing are left out. */
HIPPEL_API int hippel_subsong_count(const hippel_module *module);

typedef struct hippel_subsong_info {
	int number;         /* the subsong's number in the song's own table, from 1 */
	double duration;    /* one play-through in seconds, -1 if no end was found */
	double loop_start;  /* where the song continues from at the end of a play-through, in seconds; -1 if it doesn't */
	double loop_length; /* seconds from loop_start to the end of the play-through, 0 if it doesn't loop */
	int looped;         /* 1 if the song is heard to loop. Other songs stop at the end of a play-through,
	                       whatever loop_count and fade_seconds say */
	int repeat_of;      /* the index of a subsong that plays all this one's music too, or -1 */
} hippel_subsong_info;

/* Fills info for a subsong. Returns 0 on success. */
HIPPEL_API int hippel_get_subsong_info(const hippel_module *module, int index, hippel_subsong_info *info);

/* The subsong to play when only one will do: the longest. */
HIPPEL_API int hippel_default_subsong(const hippel_module *module);

/*
 * Playing length of a subsong in seconds with the current settings: one play-through, plus
 * loop_count extra loops and fade_seconds if it's a looped song. -1 if no end was found.
 */
HIPPEL_API double hippel_subsong_duration(const hippel_module *module, int index);

/* --- playback --- */

/* Selects and restarts a subsong. Returns 0 on success. */
HIPPEL_API int hippel_select_subsong(hippel_module *module, int index);

/*
 * Renders up to 'frames' stereo frames (interleaved left, right). Returns the number rendered,
 * which is less than 'frames' only at the end of the song (0 once it has ended).
 */
HIPPEL_API size_t hippel_render_s16(hippel_module *module, int16_t *buffer, size_t frames);
HIPPEL_API size_t hippel_render_float(hippel_module *module, float *buffer, size_t frames);

/*
 * Seeks by time played since the start of the subsong, in seconds. Returns the new position. It
 * plays from the start without rendering audio, so the sound is exact.
 */
HIPPEL_API double hippel_seek(hippel_module *module, double seconds);

/* Time played since the start of the subsong, in seconds: it keeps rising through loops. */
HIPPEL_API double hippel_position(const hippel_module *module);

/*
 * Changes settings on an open module. sample_rate takes effect from the next
 * hippel_select_subsong; the others take effect at once.
 */
HIPPEL_API int hippel_set_settings(hippel_module *module, const hippel_settings *settings);

/*
 * Chooses which Paula channels are heard, bit n for channel n (0xF = all, the default). In a 7-voice song
 * channel 3 plays the mix of voices 4-7.
 */
HIPPEL_API int hippel_set_voice_mask(hippel_module *module, unsigned mask);

#ifdef __cplusplus
}
#endif

#endif

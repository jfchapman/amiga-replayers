/*
 * amigarender: renders a song to a WAV file, using only the public amigareplay API. It knows nothing of the formats
 * themselves.
 *
 * Copyright (c) 2026 James Chapman. MIT licence: see LICENSE.
 *
 * Usage: amigarender [options] <song> [the song's other files]
 * Run without arguments for the options.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "amigareplay.h"

enum { MAX_FILES = 64, MAX_OPTIONS = 16 };

static void usage(void) {
	int i;
	fprintf(stderr,
	        "amigarender %s: render a song to WAV\n"
	        "\n"
	        "usage: amigarender [options] <song> [the song's other files]\n"
	        "\n"
	        "  -n            print the song's information only\n"
	        "  -o <file>     output file (default: <song>.wav, or <song>-<n>.wav for a song with subsongs)\n"
	        "  -s <n>        subsong index (default 0)\n"
	        "  -d            render the default subsong (the main song)\n"
	        "  -a            render every subsong, to <output file>-<n>.wav\n"
	        "  -t <seconds>  stop after this long\n"
	        "  -l <n>        extra loops of a looping song (default 0; -1 = for ever, which needs -t)\n"
	        "  -F <seconds>  fade out a looping song over this long (default 0: stop at the loop point)\n"
	        "  -r <rate>     sample rate (default 44100)\n"
	        "  -i <mode>     interpolation: none (default), linear, cubic\n"
	        "  -f <model>    fixed output filter: a500 (default), a1200, none\n"
	        "  -L <mode>     LED filter: song (default), off, on\n"
	        "  -p <percent>  stereo separation, 0-100 (default 100)\n"
	        "  -m <us>       volume ramping in microseconds, 0 = off (default 2000)\n"
	        "  -V <voices>   hear only these voices, e.g. 03 (default: all)\n"
	        "  -O <option>   an option that only some formats have, as name=value:\n",
	        amigareplay_version());
	for (i = 0; i < amigareplay_option_count(); i++)
		fprintf(stderr, "                  %s: %s (default %s)\n", amigareplay_option_name(i), amigareplay_option_values(i),
		        amigareplay_option_default(i));
	fprintf(stderr,
	        "\n"
	        "The song's other files, if it has any and they aren't given, are found from its name.\n");
}

static void *read_file(const char *path, size_t *size) {
	FILE *f = fopen(path, "rb");
	void *data = NULL;
	long length;
	if (!f)
		return NULL;
	if (fseek(f, 0, SEEK_END) == 0 && (length = ftell(f)) >= 0 && fseek(f, 0, SEEK_SET) == 0) {
		data = malloc(length ? (size_t)length : 1);
		if (data && fread(data, 1, (size_t)length, f) != (size_t)length) {
			free(data);
			data = NULL;
		}
		*size = (size_t)length;
	}
	fclose(f);
	return data;
}

/* Where the file's name starts, after any folders. */
static const char *base_name(const char *path) {
	const char *base = path + strlen(path);
	while (base > path && base[-1] != '/' && base[-1] != '\\')
		base--;
	return base;
}

/* The index of a word in a list that ends with NULL, or -1. */
static int choice(const char *value, const char *const *words) {
	int i;
	for (i = 0; words[i]; i++)
		if (!strcmp(value, words[i]))
			return i;
	return -1;
}

static void put16(unsigned char *p, unsigned v) {
	p[0] = (unsigned char)v;
	p[1] = (unsigned char)(v >> 8);
}

static void put32(unsigned char *p, unsigned long v) {
	put16(p, (unsigned)(v & 0xFFFF));
	put16(p + 2, (unsigned)(v >> 16));
}

/* Writes a 16-bit stereo WAV file, of up to 'limit' seconds (no limit if it's negative). Returns 0 on success. */
static int render_wav(amigareplay_module *module, const char *path, int rate, double limit) {
	unsigned char header[44] = {0};
	int16_t buffer[2 * 4096];
	unsigned char bytes[4 * 4096];
	unsigned long frames = 0;
	size_t n, i, want = 4096;
	FILE *f = fopen(path, "wb");
	if (!f) {
		fprintf(stderr, "can't write %s\n", path);
		return 1;
	}
	fwrite(header, 1, sizeof header, f); /* filled in at the end */
	for (;;) {
		if (limit >= 0 && frames + want > limit * rate)
			want = (size_t)(limit * rate) - frames;
		n = amigareplay_render_s16(module, buffer, want);
		if (!n)
			break;
		for (i = 0; i < 2 * n; i++)
			put16(bytes + 2 * i, (unsigned)(uint16_t)buffer[i]);
		fwrite(bytes, 4, n, f);
		frames += (unsigned long)n;
	}
	memcpy(header, "RIFF", 4);
	put32(header + 4, 36 + frames * 4);
	memcpy(header + 8, "WAVEfmt ", 8);
	put32(header + 16, 16);
	put16(header + 20, 1); /* PCM */
	put16(header + 22, 2);
	put32(header + 24, (unsigned long)rate);
	put32(header + 28, (unsigned long)rate * 4);
	put16(header + 32, 4);
	put16(header + 34, 16);
	memcpy(header + 36, "data", 4);
	put32(header + 40, frames * 4);
	fseek(f, 0, SEEK_SET);
	fwrite(header, 1, sizeof header, f);
	if (fclose(f)) {
		fprintf(stderr, "can't write %s\n", path);
		return 1;
	}
	printf("%s: %.1f seconds\n", path, (double)frames / rate);
	return 0;
}

static void print_info(amigareplay_module *module) {
	static const char *const names[] = {"format", "type", "title"};
	const char *text;
	int i;
	for (i = 0; i < 3; i++) {
		text = amigareplay_get_metadata(module, names[i]);
		if (*text)
			printf("%s:%*s%s\n", names[i], 9 - (int)strlen(names[i]), "", text);
	}
	for (i = 0; i < amigareplay_option_count(); i++) {
		text = amigareplay_get_option(module, amigareplay_option_name(i));
		if (*text)
			printf("%s: %s\n", amigareplay_option_name(i), text);
	}
	printf("voices:   %d\n", amigareplay_voice_count(module));
	printf("subsongs: %d (default %d)\n", amigareplay_subsong_count(module), amigareplay_default_subsong(module));
	for (i = 0; i < amigareplay_subsong_count(module); i++) {
		amigareplay_subsong_info s;
		double d;
		s.size = sizeof s;
		amigareplay_get_subsong_info(module, i, &s);
		d = s.duration;
		printf("  %2d: song %2d, ", i, s.number);
		if (d < 0) {
			printf("no end found\n");
			continue;
		}
		printf("%d:%04.1f (%.3f s)", (int)(d / 60), d - 60 * (int)(d / 60), d);
		if (s.loop_start >= 0)
			printf(", loop from %.3f s%s", s.loop_start, s.looped ? "" : " (ends silent)");
		if (!s.audible)
			printf(", silent");
		if (s.repeat_of >= 0)
			printf(", repeat of %d", s.repeat_of);
		printf("\n");
	}
	for (i = 0; i < amigareplay_sample_count(module); i++)
		printf("sample %d: %s\n", i + 1, amigareplay_sample_name(module, i));
	for (i = 0; i < amigareplay_instrument_count(module); i++)
		printf("instrument %d: %s\n", i + 1, amigareplay_instrument_name(module, i));
	text = amigareplay_get_metadata(module, "message");
	if (*text)
		printf("text:\n%s\n", text);
}

/*
 * Adds the song's other files to files[0], which is the song: they're in its folder, with names the library gives.
 * Returns the number of files.
 */
static int find_files(const char *song, amigareplay_file *files, char (*paths)[1024]) {
	char name[256];
	size_t folder = (size_t)(base_name(song) - song);
	int i, needed, count = 1;
	for (i = 0; count < MAX_FILES && (needed = amigareplay_companion_file(&files[0], i, name, sizeof name)) != 0; i++) {
		char *path = paths[count];
		if (folder + strlen(name) >= sizeof paths[0])
			break;
		memcpy(path, song, folder);
		strcpy(path + folder, name);
		files[count].name = path;
		files[count].data = read_file(path, &files[count].size);
		if (files[count].data)
			count++;
		else if (needed == 1)
			fprintf(stderr, "can't read %s\n", path);
	}
	return count;
}

int main(int argc, char **argv) {
	static const char *const interpolations[] = {"none", "linear", "cubic", NULL};
	static const char *const models[] = {"a500", "a1200", "none", NULL};
	static const char *const leds[] = {"song", "off", "on", NULL};
	static char paths[MAX_FILES][1024];
	amigareplay_settings settings;
	amigareplay_option options[MAX_OPTIONS + 1] = {{0}};
	amigareplay_file files[MAX_FILES] = {{0}};
	const char *out = NULL, *c;
	int count = 0, option_count = 0, subsong = 0, all = 0, info = 0, ok = 1, i, n, result = 0;
	unsigned voice_mask = ~0u;
	double limit = -1;
	char *equals;
	amigareplay_module *module;
	amigareplay_error error;

	amigareplay_default_settings(&settings, sizeof settings);
	for (i = 1; i < argc && ok; i++) {
		const char *a = argv[i];
		char *value = i + 1 < argc ? argv[i + 1] : NULL;
		if (a[0] != '-' || !a[1]) {
			ok = count < MAX_FILES;
			if (ok)
				files[count++].name = a;
		} else if (!strcmp(a, "-a")) {
			all = 1;
		} else if (!strcmp(a, "-n")) {
			info = 1;
		} else if (!strcmp(a, "-d")) {
			subsong = -1;
		} else if (value && strchr("ostrlFifLpmVO", a[1]) && !a[2]) {
			i++;
			switch (a[1]) {
			case 'o': out = value; break;
			case 's':
				subsong = atoi(value);
				ok = subsong >= 0;
				break;
			case 't': limit = atof(value); break;
			case 'r': settings.sample_rate = atoi(value); break;
			case 'l': settings.loop_count = atoi(value); break;
			case 'F': settings.fade_seconds = (float)atof(value); break;
			case 'p': settings.stereo_separation = atoi(value); break;
			case 'm': settings.volume_ramping = atoi(value); break;
			case 'i':
				n = choice(value, interpolations);
				ok = n >= 0;
				settings.interpolation = (amigareplay_interpolation)n;
				break;
			case 'f':
				n = choice(value, models);
				ok = n >= 0;
				settings.amiga_model = (amigareplay_amiga_model)n;
				break;
			case 'L':
				n = choice(value, leds);
				ok = n >= 0;
				settings.led_filter = (amigareplay_led_filter)n;
				break;
			case 'V':
				voice_mask = 0;
				for (c = value; *c; c++)
					if (*c >= '0' && *c <= '9')
						voice_mask |= 1u << (*c - '0');
				break;
			case 'O':
				equals = strchr(value, '=');
				ok = equals && option_count < MAX_OPTIONS;
				if (ok) {
					*equals = 0;
					options[option_count].name = value;
					options[option_count++].value = equals + 1;
				}
				break;
			}
		} else {
			ok = 0;
		}
	}
	if (!ok || !count) {
		usage();
		return 1;
	}
	if (settings.loop_count < 0 && limit < 0 && !info) {
		fprintf(stderr, "-l -1 needs -t\n");
		return 1;
	}

	for (i = 0; i < count; i++) {
		files[i].data = read_file(files[i].name, &files[i].size);
		if (!files[i].data) {
			fprintf(stderr, "can't read %s\n", files[i].name);
			return 1;
		}
	}
	if (count == 1)
		count = find_files(files[0].name, files, paths);
	module = amigareplay_open(files, count, &settings, options, &error);
	for (i = 0; i < count; i++)
		free((void *)files[i].data);
	if (!module) {
		fprintf(stderr, "%s: %s\n", files[0].name, amigareplay_error_string(error));
		return 1;
	}

	amigareplay_set_voice_mask(module, voice_mask);
	if (info) {
		print_info(module);
	} else {
		const char *name = out ? out : base_name(files[0].name);
		int subsongs = amigareplay_subsong_count(module), last;
		char path[1200];
		if (subsong < 0)
			subsong = amigareplay_default_subsong(module);
		last = all ? subsongs - 1 : subsong;
		for (i = all ? 0 : subsong; i <= last && !result; i++) {
			if (amigareplay_select_subsong(module, i)) {
				fprintf(stderr, "no subsong %d (the song has %d)\n", i, subsongs);
				result = 1;
			} else {
				if (out && !all)
					snprintf(path, sizeof path, "%s", out);
				else if (all || subsongs > 1)
					snprintf(path, sizeof path, "%s-%d.wav", name, i);
				else
					snprintf(path, sizeof path, "%s.wav", name);
				result = render_wav(module, path, settings.sample_rate, limit);
			}
		}
	}
	amigareplay_close(module);
	return result;
}

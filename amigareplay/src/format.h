// amigareplay: what a format provides. To add one, write its file (format_<name>.cpp) and list it in kFormats
// (amigareplay.cpp): nothing else here, nor in the programs that use the library, needs to know about it.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "amigareplay.h"

namespace amigareplay {

// An open song.
class Player {
public:
	virtual ~Player() = default;

	virtual std::string option(const std::string &name) const = 0;    // see amigareplay_get_option
	virtual std::string metadata(const std::string &name) const = 0;  // all but "format"
	virtual int voices() const { return 4; }
	virtual std::vector<std::string> samples() const { return {}; }
	virtual std::vector<std::string> instruments() const { return {}; }

	virtual int subsongs() const = 0;
	virtual int default_subsong() { return 0; }
	virtual bool subsong_info(int index, amigareplay_subsong_info &info) = 0;
	virtual int subsong_from_id(int id) const { return id >= 0 && id < subsongs() ? id : -1; }
	virtual double duration(int index) = 0;
	virtual bool select(int index) = 0;

	virtual size_t render(float *buffer, size_t frames) = 0;
	virtual size_t render(int16_t *buffer, size_t frames) = 0;
	virtual double seek(double seconds) = 0;
	virtual double position() const = 0;
	virtual bool set_settings(const amigareplay_settings &settings) = 0;
	virtual void set_voice_mask(unsigned mask) = 0;
};

// An option of a format's. The values are separated by spaces; "8-22" is a range of numbers.
struct Option {
	const char *name, *values, *preset;
};

// The options a song is opened with, already checked: each one's value, or its default.
class Options {
public:
	explicit Options(const amigareplay_option *given) : given_(given) {}
	std::string get(const Option &option) const;

private:
	const amigareplay_option *given_;
};

struct Companion {
	std::string name;
	bool needed = true;
};

struct Format {
	const char *name;  // the metadata "format"
	std::vector<Option> options;

	amigareplay_probe_result (*probe)(const amigareplay_file &file);

	// The song's other files.
	std::vector<Companion> (*companions)(const amigareplay_file &file);

	// files: the song's main file, which probe accepted, and any others.
	std::unique_ptr<Player> (*open)(const std::vector<amigareplay_file> &files, const amigareplay_settings &settings,
	                                const Options &options, amigareplay_error &error);
};

const Format &format_tfmx();
const Format &format_soundmon();
const Format &format_hippel();

// The settings every library has, in its own struct.
template <typename Settings>
void copy_settings(const amigareplay_settings &from, Settings &to) {
	to.sample_rate = from.sample_rate;
	to.stereo_separation = from.stereo_separation;
	to.interpolation = static_cast<decltype(to.interpolation)>(from.interpolation);
	to.volume_ramping = from.volume_ramping;
	to.loop_count = from.loop_count;
	to.fade_seconds = from.fade_seconds;
	to.gain = from.gain;
	to.amiga_model = static_cast<decltype(to.amiga_model)>(from.amiga_model);
}

// Amiga text (ISO 8859-1) as UTF-8.
std::string utf8(const char *text);

// The last part of a file's name, after any folders.
std::string base_name(const char *name);

// Whether the values separated by spaces include one, and where: 0 for the first, -1 for none.
int value_index(const char *values, const std::string &value);

}  // namespace amigareplay

// AmigaReplay: the C API, over the formats (format.h).
#define AMIGAREPLAY_BUILDING
#include "amigareplay.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <new>

#include "format.h"

namespace amigareplay {

// In the order they're tried: Hippel is last, as its songs are found by a header anywhere in the start of a file.
static const Format *const kFormats[] = {&format_tfmx(), &format_soundmon(), &format_hippel()};

std::string Options::get(const Option &option) const {
	for (const amigareplay_option *o = given_; o && o->name; o++)
		if (!std::strcmp(o->name, option.name))
			return o->value;
	return option.preset;
}

std::string utf8(const char *text) {
	std::string out;
	for (; *text; text++) {
		const unsigned char c = (unsigned char)*text;
		if (c < 0x80) {
			out += char(c);
		} else {
			out += char(0xC0 | c >> 6);
			out += char(0x80 | (c & 0x3F));
		}
	}
	return out;
}

std::string base_name(const char *name) {
	const std::string s = name ? name : "";
	const size_t at = s.find_last_of("/\\");
	return at == std::string::npos ? s : s.substr(at + 1);
}

int value_index(const char *values, const std::string &value) {
	int index = 0;
	for (const char *p = values; *p; index++) {
		const char *end = std::strchr(p, ' ');
		const std::string v = end ? std::string(p, end) : std::string(p);
		if (v == value)
			return index;
		p = end ? end + 1 : p + v.size();
	}
	return -1;
}

namespace {

// Every format's options.
const std::vector<Option> &all_options() {
	static const std::vector<Option> all = [] {
		std::vector<Option> v;
		for (const Format *f : kFormats)
			v.insert(v.end(), f->options.begin(), f->options.end());
		return v;
	}();
	return all;
}

bool allowed(const Option &option, const char *value) {
	if (!value || std::strchr(value, ' ') || std::strchr(value, '-'))
		return false;
	if (value_index(option.values, value) >= 0)
		return true;
	// A range of numbers.
	char *end;
	const long n = std::strtol(value, &end, 10);
	if (*end || end == value)
		return false;
	for (const char *p = option.values; p; p = std::strchr(p, ' '), p = p ? p + 1 : p) {
		const long from = std::strtol(p, &end, 10);
		if (end != p && *end == '-' && n >= from && n <= std::strtol(end + 1, nullptr, 10))
			return true;
	}
	return false;
}

bool valid_options(const amigareplay_option *options) {
	for (const amigareplay_option *o = options; o && o->name; o++) {
		bool ok = false;
		for (const Option &known : all_options())
			ok = ok || (!std::strcmp(o->name, known.name) && allowed(known, o->value));
		if (!ok)
			return false;
	}
	return true;
}

bool valid(const amigareplay_settings &s) {
	return s.sample_rate >= 8000 && s.sample_rate <= 192000 && s.stereo_separation >= 0 && s.stereo_separation <= 100 &&
	       s.interpolation >= AMIGAREPLAY_INTERPOLATION_NONE && s.interpolation <= AMIGAREPLAY_INTERPOLATION_CUBIC &&
	       s.volume_ramping >= 0 && s.loop_count >= -1 && s.fade_seconds >= 0 && s.gain >= 0 &&
	       s.amiga_model >= AMIGAREPLAY_AMIGA_A500 && s.amiga_model <= AMIGAREPLAY_AMIGA_NONE &&
	       s.led_filter >= AMIGAREPLAY_LED_SONG && s.led_filter <= AMIGAREPLAY_LED_ON;
}

// The caller's settings, which may be an older version's, on top of the defaults.
bool read_settings(const amigareplay_settings *from, amigareplay_settings &to) {
	amigareplay_default_settings(&to, sizeof to);
	if (from) {
		if (from->size < sizeof from->size)
			return false;
		std::memcpy(&to, from, std::min(from->size, sizeof to));
		to.size = sizeof to;
	}
	return valid(to);
}

const Format *find_format(const amigareplay_file &file, amigareplay_probe_result &result) {
	for (const Format *f : kFormats)
		if ((result = f->probe(file)) != AMIGAREPLAY_PROBE_NO)
			return f;
	return nullptr;
}

}  // namespace
}  // namespace amigareplay

using namespace amigareplay;

struct amigareplay_module {
	const Format *format = nullptr;
	std::unique_ptr<Player> player;
	int rate = 0;
	int subsong = 0;
	std::vector<std::string> samples, instruments;
	// Strings returned, kept for as long as the module is. Few enough to look through.
	mutable std::vector<std::unique_ptr<std::string>> strings;

	const char *keep(const std::string &s) const {
		for (const auto &k : strings)
			if (*k == s)
				return k->c_str();
		strings.push_back(std::make_unique<std::string>(s));
		return strings.back()->c_str();
	}
};

extern "C" {

const char *amigareplay_version(void) { return AMIGA_REPLAYERS_VERSION; }

const char *amigareplay_error_string(amigareplay_error error) {
	switch (error) {
	case AMIGAREPLAY_OK:
		return "no error";
	case AMIGAREPLAY_ERROR_ARGUMENT:
		return "invalid argument";
	case AMIGAREPLAY_ERROR_OPTION:
		return "unknown option, or a value it doesn't have";
	case AMIGAREPLAY_ERROR_FORMAT:
		return "not a song of any known format";
	case AMIGAREPLAY_ERROR_FILES:
		return "the song's other files weren't given";
	case AMIGAREPLAY_ERROR_UNSUPPORTED:
		return "a song that can't be played";
	case AMIGAREPLAY_ERROR_MEMORY:
		return "out of memory";
	}
	return "unknown error";
}

void amigareplay_default_settings(amigareplay_settings *settings, size_t size) {
	if (!settings || size < sizeof settings->size)
		return;
	amigareplay_settings s = {};
	s.size = std::min(size, sizeof s);
	s.sample_rate = 44100;
	s.stereo_separation = 100;
	s.interpolation = AMIGAREPLAY_INTERPOLATION_NONE;
	s.volume_ramping = 2000;
	s.loop_count = 0;
	s.fade_seconds = 0;
	s.gain = 1.0f;
	s.amiga_model = AMIGAREPLAY_AMIGA_A500;
	s.led_filter = AMIGAREPLAY_LED_SONG;
	std::memcpy(settings, &s, s.size);
}

int amigareplay_option_count(void) { return int(all_options().size()); }

static const Option *option_at(int index) {
	return index >= 0 && index < amigareplay_option_count() ? &all_options()[size_t(index)] : nullptr;
}

const char *amigareplay_option_name(int index) {
	const Option *o = option_at(index);
	return o ? o->name : nullptr;
}

const char *amigareplay_option_values(int index) {
	const Option *o = option_at(index);
	return o ? o->values : nullptr;
}

const char *amigareplay_option_default(int index) {
	const Option *o = option_at(index);
	return o ? o->preset : nullptr;
}

const char *amigareplay_get_option(const amigareplay_module *m, const char *name) {
	return m && name ? m->keep(m->player->option(name)) : "";
}

amigareplay_probe_result amigareplay_probe(const amigareplay_file *file) {
	amigareplay_probe_result result = AMIGAREPLAY_PROBE_NO;
	if (file && file->data)
		find_format(*file, result);
	return result;
}

int amigareplay_companion_file(const amigareplay_file *file, int index, char *buffer, size_t buffer_size) {
	amigareplay_probe_result result;
	const Format *f = file && file->data && file->name ? find_format(*file, result) : nullptr;
	if (!f || index < 0 || !buffer)
		return 0;
	const std::vector<Companion> all = f->companions(*file);
	if (index >= int(all.size()) || all[size_t(index)].name.size() >= buffer_size)
		return 0;
	std::memcpy(buffer, all[size_t(index)].name.c_str(), all[size_t(index)].name.size() + 1);
	return all[size_t(index)].needed ? 1 : 2;
}

int amigareplay_main_file(const char *name, int index, char *buffer, size_t buffer_size) {
	if (!name || index < 0 || !buffer)
		return 0;
	for (const Format *f : kFormats)
		for (const std::string &main : f->main_files(base_name(name)))
			if (!index--) {
				if (main.size() >= buffer_size)
					return 0;
				std::memcpy(buffer, main.c_str(), main.size() + 1);
				return 1;
			}
	return 0;
}

amigareplay_module *amigareplay_open(const amigareplay_file *files, int count, const amigareplay_settings *settings,
                                     const amigareplay_option *options, amigareplay_error *error) {
	amigareplay_error dummy;
	amigareplay_error &err = error ? *error : dummy;
	amigareplay_settings s;
	err = AMIGAREPLAY_ERROR_ARGUMENT;
	if (!files || count < 1 || !read_settings(settings, s))
		return nullptr;
	for (int i = 0; i < count; i++)
		if (!files[i].data)
			return nullptr;
	err = AMIGAREPLAY_ERROR_OPTION;
	try {
		if (!valid_options(options))
			return nullptr;
		amigareplay_probe_result result;
		const Format *f = find_format(files[0], result);
		err = AMIGAREPLAY_ERROR_FORMAT;
		if (!f)
			return nullptr;
		auto m = std::make_unique<amigareplay_module>();
		err = AMIGAREPLAY_OK;
		m->format = f;
		m->player = f->open(std::vector<amigareplay_file>(files, files + count), s, Options(options), err);
		if (!m->player)
			return nullptr;
		m->rate = s.sample_rate;
		m->samples = m->player->samples();
		m->instruments = m->player->instruments();
		return m.release();
	} catch (const std::bad_alloc &) {
		err = AMIGAREPLAY_ERROR_MEMORY;
		return nullptr;
	}
}

amigareplay_module *amigareplay_open_memory(const void *data, size_t size, const amigareplay_settings *settings,
                                            amigareplay_error *error) {
	const amigareplay_file file = {nullptr, data, size};
	return amigareplay_open(&file, 1, settings, nullptr, error);
}

void amigareplay_close(amigareplay_module *m) { delete m; }

const char *amigareplay_get_metadata(const amigareplay_module *m, const char *name) {
	if (!m || !name)
		return "";
	return !std::strcmp(name, "format") ? m->format->name : m->keep(m->player->metadata(name));
}

int amigareplay_voice_count(const amigareplay_module *m) { return m ? m->player->voices() : 0; }

int amigareplay_sample_count(const amigareplay_module *m) { return m ? int(m->samples.size()) : 0; }

const char *amigareplay_sample_name(const amigareplay_module *m, int index) {
	return index >= 0 && index < amigareplay_sample_count(m) ? m->samples[size_t(index)].c_str() : "";
}

int amigareplay_instrument_count(const amigareplay_module *m) { return m ? int(m->instruments.size()) : 0; }

const char *amigareplay_instrument_name(const amigareplay_module *m, int index) {
	return index >= 0 && index < amigareplay_instrument_count(m) ? m->instruments[size_t(index)].c_str() : "";
}

int amigareplay_subsong_count(const amigareplay_module *m) { return m ? m->player->subsongs() : 0; }

int amigareplay_default_subsong(amigareplay_module *m) { return m ? m->player->default_subsong() : -1; }

int amigareplay_get_subsong_info(amigareplay_module *m, int index, amigareplay_subsong_info *info) {
	if (!m || !info || info->size < sizeof info->size || index < 0 || index >= amigareplay_subsong_count(m))
		return -1;
	amigareplay_subsong_info i = {};
	if (!m->player->subsong_info(index, i))
		return -1;
	i.size = info->size;
	std::memcpy(info, &i, std::min(i.size, sizeof i));
	return 0;
}

int amigareplay_subsong_from_id(const amigareplay_module *m, int id) { return m ? m->player->subsong_from_id(id) : -1; }

double amigareplay_subsong_duration(amigareplay_module *m, int index) {
	return m && index >= 0 && index < amigareplay_subsong_count(m) ? m->player->duration(index) : -1;
}

int amigareplay_select_subsong(amigareplay_module *m, int index) {
	if (!m || index < 0 || index >= amigareplay_subsong_count(m) || !m->player->select(index))
		return -1;
	m->subsong = index;
	return 0;
}

int amigareplay_selected_subsong(const amigareplay_module *m) { return m ? m->subsong : -1; }

size_t amigareplay_render_float(amigareplay_module *m, float *buffer, size_t frames) {
	return m && buffer ? m->player->render(buffer, frames) : 0;
}

size_t amigareplay_render_s16(amigareplay_module *m, int16_t *buffer, size_t frames) {
	return m && buffer ? m->player->render(buffer, frames) : 0;
}

double amigareplay_seek(amigareplay_module *m, double seconds) { return m ? m->player->seek(seconds) : 0; }

double amigareplay_position(const amigareplay_module *m) { return m ? m->player->position() : 0; }

int amigareplay_set_settings(amigareplay_module *m, const amigareplay_settings *settings) {
	amigareplay_settings s;
	if (!m || !settings || !read_settings(settings, s) || s.sample_rate != m->rate)
		return -1;
	return m->player->set_settings(s) ? 0 : -1;
}

int amigareplay_set_voice_mask(amigareplay_module *m, unsigned mask) {
	if (!m)
		return -1;
	m->player->set_voice_mask(mask);
	return 0;
}

}  // extern "C"

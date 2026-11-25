// TFMX songs, played by TFMXreplay: two files (mdat and smpl), or a single TFHD file that holds both.
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>

#include "format.h"
#include "tfmxreplay.h"

namespace amigareplay {

namespace {

// In tfmx_variant's order.
const Option kVariant = {"tfmx.variant",
                         "auto pro 1.5 masterblazer xout rocknroll gemx turrican2 t3small hexuma logical dangerfreak", "auto"};
const Option kMixSetting = {"tfmx.mix_setting", "auto 8-22", "auto"};

class TfmxPlayer : public Player {
public:
	TfmxPlayer(tfmx_module *m, const tfmx_settings &settings) : m_(m), settings_(settings) {}
	~TfmxPlayer() override { tfmx_close(m_); }

	std::string option(const std::string &name) const override {
		if (name == kVariant.name) {
			const char *p = kVariant.values;
			for (int skip = int(tfmx_get_variant(m_)); skip > 0; skip--)
				p = std::strchr(p, ' ') + 1;
			return std::string(p, std::strcspn(p, " "));
		}
		if (name == kMixSetting.name)
			return settings_.mix_setting ? std::to_string(settings_.mix_setting) : "auto";
		return "";
	}
	std::string metadata(const std::string &name) const override {
		return name == "type" ? tfmx_variant_name(tfmx_get_variant(m_)) : name == "message" ? utf8(tfmx_text(m_)) : "";
	}
	int voices() const override { return tfmx_voice_count(m_); }

	int subsongs() const override { return tfmx_subsong_count(m_); }
	int default_subsong() override { return tfmx_default_subsong(m_); }
	bool subsong_info(int index, amigareplay_subsong_info &info) override {
		tfmx_subsong_info i;
		if (tfmx_get_subsong_info(m_, index, &i))
			return false;
		info.id = info.number = i.number;
		info.duration = i.duration;
		info.loop_start = i.loop_start;
		info.loop_length = i.loop_length;
		info.looped = i.looped;
		info.audible = i.audible;
		info.repeat_of = i.repeat_of;
		return true;
	}
	int subsong_from_id(int id) const override { return tfmx_subsong_index(m_, id); }
	double duration(int index) override {
		tfmx_subsong_info i;
		if (settings_.loop_count < 0 && !tfmx_get_subsong_info(m_, index, &i) && i.looped)
			return -1;
		return tfmx_subsong_duration(m_, index);
	}
	bool select(int index) override { return !tfmx_select_subsong(m_, index); }

	size_t render(float *buffer, size_t frames) override { return tfmx_render_float(m_, buffer, frames); }
	size_t render(int16_t *buffer, size_t frames) override { return tfmx_render_s16(m_, buffer, frames); }
	double seek(double seconds) override { return tfmx_seek(m_, seconds); }
	double position() const override { return tfmx_position(m_); }
	bool set_settings(const amigareplay_settings &settings) override {
		copy(settings, settings_);
		return !tfmx_set_settings(m_, &settings_);
	}
	void set_voice_mask(unsigned mask) override { tfmx_set_voice_mask(m_, mask); }

	static void copy(const amigareplay_settings &from, tfmx_settings &to) {
		copy_settings(from, to);
		to.led_filter = from.led_filter == AMIGAREPLAY_LED_ON;  // songs don't switch it
	}

private:
	tfmx_module *m_;
	tfmx_settings settings_;
};

bool is_tfhd(const amigareplay_file &file) { return file.size >= 4 && !std::memcmp(file.data, "TFHD", 4); }

amigareplay_probe_result probe(const amigareplay_file &file) {
	if (!tfmx_probe(file.data, file.size))
		return AMIGAREPLAY_PROBE_NO;
	return is_tfhd(file) ? AMIGAREPLAY_PROBE_YES : AMIGAREPLAY_PROBE_YES_FILES;
}

bool same(const std::string &s, size_t at, const char *word) {
	for (size_t i = 0; word[i]; i++)
		if (at + i >= s.size() || std::tolower((unsigned char)s[at + i]) != word[i])
			return false;
	return true;
}

// 'to' over s at 'at', keeping the case of the original (all capitals or not).
void replace(std::string &s, size_t at, size_t length, const char *to) {
	std::string word = to;
	if (std::isupper((unsigned char)s[s[at] == '.' ? at + 1 : at]))
		std::transform(word.begin(), word.end(), word.begin(), [](unsigned char c) { return char(std::toupper(c)); });
	s.replace(at, length, word);
}

// How the mdat and smpl files are named.
struct Names {
	const char *mdat, *smpl;
	bool prefix;
};
const Names kNames[] = {{"mdat.", "smpl.", true}, {".mdat", ".smpl", false}, {".tfx", ".sam", false}};

// The name of one file of a song from the other's, or "".
std::string other_name(std::string name, const char *Names::*from, const char *Names::*to) {
	for (const Names &n : kNames) {
		const size_t length = std::strlen(n.*from);
		if (name.size() <= length)
			continue;
		const size_t at = n.prefix ? 0 : name.size() - length;
		if (same(name, at, n.*from)) {
			replace(name, at, length, n.*to);
			return name;
		}
	}
	return "";
}

std::vector<Companion> companions(const amigareplay_file &file) {
	const std::string smpl = other_name(base_name(file.name), &Names::mdat, &Names::smpl);
	if (is_tfhd(file) || smpl.empty())
		return {};
	return {{smpl}};
}

std::vector<std::string> main_files(const std::string &name) {
	const std::string mdat = other_name(name, &Names::smpl, &Names::mdat);
	if (mdat.empty())
		return {};
	return {mdat};
}

std::unique_ptr<Player> open(const std::vector<amigareplay_file> &files, const amigareplay_settings &settings,
                             const Options &options, amigareplay_error &error) {
	tfmx_settings s;
	tfmx_default_settings(&s);
	TfmxPlayer::copy(settings, s);
	s.variant = static_cast<tfmx_variant>(value_index(kVariant.values, options.get(kVariant)));
	s.mix_setting = std::atoi(options.get(kMixSetting).c_str());  // 0 for auto
	tfmx_error e = TFMX_OK;
	tfmx_module *m;
	if (is_tfhd(files[0])) {
		m = tfmx_open_single(files[0].data, files[0].size, &s, &e);
	} else {
		if (files.size() < 2) {
			error = AMIGAREPLAY_ERROR_FILES;
			return nullptr;
		}
		m = tfmx_open(files[0].data, files[0].size, files[1].data, files[1].size, &s, &e);
	}
	if (!m) {
		error = e == TFMX_ERROR_MEMORY ? AMIGAREPLAY_ERROR_MEMORY : AMIGAREPLAY_ERROR_FORMAT;
		return nullptr;
	}
	return std::make_unique<TfmxPlayer>(m, s);
}

}  // namespace

const Format &format_tfmx() {
	static const Format format = {"tfmx", {kVariant, kMixSetting}, probe, companions, main_files, open};
	return format;
}

}  // namespace amigareplay

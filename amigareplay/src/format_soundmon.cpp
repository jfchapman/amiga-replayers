// BP SoundMon songs, played by SoundMonReplay.
#include "format.h"
#include "soundmonreplay.h"

namespace amigareplay {

namespace {

const int kInstruments = 15;

class SoundMonPlayer : public Player {
public:
	explicit SoundMonPlayer(smon_module *m) : m_(m) {}
	~SoundMonPlayer() override { smon_close(m_); }

	std::string option(const std::string &) const override { return ""; }
	std::string metadata(const std::string &name) const override {
		if (name == "title")
			return utf8(smon_title(m_));
		if (name != "type")
			return "";
		const std::string format = smon_format(m_);
		return "BP SoundMon " + std::string(format == "BPSM" ? "1" : format == "V.3" ? "3" : "2") + " (" + format + ")";
	}
	std::vector<std::string> instruments() const override {
		std::vector<std::string> names;
		for (int i = 1; i <= kInstruments; i++)
			names.push_back(utf8(smon_instrument_name(m_, i)));
		return names;
	}

	int subsongs() const override { return smon_subsong_count(m_); }
	bool subsong_info(int index, amigareplay_subsong_info &info) override {
		smon_subsong_info i;
		if (smon_get_subsong_info(m_, index, &i))
			return false;
		info.id = index;
		info.number = index + 1;  // songs don't number them
		info.duration = i.duration;
		info.loop_start = i.loop_start;
		info.loop_length = i.loop_length;
		info.looped = i.looped;
		info.audible = i.audible;
		info.repeat_of = -1;  // each starts on a step that no other reaches
		return true;
	}
	double duration(int index) override { return smon_subsong_duration(m_, index); }
	bool select(int index) override { return !smon_select_subsong(m_, index); }

	size_t render(float *buffer, size_t frames) override { return smon_render_float(m_, buffer, frames); }
	size_t render(int16_t *buffer, size_t frames) override { return smon_render_s16(m_, buffer, frames); }
	double seek(double seconds) override { return smon_seek(m_, seconds); }
	double position() const override { return smon_position(m_); }
	bool set_settings(const amigareplay_settings &settings) override {
		smon_settings s;
		copy(settings, s);
		return !smon_set_settings(m_, &s);
	}
	void set_voice_mask(unsigned mask) override { smon_set_voice_mask(m_, mask); }

	static void copy(const amigareplay_settings &from, smon_settings &to) {
		smon_default_settings(&to);
		copy_settings(from, to);
		to.led_filter = static_cast<smon_led_filter>(from.led_filter);
	}

private:
	smon_module *m_;
};

amigareplay_probe_result probe(const amigareplay_file &file) {
	return smon_probe(file.data, file.size) ? AMIGAREPLAY_PROBE_YES : AMIGAREPLAY_PROBE_NO;
}

std::vector<Companion> companions(const amigareplay_file &) { return {}; }

std::vector<std::string> main_files(const std::string &) { return {}; }

std::unique_ptr<Player> open(const std::vector<amigareplay_file> &files, const amigareplay_settings &settings,
                             const Options &, amigareplay_error &error) {
	smon_settings s;
	SoundMonPlayer::copy(settings, s);
	smon_error e = SMON_OK;
	smon_module *m = smon_open(files[0].data, files[0].size, &s, &e);
	if (!m) {
		error = e == SMON_ERROR_MEMORY ? AMIGAREPLAY_ERROR_MEMORY : AMIGAREPLAY_ERROR_FORMAT;
		return nullptr;
	}
	return std::make_unique<SoundMonPlayer>(m);
}

}  // namespace

const Format &format_soundmon() {
	static const Format format = {"soundmon", {}, probe, companions, main_files, open};
	return format;
}

}  // namespace amigareplay

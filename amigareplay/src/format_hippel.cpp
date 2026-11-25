// Jochen Hippel songs, played by HippelReplay.
#include "format.h"
#include "hippelreplay.h"

namespace amigareplay {

namespace {

// In hippel_replayer's order.
const Option kReplayer = {"hippel.replayer", "auto older newer 2708 coso", "auto"};

class HippelPlayer : public Player {
public:
	explicit HippelPlayer(hippel_module *m) : m_(m) {}
	~HippelPlayer() override { hippel_close(m_); }

	std::string option(const std::string &name) const override {
		if (name != kReplayer.name)
			return "";
		const std::string rules = hippel_replayer_name(m_);
		return rules == "older" || rules == "newer" || rules == "2708" ? rules : "coso";  // 7-voice songs use these too
	}
	std::string metadata(const std::string &name) const override { return name == "type" ? hippel_format(m_) : ""; }
	int voices() const override { return hippel_voice_count(m_); }
	std::vector<std::string> samples() const override {
		std::vector<std::string> names;
		for (int i = 0; i < hippel_sample_count(m_); i++)
			names.push_back(utf8(hippel_sample_name(m_, i)));
		return names;
	}

	int subsongs() const override { return hippel_subsong_count(m_); }
	int default_subsong() override { return hippel_default_subsong(m_); }
	bool subsong_info(int index, amigareplay_subsong_info &info) override {
		hippel_subsong_info i;
		if (hippel_get_subsong_info(m_, index, &i))
			return false;
		info.id = index;
		info.number = i.number;
		info.duration = i.duration;
		info.loop_start = i.loop_start;
		info.loop_length = i.loop_length;
		info.looped = i.looped;
		info.audible = 1;  // silent ones are left out
		info.repeat_of = i.repeat_of;
		return true;
	}
	double duration(int index) override { return hippel_subsong_duration(m_, index); }
	bool select(int index) override { return !hippel_select_subsong(m_, index); }

	size_t render(float *buffer, size_t frames) override { return hippel_render_float(m_, buffer, frames); }
	size_t render(int16_t *buffer, size_t frames) override { return hippel_render_s16(m_, buffer, frames); }
	double seek(double seconds) override { return hippel_seek(m_, seconds); }
	double position() const override { return hippel_position(m_); }
	bool set_settings(const amigareplay_settings &settings) override {
		hippel_settings s;
		copy(settings, s);
		return !hippel_set_settings(m_, &s);
	}
	void set_voice_mask(unsigned mask) override { hippel_set_voice_mask(m_, mask); }

	static void copy(const amigareplay_settings &from, hippel_settings &to) {
		hippel_default_settings(&to);
		copy_settings(from, to);
		to.led_filter = from.led_filter == AMIGAREPLAY_LED_ON ? HIPPEL_LED_ON : HIPPEL_LED_OFF;  // songs don't switch it
	}

private:
	hippel_module *m_;
};

amigareplay_probe_result probe(const amigareplay_file &file) {
	return hippel_probe(file.data, file.size) ? AMIGAREPLAY_PROBE_YES : AMIGAREPLAY_PROBE_NO;
}

std::vector<Companion> companions(const amigareplay_file &) { return {}; }

std::vector<std::string> main_files(const std::string &) { return {}; }

std::unique_ptr<Player> open(const std::vector<amigareplay_file> &files, const amigareplay_settings &settings,
                             const Options &options, amigareplay_error &error) {
	hippel_settings s;
	HippelPlayer::copy(settings, s);
	s.replayer = static_cast<hippel_replayer>(value_index(kReplayer.values, options.get(kReplayer)));
	hippel_error e = HIPPEL_OK;
	hippel_module *m = hippel_open(files[0].data, files[0].size, &s, &e);
	if (!m) {
		error = e == HIPPEL_ERROR_MEMORY        ? AMIGAREPLAY_ERROR_MEMORY
		        : e == HIPPEL_ERROR_UNSUPPORTED ? AMIGAREPLAY_ERROR_UNSUPPORTED
		                                        : AMIGAREPLAY_ERROR_FORMAT;
		return nullptr;
	}
	return std::make_unique<HippelPlayer>(m);
}

}  // namespace

const Format &format_hippel() {
	static const Format format = {"hippel", {kReplayer}, probe, companions, main_files, open};
	return format;
}

}  // namespace amigareplay

// SoundMonReplay: the C API.
#define SMON_BUILDING
#include "soundmonreplay.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <new>
#include <set>
#include <tuple>
#include <vector>

#include "module.h"
#include "paula.h"
#include "playback.h"
#include "replayer.h"

namespace {

// One tick is one PAL frame: 313 lines of 227 colour clocks.
const double kTickSeconds = 313.0 * 227.0 / amiga::kPaulaClock;
const int kMaxTicks = int(30 * 60 / kTickSeconds);  // 30 minutes

struct Subsong {
	int start_step = 0;
	int ticks = -1;       // one play-through
	int loop_tick = -1;   // where it continues from
	bool looped = false;
	bool audible = false;
};

// For each row (step * 16 + row), whether the V.2 repeat counter can still change what plays from
// there: whether, going on through the rows in order, the next repeat option is option 7 (which
// reads the counter) rather than option 6 (which sets it). Until a jump the rows run in order, and
// only option 7 jumps.
std::vector<bool> repeat_live(const smon::Module &m) {
	const int n = m.step_count * 16;
	std::vector<int> event(size_t(n), 0);  // 7, 6 or 0 (neither) on each row, the first voice in order
	for (int p = 0; p < n; p++)
		for (int v = 3; v >= 0 && !event[size_t(p)]; v--) {
			const smon::Entry &e = m.steps[size_t(p / 16) * 4 + size_t(v)];
			const int option = m.row(e.track, p % 16).option;
			if (option == 6 || option == 7)
				event[size_t(p)] = option;
		}
	// The next event at or after each row, going round past the last step; twice round settles it.
	std::vector<bool> live(size_t(n), false);
	int next = 0;
	for (int i = 2 * n - 1; i >= 0; i--) {
		const int p = i % n;
		if (event[size_t(p)])
			next = event[size_t(p)];
		live[size_t(p)] = next == 7;
	}
	return live;
}

// Plays a song from a step without sound and finds where it starts repeating: the first row read
// a second time in the same state: step, row, speed, LED filter and, for V.2, the repeat counter
// while it can still change what plays (alien breed - menu long sets it again before using it, so
// the song repeats from its start although the counter is different there). The speed matters
// where a subsong starts before the song's own speed setting (wacky darts (longer)).
Subsong analyse(const smon::Module &m, int start, std::set<int> &visited) {
	Subsong s;
	s.start_step = start;
	smon::Replayer r(m);
	r.start(start);
	std::map<std::tuple<int, int, int, int, int>, int> seen;
	const std::vector<bool> live = m.version == smon::Version::V3 ? std::vector<bool>() : repeat_live(m);
	std::vector<int> note_ticks;
	std::vector<smon::Write> out;
	for (int t = 0; t < kMaxTicks; t++) {
		out.clear();
		r.tick(out);
		for (const smon::Write &w : out)
			if (w.reg == smon::Reg::DMA_ON) {
				note_ticks.push_back(t);
				break;
			}
		const smon::Replayer::RowEvent &e = r.last_row();
		if (!e.read)
			continue;
		visited.insert(e.step);
		// A repeat counter that has wrapped (a loop without option 6 runs 255 times) counts as one
		// state, so such a loop is found on its second pass.
		const int next = r.step() * 16 + r.row();  // the row read next, after any jump
		const bool counts = !live.empty() && next >= 0 && next < int(live.size()) && live[size_t(next)];
		const int rep = counts ? std::min(r.repeat_counter(), 9) : 0;
		auto key = std::make_tuple(e.step, e.row, rep, r.speed(), r.filter());
		auto it = seen.find(key);
		if (it != seen.end()) {
			s.ticks = t;
			s.loop_tick = it->second;
			break;
		}
		seen[key] = t;
	}
	s.audible = !note_ticks.empty();
	if (s.ticks > 0) {
		for (int t : note_ticks)
			if (t >= s.loop_tick && t < s.ticks)
				s.looped = true;
	}
	return s;
}

}  // namespace

struct smon_module {
	smon::Module song;
	smon::Replayer *replayer = nullptr;
	amiga::Paula paula;
	smon_settings settings;
	std::vector<Subsong> subsongs;
	int current = 0;

	std::vector<smon::Write> writes;
	long long ticks_done = 0;     // ticks played since the start of the subsong
	double frames_per_tick = 0;
	amiga::Playback playback;

	~smon_module() { delete replayer; }
};

extern "C" {

const char *smon_version(void) { return AMIGA_REPLAYERS_VERSION; }

const char *smon_error_string(smon_error error) {
	switch (error) {
	case SMON_OK:
		return "no error";
	case SMON_ERROR_ARGUMENT:
		return "invalid argument";
	case SMON_ERROR_FORMAT:
		return "not a SoundMon song";
	case SMON_ERROR_MEMORY:
		return "out of memory";
	}
	return "unknown error";
}

void smon_default_settings(smon_settings *s) {
	if (!s)
		return;
	s->sample_rate = 44100;
	s->stereo_separation = 100;
	s->interpolation = SMON_INTERPOLATION_NONE;
	s->volume_ramping = 2000;
	s->loop_count = 0;
	s->gain = 1.0f;
	s->amiga_model = SMON_AMIGA_A500;
	s->led_filter = SMON_LED_SONG;
	s->fade_seconds = 0;
}

static double play_seconds(const smon_module *m, const Subsong &s) {
	if (s.ticks < 0)
		return -1;
	double t = s.ticks * kTickSeconds;
	if (s.looped) {
		if (m->settings.loop_count < 0)
			return -1;
		t += m->settings.loop_count * (s.ticks - s.loop_tick) * kTickSeconds + m->settings.fade_seconds;
	}
	return t;
}

int smon_select_subsong(smon_module *m, int index) {
	if (!m || index < 0 || index >= int(m->subsongs.size()))
		return -1;
	m->current = index;
	const Subsong &s = m->subsongs[size_t(index)];
	m->replayer->start(s.start_step);
	m->paula.reset();
	m->paula.set_memory(&m->replayer->memory());
	amiga::set_output(m->paula, m->settings);
	m->paula.set_led(m->settings.led_filter == SMON_LED_ON);
	m->ticks_done = 0;
	m->frames_per_tick = kTickSeconds * m->settings.sample_rate;
	m->playback.start(m->settings.sample_rate, play_seconds(m, s), s.looped ? m->settings.fade_seconds : 0);
	return 0;
}

smon_module *smon_open(const void *data, size_t size, const smon_settings *settings, smon_error *error) {
	smon_error dummy;
	smon_error &err = error ? *error : dummy;
	if (!data) {
		err = SMON_ERROR_ARGUMENT;
		return nullptr;
	}
	smon_module *m = new (std::nothrow) smon_module();
	if (!m) {
		err = SMON_ERROR_MEMORY;
		return nullptr;
	}
	if (settings) {
		if (!amiga::valid_settings(settings)) {
			delete m;
			err = SMON_ERROR_ARGUMENT;
			return nullptr;
		}
		m->settings = *settings;
	} else {
		smon_default_settings(&m->settings);
	}
	try {
		if (!m->song.load(static_cast<const uint8_t *>(data), size)) {
			delete m;
			err = SMON_ERROR_FORMAT;
			return nullptr;
		}
		std::set<int> visited;
		m->subsongs.push_back(analyse(m->song, 0, visited));
		for (int s = 0; s < m->song.step_count; s++) {
			if (visited.count(s))
				continue;
			Subsong sub = analyse(m->song, s, visited);
			if (sub.audible)
				m->subsongs.push_back(sub);
		}
		m->replayer = new smon::Replayer(m->song);
	} catch (const std::bad_alloc &) {
		delete m;
		err = SMON_ERROR_MEMORY;
		return nullptr;
	}
	smon_select_subsong(m, 0);
	err = SMON_OK;
	return m;
}

int smon_probe(const void *data, size_t size) {
	return smon::Module::probe(static_cast<const uint8_t *>(data), size) ? 1 : 0;
}

void smon_close(smon_module *m) { delete m; }

const char *smon_title(const smon_module *m) { return m ? m->song.title.c_str() : ""; }

const char *smon_format(const smon_module *m) {
	if (!m)
		return "";
	return m->song.version == smon::Version::BPSM ? "BPSM" : m->song.version == smon::Version::V3 ? "V.3" : "V.2";
}

const char *smon_instrument_name(const smon_module *m, int instrument) {
	if (!m || instrument < 1 || instrument > 15 || m->song.instruments[instrument].synth)
		return "";
	return m->song.instruments[instrument].sample.name.c_str();
}

int smon_subsong_count(const smon_module *m) { return m ? int(m->subsongs.size()) : 0; }

int smon_get_subsong_info(const smon_module *m, int index, smon_subsong_info *info) {
	if (!m || !info || index < 0 || index >= int(m->subsongs.size()))
		return -1;
	const Subsong &s = m->subsongs[size_t(index)];
	info->start_step = s.start_step;
	info->duration = s.ticks < 0 ? -1 : s.ticks * kTickSeconds;
	info->loop_start = s.ticks < 0 ? -1 : s.loop_tick * kTickSeconds;
	info->loop_length = s.ticks < 0 ? 0 : (s.ticks - s.loop_tick) * kTickSeconds;
	info->looped = s.looped ? 1 : 0;
	return 0;
}

double smon_subsong_duration(const smon_module *m, int index) {
	if (!m || index < 0 || index >= int(m->subsongs.size()))
		return -1;
	return play_seconds(m, m->subsongs[size_t(index)]);
}

static void run_tick(smon_module *m) {
	m->writes.clear();
	m->replayer->tick(m->writes);
	for (const smon::Write &w : m->writes) {
		amiga::Channel &c = m->paula.channel(w.channel);
		switch (w.reg) {
		case smon::Reg::LC:
			c.lc = w.value;
			break;
		case smon::Reg::LEN:
			c.len = uint16_t(w.value);
			break;
		case smon::Reg::PER:
			c.per = uint16_t(w.value);
			break;
		case smon::Reg::VOL:
			c.vol = int(w.value & 0x7F);
			break;
		case smon::Reg::DMA_ON:
			m->paula.dma_on(w.channel);
			break;
		case smon::Reg::DMA_OFF:
			m->paula.dma_off(w.channel);
			break;
		}
	}
	if (m->settings.led_filter == SMON_LED_SONG)
		m->paula.set_led(m->replayer->filter() != 0);
	m->ticks_done++;
}

size_t smon_render_float(smon_module *m, float *buffer, size_t frames) {
	if (!m || !buffer)
		return 0;
	return m->playback.render(m->paula, buffer, frames, [m] {
		run_tick(m);
		return m->frames_per_tick;
	});
}

size_t smon_render_s16(smon_module *m, int16_t *buffer, size_t frames) {
	if (!m || !buffer)
		return 0;
	return m->playback.render_s16(buffer, frames, [m](float *out, size_t n) { return smon_render_float(m, out, n); });
}

double smon_seek(smon_module *m, double seconds) {
	if (!m)
		return 0;
	smon_select_subsong(m, m->current);
	const long long target = (long long)(std::max(0.0, seconds) / kTickSeconds);
	while (m->ticks_done < target) {
		if (m->playback.past_end(double(m->ticks_done + 1) * m->frames_per_tick))
			break;
		run_tick(m);
	}
	m->playback.skip_to((long long)(double(m->ticks_done) * m->frames_per_tick));
	return m->ticks_done * kTickSeconds;
}

double smon_position(const smon_module *m) {
	return m ? double(m->playback.frames()) / m->settings.sample_rate : 0;
}

int smon_set_settings(smon_module *m, const smon_settings *settings) {
	if (!m || !settings || !amiga::valid_settings(settings))
		return -1;
	const int rate = m->settings.sample_rate;
	m->settings = *settings;
	m->settings.sample_rate = rate;
	amiga::set_output(m->paula, m->settings);
	if (m->settings.led_filter != SMON_LED_SONG)
		m->paula.set_led(m->settings.led_filter == SMON_LED_ON);
	return 0;
}

int smon_set_voice_mask(smon_module *m, unsigned mask) {
	if (!m)
		return -1;
	m->paula.set_voice_mask(mask);
	return 0;
}

}  // extern "C"

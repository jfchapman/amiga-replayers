// HippelReplay: the C API.
#define HIPPEL_BUILDING
#include "hippelreplay.h"

#include <algorithm>
#include <cmath>
#include <new>
#include <utility>
#include <vector>

#include "mixer.h"
#include "module.h"
#include "paula.h"
#include "replayer.h"

namespace {

const double kMaxSeconds = 30 * 60;

// 7 voices: where the mix buffers are in Paula's memory, after the file. The gap keeps samples that
// run past the end of the file from reaching them.
const uint32_t kMixGap = 0x30000;
const uint32_t kMixBytes = 1024;
const int kMixBuffers = 4;

struct Subsong {
	int number = 0;
	hippel::Song song;
	int ticks = -1;      // one play-through
	int loop_tick = -1;  // where it continues from
	double seconds = -1, loop_seconds = -1;  // the same as times
	bool looped = false;
	bool audible = false;
	int repeat_of = -1;
	std::vector<std::pair<int, int>> rows;  // what it plays: track row * 256 + row, and speed; sorted
};

// Plays a song without sound and finds where it starts repeating: the first row read a second
// time. A song's rows run in order from its first track row to its last and round again, so
// nothing else has to match.
Subsong analyse(const hippel::Module &m, int number) {
	Subsong s;
	s.number = number;
	s.song = m.songs[size_t(number - 1)];
	if (!s.song.speed || s.song.last < s.song.first || s.song.last >= m.track_rows)
		return s;
	hippel::Replayer r(m);
	r.start(s.song);
	std::vector<int> seen(size_t(s.song.last - s.song.first + 1) * size_t(m.rows), -1);
	std::vector<int> starts;
	hippel::VoiceOut out[7];
	std::vector<double> times;
	double now = 0;
	for (int t = 0; now < kMaxSeconds; t++) {
		times.push_back(now);
		const double length = r.tick_clocks() / hippel::kPaulaClock;
		if (length <= 0)
			return Subsong();
		r.tick(out);
		if (r.stopped()) {
			s.ticks = t;
			s.seconds = now;
			break;
		}
		const hippel::Replayer::RowEvent &e = r.last_row();
		if (e.read) {
			int &first = seen[size_t(e.pos - s.song.first) * size_t(m.rows) + size_t(e.row)];
			if (first >= 0) {
				s.ticks = t;
				s.seconds = now;
				s.loop_tick = first;
				s.loop_seconds = times[size_t(first)];
				break;
			}
			first = t;
			s.rows.push_back({e.pos * 256 + e.row, e.speed});
		}
		for (const hippel::VoiceOut &o : out)
			if (o.active && o.trigger) {
				starts.push_back(t);
				break;
			}
		now += length;
	}
	s.audible = !starts.empty();
	s.looped = s.loop_tick >= 0 && s.audible && starts.back() >= s.loop_tick;
	std::sort(s.rows.begin(), s.rows.end());
	return s;
}

// Finds the subsongs whose music is all in another: the same rows, at the same speed. The longest of
// them is the one kept.
void find_repeats(std::vector<Subsong> &subsongs) {
	std::vector<size_t> order;
	for (size_t i = 0; i < subsongs.size(); i++)
		if (subsongs[i].ticks >= 0)
			order.push_back(i);
	std::stable_sort(order.begin(), order.end(),
	                 [&](size_t a, size_t b) { return subsongs[a].rows.size() > subsongs[b].rows.size(); });
	std::vector<size_t> kept;
	for (size_t i : order) {
		for (size_t k : kept)
			if (std::includes(subsongs[k].rows.begin(), subsongs[k].rows.end(), subsongs[i].rows.begin(), subsongs[i].rows.end())) {
				subsongs[i].repeat_of = int(k);
				break;
			}
		if (subsongs[i].repeat_of < 0)
			kept.push_back(i);
	}
}

}  // namespace

struct hippel_module {
	hippel::Module song;
	hippel::Replayer *replayer = nullptr;
	hippel::Mixer *mixer = nullptr;  // 7 voices
	hippel::Paula paula;
	std::vector<uint8_t> memory;  // 7 voices: the file and the mix buffers
	hippel_settings settings;
	std::vector<Subsong> subsongs;
	int current = 0;

	// 7 voices: the next tick, played ahead because its buffer follows on from the current one
	struct Tick {
		hippel::VoiceOut out[7];
		uint32_t buffer = 0;
		int words = 0;
		double seconds = 0;
	} next;
	int buffer_no = 0;

	double seconds_done = 0;      // time played since the start of the subsong, in whole ticks
	double tick_frames_left = 0;  // output frames left in the current tick
	long long frames_done = 0;
	long long end_frame = -1;     // where playback stops, -1 = never
	long long fade_from = -1;     // where the fade-out starts
	std::vector<float> mix;

	~hippel_module() {
		delete mixer;
		delete replayer;
	}
};

extern "C" {

const char *hippel_version(void) { return AMIGA_REPLAYERS_VERSION; }

const char *hippel_error_string(hippel_error error) {
	switch (error) {
	case HIPPEL_OK:
		return "no error";
	case HIPPEL_ERROR_ARGUMENT:
		return "invalid argument";
	case HIPPEL_ERROR_FORMAT:
		return "not a Hippel song";
	case HIPPEL_ERROR_UNSUPPORTED:
		return "a Hippel song this library does not support";
	case HIPPEL_ERROR_MEMORY:
		return "out of memory";
	}
	return "unknown error";
}

void hippel_default_settings(hippel_settings *s) {
	if (!s)
		return;
	s->sample_rate = 44100;
	s->stereo_separation = 100;
	s->interpolation = HIPPEL_INTERPOLATION_NONE;
	s->volume_ramping = 2000;
	s->loop_count = 0;
	s->gain = 1.0f;
	s->amiga_model = HIPPEL_AMIGA_A500;
	s->led_filter = HIPPEL_LED_OFF;
	s->replayer = HIPPEL_REPLAYER_AUTO;
	s->fade_seconds = 0;
}

static bool valid(const hippel_settings *s) {
	return s->sample_rate >= 8000 && s->sample_rate <= 192000 && s->stereo_separation >= 0 &&
	       s->stereo_separation <= 100 && s->volume_ramping >= 0 && s->loop_count >= -1 && s->gain >= 0 &&
	       s->fade_seconds >= 0;
}

static void apply_output(hippel_module *m) {
	const hippel_settings &s = m->settings;
	m->paula.set_output(s.sample_rate, s.interpolation, s.volume_ramping, s.stereo_separation, s.gain, s.amiga_model);
}

static double play_seconds(const hippel_module *m, const Subsong &s) {
	if (s.ticks < 0)
		return -1;
	double t = s.seconds;
	if (s.looped) {
		if (m->settings.loop_count < 0)
			return -1;
		t += m->settings.loop_count * (s.seconds - s.loop_seconds) + m->settings.fade_seconds;
	}
	return t;
}

static void play_ahead(hippel_module *m) {
	hippel_module::Tick &t = m->next;
	t.words = m->replayer->mix_words();
	t.seconds = m->replayer->tick_clocks() / hippel::kPaulaClock;
	m->replayer->tick(t.out);
	m->buffer_no = (m->buffer_no + 1) % kMixBuffers;
	t.buffer = uint32_t(m->song.file.size()) + kMixGap + kMixBytes * uint32_t(m->buffer_no);
	m->mixer->tick(t.out + 3, m->replayer->mix_samples(), 2 * t.words, reinterpret_cast<int8_t *>(&m->memory[t.buffer]));
}

int hippel_select_subsong(hippel_module *m, int index) {
	if (!m || index < 0 || index >= int(m->subsongs.size()))
		return -1;
	m->current = index;
	const Subsong &s = m->subsongs[size_t(index)];
	m->replayer->start(s.song);
	m->paula.reset();
	m->paula.set_memory(m->mixer ? &m->memory : &m->song.file);
	apply_output(m);
	m->paula.set_led(m->settings.led_filter == HIPPEL_LED_ON);
	if (m->mixer) {
		m->mixer->reset();
		play_ahead(m);
	}
	m->seconds_done = 0;
	m->tick_frames_left = 0;
	m->frames_done = 0;
	const double secs = play_seconds(m, s);
	m->end_frame = secs < 0 ? -1 : (long long)(secs * m->settings.sample_rate);
	m->fade_from = (s.looped && m->settings.fade_seconds > 0 && m->end_frame >= 0)
	                   ? m->end_frame - (long long)(m->settings.fade_seconds * m->settings.sample_rate)
	                   : -1;
	return 0;
}

hippel_module *hippel_open(const void *data, size_t size, const hippel_settings *settings, hippel_error *error) {
	hippel_error dummy;
	hippel_error &err = error ? *error : dummy;
	if (!data) {
		err = HIPPEL_ERROR_ARGUMENT;
		return nullptr;
	}
	hippel_module *m = new (std::nothrow) hippel_module();
	if (!m) {
		err = HIPPEL_ERROR_MEMORY;
		return nullptr;
	}
	if (settings) {
		if (!valid(settings)) {
			delete m;
			err = HIPPEL_ERROR_ARGUMENT;
			return nullptr;
		}
		m->settings = *settings;
	} else {
		hippel_default_settings(&m->settings);
	}
	try {
		const hippel::LoadResult r = m->song.load(static_cast<const uint8_t *>(data), size, m->settings.replayer);
		if (r != hippel::LoadResult::Ok) {
			delete m;
			err = r == hippel::LoadResult::Unsupported ? HIPPEL_ERROR_UNSUPPORTED : HIPPEL_ERROR_FORMAT;
			return nullptr;
		}
		for (int n = 1; n <= int(m->song.songs.size()); n++) {
			const Subsong sub = analyse(m->song, n);
			if (sub.audible)
				m->subsongs.push_back(sub);
		}
		find_repeats(m->subsongs);
		if (m->subsongs.empty()) {
			delete m;
			err = HIPPEL_ERROR_FORMAT;
			return nullptr;
		}
		m->replayer = new hippel::Replayer(m->song);
		if (m->song.voices == 7) {
			m->mixer = new hippel::Mixer(m->song);
			m->memory = m->song.file;
			m->memory.resize(m->song.file.size() + kMixGap + kMixBytes * kMixBuffers);
		}
	} catch (const std::bad_alloc &) {
		delete m;
		err = HIPPEL_ERROR_MEMORY;
		return nullptr;
	}
	hippel_select_subsong(m, 0);
	err = HIPPEL_OK;
	return m;
}

int hippel_probe(const void *data, size_t size) {
	return hippel::Module::probe(static_cast<const uint8_t *>(data), size) == hippel::LoadResult::Ok ? 1 : 0;
}

void hippel_close(hippel_module *m) { delete m; }

const char *hippel_format(const hippel_module *m) {
	if (!m)
		return "";
	if (m->song.voices == 7)
		return m->song.coso ? "Hippel 7V COSO" : "Hippel 7V";
	return m->song.coso ? "Hippel COSO" : "Hippel";
}

const char *hippel_replayer_name(const hippel_module *m) { return m ? m->song.build.c_str() : ""; }

int hippel_voice_count(const hippel_module *m) { return m ? m->song.voices : 0; }

int hippel_default_subsong(const hippel_module *m) {
	int best = 0;
	for (int i = 1; m && i < int(m->subsongs.size()); i++)
		if (m->subsongs[size_t(i)].seconds > m->subsongs[size_t(best)].seconds && m->subsongs[size_t(i)].repeat_of < 0)
			best = i;
	return best;
}

int hippel_sample_count(const hippel_module *m) { return m ? int(m->song.samples.size()) : 0; }

const char *hippel_sample_name(const hippel_module *m, int sample) {
	if (!m || sample < 0 || sample >= int(m->song.samples.size()))
		return "";
	return m->song.samples[size_t(sample)].name.c_str();
}

int hippel_subsong_count(const hippel_module *m) { return m ? int(m->subsongs.size()) : 0; }

int hippel_get_subsong_info(const hippel_module *m, int index, hippel_subsong_info *info) {
	if (!m || !info || index < 0 || index >= int(m->subsongs.size()))
		return -1;
	const Subsong &s = m->subsongs[size_t(index)];
	info->number = s.number;
	info->duration = s.seconds;
	info->loop_start = s.loop_seconds;
	info->loop_length = s.loop_tick < 0 ? 0 : s.seconds - s.loop_seconds;
	info->looped = s.looped ? 1 : 0;
	info->repeat_of = s.repeat_of;
	return 0;
}

double hippel_subsong_duration(const hippel_module *m, int index) {
	if (!m || index < 0 || index >= int(m->subsongs.size()))
		return -1;
	return play_seconds(m, m->subsongs[size_t(index)]);
}

// Plays a tick; returns how long it lasts in seconds.
static double run_tick(hippel_module *m) {
	hippel::VoiceOut out[7];
	double seconds;
	if (m->mixer) {
		const hippel_module::Tick now = m->next;
		std::copy(now.out, now.out + 7, out);
		seconds = now.seconds;
		play_ahead(m);
		hippel::Channel &c = m->paula.channel(3);
		c.per = uint16_t(m->song.mix_period);
		c.vol = 64;
		m->paula.stream(3, now.buffer, uint16_t(now.words), m->next.buffer, uint16_t(m->next.words));
	} else {
		seconds = m->replayer->tick_clocks() / hippel::kPaulaClock;
		m->replayer->tick(out);
	}
	for (int n = 0; n < (m->mixer ? 3 : 4); n++) {
		const hippel::VoiceOut &o = out[n];
		if (!o.active)
			continue;
		hippel::Channel &c = m->paula.channel(n);
		c.per = o.per;
		c.vol = o.vol & 0x7F;
		if (o.trigger || !o.dma)
			m->paula.dma_off(n);
		if (o.trigger) {
			c.lc = o.start.at;
			c.len = o.start.words;
			m->paula.dma_on(n);
		}
		c.lc = o.loop.at;
		c.len = o.loop.words;
	}
	m->seconds_done += seconds;
	return seconds;
}

size_t hippel_render_float(hippel_module *m, float *buffer, size_t frames) {
	if (!m || !buffer)
		return 0;
	size_t done = 0;
	while (done < frames) {
		if (m->end_frame >= 0 && m->frames_done >= m->end_frame)
			break;
		if (m->tick_frames_left < 1) {
			m->tick_frames_left += run_tick(m) * m->settings.sample_rate;
		}
		size_t n = std::min<size_t>(frames - done, size_t(m->tick_frames_left));
		if (m->end_frame >= 0)
			n = std::min<size_t>(n, size_t(m->end_frame - m->frames_done));
		if (n == 0)
			n = 1;
		float *out = buffer + 2 * done;
		std::fill(out, out + 2 * n, 0.0f);
		m->paula.render(out, int(n));
		if (m->fade_from >= 0) {
			const double span = double(m->end_frame - m->fade_from);
			for (size_t i = 0; i < n; i++) {
				const long long f = m->frames_done + (long long)i;
				if (f > m->fade_from) {
					const float g = float(std::max(0.0, 1.0 - double(f - m->fade_from) / span));
					out[2 * i] *= g;
					out[2 * i + 1] *= g;
				}
			}
		}
		m->tick_frames_left -= double(n);
		m->frames_done += (long long)n;
		done += n;
	}
	return done;
}

size_t hippel_render_s16(hippel_module *m, int16_t *buffer, size_t frames) {
	if (!m || !buffer)
		return 0;
	m->mix.resize(frames * 2);
	const size_t n = hippel_render_float(m, m->mix.data(), frames);
	for (size_t i = 0; i < 2 * n; i++)
		buffer[i] = int16_t(std::lround(std::clamp(m->mix[i], -1.0f, 1.0f) * 32767.0f));
	return n;
}

double hippel_seek(hippel_module *m, double seconds) {
	if (!m)
		return 0;
	hippel_select_subsong(m, m->current);
	for (;;) {
		const double length = m->mixer ? m->next.seconds : m->replayer->tick_clocks() / hippel::kPaulaClock;
		const double end = m->seconds_done + length;
		if (end > seconds + 1e-9 || (m->end_frame >= 0 && end * m->settings.sample_rate > double(m->end_frame)))
			break;
		run_tick(m);
	}
	m->frames_done = (long long)(m->seconds_done * m->settings.sample_rate);
	m->tick_frames_left = 0;
	return m->seconds_done;
}

double hippel_position(const hippel_module *m) {
	return m ? double(m->frames_done) / m->settings.sample_rate : 0;
}

int hippel_set_settings(hippel_module *m, const hippel_settings *settings) {
	if (!m || !settings || !valid(settings))
		return -1;
	const int rate = m->settings.sample_rate;
	m->settings = *settings;
	m->settings.sample_rate = rate;
	apply_output(m);
	m->paula.set_led(m->settings.led_filter == HIPPEL_LED_ON);
	return 0;
}

int hippel_set_voice_mask(hippel_module *m, unsigned mask) {
	if (!m)
		return -1;
	m->paula.set_voice_mask(mask);
	return 0;
}

}  // extern "C"

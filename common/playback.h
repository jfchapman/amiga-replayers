// What SoundMonReplay and HippelReplay do around a replayer: render ticks up to a song's end, fading it out if asked to.
// (TFMXreplay finds its songs' ends as it plays them, so it has its own.)
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "paula.h"

namespace amiga {

// The checks on the settings every library has.
template <typename Settings>
bool valid_settings(const Settings *s) {
	return s->sample_rate >= 8000 && s->sample_rate <= 192000 && s->stereo_separation >= 0 &&
	       s->stereo_separation <= 100 && s->volume_ramping >= 0 && s->loop_count >= -1 && s->gain >= 0 &&
	       s->fade_seconds >= 0;
}

template <typename Settings>
void set_output(Paula &paula, const Settings &s) {
	paula.set_output(s.sample_rate, static_cast<Interpolation>(s.interpolation), s.volume_ramping, s.stereo_separation,
	                 s.gain, static_cast<Model>(s.amiga_model));
}

class Playback {
public:
	// seconds: how long to play, -1 = for ever. fade: how much of the end of that to fade out, 0 = none.
	void start(int sample_rate, double seconds, float fade) {
		tick_frames_left_ = 0;
		frames_done_ = 0;
		end_frame_ = seconds < 0 ? -1 : (long long)(seconds * sample_rate);
		fade_from_ = fade > 0 && end_frame_ >= 0 ? end_frame_ - (long long)(fade * sample_rate) : -1;
	}

	// After a seek: frames are already played, to the end of a tick.
	void skip_to(long long frames) {
		frames_done_ = frames;
		tick_frames_left_ = 0;
	}

	// Whether playing up to this frame would go past the end.
	bool past_end(double frame) const { return end_frame_ >= 0 && frame > double(end_frame_); }

	long long frames() const { return frames_done_; }

	// Renders up to 'frames' stereo frames. tick() plays a tick, and returns its length in frames.
	template <typename Tick>
	size_t render(Paula &paula, float *buffer, size_t frames, Tick tick) {
		size_t done = 0;
		while (done < frames) {
			if (end_frame_ >= 0 && frames_done_ >= end_frame_)
				break;
			if (tick_frames_left_ < 1)
				tick_frames_left_ += tick();
			size_t n = std::min<size_t>(frames - done, size_t(tick_frames_left_));
			if (end_frame_ >= 0)
				n = std::min<size_t>(n, size_t(end_frame_ - frames_done_));
			if (n == 0)
				n = 1;
			float *out = buffer + 2 * done;
			std::fill(out, out + 2 * n, 0.0f);
			paula.render(out, int(n));
			if (fade_from_ >= 0)
				fade(out, n);
			tick_frames_left_ -= double(n);
			frames_done_ += (long long)n;
			done += n;
		}
		return done;
	}

	// The same as 16-bit samples. render(buffer, frames) renders them as floats.
	template <typename Render>
	size_t render_s16(int16_t *buffer, size_t frames, Render render) {
		mix_.resize(frames * 2);
		const size_t n = render(mix_.data(), frames);
		for (size_t i = 0; i < 2 * n; i++)
			buffer[i] = int16_t(std::lround(std::clamp(mix_[i], -1.0f, 1.0f) * 32767.0f));
		return n;
	}

private:
	void fade(float *out, size_t n) const {
		const double span = double(end_frame_ - fade_from_);
		for (size_t i = 0; i < n; i++) {
			const long long f = frames_done_ + (long long)i;
			if (f > fade_from_) {
				const float g = float(std::max(0.0, 1.0 - double(f - fade_from_) / span));
				out[2 * i] *= g;
				out[2 * i + 1] *= g;
			}
		}
	}

	double tick_frames_left_ = 0;  // output frames left in the current tick
	long long frames_done_ = 0;
	long long end_frame_ = -1;  // where playback stops, -1 = never
	long long fade_from_ = -1;  // where the fade-out starts
	std::vector<float> mix_;
};

}  // namespace amiga

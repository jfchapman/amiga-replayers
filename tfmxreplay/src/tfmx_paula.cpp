// TFMXreplay: Paula emulation.
//
// Paula plays 'len' words from 'lc', one byte per 'per' colour clocks. When a block ends it
// reloads whatever lc/len hold at that moment and carries on, so register changes made while a
// voice plays take effect at the next loop point. Starting DMA latches lc/len at once. Both
// events raise the voice's audio interrupt, which the replayer counts for macro 1A.
#include "tfmx_paula.h"

#include <algorithm>
#include <cmath>

namespace tfmx {

namespace {

using amiga::kMinPeriod;

uint32_t bytes_of(uint16_t len) { return (len ? uint32_t(len) : 0x10000u) * 2; }

// The 7V mixer's fixed-point position, and the shortest block it plays.
const int kMixFraction = 11;
const uint32_t kMixMinimumBytes = 64;

}  // namespace

void Paula::reset(int channels) {
	ch_.assign(size_t(channels), Channel());
	tails_.assign(size_t(channels), Tail());
	seven_ = false;
	mix_front_.clear();
	mix_back_.clear();
	mix_old_.clear();
	mix_play_ = 0;
	output_.reset();
}

void Paula::set_output(int sample_rate, tfmx_interpolation interpolation, int ramp_us, int separation, float gain,
                       tfmx_amiga_model model, bool led_filter) {
	output_.set(sample_rate, static_cast<amiga::Model>(model), separation, gain);
	output_.set_led(led_filter);
	rate_ = sample_rate;
	interp_ = interpolation;
	// A full-scale volume change (64) takes ramp_us microseconds.
	ramp_step_ = ramp_us > 0 ? float(64.0 * 1e6 / (double(ramp_us) * sample_rate)) : 0.0f;
}

void Paula::dma_on(int n, double delay) {
	Channel &c = ch_[size_t(n)];
	if (c.dma)
		return;
	c.delay = delay;
	if (seven_ && n >= 4) {
		// The mixer's first byte is the sample's second.
		c.mix_lc = c.mix_next_lc;
		c.mix_bytes = bytes_of(c.mix_next_len);
		c.mix_pos = (1u << kMixFraction) - 1;
		c.mix_on = true;
		c.mix_held = false;
	} else if (ramp_step_ > 0 && c.ramp_vol > 0.5f) {
		// The voice is restarting while still audible: let the old sound fade out separately.
		tails_[size_t(n)].c = c;
		tails_[size_t(n)].active = true;
	}
	c.dma = true;
	c.block_lc = c.lc;
	c.block_bytes = bytes_of(c.len);
	c.pos = 0;
	c.prev = 0;
	c.block_starts++;
	c.ramp_vol = float(std::min(c.vol, 64));  // the attack is part of the sound: no ramp in
}

void Paula::dma_off(int n) {
	Channel &c = ch_[size_t(n)];
	if (!c.dma)
		return;
	if (ramp_step_ > 0 && c.ramp_vol > 0.5f && !(seven_ && n >= 4)) {
		tails_[size_t(n)].c = c;
		tails_[size_t(n)].active = true;
	}
	c.dma = false;
	c.ramp_vol = 0;
}

void Paula::next_block(Channel &c) {
	c.prev = sample_at(c, c.block_bytes - 1);
	c.block_lc = c.lc;
	c.block_bytes = bytes_of(c.len);
	c.block_starts++;
}

float Paula::sample_at(const Channel &c, uint32_t index) const {
	// index counts bytes from the start of the current block; past its end, the next block
	// (whatever lc holds now) follows.
	uint32_t addr;
	if (index < c.block_bytes)
		addr = c.block_lc + index;
	else
		addr = c.lc + (index - c.block_bytes) % bytes_of(c.len);
	if (!smpl_ || addr >= smpl_->size())
		return 0;
	return float(int8_t((*smpl_)[addr])) / 128.0f;
}

void Paula::step(Channel &c, double samples) {
	c.pos += samples;
	while (c.pos >= c.block_bytes) {
		c.pos -= c.block_bytes;
		next_block(c);
	}
}

void Paula::advance(double seconds) {
	for (Channel &c : ch_) {
		if (!c.dma)
			continue;
		double t = seconds;
		if (c.delay > 0) {
			const double d = std::min(c.delay, t);
			c.delay -= d;
			t -= d;
		}
		step(c, t * kPaulaClock / std::max<int>(c.per, kMinPeriod));
	}
	for (Tail &t : tails_)
		t.active = false;
	mix_play_ += seconds * kPaulaClock / mix_period_;
}

float Paula::voice_sample(Channel &c) {
	const uint32_t i = uint32_t(c.pos);
	const float s0 = sample_at(c, i);
	if (interp_ == TFMX_INTERPOLATION_NONE)
		return s0;
	const float t = float(c.pos - i);
	const float s1 = sample_at(c, i + 1);
	if (interp_ == TFMX_INTERPOLATION_LINEAR)
		return s0 + (s1 - s0) * t;
	return amiga::cubic(i ? sample_at(c, i - 1) : c.prev, s0, s1, sample_at(c, i + 2), t);
}

void Paula::mix_latch(int n, int period, int volume) {
	Channel &c = ch_[size_t(n)];
	c.mix_next_lc = c.lc;
	c.mix_next_len = c.len;
	c.mix_per = period ? period : c.per;
	c.mix_vol = volume;
}

void Paula::mix_block(int bytes, int period, int mixed, int silence) {
	mix_play_ = mix_front_.empty() ? 0 : mix_play_ - double(mix_front_.size());
	mix_old_.swap(mix_front_);
	mix_front_.swap(mix_back_);
	mix_period_ = period;

	std::vector<int> sum(size_t(std::max(bytes, mixed)), 0);
	for (size_t n = 4; n < ch_.size(); n++) {
		Channel &c = ch_[n];
		if (!c.mix_on || c.mix_bytes < kMixMinimumBytes)
			continue;
		// DMA off doesn't silence a voice: the mixer stops stepping, and holds the sample it was on.
		const uint32_t step =
		    c.mix_held ? 0 : uint32_t(period) * (1u << kMixFraction) / uint32_t(std::max(c.mix_per, 1));
		const int volume = heard(n) ? std::min(c.mix_vol, 63) : 0;  // a table of 64 volumes
		for (int i = 0; i < mixed; i++) {
			const uint32_t addr = c.mix_lc + (c.mix_pos >> kMixFraction);
			if (smpl_ && addr < smpl_->size())
				sum[size_t(i)] += (int(int8_t((*smpl_)[addr])) * volume) >> 6;
			c.mix_pos += step;
			if (c.mix_pos >= c.mix_bytes << kMixFraction) {
				c.mix_pos -= c.mix_bytes << kMixFraction;
				c.mix_lc = c.mix_next_lc;
				c.mix_bytes = bytes_of(c.mix_next_len);
			}
		}
	}
	// Everything else reaches the mixer a tick after the replayer writes it.
	for (size_t n = 4; n < ch_.size(); n++) {
		Channel &c = ch_[n];
		c.mix_next_lc = c.lc;
		c.mix_next_len = c.len;
		c.mix_per = c.per;
		c.mix_vol = c.mix_volume;
		c.mix_held = !c.dma;
	}
	mix_back_.assign(size_t(bytes + silence), 0);
	for (int i = 0; i < bytes; i++)
		mix_back_[size_t(i)] = int8_t(std::clamp(sum[size_t(i)], -128, 127));
}

// pos is relative to the block playing. Interpolation reads a little past either end of it.
float Paula::mix_sample(double pos) const {
	if (pos < 0)
		return mix_old_.empty() ? 0.0f : float(mix_old_.back()) / 128.0f;
	const size_t i = size_t(pos);
	if (i < mix_front_.size())
		return float(mix_front_[i]) / 128.0f;
	const size_t j = i - mix_front_.size();
	return j < mix_back_.size() ? float(mix_back_[j]) / 128.0f : 0.0f;
}

void Paula::render(float *out, int frames) {
	for (int f = 0; f < frames; f++) {
		float side[2] = {0, 0};
		for (size_t n = 0; n < ch_.size(); n++) {
			// Hardware voices 0 and 3 are on the left, 1 and 2 on the right. In 7V mode voices
			// 4-7 replace voice 3, sharing its side.
			const int s = (n == 1 || n == 2) ? 1 : 0;
			if (seven_ && n >= 3) {
				// Not heard directly. Voices 4-7 still count their blocks (macro 1A).
				Channel &c = ch_[n];
				if (n >= 4 && c.dma) {
					if (c.delay > 0)
						c.delay -= 1.0 / rate_;
					else
						step(c, kPaulaClock / std::max<int>(c.per, kMinPeriod) / rate_);
				}
				continue;
			}
			for (int pass = 0; pass < 2; pass++) {
				Channel &c = pass ? tails_[n].c : ch_[n];
				if (pass && !tails_[n].active)
					continue;
				if (!pass && !c.dma)
					continue;
				if (!pass && c.delay > 0) {
					c.delay -= 1.0 / rate_;  // DMA hasn't started yet
					continue;
				}
				float target = pass ? 0.0f : float(std::clamp(c.vol, 0, 64));
				if (ramp_step_ > 0) {
					if (c.ramp_vol < target)
						c.ramp_vol = std::min(target, c.ramp_vol + ramp_step_);
					else if (c.ramp_vol > target)
						c.ramp_vol = std::max(target, c.ramp_vol - ramp_step_);
				} else {
					c.ramp_vol = target;
				}
				if (c.ramp_vol > 0 && heard(n))
					side[s] += voice_sample(c) * c.ramp_vol / 64.0f;
				step(c, kPaulaClock / std::max<int>(c.per, kMinPeriod) / rate_);
				if (pass && c.ramp_vol <= 0)
					tails_[n].active = false;
			}
		}
		if (seven_) {
			// Hardware voice 3 at full volume, on the left.
			const double p = mix_play_;
			float x = mix_sample(p);
			if (interp_ != TFMX_INTERPOLATION_NONE) {
				const float t = float(p - std::floor(p)), s1 = mix_sample(p + 1);
				if (interp_ == TFMX_INTERPOLATION_LINEAR)
					x += (s1 - x) * t;
				else
					x = amiga::cubic(mix_sample(p - 1), x, s1, mix_sample(p + 2), t);
			}
			side[0] += x;
			mix_play_ += kPaulaClock / mix_period_ / rate_;
		}
		output_.add(side[0], side[1], out + 2 * f);
	}
}

}  // namespace tfmx

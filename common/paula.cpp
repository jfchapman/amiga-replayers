// Paula emulation.
//
// Paula plays 'len' words from 'lc', one byte per 'per' colour clocks. When a block ends it
// reloads whatever lc/len hold at that moment and carries on, so register changes made while a
// voice plays take effect at the next loop point. Starting DMA latches lc/len at once.
#include "paula.h"

#include <algorithm>

namespace amiga {

namespace {

uint32_t bytes_of(uint16_t len) { return (len ? uint32_t(len) : 0x10000u) * 2; }

}  // namespace

void Paula::reset() {
	for (int n = 0; n < 4; n++) {
		ch_[n] = Channel();
		tails_[n] = Tail();
	}
	output_.reset();
}

void Paula::set_output(int sample_rate, Interpolation interpolation, int ramp_us, int separation, float gain, Model model) {
	output_.set(sample_rate, model, separation, gain);
	rate_ = sample_rate;
	interp_ = interpolation;
	// A full-scale volume change (64) takes ramp_us microseconds.
	ramp_step_ = ramp_us > 0 ? float(64.0 * 1e6 / (double(ramp_us) * sample_rate)) : 0.0f;
}

void Paula::dma_on(int n) {
	Channel &c = ch_[n];
	if (c.dma)
		return;
	if (ramp_step_ > 0 && c.ramp_vol > 0.5f) {
		tails_[n].c = c;
		tails_[n].active = true;
	}
	c.dma = true;
	c.block_lc = c.lc;
	c.block_bytes = bytes_of(c.len);
	c.pos = 0;
	c.prev = 0;
	c.ramp_vol = float(std::min(c.vol, 64));  // the attack is part of the sound: no ramp in
}

void Paula::dma_off(int n) {
	Channel &c = ch_[n];
	if (!c.dma)
		return;
	if (ramp_step_ > 0 && c.ramp_vol > 0.5f) {
		tails_[n].c = c;
		tails_[n].active = true;
	}
	c.dma = false;
	c.ramp_vol = 0;
}

void Paula::stream(int n, uint32_t lc, uint16_t len, uint32_t next_lc, uint16_t next_len) {
	Channel &c = ch_[n];
	if (!c.dma) {
		c.lc = lc;
		c.len = len;
		dma_on(n);
	} else if (c.block_lc != lc) {
		c.prev = sample_at(c, c.block_bytes - 1);
		c.pos = std::max(0.0, c.pos - c.block_bytes);
		c.block_lc = lc;
		c.block_bytes = bytes_of(len);
	}
	c.lc = next_lc;
	c.len = next_len;
}

void Paula::next_block(Channel &c) {
	c.prev = sample_at(c, c.block_bytes - 1);
	c.block_lc = c.lc;
	c.block_bytes = bytes_of(c.len);
}

float Paula::sample_at(const Channel &c, uint32_t index) const {
	// index counts bytes from the start of the current block; past its end, the next block
	// (whatever lc holds now) follows.
	uint32_t addr;
	if (index < c.block_bytes)
		addr = c.block_lc + index;
	else
		addr = c.lc + (index - c.block_bytes) % bytes_of(c.len);
	if (!mem_ || addr >= mem_->size())
		return 0;
	return float(int8_t((*mem_)[addr])) / 128.0f;
}

void Paula::step(Channel &c, double samples) {
	c.pos += samples;
	while (c.pos >= c.block_bytes) {
		c.pos -= c.block_bytes;
		next_block(c);
	}
}

float Paula::voice_sample(const Channel &c) const {
	const uint32_t i = uint32_t(c.pos);
	const float s0 = sample_at(c, i);
	if (interp_ == Interpolation::None)
		return s0;
	const float t = float(c.pos - i);
	const float s1 = sample_at(c, i + 1);
	if (interp_ == Interpolation::Linear)
		return s0 + (s1 - s0) * t;
	return cubic(i ? sample_at(c, i - 1) : c.prev, s0, s1, sample_at(c, i + 2), t);
}

void Paula::render(float *out, int frames) {
	for (int f = 0; f < frames; f++) {
		float side[2] = {0, 0};
		for (int n = 0; n < 4; n++) {
			// Voices 0 and 3 are on the left, 1 and 2 on the right.
			const int s = (n == 1 || n == 2) ? 1 : 0;
			for (int pass = 0; pass < 2; pass++) {
				Channel &c = pass ? tails_[n].c : ch_[n];
				if (pass && !tails_[n].active)
					continue;
				if (!pass && !c.dma)
					continue;
				// Paula uses volume bits 0-6, and plays 64 and above at full volume.
				const float target = pass ? 0.0f : float(std::min(c.vol & 0x7F, 64));
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
		output_.add(side[0], side[1], out + 2 * f);
	}
}

}  // namespace amiga

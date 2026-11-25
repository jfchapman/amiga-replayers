// The Amiga's sound chip, Paula, as far as SoundMon and Hippel songs use it. (TFMX has its own: it also needs
// delayed starts, audio interrupts and its software mixer, all the way through.)
#pragma once

#include <cstdint>
#include <vector>

#include "output.h"

namespace amiga {

struct Channel {
	// Registers, as the replayer writes them. lc is a byte offset into the memory.
	uint32_t lc = 0;
	uint16_t len = 1;
	uint16_t per = 428;
	int vol = 0;
	bool dma = false;

	// Playback: the block Paula latched, and the position in it (in samples, i.e. bytes).
	uint32_t block_lc = 0;
	uint32_t block_bytes = 2;
	double pos = 0;

	// The last sample of the previous block (for interpolation across a loop point).
	float prev = 0;

	// Anti-click ramp: the volume actually applied, moving toward vol.
	float ramp_vol = 0;
};

class Paula {
public:
	void reset();
	void set_memory(const std::vector<uint8_t> *memory) { mem_ = memory; }
	void set_output(int sample_rate, Interpolation interpolation, int ramp_us, int separation, float gain, Model model);
	void set_led(bool on) { output_.set_led(on); }

	Channel &channel(int n) { return ch_[n]; }

	void dma_on(int n);
	void dma_off(int n);

	// Goes over to the block at lc at once, if it hasn't got there by itself, with next_lc to follow.
	// For Hippel's 7-voice mix, whose blocks are exactly one tick long.
	void stream(int n, uint32_t lc, uint16_t len, uint32_t next_lc, uint16_t next_len);

	// Mixes 'frames' stereo frames, added into out (left, right interleaved).
	void render(float *out, int frames);

	// Which voices are heard: bit n for channel n. Muted voices still play.
	void set_voice_mask(unsigned mask) { mask_ = mask; }

private:
	float sample_at(const Channel &c, uint32_t index) const;
	void next_block(Channel &c);
	void step(Channel &c, double samples);
	float voice_sample(const Channel &c) const;

	Channel ch_[4];
	struct Tail {
		Channel c;
		bool active = false;
	};
	Tail tails_[4];  // cut voices fading out (anti-click)
	const std::vector<uint8_t> *mem_ = nullptr;
	int rate_ = 44100;
	Interpolation interp_ = Interpolation::None;
	float ramp_step_ = 0;  // volume units per output frame, 0 = no ramping
	unsigned mask_ = 0xF;
	bool heard(int n) const { return (mask_ >> n) & 1; }
	Output output_;
};

}  // namespace amiga

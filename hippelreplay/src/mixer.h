// HippelReplay: the 7-voice replayer's mixer. Voices 4-7 are mixed into one buffer per tick, which
// Paula's channel 4 plays.
#pragma once

#include <cstdint>

#include "replayer.h"

namespace hippel {

class Mixer {
public:
	explicit Mixer(const Module &m) : m_(m) {}
	void reset();
	// Mixes a tick of 'samples' from the states of voices 4-7, into a buffer of 'bytes' to be played:
	// one can be longer than the other.
	void tick(const VoiceOut out[4], int samples, int bytes, int8_t *buffer);

private:
	struct Voice {
		bool playing = false;
		uint32_t at = 0;      // file offset
		uint32_t length = 0;  // bytes
		uint64_t pos = 0;     // in 1/2048 bytes
	};
	const Module &m_;
	Voice v_[4];
};

}  // namespace hippel

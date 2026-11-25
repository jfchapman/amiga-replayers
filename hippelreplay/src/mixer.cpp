#include "mixer.h"

#include <algorithm>

namespace hippel {

namespace {
const int kFrac = 11;
const int kMaxBytes = 1024;
}  // namespace

void Mixer::reset() {
	for (Voice &v : v_)
		v = Voice();
}

void Mixer::tick(const VoiceOut out[4], int samples, int bytes, int8_t *buffer) {
	int sum[kMaxBytes] = {};
	samples = std::min(samples, kMaxBytes);
	bytes = std::min(bytes, kMaxBytes);
	for (int k = 0; k < 4; k++) {
		const VoiceOut &o = out[k];
		Voice &v = v_[k];
		if (!o.active || !o.dma) {
			v.playing = false;
			continue;
		}
		if (o.trigger) {
			v.playing = o.start.words != 0;
			v.at = o.start.at;
			v.length = 2 * uint32_t(o.start.words);
			v.pos = (1 << kFrac) - 1;
		}
		if (!v.playing)
			continue;
		const uint32_t step = o.per ? uint32_t(m_.mix_period) * 2048 / o.per : 0;
		const int vol = std::clamp(o.vol, 0, 63);
		for (int i = 0; i < samples; i++) {
			sum[i] += int8_t(m_.byte(v.at + uint32_t(v.pos >> kFrac))) * vol / 63;
			v.pos += step;
			if ((v.pos >> kFrac) < v.length)
				continue;
			// The loop is taken as soon as the end is reached; a loop of 1 word is silence
			if (o.loop.words <= 1) {
				v.playing = false;
				break;
			}
			v.pos -= uint64_t(v.length) << kFrac;
			v.at = o.loop.at;
			v.length = 2 * uint32_t(o.loop.words);
			v.pos %= uint64_t(v.length) << kFrac;
		}
	}
	for (int i = 0; i < bytes; i++)
		buffer[i] = int8_t(std::clamp(sum[std::min(i, std::max(samples - 1, 0))], -128, 127));
}

}  // namespace hippel

// HippelReplay: the player, one tick (50 Hz, or a 7-voice song's own rate) at a time.
#pragma once

#include "module.h"

namespace hippel {

struct Block {  // what Paula's location and length registers point at
	uint32_t at = 0;  // file offset
	uint16_t words = 1;
};

// A voice's state after a tick. Nothing is written for a voice before its first note.
struct VoiceOut {
	bool active = false;
	uint16_t per = 0;
	int vol = 0;
	bool dma = false;
	bool trigger = false;  // a sample starts on this tick, at 'start'
	Block start;
	Block loop;  // what Paula plays next
};

class Replayer {
public:
	explicit Replayer(const Module &m) : m_(m) {}
	void start(const Song &song);
	void tick(VoiceOut out[7]);  // the first 'voices' of them are written

	// 7 voices: the length of the next tick's mix buffer, in words
	int mix_words() const { return 10 * m_.mix_khz * (256 + tempo_) / 256; }
	// and how many samples are mixed into it: half a word's worth can be left over, and is skipped
	int mix_samples() const { return 20 * m_.mix_khz * (256 + tempo_) / 256; }
	// How long the next tick lasts, in Paula clocks
	int tick_clocks() const { return m_.voices == 7 ? 2 * mix_words() * m_.mix_period : 313 * 227; }

	struct RowEvent {
		bool read = false;
		int pos = 0, row = 0;
		int speed = 0;  // ticks this row lasts
	};
	const RowEvent &last_row() const { return row_event_; }
	bool stopped() const { return stopped_; }

private:
	struct Voice {
		int note = 0;
		int8_t transpose = 0;
		uint8_t sound = 0;
		int level = 0;
		int vary = 0;  // EA: this tick's random drop in level
		uint32_t vol_seq = 0, freq_seq = 0;
		bool has_freq = false;
		int fseq_no = 0, fptr = 0, fwait = 0, vptr = 5, vwait = 0, vspeed = 1;
		uint8_t pitch = 0;
		int vol = 0;
		int vib_speed = 0, vib_depth = 0, vib_delay = 0, vib_p = 0;
		bool vib_up = false, vib_phase = false;
		int slide = 0, slide_p = 0;
		Block start, loop, pending;
		bool trigger = false, has_pending = false, dma = false, started = false;
		int e7 = -1;
		bool scan = false, scan_hold = false;
		int scan_sample = 0, scan_at = 0, scan_len = 0, scan_step = 0, scan_delay = 0, scan_wait = 0;
		uint16_t per = 0;
	};

	void do_row();
	void next_pos();
	void do_voice(Voice &v);
	void start_note(Voice &v, int note, int inst, bool keep);
	void start_sample(Voice &v, int n);
	void freq_sequence(Voice &v);
	void vol_sequence(Voice &v);
	int period(int idx) const;
	Block block(int n, uint32_t bytes, int words) const;
	const Sample &sample(int n) const;

	const Module &m_;
	Song song_;
	Voice v_[7];
	int tempo_ = 0;
	uint32_t random_ = 1;
	int pos_ = 0, row_ = 0, count_ = 0;
	bool stop_next_ = false, stopped_ = false;
	RowEvent row_event_;
};

}  // namespace hippel

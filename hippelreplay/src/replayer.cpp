#include "replayer.h"

#include <algorithm>

namespace hippel {

namespace {

const uint16_t kPeriods[72] = {
	0x6B0, 0x650, 0x5F4, 0x5A0, 0x54C, 0x500, 0x4B8, 0x474, 0x434, 0x3F8, 0x3C0, 0x38A,
	0x358, 0x328, 0x2FA, 0x2D0, 0x2A6, 0x280, 0x25C, 0x23A, 0x21A, 0x1FC, 0x1E0, 0x1C5,
	0x1AC, 0x194, 0x17D, 0x168, 0x153, 0x140, 0x12E, 0x11D, 0x10D, 0xFE, 0xF0, 0xE2,
	0xD6, 0xCA, 0xBE, 0xB4, 0xAA, 0xA0, 0x97, 0x8F, 0x87, 0x7F, 0x78, 0x71,
	0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71, 0x71,
	0xD60, 0xCA0, 0xBE8, 0xB40, 0xA98, 0xA00, 0x970, 0x8E8, 0x868, 0x7F0, 0x780, 0x714};

}  // namespace

void Replayer::start(const Song &song) {
	song_ = song;
	pos_ = song.first;
	row_ = 0;
	// Newer replayers play the first row on the first tick, older ones 'speed' ticks in
	count_ = m_.rules.newer ? 1 : song.speed;
	stop_next_ = stopped_ = false;
	row_event_ = RowEvent();
	tempo_ = m_.tempo;
	random_ = 1;
	for (Voice &v : v_) {
		v = Voice();
		if (m_.rules.coso) {  // a first note that keeps the sound plays sequence 0
			v.freq_seq = m_.freq_seq(0);
			v.has_freq = true;
		}
	}
}

int Replayer::period(int idx) const {
	const int n = m_.rules.table_len;
	if (idx < n)
		return kPeriods[idx];
	if (m_.rules.low_octave_twice && idx < 84)
		return kPeriods[idx - 12];
	// Past the table the replayer reads on: three words (0, 1, 1), an address, then the song
	const int k = idx - n;
	if (k < 4)
		return k == 1 || k == 2 ? 1 : 0;
	return k - 4 < 18 ? m_.word(m_.tag + 2 * uint32_t(k - 4)) : 0;
}

const Sample &Replayer::sample(int n) const {
	static const Sample none;
	return n < int(m_.samples.size()) ? m_.samples[size_t(n)] : none;
}

Block Replayer::block(int n, uint32_t bytes, int words) const {
	Block b;
	if (n < int(m_.samples.size())) {
		b.at = m_.sample_base + sample(n).offset + bytes;
		b.words = uint16_t(words);
	} else {  // no such sample: silence
		b.at = uint32_t(m_.file.size()) + 0x10000;
		b.words = 1;
	}
	return b;
}

void Replayer::start_sample(Voice &v, int n) {
	const Sample &s = sample(n);
	v.e7 = -1;
	v.start = block(n, 0, s.words);
	v.trigger = true;
	v.dma = true;
	v.scan = false;
	const Block loop = block(n, s.repeat, s.repeat_words);
	if (m_.rules.loop_at_trigger) {
		v.loop = loop;
	} else {
		v.loop = v.start;
		v.pending = loop;
		v.has_pending = true;
	}
	v.vptr = 5;
	v.vwait = 0;
}

void Replayer::start_note(Voice &v, int note, int inst, bool keep) {
	v.vol_seq = m_.vol_seq(inst);
	v.vspeed = m_.byte(v.vol_seq);
	const uint8_t fseq = m_.byte(v.vol_seq + 1);
	// Bit 7 keeps the sound: the frequency sequence, and so the sample, carries on
	const bool hold = (fseq & 0x80) && v.has_freq;
	if (!hold) {
		if (!keep)
			v.fseq_no = fseq;
		v.freq_seq = m_.freq_seq(v.fseq_no);
		v.has_freq = true;
		v.fptr = 0;
		v.fwait = 0;
	}
	v.vib_speed = m_.byte(v.vol_seq + 2);
	v.vib_depth = m_.byte(v.vol_seq + 3);
	v.vib_delay = m_.byte(v.vol_seq + 4);
	v.vib_p = v.vib_depth;
	v.vib_up = false;
	v.vib_phase = false;
	v.vptr = 5;
	// After a note that keeps the sound, the first volume step comes 'speed' ticks in
	v.vwait = hold ? (v.vspeed ? v.vspeed : 256) - 1 : 0;
	v.note = note;
	v.started = true;
}

void Replayer::next_pos() {
	row_ = 0;
	pos_++;
	if (pos_ == song_.last + 1)
		pos_ = song_.first;
}

void Replayer::do_row() {
	if (m_.row(m_.track(pos_, 0).pattern, row_).note == 1)
		next_pos();  // a break: the next track row starts on this row
	row_event_.read = true;
	row_event_.pos = pos_;
	row_event_.row = row_;
	row_event_.speed = count_;
	for (int n = 0; n < m_.voices; n++) {
		Voice &v = v_[n];
		const TrackEntry t = m_.track(pos_, n);
		const Row e = m_.row(t.pattern, row_);
		v.transpose = int8_t(t.transpose);  // applies at once, also to a note already playing
		if (m_.voices == 7) {
			if ((t.level & 0xF0) == 0xF0)
				v.level = t.level & 0x0F;
			else if ((t.level & 0xF0) == 0xD0)  // a tempo: the value is the next entry's level
				tempo_ = int8_t(m_.track(pos_, n + 1).level);
		}
		if (m_.rules.coso && m_.voices == 4 && t.sound > 0x80) {
			// $F0-$FF: a volume level, kept until the next one. $E1-$EF: the speed, from the next row
			// on. The sound transpose stays.
			if (t.sound >= 0xF0)
				v.level = t.sound & 0x0F;
			else if (t.sound > 0xE0)
				song_.speed = t.sound & 0x0F;
		} else {
			v.sound = t.sound;
			if (t.sound == 0x80)
				stop_next_ = true;  // the song ends on this row's second tick
		}
		if (e.note <= 1)
			continue;
		if (e.note & 0x80) {
			v.note = e.note & 0x7F;  // legato
		} else {
			const bool keep = (e.info & 0x40) && m_.rules.newer;
			start_note(v, e.note, uint8_t((e.info & 0x1F) + v.sound), keep);
			if (keep) {
				v.fseq_no = e.param;
				v.freq_seq = m_.freq_seq(v.fseq_no);
			}
		}
		v.slide = 0;
		v.slide_p = e.info & 0x20 ? int8_t(e.param) : 0;
	}
	if (++row_ >= m_.rows)
		next_pos();
}

void Replayer::freq_sequence(Voice &v) {
	if (v.fwait > 0) {
		v.fwait--;
		return;
	}
	for (int guard = 0; guard < 64; guard++) {
		const uint32_t q = v.freq_seq + uint32_t(v.fptr);
		const uint8_t b = m_.byte(q), a = m_.byte(q + 1);
		if (b == 0xE0) {
			v.fptr = a;
		} else if (b == 0xE1) {
			return;
		} else if (b == 0xE2) {
			start_sample(v, a);
			v.fptr += 2;
		} else if (b == 0xE3) {
			v.vib_speed = a;
			v.vib_depth = m_.byte(q + 2);
			v.fptr += 3;
		} else if (b == 0xE4) {
			v.loop = block(a, sample(a).repeat, sample(a).repeat_words);
			v.fptr += 2;
		} else if (b == 0xE7 && !m_.rules.newer) {  // go to another sequence
			v.fseq_no = a;
			v.freq_seq = m_.freq_seq(a);
			v.fptr = 0;
		} else if (b == 0xE7) {  // start, unless it's the sample already playing
			if (v.e7 != a) {
				start_sample(v, a);
				v.e7 = a;
			} else {
				v.scan = false;
			}
			v.fptr += 2;
		} else if (b == 0xE8) {
			v.fwait = a - 1;
			v.fptr += 2;
			return;
		} else if (b == 0xE5 && m_.rules.scans) {  // scan through a sample
			const int e7 = v.e7;
			start_sample(v, a);
			v.e7 = m_.rules.coso ? e7 : a;  // the later replayers leave E7's memory alone
			v.scan = true;
			v.scan_sample = a;
			v.scan_at = m_.word(q + 2);
			v.scan_len = m_.word(q + 4);
			v.scan_step = int16_t(m_.word(q + 6));
			v.scan_delay = v.scan_wait = m_.byte(q + 8);
			v.start = v.loop = block(a, 2 * uint32_t(v.scan_at), v.scan_len);
			v.has_pending = false;
			v.fptr += 9;
		} else if (b == 0xE6 && m_.rules.scans) {  // change the scan
			if (v.scan) {
				v.scan_len = m_.word(q + 1);
				v.scan_step = int16_t(m_.word(q + 3));
				v.scan_delay = v.scan_wait = m_.byte(q + 5);
				v.scan_hold = true;
			}
			v.fptr += 6;
		} else if (b == 0xEA && m_.voices == 7) {  // a random drop in this tick's volume
			random_ = random_ * 1103515245 + 12345;
			v.vary = a * int((random_ >> 16) % 167) >> 6;
			v.fptr += 2;
		} else if (b == 0xE9 && m_.rules.newer) {  // one part of a sample holding several
			const uint32_t body = m_.sample_base + sample(a).offset;
			const uint32_t part = body + 8 + 24 * uint32_t(m_.byte(q + 2));
			const uint32_t from = m_.lword(part), to = m_.lword(part + 4);
			start_sample(v, a);
			const uint32_t at = 8 + 24 * uint32_t(m_.word(body + 4)) + from;
			v.start = block(a, at, int((to - from) / 2));
			v.loop = block(a, at, 1);
			v.has_pending = false;
			v.fptr += 3;
		} else {
			v.pitch = b;
			v.fptr++;
			return;
		}
	}
}

void Replayer::vol_sequence(Voice &v) {
	if (v.vwait > 0) {
		v.vwait--;
		return;
	}
	const int speed = v.vspeed ? v.vspeed : 256;
	for (int guard = 0; guard < 64; guard++) {
		const uint8_t b = m_.byte(v.vol_seq + uint32_t(v.vptr)), a = m_.byte(v.vol_seq + uint32_t(v.vptr) + 1);
		if (b == 0xE0) {
			v.vptr = a;
		} else if (b == 0xE1) {
			return;
		} else if (b == 0xE8) {  // hold
			v.vwait = a + speed - 2;
			v.vptr += 2;
			return;
		} else {
			v.vol = b;
			v.vptr++;
			v.vwait = speed - 1;
			return;
		}
	}
}

void Replayer::do_voice(Voice &v) {
	const Rules &rules = m_.rules;
	freq_sequence(v);
	if (v.scan_hold) {
		v.scan_hold = false;
	} else if (v.scan && !v.trigger) {
		if (v.scan_wait > 0) {
			v.scan_wait--;
		} else {
			v.scan_wait = v.scan_delay;
			const int next = v.scan_at + v.scan_step;
			if (next < 0 || next + v.scan_len > sample(v.scan_sample).words)
				v.scan = false;
			else
				v.scan_at = next;
			v.loop = block(v.scan_sample, 2 * uint32_t(v.scan_at), v.scan_len);
		}
	}
	vol_sequence(v);

	const int idx = (v.pitch & 0x80 ? v.pitch : v.note + v.transpose + v.pitch) & 0x7F;
	int per = period(idx);

	const bool half = rules.vib2708 && (v.vib_speed & 0x80);
	const int speed = half ? v.vib_speed & 0x7F : v.vib_speed;
	const int depth = v.vib_depth;
	const bool delayed = v.vib_delay > 0;
	if (delayed) {
		v.vib_delay--;
	} else {
		if (half)
			v.vib_phase = !v.vib_phase;
		if (half && v.vib_phase) {
			// speed bit 7: every other tick only
		} else if (v.vib_up) {
			const int top = rules.coso ? depth : 2 * depth;
			v.vib_p += speed;
			if (v.vib_p > top || (v.vib_p == top && !rules.vib2708 && !rules.coso)) {
				v.vib_p = top;
				v.vib_up = false;
			}
		} else {
			v.vib_p -= speed;
			if (v.vib_p < 0) {
				v.vib_p = 0;
				v.vib_up = true;
			}
		}
	}

	v.slide++;
	if (rules.coso) {
		// Both in proportion to the period: the vibrato rounded down, the slide toward zero
		if (!delayed && depth)
			per += ((v.vib_p - depth / 2) * per) >> 10;
		per += int(int64_t(-v.slide_p) * v.slide * per / 1024);
	} else {
		if (!delayed)
			per += (v.vib_p - depth) * (rules.vib2708 ? 1 : 16 >> std::min(idx / 12, 4));
		per += -v.slide_p * v.slide / 32;
	}
	v.per = uint16_t(per);
}

void Replayer::tick(VoiceOut out[7]) {
	row_event_ = RowEvent();
	for (Voice &v : v_) {
		v.vary = 0;
		v.trigger = false;
		if (v.has_pending && v.dma) {
			v.loop = v.pending;
			v.has_pending = false;
		}
	}
	if (stop_next_)
		stopped_ = true;
	if (stopped_) {
		for (Voice &v : v_) {
			v.vol = 0;
			v.dma = false;
		}
	} else {
		if (--count_ <= 0 && song_.speed) {
			count_ = song_.speed;
			do_row();
		}
		for (Voice &v : v_)
			if (v.started)
				do_voice(v);
	}
	for (int n = 0; n < m_.voices; n++) {
		const Voice &v = v_[n];
		VoiceOut &o = out[n];
		o.active = v.started;
		o.per = v.per;
		o.vol = v.level ? v.vol * (16 - v.level) * 3 / 50 : v.vol;
		if (v.vary)
			o.vol = v.vol * std::max(0, (v.level ? 246 * (16 - v.level) / 16 : 256) - v.vary) >> 8;
		o.dma = v.dma;
		o.trigger = v.trigger;
		o.start = v.start;
		o.loop = v.loop;
	}
}

}  // namespace hippel

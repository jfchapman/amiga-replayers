#include "module.h"

#include <algorithm>

#include "hippelreplay.h"

namespace hippel {

namespace {

const uint32_t kHeader = 32;
const uint32_t kCodeLimit = 0x10000;  // how far into a file a song may start: the replayers are at most 33 KB

bool tag_at(const std::vector<uint8_t> &d, uint32_t at, const char *tag) {
	return at + 4 <= d.size() && std::equal(tag, tag + 4, d.begin() + at);
}

// The header both forms share. Chris Huelsbeck's TFMX files also start with "TFMX" but don't have it.
bool header_ok(const std::vector<uint8_t> &d, uint32_t at) {
	if (at + kHeader > d.size() || !tag_at(d, at, "TFMX"))
		return false;
	for (uint32_t i = 20; i < kHeader; i++)
		if (d[at + i])
			return false;
	const int c4 = d[at + 12] << 8 | d[at + 13];
	return c4 >= 2 && c4 <= 256 && !(c4 & 1);
}

// The first song in the data: its offset, and whether it's a COSO.
bool find_song(const std::vector<uint8_t> &d, uint32_t &at, bool &coso) {
	for (uint32_t i = 0; i + kHeader <= d.size() && i < kCodeLimit; i += 2) {
		if (tag_at(d, i, "COSO") && header_ok(d, i + 32)) {
			at = i;
			coso = true;
			return true;
		}
		if (header_ok(d, i)) {
			at = i;
			coso = false;
			return true;
		}
	}
	return false;
}

}  // namespace

LoadResult Module::probe(const uint8_t *data, size_t size) {
	if (!data)
		return LoadResult::NotHippel;
	const std::vector<uint8_t> d(data, data + std::min<size_t>(size, kCodeLimit + 64));
	uint32_t at;
	bool is_coso;
	return find_song(d, at, is_coso) ? LoadResult::Ok : LoadResult::NotHippel;
}

LoadResult Module::load(const uint8_t *data, size_t size, int replayer) {
	if (size > 0x4000000)
		return LoadResult::NotHippel;
	file.assign(data, data + size);
	uint32_t at;
	bool is_coso;
	if (!find_song(file, at, is_coso))
		return LoadResult::NotHippel;
	tag = at;
	const LoadResult r = is_coso ? load_coso(at) : load_hip();
	if (r == LoadResult::Ok)
		pick_rules(replayer);
	return r;
}

LoadResult Module::load_hip() {
	for (int i = 0; i < 8; i++)
		c_[i] = word(tag + 4 + 2 * i);
	freq_at_ = tag + kHeader;
	vol_at_ = freq_at_ + 64 * (c_[0] + 1);
	pattern_at_ = vol_at_ + 64 * (c_[1] + 1);
	track_at_ = pattern_at_ + c_[4] * (c_[2] + 1);
	rows = int(c_[4] / 2);
	track_rows = int(c_[3] + 1);
	if (c_[7] == 0)
		return LoadResult::Unsupported;
	// The header doesn't say how many voices there are. 7 make the track rows and the subsong
	// entries longer, so the sample headers only make sense where that puts them.
	uint32_t songs_at = track_at_ + 12 * (c_[3] + 1);
	if (!load_samples(songs_at + 6 * (c_[6] + 1))) {
		voices = 7;
		songs_at = track_at_ + 28 * (c_[3] + 1);
		if (!load_samples(songs_at + 8 * (c_[6] + 1)))
			return LoadResult::Unsupported;
		read_header_row();
	}
	const uint32_t entry = voices == 7 ? 8 : 6;
	for (uint32_t i = 0; i < c_[6]; i++)
		songs.push_back({word(songs_at + entry * i), word(songs_at + entry * i + 2), word(songs_at + entry * i + entry - 2)});
	return LoadResult::Ok;
}

// 7 voices: the first track row isn't played; it starts with the mixing rate and tempo.
void Module::read_header_row() {
	mix_khz = byte(track_at_ + 1);
	tempo = int8_t(byte(track_at_ + 2));
	// Amberstar's replayer always mixes at 18 kHz, whatever the song says. Its songs are told by the
	// text in this row ("JOCHEN HIPPEL 1991 JELAN", "JES 1992 JOCHEN").
	const bool amberstar = tag_at(file, track_at_ + 4, "JOCH") || tag_at(file, track_at_ + 5, "JES");
	if (mix_khz == 0 || amberstar)
		mix_khz = 18;
	if (mix_khz > 23)
		mix_khz = 23;
	mix_period = (3579545 + 500 * mix_khz) / (1000 * mix_khz);
}

bool Module::load_samples(uint32_t samples_at) {
	samples.clear();
	sample_base = samples_at + 30 * c_[7];
	if (sample_base > file.size())
		return false;
	for (uint32_t i = 0; i < c_[7]; i++) {
		const uint32_t a = samples_at + 30 * i;
		Sample s;
		for (uint32_t k = 0; k < 18 && byte(a + k); k++)
			s.name += char(byte(a + k));
		s.offset = lword(a + 18);
		s.words = word(a + 22);
		s.repeat = word(a + 26);
		s.repeat_words = word(a + 28);
		samples.push_back(s);
	}
	return samples_fit();
}

LoadResult Module::load_coso(uint32_t at) {
	coso = true;
	base_ = at;
	tag = at + 32;
	uint32_t off[7];
	for (int i = 0; i < 7; i++) {
		const uint32_t o = lword(at + 4 + 4 * i);
		if (o > file.size() - at || (i && i < 6 && at + o < off[i - 1]))
			return LoadResult::NotHippel;
		off[i] = at + o;
	}
	for (int i = 0; i < 8; i++)
		c_[i] = word(tag + 4 + 2 * i);
	if (off[6] < off[5])
		return LoadResult::Unsupported;  // the sample data isn't in the file
	freq_at_ = off[0];
	vol_at_ = off[1];
	track_at_ = off[3];
	sample_base = off[6];
	rows = int(c_[4] / 2);
	track_rows = int(c_[3] + 1);
	if (off[4] - off[3] >= 28 * (c_[3] + 1))
		voices = 7;
	const uint32_t entry = voices == 7 ? 8 : 6;
	if (off[4] - off[3] < 12 * (c_[3] + 1) || off[5] - off[4] < entry * c_[6])
		return LoadResult::NotHippel;
	if (voices == 7)
		read_header_row();

	// Patterns: events, each followed by a gap of empty rows
	patterns_.assign(size_t(c_[2] + 1) * rows, Row());
	for (uint32_t p = 0; p <= c_[2]; p++) {
		Row *out = &patterns_[size_t(p) * rows];
		uint32_t i = base_ + word(off[2] + 2 * p);
		int r = 0, gap = 0;
		while (i < file.size() && r < 0x10000) {
			const uint8_t b = byte(i);
			if (b == 0xFF) {
				if (r < rows && !out[r].note && !out[r].info)
					out[r].note = 1;  // ends early: the .hip form's break
				break;
			}
			if (b == 0xFE || b == 0xFD) {
				gap = byte(i + 1);
				if (b == 0xFD)
					r += 1 + gap;
				i += 2;
				continue;
			}
			Row e;
			e.note = b;
			e.info = byte(i + 1);
			i += 2;
			if (e.info & 0x60)
				e.param = byte(i++);
			if (r < rows)
				out[r] = e;
			r += 1 + gap;
		}
	}
	for (uint32_t i = 0; i < c_[6]; i++)
		songs.push_back({word(off[4] + entry * i), word(off[4] + entry * i + 2), word(off[4] + entry * i + entry - 2)});
	headers_end_ = off[5] + 10 * c_[7];
	for (uint32_t i = 0; i < c_[7]; i++) {
		const uint32_t a = off[5] + 10 * i;
		Sample s;
		s.offset = lword(a);
		s.words = word(a + 4);
		s.repeat = word(a + 6);
		s.repeat_words = word(a + 8);
		samples.push_back(s);
	}
	return samples_fit() ? LoadResult::Ok : LoadResult::Unsupported;
}

bool Module::samples_fit() const {
	size_t fit = 0;
	for (const Sample &s : samples)
		if (uint64_t(s.offset) + 2 * uint64_t(s.words) <= file.size() - sample_base + 4096)
			fit++;
	return fit * 2 > samples.size();
}

// COSO files don't say which of three replayer generations they were written for (docs/Hippel-format.md, section 10):
//   - the later ones (COSO) scale vibrato by the period, so their depths are around 20 where the
//     others' are 1-6, and only they keep sounds, have volume levels and (in COSO songs) scans
//   - the first one (OLDER: Astaroth) is told by its E3 and E8 commands, or by a second spare
//     sample header
//   - the rest play by the rules of the Wings of Death build
int Module::guess_coso_replayer() const {
	std::vector<int> depths;
	for (uint32_t n = 0; n <= c_[1]; n++) {
		const uint32_t q = vol_seq(int(n));
		if (byte(q + 1) & 0x80)
			return HIPPEL_REPLAYER_COSO;
		if (byte(q + 2) && byte(q + 3))
			depths.push_back(byte(q + 3));
	}
	std::sort(depths.begin(), depths.end());
	if (!depths.empty() && (depths[depths.size() / 2] >= 6 || depths.back() >= 16))
		return HIPPEL_REPLAYER_COSO;
	for (int p = 0; p < track_rows; p++)
		for (int v = 0; v < voices; v++)
			if (track(p, v).sound > 0x80)
				return HIPPEL_REPLAYER_COSO;
	bool first = sample_base >= headers_end_ + 20;
	static const uint8_t args[10] = {1, 0, 1, 2, 1, 8, 5, 1, 1, 2};
	for (uint32_t n = 0; n <= c_[0]; n++) {
		uint32_t i = freq_seq(int(n));
		for (int k = 0; k < 64 && i < vol_at_; k++) {
			const uint8_t b = byte(i);
			if (b == 0xE5 || b == 0xE6 || b == 0xE9)
				return HIPPEL_REPLAYER_COSO;
			if (b == 0xE3 || b == 0xE8)
				first = true;
			if (b == 0xE0 || b == 0xE1)
				break;
			i += 1 + (b >= 0xE0 && b <= 0xE9 ? args[b - 0xE0] : 0);
		}
	}
	return first ? HIPPEL_REPLAYER_OLDER : HIPPEL_REPLAYER_2708;
}

void Module::pick_rules(int replayer) {
	if (replayer == HIPPEL_REPLAYER_AUTO) {
		if (coso) {
			replayer = guess_coso_replayer();
		} else {
			switch (tag) {
			case 2708:
				replayer = HIPPEL_REPLAYER_2708;
				break;
			case 2446: case 2402: case 2320: case 2366: case 2636: case 2640: case 2470: case 2262: case 2278:
				replayer = HIPPEL_REPLAYER_OLDER;
				break;
			default:
				replayer = HIPPEL_REPLAYER_NEWER;
				break;
			}
		}
	}
	if (voices == 7)
		replayer = HIPPEL_REPLAYER_COSO;  // the 7-voice replayers play by these rules
	Rules r;
	switch (replayer) {
	case HIPPEL_REPLAYER_OLDER:
		build = "older";
		r.newer = false;
		r.loop_at_trigger = !coso && tag == 2262;
		break;
	case HIPPEL_REPLAYER_2708:
		build = "2708";
		r.scans = r.vib2708 = true;
		r.table_len = 69;
		break;
	case HIPPEL_REPLAYER_COSO:
		build = voices == 7 ? "7V" : "COSO";
		r.scans = r.coso = true;
		break;
	default:
		build = "newer";
		break;
	}
	if (!coso && voices == 4) {
		if (tag == 2402 || tag == 2320 || tag == 2366)
			r.table_len = 54;
		r.low_octave_twice = tag == 2470;
	}
	rules = r;
}

uint32_t Module::freq_seq(int n) const { return coso ? base_ + word(freq_at_ + 2 * uint32_t(n)) : freq_at_ + 64 * uint32_t(n); }
uint32_t Module::vol_seq(int n) const { return coso ? base_ + word(vol_at_ + 2 * uint32_t(n)) : vol_at_ + 64 * uint32_t(n); }

Row Module::row(int pattern, int r) const {
	if (coso) {
		const size_t i = size_t(pattern) * size_t(rows) + size_t(r);
		return i < patterns_.size() ? patterns_[i] : Row();
	}
	// The parameter is the info byte of the row before (the last row for row 0)
	const uint32_t p = pattern_at_ + uint32_t(pattern) * 2 * uint32_t(rows);
	Row e;
	e.note = byte(p + 2 * uint32_t(r));
	e.info = byte(p + 2 * uint32_t(r) + 1);
	e.param = byte(p + 2 * uint32_t((r + rows - 1) % rows) + 1);
	return e;
}

TrackEntry Module::track(int pos, int voice) const {
	const uint32_t size = voices == 7 ? 4 : 3;
	const uint32_t a = track_at_ + size * (uint32_t(voices) * uint32_t(pos) + uint32_t(voice));
	TrackEntry t;
	if (voices == 7)
		t.level = byte(a + 3);
	t.pattern = byte(a);
	t.transpose = byte(a + 1);
	t.sound = byte(a + 2);
	return t;
}

}  // namespace hippel

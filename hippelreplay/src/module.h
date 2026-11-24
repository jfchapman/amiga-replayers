// HippelReplay: a Jochen Hippel song file, in either of its Amiga forms, with 4 or 7 voices.
//
// .hip: usually the replayer's code, then the song from its "TFMX" tag (sequences of 64 bytes,
// patterns of 32 (note, info) rows). COSO: the same song with trimmed sequences and patterns
// stored as events. Both load into the same structures.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace hippel {

// The rules that differ between replayer builds. .hip files carry their replayer, whose code
// length tells the builds apart; COSO files don't, so the song's own features decide.
struct Rules {
	bool newer = true;            // E7 starts a sample (older builds: goto); $40 notes; E9
	bool scans = false;           // E5, E6
	bool vib2708 = false;         // vibrato: unit 1, strict top, speed bit 7 = half rate
	bool coso = false;            // the COSO game replayers' vibrato, slide, levels, kept sounds
	bool loop_at_trigger = true;  // else the loop is written a tick after the sample's start
	int table_len = 72;           // period table length
	bool low_octave_twice = false;
};

struct Sample {
	std::string name;
	uint32_t offset = 0;  // from the start of the sample data, in bytes
	uint16_t words = 0;
	uint16_t repeat = 0;  // in bytes
	uint16_t repeat_words = 1;
};

struct Row {
	uint8_t note = 0;
	uint8_t info = 0;
	uint8_t param = 0;
};

struct TrackEntry {
	uint8_t pattern = 0;
	uint8_t transpose = 0;
	uint8_t sound = 0;  // instrument transpose, or a command above $80
	uint8_t level = 0;  // 7 voices only
};

struct Song {
	uint16_t first = 0, last = 0, speed = 0;
};

enum class LoadResult { Ok, NotHippel, Unsupported };

class Module {
public:
	LoadResult load(const uint8_t *data, size_t size, int replayer);
	static LoadResult probe(const uint8_t *data, size_t size);

	bool coso = false;
	Rules rules;
	std::string build;  // which rules, for display
	std::vector<uint8_t> file;
	uint32_t tag = 0;  // offset of "TFMX"
	uint32_t sample_base = 0;
	int rows = 32;
	int voices = 4;      // or 7
	int mix_khz = 0;     // 7 voices: the mixing rate, as the song gives it
	int mix_period = 0;  // and as channel 4's period
	int tempo = 0;       // 7 voices: ticks are (256 + tempo) / 256 of 1/50 s
	int track_rows = 0;
	std::vector<Sample> samples;
	std::vector<Song> songs;

	uint8_t byte(uint32_t at) const { return at < file.size() ? file[at] : 0; }
	uint16_t word(uint32_t at) const { return uint16_t(byte(at) << 8 | byte(at + 1)); }
	uint32_t lword(uint32_t at) const { return uint32_t(word(at)) << 16 | word(at + 2); }

	// Where a sequence starts in the file. A sequence without an end command runs on into what
	// follows it, as in the original.
	uint32_t freq_seq(int n) const;
	uint32_t vol_seq(int n) const;
	Row row(int pattern, int row) const;
	TrackEntry track(int pos, int voice) const;

private:
	LoadResult load_hip();
	LoadResult load_coso(uint32_t at);
	void read_header_row();
	bool load_samples(uint32_t samples_at);
	bool samples_fit() const;
	int guess_coso_replayer() const;
	void pick_rules(int replayer);

	uint32_t base_ = 0;  // what COSO offsets count from
	uint32_t c_[8] = {};
	uint32_t freq_at_ = 0, vol_at_ = 0, pattern_at_ = 0, track_at_ = 0, headers_end_ = 0;
	std::vector<Row> patterns_;  // COSO
};

}  // namespace hippel

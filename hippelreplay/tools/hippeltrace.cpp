// hippeltrace: prints each voice's Paula state per tick, for comparison with the original replayers.
//
// usage: hippeltrace song [subsong] [ticks] [auto|older|newer|2708|coso]
//
// The subsong is the file's own entry, from 1 (or first:last:speed), whether or not the library offers it. A line per
// tick, voices separated by |: "-" before the first note, else period, volume, DMA, loop offset
// and words, then the start offset and words if a sample starts on that tick (offsets from the
// start of the sample data). For a 7-voice song, a last field has the tick's mix buffer in hex.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "hippelreplay.h"
#include "mixer.h"
#include "module.h"
#include "replayer.h"

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "usage: hippeltrace song [subsong] [ticks] [auto|older|newer|2708|coso]\n");
		return 2;
	}
	int first, last, speed;  // a subsong can also be given as first:last:speed
	const bool range = argc > 2 && sscanf(argv[2], "%d:%d:%d", &first, &last, &speed) == 3;
	const int sub = argc > 2 && !range ? atoi(argv[2]) : 1, ticks = argc > 3 ? atoi(argv[3]) : 500;
	const char *names[] = {"auto", "older", "newer", "2708", "coso"};
	int rep = 0;
	for (int i = 0; argc > 4 && i < 5; i++)
		if (!strcmp(argv[4], names[i]))
			rep = i;
	FILE *f = fopen(argv[1], "rb");
	if (!f)
		return 1;
	std::vector<uint8_t> d;
	static uint8_t buf[65536];
	for (size_t n; (n = fread(buf, 1, sizeof buf, f)) > 0;)
		d.insert(d.end(), buf, buf + n);
	fclose(f);
	hippel::Module m;
	if (m.load(d.data(), d.size(), rep) != hippel::LoadResult::Ok || (!range && (sub < 1 || sub > int(m.songs.size()))))
		return 1;
	hippel::Replayer r(m);
	hippel::Song song;
	if (range) {
		song.first = uint16_t(first);
		song.last = uint16_t(last);
		song.speed = uint16_t(speed);
	} else {
		song = m.songs[size_t(sub - 1)];
	}
	r.start(song);
	hippel::Mixer mixer(m);
	printf("# %s\n", m.build.c_str());
	for (int t = 0; t < ticks; t++) {
		hippel::VoiceOut o[7];
		const int words = r.mix_words();
		r.tick(o);
		for (int v = 0; v < m.voices; v++) {
			if (o[v].active) {
				printf("%d %d %d %lld %d", o[v].per, o[v].vol, o[v].dma ? 1 : 0, (long long)o[v].loop.at - m.sample_base, o[v].loop.words);
				if (o[v].trigger)
					printf(" %lld %d", (long long)o[v].start.at - m.sample_base, o[v].start.words);
			} else {
				printf("-");
			}
			printf(v < m.voices - 1 || m.voices == 7 ? "|" : "\n");
		}
		if (m.voices == 7) {
			int8_t mix[1024];
			mixer.tick(o + 3, r.mix_samples(), 2 * words, mix);
			for (int i = 0; i < 2 * words; i++)
				printf("%02x", uint8_t(mix[i]));
			printf("\n");
		}
	}
	return 0;
}

"""Compare the library (hippeltrace) with the original replayers, tick by tick.

usage: python tests/compare.py <hippel-trace folder> [--ticks N] [--file substring] [--show K] [--rips] [--seven] [--games <savestates folder>] [--modland <folder>]

--rips   every subsong of every .hip rip: the rip's own replayer runs in an emulated Amiga and the
         state of each voice's Paula registers after each tick is compared (period, volume, sample
         start, loop, DMA)
--seven  the 7-voice rips the same way, and the mix buffers byte by byte; with --games, also the games'
         own 7-voice replayers
--games  COSO songs against games' own replayers, from WinUAE savestates (period and volume)

Needs the hippel-trace project (the emulated Amiga) and the modland files, which can't be
distributed as they come from the games. Voices are compared from their first note on.
"""
import argparse
import os
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TRACE = os.path.join(HERE, '..', '..', 'build', 'x64', 'Release', 'hippeltrace.exe')

# Captures whose songs are in modland: savestate, song, subsong, tick the capture starts at, voices
# the music plays (the rest are the game's), whether its main program plays the music
GAMES = [
    ('prehistoric-tale-intro.uss', 'a prehistoric tale (intro).hipc', 1, 17, 4, False),
    ('prehistoric-tale-menu.uss', 'a prehistoric tale (menu).hipc', 1, 26, 4, False),
    ('prehistoric-tale-highscore.uss', 'a prehistoric tale (high).hipc', 1, 19, 4, False),
    ('prehistoric-tale-ingame.uss', 'a prehistoric tale (end).hipc', 2, 17, 4, False),
    ('enchantedland-ingame.uss', 'enchanted land (2).hipc', 2, 139, 4, False),
    ('lethalxcess-menu.uss', 'lethal xcess-main.hipc', 1, 196, 3, False),
    ('astaroth-ingame.uss', 'astaroth4.hipc', 1, 96, 4, True),
    ('astaroth-intro-song.uss', 'astaroth123.hipc', 3, 367, 4, True),
]

# The same, for games that play from a CIA-A timer (level 2)
CIA_GAMES = [
    # the title's song is the game over song too; modland calls it level 1, and the level 1 song level 2
    ('ghostbattle-gameover.uss', 'ghost battle (level 1).hipc', 1, 1046),
    ('ghostbattle-intro.uss', 'ghost battle (level 1).hipc', 1, 261),
    ('ghostbattle-level1.uss', 'ghost battle (level 2).hipc', 1, 409),
    ('returnofmedusa-intro.uss', 'return of medusa (1).hipc', 1, 168),
    ('returnofmedusa-menu.uss', 'return of medusa (2).hipc', 1, 419),
]


def library(path, sub, ticks, replayer='auto'):
    out = subprocess.run([TRACE, path, str(sub), str(ticks), replayer], capture_output=True, text=True, check=True).stdout.split('\n')
    res = []
    for line in out[1:]:
        if not line:
            continue
        row = []
        for f in line.split('|'):
            if f == '-':
                row.append(None)
                continue
            x = [int(n) for n in f.split()]
            row.append((x[0], x[1], tuple(x[5:7]) or None, tuple(x[3:5]), bool(x[2])))
        res.append(row)
    return out[0][2:].strip(), res


def rips(a):
    import compare as ref
    from model import MUSIC_VOICES
    total = exact = 0
    for f in sorted(os.listdir(ref.MODLAND)):
        if not f.endswith('.hip') or f == 'astaroth.hip' or (a.file and a.file not in f):
            continue
        path = os.path.join(ref.MODLAND, f)
        d = open(path, 'rb').read()
        key = ref.build_key(d)
        s = ref.Song(d)
        o, k, _ = s.section('subsong')
        for sub in range(1, s.counts[6] + 1):
            # the 2708 build reads the 6-byte entries 4 bytes apart: play what it plays
            at = o + (4 if key == '2708-812e' else 6) * (sub - 1)
            song = '%d:%d:%d' % struct.unpack_from('>3H', d, at)
            total += 1
            try:
                want = ref.reference(path, sub, a.ticks)
                _, got = library(path, song, a.ticks)
            except Exception as e:  # noqa: BLE001
                print('%-44s sub %2d ERROR %s' % (f[:44], sub, e))
                continue
            voices = MUSIC_VOICES.get(key, 4)
            bad = []
            for t in range(a.ticks):
                for v in range(voices):
                    if got[t][v] is None:
                        continue
                    for i, name in enumerate(('per', 'vol', 'trig', 'loop', 'dma')):
                        x, y = want[t][v][i], got[t][v][i]
                        if name in ('trig', 'loop') and x is not None:
                            x = x[1:]
                        if x != y:
                            bad.append((t, v, name, x, y))
            exact += not bad
            if bad or a.verbose:
                print('%-44s sub %2d %s %s' % (f[:44], sub, key, '%d differ' % len(bad) if bad else 'exact'))
            for b in bad[:a.show]:
                print('      tick %d voice %d %s: original %s library %s' % b)
    print('%d of %d subsongs exact over %d ticks' % (exact, total, a.ticks))


def games(a):
    sys.path.insert(0, os.path.join(a.trace, 'probes'))
    from cosogame import game_states
    coso = os.path.join(a.modland, 'Hippel COSO', 'Jochen Hippel')
    import cia2game
    for st, song, sub, k in CIA_GAMES:
        g = cia2game.states(os.path.join(a.games, st), a.ticks)
        _, got = library(os.path.join(coso, song), sub, a.ticks + k)
        bad = [(t, v, g[t][v][:2], got[k + t][v][:2]) for t in range(2, a.ticks) for v in range(4)
               if got[k + t][v] is not None and g[t][v][:2] != got[k + t][v][:2]]
        print('%-32s %-34s sub %d: %s' % (st, song, sub, '%d differ' % len(bad) if bad else 'exact over %d ticks' % a.ticks))
        for b in bad[:a.show]:
            print('      tick %d voice %d: game %s library %s' % b)
    for st, song, sub, k, voices, main in GAMES:
        g = game_states(os.path.join(a.games, st), a.ticks, main, 4)
        _, got = library(os.path.join(coso, song), sub, a.ticks + k)
        bad = [(t, v, g[t][v][:2], got[k + t][v][:2]) for t in range(1 if main else 0, a.ticks) for v in range(voices)
               if got[k + t][v] is not None and g[t][v][:2] != got[k + t][v][:2]]
        print('%-32s %-34s sub %d: %s' % (st, song, sub, '%d differ' % len(bad) if bad else 'exact over %d ticks' % a.ticks))
        for b in bad[:a.show]:
            print('      frame %d voice %d: game %s library %s' % b)


def library7(path, sub, ticks):
    out = subprocess.run([TRACE, path, str(sub), str(ticks)], capture_output=True, text=True, check=True).stdout.split('\n')
    res = []
    for line in out[1:]:
        if line:
            f = line.split('|')
            res.append(([None if x == '-' else [int(n) for n in x.split()] for x in f[:3]], bytes.fromhex(f[7])))
    return res


def check7(name, want, got, show):
    regs = mixes = worst = samples = 0
    shown = 0
    for t, ((w, wb), (g, gb)) in enumerate(zip(want, got)):
        for v in range(3):
            if g[v] is None:
                continue
            a = (w[v][0], w[v][1], w[v][2])
            b = (g[v][0], g[v][1], g[v][6] if len(g[v]) > 5 else None)
            if a != b:
                regs += 1
                if shown < show:
                    shown += 1
                    print('      tick %d voice %d: original %s library %s' % (t, v, a, b))
        if wb != gb:
            mixes += 1
            if len(wb) == len(gb):
                d = [abs((x ^ 128) - (y ^ 128)) for x, y in zip(wb, gb)]
                samples += sum(1 for x in d if x)
                worst = max(worst, max(d))
            else:
                worst = 999
            if shown < show:
                shown += 1
                print('      tick %d mix differs (%d, %d bytes)' % (t, len(wb), len(gb)))
    print('%-44s %5d ticks: %d voice states differ; %d mix buffers differ (%d samples, by up to %d)'
          % (name, min(len(want), len(got)), regs, mixes, samples, worst), flush=True)


def seven(a):
    from rip7 import Rip7
    sys.path.insert(0, os.path.join(a.trace, 'probes'))
    folder = os.path.join(a.modland, 'Hippel 7V', 'Jochen Hippel')

    def states(ticks, load):
        st = [[None, None, 0, 0] for _ in range(4)]
        out, bufs = [], []
        for t in ticks:
            trig = [None] * 4
            for x in t['log']:
                if x[1] != 'W' or isinstance(x[2], tuple):
                    continue
                reg, val = x[2], x[3]
                if 0xA0 <= reg < 0xE0:
                    v, r = (reg - 0xA0) >> 4, reg & 15
                    if r == 4:
                        st[v][3] = val
                    elif r == 6:
                        st[v][0] = val
                    elif r == 8:
                        st[v][1] = val
                elif reg == 0x96 and val & 0x8000:
                    for v in range(4):
                        if val & (1 << v):
                            trig[v] = st[v][3]
            out.append([(st[v][0], st[v][1], trig[v]) for v in range(3)])
            bufs.append(t['buf'])
        # the voices are written one interrupt after their tick's mix
        return list(zip(out[1:], bufs))

    for f in ('amberstar-extro.hip7', 'amberstar-intro.hip7', 'lethalxcess-end.hip7'):
        for sub in ((1, 2) if f == 'amberstar-intro.hip7' else (1,)):
            if a.file and a.file not in f:
                continue
            path = os.path.join(folder, f)
            r = Rip7(path, sub)

            def ticks():
                for _ in range(a.ticks + 1):
                    t = r.tick()
                    t['buf'] = r.buffer(t)
                    yield t
            check7('%s %d' % (f, sub), states(ticks(), r.load), library7(path, sub, a.ticks), a.show)
    if a.games:
        import game7
        game7.S = a.games + '/'
        for st, f in (('amberstar-intro-song.uss', 'amberstar-intro.hip7'), ('lethalxcess-intro-song.uss', 'lethalxcess-intro.hip7')):
            want = states(game7.game(st, a.ticks + 1), 0)
            got = library7(os.path.join(folder, f), 1, a.ticks + 12000)
            # where the capture starts: the periods of the voices over 300 ticks
            best = max(range(12000), key=lambda k: sum(1 for t in range(1, 300, 7) for v in range(3)
                       if got[k + t][0][v] and got[k + t][0][v][0] == want[t][0][v][0] and got[k + t][0][v][1] == want[t][0][v][1]))
            check7('%s from tick %d' % (st, best), want, got[best:], a.show)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('trace')
    ap.add_argument('--ticks', type=int, default=1500)
    ap.add_argument('--file')
    ap.add_argument('--show', type=int, default=0)
    ap.add_argument('--rips', action='store_true')
    ap.add_argument('--seven', action='store_true')
    ap.add_argument('--games')
    ap.add_argument('--modland', help="folder holding modland's Hippel, Hippel 7V and Hippel COSO")
    ap.add_argument('-v', '--verbose', action='store_true')
    a = ap.parse_args()
    if not a.modland and (a.seven or a.games):
        ap.error('--seven and --games need --modland')
    # this file is also called compare.py: make sure "import compare" finds hippel-trace's
    sys.path[:] = [p for p in sys.path if os.path.abspath(p or '.') != HERE]
    sys.path.insert(0, os.path.abspath(a.trace))
    if a.rips:
        rips(a)
    if a.seven:
        seven(a)
    elif a.games:
        games(a)


if __name__ == '__main__':
    main()

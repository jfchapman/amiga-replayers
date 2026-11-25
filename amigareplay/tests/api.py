"""Tests the API through the DLL: detection, companion files, options, settings, errors and, for each song given, its
information and playback.

Usage: python api.py <song> [more songs] [--dll amigareplay.dll path]
"""
import ctypes as c
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OK, ARGUMENT, OPTION, FORMAT, FILES = range(5)
NO, YES, YES_FILES = range(3)


class Settings(c.Structure):
    _fields_ = [('size', c.c_size_t), ('sample_rate', c.c_int), ('stereo_separation', c.c_int), ('interpolation', c.c_int),
                ('volume_ramping', c.c_int), ('loop_count', c.c_int), ('fade_seconds', c.c_float), ('gain', c.c_float),
                ('amiga_model', c.c_int), ('led_filter', c.c_int)]


class Option(c.Structure):
    _fields_ = [('name', c.c_char_p), ('value', c.c_char_p)]


class File(c.Structure):
    _fields_ = [('name', c.c_char_p), ('data', c.c_char_p), ('size', c.c_size_t)]


class Info(c.Structure):
    _fields_ = [('size', c.c_size_t), ('id', c.c_int), ('number', c.c_int), ('duration', c.c_double),
                ('loop_start', c.c_double), ('loop_length', c.c_double), ('looped', c.c_int), ('audible', c.c_int),
                ('repeat_of', c.c_int)]


args = sys.argv[1:]
if not args:
    raise SystemExit(__doc__)
dll = os.path.join(HERE, '..', '..', 'build', 'x64-dll', 'Release', 'amigareplay.dll')
if '--dll' in args:
    i = args.index('--dll')
    dll = args[i + 1]
    del args[i:i + 2]
lib = c.CDLL(os.path.abspath(dll), winmode=0)


def function(name, result, *types):
    f = getattr(lib, 'amigareplay_' + name)
    f.restype, f.argtypes = result, types
    return f


P, I, S, D = c.c_void_p, c.c_int, c.c_char_p, c.c_double
error_string = function('error_string', S, I)
default_settings = function('default_settings', None, c.POINTER(Settings), c.c_size_t)
option_count = function('option_count', I)
option_name = function('option_name', S, I)
option_values = function('option_values', S, I)
option_default = function('option_default', S, I)
get_option = function('get_option', S, P, S)
probe = function('probe', I, c.POINTER(File))
companion_file = function('companion_file', I, c.POINTER(File), I, P, c.c_size_t)
main_file = function('main_file', I, S, I, P, c.c_size_t)
open_files = function('open', P, c.POINTER(File), I, c.POINTER(Settings), c.POINTER(Option), c.POINTER(I))
open_memory = function('open_memory', P, S, c.c_size_t, c.POINTER(Settings), c.POINTER(I))
close = function('close', None, P)
metadata = function('get_metadata', S, P, S)
voice_count = function('voice_count', I, P)
subsong_count = function('subsong_count', I, P)
default_subsong = function('default_subsong', I, P)
subsong_info = function('get_subsong_info', I, P, I, c.POINTER(Info))
subsong_from_id = function('subsong_from_id', I, P, I)
subsong_duration = function('subsong_duration', D, P, I)
select = function('select_subsong', I, P, I)
selected = function('selected_subsong', I, P)
render_float = function('render_float', c.c_size_t, P, P, c.c_size_t)
render_s16 = function('render_s16', c.c_size_t, P, P, c.c_size_t)
seek = function('seek', D, P, D)
position = function('position', D, P)
set_settings = function('set_settings', I, P, c.POINTER(Settings))
set_voice_mask = function('set_voice_mask', I, P, c.c_uint)

failed = 0


def check(what, ok, detail=''):
    global failed
    if not ok:
        failed += 1
        print('FAIL', what, detail)


def new_settings(**values):
    s = Settings()
    default_settings(s, c.sizeof(s))
    for k, v in values.items():
        setattr(s, k, v)
    return s


def opened(files, settings=None, options=()):
    """(module, error)"""
    array = (File * len(files))(*files)
    opts = (Option * (len(options) + 1))(*[Option(*o) for o in options])
    e = I(-1)
    return open_files(array, len(files), settings, opts if options else None, e), e.value


def companions(f):
    out, buf = [], c.create_string_buffer(300)
    while True:
        r = companion_file(f, len(out), buf, len(buf))
        if not r:
            return out
        out.append((buf.value.decode(), r))


def file(name, data):
    return File(name.encode(), data, len(data))


# Without a song.
check('no such song', opened([file('x', b'nothing' * 100)]) == (None, FORMAT))
check('no files', opened([File()]) == (None, ARGUMENT))
check('empty file', opened([file('x', b'')]) == (None, FORMAT))
check('probe of nothing', probe(File()) == NO and probe(None) == NO)
check('errors have names', len({error_string(e) for e in range(7)}) == 7)
old = Settings()
old.sample_rate = 7
default_settings(old, c.sizeof(c.c_size_t))
check('an older, smaller struct is not overrun', old.size == c.sizeof(c.c_size_t) and old.sample_rate == 7)

# TFMX's names.
mdat = b'TFMX-SONG ' + bytes(0x1000)
for name, other in (('mdat.a b', 'smpl.a b'), ('MDAT.X', 'SMPL.X'), ('dir/mdat.x', 'smpl.x'), ('dir\\x.mdat', 'x.smpl'),
                    ('x.TFX', 'x.SAM'), ('x.tfx', 'x.sam'), ('x', None), ('mdat.', None)):
    check('companion of ' + name, companions(file(name, mdat)) == ([(other, 1)] if other else []), companions(file(name, mdat)))
for other, name in (('smpl.a b', 'mdat.a b'), ('SMPL.X', 'MDAT.X'), ('dir/x.smpl', 'x.mdat'), ('x.SAM', 'x.TFX'), ('x', None),
                    ('mdat.x', None), ('smpl.', None)):
    buf = c.create_string_buffer(300)
    found = buf.value.decode() if main_file(other.encode(), 0, buf, 300) else None
    check('main file of ' + other, found == name, found)
    check('only one', main_file(other.encode(), 1, buf, 300) == 0)
small = c.create_string_buffer(6)
check('companion: small buffer', companion_file(file('mdat.x', mdat), 0, small, 6) == 0)
check('companion: just big enough', companion_file(file('mdat.x', mdat), 0, c.create_string_buffer(7), 7) == 1)
check('probe: two files', probe(file('mdat.x', mdat)) == YES_FILES)
check('smpl missing', opened([file('mdat.x', mdat)]) == (None, FILES))

# Each song.
for path in args:
    data = open(path, 'rb').read()
    files = [file(path, data)]
    for name, needed in companions(files[0]):
        other = os.path.join(os.path.dirname(path), name)
        if os.path.exists(other):
            files.append(file(other, open(other, 'rb').read()))
    check('probe', probe(files[0]) == (YES_FILES if len(files) > 1 else YES), path)
    m, e = opened(files)
    check('opens', m and e == OK, path)
    if not m:
        continue
    if len(files) == 1:
        one = open_memory(data, len(data), None, None)
        check('opens from memory', one)
        close(one)
    form = metadata(m, b'format').decode()
    print('%s: %s, %s, %d voices, %d subsongs' % (os.path.basename(path), form, metadata(m, b'type').decode(), voice_count(m), subsong_count(m)))
    check('unknown metadata', metadata(m, b'nothing') == b'')
    check('strings stay', metadata(m, b'type') == metadata(m, b'type'))
    check('subsong 0 selected', selected(m) == 0)
    check('subsong out of range', select(m, 10000) and select(m, -1) and selected(m) == 0)
    check('default subsong', 0 <= default_subsong(m) < subsong_count(m))
    for i in range(subsong_count(m)):
        info = Info(size=c.sizeof(Info))
        check('info', subsong_info(m, i, info) == 0)
        check('id', subsong_from_id(m, info.id) == i)
        check('duration', info.duration == subsong_duration(m, i) or info.duration < 0)
    info = Info(size=0)
    check('info without a size', subsong_info(m, 0, info) == -1)
    check('unknown id', subsong_from_id(m, 9999) == -1)

    # Options: every value of every one, in any song. Another format's is ignored.
    for i in range(option_count()):
        name, values = option_name(i), option_values(i).decode().split()
        check('default is a value', option_default(i).decode() in values)
        mine = name.decode().startswith(form + '.')
        check('option of its own format', bool(get_option(m, name)) == mine)
        for v in values:
            for v in v.split('-') if v[0].isdigit() else [v]:
                o, e = opened(files, None, [(name, v.encode())])
                check('option', o and e == OK, (name, v))
                # 7-voice Hippel songs always use the same replayer.
                if mine and v != 'auto' and not (form == 'hippel' and voice_count(m) > 4):
                    check('option given', get_option(o, name).decode() == v, (name, v, get_option(o, name)))
                close(o)
        for v in ('', 'nonsense', '7', '23', '8-22', '8 ', 'auto pro', None):
            check('bad value', opened(files, None, [(name, None if v is None else v.encode())]) == (None, OPTION), (name, v))
    check('unknown option', opened(files, None, [(b'a.b', b'auto')]) == (None, OPTION))

    # Settings.
    for k, v in (('sample_rate', 7999), ('sample_rate', 192001), ('stereo_separation', 101), ('interpolation', 3),
                 ('volume_ramping', -1), ('loop_count', -2), ('fade_seconds', -1), ('gain', -1), ('amiga_model', 3),
                 ('led_filter', 3), ('size', 0)):
        check('bad setting', opened(files, new_settings(**{k: v})) == (None, ARGUMENT), (k, v))
        check('bad setting', set_settings(m, new_settings(**{k: v})) == -1, (k, v))
    check('sample rate stays', set_settings(m, new_settings(sample_rate=48000)) == -1)
    check('settings', set_settings(m, new_settings(stereo_separation=0)) == 0)
    short = new_settings(size=c.sizeof(c.c_size_t) + 4, sample_rate=48000, gain=-5)
    o, e = opened(files, short)
    check('an older settings struct: the rest are defaults', o and e == OK)
    close(o)

    # Playback.
    n = 44100
    a, b = (c.c_int16 * (2 * n))(), (c.c_int16 * (2 * n))()
    set_settings(m, new_settings())
    select(m, 0)
    got = render_s16(m, a, n)
    check('position', abs(position(m) - got / 44100) < 0.03)
    check('seek', abs(seek(m, 0)) < 1e-9)
    check('same after a seek', render_s16(m, b, n) == got and bytes(a) == bytes(b))
    set_voice_mask(m, 0)
    select(m, 0)
    f = (c.c_float * (2 * n))()
    render_float(m, f, n)
    check('no voices: silence', max(map(abs, f)) < 1e-3)
    close(m)
    m, e = opened(files, new_settings(loop_count=-1))
    info = Info(size=c.sizeof(Info))
    subsong_info(m, 0, info)
    check('plays for ever', (subsong_duration(m, 0) == -1) == bool(info.looped) or info.duration < 0)
    close(m)

print('%d failed' % failed if failed else 'all passed')
sys.exit(bool(failed))

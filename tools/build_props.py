# Rebuild ONLY the world props (tools/props_models.py) and splice their blocks
# into code/models.h, leaving every weapon, hand and hero model untouched (a
# full build_models.py run also re-rolls unrelated guns).
#
#   blender -b --factory-startup -P tools/build_props.py -- [PROP_TAG ...]
#
# No tags = every prop in PROP_MODELS. A tag not yet in models.h is appended.
import sys, os, re
ROOT = os.path.join(os.path.dirname(os.path.abspath(sys.argv[sys.argv.index('-P') + 1])), '..')
want = set(sys.argv[sys.argv.index('--') + 1:]) if '--' in sys.argv else set()
src = open(os.path.join(ROOT, 'tools', 'build_models.py')).read()
head = src[:src.index('\nstore = {}')]
g = globals()
exec(compile('\n'.join(l for l in head.split('\n') if not l.startswith('OUT = ')), 'build_models_head', 'exec'), g)
g['OUT'] = os.path.join(ROOT, 'code', 'models.h')
store = {}
exec(compile(open(os.path.join(ROOT, 'tools', 'props_models.py')).read(), 'props_models.py', 'exec'), g)
for pfn, ptag, pk, pao in PROP_MODELS:
    if want and ptag not in want: continue
    pfn(); emit(store, ptag, k=pk, aolen=pao)
M = open(OUT).read()
for tag, tris in store.items():
    rows = []; parts = []
    for (col, rough, metal, group, flag), vs in tris.items():
        parts.append((len(rows), len(vs), *col, rough, metal, group, flag)); rows.extend(vs)
    out = ['static const int16_t %s_V[][7] = {\n' % tag]
    line = []; ks = KSCALE.get(tag, 1.0)
    q = lambda v: max(-32767, min(32767, int(round(v*32000/ks))))
    n = lambda v: max(-32767, min(32767, int(round(v*32767))))
    for x, y, z, a, b, c, ao in rows:
        line.append('{%d,%d,%d,%d,%d,%d,%d}' % (q(x), q(y), q(z), n(a), n(b), n(c), ao))
        if len(line) == 8: out.append(','.join(line) + ',\n'); line = []
    if line: out.append(','.join(line) + ',\n')
    out.append('};\nstatic const model_part_t %s_P[] = {\n' % tag)
    for p in parts: out.append('  {%d,%d,%.3ff,%.3ff,%.3ff,%.3ff,%.3ff,%d,%d},\n' % p)
    out.append('};\n#define %s_NP %d\n' % (tag, len(parts)))
    if tag in KSCALE: out.append('#define %s_K %.1ff\n' % (tag, KSCALE[tag]))
    blk = ''.join(out)
    m = re.search(r'static const int16_t %s_V\[\]\[7\] = \{.*?#define %s_NP \d+\n(?:#define %s_K [\d.]+f\n)?'
                  % (tag, tag, tag), M, re.S)
    M = M[:m.start()] + blk + M[m.end():] if m else M + blk
    print('SPLICED' if m else 'APPENDED', tag)
open(OUT, 'w').write(M)

# BlindDrop first-person models. Run: blender -b --factory-startup -P tools/build_models.py
# Sculpts the rifle, gloves and forearm armour and writes code/models.h, which is
# compiled into the game. Units are metres. Rifle axes match the game's weapon
# frame: x right, y up, z forward.
import bpy, bmesh, math, os, sys
from math import cos, sin, radians
from mathutils import Vector
from mathutils.bvhtree import BVHTree

OUT = os.path.join(os.path.dirname(os.path.abspath(sys.argv[sys.argv.index('-P') + 1])), '..', 'code', 'models.h')
TAN=(.63,.53,.39); GRAPH=(.185,.188,.197); DARK=(.30,.305,.32); BLACK=(.20,.203,.21)
STEEL=(.46,.46,.47); AMBER=(1.0,.62,.10); DECAL=(.95,.78,.16); ORANGE=(1.0,.54,.08); HOLO=(.19,.195,.205); SAND=(.60,.50,.37)
# Tactical glove and forearm vambrace palette (authored in display space; shader squares vCol)
SLATE=(.204,.224,.268)          # desaturated tactical slate-navy softshell fabric (flag=2)
SEAM=(.082,.090,.112)           # recessed dark piping / seam channels (flag=4)
LEATHER=(.134,.140,.152)        # diamond-stippled charcoal palm & grip leather (flag=7)
TIP_CAP=(.242,.248,.258)        # matte grey tactical suede thumb-tip & trigger pad (flag=7)
PAD_RIM=(.078,.083,.096)        # dark recessed bezel around moulded glove plates (flag=4)
PAD=(.320,.322,.330)            # matte warm-charcoal moulded knuckle / thumb plates (flag=4)
SLEEVE=(.132,.138,.152)         # dark graphite undersleeve & wrist crease (flag=1)
HARD=(.166,.186,.230)           # dark gunmetal-slate faceted forearm armour (flag=5; *1.25 in shader)
BEZEL=(.128,.140,.168)          # recessed bezel frames on the gauntlet (flag=4)
PLATE_TAN=(.64,.54,.40)         # warm bronze-tan forearm accent plate (flag=6)

def clear():
    bpy.ops.wm.read_factory_settings(use_empty=True)

def from_bm(bm, name):
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    o = bpy.data.objects.new(name, me); bpy.context.collection.objects.link(o)
    return o

def box(c, h, name='box'):
    bm = bmesh.new(); bmesh.ops.create_cube(bm, size=2.0)
    for v in bm.verts:
        v.co.x = c[0] + v.co.x*h[0]; v.co.y = c[1] + v.co.y*h[1]; v.co.z = c[2] + v.co.z*h[2]
    return from_bm(bm, name)

def prof(pts, x0, x1, name='prof', r=None):
    """Extrude a (z, y) side profile across x from x0 to x1. Under a gun build the
    profile is remembered (with any cuts made into it), and finish() re-forms it
    as a SCULPTED body (see sculpt_prof); r overrides its edge radius, 0 keeps
    the straight extrusion."""
    o = _prof_extrude(pts, x0, x1, name)
    o['_pp'] = [c for p_ in pts for c in p_]; o['_px'] = (x0, x1)
    if r is not None: o['_pr'] = r
    return o

# ------------------------------------------------------------------ SCULPTED BODIES
# THE GUNS WERE BOXES because their bodies were side profiles pushed straight across
# x: every receiver, frame, grip, stock and magazine had dead-flat flanks meeting a
# dead-flat top at a 90-degree edge, softened only by a 1 mm bevel. A moulded or
# machined part is not that: its section is a rounded rectangle whose corner radius
# is a real fraction of its width. sculpt_prof slices the same side profile into
# vertical stations (every vertex of the outline, either side of every vertical
# edge, and every 2 mm between) and lofts a rounded-rectangle section through them:
# the side view is the authored silhouette exactly, the section is no longer a box.
# The cuts made into the extrusion (ports, slots, holes) are replayed onto the loft.
_SCULPT = False
def _prof_extrude(pts, x0, x1, name):
    bm = bmesh.new()
    a = [bm.verts.new((x0, y, z)) for z, y in pts]; b = [bm.verts.new((x1, y, z)) for z, y in pts]
    bm.faces.new(a); bm.faces.new(b[::-1])
    n = len(pts)
    for i in range(n):
        j = (i+1) % n
        bm.faces.new((a[j], a[i], b[i], b[j]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return from_bm(bm, name)

def _prof_span(pts, z):
    """The outline's (y_min, y_max) on the vertical line at z, or None."""
    ys = []
    n = len(pts)
    for i in range(n):
        (za, ya), (zb, yb) = pts[i], pts[(i + 1) % n]
        if za == zb: continue
        if min(za, zb) <= z <= max(za, zb):
            t = (z - za) / (zb - za); ys.append(ya + (yb - ya) * t)
    if len(ys) < 2: return None
    return min(ys), max(ys)

def sculpt_prof(o):
    """Re-form a prof() body as a lofted rounded-rectangle section (see above)."""
    flat = list(o['_pp']); pts = [(flat[i], flat[i + 1]) for i in range(0, len(flat), 2)]
    x0, x1 = o['_px']; hw = (x1 - x0) / 2; xc = (x0 + x1) / 2
    r = o.get('_pr', None)
    if r is None: r = min(max(.24 * (x1 - x0), .0018), .0062)
    zs = sorted(z for z, _ in pts); za, zb = zs[0], zs[-1]
    st = set()
    for z, _ in pts:
        for d in (-1.5e-4, 0.0, 1.5e-4): st.add(round(z + d, 6))
    # (the outline is straight between its vertices, so the vertex stations alone
    # carry it exactly: no stations in between)
    st = sorted(z for z in st if za + 1e-6 < z < zb - 1e-6)
    secs = []
    for z in st:
        sp = _prof_span(pts, z)
        if sp is None or sp[1] - sp[0] < 2e-5: continue
        secs.append((z, sp[0], sp[1]))
    if len(secs) < 3: return False
    # cap each end with a sliver station a hair inside the outline's extreme
    NC = 5                                   # points per corner arc
    bm = bmesh.new(); rings = []
    for z, yb, yt in secs:
        hh = (yt - yb) / 2; yc = (yt + yb) / 2
        rr = min(r, hw * .96, hh * .96)
        ring = []
        for q, (cx_, cy_) in enumerate(((1, 1), (-1, 1), (-1, -1), (1, -1))):
            a0 = q * math.pi / 2
            for j in range(NC):
                a = a0 + (math.pi / 2) * j / (NC - 1)
                px = xc + cx_ * (hw - rr) + rr * cos(a)
                py = yc + cy_ * (hh - rr) + rr * sin(a)
                ring.append(bm.verts.new((px, py, z)))
        rings.append(ring)
    m = len(rings[0])
    for kk in range(len(rings) - 1):
        for i in range(m):
            j = (i + 1) % m
            bm.faces.new((rings[kk][i], rings[kk][j], rings[kk + 1][j], rings[kk + 1][i]))
    bm.faces.new(rings[0]); bm.faces.new(rings[-1][::-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(o.name + '_sc'); bm.to_mesh(me); bm.free()
    old = o.data; o.data = me; bpy.data.meshes.remove(old)
    _replay_cuts(o)
    return True

def _replay_cuts(o):
    for cname in list(o.get('_cuts', [])):
        cm = bpy.data.meshes.get(cname)
        if cm is None: continue
        co = bpy.data.objects.new('recut', cm); bpy.context.collection.objects.link(co)
        md = o.modifiers.new('cut', 'BOOLEAN'); md.operation = 'DIFFERENCE'; md.object = co; md.solver = 'EXACT'
        bpy.context.view_layer.objects.active = o; o.select_set(True)
        bpy.ops.object.modifier_apply(modifier=md.name); o.select_set(False)
        bpy.data.objects.remove(co)

def prof_front(pts, z0, z1, name='front', r=None):
    """Extrude an (x, y) front profile along z. Under a gun build its two ends are
    rounded over by finish() (sculpt_front); r sets the radius, 0 keeps them square."""
    o = _front_extrude(pts, z0, z1, name)
    o['_fp'] = [c for p_ in pts for c in p_]; o['_fz'] = (z0, z1)
    if r is not None: o['_pr'] = r
    return o

def sculpt_front(o):
    """A prof_front body with its ends ROUNDED OVER: the section is pulled in
    toward its own centroid over the last r of each end along a quarter circle,
    so a slide or a handguard ends in a radiused edge, not a sawn-off face."""
    flat = list(o['_fp']); pts = [(flat[i], flat[i + 1]) for i in range(0, len(flat), 2)]
    z0, z1 = o['_fz']
    xs = [p_[0] for p_ in pts]; ys = [p_[1] for p_ in pts]
    cx_, cy_ = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2
    half = min(max(xs) - min(xs), max(ys) - min(ys)) / 2
    r = o.get('_pr', None)
    if r is None: r = min(.30 * half, .0040, (z1 - z0) * .25)
    if r <= 1e-5: return False
    NS = 5                                    # (z, inset) stations round each end
    rear = [(z0 + r * (1 - cos(t * math.pi / 2)), r * (1 - sin(t * math.pi / 2))) for t in (k_ / (NS - 1) for k_ in range(NS))]
    front = [(z1 - zz + z0, d) for zz, d in reversed(rear)]
    sta = rear + front
    sta = [(z, max(d, 0.0)) for z, d in sta]
    sta = [(z, d) for i_, (z, d) in enumerate(sta) if i_ == 0 or z > sta[i_ - 1][0] + 1e-7]
    bm = bmesh.new(); rings = []
    for z, d in sta:
        k = max(0.0, 1 - d / max(half, 1e-6))
        rings.append([bm.verts.new((cx_ + (x - cx_) * k, cy_ + (y - cy_) * k, z)) for x, y in pts])
    m = len(pts)
    for kk in range(len(rings) - 1):
        for i in range(m):
            j = (i + 1) % m
            bm.faces.new((rings[kk][i], rings[kk][j], rings[kk + 1][j], rings[kk + 1][i]))
    bm.faces.new(rings[0]); bm.faces.new(rings[-1][::-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(o.name + '_sf'); bm.to_mesh(me); bm.free()
    old = o.data; o.data = me; bpy.data.meshes.remove(old)
    _replay_cuts(o)
    return True

def _front_extrude(pts, z0, z1, name):
    bm = bmesh.new()
    a = [bm.verts.new((x, y, z0)) for x, y in pts]; b = [bm.verts.new((x, y, z1)) for x, y in pts]
    bm.faces.new(a); bm.faces.new(b[::-1])
    n = len(pts)
    for i in range(n):
        j = (i+1) % n
        bm.faces.new((a[j], a[i], b[i], b[j]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return from_bm(bm, name)

def lathe(rings, x=0.0, y=0.0, seg=28, name='lathe'):
    """Round part along z. rings = [(z, r), ...]."""
    seg = max(seg, 44)
    bm = bmesh.new(); rv = []
    for z, r in rings:
        rv.append([bm.verts.new((x + r*cos(2*math.pi*i/seg), y + r*sin(2*math.pi*i/seg), z)) for i in range(seg)])
    for k in range(len(rv)-1):
        for i in range(seg):
            j = (i+1) % seg
            bm.faces.new((rv[k][i], rv[k][j], rv[k+1][j], rv[k+1][i]))
    bm.faces.new(rv[0][::-1]); bm.faces.new(rv[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return from_bm(bm, name)

def pin(x0, x1, y, z, r, seg=16, name='pin'):
    """Round part across x, centred on (y, z)."""
    seg = max(seg, 40)
    return prof([(z + r*cos(2*math.pi*i/seg), y + r*sin(2*math.pi*i/seg)) for i in range(seg)], x0, x1, name)

def cut(o, cutter):
    if '_pp' in o or '_fp' in o:            # remember the cut for the sculpt's replay
        keep = cutter.data.copy(); keep.use_fake_user = True
        o['_cuts'] = list(o.get('_cuts', [])) + [keep.name]
    m = o.modifiers.new('cut', 'BOOLEAN'); m.operation = 'DIFFERENCE'; m.object = cutter; m.solver = 'EXACT'
    bpy.context.view_layer.objects.active = o; o.select_set(True)
    bpy.ops.object.modifier_apply(modifier=m.name); o.select_set(False)
    bpy.data.objects.remove(cutter)

TRITIUM = (.42, .95, .36)
_LIGHT = None          # the active palette's light-family colours (see paint_build)
LIGHT_MARK = .123
def finish(o, col, rough=.55, metal=0.0, bevel=.0009, seg=2, group=0, flag=0, harden=False):
    if _SCULPT and '_pp' in o and o.get('_pr', 1) != 0 and o['_px'][1] - o['_px'][0] >= .0095:
        if sculpt_prof(o): bevel = min(bevel, .0008)
    elif _SCULPT and '_fp' in o and o.get('_pr', 1) != 0 and o['_fz'][1] - o['_fz'][0] >= .016:
        flat = list(o['_fp']); xs = flat[0::2]; ys = flat[1::2]
        if min(max(xs) - min(xs), max(ys) - min(ys)) >= .010: sculpt_front(o)
    bpy.context.view_layer.objects.active = o
    if bevel > 0:
        m = o.modifiers.new('b', 'BEVEL'); m.width = bevel; m.segments = seg
        m.limit_method = 'ANGLE'; m.angle_limit = radians(28 if harden else 32); m.harden_normals = harden
    o.data.polygons.foreach_set('use_smooth', [True]*len(o.data.polygons))
    if harden: o['harden'] = True
    o['col'] = list(col); o['rough'] = rough; o['metal'] = metal; o['group'] = group; o['flag'] = flag
    # THE LIGHT FAMILY under a real factory finish. The Armory's themes paint the
    # gun's "light primary family" (the parts authored in TAN), and the game used
    # to find that family by its hue (r > 1.3 b). Painted in their real colours a
    # black M4 or an olive AUG has no tan left to find, so the family is marked
    # instead: metal 0.123 on a finish-flagged part, a value the draw ignores
    # (flagged parts are never metal) and code/figure/weapons.c reads as "skin me".
    # ...and no lit amber inlays on a real gun. The house style put glowing amber
    # strips on receivers, muzzles and scope bells; under a factory finish a sight
    # dot keeps its glow as a tritium night-sight green and the rest are not
    # emitted at all (an object without 'col' is skipped by emit and the LOD).
    if _LIGHT is not None and flag == 3 and tuple(round(x, 3) for x in col) == tuple(round(x, 3) for x in AMBER):
        if max(o.dimensions) <= .0045:
            o['col'] = list(TRITIUM)
        else:
            for k in ('col', 'rough', 'metal', 'group', 'flag'): del o[k]
            bpy.data.objects.remove(o)
            return None
    if _LIGHT and tuple(round(x, 4) for x in col) in _LIGHT and flag in (0, 4, 5, 6):
        o['light'] = True; o['metal'] = LIGHT_MARK
    return o

def snapshot():
    '''The evaluated geometry in the scene now, as (verts, polys): an occluder another
    model's AO bake can see without emitting it (see emit's `extra`).'''
    dg = bpy.context.evaluated_depsgraph_get()
    vs = []; ps = []
    for o in list(bpy.context.collection.objects):
        if 'col' not in o: continue
        e = o.evaluated_get(dg); me = e.to_mesh()
        base = len(vs)
        vs.extend(v.co.copy() + o.location for v in me.vertices)
        ps.extend([i + base for i in p.vertices] for p in me.polygons)
        e.to_mesh_clear()
    return vs, ps

KSCALE = {}   # tag -> position scale: a world prop is metres across, not a gun
def emit(store, tag, extra=None, k=1.0, aolen=.045):
    dg = bpy.context.evaluated_depsgraph_get()
    tris = {}
    allv = []; allp = []
    meshes = []
    for o in list(bpy.context.collection.objects):
        if 'col' not in o: continue
        e = o.evaluated_get(dg); me = e.to_mesh()
        base = len(allv)
        allv.extend(v.co.copy() + o.location for v in me.vertices)
        allp.extend([i + base for i in p.vertices] for p in me.polygons)
        meshes.append((o, e, me))
    if extra:
        # A HELD OBJECT AND ITS HAND SHADE EACH OTHER. They are separate models, so each
        # bake used to see only itself and the grenade read as pasted onto the glove.
        base = len(allv)
        allv.extend(extra[0]); allp.extend([i + base for i in p] for p in extra[1])
    bvh = BVHTree.FromPolygons(allv, allp)
    # cosine-weighted hemisphere, fixed pattern: creases go dark, open faces stay lit
    dirs = []
    for i in range(20):
        u = (i + .5) / 20; ph = i * 2.399963
        r = math.sqrt(u); dirs.append((r*cos(ph), r*sin(ph), math.sqrt(1-u)))
    cache = {}
    def occl(p, n):
        key = (round(p.x, 5), round(p.y, 5), round(p.z, 5), round(n.x, 1), round(n.y, 1), round(n.z, 1))
        if key in cache: return cache[key]
        tx = n.orthogonal().normalized(); ty = n.cross(tx)
        org = p + n * .00035 * k; hit = 0
        for dx, dy, dz in dirs:
            d = tx*dx + ty*dy + n*dz
            if bvh.ray_cast(org, d, aolen)[0] is not None: hit += 1
        v = 1.0 - hit / len(dirs)
        cache[key] = int(255 * (.34 + .66 * v))
        return cache[key]
    for o, e, me in meshes:
        if o.get('harden', False): pass
        elif o['flag'] not in (1, 2, 7): me.set_sharp_from_angle(angle=radians(38))
        elif o.get('sharp_pad', False): me.set_sharp_from_angle(angle=radians(42))
        me.calc_loop_triangles()
        key = (tuple(round(x, 3) for x in o['col']), o['rough'], o['metal'], o['group'], o['flag'])
        out = tris.setdefault(key, [])
        cn = me.corner_normals
        for t_ in me.loop_triangles:
            for l in t_.loops:
                v = me.vertices[me.loops[l].vertex_index].co + o.location; n = Vector(cn[l].vector).normalized()
                out.append((v.x, v.y, v.z, n.x, n.y, n.z, occl(v, n)))
    for o, e, me in meshes:
        e.to_mesh_clear()
    store[tag] = tris
    if k != 1.0: KSCALE[tag] = k
    print('MODEL', tag, sum(len(v) for v in tris.values())//3, 'tris')

def emit_lod(store, tag, ratio=.30, tiny=.0050):
    """THE THIRD-PERSON COPY of the gun now in the scene: no bevels, no parts
    smaller than `tiny` (rail teeth, screws, decals: invisible at 10 m), the rest
    decimated to `ratio`. Twenty figures carry one each, so it has to be ~1-2k
    triangles where the first-person mesh is 10-30k."""
    for o in list(bpy.context.collection.objects):
        if 'col' not in o: continue
        bb = [Vector(c) for c in o.bound_box]
        ext = max(max(v[i] for v in bb) - min(v[i] for v in bb) for i in range(3))
        if ext < tiny or o['flag'] == 3:
            bpy.data.objects.remove(o); continue
        for m in list(o.modifiers):
            if m.type == 'BEVEL': o.modifiers.remove(m)
        if len(o.data.polygons) > 24:
            d = o.modifiers.new('lod', 'DECIMATE'); d.ratio = ratio
    emit(store, tag, aolen=.03)

def holo_sight(dy=0.0):
    '''The EOTech-style holo, at the AR's station. dy lifts the whole sight (a rail
    on a carry handle); code/figure/weapons.c adds the same lift to VM_HOLO_Y.'''
    hz = .128
    before_sight = set(bpy.context.collection.objects)
    hous = prof_front([(-.0295,.0478),(.0295,.0478),(.0330,.0530),(.0312,.1035),(.0250,.1135),(-.0250,.1135),(-.0312,.1035),(-.0330,.0530)], hz-.038, hz+.048, 'holo')
    cut(hous, prof_front([(-.0185,.0660),(.0185,.0660),(.0235,.0710),(.0235,.0990),(.0185,.1040),(-.0185,.1040),(-.0235,.0990),(-.0235,.0710)], hz-.1, hz+.1))
    finish(hous, HOLO, .54, .08, .0032, 3)
    finish(box((0,.0568,hz-.0406), (.0255,.0074,.0030)), BLACK, .6, 0, .0010)               # rear control plate
    for xx, yy in ((-.0212,.0614),(.0212,.0614),(-.0212,.0522),(.0212,.0522)):
        finish(lathe([(hz-.0458,.0024),(hz-.0426,.0024)], xx, yy, 14), STEEL, .4, 0, .0003)  # screws
    finish(lathe([(hz-.0466,.0054),(hz-.0426,.0054)], 0, .0568, 20), DARK, .45, 0, .0005)   # dial
    finish(box((-.0115,.0568,hz-.0438), (.0048,.0032,.0012)), DARK, .5, 0, .0003)           # buttons
    finish(box(( .0115,.0568,hz-.0438), (.0048,.0032,.0012)), DARK, .5, 0, .0003)
    finish(prof([(hz-.026,.0550),(hz+.052,.0550),(hz+.052,.0820),(hz+.022,.0860),(hz-.018,.0840),(hz-.026,.0760)], -.0365, -.0305, 'bat'), HOLO, .54, .08, .0022, 3)
    finish(box((-.0370,.0685,hz-.009), (.0006,.0032,.0028)), (.58,.48,.15), .55, 0, .0002, flag=4)  # matte warning decal
    finish(box((-.0370,.0685,hz-.002), (.0006,.0032,.0028)), (.58,.48,.15), .55, 0, .0002, flag=4)
    finish(pin(-.0410,-.0320,.0540,hz+.030,.0072,20), DARK, .45, 0, .0007)                  # QD knob
    finish(pin(-.0428,-.0405,.0540,hz+.030,.0038,16), STEEL, .4, 0, .0004)
    finish(pin(-.0405,-.0320,.0540,hz-.022,.0056,16), DARK, .45, 0, .0006)
    finish(prof([(hz-.054,.0460),(hz+.058,.0460),(hz+.058,.0490),(hz-.054,.0490)], -.0170, .0170), BLACK, .55, 0, .0008)   # mount
    # The whole sight is drawn at full size above, then scaled down about its foot on
    # the rail and its own centre. code/figure/weapons.c's VM_HOLO_* carry the same SIGHT_K.
    SIGHT_K = .72
    for o in set(bpy.context.collection.objects) - before_sight:
        for v in o.data.vertices:
            v.co = Vector((v.co.x*SIGHT_K, .046 + (v.co.y-.046)*SIGHT_K, hz + (v.co.z-hz)*SIGHT_K))
            v.co.y += dy

# ------------------------------------------------------------------ rifle
def rifle():
    clear()
    # --- lower receiver: tan, flared magwell, pocketed side
    low = prof([(-.030,.012),(.150,.012),(.150,-.004),(.146,-.030),(.140,-.036),(.086,-.040),(.080,-.014),(.046,-.018),(.010,-.018),(-.012,-.012),(-.030,-.002)], -.0158, .0158, 'lower')
    cut(low, box((0,-.018,.113), (.0118,.034,.0250)))
    cut(low, prof([(.096,.004),(.136,.004),(.132,-.022),(.098,-.026)], -.0200, -.0134))
    finish(low, TAN, .60, bevel=.0013, seg=3)
    finish(prof([(-.022,.009),(.070,.009),(.070,.004),(-.022,-.002)], -.0146, -.0132), SAND, .6, bevel=.0005)
    # --- upper receiver: graphite, right-side ejection port, a long relief on the left
    up = prof([(-.030,.012),(.172,.012),(.172,.0345),(.166,.0370),(-.022,.0370),(-.030,.0315)], -.0168, .0168, 'upper')
    cut(up, box((.0188,.0225,.070), (.0040,.0070,.0270)))
    cut(up, box((-.0188,.0245,.086), (.0030,.0048,.0600)))
    finish(up, GRAPH, .52, bevel=.0015, seg=3)
    finish(box((.0136,.0225,.070), (.0020,.0068,.0265)), STEEL, .35, .9, .0003, 1)
    # Right-side AR-15 ejection port dust cover (hinged open below port), brass deflector & forward assist!
    finish(box((.0182,.0122,.070), (.0014,.0056,.0275)), BLACK, .55, .2, .0004, 1)       # open dust cover
    finish(prof([(.024,.0165),(.040,.0165),(.040,.0285),(.030,.0285)], .0164, .0242, 'deflector', r=0), GRAPH, .50, .1, .0010, 2)  # brass deflector
    fa_ = lathe([(.000,.0064),(.022,.0058)], 0, 0, 20, 'fa_body')
    fa_.rotation_euler = (0, radians(28), 0); fa_.location = (.0180, .0195, .002)
    bpy.context.view_layer.objects.active = fa_; fa_.select_set(True)
    bpy.ops.object.transform_apply(location=True, rotation=True); fa_.select_set(False)
    finish(fa_, GRAPH, .50, .1, .0006)                                                    # forward assist housing (right!)
    fp_ = lathe([(-.008,.0048),(.002,.0048)], 0, 0, 18, 'fa_plunger')
    fp_.rotation_euler = (0, radians(28), 0); fp_.location = (.0165, .0195, -.002)
    bpy.context.view_layer.objects.active = fp_; fp_.select_set(True)
    bpy.ops.object.transform_apply(location=True, rotation=True); fp_.select_set(False)
    finish(fp_, BLACK, .48, .3, .0005)                                                    # forward assist button (right!)
    finish(pin(.0150,.0178,-.0060,.106,.0052), BLACK, .55, 0, .0004)                      # right-side mag release button
    # --- rail: base plus real teeth, receiver to muzzle end of the handguard
    finish(box((0,.0392,.184), (.0098,.0024,.214)), BLACK, .55, 0, .0005)
    for i in range(44):
        finish(box((0,.0432,-.024+.00985*i), (.0106,.0019,.0028)), BLACK, .5, 0, .0005, 1)
    # --- handguard: short octagonal tube with M-LOK slots on sides & upper chamfers
    hg = prof_front([(.0245*cos(math.pi/8+i*math.pi/4), .0200+.0245*sin(math.pi/8+i*math.pi/4)) for i in range(8)], .170, .398, 'hg')
    for i in range(5):
        cut(hg, box((-.0255,.0200,.200+i*.041), (.0080,.0034,.0145)))
        cut(hg, box(( .0255,.0200,.200+i*.041), (.0080,.0034,.0145)))
        cut(hg, box((0,-.0060,.200+i*.041), (.0038,.0080,.0145)))
    for i in range(4):
        for s_ in (-1, 1):
            c_ = box((s_*.0175, .0200+.0175, .212+i*.042), (.0065,.0030,.0140))
            c_.rotation_euler = (0, 0, -s_*math.pi/4)
            bpy.context.view_layer.objects.active = c_; c_.select_set(True)
            bpy.ops.object.transform_apply(location=True, rotation=True); c_.select_set(False)
            cut(hg, c_)
    finish(hg, TAN, .60, bevel=.0011, seg=2)
    for s_ in (-1, 1):
        finish(box((s_*.0256,.0200,.3660), (.0024,.0064,.0280)), BLACK, .5, 0, .0005)
        for i in range(6):
            finish(box((s_*.0282,.0200,.3400+i*.0104), (.0018,.0070,.0029)), BLACK, .5, 0, .0004, 1)
    # Lower-left rail section & FDE rail cover panel right beside/ahead of the support thumb (zoom_sup!)
    for i in range(4):
        finish(box((-.0225,.0095,.300+i*.0095), (.0022,.0052,.0028)), BLACK, .5, 0, .0004, 1)
    finish(box((-.0230,.0095,.270), (.0026,.0062,.0210)), SAND, .60, 0, .0009, 2)
    finish(lathe([(.158,.0250),(.170,.0268),(.176,.0268),(.179,.0240)], 0, .0200, 24), GRAPH, .5, 0, .0006)   # barrel nut
    # --- exposed barrel and ported birdcage flash hider (extended clearly past handguard nose)
    finish(lathe([(.396,.0086),(.438,.0082)], 0, .0220, 20), BLACK, .42, 0, 0)
    mz = lathe([(.434,.0086),(.440,.0112),(.480,.0112),(.492,.0096),(.492,.0060)], 0, .0220, 22)
    for i in range(3):
        a_ = i*math.pi/3
        c_ = box((0,.0220,.461), (.0028,.0160,.0145))
        c_.rotation_euler = (0,0,a_); bpy.context.view_layer.objects.active = c_; c_.select_set(True)
        bpy.ops.object.transform_apply(rotation=True); c_.select_set(False)
        cut(mz, c_)
    finish(mz, BLACK, .45, 0, .0004)
    # --- wide charging handle, its latch reaching out to the left
    finish(prof([(-.070,.0330),(-.016,.0330),(-.016,.0418),(-.060,.0418),(-.070,.0392)], -.0140, .0140), BLACK, .5, 0, .0012)
    finish(prof([(-.072,.0336),(-.034,.0336),(-.040,.0412),(-.072,.0412)], -.0340, -.0120), DARK, .5, 0, .0014)
    # --- left-side controls (angled safety selector, bolt catch paddle, takedown pins)
    finish(prof([(.070,.0110),(.088,.0110),(.088,-.0040),(.074,-.0040)], -.0176, -.0136), BLACK, .55, 0, .0006)  # bolt catch
    finish(pin(-.0176,-.0134,-.0015,.028,.0068), DARK, .5, bevel=.0005)                  # selector drum
    finish(prof([(.028,.0025),(.048,-.0075),(.045,-.0120),(.025,-.0025)], -.0192, -.0164), BLACK, .48, 0, .0007)  # angled selector lever
    for z, y in ((.138,.0035),(.006,-.0010),(-.020,-.0015)):
        finish(pin(-.0164,-.0134,y,z,.0036,12), BLACK, .48, .6, .0003, 1)
    finish(pin(-.0176,-.0150,-.0060,.106,.0052), BLACK, .55, 0, .0004)                  # mag release
    # --- contoured ergonomic grip and spacious trigger guard window
    finish(prof([(.030,-.014),(-.008,-.013),(-.016,-.026),(-.036,-.100),(-.030,-.110),(.000,-.106),(.010,-.066),(.018,-.044)], -.0126, .0126, 'grip'), BLACK, .76, 0, .0052, 5)
    finish(prof([(.014,-.018),(.019,-.018),(.021,-.0475),(.068,-.0475),(.079,-.018),(.084,-.018),(.071,-.0518),(.014,-.0518)], -.0038, .0038), TAN, .60, bevel=.0007)
    finish(prof([(.044,-.016),(.050,-.016),(.044,-.042),(.039,-.042)], -.0030, .0030), DARK, .45, .6, .0005)
    # --- buffer tube, castle nut, rounded receiver tang, fuller FDE Crane stock
    finish(lathe([(-.236,.0176),(-.036,.0176)], 0, .0210, 28), GRAPH, .44, .18, .0005)
    nut = lathe([(-.052,.0238),(-.034,.0238),(-.030,.0202)], 0, .0210, 28)
    for i in range(6):
        c_ = box((0,.0210+.0230,-.0430), (.0026,.0030,.0070))
        c_.rotation_euler = (0,0,0)
        v_ = i*math.pi/3
        for vv in c_.data.vertices:
            x_, y_ = vv.co.x, vv.co.y - .0210
            vv.co.x = x_*cos(v_) - y_*sin(v_); vv.co.y = .0210 + x_*sin(v_) + y_*cos(v_)
        cut(nut, c_)
    finish(nut, BLACK, .48, .15, .0005)
    # Rounded receiver end-plate flush with lower receiver (no protruding puck!)
    finish(lathe([(-.035,.0202),(-.029,.0202)], 0, .0205, 24), DARK, .48, .15, .0006)
    finish(prof([(-.034,.002),(-.026,.002),(-.026,.020),(-.034,.020)], -.0152, .0152), DARK, .50, .12, .0012)
    OFS = -.014
    # Fuller sloped Crane cheekweld + twin side slots matching zoom_trg right side
    st = prof([(-.088+OFS,.0445),(-.226+OFS,.0445),(-.238+OFS,.036),(-.242+OFS,-.042),(-.226+OFS,-.052),(-.192+OFS,-.036),(-.142+OFS,-.012),(-.094+OFS,.002),(-.086+OFS,.024)], -.0205, .0205, 'stock')
    cut(st, prof([(-.224+OFS,.036),(-.214+OFS,.036),(-.214+OFS,-.030),(-.224+OFS,-.036)], -.03, .03))
    for y0, y1 in ((.0160,.0235),(.0015,.0090)):
        cut(st, prof([(-.106+OFS,y0),(-.168+OFS,y0),(-.168+OFS,y1),(-.106+OFS,y1)], -.03, .03))
    finish(st, TAN, .60, 0, .0036, 3, group=2)
    finish(box((0,-.0030,-.247+OFS), (.0195,.0500,.0065)), BLACK, .9, 0, .0024, 2, group=2)
    # --- holographic sight: tapered EOTech-style arch hood, rear controls, compact side decal
    holo_sight()
    # --- forward-curved polymer PMAG (group 1 so reload moves it)
    mag = prof([(.136,-.024),(.090,-.024),(.096,-.095),(.112,-.164),(.160,-.156),(.144,-.092)], -.0106, .0106, 'mag')
    finish(mag, BLACK, .70, 0, .0020, 2, group=1)
    finish(prof([(.109,-.160),(.163,-.152),(.165,-.162),(.107,-.170)], -.0118, .0118, 'magplate'), BLACK, .76, 0, .0012, 2, group=1)
    finish(prof([(.118,-.040),(.113,-.040),(.129,-.154),(.134,-.153)], -.0114, .0114, 'magrib'), DARK, .68, 0, .0007, 1, group=1)
    for i in range(4):
        t = i / 3.0
        zc = .118 + t * .016; yc = -.056 - t * .082
        finish(box((0, yc, zc), (.0113, .0016, .0175)), DARK, .70, 0, .0006, 1, group=1)

# ------------------------------------------------------------------ sculpted tactical gloves
def V3(*a): return Vector(a)

def _cr_scalar(p0, p1, p2, p3, t):
    t2 = t * t; t3 = t2 * t
    return 0.5 * ((2.0*p1) + (-p0 + p2)*t + (2.0*p0 - 5.0*p1 + 4.0*p2 - p3)*t2 + (-p0 + 3.0*p1 - 3.0*p2 + p3)*t3)

def _cr_vec(p0, p1, p2, p3, t):
    return Vector((_cr_scalar(p0.x, p1.x, p2.x, p3.x, t),
                   _cr_scalar(p0.y, p1.y, p2.y, p3.y, t),
                   _cr_scalar(p0.z, p1.z, p2.z, p3.z, t)))

def eval_ctrl(ctrl, u):
    """Interpolate [(pos, up, rx, ry), ...] cleanly at u in [0, 1]."""
    n = len(ctrl)
    u = max(0.0, min(1.0, u))
    s = u * (n - 1)
    i = min(int(s), n - 2)
    t = s - i
    def get(idx):
        if idx < 0:
            p0, u0, x0, y0 = ctrl[0]; p1, u1, x1, y1 = ctrl[1]
            return (p0*2 - p1, u0, x0, y0)
        if idx >= n:
            p0, u0, x0, y0 = ctrl[-1]; p1, u1, x1, y1 = ctrl[-2]
            return (p0*2 - p1, u0, x0, y0)
        return ctrl[idx]
    a, b, c, d = get(i-1), get(i), get(i+1), get(i+2)
    pos = _cr_vec(a[0], b[0], c[0], d[0], t)
    up  = _cr_vec(a[1], b[1], c[1], d[1], t).normalized()
    rx  = max(0.001, _cr_scalar(a[2], b[2], c[2], d[2], t))
    ry  = max(0.001, _cr_scalar(a[3], b[3], c[3], d[3], t))
    return pos, up, rx, ry

def make_tube_sampler(ctrl, palmar_flat=0.84, power=2.15):
    """Build a continuous parametric surface sampler S(u, deg, lift) -> (pos, nrm).
    deg = +90 is dorsal (+U), deg = -90 is palmar (-U), deg = 0/+180 are the sides (+W/-W)."""
    def frame_at(u):
        du = 0.004
        p0, _, _, _ = eval_ctrl(ctrl, max(0.0, u - du))
        p1, _, _, _ = eval_ctrl(ctrl, min(1.0, u + du))
        pos, up, rx, ry = eval_ctrl(ctrl, u)
        T = (p1 - p0)
        if T.length < 1e-7: T = Vector((0, 0, 1))
        else: T.normalize()
        U = up - T * up.dot(T)
        if U.length < 1e-6: U = T.orthogonal()
        U.normalize()
        W = T.cross(U).normalized()
        return pos, T, U, W, rx, ry

    ex = 2.0 / power
    def spow(x, p): return math.copysign(abs(x) ** p, x)

    def sample(u, deg, lift=0.0):
        pos, T, U, W, rx, ry = frame_at(u)
        th = radians(deg)
        ct, st = spow(cos(th), ex), spow(sin(th), ex)
        ry_eff = ry * (palmar_flat if st < 0.0 else 1.0)
        off = W * (rx * ct) + U * (ry_eff * st)
        nx = cos(th) / max(rx, 1e-4)
        ny = sin(th) / max(ry_eff, 1e-4)
        nrm = (W * nx + U * ny).normalized()
        return pos + off + nrm * lift, nrm
    return sample, frame_at

def loft_tube(ctrl, seg_u=16, seg_v=18, palmar_flat=0.84, power=2.15,
              cap_start=True, cap_end=True, col=SLATE, rough=.72, flag=2, name='tube'):
    """Smooth anatomical digit or palm hull with rounded dome caps; returns (obj, sampler)."""
    sample, frame_at = make_tube_sampler(ctrl, palmar_flat=palmar_flat, power=power)
    bm = bmesh.new()
    rings = []

    if cap_start:
        pos0, T0, U0, W0, rx0, ry0 = frame_at(0.0)
        pole0 = bm.verts.new(pos0 - T0 * (0.42 * min(rx0, ry0)))
        for c_ang in (0.38, 0.72):
            sc = sin(c_ang * math.pi * 0.5)
            ax = cos(c_ang * math.pi * 0.5) * 0.42 * min(rx0, ry0)
            ring = []
            for iv in range(seg_v):
                p, _ = sample(0.0, 360.0 * iv / seg_v, 0.0)
                ring.append(bm.verts.new(pos0 + (p - pos0) * sc - T0 * ax))
            rings.append(ring)
    else:
        pole0 = None

    for iu in range(seg_u + 1):
        u = iu / seg_u
        rings.append([bm.verts.new(sample(u, 360.0 * iv / seg_v, 0.0)[0]) for iv in range(seg_v)])

    if cap_end:
        pos1, T1, U1, W1, rx1, ry1 = frame_at(1.0)
        for c_ang in (0.68, 0.36):
            sc = sin(c_ang * math.pi * 0.5)
            ax = cos(c_ang * math.pi * 0.5) * 0.58 * min(rx1, ry1)
            ring = []
            for iv in range(seg_v):
                p, _ = sample(1.0, 360.0 * iv / seg_v, 0.0)
                ring.append(bm.verts.new(pos1 + (p - pos1) * sc + T1 * ax))
            rings.append(ring)
        pole1 = bm.verts.new(pos1 + T1 * (0.58 * min(rx1, ry1)))
    else:
        pole1 = None

    for k in range(len(rings) - 1):
        r0, r1 = rings[k], rings[k + 1]
        for iv in range(seg_v):
            jv = (iv + 1) % seg_v
            bm.faces.new((r0[iv], r1[iv], r1[jv], r0[jv]))

    if pole0 is not None:
        r0 = rings[0]
        for iv in range(seg_v):
            jv = (iv + 1) % seg_v
            bm.faces.new((pole0, r0[jv], r0[iv]))
    else:
        bm.faces.new(rings[0][::-1])

    if pole1 is not None:
        r1 = rings[-1]
        for iv in range(seg_v):
            jv = (iv + 1) % seg_v
            bm.faces.new((r1[iv], pole1, r1[jv]))
    else:
        bm.faces.new(rings[-1])

    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    o = finish(from_bm(bm, name), col, rough, 0, bevel=0.0, flag=flag)
    return o, sample

def shell_patch(sample, u0, u1, deg0, deg1, thick=.0016, bury=.0008,
                nu=6, nv=7, rim_u=.035, rim_d=8.0, col=PAD, rough=.48,
                flag=4, sharp=False, name='patch'):
    """Conformal curved tactical panel/pad sitting on `sample(u, deg, lift)` with a clean
    chamfered perimeter skirt buried inside the host surface — zero floating corners."""
    bm = bmesh.new()
    us = [u0] + [u0 + rim_u + (u1 - u0 - 2*rim_u) * (i / (nu - 1)) for i in range(nu)] + [u1]
    ds = [deg0] + [deg0 + rim_d + (deg1 - deg0 - 2*rim_d) * (j / (nv - 1)) for j in range(nv)] + [deg1]
    top = []
    bot = []
    for iu, u in enumerate(us):
        trow = []; brow = []
        edge_u = (iu == 0 or iu == len(us) - 1)
        for idg, dg in enumerate(ds):
            edge = edge_u or (idg == 0 or idg == len(ds) - 1)
            lift = -bury * 0.45 if edge else thick
            trow.append(bm.verts.new(sample(u, dg, lift)[0]))
            brow.append(bm.verts.new(sample(u, dg, -bury)[0]))
        top.append(trow); bot.append(brow)

    ru = len(us); rd = len(ds)
    for i in range(ru - 1):
        for j in range(rd - 1):
            bm.faces.new((top[i][j], top[i+1][j], top[i+1][j+1], top[i][j+1]))
            bm.faces.new((bot[i][j], bot[i][j+1], bot[i+1][j+1], bot[i+1][j]))
    for i in range(ru - 1):
        bm.faces.new((bot[i][0], bot[i+1][0], top[i+1][0], top[i][0]))
        bm.faces.new((top[i][-1], top[i+1][-1], bot[i+1][-1], bot[i][-1]))
    for j in range(rd - 1):
        bm.faces.new((top[0][j], top[0][j+1], bot[0][j+1], bot[0][j]))
        bm.faces.new((bot[-1][j], bot[-1][j+1], top[-1][j+1], top[-1][j]))

    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    o = finish(from_bm(bm, name), col, rough, 0, bevel=0.0, flag=flag)
    if sharp: o['sharp_pad'] = True
    return o

def flat(c, along, out, hl, hw, ht=.0024, col=None, b=.0010, rough=.50, flag=4):
    """A flat bevelled slab centred on c."""
    c = Vector(c); a_ = Vector(along).normalized(); o_ = Vector(out)
    o_ = (o_ - a_*o_.dot(a_)).normalized(); w_ = a_.cross(o_)
    bm = bmesh.new(); bmesh.ops.create_cube(bm, size=2.0)
    for v in bm.verts:
        v.co = c + a_*(v.co.x*hl) + w_*(v.co.y*hw) + o_*(v.co.z*ht)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return finish(from_bm(bm, 'flat'), col or PAD, rough, 0, b, 2, flag=flag)

# ------------------------------------------------------------------ support hand (HAND_L)
def hand_l():
    """Broad, muscular support hand gripping the rear-middle handguard (z ~ 0.17..0.28,
    matching zoom_sup.png):
    - Compact tapered 53 mm wrist + Velcro strap + outer-left buckle loop, flaring OUTWARD
      into a broad ~90 mm gloved palm & dorsal metacarpal above the wrist!
    - Left/dorsal half: plump slate-navy metacarpal & compact thick thumb with joint folds,
      wide bevelled charcoal knuckle pad, and rounded grey suede thumb-tip cap.
    - Right/palmar half: broad diamond-stippled charcoal leather palm cupping the visible
      left-lower 45 deg flank of the handguard right back to the barrel nut / magwell, with
      sculpted oval Thenar + Hypothenar heel cushions."""
    clear()
    W_L = V3(-.031, -.035, .198)
    AX_L = V3(-.41, -.82, -.39).normalized()   # wrist -> elbow direction

    # 1. Compact tapered wrist cuff + wide Velcro strap + outer-left silhouette buckle loop
    #    Positioned cleanly above the vambrace lip (x = .034) so strap & buckle sit 100% proud!
    cuff_ctrl = [
        (W_L + AX_L*.022, V3(-.88, .34, -.34), .0275, .0222),
        (W_L + AX_L*.004, V3(-.88, .34, -.34), .0288, .0232),
        (W_L - AX_L*.016, V3(-.86, .36, -.36), .0302, .0242),
    ]
    _, s_cuff = loft_tube(cuff_ctrl, seg_u=10, seg_v=20, palmar_flat=0.94, col=LEATHER, rough=.60, flag=7, name='l_cuff')
    shell_patch(s_cuff, 0.12, 0.78, -25.0, 145.0, thick=.0026, bury=.0010, nu=6, nv=10, rim_u=.06, rim_d=7.0,
                col=LEATHER, rough=.52, flag=7, sharp=True, name='l_strap_blk')
    shell_patch(s_cuff, 0.14, 0.76, 138.0, 340.0, thick=.0022, bury=.0010, nu=6, nv=10, rim_u=.06, rim_d=7.0,
                col=SEAM, rough=.60, flag=2, sharp=True, name='l_strap_nvy')
    # Strap buckle loop standing proud on the OUTER-LEFT wrist silhouette (deg=72 is pure -X tangent!)
    bp, bn = s_cuff(0.46, 72.0, .0046)
    flat(bp, -AX_L, bn, .0142, .0068, .0032, DARK, b=.0009, rough=.44, flag=5)
    flat(bp + bn*.0013, -AX_L, bn, .0094, .0038, .0020, BLACK, b=.0004, rough=.62, flag=4)

    # 2. Broad, muscular dark-charcoal leather palm cupping the visible left-lower 45 deg flank
    #    and underside of the handguard right back to the barrel nut / magwell front!
    palm_ctrl = [
        (V3(-.022, -.027, .196), V3(-.78, -.58, -.22), .0318, .0212),
        (V3(-.011, -.016, .216), V3(-.66, -.74, -.12), .0388, .0220),
        (V3( .001, -.009, .240), V3(-.32, -.94,  .00), .0405, .0185),
        (V3( .016, -.005, .264), V3( .35, -.93,  .00), .0370, .0145),
    ]
    _, s_palm = loft_tube(palm_ctrl, seg_u=14, seg_v=20, palmar_flat=0.82, col=LEATHER, rough=.54, flag=7, name='l_palm')

    # Sculpted rounded Hypothenar heel cushion bulging broadly to the right above the wrist!
    heel_ctrl = [
        (V3(-.014, -.028, .194), V3(-.82, -.52, -.24), .0198, .0158),
        (V3(-.006, -.015, .212), V3(-.84, -.50, -.18), .0228, .0178),
        (V3(-.004, -.006, .232), V3(-.82, -.54, -.14), .0192, .0148),
    ]
    _, s_heel = loft_tube(heel_ctrl, seg_u=12, seg_v=18, palmar_flat=0.84, col=SEAM, rough=.62, flag=4, name='l_heel')
    shell_patch(s_heel, 0.08, 0.92, -35.0, 145.0, thick=.0022, nu=7, nv=8, rim_u=.05, rim_d=8.0,
                col=LEATHER, rough=.48, flag=7, sharp=True, name='l_heel_pad')

    # Plump oval Thenar grip cushion facing the camera between the slate thumb & tan handguard!
    thenar_ctrl = [
        (V3(-.024, -.020, .206), V3(-.90, -.36, -.22), .0198, .0158),
        (V3(-.022, -.005, .228), V3(-.93, -.30, -.18), .0220, .0170),
        (V3(-.020,  .008, .252), V3(-.94, -.26, -.16), .0194, .0148),
        (V3(-.019,  .016, .272), V3(-.94, -.22, -.16), .0138, .0108),
    ]
    _, s_thenar = loft_tube(thenar_ctrl, seg_u=14, seg_v=18, palmar_flat=0.84, col=SEAM, rough=.62, flag=4, name='l_thenar')
    shell_patch(s_thenar, 0.08, 0.92, -25.0, 148.0, thick=.0024, nu=7, nv=8, rim_u=.05, rim_d=8.0,
                col=LEATHER, rough=.48, flag=7, sharp=True, name='l_thenar_pad')

    # 3. Broad Flared Dorsal Metacarpal + Compact Thick Thumb (flares OUTWARD left of wrist!)
    th_ctrl = [
        (V3(-.034, -.022, .204), V3(-.90, .35, -.25), .0208, .0182),   # CMC base flaring outside wrist
        (V3(-.037, -.006, .223), V3(-.93, .32, -.18), .0224, .0194),   # broad muscular metacarpal swell!
        (V3(-.034,  .005, .240), V3(-.94, .29, -.15), .0180, .0156),   # MCP crease fold
        (V3(-.033,  .010, .249), V3(-.95, .27, -.14), .0190, .0166),   # MCP knuckle swell
        (V3(-.0305, .016, .264), V3(-.95, .25, -.12), .0170, .0150),   # thick proximal phalanx
        (V3(-.0285, .020, .275), V3(-.95, .24, -.10), .0144, .0128),   # IP crease fold
        (V3(-.0268, .024, .287), V3(-.95, .25, -.09), .0142, .0126),   # thick distal phalanx
        (V3(-.0248, .027, .298), V3(-.94, .28, -.08), .0118, .0106),   # plump rounded thumb tip on rail
    ]
    _, s_th = loft_tube(th_ctrl, seg_u=22, seg_v=20, palmar_flat=0.82, col=SLATE, rough=.58, flag=2, name='l_thumb')
    # Wide, chunky two-tier bevelled warm-charcoal knuckle pad on proximal thumb
    shell_patch(s_th, 0.38, 0.70, 34.0, 146.0, thick=.0020, nu=6, nv=7, rim_u=.04, rim_d=7.0,
                col=PAD_RIM, rough=.52, flag=4, sharp=True, name='l_th_bezel')
    shell_patch(s_th, 0.41, 0.67, 42.0, 138.0, thick=.0036, nu=6, nv=6, rim_u=.035, rim_d=6.0,
                col=PAD, rough=.40, flag=4, sharp=True, name='l_th_pad')
    # Tailored dorsal metacarpal softshell panel + dark seam groove
    shell_patch(s_th, 0.06, 0.35, 28.0, 148.0, thick=.0014, nu=6, nv=7, rim_u=.04, rim_d=8.0,
                col=SLATE, rough=.56, flag=2, sharp=True, name='l_th_mc_panel')
    # Plump grey-charcoal suede distal thumb-tip cap (zoom_sup top of thumb!)
    shell_patch(s_th, 0.72, 0.99, -140.0, 150.0, thick=.0014, nu=6, nv=9, rim_u=.035, rim_d=8.0,
                col=TIP_CAP, rough=.52, flag=7, sharp=True, name='l_th_tipcap')
    # Dark charcoal palmar thumb seam border
    shell_patch(s_th, 0.32, 0.73, -155.0, -8.0, thick=.0013, nu=6, nv=6, rim_u=.04, rim_d=7.0,
                col=LEATHER, rough=.50, flag=7, sharp=True, name='l_th_grip')

    # 4. Four Fuller Articulated Fingers (little -> index) curling under & up the far side
    Z_F = (.214, .235, .256, .277)
    R_F = (.0102, .0110, .0114, .0108)
    Y_TIP = (.029, .033, .034, .031)
    for i in range(4):
        zf = Z_F[i]; rf = R_F[i]; yt = Y_TIP[i]
        f_ctrl = [
            (V3(-.002, -.013, zf-.005), V3( .15, -.98, 0), rf*1.12, rf*0.96),
            (V3( .017, -.009, zf),      V3( .62, -.78, 0), rf*1.08, rf*0.95),
            (V3( .028,  .004, zf+.002), V3( .94, -.34, 0), rf*1.04, rf*0.92),
            (V3( .029,  .018, zf+.003), V3( .98,  .15, 0), rf*0.96, rf*0.86),
            (V3( .022,  yt,   zf+.002), V3( .80,  .60, 0), rf*0.84, rf*0.76),
        ]
        _, s_f = loft_tube(f_ctrl, seg_u=16, seg_v=16, palmar_flat=0.80, col=SLATE, rough=.58, flag=2, name=f'l_f{i}')
        shell_patch(s_f, 0.36, 0.66, 42.0, 138.0, thick=.0016, nu=5, nv=5, rim_u=.04, rim_d=8.0,
                    col=PAD, rough=.42, flag=4, sharp=True, name=f'l_fpad{i}')
        shell_patch(s_f, 0.20, 0.96, -150.0, -20.0, thick=.0010, nu=6, nv=6, rim_u=.04, rim_d=8.0,
                    col=LEATHER, rough=.52, flag=7, name=f'l_fgrip{i}')

# ------------------------------------------------------------------ firing hand (HAND_R)
def slim(ctrl, w, t):
    return [(p, n, a*w, b*t) for p, n, a, b in ctrl]

def hand_r(fist=False, open_k=0.0):
    """Broad, muscular firing hand matching zoom_trg.png:
    - Tapered wrist flaring into a full, plump dorsal hand tucked into the receiver tang.
    - Separate broad thumb metacarpal cushion carrying a WIDE, CHUNKY ~1.55:1 bevelled
      rectangular armor plate (not a skinny strip!).
    - Short, thick, bent thumb phalanges with sculpted knuckle creases and plump rounded tip.
    - Bold 3D diamond-stippled black leather palm border + broad, snugly-packed grip fingers."""
    clear()
    W_R = V3(.018, -.080, -.066)
    AX_R = V3(.32, -.66, -.68).normalized()

    # 1. Compact tapered wrist cuff + clean Velcro strap
    cuff_ctrl = [
        (W_R + AX_R*.012, V3(-.65, .62, -.44), .0265, .0210),
        (W_R - AX_R*.004, V3(-.65, .62, -.44), .0280, .0220),
        (W_R - AX_R*.020, V3(-.62, .65, -.44), .0298, .0232),
    ]
    cuff_ctrl = slim(cuff_ctrl, .90, .92)
    _, s_cuff = loft_tube(cuff_ctrl, seg_u=10, seg_v=20, palmar_flat=0.94, col=SLATE, rough=.60, flag=2, name='r_cuff')
    shell_patch(s_cuff, 0.16, 0.78, -25.0, 205.0, thick=.0026, bury=.0010, nu=6, nv=10, rim_u=.06, rim_d=7.0,
                col=LEATHER, rough=.52, flag=7, sharp=True, name='r_strap')

    # 2. Broad, plump dorsal & palmar hand body flaring out above the wrist into the receiver tang!
    palm_ctrl = [
        (W_R - AX_R*.010,        V3(-.58,  .66, -.48), .0305, .0215),
        (V3(.005, -.055, -.044), V3(-.52,  .74, -.42), .0350, .0232),
        (V3(-.001,-.036, -.024), V3(-.44,  .82, -.36), .0325, .0202),
        (V3(-.002,-.025, -.008), V3(-.26,  .90, -.34), .0240, .0145),
    ]
    palm_ctrl = slim(palm_ctrl, .78, .82)   # the back of the hand fills the view at this range
    _, s_palm = loft_tube(palm_ctrl, seg_u=14, seg_v=20, palmar_flat=0.86, col=SEAM, rough=.64, flag=4, name='r_palm')
    # Dual tailored padded slate-navy dorsal softshell panels separated by dark seam piping!
    shell_patch(s_palm, 0.06, 0.92, 24.0, 108.0, thick=.0016, nu=7, nv=7, rim_u=.04, rim_d=7.0,
                col=SLATE, rough=.56, flag=2, sharp=True, name='r_dorsal_p1')
    shell_patch(s_palm, 0.08, 0.90, 112.0, 175.0, thick=.0015, nu=7, nv=6, rim_u=.04, rim_d=7.0,
                col=SLATE, rough=.58, flag=2, sharp=True, name='r_dorsal_p2')

    # Lower palm / hypothenar cushion filling lower backstrap cleanly
    hypo_ctrl = [
        (V3(.009, -.077, -.054), V3(-.75, -.52, -.40), .0215, .0170),
        (V3(.002, -.076, -.028), V3(-.82, -.48, -.30), .0210, .0175),
        (V3(.000, -.075, -.008), V3(-.85, -.45, -.25), .0175, .0150),
    ]
    hypo_ctrl = slim(hypo_ctrl, .82, .84)
    loft_tube(hypo_ctrl, seg_u=12, seg_v=16, palmar_flat=0.86, col=SLATE, rough=.58, flag=2, name='r_hypo')

    # Curved 3D diamond-stippled black grip leather border along LOWER palm flank (leaving slate dorsal visible above!)
    grip_panel_ctrl = [
        (V3(-.006, -.071, -.056), V3(-.90, -.28, -.28), .0185, .0145),
        (V3(-.014, -.059, -.031), V3(-.94, -.22, -.22), .0195, .0152),
        (V3(-.017, -.050, -.008), V3(-.96, -.18, -.16), .0165, .0128),
    ]
    grip_panel_ctrl = slim(grip_panel_ctrl, .82, .84)
    _, s_gp = loft_tube(grip_panel_ctrl, seg_u=14, seg_v=20, palmar_flat=0.88, col=SEAM, rough=.64, flag=4, name='r_gp_base')
    shell_patch(s_gp, 0.05, 0.95, -20.0, 158.0, thick=.0025, nu=9, nv=10, rim_u=.05, rim_d=8.0,
                col=LEATHER, rough=.48, flag=7, sharp=True, name='r_gp_leather')

    # 3A. Broad Muscular Thumb Metacarpal Cushion + Compact ~1.6:1 Rectangular Armor Plate on upper ridge!
    mc_ctrl = [
        (V3(-.006, -.044, -.044), V3(-.78, .58, -.22), .0195, .0168),
        (V3(-.013, -.033, -.026), V3(-.80, .57, -.15), .0190, .0164),
        (V3(-.018, -.029, -.010), V3(-.83, .54, -.10), .0166, .0146),
    ]
    mc_ctrl = slim(mc_ctrl, .80, .82)
    _, s_mc = loft_tube(mc_ctrl, seg_u=14, seg_v=20, palmar_flat=0.84, col=SLATE, rough=.58, flag=2, name='r_mc')
    # Compact, crisp two-tier stepped rectangular bevelled metacarpal armor plate (~21 mm x 13.5 mm!)
    shell_patch(s_mc, 0.22, 0.78, 52.0, 134.0, thick=.0020, nu=7, nv=7, rim_u=.05, rim_d=8.0,
                col=PAD_RIM, rough=.52, flag=4, sharp=True, name='r_th_bezel')
    shell_patch(s_mc, 0.27, 0.73, 59.0, 127.0, thick=.0036, nu=7, nv=7, rim_u=.045, rim_d=7.0,
                col=PAD, rough=.40, flag=4, sharp=True, name='r_th_plate')

    # 3B. Short, Plump, Bent Thumb Phalanges (bends forward at knuckle with fabric creases!)
    th_ctrl = [
        (V3(-.0195,-.0325,-.012), V3(-.88, .44, -.06), .0156, .0138),   # MCP knuckle bend
        (V3(-.0212,-.0332,-.005), V3(-.91, .40, -.01), .0138, .0122),   # knuckle crease 1
        (V3(-.0218,-.0336, .002), V3(-.92, .38,  .04), .0152, .0134),   # plump proximal phalanx swell
        (V3(-.0208,-.0345, .010), V3(-.92, .36,  .12), .0134, .0118),   # IP joint crease 2
        (V3(-.0192,-.0352, .016), V3(-.90, .36,  .22), .0140, .0124),   # plump distal phalanx
        (V3(-.0155,-.0362, .023), V3(-.86, .36,  .34), .0118, .0106),   # meaty rounded thumb tip!
    ]
    if open_k > 0.0:
        # OPENING (the knife toss): the thumb swings out of the wrap, away from the
        # palm and forward, more toward its tip, so the hand reads as letting go.
        n_th = len(th_ctrl)
        th_ctrl = [(p + V3(-.012, .004, .010) * (open_k * (i / (n_th - 1)) ** 1.2), u, a, b)
                   for i, (p, u, a, b) in enumerate(th_ctrl)]
    _, s_th = loft_tube(th_ctrl, seg_u=18, seg_v=20, palmar_flat=0.82, col=SLATE, rough=.58, flag=2, name='r_thumb')

    # Dark charcoal diamond-leather palmar thumb pad + lower seam line visible in zoom_trg!
    shell_patch(s_th, 0.10, 0.98, -150.0, 32.0, thick=.0015, nu=7, nv=8, rim_u=.04, rim_d=7.0,
                col=LEATHER, rough=.48, flag=7, sharp=True, name='r_th_leather')

    # 4. Fuller Index / Trigger Finger curling visibly across trigger inside the open trigger guard!
    # (fist=True closes it round the knife handle with the other three instead)
    idx_ctrl = [
        (V3( .015, -.025, -.004), V3( .92, .34, .18), .0116, .0104),
        (V3( .020, -.027,  .016), V3( .96, .20, .16), .0112, .0100),
        (V3( .018, -.029,  .035), V3( .92, .10, .36), .0108, .0096),
        (V3( .012, -.030,  .050), V3( .72, .06, .69), .0104, .0092),
        (V3( .001, -.0305, .054), V3( .10, .05, .99), .0102, .0090),
        (V3(-.007, -.031,  .046), V3(-.52, .05, .85), .0098, .0088),
        (V3(-.0115,-.0315, .037), V3(-.84, .05, .54), .0090, .0080),
    ]
    if not fist:
        _, s_idx = loft_tube(idx_ctrl, seg_u=18, seg_v=16, palmar_flat=0.84, col=SLATE, rough=.58, flag=2, name='r_index')
        shell_patch(s_idx, 0.42, 0.99, -155.0, 100.0, thick=.0014, nu=6, nv=8, rim_u=.04, rim_d=8.0,
                    col=TIP_CAP, rough=.50, flag=7, name='r_idx_pad')

    # 5. Broad, Snugly-Packed Middle, Ring & Little Fingers wrapping frontstrap (NO tiny beads!)
    F_SPEC = ((-.035, .026, .0114),) * bool(fist) + (
        (-.053, .018, .0116),   # full middle finger tucked right under trigger guard bow
        (-.071, .012, .0110),   # full ring finger
        (-.088, .006, .0102),   # full little finger
    )
    for i, (yf, zfs, rf) in enumerate(F_SPEC):
        f_ctrl = [
            (V3( .018, yf+.005, zfs-.024), V3( .96, .16, -.20), rf*1.08, rf*0.96),
            (V3( .016, yf+.001, zfs+.004), V3( .82, .05,  .56), rf*1.04, rf*0.94),
            (V3( .000, yf-.001, zfs+.010), V3( .00, .00,  1.0), rf*1.02, rf*0.92),
            (V3(-.0168,yf,      zfs+.005), V3(-.82, .05,  .56), rf*1.00, rf*0.90),
            (V3(-.0198,yf+.002, zfs-.010), V3(-.95, .12, -.28), rf*0.90, rf*0.82),
        ]
        if open_k > 0.0:
            # OPENING: each finger uncurls from its wrap round the handle into a
            # straight, slightly cupped finger running forward off its knuckle,
            # the back of the finger still on the dorsal side. Same five stations,
            # so the half-open hand is a true in-between, not a different sculpt.
            K = f_ctrl[0][0]
            o_ctrl = [
                (K,                              V3(.96, .16, -.20), rf*1.08, rf*0.96),
                (K + V3(-.002, -.003, .026),     V3(.97, .12,  .02), rf*1.04, rf*0.94),
                (K + V3(-.006, -.006, .047),     V3(.95, .10,  .20), rf*1.02, rf*0.92),
                (K + V3(-.012, -.008, .064),     V3(.90, .08,  .40), rf*1.00, rf*0.90),
                (K + V3(-.018, -.009, .076),     V3(.84, .08,  .52), rf*0.90, rf*0.82),
            ]
            f_ctrl = [(a[0].lerp(b[0], open_k), a[1].lerp(b[1], open_k).normalized(),
                       a[2] + (b[2] - a[2]) * open_k, a[3] + (b[3] - a[3]) * open_k)
                      for a, b in zip(f_ctrl, o_ctrl)]
        _, s_f = loft_tube(f_ctrl, seg_u=16, seg_v=16, palmar_flat=0.76, power=2.35, col=SLATE, rough=.58, flag=2, name=f'r_f{i}')
        # Broad squared dark-charcoal leather fingertip/front pad + seam visible on left of grip!
        shell_patch(s_f, 0.34, 0.99, -155.0, 68.0, thick=.0015, nu=6, nv=8, rim_u=.04, rim_d=7.0,
                    col=LEATHER, rough=.48, flag=7, sharp=True, name=f'r_fgrip{i}')
        shell_patch(s_f, 0.20, 0.54, 48.0, 134.0, thick=.0016, nu=5, nv=5, rim_u=.04, rim_d=8.0,
                    col=PAD, rough=.42, flag=4, sharp=True, name=f'r_fpad{i}')

# ------------------------------------------------------------------ forearm armour (BRACER)
def bracer():
    """Sturdy, muscular faceted forearm gauntlet matching zoom_arm.png:
    frame: x = wrist (0) toward elbow (+x), y = dorsal face toward eye, z = across arm."""
    clear()
    def arm_tube(rings, col, rough, seg=32, flag=0, b=0.0):
        bm = bmesh.new(); rv = []; off = math.pi/seg
        for x, ry, rz in rings:
            rv.append([bm.verts.new((x, ry*cos(2*math.pi*i/seg + off), rz*sin(2*math.pi*i/seg + off))) for i in range(seg)])
        for k in range(len(rv)-1):
            for i in range(seg):
                j = (i+1) % seg
                bm.faces.new((rv[k][i], rv[k][j], rv[k+1][j], rv[k+1][i]))
        bm.faces.new(rv[0][::-1]); bm.faces.new(rv[-1])
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        return finish(from_bm(bm, 'arm'), col, rough, 0, b, 2, flag=flag)

    # 1. Clean wrist crease (x = 0..034) so the glove's Velcro strap & buckle sit 100% clear!
    arm_tube([
        (-.004, .0220, .0268),
        ( .030, .0275, .0335),
        ( .090, .0385, .0450),
        ( .200, .0465, .0535),
        ( .380, .0515, .0585),
    ], SLEEVE, .78, seg=36, flag=1)

    arm_tube([
        (.024, .0272, .0332),
        (.029, .0292, .0352),
        (.034, .0292, .0352),
    ], SLEEVE, .68, seg=32, flag=1, b=.0005)

    # 2. Crisp planar-faceted dorsal vambrace shell (43 deg facet creases + harden_normals!)
    base_prof = [
        (-.040, -.008), (-.043, .003), (-.035, .021), (-.0185, .0365),
        ( .0185, .0365), ( .035, .021), ( .043, .003), ( .040, -.008),
        ( .028, -.011), ( .028, .003), ( .015, .021), (-.015,  .021),
        (-.028,  .003), (-.028,-.011),
    ]
    stations = [
        (.031, 0.84, 0.86),
        (.038, 0.94, 0.96),
        (.067, 1.02, 1.04),
        (.070, 0.96, 0.98),   # crisp seam groove 1 between Module 1 & Module 2
        (.073, 1.04, 1.06),
        (.115, 1.12, 1.14),
        (.118, 1.05, 1.07),   # crisp seam groove 2 between Module 2 & Module 3
        (.121, 1.14, 1.16),
        (.255, 1.26, 1.28),
        (.385, 1.33, 1.35),
    ]
    bm = bmesh.new(); rv = []; np = len(base_prof)
    for x, sy, sz in stations:
        rv.append([bm.verts.new((x, y*sy, z*sz)) for z, y in base_prof])
    for k in range(len(rv) - 1):
        for i in range(np):
            j = (i + 1) % np
            bm.faces.new((rv[k][i], rv[k+1][i], rv[k+1][j], rv[k][j]))
    bm.faces.new(rv[0]); bm.faces.new(rv[-1][::-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    finish(from_bm(bm, 'vambrace'), HARD, .52, .08, bevel=.0011, seg=2, flag=5, harden=True)

    # Exact piecewise-linear top-surface height y_top(x) so dorsal modules never sink inside!
    def y_top(x):
        for k in range(len(stations) - 1):
            x0, s0, _ = stations[k]; x1, s1, _ = stations[k + 1]
            if x <= x1 or k == len(stations) - 2:
                t = max(0.0, min(1.0, (x - x0) / max(1e-6, x1 - x0)))
                return .0365 * (s0 + (s1 - s0) * t)
        return .0365

    def slope_dir(x):
        dy = y_top(x + .018) - y_top(x - .018)
        return Vector((.036, dy, 0)).normalized(), Vector((-dy, .036, 0)).normalized()

    # 3. Module 1 (near wrist, x = .036..066): Dark bevelled bezel + glowing warm amber window
    a1, n1 = slope_dir(.051); y1 = y_top(.051)
    flat((.051, y1 + .0013, 0), a1, n1, .0145, .0158, .0020, BEZEL, b=.0008, rough=.50, flag=4)
    flat((.051, y1 + .0024, 0), a1, n1, .0116, .0126, .0013, BLACK, b=.0004, rough=.56, flag=4)
    flat((.051, y1 + .0032, 0), a1, n1, .0098, .0108, .0009, AMBER, b=.0002, rough=.30, flag=3)

    # 4. Module 2 (middle, x = .074..114): Landscape stepped bronze-tan metallic plate (zoom_arm center!)
    a2, n2 = slope_dir(.094); y2 = y_top(.094)
    flat((.094, y2 + .0015, 0), a2, n2, .0190, .0228, .0022, DARK,      b=.0008, rough=.48, flag=5)
    flat((.094, y2 + .0031, 0), a2, n2, .0168, .0208, .0019, PLATE_TAN, b=.0009, rough=.44, flag=6)
    flat((.094, y2 + .0045, 0), a2, n2, .0134, .0168, .0009, SAND,      b=.0005, rough=.42, flag=6)

    # 5. Module 3 (lower forearm, x = .122..310): Twin raised faceted rails + bold glowing orange bar!
    a3, n3 = slope_dir(.206); y3 = y_top(.206)
    flat((.206, y3 + .0013, 0), a3, n3, .0840, .0084, .0015, BLACK,  b=.0004, rough=.56, flag=4)
    for sz in (-1, 1):
        flat((.206, y3 + .0025, sz*.0148), a3, n3, .0840, .0068, .0024, HARD, b=.0009, rough=.48, flag=5)
    flat((.200, y3 + .0028, 0), a3, n3, .0750, .0048, .0012, ORANGE, b=.0002, rough=.28, flag=3)

def sniper():
    """Accuracy International Arctic Warfare Police (AWP / AWM): the iconic one-piece
    OD-green polymer thumbhole stock bolted around an internal aluminum spine, smooth
    rectangular-rounded AW fore-end with side vents and flush front bipod spigot,
    boxy flat-sided matte-black AI receiver, swept AI bolt handle with round ball
    knob (group 3, no glowing orange bulb), heavy fluted barrel and AI 2-port
    rectangular muzzle brake, matte-black Schmidt & Bender PM II tactical scope
    (axis y .085, ocular z -.052) in a one-piece mount, and the 10-round AI steel
    box magazine (group 1). All sniper slot stations preserved exactly."""
    clear()
    LENS = (.24, .34, .41)

    def rod_z(rings, x=0.0, y=0.0, seg=20, name='rodz'):
        bm = bmesh.new(); rv = []
        for z, r in rings:
            rv.append([bm.verts.new((x + r*cos(2*math.pi*i/seg), y + r*sin(2*math.pi*i/seg), z)) for i in range(seg)])
        for k in range(len(rv)-1):
            for i in range(seg):
                j = (i+1) % seg
                bm.faces.new((rv[k][i], rv[k][j], rv[k+1][j], rv[k+1][i]))
        bm.faces.new(rv[0][::-1]); bm.faces.new(rv[-1])
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        return from_bm(bm, name)

    def rod_x(x0, x1, y, z, r, seg=18, name='rodx'):
        return prof([(z + r*cos(2*math.pi*i/seg), y + r*sin(2*math.pi*i/seg)) for i in range(seg)], x0, x1, name, r=0)

    def rod_y(y0, y1, x, z, r, seg=18, name='rody'):
        bm = bmesh.new()
        pts = [(x + r*cos(2*math.pi*i/seg), z + r*sin(2*math.pi*i/seg)) for i in range(seg)]
        a = [bm.verts.new((px, y0, pz)) for px, pz in pts]
        b = [bm.verts.new((px, y1, pz)) for px, pz in pts]
        bm.faces.new(a[::-1]); bm.faces.new(b)
        for i in range(seg):
            j = (i+1) % seg
            bm.faces.new((a[i], a[j], b[j], b[i]))
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        return from_bm(bm, name)

    def obox(c, h, rx=0.0, ry=0.0, rz=0.0, name='obox'):
        o = box((0, 0, 0), h, name)
        o.rotation_euler = (rx, ry, rz); o.location = c
        bpy.context.view_layer.objects.active = o; o.select_set(True)
        bpy.ops.object.transform_apply(location=True, rotation=True); o.select_set(False)
        return o

    def place(o, c, rx=0.0, ry=0.0, rz=0.0):
        o.rotation_euler = (rx, ry, rz); o.location = c
        bpy.context.view_layer.objects.active = o; o.select_set(True)
        bpy.ops.object.transform_apply(location=True, rotation=True); o.select_set(False)
        return o

    # --- 1. One-piece OD-green AW polymer chassis + smooth AW fore-end (group 0)
    # Continuous rectangular-rounded polymer body from the wrist (z -.082) forward
    # through the magwell and smooth, flat-sided AW fore-end to z .316 (hand at z .208).
    ch = prof([
        (-.082, .0260), (.110, .0260), (.308, .0235), (.318, .0160),
        (.318, -.0010), (.306, -.0065), (.106, -.0115), (.088, -.0240),
        (.020, -.0240), (.016, -.0135), (-.044, -.0135), (-.082, -.0130)
    ], -.0188, .0188, 'aw_chassis', r=.0048)
    cut(ch, box((0, -.018, .052), (.0130, .030, .0280)))            # magazine well
    # Subtle AW recessed side-slot grooves along the fore-end flanks + chassis screw pockets
    for s_ in (-1, 1):
        for i in range(4):
            cut(ch, box((s_ * .0190, .0090, .148 + i * .038), (.0026, .0036, .0110)))
    finish(ch, TAN, .68, 0, .0016, 2, flag=4)
    # Internal dark aluminum chassis seam along top and underside of stock halves
    finish(box((0, -.0062, .206), (.0060, .0016, .1000)), BLACK, .55, .15, .0004, 1)
    # Recessed black Allen bolts holding the two AW polymer stock skins together
    for z, y in ((-.068, .0090), (.002, .0060), (.104, .0080), (.286, .0080)):
        finish(rod_x(-.0192, .0192, y, z, .0030, 14), BLACK, .48, .2, .0002, 1)
    # Flush steel bipod spigot protruding from the AW fore-end nose (signature AW detail)
    finish(rod_z([(.314, .0062), (.338, .0062), (.341, .0048)], 0, .0035, 22), DARK, .44, .4, .0004, 1)
    finish(box((0, .0035, .316), (.0110, .0070, .0040)), BLACK, .50, .2, .0006, 1)
    # Subtle front QD sling cup on left side of fore-end
    finish(rod_x(-.0196, -.0172, .0090, .296, .0042, 16), BLACK, .48, .2, .0003, 1)
    # Integrated OD-green thumbhole pistol grip (at exact SR_GRIP station!) + dark grip panel
    finish(prof([(-.0113, -.014), (-.044, -.014), (-.086, -.100), (-.080, -.110),
                 (-.052, -.106), (-.040, -.066)], -.0134, .0134, 'grip', r=.0055), TAN, .72, 0, .0036, 3, flag=4)
    for s_ in (-1, 1):
        x0, x1 = sorted((s_ * .0130, s_ * .0140))
        finish(prof([(-.024, -.026), (-.042, -.026), (-.076, -.096), (-.056, -.096), (-.042, -.062)], x0, x1, 'gtex', r=0),
               SAND, .88, 0, .0004, 1, flag=4)
    # Matte black AI trigger guard, blade, and paddle mag release ahead of guard
    finish(prof([(-.020, -.0135), (-.015, -.0135), (-.015, -.0465), (.023, -.0465), (.029, -.024),
                 (.034, -.024), (.026, -.0512), (-.020, -.0512)], -.0046, .0046, 'guard', r=0), BLACK, .56, .1, .0007, 2)
    finish(prof([(.0052, -.018), (.0096, -.018), (.0088, -.0405), (.0046, -.0405)], -.0028, .0028, 'trig', r=0), STEEL, .34, .85, .0004, 1)
    finish(prof([(.019, -.024), (.022, -.024), (.020, -.038), (.0165, -.038)], -.0048, .0048, 'mrel', r=0), BLACK, .52, .1, .0005, 1)

    # --- 2. Iconic one-piece OD-green AW THUMBHOLE BUTTSTOCK (group 2)
    # Thick solid polymer butt with signature thumbhole linking straight into the grip heel,
    # rear bottom monopod/hand-hook cutout, LOP spacers, ribbed buttpad & black adjustable cheekpiece.
    st = prof([
        (-.078, .0260), (-.244, .0260), (-.248, .0180), (-.248, -.0840), (-.236, -.0900),
        (-.214, -.0900), (-.204, -.0680), (-.182, -.0680), (-.172, -.0960),
        (-.082, -.1100), (-.078, -.1000), (-.078, -.0130)
    ], -.0182, .0182, 'aw_butt', r=.0052)
    # The signature large rounded-trapezoidal AWP thumbhole cutout!
    cut(st, prof([
        (-.084, .0080), (-.166, .0080), (-.172, -.0040), (-.164, -.0720),
        (-.150, -.0840), (-.088, -.0920), (-.080, -.0800), (-.072, -.0280)
    ], -.030, .030))
    finish(st, TAN, .68, 0, .0022, 3, group=2, flag=4)
    # Recessed black bolts & rear sling loop on buttstock
    for z, y in ((-.190, .0060), (-.228, -.0540)):
        finish(rod_x(-.0188, .0188, y, z, .0028, 14), BLACK, .50, .2, .0002, 1, group=2)
    finish(rod_x(-.0192, -.0168, -.0200, -.230, .0042, 16), BLACK, .50, .2, .0003, 1, group=2)
    # Matte-black contoured adjustable AI cheekpiece on twin steel posts
    cp = prof_front([
        (-.0192, .0220), (-.0192, .0385), (-.0125, .0465), (.0125, .0465),
        (.0192, .0385), (.0192, .0220), (.0145, .0220), (.0145, .0350),
        (-.0145, .0350), (-.0145, .0220)
    ], -.226, -.126, 'cheek')
    finish(cp, BLACK, .74, 0, .0016, 2, group=2, flag=4)
    for z_post in (-.148, -.204):
        finish(rod_y(.0160, .0420, 0, z_post, .0034, 14), STEEL, .36, .85, .0003, 1, group=2)
        finish(rod_x(-.0204, -.0160, .0160, z_post, .0048, 16), BLACK, .48, .2, .0004, 1, group=2)
    # AI LOP spacer plates + ribbed matte-black rubber recoil buttpad
    finish(box((0, -.0310, -.2505), (.0174, .0545, .0035)), DARK, .64, 0, .0008, 2, group=2)
    finish(box((0, -.0310, -.2595), (.0180, .0565, .0060)), BLACK, .90, 0, .0020, 2, group=2)

    # --- 3. Boxy flat-sided Accuracy International receiver & top Picatinny rail
    act = prof_front([
        (-.0158, .0240), (.0158, .0240), (.0158, .0385), (.0118, .0440),
        (-.0118, .0440), (-.0158, .0385)
    ], -.080, .110, 'action')
    cut(act, box((.0135, .0345, .032), (.0065, .0062, .0330)))     # right ejection port
    finish(act, GRAPH, .48, .25, .0010, 2)
    # Action top dovetail / Picatinny rail
    finish(box((0, .0454, .016), (.0094, .0022, .0920)), BLACK, .52, .15, .0004, 1)
    for i in range(18):
        finish(box((0, .0484, -.068 + .00995 * i), (.0102, .0014, .0026)), BLACK, .48, .15, .0003, 1)

    # --- 4. Live-cycling AI bolt, matte-black shroud & swept round-ball knob (group 3)
    # NO glowing neon orange indicator! Real AI bolt has a matte black safety flag & shroud.
    by_ = .0340
    finish(rod_z([(-.076, .0088), (.068, .0088)], 0, by_, 24, 'bolt_body'), STEEL, .32, .88, .0004, 1, group=3)
    finish(rod_z([(-.091, .0086), (-.087, .0108), (-.075, .0108), (-.073, .0090)], 0, by_, 22), BLACK, .54, .1, .0006, 2, group=3)
    finish(box((0, by_ - .0055, -.083), (.0058, .0052, .0065)), BLACK, .56, 0, .0007, 2, group=3)
    # 3-position AI wing safety tab on right rear of shroud (matte dark steel, not glowing)
    finish(box((.0110, by_ + .0010, -.082), (.0042, .0020, .0048)), DARK, .48, .3, .0004, 1, group=3)
    # Swept AI bolt handle stem + classic AI round black polymer ball knob at BOLT_KNOB_Z (-.037)
    finish(obox((.0185, .0292, -.0368), (.0115, .0034, .0034), rz=-.42), DARK, .42, .4, .0006, 2, group=3)
    bm_k = bmesh.new(); bmesh.ops.create_uvsphere(bm_k, u_segments=20, v_segments=14, radius=.0096)
    kn = from_bm(bm_k, 'bolt_ball')
    place(kn, (.0328, .0222, -.0370))
    finish(kn, BLACK, .68, 0, 0, 1, group=3)

    # --- 5. Heavy free-float barrel & iconic Accuracy International 2-port rectangular brake (muzzle .506)
    brl = lathe([(.110, .0128), (.316, .0116), (.454, .0106)], 0, .0160, 32, 'barrel')
    for i in range(6):
        a_ = i * math.pi / 3 + math.pi / 6
        cut(brl, rod_z([(.328, .0024), (.444, .0022)], .0110 * cos(a_), .0160 + .0110 * sin(a_), 12))
    finish(brl, GRAPH, .42, .35, .0004, 1)
    # Classic AI flat-sided 2-chamber tactical muzzle brake
    mz = prof_front([
        (-.0142, .0068), (-.0110, .0044), (.0110, .0044), (.0142, .0068),
        (.0142, .0252), (.0110, .0276), (-.0110, .0276), (-.0142, .0252)
    ], .454, .506, 'ai_brake')
    for zc in (.470, .490):
        cut(mz, box((0, .0160, zc), (.0200, .0072, .0058)))
    cut(mz, rod_z([(.496, .0056), (.510, .0058)], 0, .0160, 20))
    finish(mz, BLACK, .46, .25, .0006, 2)

    # --- 6. Schmidt & Bender PM II-style 34 mm black tactical scope & one-piece AI mount (sy = 0.085)
    sy = .085
    mnt = box((0, sy - .0290, .033), (.0138, .0095, .0450))
    cut(mnt, box((0, sy - .0275, .033), (.0200, .0052, .0210)))
    finish(mnt, BLACK, .50, .15, .0010, 2)
    for zb in (-.004, .010, .056, .070):
        finish(rod_x(-.0168, -.0132, sy - .0335, zb, .0024, 12), STEEL, .36, .85, .0002, 1)
    for z in (-.002, .066):
        finish(rod_z([(z - .0088, .0182), (z + .0088, .0182)], 0, sy, 32), BLACK, .48, .15, .0008, 2)
        for s_ in (-1, 1):
            finish(obox((s_ * .0136, sy + .0130, z), (.0040, .0028, .0082), rz=-s_ * .72), BLACK, .48, .15, .0005, 1)
    sc = lathe([
        (-.056, .0196), (-.052, .0205), (-.030, .0205), (-.022, .0164),
        (-.010, .0152), (.092, .0152), (.118, .0222), (.172, .0222), (.175, .0210)
    ], 0, sy, 44, 'scope_tube')
    cut(sc, rod_z([(-.060, .0183), (-.050, .0183)], 0, sy, 36))
    cut(sc, rod_z([(.158, .0196), (.178, .0196)], 0, sy, 36))
    finish(sc, BLACK, .42, .1, .0006, 2)
    finish(rod_z([(-.0552, .0188), (-.0520, .0188)], 0, sy, 32), STEEL, .32, .85, 0)
    finish(rod_z([(-.0541, .0003), (-.0536, .0105), (-.0522, .0182)], 0, sy, 32), LENS, .15, 0, 0)
    finish(rod_z([(.1585, .0198), (.1618, .0198)], 0, sy, 32), STEEL, .32, .85, 0)
    finish(rod_z([(.1588, .0194), (.1604, .0110), (.1610, .0003)], 0, sy, 32), LENS, .14, 0, 0)
    # Magnification ring & 3 tactical MIL turrets (all realistic matte black/dark)
    finish(rod_z([(-.0240, .0170), (-.0135, .0170)], 0, sy, 30), DARK, .48, .1, .0005, 2)
    tz = .033
    finish(rod_z([(tz - .014, .0172), (tz + .014, .0172)], 0, sy, 28), BLACK, .48, .1, .0008, 2)
    finish(rod_y(sy + .014, sy + .0350, 0, tz, .0110, 24), BLACK, .46, .1, .0008, 2)
    finish(rod_x(.014, .0315, sy, tz, .0094, 20), BLACK, .46, .1, .0008, 2)
    finish(rod_x(-.0325, -.014, sy, tz, .0102, 22), BLACK, .46, .1, .0008, 2)
    finish(rod_x(-.0345, -.0322, sy, tz, .0068, 18), DARK, .46, .15, .0004, 1)

    # --- 7. Stamped 10-round steel Accuracy International AICS box magazine (group 1)
    mag = prof([(.078, -.018), (.026, -.018), (.024, -.103), (.076, -.103)], -.0118, .0118, 'mag')
    finish(mag, GRAPH, .60, .25, .0016, 2, group=1)
    for zr in (.038, .052, .066):
        finish(box((0, -.064, zr), (.0124, .0320, .0022)), DARK, .62, .25, .0005, 1, group=1)
    finish(prof([(.022, -.101), (.082, -.101), (.084, -.111), (.021, -.112)], -.0132, .0132, 'magplate'), BLACK, .76, 0, .0012, 2, group=1)

# ------------------------------------------------------------------ SCAR-20 (sniper slot, variant 1)
def _rz(rings, x=0.0, y=0.0, seg=20, name='rodz', loop=False):
    """Round part along z with no minimum segment count (lathe() forces 44).
    loop=True closes the profile on itself (a tyre) instead of capping it."""
    bm = bmesh.new(); rv = []
    for z, r in rings:
        rv.append([bm.verts.new((x + r*cos(2*math.pi*i/seg), y + r*sin(2*math.pi*i/seg), z)) for i in range(seg)])
    for k in range(len(rv) - (0 if loop else 1)):
        k1 = (k + 1) % len(rv)
        for i in range(seg):
            j = (i+1) % seg
            bm.faces.new((rv[k][i], rv[k][j], rv[k1][j], rv[k1][i]))
    if not loop:
        bm.faces.new(rv[0][::-1]); bm.faces.new(rv[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return from_bm(bm, name)

def _rx(x0, x1, y, z, r, seg=18, name='rodx'):
    return prof([(z + r*cos(2*math.pi*i/seg), y + r*sin(2*math.pi*i/seg)) for i in range(seg)], x0, x1, name)

def _ry(y0, y1, x, z, r, seg=18, name='rody'):
    bm = bmesh.new()
    pts = [(x + r*cos(2*math.pi*i/seg), z + r*sin(2*math.pi*i/seg)) for i in range(seg)]
    a = [bm.verts.new((px, y0, pz)) for px, pz in pts]
    b = [bm.verts.new((px, y1, pz)) for px, pz in pts]
    bm.faces.new(a[::-1]); bm.faces.new(b)
    for i in range(seg):
        j = (i+1) % seg
        bm.faces.new((a[i], a[j], b[j], b[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return from_bm(bm, name)

def scar20():
    """FN SCAR-20S (Mk 20 SSR) precision rifle after real-life reference photographs:
    flat-dark-earth anodized monolithic upper & extended railed handguard (`TAN`),
    slightly deeper coyote/FDE polymer lower & SSR precision stock (`SAND`), matte
    black Hogue finger-grooved pistol grip, matte black cheekpiece saddle, buttpad,
    controls, folded flip-up sights & barrel/flash hider, dark FDE/coyote precision
    scope in a one-piece mount, and a 20-round 7.62 steel magazine. All sniper slot
    stations preserved identically."""
    clear()
    LENS = (.24, .34, .41)
    FDE = TAN                      # factory FDE upper/handguard (skinned as light family!)
    FDE_L = SAND                   # lower receiver & SSR stock frame, a shade deeper
    FURN = (.40, .35, .28)         # dark coyote alloy hardware / mount
    RUB = (.14, .14, .148)         # matte black rubber / polymer furniture (grip, pad, cheek, controls)
    SCOPE = (.34, .30, .24)        # matte dark-earth/bronze scope body
    EYE = (.15, .15, .16)          # matte black rubber ocular collar
    MAGC = (.46, .40, .32)         # matte FDE 7.62 steel mag
    DK = (.18, .18, .188)

    def place(o, c, rx=0.0, ry=0.0, rz=0.0):
        o.rotation_euler = (rx, ry, rz); o.location = c
        bpy.context.view_layer.objects.active = o; o.select_set(True)
        bpy.ops.object.transform_apply(location=True, rotation=True); o.select_set(False)
        return o

    # --- 1. Upper receiver + extended SSR handguard, one monolithic extrusion (z -.068 .. .360)
    up = prof([(-.068, .0020), (-.068, .0400), (-.064, .0436), (.356, .0436), (.360, .0405),
               (.360, -.0010), (.356, -.0050), (.128, -.0050), (.122, .0020)], -.0168, .0168, 'upper')
    for s_ in (-1, 1):
        cut(up, box((s_*.0168, .0322, .294), (.0040, .0062, .0320)))     # gas-block side windows
        cut(up, box((s_*.0168, .0335, .094), (.0012, .0016, .0520)))     # upper lightening slot
    cut(up, box((-.0168, .0150, .061), (.0020, .0042, .0490)))           # charging-handle channel (left)
    cut(up, box((.0170, .0210, .068), (.0040, .0068, .0260)))            # right ejection port
    cut(up, box((0, .0160, .360), (.0080, .0080, .0060)))                # barrel exit
    finish(up, FDE, .56, 0, .0012, 2, flag=6)
    for s_ in (-1, 1):
        finish(box((s_*.0129, .0322, .294), (.0002, .0061, .0319)), BLACK, .60, 0, 0)
    finish(box((-.0149, .0150, .061), (.0002, .0041, .0489)), BLACK, .60, 0, 0)
    finish(box((.0135, .0210, .068), (.0015, .0064, .0250)), STEEL, .38, .85, .0002, 1)  # bolt face
    # Brass deflector behind right ejection port
    finish(prof([(.034, .0155), (.046, .0155), (.046, .0265), (.038, .0265)], .0165, .0215, 'deflector', r=0), BLACK, .55, 0, .0006, 1)
    # Gas piston seen through windows
    finish(_rz([(.250, .0052), (.352, .0052)], 0, .0322, 18), DK, .45, .3, .0004, 1)
    finish(box((0, .0322, .262), (.0100, .0050, .0040)), BLACK, .50, .2, .0004, 1)
    # Full-length top rail
    finish(box((0, .0450, .147), (.0090, .0016, .2090)), FURN, .55, 0, .0004, 1)
    for i in range(42):
        finish(box((0, .0478, -.058 + .00995*i), (.0098, .0014, .0029)), FDE, .56, 0, .0004, 1, flag=6)
    # Side & bottom rails on handguard
    for s_ in (-1, 1):
        rl = box((s_*.0186, .0150, .283), (.0020, .0060, .0730), 'srail')
        for i in range(14):
            cut(rl, box((s_*.0206, .0150, .218 + .0100*i), (.0012, .0070, .0018)))
        finish(rl, FDE, .56, 0, .0005, 1, flag=6)
        for zb in (.236, .282, .328):
            finish(_rx(*sorted((s_*.0196, s_*.0212)), .0150, zb, .0019, 12), BLACK, .50, .2, 0)
    br = box((0, -.0062, .243), (.0085, .0014, .1110), 'brail')
    for i in range(22):
        cut(br, box((0, -.0078, .138 + .0100*i), (.0100, .0010, .0018)))
    finish(br, FDE, .56, 0, .0004, 1, flag=6)
    # Authentic SCAR Torx barrel-retention fasteners along the upper flanks (no fake orange plates!)
    for s_ in (-1, 1):
        for z, y in ((.236, .0360), (.246, .0360), (.170, .0330), (.180, .0330), (-.050, .0300),
                     (.110, .0060), (.345, .0050), (.345, .0380)):
            finish(_rx(*sorted((s_*.0164, s_*.0176)), y, z, .0018, 12), DK, .50, .3, 0)
        pl = prof([(.118, .0115), (.166, .0115), (.170, .0150), (.166, .0185), (.118, .0185), (.114, .0150)],
                  *sorted((s_*.0164, s_*.0176)), 'torxplate', r=0)
        finish(pl, FURN, .52, .1, .0004, 1)
        for k in range(3):
            finish(_rx(*sorted((s_*.0170, s_*.0182)), .0150, .128 + k*.014, .0020, 12), BLACK, .48, .3, 0)
    # Front sling plate, left
    sp = box((-.0184, .0330, .340), (.0018, .0080, .0150), 'sling')
    for zz in (.332, .348):
        cut(sp, _rx(-.03, .03, .0330, zz, .0028, 12))
    finish(sp, DK, .50, .3, .0006, 1)
    # Folded flip-up sights on the rail (matte black)
    finish(box((0, .0506, .252), (.0068, .0024, .0120)), RUB, .58, .1, .0006, 1)
    finish(box((0, .0498, .240), (.0074, .0018, .0040)), DK, .50, .2, .0004, 1)
    finish(box((0, .0506, -.047), (.0068, .0024, .0110)), RUB, .58, .1, .0006, 1)
    finish(box((0, .0498, -.058), (.0074, .0018, .0040)), DK, .50, .2, .0004, 1)
    # Rear receiver stock adapter plate (dark coyote / black)
    finish(box((0, .0150, -.0670), (.0150, .0270, .0040)), FURN, .56, .1, .0008, 2)

    # --- 2. Lower receiver, magwell, controls (z -.064 .. .124)
    lo = prof([(-.064, .0030), (.124, .0030), (.124, -.0060), (.108, -.0200), (.094, -.0300),
               (.020, -.0300), (.014, -.0160), (-.040, -.0160), (-.060, -.0120), (-.064, -.0040)],
              -.0160, .0160, 'lower')
    cut(lo, box((0, -.0200, .052), (.0122, .0120, .0270)))
    finish(lo, FDE_L, .64, 0, .0012, 2, flag=4)
    for s_ in (-1, 1):
        for z, y in ((-.052, -.006), (.002, -.008), (.104, -.012)):
            finish(_rx(*sorted((s_*.0156, s_*.0170)), y, z, .0020, 12), DK, .50, .2, 0)
        finish(_rx(*sorted((s_*.0158, s_*.0178)), -.0060, -.006, .0040, 16), DK, .48, .2, .0003)
        finish(place(box((0, 0, 0), (.0012, .0026, .0085)), (s_*.0184, -.0045, -.0045), rx=-.6), RUB, .58, .1, .0005, 1)
    finish(box((-.0168, -.0010, .012), (.0012, .0040, .0028)), RUB, .55, .2, .0004, 1)
    finish(prof([(.019, -.024), (.022, -.024), (.020, -.038), (.0165, -.038)], -.0048, .0048, r=0), RUB, .55, .2, .0005, 1)

    # --- 3. Matte-black Hogue finger-grooved pistol grip and trigger group
    gr = prof([(-.0113, -.014), (-.044, -.014), (-.086, -.100), (-.080, -.110), (-.052, -.106), (-.040, -.066)],
              -.0126, .0126, 'grip')
    for (z0, y0), (z1, y1), ts in (((-.0113, -.014), (-.040, -.066), (.45, .80)), ((-.040, -.066), (-.052, -.106), (.45,))):
        dz, dy = z1 - z0, y1 - y0; L = math.hypot(dz, dy); nz, ny = dy / L, -dz / L
        if nz < 0: nz, ny = -nz, -ny
        for t in ts:
            cut(gr, _rx(-.03, .03, y0 + dy*t + ny*.0100, z0 + dz*t + nz*.0100, .0094, 20))
    finish(gr, RUB, .86, 0, .0040, 4, flag=4)
    finish(prof([(-.083, -.104), (-.049, -.101), (-.047, -.111), (-.082, -.114)], -.0140, .0140), RUB, .80, 0, .0012, 2, flag=4)
    finish(prof([(-.020, -.0135), (-.015, -.0135), (-.015, -.0465), (.023, -.0465), (.029, -.024), (.034, -.024),
                 (.026, -.0512), (-.020, -.0512)], -.0042, .0042, r=0), FDE_L, .62, 0, .0007, 2, flag=4)
    finish(box((0, -.0180, .0075), (.0034, .0048, .0052)), DK, .48, .2, .0005, 1)
    finish(prof([(.0052, -.018), (.0096, -.018), (.0088, -.0405), (.0046, -.0405)], -.0028, .0028, r=0), BLACK, .42, .5, .0004, 1)

    # --- 4. Heavy barrel and SureFire/3-prong flash hider (muzzle at z .506, bore at y .016)
    finish(lathe([(.356, .0104), (.372, .0104), (.376, .0090), (.448, .0086), (.452, .0094)], 0, .0160, 28), GRAPH, .42, .35, .0004, 1)
    fh = lathe([(.450, .0106), (.478, .0106), (.481, .0098), (.506, .0098)], 0, .0160, 28, 'hider')
    for k in range(3):
        a_ = math.pi/2 + k*2*math.pi/3
        cut(fh, place(box((0, 0, 0), (.0150, .0014, .0130)), (0, .0160, .497), rz=a_))
    cut(fh, _rz([(.470, .0062), (.510, .0062)], 0, .0160, 20))
    for zr in (.455, .460, .465, .470):
        cut(fh, _rz([(zr - .0012, .0120), (zr + .0012, .0120)], 0, .0160, 24))
    finish(fh, BLACK, .44, .3, .0005, 1)

    # --- 5. 20-round 7.62 FN steel magazine (group 1)
    mag = prof([(.078, -.018), (.026, -.018), (.024, -.103), (.076, -.103)], -.0118, .0118, 'mag')
    for s_ in (-1, 1):
        for zr in (.034, .068):
            cut(mag, box((s_*.0118, -.062, zr), (.0010, .036, .0022)))
    finish(mag, MAGC, .60, .1, .0016, 2, group=1)
    finish(prof([(.022, -.101), (.080, -.101), (.082, -.110), (.021, -.111)], -.0128, .0128, 'magplate'), RUB, .74, 0, .0012, 2, group=1)

    # --- 6. FN SSR Precision stock (group 2): FDE chassis + matte black rubber cheek riser & buttpad
    finish(box((0, .0110, -.0745), (.0130, .0230, .0035)), FDE_L, .58, 0, .0008, 2, group=2, flag=4)
    fr = prof([(-.078, .0320), (-.142, .0350), (-.142, -.0470), (-.122, -.0470), (-.078, -.0120)], -.0105, .0105, 'strut')
    cut(fr, prof([(-.086, .0220), (-.132, .0250), (-.132, .0040)], -.03, .03))
    cut(fr, prof([(-.092, -.0040), (-.132, -.0050), (-.132, -.0380), (-.124, -.0380)], -.03, .03))
    finish(fr, FDE, .58, 0, .0012, 2, group=2, flag=6)
    rb = box((0, -.0190, -.175), (.0140, .0290, .0340), 'rblock')
    for s_ in (-1, 1):
        cut(rb, box((s_*.0140, -.0160, -.182), (.0032, .0170, .0220)))
    finish(rb, FDE_L, .60, 0, .0014, 2, group=2, flag=4)
    for s_ in (-1, 1):
        finish(_rx(*sorted((s_*.0110, s_*.0152)), -.0200, -.194, .0058, 18), RUB, .56, 0, .0004, 1, group=2)
        finish(_rx(*sorted((s_*.0150, s_*.0156)), -.0200, -.194, .0020, 10), STEEL, .35, .85, 0, group=2)
    sr_ = box((0, -.0505, -.178), (.0080, .0022, .0360), 'srail')
    for i in range(7):
        cut(sr_, box((0, -.0525, -.208 + .0098*i), (.0100, .0012, .0018)))
    finish(sr_, FURN, .58, 0, .0005, 1, group=2)
    for yr in (-.0060, -.0340):
        finish(_rz([(-.230, .0028), (-.150, .0028)], 0, yr, 14), STEEL, .34, .85, .0002, group=2)
        for zc in (-.222, -.216, -.212):
            finish(_rz([(zc - .0012, .0036), (zc + .0012, .0036)], 0, yr, 14), DK, .50, .2, 0, group=2)
    finish(box((0, -.0310, -.2270), (.0128, .0480, .0030)), FURN, .58, 0, .0008, 2, group=2)
    pad = prof([(-.2300, .0200), (-.2380, .0250), (-.2500, .0240), (-.2580, .0100), (-.2590, -.0690),
                (-.2550, -.0800), (-.2320, -.0800), (-.2300, -.0740)], -.0145, .0145, 'pad')
    cut(pad, box((0, -.0280, -.2300), (.0105, .0440, .0040)))
    finish(pad, RUB, .88, 0, .0026, 3, group=2, flag=4)
    finish(box((0, -.0280, -.2306), (.0095, .0420, .0016)), FURN, .66, 0, .0006, 1, group=2)
    rf = box((0, .0235, -.163), (.0105, .0105, .0400), 'riser')
    for zc in (-.188, -.140):
        cut(rf, box((0, .0235, zc), (.0200, .0060, .0150)))
    finish(rf, FURN, .58, 0, .0010, 2, group=2)
    ch = prof([(-.212, .0480), (-.098, .0480), (-.088, .0464), (-.096, .0360), (-.212, .0360)], -.0135, .0135, 'cheek')
    finish(ch, RUB, .82, 0, .0024, 3, group=2, flag=4)
    finish(box((0, .0350, -.154), (.0128, .0012, .0570)), DK, .58, 0, .0004, 1, group=2)
    finish(box((-.0118, .0300, -.210), (.0030, .0042, .0050)), RUB, .60, 0, .0006, 1, group=2)

    # --- 7. Scope: matte dark-earth/black precision optic on a one-piece mount (sy = .085)
    sy = .085
    ocu = lathe([(-.056, .0198), (-.054, .0210), (-.005, .0210), (-.002, .0192), (.002, .0172)], 0, sy, 44, 'ocular')
    cut(ocu, _rz([(-.060, .0183), (-.050, .0183)], 0, sy, 36))
    finish(ocu, EYE, .74, 0, .0006, 2)
    for i in range(18):
        a_ = i * 2*math.pi/18
        finish(_rz([(-.047, .0010), (-.010, .0010)], .0208*cos(a_), sy + .0208*sin(a_), 8), DK, .76, 0, 0)
    finish(_rz([(-.0565, .0214), (-.0535, .0214)], 0, sy, 40), BLACK, .50, 0, 0)
    finish(_rz([(-.0552, .0188), (-.0520, .0188)], 0, sy, 32), STEEL, .32, .85, 0)
    finish(_rz([(-.0541, .0003), (-.0536, .0105), (-.0522, .0182)], 0, sy, 32), LENS, .15, 0, 0)
    finish(_rz([(.0010, .0176), (.0060, .0176)], 0, sy, 32), FURN, .55, .1, .0002)
    tb = lathe([(.004, .0156), (.140, .0152), (.176, .0226), (.222, .0226), (.225, .0214)], 0, sy, 44, 'tube')
    cut(tb, _rz([(.208, .0196), (.230, .0196)], 0, sy, 36))
    finish(tb, SCOPE, .54, .1, .0006, 2)
    finish(_rz([(.2205, .0232), (.2250, .0232)], 0, sy, 40), BLACK, .50, 0, .0003)
    finish(_rz([(.2085, .0198), (.2110, .0198)], 0, sy, 32), STEEL, .32, .85, 0)
    finish(_rz([(.2088, .0192), (.2098, .0110), (.2104, .0003)], 0, sy, 32), LENS, .14, 0, 0)
    tz = .063
    finish(_rz([(tz - .016, .0176), (tz + .016, .0176)], 0, sy, 30), SCOPE, .52, .1, .0010, 2)
    finish(_ry(sy + .015, sy + .021, 0, tz, .0110, 24), SCOPE, .50, .1, .0005, 1)
    finish(_ry(sy + .021, sy + .033, 0, tz, .0106, 24), BLACK, .46, .1, .0008, 2)
    for i in range(14):
        a_ = i * 2*math.pi/14
        finish(_ry(sy + .0225, sy + .0322, .0106*cos(a_), tz + .0106*sin(a_), .0009, 8), DK, .5, 0, 0)
    finish(_ry(sy + .0328, sy + .0345, 0, tz, .0046, 14), STEEL, .32, .85, .0002, 1)
    for s_ in (-1, 1):
        finish(_rx(*sorted((s_*.015, s_*.021)), sy, tz, .0118, 24), SCOPE, .50, .1, .0005, 1)
        finish(_rx(*sorted((s_*.0205, s_*.0225)), sy, tz, .0124, 24), FURN, .55, .1, .0003)
        finish(_rx(*sorted((s_*.0222, s_*.0300)), sy, tz, .0108, 24), BLACK, .46, .1, .0008, 2)
        finish(_rx(*sorted((s_*.0298, s_*.0310)), sy, tz, .0050, 14), DK, .48, 0, 0)
    finish(box((0, .0540, .061), (.0118, .0040, .0520)), FURN, .52, .1, .0008, 2)
    for zr in (.021, .101):
        finish(box((0, .0625, zr), (.0090, .0060, .0046)), FURN, .52, .1, .0006, 2)
        rg = _rz([(zr - .0050, .0186), (zr + .0050, .0186)], 0, sy, 32, 'ring')
        cut(rg, _rz([(zr - .0060, .0154), (zr + .0060, .0154)], 0, sy, 32))
        finish(rg, FURN, .52, .1, .0006, 1)
        for s_ in (-1, 1):
            finish(box((s_*.0168, sy, zr), (.0030, .0040, .0052)), FURN, .52, .1, .0005, 1)
            finish(_rx(*sorted((s_*.0196, s_*.0206)), sy, zr, .0020, 12), STEEL, .34, .85, 0)
    for s_ in (-1, 1):
        for zb in (.040, .082):
            finish(_rx(*sorted((s_*.0116, s_*.0126)), .0545, zb, .0018, 12), DK, .50, .2, 0)

    # --- 8. Reciprocating charging handle & bolt carrier (group 3)
    finish(box((-.0186, .0150, .100), (.0032, .0038, .0065)), (.32, .31, .30), .44, .2, .0006, 2, group=3)
    finish(_rx(-.0300, -.0200, .0150, .100, .0052, 18), DK, .46, .2, .0008, 2, group=3)
    finish(_rx(-.0312, -.0296, .0150, .100, .0034, 14), STEEL, .32, .85, .0002, group=3)
    for s_ in (-1, 1):
        finish(box((s_*.0154, .0335, .121), (.0004, .0016, .0850)), STEEL, .32, .85, .0002, group=3)
        for k in range(10):
            finish(box((s_*.0158, .0335, .050 + k*.016), (.0002, .0016, .0035)), BLACK, .60, 0, 0, group=3)

# ------------------------------------------------------------------ pistol (H&K P2000)
def pistol():
    """H&K P2000 duty sidearm: compact polymer frame with rounded dust cover and
    2-slot Picatinny rail, curved ergonomic H&K trigger guard with integrated
    paddle magazine release, interchangeable curved backstrap swell, ambidextrous
    slim slide-release levers, tapered trapezoidal H&K steel slide with rear
    angled serrations only (no front serrations), subtle holstering nose taper,
    bobbed recessed LEM/V3 hammer, and flush 13-round magazine floorplate."""
    clear()
    # --- compact H&K polymer frame with rounded dust cover & 2-slot rail
    fr = prof([(-.048,-.008),(.135,-.008),(.135,-.021),(.126,-.026),(.068,-.026),
               (.056,-.017),(.010,-.017),(-.012,-.015),(-.046,-.018)], -.0126, .0126, 'frame', r=.0034)
    finish(fr, TAN, .70, 0, .0012, 2, flag=4)
    for i in range(2):                                         # 2-slot P2000 rail
        finish(box((0,-.0268,.102+i*.0125), (.0082,.0018,.0034)), BLACK, .6, 0, .0004, 1)
    # Ambidextrous slim H&K slide release levers (P2000 signature)
    for s_ in (-1, 1):
        x0, x1 = sorted((s_*.0124, s_*.0139))
        finish(prof([(-.014,-.0095),(.028,-.0095),(.028,-.0132),(-.014,-.0132)], x0, x1, 'slrel', r=0), BLACK, .48, .3, .0004)
    # --- ergonomic H&K grip with curved backstrap swell & side stipple panels
    finish(prof([(.028,-.014),(-.008,-.013),(-.018,-.027),(-.038,-.068),(-.036,-.100),(-.030,-.110),
                 (.000,-.106),(.010,-.066),(.018,-.044)], -.0126, .0126, 'grip', r=.0055), BLACK, .82, 0, .0038, 4, flag=4)
    finish(prof([(-.049,-.0110),(-.012,-.0110),(-.012,-.0185),(-.028,-.0215),(-.047,-.0170)], -.0112, .0112, 'tail', r=.003), TAN, .70, 0, .0014, 2, flag=4)
    for s_ in (-1, 1):
        x0, x1 = sorted((s_*.0120, s_*.0132))
        finish(prof([(-.008,-.040),(-.026,-.042),(-.034,-.092),(-.010,-.092),(.000,-.066)], x0, x1, 'panel', r=0), TAN, .88, 0, .0004, 1, flag=4)
    # --- curved H&K trigger guard (compact, realistic proportions clearing HAND_R!) + HK paddle mag release
    finish(prof([(.014,-.016),(.020,-.016),(.022,-.0455),(.061,-.0455),(.068,-.036),(.072,-.016),(.077,-.016),
                 (.072,-.038),(.064,-.0495),(.014,-.0495)], -.0040, .0040, 'guard', r=0), TAN, .70, 0, .0007, 2, flag=4)
    finish(prof([(.012,-.045),(.024,-.045),(.024,-.051),(.014,-.051)], -.0056, .0056, 'hk_paddle', r=0), BLACK, .55, .2, .0003)
    finish(prof([(.044,-.016),(.050,-.016),(.044,-.041),(.039,-.041)], -.0028, .0028, 'trig', r=0), DARK, .45, .5, .0005)
    # --- tapered trapezoidal H&K steel slide (group 3): rear angled serrations ONLY, sloped holstering nose
    sl = prof_front([(-.0118,-.0095),(.0118,-.0095),(.0124,-.0020),(.0120,.0160),(.0076,.0245),
                     (-.0076,.0245),(-.0120,.0160),(-.0124,-.0020)], -.050, .144, 'slide')
    cut(sl, box((.0128,.0135,.042), (.0066,.0070,.0215)))                                     # ejection port
    cut(sl, lathe([(.136,.0056),(.148,.0056)], 0, .0060, 20))                                 # bore
    # Subtle H&K sloped lower-front holstering bevel
    cut(sl, prof([(.136,-.014),(.152,-.014),(.152,.000),(.144,.000)], -.02, .02))
    # Rear angled serrations ONLY (real P2000 has no front serrations!)
    for i in range(7):
        for s_ in (-1, 1):
            c_ = box((0,0,0), (.0014,.0092,.0012))
            c_.rotation_euler = (radians(-10), 0, 0); c_.location = (s_*.0125, .0075, -.044 + i*.0048)
            bpy.context.view_layer.objects.active = c_; c_.select_set(True)
            bpy.ops.object.transform_apply(location=True, rotation=True); c_.select_set(False)
            cut(sl, c_)
    finish(sl, GRAPH, .48, .22, .0010, 2, group=3)
    finish(box((.0094,.0135,.042), (.0032,.0062,.0198)), STEEL, .36, .85, .0003, 1, group=3)   # chamber hood
    finish(lathe([(.134,.0062),(.1465,.0058)], 0, .0060, 20), STEEL, .36, .85, .0002, group=3) # barrel crown
    # --- sights (on the slide): 3-dot H&K sights, rear at z -.0405 top .0342
    finish(box((0,.0288,.1310), (.0019,.0050,.0048)), BLACK, .55, 0, .0004, group=3)
    finish(lathe([(.1254,.0017),(.1268,.0017)], 0, .0318, 14), AMBER, .35, 0, 0, group=3, flag=3)
    rear = box((0,.0288,-.0405), (.0102,.0050,.0040), 'rear')
    cut(rear, box((0,.0338,-.0405), (.0026,.0048,.0100)))
    finish(rear, BLACK, .55, 0, .0004, group=3)
    for s_ in (-1, 1):
        finish(box((s_*.0054,.0316,-.0446), (.0014,.0014,.0004)), AMBER, .35, 0, 0, group=3, flag=3)
    # --- bobbed recessed P2000 hammer (barely proud of the rear slide cut)
    finish(prof([(-.048,.0050),(-.054,.0050),(-.055,.0145),(-.050,.0155)], -.0032, .0032, 'hammer', r=0), DARK, .46, .4, .0005)
    # --- flush double-stack magazine (group 1)
    finish(prof([(.012,-.024),(-.012,-.024),(-.032,-.104),(-.008,-.104)], -.0096, .0096, 'mag'), GRAPH, .60, .2, .0012, 2, group=1)
    finish(prof([(-.034,-.1040),(-.002,-.1040),(.000,-.1115),(-.033,-.1135)], -.0122, .0122, 'magplate'), BLACK, .78, 0, .0012, 2, group=1)

# ------------------------------------------------------------------ pistol support hand (HAND_P)
def hand_p():
    '''Thumbs-forward cup: the left palm closes on the grip's bare left panel and the four
    fingers wrap the firing hand's fingers under the trigger guard. Built in the pistol's frame.'''
    clear()
    W_P = V3(-.038, -.102, -.018)
    AX_P = V3(-.50, -.52, -.69).normalized()
    cuff_ctrl = [
        (W_P + AX_P*.018, V3(-.30, .86, -.40), .0270, .0218),
        (W_P + AX_P*.002, V3(-.30, .86, -.40), .0282, .0228),
        (W_P - AX_P*.016, V3(-.30, .86, -.40), .0298, .0238),
    ]
    _, s_cuff = loft_tube(cuff_ctrl, seg_u=10, seg_v=20, palmar_flat=0.94, col=LEATHER, rough=.60, flag=7, name='p_cuff')
    shell_patch(s_cuff, 0.14, 0.78, -25.0, 150.0, thick=.0026, bury=.0010, nu=6, nv=10, rim_u=.06, rim_d=7.0,
                col=LEATHER, rough=.52, flag=7, sharp=True, name='p_strap')
    palm_ctrl = [
        (V3(-.032, -.097, -.012), V3(-.92, .22, -.32), .0310, .0200),
        (V3(-.029, -.080,  .004), V3(-.94, .10, -.32), .0380, .0210),
        (V3(-.027, -.066,  .022), V3(-.95, .00, -.30), .0385, .0190),
        (V3(-.025, -.059,  .040), V3(-.96, -.08, -.26), .0330, .0150),
    ]
    _, s_palm = loft_tube(palm_ctrl, seg_u=14, seg_v=20, palmar_flat=0.82, col=LEATHER, rough=.54, flag=7, name='p_palm')
    shell_patch(s_palm, 0.06, 0.94, 30.0, 150.0, thick=.0016, nu=7, nv=7, rim_u=.04, rim_d=7.0,
                col=SLATE, rough=.56, flag=2, sharp=True, name='p_dorsal')
    th_ctrl = [
        (V3(-.031, -.070, -.006), V3(-.93, .30, -.20), .0200, .0176),
        (V3(-.0285,-.052,  .012), V3(-.94, .30, -.12), .0188, .0162),
        (V3(-.0235,-.040,  .030), V3(-.95, .28, -.06), .0160, .0140),
        (V3(-.0205,-.033,  .048), V3(-.95, .28, -.04), .0146, .0128),
        (V3(-.0190,-.029,  .064), V3(-.95, .28, -.02), .0120, .0106),
    ]
    _, s_th = loft_tube(th_ctrl, seg_u=18, seg_v=20, palmar_flat=0.82, col=SLATE, rough=.58, flag=2, name='p_thumb')
    shell_patch(s_th, 0.28, 0.62, 40.0, 140.0, thick=.0032, nu=6, nv=6, rim_u=.04, rim_d=7.0,
                col=PAD, rough=.40, flag=4, sharp=True, name='p_th_pad')
    shell_patch(s_th, 0.72, 0.99, -140.0, 150.0, thick=.0014, nu=6, nv=9, rim_u=.035, rim_d=8.0,
                col=TIP_CAP, rough=.52, flag=7, sharp=True, name='p_th_tip')
    for i, (yf, zf, rf) in enumerate(((-.060,.058,.0108), (-.077,.052,.0112), (-.094,.046,.0108), (-.110,.038,.0098))):
        f_ctrl = [
            (V3(-.024, yf, zf-.016), V3(-.70, 0, .71), rf*1.10, rf*0.96),
            (V3(-.012, yf, zf+.002), V3(-.42, 0, .90), rf*1.06, rf*0.94),
            (V3( .006, yf, zf+.006), V3( .14, 0, .98), rf*1.02, rf*0.92),
            (V3( .024, yf, zf-.004), V3( .66, 0, .75), rf*0.96, rf*0.86),
            (V3( .032, yf, zf-.020), V3( .94, 0, .34), rf*0.84, rf*0.76),
        ]
        _, s_f = loft_tube(f_ctrl, seg_u=16, seg_v=16, palmar_flat=0.80, col=SLATE, rough=.58, flag=2, name=f'p_f{i}')
        shell_patch(s_f, 0.30, 0.60, 42.0, 138.0, thick=.0016, nu=5, nv=5, rim_u=.04, rim_d=8.0,
                    col=PAD, rough=.42, flag=4, sharp=True, name=f'p_fpad{i}')

# ------------------------------------------------------------------ combat knife
def knife():
    '''Fixed-blade combat knife, KA-BAR proportions. Authored with +z toward the tip, +y along
    the spine, -y the edge, then carried onto the rifle's grip cylinder so the closed firing
    hand fits the handle unchanged. The blade comes from one profile function, section by
    section, so spine, grind and edge all converge on a single welded point.'''
    clear()
    TIP, YTIP, CLIP, GUARD = .2500, -.0075, .1780, .0560
    def section(z):
        # spine: straight, thumb notches cut INTO it, then a concave clip to the point
        ys = .0125
        if z < .0860 and z > GUARD and int((z-GUARD)/.0032) % 2 == 1: ys -= .0013
        if z > CLIP: ys += (YTIP-.0125) * ((z-CLIP)/(TIP-CLIP))**.62
        # edge: the ricasso's flat foot, the round choil notch, then the belly
        ye = -.0195
        c = (z-.0820)/.0075
        if abs(c) < 1: ye += .0072 * math.sqrt(1-c*c)
        if z > .1900: ye += (YTIP+.0195) * ((z-.1900)/(TIP-.1900))**2.2
        # grind: nothing on the ricasso, full width past the choil
        h = .0150 * min(1, max(0, (z-.0895)/.0060))
        yg = ye + min(h, .66*(ys-ye))
        ym = ye + min(.0015 if h > 0 else 0, .24*(ys-ye))
        t = .0021 * min(1, (TIP-z)/.075) ** .7
        return ys, yg, ym, ye, t, t*.20 if h > 0 else t
    zs = sorted(set([GUARD-.002] + [GUARD+i*.0032 for i in range(10)] + [.0745+i*.0015 for i in range(11)]
                    + [.092+i*(TIP-.092)/34 for i in range(35)]))
    def band(pick, col, rough, metal, name):
        bm = bmesh.new(); rings = []
        for z in zs:
            rings.append([bm.verts.new((x, y, z)) for x, y in pick(*section(z))])
        n = len(rings[0])
        for k in range(len(rings)-1):
            for i in range(n):
                j = (i+1) % n
                bm.faces.new((rings[k][i], rings[k][j], rings[k+1][j], rings[k+1][i]))
        bm.faces.new(rings[0])
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        return finish(from_bm(bm, name), col, rough, metal, 0, 1)
    band(lambda ys,yg,ym,ye,t,tm: ((t,ys),(t,yg),(-t,yg),(-t,ys)), (.48,.49,.52), .30, .92, 'flat')
    band(lambda ys,yg,ym,ye,t,tm: ((t,yg),(tm,ym),(-tm,ym),(-t,yg)), (.70,.70,.72), .22, .98, 'grind')
    band(lambda ys,yg,ym,ye,t,tm: ((tm,ym),(0,ye),(-tm,ym)), (.96,.97,.99), .15, 1.0, 'edge')
    finish(box((0,.0058,.1320), (.00225,.0017,.0380)), BLACK, .55, .2, .0002, 1)              # fuller
    # --- heavy chamfered crossguard, a forward thumb quillon on the spine side
    finish(prof_front([(-.0085,-.0300),(.0085,-.0300),(.0130,-.0215),(.0130,.0150),(.0085,.0230),(-.0085,.0230),(-.0130,.0150),(-.0130,-.0215)], .0440, GUARD), DARK, .44, .35, .0014, 2)
    finish(prof([(.0440,.0160),(GUARD,.0160),(.0610,.0240),(.0660,.0325),(.0600,.0355),(.0520,.0285),(.0440,.0235)], -.0080, .0080, 'quillon'), DARK, .44, .35, .0012, 2)
    finish(prof_front([(-.0072,-.0285),(.0072,-.0285),(.0072,-.0245),(-.0072,-.0245)], .0425, .0575), ORANGE, .5, 0, .0004)
    # --- handle: chunky ribbed polymer, longer toward the guard so it shows above the fist
    rings = []
    for i in range(32):
        z = -.0800 + i*.00395
        rings.append((z, .0150 + .0009*cos(z/.0120*math.pi)))
    finish(lathe(rings, 0, 0, 28), BLACK, .76, 0, 0)
    for z in (-.012,.002,.016,.030):
        finish(lathe([(z-.0026,.0150),(z-.0016,.0162),(z+.0016,.0162),(z+.0026,.0150)], 0, 0, 28), DARK, .66, 0, 0)
    # --- pommel: tan collar and a steel skull-crusher with a lanyard pin
    finish(lathe([(-.0820,.0132),(-.0800,.0164),(-.0760,.0164),(-.0740,.0144)], 0, 0, 28), TAN, .60, 0, .0005)
    finish(lathe([(-.0950,.0064),(-.0920,.0128),(-.0840,.0146),(-.0806,.0130)], 0, 0, 28), DARK, .46, .3, .0008)
    finish(pin(-.0152,.0152,0,-.0882,.0024,14), STEEL, .36, .9, .0002, 1)
    a = Vector((0,.921,.386)).normalized(); sp = Vector((0,.386,-.921)).normalized()
    c = Vector((0,-.0500,-.0078)) + a*.0140      # the fist sits below the guard, not against it
    for o in bpy.context.collection.objects:
        for v in o.data.vertices:
            q = v.co.copy(); v.co = c + Vector((q.x,0,0)) + sp*q.y + a*q.z

# ------------------------------------------------------------------ knife variants
# Every variant shares the combat knife's contract: authored +z toward the tip, +y the
# spine, -y the edge, a ~15 mm round handle from z -.080 to the guard at .056, then
# carried onto the same grip cylinder as the combat knife so the one closed fist holds
# them all. Only the blade outline, the furniture and the colours differ.
KN_GUARD = .0560
BRASS = (.80,.62,.30); WOOD = (.40,.24,.13); COYOTE = (.52,.44,.32); BLADE_DLC = (.17,.17,.18)

def knife_blade(zs, bands):
    """bands: [(ring(z) -> [(x, y), ...], col, rough, metal, name)]; each ring a closed
    convex outline, every band converging on one point at the last z."""
    for ring, col, rough, metal, name in bands:
        bm = bmesh.new(); rings = []
        for z in zs:
            rings.append([bm.verts.new((x, y, z)) for x, y in ring(z)])
        n = len(rings[0])
        for k in range(len(rings)-1):
            for i in range(n):
                j = (i+1) % n
                bm.faces.new((rings[k][i], rings[k][j], rings[k+1][j], rings[k+1][i]))
        bm.faces.new(rings[0])
        bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-7)
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        finish(from_bm(bm, name), col, rough, metal, 0, 1)

def single_edge(section, lo, hi, n, flat_col, grind_col, edge_col, flat_rough=.30):
    """Spine/flat, grind and edge bands from section(z) -> (ys, yg, ym, ye, t, tm)."""
    zs = [lo + (hi-lo)*i/(n-1) for i in range(n)]
    def flat_r(z):
        ys, yg, ym, ye, t, tm = section(z); return ((t,ys),(t,yg),(-t,yg),(-t,ys))
    def grind_r(z):
        ys, yg, ym, ye, t, tm = section(z); return ((t,yg),(tm,ym),(-tm,ym),(-t,yg))
    def edge_r(z):
        ys, yg, ym, ye, t, tm = section(z); return ((tm,ym),(0,ye),(-tm,ym))
    knife_blade(zs, [(flat_r, flat_col, flat_rough, .92, 'flat'),
                     (grind_r, grind_col, .22, .98, 'grind'),
                     (edge_r, edge_col, .15, 1.0, 'edge')])

def knife_handle(col, rough, ribs, rib_col, r0=.0150, swell=.0009, flag=4):
    rings = []
    for i in range(32):
        z = -.0800 + i*.00395
        rings.append((z, r0 + swell*cos(z/.0120*math.pi)))
    finish(lathe(rings, 0, 0, 28), col, rough, 0, 0, flag=flag)
    for z in ribs:
        finish(lathe([(z-.0026,r0),(z-.0016,r0+.0012),(z+.0016,r0+.0012),(z+.0026,r0)], 0, 0, 28),
               rib_col, .60, 0, 0, flag=flag)

def knife_place():
    """The combat knife's own carry onto the grip cylinder."""
    a = Vector((0,.921,.386)).normalized(); sp = Vector((0,.386,-.921)).normalized()
    c = Vector((0,-.0500,-.0078)) + a*.0140
    for o in bpy.context.collection.objects:
        for v in o.data.vertices:
            q = v.co.copy(); v.co = c + Vector((q.x,0,0)) + sp*q.y + a*q.z

def knife_tanto():
    """American tanto: straight spine, a straight primary edge breaking at an angle into
    the short secondary edge that runs up to a point high on the spine. Black-coated
    flats, a stonewashed grind, coyote cord-wrap handle."""
    clear()
    TIP, KINK = .2400, .2000
    def section(z):
        ys = .0120
        if z > .2240: ys += (.0080-.0120) * (z-.2240)/(TIP-.2240)
        ye = -.0175
        if z > KINK: ye += (.0080+.0175) * (z-KINK)/(TIP-KINK)
        h = .0110 * min(1, max(0, (z-.0640)/.0060))
        yg = ye + min(h, .60*(ys-ye))
        ym = ye + min(.0013 if h > 0 else 0, .24*(ys-ye))
        t = .0024 * min(1, (TIP-z)/.030) ** .55
        return ys, yg, ym, ye, t, t*.22 if h > 0 else t
    single_edge(section, KN_GUARD-.002, TIP, 48, BLADE_DLC, (.56,.56,.58), (.90,.91,.93), .40)
    finish(prof_front([(-.0095,-.0240),(.0095,-.0240),(.0110,-.0200),(.0110,.0150),(.0095,.0185),
                       (-.0095,.0185),(-.0110,.0150),(-.0110,-.0200)], .0460, KN_GUARD), BLACK, .50, .3, .0012, 2)
    knife_handle(COYOTE, .84, [-.068 + i*.0098 for i in range(11)], (.42,.35,.25))
    finish(lathe([(-.0950,.0070),(-.0925,.0128),(-.0845,.0142),(-.0806,.0128)], 0, 0, 28), BLACK, .50, .3, .0008)
    knife_place()

def knife_dagger():
    """Double-edged spear-point fighting dagger: a diamond section with a raised centre
    ridge, both edges bright, a straight brass crossguard and a brass pommel."""
    clear()
    TIP = .2550
    def w(z): return .0148 * max(0.0, (TIP-z)/(TIP-KN_GUARD)) ** .80
    def t(z): return .0034 * max(0.0, (TIP-z)/(TIP-KN_GUARD)) ** .55
    def g(z): return min(.0042, .34*w(z))
    zs = [KN_GUARD-.002 + (TIP-KN_GUARD+.002)*i/47 for i in range(48)]
    def body(z):
        a, b, c = t(z), w(z)-g(z), t(z)*.25
        return ((a,0),(c,b),(-c,b),(-a,0),(-c,-b),(c,-b))
    def up(z):
        b, c = w(z)-g(z), t(z)*.25
        return ((c,b),(0,w(z)),(-c,b))
    def dn(z):
        b, c = w(z)-g(z), t(z)*.25
        return ((c,-b),(-c,-b),(0,-w(z)))
    knife_blade(zs, [(body, (.60,.60,.62), .26, .96, 'body'),
                     (up, (.94,.95,.97), .14, 1.0, 'edge_u'),
                     (dn, (.94,.95,.97), .14, 1.0, 'edge_d')])
    finish(prof_front([(-.0070,-.0380),(.0070,-.0380),(.0085,-.0330),(.0085,.0330),(.0070,.0380),
                       (-.0070,.0380),(-.0085,.0330),(-.0085,-.0330)], .0450, KN_GUARD), BRASS, .34, .92, .0015, 2)
    knife_handle(BLACK, .70, [-.070 + i*.0060 for i in range(19)], DARK, r0=.0142, swell=.0014)
    finish(lathe([(-.1010,.0040),(-.0990,.0110),(-.0950,.0150),(-.0890,.0155),(-.0840,.0130),(-.0806,.0120)],
                 0, 0, 28), BRASS, .34, .92, .0008)
    knife_place()

def knife_kukri():
    """Kukri: a heavy forward-weighted blade that drops toward the edge and swells into a
    deep belly, the cho notch at its root, a hardwood handle between brass bolsters."""
    clear()
    TIP = .2680
    def section(z):
        s = min(1.0, max(0.0, (z-KN_GUARD)/(TIP-KN_GUARD)))
        c = -.0300 * s*s
        ws = (.0110 + .0060*s) * (1 - s**3)
        we = (.0120 + .0180*sin(math.pi*s*.85)) * (1 - s**6)
        ys, ye = c + ws, c - we
        k = (z-.0700)/.0050
        if abs(k) < 1: ye += .0040 * math.sqrt(1-k*k)       # the cho
        h = .0120 * min(1, max(0, (z-.0780)/.0060))
        yg = ye + min(h, .55*(ys-ye))
        ym = ye + min(.0016 if h > 0 else 0, .24*(ys-ye))
        tt = .0034 * (1-s) ** .45
        return ys, yg, ym, ye, tt, tt*.22 if h > 0 else tt
    single_edge(section, KN_GUARD-.002, TIP, 56, (.40,.40,.42), (.68,.68,.70), (.94,.95,.97), .42)
    finish(lathe([(.0440,.0130),(.0450,.0172),(.0545,.0172),(KN_GUARD+.001,.0130)], 0, 0, 28), BRASS, .34, .92, .0006)
    knife_handle(WOOD, .64, (-.040, -.004), (.30,.17,.09), r0=.0152, swell=.0016)
    finish(lathe([(-.0960,.0070),(-.0935,.0150),(-.0835,.0166),(-.0806,.0150)], 0, 0, 28), BRASS, .34, .92, .0008)
    knife_place()

def knife_talon():
    """Hawkbill talon: a short blade that hooks down to a claw point, the edge on the
    inside of the curve; satin steel, black G10 scales with blaze liners."""
    clear()
    TIP = .2150
    def section(z):
        s = min(1.0, max(0.0, (z-KN_GUARD)/(TIP-KN_GUARD)))
        c = -.0420 * s ** 2.4
        ws = .0110 * (1 - s**3)
        we = .0092 * (1 - s**1.6)
        ys, ye = c + ws, c - we
        h = .0080 * min(1, max(0, (z-.0640)/.0060))
        yg = ye + min(h, .55*(ys-ye))
        ym = ye + min(.0012 if h > 0 else 0, .24*(ys-ye))
        tt = .0026 * (1-s) ** .45
        return ys, yg, ym, ye, tt, tt*.22 if h > 0 else tt
    single_edge(section, KN_GUARD-.002, TIP, 48, (.56,.56,.58), (.72,.72,.74), (.95,.96,.98), .30)
    finish(lathe([(.0450,.0125),(.0460,.0160),(.0545,.0160),(KN_GUARD+.001,.0120)], 0, 0, 28), BLACK, .48, .3, .0006)
    knife_handle(BLACK, .66, (-.060, -.030, .000, .030), ORANGE, r0=.0148, swell=.0010)
    finish(lathe([(-.0960,.0068),(-.0930,.0132),(-.0840,.0150),(-.0806,.0134)], 0, 0, 28), ORANGE, .50, 0, .0006)
    finish(pin(-.0156,.0156,0,-.0882,.0026,14), STEEL, .36, .9, .0002, 1)
    knife_place()

# ------------------------------------------------------------------ open guard hand (HAND_KO)
def hand_open():
    """The knife's off hand in the guard: open, fingers together and up, palm turned AWAY
    from the eye toward the threat, so the camera sees the armoured back of a left hand.
    Frame: origin at the wrist, +y toward the fingertips, +z the palm normal (forward),
    +x the thumb side. game.c places it with its own rotations."""
    clear()
    D = V3(0, 0, -1)                                   # dorsal: toward the eye
    cuff = [(V3(0,-.034,-.006), D, .0262, .0205), (V3(0,-.016,-.003), D, .0276, .0214),
            (V3(0, .002, 0), D, .0290, .0222)]
    _, s_cuff = loft_tube(cuff, seg_u=10, seg_v=20, palmar_flat=0.94, col=SLATE, rough=.60, flag=2, name='o_cuff')
    shell_patch(s_cuff, 0.14, 0.80, -25.0, 205.0, thick=.0026, bury=.0010, nu=6, nv=10, rim_u=.06, rim_d=7.0,
                col=LEATHER, rough=.52, flag=7, sharp=True, name='o_strap')
    # The palm tapers from the knuckles into the wrist and is slightly cupped:
    # a flat paddle is what read as a cut-out.
    palm = [(V3(.003,-.004,.002), D, .0270, .0172), (V3(.003,.024,.003), D, .0350, .0170),
            (V3(.001,.052,.004), D, .0385, .0158), (V3(-.001,.074,.006), D, .0370, .0130)]
    _, s_palm = loft_tube(palm, seg_u=14, seg_v=22, palmar_flat=0.78, col=LEATHER, rough=.58, flag=7, name='o_palm')
    # The back of the hand: slate softshell and ONE moulded knuckle guard across it.
    shell_patch(s_palm, 0.04, 0.96, 18.0, 162.0, thick=.0016, nu=8, nv=8, rim_u=.04, rim_d=7.0,
                col=SLATE, rough=.56, flag=2, sharp=True, name='o_dorsal')
    shell_patch(s_palm, 0.70, 0.97, 32.0, 148.0, thick=.0020, nu=6, nv=8, rim_u=.05, rim_d=8.0,
                col=PAD_RIM, rough=.52, flag=4, sharp=True, name='o_kn_bezel')
    shell_patch(s_palm, 0.73, 0.94, 40.0, 140.0, thick=.0034, nu=6, nv=8, rim_u=.05, rim_d=7.0,
                col=PAD, rough=.40, flag=4, sharp=True, name='o_kn_plate')
    # A seam from wrist to each knuckle row, the way a real glove is cut.
    shell_patch(s_palm, 0.10, 0.66, 84.0, 96.0, thick=.0008, nu=5, nv=3, rim_u=.04, rim_d=3.0,
                col=SEAM, rough=.62, flag=4, name='o_seam')
    # FOUR FINGERS AS JOINTED CHAINS: proximal, middle and distal phalanx, each
    # flexed at its own joint toward the palm. A relaxed hand curls more toward
    # the little finger and fans slightly, so no two fingers share a pose.
    #   (x at the knuckle, length, radius, splay deg, MCP/PIP/DIP flex deg)
    FING = ((.0255, .0700, .0094,  5.0, 10.0, 16.0, 10.0),
            (.0085, .0780, .0099,  1.0, 13.0, 20.0, 12.0),
            (-.0088, .0730, .0096, -3.0, 17.0, 26.0, 15.0),
            (-.0250, .0580, .0084, -8.0, 22.0, 32.0, 18.0))
    for i, (x, L, r, spl, f1, f2, f3) in enumerate(FING):
        p = V3(x, .072 - abs(x)*.12, .005)
        ang = 0.0
        sp = radians(spl)
        pts = [p.copy()]; ups = []
        for seg_len, flex in zip((L*.46, L*.30, L*.24), (f1, f2, f3)):
            ang += radians(flex)
            d = V3(math.sin(sp)*math.cos(ang), math.cos(sp)*math.cos(ang), math.sin(ang))
            p = p + d*seg_len; pts.append(p.copy())
            ups.append(V3(0, math.sin(ang), -math.cos(ang)))
        ctrl = []
        rr = (1.06, .98, .90, .78)
        for k in range(3):
            for u in ((0.0, .5) if k < 2 else (0.0, .5, 1.0)):
                q = rr[k] + (rr[k+1]-rr[k])*u
                kn = 1.0 + .06*math.sin(math.pi*u) if u == 0.0 and k > 0 else 1.0    # joint knuckles
                ctrl.append((pts[k].lerp(pts[k+1], u), ups[k], r*q*kn, r*q*.90*kn))
        _, s_f = loft_tube(ctrl, seg_u=20, seg_v=16, palmar_flat=0.80, col=SLATE, rough=.58, flag=2, name=f'o_f{i}')
        shell_patch(s_f, 0.14, 0.34, 62.0, 118.0, thick=.0013, nu=5, nv=5, rim_u=.04, rim_d=8.0,
                    col=PAD, rough=.42, flag=4, sharp=True, name=f'o_fpad{i}')
        shell_patch(s_f, 0.55, 0.99, -150.0, -30.0, thick=.0010, nu=6, nv=6, rim_u=.04, rim_d=8.0,
                    col=LEATHER, rough=.52, flag=7, name=f'o_ftip{i}')
        shell_patch(s_f, 0.80, 0.99, 40.0, 140.0, thick=.0010, nu=5, nv=5, rim_u=.04, rim_d=8.0,
                    col=TIP_CAP, rough=.50, flag=7, name=f'o_fcap{i}')
    # The thumb: rooted low on the palm's thumb side, rotated toward the palm
    # (opposition) and lying alongside the index rather than standing out.
    thenar = [(V3(.016,.002,.010), V3(.5,0,-1), .0165, .0138), (V3(.024,.022,.013), V3(.5,0,-1), .0175, .0142),
              (V3(.029,.040,.015), V3(.5,0,-1), .0145, .0122)]
    loft_tube(thenar, seg_u=12, seg_v=18, palmar_flat=0.86, col=LEATHER, rough=.56, flag=7, name='o_thenar')
    th = [(V3(.027,.026,.012), V3(.45,0,-1), .0148, .0128), (V3(.036,.044,.019), V3(.55,-.1,-1), .0138, .0120),
          (V3(.040,.060,.026), V3(.7,-.2,-1), .0124, .0108), (V3(.039,.074,.033), V3(.8,-.3,-1), .0110, .0097),
          (V3(.036,.085,.039), V3(.8,-.4,-1), .0088, .0079)]
    _, s_th = loft_tube(th, seg_u=18, seg_v=18, palmar_flat=0.84, col=SLATE, rough=.58, flag=2, name='o_thumb')
    shell_patch(s_th, 0.22, 0.56, 40.0, 140.0, thick=.0024, nu=6, nv=6, rim_u=.04, rim_d=7.0,
                col=PAD, rough=.40, flag=4, sharp=True, name='o_th_pad')
    shell_patch(s_th, 0.70, 0.99, -150.0, 150.0, thick=.0012, nu=6, nv=9, rim_u=.035, rim_d=8.0,
                col=TIP_CAP, rough=.52, flag=7, sharp=True, name='o_th_tip')

# ------------------------------------------------------------------ grenade pin hand (HAND_GP)
EXTRA_DEFINES = []
def hand_pin():
    """The grenade's off hand taking the pin: the open hand's anatomy (HAND_KO, same
    glove) with the INDEX hooked through the ring, the other three clenched in a fist behind
    it and the thumb laid along the index's side. Frame as hand_open: origin at the
    wrist, +y toward the knuckles, +z the palm normal, +x the thumb side. The hook's
    crook, its wire axis and its opening are exported for game.c, which places the ring
    wire IN the crook rather than at a guessed station."""
    clear()
    D = V3(0, 0, -1)
    cuff = [(V3(0,-.034,-.006), D, .0262, .0205), (V3(0,-.016,-.003), D, .0276, .0214),
            (V3(0, .002, 0), D, .0290, .0222)]
    _, s_cuff = loft_tube(cuff, seg_u=10, seg_v=20, palmar_flat=0.94, col=SLATE, rough=.60, flag=2, name='p_cuff')
    shell_patch(s_cuff, 0.14, 0.80, -25.0, 205.0, thick=.0026, bury=.0010, nu=6, nv=10, rim_u=.06, rim_d=7.0,
                col=LEATHER, rough=.52, flag=7, sharp=True, name='p_strap')
    palm = [(V3(.003,-.004,.002), D, .0270, .0172), (V3(.003,.024,.004), D, .0350, .0172),
            (V3(.001,.052,.006), D, .0385, .0162), (V3(-.001,.072,.009), D, .0370, .0136)]
    _, s_palm = loft_tube(palm, seg_u=14, seg_v=22, palmar_flat=0.78, col=LEATHER, rough=.58, flag=7, name='p_palm')
    shell_patch(s_palm, 0.04, 0.96, 18.0, 162.0, thick=.0016, nu=8, nv=8, rim_u=.04, rim_d=7.0,
                col=SLATE, rough=.56, flag=2, sharp=True, name='p_dorsal')
    shell_patch(s_palm, 0.70, 0.97, 32.0, 148.0, thick=.0020, nu=6, nv=8, rim_u=.05, rim_d=8.0,
                col=PAD_RIM, rough=.52, flag=4, sharp=True, name='p_kn_bezel')
    shell_patch(s_palm, 0.73, 0.94, 40.0, 140.0, thick=.0034, nu=6, nv=8, rim_u=.05, rim_d=7.0,
                col=PAD, rough=.40, flag=4, sharp=True, name='p_kn_plate')
    shell_patch(s_palm, 0.10, 0.66, 84.0, 96.0, thick=.0008, nu=5, nv=3, rim_u=.04, rim_d=3.0,
                col=SEAM, rough=.62, flag=4, name='p_seam')
    # (x at the knuckle, length, radius, splay deg, MCP/PIP/DIP flex deg)
    # Index: a HOOK — little at the knuckle, hard at the middle joint, so the crook
    # between the proximal and middle phalanges is where the wire rides.
    # The other three CLOSE into a fist behind it: a pin takes a real pull, and a
    # hooked finger only has leverage with the rest of the hand clenched under it.
    FING = ((.0255, .0700, .0094,  4.0, 34.0, 96.0, 50.0),
            (.0085, .0780, .0099,  0.0, 84.0, 102.0, 58.0),
            (-.0088, .0730, .0096, -2.0, 88.0, 104.0, 58.0),
            (-.0250, .0580, .0084, -5.0, 92.0, 104.0, 56.0))
    hook = None
    for i, (x, L, r, spl, f1, f2, f3) in enumerate(FING):
        p = V3(x, .072 - abs(x)*.12, .007)
        ang = 0.0
        sp = radians(spl)
        pts = [p.copy()]; ups = []
        for seg_len, flex in zip((L*.46, L*.30, L*.24), (f1, f2, f3)):
            ang += radians(flex)
            d = V3(math.sin(sp)*math.cos(ang), math.cos(sp)*math.cos(ang), math.sin(ang))
            p = p + d*seg_len; pts.append(p.copy())
            ups.append(V3(0, math.sin(ang), -math.cos(ang)))
        ctrl = []
        rr = (1.06, .98, .90, .78)
        for k in range(3):
            for u in ((0.0, .5) if k < 2 else (0.0, .5, 1.0)):
                q = rr[k] + (rr[k+1]-rr[k])*u
                kn = 1.06 if u == 0.0 and k > 0 else 1.0       # the joints swell
                # bend the frame's "up" smoothly through each joint
                up = ups[k] if u > 0.0 or k == 0 else (ups[k-1] + ups[k]).normalized()
                ctrl.append((pts[k].lerp(pts[k+1], u), up, r*q*kn, r*q*.90*kn))
        _, s_f = loft_tube(ctrl, seg_u=24, seg_v=16, palmar_flat=0.80, col=SLATE, rough=.58, flag=2, name=f'p_f{i}')
        shell_patch(s_f, 0.10, 0.30, 62.0, 118.0, thick=.0013, nu=5, nv=5, rim_u=.04, rim_d=8.0,
                    col=PAD, rough=.42, flag=4, sharp=True, name=f'p_fpad{i}')
        shell_patch(s_f, 0.55, 0.99, -150.0, -30.0, thick=.0010, nu=6, nv=6, rim_u=.04, rim_d=8.0,
                    col=LEATHER, rough=.52, flag=7, name=f'p_ftip{i}')
        shell_patch(s_f, 0.80, 0.99, 40.0, 140.0, thick=.0010, nu=5, nv=5, rim_u=.04, rim_d=8.0,
                    col=TIP_CAP, rough=.50, flag=7, name=f'p_fcap{i}')
        if i == 0:
            # The crook: on the inside bisector of the PIP joint, a finger radius
            # plus the 1.5 mm wire in from both phalanges.
            u0 = (pts[0] - pts[1]).normalized(); u1 = (pts[2] - pts[1]).normalized()
            b = (u0 + u1).normalized()
            half = math.acos(max(-1.0, min(1.0, u0.dot(u1)))) * 0.5
            hook = pts[1] + b * ((r*.98 + .0016) / max(math.sin(half), .3))
            wire = u0.cross(u1).normalized()                     # across the finger
            opening = ((pts[0] + pts[3]) * 0.5 - hook)
            opening = (opening - wire*opening.dot(wire)).normalized()
    # Thumb: laid along the radial side of the index's proximal phalanx, tip just
    # short of the crook — the hand of someone pulling a key ring, not a fist.
    thenar = [(V3(.016,.002,.010), V3(.5,0,-1), .0165, .0138), (V3(.024,.022,.014), V3(.5,0,-1), .0175, .0142),
              (V3(.029,.040,.017), V3(.5,0,-1), .0145, .0122)]
    loft_tube(thenar, seg_u=12, seg_v=18, palmar_flat=0.86, col=LEATHER, rough=.56, flag=7, name='p_thenar')
    th = [(V3(.027,.026,.014), V3(.6,0,-1), .0148, .0128), (V3(.037,.042,.022), V3(.7,-.1,-1), .0138, .0120),
          (V3(.042,.057,.031), V3(.8,-.2,-1), .0124, .0108), (V3(.042,.070,.039), V3(.9,-.3,-1), .0110, .0097),
          (V3(.039,.080,.045), V3(.9,-.4,-1), .0088, .0079)]
    _, s_th = loft_tube(th, seg_u=18, seg_v=18, palmar_flat=0.84, col=SLATE, rough=.58, flag=2, name='p_thumb')
    shell_patch(s_th, 0.22, 0.56, 40.0, 140.0, thick=.0024, nu=6, nv=6, rim_u=.04, rim_d=7.0,
                col=PAD, rough=.40, flag=4, sharp=True, name='p_th_pad')
    shell_patch(s_th, 0.70, 0.99, -150.0, 150.0, thick=.0012, nu=6, nv=9, rim_u=.035, rim_d=8.0,
                col=TIP_CAP, rough=.52, flag=7, sharp=True, name='p_th_tip')
    f3 = lambda v: '%.5ff, %.5ff, %.5ff' % (v.x, v.y, v.z)
    EXTRA_DEFINES.append('#define HAND_GP_HOOK  {{%s}}   /* the crook of the index */' % f3(hook))
    EXTRA_DEFINES.append('#define HAND_GP_WIRE  {{%s}}   /* the wire\'s line through it */' % f3(wire))
    EXTRA_DEFINES.append('#define HAND_GP_OPEN  {{%s}}   /* out of the hook, toward what it holds */' % f3(opening))

# ------------------------------------------------------------------ fragmentation grenade
NADE_R = .0286    # shell radius: a hair narrower than the .035 half-height, so it reads egg-shaped
NADE_SPOON_TH = -28.0                            # where the lever lies round the body
NADE_E = Vector((0,-.386,.921)).normalized()    # from the palm toward the fingers
NADE_A = Vector((0,.921,.386)).normalized()      # up the fuze
NADE_C = Vector((0,-.0500,-.0078)) + NADE_E*.0370   # the body's centre, clear of the palm

# THE CANISTERS (smoke, incendiary): straight cans on the same grip station and the
# same fuze, so the hand, the pin and the throw are the frag's. (z0, z1, r) of the body.
CAN_SMOKE = (-.0440, .0280, .0235)   # M18-style: tall and slim
CAN_INC   = (-.0440, .0280, .0235)   # the smoke can's size (they share a pouch); domed lid, red bands

def can_r(z, can):
    z0, z1, r = can
    if z < z0 or z > z1 + .0045: return 0.0
    if can is CAN_INC and z > z1: return r - (r - .0165)*min((z - z1)/.0045, 1.0)**.6
    return r if z <= z1 else 0.0

def nade_r(z, can=None):
    '''Outer radius of the grenade at height z along its axis: shell (or can), then fuze neck.'''
    if can is None:
        t = min(abs(z)/.0350, 1.0)
        r = NADE_R*(1-t**2.6)**.46 + .0004
    else:
        r = can_r(z, can) + .0004
    pts = ((.0300,.0172),(.0345,.0142),(.0410,.0126),(.0470,.0118),(.0540,.0090))
    for (z0,r0),(z1,r1) in zip(pts, pts[1:]):
        if z0 <= z <= z1: r = max(r, r0 + (r1-r0)*(z-z0)/(z1-z0))
    return r

def grenade(kind='frag'):
    '''Round-bodied frag grenade on the same grip cylinder as the knife, seated forward of it
    by the body's radius. Group 0 is what flies; group 2 the spoon, which is left behind at
    the throw, and group 3 the pin and ring, which the off hand pulls.
    kind 'smoke' / 'inc' swaps the egg for a canister (CAN_SMOKE / CAN_INC) under the
    same fuze, spoon, pin and ring, so every hand pose and throw still fits.'''
    clear()
    can = {'smoke': CAN_SMOKE, 'inc': CAN_INC}.get(kind)
    R = lambda z: nade_r(z, can)
    def hoop(z0, z1, h):
        n_ = 6
        return [(z0, R(z0)-.0003)] + [(z0+(z1-z0)*i/(n_-1), R(z0+(z1-z0)*i/(n_-1))+h) for i in range(n_)] + [(z1, R(z1)-.0003)]
    if can is None:
        def shell_r(z):
            t = min(abs(z)/.0350, 1.0)
            return NADE_R*(1-t**2.6)**.46 + .0004
        rings = [(.0350*(-1 + 2*i/22), shell_r(.0350*(-1 + 2*i/22))) for i in range(23)]
        finish(lathe(rings, 0, 0, 44), (.24,.26,.175), .62, 0, 0, flag=4)        # muted olive drab: worn service paint, not lime
        # Band and ribs follow the shell's own curve: at a fixed radius they stood off it
        # as floating hoops that read as stripes across the body.
        finish(lathe(hoop(.0090, .0165, .0004), 0, 0, 44), (.68,.39,.12), .58, 0, 0, flag=4)  # blaze band: live, a weathered ochre
        for z in (-.0185, -.0090, .0005, .0225):   # three under the band, one over it
            finish(lathe(hoop(z-.0006, z+.0006, .0004), 0, 0, 44), (.15,.165,.105), .66, 0, 0, flag=4)  # seam ribs, proud enough to catch the light
    elif kind == 'smoke':
        z0, z1, r = CAN_SMOKE
        # A plain drawn can: rolled bottom edge, flat lid, the fuze rising from its middle.
        finish(lathe([(z0, r - .0030), (z0 + .0008, r - .0004), (z0 + .0022, r + .0002), (z0 + .0040, r),
                      (z1 - .0020, r), (z1 - .0004, r - .0006), (z1, r - .0040), (z1, .0140)], 0, 0, 44),
               (.400,.430,.460), .64, 0, .0006, flag=4)                     # grey-blue body
        finish(lathe(hoop(.0010, .0150, .0003), 0, 0, 44), (.880,.880,.860), .60, 0, 0, flag=4)  # the white band (the label)
        finish(lathe(hoop(z1 - .0060, z1 - .0030, .0003), 0, 0, 44), (.880,.880,.860), .60, 0, 0, flag=4)  # a thin one under the lid
        # Four emission ports round the lid: where the smoke comes out.
        for i in range(4):
            a_ = math.pi/4 + i*math.pi/2
            finish(lathe([(z1 - .0004, .0036), (z1 + .0006, .0036)], .0185*cos(a_), .0185*sin(a_), 20),
                   (.070,.072,.078), .80, 0, 0, flag=4)
        finish(lathe([(z0 - .0002, r - .0020), (z0 + .0010, r - .0020)], 0, 0, 44), (.240,.260,.290), .70, 0, 0, flag=4)  # base cap
    else:
        z0, z1, r = CAN_INC
        dome = [(z1 + .0045*i/6, can_r(z1 + .0045*i/6, CAN_INC)) for i in range(1, 7)]
        finish(lathe([(z0, r - .0030), (z0 + .0010, r - .0004), (z0 + .0030, r), (z1, r)] + dome + [(z1 + .0045, .0140)], 0, 0, 44),
               (.300,.305,.285), .66, 0, .0006, flag=4)                     # dark grey body
        # Rolled seams, proud of the body top and bottom: the can's silhouette.
        for zc in (z0 + .0032, z1 - .0008):
            finish(lathe([(zc - .0016, r - .0002), (zc - .0010, r + .0016), (zc + .0010, r + .0016), (zc + .0016, r - .0002)], 0, 0, 44),
                   (.220,.225,.215), .52, .3, .0004, flag=4)
        finish(lathe(hoop(.0005, .0115, .0003), 0, 0, 44), (.660,.110,.070), .58, 0, 0, flag=4)   # the red band: fire
        finish(lathe(hoop(-.0150, -.0120, .0003), 0, 0, 44), (.660,.110,.070), .58, 0, 0, flag=4)
    finish(lathe([(.0280,.0140),(.0300,.0176),(.0345,.0146),(.0410,.0126),(.0470,.0118),(.0495,.0086)], 0, 0, 44), GRAPH, .32, .75, 0)  # fuze well: bare metal against the matte paint
    finish(lathe([(.0470,.0092),(.0530,.0088),(.0540,.0060)], 0, 0, 24), STEEL, .36, .9, .0003)  # striker cap
    # The spoon: stamped sheet 1.5 mm thick. Its cap sits flush on the striker, then it
    # drops tight down the neck and shell, with only a hair of air. -y is the palm side.
    zs = [.0300 - i*.0030 for i in range(21)]
    if can is not None:   # down the can's flat side: the lever lies straight
        zs = [z for z in zs if z >= can[0] + .0040]
    out = [(.0572,.0070),(.0572,-.0100)] + [(.0520,-.0138),(.0460,-.0142),(.0400,-.0152),(.0340,-.0172)] + [(z,-(R(z)+.0021)) for z in zs]
    inn = [(z,-(R(z)+.0006)) for z in zs][::-1] + [(.0340,-.0157),(.0400,-.0136),(.0460,-.0126),(.0505,-.0122),(.0552,-.0086),(.0552,.0070)]
    sp = finish(prof(out + inn, -.0092, .0092, 'spoon'), (.38,.38,.37), .46, .62, .0004, 2, group=2)   # dull gunmetal
    psi = radians(-NADE_SPOON_TH)     # about the fuze axis: the side under the thumb
    for v in sp.data.vertices:
        x_, y_ = v.co.x, v.co.y
        v.co.x = x_*cos(psi) - y_*sin(psi); v.co.y = x_*sin(psi) + y_*cos(psi)
    # Pin through the fuze, ring on the LEFT: where the off hand reaches to pull it.
    finish(pin(-.0170,.0200,.0030,.0445,.0015,14), STEEL, .34, .92, 0, group=3)
    bm = bmesh.new()   # ring: a torus swept by hand
    # 25 mm across, as a real ring is: at 20 mm a gloved finger could not pass it.
    n, m = 36, 10; R_, r_ = .0125, .0015; vs = []
    for i in range(n):
        a_ = 2*math.pi*i/n; row = []
        for j in range(m):
            b_ = 2*math.pi*j/m; k = R_ + r_*cos(b_)
            row.append(bm.verts.new((.0315 + k*cos(a_), .0030 + r_*sin(b_), .0445 + k*sin(a_))))
        vs.append(row)
    for i in range(n):
        for j in range(m):
            bm.faces.new((vs[i][j], vs[(i+1)%n][j], vs[(i+1)%n][(j+1)%m], vs[i][(j+1)%m]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    # Its own group: the ring hangs from the pin's eye and swings on it, so the
    # pin hand can take it at whatever angle its arm arrives from.
    finish(from_bm(bm, 'ring'), STEEL, .34, .92, 0, group=4)
    for o in bpy.context.collection.objects:
        for v in o.data.vertices:
            # -x keeps this frame right-handed: a mirrored frame turns the mesh inside out.
            q = v.co.copy(); v.co = NADE_C + Vector((-q.x,0,0)) + NADE_E*q.y + NADE_A*q.z

# ------------------------------------------------------------------ grenade hand (HAND_G)
HAND_G_EXPORTED = False
def hand_g():
    '''Anatomical right-hand tactical glove cradling the fragmentation grenade upright:
    - Wrist sits behind-right-below the shell (ulnar/dorsal alignment matching HAND_R),
      cuff tucked cleanly inside BRACER's sleeve with a crisp Velcro wrist strap.
    - Muscular palm cups the right-rear flank (th 34..82 deg) with tailored dorsal softshell,
      dark seam piping, and two-tier bevelled knuckle plate matching HAND_GP / HAND_KO.
    - Four snugly-packed, jointed fingers wrap around the front onto the left flank of the
      shell belly (all well above the south pole, zero air gaps, straight phalanx bones
      between swelled MCP/PIP/DIP joints, palmar grip leather and suede distal caps).
    - Plump thenar cushion + articulated thumb rising onto the spoon shoulder.'''
    global HAND_G_EXPORTED
    clear()
    X = V3(1,0,0)
    def rad(th):   # outward from the body's axis at angle th
        t = radians(th); return (-NADE_E*cos(t) + X*sin(t)).normalized()
    def S(th, w, off):   # th degrees, w along the axis, off metres clear of the shell
        return NADE_C + rad(th)*(nade_r(w)+off) + NADE_A*w
    def CYL(th, w, rc):  # cylindrical radius from axis (smooth below the south pole)
        return NADE_C + rad(th)*rc + NADE_A*w

    # 1. Wrist & Velcro cuff on the true right-hand carpal station (behind-right-below).
    K_MID = S(78, .0030, .0088)
    W_R = CYL(42, -.066, .0385)
    palm_back = (W_R - K_MID).normalized()
    AX_G = (palm_back*0.93 + rad(50)*0.24).normalized()
    if not HAND_G_EXPORTED:
        f3 = lambda v: '%.5ff, %.5ff, %.5ff' % (v.x, v.y, v.z)
        EXTRA_DEFINES.append('#define HAND_G_WRIST {{%s}}   /* grenade right wrist */' % f3(W_R))
        EXTRA_DEFINES.append('#define HAND_G_AX    {{%s}}   /* wrist -> elbow axis */' % f3(AX_G))
        HAND_G_EXPORTED = True

    N_W = (rad(52) - AX_G*rad(52).dot(AX_G)).normalized()
    W_SIDE = (NADE_A - AX_G*NADE_A.dot(AX_G) - N_W*NADE_A.dot(N_W)).normalized()
    # Cuff tucks +12 mm into BRACER's soft inner sleeve (well clear of the +31 mm hard mouth)
    # and runs forward into the wrist crease where the Velcro strap wraps cleanly.
    cuff = [(W_R + AX_G*.016, N_W, .0278, .0224),
            (W_R - AX_G*.002, N_W, .0284, .0228),
            (W_R - AX_G*.019, N_W, .0292, .0234)]
    _, s_cuff = loft_tube(cuff, seg_u=10, seg_v=20, palmar_flat=0.94, col=SLATE, rough=.60, flag=2, name='g_cuff')
    shell_patch(s_cuff, 0.14, 0.82, -25.0, 205.0, thick=.0025, bury=.0010, nu=6, nv=10, rim_u=.06, rim_d=7.0,
                col=LEATHER, rough=.52, flag=7, sharp=True, name='g_strap')

    # 2. Spherical-cupping palm: cross-sections curve concentrically around the grenade shell
    #    and align their width axis with the 4-MCP knuckle arch (NADE_A), eliminating flat
    #    tangent wings behind the thumb while hugging the right-rear flank with zero air gap.
    def loft_cup_palm(p_ctrl, seg_u=18, seg_v=22, pf=0.82, power=2.15):
        ex = 2.0 / power
        def spow(x, p): return math.copysign(abs(x) ** p, x)
        def eval_p(u):
            n = len(p_ctrl); u = max(0.0, min(1.0, u))
            s = u * (n - 1); i = min(int(s), n - 2); t = s - i
            def g(idx):
                if idx < 0:
                    a0, b0 = p_ctrl[0], p_ctrl[1]
                    return (a0[0]*2 - b0[0], a0[1], a0[2], a0[3], a0[4], a0[5])
                if idx >= n:
                    a0, b0 = p_ctrl[-1], p_ctrl[-2]
                    return (a0[0]*2 - b0[0], a0[1], a0[2], a0[3], a0[4], a0[5])
                return p_ctrl[idx]
            a, b, c, d = g(i-1), g(i), g(i+1), g(i+2)
            pos = _cr_vec(a[0], b[0], c[0], d[0], t)
            up  = _cr_vec(a[1], b[1], c[1], d[1], t).normalized()
            sw  = _cr_vec(a[2], b[2], c[2], d[2], t)
            W   = (sw - up * sw.dot(up)).normalized()
            rx  = max(.001, _cr_scalar(a[3], b[3], c[3], d[3], t))
            ry  = max(.001, _cr_scalar(a[4], b[4], c[4], d[4], t))
            cr  = max(.020, _cr_scalar(a[5], b[5], c[5], d[5], t))
            return pos, up, W, rx, ry, cr
        def sample(u, deg, lift=0.0):
            pos, U, W, rx, ry, cr = eval_p(u)
            th = radians(deg)
            ct, st = spow(cos(th), ex), spow(sin(th), ex)
            ry_e = ry * (pf if st < 0.0 else 1.0)
            s = rx * ct; h = ry_e * st
            phi = s / cr; R = cr + h
            cp, sp_ = cos(phi), sin(phi)
            pt = (pos - U * cr) + W * (R * sp_) + U * (R * cp)
            W_r = W * cp - U * sp_; U_r = W * sp_ + U * cp
            nrm = (W_r * (cos(th) / rx) + U_r * (sin(th) / ry_e)).normalized()
            return pt + nrm * lift, nrm
        bm = bmesh.new(); rings = []
        pos0, _, _, rx0, ry0, _ = eval_p(0.0)
        pos0b, _, _, _, _, _ = eval_p(0.04)
        T0 = (pos0b - pos0).normalized()
        pole0 = bm.verts.new(pos0 - T0 * (0.38 * min(rx0, ry0)))
        for c_ang in (0.40, 0.74):
            sc = sin(c_ang * math.pi * 0.5); ax = cos(c_ang * math.pi * 0.5) * 0.38 * min(rx0, ry0)
            rings.append([bm.verts.new(pos0 + (sample(0.0, 360.0*iv/seg_v)[0] - pos0)*sc - T0*ax) for iv in range(seg_v)])
        for iu in range(seg_u + 1):
            rings.append([bm.verts.new(sample(iu/seg_u, 360.0*iv/seg_v)[0]) for iv in range(seg_v)])
        pos1, _, _, rx1, ry1, _ = eval_p(1.0)
        pos1a, _, _, _, _, _ = eval_p(0.96)
        T1 = (pos1 - pos1a).normalized()
        for c_ang in (0.70, 0.38):
            sc = sin(c_ang * math.pi * 0.5); ax = cos(c_ang * math.pi * 0.5) * 0.52 * min(rx1, ry1)
            rings.append([bm.verts.new(pos1 + (sample(1.0, 360.0*iv/seg_v)[0] - pos1)*sc + T1*ax) for iv in range(seg_v)])
        pole1 = bm.verts.new(pos1 + T1 * (0.52 * min(rx1, ry1)))
        for k in range(len(rings) - 1):
            r0, r1 = rings[k], rings[k+1]
            for iv in range(seg_v):
                jv = (iv + 1) % seg_v
                bm.faces.new((r0[iv], r1[iv], r1[jv], r0[jv]))
        for iv in range(seg_v):
            jv = (iv + 1) % seg_v
            bm.faces.new((pole0, rings[0][jv], rings[0][iv]))
            bm.faces.new((rings[-1][iv], pole1, rings[-1][jv]))
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        return finish(from_bm(bm, 'g_palm'), LEATHER, .58, 0, bevel=0.0, flag=7), sample

    SW_MID = (W_SIDE*0.42 + NADE_A*0.58).normalized()
    palm = [(W_R - AX_G*.014,       N_W,     W_SIDE, .0258, .0176, .120),
            (CYL(58, -.036, .0378), rad(60), SW_MID, .0268, .0140, .052),
            (S(72,  -.013, .0090),  rad(73), NADE_A, .0270, .0116, .040),
            (S(79,   .000, .0084),  rad(79), NADE_A, .0256, .0102, .038)]
    _, s_palm = loft_cup_palm(palm)
    shell_patch(s_palm, 0.08, 0.96, 18.0, 162.0, thick=.0016, nu=8, nv=8, rim_u=.04, rim_d=7.0,
                col=SLATE, rough=.56, flag=2, sharp=True, name='g_dorsal')
    shell_patch(s_palm, 0.68, 0.96, 28.0, 152.0, thick=.0020, nu=6, nv=8, rim_u=.05, rim_d=8.0,
                col=PAD_RIM, rough=.52, flag=4, sharp=True, name='g_kn_bezel')
    shell_patch(s_palm, 0.71, 0.93, 36.0, 144.0, thick=.0034, nu=6, nv=8, rim_u=.05, rim_d=7.0,
                col=PAD, rough=.40, flag=4, sharp=True, name='g_kn_plate')
    shell_patch(s_palm, 0.14, 0.64, 84.0, 96.0, thick=.0008, nu=5, nv=3, rim_u=.04, rim_d=3.0,
                col=SEAM, rough=.62, flag=4, name='g_seam')

    # Hypothenar heel cushion along the ulnar base so the lower-right silhouette curves naturally.
    hypo = [(CYL(56, -.050, .0372), rad(62), .0168, .0134),
            (CYL(70, -.034, .0375), rad(74), .0176, .0136),
            (S(80,  -.020, .0084),  rad(82), .0142, .0112)]
    loft_tube(hypo, seg_u=12, seg_v=16, palmar_flat=0.86, col=LEATHER, rough=.56, flag=7, name='g_hypo')

    # 3. Four snugly-packed, jointed fingers wrapping across the front onto the left flank.
    #    Palmar skin stays -0.6 mm tucked against the shell along 100% of every finger (zero
    #    air gap even at the upper shoulder), while PIP/DIP swell projects purely dorsally.
    rows = ((( .0176,  .0184,  .0178,  .0164), .0095, (78, 134, 178, 212)),   # index
            (( .0050,  .0056,  .0052,  .0042), .0100, (76, 138, 184, 222)),   # middle
            ((-.0076, -.0073, -.0075, -.0080), .0095, (78, 136, 180, 215)),   # ring
            ((-.0192, -.0190, -.0188, -.0182), .0086, (82, 132, 172, 202)))   # little
    PF = 0.80
    for i, (ws, r, ths) in enumerate(rows):
        rr = (1.07, 1.00, 0.90, 0.76)
        ctrl = []
        for k in range(3):
            us = (0.0, 0.36, 0.70) if k < 2 else (0.0, 0.36, 0.70, 1.0)
            for u in us:
                t_ = ths[k] + (ths[k+1] - ths[k]) * u
                w_ = ws[k] + (ws[k+1] - ws[k]) * u
                q_ = rr[k] + (rr[k+1] - rr[k]) * u
                joint = (u == 0.0 and k > 0)
                q_rx = q_ * (1.05 if joint else 0.98)
                q_ry = q_ * 0.90 * (1.12 if joint else 0.93)
                ry_ = r * q_ry
                p_ = S(t_, w_, ry_ * PF - .0006)
                ctrl.append((p_, rad(t_), r * q_rx, ry_))
        _, s_f = loft_tube(ctrl, seg_u=26, seg_v=16, palmar_flat=PF, col=SLATE, rough=.58, flag=2, name=f'g_f{i}')
        shell_patch(s_f, 0.08, 0.28, 52.0, 128.0, thick=.0013, nu=5, nv=5, rim_u=.04, rim_d=8.0,
                    col=PAD, rough=.40, flag=4, sharp=True, name=f'g_kn{i}')
        shell_patch(s_f, 0.16, 0.90, -150.0, -30.0, thick=.0008, bury=.0006, nu=6, nv=6, rim_u=.04, rim_d=8.0,
                    col=LEATHER, rough=.52, flag=7, name=f'g_fgrip{i}')
        shell_patch(s_f, 0.80, 0.98, 40.0, 140.0, thick=.0009, bury=.0006, nu=5, nv=5, rim_u=.04, rim_d=8.0,
                    col=TIP_CAP, rough=.50, flag=7, name=f'g_fcap{i}')

    # 4. Plump Thenar grip cushion + articulated Thumb pressing the spoon down cleanly.
    SP = NADE_SPOON_TH
    thenar = [(CYL(36, -.050, .0370), rad(34), .0172, .0134),
              (CYL(16, -.033, .0358), rad(16), .0180, .0138),
              (S(0,    -.018, .0084), rad(0),  .0148, .0112)]
    loft_tube(thenar, seg_u=12, seg_v=18, palmar_flat=0.88, col=LEATHER, rough=.54, flag=7, name='g_thenar')

    def tp(th_, w, ry, extra=0.0):
        return S(th_, w, .0013 + ry * 0.84 + extra)
    th = [(CYL(12,  -.034, .0356),        rad(12),   .0148, .0124),   # CMC base inside thenar
          (tp(-10,  -.020, .0114, .0004), rad(-10),  .0138, .0114),   # MCP knuckle bend
          (tp(SP+5, -.009, .0104),        rad(SP+5), .0128, .0104),   # proximal phalanx onto spoon
          (tp(SP+3,  .002, .0100),        rad(SP+3), .0124, .0100),
          (tp(SP+2,  .011, .0104, .0005), rad(SP+2), .0126, .0104),   # IP joint swell
          (tp(SP+2,  .019, .0094),        rad(SP+2), .0114, .0094),   # distal pad pressing shoulder
          (tp(SP+3,  .0255,.0076,-.0004), rad(SP+3), .0092, .0076)]   # rounded thumb tip
    _, s_th = loft_tube(th, seg_u=26, seg_v=18, palmar_flat=0.84, col=SLATE, rough=.58, flag=2, name='g_thumb')
    shell_patch(s_th, 0.18, 0.52, 42.0, 138.0, thick=.0018, nu=6, nv=6, rim_u=.04, rim_d=7.0,
                col=PAD_RIM, rough=.52, flag=4, sharp=True, name='g_th_bezel')
    shell_patch(s_th, 0.21, 0.49, 50.0, 130.0, thick=.0030, nu=6, nv=6, rim_u=.04, rim_d=7.0,
                col=PAD, rough=.40, flag=4, sharp=True, name='g_th_pad')
    shell_patch(s_th, 0.22, 0.88, -152.0, -24.0, thick=.0008, bury=.0006, nu=6, nv=6, rim_u=.04, rim_d=8.0,
                col=LEATHER, rough=.50, flag=7, name='g_th_grip')
    shell_patch(s_th, 0.76, 0.98, -145.0, 145.0, thick=.0010, bury=.0006, nu=6, nv=8, rim_u=.035, rim_d=8.0,
                col=TIP_CAP, rough=.52, flag=7, sharp=True, name='g_th_tip')

# ------------------------------------------------------------------ rifle & pistol variants
# Every variant is authored in its slot's own frame and keeps the stations the game's
# rig is built on: the AR grip, trigger guard and blade (the sculpted firing hand closes
# on them unchanged), the fore-end round z .264 (the support hand), the holo at the AR's
# station (ADS), and for the pistols the rear sight at z -.0405 with its top at y .0342.
# Groups: 1 magazine, 2 stock (hidden when aiming / reloading), 3 the pistol slide.
WOOD=(.255,.150,.090); WOOD_D=(.190,.115,.072); BLUED=(.155,.158,.168); STAINLESS=(.60,.60,.62)
POLY=(.115,.118,.125); WHITE_DOT=(.92,.92,.88)

def ar_grip(grip_col=BLACK, guard_col=None, grip_rough=.76, guard_rough=.60, guard_metal=0.0):
    '''The AR's pistol grip, trigger guard and blade, at exactly the AR's stations.'''
    if guard_col is None: guard_col = TAN   # resolved at call: the factory palette swaps it
    finish(prof([(.030,-.014),(-.008,-.013),(-.016,-.026),(-.036,-.100),(-.030,-.110),(.000,-.106),(.010,-.066),(.018,-.044)], -.0126, .0126, 'grip'), grip_col, grip_rough, 0, .0052, 5)
    finish(prof([(.014,-.018),(.019,-.018),(.021,-.0475),(.068,-.0475),(.079,-.018),(.084,-.018),(.071,-.0518),(.014,-.0518)], -.0038, .0038), guard_col, guard_rough, guard_metal, bevel=.0007)
    finish(prof([(.044,-.016),(.050,-.016),(.044,-.042),(.039,-.042)], -.0030, .0030), DARK, .45, .6, .0005)

def rot_x(o, ang, pivot):
    '''Rotate an object's mesh about the x axis through pivot (y, z).'''
    py, pz = pivot; c, s = cos(ang), sin(ang)
    for v in o.data.vertices:
        y, z = v.co.y - py, v.co.z - pz
        v.co.y = py + y*c - z*s; v.co.z = pz + y*s + z*c
    return o

def kobra_sight():
    """The AK's own red dot: the Russian Kobra EKP-1S-03 collimator (Axion, 144 x 66 x
    144 mm with its mount). It does not sit on a top rail: it clamps to the AK's LEFT
    side dovetail — the skeletal side mount on this receiver — with a long throw lever,
    and a bracket bridges over the dust cover to a long, low housing above the receiver.
    The reticle selector is a ribbed knob on the right of the housing, the battery cap
    on its left, and the front carries Kobra's signature tall open window in a thick
    frame whose front leans back from the lens' foot to its top. The window keeps the holo's centre and size
    (VM_HOLO_Y, VM_HOLO_R, 1.18 R wide, front at .172), so ADS, the eye line and the
    HUD dot's clip are unchanged; the housing stays below the sight line."""
    cy = .0741; hh = .0133; hw = .0157
    zf = .172                                          # front of the frame
    # --- 0.8.3: CENTRED ON A RAILED DUST COVER. The side mount clamped to the
    # left dovetail put a slab of plate, a jaw and a long lever down one flank
    # of the gun only, and in a viewmodel that is the flank you look at. The
    # sight now rides a Picatinny rail along the top of a railed dust cover (the
    # common modern AK fit), on a low cross-bolt base with a nut either side.
    finish(box((0, .0420, .104), (.0088, .0024, .0560)), BLACK, .55, .1, .0006, 2)               # rail spine
    for i in range(12):
        finish(box((0, .0449, .055 + i * .0088), (.0094, .0010, .0024)), BLACK, .55, .1, .0002, 1)   # rail teeth
    finish(box((0, .0472, .100), (.0120, .0022, .0320)), BLACK, .58, .05, .0008, 2)               # the sight's base
    for s_ in (-1, 1):
        finish(pin(*sorted((s_ * .0118, s_ * .0150)), .0440, .088, .0034, 14), BLUED, .42, .6, .0004)   # cross-bolt nuts
        finish(pin(*sorted((s_ * .0118, s_ * .0150)), .0440, .112, .0034, 14), BLUED, .42, .6, .0004)
    # --- the housing: long and low over the receiver, chamfered at the back
    hb = prof([(.052, .0450), (.150, .0450), (.150, .0592), (.066, .0592), (.052, .0540)], -.0135, .0135, 'kobra_body')
    finish(hb, GRAPH, .60, .06, .0024, 3)
    finish(box((0, .0596, .100), (.0090, .0010, .0300)), BLACK, .62, 0, .0004)                 # top seam plate
    # reticle selector on the right (ribbed), battery cap on the left
    finish(pin(.0135, .0215, .0520, .078, .0085, 22), BLACK, .55, .1, .0006)
    for i in range(12):
        a = 2 * math.pi * i / 12
        finish(box((.0178, .0520 + .0086 * cos(a), .078 + .0086 * sin(a)), (.0036, .0011, .0011)), BLACK, .55, .1, 0, 1)
    finish(pin(.0215, .0228, .0520, .078, .0030, 12), (.85, .38, .10), .30, .2, 0, flag=3)     # the selector's index
    finish(pin(-.0200, -.0135, .0520, .118, .0068, 18), DARK, .48, .2, .0006)                  # battery cap
    finish(box((.0137, .0505, .118), (.0010, .0030, .0060)), BLACK, .55, 0, .0003)             # brightness button
    # --- the window frame: tall, open, its front leaning back
    ow = hw + .0052; o_lo = .0450; o_hi = cy + hh + .0060
    fr = prof([(.136, o_lo), (zf, o_lo), (zf, cy - hh), (zf - .0150, o_hi), (.142, o_hi), (.136, o_hi - .0060)], -ow, ow, 'kobra_frame')
    cut(fr, prof_front([(-hw, cy - hh), (hw, cy - hh), (hw, cy + hh), (-hw, cy + hh)], .10, .20))
    finish(fr, HOLO, .52, .08, .0020, 3)
    # the coated lens' edge inside the window, what makes it read as glass
    for (x, y, hx, hy) in ((0, cy - hh + .0006, hw, .0006), (0, cy + hh - .0006, hw, .0006),
                           (hw - .0006, cy, .0006, hh), (-hw + .0006, cy, .0006, hh)):
        finish(box((x, y, .150), (hx, hy, .0010)), (.85, .38, .10), .25, .9, 0)

def ak_stock():
    """M4-type collapsible stock on a buffer tube, behind an AK stock adapter (group 2
    for the stock, the tube stays)."""
    finish(box((0,.012,-.044), (.0150,.0150,.0110)), BLUED, .46, .5, .0012)                    # adapter
    finish(lathe([(-.250,.0168),(-.050,.0168)], 0, .0200, 28), GRAPH, .44, .18, .0005)          # tube
    finish(lathe([(-.064,.0225),(-.052,.0225),(-.049,.0190)], 0, .0200, 28), BLACK, .48, .15, .0005)   # castle nut
    O = -.020
    st = prof([(-.098+O,.0435),(-.226+O,.0435),(-.240+O,.034),(-.244+O,-.044),(-.228+O,-.054),(-.196+O,-.040),(-.146+O,-.014),(-.100+O,.001),(-.092+O,.022)], -.0200, .0200, 'stock')
    cut(st, prof([(-.222+O,.032),(-.212+O,.032),(-.212+O,-.032),(-.222+O,-.038)], -.03, .03))
    cut(st, prof([(-.112+O,.004),(-.180+O,.030),(-.192+O,.030),(-.192+O,.020),(-.124+O,-.006)], -.03, .03))   # the angled slot
    finish(st, TAN, .64, 0, .0034, 3, group=2, flag=4)
    finish(box((0,-.005,-.249+O), (.0190,.0490,.0060)), BLACK, .9, 0, .0022, 2, group=2)       # butt pad
    finish(box((0,-.016,-.120+O), (.0060,.0060,.0120)), BLACK, .6, 0, .0008, group=2)          # adjustment lever

def glow(c, h):
    """A lit amber inlay (the shader's flag 3: the gauntlet's LED family). Not on
    a gun built under its real factory finish (see PALETTES)."""
    if _LIGHT is not None: return None
    finish(box(c, h), AMBER, .40, 0, .0003, flag=3)

AK_WOOD = (.470, .275, .145)       # oiled birch laminate, the AKM's furniture
AK_WOOD_D = (.350, .200, .105)
AK_BAKELITE = (.330, .150, .085)   # the reddish-brown moulded pistol grip

def ak_wood_stock():
    """THE AKM's FIXED WOODEN STOCK (group 2), after the reference photographs: the
    comb line falls gently from the receiver to the heel, the belly drops away
    from the wrist to a tall butt, a steel butt plate with its trap door, the
    sling loop under the wrist. Width tapers by building the butt in two slabs."""
    # the comb falls from the receiver line to the heel, the belly from the wrist
    # to the toe: the AKM's drop, about 4 cm at the heel at this scale
    pts = [(-.030,.0225),(-.070,.0190),(-.130,.0120),(-.200,.0040),(-.262,-.0010),(-.268,-.0050),
           (-.268,-.0880),(-.258,-.0920),(-.200,-.0700),(-.130,-.0440),(-.075,-.0250),(-.040,-.0120),(-.030,-.0040)]
    st = prof(pts, -.0165, .0165, 'stock_core')
    finish(st, AK_WOOD, .58, 0, .0034, 3, group=2, flag=4)
    # steel butt plate and its trap-door hinge
    bp = prof([(-.2660,-.0010),(-.2735,-.0020),(-.2735,-.0900),(-.2620,-.0935),(-.2600,-.0900),(-.2660,-.0880)], -.0175, .0175, 'buttplate')
    finish(bp, BLUED, .48, .55, .0010, 2, group=2)
    finish(pin(-.0060, .0060, -.0420, -.2700, .0030, 14), BLUED, .44, .6, .0003, group=2)
    # stock tang into the receiver and its screw
    finish(box((0, .0090, -.0420), (.0152, .0140, .0090)), BLUED, .46, .5, .0008, group=2)
    for s_ in (-1, 1):
        finish(pin(*sorted((s_*.0150, s_*.0160)), .0080, -.0420, .0022, 12), BLUED, .42, .6, .0002, 1, group=2)
    # sling loop under the wrist
    finish(prof([(-.090,-.0290),(-.078,-.0270),(-.078,-.0340),(-.090,-.0360)], -.0034, .0034, 'sling'), BLUED, .45, .6, .0006, group=2)

def rifle_ak():
    """THE AK-47 — the classic AKM after the reference photographs, not the tan
    tactical rebuild it replaced: a phosphate-black stamped receiver with its
    rivets, the ribbed stamped dust cover, oiled laminate handguards upper and
    lower, the gas tube and its 45-degree gas block, the triangular front sight
    with bayonet lug and cleaning rod, the slant muzzle brake, a bakelite grip,
    the fixed wooden stock, the ribbed steel magazine — and the Kobra centred on
    the dust cover's rail (the holo station ADS is built on). Stations unchanged:
    magwell z .118, the AR grip, the support hand round z .264 y .019."""
    clear()
    # --- stamped receiver, magwell, rivets, trunnion
    rc = prof([(-.034,.024),(.205,.024),(.205,-.008),(.150,-.016),(.150,-.020),(.088,-.020),(.088,-.016),(.040,-.016),(.010,-.018),(-.012,-.014),(-.034,-.004)], -.0150, .0150, 'rcv')
    cut(rc, box((0,-.018,.118), (.0110,.010,.0240)))
    finish(rc, TAN, .52, 0, bevel=.0010, seg=2, flag=6)
    # the stamped dimple over the magazine and the rail of the trigger group
    for s_ in (-1, 1):
        finish(box((s_*.0151, .0050, .118), (.0006, .0040, .0100)), SAND, .55, 0, .0004, 1, flag=6)
    for z in (.020,.060,.100,.180,.196):
        for x0, x1 in ((.0144, .0158), (-.0158, -.0144)):
            finish(pin(x0, x1, .002, z, .0016, 12), BLUED, .42, .6, .0002, 1)
    finish(pin(-.0160, .0160, -.0040, .040, .0018, 12), BLUED, .42, .6, .0002, 1)        # trigger pins
    finish(pin(-.0160, .0160, .0060, .012, .0018, 12), BLUED, .42, .6, .0002, 1)
    finish(box((0,.006,.208), (.0158,.0180,.0060)), BLUED, .45, .6, .0006)                # front trunnion
    # --- the stamped dust cover: domed, right-side bolt carrier opening, three stiffening ribs
    dc = prof_front([(-.0150,.0230),(.0150,.0230),(.0150,.0300),(.0128,.0356),(.0080,.0386),(.0000,.0394),(-.0080,.0386),(-.0128,.0356),(-.0150,.0300)], -.026, .186, 'cover')
    cut(dc, box((.0145, .0255, .122), (.0050, .0065, .0480)))                              # right ejection/carrier slot
    finish(dc, TAN, .52, 0, .0012, 2, flag=6)
    for z in (.040, .090, .140):
        rb = prof_front([(-.0154,.0290),(.0154,.0290),(.0132,.0360),(.0082,.0392),(.0000,.0400),(-.0082,.0392),(-.0132,.0360)], z - .0040, z + .0040, 'rib')
        if z > .060: cut(rb, box((.0155, .0255, .122), (.0050, .0065, .0480)))
        finish(rb, TAN, .52, 0, .0010, 2, flag=6)
    finish(pin(-.0040, .0040, .0300, -.031, .0030, 14), BLUED, .40, .6, .0004)             # recoil spring button
    # --- the rear sight: base block, the tangent leaf and its slider
    finish(prof([(.186,.0240),(.214,.0240),(.214,.0380),(.200,.0400),(.186,.0390)], -.0112, .0112), BLUED, .45, .6, .0008)
    finish(prof([(.188,.0380),(.212,.0400),(.212,.0436),(.188,.0412)], -.0074, .0074, 'leaf'), BLUED, .42, .6, .0005)
    finish(box((0,.0428,.204), (.0086,.0026,.0040)), BLUED, .42, .6, .0004)
    finish(box((0,.0450,.2105), (.0040,.0012,.0010)), BLACK, .5, 0, .0002)
    # --- bolt carrier in the right slot, authentic upturned AK charging handle & right-side dust-cover safety lever
    finish(box((.0124,.0245,.124), (.0026,.0050,.0440)), STEEL, .36, .85, .0004, 1)        # bolt carrier
    finish(prof([(.138,.0225),(.156,.0225),(.158,.0275),(.142,.0285)], .0140, .0280, 'ch', r=0), BLUED, .38, .7, .0006)
    sel = prof([(-.006,.0140),(.002,.0190),(.108,.0090),(.108,.0030),(.010,.0070),(-.006,.0070)], .0150, .0168, 'selector', r=0)
    finish(sel, BLUED, .40, .7, .0003)
    finish(box((.0185,.0065,.104), (.0022,.0035,.0060)), BLUED, .40, .7, .0003)            # selector finger shelf
    finish(pin(.0150, .0182, .0130, .000, .0040, 14), BLUED, .40, .7, .0003)
    # --- AK paddle magazine release behind magwell
    finish(prof([(.084,-.018),(.089,-.018),(.086,-.037),(.081,-.037)], -.0050, .0050, 'ak_mrel', r=0), BLUED, .42, .6, .0004)
    # --- LOWER HANDGUARD (wood): palm swells and steel ferrule/retainer
    lo = prof_front([(-.0205,-.0040),(.0205,-.0040),(.0236,.0030),(.0250,.0150),(.0258,.0225),(.0248,.0272),(-.0248,.0272),(-.0258,.0225),(-.0250,.0150),(-.0236,.0030)], .213, .343, 'hgl')
    finish(lo, SAND, .56, 0, .0030, 3, flag=4)
    for s_ in (-1, 1):
        finish(box((s_*.0254, .0120, .278), (.0008, .0010, .0560)), AK_WOOD_D, .6, 0, .0002, flag=4)
    finish(prof_front([(-.0215,-.0050),(.0215,-.0050),(.0262,.0060),(.0262,.0282),(-.0262,.0282),(-.0262,.0060)], .205, .213, 'ferrule'), BLUED, .45, .6, .0008)
    finish(prof_front([(-.0200,-.0050),(.0200,-.0050),(.0252,.0060),(.0252,.0282),(-.0252,.0282),(-.0252,.0060)], .343, .352, 'retainer'), BLUED, .45, .6, .0008)
    finish(box((.0252, .0140, .3475), (.0016, .0050, .0030)), BLUED, .42, .6, .0003)
    # --- UPPER HANDGUARD (wood) over the gas tube
    up = prof_front([(-.0150,.0272),(.0150,.0272),(.0162,.0340),(.0140,.0420),(.0085,.0470),(-.0085,.0470),(-.0140,.0420),(-.0162,.0340)], .216, .328, 'hgu')
    finish(up, SAND, .56, 0, .0026, 3, flag=4)
    finish(prof_front([(-.0156,.0270),(.0156,.0270),(.0168,.0340),(.0146,.0424),(.0090,.0476),(-.0090,.0476),(-.0146,.0424),(-.0168,.0340)], .208, .216, 'hgu_cap'), BLUED, .45, .6, .0006)
    finish(lathe([(.326,.0074),(.364,.0074)], 0, .0400, 20), BLUED, .44, .6, .0004)
    # 45-degree AKM gas block straddling barrel cleanly
    finish(lathe([(.352,.0120),(.374,.0120)], 0, .0200, 22), BLUED, .44, .6, .0006)
    gb = prof([(.352,.0240),(.374,.0240),(.376,.0470),(.362,.0470)], -.0084, .0084, 'gasblock')
    finish(gb, BLUED, .44, .6, .0008)
    # --- barrel, cleaning rod, front sight block, bayonet lug, slant brake
    finish(lathe([(.340,.0090),(.464,.0082)], 0, .0200, 22), BLUED, .40, .6, .0003)
    finish(lathe([(.352,.0022),(.440,.0022)], 0, .0060, 12), BLUED, .40, .7, .0002)
    finish(lathe([(.440,.0030),(.446,.0030)], 0, .0060, 12), BLUED, .40, .7, .0002)
    fs = prof([(.416,.0040),(.446,.0040),(.446,.0270),(.440,.0300),(.436,.0480),(.426,.0480),(.420,.0300),(.416,.0270)], -.0060, .0060, 'fsb')
    finish(fs, BLUED, .44, .6, .0006)
    for s_ in (-1,1):
        finish(prof([(.424,.0440),(.440,.0440),(.437,.0640),(.428,.0640)], *sorted((s_*.0040, s_*.0062))), BLUED, .44, .6, .0004)
    finish(box((0,.0560,.432), (.0012,.0080,.0012)), BLACK, .5, 0, .0002)
    finish(lathe([(.414,.0120),(.446,.0120)], 0, .0200, 22), BLUED, .44, .6, .0006)
    finish(box((0,.0010,.4300), (.0042,.0060,.0110)), BLUED, .44, .6, .0004)
    mz = lathe([(.460,.0102),(.492,.0102),(.492,.0060)], 0, .0200, 22, 'comp')
    cut(mz, prof([(.476,.0300),(.500,.0300),(.500,.0150),(.490,.0140)], -.0300, .0300))
    finish(mz, BLUED, .42, .6, .0004)
    # --- grip, guard, stock, sight, magazine
    ar_grip(grip_col=AK_BAKELITE, guard_col=BLUED, grip_rough=.62, guard_rough=.45, guard_metal=.5)
    ak_wood_stock()
    kobra_sight()
    ak_magazine()

def ak_magazine():
    """THE AKM's 30-ROUND RIBBED STEEL MAGAZINE (group 1), scaled accurately to this
    rifle's 0.87x metric scale (protrudes ~150 mm below receiver rather than an
    oversized RPK-45 banana): true 7.62x39 arc, stamped recessed side panels, spine
    rib, locking lugs, and rolled steel floorplate."""
    MCOL = (.180,.182,.190); MDK = (.148,.150,.158)
    Z0, Y0, SW, L = .1185, -.0060, .46, .150
    R = L / SW
    def st(t, w):
        a = SW * t
        cz = Z0 + R * (1 - math.cos(a)); cy = Y0 - .014 - R * math.sin(a)
        nz, ny = math.cos(a), math.sin(a)
        return (cz + w * nz, cy + w * ny)
    n = 12
    hw = lambda t: .0236 + .0016 * t
    f = [st(i / (n - 1), hw(i / (n - 1))) for i in range(n)]
    b = [st(i / (n - 1), -hw(i / (n - 1)) * .96) for i in range(n)]
    throat = [(Z0 + .0236, Y0), (Z0 - .0226, Y0)]
    body = prof([throat[0]] + f + b[::-1] + [throat[1]], -.0122, .0122, 'mag')
    finish(body, MCOL, .50, .35, .0018, 2, group=1)
    pf = [st(.12 + .76 * i / (n - 1), hw(.12 + .76 * i / (n - 1)) * .68) for i in range(n)]
    pb = [st(.12 + .76 * i / (n - 1), -hw(.12 + .76 * i / (n - 1)) * .64) for i in range(n)]
    for s_ in (-1, 1):
        x0, x1 = sorted((s_ * .0116, s_ * .0130))
        finish(prof(pf + pb[::-1], x0, x1, 'mag_panel', r=0), MDK, .50, .35, .0004, 1, group=1)
    a = SW; cz, cy = st(1.0, 0.0)
    nz, ny = math.cos(a), math.sin(a); tz, ty = math.sin(a), -math.cos(a)
    w0 = hw(1.0) + .0014
    fp = [(cz + nz*w0, cy + ny*w0), (cz + nz*w0 + tz*.0056, cy + ny*w0 + ty*.0056),
          (cz - nz*w0*.96 + tz*.0056, cy - ny*w0*.96 + ty*.0056), (cz - nz*w0*.96, cy - ny*w0*.96)]
    finish(prof(fp, -.0136, .0136, 'floor'), MDK, .48, .40, .0010, 2, group=1)
    finish(box((0, Y0 - .0030, Z0 + .0255), (.0048, .0038, .0024)), MDK, .50, .35, .0004, group=1)
    finish(box((0, Y0 - .0120, Z0 - .0238), (.0066, .0032, .0020)), MDK, .50, .35, .0004, group=1)

def rifle_famas():
    """FAMAS F1 ("Le Clairon") after real-life reference photographs: authentic
    compact 760 mm bullpup length (butt -.310 to muzzle +.450), iconic full-length
    high-profile polymer carry handle running ALL THE WAY forward over the fore-end
    to the front handguard nose (z -.214 to +.264), T-shaped ambidextrous charging
    handle centered inside the carry-handle opening, iron sights inside the handle
    window (rear aperture z -.140 y .060, front post z +.220 y .060), integrated
    top-flank folding steel bipod legs tucked along the upper receiver grooves, ONE
    clean full-hand trigger guard (no nested double guard), slim exposed barrel with
    22 mm rifle-grenade rings and flash hider, stepped cheek rest, and the straight
    vertical 25-round steel FAMAS F1 magazine with vertical flutes."""
    clear()
    # --- compact main polymer body & integral fore-end (z -.200 to +.272)
    body = prof([(-.200,.038),(.266,.038),(.274,.030),(.274,.004),(.252,-.005),(.092,-.008),
                 (.086,-.018),(-.020,-.018),(-.060,-.020),(-.188,-.024),(-.200,-.020)], -.0218, .0218, 'body', r=.0052)
    cut(body, box((0,-.024,-.142), (.0114,.014,.0250)))            # rear bullpup magwell
    cut(body, box((.0222,.014,-.050), (.0040,.0080,.0380)))        # right ejection port
    for i in range(4):                                             # upper fore-end vent slots
        for s_ in (-1, 1):
            cut(body, box((s_*.0220,.026,.145+i*.028), (.0036,.0040,.0090)))
    finish(body, TAN, .68, 0, .0028, 3, flag=4)
    finish(box((.0198,.014,-.050), (.0020,.0070,.0360)), BLUED, .40, .6, .0003, 1)
    # Ribbed lower handguard grip panels along both flanks (support hand z .214)
    for s_ in (-1, 1):
        x0, x1 = sorted((s_*.0214, s_*.0228))
        finish(prof([(.118,-.004),(.250,-.004),(.254,.010),(.118,.010)], x0, x1, 'hg_panel', r=0), DK, .84, 0, .0006, 1, flag=4)
        for i in range(7):
            finish(box((s_*.0230,.003,.132+i*.018), (.0008,.0056,.0016)), BLACK, .84, 0, .0002)
        finish(box((s_*.0220,.0050,.010), (.0006,.0007,.1900)), DK, .7, 0, .0002, 1)
    # --- buttstock (group 2): stepped FAMAS cheek rest & angled rubber buttpad
    bt = prof([(-.200,.038),(-.200,-.024),(-.302,-.030),(-.312,-.024),(-.312,.038),(-.298,.044),(-.220,.044)], -.0212, .0212, 'butt', r=.0050)
    finish(bt, TAN, .68, 0, .0028, 3, group=2, flag=4)
    finish(prof([(-.310,.039),(-.320,.039),(-.320,-.026),(-.310,-.030)], -.0216, .0216, 'pad', r=.004), BLACK, .90, 0, .0016, 2, group=2)
    # Detachable cheekpiece cover plate (left side)
    finish(prof([(-.282,.010),(-.212,.010),(-.212,.032),(-.282,.032)], -.0224, -.0208, 'cheek', r=0), DK, .76, 0, .0005, 1, group=2, flag=4)
    # --- ICONIC FULL-LENGTH FAMAS F1 CARRY HANDLE (z -.214 all the way to +.266!)
    hd = prof([(-.214,.036),(-.200,.066),(-.184,.082),(-.168,.087),(.236,.087),(.254,.082),(.264,.066),(.268,.036)], -.0116, .0116, 'handle', r=.0035)
    cut(hd, prof([(-.164,.0470),(-.154,.0410),(.218,.0410),(.228,.0470),(.228,.0690),(.218,.0750),(-.154,.0750),(-.164,.0690)], -.024, .024))
    cut(hd, box((0,.0600,-.192), (.0078,.0082,.0420)))            # rear sight tunnel through rear pillar
    cut(hd, box((0,.0600,.246), (.0076,.0082,.0420)))             # front sight tunnel through front pillar
    finish(hd, TAN, .68, 0, .0024, 3, flag=4)
    # Classic FAMAS top carry-handle longitudinal rib & sight-channel grooves (no clunky AR rail)
    finish(box((0,.0878,.032), (.0086,.0014,.2020)), DK, .72, 0, .0005, 1)
    # --- Iron sights INSIDE the carry-handle window (VM_VAR sy .0600, sz -.1400)
    finish(prof([(-.152,.0400),(-.128,.0400),(-.133,.0540),(-.147,.0540)], -.0058, .0058, r=0), BLUED, .44, .5, .0005)
    ap = lathe([(-.1420,.0066),(-.1380,.0066)], 0, .0600, 24, 'aperture')
    cut(ap, lathe([(-.1500,.0026),(-.1300,.0026)], 0, .0600, 16))
    finish(ap, BLUED, .44, .5, .0004)
    for s_ in (-1,1):
        finish(box((s_*.0080,.0590,-.1400), (.0014,.0078,.0038)), BLUED, .44, .5, .0004)
    finish(box((0,.0450,.2160), (.0042,.0050,.0050)), BLUED, .44, .5, .0004)
    finish(box((0,.0547,.2160), (.0011,.0056,.0011)), BLACK, .50, 0, .0002)
    for s_ in (-1,1):
        finish(prof([(.208,.0410),(.224,.0410),(.222,.0660),(.212,.0640)], *sorted((s_*.0054, s_*.0086)), r=0), BLUED, .44, .5, .0004)
    # Ambidextrous T-shaped FAMAS charging handle riding inside the carry handle!
    finish(box((0,.0435,.040), (.0036,.0024,.1200)), BLUED, .42, .6, .0004)
    finish(prof([(.058,.0445),(.082,.0445),(.082,.0530),(.058,.0530)], -.0080, .0080, 'famas_ch', r=0), DK, .58, .1, .0006)
    # --- Authentic FAMAS top-flank integrated folding steel bipod legs!
    # Hinged high on the front flanks of the upper receiver (z .254, y .030) and folded BACK along the sides
    for s_ in (-1, 1):
        finish(pin(*sorted((s_*.0200, s_*.0265)), .0300, .254, .0042, 16), BLUED, .44, .5, .0004)
        finish(lathe([(-.010,.0030),(.254,.0030)], s_*.0248, .0300, 14), BLUED, .44, .5, .0003)
        finish(box((s_*.0255,.0300,-.016), (.0036,.0042,.0055)), BLACK, .6, .2, .0004)     # bipod foot
    # --- Slim exposed FAMAS barrel with rifle-grenade launcher collar & 22mm flash hider (muzzle .450)
    finish(lathe([(.270,.0086),(.414,.0080)], 0, .0210, 24), BLUED, .40, .6, .0003)
    for zr in (.350, .372, .392):                                  # iconic FAMAS rifle-grenade launch rings
        finish(lathe([(zr-.0025,.0108),(zr+.0025,.0108)], 0, .0210, 24), BLUED, .44, .6, .0003)
    mz = lathe([(.412,.0106),(.446,.0106),(.450,.0094),(.450,.0055)], 0, .0210, 24, 'mz')
    for i in range(4):
        c_ = box((0,.0210,.432), (.0020,.0140,.0090))
        c_.rotation_euler = (0,0,i*math.pi/4); bpy.context.view_layer.objects.active = c_; c_.select_set(True)
        bpy.ops.object.transform_apply(rotation=True); c_.select_set(False)
        cut(mz, c_)
    finish(mz, BLUED, .42, .6, .0004)
    # --- Pistol grip + ONE clean full-hand FAMAS trigger guard (NO nested double guard!)
    finish(prof([(.030,-.014),(-.008,-.013),(-.016,-.026),(-.036,-.100),(-.030,-.110),(.000,-.106),(.010,-.066),(.018,-.044)],
                -.0126, .0126, 'grip', r=.0052), DK, .80, 0, .0040, 4, flag=4)
    finish(prof([(.084,-.014),(.093,-.014),(.091,-.060),(-.006,-.114),(-.024,-.114),(-.024,-.104),(.079,-.053)],
                -.0060, .0060, 'famas_guard', r=0), TAN, .68, 0, .0016, 2, flag=4)
    finish(prof([(.044,-.016),(.050,-.016),(.044,-.042),(.039,-.042)], -.0030, .0030, 'trig', r=0), DARK, .45, .6, .0005)
    # --- Straight vertical 25-round steel FAMAS F1 magazine (group 1) with vertical flutes!
    mag = prof([(-.121,-.018),(-.163,-.018),(-.165,-.124),(-.123,-.124)], -.0104, .0104, 'mag')
    for s_ in (-1, 1):
        for zm in (-.132, -.152):
            cut(mag, box((s_*.0106, -.072, zm), (.0010, .042, .0024)))
    finish(mag, (.21,.21,.22), .52, .35, .0014, 2, group=1)
    finish(prof([(-.121,-.122),(-.167,-.122),(-.167,-.130),(-.121,-.130)], -.0114, .0114, 'magplate'), BLACK, .72, .2, .0010, 2, group=1)

def pistol_glock():
    """Glock 18C after real-life reference photographs: iconic boxy Austrian flat-sided
    slide with crisp small top bevels, vertical REAR serrations ONLY (no front serrations),
    dual top-front 18C compensator ports, prominent left-rear rotary fire-selector switch,
    flat hammerless rear slide cover plate, U-notch rear & dot front sights; Gen-3 matte
    black polymer frame with single accessory rail slot, squared concave-front serrated
    trigger guard, Safe-Action trigger with central safety tab, subtle Gen-3 finger grooves
    & rectangular stippled grip panels, and flush 20-round Glock magazine."""
    clear()
    # --- Gen-3 Glock polymer frame: slim flat dust cover with SINGLE Picatinny slot & short tang
    fr = prof([(-.046,-.008),(.142,-.008),(.142,-.021),(.135,-.024),(.070,-.024),
               (.058,-.017),(.010,-.017),(-.012,-.015),(-.044,-.018)], -.0126, .0126, 'frame', r=.0022)
    cut(fr, box((0,-.0238,.116), (.0140,.0018,.0026)))             # single Gen-3 accessory slot
    finish(fr, DK, .76, 0, .0010, 2, flag=4)
    finish(prof([(-.010,-.0105),(.012,-.0105),(.012,-.0135),(-.010,-.0135)], -.0136, -.0122, 'slstop', r=0), BLACK, .50, .4, .0003)
    finish(box((0,-.0130,.080), (.0134,.0024,.0030)), BLACK, .50, .4, .0003)                 # takedown slide lock
    # --- Glock grip with subtle Gen-3 frontstrap finger grooves & rectangular side texture
    gp = [(.028,-.014),(-.008,-.013),(-.016,-.026),(-.036,-.100),(-.030,-.110),(.000,-.106),
          (.005,-.090),(.002,-.082),(.010,-.070),(.007,-.062),(.016,-.048),(.018,-.044)]
    finish(prof(gp, -.0126, .0126, 'grip', r=.0040), DK, .82, 0, .0030, 3, flag=4)
    for s_ in (-1, 1):
        x0, x1 = sorted((s_*.0120, s_*.0132))
        finish(prof([(-.008,-.036),(-.024,-.038),(-.032,-.092),(-.008,-.092),(.002,-.064)], x0, x1, 'panel', r=0), BLACK, .94, 0, .0004, 1, flag=4)
    # --- Iconic squared concave-front Glock trigger guard & Safe-Action trigger
    finish(prof([(.014,-.016),(.019,-.016),(.021,-.0455),(.068,-.0455),(.072,-.042),(.069,-.030),(.074,-.016),
                 (.079,-.016),(.074,-.030),(.077,-.044),(.071,-.0495),(.014,-.0495)], -.0038, .0038, 'guard', r=0), DK, .76, 0, .0006, 2, flag=4)
    finish(prof([(.044,-.016),(.050,-.016),(.044,-.041),(.039,-.041)], -.0030, .0030, 'trig', r=0), BLACK, .58, 0, .0004)
    finish(prof([(.0455,-.020),(.0482,-.020),(.0455,-.036),(.0435,-.036)], -.0012, .0012, 'tsafe', r=0), DARK, .52, 0, .0002)
    # --- Iconic square boxy Austrian Glock-18C slide (group 3): rear serrations ONLY, 18C top ports, left selector
    sl = prof_front([(-.0120,-.0095),(.0120,-.0095),(.0120,.0222),(.0098,.0252),(-.0098,.0252),(-.0120,.0222)], -.052, .146, 'slide', r=.0016)
    cut(sl, box((.0126,.0135,.040), (.0062,.0070,.0220)))          # ejection port
    cut(sl, lathe([(.138,.0056),(.150,.0056)], 0, .0060, 20))      # bore
    for zc in (.106, .122):                                        # dual Glock 18C top compensator slots
        cut(sl, box((0,.0252,zc), (.0036,.0060,.0042)))
    for i in range(7):                                             # rear vertical serrations ONLY (authentic Glock!)
        for s_ in (-1, 1):
            cut(sl, box((s_*.0122,.0085,-.046+i*.0046), (.0012,.0110,.0012)))
    finish(sl, TAN, .52, .12, .0008, 2, group=3, flag=6)
    finish(box((.0094,.0135,.040), (.0030,.0062,.0200)), BLUED, .42, .6, .0003, 1, group=3)
    finish(lathe([(.134,.0060),(.1475,.0056)], 0, .0060, 20), BLUED, .40, .6, .0002, group=3)
    # Prominent Glock-18 left-rear rotary fire-selector switch (signature visual tell!)
    finish(pin(-.0144,-.0118,.0105,-.036,.0044,18), BLUED, .42, .5, .0004, group=3)
    finish(prof([(-.040,.0085),(-.030,.0085),(-.028,.0135),(-.038,.0135)], -.0156, -.0136, 'g18sel', r=0), BLACK, .50, .3, .0004, group=3)
    # Flat rear slide backplate (striker-fired, NO hammer!)
    finish(box((0,.0065,-.0524), (.0088,.0090,.0006)), BLACK, .60, 0, .0002, group=3)
    # Sights: rear at z -.0405 top .0342, front dot
    finish(box((0,.0290,.1340), (.0018,.0052,.0044)), BLACK, .55, 0, .0004, group=3)
    finish(lathe([(.1288,.0017),(.1300,.0017)], 0, .0320, 14), AMBER, .40, 0, 0, group=3, flag=3)
    rear = box((0,.0290,-.0405), (.0100,.0052,.0040), 'rear')
    cut(rear, box((0,.0340,-.0405), (.0026,.0048,.0100)))
    finish(rear, BLACK, .55, 0, .0004, group=3)
    for s_ in (-1, 1):
        finish(box((s_*.0056,.0318,-.0446), (.0015,.0015,.0004)), AMBER, .40, 0, 0, group=3, flag=3)
    # --- Flush 20-round Glock magazine (group 1)
    finish(prof([(.012,-.024),(-.012,-.024),(-.032,-.104),(-.008,-.104)], -.0096, .0096, 'mag'), DK, .72, 0, .0012, 2, group=1)
    finish(prof([(-.036,-.1040),(-.002,-.1040),(.001,-.1135),(-.034,-.1155)], -.0124, .0124, 'magplate'), BLACK, .82, 0, .0012, 2, group=1)

def pistol_deagle():
    """Desert Eagle Mark XIX (.50 AE) after real-life reference photographs: all-brushed
    stainless steel construction (full-length dust cover sweeping cleanly up at 45 deg
    directly under the muzzle, polygonal fixed barrel with deep trapezoidal side flutes,
    smooth top rib with 2 authentic Mark XIX Weaver slots, huge .50-cal bore, rear-half
    reciprocating slide with angled serrations and ambidextrous butterfly safety wings,
    frame-mounted spur hammer [group 0, does not ride the slide!], compact squared
    Deagle trigger guard, and matte black wrap-around Hogue rubber grip."""
    clear()
    # --- Full-length stainless steel frame: dust cover runs all the way to the muzzle with iconic 45-deg front chamfer!
    fr = prof([(-.054,-.008),(.192,-.008),(.192,-.014),(.178,-.027),(.074,-.027),(.060,-.017),
               (.010,-.017),(-.012,-.016),(-.052,-.022)], -.0136, .0136, 'frame', r=.0022)
    finish(fr, SAND, .40, 0, .0010, 2, flag=5)
    finish(pin(-.0148,-.0126,-.0130,.028,.0038,14), BLUED, .40, .6, .0003, 1)
    finish(prof([(.050,-.0096),(.082,-.0096),(.082,-.0146),(.050,-.0146)], -.0152, -.0130, 'takedown', r=0), BLUED, .40, .6, .0004)
    # Prominent curved stainless beavertail
    finish(prof([(-.058,-.0105),(-.012,-.0105),(-.012,-.0200),(-.030,-.0245),(-.056,-.0195)], -.0120, .0120, 'tail', r=.0026), SAND, .40, 0, .0014, 2, flag=5)
    # Matte-black wrap-around rubber grip with screws
    finish(prof([(.030,-.014),(-.008,-.013),(-.016,-.026),(-.036,-.100),(-.030,-.110),(.000,-.106),(.010,-.066),(.018,-.044)],
                -.0130, .0130, 'grip', r=.0050), (.09,.09,.095), .92, 0, .0045, 4, flag=4)
    for s_ in (-1, 1):
        finish(pin(*sorted((s_*.0126, s_*.0138)), -.050, -.006, .0024, 12), BLUED, .40, .6, .0002)
        finish(pin(*sorted((s_*.0126, s_*.0138)), -.088, -.024, .0024, 12), BLUED, .40, .6, .0002)
    # Compact squared-front Desert Eagle stainless trigger guard & curved trigger
    finish(prof([(.014,-.017),(.020,-.017),(.022,-.0465),(.068,-.0465),(.073,-.017),(.079,-.017),
                 (.073,-.0505),(.014,-.0505)], -.0042, .0042, 'guard', r=0), SAND, .40, 0, .0007, 2, flag=5)
    finish(prof([(.044,-.016),(.050,-.016),(.044,-.041),(.039,-.041)], -.0030, .0030, 'trig', r=0), BLUED, .40, .6, .0005)
    # --- FIXED MARK XIX POLYGONAL BARREL: flat sides, 45-deg shoulders, deep side flutes, 2-slot Weaver top rib, huge .50 bore
    bar = prof_front([(-.0136,-.0080),(.0136,-.0080),(.0136,.0165),(.0076,.0245),(-.0076,.0245),(-.0136,.0165)], .052, .196, 'barrel', r=.0014)
    cut(bar, lathe([(.186,.0065),(.202,.0065)], 0, .0060, 22))                                # menacing .50 AE bore!
    for s_ in (-1, 1):
        cut(bar, prof([(.074,.0005),(.178,.0005),(.184,.0070),(.178,.0135),(.074,.0135)], *sorted((s_*.0120, s_*.0160))))
    finish(bar, TAN, .36, 0, .0010, 2, flag=5)
    # Authentic Mark XIX integral Weaver top rib with 2 wide transverse mounting slots (NOT 16 tiny teeth!)
    rib = box((0,.0264,.124), (.0074,.0022,.0710), 'rib')
    for zs in (.090, .148):
        cut(rib, box((0,.0280,zs), (.0100,.0018,.0040)))
    finish(rib, TAN, .38, 0, .0004, 1, flag=5)
    finish(box((0,.0308,.1860), (.0020,.0034,.0056)), BLACK, .55, 0, .0004)                   # front ramp sight
    finish(box((0,.0334,.1830), (.0009,.0008,.0006)), AMBER, .40, 0, 0, flag=3)
    # --- REAR-HALF STAINLESS SLIDE (group 3): sloped front wrap, angled rear serrations, ambi butterfly safeties
    sl = prof_front([(-.0142,-.0095),(.0142,-.0095),(.0146,.0040),(.0142,.0215),(.0088,.0275),
                     (-.0088,.0275),(-.0142,.0215),(-.0146,.0040)], -.058, .056, 'slide', r=.0016)
    cut(sl, box((.0148,.0140,.026), (.0060,.0072,.0220)))
    for i in range(8):
        for s_ in (-1, 1):
            c_ = box((0,0,0), (.0014,.0092,.0013))
            c_.rotation_euler = (radians(-12), 0, 0); c_.location = (s_*.0148, .0080, -.051 + i*.0050)
            bpy.context.view_layer.objects.active = c_; c_.select_set(True)
            bpy.ops.object.transform_apply(location=True, rotation=True); c_.select_set(False)
            cut(sl, c_)
    finish(sl, TAN, .38, 0, .0010, 2, group=3, flag=5)
    for s_ in (-1, 1):                                         # ambi butterfly thumb safeties on rear slide
        finish(pin(*sorted((s_*.0140, s_*.0164)), .0160, -.045, .0042, 14), DK, .46, .35, .0003, group=3)
        finish(box((s_*.0166,.0205,-.044), (.0016,.0036,.0065)), DK, .46, .35, .0003, group=3)
    rear = box((0,.0290,-.0405), (.0108,.0052,.0042), 'rear')
    cut(rear, box((0,.0340,-.0405), (.0026,.0048,.0100)))
    finish(rear, BLACK, .55, 0, .0005, group=3)
    for s_ in (-1, 1):
        finish(box((s_*.0058,.0318,-.0449), (.0015,.0015,.0004)), AMBER, .40, 0, 0, group=3, flag=3)
    # --- Frame-mounted big spur hammer (group 0 — does NOT fly back with slide!)
    hm = prof([(-.055,.0060),(-.063,.0060),(-.071,.0200),(-.069,.0260),(-.061,.0250),(-.057,.0160)], -.0040, .0040, 'hammer', r=0)
    finish(hm, DK, .44, .5, .0006, group=0)
    finish(pin(-.0044,.0044,.0120,-.059,.0040,14), DK, .44, .5, .0004, group=0)
    # --- Single-stack .50 AE magazine (group 1)
    finish(prof([(.010,-.024),(-.012,-.024),(-.032,-.104),(-.010,-.104)], -.0090, .0090, 'mag'), DK, .52, .3, .0010, 2, group=1)
    finish(prof([(-.036,-.1040),(-.002,-.1040),(.002,-.1135),(-.034,-.1160)], -.0124, .0124, 'magplate'), BLACK, .80, 0, .0012, 2, group=1)


def mirror_x():
    '''Left-hand copy of what is in the scene. Winding is flipped so the mesh stays outward.'''
    for o in bpy.context.collection.objects:
        bm = bmesh.new(); bm.from_mesh(o.data)
        for v in bm.verts: v.co.x = -v.co.x
        bmesh.ops.reverse_faces(bm, faces=bm.faces)
        bm.to_mesh(o.data); bm.free()

store = {}
def assign_finishes():
    # 4 moulded polymer, 5 anodised alloy, 6 painted alloy
    for o in bpy.context.collection.objects:
        if 'col' not in o or o['flag'] != 0 or o['metal'] >= .8: continue
        tan = o['col'][0] > o['col'][2] * 1.3 or o.get('light', False)
        if o['rough'] >= .62 and not (tan and o['group'] == 0): o['flag'] = 4
        elif tan: o['flag'] = 6
        else: o['flag'] = 5

# ------------------------------------------------------------------ factory finishes
# EVERY GUN IN ITS REAL COLOURS. The arsenal was authored in one house palette — a
# desert tan receiver and furniture on a warm gunmetal — and the loadout guns then
# shipped under a desert-digital camo on top of it, so an M4, a Glock and a P90 all
# came out the same sand colour. These are the guns' own factory finishes, from
# reference photographs: hard-anodised black aluminium, black polymer, Parkerised
# grey steel, the AUG's olive drab, the Five-seveN's and SCAR's flat dark earth,
# stainless steel. Each swaps the palette constants the gun is built from for the
# length of its build (the AWP did this first, for its green), and the parts drawn
# in the swapped TAN/SAND stay marked as the light family the Armory paints.
#             TAN (receiver / slide / furniture)  SAND (secondary light)   DK (dark family)
_AL   = {'TAN': (.245,.250,.262), 'SAND': (.215,.220,.232), 'DK': (.165,.168,.176), 'MAGC': (.165,.168,.176)}   # black anodised
_POLY = {'TAN': (.195,.198,.205), 'SAND': (.175,.178,.186), 'DK': (.150,.152,.158), 'MAGC': (.150,.152,.158)}   # black polymer
_PARK = {'TAN': (.285,.292,.300), 'SAND': (.250,.256,.264), 'DK': (.175,.178,.184), 'MAGC': (.170,.172,.178)}   # Parkerised steel
_ODG  = {'TAN': (.330,.355,.270), 'SAND': (.285,.305,.235), 'DK': (.165,.168,.172), 'MAGC': (.165,.168,.172)}   # olive drab
_FDE  = {'TAN': (.560,.490,.380), 'SAND': (.500,.440,.340), 'DK': (.170,.170,.175), 'MAGC': (.170,.170,.175)}   # flat dark earth
_SS   = {'TAN': (.600,.605,.620), 'SAND': (.520,.525,.540), 'DK': (.180,.182,.190), 'MAGC': (.180,.182,.190)}   # stainless
_AWG  = {'TAN': (.310,.370,.250), 'SAND': (.240,.290,.200)}                          # AW green
PALETTES = {
    'RIFLE': _AL, 'RIFLE_AK': {'TAN': (.215,.218,.226), 'SAND': AK_WOOD, 'DK': (.165,.168,.176), 'MAGC': (.165,.168,.176)}, 'RIFLE_FAMAS': _POLY, 'RIFLE_M4A1S': _AL, 'RIFLE_GALIL': _PARK,
    'RIFLE_SG553': _PARK, 'RIFLE_AUG': _ODG,
    'SMG_MAC10': _PARK, 'SMG_MP9': _POLY, 'SMG_MP7': _POLY, 'SMG_MP5SD': _AL, 'SMG_UMP45': _POLY,
    'SMG_P90': _POLY, 'SMG_BIZON': _AL,
    'SG_NOVA': _POLY, 'SG_XM1014': _AL, 'SG_SAWEDOFF': _AL, 'SG_MAG7': _PARK,
    'MG_M249': _AL, 'MG_NEGEV': _AL,
    'PISTOL': _POLY, 'PISTOL_GLOCK': _POLY, 'PISTOL_DEAGLE': _SS, 'PISTOL_USPS': _POLY,
    'PISTOL_P250': _POLY, 'PISTOL_FIVESEVEN': _FDE, 'PISTOL_TEC9': _PARK, 'PISTOL_CZ75': _AL,
    'PISTOL_R8': _AL, 'PISTOL_ELITES': _SS, 'PISTOL_ELITES_L': _SS,
    'SNIPER': _AWG, 'SNIPER_SCAR': _FDE, 'SNIPER_SSG08': _AL, 'SNIPER_G3SG1': _AL,
}
def paint_build(tag, fn):
    """Build `fn` under the gun's factory palette (PALETTES[tag]); None keeps the
    authored colours. The light family is TAN and SAND as swapped."""
    global _LIGHT, _SCULPT
    g = globals(); pal = PALETTES.get(tag)
    _SCULPT = True
    if not pal:
        _LIGHT = None
        try: fn()
        finally: _SCULPT = False
        return
    saved = {k: g.get(k) for k in pal}
    g.update(pal)
    _LIGHT = {tuple(round(x, 4) for x in g[k]) for k in ('TAN', 'SAND')}
    try:
        fn()
    finally:
        g.update(saved); _LIGHT = None; _SCULPT = False

# World props (vehicles, street furniture): tools/props_models.py, same helpers.
exec(compile(open(os.path.join(os.path.dirname(OUT), '..', 'tools', 'props_models.py')).read(),
             'props_models.py', 'exec'))

paint_build('RIFLE', rifle); assign_finishes()
emit(store, 'RIFLE')
hand_r(); emit(store, 'HAND_R')
hand_l(); emit(store, 'HAND_L')
bracer(); emit(store, 'BRACER')
# THE AWP: the bolt gun's chassis in Arctic Warfare green (TAN and SAND repainted
# for this one build; every other model keeps the desert palette).
paint_build('SNIPER', sniper); assign_finishes()
emit(store, 'SNIPER')
paint_build('PISTOL', pistol); assign_finishes()
emit(store, 'PISTOL')
hand_p(); emit(store, 'HAND_P')
knife(); assign_finishes()
emit(store, 'KNIFE')
hand_r(fist=True); emit(store, 'HAND_K')
hand_r(fist=True); mirror_x(); emit(store, 'HAND_KL')
hand_r(fist=True, open_k=.5); emit(store, 'HAND_KH')    # the knife toss: letting go...
hand_r(fist=True, open_k=1.0); emit(store, 'HAND_KX')   # ...and open for the catch
hand_g(); occ_hand = snapshot()
grenade(); assign_finishes(); occ_nade = snapshot()
emit(store, 'GRENADE', extra=occ_hand)
hand_g(); emit(store, 'HAND_G', extra=occ_nade)
grenade('smoke'); assign_finishes(); emit(store, 'GRENADE_SMK', extra=occ_hand)
grenade('inc'); assign_finishes(); emit(store, 'GRENADE_INC', extra=occ_hand)
for kfn, ktag in ((knife_tanto, 'KNIFE_TANTO'), (knife_dagger, 'KNIFE_DAGGER'),
                  (knife_kukri, 'KNIFE_KUKRI'), (knife_talon, 'KNIFE_TALON')):
    kfn(); assign_finishes(); emit(store, ktag)
hand_open(); emit(store, 'HAND_KO')
hand_pin(); emit(store, 'HAND_GP')
for gfn, gtag in ((rifle_ak, 'RIFLE_AK'), (rifle_famas, 'RIFLE_FAMAS'),
                  (pistol_glock, 'PISTOL_GLOCK'), (pistol_deagle, 'PISTOL_DEAGLE'),
                  (scar20, 'SNIPER_SCAR')):
    paint_build(gtag, gfn); assign_finishes(); emit(store, gtag); emit_lod(store, 'TP_' + gtag)
# The rest of the CS2 arsenal (tools/guns_cs2.py): same helpers, same stations.
exec(compile(open(os.path.join(os.path.dirname(OUT), '..', 'tools', 'guns_cs2.py')).read(),
             'guns_cs2.py', 'exec'))
exec(compile(open(os.path.join(os.path.dirname(OUT), '..', 'tools', 'guns_cs2_heavy.py')).read(),
             'guns_cs2_heavy.py', 'exec'))
# The arsenal rebuilt after reference photographs (0.8.3): replaces functions above.
exec(compile(open(os.path.join(os.path.dirname(OUT), '..', 'tools', 'guns_rebuild.py')).read(),
             'guns_rebuild.py', 'exec'))
for gfn, gtag in CS2_GUNS:
    paint_build(gtag, gfn); assign_finishes(); emit(store, gtag); emit_lod(store, 'TP_' + gtag)
# THE OPERATOR'S HERO HEAD (tools/hero_models.py): helmet, visor, skull mask.
exec(compile(open(os.path.join(os.path.dirname(OUT), '..', 'tools', 'hero_models.py')).read(),
             'hero_models.py', 'exec'))
for hfn, htag in HERO_PARTS:
    hfn(); assign_finishes(); emit(store, htag)
# THE LEFT BERETTA AND THE LEFT HAND THAT HOLDS IT: mirror images of the right.
paint_build('PISTOL_ELITES_L', pistol_elites); assign_finishes(); mirror_x(); emit(store, 'PISTOL_ELITES_L')
hand_r(); mirror_x(); emit(store, 'HAND_RL')
for pfn, ptag, pk, pao in PROP_MODELS:     # world props: flags are authored, not assigned
    pfn(); emit(store, ptag, k=pk, aolen=pao)

with open(OUT, 'w') as f:
    f.write('// Generated by tools/build_models.py. Do not edit by hand.\n')
    f.write('typedef struct { int start, count; float r, g, b, rough, metal; int group, flag; } model_part_t;\n')
    f.write('#define MODEL_SCALE (1.0f / 32000.0f)\n#define MODEL_NSCALE (1.0f / 32767.0f)\n')
    for tag, tris in store.items():
        rows = []; parts = []
        for (col, rough, metal, group, flag), vs in tris.items():
            parts.append((len(rows), len(vs), *col, rough, metal, group, flag))
            rows.extend(vs)
        f.write('static const int16_t %s_V[][7] = {\n' % tag)
        line = []
        ks = KSCALE.get(tag, 1.0)
        for x, y, z, a, b, c, ao in rows:
            q = lambda v: max(-32767, min(32767, int(round(v*32000/ks))))
            n = lambda v: max(-32767, min(32767, int(round(v*32767))))
            line.append('{%d,%d,%d,%d,%d,%d,%d}' % (q(x), q(y), q(z), n(a), n(b), n(c), ao))
            if len(line) == 8:
                f.write(','.join(line) + ',\n'); line = []
        if line: f.write(','.join(line) + ',\n')
        f.write('};\nstatic const model_part_t %s_P[] = {\n' % tag)
        for p in parts:
            f.write('  {%d,%d,%.3ff,%.3ff,%.3ff,%.3ff,%.3ff,%d,%d},\n' % p)
        f.write('};\n#define %s_NP %d\n' % (tag, len(parts)))
        if tag in KSCALE: f.write('#define %s_K %.1ff\n' % (tag, KSCALE[tag]))
    for d in EXTRA_DEFINES: f.write(d + '\n')
print('WROTE', OUT)

# GitHub refuses any file over 100 MB, and models.h is past that: split it at a
# model boundary into models.h + models_2.h (the first includes the second, so
# nothing else changes). Lossless; rerunnable on an already split pair.
def split_models(path, limit=60_000_000):
    p2 = os.path.join(os.path.dirname(path), 'models_2.h')
    inc = '#include "models_2.h"\n'
    s = open(path).read()
    if s.endswith(inc) and os.path.exists(p2):
        s = s[:-len(inc)] + open(p2).read()
    if len(s) <= limit:
        open(path, 'w').write(s)
        if os.path.exists(p2): os.remove(p2)
        return
    cut = s.index('\nstatic const int16_t ', len(s) // 2) + 1
    open(path, 'w').write(s[:cut] + inc)
    open(p2, 'w').write('// Generated by tools/build_models.py (second half of models.h). Do not edit by hand.\n' + s[cut:])
split_models(OUT)

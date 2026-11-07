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

def prof(pts, x0, x1, name='prof'):
    """Extrude a (z, y) side profile across x from x0 to x1."""
    bm = bmesh.new()
    a = [bm.verts.new((x0, y, z)) for z, y in pts]; b = [bm.verts.new((x1, y, z)) for z, y in pts]
    bm.faces.new(a); bm.faces.new(b[::-1])
    n = len(pts)
    for i in range(n):
        j = (i+1) % n
        bm.faces.new((a[j], a[i], b[i], b[j]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return from_bm(bm, name)

def prof_front(pts, z0, z1, name='front'):
    """Extrude an (x, y) front profile along z."""
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
    m = o.modifiers.new('cut', 'BOOLEAN'); m.operation = 'DIFFERENCE'; m.object = cutter; m.solver = 'EXACT'
    bpy.context.view_layer.objects.active = o; o.select_set(True)
    bpy.ops.object.modifier_apply(modifier=m.name); o.select_set(False)
    bpy.data.objects.remove(cutter)

def finish(o, col, rough=.55, metal=0.0, bevel=.0009, seg=2, group=0, flag=0, harden=False):
    bpy.context.view_layer.objects.active = o
    if bevel > 0:
        m = o.modifiers.new('b', 'BEVEL'); m.width = bevel; m.segments = seg
        m.limit_method = 'ANGLE'; m.angle_limit = radians(28 if harden else 32); m.harden_normals = harden
    o.data.polygons.foreach_set('use_smooth', [True]*len(o.data.polygons))
    if harden: o['harden'] = True
    o['col'] = list(col); o['rough'] = rough; o['metal'] = metal; o['group'] = group; o['flag'] = flag
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

def emit(store, tag, extra=None):
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
        org = p + n * .00035; hit = 0
        for dx, dy, dz in dirs:
            d = tx*dx + ty*dy + n*dz
            if bvh.ray_cast(org, d, .045)[0] is not None: hit += 1
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
    print('MODEL', tag, sum(len(v) for v in tris.values())//3, 'tris')

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
    finish(pin(-.0170,-.0100,.0225,.022,.0072), DARK, .5, bevel=.0005)                  # forward assist boss
    finish(pin(-.0186,-.0150,.0225,.022,.0046), BLACK, .5, bevel=.0004)
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
    # the rail and its own centre. code/game.c's VM_HOLO_* carry the same SIGHT_K.
    SIGHT_K = .72
    for o in set(bpy.context.collection.objects) - before_sight:
        for v in o.data.vertices:
            v.co = Vector((v.co.x*SIGHT_K, .046 + (v.co.y-.046)*SIGHT_K, hz + (v.co.z-hz)*SIGHT_K))
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

def hand_r(fist=False):
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
        return prof([(z + r*cos(2*math.pi*i/seg), y + r*sin(2*math.pi*i/seg)) for i in range(seg)], x0, x1, name)

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

    # --- 1. CNC milled FDE precision chassis (group 0): weight-reduction pockets, barricade stop & controls
    ch = prof([
        (-.078, .0265), (.106, .0265), (.112, .0285), (.146, .0285),
        (.146, -.0055), (.106, -.0120), (.086, -.0245), (.022, -.0245),
        (.018, -.0135), (-.046, -.0135), (-.086, -.0125), (-.086, .0205)
    ], -.0162, .0162, 'chassis')
    cut(ch, box((0, -.018, .052), (.0130, .030, .0280)))
    # Milled CNC side pockets on both flanks leaving a diagonal stiffening web
    for x0, x1 in ((-.0200, -.0134), (.0134, .0200)):
        cut(ch, prof([(-.073, .0195), (-.014, .0195), (-.022, .0005), (-.073, -.0025)], x0, x1))
        cut(ch, prof([(.004, .0195), (.092, .0195), (.080, -.0165), (.028, -.0165), (.016, -.0020)], x0, x1))
    finish(ch, TAN, .60, 0, .0013, 3)
    # Two-tone SAND upper bevel accent rails along both sides of the chassis
    for x0, x1 in ((-.0168, -.0148), (.0148, .0168)):
        finish(prof([(-.075, .0258), (.102, .0258), (.102, .0218), (-.075, .0218)], x0, x1), SAND, .60, 0, .0004, 1)
    # Flared competition magwell skirt + front barricade-stop serrations
    mw = box((0, -.0252, .0535), (.0176, .0032, .0345))
    cut(mw, box((0, -.0252, .0520), (.0132, .0080, .0282)))
    finish(mw, DARK, .50, 0, .0008, 2)
    for i in range(4):
        yc = -.0225 + i * .0028
        zc = .0885 + i * .0038
        finish(box((0, yc, zc), (.0150, .0009, .0016)), BLACK, .52, 0, .0003, 1)
    # Counterbored action-bedding Torx bolts on both flanks + ballistic data decal in left pocket
    for z, y in ((-.064, .0085), (-.005, .0095), (.098, .0045)):
        finish(rod_x(-.0168, .0168, y, z, .0038, 16), DARK, .48, 0, .0003, 1)
        finish(rod_x(-.0174, .0174, y, z, .0021, 14), STEEL, .34, .90, .0002, 1)
    finish(box((-.0136, .0065, .058), (.0008, .0048, .0135)), BLACK, .55, 0, .0003, 1)
    finish(box((-.0141, .0065, .055), (.0005, .0032, .0058)), (.58, .48, .15), .55, 0, .0002, 1, flag=4)
    finish(box((-.0141, .0065, .064), (.0005, .0032, .0032)), (.58, .48, .15), .55, 0, .0002, 1, flag=4)
    # Left-side tactical controls: thumb safety selector + fire dot, bolt-stop latch, ambi mag catch
    finish(rod_x(-.0182, -.0140, .0115, -.056, .0054, 18), DARK, .48, 0, .0005, 1)
    finish(prof([(-.058, .0135), (-.040, .0065), (-.042, .0025), (-.060, .0095)], -.0196, -.0168), BLACK, .46, 0, .0006, 2)
    finish(rod_x(-.0200, -.0160, .0115, -.056, .0020, 12), STEEL, .34, .90, .0002, 1)
    finish(rod_x(-.0167, -.0158, .0168, -.048, .0011, 10), ORANGE, .30, 0, 0, 1, flag=3)
    finish(prof([(-.026, .0295), (-.008, .0295), (-.010, .0220), (-.026, .0235)], -.0184, -.0140), DARK, .48, 0, .0006, 2)
    finish(rod_x(-.0188, -.0140, .0260, -.017, .0016, 12), STEEL, .35, .90, .0002, 1)
    finish(prof([(.034, -.0025), (.054, -.0025), (.054, -.0095), (.034, -.0095)], -.0174, -.0130), DARK, .50, 0, .0006, 2)
    finish(rod_x(-.0178, -.0130, -.0060, .044, .0020, 12), STEEL, .35, .90, .0002, 1)
    # CNC folding-stock hinge knuckle at z = -0.086 (prominent in 1P lower-right foreground)
    finish(box((-.0168, .0145, -.0855), (.0036, .0062, .0054)), GRAPH, .48, 0, .0008, 2)
    finish(box((-.0168, -.0035, -.0855), (.0036, .0062, .0054)), GRAPH, .48, 0, .0008, 2)
    finish(box((-.0168, .0055, -.0855), (.0034, .0038, .0050)), DARK, .46, 0, .0007, 2)
    finish(rod_y(-.0115, .0225, -.0172, -.0855, .0024, 16), STEEL, .32, .92, .0003, 1)
    finish(rod_x(-.0208, -.0165, .0055, -.0795, .0030, 14), BLACK, .50, 0, .0004, 1)

    # --- 2. Faceted precision action, barrel tenon nut & monolithic 20-MOA Picatinny rail
    act = prof_front([
        (-.0146, .0255), (.0146, .0255), (.0146, .0365), (.0102, .0438),
        (-.0102, .0438), (-.0146, .0365)
    ], -.078, .104, 'action')
    cut(act, box((.0125, .0355, .029), (.0065, .0058, .0310)))     # right-side ejection port
    cut(act, box((-.0152, .0348, .034), (.0026, .0032, .0400)))    # left-side machined relief flute
    finish(act, GRAPH, .48, 0, .0012, 3)
    finish(rod_x(-.0150, -.0125, .0345, .086, .0022, 12), BLACK, .50, 0, 0)   # left gas-vent port
    # Notched barrel nut / recoil-lug collar bridging action into fore-end
    bnut = rod_z([(.098, .0195), (.103, .0212), (.114, .0212), (.117, .0192)], 0, .0200, 28, 'bnut')
    for i in range(6):
        cut(bnut, obox((0, .0200, .1085), (.0024, .0235, .0038), rz=i * math.pi / 3))
    finish(bnut, DARK, .46, 0, .0005, 1)
    # Continuous 20-MOA action + monolithic NV fore-end Picatinny rail & recoil teeth
    finish(box((0, .0448, .125), (.0094, .0024, .1950)), BLACK, .52, 0, .0005, 1)
    finish(prof([(.104, .0365), (.320, .0365), (.316, .0432), (.104, .0432)], -.0082, .0082), TAN, .60, 0, .0008, 2)
    for i in range(38):
        finish(box((0, .0484, -.062 + .00995 * i), (.0102, .0016, .0028)), BLACK, .48, 0, .0004, 1)

    # --- 3. Live-cycling fluted bolt, rear shroud, cocking piece & swept tactical knob (group 3)
    by_ = .0340
    bolt = rod_z([(-.076, .0088), (.068, .0088)], 0, by_, 24, 'bolt_body')
    for i in range(6):
        a_ = i * math.pi / 3 + .25
        fx = .0088 * cos(a_); fy = by_ + .0088 * sin(a_)
        finish(rod_z([(-.064, .0016), (.058, .0016)], fx * .86, by_ + (fy - by_) * .86, 10), GRAPH, .45, 0, 0, group=3)
    finish(bolt, STEEL, .30, .92, .0004, 1, group=3)
    # Sculpted rear bolt shroud + cocking indicator protruding behind the receiver
    finish(rod_z([(-.090, .0082), (-.086, .0106), (-.075, .0106), (-.073, .0090)], 0, by_, 22), DARK, .46, 0, .0006, 2, group=3)
    finish(box((0, by_ - .0055, -.082), (.0056, .0052, .0065)), BLACK, .50, 0, .0007, 2, group=3)
    finish(rod_z([(-.096, .0030), (-.089, .0034)], 0, by_, 14), STEEL, .32, .92, .0003, 1, group=3)
    finish(rod_z([(-.0935, .0036), (-.0915, .0036)], 0, by_, 14), ORANGE, .30, 0, 0, group=3, flag=3)
    # Swept bolt handle stem + oversized grooved tactical knob at z = -0.037 (matches BOLT_KNOB_Z)
    finish(obox((.0180, .0290, -.0368), (.0108, .0034, .0036), rz=-.42), STEEL, .32, .92, .0006, 2, group=3)
    kc_ = (.0328, .0222, -.0370)
    finish(place(rod_x(-.0084, .0084, 0, 0, .0090, 22, 'bolt_knob'), kc_, rz=-.34), DARK, .44, 0, .0014, 2, group=3)
    for lx in (-.0044, .0000, .0044):
        finish(place(rod_x(lx - .0008, lx + .0008, 0, 0, .0095, 20), kc_, rz=-.34), BLACK, .52, 0, .0003, 1, group=3)
    finish(place(rod_x(.0076, .0096, 0, 0, .0042, 14), kc_, rz=-.34), STEEL, .32, .92, .0003, 1, group=3)

    # --- 4. Extended octagonal M-LOK fore-end, front rails, spigot bipod mount, fluted barrel & 3-baffle brake
    fe = prof_front([(.0235*cos(math.pi/8 + i*math.pi/4), .0140 + .0235*sin(math.pi/8 + i*math.pi/4)) for i in range(8)], .104, .324, 'fore')
    # Aggressive slanted muzzle-end bevel on the fore-end nose
    cut(fe, obox((0, .0140, .329), (.030, .032, .008), rx=-.38))
    for i in range(5):
        zs = .126 + i * .042
        cut(fe, box((-.0245, .0140, zs), (.0070, .0035, .0148)))
        cut(fe, box(( .0245, .0140, zs), (.0070, .0035, .0148)))
        if i < 4:
            cut(fe, box((0, -.0085, zs + .010), (.0042, .0070, .0145)))
    for i in range(4):
        for s_ in (-1, 1):
            cut(fe, obox((s_ * .0172, .0140 + .0172, .138 + i * .043), (.0062, .0028, .0138), rz=-s_ * math.pi / 4))
    finish(fe, TAN, .60, 0, .0011, 2)
    # Rear QD sling cup + forward M-LOK thumb-index panel & front side Picatinny accessory rails (all clear of HAND_L!)
    finish(rod_x(-.0232, -.0205, .0140, .114, .0054, 18), DARK, .46, 0, .0005, 1)
    finish(rod_x(-.0236, -.0216, .0140, .114, .0030, 14), BLACK, .55, 0, 0)
    finish(box((-.0228, .0140, .270), (.0022, .0056, .0135)), SAND, .60, 0, .0008, 2)
    for zz in (.263, .277):
        finish(rod_x(-.0252, -.0220, .0140, zz, .0016, 12), STEEL, .35, .90, .0002, 1)
    for s_ in (-1, 1):
        finish(box((s_ * .0236, .0140, .302), (.0022, .0056, .0155)), BLACK, .50, 0, .0005, 1)
        for i in range(4):
            finish(box((s_ * .0258, .0140, .290 + i * .0080), (.0016, .0062, .0024)), BLACK, .48, 0, .0003, 1)
    # Front chassis spigot + heavy CNC underslung bipod mount block & QD throw lever
    finish(box((0, .0025, .326), (.0088, .0062, .0135)), DARK, .48, 0, .0009, 2)
    finish(box((0, -.0115, .296), (.0128, .0052, .0210)), DARK, .48, 0, .0010, 2)
    finish(box((0, -.0182, .298), (.0092, .0034, .0155)), BLACK, .50, 0, .0008, 2)
    finish(rod_z([(.282, .0048), (.316, .0048)], 0, -.0218, 16), GRAPH, .46, 0, .0005, 1)
    finish(rod_x(-.0162, .0150, -.0115, .296, .0024, 14), STEEL, .34, .90, .0002, 1)
    finish(prof([(.286, -.0085), (.308, -.0085), (.306, -.0145), (.288, -.0145)], -.0182, -.0148), SAND, .58, 0, .0006, 2)
    # Heavy free-float fluted match barrel (visible through M-LOK vents + 6 exposed flutes ahead of fore-end)
    brl = lathe([(.116, .0122), (.320, .0110), (.444, .0096), (.451, .0096)], 0, .0160, 36, 'barrel')
    for i in range(6):
        a_ = i * math.pi / 3 + math.pi / 6
        c_ = rod_z([(.330, .0028), (.434, .0025)], .0105 * cos(a_), .0160 + .0105 * sin(a_), 14)
        cut(brl, c_)
    finish(brl, GRAPH, .40, 0, .0004, 1)
    finish(rod_z([(.443, .0108), (.451, .0112)], 0, .0160, 24), DARK, .45, 0, .0005, 1)
    # Wide 3-chamber angled .338 precision muzzle brake
    mz = prof_front([
        (-.0144, .0085), (-.0102, .0046), (.0102, .0046), (.0144, .0085),
        (.0144, .0235), (.0102, .0274), (-.0102, .0274), (-.0144, .0235)
    ], .451, .506, 'brake')
    for i in range(3):
        zc = .463 + i * .0135
        ch_cut = box((0, .0160, zc), (.0200, .0068, .0042))
        cut(mz, ch_cut)
        cut(mz, box((0, .0265, zc), (.0036, .0050, .0034)))     # top gas compensator ports
    cut(mz, rod_z([(.497, .0054), (.509, .0058)], 0, .0160, 20))  # recessed muzzle crown bore
    finish(mz, BLACK, .44, 0, .0007, 2)

    # --- 5. Ergonomic target grip, squared chassis trigger guard & two-stage flat match trigger
    finish(prof([(-.0113, -.014), (-.044, -.014), (-.086, -.100), (-.080, -.110), (-.052, -.106), (-.040, -.066)], -.0126, .0126, 'grip'), BLACK, .76, 0, .0052, 5)
    finish(prof([(-.083, -.104), (-.049, -.101), (-.047, -.111), (-.082, -.114)], -.0142, .0142), DARK, .68, 0, .0014, 2)
    finish(prof([(-.020, -.0135), (-.015, -.0135), (-.015, -.0465), (.023, -.0465), (.029, -.024), (.034, -.024), (.026, -.0512), (-.020, -.0512)], -.0042, .0042), TAN, .60, 0, .0007, 2)
    finish(box((0, -.0180, .0075), (.0034, .0048, .0052)), DARK, .48, 0, .0005, 1)
    finish(prof([(.0052, -.018), (.0096, -.018), (.0088, -.0405), (.0046, -.0405)], -.0028, .0028), STEEL, .32, .92, .0004, 1)
    finish(prof([(.019, -.024), (.022, -.024), (.020, -.038), (.0165, -.038)], -.0048, .0048), BLACK, .52, 0, .0005, 1)  # ambi mag paddle

    # --- 6. Adjustable thumbhole skeleton stock (group 2): guide posts, thumbwheels, bag rider & spacers
    st = prof([(-.088, .0255), (-.236, .0255), (-.242, .0180), (-.244, -.0440), (-.228, -.0520), (-.186, -.0410), (-.088, -.0125)], -.0150, .0150, 'stock')
    cut(st, prof([(-.112, .0120), (-.208, .0120), (-.208, -.0305), (-.186, -.0275), (-.112, -.0050)], -.03, .03))
    finish(st, TAN, .60, 0, .0022, 3, group=2)
    # Contoured saddle cheekpiece + twin STEEL riser rods + left-side knurled locking thumbwheels
    cp = prof_front([
        (-.0168, .0210), (-.0168, .0375), (-.0115, .0445), (.0115, .0445),
        (.0168, .0375), (.0168, .0210), (.0128, .0210), (.0128, .0345),
        (-.0128, .0345), (-.0128, .0210)
    ], -.222, -.138, 'cheek')
    finish(cp, BLACK, .70, 0, .0014, 2, group=2)
    for z_post in (-.156, -.204):
        finish(rod_y(.0160, .0400, 0, z_post, .0034, 16), STEEL, .32, .92, .0003, 1, group=2)
        finish(rod_x(-.0202, -.0150, .0175, z_post, .0062, 18), DARK, .46, 0, .0006, 2, group=2)
        finish(rod_x(-.0212, -.0195, .0175, z_post, .0026, 12), STEEL, .32, .92, .0002, 1, group=2)
    # Lower bag-rider rail + LOP spacer plates + ribbed recoil buttpad
    finish(prof([(-.198, -.0390), (-.238, -.0455), (-.238, -.0535), (-.204, -.0535)], -.0095, .0095), DARK, .48, 0, .0008, 2, group=2)
    finish(rod_x(-.0160, -.0135, -.0280, -.226, .0050, 16), DARK, .46, 0, .0005, 1, group=2)
    finish(box((0, -.0130, -.2445), (.0154, .0395, .0032)), DARK, .48, 0, .0008, 2, group=2)
    finish(box((0, -.0130, -.2535), (.0162, .0425, .0062)), BLACK, .88, 0, .0020, 2, group=2)
    for ry in (-.036, -.021, -.006, .009):
        finish(box((0, ry, -.2592), (.0150, .0022, .0016)), DARK, .82, 0, .0004, 1, group=2)

    # --- 7. 34 mm precision tactical scope & Spuhr-style one-piece cantilever unimount (sy = 0.085)
    sy = .085
    # One-piece bridged mount base with central lightening cut, left clamp bar, 4 STEEL Torx bolts & bubble level
    mnt = box((0, sy - .0290, .033), (.0144, .0095, .0460))
    cut(mnt, box((0, sy - .0275, .033), (.0200, .0052, .0210)))
    finish(mnt, DARK, .48, 0, .0010, 2)
    finish(box((-.0154, sy - .0335, .033), (.0018, .0042, .0440)), BLACK, .48, 0, .0005, 1)
    for zb in (-.004, .010, .056, .070):
        finish(rod_x(-.0182, -.0145, sy - .0335, zb, .0026, 14), STEEL, .32, .92, .0003, 1)
    # Rear anti-cant spirit bubble level housing on left of mount base
    finish(box((-.0175, sy - .0340, -.0145), (.0032, .0028, .0040)), DARK, .48, 0, .0005, 1)
    finish(box((-.0178, sy - .0308, -.0145), (.0020, .0008, .0024)), (.36, .92, .28), .28, 0, .0002, 1, flag=3)
    # Split scope rings with 45-deg ears & STEEL cap screws
    for z in (-.002, .066):
        finish(rod_z([(z - .0090, .0184), (z + .0090, .0184)], 0, sy, 32), DARK, .46, 0, .0008, 2)
        for s_ in (-1, 1):
            finish(obox((s_ * .0138, sy + .0132, z), (.0042, .0030, .0084), rz=-s_ * .72), DARK, .46, 0, .0006, 1)
            for dz in (-.0045, .0045):
                finish(rod_y(sy + .0128, sy + .0174, s_ * .0142, z + dz, .0014, 10), STEEL, .32, .92, .0002, 1)
    # Main 34 mm scope body + hollow ocular (SCOPE_OCU_Z=-0.052, SCOPE_OCU_R=0.0205) & 50 mm objective bell
    sc = lathe([
        (-.056, .0196), (-.052, .0205), (-.031, .0205), (-.024, .0164),
        (-.012, .0152), (.092, .0152), (.118, .0218), (.166, .0218), (.169, .0206)
    ], 0, sy, 44, 'scope_tube')
    cut(sc, rod_z([(-.060, .0183), (-.050, .0183)], 0, sy, 36))    # recessed ocular cavity
    cut(sc, rod_z([(.153, .0192), (.172, .0192)], 0, sy, 36))     # recessed objective sunshade cavity
    finish(sc, BLACK, .40, 0, .0006, 2)
    # Ocular & objective STEEL retaining rings + convex multi-coated glass lenses + sunshade accent grooves
    finish(rod_z([(-.0552, .0188), (-.0520, .0188)], 0, sy, 32), STEEL, .32, .90, 0)
    finish(rod_z([(-.0541, .0003), (-.0536, .0105), (-.0522, .0182)], 0, sy, 32), LENS, .15, 0, 0)
    finish(rod_z([(.1535, .0195), (.1568, .0195)], 0, sy, 32), STEEL, .32, .90, 0)
    finish(rod_z([(.1538, .0190), (.1554, .0108), (.1560, .0003)], 0, sy, 32), LENS, .14, 0, 0)
    for zg in (.128, .158):
        finish(rod_z([(zg - .0015, .0222), (zg + .0015, .0222)], 0, sy, 36), DARK, .46, 0, .0003, 1)
    # Fast-focus diopter ribs (tucked behind eyecup rim so scoped ADS silhouette stays a clean circle!)
    for i in range(10):
        a_ = i * 2 * math.pi / 10
        finish(rod_z([(-.0485, .0010), (-.0355, .0010)], .0200 * cos(a_), sy + .0200 * sin(a_), 8), DARK, .55, 0, 0)
    finish(rod_z([(-.0245, .0170), (-.0135, .0170)], 0, sy, 30), DARK, .46, 0, .0006, 2)
    for i in range(12):
        a_ = i * 2 * math.pi / 12
        finish(rod_z([(-.0235, .0009), (-.0145, .0009)], .0170 * cos(a_), sy + .0170 * sin(a_), 8), BLACK, .50, 0, 0)
    finish(obox((-.0172, sy + .0152, -.0190), (.0070, .0024, .0034), rz=-.72), DARK, .44, 0, .0008, 2)
    finish(rod_z([(-.0208, .0024), (-.0172, .0024)], -.0212, sy + .0188, 12), STEEL, .32, .92, .0002, 1)
    # Turret saddle block + 3 full tactical knurled MIL turrets (top elevation, right windage, left parallax/illum)
    tz = .033
    finish(rod_z([(tz - .014, .0172), (tz + .014, .0172)], 0, sy, 28), GRAPH, .46, 0, .0010, 2)
    # Top Zero-Stop Elevation MIL Turret
    finish(rod_y(sy + .014, sy + .021, 0, tz, .0112, 24), DARK, .46, 0, .0005, 1)
    finish(rod_y(sy + .0205, sy + .0222, 0, tz, .0104, 24), SAND, .50, 0, 0)
    finish(rod_y(sy + .022, sy + .0345, 0, tz, .0108, 24), BLACK, .44, 0, .0008, 2)
    for i in range(12):
        a_ = i * 2 * math.pi / 12
        finish(rod_y(sy + .0235, sy + .0335, .0108 * cos(a_), tz + .0108 * sin(a_), .0009, 8), DARK, .48, 0, 0)
    finish(rod_y(sy + .0342, sy + .0362, 0, tz, .0042, 14), STEEL, .32, .92, .0003, 1)
    # Right-Side Windage Turret
    finish(rod_x(.014, .021, sy, tz, .0096, 20), DARK, .46, 0, .0005, 1)
    finish(rod_x(.021, .031, sy, tz, .0092, 20), BLACK, .44, 0, .0008, 2)
    finish(rod_x(.0308, .0324, sy, tz, .0036, 12), STEEL, .32, .92, .0002, 1)
    # Left-Side Parallax + Reticle Illumination Turret (facing the 1P camera!)
    finish(rod_x(-.0215, -.014, sy, tz, .0108, 22), DARK, .46, 0, .0005, 1)
    finish(rod_x(-.0228, -.0212, sy, tz, .0100, 22), SAND, .50, 0, 0)
    finish(rod_x(-.0325, -.0225, sy, tz, .0098, 22), BLACK, .44, 0, .0008, 2)
    for i in range(12):
        a_ = i * 2 * math.pi / 12
        finish(rod_x(-.0315, -.0235, sy + .0098 * sin(a_), tz + .0098 * cos(a_), .0008, 8), DARK, .48, 0, 0)
    finish(rod_x(-.0342, -.0322, sy, tz, .0068, 18), GRAPH, .44, 0, .0005, 1)
    finish(box((-.0344, sy, tz), (.0006, .0048, .0009)), STEEL, .32, .92, 0)

    # --- 8. Stamped 10-round steel AICS box magazine (group 1): ribs, witness holes, floorplate & front hook
    mag = prof([(.078, -.018), (.026, -.018), (.024, -.103), (.076, -.103)], -.0118, .0118, 'mag')
    finish(mag, GRAPH, .64, 0, .0016, 2, group=1)
    finish(box((0, -.060, .0772), (.0058, .0400, .0018)), DARK, .64, 0, .0006, 1, group=1)   # front spine rib
    for zr in (.037, .051, .065):
        finish(box((0, -.064, zr), (.0125, .0330, .0024)), DARK, .66, 0, .0006, 1, group=1)    # side stamped ribs
    for yw in (-.052, -.078):
        finish(rod_x(-.0126, .0126, yw, .029, .0018, 12), BLACK, .40, 0, 0, group=1)           # round witness holes
    finish(prof([(.022, -.101), (.082, -.101), (.084, -.111), (.021, -.112)], -.0134, .0134, 'magplate'), BLACK, .76, 0, .0012, 2, group=1)
    finish(prof([(.076, -.092), (.082, -.092), (.085, -.110), (.077, -.110)], -.0102, .0102, 'maghook'), BLACK, .76, 0, .0010, 2, group=1)
    finish(rod_y(-.1132, -.1105, 0, .050, .0026, 12), STEEL, .35, .90, .0002, 1, group=1)

# ------------------------------------------------------------------ pistol
def pistol():
    '''Two-tone duty pistol in the rifle's own frame: the grip, trigger and guard sit on
    exactly the rifle's stations, so the sculpted firing hand closes on it unchanged.
    Slide is group 3 (it cycles), magazine group 1, sights on the bore's centreline.'''
    clear()
    # --- polymer frame: tan like the rifle's furniture, dust cover and accessory rail
    fr = prof([(-.050,-.008),(.137,-.008),(.137,-.024),(.126,-.029),(.074,-.029),(.060,-.018),(.010,-.018),(-.012,-.016),(-.048,-.020)], -.0128, .0128, 'frame')
    finish(fr, TAN, .60, 0, .0014, 3)
    for i in range(4):
        finish(box((0,-.0312,.084+i*.0115), (.0086,.0026,.0030)), BLACK, .5, 0, .0005, 1)      # rail teeth
    finish(pin(-.0138,-.0120,-.0130,.030,.0036,12), BLACK, .48, .6, .0003, 1)                  # takedown pin
    finish(prof([(.060,-.0098),(.086,-.0098),(.086,-.0150),(.060,-.0150)], -.0142, -.0120), DARK, .5, 0, .0005)  # takedown lever
    finish(prof([(-.012,-.0110),(.018,-.0110),(.018,-.0150),(-.012,-.0150)], -.0142, -.0122), BLACK, .5, 0, .0005)  # slide catch
    # --- grip: the rifle's contour, with a beavertail and stippled side panels
    finish(prof([(.030,-.014),(-.008,-.013),(-.016,-.026),(-.036,-.100),(-.030,-.110),(.000,-.106),(.010,-.066),(.018,-.044)], -.0126, .0126, 'grip'), BLACK, .76, 0, .0052, 5)
    finish(prof([(-.052,-.0120),(-.012,-.0120),(-.012,-.0200),(-.030,-.0240),(-.050,-.0190)], -.0116, .0116, 'tail'), TAN, .60, 0, .0020, 2)
    for s_ in (-1, 1):
        x0, x1 = (s_*.0120, s_*.0136) if s_ > 0 else (s_*.0136, s_*.0120)
        finish(prof([(-.006,-.040),(-.024,-.042),(-.033,-.094),(-.010,-.094),(.002,-.066)], x0, x1, 'panel'), TAN, .72, 0, .0007, 2)
    # --- trigger guard and blade: identical stations to the rifle's
    finish(prof([(.014,-.018),(.019,-.018),(.021,-.0475),(.068,-.0475),(.079,-.018),(.084,-.018),(.071,-.0518),(.014,-.0518)], -.0038, .0038), TAN, .60, bevel=.0007)
    finish(prof([(.044,-.016),(.050,-.016),(.044,-.042),(.039,-.042)], -.0030, .0030), DARK, .45, .6, .0005)
    # --- slide (group 3): graphite, ejection port, chamfered top, rear and front serrations
    sl = prof_front([(-.0118,-.0100),(.0118,-.0100),(.0128,-.0040),(.0128,.0190),(.0092,.0250),(-.0092,.0250),(-.0128,.0190),(-.0128,-.0040)], -.052, .146, 'slide')
    cut(sl, box((.0130,.0135,.040), (.0068,.0072,.0225)))                                     # ejection port
    cut(sl, lathe([(.138,.0058),(.150,.0058)], 0, .0060, 20))                                 # bore
    for i in range(7):
        for s_ in (-1, 1):
            cut(sl, box((s_*.0134,.0080,-.044+i*.0052), (.0014,.0086,.0012)))
    for i in range(4):
        for s_ in (-1, 1):
            cut(sl, box((s_*.0134,.0080,.112+i*.0052), (.0014,.0086,.0012)))
    finish(sl, GRAPH, .50, .12, .0012, 3, group=3)
    finish(box((.0098,.0135,.040), (.0034,.0064,.0205)), STEEL, .35, .9, .0003, 1, group=3)   # chamber hood
    finish(lathe([(.134,.0064),(.1475,.0060)], 0, .0060, 20), STEEL, .36, .9, .0002, group=3) # barrel crown
    # --- sights (on the slide): fibre-dot front post and a notched rear blade
    finish(box((0,.0290,.1305), (.0021,.0052,.0052)), BLACK, .55, 0, .0005, group=3)
    finish(lathe([(.1244,.0019),(.1262,.0019)], 0, .0322, 14), AMBER, .35, 0, 0, group=3, flag=4)
    rear = box((0,.0290,-.0405), (.0106,.0052,.0042), 'rear')
    cut(rear, box((0,.0340,-.0405), (.0026,.0048,.0100)))
    finish(rear, BLACK, .55, 0, .0005, group=3)
    # --- double-stack magazine (group 1), raked with the grip
    finish(prof([(.012,-.024),(-.012,-.024),(-.032,-.104),(-.008,-.104)], -.0098, .0098, 'mag'), GRAPH, .64, 0, .0012, 2, group=1)
    finish(prof([(-.036,-.1040),(-.002,-.1040),(.002,-.1150),(-.034,-.1180)], -.0124, .0124, 'magplate'), BLACK, .76, 0, .0012, 2, group=1)

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
NADE_SPOON_TH = -28.0                            # where the lever lies round the body
NADE_E = Vector((0,-.386,.921)).normalized()    # from the palm toward the fingers
NADE_A = Vector((0,.921,.386)).normalized()      # up the fuze
NADE_C = Vector((0,-.0500,-.0078)) + NADE_E*.0370   # the body's centre, clear of the palm

def nade_r(z):
    '''Outer radius of the grenade at height z along its axis: shell, then fuze neck.'''
    t = min(abs(z)/.0350, 1.0)
    r = .0300*(1-t**2.6)**.46 + .0004
    pts = ((.0300,.0172),(.0345,.0142),(.0410,.0126),(.0470,.0118),(.0540,.0090))
    for (z0,r0),(z1,r1) in zip(pts, pts[1:]):
        if z0 <= z <= z1: r = max(r, r0 + (r1-r0)*(z-z0)/(z1-z0))
    return r

def grenade():
    '''Round-bodied frag grenade on the same grip cylinder as the knife, seated forward of it
    by the body's radius. Group 0 is what flies; group 2 the spoon, which is left behind at
    the throw, and group 3 the pin and ring, which the off hand pulls.'''
    clear()
    def shell_r(z):
        t = min(abs(z)/.0350, 1.0)
        return .0300*(1-t**2.6)**.46 + .0004
    rings = [(.0350*(-1 + 2*i/22), shell_r(.0350*(-1 + 2*i/22))) for i in range(23)]
    finish(lathe(rings, 0, 0, 44), (.34,.40,.25), .52, 0, 0, flag=4)        # olive painted body
    # Band and ribs follow the shell's own curve: at a fixed radius they stood off it
    # as floating hoops that read as stripes across the body.
    def hoop(z0, z1, h):
        n_ = 6
        return [(z0, nade_r(z0)-.0003)] + [(z0+(z1-z0)*i/(n_-1), nade_r(z0+(z1-z0)*i/(n_-1))+h) for i in range(n_)] + [(z1, nade_r(z1)-.0003)]
    finish(lathe(hoop(.0090, .0165, .0004), 0, 0, 44), ORANGE, .52, 0, 0, flag=4)  # blaze band: live
    for i in range(3):
        z = -.018 + i*.0095
        finish(lathe(hoop(z-.0008, z+.0008, .0007), 0, 0, 44), (.22,.26,.16), .58, 0, 0, flag=4)  # seam ribs, proud enough to catch the light
    finish(lathe([(.0280,.0140),(.0300,.0176),(.0345,.0146),(.0410,.0126),(.0470,.0118),(.0495,.0086)], 0, 0, 44), GRAPH, .32, .75, 0)  # fuze well: bare metal against the matte paint
    finish(lathe([(.0470,.0092),(.0530,.0088),(.0540,.0060)], 0, 0, 24), STEEL, .36, .9, .0003)  # striker cap
    # The spoon: stamped sheet 1.5 mm thick. Its cap sits flush on the striker, then it
    # drops tight down the neck and shell, with only a hair of air. -y is the palm side.
    zs = [.0300 - i*.0030 for i in range(21)]
    out = [(.0572,.0070),(.0572,-.0100)] + [(.0520,-.0138),(.0460,-.0142),(.0400,-.0152),(.0340,-.0172)] + [(z,-(nade_r(z)+.0021)) for z in zs]
    inn = [(z,-(nade_r(z)+.0006)) for z in zs][::-1] + [(.0340,-.0157),(.0400,-.0136),(.0460,-.0126),(.0505,-.0122),(.0552,-.0086),(.0552,.0070)]
    sp = finish(prof(out + inn, -.0088, .0088, 'spoon'), (.47,.47,.43), .40, .70, .0004, 2, group=2)
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
        tan = o['col'][0] > o['col'][2] * 1.3
        if o['rough'] >= .62 and not (tan and o['group'] == 0): o['flag'] = 4
        elif tan: o['flag'] = 6
        else: o['flag'] = 5

rifle(); assign_finishes()
emit(store, 'RIFLE')
hand_r(); emit(store, 'HAND_R')
hand_l(); emit(store, 'HAND_L')
bracer(); emit(store, 'BRACER')
sniper(); assign_finishes()
emit(store, 'SNIPER')
pistol(); assign_finishes()
emit(store, 'PISTOL')
hand_p(); emit(store, 'HAND_P')
knife(); assign_finishes()
emit(store, 'KNIFE')
hand_r(fist=True); emit(store, 'HAND_K')
hand_r(fist=True); mirror_x(); emit(store, 'HAND_KL')
hand_g(); occ_hand = snapshot()
grenade(); assign_finishes(); occ_nade = snapshot()
emit(store, 'GRENADE', extra=occ_hand)
hand_g(); emit(store, 'HAND_G', extra=occ_nade)
for kfn, ktag in ((knife_tanto, 'KNIFE_TANTO'), (knife_dagger, 'KNIFE_DAGGER'),
                  (knife_kukri, 'KNIFE_KUKRI'), (knife_talon, 'KNIFE_TALON')):
    kfn(); assign_finishes(); emit(store, ktag)
hand_open(); emit(store, 'HAND_KO')
hand_pin(); emit(store, 'HAND_GP')

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
        for x, y, z, a, b, c, ao in rows:
            q = lambda v: max(-32767, min(32767, int(round(v*32000))))
            n = lambda v: max(-32767, min(32767, int(round(v*32767))))
            line.append('{%d,%d,%d,%d,%d,%d,%d}' % (q(x), q(y), q(z), n(a), n(b), n(c), ao))
            if len(line) == 8:
                f.write(','.join(line) + ',\n'); line = []
        if line: f.write(','.join(line) + ',\n')
        f.write('};\nstatic const model_part_t %s_P[] = {\n' % tag)
        for p in parts:
            f.write('  {%d,%d,%.3ff,%.3ff,%.3ff,%.3ff,%.3ff,%d,%d},\n' % p)
        f.write('};\n#define %s_NP %d\n' % (tag, len(parts)))
    for d in EXTRA_DEFINES: f.write(d + '\n')
print('WROTE', OUT)

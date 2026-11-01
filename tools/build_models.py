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
STEEL=(.46,.46,.47); AMBER=(1.0,.60,.10); DECAL=(.95,.78,.16); ORANGE=(1.0,.52,.08); SLATE=(.145,.17,.24); PAD=(.115,.118,.128); HOLO=(.19,.195,.205); SAND=(.60,.50,.37)
SLEEVE=(.165,.18,.215); HARD=(.235,.285,.375)

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

def finish(o, col, rough=.55, metal=0.0, bevel=.0009, seg=2, group=0, flag=0):
    bpy.context.view_layer.objects.active = o
    if bevel > 0:
        m = o.modifiers.new('b', 'BEVEL'); m.width = bevel; m.segments = seg
        m.limit_method = 'ANGLE'; m.angle_limit = radians(32); m.harden_normals = False
    o.data.polygons.foreach_set('use_smooth', [True]*len(o.data.polygons))
    o['col'] = list(col); o['rough'] = rough; o['metal'] = metal; o['group'] = group; o['flag'] = flag
    return o

def emit(store, tag):
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
        if o['flag'] not in (1, 2): me.set_sharp_from_angle(angle=radians(38))
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
    # --- handguard: short octagonal tube with cut slots
    hg = prof_front([(.0245*cos(math.pi/8+i*math.pi/4), .0200+.0245*sin(math.pi/8+i*math.pi/4)) for i in range(8)], .170, .398, 'hg')
    for i in range(5):
        cut(hg, box((-.0255,.0200,.200+i*.041), (.0080,.0038,.0145)))
        cut(hg, box(( .0255,.0200,.200+i*.041), (.0080,.0038,.0145)))
        cut(hg, box((0,-.0060,.200+i*.041), (.0038,.0080,.0145)))
    finish(hg, TAN, .60, bevel=.0012, seg=2)
    for s_ in (-1, 1):
        finish(box((s_*.0262,.0200,.3660), (.0024,.0068,.0280)), BLACK, .5, 0, .0005)
        for i in range(6):
            finish(box((s_*.0288,.0200,.3400+i*.0104), (.0018,.0074,.0029)), BLACK, .5, 0, .0004, 1)
    finish(lathe([(.158,.0250),(.170,.0268),(.176,.0268),(.179,.0240)], 0, .0200, 24), GRAPH, .5, 0, .0006)   # barrel nut
    # --- short exposed barrel and birdcage
    finish(lathe([(.396,.0085),(.418,.0080)], 0, .0220, 20), BLACK, .42, 0, 0)
    mz = lathe([(.412,.0084),(.416,.0108),(.452,.0108),(.462,.0094),(.462,.0060)], 0, .0220, 22)
    for i in range(3):
        a_ = i*math.pi/3
        c_ = box((0,.0220,.436), (.0026,.0160,.0130))
        c_.rotation_euler = (0,0,a_); bpy.context.view_layer.objects.active = c_; c_.select_set(True)
        bpy.ops.object.transform_apply(rotation=True); c_.select_set(False)
        cut(mz, c_)
    finish(mz, BLACK, .45, 0, .0004)
    # --- wide charging handle, its latch reaching out to the left
    finish(prof([(-.070,.0330),(-.016,.0330),(-.016,.0418),(-.060,.0418),(-.070,.0392)], -.0140, .0140), BLACK, .5, 0, .0012)
    finish(prof([(-.072,.0336),(-.034,.0336),(-.040,.0412),(-.072,.0412)], -.0340, -.0120), DARK, .5, 0, .0014)
    # --- left-side controls
    finish(prof([(.066,.0090),(.094,.0090),(.094,.0005),(.072,.0005)], -.0172, -.0132), BLACK, .55, 0, .0006)  # bolt catch
    finish(pin(-.0172,-.0132,.0005,.030,.0078), DARK, .5, bevel=.0005)                  # selector drum
    finish(prof([(.030,.0035),(.054,.0020),(.054,-.0030),(.030,-.0025)], -.0186, -.0162), BLACK, .5, 0, .0006)
    for z, y in ((.138,.0035),(.010,.0005),(-.018,.0035)):
        finish(pin(-.0150,-.0134,y,z,.0032,12), STEEL, .42, .9, .0002, 1)
    finish(pin(-.0176,-.0150,-.0060,.106,.0052), BLACK, .55, 0, .0004)                  # mag release
    # --- grip and guard
    finish(prof([(.030,-.014),(.002,-.014),(-.036,-.100),(-.030,-.110),(.000,-.106),(.010,-.066),(.018,-.044)], -.0120, .0120, 'grip'), BLACK, .82, 0, .0034, 4)
    finish(prof([(.012,-.018),(.016,-.018),(.016,-.0465),(.068,-.0465),(.080,-.018),(.084,-.018),(.071,-.0505),(.012,-.0505)], -.0036, .0036), BLACK, .6, bevel=.0007)
    finish(prof([(.040,-.016),(.046,-.016),(.040,-.040),(.035,-.040)], -.0030, .0030), STEEL, .4, .8, .0004)
    # --- buffer tube, castle nut, carbine stock
    finish(lathe([(-.182,.0172),(-.040,.0172)], 0, .0210, 28), GRAPH, .46, 0, .0005)
    nut = lathe([(-.056,.0240),(-.036,.0240),(-.032,.0205)], 0, .0210, 28)
    for i in range(6):
        c_ = box((0,.0210+.0232,-.0456), (.0026,.0030,.0074))
        c_.rotation_euler = (0,0,0)
        v_ = i*math.pi/3
        for vv in c_.data.vertices:
            x_, y_ = vv.co.x, vv.co.y - .0210
            vv.co.x = x_*cos(v_) - y_*sin(v_); vv.co.y = .0210 + x_*sin(v_) + y_*cos(v_)
        cut(nut, c_)
    finish(nut, BLACK, .5, 0, .0005)
    finish(prof([(-.040,-.012),(-.024,-.012),(-.024,.012),(-.040,.012)], -.0185, .0185), DARK, .55, 0, .0012)  # receiver end plate
    OFS = .052
    st = prof([(-.114+OFS,.0425),(-.226+OFS,.0425),(-.236+OFS,.036),(-.240+OFS,-.040),(-.226+OFS,-.050),(-.196+OFS,-.034),(-.160+OFS,-.006),(-.114+OFS,-.004)], -.0170, .0170, 'stock')
    cut(st, prof([(-.222+OFS,.036),(-.212+OFS,.036),(-.212+OFS,-.030),(-.222+OFS,-.036)], -.03, .03))
    for y0, y1 in ((.0200,.0300),(.0020,.0120)):
        cut(st, prof([(-.116,y0),(-.172,y0),(-.172,y1),(-.116,y1)], -.03, .03))
    finish(st, TAN, .62, 0, .0024, 3, group=2)
    finish(box((0,-.0020,-.2455+OFS), (.0178,.0500,.0065)), BLACK, .9, 0, .0024, 2, group=2)
    finish(box((0,-.0090,-.132+OFS), (.0080,.0050,.012)), DARK, .5, 0, .0008, group=2)
    # --- holographic sight: bulky hood, rear electronics, side controls
    hz = .128
    hous = prof_front([(-.0300,.0478),(.0300,.0478),(.0345,.0525),(.0345,.1040),(.0280,.1125),(-.0280,.1125),(-.0345,.1040),(-.0345,.0525)], hz-.040, hz+.052, 'holo')
    cut(hous, prof_front([(-.0185,.0660),(.0185,.0660),(.0235,.0710),(.0235,.0990),(.0185,.1040),(-.0185,.1040),(-.0235,.0990),(-.0235,.0710)], hz-.1, hz+.1))
    finish(hous, HOLO, .58, 0, .0026, 3)
    finish(box((0,.0568,hz-.0426), (.0268,.0074,.0030)), BLACK, .6, 0, .0010)               # rear control plate
    for xx, yy in ((-.0222,.0614),(.0222,.0614),(-.0222,.0522),(.0222,.0522)):
        finish(lathe([(hz-.0478,.0026),(hz-.0446,.0026)], xx, yy, 14), STEEL, .4, 0, .0003)  # screws
    finish(lathe([(hz-.0486,.0058),(hz-.0446,.0058)], 0, .0568, 20), DARK, .45, 0, .0005)   # dial
    finish(box((-.0120,.0568,hz-.0458), (.0052,.0034,.0012)), DARK, .5, 0, .0003)           # buttons
    finish(box(( .0120,.0568,hz-.0458), (.0052,.0034,.0012)), DARK, .5, 0, .0003)
    finish(prof([(hz-.032,.0560),(hz+.026,.0560),(hz+.026,.0880),(hz-.022,.0880),(hz-.032,.0800)], -.0410, -.0330, 'bat'), HOLO, .55, 0, .0018, 3)
    finish(box((-.0416,.0720,hz-.008), (.0006,.0058,.0044)), AMBER, .4, 0, .0002, flag=3)   # twin lights
    finish(box((-.0416,.0720,hz+.003), (.0006,.0058,.0044)), AMBER, .4, 0, .0002, flag=3)
    finish(pin(-.0460,-.0340,.0540,hz+.036,.0088,20), DARK, .45, 0, .0007)                  # QD knob
    finish(pin(-.0480,-.0455,.0540,hz+.036,.0044,16), STEEL, .4, 0, .0004)
    finish(pin(-.0460,-.0340,.0540,hz-.026,.0064,16), DARK, .45, 0, .0006)
    finish(prof([(hz-.058,.0460),(hz+.062,.0460),(hz+.062,.0490),(hz-.058,.0490)], -.0170, .0170), BLACK, .55, 0, .0008)   # mount
    # --- the magazine moves, so it is its own group
    mag = prof([(.136,-.024),(.092,-.024),(.060,-.150),(.056,-.168),(.106,-.172),(.110,-.150)], -.0104, .0104, 'mag')
    finish(mag, DARK, .70, 0, .0020, 2, group=1)
    for i in range(6):
        finish(box((0,-.060-.016*i,.113-.0050*i), (.0112,.0022,.0225)), BLACK, .72, 0, .0009, 1, group=1)

# ------------------------------------------------------------------ gloves
def path_skin(pts, name='digit'):
    """pts = [((x,y,z), rx, ry), ...]; a soft tube along a path."""
    me = bpy.data.meshes.new(name)
    me.from_pydata([p[0] for p in pts], [(i, i+1) for i in range(len(pts)-1)], [])
    o = bpy.data.objects.new(name, me); bpy.context.collection.objects.link(o)
    m = o.modifiers.new('s', 'SKIN')
    sk = me.skin_vertices[0].data
    for i, p in enumerate(pts):
        sk[i].radius = (p[1], p[2])
    sk[0].use_root = True
    return o

def soft(o, lv=2):
    bpy.context.view_layer.objects.active = o
    bpy.ops.object.modifier_apply(modifier='s')
    m = o.modifiers.new('ss', 'SUBSURF'); m.levels = lv; m.render_levels = lv
    bpy.ops.object.modifier_apply(modifier='ss')
    return o

def V3(*a): return Vector(a)

def tube(pts):
    """pts = [(point, radius), ...]"""
    return path_skin([(tuple(p), r, r) for p, r in pts], 'digit')

def merge_glove(parts):
    """Fuse the bones into one skin, as a stuffed glove is one surface."""
    for p in parts: p.select_set(True)
    for p in parts:
        bpy.context.view_layer.objects.active = p
        bpy.ops.object.modifier_apply(modifier='s')
    bpy.context.view_layer.objects.active = parts[0]
    bpy.ops.object.join(); g = bpy.context.view_layer.objects.active
    r = g.modifiers.new('r', 'REMESH'); r.mode = 'VOXEL'; r.voxel_size = .0020; r.use_smooth_shade = True
    bpy.ops.object.modifier_apply(modifier='r')
    s = g.modifiers.new('sm', 'SMOOTH'); s.iterations = 34; s.factor = .8
    bpy.ops.object.modifier_apply(modifier='sm')
    d = g.modifiers.new('d', 'DECIMATE'); d.ratio = .22
    finish(g, SLATE, .80, 0, 0, flag=2)

def flat(c, along, out, hl, hw, ht=.0024, col=None, b=.0014):
    """A flat stitched pad: a bevelled slab centred on c."""
    c = Vector(c); a_ = Vector(along).normalized(); o_ = Vector(out)
    o_ = (o_ - a_*o_.dot(a_)).normalized(); w_ = a_.cross(o_)
    bm = bmesh.new(); bmesh.ops.create_cube(bm, size=2.0)
    for v in bm.verts:
        v.co = c + a_*(v.co.x*hl) + w_*(v.co.y*hw) + o_*(v.co.z*ht)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    finish(from_bm(bm, 'flat'), col or PAD, .58, 0, b, 2)

def pad(a, b, out, r=.0088, lift=.0086, col=None):
    """A stitched hard pad lying along a-b, raised on `out`."""
    a = Vector(a); b = Vector(b); o_ = Vector(out).normalized() * lift
    o = tube([(a + o_, r), (b + o_, r)])
    soft(o); finish(o, col or PAD, .56, 0, 0)

def hand_l():
    """Support hand. Every digit is routed around the outside of the handguard
    section, so a finger cannot pass through the gun whatever the view."""
    clear()
    CY = .0200                                      # handguard axis height
    def cyl(deg, r, z):
        a_ = radians(deg); return V3(r*cos(a_), CY + r*sin(a_), z)
    W = V3(-.060, -.112, .262)
    parts = []; rows = []
    #            little ring   middle index
    TH0 =       (252.,  234.,  216.,  199.)         # knuckle bearing round the section
    Z0  =       (.2520, .2805, .3090, .3375)
    TH1 =       (128.,  116.,  110.,  116.)         # fingertip bearing, clear of the rail
    RAD =       (.0098, .0104, .0108, .0102)
    for i in range(4):
        R_ = .0245 + RAD[i] + .0006                 # lies on the handguard's skin
        mcp = cyl(TH0[i], R_ + .0050, Z0[i])
        base = W + V3(0, 0, -.0240 + .0160*i)
        parts.append(tube([(base, .0165), ((base+mcp)/2 + V3(-.003,-.002,0), .0160), (mcp, .0145)]))
        pts = []
        for k, u in enumerate((0.0, .33, .66, 1.0)):
            d = TH0[i] + (TH1[i] - TH0[i]) * u
            rr = RAD[i] * (1.02, 1.0, .96, .84)[k]
            pts.append((cyl(d, R_ + (.0050 if k == 0 else 0.0), Z0[i] + .0030*(i-1.5)*u), rr))
        parts.append(tube(pts))
        rows.append((pts, R_))
    # the thumb runs under the handguard and up the far side
    th = [(V3(-.0500,-.0800,.2860), .0200), (cyl(262., .0450, .3220)+V3(0,-.0040,0), .0165), (cyl(305., .0392, .3360), .0132), (cyl(345., .0376, .3420), .0108)]
    parts.append(tube(th))
    parts.append(tube([(W + V3(0,0,-.018), .0230), (W + V3(0,0,.018), .0230)]))
    merge_glove(parts)
    for pts, R_ in rows:
        for a_, b_ in ((0, 1), (1, 2)):
            c = (pts[a_][0] + pts[b_][0]) / 2
            o_ = V3(c.x, c.y - CY, 0)
            flat(c + o_.normalized()*(pts[a_][1]*1.06), pts[b_][0]-pts[a_][0], o_, .0062, .0072, .0016, b=.0011)
        o_ = V3(pts[0][0].x, pts[0][0].y - CY, 0)
        flat(pts[0][0] + o_.normalized()*.0128, V3(0,0,1).cross(o_), o_, .0064, .0080, .0022, b=.0016)
    flat(W + V3(-.0225,.0130,.0030), (0,0,1), (-.9,.1,-.3), .0250, .0072, .0024, DARK)      # wrist strap

def hand_r():
    """Firing hand: fingers round the grip's front, thumb across the near side."""
    clear()
    G0 = V3(0, -.020, .014)
    u = V3(0, -.897, -.443); n = V3(0, -.443, .897); x = V3(1, 0, 0)
    W = V3(.058, -.100, -.098)
    parts = []
    finger = []
    for i, (s, r) in enumerate(((.040, .0136), (.0635, .0134), (.0860, .0124))):
        c = G0 + u*s
        mcp = c + x*.0360 + n*.0080
        pip = c + x*.0250 + n*.0400
        dip = c + x*-.0105 + n*.0400
        tip = c + x*-.0290 + n*.0140
        parts.append(tube([(mcp, r*1.12), (pip, r*1.08), (dip, r*1.0), (tip, r*.86)]))
        base = W + V3(-.006*i, -.014 + .016*i, 0)
        parts.append(tube([(base, .0205), (mcp, .0190)]))
        finger.append((pip, dip, tip))
    c = G0 + u*.012
    m = c + x*.0360 + n*.0120
    parts.append(tube([(m, .0150), (V3(.0250,-.0290,.0520), .0132), (V3(.0120,-.0300,.0600), .0118), (V3(.0005,-.0320,.0440), .0100)]))
    parts.append(tube([(W + V3(.000,.022,.000), .0205), (m, .0190)]))
    th = [(V3(.0300,-.0480,-.0660), .0240), (V3(.0120,-.0140,-.0420), .0200), (V3(-.0230,-.0130,-.0090), .0158), (V3(-.0290,-.0100,.0240), .0132)]
    parts.append(tube(th))
    parts.append(tube([(W + V3(0,-.018,0), .0280), (W + V3(0,.018,0), .0280), (V3(.022,-.066,-.056), .0280)]))
    merge_glove(parts)
    for pip, dip, tip in finger:          # segment panels wrapping the grip's front
        o_ = (dip - (G0 + u*((dip-G0).dot(u)))).normalized()
        flat((pip+dip)/2 + o_*.0118, dip-pip, o_, .0120, .0086, .0020)
        flat((dip+tip)/2 + V3(-.0102,0,.0040), tip-dip, (-1,0,.3), .0092, .0078, .0018)
    flat(th[1][0]*.5 + th[2][0]*.5 + V3(-.0040,.0125,-.0040), th[2][0]-th[1][0], (-.45,.85,-.2), .0190, .0115, .0028)
    flat(th[2][0]*.3 + th[3][0]*.7 + V3(-.0100,.0090,0), th[3][0]-th[2][0], (-.75,.6,0), .0100, .0078, .0020)
    flat(V3(.016,-.0720,-.0760), (.45,-.55,-.7), (-.8,.1,-.55), .0300, .0215, .0030)        # back of hand

# ------------------------------------------------------------------ forearm armour
def bracer():
    clear()
    # frame: x = wrist to elbow, y = the arm's visible face, z = across the arm
    def arm(rings, col, rough, seg=32, flag=0, b=0.0):
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
    arm([(-.030,.0300,.0345),(.054,.0335,.0385)], SLEEVE, .85, flag=1)
    for i in range(4):                                  # ribbed cuff
        x = -.018 + i*.0170; r = .0318 + i*.0008
        arm([(x-.0052,r,r*1.15),(x-.0030,r+.0026,(r+.0026)*1.15),(x+.0030,r+.0026,(r+.0026)*1.15),(x+.0052,r,r*1.15)], PAD, .72)
    K = 1.14
    arm([(.052,.0352,.0352*K),(.058,.0410,.0410*K),(.126,.0425,.0425*K),(.132,.0385,.0385*K),(.138,.0440,.0440*K),(.214,.0465,.0465*K),(.220,.0425,.0425*K),(.226,.0480,.0480*K),(.340,.0515,.0515*K),(.460,.0530,.0530*K)], HARD, .44, seg=8, b=.0009)
    fy = lambda x: .924 * (.0410 + (x-.058)*.0275)      # the flat top of the octagon
    flat((.092,fy(.092),0), (1,0,0), (0,1,0), .0240, .0172, .0016, BLACK, .0006)
    finish(box((.092,fy(.092)+.0020,0), (.0178,.0008,.0124)), AMBER, .3, 0, .0003, flag=3)
    flat((.176,fy(.176)+.0015,0), (1,0,0), (0,1,0), .0270, .0178, .0014, DARK, .0006)
    flat((.176,fy(.176)+.0032,0), (1,0,0), (0,1,0), .0224, .0138, .0010, SAND, .0005)
    finish(box((.282,.924*.0495+.0008,0), (.0380,.0009,.0052)), ORANGE, .3, 0, .0003, flag=3)

def sniper():
    clear()
    # chassis in tan, action in black; hands and scope positions come from the game
    ch = prof([(.140,.0300),(-.078,.0300),(-.086,.0220),(-.086,-.0120),(-.040,-.0220),(.100,-.0220),(.140,-.0060)], -.0160, .0160, 'chassis')
    cut(ch, box((0,-.018,.052), (.0130,.030,.0275)))
    finish(ch, TAN, .60, 0, .0014, 3)
    finish(prof([(.100,.0420),(-.070,.0420),(-.076,.0360),(-.076,.0250),(.100,.0250)], -.0142, .0142, 'action'), BLACK, .5, 0, .0014, 3)
    finish(box((0,.0444,.024), (.0090,.0026,.0800)), BLACK, .55, 0, .0006)
    for i in range(14):
        finish(box((0,.0478,-.040+.0100*i), (.0100,.0014,.0027)), BLACK, .5, 0, .0004, 1)
    fe = prof_front([(.0235*cos(math.pi/8+i*math.pi/4), .0140+.0235*sin(math.pi/8+i*math.pi/4)) for i in range(8)], .100, .286, 'fore')
    for i in range(3):
        cut(fe, box((-.0250,.0140,.150+i*.044), (.0075,.0036,.0160)))
        cut(fe, box(( .0250,.0140,.150+i*.044), (.0075,.0036,.0160)))
    finish(fe, TAN, .60, 0, .0012, 2)
    b_ = lathe([(.280,.0108),(.452,.0086)], 0, .0160, 24)
    finish(b_, BLACK, .42, 0, 0)
    mz = lathe([(.446,.0100),(.452,.0122),(.494,.0122),(.500,.0100),(.500,.0060)], 0, .0160, 24)
    for i in range(3):
        cut(mz, box((0,.0160,.462+i*.0130), (.016,.0034,.0034)))
    finish(mz, BLACK, .45, 0, .0005)
    finish(prof([(-.0233+.012,-.016),(-.044,-.016),(-.086,-.100),(-.080,-.110),(-.052,-.106),(-.040,-.066)], -.0120, .0120), BLACK, .82, 0, .0034, 4)
    finish(prof([(-.018,-.018),(-.014,-.018),(-.014,-.0465),(.028,-.0465),(.034,-.022),(.038,-.022),(.031,-.0505),(-.018,-.0505)], -.0036, .0036), BLACK, .6, 0, .0007)
    finish(pin(.0130,.0300,.0350,-.018,.0052), STEEL, .4, 0, .0005)                         # bolt handle
    finish(pin(.0290,.0350,.0300,-.018,.0092,18), DARK, .45, 0, .0007)
    # stock
    st = prof([(-.086,.0260),(-.238,.0260),(-.246,.0180),(-.248,-.0440),(-.232,-.0520),(-.190,-.0400),(-.086,-.0120)], -.0150, .0150, 'stock')
    cut(st, prof([(-.112,.0120),(-.206,.0120),(-.206,-.0300),(-.186,-.0270),(-.112,-.0050)], -.03, .03))
    finish(st, TAN, .62, 0, .0022, 3, group=2)
    finish(box((0,.0380,-.190), (.0120,.0070,.0420)), BLACK, .7, 0, .0016, 2, group=2)      # cheek
    finish(box((0,-.0130,-.2530), (.0160,.0420,.0060)), BLACK, .9, 0, .0022, 2, group=2)
    # scope, on the game's sight line
    sy = .085
    finish(lathe([(-.058,.0170),(-.052,.0205),(-.030,.0205),(-.018,.0150),(.092,.0150),(.112,.0195),(.150,.0205),(.156,.0190)], 0, sy, 36), BLACK, .42, 0, .0006)
    finish(lathe([(-.056,.0165),(-.053,.0165)], 0, sy, 30), STEEL, .4, 0, 0)
    for z in (-.002,.066):
        finish(box((0,sy-.0250,z), (.0130,.0090,.0090)), DARK, .5, 0, .0010)                # ring bases
        finish(lathe([(z-.0080,.0176),(z+.0080,.0176)], 0, sy, 30), DARK, .5, 0, .0008)
    finish(pin(-.0065,.0065,sy+.0240,.034,.0088,22), DARK, .45, 0, .0007)                   # placeholder turret base
    finish(pin(.0130,.0240,sy,.034,.0085,22), BLACK, .45, 0, .0006)
    finish(box((0,sy+.0215,.034), (.0078,.0120,.0078)), BLACK, .45, 0, .0030, 3)
    # magazine
    finish(prof([(.078,-.018),(.026,-.018),(.024,-.104),(.076,-.104)], -.0120, .0120), DARK, .7, 0, .0018, 2, group=1)

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
print('WROTE', OUT)

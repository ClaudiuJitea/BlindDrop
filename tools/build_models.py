# BlindDrop first-person models. Run: blender -b --factory-startup -P tools/build_models.py
# Sculpts the rifle, gloves and forearm armour and writes code/models.h, which is
# compiled into the game. Units are metres. Rifle axes match the game's weapon
# frame: x right, y up, z forward.
import bpy, bmesh, math, os, sys
from math import cos, sin, radians
from mathutils import Vector
from mathutils.bvhtree import BVHTree

OUT = os.path.join(os.path.dirname(os.path.abspath(sys.argv[sys.argv.index('-P') + 1])), '..', 'code', 'models.h')
TAN=(.63,.53,.39); GRAPH=(.36,.37,.39); DARK=(.30,.305,.32); BLACK=(.20,.203,.21)
STEEL=(.46,.46,.47); AMBER=(1.25,.78,.18); ORANGE=(1.0,.52,.08); SLATE=(.215,.24,.295); PAD=(.31,.315,.335); SAND=(.60,.50,.37)
SLEEVE=(.165,.18,.215); HARD=(.19,.215,.275)

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
        me.set_sharp_from_angle(angle=radians(38))
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
    low = prof([(-.030,.012),(.150,.012),(.150,-.004),(.146,-.030),(.140,-.036),(.086,-.040),(.080,-.014),(.046,-.018),(.010,-.018),(-.012,-.012),(-.030,-.002)], -.0140, .0140, 'lower')
    cut(low, box((0,-.018,.113), (.0118,.034,.0250)))
    cut(low, prof([(.096,.004),(.136,.004),(.132,-.022),(.098,-.026)], -.0200, -.0134))
    finish(low, TAN, .60, bevel=.0013, seg=3)
    finish(prof([(-.022,.009),(.070,.009),(.070,.004),(-.022,-.002)], -.0146, -.0132), SAND, .6, bevel=.0005)
    # --- upper receiver: graphite, right-side ejection port, a long relief on the left
    up = prof([(-.030,.012),(.172,.012),(.172,.0345),(.166,.0370),(-.022,.0370),(-.030,.0315)], -.0150, .0150, 'upper')
    cut(up, box((.0170,.0225,.070), (.0040,.0070,.0270)))
    cut(up, box((-.0170,.0245,.086), (.0030,.0048,.0600)))
    finish(up, GRAPH, .52, bevel=.0015, seg=3)
    finish(box((.0118,.0225,.070), (.0020,.0068,.0265)), STEEL, .35, .9, .0003, 1)
    finish(pin(-.0170,-.0100,.0225,.022,.0072), DARK, .5, bevel=.0005)                  # forward assist boss
    finish(pin(-.0186,-.0150,.0225,.022,.0046), BLACK, .5, bevel=.0004)
    # --- rail: base plus real teeth, receiver to muzzle end of the handguard
    finish(box((0,.0392,.184), (.0098,.0024,.214)), BLACK, .55, 0, .0005)
    for i in range(44):
        finish(box((0,.0432,-.024+.00985*i), (.0106,.0019,.0028)), BLACK, .5, 0, .0005, 1)
    # --- handguard: short octagonal tube with cut slots
    hg = prof_front([(.0216*cos(math.pi/8+i*math.pi/4), .0200+.0216*sin(math.pi/8+i*math.pi/4)) for i in range(8)], .170, .398, 'hg')
    for i in range(5):
        cut(hg, box((-.022,.0200,.200+i*.041), (.0070,.0034,.0145)))
        cut(hg, box(( .022,.0200,.200+i*.041), (.0070,.0034,.0145)))
        cut(hg, box((0,-.0020,.200+i*.041), (.0034,.0070,.0145)))
    finish(hg, DARK, .55, bevel=.0010, seg=2)
    finish(lathe([(.160,.0220),(.172,.0235),(.178,.0235),(.181,.0205)], 0, .0200, 24), BLACK, .5, 0, .0006)   # barrel nut
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
    finish(prof([(-.058,.0318),(-.016,.0318),(-.016,.0378),(-.052,.0378),(-.058,.0360)], -.0118, .0118), BLACK, .5, 0, .0008)
    finish(prof([(-.060,.0326),(-.030,.0326),(-.036,.0376),(-.060,.0376)], -.0250, -.0100), DARK, .5, 0, .0010)
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
    finish(lathe([(-.132,.0150),(-.040,.0150)], 0, .0210, 26), GRAPH, .46, 0, .0005)
    nut = lathe([(-.052,.0232),(-.036,.0232),(-.032,.0200)], 0, .0210, 28)
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
    cut(st, box((0,.0210,-.150+OFS), (.0156,.0156,.09)))
    cut(st, prof([(-.222+OFS,.036),(-.212+OFS,.036),(-.212+OFS,-.030),(-.222+OFS,-.036)], -.03, .03))
    for i in range(3):
        cut(st, box((-.0185,-.0030,-.150-.019*i+OFS), (.0040,.0050,.0072)))
        cut(st, box(( .0185,-.0030,-.150-.019*i+OFS), (.0040,.0050,.0072)))
    finish(st, TAN, .62, 0, .0024, 3, group=2)
    finish(box((0,-.0020,-.2455+OFS), (.0178,.0500,.0065)), BLACK, .9, 0, .0024, 2, group=2)
    finish(box((0,-.0090,-.132+OFS), (.0080,.0050,.012)), DARK, .5, 0, .0008, group=2)
    # --- holographic sight: bulky hood, rear electronics, side controls
    hz = .142
    hous = prof_front([(-.0300,.0478),(.0300,.0478),(.0345,.0525),(.0345,.0890),(.0285,.0965),(-.0285,.0965),(-.0345,.0890),(-.0345,.0525)], hz-.014, hz+.046, 'holo')
    cut(hous, box((0,.0640,hz+.016), (.0250,.0170,.06)))
    finish(hous, DARK, .58, 0, .0024, 3)
    finish(box((0,.0498,hz-.032), (.0300,.0030,.0200)), DARK, .55, 0, .0016)                # rear electronics
    side = prof([(hz-.054,.0478),(hz-.012,.0478),(hz-.012,.0790),(hz-.020,.0830),(hz-.054,.0720)], -.0460, -.0318, 'side')
    finish(side, DARK, .55, 0, .0020, 3)
    finish(box((-.0472,.0620,hz-.032), (.0012,.0118,.0156)), BLACK, .6, 0, .0007)           # control face
    finish(box((-.0490,.0680,hz-.024), (.0008,.0030,.0052)), AMBER, .35, 0, .0002, 1)
    for k, (yy, zz) in enumerate(((.0586,hz-.024),(.0586,hz-.040),(.0680,hz-.040))):
        finish(pin(-.0494,-.0474,yy,zz,.0032,14), DARK if k else STEEL, .45, 0, .0003)
    finish(box((.0360,.0640,hz+.006), (.0024,.0100,.0200)), BLACK, .6, 0, .0010)            # right panel
    finish(prof([(hz-.060,.0460),(hz+.058,.0460),(hz+.058,.0486),(hz-.060,.0486)], -.0165, .0165), BLACK, .55, 0, .0008)   # mount
    finish(pin(-.0262,-.0150,.0462,hz+.020,.0058), STEEL, .42, .8, .0005)                   # cross bolt
    finish(pin(-.0262,-.0150,.0462,hz-.030,.0058), STEEL, .42, .8, .0005)
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

def hp(ax, ph, rad):
    # hand frame: x = grip axis, y = toward the wrist, z = completes the frame
    return (ax, cos(ph)*rad, sin(ph)*rad)

def hand(side, gr, style):
    clear()
    FAX = (-.0300, -.0100, .0100, .0295); FGR = (.0100, .0112, .0120, .0113)
    FSW = (2.71, 3.07, 3.20, 3.32); FKN = (-.22, -.08, .01, .06)
    lift = .070 if style == 1 else .066; axo = -.048 if style == 1 else -.046
    wrist = (axo, gr+lift, 0.0)
    KN = side*1.52
    pk = hp(0.0, KN, gr+.0113-.003)
    L = lambda a, b, t: tuple(a[i]+(b[i]-a[i])*t for i in range(3))
    parts = []
    parts.append(path_skin([(L(wrist,pk,-.10),.026,.0165),(L(wrist,pk,.22),.0335,.0205),(L(wrist,pk,.55),.0400,.0210),(L(wrist,pk,.86),.0470,.0170),(L(wrist,pk,1.0),.0440,.0115)],'palm'))
    pads = []
    for f in range(4):
        r = FGR[f]; rr = gr + r - .0034
        sw = FSW[f]*(.66 if style == 2 else 1.0)
        ph0 = KN + side*FKN[f]
        if style == 1 and f == 3:   # the trigger finger lies along the receiver
            k0 = L(wrist, pk, .94); k0 = (FAX[f]*.95, k0[1], k0[2])
            p1 = hp(FAX[f], ph0+side*.42, rr+.0030)
            p2 = (p1[0]+.0030, p1[1]-.0060*0, p1[2]+side*.0)
            pts = [(k0, r*.98, r*.9), (p1, r*1.08, r), (hp(FAX[f]+.006, ph0+side*.86, rr+.0100), r*.96, r*.9), (hp(FAX[f]+.010, ph0+side*1.25, rr+.0140), r*.86, r*.8)]
            parts.append(path_skin(pts, 'trig'))
            joints = [pts[1][0], pts[2][0], pts[3][0]]
        else:
            T = (0.0, .10, .46, .76, 1.0); RS = (1.10, 1.14, 1.02, .93, .78)
            pts = []
            for t, rs in zip(T, RS):
                rad = (gr+.0113-.003) if t == 0.0 else rr + .0016 - .0074*t*t
                pts.append((hp(FAX[f], ph0+side*sw*t, rad), r*rs, r*rs*.92))
            parts.append(path_skin(pts, 'finger'))
            joints = [pts[1][0], pts[2][0], pts[3][0]]
        pads.append(joints)
    # thumb
    if style == 2:
        tp = [hp(.0420,-side*1.10,gr+.0050), hp(.0560,-side*1.14,gr+.0030), hp(.0660,-side*1.18,gr+.0015)]
    else:
        tp = [hp(.0455,-side*.62*.68,gr+.0092), hp(.0430,-side*1.28*.68,gr+.0079), hp(.0395,-side*1.76*.68,gr+.0040)]
    t0 = L(wrist, pk, .42); t0 = (t0[0]+.015, t0[1], t0[2]); t1 = L(wrist, pk, .62); t1 = (t1[0]+.038, t1[1], t1[2])
    parts.append(path_skin([(t0,.0130,.0145),(t1,.0145,.0160),(tp[0],.0132,.0146),(tp[1],.0118,.0130),(tp[2],.0080,.0086)],'thumb'))
    # one continuous glove skin
    bpy.context.view_layer.objects.active = parts[0]
    for p in parts: p.select_set(True)
    for p in parts:
        bpy.context.view_layer.objects.active = p
        bpy.ops.object.modifier_apply(modifier='s')
    bpy.context.view_layer.objects.active = parts[0]
    bpy.ops.object.join(); g = bpy.context.view_layer.objects.active
    r = g.modifiers.new('r', 'REMESH'); r.mode = 'VOXEL'; r.voxel_size = .0016; r.use_smooth_shade = True
    bpy.ops.object.modifier_apply(modifier='r')
    s = g.modifiers.new('sm', 'SMOOTH'); s.iterations = 18; s.factor = .7
    bpy.ops.object.modifier_apply(modifier='sm')
    d = g.modifiers.new('d', 'DECIMATE'); d.ratio = .10
    finish(g, SLATE, .78, 0, 0, flag=2)
    # hard padding over the soft glove
    vecs = lambda a, b: tuple(b[i]-a[i] for i in range(3))
    for f, j in enumerate(pads):
        for k in range(2):
            a, b = j[k], j[k+1]; c = L(a, b, .5)
            l = math.dist(a, b)*.42
            o = path_skin([(L(a,b,.5-.34),.0072,.0056),(L(a,b,.5+.34),.0072,.0056)], 'pad')
            # lift the pad out through the back of the finger
            n = (0.0, c[1], c[2]); nl = math.hypot(n[1], n[2]) or 1.0
            o.location = (0.0, n[1]/nl*.0052, n[2]/nl*.0052)
            soft(o); finish(o, PAD, .52, 0, 0)
    # knuckle guard row, back-of-hand plate, wrist cuff
    for f in range(4):
        k = hp(FAX[f], KN+side*FKN[f]+side*.05, gr+.0300)
        o = path_skin([((k[0]-.0058,k[1],k[2]),.0060,.0062),((k[0]+.0058,k[1],k[2]),.0060,.0062)], 'kn')
        soft(o); finish(o, PAD, .46, 0, 0)
    m0 = L(wrist, pk, .52)
    o = path_skin([((m0[0]-.024,m0[1],m0[2]),.0080,.0075),((m0[0]+.024,m0[1],m0[2]),.0080,.0075)], 'back')
    o.location = (0.0, 0.0, side*.0185)
    soft(o); finish(o, PAD, .55, 0, 0)
    c0 = L(wrist, pk, -.18); c1 = L(wrist, pk, .06)
    o = path_skin([(c0,.0300,.0240),(L(c0,c1,.5),.0325,.0250),(c1,.0335,.0235)], 'cuff')
    soft(o); finish(o, DARK, .66, 0, 0)
    ct = L(wrist, pk, -.05)
    o = box((ct[0], ct[1], ct[2]+side*.0262), (.0190,.0085,.0022)); finish(o, PAD, .6, 0, .0010)

# ------------------------------------------------------------------ forearm armour
def bracer():
    clear()
    # frame: x = wrist to elbow, y = the arm's visible face, z = across the arm
    def tube(rings, col, rough, flag=0):
        seg = 32; bm = bmesh.new(); rv = []
        for x, ry, rz in rings:
            rv.append([bm.verts.new((x, ry*cos(2*math.pi*i/seg), rz*sin(2*math.pi*i/seg))) for i in range(seg)])
        for k in range(len(rv)-1):
            for i in range(seg):
                j = (i+1) % seg
                bm.faces.new((rv[k][i], rv[k][j], rv[k+1][j], rv[k+1][i]))
        bm.faces.new(rv[0][::-1]); bm.faces.new(rv[-1])
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        return finish(from_bm(bm, 'tube'), col, rough, 0, 0, flag=flag)
    tube([(.010,.0262,.0300),(.050,.0292,.0345),(.110,.0330,.0395),(.190,.0370,.0445),(.290,.0395,.0470),(.400,.0400,.0480)], SLEEVE, .85, flag=1)
    def plate(x0, x1, hw, y, col, rough=.5, th=.0030, b=.0012):
        # a fitted panel: follows the arm's curve across, flat along it
        n = 10; pts = []
        ry, rz = y, y*1.16
        for i in range(n+1):
            a = -hw + 2*hw*i/n; pts.append((rz*sin(a), ry*cos(a)))
        for i in range(n, -1, -1):
            a = -hw + 2*hw*i/n; pts.append(((rz+th)*sin(a), (ry+th)*cos(a)))
        bm = bmesh.new()
        A = [bm.verts.new((x0, yy, z)) for z, yy in pts]; B = [bm.verts.new((x1, yy, z)) for z, yy in pts]
        bm.faces.new(A); bm.faces.new(B[::-1])
        for i in range(len(pts)):
            j = (i+1) % len(pts); bm.faces.new((A[j], A[i], B[i], B[j]))
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        finish(from_bm(bm, 'plate'), col, rough, 0, b, 2)
    plate(.068,.128,.82,.0348,HARD)                     # screen housing
    plate(.136,.208,.86,.0376,HARD)                     # middle plate
    plate(.216,.300,.88,.0410,HARD)                     # rear plate
    plate(.145,.199,.56,.0404,SAND,.56,.0024,.0008)     # tan inset
    plate(.226,.290,.15,.0438,ORANGE,.5,.0018,.0005)    # accent stripe
    finish(box((.098,.0384,0), (.0178,.0020,.0170)), BLACK, .45, 0, .0006)
    finish(box((.098,.0404,0), (.0140,.0012,.0132)), AMBER, .30, 0, .0004)
    for x in (.132,.212):
        tube([(x-.003,.0362+(x-.132)*.045,.0420+(x-.132)*.05),(x+.003,.0362+(x-.132)*.045,.0420+(x-.132)*.05)], DARK, .8)

store = {}
rifle(); emit(store, 'RIFLE')
hand(1.0, .0130, 1); emit(store, 'HAND_R')
hand(-1.0, .0200, 2); emit(store, 'HAND_L')
bracer(); emit(store, 'BRACER')

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

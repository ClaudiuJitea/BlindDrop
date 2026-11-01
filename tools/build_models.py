# BlindDrop first-person models. Run: blender -b --factory-startup -P tools/build_models.py
# Sculpts the rifle, gloves and forearm armour and writes code/models.h, which is
# compiled into the game. Units are metres. Rifle axes match the game's weapon
# frame: x right, y up, z forward.
import bpy, bmesh, math, os, sys
from math import cos, sin, radians

OUT = os.path.join(os.path.dirname(os.path.abspath(sys.argv[sys.argv.index('-P') + 1])), '..', 'code', 'models.h')
TAN=(.56,.46,.33); TAN2=(.48,.40,.29); GRAPH=(.24,.245,.26); DARK=(.15,.155,.165); BLACK=(.105,.107,.112)
STEEL=(.43,.43,.44); ORANGE=(1.0,.5,.08); AMBER=(1.25,.72,.16); NAVY=(.16,.20,.30); PAD=(.20,.215,.25); SAND=(.52,.43,.31)

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
    for o in list(bpy.context.collection.objects):
        if 'col' not in o: continue
        e = o.evaluated_get(dg); me = e.to_mesh()
        me.set_sharp_from_angle(angle=radians(38))
        me.calc_loop_triangles()
        key = (tuple(round(x, 3) for x in o['col']), o['rough'], o['metal'], o['group'], o['flag'])
        out = tris.setdefault(key, [])
        cn = me.corner_normals
        for t in me.loop_triangles:
            for l in t.loops:
                v = me.vertices[me.loops[l].vertex_index].co; n = cn[l].vector
                out.append((v.x, v.y, v.z, n.x, n.y, n.z))
        e.to_mesh_clear()
    store[tag] = tris
    print('MODEL', tag, sum(len(v) for v in tris.values())//3, 'tris')

# ------------------------------------------------------------------ rifle
def rifle():
    clear()
    # lower receiver with magwell, tan
    low = prof([(-.018,.012),(.138,.012),(.138,-.006),(.134,-.030),(.082,-.036),(.078,-.012),(.044,-.016),(.012,-.016),(-.018,-.010)], -.0135, .0135, 'lower')
    cut(low, box((0,-.014,.108), (.0115,.030,.0235)))
    finish(low, TAN, .62, bevel=.0012, seg=3)
    # upper receiver, graphite, with a real ejection port and forward assist boss
    up = prof([(-.022,.012),(.178,.012),(.178,.034),(.170,.036),(-.012,.036),(-.022,.030)], -.0148, .0148, 'upper')
    cut(up, box((-.0160,.0225,.060), (.0036,.0065,.0240)))
    cut(up, box(( .0160,.0225,.060), (.0036,.0065,.0240)))
    finish(up, TAN2, .50, 0, bevel=.0014, seg=3)
    finish(box((0,.0215,.060), (.0128,.0058,.0225)), STEEL, .35, .9, .0003, 1)     # bolt carrier in the port
    finish(prof([(.028,.030),(.078,.030),(.084,.018),(.026,.018)], -.0172, -.0156), DARK, .5, bevel=.0005)   # dust cover door
    # top rail, one piece with real teeth
    finish(box((0,.0372,.158), (.0102,.0024,.175)), BLACK, .5, .3, .0005)
    for i in range(36):
        finish(box((0,.0414,-.008+.0097*i), (.0104,.0020,.0030)), BLACK, .45, .3, .0005, 1)
    # handguard: octagonal free-float tube with cut slots
    hg = lathe([(.176,.0205),(.336,.0205)], 0, .0200, 8, 'hg')
    hg.rotation_euler = (0,0,math.pi/8); bpy.context.view_layer.objects.active = hg; hg.select_set(True)
    bpy.ops.object.transform_apply(rotation=True); hg.select_set(False)
    hg.location = (0,0,0)
    for s in (-1, 1):
        for i in range(4):
            cut(hg, box((s*.0200,.0200,.206+i*.034), (.0060,.0029,.0120)))
    for i in range(4):
        cut(hg, box((0,.0010,.206+i*.034), (.0029,.0060,.0120)))
    finish(hg, GRAPH, .46, .4, bevel=.0009, seg=2)
    finish(lathe([(.170,.0235),(.180,.0235),(.182,.0215)], 0, .0200), DARK, .5, .3, .0006)     # barrel nut
    # barrel, gas block, muzzle brake
    finish(lathe([(.33,.0072),(.46,.0062)], 0, .0220), DARK, .38, .7, 0)
    mz = lathe([(.452,.0090),(.456,.0102),(.492,.0102),(.495,.0088)], 0, .0220, 24)
    for i in range(3):
        cut(mz, box((0,.0220,.463+i*.0095), (.0032,.016,.0024)))
    finish(mz, BLACK, .42, .55, .0004)
    # charging handle and latch
    finish(prof([(-.035,.0300),(-.012,.0300),(-.012,.0352),(-.035,.0352)], -.0125, .0125), BLACK, .5, .2, .0007)
    finish(box((-.0162,.0326,-.029), (.0052,.0026,.0060)), ORANGE, .5, 0, .0006)
    # forward assist, deflector, controls
    finish(lathe([(.0,.0058),(.012,.0058)], -.0168, .0300, 16), DARK, .4, .5, .0004)
    finish(box((.0172,.0210,.030), (.0030,.0046,.0065)), GRAPH, .45, .4, .0007)
    finish(box((-.0150,-.0035,.094), (.0032,.0046,.0044)), BLACK, .5, 0, .0007)             # mag release
    finish(prof([(.058,.010),(.082,.010),(.082,.002),(.058,.002)], -.0156, -.0130), BLACK, .5, 0, .0005)  # bolt catch
    finish(box((-.0145,.0020,.034), (.0020,.0052,.0120)), ORANGE, .55, 0, .0006)            # selector
    for z, y in ((.012,-.003),(.120,.000),(.150,.020)):
        finish(lathe([(0,.0030),(0.0001,.0030)], 0, 0, 12), STEEL, .4, .9, 0).location = (0,0,0)
        bpy.data.objects.remove(bpy.context.collection.objects[-1])
        finish(box((-.0141,y,z), (.0008,.0028,.0028)), STEEL, .4, .9, .0004, 1)
    # grip
    gr = prof([(.028,-.012),(.004,-.012),(-.030,-.092),(-.024,-.100),(.002,-.098),(.012,-.060)], -.0115, .0115, 'grip')
    finish(gr, BLACK, .78, 0, .0030, 4)
    # trigger guard and trigger
    finish(prof([(.012,-.016),(.018,-.016),(.018,-.044),(.062,-.044),(.074,-.016),(.078,-.016),(.066,-.050),(.012,-.050)], -.0040, .0040), TAN, .6, bevel=.0008)
    finish(prof([(.034,-.014),(.040,-.014),(.033,-.038),(.029,-.038)], -.0028, .0028), STEEL, .4, .8, .0004)
    # buffer tube and a shaped carbine stock
    finish(lathe([(-.150,.0136),(-.024,.0136),(-.020,.0165),(-.012,.0165)], 0, .0210, 26), DARK, .44, .4, .0005)
    st = prof([(-.120,.040),(-.205,.044),(-.214,.040),(-.218,-.034),(-.206,-.042),(-.170,-.026),(-.122,.001)], -.0150, .0150, 'stock')
    cut(st, prof([(-.138,.031),(-.150,.031),(-.150,.012),(-.138,.012)], -.03, .03))
    cut(st, prof([(-.160,.032),(-.200,.034),(-.200,-.018),(-.178,-.012),(-.160,.006)], -.03, .03))
    cut(st, box((0,.0210,-.150), (.0142,.0142,.05)))
    finish(st, TAN, .64, 0, .0022, 3, group=2)
    finish(box((0,.0040,-.2220), (.0155,.0440,.0060)), BLACK, .85, 0, .0020, 2, group=2)            # butt pad
    finish(box((0,.0030,-.128), (.0080,.0050,.010)), DARK, .5, 0, .0008, group=2)                  # latch
    # holographic sight: real housing with a cut window
    hz = .150
    hous = prof_front([(-.0285,.0468),(.0285,.0468),(.0310,.0520),(.0310,.0800),(.0255,.0880),(-.0255,.0880),(-.0310,.0800),(-.0310,.0520)], hz-.012, hz+.040, 'holo')
    cut(hous, box((0,.0622,hz+.014), (.0232,.0170,.05)))
    finish(hous, DARK, .52, .15, .0020, 3)
    rear = prof_front([(-.0250,.0468),(.0250,.0468),(.0250,.0605),(-.0250,.0605)], hz-.050, hz-.012)
    finish(rear, DARK, .5, .15, .0018, 3)
    finish(box((0,.0425,hz-.002), (.0145,.0036,.0420)), BLACK, .5, .2, .0010)              # mount
    finish(box((.0345,.0444,hz+.006), (.0042,.0044,.0140)), DARK, .45, .2, .0012)          # QD lever
    finish(box((.0310,.0444,hz+.006), (.0030,.0030,.0180)), STEEL, .4, .8, .0006)
    finish(box((-.0268,.0548,hz-.024), (.0024,.0056,.0100)), BLACK, .5, 0, .0008)          # control panel
    finish(box((-.0294,.0572,hz-.018), (.0006,.0018,.0024)), AMBER, .4, 0, .0002, 1)
    finish(box((-.0294,.0530,hz-.018), (.0006,.0018,.0024)), STEEL, .4, .6, .0002, 1)
    finish(box((-.0294,.0572,hz-.030), (.0006,.0018,.0024)), DARK, .4, 0, .0002, 1)
    finish(box((.0270,.0548,hz-.024), (.0060,.0072,.0110)), BLACK, .5, 0, .0016, 2)        # battery cap
    # the magazine moves, so it is its own group
    mag = prof([(.132,-.024),(.084,-.024),(.056,-.150),(.056,-.162),(.104,-.162),(.106,-.150)], -.0098, .0098, 'mag')
    finish(mag, BLACK, .68, 0, .0016, 2, group=1)
    for i in range(5):
        finish(box((0,-.070-.016*i,.102-.0052*i), (.0108,.0024,.0200)), DARK, .7, 0, .0008, 1, group=1)

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
    finish(g, NAVY, .74, 0, 0, flag=2)
    # hard padding over the soft glove
    vecs = lambda a, b: tuple(b[i]-a[i] for i in range(3))
    for f, j in enumerate(pads):
        for k in range(2):
            a, b = j[k], j[k+1]; c = L(a, b, .5)
            l = math.dist(a, b)*.42
            o = path_skin([(L(a,b,.5-.34),.0072,.0056),(L(a,b,.5+.34),.0072,.0056)], 'pad')
            # lift the pad out through the back of the finger
            bpy.context.view_layer.objects.active = o
            n = (0.0, c[1], c[2]); nl = math.hypot(n[1], n[2]) or 1.0
            o.location = (0.0, n[1]/nl*.0058, n[2]/nl*.0058)
            bpy.ops.object.modifier_apply(modifier='s')
            finish(o, PAD, .52, 0, 0)
    # knuckle guard row, back-of-hand plate, wrist cuff
    for f in range(4):
        k = hp(FAX[f], KN+side*FKN[f]+side*.05, gr+.0300)
        o = path_skin([((k[0]-.0058,k[1],k[2]),.0060,.0062),((k[0]+.0058,k[1],k[2]),.0060,.0062)], 'kn')
        bpy.context.view_layer.objects.active = o; bpy.ops.object.modifier_apply(modifier='s')
        finish(o, PAD, .46, 0, 0)
    m0 = L(wrist, pk, .52)
    o = path_skin([((m0[0]-.024,m0[1],m0[2]),.0080,.0075),((m0[0]+.024,m0[1],m0[2]),.0080,.0075)], 'back')
    bpy.context.view_layer.objects.active = o
    n = (0.0, 0.0, side*.0185); o.location = n
    bpy.ops.object.modifier_apply(modifier='s'); finish(o, SAND, .55, 0, 0)
    c0 = L(wrist, pk, -.18); c1 = L(wrist, pk, .06)
    o = path_skin([(c0,.0335,.0255),(c1,.0345,.0240)], 'cuff')
    bpy.context.view_layer.objects.active = o; bpy.ops.object.modifier_apply(modifier='s')
    finish(o, DARK, .62, 0, 0)
    ct = L(wrist, pk, -.05)
    o = box((ct[0], ct[1], ct[2]+side*.0250), (.0200,.0060,.0026)); finish(o, ORANGE, .5, 0, .0006)

# ------------------------------------------------------------------ forearm armour
def bracer():
    clear()
    # frame: x = along the forearm, y = out of the visible face, z = across
    def shell(x0, x1, w, r, th, col, rough=.42, metal=0.0):
        pts = []
        n = 14
        for i in range(n+1):
            a = -w + 2*w*i/n
            pts.append((r*sin(a), r*cos(a)))
        for i in range(n, -1, -1):
            a = -w + 2*w*i/n
            pts.append(((r+th)*sin(a), (r+th)*cos(a)))
        bm = bmesh.new()
        A = [bm.verts.new((x0, y, z)) for z, y in pts]; B = [bm.verts.new((x1, y, z)) for z, y in pts]
        bm.faces.new(A); bm.faces.new(B[::-1])
        for i in range(len(pts)):
            j = (i+1) % len(pts)
            bm.faces.new((A[j], A[i], B[i], B[j]))
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        finish(from_bm(bm, 'shell'), col, rough, metal, .0012, 2)
    shell(.074, .118, .92, .0375, .0042, DARK)          # wrist computer bay
    shell(.128, .194, .78, .0420, .0048, SAND, .5)      # tan plate
    shell(.204, .262, .72, .0450, .0046, NAVY, .44)     # rear plate
    shell(.212, .254, .10, .0496, .0020, ORANGE, .5)    # accent bar
    finish(box((.096, .0436, 0), (.0150, .0014, .0130)), AMBER, .3, 0, .0005)
    finish(box((.096, .0426, 0), (.0178, .0014, .0158)), BLACK, .4, 0, .0005)
    for x in (.124, .199):                              # straps
        shell(x-.0034, x+.0034, 2.55, .0372+(x-.124)*.095, .0020, BLACK, .8)

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
        f.write('static const int16_t %s_V[][6] = {\n' % tag)
        line = []
        for x, y, z, a, b, c in rows:
            q = lambda v: max(-32767, min(32767, int(round(v*32000))))
            n = lambda v: max(-32767, min(32767, int(round(v*32767))))
            line.append('{%d,%d,%d,%d,%d,%d}' % (q(x), q(y), q(z), n(a), n(b), n(c)))
            if len(line) == 8:
                f.write(','.join(line) + ',\n'); line = []
        if line: f.write(','.join(line) + ',\n')
        f.write('};\nstatic const model_part_t %s_P[] = {\n' % tag)
        for p in parts:
            f.write('  {%d,%d,%.3ff,%.3ff,%.3ff,%.3ff,%.3ff,%d,%d},\n' % p)
        f.write('};\n#define %s_NP %d\n' % (tag, len(parts)))
print('WROTE', OUT)

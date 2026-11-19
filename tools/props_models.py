# BlindDrop world props: vehicles and street furniture for the town maps.
# Loaded by tools/build_models.py (exec'd into its namespace, so it shares
# clear/box/prof/lathe/finish/cut/emit...). Units are metres. PROP FRAME:
# x forward (the vehicle's nose), y up, z lateral; the origin is on the ground
# at the footprint's centre, which is where prop_add puts it.
#
# Finish flags the renderer reads (code/render/props.c, pmesh_draw):
#   20 PAINT   the prop's own colour replaces the authored one
#   21 GLASS   vehicle glass (SMAT_VGLASS: tinted, a dark cabin behind)
#   23 PAINT2  the prop's second colour
#   anything else: the authored colour, roughness and metal as they are.
F_PAINT, F_GLASS, F_PAINT2 = 20, 21, 23

P_RUBBER = (.055, .055, .06); TRIM = (.035, .035, .04); CHROME = (.72, .73, .75)
ALLOY = (.62, .63, .65); PLATE = (.90, .90, .86); LENS_W = (.86, .87, .84)
LENS_R = (.55, .03, .02); LENS_A = (.95, .52, .08); PLASTIC = (.11, .11, .12)

def _bvh_of(objs):
    dg = bpy.context.evaluated_depsgraph_get()
    vs = []; ps = []
    for o in objs:
        e = o.evaluated_get(dg); me = e.to_mesh()
        base = len(vs)
        vs.extend(o.matrix_world @ v.co for v in me.vertices)
        ps.extend([i + base for i in p.vertices] for p in me.polygons)
        e.to_mesh_clear()
    return BVHTree.FromPolygons(vs, ps)

def _hit(bvh, org, d):
    r = bvh.ray_cast(Vector(org), Vector(d).normalized(), 10.0)
    return (r[0], r[1]) if r[0] is not None else (None, None)

def patch(bvh, fn, nu, nv, lift, name='patch'):
    """A surface patch hugging the body: fn(u, v) -> (ray origin, ray dir) on a
    grid of (nu+1) x (nv+1); every grid point is the ray's hit, stood `lift`
    off along the surface normal. Points that miss are dropped with their quads."""
    bm = bmesh.new(); g = {}
    for i in range(nu + 1):
        for j in range(nv + 1):
            o, d = fn(i / nu, j / nv)
            p, n = _hit(bvh, o, d)
            if p is not None:
                if n.dot(Vector(d)) > 0: n = -n
                g[i, j] = bm.verts.new(p + n * lift)
    for i in range(nu):
        for j in range(nv):
            q = [g.get((i, j)), g.get((i + 1, j)), g.get((i + 1, j + 1)), g.get((i, j + 1))]
            if all(q): bm.faces.new(q)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    o = from_bm(bm, name)
    # outward = against the ray: flip if the recalculated normals point inward
    if len(o.data.polygons):
        p0 = o.data.polygons[0]
        oo, dd = fn(.5, .5)
        if p0.normal.dot(Vector(dd)) > 0:
            bm = bmesh.new(); bm.from_mesh(o.data)
            bmesh.ops.reverse_faces(bm, faces=bm.faces); bm.to_mesh(o.data); bm.free()
    return o

def ribbon(bvh, pts_fn, n, w, lift, name='rib'):
    """A thin strip along a surface polyline (a shut line, a seal): pts_fn(t) ->
    (ray origin, ray dir); w is the strip's width across the line."""
    P = []; N = []
    for i in range(n + 1):
        o, d = pts_fn(i / n)
        p, nn = _hit(bvh, o, d)
        if p is None: continue
        if nn.dot(Vector(d)) > 0: nn = -nn
        P.append(p + nn * lift); N.append(nn)
    bm = bmesh.new(); prev = None
    for i in range(len(P)):
        t = (P[min(i + 1, len(P) - 1)] - P[max(i - 1, 0)]).normalized()
        s = t.cross(N[i]).normalized() * (w * .5)
        a = bm.verts.new(P[i] + s); b = bm.verts.new(P[i] - s)
        if prev: bm.faces.new((prev[0], prev[1], b, a))
        prev = (a, b)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    o = from_bm(bm, name)
    o.modifiers.new('solid', 'SOLIDIFY').thickness = .002
    return o

def oriented(o, at, nrm, up=(0, 1, 0)):
    """Turn an object authored facing +x so +x lies along `nrm`, at `at`."""
    nrm = Vector(nrm).normalized()
    up = Vector(up); side = nrm.cross(up)
    if side.length < 1e-4: side = nrm.orthogonal()
    side.normalize(); up = side.cross(nrm).normalized()
    from mathutils import Matrix
    m = Matrix((nrm, up, side)).transposed().to_4x4()   # columns: x->nrm, y->up, z->side
    o.matrix_world = Matrix.Translation(Vector(at)) @ m
    bpy.context.view_layer.objects.active = o; o.select_set(True)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True); o.select_set(False)
    return o

def rbox(hx, hy, hz, r, name='rbox', seg=1):
    """A box with rounded edges (bevel applied), centred on the origin."""
    o = box((0, 0, 0), (hx, hy, hz), name)
    m = o.modifiers.new('b', 'BEVEL'); m.width = r; m.segments = seg
    bpy.context.view_layer.objects.active = o; o.select_set(True)
    bpy.ops.object.modifier_apply(modifier=m.name); o.select_set(False)
    return o

def cyl_z(x, y, z0, z1, r, seg=24, name='cylz'):
    """A capped cylinder along z (an axle, a wheel), centred (x, y)."""
    bm = bmesh.new()
    a = [bm.verts.new((x + r*cos(2*math.pi*i/seg), y + r*sin(2*math.pi*i/seg), z0)) for i in range(seg)]
    b = [bm.verts.new((x + r*cos(2*math.pi*i/seg), y + r*sin(2*math.pi*i/seg), z1)) for i in range(seg)]
    bm.faces.new(a[::-1]); bm.faces.new(b)
    for i in range(seg):
        j = (i + 1) % seg
        bm.faces.new((a[i], a[j], b[j], b[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return from_bm(bm, name)

def lathe_z(rings, x, y, sgn, seg=24, name='lz', cap=True):
    """Rings (dz, r) swept round an axis along z through (x, y); dz is measured
    OUTWARD (sgn = the side the face looks at)."""
    bm = bmesh.new(); rv = []
    for dz, r in rings:
        rv.append([bm.verts.new((x + r*cos(2*math.pi*i/seg), y + r*sin(2*math.pi*i/seg), sgn*dz)) for i in range(seg)])
    for k in range(len(rv) - 1):
        for i in range(seg):
            j = (i + 1) % seg
            bm.faces.new((rv[k][i], rv[k][j], rv[k+1][j], rv[k+1][i]))
    if cap:
        bm.faces.new(rv[0][::-1]); bm.faces.new(rv[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return from_bm(bm, name)

def road_wheel(x, y, zc, sgn, R=.32, W=.20, spokes=5, rimc=ALLOY, name='wheel', tread=14, seg=24):
    """Tyre with a rounded shoulder and tread blocks, an alloy (or steel) rim with
    spokes, a dark well behind it and a centre cap. sgn = which side faces out."""
    zi = zc - sgn * W * .5
    rings = [(0, R*.70), (.004, R*.88), (.018, R*.97), (.04, R), (W - .04, R), (W - .018, R*.97),
             (W - .004, R*.88), (W, R*.70)]
    t = lathe_z([(zi*sgn + dz, r) for dz, r in rings], x, y, sgn, seg, name + '_tyre')
    finish(t, P_RUBBER, .92, 0, 0, flag=0)
    for k in range(tread):                                # tread grooves
        a = 2*math.pi*k/tread
        g = box((0, 0, 0), (.004, .012, W*.42))
        g.location = (x + (R+.006)*cos(a), y + (R+.006)*sin(a), zc); g.rotation_euler = (0, 0, a)
        bpy.context.view_layer.objects.active = g; g.select_set(True)
        bpy.ops.object.transform_apply(location=True, rotation=True); g.select_set(False)
        cut(t, g)
    zo = zc + sgn * W * .5                                # outer face plane
    well = lathe_z([(zo*sgn - .05, R*.69), (zo*sgn - .004, R*.69)], x, y, sgn, 24, name + '_well')
    finish(well, (.08, .08, .085), .7, .2, 0)
    rim = lathe_z([(zo*sgn - .006, R*.69), (zo*sgn + .002, R*.66), (zo*sgn + .004, R*.60),
                   (zo*sgn - .010, R*.56)], x, y, sgn, 28, name + '_rim', cap=False)
    finish(rim, rimc, .30, .85, 0)
    for k in range(spokes):                               # spokes from hub to rim
        a = 2*math.pi*k/spokes + .3
        s = box((0, 0, 0), (R*.23, R*.055, .012))
        s.location = (x + R*.33*cos(a), y + R*.33*sin(a), zo - sgn*.008); s.rotation_euler = (0, 0, a)
        bpy.context.view_layer.objects.active = s; s.select_set(True)
        bpy.ops.object.transform_apply(location=True, rotation=True); s.select_set(False)
        finish(s, rimc, .30, .85, .004, 1)
    hub = lathe_z([(zo*sgn - .02, R*.17), (zo*sgn + .002, R*.16), (zo*sgn + .008, R*.10)], x, y, sgn, 16, name + '_hub')
    finish(hub, rimc, .28, .9, 0)
    finish(lathe_z([(zo*sgn + .007, R*.075), (zo*sgn + .011, R*.06)], x, y, sgn, 12), TRIM, .4, 0, 0)
    for k in range(spokes):                               # lug nuts
        a = 2*math.pi*k/spokes
        finish(lathe_z([(zo*sgn - .002, .011), (zo*sgn + .010, .009)], x + R*.12*cos(a), y + R*.12*sin(a), sgn, 6),
               (.30, .30, .32), .4, .7, 0)
    # the brake disc behind the spokes
    finish(lathe_z([(zo*sgn - .07, R*.52), (zo*sgn - .055, R*.52)], x, y, sgn, 20), (.33, .32, .31), .5, .6, 0)

# ---------------------------------------------------------------------------
# THE BODY: a loft of sections along x. Each section is a closed ring (z, y)
# from the floor's centre round the sill, the side (with its tumblehome), the
# shoulder and either the deck (bonnet / boot) or the glasshouse, blended by how
# much cabin stands over that station. One subdivision pass makes it one skin.
# ---------------------------------------------------------------------------
def body_loft(xs, sec, name='body', subd=1):
    rings = [sec(x) for x in xs]
    bm = bmesh.new(); R = []
    for x, ring in zip(xs, rings):
        half = [bm.verts.new((x, y, z)) for z, y in ring]
        other = [bm.verts.new((x, y, -z)) for z, y in ring[1:-1][::-1]]
        R.append(half + other)
    n = len(R[0])
    for k in range(len(R) - 1):
        for i in range(n):
            j = (i + 1) % n
            bm.faces.new((R[k][i], R[k][j], R[k+1][j], R[k+1][i]))
    bm.faces.new(R[0]); bm.faces.new(R[-1][::-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    o = from_bm(bm, name)
    if subd:
        m = o.modifiers.new('s', 'SUBSURF'); m.levels = subd; m.render_levels = subd
        bpy.context.view_layer.objects.active = o; o.select_set(True)
        bpy.ops.object.modifier_apply(modifier=m.name); o.select_set(False)
    return o

def car_section(x, P):
    """One ring of the body at station x for the parameter set P."""
    ax = abs(x)
    L = P['L']
    # plan: a superellipse, so the corners round off and the ends close
    plan = max(1e-3, 1 - (ax / L) ** P['plan_e']) ** (1 / P['plan_e'])
    wb = P['W'] * plan
    belt = P['belt'](x)
    deck = P['deck'](x)
    top = max(P['top'](x), deck)
    y0 = P['sill'](x)
    c = max(0.0, min(1.0, (top - deck) / max(P['roof'] - deck, 1e-3)))
    wl = wb * .93
    pts = [(0, y0), (wl * .80, y0), (wl, y0 + .07), (wb, y0 + .30 * (belt - y0)),
           (wb, belt - .05), (wb * .975, belt)]
    # deck shape (c = 0) and glasshouse shape (c = 1), then blend
    d6, d7, d8, d9 = (wb * .92, deck - .005), (wb * .66, deck + .012), (wb * .33, deck + .02), (0, deck + .024)
    wr = P['roofw'] * plan
    g6 = (wb * .935, belt + .035)
    g7 = (wr + (wb * .935 - wr) * .45, belt + .55 * (top - belt))
    g8 = (wr, top - .035)
    g9 = (0, top)
    lerp = lambda a, b: (a[0] + (b[0] - a[0]) * c, a[1] + (b[1] - a[1]) * c)
    pts += [lerp(d6, g6), lerp(d7, g7), lerp(d8, g8), lerp(d9, g9)]
    # never let a deck point dip under the shoulder
    out = []
    for z, y in pts:
        out.append((z, max(y, y0)))
    return out

def stations(L, n, ends=6):
    """Stations along the length, denser at the rounded ends."""
    xs = []
    for i in range(n + 1):
        t = -1 + 2 * i / n
        xs.append(L * .985 * math.copysign(abs(t) ** .9, t))
    return xs

def piecewise(pts):
    """y(x) by linear interpolation through (x, y) knots (x descending ok)."""
    pts = sorted(pts)
    def f(x):
        if x <= pts[0][0]: return pts[0][1]
        for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
            if x <= x1:
                t = (x - x0) / (x1 - x0); t = t * t * (3 - 2 * t)
                return y0 + (y1 - y0) * t
        return pts[-1][1]
    return f

def arches(o, xs_w, R, y, W):
    """Wheel arches with a WHEELHOUSE: each side's cut stops 0.30 m in, so the
    arch has an inner wall instead of a tunnel through the car."""
    for xw in xs_w:
        cut(o, cyl_z(xw, y, W - .32, 2, R, 36))
        cut(o, cyl_z(xw, y, -2, -(W - .32), R, 36))

def classify_split(o, rules, name):
    """Split a mesh by per-face rules: rules(center, normal) -> key; returns
    {key: object}. Keys are finish tuples (col, rough, metal, flag)."""
    bm = bmesh.new(); bm.from_mesh(o.data)
    groups = {}
    for f in bm.faces:
        k = rules(f.calc_center_median(), f.normal)
        groups.setdefault(k, []).append(f.index)
    objs = {}
    for k, idx in groups.items():
        b2 = bm.copy()
        keep = set(idx)
        bmesh.ops.delete(b2, geom=[f for f in b2.faces if f.index not in keep], context='FACES')
        ob = from_bm(b2, name)
        col, rough, metal, flag = k
        finish(ob, col, rough, metal, 0, flag=flag)
        objs[k] = ob
    bm.free()
    bpy.data.objects.remove(o)
    return objs

def place_o(o, loc=(0, 0, 0), rot=(0, 0, 0)):
    o.location = loc; o.rotation_euler = rot
    bpy.context.view_layer.objects.active = o; o.select_set(True)
    bpy.ops.object.transform_apply(location=True, rotation=True); o.select_set(False)
    return o

def tube(a, b, r, seg=10, name='tube'):
    """A round bar from point a to point b."""
    a = Vector(a); b = Vector(b); d = b - a
    o = cyl_z(0, 0, 0, d.length, r, seg, name)
    q = Vector((0, 0, 1)).rotation_difference(d.normalized())
    o.rotation_mode = 'QUATERNION'; o.rotation_quaternion = q; o.location = a
    bpy.context.view_layer.objects.active = o; o.select_set(True)
    bpy.ops.object.transform_apply(location=True, rotation=True); o.select_set(False)
    return o

# ---------------------------------------------------------------------------
# THE CARS: a compact early-90s four-door saloon and a single-cab pickup, both
# built hard-surface. Each body is the INTERSECTION of three prisms (the side
# profile, the plan with rounded corners and a bowed nose, the front view with
# its tumblehome), bevelled, so every panel is flat or gently curved and every
# edge holds a clean highlight. The glasshouse is its own prism on top; glass
# panes are exact planar polygons on its faces, in black frames. Lamp bands are
# recessed into nose and tail and the lenses follow the plan; bumpers are
# separate shells standing proud; wheelhouses are walled and lined.
# ---------------------------------------------------------------------------
PBLK = (.025, .025, .028); PGLS = (.12, .17,.21); PPLA = (.075, .075, .08); PPNT = (.5, .5, .5)

def _plan_pts(pl, g=0.0, n=10):
    """A plan outline (x, z), grown by g. pl: Lf, Lr (nose and tail x), W (half
    width), rf, rr (corner radii), bowf, bowr (how far the ends bow back)."""
    Lf, Lr, W = pl['Lf'] + g, pl['Lr'] + g, pl['W'] + g
    rf, rr = max(.02, pl['rf'] + g), max(.02, pl['rr'] + g)
    bowf, bowr = pl['bowf'], pl['bowr']
    bx = lambda x0, z, bow: x0 - bow * (z / W) ** 2
    pts = []
    zc = W - rf
    for i in range(n + 1):
        z = -zc + 2 * zc * i / n
        pts.append((bx(Lf, z, bowf), z))
    xs = bx(Lf, zc, bowf) - rf
    for i in range(1, n + 1):
        a = (math.pi / 2) * i / n
        pts.append((xs + rf * math.cos(a), zc + rf * math.sin(a)))
    zr = W - rr
    xr = -bx(Lr, zr, bowr) + rr
    for i in range(0, n + 1):
        a = math.pi / 2 + (math.pi / 2) * i / n
        pts.append((xr + rr * math.cos(a), zr + rr * math.sin(a)))
    for i in range(1, n):
        z = zr - 2 * zr * i / n
        pts.append((-bx(Lr, z, bowr), z))
    for i in range(0, n + 1):
        a = math.pi + (math.pi / 2) * i / n
        pts.append((xr + rr * math.cos(a), -zr + rr * math.sin(a)))
    for i in range(0, n):
        a = 1.5 * math.pi + (math.pi / 2) * i / n
        pts.append((xs + rf * math.cos(a), -zc + rf * math.sin(a)))
    return pts

def _plan_prism(pl, g=0.0, y0=-.5, y1=2.5):
    pts = _plan_pts(pl, g)
    bm = bmesh.new()
    a = [bm.verts.new((x, y0, z)) for x, z in pts]; b = [bm.verts.new((x, y1, z)) for x, z in pts]
    bm.faces.new(a); bm.faces.new(b[::-1])
    for i in range(len(pts)):
        j = (i + 1) % len(pts)
        bm.faces.new((a[i], a[j], b[j], b[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return from_bm(bm, 'plan')

def _isect(o, c):
    m = o.modifiers.new('is', 'BOOLEAN'); m.operation = 'INTERSECT'; m.object = c; m.solver = 'EXACT'
    bpy.context.view_layer.objects.active = o; o.select_set(True)
    bpy.ops.object.modifier_apply(modifier=m.name); o.select_set(False)
    bpy.data.objects.remove(c)
    return o

def _plan_x(pl, z, front=True, g=0.0):
    """Where the plan outline (grown by g) crosses lateral position z."""
    pts = _plan_pts(pl, g, n=60)
    cand = [(x, zz) for x, zz in pts if (x > 0) == front]
    x, _ = min(cand, key=lambda p: abs(p[1] - z) - (1e-4 * p[0] if front else -1e-4 * p[0]))
    return x

def _plan_skin(pl, g, y0, y1, z0, z1, front, xlim, name='skin'):
    """A 3 cm shell following the plan grown by g, between heights y0..y1 and
    lateral z0..z1, on the nose (front) or the tail, beyond |x| > xlim."""
    o = _plan_prism(pl, g, y0, y1)
    xa, xb = (xlim, 3.5) if front else (-3.5, -xlim)
    c = box(((xa + xb) / 2, (y0 + y1) / 2, (z0 + z1) / 2), ((xb - xa) / 2, (y1 - y0) / 2 + .01, (z1 - z0) / 2))
    _isect(o, c)
    cut(o, _plan_prism(pl, g - .03, y0 - 1, y1 + 1))
    return o

def _recess(body, pl, y0, y1, z0, z1, front, xlim, d=.022):
    xa, xb = (xlim, 3.5) if front else (-3.5, -xlim)
    c = box(((xa + xb) / 2, (y0 + y1) / 2, (z0 + z1) / 2), ((xb - xa) / 2, (y1 - y0) / 2, (z1 - z0) / 2))
    cut(c, _plan_prism(pl, -d))
    cut(body, c)

def _car_body(pl, side, front, bev=.028):
    o = prof_front(side, -2, 2, 'car_side')
    _isect(o, _plan_prism(pl))
    _isect(o, prof(front, -3, 3, 'car_front'))
    return _bev(o, bev, 3)

def _glasshouse(side, front, bev=.03):
    g = prof_front(side, -1.5, 1.5, 'gh_side')
    _isect(g, prof(front, -3, 3, 'gh_front'))
    return _bev(g, bev, 3)

def _pane(pts, nrm, lift, col, rough, metal, flag=0):
    """A flat polygon (3D points) stood `lift` off along nrm, 4 mm thick."""
    bm = bmesh.new(); n = Vector(nrm).normalized()
    vs = [bm.verts.new(Vector(p) + n * lift) for p in pts]
    f = bm.faces.new(vs)
    bm.normal_update()
    if f.normal.dot(n) < 0: f.normal_flip()
    o = from_bm(bm, 'pane')
    o.modifiers.new('sol', 'SOLIDIFY').thickness = .004
    return finish(o, col, rough, metal, 0, flag=flag)

def _side_pts(s, zf, x0f, x1f, y0, y1, grow=0.0, n=6):
    """A side-window outline on the plane z = s*zf(y): front edge x0f(y), rear x1f(y)."""
    P = []
    for k in range(n + 1):
        y = y0 - grow + (y1 - y0 + 2 * grow) * k / n
        P.append((x0f(y) + grow, y, s * zf(y)))
    for k in range(n + 1):
        y = y1 + grow - (y1 - y0 + 2 * grow) * k / n
        P.append((x1f(y) - grow, y, s * zf(y)))
    return P

def _screen_pts(xf, zf, y0, y1, inset, grow=0.0, n=8):
    """A windscreen / rear-screen outline on the plane x = xf(y), between the
    side planes less `inset`."""
    P = []
    for k in range(n + 1):
        y = y0 - grow + (y1 - y0 + 2 * grow) * k / n
        P.append((xf(y), y, zf(y) - inset + grow))
    for k in range(n + 1):
        y = y1 + grow - (y1 - y0 + 2 * grow) * k / n
        P.append((xf(y), y, -(zf(y) - inset + grow)))
    return P

def _screen_n(p0, p1, sx):
    """Outward normal of a screen whose line in side view runs p0 -> p1 (x, y)."""
    n = Vector((p1[1] - p0[1], -(p1[0] - p0[0]), 0)).normalized()
    if n.x * sx < 0: n = -n
    return n

def _glazed(gh, side_glass, screens, frame=.022):
    """Panes and their black frames. side_glass: [(zf, front_f, rear_f, y0, y1,
    [(pane front_f, pane rear_f), ...])]; screens: [(xf, zf, y0, y1, inset, nrm)]."""
    for zf, ff, rf_, y0, y1, panes in side_glass:
        for s in (-1, 1):
            nn = Vector((0, -(zf(1.4) - zf(1.2)) / .2, s)).normalized()
            _pane(_side_pts(s, zf, ff, rf_, y0, y1, frame), nn, .003, PBLK, .5, 0)
            for pf, pr in panes:
                _pane(_side_pts(s, zf, pf, pr, y0, y1), nn, .006, PGLS, .03, .1, F_GLASS)
    for xf, zf, y0, y1, inset, nrm in screens:
        _pane(_screen_pts(xf, zf, y0, y1, inset, frame), nrm, .003, PBLK, .5, 0)
        _pane(_screen_pts(xf, zf, y0, y1, inset), nrm, .006, PGLS, .03, .1, F_GLASS)

def _disc(rings, at, nrm, seg=24):
    """Rings (dz, r) swept round +z, then turned so +z looks along nrm, at `at`."""
    o = lathe_z(rings, 0, 0, 1, seg, 'disc')
    q = Vector((0, 0, 1)).rotation_difference(Vector(nrm).normalized())
    o.rotation_mode = 'QUATERNION'; o.rotation_quaternion = q; o.location = Vector(at)
    bpy.context.view_layer.objects.active = o; o.select_set(True)
    bpy.ops.object.transform_apply(location=True, rotation=True); o.select_set(False)
    return o

def _poly_line(pts):
    """t in 0..1 -> point along a polyline (by length)."""
    P = [Vector(p) for p in pts]
    L = [0.0]
    for a, b in zip(P, P[1:]): L.append(L[-1] + (b - a).length)
    def f(t):
        s = t * L[-1]
        for k in range(len(P) - 1):
            if s <= L[k + 1] or k == len(P) - 2:
                u = (s - L[k]) / max(L[k + 1] - L[k], 1e-6)
                return P[k] + (P[k + 1] - P[k]) * u
    return f

def _side_line(bvh, s, pts, w=.0055, col=(.03, .03, .03)):
    """A shut line (or strip) on the flank along side-view points (x, y)."""
    f = _poly_line(pts)
    finish(ribbon(bvh, lambda t: ((f(t).x, f(t).y, s * 2.0), (0, 0, -s)), max(6, len(pts) * 8), w, .0012),
           col, .7, 0, 0)

def _top_line(bvh, pts, w=.0055, col=(.03, .03, .03)):
    """A shut line seen from above along plan points (x, z)."""
    f = _poly_line(pts)
    finish(ribbon(bvh, lambda t: ((f(t).x, 3.0, f(t).y), (0, -1, 0)), 24, w, .0012), col, .7, 0, 0)

def _wheelhouses(body, WX, WY, W, RA, depth=.30):
    for xw in WX:
        for s in (-1, 1):
            cut(body, cyl_z(xw, WY, s * (W - depth), s * 2.0, RA, 48))

def _mirror(s, xm, ym, zm, scale=1.0):
    """A black sail at the A-pillar foot, a stalk, a painted shell and its glass."""
    k = scale
    finish(place_o(rbox(.07 * k, .045 * k, .012, .008), (xm, ym, zm + s * .01)), PBLK, .5, 0, 0)
    finish(place_o(rbox(.035 * k, .015 * k, .05 * k, .008), (xm - .01, ym + .005, zm + s * .06 * k)), PBLK, .5, 0, 0)
    sh = place_o(_bev(box((0, 0, 0), (.05 * k, .055 * k, .095 * k)), .022 * k, 3),
                 (xm - .015, ym + .03 * k, zm + s * .15 * k), (0, math.radians(-s * 8), 0))
    finish(sh, PPNT, .26, .1, 0, flag=F_PAINT)
    mg = place_o(box((0, 0, 0), (.003, .045 * k, .082 * k)), (xm - .015 - .052 * k, ym + .03 * k, zm + s * .152 * k),
                 (0, math.radians(-s * 8), 0))
    finish(mg, (.55, .6, .65), .05, .9, 0)

def _handle(bvh, s, x, y):
    p, nn = _hit(bvh, (x, y, s * 2), (0, 0, -s))
    if p is not None:
        finish(oriented(rbox(.006, .014, .07, .006), p + nn * .003, nn), (.05, .05, .055), .4, .3, 0)
        finish(oriented(rbox(.003, .006, .05, .003), p + nn * .008 + Vector((0, -.004, 0)), nn), CHROME, .2, .9, 0)

def _wipers(xf, y, zs, length=.30):
    for zz in zs:
        x = xf(y) + .03
        finish(place_o(box((0, 0, 0), (.008, .006, length)), (x, y, zz), (0, math.radians(8), 0)), PBLK, .55, 0, 0)
        finish(place_o(box((0, 0, 0), (.02, .012, .02)), (x + .02, y - .004, zz + length - .02)), PBLK, .55, 0, 0)

# ---------------------------------------------------------------- the saloon
SD_PL = dict(Lf=2.12, Lr=2.13, W=.905, rf=.24, rr=.20, bowf=.05, bowr=.035)
SD_WX, SD_WY, SD_TR, SD_RA = (1.27, -1.26), .325, .325, .385
SD_SIDE = [(-2.06, .27), (2.06, .27), (2.11, .30), (2.12, .34), (2.12, .745), (2.105, .772), (2.05, .787),
           (1.40, .842), (0.92, .866), (0.86, .885), (0.80, .925), (-1.38, .94), (-1.44, .945), (-1.98, .937),
           (-2.08, .922), (-2.12, .895), (-2.13, .86), (-2.13, .34), (-2.11, .30)]
SD_FRONT = [(0, .20), (.84, .20), (.895, .30), (.905, .40), (.905, .64), (.895, .80), (.87, .96), (.80, 1.05),
            (-.80, 1.05), (-.87, .96), (-.895, .80), (-.905, .64), (-.905, .40), (-.895, .30), (-.84, .20)]
SD_WS0, SD_WS1 = (.93, .86), (.20, 1.385)       # windscreen base and top (x, y)
SD_RS0, SD_RS1 = (-1.40, .92), (-.74, 1.385)    # rear screen base and top
def sd_ws_x(y): return SD_WS0[0] + (y - SD_WS0[1]) * (SD_WS1[0] - SD_WS0[0]) / (SD_WS1[1] - SD_WS0[1])
def sd_rs_x(y): return SD_RS0[0] + (y - SD_RS0[1]) * (SD_RS1[0] - SD_RS0[0]) / (SD_RS1[1] - SD_RS0[1])
def sd_gz(y):   return .855 - .155 * max(0.0, (y - .93)) / .43   # side glass plane: half width at y

def sedan():
    """A compact four-door notchback saloon in the 4.2 m footprint the game's
    collision gives it (code/world/prefabs.c, car())."""
    clear()
    pl = SD_PL; W, WX, WY, TR, RA = pl['W'], SD_WX, SD_WY, SD_TR, SD_RA
    body = _car_body(pl, SD_SIDE, SD_FRONT)
    BF = (.585, .735, .865)        # front lamp band: y0, y1, half width
    BR = (.755, .862, .875)        # tail lamp band
    PR = (.585, .705, .265)        # rear plate recess
    _recess(body, pl, BF[0], BF[1], -BF[2], BF[2], True, 1.90)
    _recess(body, pl, BR[0], BR[1], -BR[2], BR[2], False, 1.93)
    _recess(body, pl, PR[0], PR[1], -PR[2], PR[2], False, 1.93, .012)
    _wheelhouses(body, WX, WY, W, RA)
    def rules(c, n):
        x, y, z = c.x, c.y, abs(c.z)
        for xw in WX:
            if (x - xw) ** 2 + (y - WY) ** 2 < (RA + .006) ** 2 and z < W - .003:
                return (PBLK, .95, 0.0, 0)
        for (y0, y1, zz), front in ((BF, True), (BR, False), (PR, False)):
            if (x > 1.9 if front else x < -1.9) and y0 - .004 < y < y1 + .004 and z < zz + .004:
                return (PBLK, .55, 0.0, 0)
        if y < .345: return (PPLA, .8, 0.0, 0)
        return (PPNT, .26, .1, F_PAINT)
    parts = classify_split(body, rules, 'sd_skin')
    gh = _glasshouse([SD_WS0, SD_WS1, (.08, 1.418), (-.62, 1.418), SD_RS1, SD_RS0, (-1.40, .80), (.93, .80)][::-1],
                     [(-.86, .80), (.86, .80), (.855, .93), (.70, 1.36), (.62, 1.405), (0, 1.425),
                      (-.62, 1.405), (-.70, 1.36), (-.855, .93)])
    finish(gh, PPNT, .26, .1, 0, flag=F_PAINT)
    bvh = _bvh_of(list(parts.values()) + [gh])
    # GLASS: two door windows a side (the B-pillar blacked out), both screens
    _glazed(gh, [(sd_gz, lambda y: sd_ws_x(y) - .085, lambda y: sd_rs_x(y) + .085, .955, 1.335,
                  [(lambda y: sd_ws_x(y) - .105, lambda y: -.215), (lambda y: -.315, lambda y: sd_rs_x(y) + .105)])],
            [(sd_ws_x, sd_gz, .905, 1.345, .075, _screen_n(SD_WS0, SD_WS1, 1)),
             (sd_rs_x, sd_gz, .975, 1.345, .085, _screen_n(SD_RS0, SD_RS1, -1))])
    for s in (-1, 1):                                       # roof drip rails
        finish(tube((sd_ws_x(1.37) - .02, 1.372, s * .706), (sd_rs_x(1.37) + .02, 1.372, s * .706), .009, 8), PBLK, .5, 0, 0)
    _wipers(sd_ws_x, .915, (-.38, .12))
    # FLANKS: door shut lines, handles, rubbing strip, belt finisher, mirrors
    for s in (-1, 1):
        _side_line(bvh, s, [(.86, .93), (.865, .60), (.84, .40), (.80, .345)])
        _side_line(bvh, s, [(-.26, .935), (-.25, .345)])
        _side_line(bvh, s, [(-1.02, .94), (-1.0, .66), (-.92, .52), (-.86, .40), (-.84, .345)])
        _side_line(bvh, s, [(.80, .345), (-.84, .345)])
        for xh in (.40, -.70): _handle(bvh, s, xh, .83)
        _side_line(bvh, s, [(.82, .60), (-.82, .60)], .038, (.05, .05, .055))
        finish(tube((sd_ws_x(.945) - .04, .945, s * (sd_gz(.945) + .004)), (sd_rs_x(.945) + .05, .945, s * (sd_gz(.945) + .004)),
                    .007, 8), CHROME, .2, .9, 0)
        _mirror(s, .74, .975, s * sd_gz(.975))
        p, nn = _hit(bvh, (1.62, .70, s * 2), (0, 0, -s))
        if p is not None: finish(oriented(rbox(.004, .012, .028, .005), p + nn * .002, nn), LENS_A, .15, 0, 0)
    p, nn = _hit(bvh, (-1.75, .80, 2.0), (0, 0, -1))           # fuel filler flap
    if p is not None:
        finish(_disc([(0, .055), (.004, .052)], p - nn * .001, nn, 24), PPNT, .26, .1, 0, flag=F_PAINT)
        _side_line(bvh, 1, [(-1.75 + .056 * math.cos(k * math.pi / 8), .80 + .056 * math.sin(k * math.pi / 8)) for k in range(17)], .004)
    _top_line(bvh, [(2.045, -.80), (2.045, .80)])                  # bonnet and boot
    for s in (-1, 1):
        _top_line(bvh, [(2.045, s * .80), (1.0, s * .815), (.86, s * .825)])
        _top_line(bvh, [(-1.45, s * .80), (-2.06, s * .80)])
    _top_line(bvh, [(-1.45, -.80), (-1.45, .80)])
    # BUMPERS: painted shells 2.6 cm proud, a black rubbing strip, a black lip
    for front in (True, False):
        xa, xb = (1.45, 3.5) if front else (-3.5, -1.45)
        xw = WX[0] if front else WX[1]
        o = _plan_prism(pl, .026, .25, .545 if front else .56)
        _isect(o, box(((xa + xb) / 2, .5, 0), ((xb - xa) / 2, .5, 2)))
        for s in (-1, 1): cut(o, cyl_z(xw, WY, s * (W - .45), s * 2.0, RA + .012, 48))
        finish(_bev(o, .02, 3), PPNT, .26, .1, 0, flag=F_PAINT)
        st = _plan_prism(pl, .038, .405, .450)
        _isect(st, box(((xa + xb) / 2, .42, 0), ((xb - xa) / 2, .1, 2)))
        for s in (-1, 1): cut(st, cyl_z(xw, WY, s * (W - .45), s * 2.0, RA + .02, 48))
        finish(_bev(st, .008, 2), (.05, .05, .055), .55, 0, 0)
        lp = _plan_prism(pl, .010, .225, .27)
        _isect(lp, box(((xa + xb) / 2, .25, 0), ((xb - xa) / 2, .1, .78)))
        finish(_bev(lp, .01, 2), PPLA, .8, 0, 0)
    # NOSE: twin round headlamps on chrome reflectors, corner indicators, grille
    y0, y1 = BF[0] + .012, BF[1] - .012
    for s in (-1, 1):
        finish(_plan_skin(pl, -.014, y0, y1, *sorted((s * .43, s * .80)), True, 1.9), (.70, .71, .72), .18, .9, 0)
        for zc in (.52, .70):
            zz = s * zc; x = _plan_x(pl, zz, True, -.014); nl = (1, 0, .25 * s * (zc / .8))
            finish(_disc([(0, .056), (.004, .054), (.006, .046)], (x - .004, (y0 + y1) / 2, zz), nl, 28), (.80, .81, .82), .15, .95, 0)
            finish(_disc([(0, .042), (.005, .040), (.008, .030)], (x - .002, (y0 + y1) / 2, zz), nl, 24), (.97, .97, .94), .05, .1, 0)
        finish(_plan_skin(pl, -.012, y0, y1, *sorted((s * .815, s * .862)), True, 1.9), LENS_A, .15, 0, 0)
    for k in range(4):
        yy = y0 + .012 + k * (y1 - y0 - .024) / 3
        finish(_plan_skin(pl, -.010, yy - .006, yy + .006, -.40, .40, True, 1.9), (.32, .33, .35), .35, .8, 0)
    for zz in (-.405, .405):
        finish(_plan_skin(pl, -.008, y0 - .004, y1 + .004, zz - .008, zz + .008, True, 1.9), CHROME, .2, .9, 0)
    finish(_disc([(0, .03), (.008, .026)], (_plan_x(pl, 0, True, -.006), (y0 + y1) / 2, 0), (1, 0, 0), 20), CHROME, .2, .95, 0)
    x = _plan_x(pl, 0, True, .026)
    finish(box((x - .004, .335, 0), (.006, .052, .24)), PLATE, .5, 0, 0)
    finish(box((x - .002, .335, -.215), (.006, .045, .02)), (.15, .25, .55), .5, 0, 0)
    for s in (-1, 1):
        x = _plan_x(pl, s * .62, True, .026)
        finish(_disc([(0, .035), (.006, .032)], (x - .002, .33, s * .62), (1, 0, .2 * s), 16), (.85, .85, .80), .1, .2, 0)
    # TAIL: wrap-round clusters (red, white reverse, amber), garnish, plate, pipe
    y0, y1 = BR[0] + .010, BR[1] - .010
    for s in (-1, 1):
        finish(_plan_skin(pl, -.010, y0, y1, *sorted((s * .40, s * .875)), False, 1.93), LENS_R, .12, 0, 0)
        finish(_plan_skin(pl, -.006, y0 + .004, (y0 + y1) / 2 - .004, *sorted((s * .40, s * .54)), False, 1.93), LENS_W, .1, 0, 0)
        finish(_plan_skin(pl, -.006, (y0 + y1) / 2 + .004, y1 - .004, *sorted((s * .76, s * .87)), False, 1.93), LENS_A, .12, 0, 0)
        for zz in (.56, .75):
            finish(_plan_skin(pl, -.004, y0, y1, *sorted((s * (zz - .006), s * (zz + .006))), False, 1.93), PBLK, .5, 0, 0)
    finish(_plan_skin(pl, -.008, y0 + .01, y1 - .01, -.38, .38, False, 1.93), (.06, .06, .065), .3, .4, 0)
    x = _plan_x(pl, 0, False, -.012)
    finish(box((x - .003, (PR[0] + PR[1]) / 2, 0), (.004, .052, .24)), PLATE, .5, 0, 0)
    finish(box((x - .005, (PR[0] + PR[1]) / 2, -.215), (.004, .045, .02)), (.15, .25, .55), .5, 0, 0)
    finish(_rx(-2.19, -1.95, .235, .52, .026, 14), CHROME, .3, .9, 0)
    finish(box((0, .265, 0), (1.95, .012, .78)), (.03, .03, .032), .9, 0, 0)          # floor pan
    for xw in WX:
        for s in (-1, 1):
            road_wheel(xw, WY, s * (W - .14), s, R=TR, W=.21, spokes=5, name='w', tread=14, seg=28)

# ---------------------------------------------------------------- the pickup
PK_PL = dict(Lf=2.12, Lr=2.125, W=.91, rf=.20, rr=.07, bowf=.04, bowr=.0)
PK_WX, PK_WY, PK_TR, PK_RA = (1.32, -1.30), .365, .365, .43
PK_SIDE = [(-2.10, .43), (2.06, .43), (2.11, .46), (2.12, .50), (2.12, .925), (2.105, .955), (2.05, .972),
           (1.40, 1.00), (0.98, 1.015), (0.92, 1.03), (0.86, 1.05), (-2.105, 1.05), (-2.125, 1.03),
           (-2.125, .47), (-2.115, .44)]
PK_FRONT = [(0, .30), (.86, .30), (.905, .40), (.91, .50), (.91, 1.0), (.895, 1.08), (.80, 1.20),
            (-.80, 1.20), (-.895, 1.08), (-.91, 1.0), (-.91, .50), (-.905, .40), (-.86, .30)]
PK_WS0, PK_WS1 = (.97, 1.00), (.45, 1.605)
def pk_ws_x(y): return PK_WS0[0] + (y - PK_WS0[1]) * (PK_WS1[0] - PK_WS0[0]) / (PK_WS1[1] - PK_WS0[1])
def pk_rw_x(y): return -.462 + (y - 1.0) * (.025 / .60)          # the cab's back wall
def pk_gz(y):   return .875 - .135 * max(0.0, (y - 1.06)) / .52
BED_X0, BED_X1, BED_FL, BED_Z = -2.07, -.535, .70, .835

def pickup():
    """A single-cab pickup, the workhorse of every desert town: a short cab, a
    long open bed, a ladder-frame ride height, chrome bumper, bigger tyres."""
    clear()
    pl = PK_PL; W, WX, WY, TR, RA = pl['W'], PK_WX, PK_WY, PK_TR, PK_RA
    body = _car_body(pl, PK_SIDE, PK_FRONT, .026)
    cut(body, box(((BED_X0 + BED_X1) / 2, BED_FL + .5, 0), ((BED_X1 - BED_X0) / 2, .5, BED_Z)))
    BF = (.68, .90, .865)
    _recess(body, pl, BF[0], BF[1], -BF[2], BF[2], True, 1.90)
    _wheelhouses(body, WX, WY, W, RA)
    def rules(c, n):
        x, y, z = c.x, c.y, abs(c.z)
        for xw in WX:
            if (x - xw) ** 2 + (y - WY) ** 2 < (RA + .006) ** 2 and z < W - .003:
                return (PBLK, .95, 0.0, 0)
        if x > 1.9 and BF[0] - .004 < y < BF[1] + .004 and z < BF[2] + .004: return (PBLK, .55, 0.0, 0)
        if BED_X0 - .004 < x < BED_X1 + .004 and y > BED_FL - .004 and z < BED_Z + .004 and n.y < .9 or \
           BED_X0 < x < BED_X1 and abs(y - BED_FL) < .004 and z < BED_Z:
            return ((.10, .10, .105), .85, 0.0, 0)
        if y < .50: return (PPLA, .8, 0.0, 0)
        return (PPNT, .26, .1, F_PAINT)
    parts = classify_split(body, rules, 'pk_skin')
    gh = _glasshouse([PK_WS0, PK_WS1, (.33, 1.64), (-.36, 1.64), (pk_rw_x(1.60), 1.60), (pk_rw_x(.95), .95), (.97, .95)][::-1],
                     [(-.88, .95), (.88, .95), (.875, 1.06), (.74, 1.58), (.66, 1.63), (0, 1.645),
                      (-.66, 1.63), (-.74, 1.58), (-.875, 1.06)])
    finish(gh, PPNT, .26, .1, 0, flag=F_PAINT)
    bvh = _bvh_of(list(parts.values()) + [gh])
    _glazed(gh, [(pk_gz, lambda y: pk_ws_x(y) - .085, lambda y: pk_rw_x(y) + .10, 1.085, 1.57,
                  [(lambda y: pk_ws_x(y) - .105, lambda y: -.10), (lambda y: -.15, lambda y: pk_rw_x(y) + .12)])],
            [(pk_ws_x, pk_gz, 1.045, 1.58, .075, _screen_n(PK_WS0, PK_WS1, 1)),
             (pk_rw_x, pk_gz, 1.13, 1.555, .21, Vector((-1, .04, 0)).normalized())])
    for s in (-1, 1):
        finish(tube((pk_ws_x(1.59) - .02, 1.592, s * .745), (pk_rw_x(1.59) + .03, 1.592, s * .745), .009, 8), PBLK, .5, 0, 0)
    _wipers(pk_ws_x, 1.06, (-.40, .10), .32)
    # FLANKS: one long door, the cab/bed gap, handle, strip, mirror, flares
    for s in (-1, 1):
        _side_line(bvh, s, [(.90, 1.05), (.905, .70), (.88, .52), (.85, .50)])
        _side_line(bvh, s, [(-.38, 1.05), (-.38, .50)])
        _side_line(bvh, s, [(.85, .50), (-.38, .50)])
        _side_line(bvh, s, [(-.495, 1.05), (-.495, .44)], .012, (.015, .015, .016))  # the gap between cab and bed
        _handle(bvh, s, -.22, .95)
        _side_line(bvh, s, [(.84, .72), (-.36, .72)], .04, (.05, .05, .055))
        _mirror(s, .80, 1.12, s * pk_gz(1.12), 1.25)
        for xw in WX:                                   # black wheel-arch flares
            fz0, fz1 = sorted((s * (W - .05), s * (W + .028)))
            fl2 = cyl_z(xw, WY, fz0, fz1, RA + .06, 48)
            cut(fl2, cyl_z(xw, WY, -3, 3, RA - .004, 48))
            cut(fl2, box((xw, 0, 0), (1, .5, 3)))
            finish(_bev(fl2, .012, 2), PPLA, .75, 0, 0)
        p, nn = _hit(bvh, (1.70, .86, s * 2), (0, 0, -s))
        if p is not None: finish(oriented(rbox(.004, .014, .03, .005), p + nn * .002, nn), LENS_A, .15, 0, 0)
        # mud flaps behind the rear wheels
        finish(box((WX[1] - .47, .29, s * (W - .14)), (.006, .16, .13)), PBLK, .8, 0, 0)
    _top_line(bvh, [(2.045, -.80), (2.045, .80)])
    for s in (-1, 1): _top_line(bvh, [(2.045, s * .80), (1.0, s * .82), (.93, s * .825)])
    # BED: rail caps, the wheel tubs, floor ribs, the tailgate
    for s in (-1, 1):
        finish(_bev(box(((BED_X0 + BED_X1) / 2 - .02, 1.058, s * (BED_Z + .036)), ((BED_X1 - BED_X0) / 2 + .02, .009, .046)), .006, 2),
               PPLA, .7, 0, 0)
        tub = cyl_z(WX[1], WY, *sorted((s * (BED_Z - .26), s * BED_Z)), RA + .035, 40)
        cut(tub, cyl_z(WX[1], WY, *sorted((s * (BED_Z - .23), s * (BED_Z + .1))), RA + .005, 40))
        cut(tub, box((WX[1], BED_FL - .5, 0), (1, .5, 3)))
        finish(_bev(tub, .01, 2), (.10, .10, .105), .85, 0, 0)
    for k in range(7):
        zz = -.72 + k * .24
        finish(box(((BED_X0 + BED_X1) / 2, BED_FL + .008, zz), ((BED_X1 - BED_X0) / 2 - .01, .008, .025)), (.12, .12, .125), .8, .1, 0)
    p, nn = _hit(bvh, (-3, .92, 0), (1, 0, 0))
    for s in (-1, 1):
        finish(ribbon(bvh, lambda t, s=s: ((-3, .48 + t * .56, s * .76), (1, 0, 0)), 8, .006, .0012), (.03, .03, .03), .7, 0, 0)
    finish(ribbon(bvh, lambda t: ((-3, .48, -.76 + 1.52 * t), (1, 0, 0)), 10, .006, .0012), (.03, .03, .03), .7, 0, 0)
    if p is not None:
        finish(oriented(rbox(.012, .03, .11, .01), p + nn * .006, nn), PPLA, .5, 0, 0)
    for yy in (.62, .80):                                  # pressed ribs across the tailgate
        finish(ribbon(bvh, lambda t, yy=yy: ((-3, yy, -.68 + 1.36 * t), (1, 0, 0)), 10, .022, .004), PPNT, .26, .1, 0, flag=F_PAINT)
    # TAIL LAMPS on the corner posts, wrapping round
    for s in (-1, 1):
        finish(_plan_skin(pl, .004, .64, .98, *sorted((s * .775, s * .92)), False, 2.035), LENS_R, .12, 0, 0)
        finish(_plan_skin(pl, .008, .66, .74, *sorted((s * .78, s * .86)), False, 2.035), LENS_W, .1, 0, 0)
        finish(_plan_skin(pl, .008, .88, .96, *sorted((s * .78, s * .86)), False, 2.035), LENS_A, .12, 0, 0)
    # REAR STEP BUMPER with a plate in it, a tow hitch, the exhaust
    rb = _bev(box((-2.17, .43, 0), (.06, .07, .88)), .02, 3)
    finish(rb, (.62, .63, .64), .3, .85, 0)
    finish(box((-2.18, .502, 0), (.05, .004, .80)), PBLK, .8, 0, 0)
    finish(box((-2.234, .43, 0), (.004, .05, .22)), PLATE, .5, 0, 0)
    finish(box((-2.236, .43, -.20), (.004, .044, .018)), (.15, .25, .55), .5, 0, 0)
    finish(_rx(-2.30, -2.22, .33, 0, .025, 12), (.25, .25, .27), .5, .7, 0)
    finish(_rx(-2.15, -1.95, .30, .55, .028, 14), (.35, .34, .33), .5, .6, 0)
    # NOSE: round headlamps in chrome bezels, a chrome grille, a chrome bumper
    y0, y1 = BF[0] + .012, BF[1] - .012; ym = (y0 + y1) / 2
    for s in (-1, 1):
        finish(_plan_skin(pl, -.010, y0, y1, *sorted((s * .47, s * .80)), True, 1.9), CHROME, .2, .9, 0)
        finish(_plan_skin(pl, -.007, y0 + .012, y1 - .012, *sorted((s * .485, s * .785)), True, 1.9), PBLK, .5, 0, 0)
        zz = s * .635; x = _plan_x(pl, zz, True, -.010)
        nl = (1, 0, .25 * s)
        finish(_disc([(0, .088), (.006, .086), (.010, .074)], (x - .002, ym, zz), nl, 32), (.80, .81, .82), .15, .95, 0)
        finish(_disc([(0, .072), (.006, .070), (.014, .055)], (x, ym, zz), nl, 32), (.95, .95, .92), .05, .1, 0)
        finish(_plan_skin(pl, -.012, y0, y1, *sorted((s * .815, s * .862)), True, 1.9), LENS_A, .15, 0, 0)
    finish(_plan_skin(pl, -.004, y0 - .006, y1 + .006, -.455, .455, True, 1.9), CHROME, .2, .9, 0)
    finish(_plan_skin(pl, -.001, y0 + .012, y1 - .012, -.435, .435, True, 1.9), PBLK, .6, 0, 0)
    for k in range(3):
        yy = y0 + .04 + k * (y1 - y0 - .08) / 2
        finish(_plan_skin(pl, .004, yy - .014, yy + .014, -.43, .43, True, 1.9), PPNT, .26, .1, 0, flag=F_PAINT)
    finish(_disc([(0, .035), (.01, .03)], (_plan_x(pl, 0, True, .004), ym, 0), (1, 0, 0), 20), CHROME, .2, .95, 0)
    fb = _plan_prism(pl, .045, .46, .625)
    _isect(fb, box((2.6, .5, 0), (.75, .5, 2)))
    for s in (-1, 1): cut(fb, cyl_z(WX[0], WY, s * (W - .45), s * 2.0, RA + .065, 48))
    finish(_bev(fb, .02, 3), (.66, .67, .68), .25, .9, 0)
    va = _plan_prism(pl, .02, .38, .46)
    _isect(va, box((2.6, .42, 0), (.75, .2, .80)))
    finish(_bev(va, .012, 2), PPLA, .8, 0, 0)
    x = _plan_x(pl, 0, True, .05)
    finish(box((x + .002, .54, 0), (.004, .052, .24)), PLATE, .5, 0, 0)
    finish(box((x + .004, .54, -.215), (.004, .045, .02)), (.15, .25, .55), .5, 0, 0)
    finish(box((0, .43, 0), (1.95, .012, .70)), (.03, .03, .032), .9, 0, 0)        # floor pan
    for s in (-1, 1):                                      # the ladder frame showing under the sills
        finish(box((0, .36, s * .45), (1.85, .06, .04)), (.06, .06, .065), .7, .3, 0)
    # LOAD: a spare wheel, a jerry can, a tarp bundle
    finish(cyl_z(-1.62, BED_FL + .12, -.45, -.22, .34, 28), P_RUBBER, .92, 0, 0)
    finish(cyl_z(-1.62, BED_FL + .12, -.22, -.21, .22, 20), ALLOY, .35, .8, 0)
    finish(place_o(rbox(.18, .24, .08, .02), (-.80, BED_FL + .24, .58)), (.22, .30, .18), .7, 0, 0)
    finish(place_o(rbox(.20, .14, .55, .10), (-.82, BED_FL + .14, -.15)), (.42, .40, .30), .9, 0, 0)
    for xw in WX:
        for s in (-1, 1):
            road_wheel(xw, WY, s * (W - .15), s, R=TR, W=.24, spokes=6, name='w', tread=16, seg=28)

# ---------------------------------------------------------------- the dumpster
def dumpster():
    """A 1100 L wheeled refuse bin: tapered steel body (paint), ribbed, two
    black lids (one propped open), side trunnions, four castors."""
    clear()
    HX, HZ = .62, .52
    body = prof([(-HX, .18), (HX, .18), (HX + .08, 1.10), (-HX - .08, 1.10)], -HZ, HZ, 'bin')
    cut(body, prof([(-HX + .04, .24), (HX - .04, .24), (HX + .04, 1.2), (-HX - .04, 1.2)], -HZ + .04, HZ - .04))
    finish(body, (.5, .5, .5), .55, .1, .006, 2, flag=F_PAINT)
    for xr in (-.42, -.14, .14, .42):                     # pressed ribs on the long sides
        for sg in (-1, 1):
            finish(box((xr, .62, sg * (HZ + .008)), (.03, .38, .01)), (.5, .5, .5), .55, .1, .004, flag=F_PAINT)
    finish(box((0, 1.10, 0), (HX + .11, .035, HZ + .04)), (.5, .5, .5), .55, .1, .006, flag=F_PAINT)   # top rim
    finish(place_o(box((0, 0, 0), (HX * .5 + .02, .025, HZ + .02)), (-HX * .5 - .02, 1.16, 0)), PLASTIC, .7, 0, .01)
    lid = box((0, 0, 0), (HX * .5 + .02, .025, HZ + .02))
    finish(place_o(lid, (HX * .5 + .02 - .18, 1.33, 0), (0, 0, .78)), PLASTIC, .7, 0, .01)
    for sg in (-1, 1):
        finish(cyl_z(0, .78, sg * HZ, sg * (HZ + .09), .045, 14), (.25, .25, .27), .5, .6, 0)    # trunnions
    finish(tube((-HX - .12, .95, -.38), (-HX - .12, .95, .38), .018), (.25, .25, .27), .5, .6, 0)   # push bar
    for x in (-HX + .12, HX - .12):
        for z in (-HZ + .12, HZ - .12):
            finish(box((x, .15, z), (.05, .03, .05)), (.25, .25, .27), .5, .6, 0)
            finish(cyl_z(x, .085, z - .025, z + .025, .085, 14), P_RUBBER, .9, 0, 0)
    # rubbish overflowing: two bags
    for (x, y, z, r) in ((.30, 1.17, .18, .20), (-.05, 1.12, -.25, .17)):
        b = bpy.data.objects.new('bag', bpy.data.meshes.new('bag'))
        bm = bmesh.new(); bmesh.ops.create_icosphere(bm, subdivisions=2, radius=r)
        for v in bm.verts: v.co.y *= .75; v.co.x *= 1.15
        bm.to_mesh(b.data); bm.free(); bpy.context.collection.objects.link(b)
        place_o(b, (x, y, z), (0, .4, .2))
        finish(b, (.05, .05, .06), .35, 0, 0)

# ---------------------------------------------------------------- the motorbike
# A 125 cc commuter single (the CG125 family every town in the region runs on),
# rebuilt part by part: wire-spoked wheels on drum hubs, a single-downtube
# cradle frame, a raked fork with chrome stanchions and alloy sliders, a
# teardrop tank, a stitched bench seat with a grab rail, a finned air-cooled
# single leaning forward out of its cases, the header and a heat-shielded
# silencer down the right, chain and sprockets down the left, twin rear shocks
# on a box swingarm, a round headlamp and twin clocks, mirrors on stalks, four
# indicators, the plate and the tail lamp. Stood on its side stand.
MB_R = .300                                  # wheel radius (18 in rim + tyre)
MB_WF, MB_WR = .650, -.640                   # front / rear axle x
MB_STEEL = (.10, .10, .11); MB_DARK = (.16, .16, .17); MB_ALU = (.58, .58, .60)

def _bent(pts, r, col, rough=.4, metal=.5, seg=8, name='bent'):
    """A round bar through a polyline, every joint filled with a ball so the
    bends are swept rather than mitred."""
    if r < .008: seg = min(seg, 5)                        # levers, stays, stalks
    for a, b in zip(pts, pts[1:]):
        finish(tube(a, b, r, seg, name), col, rough, metal, 0)
    for i in range(1, len(pts) - 1) if r >= .008 else ():   # a lever's bend needs no ball
        u_ = (Vector(pts[i]) - Vector(pts[i - 1])).normalized(); w_ = (Vector(pts[i + 1]) - Vector(pts[i])).normalized()
        if u_.dot(w_) > .93: continue                     # ...nor a shallow one: the tubes' ends meet
        p = pts[i]
        bm = bmesh.new(); bmesh.ops.create_uvsphere(bm, u_segments=min(seg, 6), v_segments=4, radius=r)
        o = from_bm(bm, name + '_j'); place_o(o, p)
        finish(o, col, rough, metal, 0)

def _ball(p, r, col, rough=.4, metal=.5, seg=12, sc=(1, 1, 1)):
    bm = bmesh.new(); bmesh.ops.create_uvsphere(bm, u_segments=seg, v_segments=max(6, seg // 2), radius=r)
    for v in bm.verts: v.co.x *= sc[0]; v.co.y *= sc[1]; v.co.z *= sc[2]
    o = from_bm(bm, 'ball'); place_o(o, p)
    return finish(o, col, rough, metal, 0)

def _mb_wheel(xw, front):
    """Tyre, chrome rim, 36 laced wire spokes, a drum-brake hub."""
    y = MB_R
    tyre = [(-.046, .232), (-.052, .248), (-.050, .266), (-.040, .284), (-.022, .296), (0, .300),
            (.022, .296), (.040, .284), (.050, .266), (.052, .248), (.046, .232)]
    finish(lathe_z(tyre, xw, y, 1, 20, 'mtyre', cap=False), P_RUBBER, .9, 0, 0)
    for k in range(16):                                   # block tread on the crown
        a = 2 * math.pi * k / 16
        for sz in (0,):
            g = box((0, 0, 0), (.0035, .011, .015))
            place_o(g, (xw + .2990 * cos(a), y + .2990 * sin(a), .014 if k % 2 else -.014),
                    (0, 0, a))
            finish(g, (.035, .035, .04), .95, 0, 0)
    rim = [(-.030, .238), (-.032, .228), (-.024, .222), (-.010, .219), (.010, .219), (.024, .222),
           (.032, .228), (.030, .238)]
    finish(lathe_z(rim, xw, y, 1, 20, 'mrim', cap=False), CHROME, .18, .95, 0)
    finish(lathe_z([(-.033, .230), (-.033, .240), (.033, .240), (.033, .230)], xw, y, 1, 20, 'mrimw', cap=False),
           (.35, .35, .37), .3, .8, 0)
    # the hub: a drum on the brake side, a flange each side for the spokes
    hub = [(-.060, .030), (-.058, .070 if front else .085), (-.040, .072 if front else .088),
           (-.034, .050), (.034, .050), (.040, .058), (.050, .040), (.058, .024)]
    finish(lathe_z(hub, xw, y, 1, 16, 'mhub'), MB_ALU, .35, .7, 0)
    for sz in (-1, 1):
        finish(lathe_z([(sz * .034 - .003, .062), (sz * .034 + .003, .062)], xw, y, 1, 16, 'flange'), MB_ALU, .35, .7, 0)
    finish(lathe_z([(-.075, .016), (.075, .016)], xw, y, 1, 12, 'axle'), (.40, .40, .42), .3, .9, 0)
    # 24 spokes, laced tangentially: 12 per flange, alternate leading/trailing
    for k in range(24):
        sz = -1 if k % 2 else 1
        a0 = 2 * math.pi * k / 24
        a1 = a0 + (.55 if (k // 2) % 2 else -.55)
        h = Vector((xw + .058 * cos(a0), y + .058 * sin(a0), sz * .034))
        r_ = Vector((xw + .220 * cos(a1), y + .220 * sin(a1), sz * .010))
        finish(tube(h, r_, .0026, 4, 'spoke'), (.78, .78, .80), .25, .9, 0)

def motorbike():
    """A 125 cc commuter motorcycle on its side stand."""
    clear()
    R = MB_R; WF, WR = MB_WF, MB_WR
    _mb_wheel(WF, True); _mb_wheel(WR, False)
    paint = (.5, .5, .5)
    # ---- the frame: steering head, backbone, single downtube splitting into a
    #      cradle under the engine, seat rails and the subframe struts
    HEAD_T, HEAD_B = Vector((.385, 1.000, 0)), Vector((.430, .860, 0))
    finish(tube(HEAD_B - (HEAD_T - HEAD_B) * .1, HEAD_T + (HEAD_T - HEAD_B) * .1, .030, 16, 'head'), MB_STEEL, .45, .4, 0)
    _bent([(.40, .95, 0), (.18, .92, 0), (-.10, .86, 0), (-.18, .70, 0), (-.16, .42, 0)], .024, MB_STEEL)   # backbone + spine
    _bent([(.42, .89, 0), (.33, .62, 0), (.26, .36, 0)], .024, MB_STEEL)                                   # downtube
    for sz in (-1, 1):
        _bent([(.26, .36, 0), (.22, .26, sz * .07), (.06, .21, sz * .08), (-.14, .22, sz * .08), (-.17, .40, sz * .06)],
              .019, MB_STEEL)                                                                               # cradle
        _bent([(-.10, .84, sz * .04), (-.40, .80, sz * .085), (-.78, .78, sz * .075)], .016, MB_STEEL)   # seat rail
        _bent([(-.16, .50, sz * .06), (-.46, .74, sz * .088)], .014, MB_STEEL)                           # strut
    finish(tube((-.17, .40, -.11), (-.17, .40, .11), .026, 16), MB_STEEL, .4, .4, 0)                    # swingarm pivot
    # ---- swingarm: rectangular steel each side, the axle adjusters at the end
    for sz in (-1, 1):
        a = Vector((-.17, .40, sz * .085)); b = Vector((WR, R, sz * .085)); d = (b - a)
        sa = rbox(d.length * .5, .022, .012, .006)
        oriented(sa, (a + b) * .5, d.normalized())
        finish(sa, MB_STEEL, .45, .35, 0)
        finish(place_o(box((0, 0, 0), (.035, .020, .006)), (WR + .01, R, sz * .099)), MB_ALU, .35, .7, 0)
    # ---- twin rear shocks: chrome spring over a black damper, eyes top and bottom
    for sz in (-1, 1):
        lo = Vector((WR + .10, R + .06, sz * .105)); hi = Vector((-.46, .77, sz * .100))
        d = (hi - lo).normalized()
        finish(tube(lo, hi, .016, 12, 'damp'), (.06, .06, .065), .4, .4, 0)
        finish(tube(lo + d * .02, lo + d * .10, .024, 14, 'body'), (.08, .08, .09), .4, .4, 0)
        finish(tube(hi - d * .08, hi - d * .005, .030, 14, 'cap'), CHROME, .2, .95, 0)
        n = 6
        for k in range(n):
            c = lo + (hi - lo) * (.24 + .52 * k / (n - 1))
            ring = _disc([(-.0045, .030), (.0045, .030)], c, d, 8)
            finish(ring, CHROME, .2, .95, 0)
        for e in (lo, hi):
            _ball(e, .018, (.06, .06, .065), .5, .3, 8, (1, 1, .8))
    # ---- the fork: chrome stanchions into alloy sliders, two triple clamps
    fax = (HEAD_T - HEAD_B).normalized()
    off = .045                                            # fork offset ahead of the head axis
    nrm = Vector((fax.y, -fax.x, 0))                      # forward, square to the head axis
    axle = Vector((WF, R, 0))
    for sz in (-1, 1):
        top = HEAD_T + nrm * off + fax * .02 + Vector((0, 0, sz * .085))
        bot = axle + Vector((0, 0, sz * .085))
        dd = (top - bot); L = dd.length; u = dd.normalized()
        finish(tube(bot - u * .02, bot + u * L * .45, .027, 16, 'slider'), MB_ALU, .35, .75, 0)
        finish(tube(bot + u * L * .45, top + u * .02, .0185, 16, 'stanch'), CHROME, .15, .95, 0)
        finish(tube(bot + u * L * .42, bot + u * L * .46, .0295, 16, 'seal'), (.05, .05, .055), .6, 0, 0)
        _ball(bot, .028, MB_ALU, .35, .75, 8)
    for t_, h in ((HEAD_T + fax * .02, .020), (HEAD_B - fax * .01, .024)):
        tc = rbox(.040, h * .5, .120, .012)
        oriented(tc, t_ + nrm * .025, nrm, up=fax)
        finish(tc, MB_ALU if h < .022 else MB_DARK, .35, .7, 0)
    # ---- front mudguard: a chrome blade on two stays
    bm = bmesh.new(); prev = None
    for k in range(13):
        a = .25 + (2.25 - .25) * k / 12
        c = Vector((WF + (R + .035) * cos(a), R + (R + .035) * sin(a), 0))
        n_ = Vector((cos(a), sin(a), 0))
        row = [bm.verts.new(c + Vector((0, 0, w)) - n_ * (.012 * (1 - (w / .062) ** 2))) for w in (-.062, -.031, 0, .031, .062)]
        if prev:
            for i in range(4): bm.faces.new((prev[i], prev[i + 1], row[i + 1], row[i]))
        prev = row
    o = from_bm(bm, 'fguard'); o.modifiers.new('s', 'SOLIDIFY').thickness = .003
    finish(o, paint, .25, .1, 0, flag=F_PAINT)
    for sz in (-1, 1):
        _bent([(WF - .06, R + .02, sz * .086), (WF - .02, R + .30, sz * .062)], .005, CHROME, .2, .95, 6)
    # ---- the tank: a teardrop, knee recesses, a chrome filler cap and badges
    tank = superloft([(.36, .945, .055, .040, 2.2), (.32, .955, .105, .070, 2.6), (.22, .960, .135, .092, 2.8),
                      (.10, .955, .138, .096, 2.8), (-.02, .935, .118, .088, 2.6), (-.10, .905, .085, .068, 2.4),
                      (-.13, .890, .050, .040, 2.2)], 16, 'tank', 1)
    finish(tank, paint, .22, .15, 0, flag=F_PAINT)
    finish(_disc([(0, .034), (.008, .030), (.012, .022)], (.12, 1.048, 0), (0, 1, 0), 12), CHROME, .2, .95, 0)
    for sz in (-1, 1):
        kp = superloft([(.02, .90, .006, .032, 3), (-.03, .895, .006, .036, 3), (-.09, .88, .006, .030, 3)], 12, 'knee', 1)
        place_o(kp, (0, 0, sz * .108))
        finish(kp, (.05, .05, .055), .8, 0, 0)
        bd = rbox(.045, .014, .003, .004); place_o(bd, (.20, .955, sz * .136), (0, -sz * .15, 0))
        finish(bd, CHROME, .2, .95, 0)
    # ---- the seat: stitched bench, piping, grab rail
    seat = superloft([(-.10, .925, .060, .030, 3), (-.16, .935, .125, .040, 3.4), (-.34, .915, .140, .040, 3.6),
                      (-.56, .905, .132, .036, 3.4), (-.70, .900, .100, .030, 3), (-.74, .895, .060, .022, 2.6)],
                     16, 'seat', 1)
    finish(seat, (.045, .045, .05), .75, 0, 0)
    for xs in (-.24, -.40, -.56):
        rib = superloft([(xs + .006, .938, .128, .032, 3.4), (xs - .006, .938, .128, .032, 3.4)], 18, 'rib', 0)
        finish(rib, (.03, .03, .033), .85, 0, 0)
    _bent([(-.50, .87, .120), (-.70, .88, .118), (-.79, .88, .07), (-.80, .88, 0), (-.79, .88, -.07),
           (-.70, .88, -.118), (-.50, .87, -.120)], .011, CHROME, .2, .95, 10)
    # ---- side panels under the seat (paint), the battery box between them
    for sz in (-1, 1):
        sp = prof_front([(-.15, .80), (-.21, .69), (-.37, .67), (-.43, .76), (-.40, .855), (-.17, .865)],
                        sz * .108 - .007, sz * .108 + .007, 'side')
        finish(sp, paint, .25, .1, .006, 1, flag=F_PAINT)
        finish(_disc([(0, .016), (.003, .013)], (-.29, .77, sz * .115), (0, 0, sz), 16), CHROME, .2, .95, 0)
    finish(place_o(rbox(.11, .09, .075, .012), (-.30, .74, 0)), (.08, .08, .085), .6, 0, 0)          # battery / airbox
    # ---- the engine: crankcases, the finned barrel and head leaning forward,
    #      covers either side, the carburettor behind the barrel
    cc = prof_front([(-.14, .28), (.10, .25), (.22, .30), (.24, .42), (.16, .52), (.02, .56), (-.12, .52), (-.17, .42)],
                    -.110, .110, 'case')
    finish(cc, (.52, .52, .54), .4, .6, .008, 2)
    finish(_disc([(0, .105), (.020, .102), (.034, .090), (.040, .060)], (.04, .40, .110), (0, 0, 1), 12), (.60, .60, .62), .3, .75, 0)   # clutch cover
    finish(_disc([(0, .085), (.022, .082), (.032, .060)], (-.04, .40, -.110), (0, 0, -1), 12), (.60, .60, .62), .3, .75, 0)  # alternator cover
    finish(_disc([(0, .030), (.006, .026)], (.04, .40, .150), (0, 0, 1), 12), (.30, .30, .32), .3, .8, 0)
    cax = Vector((.30, 1.0, 0)).normalized()             # the cylinder leans 17 degrees forward
    c0 = Vector((.14, .52, 0))
    finish(tube(c0 - cax * .02, c0 + cax * .20, .052, 20, 'barrel'), (.20, .20, .21), .5, .5, 0)
    for k in range(9):
        c = c0 + cax * (.025 + .019 * k)
        f = box((0, 0, 0), (.082 if k < 6 else .090, .0035, .078 if k < 6 else .086))
        oriented(f, c, Vector((cax.y, -cax.x, 0)), up=cax)
        finish(f, (.24, .24, .25) if k < 6 else (.50, .50, .52), .5, .5 if k < 6 else .65, 0)
    rk = rbox(.075, .030, .062, .016); oriented(rk, c0 + cax * .215, Vector((cax.y, -cax.x, 0)), up=cax)
    finish(rk, (.55, .55, .57), .3, .7, 0)                                                            # rocker cover
    finish(tube(c0 + cax * .14 + Vector((-.06, 0, 0)), (-.10, .64, 0), .022, 12), (.12, .12, .13), .5, .3, 0)   # inlet
    carb = rbox(.035, .030, .032, .008); place_o(carb, (-.08, .63, .01))
    finish(carb, (.55, .55, .57), .35, .65, 0)
    finish(tube((-.11, .63, .01), (-.20, .66, .03), .030, 14), (.07, .07, .075), .7, 0, 0)        # intake boot
    finish(tube((.18, .74, .02), (.18, .78, .055), .006, 6), (.05, .05, .05), .6, 0, 0)            # plug lead
    # ---- exhaust: header off the front of the head, down and back on the right
    ex0 = c0 + cax * .10 + Vector((.06, 0, .02))
    _bent([ex0, (.29, .52, .05), (.30, .32, .09), (.20, .22, .125), (-.10, .23, .14), (-.26, .28, .145)],
          .019, CHROME, .18, .95, 10)
    m0, m1 = Vector((-.24, .275, .145)), Vector((-.82, .43, .145))
    md = (m1 - m0).normalized()
    sil = _disc([(0, .022), (.04, .044), (.10, .050), (.50, .050), (.56, .046), (.60, .040)], m0, md, 12)
    finish(sil, CHROME, .18, .95, 0)
    finish(_disc([(.598, .032), (.608, .030), (.612, .014), (.65, .014)], m0, md, 12), (.18, .18, .19), .4, .7, 0)
    hs = _disc([(.14, .057), (.40, .057)], m0, md, 12)
    finish(hs, (.06, .06, .065), .6, .3, 0)                                                         # heat shield
    # ---- chain and sprockets on the left
    fs, rs = Vector((-.02, .33, -.095)), Vector((WR, R, -.095))
    finish(_disc([(-.004, .045), (.004, .045)], fs, (0, 0, -1), 10), (.30, .30, .32), .45, .8, 0)
    finish(_disc([(-.004, .110), (.004, .110), (.004, .060)], rs, (0, 0, -1), 12), (.38, .38, .40), .45, .8, 0)
    for sg in (1, -1):
        a = fs + Vector((0, sg * .048, 0)); b = rs + Vector((0, sg * .113, 0))
        finish(tube(a, b, .0065, 6, 'chain'), (.12, .11, .10), .6, .6, 0)
    cg = rbox(((rs - fs).length) * .5 - .03, .006, .030, .004)
    oriented(cg, (fs + rs) * .5 + Vector((0, .145, -.012)), (rs - fs).normalized())
    finish(cg, (.05, .05, .055), .5, .2, 0)                                                         # chain guard
    # ---- footrests, kick-start, gear and brake levers
    for sz in (-1, 1):
        finish(tube((-.06, .30, sz * .13), (-.06, .30, sz * .23), .016, 10), P_RUBBER, .9, 0, 0)
        finish(tube((-.06, .30, sz * .10), (-.06, .30, sz * .13), .009, 8), MB_STEEL, .4, .5, 0)
        finish(tube((-.40, .44, sz * .14), (-.40, .44, sz * .21), .011, 8), P_RUBBER, .9, 0, 0)   # pillion pegs
    _bent([(-.10, .44, .155), (-.22, .38, .17), (-.24, .30, .18)], .009, CHROME, .25, .9, 8)       # kick-start
    _bent([(.02, .37, -.13), (.06, .31, -.17), (.12, .31, -.17)], .007, MB_STEEL, .4, .5, 8)       # gear lever
    _bent([(-.02, .30, .12), (.12, .29, .14), (.16, .30, .15)], .007, MB_STEEL, .4, .5, 8)         # brake pedal
    # ---- headlamp, clocks, handlebar, levers, mirrors, indicators
    hl_c = Vector((.505, .915, 0))
    finish(_disc([(-.070, .040), (-.050, .072), (-.010, .086), (0, .086)], hl_c, (1, 0, 0), 18), (.06, .06, .065), .4, .3, 0)
    finish(_disc([(0, .088), (.010, .088), (.016, .082)], hl_c, (1, 0, 0), 18), CHROME, .15, .95, 0)
    finish(_disc([(.002, .078), (.012, .076), (.016, .060), (.018, .0)], hl_c, (1, 0, 0), 18), LENS_W, .06, 0, 0)
    for sz in (-1, 1):
        finish(tube(hl_c + Vector((-.03, 0, sz * .085)), hl_c + Vector((-.06, 0, sz * .092)), .008, 8), MB_STEEL, .4, .5, 0)
    for sz in (-1, 1):                                                                              # twin clocks
        cl = Vector((.395, 1.055, sz * .052))
        cn = Vector((-.35, 1.0, 0)).normalized()
        finish(_disc([(-.040, .030), (-.010, .040), (0, .040)], cl, cn, 12), (.05, .05, .055), .5, .2, 0)
        finish(_disc([(0, .041), (.005, .041), (.008, .036)], cl, cn, 12), CHROME, .15, .95, 0)
        finish(_disc([(.001, .035), (.004, .0)], cl, cn, 12), (.85, .85, .80), .2, 0, 0)
    bar = [(.40, 1.035, -.10), (.38, 1.08, -.20), (.31, 1.10, -.28), (.27, 1.095, -.33)]
    _bent([(x, y, -z) for x, y, z in bar[::-1]] + bar, .0115, CHROME, .18, .95, 10)
    for sz in (-1, 1):
        g0 = Vector((.31, 1.10, sz * .28)); g1 = Vector((.255, 1.092, sz * .385))
        finish(tube(g0 + (g1 - g0) * .30, g1, .0175, 14), P_RUBBER, .9, 0, 0)                      # grips
        finish(_disc([(0, .018), (.008, .015)], g1, g1 - g0, 12), (.06, .06, .06), .6, 0, 0)
        pc = rbox(.026, .022, .024, .008); place_o(pc, g0 + Vector((0, 0, sz * .005)))
        finish(pc, (.06, .06, .065), .5, .2, 0)                                                       # switchgear
        lv0 = g0 + Vector((.03, -.005, 0)); lv1 = g1 + Vector((.065, -.012, sz * .010))
        _bent([lv0, lv0 + Vector((.035, 0, 0)), lv1], .0055, MB_ALU, .3, .8, 8)                    # levers
        st0 = Vector((.33, 1.11, sz * .225)); st1 = Vector((.30, 1.235, sz * .285))
        _bent([st0, st0 + Vector((0, .07, sz * .01)), st1], .0055, CHROME, .2, .95, 8)             # mirror stalks
        mh = _disc([(-.006, .050), (.010, .052), (.024, .040)], st1 + Vector((.010, 0, 0)), (1, 0, 0), 10)
        for v in mh.data.vertices:                                                                  # oval head
            v.co.z = st1.z + (v.co.z - st1.z) * 1.25; v.co.y = st1.y + (v.co.y - st1.y) * .82
        finish(mh, (.05, .05, .055), .45, .3, 0)
        mg = _disc([(-.007, .046), (-.006, .0)], st1 + Vector((.010, 0, 0)), (1, 0, 0), 10)
        for v in mg.data.vertices:
            v.co.z = st1.z + (v.co.z - st1.z) * 1.25; v.co.y = st1.y + (v.co.y - st1.y) * .82
        finish(mg, (.55, .60, .64), .05, .9, 0)
        for (ix, iy, iz, dx_) in ((.47, .925, sz * .135, 1), (-.80, .78, sz * .110, -1)):           # indicators
            finish(tube((ix - dx_ * .045, iy, iz * .80), (ix, iy, iz), .006, 8), (.06, .06, .065), .5, .2, 0)
            finish(_disc([(-.030, .020), (-.006, .024), (.008, .018), (.014, .0)], (ix, iy, iz), (dx_, 0, 0), 8),
                   LENS_A, .1, 0, 0)
    # ---- rear mudguard, tail lamp, number plate
    bm = bmesh.new(); prev = None
    for k in range(11):
        a = .55 + (1.95 - .55) * k / 10
        c = Vector((WR + (R + .045) * cos(a), R + (R + .045) * sin(a), 0))
        n_ = Vector((cos(a), sin(a), 0))
        row = [bm.verts.new(c + Vector((0, 0, w)) - n_ * (.014 * (1 - (w / .070) ** 2))) for w in (-.070, -.035, 0, .035, .070)]
        if prev:
            for i in range(4): bm.faces.new((prev[i], prev[i + 1], row[i + 1], row[i]))
        prev = row
    o = from_bm(bm, 'rguard'); o.modifiers.new('s', 'SOLIDIFY').thickness = .003
    finish(o, paint, .25, .1, 0, flag=F_PAINT)
    tl = rbox(.030, .028, .055, .012); place_o(tl, (-.86, .73, 0))
    finish(tl, (.06, .06, .065), .5, .2, 0)
    finish(place_o(rbox(.006, .022, .046, .008), (-.892, .735, 0)), LENS_R, .1, 0, 0)
    pl = place_o(box((0, 0, 0), (.004, .060, .088)), (-.90, .615, 0), (0, 0, -.25))
    finish(pl, PLATE, .5, 0, 0)
    finish(place_o(box((0, 0, 0), (.003, .066, .094)), (-.896, .614, 0), (0, 0, -.25)), (.05, .05, .055), .5, .2, 0)
    finish(tube((-.86, .70, 0), (-.89, .67, 0), .008, 8), (.06, .06, .065), .5, .2, 0)
    # ---- the side stand, down on the left
    _bent([(-.08, .25, -.09), (-.10, .14, -.16), (-.14, .015, -.245)], .011, MB_STEEL, .45, .4, 8)
    finish(place_o(rbox(.030, .006, .022, .004), (-.145, .006, -.250)), MB_STEEL, .45, .4, 0)
    # lean the whole bike onto its stand (rolled about the ground line). The
    # mode reset matters: tube() leaves its bars in QUATERNION mode, where an
    # euler rotation is silently ignored and the frame stayed bolt upright
    # inside a leaning body.
    for o in list(bpy.context.collection.objects):
        o.rotation_mode = 'XYZ'
        o.rotation_euler = (math.radians(-8), 0, 0)
        bpy.context.view_layer.objects.active = o; o.select_set(True)
        bpy.ops.object.transform_apply(rotation=True); o.select_set(False)

# ---------------------------------------------------------------- the wall air conditioner
def ac_unit():
    """A split-system outdoor unit on wall brackets. PROP frame for wall items:
    x out of the wall (the wall face at x = 0), y up, z along the wall."""
    clear()
    W, H, D = .40, .30, .14
    cas = rbox(D, H, W, .012); place_o(cas, (D + .06, H, 0))
    cut(cas, box((2 * D + .06, H, -W * .28), (.02, .22, .22)))            # the fan's recess
    finish(cas, (.86, .84, .78), .45, 0, 0)
    for k in range(3):                                    # the fan guard rings
        r = .07 + k * .06
        ring = lathe_z([(0, r), (.006, r)], 0, 0, 1, 16, cap=False)
        finish(place_o(ring, (2 * D + .045, H, -W * .28), (0, math.pi / 2, 0)), (.20, .20, .21), .5, .4, 0)
    finish(place_o(cyl_z(0, 0, 0, .01, .21, 16), (2 * D + .02, H, -W * .28), (0, math.pi / 2, 0)), (.03, .03, .035), .9, 0, 0)
    for k in range(4):                                    # fan blades
        b = box((0, 0, 0), (.004, .07, .025))
        finish(place_o(b, (2 * D + .03, H + .09 * cos(k * 1.57), -W * .28 + .09 * sin(k * 1.57)), (k * 1.57 + .5, 0, 0)), (.25, .25, .27), .5, .2, 0)
    for k in range(5):                                    # side louvres
        finish(box((2 * D + .062, .12 + k * .08, W * .45), (.004, .008, .12)), (.70, .69, .64), .5, 0, 0)
    for zz in (-W + .06, W - .06):                        # brackets: L-angles off the wall
        finish(prof_front([(0, -.02), (.40, -.02), (.40, .015), (.03, .015), (.03, .22), (0, .22)], zz - .012, zz + .012),
               (.28, .28, .30), .5, .5, 0)
    finish(box((.08, .70, W + .03), (.025, .50, .025)), (.88, .88, .86), .6, 0, 0)                 # pipe duct
    finish(tube((.28, -.02, W - .05), (.30, -.25, W - .05), .006, 5), (.10, .10, .11), .6, 0, 0)   # drip hose

# ---------------------------------------------------------------- the satellite dish
def sat_dish():
    """A 75 cm offset dish on a wall arm: the reflector, the feed arm and LNB.
    Same wall frame as the AC unit; it looks out and up."""
    clear()
    rings = [(0, .001)] + [(.001 + .07 * (k / 4) ** 2, .36 * k / 4) for k in range(1, 5)]
    d = lathe_z(rings, 0, 0, 1, 16, 'dish', cap=False)
    d.modifiers.new('s', 'SOLIDIFY').thickness = .006
    place_o(d, (.62, .26, 0), (0, math.radians(-90 + 0), 0))
    d.rotation_euler = (0, 0, math.radians(32)); d.location = (.62, .26, 0)
    finish(d, (.86, .86, .84), .45, .1, 0)
    finish(tube((0, 0, 0), (.50, .10, 0), .018, 6), (.55, .55, .57), .4, .6, 0)          # wall arm
    finish(box((.02, 0, 0), (.02, .09, .07)), (.55, .55, .57), .4, .6, 0)              # wall plate
    finish(tube((.50, .10, 0), (.60, .26, 0), .016, 6), (.55, .55, .57), .4, .6, 0)
    feed_end = Vector((.62, .26, 0)) + Vector((cos(math.radians(32)), sin(math.radians(32)), 0)) * .40
    finish(tube((.58, .12, 0), feed_end, .010, 6), (.55, .55, .57), .4, .6, 0)
    lnb = rbox(.05, .035, .03, .012); place_o(lnb, feed_end)
    finish(lnb, (.15, .15, .16), .5, 0, 0)
    finish(tube((.02, -.05, .03), (.02, -1.2, .03), .004, 4), (.05, .05, .05), .6, 0, 0)   # the coax down the wall

# ---------------------------------------------------------------- the rooftop water tank
def water_tank():
    """A 1000 L polyethylene roof tank on a steel stand: ribbed drum, screw lid,
    the outlet pipe down to the roof."""
    clear()
    rings = [(0, .40)]
    for k in range(4):
        y = .08 + k * .24
        rings += [(y, .44), (y + .12, .455)]
    rings += [(1.04, .43), (1.10, .34), (1.15, .15), (1.17, .001)]
    t = _rz([(.55 + y, r) for y, r in rings], 0, 0, 16, 'tank')   # along z: turn it upright
    place_o(t, (0, 0, 0), (math.radians(-90), 0, 0))
    finish(t, (.5, .5, .5), .55, 0, 0, flag=F_PAINT)
    lid = _rz([(1.71, .13), (1.76, .13), (1.77, .11)], 0, 0, 10)
    finish(place_o(lid, (0, 0, 0), (math.radians(-90), 0, 0)), (.5, .5, .5), .5, 0, 0, flag=F_PAINT)
    st = (.32, .32, .34)
    finish(box((0, .53, 0), (.50, .02, .50)), st, .5, .5, 0)
    for x in (-.45, .45):
        for z in (-.45, .45):
            finish(box((x, .27, z), (.025, .27, .025)), st, .5, .5, 0)
    for x in (-.45, .45):
        finish(tube((x, .05, -.45), (x, .50, .45), .012, 6), st, .5, .5, 0)
    finish(tube((.38, .65, 0), (.55, .65, 0), .025, 8), (.80, .80, .78), .5, 0, 0)
    finish(tube((.55, .65, 0), (.55, 0, 0), .025, 8), (.80, .80, .78), .5, 0, 0)
    finish(place_o(cyl_z(0, 0, -.04, .04, .045, 8), (.55, .62, 0)), (.60, .15, .10), .5, .3, 0)   # valve

# ---------------------------------------------------------------- the street cafe set
def cafe_set():
    """A tin table with two white monobloc chairs and a tea tray: the street cafe."""
    clear()
    finish(box((0, .73, 0), (.36, .015, .36)), (.70, .68, .62), .45, .5, 0)
    finish(tube((0, 0, 0), (0, .72, 0), .025), (.25, .25, .27), .4, .6, 0)
    for k in range(4):
        a = k * math.pi / 2 + .785
        finish(tube((0, .02, 0), (.30 * cos(a), .01, .30 * sin(a)), .015), (.25, .25, .27), .4, .6, 0)
    for k, (x, z, ry) in enumerate(((.62, 0, math.pi), (-.10, .62, -math.pi / 2 + .3))):
        c = []
        seat = rbox(.21, .02, .21, .015); c.append(seat)
        place_o(seat, (0, .44, 0))
        back = rbox(.02, .20, .21, .015); place_o(back, (.22, .66, 0), (0, 0, -.18)); c.append(back)
        for lx in (-.18, .18):
            for lz in (-.18, .18):
                c.append(tube((lx, .43, lz), (lx * 1.15, 0, lz * 1.15), .016))
        for o in c:
            o.rotation_euler = (0, ry, 0); o.location = (x, 0, z)
            bpy.context.view_layer.objects.active = o; o.select_set(True)
            bpy.ops.object.transform_apply(location=True, rotation=True); o.select_set(False)
            finish(o, (.90, .90, .87), .55, 0, 0)
    finish(box((-.05, .755, .02), (.15, .008, .10)), (.75, .70, .45), .3, .8, 0)      # tray
    for k in range(3):
        g = _rz([(.763, .022), (.85, .026)], 0, 0, 8)
        place_o(g, (0, 0, 0), (math.radians(-90), 0, 0))
        g.location = (-.12 + k * .07, 0, .02); bpy.context.view_layer.objects.active = g; g.select_set(True)
        bpy.ops.object.transform_apply(location=True); g.select_set(False)
        finish(g, (.55, .25, .10), .1, 0, 0)
    finish(place_o(_rz([(.763, .05), (.86, .06), (.90, .03), (.92, .015)], 0, 0, 10), (.10, 0, .02), (math.radians(-90), 0, 0)),
           (.70, .70, .72), .25, .9, 0)                                                 # teapot

# ---------------------------------------------------------------- tyres
def tyre_stack():
    """Used tyres outside a workshop: a stack of four and one leaning on it."""
    clear()
    def tyre(c, axis_rot):
        rings = [(-.09, .21), (-.085, .30), (-.06, .33), (.06, .33), (.085, .30), (.09, .21),
                 (.05, .205), (-.05, .205)]
        t = _rz(rings, 0, 0, 18, 'ty', loop=True)
        place_o(t, c, axis_rot)
        finish(t, P_RUBBER, .92, 0, 0)
    for k in range(4):
        tyre((.02 * (k % 2), .09 + k * .18, .01 * k), (math.radians(90), 0, k * .3))
    tyre((.55, .32, .05), (0, math.radians(90), math.radians(-18)))

# ---------------------------------------------------------------- the well head
def well_frame():
    """The town well's head over the stone drum (octa r 1.0, 0.9 high): a dressed
    coping, the dark shaft, two timber posts and a crossbeam with a pulley, the
    rope down to a bucket, and the winch crank."""
    clear()
    WOOD = (.36, .24, .15); STONE = (.78, .70, .58)
    finish(place_o(_rz([(0, .99), (.06, .99), (.065, .72)], 0, 0, 16, 'cope'), (0, .90, 0), (math.radians(-90), 0, 0)),
           STONE, .85, 0, 0)
    finish(place_o(_rz([(0, .70), (.004, .70)], 0, 0, 16, 'shaft'), (0, .962, 0), (math.radians(-90), 0, 0)), (.02, .02, .025), .95, 0, 0)
    for sx in (-.86, .86):
        finish(box((sx, 1.62, 0), (.055, .70, .055)), WOOD, .85, 0, .006)
        finish(box((sx, .98, 0), (.11, .03, .11)), WOOD, .85, 0, .006)
    finish(box((0, 2.34, 0), (.98, .05, .05)), WOOD, .85, 0, .006)
    finish(cyl_z(0, 2.20, -.04, .04, .11, 14), (.30, .30, .32), .5, .6, 0)          # pulley
    finish(box((0, 2.28, 0), (.02, .06, .05)), (.30, .30, .32), .5, .6, 0)
    finish(tube((0.11, 2.20, 0), (0.11, 1.52, 0), .009, 5), (.58, .50, .36), .9, 0, 0)    # rope
    finish(place_o(_rz([(0, .10), (.20, .12), (.205, .115)], 0, 0, 12, 'bucket'), (0.11, 1.30, 0), (math.radians(-90), 0, 0)),
           (.40, .38, .36), .5, .6, 0)
    finish(tube((-0.01, 1.52, 0), (0.23, 1.52, 0), .006, 5), (.25, .25, .27), .5, .6, 0)   # bail
    finish(tube((.86, 1.85, .06), (.86, 1.85, .28), .02, 6), (.25, .25, .27), .5, .6, 0)   # crank
    finish(tube((.86, 1.85, .28), (.86, 1.70, .28), .02, 6), (.25, .25, .27), .5, .6, 0)

# ---------------------------------------------------------------- the industrial diesel generator
def genset():
    """A 2.2 m skid-mounted industrial diesel generator set: heavy I-beam base
    skid with forklift pockets, acoustic painted canopy with ventilation
    louvres, latched access doors, control panel and overhead exhaust silencer."""
    clear()
    HX, HZ = 1.04, 0.48
    SKID = (.14, .14, .15); DRK = (.10, .10, .11); CHR = (.66, .67, .69)
    # base skid rails and crossmembers
    for sz in (-1, 1):
        finish(box((0, .065, sz * .44), (HX + .06, .065, .045)), SKID, .55, .4, 0)
    for xx in (-1.04, -.46, .46, 1.04):
        finish(box((xx, .055, 0), (.045, .055, HZ + .01)), SKID, .55, .4, 0)
    finish(box((0, .135, 0), (HX + .04, .02, HZ)), SKID, .6, .3, 0)
    # forklift pockets in the skid
    for fx in (-.32, .32):
        finish(box((fx, .065, 0), (.09, .042, HZ + .02)), (.04, .04, .045), .8, 0, 0)
    # acoustic canopy with chamfered roof (painted body)
    body = prof([(-HX, .155), (HX, .155), (HX, 1.18), (HX - .10, 1.28),
                 (-HX + .10, 1.28), (-HX, 1.18)], -HZ, HZ, 'canopy')
    finish(body, (.5, .5, .5), .48, .15, 0, flag=F_PAINT)
    # recessed side access doors, hinges, handles and acoustic louvers
    for sz in (-1, 1):
        for dx in (-.38, .38):
            finish(box((dx, .68, sz * (HZ + .003)), (.30, .38, .005)), (.5, .5, .5), .46, .15, 0, flag=F_PAINT2)
            finish(box((dx + .22, .68, sz * (HZ + .010)), (.018, .055, .008)), CHR, .3, .8, 0)   # T-latch
            for hy in (.44, .92):
                finish(box((dx - .29, hy, sz * (HZ + .009)), (.015, .032, .006)), DRK, .5, .4, 0)
            for k in range(5):
                finish(box((dx, .86 + k * .038, sz * (HZ + .009)), (.22, .008, .006)), DRK, .65, 0, 0)
    # control panel & viewing glass (left side, rear bay)
    finish(box((-.78, .76, HZ + .010), (.15, .19, .012)), DRK, .55, .2, 0)
    finish(box((-.78, .82, HZ + .022), (.11, .09, .004)), (.12, .16, .18), .08, 0, 0, flag=F_GLASS)
    finish(lathe_z([(0, .026), (.016, .024)], -.85, .64, 1, 10), (.78, .10, .08), .35, 0, 0)   # E-stop button
    finish(box((-.72, .64, HZ + .022), (.045, .032, .006)), (.25, .26, .28), .5, .4, 0)          # breaker
    # front radiator grille (+x) & rear louvered intake (-x)
    for lx, nx in ((HX + .008, 1), (-HX - .008, -1)):
        finish(box((lx, .70, 0), (.008, .36, .34)), DRK, .7, .2, 0)
        for k in range(7):
            finish(box((lx + nx * .006, .42 + k * .09, 0), (.006, .012, .32)), SKID, .55, .3, 0)
    # top cylindrical exhaust silencer + vertical tailpipe + lifting eye
    finish(_rx(-.55, .32, 1.38, 0.0, .105, 12, 'silencer'), (.36, .36, .38), .52, .65, 0)
    for bx in (-.40, .18):
        finish(box((bx, 1.32, 0), (.02, .06, .12)), SKID, .5, .5, 0)
    finish(_ry(1.38, 1.62, -.42, 0.0, .042, 10, 'stack'), (.36, .36, .38), .5, .65, 0)
    finish(place_o(box((0, 0, 0), (.055, .006, .055)), (-.42, 1.63, 0), (0, 0, .25)), CHR, .35, .8, 0)
    finish(box((0, 1.33, 0), (.09, .05, .016)), SKID, .5, .5, 0)   # central lifting lug

# ---------------------------------------------------------------- the forklift
def _bev(o, w, seg=3):
    """Round every edge of a hard-surface shell (bevel applied, so the shell
    keeps its silhouette at range and still catches a highlight on each edge)."""
    m = o.modifiers.new('rb', 'BEVEL'); m.width = w; m.segments = seg
    m.limit_method = 'ANGLE'; m.angle_limit = radians(30)
    bpy.context.view_layer.objects.active = o; o.select_set(True)
    bpy.ops.object.modifier_apply(modifier=m.name); o.select_set(False)
    return o

def _ind_wheel(x, R, W, zc, sgn, name):
    """An industrial cushion wheel: a deep solid tyre with a rounded shoulder and
    lug tread, a pressed-steel rim with a dished centre, the hub and its nuts."""
    zi = zc - sgn * W * .5
    rings = [(0, R*.62), (.006, R*.86), (.03, R*.97), (.055, R), (W - .055, R), (W - .03, R*.97),
             (W - .006, R*.86), (W, R*.62)]
    t = lathe_z([(zi*sgn + dz, r) for dz, r in rings], x, R, sgn, 28, name + '_tyre')
    finish(t, P_RUBBER, .93, 0, 0)
    for k in range(18):                                       # lug tread, staggered
        a = 2*math.pi*k/18
        off = (W * .18) * (1 if k % 2 else -1)
        g = box((0, 0, 0), (.010, .022, W * .26))
        g.location = (x + (R + .006)*cos(a), R + (R + .006)*sin(a), zc + off)
        g.rotation_euler = (0, 0, a)
        bpy.context.view_layer.objects.active = g; g.select_set(True)
        bpy.ops.object.transform_apply(location=True, rotation=True); g.select_set(False)
        cut(t, g)
    zo = zc + sgn * W * .5
    finish(lathe_z([(zo*sgn - .06, R*.61), (zo*sgn - .004, R*.61)], x, R, sgn, 24), (.05, .05, .055), .8, 0, 0)
    rim = lathe_z([(zo*sgn - .010, R*.61), (zo*sgn + .004, R*.58), (zo*sgn + .002, R*.50),
                   (zo*sgn - .030, R*.40), (zo*sgn - .028, R*.22)], x, R, sgn, 24, name + '_rim', cap=False)
    finish(rim, (.62, .10, .07), .45, .35, 0)                 # red pressed rim
    for k in range(6):                                        # dish holes
        a = 2*math.pi*k/6 + .26
        finish(lathe_z([(zo*sgn - .029, .022), (zo*sgn - .026, .022)], x + R*.31*cos(a), R + R*.31*sin(a),
                       sgn, 10), (.04, .04, .045), .9, 0, 0)
    finish(lathe_z([(zo*sgn - .04, R*.21), (zo*sgn - .012, R*.20), (zo*sgn - .004, R*.12)], x, R, sgn, 16),
           (.30, .30, .32), .4, .7, 0)
    for k in range(6):
        a = 2*math.pi*k/6
        finish(lathe_z([(zo*sgn - .012, .012), (zo*sgn + .002, .010)], x + R*.15*cos(a), R + R*.15*sin(a),
                       sgn, 6), (.55, .55, .57), .35, .8, 0)

def forklift():
    """A 2.7 m counterbalance LPG forklift, built along +x (forks forward):
    a rounded steel body in the instance paint with wheel arches and fenders,
    a cast counterweight with a black-and-yellow rear plate and tow pin, red
    pressed-steel wheels on cushion tyres, an operator deck with a suspension
    seat, dash, tilted column and steering wheel, three hydraulic levers, the
    overhead guard on four posts with work lights and a beacon, the LPG bottle
    on its cradle, and a two-stage mast with chains, a lift ram, tilt rams, the
    carriage, the load backrest and two tapered forks."""
    clear()
    BLK = (.045, .045, .05); DST = (.13, .13, .14); STL = (.30, .31, .33); MAST = (.10, .10, .11)
    CHR = (.74, .75, .77); FORK = (.17, .17, .18)
    HW = .50                                                   # body half width
    FX, FR, FW = .42, .31, .22                                 # front wheel: x, radius, width
    RX, RR, RW = -.90, .25, .18                                # rear wheel
    # THE BODY SHELL: side profile in (x, y), across z. The operator well is
    # carved out of it, the arches are cut, and every edge is rounded.
    body = prof_front([(-.88, .26), (.66, .26), (.70, .34), (.70, .62), (.56, .70), (.47, .74),
                       (.44, 1.00), (.36, 1.06), (.24, 1.05), (.20, .62), (-.24, .62), (-.28, .96),
                       (-.86, .98)], -HW, HW, 'fl_body')
    for xw, rr in ((FX, FR + .045), (RX, RR + .04)):
        cut(body, cyl_z(xw, rr - .045 if xw == FX else rr - .04, -HW - .1, HW + .1, rr, 32))
    _bev(body, .035, 3)
    finish(body, (.5, .5, .5), .42, .12, 0, flag=F_PAINT)
    # the operator deck: an inner tub, a ribbed rubber mat and the step
    finish(box((-.02, .625, 0), (.21, .006, HW - .06)), BLK, .95, 0, 0)
    for k in range(8):
        finish(box((-.20 + k * .052, .634, 0), (.010, .004, HW - .09)), (.08, .08, .085), .9, 0, 0)
    for sz in (-1, 1):
        finish(box((-.02, .44, sz * (HW + .03)), (.20, .016, .05)), DST, .6, .5, 0)       # side step
        for k in range(5):
            finish(box((-.18 + k * .08, .458, sz * (HW + .03)), (.012, .003, .045)), STL, .5, .6, 0)
    # fenders over the front wheels, black, standing proud of the shell
    for sz in (-1, 1):
        fen = _rz([(sz * (HW - .02), FR + .06), (sz * (HW + .12), FR + .06), (sz * (HW + .12), FR + .085),
                   (sz * (HW - .02), FR + .085)], FX, FR, 32, 'fender', loop=True)
        cut(fen, box((FX, FR - .45, 0), (.6, .45, 1.0)))
        finish(fen, BLK, .7, 0, 0)
    # THE COUNTERWEIGHT: a cast block rounded hard, the rear plate in hazard
    # stripes, the tow pin, two tail lamps.
    cw = prof_front([(-1.24, .20), (-.84, .20), (-.84, 1.10), (-.98, 1.16), (-1.18, 1.12),
                     (-1.28, .92), (-1.30, .45)], -HW - .02, HW + .02, 'fl_cw')
    cut(cw, cyl_z(RX, RR, -HW - .2, HW + .2, RR + .04, 32))
    _bev(cw, .06, 4)
    finish(cw, (.5, .5, .5), .55, .15, 0, flag=F_PAINT)
    for k in range(9):
        z0 = -.40 + k * .09
        finish(place_o(box((0, 0, 0), (.006, .085, .026)), (-1.305, .62, z0), (math.radians(35), 0, 0)),
               (.86, .66, .10) if k % 2 else BLK, .5, .1, 0)
    finish(box((-1.305, .62, 0), (.003, .11, .44)), BLK, .6, 0, 0)
    finish(_rx(-1.36, -1.26, .40, 0, .035, 12), STL, .4, .7, 0)                          # tow pin
    finish(box((-1.29, .40, 0), (.04, .06, .08)), DST, .5, .4, 0)
    for sz in (-1, 1):
        finish(box((-1.29, .96, sz * .38), (.012, .045, .06)), LENS_R, .15, 0, 0)
        finish(box((-1.29, .88, sz * .38), (.012, .025, .06)), LENS_A, .15, 0, 0)
    # THE WHEELS
    for sz in (-1, 1):
        _ind_wheel(FX, FR, FW, sz * (HW - .06), sz, 'fw')
        _ind_wheel(RX, RR, RW, sz * (HW - .07), sz, 'rw')
    # THE SEAT: base on the hood, cushion, backrest, hip restraints, a belt
    finish(box((-.50, 1.02, 0), (.17, .04, .20)), DST, .6, .3, 0)
    seat = _bev(box((-.50, 1.11, 0), (.21, .055, .23)), .04, 3)
    finish(seat, BLK, .85, 0, 0)
    back = _bev(box((0, 0, 0), (.05, .24, .22)), .04, 3)
    finish(place_o(back, (-.71, 1.38, 0), (0, 0, math.radians(-10))), BLK, .85, 0, 0)
    for sz in (-1, 1):
        hr = _bev(box((0, 0, 0), (.13, .06, .03)), .02, 2)
        finish(place_o(hr, (-.52, 1.24, sz * .25)), BLK, .8, 0, 0)
    finish(tube((-.62, 1.36, .22), (-.44, 1.16, -.18), .012, 5), (.70, .30, .06), .8, 0, 0)      # belt
    # THE DASH: the cowl top, a display, the column, the wheel and its knob
    finish(box((.36, 1.065, 0), (.10, .012, .30)), BLK, .7, 0, 0)
    finish(place_o(box((0, 0, 0), (.006, .045, .08)), (.27, 1.11, -.12), (0, 0, math.radians(20))),
           (.06, .10, .12), .1, 0, 0, flag=F_GLASS)
    finish(tube((.30, 1.05, 0), (.10, 1.34, 0), .03, 10), BLK, .6, .2, 0)
    whl = _rz([(-.014, .17), (0, .19), (.014, .17), (0, .15)], 0, 0, 24, 'swheel', loop=True)
    finish(place_o(whl, (.09, 1.36, 0), (math.radians(90), math.radians(-35), 0)), BLK, .65, 0, 0)
    for k in range(3):
        a = 2*math.pi*k/3 + .5
        finish(place_o(tube((0, 0, 0), (.15*cos(a), 0, .15*sin(a)), .012, 6), (.09, 1.36, 0),
                       (0, 0, math.radians(-35))), BLK, .65, 0, 0)
    finish(place_o(_ry(0, .06, .15, 0, .016, 8), (.09, 1.36, 0), (0, 0, math.radians(-35))), BLK, .6, 0, 0)
    for k, lz in enumerate((.18, .25, .32)):                                              # levers
        finish(tube((.32, 1.04, lz), (.27, 1.24, lz), .009, 6), STL, .4, .6, 0)
        finish(lathe_z([(-.022, .0), (-.015, .018), (.015, .018), (.022, 0)], .27, 1.255, 1, 10) if False else
               _rz([(lz - .02, .002), (lz - .016, .019), (lz + .016, .019), (lz + .02, .002)], .27, 1.255, 10),
               ((.85, .65, .1), (.12, .12, .13), (.65, .1, .07))[k], .35, 0, 0)
    # THE OVERHEAD GUARD: four posts, the frame, slats, grab handle, lights
    for sz in (-1, 1):
        z = sz * (HW - .04)
        finish(tube((.47, .98, z), (.33, 2.02, z), .032, 10), MAST, .5, .4, 0)
        finish(tube((-.86, 1.10, z), (-.80, 2.02, z), .032, 10), MAST, .5, .4, 0)
        finish(box((-.24, 2.03, z), (.60, .035, .035)), MAST, .5, .4, 0)
        finish(box((.33, 1.86, z * 1.02), (.035, .05, .05)), DST, .4, .5, 0)            # work light housings
        finish(box((.37, 1.86, z * 1.02), (.008, .04, .04)), LENS_W, .1, 0, 0)
    for xs in (-.80, .33):
        finish(box((xs, 2.03, 0), (.035, .035, HW - .04)), MAST, .5, .4, 0)
    for k in range(7):
        finish(box((-.68 + k * .16, 2.04, 0), (.020, .016, HW - .06)), MAST, .5, .4, 0)
    finish(tube((-.80, 1.70, -.38), (-.80, 1.70, .38), .016, 8), MAST, .5, .4, 0)        # rear grab bar
    for sz in (-1, 1):
        finish(box((-.81, 1.86, sz * .40), (.03, .04, .04)), DST, .4, .5, 0)
        finish(box((-.845, 1.86, sz * .40), (.006, .03, .03)), LENS_W, .1, 0, 0)
    finish(_ry(2.07, 2.12, -.70, .30, .035, 12), DST, .5, .2, 0)                         # beacon
    finish(_ry(2.12, 2.20, -.70, .30, .032, 12), LENS_A, .15, 0, 0)
    for sz in (-1, 1):                                                                   # mirrors
        finish(tube((.33, 1.70, sz * (HW - .02)), (.38, 1.72, sz * (HW + .12)), .008, 5), MAST, .5, .4, 0)
        finish(box((.38, 1.72, sz * (HW + .14)), (.01, .05, .06)), (.12, .14, .16), .1, .8, 0)
    # THE LPG BOTTLE on its cradle over the counterweight, valve and hose
    finish(_rz([(-.40, .03), (-.38, .15), (.38, .15), (.40, .03)], -.98, 1.42, 20, 'lpg'), (.78, .78, .76), .35, .6, 0)
    for sz in (-.24, .24):
        finish(_rz([(sz - .02, .158), (sz + .02, .158)], -.98, 1.42, 20), BLK, .6, 0, 0)
        finish(box((-.98, 1.24, sz), (.10, .04, .02)), MAST, .5, .4, 0)
    finish(_rz([(.40, .02), (.47, .02)], -.98, 1.42, 8), (.55, .45, .12), .3, .8, 0)
    finish(tube((-.98, 1.42, .47), (-.86, 1.10, .45), .010, 5), BLK, .7, 0, 0)
    # THE MAST: outer and inner channels, crossheads, the lift ram, chains
    for sz in (-1, 1):
        z = sz * .31
        finish(box((.80, 1.08, z), (.07, 1.02, .016)), MAST, .45, .5, 0)                  # outer web
        finish(box((.80, 1.08, z + sz * .03), (.012, 1.02, .03)), MAST, .45, .5, 0)       # flanges
        finish(box((.74, 1.08, z - sz * .02), (.012, 1.02, .035)), MAST, .45, .5, 0)
        finish(box((.84, 1.16, z - sz * .07), (.05, .98, .012)), DST, .45, .55, 0)        # inner channel
        finish(_rz([(z - .02, .045), (z + .02, .045)], .86, 2.10, 14), STL, .4, .6, 0)    # chain sheave
        for k in range(26):                                                               # the chain
            finish(box((.90, .70 + k * .055, z - sz * .07), (.012, .022, .018)), (.20, .20, .21), .5, .7, 0)
        # tilt ram from the frame to the mast
        finish(tube((.40, .55, sz * (HW - .10)), (.74, .80, sz * .30), .030, 10), BLK, .5, .3, 0)
        finish(tube((.58, .67, sz * (HW - .10) * .9), (.74, .80, sz * .30), .016, 8), CHR, .2, .9, 0)
    for y in (.16, 1.30, 2.05):
        finish(box((.80, y, 0), (.05, .045, .29)), MAST, .45, .5, 0)                      # crossheads
    finish(_ry(.20, 1.70, .76, 0, .045, 14), BLK, .5, .3, 0)                              # lift ram
    finish(_ry(1.70, 2.10, .76, 0, .026, 12), CHR, .18, .95, 0)
    # THE CARRIAGE: two plates, the load backrest grid, the fork bar
    for y in (.22, .58):
        finish(box((.93, y, 0), (.025, .06, .52)), MAST, .45, .5, 0)
    finish(box((.93, 1.24, 0), (.02, .02, .50)), MAST, .45, .5, 0)
    for k in range(7):
        finish(box((.93, .92, -.48 + k * .16), (.016, .32, .014)), MAST, .45, .5, 0)
    finish(box((.96, .60, 0), (.008, .02, .50)), (.86, .66, .10), .5, .1, 0)
    # THE FORKS: shank, heel and a blade that tapers to the tip
    for fz in (-.24, .24):
        finish(box((.975, .36, fz), (.024, .30, .055)), FORK, .55, .55, 0)
        finish(prof_front([(.95, .02), (1.70, .02), (1.75, .03), (1.75, .045), (1.40, .07), (.95, .085)],
                          fz - .055, fz + .055, 'fork'), FORK, .55, .55, 0)
        finish(box((.985, .66, fz), (.03, .03, .06)), FORK, .55, .55, 0)
    # headlights on the front posts' feet, and a data plate on the cowl
    for sz in (-1, 1):
        finish(box((.52, .82, sz * .40), (.015, .045, .06)), DST, .4, .5, 0)
        finish(box((.536, .82, sz * .40), (.004, .035, .05)), LENS_W, .1, 0, 0)
    finish(box((.20, .84, -HW - .002), (.06, .04, .002)), (.75, .75, .72), .3, .7, 0)

# ---------------------------------------------------------------- the rooftop solar water heater
def solar_heater():
    """A rooftop thermosiphon solar water heater: galvanised A-frame stand,
    tilted glazed collector panel and top cylindrical insulated tank."""
    clear()
    FRAME = (.56, .58, .60); ALUM = (.22, .23, .25); COPPER = (.68, .38, .20)
    # steel A-frame legs (+x faces south / front low edge)
    for sz in (-1, 1):
        zz = sz * .72
        finish(box((-.54, .60, zz), (.025, .60, .025)), FRAME, .48, .65, 0)
        finish(tube((-.54, 1.18, zz), (.62, .12, zz), .022, 6), FRAME, .48, .65, 0)
        finish(tube((-.54, .18, zz), (.10, .64, zz), .016, 5), FRAME, .48, .65, 0)
        finish(box((-.54, .02, zz), (.07, .02, .07)), FRAME, .5, .6, 0)
        finish(box((.62, .02, zz), (.07, .02, .07)), FRAME, .5, .6, 0)
    finish(box((-.54, .98, 0), (.02, .02, .74)), FRAME, .48, .65, 0)
    finish(box((.54, .14, 0), (.025, .025, .78)), FRAME, .48, .65, 0)
    # tilted collector tray + glass over dark absorber fins
    tray = place_o(box((0, 0, 0), (.56, .035, .82)), (.06, .56, 0), (0, 0, math.radians(-37)))
    finish(tray, ALUM, .45, .5, 0)
    glass = place_o(box((0, 0, 0), (.52, .006, .78)), (.085, .592, 0), (0, 0, math.radians(-37)))
    finish(glass, (.06, .09, .14), .06, .1, 0, flag=F_GLASS)
    # horizontal insulated boiler drum + saddles + copper plumbing
    tank = _rz([(-.84, .01), (-.82, .21), (.82, .21), (.84, .01)], -.48, 1.32, 16, 'stank')
    finish(tank, (.5, .5, .5), .40, .2, 0, flag=F_PAINT)
    for ez in (-.83, .83):
        finish(_rz([(ez - .015, .215), (ez + .015, .215)], -.48, 1.32, 16), (.5, .5, .5), .5, .2, 0, flag=F_PAINT2)
    for sz in (-.62, .62):
        finish(_rz([(sz - .025, .216), (sz + .025, .216)], -.48, 1.32, 16), FRAME, .45, .6, 0)
    finish(tube((-.34, 1.15, -.40), (-.28, .96, -.40), .016, 6), COPPER, .35, .85, 0)
    finish(tube((-.46, 1.12, .45), (.44, .22, .45), .014, 6), COPPER, .35, .85, 0)
    finish(_ry(1.50, 1.66, -.48, .62, .014, 6, 'vent'), COPPER, .35, .85, 0)

# ---------------------------------------------------------------- the IBC tote
def ibc_tote():
    """A 1000 L intermediate bulk container: a milky HDPE bottle in a galvanised
    tube cage on a steel pallet, the filler cap on top, the ball valve and its
    spout at the front foot, a label plate. x = the valve side (out from a wall)."""
    clear()
    GAL = (.58, .60, .62); GD = (.40, .41, .43)
    HX, HZ = .58, .48
    finish(box((0, .07, 0), (HX + .02, .07, HZ + .02)), GD, .55, .6, 0)                      # pallet
    for zz in (-HZ + .06, 0, HZ - .06):
        finish(box((0, .035, zz), (HX + .02, .035, .05)), (.30, .31, .33), .6, .5, 0)
    bottle = _bev(box((0, .62, 0), (HX - .04, .47, HZ - .04)), .08, 4)
    finish(bottle, (.5, .5, .5), .55, 0, 0, flag=F_PAINT)
    finish(_ry(1.09, 1.15, 0, 0, .15, 20), (.5, .5, .5), .5, 0, 0, flag=F_PAINT2)           # cap
    # the cage: tubes round each face, verticals and horizontals
    for sz in (-1, 1):
        for xx in (-HX, -HX/3, HX/3, HX):
            finish(_ry(.14, 1.12, xx, sz * HZ, .012, 6), GAL, .4, .75, 0)
        for xx in (-1, 1):
            for zz in (-HZ/3, HZ/3):
                finish(_ry(.14, 1.12, xx * HX, zz, .012, 6), GAL, .4, .75, 0)
    for y in (.15, .48, .81, 1.12):
        for sz in (-1, 1):
            finish(_rx(-HX, HX, y, sz * HZ, .012, 6), GAL, .4, .75, 0)
            finish(prof([(-HZ, y - .012), (HZ, y - .012), (HZ, y + .012), (-HZ, y + .012)],
                        sz * HX - .012, sz * HX + .012), GAL, .4, .75, 0)
    for xx in (-HX/3, HX/3):
        finish(prof([(-HZ, 1.108), (HZ, 1.108), (HZ, 1.132), (-HZ, 1.132)], xx - .012, xx + .012), GAL, .4, .75, 0)
    # valve and spout
    finish(_rx(HX - .06, HX + .06, .24, 0, .055, 14), (.12, .12, .13), .6, 0, 0)
    finish(_rx(HX + .06, HX + .14, .24, 0, .032, 12), (.12, .12, .13), .6, 0, 0)
    finish(box((HX + .07, .32, 0), (.012, .05, .02)), (.85, .55, .08), .5, 0, 0)
    finish(box((HX - .035, .80, -.20), (.002, .10, .14)), (.92, .92, .90), .4, 0, 0)        # label
    finish(box((HX - .033, .84, -.20), (.002, .03, .10)), (.75, .12, .08), .4, 0, 0)

# ---------------------------------------------------------------- a pallet of sacks
def sack_pallet():
    """A timber pallet stacked five courses high with filled sacks, each course
    turned against the one below, a bag or two out of line, a strap round it."""
    clear()
    W = (.52, .40, .27)
    for zz in (-.45, 0, .45):                                                         # bearers
        finish(box((0, .05, zz), (.60, .05, .05)), tuple(c * .8 for c in W), .9, 0, 0)
    for k in range(7):
        finish(box((-.54 + k * .18, .118, 0), (.065, .011, .50)), W, .9, 0, 0)
    sack = 0
    for c in range(5):
        y = .13 + .135 * c + .067
        if c % 2 == 0:
            cells = [(-.30, z) for z in (-.33, 0, .33)] + [(.30, z) for z in (-.33, 0, .33)]
            hx, hz = .29, .16
        else:
            cells = [(x, -.25) for x in (-.40, 0, .40)] + [(x, .25) for x in (-.40, 0, .40)]
            hx, hz = .19, .24
        for (x, z) in cells:
            if c == 4 and (x, z) in ((.30, .33), (.40, .25)): continue
            sack += 1
            j = .012 * ((sack * 7) % 5 - 2)
            b = _bev(box((0, 0, 0), (hx - .012, .062, hz - .012)), .045, 3)
            place_o(b, (x + j, y, z - j), (0, math.radians(((sack * 13) % 7 - 3) * 1.2), 0))
            finish(b, (.5, .5, .5) if sack % 4 else (.46, .46, .46), .95, 0, 0, flag=F_PAINT)
            if sack % 3 == 0:                                                         # a stencil band
                finish(box((x + j, y + .063, z - j), (hx * .6, .002, .04)), (.30, .18, .12), .9, 0, 0)
    for zz in (-.22, .22):                                                            # straps
        finish(box((0, .47, zz), (.62, .34, .008)), (.10, .22, .48), .5, 0, 0)

# ---------------------------------------------------------------- a street bench
def bench():
    """A park bench: two cast-iron end frames, five timber slats for the seat
    and three for the back, bolts, set 1.8 m long. x = out from the wall."""
    clear()
    IRON = (.10, .11, .11); WOOD = (.46, .30, .17)
    for sz in (-.80, .80):
        finish(prof_front([(.26, 0), (.32, 0), (.28, .44), (.22, .46), (-.18, .46), (-.24, .44),
                           (-.28, 0), (-.22, 0), (-.20, .40), (.22, .40)], sz - .025, sz + .025, 'leg'),
               IRON, .6, .4, 0)
        finish(prof_front([(-.22, .44), (-.17, .44), (-.25, .90), (-.30, .88)], sz - .025, sz + .025, 'back'),
               IRON, .6, .4, 0)
        finish(prof_front([(.18, .46), (.26, .62), (.10, .62), (.08, .58), (.18, .58)], sz - .02, sz + .02, 'arm'),
               IRON, .6, .4, 0)
    for k in range(5):
        x = -.18 + k * .085
        finish(_bev(box((x, .475, 0), (.036, .016, .90)), .008, 2), WOOD, .8, 0, 0, flag=F_PAINT)
    for k in range(3):
        y = .58 + k * .11
        b = _bev(box((0, 0, 0), (.016, .042, .90)), .008, 2)
        finish(place_o(b, (-.21 - (y - .44) * .17, y, 0), (0, 0, math.radians(-10))), WOOD, .8, 0, 0, flag=F_PAINT)

# ---------------------------------------------------------------- an electrical cabinet
def elec_cabinet():
    """A street electrical distribution cabinet on a concrete plinth: two
    louvred doors, hinges and a lock, a hazard sign, a rain hood, a conduit
    running down into the ground. x = the door side (out from a wall)."""
    clear()
    HX, HZ = .22, .60
    finish(box((0, .06, 0), (HX + .06, .06, HZ + .06)), (.62, .61, .58), .9, 0, 0)             # plinth
    body = _bev(box((0, .80, 0), (HX, .68, HZ)), .015, 2)
    finish(body, (.5, .5, .5), .5, .3, 0, flag=F_PAINT)
    finish(_bev(box((0, 1.50, 0), (HX + .04, .025, HZ + .04)), .01, 2), (.5, .5, .5), .5, .3, 0, flag=F_PAINT)
    for dz in (-.30, .30):
        finish(box((HX + .004, .80, dz), (.004, .62, .28)), (.5, .5, .5), .45, .3, 0, flag=F_PAINT2)
        for k in range(6):
            finish(box((HX + .010, .30 + k * .04, dz), (.004, .008, .20)), (.12, .12, .13), .6, 0, 0)
        for hy in (.40, 1.20):
            finish(box((HX + .010, hy, dz + (.27 if dz > 0 else -.27)), (.008, .03, .012)),
                   (.20, .20, .21), .5, .6, 0)
    finish(box((HX + .012, .95, 0), (.006, .04, .012)), (.70, .70, .72), .3, .8, 0)          # lock
    sign = prof([(-.08, 1.12), (.08, 1.12), (0, 1.26)], HX + .008, HX + .012, 'sign')
    finish(sign, (.95, .78, .10), .4, 0, 0)
    finish(prof([(-.035, 1.145), (.035, 1.145), (0, 1.21)], HX + .012, HX + .014, 'bolt'), (.05, .05, .05), .4, 0, 0)
    finish(_ry(0, .70, -HX + .06, HZ - .10, .035, 10), (.30, .31, .33), .5, .5, 0)            # conduit
    finish(_ry(.12, .16, -HX + .06, HZ - .10, .05, 10), (.30, .31, .33), .5, .5, 0)

# ---------------------------------------------------------------- supply stations
# Prop frame as everywhere here: x OUT from the wall the station stands
# against, y up, z along the wall. The station mesh is the furniture; the
# *_ITEM mesh is what you take, drawn only while the station is stocked.
GREEN_X = (.05, .52, .24); WHITE_P = (.88, .89, .87); STEELG = (.36, .37, .39)
def _cross(x, y, z, s, t, col, axis='x'):
    """A raised cross (two bars) on a face looking along +x, centred (y, z)."""
    finish(box((x, y, z), (t, s * .30, s)), col, .45, 0, 0)
    finish(box((x, y, z), (t, s, s * .30)), col, .45, 0, 0)

def supply_med():
    """A MEDICAL STATION: a concrete plinth, two square steel posts carrying a
    white powder-coated cabinet with two shelves, its door swung open showing a
    green cross, and above it the sign — a green panel with a white cross in a
    white border — with a lamp housing on top (the lamp itself is drawn live)."""
    clear()
    finish(_bev(box((0, .04, 0), (.30, .04, .46)), .012, 2), (.62, .61, .58), .9, 0, 0)        # plinth
    for pz in (-.40, .40):
        finish(box((-.24, 1.16, pz), (.025, 1.08, .025)), STEELG, .45, .6, 0)              # posts to the sign
        finish(box((-.24, .085, pz), (.06, .006, .06)), STEELG, .5, .5, 0)                 # base plates
    # the cabinet: an open box (back, sides, top, bottom) and two shelves
    W, H0, H1 = .40, .58, 1.52
    finish(box((-.205, (H0 + H1) / 2, 0), (.015, (H1 - H0) / 2, W)), WHITE_P, .4, .1, 0)     # back
    for sz in (-1, 1):
        finish(box((-.06, (H0 + H1) / 2, sz * (W - .012)), (.16, (H1 - H0) / 2, .012)), WHITE_P, .4, .1, 0)
    for y in (H0, H1):
        finish(box((-.06, y, 0), (.16, .014, W)), WHITE_P, .4, .1, 0)
    finish(box((-.07, .90, 0), (.13, .008, W - .014)), (.75, .76, .74), .45, .2, 0)        # shelves
    finish(box((-.07, 1.20, 0), (.13, .008, W - .014)), (.75, .76, .74), .45, .2, 0)
    for y in (H0 + .01, H1 - .01):                                                         # front frame
        finish(box((.10, y, 0), (.008, .018, W)), (.82, .83, .80), .4, .1, 0)
    for sz in (-1, 1):
        finish(box((.10, (H0 + H1) / 2, sz * (W - .016)), (.008, (H1 - H0) / 2, .018)), (.82, .83, .80), .4, .1, 0)
    # the door, swung open 110 degrees on its hinge at +z, a green cross on it
    door = box((0, 0, 0), (.012, .46, .19))
    finish(place_o(door, (.04, 1.05, W + .17), (0, math.radians(-70), 0)), WHITE_P, .4, .1, 0)
    gx = place_o(box((0, 0, 0), (.008, .09, .028)), (.105, 1.10, W + .135), (0, math.radians(-70), 0))
    finish(gx, GREEN_X, .45, 0, 0)
    gy = place_o(box((0, 0, 0), (.008, .028, .09)), (.105, 1.10, W + .135), (0, math.radians(-70), 0))
    finish(gy, GREEN_X, .45, 0, 0)
    finish(box((.10, .66, -W + .05), (.012, .03, .012)), STEELG, .4, .6, 0)                # catch
    # a cross on each side panel too: readable from along the wall
    for sz in (-1, 1):
        finish(box((-.06, 1.30, sz * (W + .002)), (.075, .022, .002)), GREEN_X, .45, 0, 0)
        finish(box((-.06, 1.30, sz * (W + .002)), (.022, .075, .002)), GREEN_X, .45, 0, 0)
    # THE SIGN: a green panel between the posts, white border, white cross
    finish(_bev(box((-.215, 1.97, 0), (.018, .21, .38)), .006, 1), WHITE_P, .4, 0, 0)
    finish(box((-.195, 1.97, 0), (.004, .18, .35)), GREEN_X, .4, 0, 0)
    finish(box((-.188, 1.97, 0), (.004, .13, .042)), WHITE_P, .4, 0, 0)
    finish(box((-.188, 1.97, 0), (.004, .042, .13)), WHITE_P, .4, 0, 0)
    finish(box((-.215, 2.20, 0), (.05, .02, .07)), (.20, .21, .22), .5, .5, 0)            # lamp housing

def supply_med_item():
    """What the medical station holds: a red trauma bag on the low shelf and a
    white first-aid box with a green cross on the upper one."""
    clear()
    bag = _bev(box((-.07, .99, -.08), (.12, .085, .22)), .035, 3)
    finish(bag, (.62, .08, .07), .8, 0, 0)
    finish(box((.054, 1.00, -.08), (.004, .045, .014)), WHITE_P, .5, 0, 0)
    finish(box((.054, 1.00, -.08), (.004, .014, .045)), WHITE_P, .5, 0, 0)
    finish(_rx(-.16, .02, 1.085, -.08, .012, 6), (.12, .12, .12), .6, 0, 0)                # handle
    bx_ = _bev(box((-.08, 1.27, .10), (.10, .065, .16)), .012, 2)
    finish(bx_, WHITE_P, .4, 0, 0)
    finish(box((.024, 1.27, .10), (.004, .040, .012)), GREEN_X, .45, 0, 0)
    finish(box((.024, 1.27, .10), (.004, .012, .040)), GREEN_X, .45, 0, 0)
    finish(box((-.08, 1.21, -.20), (.08, .008, .06)), (.85, .82, .70), .7, 0, 0)            # a roll of bandage
    finish(_rx(-.13, -.03, 1.245, -.24, .035, 12), (.92, .91, .86), .8, 0, 0)

OLIVE_C = (.29, .32, .20); YEL_S = (.86, .66, .10)
def supply_ammo():
    """AN AMMUNITION CRATE: an olive wooden crate on skids, steel corner caps,
    rope handles at the ends, a yellow stencil band, a dark foam insert and its
    lid propped open against the wall; beside it a post carrying a yellow and
    black hazard-bordered sign with a grenade silhouette and a lamp on top."""
    clear()
    L, D = .58, .28
    for sz in (-.45, .45):
        finish(box((0, .03, sz), (D + .02, .03, .045)), (.30, .24, .16), .9, 0, 0)          # skids
    body = _bev(box((0, .34, 0), (D, .28, L)), .012, 2)
    finish(body, OLIVE_C, .75, 0, 0)
    for k in range(4):                                                                     # plank seams
        finish(box((D + .002, .14 + k * .14, 0), (.002, .004, L - .01)), (.18, .20, .12), .9, 0, 0)
    for sx in (-1, 1):
        for sz in (-1, 1):
            finish(box((sx * (D - .02), .34, sz * (L - .02)), (.03, .29, .03)), STEELG, .45, .6, 0)
    finish(box((D + .004, .45, 0), (.003, .05, L - .06)), YEL_S, .55, 0, 0)                 # stencil band
    for k in range(5):                                                                     # stencil marks
        finish(box((D + .007, .33, -.30 + k * .15), (.002, .02, .045)), (.08, .08, .07), .6, 0, 0)
    for sz in (-1, 1):                                                                     # rope handles
        finish(tube((-.12, .46, sz * (L + .03)), (.12, .46, sz * (L + .03)), .012, 6), (.62, .55, .40), .9, 0, 0)
        finish(box((-.12, .46, sz * (L + .008)), (.02, .03, .02)), STEELG, .5, .5, 0)
        finish(box((.12, .46, sz * (L + .008)), (.02, .03, .02)), STEELG, .5, .5, 0)
    finish(box((0, .605, 0), (D - .03, .012, L - .03)), (.07, .07, .07), .95, 0, 0)        # foam
    # the lid, hinged on the back edge and leaning back on the wall
    lid = _bev(box((0, 0, 0), (.025, .29, L)), .01, 2)
    finish(place_o(lid, (-D - .045, .88, 0), (0, 0, math.radians(8))), OLIVE_C, .75, 0, 0)
    finish(place_o(box((0, 0, 0), (.003, .05, L - .08)), (-D - .015, .90, 0), (0, 0, math.radians(8))), YEL_S, .55, 0, 0)
    # THE SIGN on its post at the crate's end
    finish(box((-.20, 1.05, -L - .10), (.03, 1.05, .03)), STEELG, .45, .6, 0)
    finish(box((-.20, .02, -L - .10), (.08, .02, .08)), STEELG, .5, .5, 0)
    sz0 = -L - .10
    finish(_bev(box((-.16, 1.78, sz0), (.012, .24, .26)), .006, 1), (.06, .06, .06), .5, 0, 0)     # black border
    finish(box((-.145, 1.78, sz0), (.004, .21, .23)), YEL_S, .5, 0, 0)
    for k in range(6):                                                                     # hazard teeth
        finish(place_o(box((0, 0, 0), (.004, .012, .05)), (-.141, 1.585, sz0 - .19 + k * .076), (math.radians(45), 0, 0)),
               (.06, .06, .06), .5, 0, 0)
    # the grenade: body, fuze, lever, ring — in black on the yellow
    g = _bev(box((-.138, 1.77, sz0), (.004, .085, .065)), .003, 1)
    finish(g, (.06, .06, .06), .5, 0, 0)
    finish(box((-.138, 1.875, sz0), (.004, .022, .028)), (.06, .06, .06), .5, 0, 0)
    finish(place_o(box((0, 0, 0), (.004, .07, .012)), (-.138, 1.85, sz0 + .05), (math.radians(-25), 0, 0)), (.06, .06, .06), .5, 0, 0)
    ring = _rz([(-.003, .022), (.003, .022), (.003, .015), (-.003, .015)], 0, 0, 14, 'ring', loop=True)
    finish(place_o(ring, (-.138, 1.90, sz0 - .045), (0, math.radians(90), 0)), (.06, .06, .06), .5, 0, 0)
    finish(box((-.20, 2.12, sz0), (.05, .02, .05)), (.20, .21, .22), .5, .5, 0)            # lamp housing

def supply_ammo_item():
    """What the crate holds: three frags, three smoke cans and three incendiary
    cans standing in the foam, in rows across it."""
    clear()
    for i in range(3):
        z = -.40 + i * .13
        e = _bev(box((0, .66, z), (.030, .045, .030)), .026, 3)                            # frag egg
        finish(e, (.24, .26, .175), .62, 0, 0)
        finish(_ry(.70, .73, 0, z, .013, 10), GRAPH if 'GRAPH' in globals() else (.18, .19, .2), .4, .7, 0)
        finish(box((.016, .70, z), (.004, .022, .008)), (.38, .38, .37), .5, .6, 0)
    for i in range(3):
        z = -.03 + i * .11
        finish(_ry(.61, .73, 0, z, .028, 14), (.40, .43, .46), .6, 0, 0)                   # smoke can
        finish(_ry(.655, .675, 0, z, .0285, 14), (.88, .88, .86), .6, 0, 0)
        finish(_ry(.73, .755, 0, z, .013, 10), (.30, .30, .32), .4, .7, 0)
    for i in range(3):
        z = .30 + i * .11
        finish(_ry(.61, .73, 0, z, .028, 14), (.30, .305, .285), .6, 0, 0)                 # incendiary can
        finish(_ry(.645, .665, 0, z, .0285, 14), (.66, .11, .07), .6, 0, 0)
        finish(_ry(.73, .755, 0, z, .013, 10), (.30, .30, .32), .4, .7, 0)

# ---------------------------------------------------------------- lofted shells
def superloft(secs, n=32, name='sloft', subd=1, cap=True):
    """A smooth shell through super-ellipse sections along x. secs: (x, yc, hw,
    hh, e) — centre height, half width (z), half height (y), squareness (2 =
    ellipse, 4+ = boxy). One subdivision pass, ends capped."""
    bm = bmesh.new(); rings = []
    for x, yc, hw, hh, e in secs:
        ring = []
        for i in range(n):
            t = 2 * math.pi * i / n
            c, s_ = cos(t), sin(t)
            z = hw * (abs(c) ** (2 / e)) * (1 if c >= 0 else -1)
            y = hh * (abs(s_) ** (2 / e)) * (1 if s_ >= 0 else -1)
            ring.append(bm.verts.new((x, yc + y, z)))
        rings.append(ring)
    for k in range(len(rings) - 1):
        for i in range(n):
            j = (i + 1) % n
            bm.faces.new((rings[k][i], rings[k][j], rings[k + 1][j], rings[k + 1][i]))
    if cap:
        bm.faces.new(rings[0][::-1]); bm.faces.new(rings[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    o = from_bm(bm, name)
    if subd:
        m = o.modifiers.new('sd', 'SUBSURF'); m.levels = subd
        bpy.context.view_layer.objects.active = o; o.select_set(True)
        bpy.ops.object.modifier_apply(modifier=m.name); o.select_set(False)
    return o

def _glass_patch(bvh, org_fn, nu, nv, lift=.006, col=(.10, .14, .17)):
    finish(patch(bvh, org_fn, nu, nv, lift), col, .04, 0, 0, flag=F_GLASS)

# ---------------------------------------------------------------- the helicopter
def _tb(secs):
    """(x, top, bottom, half width, squareness) -> superloft sections."""
    return [(x, (t + b_) / 2, hw, (t - b_) / 2, e) for x, t, b_, hw, e in secs]

def _region(secs, grow, lo, hi, name='region', subd=1):
    """The part of a shell `grow` metres proud of the fuselage that lies inside
    the box lo..hi: a clean glass pane (or panel) that follows the skin."""
    g = [(x, t + grow, b_ - grow, hw + grow, e) for x, t, b_, hw, e in secs]
    o = superloft(_tb(g), 40, name, subd)
    c = box(((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2),
            ((hi[0] - lo[0]) / 2, (hi[1] - lo[1]) / 2, (hi[2] - lo[2]) / 2))
    m = o.modifiers.new('isect', 'BOOLEAN'); m.operation = 'INTERSECT'; m.object = c; m.solver = 'EXACT'
    bpy.context.view_layer.objects.active = o; o.select_set(True)
    bpy.ops.object.modifier_apply(modifier=m.name); o.select_set(False)
    bpy.data.objects.remove(c)
    return o

HELI_SECS = [  # x, top, bottom, half width, squareness — nose to tail
    (4.66, 1.30, 1.02, .20, 2.2), (4.55, 1.52, .90, .50, 2.5), (4.36, 1.80, .82, .76, 2.9),
    (4.08, 2.08, .76, .95, 3.3), (3.70, 2.34, .72, 1.08, 3.9), (3.20, 2.50, .70, 1.16, 4.6),
    (2.60, 2.55, .70, 1.18, 5.2), (0.00, 2.56, .70, 1.19, 5.4), (-1.90, 2.56, .72, 1.18, 5.2),
    (-2.50, 2.53, .94, 1.02, 4.4), (-3.10, 2.46, 1.38, .64, 3.6), (-3.80, 2.40, 1.80, .36, 3.0),
    (-5.00, 2.37, 2.04, .23, 2.6), (-8.00, 2.37, 2.12, .16, 2.4), (-10.50, 2.38, 2.17, .12, 2.3),
    (-10.78, 2.38, 2.19, .10, 2.2)]

def helicopter():
    """A utility helicopter in the instance's paint: a boxy cabin on a sloped,
    glazed nose — two framed windscreen panes, chin windows, green-tinted roof
    windows and cockpit door windows, all cut cleanly from a shell over the
    skin so the paint between them reads as the frames — the cabin's sliding
    door run open on the right (a dark cabin, seats and a bulkhead behind it),
    closed with its window on the left; an engine deck with its intake, top
    grille, two exhausts and the mast; wire cutters over and under the nose, a
    searchlight, panel lines, a tapering boom with a stabiliser, the fin and the
    tail rotor gearbox, a tail skid, skids on bowed cross tubes with steps,
    aerials and a pitot. The rotor BLADES turn, so they are drawn live."""
    clear()
    DK = (.06, .065, .06); STL = (.24, .25, .26); IN = (.03, .032, .03); SEAT = (.16, .15, .12)
    GL = (.16, .23, .27); GLG = (.16, .27, .20)
    body = superloft(_tb(HELI_SECS), 44, 'heli_body', 1)
    finish(body, (.5, .5, .5), .58, .05, 0, flag=F_PAINT)
    bvh = _bvh_of([body])
    # GLAZING: panes cut from a 6 mm shell, separated by 6-8 cm of paint, each
    # in a black rubber seal 3.5 cm wider than the pane, 4 mm proud
    SEAL = (.025, .025, .028)
    def seal(lo, hi, name, side_axis):
        e = .035
        lo2 = [lo[0] - e, lo[1] - e, lo[2]]; hi2 = [hi[0] + e, hi[1] + e, hi[2]]
        if not side_axis:   # a nose pane grows across z too; a side pane only in x and y
            lo2[2] -= e; hi2[2] += e
        finish(_region(HELI_SECS, .0035, tuple(lo2), tuple(hi2), name), SEAL, .7, 0, 0)
    for sgn in (-1, 1):
        z0, z1 = sorted((sgn * .045, sgn * 1.00)); seal((3.05, 1.62, z0), (4.62, 2.52, z1), 'ws_s', 0)
        z0, z1 = sorted((sgn * .16, sgn * .78)); seal((3.95, .90, z0), (4.70, 1.50, z1), 'chin_s', 0)
        z0, z1 = sorted((sgn * 1.0, sgn * 1.4))
        seal((2.12, 1.64, z0), (2.95, 2.40, z1), 'cd_s', 1)
        seal((2.20, 1.00, z0), (2.86, 1.42, z1), 'cdl_s', 1)
        seal((-2.35, 1.70, z0), (-1.90, 2.25, z1), 'aft_s', 1)
    seal((2.80, 2.40, -.52), (3.50, 2.70, -.06), 'rw_s', 0)
    seal((2.80, 2.40, .06), (3.50, 2.70, .52), 'rw2_s', 0)
    # the anti-glare panel under the windscreen and a darker belly
    finish(_region(HELI_SECS, .0025, (-2.6, .55, -1.3), (4.2, .80, 1.3), 'belly'), (.20, .22, .17), .65, .05, 0)
    for sgn in (-1, 1):
        z0, z1 = sorted((sgn * .045, sgn * 1.00))
        finish(_region(HELI_SECS, .006, (3.05, 1.62, z0), (4.62, 2.52, z1), 'ws'), GL, .03, .1, 0, flag=F_GLASS)
        z0, z1 = sorted((sgn * .16, sgn * .78))
        finish(_region(HELI_SECS, .006, (3.95, .90, z0), (4.70, 1.50, z1), 'chin'), GL, .03, .1, 0, flag=F_GLASS)
        z0, z1 = sorted((sgn * 1.0, sgn * 1.4))
        finish(_region(HELI_SECS, .006, (2.12, 1.64, z0), (2.95, 2.40, z1), 'cdoor'), GL, .03, .1, 0, flag=F_GLASS)
        finish(_region(HELI_SECS, .006, (2.20, 1.00, z0), (2.86, 1.42, z1), 'cdoorlo'), GL, .03, .1, 0, flag=F_GLASS)
        finish(_region(HELI_SECS, .006, (-2.35, 1.70, z0), (-1.90, 2.25, z1), 'aftwin'), GL, .03, .1, 0, flag=F_GLASS)
    finish(_region(HELI_SECS, .006, (2.80, 2.40, -.52), (3.50, 2.70, -.06), 'roofw'), GLG, .03, .1, 0, flag=F_GLASS)
    finish(_region(HELI_SECS, .006, (2.80, 2.40, .06), (3.50, 2.70, .52), 'roofw2'), GLG, .03, .1, 0, flag=F_GLASS)
    # THE CABIN, door open on the right: a dark interior panel set into the
    # side, the door run back over the boom root, seats and a bulkhead inside
    finish(_region(HELI_SECS, .004, (-1.45, .86, 1.0), (.95, 2.36, 1.4), 'cabin_in'), IN, .95, 0, 0)
    for k in range(3):                                                       # troop seats
        finish(box((-1.05 + k * .62, 1.15, 1.10), (.24, .04, .06)), SEAT, .9, 0, 0)
        finish(box((-1.05 + k * .62, 1.45, 1.13), (.24, .26, .02)), SEAT, .9, 0, 0)
    finish(_region(HELI_SECS, .045, (-2.95, .86, 1.0), (-1.50, 2.36, 1.5), 'door_open'), (.5, .5, .5), .58, .05, 0, flag=F_PAINT)
    finish(_region(HELI_SECS, .052, (-2.55, 1.66, 1.0), (-1.95, 2.18, 1.5), 'door_open_w'), GL, .03, .1, 0, flag=F_GLASS)
    finish(_region(HELI_SECS, .012, (-1.05, 1.66, -1.5), (-.35, 2.18, -1.0), 'door_l_w'), GL, .03, .1, 0, flag=F_GLASS)
    for sgn in (-1, 1):
        finish(box((-.75, 2.36, sgn * 1.105), (2.30, .022, .016)), DK, .5, .5, 0)                 # door rails (on the skin)
        finish(box((-.75, .88, sgn * 1.105), (2.30, .016, .016)), DK, .5, .5, 0)
    # panel lines: cockpit door edges, the closed cabin door, boom joints
    for sgn in (-1, 1):
        for xl in (2.05, 3.00):
            finish(ribbon(bvh, lambda t, xl=xl, sgn=sgn: ((xl, .82 + 1.62 * t, sgn * 3), (0, 0, -sgn)), 12, .012, .0012), DK, .7, 0, 0)
        if sgn < 0:
            for xl in (-1.45, .95):
                finish(ribbon(bvh, lambda t, xl=xl: ((xl, .86 + 1.50 * t, -3), (0, 0, 1)), 12, .012, .0012), DK, .7, 0, 0)
        for xl in (-4.6, -7.4):
            finish(ribbon(bvh, lambda t, xl=xl, sgn=sgn: ((xl, 2.05 + .32 * t, sgn * 3), (0, 0, -sgn)), 6, .010, .001), DK, .7, 0, 0)
        finish(box((2.40, 1.52, sgn * 1.19), (.07, .018, .012)), STL, .4, .7, 0)                  # handles
        finish(box((-.30, 1.55, -1.195), (.08, .018, .012)), STL, .4, .7, 0)
    # ENGINE DECK: the cowl, intake, top grille, exhausts; transmission and mast
    cowl = superloft([(1.95, 2.62, .30, .06, 3.0), (1.55, 2.78, .62, .24, 4.0), (.40, 2.84, .66, .30, 5.0),
                      (-1.60, 2.84, .64, .30, 5.0), (-2.55, 2.76, .52, .22, 4.0), (-3.05, 2.62, .26, .06, 3.0)],
                     32, 'cowl', 1)
    finish(cowl, (.47, .47, .47), .55, .05, 0, flag=F_PAINT)
    finish(box((1.54, 2.80, 0), (.06, .10, .40)), (.03, .03, .03), .9, .2, 0)                    # intake
    for k in range(6):
        finish(box((-1.40 + k * .26, 3.13, 0), (.07, .012, .34)), (.05, .05, .05), .8, .2, 0)      # top grille
    for sgn in (-1, 1):
        finish(tube((-2.55, 2.92, sgn * .26), (-3.25, 3.16, sgn * .40), .12, 16), (.10, .09, .08), .8, .3, 0)
        finish(tube((-3.24, 3.155, sgn * .395), (-3.27, 3.165, sgn * .40), .10, 16), (.02, .02, .02), .9, 0, 0)
    finish(_ry(3.10, 3.52, 0, 0, .13, 18), STL, .35, .8, 0)
    finish(_ry(3.22, 3.28, 0, 0, .34, 22), STL, .35, .8, 0)
    finish(_ry(3.46, 3.64, 0, 0, .22, 18), DK, .4, .6, 0)
    for k in range(2):
        a_ = k * math.pi / 2 + .3
        finish(tube((.32 * cos(a_), 3.24, .32 * sin(a_)), (.19 * cos(a_), 3.50, .19 * sin(a_)), .024, 6), STL, .4, .7, 0)
    # wire cutters, the searchlight, aerials, pitot
    finish(_ry(.66, .76, 3.55, .30, .10, 16), DK, .4, .5, 0)                                 # searchlight, on the belly
    finish(_ry(.62, .66, 3.55, .30, .085, 16), (.90, .90, .84), .1, .3, 0)
    finish(prof_front([(-1.70, 2.84), (-1.25, 2.84), (-1.45, 3.22), (-1.62, 3.22)], -.012, .012, 'aer'), DK, .5, .4, 0)
    finish(prof_front([(-.70, .72), (-.45, .72), (-.55, .40), (-.66, .40)], -.012, .012, 'aer2'), DK, .5, .4, 0)
    # THE TAIL: fin, stabiliser with end plates, gearbox, tail skid, a band
    fin = prof_front([(-10.05, 2.30), (-10.70, 2.26), (-11.15, 4.40), (-10.92, 4.46), (-10.32, 3.00)], -.06, .06, 'fin')
    finish(_bev(fin, .025, 2), (.5, .5, .5), .58, .05, 0, flag=F_PAINT)
    stab = prof([(-1.45, 2.22), (1.45, 2.22), (1.45, 2.28), (-1.45, 2.28)], -7.95, -7.35, 'stab')
    finish(_bev(stab, .02, 2), (.5, .5, .5), .58, .05, 0, flag=F_PAINT)
    finish(_rz([(-.30, .02), (-.24, .15), (-.10, .16), (-.06, .10)], -10.72, 3.42, 16, 'tgb'), STL, .45, .6, 0)
    finish(_rz([(-.36, .02), (-.30, .06), (-.24, .06)], -10.72, 3.42, 12), DK, .4, .6, 0)
    finish(tube((-10.30, 2.18, 0), (-10.95, 1.62, 0), .025, 8), STL, .4, .7, 0)                    # tail skid
    finish(_region(HELI_SECS, .004, (-9.95, 1.9, -.5), (-9.65, 2.6, .5), 'band', 0), (.80, .74, .20), .5, 0, 0)
    finish(_region(HELI_SECS, .004, (-10.55, 1.9, -.5), (-10.25, 2.6, .5), 'band2', 0), (.85, .85, .82), .5, 0, 0)
    # SKIDS: tubes with upturned toes on two bowed cross tubes, steps
    for sgn in (-1, 1):
        bs = sgn * 1.30
        finish(tube((-2.75, .07, bs), (2.60, .07, bs), .052, 14), STL, .4, .7, 0)
        finish(tube((2.60, .07, bs), (3.05, .32, bs), .05, 14), STL, .4, .7, 0)
        finish(tube((-2.75, .07, bs), (-2.88, .15, bs), .05, 14), STL, .4, .7, 0)
        for a_ in (-1.40, 1.60):
            finish(tube((a_, .07, bs), (a_, .52, sgn * 1.20), .055, 12), STL, .4, .7, 0)
            finish(tube((a_, .52, sgn * 1.20), (a_, .70, sgn * .72), .055, 12), STL, .4, .7, 0)
        finish(box((1.60, .30, sgn * 1.27), (.24, .015, .07)), STL, .5, .6, 0)                    # step, on the cross tube
    # grab handles by the doors, roof handrails, the cargo hook, rivet lines,
    # warning stripes on the tail rotor gearbox
    for sgn in (-1, 1):
        finish(tube((1.98, 1.30, sgn * 1.19), (1.98, 2.00, sgn * 1.19), .016, 8), STL, .35, .8, 0)
        finish(tube((-1.55, 1.10, sgn * 1.175), (-1.55, 2.10, sgn * 1.175), .016, 8), STL, .35, .8, 0)
        for xr in (-3.8, -5.6, -7.0, -8.6, -9.4):
            finish(ribbon(bvh, lambda t, xr=xr, sgn=sgn: ((xr + .9 * t, 2.30, sgn * 3), (0, 0, -sgn)), 8, .006, .001), (.20, .21, .17), .7, 0, 0)
    finish(box((.0, .66, 0), (.10, .04, .08)), STL, .4, .7, 0)
    finish(tube((0, .62, 0), (.06, .52, 0), .012, 6), STL, .4, .7, 0)
    for k in range(4):
        finish(box((-10.82 + k * .07, 3.60, -.16), (.03, .015, .06)), (.80, .10, .08) if k % 2 == 0 else (.92, .92, .90), .5, 0, 0)
    # light housings (the lamps themselves are drawn lit)
    for sgn in (-1, 1):
        finish(box((.87, 2.45, sgn * 1.05), (.09, .04, .03)), DK, .5, .4, 0)              # nav light housings, on the skin

# ---------------------------------------------------------------- the truck
def truck_cab():
    """A cab-over container truck without its box (the container is world
    geometry): a ladder frame and the flat bed with twist-lock corners, a
    rounded cab in the instance's paint with a raked windscreen, side windows,
    a grille of chrome bars, headlamps, indicators, a deep bumper with fog
    lamps and the plate, steps, grab handles, big mirrors on arms, a sun visor
    and a roof visor with amber marker lamps, wipers; front fenders and wheel
    arches; a chrome fuel tank, air tanks, battery box, side guards; dual rear
    wheels on a tandem; mud flaps and tail lamps; the exhaust stack with its
    heat shield, and air horns."""
    clear()
    STL = (.24, .25, .26); DK = (.07, .07, .075); CHR = (.72, .73, .75); YL = (.85, .70, .15)
    # chassis and bed
    for sgn in (-1, 1):
        finish(box((0, .85, sgn * .52), (3.6, .15, .06)), STL, .55, .6, 0)
    for a_ in (-3.2, -2.1, -1.0, .1, 1.2, 2.3):
        finish(box((a_, .86, 0), (.05, .08, .52)), STL, .55, .6, 0)
    finish(box((-.11, 1.12, 0), (3.49, .08, 1.10)), (.18, .19, .20), .6, .4, 0)
    for sgn in (-1, 1):
        finish(box((-.11, 1.08, sgn * 1.10), (3.49, .02, .006)), YL, .4, 0, 0)
        for a_ in (-3.55, 3.30):
            finish(box((a_, 1.22, sgn * 1.05), (.05, .03, .05)), (.55, .45, .10), .5, .5, 0)  # twist locks
    # THE CAB: a raked side profile across the width, edges rounded hard
    cab = prof_front([(3.42, .95), (5.30, .95), (5.38, 1.90), (5.30, 2.70), (5.12, 3.00), (3.42, 3.00)],
                     -1.15, 1.15, 'cab')
    _bev(cab, .14, 4)
    finish(cab, (.5, .5, .5), .32, .1, 0, flag=F_PAINT)
    bvh = _bvh_of([cab])
    # windscreen, two panes, and the side windows
    for sgn in (-1, 1):
        _glass_patch(bvh, lambda u, v, sgn=sgn: ((7.0, 1.98 + .74 * v, sgn * (.04 + .92 * u)), (-1, 0, 0)), 8, 8, .006)
        _glass_patch(bvh, lambda u, v, sgn=sgn: ((4.40 + .72 * u, 2.00 + .68 * v, sgn * 3.0), (0, 0, -sgn)), 6, 6, .006)
        finish(ribbon(bvh, lambda t, sgn=sgn: ((4.34, 1.0 + 1.9 * t, sgn * 3.0), (0, 0, -sgn)), 10, .012, .0015), DK, .7, 0, 0)
        finish(ribbon(bvh, lambda t, sgn=sgn: ((5.18, 1.0 + 1.7 * t, sgn * 3.0), (0, 0, -sgn)), 10, .012, .0015), DK, .7, 0, 0)
    finish(box((5.40, 2.35, 0), (.015, .40, .025)), DK, .6, 0, 0)                     # centre pillar
    # livery: a white stripe down each side and a dark lower skirt round the cab
    for sgn in (-1, 1):
        finish(ribbon(bvh, lambda t, sgn=sgn: ((3.50 + 1.75 * t, 1.62, sgn * 3.0), (0, 0, -sgn)), 16, .07, .003),
               (.90, .90, .86), .35, 0, 0)
        finish(patch(bvh, lambda u, v, sgn=sgn: ((3.48 + 1.80 * u, .98 + .26 * v, sgn * 3.0), (0, 0, -sgn)), 10, 3, .003),
               (.14, .15, .16), .5, .2, 0)
        finish(box((4.62, 1.80, sgn * 1.152), (.07, .012, .006)), CHR, .2, .9, 0)        # door handle
    # grille: chrome bars on black, badge; headlamps and indicators
    finish(box((5.39, 1.48, 0), (.02, .28, .74)), (.03, .03, .035), .7, .2, 0)
    for k in range(6):
        finish(box((5.41, 1.24 + k * .095, 0), (.012, .016, .72)), CHR, .2, .9, 0)
    finish(box((5.42, 1.84, 0), (.01, .03, .20)), CHR, .2, .95, 0)
    for sgn in (-1, 1):
        finish(_bev(box((5.36, 1.32, sgn * .92), (.04, .12, .16)), .02, 2), DK, .5, .3, 0)
        finish(box((5.40, 1.32, sgn * .92), (.01, .10, .14)), (.85, .86, .82), .08, .5, 0)
        finish(box((5.40, 1.50, sgn * .92), (.01, .035, .14)), LENS_A, .15, 0, 0)
    # bumper, fog lamps, plate, steps, grab handles
    finish(_bev(box((5.36, .80, 0), (.10, .18, 1.16)), .04, 2), (.16, .17, .18), .45, .4, 0)
    for sgn in (-1, 1):
        finish(_rx(5.44, 5.47, .80, sgn * .82, .06, 14), (.92, .90, .80), .1, .2, 0)
    finish(box((5.465, .76, 0), (.004, .07, .26)), (.92, .82, .20), .4, 0, 0)
    for sgn in (-1, 1):
        for y in (.74, 1.00):
            finish(box((4.70, y, sgn * 1.03), (.36, .02, .10)), STL, .5, .6, 0)
        finish(tube((4.36, 1.30, sgn * 1.17), (4.36, 2.10, sgn * 1.17), .018, 8), CHR, .2, .9, 0)
        # mirror on its arm
        finish(tube((5.15, 2.55, sgn * 1.13), (5.20, 2.55, sgn * 1.36), .02, 8), DK, .4, .6, 0)
        finish(tube((5.15, 2.05, sgn * 1.13), (5.20, 2.05, sgn * 1.36), .02, 8), DK, .4, .6, 0)
        finish(_bev(box((5.18, 2.25, sgn * 1.40), (.05, .32, .08)), .02, 2), DK, .5, .3, 0)
        finish(box((5.13, 2.25, sgn * 1.40), (.004, .29, .065)), (.55, .60, .65), .05, .9, 0)
    # roof visor with amber markers, sun visor, wipers
    finish(_bev(box((4.25, 3.12, 0), (.70, .13, .98)), .06, 3), (.45, .45, .45), .32, .1, 0, flag=F_PAINT)
    for k in range(-2, 3):
        finish(box((4.95, 3.12, k * .22), (.02, .025, .04)), LENS_A, .15, 0, 0)
    finish(box((5.25, 2.80, 0), (.12, .015, 1.0)), DK, .5, .2, 0)
    for z in (-.45, .45):
        finish(place_o(box((0, 0, 0), (.008, .30, .012)), (5.39, 2.15, z), (math.radians(40), 0, 0)), DK, .6, 0, 0)
    # wheels: single front, dual rear on a tandem, fenders and arches
    for sgn in (-1, 1):
        road_wheel(4.40, .48, sgn * 1.00, sgn, R=.48, W=.30, spokes=10, rimc=(.62, .63, .65), name='tf', tread=16, seg=28)
        for xw in (-2.00, -3.05):
            road_wheel(xw, .48, sgn * 1.02, sgn, R=.48, W=.28, spokes=10, rimc=(.62, .63, .65), name='tr', tread=16, seg=28)
            road_wheel(xw, .48, sgn * .72, sgn, R=.48, W=.28, spokes=10, rimc=(.40, .41, .42), name='tr2', tread=0, seg=20)
        fen = _rz([(sgn * .84, .56), (sgn * 1.18, .56), (sgn * 1.18, .60), (sgn * .84, .60)], 4.40, .48, 32, 'fen', loop=True)
        cut(fen, box((4.40, .00, 0), (.8, .46, 2.0)))
        finish(fen, (.5, .5, .5), .4, .1, 0, flag=F_PAINT)
        for xw in (-2.00, -3.05):
            ff = _rz([(sgn * .60, .56), (sgn * 1.18, .56), (sgn * 1.18, .59), (sgn * .60, .59)], xw, .48, 28, 'ffr', loop=True)
            cut(ff, box((xw, .20, 0), (.8, .72, 2.0)))
            finish(ff, DK, .6, 0, 0)
        # mud flaps and tail lamps
        finish(box((-3.60, .52, sgn * .98), (.01, .32, .18)), DK, .8, 0, 0)
        finish(box((-3.61, .92, sgn * .85), (.01, .06, .15)), LENS_R, .12, 0, 0)
        finish(box((-3.61, .92, sgn * .62), (.01, .06, .06)), LENS_A, .12, 0, 0)
        # side guards
        for y in (.50, .76):
            finish(box((.10, y, sgn * 1.04), (1.50, .022, .02)), STL, .5, .6, 0)
    # fuel tank (chrome), air tanks, battery box
    finish(_rx(1.60, 2.70, .80, -.86, .24, 20), CHR, .18, .95, 0)
    for xs_ in (1.62, 2.68):
        finish(_rx(xs_ - .015, xs_ + .015, .80, -.86, .25, 20), STL, .4, .7, 0)
    finish(_rx(.40, .80, .78, .80, .13, 14), (.30, .31, .33), .5, .4, 0)
    finish(_rx(-.60, -.20, .78, .80, .13, 14), (.30, .31, .33), .5, .4, 0)
    finish(box((1.25, .80, .88), (.25, .18, .17)), DK, .6, 0, 0)
    # exhaust stack, heat shield, rain cap; air horns
    finish(_ry(1.70, 3.40, 3.30, .92, .065, 14), CHR, .2, .95, 0)
    finish(_ry(2.00, 2.60, 3.30, .92, .085, 14), STL, .5, .6, 0)
    finish(place_o(box((0, 0, 0), (.08, .006, .08)), (3.30, 3.41, .92), (0, 0, math.radians(18))), DK, .5, .5, 0)
    for k in range(2):
        finish(tube((4.0 + k * .3, 3.30, .35), (4.4 + k * .3, 3.30, .35), .04, 10), CHR, .2, .95, 0)

# ---------------------------------------------------------------- the drop-leg holster
# Worn on the figure, not placed in the world: drawn by code/figure/draw.c in
# the PISTOL'S OWN FRAME (x across the slide, y up = the slide's top, z forward
# = the muzzle; metres), around the standard pistol's measured box
# x +-0.020, y -0.112..0.036, z -0.054..0.153, and stretched per axis to the
# box of whichever pistol is carried. +x is the side against the thigh.
# A molded polymer duty holster: the shell is deep where it closes over the
# trigger guard and STEPS to a slim sheath over the slide and dust cover down
# to a rounded toe; a raised lip round the mouth (which stops ahead of the
# grip, so the whole grip stands clear); a rotating hood over the back of the
# slide on pivot hubs; a thigh platform behind it all, screwed to the shell.
# Colour flags 8/9 take the wearer's webbing / kit colours (g_md_slots), the
# part's red channel being its brightness against them.
HOL_W = .033                                  # shell half-thickness

def _hol_side(pts, x0, x1, b, seg, col, rough, flag, name):
    o = prof(pts, x0, x1, name)
    return finish(o, col, rough, 0.0, b, seg, flag=flag)

def holster():
    clear()
    shell = [(.010, -.074), (.008, .000), (-.004, .052),      # the mouth, slanting back at the top
             (.150, .052), (.166, .046), (.176, .028), (.178, .000),   # top edge to the rounded toe
             (.172, -.030), (.160, -.042), (.100, -.046),     # the slide sheath's underside
             (.088, -.054), (.076, -.066), (.064, -.072),     # the STEP up from the trigger guard
             (.030, -.074)]
    _hol_side(shell, -HOL_W, HOL_W, .010, 2, (1.0, 1.0, 1.0), .42, 9, 'shell')
    # a raised molded rib down the outer face over the trigger guard: the read
    # that this shell was formed ROUND a gun
    rib = [(.016, -.060), (.016, .020), (.044, .030), (.058, -.040), (.044, -.062)]
    _hol_side(rib, -HOL_W - .004, -HOL_W + .006, 0, 1, (.92, .92, .92), .45, 9, 'rib')
    # the mouth's lip: a band a few millimetres proud of the shell
    lip = [(.014, -.074), (.012, .000), (.000, .056), (-.010, .056), (.000, .000), (.002, -.074)]
    _hol_side(lip, -HOL_W - .003, HOL_W + .003, 0, 1, (.80, .80, .80), .50, 9, 'lip')
    # THE HOOD: a U over the slide just behind the mouth, its legs down both
    # faces to the pivot hubs
    U = [(-.039, -.012), (-.039, .052), (-.027, .066), (.027, .066), (.039, .052), (.039, -.012),
         (.031, -.012), (.031, .048), (.023, .057), (-.023, .057), (-.031, .048), (-.031, -.012)]
    hood = prof_front(U, -.054, -.034, 'hood')
    finish(hood, (.70, .70, .70), .40, 0.0, .005, 1, flag=9)
    # ...joined to the shell by a strap along each side of the slide
    for sx in (-1, 1):
        st = box((sx * .035, .024, -.020), (.0035, .030, .016), 'hstrap')
        finish(st, (.70, .70, .70), .40, 0.0, 0, 1, flag=9)
    top = box((0, .060, -.020), (.0385, .0055, .016), 'htop')       # ...and over the slide's top
    finish(top, (.70, .70, .70), .40, 0.0, 0, 1, flag=9)
    for sx in (-1, 1):
        hub = _disc([(0, .011), (.006, .011), (.008, .007)], (sx * .038, -.012, -.044), (sx, 0, 0), 12)
        finish(hub, (.60, .60, .60), .40, 0.0, 0, 2, flag=9)
    # the release tab on the outer face, under the thumb
    tab = prof([(.000, .006), (.022, .010), (.024, .022), (.002, .020)], -HOL_W - .006, -HOL_W + .002, 'tab')
    finish(tab, (.62, .62, .62), .45, 0.0, 0, 1, flag=9)
    # THE PLATFORM against the thigh: taller than the holster (its top runs up
    # toward the belt hanger), rounded, a hair thinner than a finger
    plat = [(-.080, -.058), (-.092, -.040), (-.092, .058), (-.080, .072), (.150, .072),
            (.170, .056), (.170, -.044), (.150, -.060)]
    _hol_side(plat, HOL_W - .004, HOL_W + .008, .006, 2, (.82, .82, .82), .70, 9, 'plat')
    # two strap slots across it (the leg straps' webbing runs through them)
    for zc in (.002, .118):
        sl = box((HOL_W + .0085, .007, zc), (.0012, .055, .012), 'slot')
        finish(sl, (1.0, 1.0, 1.0), .85, 0.0, 0, 1, flag=8)
    # the screws that hold shell to platform, through the outer face
    for zc, yc in ((.034, .036), (.130, .034), (.120, -.030), (.026, -.050)):
        sc = _disc([(0, .0055), (.0020, .0050), (.0026, .0030)], (-HOL_W - .0005, yc, zc), (-1, 0, 0), 8)
        finish(sc, (.30, .30, .32), .35, .9, 0, 1)
    _hol_clean()

def _hol_clean():
    """Bake every modifier, then collapse anything under 0.1 mm. The game
    stores a vertex to ~0.03 mm, and a bevel's corner slivers on a thin part
    land on one grid point: zero-area triangles the figure check counts as
    degenerate. Collapsing (rather than dropping) them keeps the mesh closed."""
    for o in list(bpy.context.collection.objects):
        if 'col' not in o or o.type != 'MESH': continue
        bpy.context.view_layer.objects.active = o; o.select_set(True)
        for m in list(o.modifiers):
            bpy.ops.object.modifier_apply(modifier=m.name)
        o.select_set(False)
        bm = bmesh.new(); bm.from_mesh(o.data)
        for _ in range(3):
            bmesh.ops.dissolve_degenerate(bm, dist=1.0e-4, edges=bm.edges[:])
        bmesh.ops.triangulate(bm, faces=bm.faces[:])
        bad = [f for f in bm.faces if f.calc_area() < 2.0e-9]
        if bad: bmesh.ops.dissolve_degenerate(bm, dist=2.0e-4, edges=bm.edges[:])
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        bm.to_mesh(o.data); bm.free()

PROP_MODELS = [(well_frame, 'PROP_WELL', 4.0, .3), (sedan, 'PROP_SEDAN', 8.0, .5), (pickup, 'PROP_PICKUP', 8.0, .5),
               (dumpster, 'PROP_DUMPSTER', 4.0, .3), (motorbike, 'PROP_MOTO', 4.0, .2),
               (ac_unit, 'PROP_AC', 2.0, .15), (sat_dish, 'PROP_DISH', 2.0, .15),
               (water_tank, 'PROP_WTANK', 4.0, .3), (cafe_set, 'PROP_CAFE', 4.0, .2),
               (tyre_stack, 'PROP_TYRES', 4.0, .2), (genset, 'PROP_GENSET', 4.0, .25),
               (forklift, 'PROP_FORKLIFT', 4.0, .25), (solar_heater, 'PROP_SOLAR', 4.0, .20),
               (ibc_tote, 'PROP_IBC', 2.0, .2), (sack_pallet, 'PROP_SACKS', 2.0, .15),
               (bench, 'PROP_BENCH', 2.0, .15), (elec_cabinet, 'PROP_ELEC', 2.0, .15),
               (supply_med, 'PROP_SUP_MED', 4.0, .15), (supply_med_item, 'PROP_SUP_MED_I', 2.0, .10),
               (supply_ammo, 'PROP_SUP_AMMO', 4.0, .15), (supply_ammo_item, 'PROP_SUP_AMMO_I', 2.0, .08),
               (helicopter, 'PROP_HELI', 16.0, .5), (truck_cab, 'PROP_TRUCK', 8.0, .4),
               (holster, 'FIG_HOLSTER', 1.0, .02)]

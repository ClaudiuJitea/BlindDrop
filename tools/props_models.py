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

# ---------------------------------------------------------------- the saloon
SEDAN_P = dict(L=2.115, W=.925, plan_e=7.0, roofw=.66, roof=1.445,
               sill=piecewise([(-2.12, .36), (-1.95, .28), (-1.70, .25), (1.70, .25), (1.95, .28), (2.12, .37)]),
               belt=piecewise([(-2.12, .86), (-1.30, .925), (0.95, .885), (2.12, .70)]),
               deck=piecewise([(-2.12, .80), (-1.95, .90), (-1.30, .925), (-1.20, .93),
                               (0.95, .88), (1.70, .80), (2.0, .73), (2.12, .64)]),
               top=piecewise([(-2.2, 0), (-1.36, .93), (-0.72, 1.415), (-0.55, 1.44), (0.25, 1.445),
                              (0.42, 1.40), (0.98, .885), (2.2, 0)]))

def sedan_like(P, wheel_x, cab, name, pickup=False):
    """Shared by the saloon and the pickup: body, arches, glass, lamps, trim, wheels."""
    xs = stations(P['L'], 26)
    body = body_loft(xs, lambda x: car_section(x, P), name + '_body')
    WY = cab.get('wy', .33); WR = cab['tyre_r'] + .052
    arches(body, wheel_x, WR + .02, WY, P['W'])
    bed = None
    if pickup:
        # the load bed: a tub cut down into the deck behind the cab
        bx0, bx1 = -P['L'] + .06, cab['bed_front']
        bed = box(((bx0 + bx1) / 2, cab['bed_floor'] + .70, 0), ((bx1 - bx0) / 2, .70, P['W'] * .87))
        cut(body, bed)
    belt, top, deck = P['belt'], P['top'], P['deck']
    xa0, xa1 = cab['ws_base'], cab['ws_top']            # windscreen: base x and top x
    xc0, xc1 = cab['rs_top'], cab['rs_base']            # rear screen: top x and base x
    def ws_x(y):  # windscreen x at height y (linear between base and top)
        t = (y - belt(xa0)) / (top(xa1) - belt(xa0)); return xa0 + (xa1 - xa0) * t
    def rs_x(y):
        t = (y - belt(xc1)) / (top(xc0) - belt(xc1)); return xc1 + (xc0 - xc1) * t
    def rules(c, n):
        x, y, z = c.x, c.y, abs(c.z)
        # arch liners and the floor pan
        for xw in wheel_x:
            if (x - xw) ** 2 + (y - WY) ** 2 < (WR + .028) ** 2 and z < P['W'] - .005:
                return (TRIM, .95, 0.0, 0)
        if n.y < -.7 and y < .40: return (TRIM, .9, 0.0, 0)
        if pickup and bx_in(x) and z < P['W'] * .875 and y > cab['bed_floor'] - .01 and y < deck(x) - .01:
            return ((.10, .10, .105), .85, 0.0, 0)    # the bed's liner
        # THE GLASSHOUSE UNDER THE GLASS IS BLACK: the glass patches are a chord
        # surface over a curved skin, so any paint between them would show
        # through as red flecks; black reads as the seal and the blacked pillars.
        if y > belt(x) + .03 and y < top(x) - .035 and n.y < .80 and top(x) > deck(x) + .05:
            return (TRIM, .55, 0.0, 0)
        return ((.5, .5, .5), .24, 0.0, F_PAINT)
    def bx_in(x): return pickup and x < cab['bed_front'] + .02
    parts = classify_split(body, rules, name + '_skin')
    bvh = _bvh_of(list(parts.values()))
    W = P['W']
    # --- glass: side windows, windscreen, rear screen, each in a black seal
    def side_win(sgn, x0f, x1f, ylo, yhi, lift):
        def fn(u, v):
            y = ylo + (yhi - ylo) * v
            xf = min(x0f(y), ws_x(y) - .075)
            xr = max(x1f(y), rs_x(y) + .09) if cab['rs_top'] is not None else x1f(y)
            x = xr + (xf - xr) * u
            return ((x, y, sgn * 2.0), (0, 0, -sgn))
        return fn
    for sgn in (-1, 1):
        for (xf, xr) in cab['windows']:
            ylo = lambda x: belt(x)
            y0w = max(belt(xf), belt(xr)) + .045
            y1w = cab['win_top']
            gl = patch(bvh, side_win(sgn, lambda y, xf=xf: xf, lambda y, xr=xr: xr, y0w, y1w, .0), 12, 6, .006)
            finish(gl, (.07, .09, .10), .04, 0, 0, flag=F_GLASS)
    def screen(x_of_y, ylo, yhi, wfn, sgnx, lift):
        def fn(u, v):
            y = ylo + (yhi - ylo) * v
            w = wfn(y)
            z = -w + 2 * w * u
            return ((x_of_y(y) + sgnx * 1.5, y, z), (-sgnx, 0, 0))
        return fn
    wsw = lambda y: P['roofw'] - .05 + (W * .86 - P['roofw']) * max(0, (top(xa1) - y)) / (top(xa1) - belt(xa0)) * .9
    ylo_ws, yhi_ws = belt(xa0) + .03, top(xa1) - .04
    finish(patch(bvh, screen(ws_x, ylo_ws, yhi_ws, wsw, 1, 0), 16, 10, .006), (.07, .09, .10), .04, 0, 0, flag=F_GLASS)
    if cab['rs_top'] is not None:
        ylo_rs, yhi_rs = belt(xc1) + .04, top(xc0) - .04
        rsw = lambda y: P['roofw'] - .07 + (W * .84 - P['roofw']) * max(0, (top(xc0) - y)) / (top(xc0) - belt(xc1)) * .85
        finish(patch(bvh, screen(rs_x, ylo_rs, yhi_rs, rsw, -1, 0), 16, 8, .006), (.07, .09, .10), .04, 0, 0, flag=F_GLASS)
    # --- shut lines: doors, bonnet, boot
    for sgn in (-1, 1):
        for xs_ in cab['door_lines']:
            finish(ribbon(bvh, lambda t, xs_=xs_: ((xs_, .30 + t * (belt(xs_) - .30 + .02), sgn * 2), (0, 0, -sgn)), 10, .006, .0012),
                   v3s(.03), .7, 0, 0)
        # the door bottoms
        finish(ribbon(bvh, lambda t: ((cab['door_lines'][0] + (cab['door_lines'][-1] - cab['door_lines'][0]) * t, .31, sgn * 2), (0, 0, -sgn)), 12, .006, .0012),
               v3s(.03), .7, 0, 0)
        # handles
        for xh in cab['handles']:
            p, n = _hit(bvh, (xh, belt(xh) - .09, sgn * 2), (0, 0, -sgn))
            if p is not None:
                h = rbox(.065, .011, .012, .005)
                finish(oriented(h, p + n * .006, n.normalized()), CHROME, .25, .9, 0)
        # mirror: a stalk off the door's front corner and a housing
        xm = cab['mirror_x']
        p, n = _hit(bvh, (xm, belt(xm) + .07, sgn * 2), (0, 0, -sgn))
        if p is not None:
            st = rbox(.03, .02, .05, .008); st.location = (p.x, p.y, p.z + sgn * .04)
            bpy.context.view_layer.objects.active = st; st.select_set(True)
            bpy.ops.object.transform_apply(location=True); st.select_set(False)
            finish(st, TRIM, .5, 0, 0)
            hs = rbox(.05, .055, .09, .025); hs.location = (p.x - .02, p.y + .03, p.z + sgn * .13)
            bpy.context.view_layer.objects.active = hs; hs.select_set(True)
            bpy.ops.object.transform_apply(location=True); hs.select_set(False)
            finish(hs, (.5, .5, .5), .24, 0, 0, flag=F_PAINT)
            mg = box((p.x - .072, p.y + .03, p.z + sgn * .13), (.002, .045, .075))
            finish(mg, (.55, .6, .65), .05, .9, 0)
        # side repeater on the front wing
        p, n = _hit(bvh, (P['L'] - .55, belt(P['L'] - .55) - .12, sgn * 2), (0, 0, -sgn))
        if p is not None:
            finish(oriented(rbox(.012, .012, .03, .005), p + n * .003, n), LENS_A, .2, 0, 0)
    for xs_ in cab['deck_lines']:
        finish(ribbon(bvh, lambda t, xs_=xs_: ((xs_, 3.0, -W * .86 + 2 * W * .86 * t), (0, -1, 0)), 14, .006, .0012),
               v3s(.03), .7, 0, 0)
    # --- lamps, grille, plates on the nose and the tail
    L = P['L']
    # wrap-round rubbing strips on both bumpers, and a lower valance in black
    for e in (-1, 1):
        for yy, wdt, colr in ((cab['bump_y'], .05, PLASTIC), (.30, .07, PLASTIC)):
            def wrap(t, e=e, yy=yy):
                th = (t * 2 - 1) * math.radians(84)
                cx = e * (L - .75)
                d = Vector((e * math.cos(th), 0, math.sin(th)))
                return ((cx + d.x * 3, yy, d.z * 3), (-d.x, 0, -d.z))
            finish(ribbon(bvh, wrap, 16, wdt, .004), colr, .6, 0, 0)
    for sgn in (-1, 1):
        # headlamp: a dark housing, a clear lens and a chrome reflector behind it
        for zz, ww, col in ((.62, .17, LENS_W),):
            p, n = _hit(bvh, (3.0, cab['head_y'], sgn * zz), (-1, 0, 0))
            if p is not None:
                finish(oriented(rbox(.03, .065, ww, .02), p - n * .012, n), TRIM, .5, 0, 0)
                finish(oriented(rbox(.03, .055, ww - .02, .018), p + n * .004, n), (.80, .82, .82), .08, .6, 0)
                finish(oriented(lathe_z([(0, .035), (.012, .03)], 0, 0, 1, 16), p + n * .012 + Vector((0, 0, -sgn * .07)), n),
                       (.96, .96, .92), .05, .2, 0)
        p, n = _hit(bvh, (3.0, cab['head_y'] - .10, sgn * .78), (-1, 0, 0))
        if p is not None: finish(oriented(rbox(.02, .022, .07, .01), p, n), LENS_A, .2, 0, 0)
        # tail lamp: a red cluster with the white reverse and amber indicator
        p, n = _hit(bvh, (-3.0, cab['tail_y'], sgn * .66), (1, 0, 0))
        if p is not None:
            finish(oriented(rbox(.03, .07, .20, .02), p - n * .010, n), TRIM, .5, 0, 0)
            finish(oriented(rbox(.03, .06, .19, .018), p + n * .004, n), LENS_R, .12, 0, 0)
            finish(oriented(rbox(.03, .02, .05, .008), p + n * .010 + Vector((0, -.02, -sgn * .09)), n), LENS_W, .1, 0, 0)
            finish(oriented(rbox(.03, .02, .05, .008), p + n * .010 + Vector((0, .025, sgn * .08)), n), LENS_A, .1, 0, 0)
    # grille: a dark mesh in a chrome frame, with a badge
    p, n = _hit(bvh, (3.0, cab['grille_y'], 0), (-1, 0, 0))
    if p is not None:
        finish(oriented(rbox(.03, cab['grille_h'], .36, .015), p - n * .012, n), CHROME, .25, .9, 0)
        finish(oriented(box((0, 0, 0), (.03, cab['grille_h'] - .012, .345)), p - n * .006, n), (.04, .04, .045), .7, .2, 0)
        for k in range(4):
            yy = -cab['grille_h'] + .02 + k * (2 * cab['grille_h'] - .04) / 3
            finish(oriented(box((0, 0, 0), (.03, .004, .34)), p - n * .004 + Vector((0, yy, 0)), n), (.32, .33, .35), .4, .8, 0)
        finish(oriented(lathe_z([(0, .028), (.01, .024)], 0, 0, 1, 16), p + n * .012, n), CHROME, .2, .95, 0)
    for xe, yy in ((3.0, .36), (-3.0, cab['plate_ry'])):
        p, n = _hit(bvh, (xe, yy, 0), (-1 if xe > 0 else 1, 0, 0))
        if p is not None:
            finish(oriented(box((0, 0, 0), (.006, .055, .24)), p + n * .004, n), PLATE, .5, 0, 0)
            finish(oriented(box((0, 0, 0), (.006, .010, .22)), p + n * .008 + Vector((0, .02, 0)), n), (.15, .25, .55), .5, 0, 0)
    # wipers on the cowl, an aerial, the exhaust
    for zz in (-.35, .15):
        p, n = _hit(bvh, (xa0 + .05, 3, zz), (0, -1, 0))
        if p is not None:
            w = box((0, 0, 0), (.012, .006, .26)); w.location = p + Vector((0, .01, .0))
            w.rotation_euler = (0, .18, 0)
            bpy.context.view_layer.objects.active = w; w.select_set(True)
            bpy.ops.object.transform_apply(location=True, rotation=True); w.select_set(False)
            finish(w, TRIM, .6, 0, 0)
    ex = lathe_z([(0, .026), (.10, .026)], 0, 0, 1, 12)
    oriented(ex, (-L + .02, .27, .45), (-1, 0, 0))
    finish(ex, CHROME, .3, .9, 0)
    # wheels in the arches
    for xw in wheel_x:
        for sgn in (-1, 1):
            road_wheel(xw, WY, sgn * (W - .135), sgn, R=cab['tyre_r'], W=.20, spokes=cab['spokes'], name='w',
                       tread=0, seg=20)
    return bvh

def v3s(v): return (v, v, v)

def sedan():
    """A four-door saloon, the town's taxi and family car: the 4.2 m hull the
    game's collision already gives it (code/world/prefabs.c, car())."""
    clear()
    P = SEDAN_P
    cab = dict(ws_base=.97, ws_top=.40, rs_top=-.70, rs_base=-1.33,
               windows=[(.86, -.10), (-.22, -1.20)], win_top=1.37,
               door_lines=[.90, -.16, -1.10], deck_lines=[1.02, -1.42], handles=[.40, -.62],
               mirror_x=.80, head_y=.64, tail_y=.80, grille_y=.55, grille_h=.055, plate_ry=.55, bump_y=.42,
               tyre_r=.32, spokes=5)
    sedan_like(P, (-1.25, 1.27), cab, 'sedan')
    # the B-pillar between the windows, black, both sides
    bvh = _bvh_of([o for o in bpy.context.collection.objects if 'col' in o])
    for sgn in (-1, 1):
        def fn(u, v, sgn=sgn):
            return ((-.10 - .12 * u, .98 + .38 * v, sgn * 2), (0, 0, -sgn))
        finish(patch(bvh, fn, 2, 4, .005), TRIM, .5, 0, 0)

# ---------------------------------------------------------------- the pickup
PICKUP_P = dict(L=2.125, W=.93, plan_e=9.0, roofw=.70, roof=1.66,
                sill=piecewise([(-2.13, .48), (-1.90, .42), (1.85, .42), (2.13, .46)]),
                belt=piecewise([(-2.13, 1.00), (-0.45, 1.00), (0.95, .985), (2.13, .90)]),
                deck=piecewise([(-2.13, 1.00), (-0.46, 1.00), (-0.40, .99), (0.95, .985),
                                (1.70, .965), (2.0, .93), (2.13, .84)]),
                top=piecewise([(-2.2, 0), (-0.50, 1.00), (-0.42, 1.62), (-0.30, 1.655),
                               (0.30, 1.66), (0.48, 1.62), (0.98, .98), (2.2, 0)]))

def pickup():
    """A single-cab pickup, the workhorse of every desert town: a short cab, a
    long open bed, a ladder-frame ride height and bigger tyres."""
    clear()
    P = PICKUP_P
    cab = dict(ws_base=.97, ws_top=.47, rs_top=-.41, rs_base=-.47, bed_front=-.52, bed_floor=.66,
               windows=[(.88, -.34)], win_top=1.57,
               door_lines=[.92, -.38], deck_lines=[1.02], handles=[-.20],
               mirror_x=.82, head_y=.76, tail_y=.86, grille_y=.64, grille_h=.09, plate_ry=.58, bump_y=.48,
               tyre_r=.36, wy=.37, spokes=6)
    sedan_like(P, (-1.30, 1.32), cab, 'pickup', pickup=True)
    bvh = _bvh_of([o for o in bpy.context.collection.objects if 'col' in o])
    W = P['W']
    # tailgate shut lines and its latch handle; bed rail caps; the step bumper
    for sgn in (-1, 1):
        finish(ribbon(bvh, lambda t, sgn=sgn: ((-3, .60 + t * .38, sgn * (W * .87)), (1, 0, 0)), 8, .006, .0012), v3s(.03), .7, 0, 0)
        cap = box((-1.32, 1.006, sgn * (W * .90)), (.80, .008, .035))
        finish(cap, PLASTIC, .6, 0, 0)
    p, n = _hit(bvh, (-3, .90, 0), (1, 0, 0))
    if p is not None: finish(oriented(rbox(.01, .025, .09, .008), p + n * .006, n), PLASTIC, .5, 0, 0)
    # the wheel tubs inside the bed, over the rear wheelhouses
    for sgn in (-1, 1):
        tub = cyl_z(-1.30, .37, sgn * (W - .34), sgn * (W * .875), .46, 28)
        cut(tub, box((-1.30, .37 - .5 + cab['bed_floor'] - .37 + .0, 0), (1, .5, 2)))
        finish(tub, (.10, .10, .105), .85, 0, 0)
    sb = rbox(.07, .05, W * .82, .02); sb.location = (-P['L'] + .05, .50, 0)
    bpy.context.view_layer.objects.active = sb; sb.select_set(True)
    bpy.ops.object.transform_apply(location=True); sb.select_set(False)
    finish(sb, (.30, .31, .32), .45, .7, 0)
    # a spare wheel and a jerry can in the bed, a tarp bundle at the front of it
    finish(cyl_z(-1.55, cab['bed_floor'] + .11, -.40, -.18, .34, 24), P_RUBBER, .92, 0, 0)
    finish(cyl_z(-1.55, cab['bed_floor'] + .11, -.18, -.17, .22, 20), ALLOY, .35, .8, 0)
    jc = rbox(.18, .24, .08, .02); jc.location = (-.75, cab['bed_floor'] + .24, .55)
    bpy.context.view_layer.objects.active = jc; jc.select_set(True)
    bpy.ops.object.transform_apply(location=True); jc.select_set(False)
    finish(jc, (.22, .30, .18), .7, 0, 0)
    tp = rbox(.20, .14, .62, .10); tp.location = (-.78, cab['bed_floor'] + .14, -.12)
    bpy.context.view_layer.objects.active = tp; tp.select_set(True)
    bpy.ops.object.transform_apply(location=True); tp.select_set(False)
    finish(tp, (.42, .40, .30), .9, 0, 0)

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
def motorbike():
    """A 125 cc commuter motorcycle on its side stand: spoked wheels, a cradle
    frame, tank and seat, the engine and its exhaust, forks and bars."""
    clear()
    R = .30
    for xw in (-.66, .66):
        road_wheel(xw, R, 0, 1, R=R, W=.09, spokes=8, rimc=(.70, .70, .72), name='mw', tread=0, seg=16)
    fr = (.12, .12, .13)
    for a, b in (((.42, .86, 0), (-.20, .80, 0)), ((.42, .86, 0), (.10, .30, 0)), ((-.20, .80, 0), (-.66, .32, 0)),
                 ((.10, .30, 0), (-.30, .30, 0)), ((-.30, .30, 0), (-.66, .32, 0))):
        for sg in ((-1, 1) if a[1] < .5 else (0,)):
            finish(tube((a[0], a[1], sg * .06), (b[0], b[1], sg * .06), .02), fr, .45, .3, 0)
    finish(tube((-.25, .72, .09), (-.66, .32, .09), .016), fr, .45, .3, 0)          # swingarm + shocks
    finish(tube((-.25, .72, -.09), (-.66, .32, -.09), .016), fr, .45, .3, 0)
    for sg in (-1, 1):
        finish(tube((.66, .30, sg * .075), (.44, .96, sg * .075), .022), (.75, .75, .77), .25, .9, 0)   # forks
    tank = rbox(.20, .09, .14, .06); place_o(tank, (.18, .93, 0), (0, 0, -.10))
    finish(tank, (.5, .5, .5), .25, 0, 0, flag=F_PAINT)
    seat = rbox(.32, .05, .12, .04); place_o(seat, (-.30, .90, 0), (0, 0, .03))
    finish(seat, (.05, .05, .055), .6, 0, 0)
    finish(place_o(rbox(.24, .05, .12, .03), (-.30, .83, 0)), (.5, .5, .5), .3, 0, 0, flag=F_PAINT)   # side covers
    finish(place_o(rbox(.17, .14, .11, .03), (.08, .45, 0)), (.38, .38, .40), .45, .6, 0)              # engine case
    cyl = place_o(rbox(.07, .11, .07, .02), (.24, .58, 0), (0, 0, -.6))
    finish(cyl, (.48, .48, .50), .5, .6, 0)
    for k in range(5):
        finish(place_o(box((0, 0, 0), (.09, .006, .09)), (.24 + .03 * k * .56, .52 + .03 * k * .82, 0), (0, 0, -.6)), (.42, .42, .44), .5, .6, 0)
    finish(tube((.28, .50, .07), (.05, .26, .14), .022), CHROME, .3, .9, 0)                            # exhaust
    finish(tube((.05, .26, .14), (-.55, .38, .14), .028), CHROME, .3, .9, 0)
    finish(tube((-.55, .38, .14), (-.72, .40, .14), .035), (.20, .20, .21), .4, .7, 0)
    finish(tube((.44, .98, -.34), (.44, .98, .34), .012), (.70, .70, .72), .3, .9, 0)                  # handlebar
    for sg in (-1, 1):
        finish(tube((.44, .98, sg * .26), (.44, .98, sg * .36), .018), P_RUBBER, .8, 0, 0)
        finish(tube((.44, 1.0, sg * .18), (.40, 1.16, sg * .24), .005), TRIM, .4, .5, 0)              # mirror stalks
        finish(place_o(lathe_z([(0, .035), (.01, .033)], 0, 0, 1, 12), (.40, 1.17, sg * .24), (0, math.pi / 2, 0)), TRIM, .4, .5, 0)
    hl = lathe_z([(0, .075), (.05, .08), (.08, .07)], 0, 0, 1, 16)
    finish(place_o(hl, (.50, .88, 0), (0, math.pi / 2, 0)), (.10, .10, .11), .4, .3, 0)
    finish(place_o(lathe_z([(0, .068), (.006, .066)], 0, 0, 1, 16), (.58, .88, 0), (0, math.pi / 2, 0)), LENS_W, .08, 0, 0)
    for xw, a0, a1 in ((.66, .3, 2.0), (-.66, 1.3, 2.9)):                                           # mudguards
        bm = bmesh.new(); prev = None
        for k in range(9):
            a = a0 + (a1 - a0) * k / 8
            c = Vector((xw + (R + .04) * cos(a), R + (R + .04) * sin(a), 0))
            v1 = bm.verts.new(c + Vector((0, 0, .055))); v2 = bm.verts.new(c - Vector((0, 0, .055)))
            if prev: bm.faces.new((prev[0], prev[1], v2, v1))
            prev = (v1, v2)
        o = from_bm(bm, 'guard'); o.modifiers.new('s', 'SOLIDIFY').thickness = .004
        finish(o, (.5, .5, .5), .3, 0, 0, flag=F_PAINT)
    finish(tube((-.60, .55, -.06), (-.88, .55, -.06), .006), TRIM, .4, .5, 0)                      # rear rack
    finish(place_o(rbox(.02, .03, .06, .008), (-.86, .56, 0)), LENS_R, .15, 0, 0)                  # tail lamp
    finish(tube((-.05, .30, -.10), (-.15, .02, -.32), .012), fr, .45, .4, 0)                        # side stand
    # lean the whole bike onto its stand
    for o in list(bpy.context.collection.objects):
        o.rotation_euler = (math.radians(-9), 0, 0)
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

PROP_MODELS = [(well_frame, 'PROP_WELL', 4.0, .3), (sedan, 'PROP_SEDAN', 8.0, .5), (pickup, 'PROP_PICKUP', 8.0, .5),
               (dumpster, 'PROP_DUMPSTER', 4.0, .3), (motorbike, 'PROP_MOTO', 4.0, .2),
               (ac_unit, 'PROP_AC', 2.0, .15), (sat_dish, 'PROP_DISH', 2.0, .15),
               (water_tank, 'PROP_WTANK', 4.0, .3), (cafe_set, 'PROP_CAFE', 4.0, .2),
               (tyre_stack, 'PROP_TYRES', 4.0, .2), (genset, 'PROP_GENSET', 4.0, .25),
               (forklift, 'PROP_FORKLIFT', 4.0, .25), (solar_heater, 'PROP_SOLAR', 4.0, .20),
               (ibc_tote, 'PROP_IBC', 2.0, .2), (sack_pallet, 'PROP_SACKS', 2.0, .15),
               (bench, 'PROP_BENCH', 2.0, .15), (elec_cabinet, 'PROP_ELEC', 2.0, .15),
               (supply_med, 'PROP_SUP_MED', 4.0, .15), (supply_med_item, 'PROP_SUP_MED_I', 2.0, .10),
               (supply_ammo, 'PROP_SUP_AMMO', 4.0, .15), (supply_ammo_item, 'PROP_SUP_AMMO_I', 2.0, .08)]

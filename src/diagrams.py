"""Generate SVG diagrams of the geometry in the CPU-Z 17.01.64 kernel. Usage: diagrams.py <outdir>"""
import math, sys
OUT = sys.argv[1]
F, G = (math.sqrt(3) - 1) / 2, (3 - math.sqrt(3)) / 6
INK, MUTED, GRID, ACC, ACC2, BG = '#1d2633', '#5b6676', '#c5ccd6', '#2d6cdf', '#e0782a', '#ffffff'
FONT = 'font-family="Helvetica, Arial, sans-serif"'

def svg(w, h, body):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" width="{w}" height="{h}">'
            f'<rect width="{w}" height="{h}" fill="{BG}"/>{body}</svg>')
def text(x, y, s, size=14, fill=INK, anchor='start', weight='normal'):
    return f'<text x="{x:.1f}" y="{y:.1f}" {FONT} font-size="{size}" fill="{fill}" text-anchor="{anchor}" font-weight="{weight}" style="white-space:pre">{s}</text>'
def line(a, b, stroke=GRID, w=1.2, dash=None):
    d = f' stroke-dasharray="{dash}"' if dash else ''
    return f'<line x1="{a[0]:.1f}" y1="{a[1]:.1f}" x2="{b[0]:.1f}" y2="{b[1]:.1f}" stroke="{stroke}" stroke-width="{w}"{d}/>'
def poly(pts, fill, stroke='none', op=1):
    p = ' '.join(f'{x:.1f},{y:.1f}' for x, y in pts)
    return f'<polygon points="{p}" fill="{fill}" fill-opacity="{op}" stroke="{stroke}" stroke-width="1.5"/>'
def dot(p, r=4, fill=INK): return f'<circle cx="{p[0]:.1f}" cy="{p[1]:.1f}" r="{r}" fill="{fill}"/>'
def arrow_defs(): return (f'<defs><marker id="ah" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse">'
                          f'<path d="M0,0 L10,5 L0,10 z" fill="{INK}"/></marker></defs>')

# 1. Simplex skew: square grid <-> triangular lattice
def skew_diagram():
    W, H, S = 900, 430, 110
    P = (1.30, 0.90)                                  # sample point, real space
    s = (P[0] + P[1]) * F; Ps = (P[0] + s, P[1] + s)  # skewed
    i, j = math.floor(Ps[0]), math.floor(Ps[1])
    x0, y0 = Ps[0] - i, Ps[1] - j
    c2 = (i + 1, j) if x0 > y0 else (i, j + 1)
    corners = [(i, j), c2, (i + 1, j + 1)]
    unskew = lambda q: (q[0] - (q[0] + q[1]) * G, q[1] - (q[0] + q[1]) * G)
    # left panel: skewed space, origin at (60, 380)
    L = lambda q: (60 + q[0] * S, 380 - q[1] * S)
    R0 = (530, 360); R = lambda q: (R0[0] + q[0] * S * 1.0, R0[1] - q[1] * S * 1.0)
    b = [arrow_defs()]
    for panel, T, tf in ((0, L, lambda q: q), (1, R, unskew)):
        for a in range(0, 4):
            b.append(line(T(tf((a, 0))), T(tf((a, 3)))))
            b.append(line(T(tf((0, a))), T(tf((3, a)))))
        for a in range(0, 3):
            for c in range(0, 3):
                b.append(line(T(tf((a, c))), T(tf((a + 1, c + 1))), dash='4 3'))
        tri = [T(tf(q)) for q in corners]
        b.append(poly(tri, ACC, ACC, 0.18))
        for q in corners: b.append(dot(T(tf(q)), 4.5, ACC))
    if True:
        for q in corners:   # falloff support r^2 < 0.5 in real space
            c = R(unskew(q)); r = math.sqrt(0.5) * S * 1.0
            b.append(f'<circle cx="{c[0]:.1f}" cy="{c[1]:.1f}" r="{r:.1f}" fill="none" stroke="{ACC}" stroke-opacity="0.35" stroke-dasharray="3 4"/>')
    b.append(dot(L(Ps), 5, ACC2)); b.append(dot(R(P), 5, ACC2))
    b.append(text(*[L(Ps)[0] + 8, L(Ps)[1] - 8], 'p′', 15, ACC2, weight='bold'))
    b.append(text(*[R(P)[0] + 8, R(P)[1] - 8], 'p', 15, ACC2, weight='bold'))
    b.append(text(60, 30, 'Skewed space: p′ = p + (px + py)·F', 16, weight='bold'))
    b.append(text(60, 50, 'cell = floor(p′); x0 &gt; y0 picks the lower triangle', 13, MUTED))
    b.append(text(520, 30, 'Image space: lattice point − (i + j)·G', 16, weight='bold'))
    b.append(text(520, 50, 'equilateral triangles, side √(2/3); dashed circles = falloff r² &lt; ½', 13, MUTED))
    b.append(f'<path d="M 420 215 C 455 200, 470 200, 500 215" fill="none" stroke="{INK}" stroke-width="1.5" marker-end="url(#ah)"/>')
    b.append(text(460, 195, 'unskew', 13, MUTED, 'middle'))
    b.append(text(60, 418, f'F = (√3 − 1)/2 ≈ {F:.4f}   ·   G = (3 − √3)/6 ≈ {G:.4f}   ·   each corner adds (½ − r²)⁴ · (g · d)', 13, MUTED))
    return svg(W, H, ''.join(b))

# 2. Octave transform R = [[1.6, -1.2], [1.2, 1.6]]: noise-lattice cell seen in image space
def octave_diagram():
    W, H = 900, 440
    th = math.atan2(1.2, 1.6)
    b = [arrow_defs()]
    C = (240, 230); S = 300
    cols = ['#1d2633', '#2d6cdf', '#3f9a6b', '#e0782a', '#b8437a', '#7a5bd1']
    for k in range(5):   # image-space footprint of one lattice cell at octave k: R^-k (unit square)
        sc = S / 2 ** k; a = -k * th
        ca, sa = math.cos(a), math.sin(a)
        pts = [(C[0] + (x * ca - y * sa) * sc, C[1] - (x * sa + y * ca) * sc) for x, y in ((0, 0), (1, 0), (1, 1), (0, 1))]
        pts = [(p[0] - (pts[2][0] - C[0]) / 2, p[1] - (pts[2][1] - C[1]) / 2) for p in pts]   # center the square on C
        b.append(poly(pts, 'none', cols[k]))
        if k <= 2: b.append(text(pts[1][0] + 5, pts[1][1] + (14 if k == 0 else 4), f'k={k}', 12, cols[k]))
    b.append(text(40, 30, 'One noise-lattice cell, seen in image space, per octave k', 16, weight='bold'))
    b.append(text(40, 50, 'Each octave samples at R·p: cells shrink by ½ and turn −36.87° (k = 0 to 4 shown)', 13, MUTED))
    # right: R as a matrix acting on the unit vectors
    O = (640, 300); s = 60
    b.append(line((O[0] - 20, O[1]), (O[0] + 230, O[1]), GRID)); b.append(line((O[0], O[1] + 20), (O[0], O[1] - 220), GRID))
    for v, lab, col in (((1, 0), 'e1', MUTED), ((0, 1), 'e2', MUTED), ((1.6, 1.2), 'R·e1 = (1.6, 1.2)', ACC), ((-1.2, 1.6), 'R·e2 = (−1.2, 1.6)', ACC2)):
        e = (O[0] + v[0] * s * (1 if col == MUTED else 1), O[1] - v[1] * s)
        b.append(f'<line x1="{O[0]}" y1="{O[1]}" x2="{e[0]:.1f}" y2="{e[1]:.1f}" stroke="{col}" stroke-width="2.2" marker-end="url(#ah)"/>')
        b.append(text(e[0] + (6 if v[0] >= 0 else -6), e[1] - 6, lab, 13, col, 'start' if v[0] >= 0 else 'end'))
    r = 38; a1 = th
    b.append(f'<path d="M {O[0]+r} {O[1]} A {r} {r} 0 0 0 {O[0]+r*math.cos(a1):.1f} {O[1]-r*math.sin(a1):.1f}" fill="none" stroke="{INK}"/>')
    b.append(text(O[0] + r + 30, O[1] - 22, '36.87°', 12))
    b.append(text(560, 80, 'R =', 15, weight='bold'))
    for r_, row in enumerate((('1.6', '−1.2'), ('1.2', '1.6'))):
        b.append(text(640, 80 + 20 * r_, row[0], 15, weight='bold', anchor='end')); b.append(text(700, 80 + 20 * r_, row[1], 15, weight='bold', anchor='end'))
    b.append(line((598, 64), (598, 106), INK, 1.5)); b.append(line((708, 64), (708, 106), INK, 1.5))
    b.append(text(560, 124, '= 2 × rotation by atan(3/4)', 13, MUTED))
    b.append(text(560, 360, 'fbm A: 10 + 10 octaves at 1.1/1024', 13, MUTED))
    b.append(text(560, 380, 'fbm B: 7 octaves at 2.2/1024, 7 at 3.3/1024', 13, MUTED))
    b.append(text(560, 400, 'amplitude × 0.7 or 0.6 per octave', 13, MUTED))
    return svg(W, H, ''.join(b))

# 3. Per-pass shift: frame p is the 1024x1024 window at (p, p)
def pass_diagram():
    W, H = 900, 360
    b = [arrow_defs()]
    ox, oy, sc = 80, 60, 0.2
    b.append(poly([(ox, oy), (ox + 1744 * sc, oy), (ox + 1744 * sc, oy + 1250 * sc), (ox, oy + 1250 * sc)], '#eef2f7', GRID))
    for p, col in ((0, ACC), (120, ACC2)):
        x, y = ox + p * sc, oy + p * sc
        b.append(poly([(x, y), (x + 1024 * sc, y), (x + 1024 * sc, y + 1024 * sc), (x, y + 1024 * sc)], col, col, 0.08))
        b.append(text(x + 1024 * sc - 6, y + 1024 * sc - 8, f'pass {p}', 13, col, 'end', 'bold'))
    b.append(f'<line x1="{ox+6}" y1="{oy+6}" x2="{ox+120*sc-2:.1f}" y2="{oy+120*sc-2:.1f}" stroke="{INK}" stroke-width="1.5" marker-end="url(#ah)"/>')
    b.append(text(80, 36, 'Pass p renders pixels (x + p, y + p) of one unbounded cloud field', 16, weight='bold'))
    tx = 500
    for n, s in enumerate(['Row k of pass p → pixels (p + i, p + k), i &lt; 1024',
                           'After 1024 rows, p increases by 1',
                           'Same cloud field, shifted 1 pixel diagonally per pass',
                           'No pixel is computed twice with the same input',
                           '',
                           'aether: ~0.8 passes/s single-thread,',
                           '~10 passes/s multi-thread (10 s phases)']):
        b.append(text(tx, 110 + 22 * n, s, 13, INK if n < 4 else MUTED))
    return svg(W, H, ''.join(b))

for name, fn in (('geometry-simplex.svg', skew_diagram), ('geometry-octaves.svg', octave_diagram), ('geometry-passes.svg', pass_diagram)):
    open(f'{OUT}/{name}', 'w').write(fn()); print(name)

"""
Tubercle-edge mixer-grinder blade - parametric geometry generator (no CAD kernel needed).
Team Wayne Enterprises, atomQUEST 2026 Round 2.

Outputs (in this folder):
  blade_A_baseline.stl     straight-edge control blade (same envelope, thickness, pitch, tip lift)
  blade_B_tubercle.stl     leading-edge tubercles only
  blade_C_hybrid.stl       leading-edge tubercles + trailing-edge serrations (fallback in proposal)
  blade_planform.dxf       flat 2-arm outline of variant C + hub / bore circles (sketch for SolidWorks)
All units: mm.

Coordinates: Z = rotation axis (jar axis), blade rotates counter-clockwise seen from +Z,
so the +Y side of the arm at +X is the LEADING edge.
Each STL is made of three overlapping closed shells (arm 1, arm 2 = arm 1 rotated 180 deg, hub).
Union them in your CAD / COMSOL before meshing.
"""
import math, os, struct, sys
import numpy as np

OUT = os.path.dirname(os.path.abspath(__file__))

P = dict(
    T=1.5,            # blade thickness (given)
    L=40.0,           # radius to tip -> 80 mm tip-to-tip
    r_root=4.1,       # arm starts inside the hub wall (just outside the bore, corners buried in hub)
    w_root=8.0,       # half chord at root  (chord 16)
    w_tip=6.0,        # half chord at tip   (chord 12)
    taper_start=12.0, # chord taper begins here
    tub_amp=1.5,      # tubercle peak-to-valley height (range 1-2)
    tub_wl=4.0,       # tubercle wavelength            (range 3-5)
    ser_amp=1.0,      # trailing-edge serration depth
    ser_wl=3.0,       # trailing-edge serration pitch
    x0=12.0, x1=38.0, blend=3.0,   # edge-feature zone along the arm, smooth run-in / run-out
    pitch=17.5,       # pitch angle (range 15-20), leading edge DOWN -> pumps food upward
    bend_start=28.0, bend_len=6.0, bend_deg=25.0,   # lifted outer edge (tip lift, upward)
    edge_r=0.4,       # rounding radius on the thickness edges
    hub_d=18.0, hub_h=10.0, bore_d=8.0,
    N=360,            # stations along arm
)


def sstep(t):
    t = np.clip(t, 0.0, 1.0)
    return t * t * (3 - 2 * t)


def half_chord(x, p):
    return p["w_root"] + (p["w_tip"] - p["w_root"]) * np.clip((x - p["taper_start"]) / (p["L"] - p["taper_start"]), 0, 1)


def envelope(x, p):
    return sstep((x - p["x0"]) / p["blend"]) * (1 - sstep((x - (p["x1"] - p["blend"])) / p["blend"]))


def y_le(x, p, tub=True):
    w = half_chord(x, p)
    if not tub:
        return w
    return w + p["tub_amp"] * envelope(x, p) * (0.5 - 0.5 * np.cos(2 * np.pi * (x - p["x0"]) / p["tub_wl"]))


def y_te(x, p, ser=True):
    w = half_chord(x, p)
    if not ser:
        return -w
    u = (x - p["x0"]) / p["ser_wl"]
    tri = 1 - np.abs(2 * (u - np.floor(u)) - 1)          # 0..1 triangle wave
    return -w - p["ser_amp"] * envelope(x, p) * tri


def build_arm(p, tub, ser):
    N, T, r = p["N"], p["T"], p["edge_r"]
    xs = np.linspace(p["r_root"], p["L"], N)
    dx = np.diff(xs, prepend=xs[0])
    phi = np.radians(p["bend_deg"]) * sstep((xs - p["bend_start"]) / p["bend_len"])
    cx = p["r_root"] + np.cumsum(np.cos(phi) * dx)
    cz = np.cumsum(np.sin(phi) * dx)
    al = -math.radians(p["pitch"])
    h = T / 2
    m = 5
    verts = []
    for i, x in enumerate(xs):
        ymax, ymin = float(y_le(x, p, tub)), float(y_te(x, p, ser))
        loop = []
        for (cy, cz0, a0) in ((ymax - r, h - r, 0), (ymin + r, h - r, 90), (ymin + r, -h + r, 180), (ymax - r, -h + r, 270)):
            for k in range(m):
                a = math.radians(a0 + 90 * k / (m - 1))
                loop.append((cy + r * math.cos(a), cz0 + r * math.sin(a)))
        for (y, z) in loop:
            ys = y * math.cos(al) - z * math.sin(al)
            zs = y * math.sin(al) + z * math.cos(al)
            nx, nz = -math.sin(phi[i]), math.cos(phi[i])
            verts.append((cx[i] + zs * nx, ys, cz[i] + zs * nz))
    V = np.array(verts)
    M = 4 * m
    F = []
    for i in range(N - 1):
        for j in range(M):
            j2 = (j + 1) % M
            a, b, c, d = i * M + j, i * M + j2, (i + 1) * M + j2, (i + 1) * M + j
            F += [(a, b, c), (a, c, d)]
    c0 = len(V); V = np.vstack([V, V[:M].mean(0)])
    for j in range(M):
        F.append((c0, j2 := (j + 1) % M, j))
    c1 = len(V); V = np.vstack([V, V[(N - 1) * M:].mean(0)])
    for j in range(M):
        F.append((c1, (N - 1) * M + j, (N - 1) * M + (j + 1) % M))
    return orient(V, np.array(F))


def build_hub(p, seg=96):
    R, r, hh = p["hub_d"] / 2, p["bore_d"] / 2, p["hub_h"] / 2
    th = np.linspace(0, 2 * np.pi, seg, endpoint=False)
    ring = lambda rad, z: np.c_[rad * np.cos(th), rad * np.sin(th), np.full(seg, z)]
    V = np.vstack([ring(R, -hh), ring(R, hh), ring(r, -hh), ring(r, hh)])
    Ob, Ot, Ib, It = 0, seg, 2 * seg, 3 * seg
    F = []
    for k in range(seg):
        k2 = (k + 1) % seg
        F += [(Ob + k, Ob + k2, Ot + k2), (Ob + k, Ot + k2, Ot + k)]
        F += [(Ot + k, Ot + k2, It + k2), (Ot + k, It + k2, It + k)]
        F += [(Ob + k, Ib + k2, Ob + k2), (Ob + k, Ib + k, Ib + k2)]
        F += [(Ib + k, It + k2, Ib + k2), (Ib + k, It + k, It + k2)]
    return orient(V, np.array(F))


def signed_volume(V, F):
    a, b, c = V[F[:, 0]], V[F[:, 1]], V[F[:, 2]]
    return float(np.einsum("ij,ij->i", a, np.cross(b, c)).sum() / 6)


def orient(V, F):
    if signed_volume(V, F) < 0:
        F = F[:, ::-1]
    return V, F


def is_watertight(F):
    e = {}
    for t in F:
        for u, v in ((t[0], t[1]), (t[1], t[2]), (t[2], t[0])):
            e[(u, v)] = e.get((u, v), 0) + 1
    return all(c == 1 and (v, u) in e for (u, v), c in e.items())


def rot180z(V):
    W = V.copy(); W[:, 0] *= -1; W[:, 1] *= -1
    return W


def assemble(p, tub, ser):
    Va, Fa = build_arm(p, tub, ser)
    Vh, Fh = build_hub(p)
    shells = [(Va, Fa), (rot180z(Va), Fa), (Vh, Fh)]
    V, F, off = [], [], 0
    for v, f in shells:
        V.append(v); F.append(f + off); off += len(v)
    info = dict(arm_vol=signed_volume(Va, Fa), hub_vol=signed_volume(Vh, Fh),
                watertight=all(is_watertight(f) for _, f in shells))
    return np.vstack(V), np.vstack(F), info


def write_stl(path, V, F, name):
    tri = V[F]
    n = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
    n /= np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-12)
    rec = np.zeros(len(F), dtype=[("n", "<f4", 3), ("v", "<f4", (3, 3)), ("a", "<u2")])
    rec["n"], rec["v"] = n, tri
    with open(path, "wb") as f:
        f.write(name.encode()[:80].ljust(80, b" "))
        f.write(struct.pack("<I", len(F)))
        f.write(rec.tobytes())


def write_dxf(path, p):
    xs = np.linspace(p["r_root"], p["L"], 1200)
    top = [(x, float(y_le(x, p))) for x in xs]
    bot = [(x, float(y_te(x, p))) for x in xs[::-1]]
    arm = top + bot
    pts = arm + [(-x, -y) for x, y in arm]
    out = ["0\nSECTION\n2\nENTITIES"]
    out.append("0\nPOLYLINE\n8\nPLANFORM\n66\n1\n70\n1")
    for x, y in pts:
        out.append(f"0\nVERTEX\n8\nPLANFORM\n10\n{x:.4f}\n20\n{y:.4f}\n30\n0.0")
    out.append("0\nSEQEND\n8\nPLANFORM")
    out.append(f"0\nCIRCLE\n8\nHUB\n10\n0\n20\n0\n30\n0\n40\n{p['hub_d']/2}")
    out.append(f"0\nCIRCLE\n8\nBORE\n10\n0\n20\n0\n30\n0\n40\n{p['bore_d']/2}")
    out.append("0\nENDSEC\n0\nEOF")
    open(path, "w").write("\n".join(out) + "\n")


VARIANTS = {
    "blade_A_baseline": (False, False),
    "blade_B_tubercle": (True, False),
    "blade_C_hybrid": (True, True),
}

if __name__ == "__main__":
    rho = 7.93e-3  # g/mm^3, AISI 304
    for name, (tub, ser) in VARIANTS.items():
        V, F, info = assemble(P, tub, ser)
        write_stl(os.path.join(OUT, name + ".stl"), V, F, name)
        bb = V.min(0), V.max(0)
        print(f"{name}: tris={len(F)} watertight_shells={info['watertight']} "
              f"arm_vol={info['arm_vol']:.1f} mm3 hub_vol={info['hub_vol']:.1f} mm3 "
              f"approx_mass(2 arms+hub, overlap not removed)={(2*info['arm_vol']+info['hub_vol'])*rho:.2f} g "
              f"bbox_x={bb[1][0]-bb[0][0]:.1f} y={bb[1][1]-bb[0][1]:.1f} z={bb[1][2]-bb[0][2]:.1f}")
    write_dxf(os.path.join(OUT, "blade_planform.dxf"), P)
    print("dxf written")

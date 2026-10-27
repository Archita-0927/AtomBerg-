"""
CadQuery version of the tubercle blade -> produces a true solid and exports STEP.
NOT executed in the session that created it (no CAD kernel available there) - run it once on
your machine:   pip install cadquery   then   python blade_cadquery.py
Differences from the STL generator: tip lift is a sharp bend (no bend radius) and the
thickness edges are filleted only if the fillet succeeds.
Units: mm. Z = rotation axis, +Y at +X arm = leading edge (counter-clockwise rotation).
"""
import math
import cadquery as cq

T = 1.5
L, R_ROOT = 40.0, 4.1
W_ROOT, W_TIP, TAPER_START = 8.0, 6.0, 12.0
TUB_AMP, TUB_WL = 1.5, 4.0
SER_AMP, SER_WL = 1.0, 3.0
X0, X1, BLEND = 12.0, 38.0, 3.0
PITCH = 17.5                      # leading edge down
BEND_START, BEND_DEG = 28.0, 25.0  # tip lift
HUB_D, HUB_H, BORE_D = 18.0, 10.0, 8.0
HYBRID = True                     # False -> tubercles only; set TUB_AMP = SER_AMP = 0 for the control blade


def sstep(t):
    t = min(max(t, 0.0), 1.0)
    return t * t * (3 - 2 * t)


def w(x):
    return W_ROOT + (W_TIP - W_ROOT) * min(max((x - TAPER_START) / (L - TAPER_START), 0), 1)


def env(x):
    return sstep((x - X0) / BLEND) * (1 - sstep((x - (X1 - BLEND)) / BLEND))


def y_le(x):
    return w(x) + TUB_AMP * env(x) * (0.5 - 0.5 * math.cos(2 * math.pi * (x - X0) / TUB_WL))


def y_te(x):
    if not HYBRID:
        return -w(x)
    u = (x - X0) / SER_WL
    tri = 1 - abs(2 * (u - math.floor(u)) - 1)
    return -w(x) - SER_AMP * env(x) * tri


n = 600
xs = [R_ROOT + (L - R_ROOT) * i / (n - 1) for i in range(n)]
outline = [(x, y_le(x)) for x in xs] + [(x, y_te(x)) for x in reversed(xs)]

arm = (cq.Workplane("XY").polyline(outline).close().extrude(T / 2, both=True))   # centred on z = 0

# pitch about the arm axis (X), leading edge down
arm = arm.rotate((0, 0, 0), (1, 0, 0), -PITCH)

# tip lift: split at x = BEND_START and rotate the outer piece about the Y axis through that station
cutter = cq.Workplane("XY").box(100, 100, 100, centered=(False, True, True)).translate((BEND_START, 0, 0))
tip = arm.intersect(cutter).rotate((BEND_START, 0, 0), (BEND_START, 1, 0), -BEND_DEG)
root = arm.cut(cutter)
arm = root.union(tip)

arm2 = arm.rotate((0, 0, 0), (0, 0, 1), 180)
hub = (cq.Workplane("XY").circle(HUB_D / 2).circle(BORE_D / 2).extrude(HUB_H).translate((0, 0, -HUB_H / 2)))

blade = hub.union(arm).union(arm2)
try:
    blade = blade.edges("not %CIRCLE").fillet(0.3)      # rounded edges; skip if the kernel refuses
except Exception as e:
    print("fillet skipped:", e)

cq.exporters.export(blade, "blade_hybrid.step")
cq.exporters.export(blade, "blade_hybrid.stl")
print("exported blade_hybrid.step / .stl")

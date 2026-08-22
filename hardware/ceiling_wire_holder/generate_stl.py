#!/usr/bin/env python3
"""
Ceiling wire holder - pass-through cable clamp.

Generates printable STL meshes from parametric primitives using trimesh +
manifold3d (exact boolean CSG). The part is a ceiling-mount plate with a
central drum that the cable runs straight through; the drum is split at the
wire axis into an integral BASE and a bolt-down CAP that pinches the cable.

Run:  python3 generate_stl.py
Out:  stl/base.stl, stl/cap.stl, stl/assembly.stl

All dimensions are millimetres. Keep this in sync with wire_holder.scad.
"""

import os
import numpy as np
import trimesh
from trimesh.creation import box, cylinder
from trimesh.transformations import rotation_matrix, translation_matrix

# ----------------------------------------------------------------------------
# Parameters (mm) -- edit these to fit your cable / screws
# ----------------------------------------------------------------------------
WIRE_DIA   = 6.0    # nominal cable outer diameter
GRIP       = 0.4    # channel is undersized by this so the halves pinch the cable

# Mounting plate
PLATE_W    = 64.0   # X (along the wire run)
PLATE_D    = 46.0   # Y (across)
PLATE_T    = 4.0    # thickness
CORNER_R   = 6.0    # rounded corner radius
MOUNT_INSET = 9.0   # ceiling-screw hole centre inset from each plate edge
MOUNT_CLR  = 4.3    # M4 clearance
MOUNT_HEAD = 8.6    # countersink head dia (flat-head M4)

# Drum (the cable passes along its axis, X)
DRUM_R     = 11.0   # drum radius
DRUM_L     = 26.0   # drum length along the wire

# Screw ears (straddle the parting plane so both halves get material)
EAR_EXT    = 8.0    # how far the ear sticks out past the drum in Y
EAR_LEN    = 16.0   # ear length along X
EAR_H      = 14.0   # ear height, centred on the parting plane (+/- EAR_H/2)

# Cap screws (M3), one each side of the cable
SCREW_TAP  = 2.5    # base pilot/tap hole (self-tapping M3 into plastic)
SCREW_CLR  = 3.4    # cap clearance
SCREW_HEAD = 6.2    # cap countersink head dia (flat-head M3)

SEG        = 96     # cylinder facet count (smooth curves)
EPS        = 0.01

# ----------------------------------------------------------------------------
# Derived
# ----------------------------------------------------------------------------
CHAN_R   = (WIRE_DIA - GRIP) / 2.0          # channel radius (undersized -> pinch)
DRUM_CZ  = PLATE_T + DRUM_R                  # drum axis height = parting plane z
DRUM_TOP = PLATE_T + 2 * DRUM_R
SCREW_Y  = DRUM_R + EAR_EXT / 2.0            # screw centre in Y
EAR_OUT  = DRUM_R + EAR_EXT                  # ear outer edge in Y
EAR_TOP  = DRUM_CZ + EAR_H / 2.0
EAR_BOT  = DRUM_CZ - EAR_H / 2.0

MOUNT_XY = [
    (+(PLATE_W / 2 - MOUNT_INSET), +(PLATE_D / 2 - MOUNT_INSET)),
    (-(PLATE_W / 2 - MOUNT_INSET), +(PLATE_D / 2 - MOUNT_INSET)),
    (+(PLATE_W / 2 - MOUNT_INSET), -(PLATE_D / 2 - MOUNT_INSET)),
    (-(PLATE_W / 2 - MOUNT_INSET), -(PLATE_D / 2 - MOUNT_INSET)),
]

ROT_Y90 = rotation_matrix(np.pi / 2, [0, 1, 0])   # Z-axis cylinder -> X-axis
ROT_X90 = rotation_matrix(np.pi / 2, [1, 0, 0])   # Z-axis cylinder -> Y-axis


def at(mesh, x=0, y=0, z=0):
    m = mesh.copy()
    m.apply_transform(translation_matrix([x, y, z]))
    return m


def x_cyl(radius, length, x=0, y=0, z=0):
    """Cylinder whose axis runs along X, centred at (x,y,z)."""
    c = cylinder(radius=radius, height=length, sections=SEG)
    c.apply_transform(ROT_Y90)
    return at(c, x, y, z)


def z_cyl(radius, length, x=0, y=0, z=0):
    """Cylinder along Z, centred at (x,y,z)."""
    c = cylinder(radius=radius, height=length, sections=SEG)
    return at(c, x, y, z)


def z_cone(r_bottom, r_top, height, x=0, y=0, z=0):
    """Truncated cone along +Z; base at z, apex end at z+height."""
    c = cylinder(radius=1.0, height=height, sections=SEG)  # placeholder, rebuilt below
    # Build cone manually as a linear-radius extrusion.
    ang = np.linspace(0, 2 * np.pi, SEG, endpoint=False)
    bottom = np.column_stack([r_bottom * np.cos(ang), r_bottom * np.sin(ang),
                              np.zeros(SEG)])
    top = np.column_stack([r_top * np.cos(ang), r_top * np.sin(ang),
                           np.full(SEG, height)])
    verts = np.vstack([bottom, top, [0, 0, 0], [0, 0, height]])
    cb, ct = 2 * SEG, 2 * SEG + 1
    faces = []
    for i in range(SEG):
        j = (i + 1) % SEG
        faces.append([i, j, SEG + j])
        faces.append([i, SEG + j, SEG + i])
        faces.append([cb, j, i])            # bottom cap
        faces.append([ct, SEG + i, SEG + j])  # top cap
    m = trimesh.Trimesh(vertices=verts, faces=faces, process=True)
    return at(m, x, y, z)


def rounded_plate():
    """Rounded rectangular plate, bottom face on z=0."""
    w, d, t, r = PLATE_W, PLATE_D, PLATE_T, CORNER_R
    a = box(extents=[w - 2 * r, d, t])
    b = box(extents=[w, d - 2 * r, t])
    parts = [a, b]
    for sx in (+1, -1):
        for sy in (+1, -1):
            parts.append(z_cyl(r, t, sx * (w / 2 - r), sy * (d / 2 - r), 0))
    plate = trimesh.boolean.union(parts, engine="manifold")
    return at(plate, 0, 0, t / 2)  # sit on z=0


def clamp_solid():
    """Drum + screw ears as one solid, before splitting."""
    drum = x_cyl(DRUM_R, DRUM_L, 0, 0, DRUM_CZ)
    ears = box(extents=[EAR_LEN, 2 * EAR_OUT, EAR_H])
    ears = at(ears, 0, 0, DRUM_CZ)
    return trimesh.boolean.union([drum, ears], engine="manifold")


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out = os.path.join(here, "stl")
    os.makedirs(out, exist_ok=True)

    plate = rounded_plate()
    clamp = clamp_solid()

    # Cable channel through the whole drum along X (open both ends).
    channel = x_cyl(CHAN_R, DRUM_L + 4, 0, 0, DRUM_CZ)
    clamp = trimesh.boolean.difference([clamp, channel], engine="manifold")

    # Split at the wire axis (z = DRUM_CZ) into lower (base) and upper (cap)
    # via boolean intersection with half-space boxes (avoids scipy).
    big = max(PLATE_W, PLATE_D, DRUM_TOP) * 3
    half = big / 2
    lower_hs = at(box(extents=[big, big, big]), 0, 0, DRUM_CZ - half)
    upper_hs = at(box(extents=[big, big, big]), 0, 0, DRUM_CZ + half)
    lower = trimesh.boolean.intersection([clamp, lower_hs], engine="manifold")
    upper = trimesh.boolean.intersection([clamp, upper_hs], engine="manifold")

    # ---- BASE = plate + lower drum, minus screw pilots and mount holes ----
    base = trimesh.boolean.union([plate, lower], engine="manifold")
    cuts = []
    for sy in (+1, -1):
        # pilot hole from parting plane down into the plate
        cuts.append(z_cyl(SCREW_TAP / 2, DRUM_CZ + 2 * EPS,
                          0, sy * SCREW_Y, (DRUM_CZ) / 2))
    for (mx, my) in MOUNT_XY:
        cuts.append(z_cyl(MOUNT_CLR / 2, PLATE_T + 2 * EPS, mx, my, PLATE_T / 2))
        # countersink on the underside (room-facing), opening downward
        cuts.append(z_cone(MOUNT_HEAD / 2, MOUNT_CLR / 2, MOUNT_HEAD / 2,
                           mx, my, 0))
    base = trimesh.boolean.difference([base] + cuts, engine="manifold")

    # ---- CAP = upper drum, minus clearance holes + countersinks ----
    cap = upper
    cuts = []
    for sy in (+1, -1):
        cuts.append(z_cyl(SCREW_CLR / 2, EAR_H + 2 * EPS,
                          0, sy * SCREW_Y, DRUM_CZ + EAR_H / 2 - EPS))
        # countersink on the ear top, opening upward
        cuts.append(z_cone(SCREW_HEAD / 2, SCREW_CLR / 2, SCREW_HEAD / 2,
                           0, sy * SCREW_Y, EAR_TOP - SCREW_HEAD / 2))
    cap = trimesh.boolean.difference([cap] + cuts, engine="manifold")

    # ---- Write ----
    for name, mesh in [("base", base), ("cap", cap)]:
        mesh.export(os.path.join(out, f"{name}.stl"))
        print(f"{name+'.stl':14s} watertight={mesh.is_watertight} "
              f"vol={mesh.volume/1000:6.2f} cm^3  tris={len(mesh.faces)}")

    assembly = trimesh.util.concatenate([base, cap])
    assembly.export(os.path.join(out, "assembly.stl"))
    print(f"{'assembly.stl':14s} tris={len(assembly.faces)}")

    bb = base.bounds
    print(f"\nBase footprint: {bb[1][0]-bb[0][0]:.1f} x {bb[1][1]-bb[0][1]:.1f}"
          f" x {bb[1][2]-bb[0][2]:.1f} mm")
    print(f"Cap height: {cap.bounds[1][2]-cap.bounds[0][2]:.1f} mm")
    print(f"Cable channel dia: {2*CHAN_R:.1f} mm (fits ~{WIRE_DIA:.1f} mm cable)")


if __name__ == "__main__":
    main()

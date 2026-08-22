// Ceiling wire holder - pass-through cable clamp
// Parametric OpenSCAD source. Mirrors generate_stl.py.
// Render one part at a time with `part` (or F6 -> Export STL).
//
//   part = "base"     integral mounting plate + lower drum
//   part = "cap"      bolt-down top half that pinches the cable
//   part = "assembly" both, for preview
//   part = "print"    base and cap laid flat, side by side, for a print plate

part = "assembly";   // ["base","cap","assembly","print"]

/* ---------------- Parameters (mm) ---------------- */
wire_dia   = 6.0;    // nominal cable outer diameter
grip       = 0.4;    // channel undersized by this so the halves pinch the cable

plate_w    = 64.0;   // X (along the wire run)
plate_d    = 46.0;   // Y (across)
plate_t    = 4.0;    // thickness
corner_r   = 6.0;    // rounded corner radius
mount_inset = 9.0;   // ceiling-screw hole inset from each plate edge
mount_clr  = 4.3;    // M4 clearance
mount_head = 8.6;    // countersink head dia (flat-head M4)

drum_r     = 11.0;   // drum radius
drum_l     = 26.0;   // drum length along the wire

ear_ext    = 8.0;    // ear stick-out past the drum in Y
ear_len    = 16.0;   // ear length along X
ear_h      = 14.0;   // ear height, centred on the parting plane

screw_tap  = 2.5;    // base pilot hole (self-tapping M3)
screw_clr  = 3.4;    // cap clearance
screw_head = 6.2;    // cap countersink head dia (flat-head M3)

$fn = 96;
eps = 0.01;

/* ---------------- Derived ---------------- */
chan_r  = (wire_dia - grip) / 2;
drum_cz = plate_t + drum_r;          // drum axis height = parting plane
screw_y = drum_r + ear_ext / 2;
ear_top = drum_cz + ear_h / 2;

mount_pts = [
  [ plate_w/2 - mount_inset,  plate_d/2 - mount_inset],
  [-plate_w/2 + mount_inset,  plate_d/2 - mount_inset],
  [ plate_w/2 - mount_inset, -plate_d/2 + mount_inset],
  [-plate_w/2 + mount_inset, -plate_d/2 + mount_inset],
];

/* ---------------- Helpers ---------------- */
module rounded_plate() {
  linear_extrude(plate_t)
    offset(r = corner_r) offset(delta = -corner_r)
      square([plate_w, plate_d], center = true);
}

module clamp_solid() {              // drum + ears, centred on parting plane
  union() {
    translate([0, 0, drum_cz]) rotate([0, 90, 0])
      cylinder(h = drum_l, r = drum_r, center = true);
    translate([0, 0, drum_cz])
      cube([ear_len, 2*(drum_r + ear_ext), ear_h], center = true);
  }
}

module channel() {                  // cable bore along X
  translate([0, 0, drum_cz]) rotate([0, 90, 0])
    cylinder(h = drum_l + 4, r = chan_r, center = true);
}

module base() {
  difference() {
    union() {
      rounded_plate();
      intersection() {               // lower half of the clamp
        clamp_solid();
        translate([0,0,-500+drum_cz]) cube([1000,1000,1000], center=true);
      }
    }
    channel();
    // cap screw pilots
    for (s = [-1, 1])
      translate([0, s*screw_y, drum_cz/2])
        cylinder(h = drum_cz + 2*eps, r = screw_tap/2, center = true);
    // ceiling mount holes + countersinks (heads on the underside)
    for (p = mount_pts) {
      translate([p[0], p[1], plate_t/2])
        cylinder(h = plate_t + 2*eps, r = mount_clr/2, center = true);
      translate([p[0], p[1], 0])
        cylinder(h = mount_head/2, r1 = mount_head/2, r2 = mount_clr/2);
    }
  }
}

module cap() {
  difference() {
    intersection() {                 // upper half of the clamp
      clamp_solid();
      translate([0,0,500+drum_cz]) cube([1000,1000,1000], center=true);
    }
    channel();
    for (s = [-1, 1]) {
      translate([0, s*screw_y, drum_cz])
        cylinder(h = ear_h + 2*eps, r = screw_clr/2);
      translate([0, s*screw_y, ear_top - screw_head/2])
        cylinder(h = screw_head/2, r1 = screw_clr/2, r2 = screw_head/2);
    }
  }
}

/* ---------------- Output ---------------- */
if (part == "base") base();
else if (part == "cap") cap();
else if (part == "assembly") { base(); color("#d98c5f") cap(); }
else if (part == "print") {
  // base flat as-is; cap flipped onto its ear top, next to it
  translate([0, -plate_d/2 - 4, 0]) base();
  translate([0,  drum_r + ear_ext + 4, ear_top])
    rotate([180, 0, 0]) cap();
}

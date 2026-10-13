// TPU snap tray for the Sunton ESP32-3248S035 (3.5 inch CYD, 480x320)
// used by the bambu-screen project.
//
// Closed back, open front. The board drops in from the front and a short
// lip around the rim holds it by the top face of the PCB. TPU stretches, so
// the lip clears the PCB when you flex one long side.
//
// Orientation: X is the long axis, Y the short axis, Z up is the screen.
// Origin is the center of the board. Print face down (back of tray on the
// bed) with no supports.
//
// Every number below is a starting point from published board dimensions
// (101.5 x 54.9 mm PCB). Verify with calipers before the final print:
//   board_len, board_wid, pcb_t, lip_inset, back_clear, and the cutouts.

// ---------- board ----------
board_len   = 101.5;   // PCB length (X), mm
board_wid   = 54.9;    // PCB width (Y), mm
pcb_t       = 1.6;     // PCB thickness
back_clear  = 6.5;     // tallest part under the PCB (JST sockets, USB shells)

// ---------- fit ----------
clear       = 0.4;     // gap per side between PCB edge and tray wall
wall        = 2.0;     // side wall thickness
back_t      = 2.0;     // back plate thickness
lip_inset   = 1.5;     // how far the lip reaches over the PCB top face
lip_t       = 1.4;     // lip thickness (Z)
lip_chamfer = 0.9;     // chamfer on the lip's top inner edge for easy entry
corner_r    = 4;       // outer corner radius

// ---------- edge cutouts ----------
// [edge, center offset along that edge from the board center, width, height]
// edge: "x+" right short edge, "x-" left short edge, "y+" top long edge,
//       "y-" bottom long edge. Height is measured up from the back plate's
//       inside face and runs through the lip when it is tall enough.
// Default: USB-C + micro USB share one short edge; the microSD slot and the
// power / expansion JSTs sit near the other. Measure yours and adjust.
cutouts = [
    ["x+",  0,   36, back_clear + pcb_t + lip_t + 1],   // USB ports, through lip
    ["x-",  0,   30, back_clear + 0.5],                 // SD card + JST plugs
    ["y-", -28,  18, back_clear + 0.5],                 // speaker / P1 power lead
];

// ---------- back features ----------
vent_slots   = true;   // slots behind the ESP32 module and regulator
vent_center  = [-31, 0];   // XY center of the vent field
vent_field   = [24, 26];   // overall size of the vent field
vent_pitch   = 5;
vent_w       = 2;

strap_slots  = true;   // two slots on the back for a hook-and-loop strap
strap_w      = 22;     // strap width
strap_t      = 2.6;    // slot thickness
strap_x      = 47;     // distance of each slot from the center (X)

keyholes     = true;   // two screw keyholes for hanging the tray
keyhole_x    = 8;      // distance from center (X)
keyhole_d    = 7;      // head clearance
keyhole_neck = 3.6;    // shank slot width
keyhole_len  = 7;      // slot length

$fn = 48;

// ---------- derived ----------
in_len  = board_len + 2 * clear;
in_wid  = board_wid + 2 * clear;
out_len = in_len + 2 * wall;
out_wid = in_wid + 2 * wall;
cavity_h = back_clear + pcb_t;            // PCB top face sits here
total_h  = back_t + cavity_h + lip_t;     // outer height

module rrect(l, w, r) {
    rr = min(r, l / 2 - 0.05, w / 2 - 0.05);   // keep slots from collapsing
    offset(r = rr) offset(delta = -rr) square([l, w], center = true);
}

module outer_body() {
    linear_extrude(total_h) rrect(out_len, out_wid, corner_r);
}

module cavity() {
    // main pocket for the board and its underside components
    translate([0, 0, back_t])
        linear_extrude(cavity_h + 0.01)
            rrect(in_len, in_wid, max(corner_r - wall, 0.5));
    // opening above the lip (what the lip does not cover)
    translate([0, 0, back_t + cavity_h - 0.01])
        linear_extrude(lip_t + 1)
            rrect(in_len - 2 * lip_inset, in_wid - 2 * lip_inset,
                  max(corner_r - wall - lip_inset, 0.5));
    // entry chamfer on the lip's inner top edge
    translate([0, 0, total_h - lip_chamfer])
        linear_extrude(height = lip_chamfer + 0.01, scale = [
            (in_len - 2 * lip_inset + 2 * lip_chamfer) / (in_len - 2 * lip_inset),
            (in_wid - 2 * lip_inset + 2 * lip_chamfer) / (in_wid - 2 * lip_inset)])
            rrect(in_len - 2 * lip_inset, in_wid - 2 * lip_inset,
                  max(corner_r - wall - lip_inset, 0.5));
}

module edge_cutout(c) {
    edge = c[0]; off = c[1]; w = c[2]; h = c[3];
    depth = wall + lip_inset + 2;
    z0 = back_t;                                  // from the inside back face
    if (edge == "x+" || edge == "x-") {
        sx = (edge == "x+") ? 1 : -1;
        translate([sx * (in_len / 2 + depth / 2 - 1), off, z0 + h / 2])
            cube([depth, w, h], center = true);
    } else {
        sy = (edge == "y+") ? 1 : -1;
        translate([off, sy * (in_wid / 2 + depth / 2 - 1), z0 + h / 2])
            cube([w, depth, h], center = true);
    }
}

module vents() {
    n = floor(vent_field[0] / vent_pitch);
    for (i = [0 : n - 1])
        translate([vent_center[0] - vent_field[0] / 2 + vent_pitch / 2 + i * vent_pitch,
                   vent_center[1], -1])
            linear_extrude(back_t + 2)
                rrect(vent_w, vent_field[1], vent_w / 2);
}

module straps() {
    for (sx = [-1, 1])
        translate([sx * strap_x, 0, -1])
            linear_extrude(back_t + 2)
                rrect(strap_t, strap_w, strap_t / 2);
}

module keyhole() {
    translate([0, 0, -1]) {
        cylinder(d = keyhole_d, h = back_t + 2);
        translate([0, keyhole_len / 2, 0])
            linear_extrude(back_t + 2) rrect(keyhole_neck, keyhole_len, keyhole_neck / 2);
    }
}

module tray() {
    difference() {
        outer_body();
        cavity();
        for (c = cutouts) edge_cutout(c);
        if (vent_slots) vents();
        if (strap_slots) straps();
        if (keyholes) for (sx = [-1, 1]) translate([sx * keyhole_x, -in_wid / 2 + 12, 0]) keyhole();
    }
}

// Ghost of the PCB for sanity checking in the preview (F5 only, not exported)
module board_ghost() {
    %translate([0, 0, back_t + back_clear])
        linear_extrude(pcb_t) square([board_len, board_wid], center = true);
}

tray();
board_ghost();

echo(str("Outer size: ", out_len, " x ", out_wid, " x ", total_h, " mm"));

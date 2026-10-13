# TPU tray for the 3.5 inch CYD (ESP32-3248S035)

Closed-back snap tray for the bambu-screen board. Open front, lip around the
rim holds the PCB by its top face. Flex one long side to pop the board in or
out.

Files

- `cyd35_tpu_tray.scad` is the source. Every dimension is a variable at the top.
- Export the STL from OpenSCAD (F6, then File > Export) or `openscad -o cyd35_tpu_tray.stl cyd35_tpu_tray.scad`. Default size is 106.3 x 59.7 x 11.5 mm.

Print settings (P1S, TPU 95A)

- Back face down, no supports, no brim needed
- 0.2 mm layers, 3 walls, 4 top/bottom layers, 15 to 20 percent gyroid
- Slow it down: 30 mm/s outer wall, retraction 0.8 mm or less
- Dry the TPU first; wet TPU strings badly on the vent slots

Before the final print, measure the board and update these variables

- `board_len`, `board_wid`: PCB outline (defaults 101.5 x 54.9 from published specs)
- `back_clear`: tallest thing under the PCB (JST sockets, USB shells). Default 6.5
- `lip_inset`: how far the lip reaches over the PCB. It must land on bare PCB,
  not on the display module. Default 1.5
- `cutouts`: USB edge, SD/JST edge, and speaker lead. Which short edge has the
  USB ports depends on how the board sits in the tray, so flip `x+` and `x-`
  if needed. Widths are generous on purpose since TPU is forgiving.

Back features (each can be switched off)

- Vent slots behind the ESP32 module
- Two slots for a 20 mm hook-and-loop strap, to band it to the printer or a shelf
- Two screw keyholes for hanging

A quick way to dial in fit: print just a 10 mm tall slice by replacing the
`tray();` call with `intersection() { tray(); cube([200, 200, 10], center = true); }`,
check the lip and cutouts, then print the full part.

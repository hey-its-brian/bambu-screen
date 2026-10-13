# Fonts

Generated with lv_font_conv for LVGL 8.3 (4 bpp, uncompressed). Regenerate
with the commands below if you change sizes or ranges. Orbitron is the
variable font from google/fonts instantiated at weight 700; Share Tech Mono
is the regular static face.

    lv_font_conv --no-compress --bpp 4 --format lvgl --lv-include lvgl.h \
      --font Orbitron-Bold.ttf -r 0x25,0x2D,0x30-0x39 --size 44 -o deck_orbitron_44.c
    lv_font_conv --no-compress --bpp 4 --format lvgl --lv-include lvgl.h \
      --font Orbitron-Bold.ttf -r 0x25,0x2D,0x30-0x39 --size 32 -o deck_orbitron_32.c
    lv_font_conv --no-compress --bpp 4 --format lvgl --lv-include lvgl.h \
      --font Orbitron-Bold.ttf -r 0x20-0x7E --size 16 -o deck_orbitron_16.c
    lv_font_conv --no-compress --bpp 4 --format lvgl --lv-include lvgl.h \
      --font ShareTechMono-Regular.ttf -r 0x20-0x7E,0xB0 --size 20 -o deck_mono_20.c
    lv_font_conv --no-compress --bpp 4 --format lvgl --lv-include lvgl.h \
      --font ShareTechMono-Regular.ttf -r 0x20-0x7E,0xB0 --size 16 -o deck_mono_16.c

After generating, the text fonts (mono_16, mono_20, orbitron_16) get a
`.fallback` to the matching Montserrat font so LV_SYMBOL glyphs still render.
The fonts are const, so the fallback has to be set in the file, not at runtime.

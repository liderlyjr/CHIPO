#!/usr/bin/env python3
"""Generates components/chipo_tools/chipo_icons.cpp: one 80x80 A8 (alpha-only) icon per CHIPO app.
LVGL recolours them with the app colour, so a single image serves any theme. Run: python3 scripts/gen_icons.py"""
import math
from pathlib import Path
from PIL import Image, ImageDraw

S, N = 4, 80          # supersampling factor and final size
W = N * S
OUT = Path(__file__).resolve().parent.parent / "components" / "chipo_tools" / "chipo_icons.cpp"

def new():
    im = Image.new("L", (W, W), 0)
    return im, ImageDraw.Draw(im)

def p(v):  # 0..80 coordinates -> supersampled pixels
    return v * S

def circle(d, cx, cy, r, fill=255, width=0):
    box = [p(cx - r), p(cy - r), p(cx + r), p(cy + r)]
    if width:
        d.ellipse(box, outline=fill, width=p(width))
    else:
        d.ellipse(box, fill=fill)

def line(d, pts, w=5):
    d.line([(p(x), p(y)) for x, y in pts], fill=255, width=p(w), joint="curve")
    for x, y in (pts[0], pts[-1]):
        circle(d, x, y, w / 2)

def rrect(d, x0, y0, x1, y1, r, fill=255, width=0):
    d.rounded_rectangle([p(x0), p(y0), p(x1), p(y1)], radius=p(r), fill=None if width else fill,
                        outline=fill if width else None, width=p(width) if width else 0)

def poly(d, pts, fill=255):
    d.polygon([(p(x), p(y)) for x, y in pts], fill=fill)

def cut(d, f):  # erase with 0
    f(d, 0)

icons = {}

def icon(name):
    def deco(fn):
        im, d = new()
        fn(d)
        icons[name] = im.resize((N, N), Image.LANCZOS)
        return fn
    return deco

@icon("level")
def _(d):
    rrect(d, 8, 28, 72, 52, 12, width=5)
    circle(d, 40, 40, 7)
    line(d, [(28, 32), (28, 48)], 3); line(d, [(52, 32), (52, 48)], 3)

@icon("gravity")
def _(d):
    circle(d, 40, 40, 31, width=5)
    circle(d, 32, 48, 12)
    line(d, [(46, 34), (60, 20)], 4); poly(d, [(62, 14), (68, 26), (54, 24)])

@icon("horizon")
def _(d):
    circle(d, 40, 40, 31, width=5)
    d.pieslice([p(13), p(13), p(67), p(67)], start=15, end=195, fill=255)
    line(d, [(6, 48), (74, 32)], 4)

@icon("canvas")
def _(d):
    poly(d, [(16, 64), (14, 52), (50, 16), (64, 30), (28, 66)])
    cut(d, lambda dd, f: poly(dd, [(22, 56), (24, 60), (28, 58)], f))
    line(d, [(46, 20), (60, 34)], 3)
    line(d, [(10, 72), (36, 72)], 3)

@icon("weather")
def _(d):
    circle(d, 30, 30, 11)
    for a in range(0, 360, 45):
        r = math.radians(a)
        line(d, [(30 + 17 * math.cos(r), 30 + 17 * math.sin(r)), (30 + 23 * math.cos(r), 30 + 23 * math.sin(r))], 3)
    circle(d, 46, 52, 14); circle(d, 60, 56, 11); circle(d, 32, 58, 10); rrect(d, 28, 52, 62, 68, 8)

@icon("settings")
def _(d):
    for a in range(0, 360, 45):
        r = math.radians(a)
        line(d, [(40 + 22 * math.cos(r), 40 + 22 * math.sin(r)), (40 + 31 * math.cos(r), 40 + 31 * math.sin(r))], 9)
    circle(d, 40, 40, 25)
    cut(d, lambda dd, f: circle(dd, 40, 40, 11, f))

@icon("dice")
def _(d):
    rrect(d, 10, 10, 70, 70, 14, width=5)
    for x, y in ((26, 26), (54, 26), (40, 40), (26, 54), (54, 54)):
        circle(d, x, y, 5)

@icon("coin")
def _(d):
    circle(d, 40, 40, 31, width=5)
    circle(d, 40, 40, 20, width=3)
    line(d, [(40, 28), (40, 52)], 4); line(d, [(33, 34), (47, 34)], 3); line(d, [(33, 46), (47, 46)], 3)

@icon("night")
def _(d):
    circle(d, 38, 40, 28)
    cut(d, lambda dd, f: circle(dd, 52, 32, 24, f))
    circle(d, 60, 56, 3); circle(d, 68, 40, 2)

@icon("drive")
def _(d):
    circle(d, 40, 40, 31, width=5)
    circle(d, 40, 40, 8)
    line(d, [(40, 32), (40, 12)], 5); line(d, [(33, 44), (13, 52)], 5); line(d, [(47, 44), (67, 52)], 5)

@icon("pomodoro")
def _(d):
    circle(d, 40, 46, 26)
    poly(d, [(40, 22), (28, 12), (36, 24)]); poly(d, [(40, 22), (52, 12), (44, 24)])
    poly(d, [(40, 22), (30, 18), (40, 14), (50, 18)])

@icon("clock")
def _(d):
    circle(d, 40, 40, 31, width=5)
    line(d, [(40, 40), (40, 20)], 5); line(d, [(40, 40), (54, 48)], 5)

@icon("timer")
def _(d):
    line(d, [(18, 10), (62, 10)], 5); line(d, [(18, 70), (62, 70)], 5)
    poly(d, [(22, 12), (58, 12), (44, 38), (58, 68), (22, 68), (36, 38)])
    cut(d, lambda dd, f: poly(dd, [(30, 56), (50, 56), (40, 46)], f))

@icon("stopwatch")
def _(d):
    circle(d, 40, 45, 27, width=5)
    rrect(d, 34, 8, 46, 14, 3); line(d, [(40, 14), (40, 18)], 4); line(d, [(58, 20), (63, 15)], 4)
    line(d, [(40, 45), (52, 33)], 5)

@icon("stop")
def _(d):
    rrect(d, 14, 14, 66, 66, 10)

@icon("visualizer")
def _(d):
    for i, h in enumerate((22, 44, 62, 36, 52, 28)):
        rrect(d, 8 + i * 11, 72 - h, 15 + i * 11, 72, 3)

@icon("aquarium")
def _(d):
    poly(d, [(8, 40), (28, 20), (58, 24), (62, 40), (58, 56), (28, 60)])
    poly(d, [(58, 40), (74, 24), (74, 56)])
    cut(d, lambda dd, f: circle(dd, 24, 36, 3.5, f))

@icon("lava")
def _(d):
    circle(d, 30, 50, 17); circle(d, 52, 40, 13); circle(d, 40, 22, 11); circle(d, 56, 60, 8)
    rrect(d, 26, 30, 46, 60, 6)

@icon("dance")
def _(d):
    circle(d, 26, 58, 11); circle(d, 56, 50, 11)
    line(d, [(36, 58), (36, 16)], 5); line(d, [(66, 50), (66, 12)], 5); poly(d, [(36, 12), (66, 6), (66, 20), (36, 26)])

@icon("rain")
def _(d):
    circle(d, 28, 34, 13); circle(d, 46, 28, 16); circle(d, 62, 36, 11); rrect(d, 22, 34, 68, 46, 6)
    for x in (22, 38, 54):
        line(d, [(x + 4, 54), (x - 2, 68)], 4)

@icon("fire")
def _(d):
    poly(d, [(40, 6), (56, 28), (66, 46), (60, 64), (40, 72), (20, 64), (14, 46), (24, 30), (30, 40), (34, 22)])
    circle(d, 40, 56, 12)
    cut(d, lambda dd, f: poly(dd, [(40, 44), (48, 56), (40, 66), (32, 56)], f))

@icon("noise")
def _(d):
    pts = [(6, 40), (14, 40), (20, 20), (28, 60), (36, 12), (44, 66), (52, 28), (60, 50), (66, 40), (74, 40)]
    line(d, pts, 4)

@icon("lamp")
def _(d):
    circle(d, 40, 32, 22)
    rrect(d, 30, 50, 50, 62, 4); line(d, [(32, 68), (48, 68)], 5)
    cut(d, lambda dd, f: line(dd, [(34, 34), (40, 44), (46, 34)], 3))

def emit():
    names = list(icons)
    lines = ['// Generated by scripts/gen_icons.py - do not edit by hand. 80x80 alpha-only icons, recoloured by LVGL.',
             '#include "chipo_icons.h"', '', '#include <cstring>', '', 'namespace {']
    for n in names:
        data = list(icons[n].tobytes())
        lines.append(f'const uint8_t kIcon_{n}[{N * N}] = {{')
        for i in range(0, len(data), 24):
            lines.append('    ' + ', '.join(str(v) for v in data[i:i + 24]) + ',')
        lines.append('};')
    lines += ['', f'constexpr int kIconCount = {len(names)};', 'struct IconEntry { const char* id; const uint8_t* data; };',
              'const IconEntry kIcons[kIconCount] = {']
    lines += [f'    {{"{n}", kIcon_{n}}},' for n in names]
    lines += ['};', '', 'lv_image_dsc_t g_dsc[kIconCount];', 'bool g_ready = false;', '}  // namespace', '',
              'const lv_image_dsc_t* ChipoIcon(const char* id) {',
              '    if (!id) return nullptr;',
              '    if (!g_ready) {',
              '        for (int i = 0; i < kIconCount; ++i) {',
              '            std::memset(&g_dsc[i], 0, sizeof(g_dsc[i]));',
              '            g_dsc[i].header.magic = LV_IMAGE_HEADER_MAGIC;',
              '            g_dsc[i].header.cf = LV_COLOR_FORMAT_A8;',
              f'            g_dsc[i].header.w = {N};', f'            g_dsc[i].header.h = {N};', f'            g_dsc[i].header.stride = {N};',
              f'            g_dsc[i].data_size = {N * N};', '            g_dsc[i].data = kIcons[i].data;',
              '        }', '        g_ready = true;', '    }',
              '    for (int i = 0; i < kIconCount; ++i)', '        if (std::strcmp(kIcons[i].id, id) == 0) return &g_dsc[i];',
              '    return nullptr;', '}', '']
    OUT.write_text('\n'.join(lines), encoding='utf-8', newline='\n')
    print(f'{len(names)} icons -> {OUT} ({OUT.stat().st_size // 1024} KB source)')
    # contact sheet for a visual check
    sheet = Image.new('L', (N * 8 + 18, N * 3 + 8), 0)
    for i, n in enumerate(names):
        sheet.paste(icons[n], (2 + (i % 8) * (N + 2), 2 + (i // 8) * (N + 2)))
    sheet.resize((sheet.width * 2, sheet.height * 2)).save('/tmp/chipo_icons_sheet.png')

emit()

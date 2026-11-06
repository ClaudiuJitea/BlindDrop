#!/usr/bin/env python3
"""Side-by-side before/after sheet for tools/art_capture.sh output.

    python3 -I tools/art_compare.py BEFORE_DIR AFTER_DIR SHEET.png [point ...]

One row per capture point (before | after), with the share of pixels that
changed by more than 8/255 printed beside each name, so "it looks better" is
always paired with "it is actually different". Needs Pillow + numpy.
"""
import sys, os
from PIL import Image, ImageDraw
import numpy as np

def main():
    b, a, out = sys.argv[1:4]
    want = sys.argv[4:]
    names = sorted(f for f in os.listdir(b) if f.endswith('.png') and os.path.exists(os.path.join(a, f)))
    if want:
        names = [n for n in names if any(n.startswith(w) for w in want)]
    if not names:
        sys.exit('no common points')
    tw, th = 640, 360
    sheet = Image.new('RGB', (tw * 2, (th + 18) * len(names)), (12, 14, 18))
    d = ImageDraw.Draw(sheet)
    for i, n in enumerate(names):
        ib = Image.open(os.path.join(b, n)).convert('RGB')
        ia = Image.open(os.path.join(a, n)).convert('RGB')
        if ib.size == ia.size:
            diff = (np.abs(np.asarray(ib, int) - np.asarray(ia, int)).max(axis=2) > 8).mean()
        else:
            diff = float('nan')
        y = i * (th + 18)
        d.text((6, y + 3), f'{n[:-4]}   BEFORE', fill=(200, 200, 200))
        d.text((tw + 6, y + 3), f'AFTER   changed {100 * diff:.1f}% of pixels', fill=(255, 160, 60))
        sheet.paste(ib.resize((tw, th)), (0, y + 18))
        sheet.paste(ia.resize((tw, th)), (tw, y + 18))
        print(f'{n[:-4]:20s} changed {100 * diff:5.1f}%')
    sheet.save(out)

if __name__ == '__main__':
    main()

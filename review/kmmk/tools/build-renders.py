#!/usr/bin/env python3
"""Build the web renders for the KMMK site at true relative scale.

Every image in a group is cut with the same window, centred on the frame
centre (where the renders put the product), so zoom is identical across the
group and nothing shifts between images. 2160x1440 renders are scaled to
1620x1080 first: they share the camera of the 1620 sets at 4/3 resolution.

Run from the repo root:  python3 review/kmmk/tools/build-renders.py
Needs ffmpeg and cwebp.
"""
import os, re, subprocess, tempfile

SRC = 'review/src/for claude'
OUT = 'review/kmmk/img/r'
X = 'untitled folder/updated xlr 2.4'
E = 'untitled folder/exploded 12.4'
C = 'single-connectors-v2 '

# name -> source (relative to SRC)
GROUPS = {
    # Product rows (3/4 views)
    'product': {
        'spxa': f'{X}/SPXA-M/SPXA-M_2.png',
        'spx':  f'{X}/SPX-M/SPX-M_2.png',
        'sp':   f'{C}/SP400/SP400-5.png',
        'sps':  f'{C}/SPS6M/SPS6M-5.png',
        'spdc': 'untitled folder/DC 5.4/SPA2.1DC/SPA2.1DC_5.png',
        'sp35': 'untitled folder/Aux jacks/SPA3.5-TRS/SPA3.5-TRS_5.png',
        'spd':  f'{X}/SDP5/SDP5/SDP5_2.png',
    },
    # Side views for the family line-up
    'lineup': {
        'l-spxa': f'{X}/SPXA-M/SPXA-M_1.png',
        'l-spx':  f'{X}/SPX-M/SPX-M_1.png',
        'l-spd':  f'{X}/SDP5/SDP5/SDP5_1.png',
        'l-sp':   f'{C}/SP400/SP400-1.png',
        'l-sps':  f'{C}/SPS6M/SPS6M-1.png',
        'l-sp35': 'untitled folder/Aux jacks/SPA3.5-TRS/SPA3.5-TRS_1.png',
        'l-spdc': 'untitled folder/DC 5.4/SPA2.1DC/SPA2.1DC_1.png',
    },
    'exploded': {
        'exp-spxa-m':   f'{X}/EPX_VIEW/SPXA-M_EXP.png',
        'exp-spx-f':    f'{X}/EPX_VIEW/SPX-F_EXP.png',
        'exp-spd5':     f'{X}/EPX_VIEW/SPD5_EXP.png',
        'exp-sps7-m':   f'{E}/SPS7-M/SPS7-M_EXP.png',
        'exp-sps4':     f'{E}/SPS4/SPS4_EXP.png',
        'exp-sp21dc':   f'{E}/SP2.1DC_EXP/SP2.1DC_EXP.png',
        'exp-sp35-trs': f'{E}/SP3.5-TRS_EXP/SP3.5-TRS_EXP.png',
    },
    'cable': {
        **{f'c01-{s}': f'01_transp_ light/01.{s}(transp_light).png' for s in 'ABCD'},
        **{f'c02-{s}': f'2026 12.4/02/02.{s}.png' for s in 'ABCD'},
        **{f'c03-{s}': f'2026 12.4/03/03{s}.png' for s in 'ABD'},
        **{f'c04-{s}': f'2026 12.4/04/04{s}.png' for s in 'ABC'},
    },
}
MARGIN = 0.04      # extra room around the largest item, as a share of frame width
OUT_W = {'product': 1400, 'lineup': 900, 'exploded': 1800, 'cable': 1200}


def size(path):
    out = subprocess.run(['sips', '-g', 'pixelWidth', '-g', 'pixelHeight', path],
                         capture_output=True, text=True).stdout
    return tuple(int(v) for v in re.findall(r'pixel(?:Width|Height): (\d+)', out))


def bbox(path, scale):
    vf = (f'scale=iw*{scale}:ih*{scale},' if scale != 1 else '') + 'format=rgba,alphaextract,bbox=min_val=8'
    err = subprocess.run(['ffmpeg', '-hide_banner', '-i', path, '-vf', vf, '-f', 'null', '-'],
                         capture_output=True, text=True).stderr
    m = re.findall(r'x1:(\d+) x2:(\d+) y1:(\d+) y2:(\d+)', err)[-1]
    return tuple(int(v) for v in m)


def build(group, items):
    # Normalise every source to the group's base frame size
    srcs = {}
    for name, rel in items.items():
        p = os.path.join(SRC, rel)
        w, h = size(p)
        srcs[name] = (p, w, h)
    base_w = min(w for _, w, _ in srcs.values())
    base_h = min(h for _, _, h in srcs.values())

    # Symmetric window about the frame centre that fits every item
    cx, cy = base_w / 2, base_h / 2
    hw = hh = 0
    tight = {}
    for name, (p, w, h) in srcs.items():
        s = base_w / w
        x1, x2, y1, y2 = bbox(p, s)
        tight[name] = (x1, x2, y1, y2, s)
        hw = max(hw, cx - x1, x2 - cx)
        hh = max(hh, cy - y1, y2 - cy)
    m = MARGIN * base_w
    win_w = int(min(base_w, 2 * (hw + m))) // 2 * 2
    win_h = int(min(base_h, 2 * (hh + m))) // 2 * 2
    wx, wy = int(cx - win_w / 2), int(cy - win_h / 2)
    print(f'{group}: frame {base_w}x{base_h}, window {win_w}x{win_h}')

    out_w = min(OUT_W[group], win_w)
    for name, (p, w, h) in srcs.items():
        s = base_w / w
        with tempfile.NamedTemporaryFile(suffix='.png') as tmp:
            subprocess.run(['ffmpeg', '-loglevel', 'error', '-y', '-i', p, '-vf',
                            f'format=rgba,scale={base_w}:{base_h},crop={win_w}:{win_h}:{wx}:{wy},scale={out_w}:-2',
                            tmp.name], check=True)
            subprocess.run(['cwebp', '-quiet', '-q', '86', '-alpha_q', '95', tmp.name,
                            '-o', os.path.join(OUT, name + '.webp')], check=True)
    return tight, base_w, base_h


def lineup(tight):
    """Side-by-side family shot: tight crops at one shared scale, centred vertically."""
    order = ['l-spxa', 'l-spx', 'l-spd', 'l-sp', 'l-sps', 'l-sp35', 'l-spdc']
    srcs = {n: os.path.join(SRC, GROUPS['lineup'][n]) for n in order}
    gap = 60
    tallest = max(tight[n][3] - tight[n][2] for n in order) + 20
    inputs, chains, labels = [], [], []
    for i, n in enumerate(order):
        x1, x2, y1, y2, s = tight[n]
        w, h = x2 - x1 + 1, y2 - y1 + 1
        inputs += ['-i', srcs[n]]
        chains.append(f'[{i}]format=rgba,scale=iw*{s}:ih*{s},crop={w}:{h}:{x1}:{y1},format=rgba,'
                      f'pad={w + gap}:{tallest}:{gap // 2}:(oh-ih)/2:color=0x00000000[v{i}]')
        labels.append(f'[v{i}]')
    fc = ';'.join(chains) + ';' + ''.join(labels) + f'hstack=inputs={len(order)}[out]'
    with tempfile.NamedTemporaryFile(suffix='.png') as tmp:
        subprocess.run(['ffmpeg', '-loglevel', 'error', '-y', *inputs, '-filter_complex', fc,
                        '-map', '[out]', '-pix_fmt', 'rgba', tmp.name], check=True)
        subprocess.run(['cwebp', '-quiet', '-q', '88', '-alpha_q', '95', '-resize', '2400', '0',
                        tmp.name, '-o', os.path.join(OUT, 'family.webp')], check=True)
    print('lineup: family.webp')


if __name__ == '__main__':
    os.makedirs(OUT, exist_ok=True)
    for f in os.listdir(OUT):
        os.remove(os.path.join(OUT, f))
    for g in ('product', 'exploded', 'cable'):
        build(g, GROUPS[g])
    # The line-up uses tight per-item crops (same scale), not a shared window
    tight = {}
    for name, rel in GROUPS['lineup'].items():
        p = os.path.join(SRC, rel)
        w, _ = size(p)
        s = 1620 / w
        tight[name] = (*bbox(p, s), s)
    lineup(tight)

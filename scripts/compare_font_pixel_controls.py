"""Quantify synthetic text pixel differences exported by the opt-in corpus test."""
import argparse
import base64
import json
import xml.etree.ElementTree as ET
import zlib


def read_control(path):
    values = [json.loads(p.get('value')) for p in ET.parse(path).getroot().iter('property')
              if p.get('name') == 'font_pixel_controls']
    if len(values) != 1:
        raise ValueError('Expected exactly one synthetic font pixel control')
    control = values[0]
    width, height = control['width'], control['height']
    if not (0 < width <= 4096 and 0 < height <= 4096):
        raise ValueError('Invalid control dimensions')
    size = width * height * 4
    decoded = {}
    for mode, encoded in control['rgba_zlib_base64'].items():
        stream = zlib.decompressobj()
        pixels = stream.decompress(base64.b64decode(encoded, validate=True), size + 1)
        if len(pixels) != size or not stream.eof or stream.unused_data:
            raise ValueError('Invalid pixel payload length or trailing data')
        decoded[mode] = pixels
    gamma = [p.get('value') for p in ET.parse(path).getroot().iter('property')
             if p.get('name') == 'text_gamma_control']
    composite = [p.get('value') for p in ET.parse(path).getroot().iter('property')
                 if p.get('name') == 'text_composite_control']
    return (width, height, control['seed']), decoded, {
        'gamma': gamma[0] if gamma else 'False',
        'composite': composite[0] if composite else 'multiply',
        'canvas_policy':control.get('canvas_policy', 'override')}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('left')
    parser.add_argument('right')
    args = parser.parse_args()
    identity, left, left_controls = read_control(args.left)
    other_identity, right, right_controls = read_control(args.right)
    if (identity != other_identity or left.keys() != right.keys() or not left or
            left_controls['composite'] != right_controls['composite'] or
            left_controls['canvas_policy'] != right_controls['canvas_policy']):
        raise ValueError('Control dimensions, seeds, policies, blend modes, or rendering modes differ')
    width, height, seed = identity
    results = []
    for mode in sorted(left):
        a, b = left[mode], right[mode]
        deltas = [abs(x - y) for x, y in zip(a, b)]
        pixels = [i // 4 for i in range(0, len(a), 4) if a[i:i+4] != b[i:i+4]]
        bounds = ([min(p % width for p in pixels), min(p // width for p in pixels),
                   max(p % width for p in pixels), max(p // width for p in pixels)]
                  if pixels else None)
        results.append({'mode':mode, 'different_pixels':len(pixels),
                        'different_channels':sum(d != 0 for d in deltas),
                        'different_alpha':sum(d != 0 for d in deltas[3::4]),
                        'max_absolute_delta':max(deltas),
                        'mean_absolute_delta':sum(deltas) / len(deltas),
                        'difference_bounds_inclusive':bounds})
    print(json.dumps({'width':width, 'height':height, 'seed':seed,
                      'left_controls':left_controls, 'right_controls':right_controls,
                      'results':results}, indent=2))
    return int(any(row['different_pixels'] for row in results))


if __name__ == '__main__':
    raise SystemExit(main())

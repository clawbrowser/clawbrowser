"""Compare protected Canvas observations from two pytest JUnit reports.

This is a cross-machine diagnostic, not proof of identical test inputs: callers
must record candidate hashes, fixture versions and host architectures separately.
Exit 1 means differing comparable hashes; exit 2 means no comparable evidence.
"""
import argparse
import json
import xml.etree.ElementTree as ET


def observations(path):
    result = {}
    for case in ET.parse(path).getroot().iter('testcase'):
        name = case.get('name', '')
        if '[override]' not in name:
            continue
        for prop in case.findall('./properties/property'):
            if prop.get('name') not in ('cross_context_control', 'color_format_matrix', 'png_format_matrix', 'raster_primitive_matrix'):
                continue
            value = json.loads(prop.get('value'))
            for row in value['observations']:
                if 'hashes' in row:
                    for key, digest in row['hashes'].items():
                        identity = (name, json.dumps({'args': row['args'], 'path': key}, sort_keys=True))
                        result.setdefault(identity, set()).add(digest)
                else:
                    fields = {k: v for k, v in row.items() if k != 'hash'}
                    identity = (name, json.dumps(fields, sort_keys=True))
                    result.setdefault(identity, set()).add(row['hash'])
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('left')
    parser.add_argument('right')
    args = parser.parse_args()
    left, right = observations(args.left), observations(args.right)
    common = sorted(left.keys() & right.keys())
    different = [key for key in common if left[key] != right[key]]
    print(json.dumps({
        'comparable': len(common), 'different': len(different),
        'left_only': len(left.keys() - right.keys()),
        'right_only': len(right.keys() - left.keys()),
        'differences': [{'test': name, 'settings': json.loads(settings)}
                        for name, settings in different],
    }, indent=2))
    return 2 if not common else int(bool(different))


if __name__ == '__main__':
    raise SystemExit(main())

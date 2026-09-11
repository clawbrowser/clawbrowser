"""Prototype-only font asset staging; not wired into product startup."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
from xml.sax.saxutils import escape


def stage(manifest_path, chromium, destination):
    manifest = json.loads(manifest_path.read_text())
    if manifest.get('release_ready') is not False:
        raise ValueError('only an explicitly non-release prototype is accepted')
    revision = subprocess.check_output(
        ['git', '-C', str(chromium), 'rev-parse', 'HEAD'], text=True).strip()
    if revision != manifest['chromium_revision']:
        raise ValueError('Chromium revision does not match manifest')
    if manifest['source_directory'] != 'third_party/test_fonts/test_fonts':
        raise ValueError('unexpected source directory')
    source = chromium / manifest['source_directory']
    vendor = manifest_path.absolute().parent / 'font-vendor-prototype'
    names = set()
    files = []
    for entry in manifest['fonts']:
        name = entry['file']
        if not re.fullmatch(r'[A-Za-z0-9_-]+(?:\.otf)?\.(ttf|otf|ttc)', name) or name in names:
            raise ValueError('invalid or duplicate font filename')
        names.add(name)
        source_kind = entry.get('source', 'chromium')
        if source_kind not in ('chromium', 'vendor'):
            raise ValueError('unknown asset source')
        data = ((vendor if source_kind == 'vendor' else source) / name).read_bytes()
        if hashlib.sha256(data).hexdigest() != entry['sha256']:
            raise ValueError('font checksum mismatch: ' + name)
        files.append((name, data))
    if not files:
        raise ValueError('empty catalog')
    license_data = (source.parent / 'LICENSE').read_bytes()
    vendor_license = ((vendor / 'NOTO-LICENSE').read_bytes()
                      if any(entry.get('source') == 'vendor' for entry in manifest['fonts']) else None)
    manifest_text = json.dumps(manifest, indent=2)
    manifest_digest = hashlib.sha256(manifest_text.encode()).hexdigest()
    # Validate every input before creating output. Existing output is never reused
    # or overwritten. Any later I/O failure leaves a visibly incomplete directory.
    destination = destination.absolute()
    destination.mkdir(mode=0o755)
    fonts = destination / 'fonts'
    fonts.mkdir()
    for name, data in files:
        with (fonts / name).open('xb') as output:
            output.write(data)
    (destination / 'UPSTREAM-LICENSE').write_bytes(license_data)
    if vendor_license is not None:
        (destination / 'NOTO-LICENSE').write_bytes(vendor_license)
    (destination / 'manifest.json').write_text(manifest_text)
    lines = ['<?xml version="1.0"?>',
             '<!DOCTYPE fontconfig SYSTEM "urn:fontconfig:fonts.dtd">',
             '<fontconfig>', '  <reset-dirs/>',
             '  <dir>' + escape(str(fonts)) + '</dir>',
             '  <cachedir>' + escape(str(destination / 'cache')) + '</cachedir>']
    for generic, family in manifest['generics'].items():
        lines.append('  <alias binding="strong"><family>' + escape(generic) +
                     '</family><prefer><family>' + escape(family) +
                     '</family></prefer></alias>')
    for family in manifest['fallback']:
        lines.append('  <match target="pattern"><edit name="family" mode="append" binding="weak">'
                     '<string>' + escape(family) + '</string></edit></match>')
    lines.append('</fontconfig>')
    (destination / 'fonts.conf').write_text('\n'.join(lines) + '\n')
    # Written last: absence means staging was interrupted; never launch from it.
    (destination / 'STAGED').write_text(manifest['catalog_id'] + '\n' + manifest_digest + '\n')
    return {'catalog': manifest['catalog_id'], 'fonts': len(files),
            'bytes': sum(len(data) for _, data in files), 'manifestSha256': manifest_digest,
            'releaseReady': False}


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--manifest', required=True, type=Path)
    parser.add_argument('--chromium', required=True, type=Path)
    parser.add_argument('--destination', required=True, type=Path)
    args = parser.parse_args()
    print(json.dumps(stage(args.manifest, args.chromium, args.destination)))

"""Pinned catalog entries for portable and versioned Windows installer layouts."""
import argparse
import hashlib
import json
from pathlib import Path, PureWindowsPath
import re

BEGIN = '# BEGIN CLAWBROWSER FONT CATALOG'
END = '# END CLAWBROWSER FONT CATALOG'
METADATA = ('manifest.json', 'STAGED', 'UPSTREAM-LICENSE', 'NOTO-LICENSE', 'fonts.conf')


def specification(manifest):
    spec = json.loads(manifest.read_text(encoding='utf-8'))
    if not re.fullmatch(r'[A-Za-z0-9_-]+', spec['catalog_id']):
        raise ValueError('unsafe catalog id')
    seen = set()
    for font in spec['fonts']:
        name = font['file']
        if (not re.fullmatch(r'[A-Za-z0-9_-]+(?:\.otf)?\.(?:ttf|otf|ttc)', name)
                or name.casefold() in seen
                or not re.fullmatch(r'[0-9a-f]{64}', font['sha256'])):
            raise ValueError('invalid or duplicate Windows font entry')
        seen.add(name.casefold())
    if not seen:
        raise ValueError('empty catalog')
    return spec


def relative_files(spec):
    return list(METADATA) + ['fonts/' + f['file'] for f in spec['fonts']]


def installer_text(text, spec):
    # Preserve unrelated sections; replace only our own bounded generated block.
    text = text.replace('\r\n', '\n')
    if text.count(BEGIN) != text.count(END) or text.count(BEGIN) > 1:
        raise ValueError('malformed generated catalog block')
    if BEGIN in text:
        text, count = re.subn(re.escape(BEGIN) + r'\n.*?' + re.escape(END) + r'\n?',
                              '', text, flags=re.S)
        if count != 1:
            raise ValueError('malformed generated catalog block')
    if text.count('[GENERAL]\n') != 1:
        raise ValueError('installer requires one GENERAL section')
    if re.search(r'^clawbrowser-fonts[\\/]', text, re.M | re.I):
        raise ValueError('unmanaged catalog entries already exist')
    prefix = PureWindowsPath('clawbrowser-fonts', spec['catalog_id'])
    entries = []
    for relative in relative_files(spec):
        path = prefix / relative
        entries.append(str(path) + ': %(VersionDir)s\\' + str(path.parent) + '\\')
    block = BEGIN + '\n' + '\n'.join(entries) + '\n' + END + '\n'
    return text.replace('[GENERAL]\n', '[GENERAL]\n' + block, 1)


def validated_payload(manifest, chromium, source):
    spec = specification(manifest)
    if source.is_symlink() or not source.is_dir():
        raise ValueError('missing or linked catalog')
    expected = set(relative_files(spec))
    actual = set()
    payload = {}
    for path in source.rglob('*'):
        if path.is_symlink():
            raise ValueError('linked catalog asset')
        relative = path.relative_to(source).as_posix()
        if path.is_dir():
            if relative != 'fonts':
                raise ValueError('unexpected catalog directory')
            continue
        actual.add(relative)
        payload[relative] = path.read_bytes()
    if actual != expected:
        raise ValueError('missing or unexpected catalog files')
    canonical = json.dumps(spec, indent=2).encode('utf-8')
    digest = hashlib.sha256(canonical).hexdigest()
    if payload['manifest.json'] != canonical:
        raise ValueError('manifest mismatch')
    if payload['STAGED'] != (spec['catalog_id'] + '\n' + digest + '\n').encode():
        raise ValueError('staging marker mismatch')
    for font in spec['fonts']:
        if hashlib.sha256(payload['fonts/' + font['file']]).hexdigest() != font['sha256']:
            raise ValueError('font checksum mismatch')
    notices = {'UPSTREAM-LICENSE': chromium / 'third_party/test_fonts/LICENSE',
               'NOTO-LICENSE': manifest.parent / 'font-vendor-prototype/NOTO-LICENSE'}
    for name, path in notices.items():
        if payload[name] != path.read_bytes():
            raise ValueError('font notice mismatch')
    return spec, payload


def stage(manifest, chromium, output, destination):
    spec = specification(manifest)
    relative_root = Path('clawbrowser-fonts') / spec['catalog_id']
    _, payload = validated_payload(manifest, chromium, output / relative_root)
    target = destination / relative_root
    # GN runtime_deps may have copied some assets already. Accept exact bytes,
    # never overwrite a different file, and never follow package symlinks.
    for path in (destination, destination / 'clawbrowser-fonts', target, target / 'fonts'):
        if path.is_symlink():
            raise ValueError('linked package directory')
    for relative, data in payload.items():
        path = target / relative
        if path.is_symlink() or (path.exists() and (not path.is_file() or path.read_bytes() != data)):
            raise ValueError('conflicting packaged catalog asset')
    for relative, data in payload.items():
        path = target / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        if not path.exists():
            with path.open('xb') as stream:
                stream.write(data)
    validated_payload(manifest, chromium, target)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('mode', choices=['installer', 'stage', 'verify'])
    parser.add_argument('--manifest', required=True, type=Path)
    parser.add_argument('--release', type=Path)
    parser.add_argument('--chromium', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--destination', type=Path)
    args = parser.parse_args()
    if args.mode == 'installer':
        if not args.release:
            parser.error('--release is required')
        args.release.write_bytes(installer_text(args.release.read_text(encoding='utf-8'),
                                               specification(args.manifest)).encode('utf-8'))
    elif args.mode == 'verify':
        if not all([args.chromium, args.output]):
            parser.error('--chromium and --output are required')
        spec = specification(args.manifest)
        validated_payload(args.manifest, args.chromium,
                          args.output / 'clawbrowser-fonts' / spec['catalog_id'])
    else:
        if not all([args.chromium, args.output, args.destination]):
            parser.error('--chromium, --output and --destination are required')
        stage(args.manifest, args.chromium, args.output, args.destination)

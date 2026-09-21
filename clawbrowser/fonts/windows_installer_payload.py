"""Verify the font catalog embedded in a Windows mini_installer, without running it.

Only the pinned full B7 installer format is supported. Differential/uncompressed
installers fail closed. Archives are streamed, never extracted to archive paths.
"""
import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

from windows_package import specification, validated_payload


def copy_packed_resource(installer, destination):
    if os.name != 'nt':
        raise ValueError('PE resource inspection requires Windows')
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel.LoadLibraryExW.argtypes = [wintypes.LPCWSTR, wintypes.HANDLE, wintypes.DWORD]
    kernel.LoadLibraryExW.restype = wintypes.HMODULE
    kernel.FindResourceW.argtypes = [wintypes.HMODULE, wintypes.LPCWSTR, wintypes.LPCWSTR]
    kernel.FindResourceW.restype = wintypes.HANDLE
    kernel.SizeofResource.argtypes = [wintypes.HMODULE, wintypes.HANDLE]
    kernel.SizeofResource.restype = wintypes.DWORD
    kernel.LoadResource.argtypes = [wintypes.HMODULE, wintypes.HANDLE]
    kernel.LoadResource.restype = wintypes.HANDLE
    kernel.LockResource.argtypes = [wintypes.HANDLE]
    kernel.LockResource.restype = ctypes.c_void_p
    kernel.FreeLibrary.argtypes = [wintypes.HMODULE]
    kernel.FreeLibrary.restype = wintypes.BOOL
    # LOAD_LIBRARY_AS_DATAFILE: no DllMain, imports or executable entry point.
    module = kernel.LoadLibraryExW(str(installer.resolve()), None, 0x00000002)
    if not module:
        raise ValueError('cannot map installer as a data file')
    try:
        # RC uppercases the unquoted name emitted by create_installer_archive.py.
        resource = kernel.FindResourceW(module, 'CHROME.PACKED.7Z', 'B7')
        if not resource:
            raise ValueError('missing full B7 CHROME.PACKED.7Z resource')
        size = kernel.SizeofResource(module, resource)
        if not 0 < size <= 2 * 1024**3:
            raise ValueError('invalid packed resource size')
        handle = kernel.LoadResource(module, resource)
        pointer = kernel.LockResource(handle) if handle else None
        if not pointer:
            raise ValueError('cannot read packed resource')
        digest = hashlib.sha256()
        with destination.open('xb') as stream:
            for offset in range(0, size, 1024**2):
                chunk = ctypes.string_at(pointer + offset, min(1024**2, size - offset))
                digest.update(chunk)
                stream.write(chunk)
        return digest.hexdigest()
    finally:
        kernel.FreeLibrary(module)


def archive_entries(seven_zip, archive):
    result = subprocess.run([str(seven_zip), 'l', '-slt', '-ba', '-sccUTF-8',
                             str(archive)], check=True, capture_output=True,
                            timeout=120, stdin=subprocess.DEVNULL)
    return parse_entries(result.stdout.decode('utf-8'))


def parse_entries(text):
    entries = {}
    seen = set()
    for block in re.split(r'\n\s*\n', text.replace('\r\n', '\n').strip()):
        if not block:
            continue
        fields = {}
        for line in block.splitlines():
            key, separator, value = line.partition(' = ')
            if not separator or key in fields:
                raise ValueError('malformed 7z listing')
            fields[key] = value
        name = fields.get('Path', '').replace('\\', '/')
        parts = name.split('/')
        if (not name or any(p in ('', '.', '..') for p in parts)
                or re.search(r'[:*?\x00-\x1f]', name) or name.casefold() in seen):
            raise ValueError('unsafe or duplicate archive path')
        seen.add(name.casefold())
        attributes = fields.get('Attributes', '')
        if (fields.get('Encrypted') == '+' or fields.get('Symbolic Link')
                or fields.get('Hard Link') or re.search(r'(?:^|\s)l[rwx-]{9}', attributes)):
            raise ValueError('encrypted or linked archive member')
        if fields.get('Folder') == '+' or attributes.startswith('D'):
            continue
        size = fields.get('Size', '')
        if not size.isdigit():
            raise ValueError('missing archive member size')
        entries[name] = int(size)
    return entries


def stream_member(seven_zip, archive, member, destination, expected_size):
    # -so writes to our own chosen file, not any path supplied by the archive.
    with destination.open('xb') as stream:
        subprocess.run([str(seven_zip), 'x', '-so', '-y', str(archive), member],
                       check=True, stdout=stream, stderr=subprocess.PIPE,
                       stdin=subprocess.DEVNULL, timeout=600)
    if destination.stat().st_size != expected_size:
        raise ValueError('archive member size mismatch')


def catalog_members(entries, catalog_id, payload):
    roots = set()
    catalog_paths = set()
    for name in entries:
        parts = name.split('/')
        if 'clawbrowser-fonts' in [p.casefold() for p in parts]:
            if (len(parts) < 5 or parts[0] != 'Chrome-bin'
                    or not re.fullmatch(r'\d+\.\d+\.\d+\.\d+', parts[1])
                    or parts[2:4] != ['clawbrowser-fonts', catalog_id]):
                raise ValueError('catalog is not in the installed version directory')
            roots.add('/'.join(parts[:4]) + '/')
            catalog_paths.add(name)
    if len(roots) != 1:
        raise ValueError('missing or ambiguous installed catalog')
    root = roots.pop()
    expected = {root + name: data for name, data in payload.items()}
    if catalog_paths != expected.keys():
        raise ValueError('missing or unexpected embedded catalog files')
    version = root.split('/')[1]
    if (f'Chrome-bin/{version}/chrome.dll' not in entries
            or 'Chrome-bin/clawbrowser.exe' not in entries):
        raise ValueError('catalog must be adjacent to the installed browser module')
    for name, data in expected.items():
        if entries[name] != len(data):
            raise ValueError('embedded font asset size mismatch')
    return version, expected


def verify_packed(seven_zip, packed, catalog_id, payload, scratch):
    outer = archive_entries(seven_zip, packed)
    if set(outer) != {'chrome.7z'} or not 0 < outer['chrome.7z'] <= 8 * 1024**3:
        raise ValueError('not a full nested chrome.7z payload')
    inner = scratch / 'chrome.7z'
    stream_member(seven_zip, packed, 'chrome.7z', inner, outer['chrome.7z'])
    version, members = catalog_members(archive_entries(seven_zip, inner), catalog_id, payload)
    # The inner archive is uncompressed (-mx0) in Chromium's installer builder;
    # reading each exact member is bounded and avoids archive path extraction.
    for index, (name, expected) in enumerate(sorted(members.items())):
        asset = scratch / f'checked-{index}.bin'
        stream_member(seven_zip, inner, name, asset, len(expected))
        if hashlib.sha256(asset.read_bytes()).digest() != hashlib.sha256(expected).digest():
            raise ValueError('embedded font asset checksum mismatch: ' + name)
        asset.unlink()  # Only this verifier's own numbered temporary asset.
    return {'version': version, 'catalog_id': catalog_id, 'verified_files': len(members)}


def verify_installer(installer, seven_zip, manifest, chromium, output):
    spec = specification(manifest)
    _, payload = validated_payload(manifest, chromium,
                                   output / 'clawbrowser-fonts' / spec['catalog_id'])
    with tempfile.TemporaryDirectory(prefix='clawbrowser-installer-check-') as temporary:
        scratch = Path(temporary)
        packed = scratch / 'chrome.packed.7z'
        digest = copy_packed_resource(installer, packed)
        report = verify_packed(seven_zip, packed, spec['catalog_id'], payload, scratch)
        report['packed_resource_sha256'] = digest
        return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ('installer', 'seven-zip', 'manifest', 'chromium', 'output'):
        parser.add_argument('--' + option, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(verify_installer(args.installer, args.seven_zip, args.manifest,
                                     args.chromium, args.output), sort_keys=True))

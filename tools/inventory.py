"""Read-only executable baseline. No binary copying, memory writes, or asset extraction."""
import argparse
import ctypes
import hashlib
import json
import pathlib
import re
import struct
from datetime import datetime, timezone


def inspect(executable):
    executable = pathlib.Path(executable).resolve(strict=True)
    digest = hashlib.sha256()
    with executable.open('rb') as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b''):
            digest.update(block)
        handle.seek(0)
        header = handle.read(4096)
    if header[:2] != b'MZ':
        raise ValueError('Not a PE executable')
    pe = struct.unpack_from('<I', header, 0x3c)[0]
    with executable.open('rb') as handle:
        handle.seek(pe)
        nt = handle.read(4096)
    if nt[:4] != b'PE\0\0':
        raise ValueError('Invalid PE signature')
    machine, section_count = struct.unpack_from('<HH', nt, 4)
    optional_size = struct.unpack_from('<H', nt, 20)[0]
    magic = struct.unpack_from('<H', nt, 24)[0]
    if machine != 0x8664 or magic != 0x20b:
        raise ValueError('Expected Windows x64 PE32+')
    entry = struct.unpack_from('<I', nt, 40)[0]
    image_base = struct.unpack_from('<Q', nt, 48)[0]
    sections = []
    for index in range(section_count):
        pos = 24 + optional_size + index * 40
        name, vsize, rva, size, offset = struct.unpack_from('<8sIIII', nt, pos)
        sections.append(dict(name=name.rstrip(b'\0').decode('ascii', 'replace'), rva=hex(rva),
                             virtual_size=vsize, file_offset=offset, file_size=size))
    version_dll = ctypes.WinDLL('version', use_last_error=True)
    unused = ctypes.c_ulong()
    size = version_dll.GetFileVersionInfoSizeW(str(executable), ctypes.byref(unused))
    if not size:
        raise ctypes.WinError(ctypes.get_last_error())
    data = ctypes.create_string_buffer(size)
    if not version_dll.GetFileVersionInfoW(str(executable), 0, size, data):
        raise ctypes.WinError(ctypes.get_last_error())
    pointer = ctypes.c_void_p()
    length = ctypes.c_uint()
    if not version_dll.VerQueryValueW(data, '\\', ctypes.byref(pointer), ctypes.byref(length)):
        raise ctypes.WinError(ctypes.get_last_error())
    fixed = ctypes.cast(pointer, ctypes.POINTER(ctypes.c_uint32))
    ms, ls = fixed[2], fixed[3]
    version = f'{ms >> 16}.{ms & 65535}.{ls >> 16}.{ls & 65535}'
    manifest = executable.parent.parent.parent / 'appmanifest_287700.acf'
    build = None
    if manifest.exists():
        match = re.search(r'"buildid"\s+"(\d+)"', manifest.read_text(encoding='utf-8'))
        if match:
            build = match[1]
    return dict(schema=1, observed_utc=datetime.now(timezone.utc).isoformat(),
                executable=str(executable), sha256=digest.hexdigest(), bytes=executable.stat().st_size,
                file_version=version, steam_build_id=build, machine='x64', image_base=hex(image_base),
                entry_rva=hex(entry), sections=sections,
                verified_engine_hooks=[], headset_acceptance='unproven')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=pathlib.Path)
    parser.add_argument('--out', type=pathlib.Path, required=True)
    args = parser.parse_args()
    report = inspect(args.executable)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f"{report['file_version']} SHA256={report['sha256']} -> {args.out.resolve()}")

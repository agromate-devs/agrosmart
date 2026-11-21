#!/usr/bin/env python3
"""Collect the flashable esp32c5 images of a build into a release folder.

Writes, into the output directory:

  agrosmart-esp32c5-<name>.bin   one image per flash region
  manifest.json                  chip, flash settings, offset/hash of each image and
                                 release info (tag, page URL, publication date)
  SHA256SUMS                     checksums of every file above

manifest.json is what the web flasher on the Agromate website reads.
Offsets and build files are hardcoded: keep them in sync with partitions.csv
and .github/release-notes.md.

Usage: tools/package_release.py <build_dir> <out_dir> <version>
"""

import hashlib
import json
import os
import shutil
import sys
from datetime import datetime, timezone
from pathlib import Path

# GITHUB_REPOSITORY is set by GitHub Actions
REPO = os.environ.get('GITHUB_REPOSITORY', 'agromate-devs/agrosmart')
CHIP = 'esp32c5'
FLASH = {'flash_mode': 'dio', 'flash_size': '4MB', 'flash_freq': '80m'}
FILE_TEMPLATE = 'agrosmart-' + CHIP + '-{}.bin'

# Flash regions, in flash order
NAMES = ['bootloader', 'partition-table', 'ota-data', 'app']
OFFSETS = [0x2000, 0x8000, 0xE000, 0x10000]
SOURCES = [
    'bootloader/bootloader.bin',
    'partition_table/partition-table.bin',
    'ota_data_initial.bin',
    'agrosmart.bin',
]


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    build_dir, out_dir, version = Path(sys.argv[1]), Path(sys.argv[2]), sys.argv[3]
    out_dir.mkdir(parents=True, exist_ok=True)

    parts = []
    for name, offset, source in zip(NAMES, OFFSETS, SOURCES):
        dst = out_dir / FILE_TEMPLATE.format(name)
        shutil.copyfile(build_dir / source, dst)
        parts.append(
            {
                'name': name,
                'file': dst.name,
                'offset': offset,
                'size': dst.stat().st_size,
                'sha256': sha256(dst),
            }
        )

    manifest = {
        'name': 'agrosmart',
        'version': version,
        'chip': CHIP,
        'flash': FLASH,
        'parts': parts,
        'release': {
            'tag': version,
            'url': f'https://github.com/{REPO}/releases/tag/{version}',
            'published_at': datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ'),
        },
    }
    (out_dir / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')

    files = sorted(p for p in out_dir.iterdir() if p.name != 'SHA256SUMS')
    (out_dir / 'SHA256SUMS').write_text(''.join(f'{sha256(p)}  {p.name}\n' for p in files))

    print(json.dumps(manifest, indent=2))


if __name__ == '__main__':
    main()

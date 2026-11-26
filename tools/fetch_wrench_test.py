#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Fetch the pinned Contextual Wrench and Fire plugin used by the offline test."""

import argparse
import hashlib
import io
from pathlib import Path
import urllib.request
from zipfile import ZipFile

RELEASE_URL = "https://github.com/Joebannerlord/openRAC-RC2-contextual-wrench-throw/releases/download"
VERSION = "0.1.2"
ARCHIVES = {
    "windows-x86_64": (
        "rac2_contextual_wrench_012.dll",
        "03d86ad9ae568987e24632d0c12559e3f283e0b20c23cc8f9b304389aaae565b",
    ),
    "linux-x86_64": (
        "rac2_contextual_wrench_012.so",
        "165aad972f06906a08e4c638b43f20081cb43ba069683429c5709023f40808cf",
    ),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("platform", choices=ARCHIVES)
    platform = parser.parse_args().platform
    library, sha256 = ARCHIVES[platform]
    archive_name = f"rac2-contextual-wrench-{VERSION}-{platform}.zip"
    dest = Path(__file__).resolve().parents[1] / ".test-deps" / platform / library
    with urllib.request.urlopen(f"{RELEASE_URL}/v{VERSION}/{archive_name}", timeout=60) as response:
        archive = response.read()
    if hashlib.sha256(archive).hexdigest() != sha256:
        raise SystemExit("Contextual Wrench and Fire archive did not match the pinned SHA-256")
    with ZipFile(io.BytesIO(archive)) as package:
        plugin = package.read(f"lib/{platform}/{library}")
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_bytes(plugin)
    print(dest)


if __name__ == "__main__":
    main()

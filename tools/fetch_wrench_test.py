#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Fetch the pinned Contextual Wrench and Fire DLL used by the offline test."""

import hashlib
import io
from pathlib import Path
import urllib.request
from zipfile import ZipFile

URL = (
    "https://github.com/Joebannerlord/openRAC-RC2-contextual-wrench-throw/releases/download/"
    "v0.1.1/rac2-contextual-wrench-0.1.1-windows-x86_64.zip"
)
SHA256 = "403aaa935c909d7d355aa88f7ce03274cdf58879ae623e3f62a8e35d6c234f27"
DLL = "rac2_contextual_wrench_011.dll"


def main():
    dest = Path(__file__).resolve().parents[1] / ".test-deps" / DLL
    with urllib.request.urlopen(URL, timeout=60) as response:
        archive = response.read()
    if hashlib.sha256(archive).hexdigest() != SHA256:
        raise SystemExit("Contextual Wrench and Fire archive did not match the pinned SHA-256")
    dest.parent.mkdir(exist_ok=True)
    with ZipFile(io.BytesIO(archive)) as package:
        dest.write_bytes(package.read(f"lib/windows-x86_64/{DLL}"))
    print(dest)


if __name__ == "__main__":
    main()

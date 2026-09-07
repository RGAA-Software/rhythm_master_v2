"""Prepare the pinned QR-reader candidate without modifying reference repositories."""

import hashlib
import json
import re
import shutil
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "third_party/sources/zxing-probe"
URL = "https://github.com/zxing-cpp/zxing-cpp.git"
REVISION = "d6068bcebeb8fd9f0d35a99b00d202be86a14dbe"


def main():
    if not SOURCE.exists():
        subprocess.run(["git", "clone", "--depth", "1", "--branch", "v2.3.0", "--single-branch", URL, str(SOURCE)], check=True)
    actual = subprocess.check_output(["git", "-C", str(SOURCE), "rev-parse", "HEAD"], text=True).strip()
    if actual != REVISION:
        raise SystemExit("QR-reader candidate revision differs from the reviewed pin")
    changed = subprocess.check_output(["git", "-C", str(SOURCE), "status", "--porcelain"], text=True)
    if changed:
        raise SystemExit("Candidate has local changes; review before recording provenance")
    names = subprocess.check_output(["git", "-C", str(SOURCE), "ls-files", "-z"]).decode().split("\0")
    files = {name: hashlib.sha256((SOURCE / name).read_bytes()).hexdigest()
             for name in names if name and (SOURCE / name).is_file()}
    record = {"name": "zxing-cpp QR reader candidate", "url": URL, "tag": "v2.3.0", "revision": REVISION,
              "license": "Apache-2.0 AND BSD-3-Clause (libzueci) AND retained Hoehrmann UTF-8 permission notice", "modifications": [],
              "target": "isolated qr_reader probe; not linked into Studio/Player", "files_sha256": files}
    (ROOT / "third_party/zxing_probe_source.json").write_text(json.dumps(record, indent=4) + "\n", encoding="utf-8")
    notices = ROOT / "third_party/notices/zxing"
    notices.mkdir(parents=True, exist_ok=True)
    shutil.copy2(SOURCE / "LICENSE", notices / "LICENSE")
    sections = []
    for name in names:
        if not name.startswith("core/src/") or not (SOURCE / name).is_file():
            continue
        text = (SOURCE / name).read_text(encoding="utf-8", errors="replace")
        leading = re.match(r"\s*(?:(?:/\*.*?\*/|//[^\n]*)(?:\s|\n)*)+", text, re.S)
        if leading and ("Copyright" in leading[0] or "SPDX-License" in leading[0]):
            sections.append(name + "\n" + leading[0].strip())
    embedded = (SOURCE / "core/src/libzueci/zueci.c").read_text()
    permission = next(block for block in re.findall(r"/\*.*?\*/", embedded, re.S)
                      if "Permission is hereby granted" in block)
    sections.append("core/src/libzueci/zueci.c embedded UTF-8 decoder\n" + permission)
    (notices / "FILE_NOTICES.txt").write_text("\n\n".join(sections) + "\n", encoding="utf-8")
    (notices / "PROVENANCE.txt").write_text(
        f"ZXing-C++ v2.3.0, {URL}, commit {REVISION}.\n"
        "Reader-only candidate; libzint writer backend is disabled.\n"
        "libzueci: https://sourceforge.net/projects/libzueci/, embedded version 1.0.1;\n"
        "BSD-3-Clause text expands its explicit SPDX identifier; copyright retained from source.\n"
        "Canonical terms: https://spdx.org/licenses/BSD-3-Clause.html\n"
        "Embedded UTF-8 decoder: https://bjoern.hoehrmann.de/utf-8/decoder/dfa/;\n"
        "the exact embedded permission/copyright notice is included in FILE_NOTICES.txt.\n",
        encoding="utf-8")
    print("Pinned QR reader candidate and source inventory recorded")


if __name__ == "__main__":
    main()

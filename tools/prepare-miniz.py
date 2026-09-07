"""Fetch the pinned ZIP candidate; preserve source identity and MIT notices."""

import hashlib
import json
from pathlib import Path
import shutil
from urllib.request import urlopen


def main():
    root = Path(__file__).resolve().parents[1]
    revision = "d10b03cc73475af673df40f06e5cefd1d5f940d9"
    destination = root / "third_party/sources/miniz"
    destination.mkdir(parents=True, exist_ok=True)
    names = ["LICENSE", "miniz.c", "miniz.h", "miniz_common.h", "miniz_tdef.c", "miniz_tdef.h",
             "miniz_tinfl.c", "miniz_tinfl.h", "miniz_zip.c", "miniz_zip.h"]
    record_path = root / "third_party/miniz_source.json"
    previous = json.loads(record_path.read_text(encoding="utf-8")) if record_path.exists() else {}
    hashes = {}
    for name in names:
        path = destination / name
        if path.exists() and previous.get("revision") == revision:
            digest = hashlib.sha256(path.read_bytes()).hexdigest()
            if digest != previous["files"].get(name):
                raise RuntimeError(f"Imported source changed: {path}")
        else:
            with urlopen(f"https://raw.githubusercontent.com/richgel999/miniz/{revision}/{name}", timeout=30) as response:
                data = response.read()
            path.write_bytes(data)
            digest = hashlib.sha256(data).hexdigest()
        hashes[name] = digest
    notices = root / "third_party/notices/miniz"
    notices.mkdir(parents=True, exist_ok=True)
    shutil.copy2(destination / "LICENSE", notices / "LICENSE")
    copyrights = sorted({line.strip().lstrip("* ") for name in names
                         for line in (destination / name).read_text(encoding="utf-8").splitlines()
                         if "Copyright " in line})
    (notices / "SOURCE_COPYRIGHTS.txt").write_text("\n".join(copyrights) + "\n", encoding="utf-8")
    record = {"source": "https://github.com/richgel999/miniz", "revision": revision, "version": "3.1.1",
              "license": "MIT", "modifications": "None; project-owned static CMake target and bounded in-memory adapter.",
              "target": "runtime_package", "status": previous.get("status", "Candidate pending Windows and Android package validation"),
              "files": hashes}
    record_path.write_text(json.dumps(record, indent=4) + "\n", encoding="utf-8")
    print(f"Prepared miniz 3.1.1 ({revision}), {len(hashes)} verified source files")


if __name__ == "__main__":
    main()

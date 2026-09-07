"""Relink the companion Android acceptance app with replaceable FFmpeg archives."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ndk", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parent
    host = "windows-x86_64" if os.name == "nt" else "linux-x86_64"
    toolchain = args.ndk.resolve() / "toolchains/llvm/prebuilt" / host
    compiler = toolchain / "bin" / ("clang++.exe" if os.name == "nt" else "clang++")
    arguments = ["--target=aarch64-none-linux-android26", "--sysroot=" + (toolchain / "sysroot").as_posix()]
    for value in json.loads((root / "link.json").read_text(encoding="utf-8")):
        arguments.append((root / value).as_posix() if value.startswith("inputs/") else value)
    arguments += ["-o", (root / "libmain.so").as_posix()]
    # LLVM response files, not shell interpolation; quotes/backslashes are escaped.
    response = root / "link.rsp"
    response.write_text("\n".join('"' + value.replace('\\', '\\\\').replace('"', '\\"') + '"'
                                   for value in arguments), encoding="utf-8")
    subprocess.run([str(compiler), "@" + str(response)], cwd=root, check=True)
    data = (root / "libmain.so").read_bytes()
    if data[:6] != b"\x7fELF\x02\x01" or b"Java_org_rhythmmaster_player_PlayerActivity_nativeMusic" not in data:
        raise ValueError("Relinked library lacks the required Android entry points")
    (root / "verification.json").write_text(json.dumps({"relinked_sha256": hashlib.sha256(data).hexdigest(),
                                                       "source": "only enclosed objects/libraries and supplied NDK",
                                                       "device_acceptance": "not established by relinking"}, indent=4) + "\n", encoding="utf-8")
    print("Android relink succeeded:", root / "libmain.so")


if __name__ == "__main__":
    main()

"""Inventory the resolved candidate's normal dependencies and retain their notices."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--platform", choices=("windows", "android"), default="windows")
    args = parser.parse_args()
    metadata = json.loads((ROOT / f"out/quic/quiche-metadata-{args.platform}.json").read_text(encoding="utf-8-sig"))
    packages = {value["id"]: value for value in metadata["packages"]}
    target = "x86_64-pc-windows-msvc" if args.platform == "windows" else "aarch64-linux-android"
    # Workspace-wide metadata unifies features of unrelated apps. The package-
    # selected tree reflects this actual ffi build and excludes host proc macros.
    tree = subprocess.check_output([
        "cargo", "tree", "--locked", "--manifest-path", str(ROOT / "third_party/sources/quiche-probe/Cargo.toml"),
        "-p", "quiche", "--features", "ffi", "--target", target, "--edges", "normal,no-proc-macro",
        "--prefix", "none", "--format", "{p}", "--color", "never"], text=True)
    identities = {tuple(match.groups()) for line in tree.splitlines()
                  if (match := re.match(r"^([\w-]+) v([^ ]+)", line))}
    selected = {key for key, value in packages.items() if (value["name"], value["version"]) in identities}
    if len(selected) != len(identities):
        raise SystemExit("Candidate metadata does not cover the resolved runtime tree")
    destination = ROOT / "out/quic/notices/quiche"
    records = []
    for key in sorted(selected):
        package = packages[key]
        source = Path(package["manifest_path"]).parent
        notices = [path for path in source.iterdir() if path.is_file()
                   and path.name.upper().startswith(("LICENSE", "COPYING", "NOTICE", "COPYRIGHT"))]
        if not notices and (source.parent / "COPYING").is_file():
            notices.append(source.parent / "COPYING")
        if package["name"] == "boring-sys":
            notices.append(source / "deps/boringssl/LICENSE")
        record = {name: package[name] for name in ("name", "version", "license", "repository", "source")}
        record["manifest_sha256"] = hashlib.sha256(Path(package["manifest_path"]).read_bytes()).hexdigest()
        record["notices"] = {}
        for index, notice in enumerate(notices):
            path = destination / f"{package['name']}-{package['version']}" / f"{index}-{notice.name}"
            path.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(notice, path)
            record["notices"][str(path.relative_to(destination))] = hashlib.sha256(path.read_bytes()).hexdigest()
        if not notices:
            raise SystemExit(f"Missing candidate notice: {package['name']}")
        records.append(record)
        print(package["name"], package["version"], package["license"])
    sysroot = Path(subprocess.check_output(["rustc", "--print", "sysroot"], text=True).strip())
    runtime_docs = sysroot / "share/doc/rust"
    # Current rustup packages provide HTML attribution and a licenses directory.
    # Retain the complete supplied library attribution, including LLVM exceptions.
    attribution = runtime_docs / "COPYRIGHT-library.html"
    if not attribution.is_file():
        raise SystemExit("Missing Rust standard-library attribution")
    runtime_destination = destination / "rust-runtime"
    runtime_destination.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(attribution, runtime_destination / attribution.name)
    shutil.copytree(runtime_docs / "licenses", runtime_destination / "licenses", dirs_exist_ok=True)
    record_path = ROOT / f"third_party/probe_locks/quiche-runtime-{args.platform}.json"
    record_path.parent.mkdir(parents=True, exist_ok=True)
    record_path.write_text(json.dumps({
        "status": "Candidate experiment only; no product adoption or outbound license selection",
        "platform": args.platform, "runtime_dependencies": records,
        "rustc": subprocess.check_output(["rustc", "--version"], text=True).strip(),
        "declared_licenses": sorted({record["license"] for record in records}),
        "build_dependencies": "Not included as runtime code; full Cargo.lock retained separately",
    }, indent=4) + "\n", encoding="utf-8")
    print(f"Recorded {len(records)} normal dependency notices for {args.platform}")


if __name__ == "__main__":
    main()

"""Record exact candidate sources and local patches, separately from adopted dependencies."""

import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def git(source, *args):
    return subprocess.check_output(["git", "-C", str(source), *args])


def inventory(source):
    return {name: hashlib.sha256((source / name).read_bytes()).hexdigest()
            for name in git(source, "ls-files", "-z").decode().split("\0")
            if name and (source / name).is_file()}


def main():
    candidates = {}
    for name, url, license_name in (
        ("msquic", "https://github.com/microsoft/msquic", "MIT"),
        ("quiche", "https://github.com/cloudflare/quiche", "BSD-2-Clause"),
    ):
        source = ROOT / "third_party/sources" / (name + "-probe")
        record = {"url": url, "revision": git(source, "rev-parse", "HEAD").decode().strip(),
                  "license": license_name, "files_sha256": inventory(source)}
        patch = git(source, "diff", "--no-ext-diff", "--binary")
        if patch:
            path = ROOT / "third_party/patches" / (name + "-probe.patch")
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(patch)
            record["patch"] = path.relative_to(ROOT).as_posix()
        if name == "msquic":
            record["submodules"] = {}
            for child, child_url, terms in (
                ("openssl", "https://github.com/openssl/openssl", "Apache-2.0"),
                ("xdp-for-windows", "https://github.com/microsoft/xdp-for-windows", "MIT"),
            ):
                sub = source / "submodules" / child
                record["submodules"][child] = {
                    "url": child_url, "revision": git(sub, "rev-parse", "HEAD").decode().strip(),
                    "license": terms, "files_sha256": inventory(sub),
                    "modified": bool(git(sub, "diff", "--no-ext-diff")),
                }
            record["modifications"] = (
                "Timestamp flag temporaries use uint64_t to avoid MSVC 19.51 SAL range warning; "
                "explicit uint32_t cast retained for existing trace format. "
                "CMake accepts measured localized MSVC include prefix and /FC diagnostics; "
                "Android API <28 runtime-only build excludes unused test certificate glob helper. "
                "Explicit shared OpenSSL Windows profile resolves dependencies from application directory and System32 (0xA00), "
                "retaining system-only upstream defaults for other profiles. "
                "No protocol changes.")
            record["tls_snapshot"] = "Upstream submodule VERSION.dat reports 3.5.8-dev; not a release TLS lock"
        else:
            lock = source / "Cargo.lock"
            if lock.is_file():
                record["cargo_lock_sha256"] = hashlib.sha256(lock.read_bytes()).hexdigest()
                destination = ROOT / "third_party/probe_locks/quiche-Cargo.lock"
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_bytes(lock.read_bytes())
            record["transitive_license_status"] = (
                "Resolved normal dependency notices recorded for Windows (20) and Android (17), "
                "including embedded BoringSSL notice; runtime ledgers in third_party/probe_locks")
        candidates[name] = record
    destination = ROOT / "third_party/quic_probe_sources.json"
    tls_source = ROOT / "third_party/sources/openssl-quic-3.5.8"
    released_tls = {
        "url": "https://github.com/openssl/openssl", "version": "3.5.8", "license": "Apache-2.0",
        "revision": git(tls_source, "rev-parse", "HEAD").decode().strip(),
        "files_sha256": inventory(tls_source), "modified": bool(git(tls_source, "diff", "--no-ext-diff")),
        "usage": "Isolated MsQuic and identity external static/shared TLS candidates, separate from upstream dev snapshot",
    }
    destination.write_text(json.dumps({
        "status": "Isolated build/functional experiments only; no dependency or TLS provider adopted",
        "artifacts": "out/quic and out/security only; no Studio/Player linkage",
        "project_license": "pending; original third-party terms retained",
        "candidates": candidates,
        "released_tls_candidate": released_tls,
    }, indent=2) + "\n", encoding="utf-8")
    print(f"Recorded candidate source inventories and patch: {destination}")


if __name__ == "__main__":
    main()

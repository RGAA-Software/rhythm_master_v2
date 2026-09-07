"""Run isolated identity/QUIC contracts; native adb execution does not install an APK."""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
PROBES = ("identity_contract_tests", "certificate_policy_tests", "vault_contract_tests", "identity_quic_tests", "room_authority_tests", "invitation_link_tests", "admission_codec_tests", "admission_gate_tests", "transport_contract_tests", "session_envelope_tests")


def run(command, log, **options):
    result = subprocess.run(command, capture_output=True, text=True, timeout=60, **options)
    log.write_text(result.stdout + result.stderr, encoding="utf-8")
    if result.returncode:
        raise RuntimeError(f"Probe failed: {log}")
    print(result.stdout.strip(), flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--platform", choices=("windows", "android", "both"), default="both")
    parser.add_argument("--serial")
    parser.add_argument("--adb", default="D:/android/sdk/platform-tools/adb.exe")
    parser.add_argument("--shared-tls", action="store_true")
    args = parser.parse_args()
    if args.platform != "windows" and not args.serial:
        parser.error("--serial is required for native device tests")
    suffix = "-shared" if args.shared_tls else ""
    result = {"time_utc": datetime.now(timezone.utc).isoformat(), "tls_linkage": "shared" if args.shared_tls else "static", "platforms": {}}
    if args.platform != "android":
        binaries = ROOT / ("out/security/windows" + suffix + "/deploy")
        hashes = {}
        environment = os.environ.copy()
        system_root = Path(environment.get("SystemRoot", "C:/Windows"))
        environment["PATH"] = os.pathsep.join([str(system_root / "System32"), str(system_root)])
        for probe in PROBES:
            executable = binaries / (probe + ".exe")
            run([str(executable)], ROOT / f"out/security-windows{suffix}-{probe}.log",
                env=environment, cwd=ROOT / "out/security")
            hashes[probe] = hashlib.sha256(executable.read_bytes()).hexdigest()
        result["platforms"]["windows"] = hashes
    if args.platform != "windows":
        adb = [args.adb, "-s", args.serial]
        remote = "/data/local/tmp/rhythm-identity-" + datetime.now(timezone.utc).strftime("%Y%m%d%H%M%S")
        binaries = [ROOT / ("out/security/android" + suffix) / probe for probe in PROBES]
        binaries.append(ROOT / ("out/quic/msquic-android" + suffix + "/bin/Release/libmsquic.so"))
        if args.shared_tls:
            tls_root = ROOT / "out/quic/openssl-3.5.8-android-shared-install/lib"
            binaries.extend([tls_root / "libcrypto.so", tls_root / "libssl.so"])
        subprocess.run([*adb, "shell", "mkdir", "-p", remote], check=True)
        subprocess.run([*adb, "push", *map(str, binaries), remote + "/"], check=True, stdout=subprocess.DEVNULL)
        notices = ROOT / "out/security/notices"
        for source, destination in (("third_party/sources/openssl-quic-3.5.8/LICENSE.txt", "openssl/LICENSE.txt"),
                                    ("third_party/sources/msquic-probe/LICENSE", "msquic/LICENSE")):
            target = notices / destination
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / source, target)
        subprocess.run([*adb, "push", str(notices), remote + "/"],
                       check=True, stdout=subprocess.DEVNULL)
        for probe in PROBES:
            executable = remote + "/" + probe
            command = "chmod 700 " + shlex.quote(executable) + " && "
            command += "LD_LIBRARY_PATH=" + shlex.quote(remote) + " TMPDIR=" + shlex.quote(remote) + " "
            run([*adb, "shell", command + shlex.quote(executable)], ROOT / f"out/security-android{suffix}-{probe}.log")
        result["platforms"]["android"] = {
            "device_directory": remote,
            "sha256": {path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in binaries}}
    platform_suffix = "" if args.platform == "both" else "-" + args.platform
    output = ROOT / ("out/security/validation" + suffix + platform_suffix + ".json")
    output.write_text(json.dumps(result, indent=4) + "\n", encoding="utf-8")
    print("Recorded", output)


if __name__ == "__main__":
    main()

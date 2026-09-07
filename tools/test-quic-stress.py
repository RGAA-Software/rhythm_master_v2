"""Measure isolated real QUIC loopback connections, not room or venue capacity."""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def run_case(command, platform_name, count):
    result = subprocess.run(command, capture_output=True, text=True, timeout=120)
    log = ROOT / f"out/quic-stress-{platform_name}-{count}.log"
    log.write_text(result.stdout + result.stderr, encoding="utf-8")
    if result.returncode:
        raise RuntimeError(f"Stress probe failed; see {log}")
    match = re.search(r"^QUIC_STRESS (.+)$", result.stdout, re.MULTILINE)
    if not match:
        raise RuntimeError("Missing native stress metrics")
    values = dict(field.split("=", 1) for field in match[1].split())
    for key in ("peers", "peak_resident_bytes", "limit_rejected", "in_flight_after_close"):
        values[key] = int(values[key])
    for key in ("handshake_ms", "total_ms", "cpu_ms"):
        values[key] = float(values[key])
    if values["peers"] != count or values["in_flight_after_close"] or values["reliable_and_datagram"] != "passed":
        raise RuntimeError("Incomplete stress result")
    print(platform_name, values, flush=True)
    return values


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--platform", choices=("windows", "android", "both"), default="both")
    parser.add_argument("--serial")
    parser.add_argument("--adb", default="D:/android/sdk/platform-tools/adb.exe")
    args = parser.parse_args()
    if args.platform != "windows" and not args.serial:
        parser.error("--serial is required for native Android measurements")
    results = {"time_utc": datetime.now(timezone.utc).isoformat(),
               "scope": "One burst, client+server in one process, no GPU/audio/room workload; not venue capacity",
               "msquic": "v2.6.1", "openssl": "3.5.8", "platforms": {}}
    if args.platform != "android":
        import winreg
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r"HARDWARE\DESCRIPTION\System\CentralProcessor\0") as key:
            cpu = winreg.QueryValueEx(key, "ProcessorNameString")[0].strip()
        executable = ROOT / "out/quic/probe-windows-openssl/deploy/msquic_probe.exe"
        results["platforms"]["windows"] = {
            "os": platform.platform(), "cpu": cpu, "logical_cpus": os.cpu_count(),
            "executable_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
            "cases": [run_case([str(executable), str(executable.parent / "fixtures"), "stress", str(count)],
                               "windows", count) for count in (10, 100, 1000)]}
    if args.platform != "windows":
        adb = [args.adb, "-s", args.serial]
        remote = "/data/local/tmp/rhythm-quic-stress-" + datetime.now(timezone.utc).strftime("%Y%m%d%H%M%S")
        executable = ROOT / "out/quic/probe-android/msquic_probe"
        runtime = ROOT / "out/quic/msquic-android/bin/Release/libmsquic.so"
        subprocess.run([*adb, "shell", "mkdir", "-p", remote], check=True)
        subprocess.run([*adb, "push", str(executable), str(runtime),
                        str(ROOT / "out/quic/content/fixtures"),
                        str(ROOT / "out/quic/probe-windows-openssl/deploy/notices"), remote + "/"],
                       check=True, stdout=subprocess.DEVNULL)
        subprocess.run([*adb, "shell", "chmod", "700", remote + "/msquic_probe"], check=True)
        metadata = {name: subprocess.check_output([*adb, "shell", "getprop", prop], text=True).strip()
                    for name, prop in (("model", "ro.product.model"), ("api", "ro.build.version.sdk"),
                                       ("soc", "ro.soc.model"))}
        metadata.update({"device_directory": remote,
                         "executable_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
                         "library_sha256": hashlib.sha256(runtime.read_bytes()).hexdigest(),
                         "cases": []})
        for count in (10, 100):
            command = "cd " + shlex.quote(remote) + " && LD_LIBRARY_PATH=. ./msquic_probe fixtures stress " + str(count)
            metadata["cases"].append(run_case([*adb, "shell", command], "android", count))
        results["platforms"]["android"] = metadata
    output = ROOT / f"out/quic/stress-{args.platform}.json"
    output.write_text(json.dumps(results, ensure_ascii=False, indent=4) + "\n", encoding="utf-8")
    print("Recorded", output)


if __name__ == "__main__":
    main()

"""Validate the candidate public transport API in both actual PC/Android LAN directions."""

import argparse
from datetime import datetime, timezone
import hashlib
import ipaddress
import json
import os
from pathlib import Path
import queue
import re
import shlex
import subprocess
import threading

ROOT = Path(__file__).resolve().parents[1]


def pair(server_command, client_factory):
    flags = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0
    with subprocess.Popen(server_command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          text=True, creationflags=flags) as server:
        lines = queue.Queue(maxsize=1)
        reader = threading.Thread(target=lambda: lines.put(server.stdout.readline()))
        reader.start()
        try:
            ready = lines.get(timeout=15)
            match = re.fullmatch(r"READY (\d{1,5}) ([0-9a-f]{64})\s*", ready)
            if not match or not 1 <= int(match[1]) <= 65535:
                raise RuntimeError("Transport server startup failed: " + ready)
            client = subprocess.run(client_factory(match[1], match[2]), capture_output=True,
                                    text=True, timeout=25, creationflags=flags)
            output, _ = server.communicate(timeout=25)
            if client.returncode or server.returncode:
                raise RuntimeError("Transport LAN failed: " + client.stdout + client.stderr + output)
            print(client.stdout.strip(), flush=True)
            print(output.strip(), flush=True)
            return {"client": client.stdout.strip(), "server": output.strip()}
        finally:
            if server.poll() is None:
                server.terminate()  # Only this tool's own child process.
                server.wait(timeout=5)
            reader.join(timeout=5)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial", required=True)
    parser.add_argument("--pc-address", required=True, type=ipaddress.IPv4Address)
    parser.add_argument("--adb", default="D:/android/sdk/platform-tools/adb.exe")
    args = parser.parse_args()
    adb = [args.adb, "-s", args.serial]
    addresses = subprocess.check_output([*adb, "shell", "ip", "-4", "addr", "show", "wlan0"], text=True)
    matches = re.findall(r"\binet (\d+\.\d+\.\d+\.\d+)/", addresses)
    if len(matches) != 1:
        raise SystemExit("Expected one current Android Wi-Fi IPv4 address")
    phone = str(ipaddress.IPv4Address(matches[0]))
    remote = "/data/local/tmp/rhythm-transport-lan-" + datetime.now(timezone.utc).strftime("%Y%m%d%H%M%S")
    executable = ROOT / "out/security/windows-shared/deploy/transport_contract_tests.exe"
    files = [ROOT / "out/security/android-shared/transport_contract_tests",
             ROOT / "out/quic/msquic-android-shared/bin/Release/libmsquic.so",
             ROOT / "out/quic/openssl-3.5.8-android-shared-install/lib/libcrypto.so",
             ROOT / "out/quic/openssl-3.5.8-android-shared-install/lib/libssl.so"]
    subprocess.run([*adb, "shell", "mkdir", "-p", remote], check=True)
    subprocess.run([*adb, "push", *map(str, files), str(executable.parent / "notices"), remote + "/"],
                   check=True, stdout=subprocess.DEVNULL)
    subprocess.run([*adb, "shell", "chmod", "700", remote + "/transport_contract_tests"], check=True)

    def android(*arguments):
        command = "cd " + shlex.quote(remote) + " && LD_LIBRARY_PATH=. ./transport_contract_tests "
        return [*adb, "shell", command + shlex.join(arguments)]

    print(f"Public transport LAN: Windows {args.pc_address}, Android {phone}", flush=True)
    forward = pair([str(executable), "server", str(args.pc_address)],
                   lambda port, pin: android("client", str(args.pc_address), port, pin))
    reverse = pair(android("server", phone),
                   lambda port, pin: [str(executable), "client", phone, port, pin])
    result = {"time_utc": datetime.now(timezone.utc).isoformat(), "device_directory": remote,
              "windows_host": forward, "android_host_probe_only": reverse,
              "sha256": {path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in [executable, *files]}}
    output = ROOT / "out/security/transport-lan.json"
    output.write_text(json.dumps(result, indent=4) + "\n", encoding="utf-8")
    print("Recorded", output, flush=True)


if __name__ == "__main__":
    main()

"""Run the isolated pinned-TLS/stream/DATAGRAM probe across the selected PC and phone LAN."""

import argparse
from datetime import datetime, timezone
import ipaddress
import os
from pathlib import Path
import queue
import re
import shlex
import subprocess
import threading

ROOT = Path(__file__).resolve().parents[1]


def pair(server_command, client_command):
    flags = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0
    with subprocess.Popen(server_command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          text=True, creationflags=flags) as server:
        lines = queue.Queue(maxsize=1)
        reader = threading.Thread(target=lambda: lines.put(server.stdout.readline()))
        reader.start()
        try:
            ready = lines.get(timeout=12)
            if "QUIC server ready" not in ready:
                raise RuntimeError("Server did not start: " + ready)
            client = subprocess.run(client_command, capture_output=True, text=True, timeout=30,
                                    creationflags=flags)
            if client.returncode:
                output, _ = server.communicate(timeout=20)
                raise RuntimeError("Client failed: " + client.stdout + client.stderr + "\nServer: " + output)
            output, _ = server.communicate(timeout=20)
            if server.returncode:
                raise RuntimeError("Server failed: " + output)
            print(client.stdout.strip())
            print(output.strip())
        finally:
            if server.poll() is None:
                server.terminate()  # Only this tool's own child process.
                server.wait(timeout=5)
            reader.join(timeout=5)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial", required=True)
    parser.add_argument("--pc-address", required=True, type=ipaddress.IPv4Address)
    parser.add_argument("--phone-address", type=ipaddress.IPv4Address)
    parser.add_argument("--adb", default="D:/android/sdk/platform-tools/adb.exe")
    parser.add_argument("--shared-tls", action="store_true")
    args = parser.parse_args()
    adb = [args.adb, "-s", args.serial]
    addresses = subprocess.check_output([*adb, "shell", "ip", "-4", "addr", "show", "wlan0"], text=True)
    matches = re.findall(r"\binet (\d+\.\d+\.\d+\.\d+)/", addresses)
    if len(matches) != 1:
        raise SystemExit("Expected one current Android Wi-Fi IPv4 address")
    current_phone = ipaddress.IPv4Address(matches[0])
    if args.phone_address and args.phone_address != current_phone:
        raise SystemExit(f"Phone address changed: supplied {args.phone_address}, current {current_phone}")
    args.phone_address = current_phone
    print(f"Selected LAN endpoints: PC {args.pc_address}, Android {current_phone}", flush=True)
    remote = "/data/local/tmp/rhythm-quic-lan-" + datetime.now(timezone.utc).strftime("%Y%m%d%H%M%S")
    suffix = "-shared" if args.shared_tls else ""
    files = [ROOT / ("out/quic/probe-android" + suffix + "/msquic_probe"),
             ROOT / ("out/quic/msquic-android" + suffix + "/bin/Release/libmsquic.so"),
             ROOT / "out/quic/content/fixtures",
             ROOT / ("out/quic/probe-windows-openssl" + suffix + "/deploy/notices")]
    if args.shared_tls:
        files += [ROOT / "out/quic/openssl-3.5.8-android-shared-install/lib/libcrypto.so",
                  ROOT / "out/quic/openssl-3.5.8-android-shared-install/lib/libssl.so"]
    subprocess.run([*adb, "shell", "mkdir", "-p", remote], check=True)
    subprocess.run([*adb, "push", *map(str, files), remote + "/"], check=True,
                   stdout=subprocess.DEVNULL)
    subprocess.run([*adb, "shell", "chmod", "700", remote + "/msquic_probe"], check=True)
    executable = ROOT / ("out/quic/probe-windows-openssl" + suffix + "/deploy/msquic_probe.exe")
    fixtures = executable.parent / "fixtures"
    print("Windows loopback: certificate rejection and cancellation races", flush=True)
    subprocess.run([str(executable), str(fixtures)], check=True, timeout=90)
    print("Android loopback: certificate rejection and cancellation races", flush=True)
    subprocess.run([*adb, "shell", "cd " + shlex.quote(remote) +
                    " && LD_LIBRARY_PATH=. ./msquic_probe fixtures"], check=True, timeout=90)
    print("Both loopbacks: bounded receive pause/resume and cancellation", flush=True)
    subprocess.run([str(executable), str(fixtures), "flow"], check=True, timeout=60)
    subprocess.run([*adb, "shell", "cd " + shlex.quote(remote) +
                    " && LD_LIBRARY_PATH=. ./msquic_probe fixtures flow"], check=True, timeout=60)

    def windows(role, address):
        return [str(executable), str(fixtures), role, str(address), "30451"]

    def android(role, address):
        command = "cd " + shlex.quote(remote) + " && LD_LIBRARY_PATH=. ./msquic_probe fixtures "
        command += shlex.join([role, str(address), "30451"])
        return [*adb, "shell", command]

    print("Windows Host -> Android Player", flush=True)
    pair(windows("server", args.pc_address), android("client", args.pc_address))
    print("Android server -> Windows client (reverse-path probe)", flush=True)
    pair(android("server", args.phone_address), windows("client", args.phone_address))
    print(f"Both LAN directions passed; device evidence retained at {remote}")


if __name__ == "__main__":
    main()

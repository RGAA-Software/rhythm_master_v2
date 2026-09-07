"""Generate disposable loopback TLS credentials for the isolated QUIC experiment."""

import argparse
from pathlib import Path
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--openssl", default="C:/Program Files/Git/mingw64/bin/openssl.exe")
    args = parser.parse_args()
    fixtures = ROOT / "out/quic/content/fixtures"
    fixtures.mkdir(parents=True, exist_ok=True)
    version = subprocess.check_output([args.openssl, "version"], text=True)
    if not version.startswith("OpenSSL 3."):
        raise SystemExit("Use OpenSSL 3.x for the TLS candidate fixtures")
    if all((fixtures / name).is_file() and time.time() - (fixtures / name).stat().st_mtime < 86400
           for name in ("server.pfx", "server.der", "server.pem", "server.key", "other.pem", "other.key")):
        print("Reusing current disposable QUIC test credentials")
        return
    for command in (
        ["req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "2", "-subj", "/CN=localhost",
         "-addext", "subjectAltName=IP:127.0.0.1,DNS:localhost", "-keyout", "server.key", "-out", "server.pem"],
        ["x509", "-in", "server.pem", "-outform", "DER", "-out", "server.der"],
        ["pkcs12", "-export", "-inkey", "server.key", "-in", "server.pem", "-out", "server.pfx",
         "-passout", "pass:probe-only"],
        ["req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "2", "-subj", "/CN=untrusted-test-ca",
         "-keyout", "other.key", "-out", "other.pem"],
    ):
        result = subprocess.run([args.openssl, *command], cwd=fixtures, stdout=subprocess.DEVNULL,
                                stderr=subprocess.PIPE)
        if result.returncode:
            raise SystemExit(result.stderr.decode(errors="replace"))
    print("Disposable loopback certificate/PFX prepared; no machine trust-store changes")


if __name__ == "__main__":
    main()

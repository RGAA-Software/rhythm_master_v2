"""Pin a separate released TLS source for QUIC experiments; retain upstream snapshot."""

from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
REVISION = "f4dc4d58b48d346a8270183f89acf826d459b0ca"
source = ROOT / "third_party/sources/openssl-quic-3.5.8"
upstream = ROOT / "third_party/sources/msquic-probe/submodules/openssl"
if not source.exists():
    subprocess.run(["git", "-C", str(upstream), "fetch", "--depth=1", "origin", "tag", "openssl-3.5.8"], check=True)
    actual = subprocess.check_output(["git", "-C", str(upstream), "rev-parse", "openssl-3.5.8^{commit}"], text=True).strip()
    if actual != REVISION:
        raise SystemExit("Unexpected OpenSSL release tag revision")
    subprocess.run(["git", "-C", str(upstream), "worktree", "add", "--detach", str(source), REVISION], check=True)
actual = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
if actual != REVISION or subprocess.check_output(["git", "-C", str(source), "status", "--porcelain"]):
    raise SystemExit("TLS candidate source differs from pinned pristine release")
print("Pinned OpenSSL 3.5.8 release:", REVISION)

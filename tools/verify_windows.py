"""Serialize Windows delivery checks, including independently launched commands."""

import argparse
from contextlib import contextmanager
from datetime import datetime, timezone
import errno
import json
import os
from pathlib import Path
import shutil
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
LOCK = ROOT / "out" / ".windows-verification.lock"


@contextmanager
def verification_lease(path=LOCK, timeout=300):
    """OS-owned lock: a killed checker cannot leave a stale ownership flag."""
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("a+b") as lease:
        if lease.seek(0, os.SEEK_END) == 0:
            lease.write(b"0")
            lease.flush()
        if os.name == "nt":
            import msvcrt

            def acquire():
                lease.seek(0)
                msvcrt.locking(lease.fileno(), msvcrt.LK_NBLCK, 1)

            def release():
                lease.seek(0)
                msvcrt.locking(lease.fileno(), msvcrt.LK_UNLCK, 1)
        else:
            import fcntl

            def acquire():
                fcntl.flock(lease, fcntl.LOCK_EX | fcntl.LOCK_NB)

            def release():
                fcntl.flock(lease, fcntl.LOCK_UN)
        started = time.monotonic()
        announced = False
        while True:
            try:
                acquire()
                break
            except OSError as error:
                if error.errno not in (errno.EACCES, errno.EAGAIN):
                    raise
                if not announced:
                    print("Waiting for the other Windows build/check to release its lease.", flush=True)
                    announced = True
                if time.monotonic() - started >= timeout:
                    raise TimeoutError("Windows verification lease timed out") from error
                time.sleep(0.1)
        try:
            yield
        finally:
            release()


def run_logged(command, log, environment=None):
    """Keep the first failure and its output; never retry or reinterpret it."""
    log = Path(log)
    log.parent.mkdir(parents=True, exist_ok=True)
    archive_directory = log.parent / (log.name + ".runs")
    archive_directory.mkdir(parents=True, exist_ok=True)
    archive = archive_directory / (str(time.time_ns()) + ".log")
    started = time.monotonic()
    evidence = {"command": [str(value) for value in command], "cwd": str(Path.cwd()),
                "started_utc": datetime.now(timezone.utc).isoformat(), "returncode": None}
    evidence["archive"] = str(archive.resolve())
    metadata = archive.with_suffix(".json")
    metadata.write_text(json.dumps(evidence, indent=4) + "\n", encoding="utf-8")
    try:
        print("Verification log: " + str(archive), flush=True)
        with archive.open("xb") as output:
            result = subprocess.run(command, env=environment, stdout=output, stderr=subprocess.STDOUT)
        evidence["returncode"] = result.returncode
        print(archive.read_text(encoding="utf-8", errors="replace"), end="")
        result.check_returncode()
    finally:
        evidence["elapsed_seconds"] = time.monotonic() - started
        metadata.write_text(json.dumps(evidence, indent=4) + "\n", encoding="utf-8")
        if archive.is_file():
            shutil.copyfile(archive, log)
        shutil.copyfile(metadata, log.with_suffix(log.suffix + ".json"))


def run_ctest(build, pattern, required, log, environment=None):
    listing = subprocess.run(["ctest", "--test-dir", str(build), "--show-only=json-v1",
                              "-R", pattern], env=environment, capture_output=True, check=True)
    available = {test["name"] for test in json.loads(listing.stdout)["tests"]}
    if not available or set(required) - available:
        raise RuntimeError("Required checks are missing: " + ", ".join(sorted(set(required) - available)))
    run_logged(["ctest", "--test-dir", str(build), "--output-on-failure", "--no-tests=error",
                "--parallel", "1", "-R", pattern], log, environment)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, default=ROOT / "out/windows-release")
    parser.add_argument("--match", help="CTest regular expression; command mode otherwise")
    parser.add_argument("--require", action="append", default=[])
    parser.add_argument("--log", type=Path, required=True)
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command[1:] if args.command[:1] == ["--"] else args.command
    if bool(args.match) == bool(command):
        parser.error("Choose either --match or -- followed by an executable and its arguments")
    environment = os.environ.copy()
    runtime = args.build.resolve() / "src/windows_spike/deploy"
    environment["PATH"] = str(runtime) + os.pathsep + environment.get("PATH", "")
    with verification_lease():
        if args.match:
            run_ctest(args.build, args.match, args.require, args.log, environment)
        else:
            run_logged(command, args.log, environment)


if __name__ == "__main__":
    main()

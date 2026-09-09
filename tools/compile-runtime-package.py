"""Invoke the native package compiler with the selected build-tool DLL paths."""

import argparse
import os
from pathlib import Path
import subprocess
import content_identity


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--publisher", type=Path, required=True)
    parser.add_argument("--sdk", type=Path, required=True)
    parser.add_argument("--media-sdk", type=Path)
    parser.add_argument("--template", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    environment = dict(os.environ)
    environment["PATH"] = os.pathsep.join([str(args.sdk / "debug/bin"), str(args.sdk / "bin"), os.environ["PATH"]])
    if args.media_sdk and args.media_sdk.is_dir():
        environment["PATH"] = os.pathsep.join(
            [str(args.media_sdk / "bin"), environment["PATH"]])
    result = subprocess.run(
        [str(args.publisher.resolve()), str(args.template.resolve()), str(args.output.resolve())],
        env=environment, capture_output=True, timeout=60,
        creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
    # Explicit pipes preserve native diagnostics when Windows has no console.
    print(result.stdout.decode('utf-8', errors='replace'), end='')
    print(result.stderr.decode('utf-8', errors='replace'), end='')
    result.check_returncode()
    content_identity.write_package_identity(args.template, args.output)


if __name__ == "__main__":
    main()

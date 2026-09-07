"""Deploy a dependency probe, its recursive DLL closure and vcpkg notices."""

import argparse
import importlib.util
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path, required=True)
    args = parser.parse_args()
    config = dict(line.split("=", 1) for line in args.config.read_text(encoding="utf-8").splitlines() if line)
    spec = importlib.util.spec_from_file_location("deployment", ROOT / "tools/deploy-windows.py")
    deployment = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(deployment)
    executable = Path(config["executable"])
    destination = executable.parent / "deploy"
    dependencies = deployment.resolve_dependencies(config)
    for source in dependencies.values():
        deployment.copy_file(source, executable.parent / source.name)
        deployment.copy_file(source, destination / source.name)
    deployment.copy_file(executable, destination / executable.name)
    for package in config.get("packages", "").split(";"):
        if not package:
            continue
        if not package.replace("-", "").isalnum():
            raise ValueError("invalid vcpkg package name")
        for name in ("copyright", "vcpkg.spdx.json"):
            source = Path(config["sdk"]) / "share" / package / name
            if source.is_file():
                deployment.copy_file(source, destination / "notices" / package / name)
    (destination / "dependencies.json").write_text(
        json.dumps({name: str(path) for name, path in dependencies.items()}, indent=4) + "\n", encoding="utf-8")
    print(f"Deployed probe: {len(dependencies)} DLLs -> {destination}")


if __name__ == "__main__":
    main()

"""Exercise every retained Advanced template from a fresh Studio process."""

import argparse
import json
from pathlib import Path
import subprocess


def advanced_templates(resources: Path) -> list[str]:
    templates = []
    for manifest_path in sorted((resources / "content" / "templates").glob("*/manifest.json")):
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        if manifest.get("tier") == "advanced":
            templates.append(manifest_path.parent.name)
    if not templates:
        raise RuntimeError("No Advanced template is available for Studio switching")
    return templates


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--resources", type=Path, required=True)
    parser.add_argument("--locale", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    for name in advanced_templates(args.resources):
        subprocess.run([str(args.executable), str(args.resources), args.locale,
                        str(args.output / name), "--template", name], check=True)


if __name__ == "__main__":
    main()

"""Inventory content and bind independent quality reviews to exact source bytes.

Manifest maturity, template tier and successful loading are declarations, never
visual acceptance. Default presets do not count toward the visual preset target.
"""

import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path

from content_identity import authoring_digest

ROOT = Path(__file__).resolve().parents[1]
STAGES = ("functional", "music", "visual", "device")


def read(path):
    return json.loads(path.read_text(encoding="utf-8"))


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, ensure_ascii=False,
                                    separators=(",", ":")).encode("utf-8")).hexdigest()


def inventory(root):
    entries = []
    preset_files = [root / "content/presets/catalog.json"]
    for kind, folder in (("template", "templates"), ("component", "semantic")):
        for manifest_path in sorted((root / "content" / folder).glob("*/manifest.json")):
            source = manifest_path.parent
            manifest = read(manifest_path)
            entries.append({"id": manifest["content_id"], "kind": kind,
                            "source": source.relative_to(root).as_posix(),
                            "source_sha256": authoring_digest(source),
                            "version": manifest["content_version"],
                            "titles": manifest["titles"], "tier": manifest.get("tier", "component"),
                            "declared_maturity": manifest.get("maturity", "unspecified"),
                            "declared_players": manifest.get("compatible_players", []),
                            "source_license_note": manifest.get("license_status", "unspecified"),
                            "soundtrack": manifest.get("soundtrack"),
                            "is_default": False})
            if (source / "presets.json").is_file():
                preset_files.append(source / "presets.json")
    for path in preset_files:
        parent_digest = authoring_digest(path.parent) if path.name == "presets.json" else None
        for preset in read(path)["presets"]:
            entries.append({"id": preset["id"], "kind": "preset",
                            "source": path.relative_to(root).as_posix(),
                            "source_sha256": digest({"preset": preset, "parent": parent_digest}),
                            "version": preset["version"], "titles": preset["titles"],
                            "operator": preset["operator"], "tier": "preset",
                            "is_default": preset["id"].endswith(".default"),
                            "exact_setting_group": digest({"operator": preset["operator"],
                                                            "properties": preset.get("properties", {}),
                                                            "reset": preset.get("reset", False)})})
    ids = [entry["id"] for entry in entries]
    if len(ids) != len(set(ids)):
        raise ValueError("Content IDs are ambiguous across the quality inventory")
    return sorted(entries, key=lambda entry: entry["id"])


def report(root, ledger):
    entries = inventory(root)
    by_id = {entry["id"]: entry for entry in entries}
    reviews = ledger.get("reviews", [])
    if ledger.get("schema_version") != 1:
        raise ValueError("Unknown content review schema")
    unknown = sorted({review["id"] for review in reviews} - by_id.keys())
    if unknown:
        raise ValueError("Reviews reference unknown content IDs: " + repr(unknown))
    accepted_groups = set()
    accepted_settings = set()
    counts = Counter({"template:basic": 0, "template:advanced": 0,
                      "component:component": 0, "preset:preset": 0})
    for entry in entries:
        history = [review for review in reviews if review["id"] == entry["id"]]
        current = [review for review in history if review["source_sha256"] == entry["source_sha256"]]
        entry["review_history_count"] = len(history)
        entry["stale_review_count"] = len(history) - len(current)
        entry["quality_accepted"] = False
        entry["user_acceptance"] = "not_recorded"
        entry["review"] = {stage: "pending" for stage in STAGES}
        if current:
            latest = current[-1]
            if not latest.get("reviewer") or not latest.get("date"):
                raise ValueError("Review identity and date required: " + entry["id"])
            for stage in STAGES:
                decision = latest.get(stage, {"status": "pending"})
                if decision["status"] not in ("pending", "passed", "failed", "not_applicable"):
                    raise ValueError("Invalid review status: " + entry["id"])
                if decision["status"] != "pending":
                    evidence = decision.get("evidence", [])
                    if not evidence or not decision.get("notes"):
                        raise ValueError("Decision lacks concrete evidence and notes: " + entry["id"])
                    for reference in evidence:
                        path = (root / reference).resolve()
                        if not path.is_relative_to(root.resolve()) or not path.is_file():
                            raise ValueError("Review evidence missing or outside project: " + reference)
                entry["review"][stage] = decision["status"]
            # No stage can silently bypass the content quality gate.
            accepted = all(entry["review"][stage] == "passed" for stage in STAGES)
            if accepted:
                group = latest.get("independence_group")
                if not group or not latest.get("independence_notes"):
                    raise ValueError("Accepted content lacks independence review: " + entry["id"])
                if entry["is_default"]:
                    raise ValueError("Default preset cannot count as a visual work: " + entry["id"])
                key = (entry["kind"], group)
                if key in accepted_groups:
                    raise ValueError("Same independent work counted twice: " + group)
                accepted_groups.add(key)
                if entry["kind"] == "preset":
                    settings = entry["exact_setting_group"]
                    if settings in accepted_settings:
                        raise ValueError("Identical preset settings counted twice: " + entry["id"])
                    accepted_settings.add(settings)
                entry["quality_accepted"] = True
                counts[entry["kind"] + ":" + entry["tier"]] += 1
        counts["authored:" + entry["kind"]] += 1
    duplicates = defaultdict(list)
    for entry in entries:
        if entry["kind"] == "preset":
            duplicates[entry["exact_setting_group"]].append(entry["id"])
    return {"schema_version": 1,
            "scope": "Source inventory and hash-bound review bookkeeping; no automatic visual or user acceptance",
            "targets": {"template:basic": 50, "template:advanced": 50,
                        "component:component": 40, "preset:preset": 120},
            "counts": dict(sorted(counts.items())),
            "default_presets_excluded": sum(entry["is_default"] for entry in entries),
            "identical_preset_settings": [ids for ids in duplicates.values() if len(ids) > 1],
            "entries": entries}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reviews", type=Path, default=ROOT / "docs/content_reviews.json")
    parser.add_argument("--output", type=Path, default=ROOT / "docs/content_quality_index.json")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    result = report(ROOT, read(args.reviews))
    rendered = json.dumps(result, ensure_ascii=False, indent=4) + "\n"
    if args.check:
        if not args.output.is_file() or args.output.read_text(encoding="utf-8") != rendered:
            raise ValueError("Content quality inventory is stale; regenerate after reviewing source changes")
    else:
        args.output.write_text(rendered, encoding="utf-8")
    print(json.dumps({"counts": result["counts"], "targets": result["targets"],
                      "default_presets_excluded": result["default_presets_excluded"]}, indent=4))


if __name__ == "__main__":
    main()

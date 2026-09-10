"""Check acceptance bookkeeping against stale evidence and duplicate inflation."""
import copy
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("quality", Path(__file__).with_name("audit-content-quality.py"))
quality = importlib.util.module_from_spec(spec)
spec.loader.exec_module(quality)

ENTRY = {"id": "test.one", "kind": "preset", "tier": "preset", "source_sha256": "a" * 64,
         "is_default": False, "exact_setting_group": "settings"}


class QualityTests(unittest.TestCase):
    def evaluate(self, entries, reviews):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "evidence.md").write_text("synthetic test fixture", encoding="utf-8")
            with patch.object(quality, "inventory", return_value=copy.deepcopy(entries)):
                return quality.report(root, {"schema_version": 1, "reviews": reviews})

    def review(self, entry=ENTRY):
        result = {"id": entry["id"], "source_sha256": entry["source_sha256"],
                  "reviewer": "unit-test-only", "date": "2026-09-10",
                  "independence_group": entry["id"], "independence_notes": "fixture"}
        result.update({stage: {"status": "passed", "notes": "fixture",
                               "evidence": ["evidence.md"]} for stage in quality.STAGES})
        return result

    def test_authored_and_stale_are_not_accepted(self):
        self.assertEqual(self.evaluate([ENTRY], [])["counts"]["preset:preset"], 0)
        self.assertEqual(self.evaluate([ENTRY], [self.review()])["counts"]["preset:preset"], 1)
        changed = dict(ENTRY, source_sha256="b" * 64)
        result = self.evaluate([changed], [self.review()])
        self.assertEqual(result["counts"]["preset:preset"], 0)
        self.assertEqual(result["entries"][0]["stale_review_count"], 1)
        self.assertEqual(result["entries"][0]["user_acceptance"], "not_recorded")

    def test_defaults_and_identical_presets_cannot_inflate_count(self):
        with self.assertRaisesRegex(ValueError, "Default"):
            self.evaluate([dict(ENTRY, is_default=True)], [self.review()])
        other = dict(ENTRY, id="test.two")
        with self.assertRaisesRegex(ValueError, "Identical"):
            self.evaluate([ENTRY, other], [self.review(), self.review(other)])
        review = self.review(other)
        review["independence_group"] = ENTRY["id"]
        with self.assertRaisesRegex(ValueError, "counted twice"):
            self.evaluate([ENTRY, other], [self.review(), review])

    def test_missing_evidence_and_pending_stages(self):
        review = self.review()
        review["music"] = {"status": "pending"}
        self.assertEqual(self.evaluate([ENTRY], [review])["counts"]["preset:preset"], 0)
        review["visual"]["evidence"] = ["missing.md"]
        with self.assertRaisesRegex(ValueError, "missing"):
            self.evaluate([ENTRY], [review])


if __name__ == "__main__":
    unittest.main()

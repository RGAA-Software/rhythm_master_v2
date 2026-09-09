"""Exercise cross-process exclusion and failure preservation without a GPU."""

from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from verify_windows import run_logged, verification_lease


class VerificationTests(unittest.TestCase):
    def test_independent_process_waits_and_owner_exit_releases(self):
        with tempfile.TemporaryDirectory() as directory:
            lock = Path(directory) / "check.lock"
            script = "from verify_windows import verification_lease; import sys; " \
                     "lease=verification_lease(sys.argv[1]); lease.__enter__(); " \
                     "print('locked',flush=True); sys.stdin.read()"
            with subprocess.Popen([sys.executable, "-c", script, str(lock)],
                                  cwd=Path(__file__).parent, stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE, text=True) as owner:
                self.assertEqual(owner.stdout.readline().strip(), "locked")
                with self.assertRaises(TimeoutError):
                    with verification_lease(lock, timeout=0.2):
                        self.fail("independent owner was not excluded")
                owner.kill()
                owner.wait()
            with verification_lease(lock, timeout=0.2):
                pass

    def test_nonzero_exit_and_evidence_survive(self):
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory) / "failure.log"
            with self.assertRaises(subprocess.CalledProcessError):
                run_logged([sys.executable, "-c", "print('deliberate failure'); raise SystemExit(7)"], log)
            self.assertIn("deliberate failure", log.read_text(encoding="utf-8"))
            self.assertIn('"returncode": 7', log.with_suffix(".log.json").read_text(encoding="utf-8"))
            run_logged([sys.executable, "-c", "print('later pass')"], log)
            archives = list((log.parent / (log.name + ".runs")).glob("*.log"))
            self.assertEqual(len(archives), 2)
            self.assertTrue(any("deliberate failure" in p.read_text(encoding="utf-8") for p in archives))


if __name__ == "__main__":
    unittest.main()

import json
from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class PinwatchCliTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls._temporary_directory = tempfile.TemporaryDirectory()
        cls.binary = Path(cls._temporary_directory.name) / "pinwatch"
        subprocess.run(
            [
                "cc",
                "-std=c11",
                "-O2",
                "-Wall",
                "-Wextra",
                "-Werror",
                f"-I{ROOT / 'tests'}",
                str(ROOT / "pinwatch.c"),
                str(ROOT / "tests" / "mock_gpiolib.c"),
                "-o",
                str(cls.binary),
            ],
            check=True,
        )

    @classmethod
    def tearDownClass(cls):
        cls._temporary_directory.cleanup()

    def run_pinwatch(self, *arguments):
        return subprocess.run(
            [str(self.binary), *arguments],
            input="",
            capture_output=True,
            text=True,
            timeout=2,
        )

    def test_default_stream_omits_snapshot(self):
        result = self.run_pinwatch("--exit-on-stdin-close")

        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, "")

    def test_snapshot_once_emits_initial_pin_states(self):
        result = self.run_pinwatch("--snapshot", "--once", "--sample-us", "5000")

        self.assertEqual(result.returncode, 0, result.stderr)
        events = [json.loads(line) for line in result.stdout.splitlines()]
        self.assertEqual([event["event"] for event in events], ["pin_snapshot", "pin_snapshot"])
        self.assertEqual([event["physical_pin"] for event in events], [2, 3])
        self.assertTrue(all(event["sample_us"] == 5000 for event in events))

    def test_once_requires_snapshot(self):
        result = self.run_pinwatch("--once")

        self.assertEqual(result.returncode, 2)
        self.assertIn("--once requires --snapshot", result.stderr)

    def test_sample_interval_is_validated(self):
        result = self.run_pinwatch("--sample-us", "249")

        self.assertEqual(result.returncode, 2)
        self.assertIn("--sample-us must be between 250 and 1000000", result.stderr)


if __name__ == "__main__":
    unittest.main()

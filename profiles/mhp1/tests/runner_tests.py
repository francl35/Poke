#!/usr/bin/env python3
"""Process-level tests for the bounded macOS/Linux diagnostic runner."""
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest

RUNNER = Path(__file__).resolve().parents[1] / "scripts/run_headless.py"


class RunnerTests(unittest.TestCase):
    def setUp(self):
        self.scratch = tempfile.TemporaryDirectory()
        self.addCleanup(self.scratch.cleanup)
        self.root = Path(self.scratch.name)
        self.build = self.root / "build"
        (self.build / "bin").mkdir(parents=True)
        self.elf = self.root / "fixture.elf"
        self.iso = self.root / "fixture.iso"
        self.elf.write_bytes(b"synthetic placeholder")
        self.iso.write_bytes(b"synthetic placeholder")

    def run_fixture(self, body, seconds=1, extra=()):
        executable = self.build / "bin/PokeFreedomDXBoot"
        executable.write_text(f"#!{sys.executable}\n{body}\n")
        executable.chmod(0o700)
        return subprocess.run([sys.executable, str(RUNNER), "--iso", str(self.iso),
                               "--elf", str(self.elf), "--build-dir", str(self.build),
                               "--seconds", str(seconds), *extra], capture_output=True,
                              text=True, timeout=12, check=False)

    def test_diagnostic_stop_and_log(self):
        result = self.run_fixture("import sys\nprint('fixture boundary')\nsys.exit(4)")
        self.assertEqual(result.returncode, 4, result.stderr)
        self.assertIn("fixture boundary", (self.build / "boot.log").read_text())
        self.assertIn("Diagnostic exit 4", result.stdout)

    def test_renderer_and_capture_options(self):
        captures = self.root / "captures"
        result = self.run_fixture(
            "import os\nprint(os.environ.get('MHP1_RENDERER'))\nprint(os.environ.get('MHP1_RENDER_DIR'))\nraise SystemExit(4)",
            extra=("--renderer", "--capture-dir", str(captures)))
        self.assertEqual(result.returncode, 4, result.stderr)
        log = (self.build / "boot.log").read_text()
        self.assertIn("1\n", log)
        self.assertIn(str(captures.resolve()), log)

    def test_external_timeout(self):
        start = time.monotonic()
        result = self.run_fixture("import time\nprint('fixture started', flush=True)\ntime.sleep(8)")
        self.assertEqual(result.returncode, 124, result.stderr)
        self.assertLess(time.monotonic() - start, 8)
        self.assertIn("External timeout", (self.build / "boot.log").read_text())

    def test_invalid_seconds(self):
        result = self.run_fixture("raise SystemExit(4)", seconds=0)
        self.assertEqual(result.returncode, 2)
        self.assertFalse((self.build / "boot.log").exists())


if __name__ == "__main__":
    unittest.main()

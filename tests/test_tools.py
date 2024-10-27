import contextlib
import io
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]


def run(*args, cwd=None):
    return subprocess.run(args, cwd=cwd, check=True, capture_output=True, text=True)


@unittest.skipUnless(shutil.which("ffmpeg") and shutil.which("ffprobe"), "requires ffmpeg/ffprobe")
class ScriptTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name)

    def test_raw_rgb_frames(self):
        clip = self.work / "input.mkv"
        run("ffmpeg", "-v", "error", "-y", "-f", "lavfi", "-i",
            "testsrc2=size=32x32:rate=4:duration=1", "-c:v", "ffv1", str(clip))
        raw = self.work / "frames.rgb"
        run("sh", str(ROOT / "scripts/frames.sh"), str(clip), str(raw))
        self.assertEqual(raw.stat().st_size, 4 * 32 * 32 * 3)

    def test_gif_includes_frame_zero(self):
        frames = self.work / "out/frames"
        frames.mkdir(parents=True)
        for i in range(3):
            Image.new("RGB", (32, 32), (i * 80, 0, 0)).save(frames / f"f{i:04d}.ppm")
        run("sh", str(ROOT / "scripts/gif.sh"), "12", cwd=self.work)
        probe = run("ffprobe", "-v", "error", "-select_streams", "v:0", "-count_frames",
                    "-show_entries", "stream=nb_read_frames", "-of", "csv=p=0",
                    str(self.work / "out/moshed.mp4"))
        self.assertEqual(int(probe.stdout.strip()), 3)
        self.assertGreater((self.work / "out/moshed.gif").stat().st_size, 0)

    def test_silence_end_timestamps(self):
        wav = self.work / "pulse.wav"
        run("ffmpeg", "-v", "error", "-y", "-f", "lavfi", "-i",
            "sine=frequency=440:sample_rate=8000:duration=0.3", "-af", "adelay=300", str(wav))
        result = run("sh", str(ROOT / "scripts/beats.sh"), str(wav))
        timestamps = [int(line) for line in result.stdout.splitlines()]
        self.assertEqual(len(timestamps), 1)
        self.assertTrue(290 <= timestamps[0] <= 310, timestamps)

    def test_missing_inputs_fail(self):
        missing = str(self.work / "missing.mp4")
        for script, args in [("frames.sh", [missing, str(self.work / "frames.rgb")]),
                             ("beats.sh", [missing])]:
            result = subprocess.run(["sh", str(ROOT / "scripts" / script), *args],
                                    capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertTrue(result.stderr)


class ViewerTests(unittest.TestCase):
    def test_viewer_usage(self):
        result = subprocess.run([sys.executable, str(ROOT / "viewer.py"), "unused", "8"],
                                capture_output=True, text=True)
        self.assertIn("usage:", result.stdout)
        self.assertNotIn("Traceback", result.stderr)

    def test_tk_usage_and_input_validation(self):
        try:
            import viewer2
        except ImportError as error:
            self.skipTest(str(error))
        with contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(viewer2.main([]), 1)
        with tempfile.TemporaryDirectory() as work:
            path = Path(work) / "bad.rgb"
            path.write_bytes(b"short")
            with self.assertRaisesRegex(ValueError, "whole frames"):
                viewer2.main([str(path), "8", "8"])

    @unittest.skipUnless(os.environ.get("DISPLAY"), "requires X display (or xvfb-run)")
    def test_tk_can_show_and_close(self):
        try:
            import viewer2
        except ImportError as error:
            self.skipTest(str(error))
        make_root = viewer2.tk.Tk

        def auto_close():
            root = make_root()
            root.after(150, root.destroy)
            return root

        with tempfile.TemporaryDirectory() as work:
            path = Path(work) / "frames.rgb"
            path.write_bytes(bytes([255, 0, 0]) * 8 * 8 * 2)
            with mock.patch.object(viewer2.tk, "Tk", side_effect=auto_close):
                self.assertEqual(viewer2.main([str(path), "8", "8"]), 0)


if __name__ == "__main__":
    unittest.main()

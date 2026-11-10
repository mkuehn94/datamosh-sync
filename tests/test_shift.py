import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

import numpy
from PIL import Image

import datamosh

ROOT = Path(__file__).resolve().parents[1]


def numbered_frames(count=60, width=4, height=4):
    frames = numpy.empty((count, height, width, 3), dtype=numpy.uint8)
    for i in range(count):
        frames[i] = i
    return frames


def vector(frame=0, x=0, y=0, width=1, height=1, dx=1, dy=0):
    return (frame, x, y, width, height, dx, dy, "hold")


class ShiftTests(unittest.TestCase):
    def test_shift_lands_down_right(self):
        frames = numbered_frames(4, 8, 8)
        out = datamosh.mosh(frames, [vector(0, width=2, height=2, dy=1)], 8, 8)
        numpy.testing.assert_array_equal(out[1, 1:3, 1:3], frames[0, :2, :2])
        self.assertFalse(numpy.array_equal(out[1, 1:3, 1:3], frames[1, 1:3, 1:3]))

    def test_without_grid_only_next_frame_changes(self):
        frames = numbered_frames(4)
        out = datamosh.mosh(frames, [vector()], 4, 4)
        numpy.testing.assert_array_equal(out[0], frames[0])
        numpy.testing.assert_array_equal(out[2:], frames[2:])
        numpy.testing.assert_array_equal(frames[1], numpy.full((4, 4, 3), 1))

    def test_invalid_source_or_destination_is_skipped(self):
        frames = numbered_frames(4)
        invalid = [vector(-1), vector(4), vector(3), vector(x=-1), vector(y=-1),
                   vector(x=4), vector(y=4), vector(width=0), vector(height=-1),
                   vector(width=5), vector(dx=-1), vector(dx=4), vector(dy=4)]
        for v in invalid:
            with self.subTest(vector=v):
                numpy.testing.assert_array_equal(datamosh.mosh(frames, [v], 4, 4), frames)

    def test_empty_grid_keeps_one_frame_behavior(self):
        frames = numbered_frames(4)
        plain = datamosh.mosh(frames, [vector()], 4, 4)
        with_grid = datamosh.mosh(frames, [vector()], 4, 4, beats=[], fps=24)
        numpy.testing.assert_array_equal(with_grid, plain)
        self.assertEqual(datamosh.quantize([vector()], [], 24), [vector()])


class BeatTests(unittest.TestCase):
    def test_quantize_keeps_source_frame_numbers(self):
        vecs = [vector(0), vector(3)]
        self.assertEqual(datamosh.quantize(vecs, [0, 42, 83], 24), vecs)

    def test_first_vector_in_sidecar_order_per_beat_wins(self):
        vecs = [vector(6), vector(2), vector(24), vector(30)]
        self.assertEqual(datamosh.quantize(vecs, [0, 1000, 2000], 24), [vecs[0], vecs[2]])

    def test_two_second_vector_does_not_jump_to_start_of_video(self):
        frames = numbered_frames()
        out = datamosh.mosh(frames, [vector(48)], 4, 4, beats=[0, 1000, 2000], fps=24)
        numpy.testing.assert_array_equal(out[:49], frames[:49])
        numpy.testing.assert_array_equal(out[49:, 0, 1], numpy.full((11, 3), 48))

    def test_patch_holds_until_next_beat_then_releases(self):
        frames = numbered_frames()
        out = datamosh.mosh(frames, [vector(6)], 4, 4, beats=[0, 1000, 2000], fps=24)
        numpy.testing.assert_array_equal(out[:7], frames[:7])
        numpy.testing.assert_array_equal(out[7:24, 0, 1], numpy.full((17, 3), 6))
        numpy.testing.assert_array_equal(out[24:], frames[24:])
        numpy.testing.assert_array_equal(out[:, 1:], frames[:, 1:])
        numpy.testing.assert_array_equal(out[:, 0, 0], frames[:, 0, 0])
        numpy.testing.assert_array_equal(out[:, 0, 2:], frames[:, 0, 2:])

    def test_following_beat_uses_its_own_source(self):
        frames = numbered_frames()
        out = datamosh.mosh(frames, [vector(6), vector(24)], 4, 4,
                            beats=[0, 1000, 2000], fps=24)
        numpy.testing.assert_array_equal(out[24], frames[24])
        numpy.testing.assert_array_equal(out[25:48, 0, 1], numpy.full((23, 3), 24))
        numpy.testing.assert_array_equal(out[48:], frames[48:])

    def test_vectors_before_first_beat_are_not_moved_forward(self):
        frames = numbered_frames()
        vecs = [vector(0), vector(12)]
        self.assertEqual(datamosh.quantize(vecs, [500, 1000], 24), [vecs[1]])
        out = datamosh.mosh(frames, vecs, 4, 4, beats=[500, 1000], fps=24)
        numpy.testing.assert_array_equal(out[:13], frames[:13])
        numpy.testing.assert_array_equal(out[13:24, 0, 1], numpy.full((11, 3), 12))
        numpy.testing.assert_array_equal(out[24:], frames[24:])

    def test_submillisecond_precision_is_not_truncated(self):
        # Frame 1 is at ~41.708ms, after the 41.7ms boundary.
        vecs = [vector(0), vector(1)]
        self.assertEqual(datamosh.quantize(vecs, [0, 41.7], 23.976), vecs)

    def test_fractional_fps_hold_boundary(self):
        frames = numbered_frames()
        out = datamosh.mosh(frames, [vector(15)], 4, 4, beats=[0, 1000], fps=29.97)
        numpy.testing.assert_array_equal(out[16:30, 0, 1], numpy.full((14, 3), 15))
        numpy.testing.assert_array_equal(out[30:], frames[30:])

    def test_vector_just_before_boundary_cannot_outlive_its_beat(self):
        frames = numbered_frames()
        out = datamosh.mosh(frames, [vector(23)], 4, 4, beats=[0, 1000], fps=24)
        numpy.testing.assert_array_equal(out, frames)

    def test_single_beat_holds_to_end_of_clip(self):
        frames = numbered_frames(8)
        out = datamosh.mosh(frames, [vector(2)], 4, 4, beats=[0], fps=24)
        numpy.testing.assert_array_equal(out[3:, 0, 1], numpy.full((5, 3), 2))

    def test_read_beats_keeps_fractional_milliseconds(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "beats.txt"
            path.write_text("0\n41.7\n")
            self.assertEqual(datamosh.read_beats(path), [0, 41.7])

    def test_invalid_fps_is_rejected(self):
        for fps in (0, -24, float("nan"), float("inf")):
            with self.subTest(fps=fps), self.assertRaises(ValueError):
                datamosh.quantize([vector()], [0, 1000], fps)

    def test_invalid_beat_grid_is_rejected(self):
        for beats in ([1000, 0], [0, 0], [-1, 1000], [0, float("nan")], [0, float("inf")]):
            with self.subTest(beats=beats), self.assertRaises(ValueError):
                datamosh.quantize([vector()], beats, 24)

    def test_cli_writes_held_patch_at_correct_time(self):
        frames = numbered_frames()
        with tempfile.TemporaryDirectory() as temp:
            work = Path(temp)
            (work / "frames.rgb").write_bytes(frames.tobytes())
            (work / "blocks.mv").write_text("48 0 0 1 1 1 0 hold\n")
            (work / "beats.txt").write_text("0\n1000\n2000\n")
            subprocess.run([sys.executable, str(ROOT / "datamosh.py"), "frames.rgb", "4", "4",
                            "blocks.mv", "beats.txt", "24"], cwd=work, check=True, capture_output=True)
            output = work / "out/frames"
            self.assertEqual(len(list(output.glob("f*.ppm"))), 60)
            for index in (3, 48, 49, 59):
                with Image.open(output / f"f{index:04d}.ppm") as image:
                    actual = numpy.array(image)
                expected = frames[index].copy()
                if index >= 49:
                    expected[0, 1] = frames[48, 0, 0]
                numpy.testing.assert_array_equal(actual, expected)

    def test_cli_accepts_fractional_fps(self):
        frames = numbered_frames(40)
        with tempfile.TemporaryDirectory() as temp:
            work = Path(temp)
            (work / "frames.rgb").write_bytes(frames.tobytes())
            (work / "blocks.mv").write_text("15 0 0 1 1 1 0 hold\n")
            (work / "beats.txt").write_text("0\n1000\n")
            subprocess.run([sys.executable, str(ROOT / "datamosh.py"), "frames.rgb", "4", "4",
                            "blocks.mv", "beats.txt", "29.97"], cwd=work, check=True, capture_output=True)
            for index in (15, 16, 29, 30):
                with Image.open(work / "out/frames" / f"f{index:04d}.ppm") as image:
                    actual = numpy.array(image)
                expected = frames[index].copy()
                if 16 <= index < 30:
                    expected[0, 1] = frames[15, 0, 0]
                numpy.testing.assert_array_equal(actual, expected)


if __name__ == "__main__":
    unittest.main()

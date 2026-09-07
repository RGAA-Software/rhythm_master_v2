"""Generate tiny, deterministic original media fixtures with vcpkg's offline FFmpeg tool."""

import argparse
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--ffmpeg", type=Path, default=Path(
        "C:/source/vcpkg/installed/x64-windows-static-release/tools/ffmpeg/ffmpeg.exe"))
    args = parser.parse_args()
    root = args.output.resolve()
    root.mkdir(parents=True, exist_ok=True)

    def ffmpeg(*options):
        subprocess.run([str(args.ffmpeg), "-hide_banner", "-loglevel", "error", "-nostdin", "-y",
                        *map(str, options)], check=True, timeout=60)

    raw = root / "frames.rgba"
    raw.write_bytes(b"".join(bytes([index * 20, 80, 200, 255]) * (64 * 48) for index in range(10)))
    source = ["-f", "rawvideo", "-pixel_format", "rgba", "-video_size", "64x48", "-framerate", "5", "-i", raw]
    ffmpeg(*source, "-c:v", "ffv1", "-pix_fmt", "bgra", root / "视频.mkv")
    ffmpeg(*source, "-c:v", "mpeg4", "-bf", "2", "-q:v", "2", root / "reordered.mp4")
    ffmpeg("-display_rotation", "90", "-i", root / "reordered.mp4", "-c", "copy", root / "rotated.mp4")
    (root / "alpha.rgba").write_bytes(bytes([120, 40, 200, 64]) * (64 * 48))
    ffmpeg("-f", "rawvideo", "-pixel_format", "rgba", "-video_size", "64x48", "-i", root / "alpha.rgba",
           "-frames:v", "1", root / "alpha.png")
    (root / "sequence.ffconcat").write_text(
        "ffconcat version 1.0\nfile alpha.png\nduration 0.04\nfile alpha.png\nduration 0.12\n"
        "file alpha.png\nduration 0.08\nfile alpha.png\n", encoding="utf-8")
    ffmpeg("-f", "concat", "-i", root / "sequence.ffconcat", "-fps_mode", "vfr", "-c:v", "ffv1", root / "variable.mkv")
    (root / "broken.bin").write_bytes(b"invalid original fixture\x00" * 30)
    print(root)


if __name__ == "__main__":
    main()

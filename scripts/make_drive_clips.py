#!/usr/bin/env python3
"""Turn your recordings into the clips CHIPO DRIVE embeds in the firmware.

  python scripts/make_drive_clips.py <folder with recordings>      convert what is there
  python scripts/make_drive_clips.py --list                        show every line to record

A recording is matched by its file name (any audio format ffmpeg reads: wav, mp3, m4a...):
  drive_start.m4a   ->  components/chipo_drive/clips/drive_start.ogg   (OGG/Opus, 48 kHz, mono)
Needs ffmpeg on the PATH. Rebuild the firmware afterwards. Missing clips are fine: CHIPO shows the
text of the line instead.
"""
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "components" / "chipo_drive" / "chipo_drive_personality.cpp"
HEADER = ROOT / "components" / "chipo_drive" / "include" / "chipo_drive_personality.h"
CLIPS = ROOT / "components" / "chipo_drive" / "clips"


def lines_to_record():
    """(id, text) for every spoken line, read from the source so the list never goes stale."""
    out = []
    text = SRC.read_text(encoding="utf-8")
    for m in re.finditer(r'\{"(drive_[a-z0-9_]+)",\s*"([^"]+)"', text):
        out.append((m.group(1), m.group(2)))
    for m in re.finditer(r'\{(kDriveClip\w+),\s*"([^"]+)"\}', text):
        name = re.search(r'constexpr const char\* ' + m.group(1) + r' = "([^"]+)"', HEADER.read_text(encoding="utf-8"))
        if name:
            out.append((name.group(1), m.group(2)))
    return out


def main():
    lines = lines_to_record()
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    if sys.argv[1] == "--list":
        print(f"{'file name':<22} {'in firmware':<12} line to record")
        for clip_id, text in lines:
            have = (CLIPS / f"{clip_id}.ogg").exists()
            print(f"{clip_id:<22} {'yes' if have else 'no':<12} {text}")
        print(f"{'drive_start_sfx':<22} {'yes' if (CLIPS / 'drive_start_sfx.ogg').exists() else 'no':<12} (engine sound: plays if drive_start is not recorded)")
        return
    folder = Path(sys.argv[1])
    if not folder.is_dir():
        raise SystemExit(f"Not a folder: {folder}")
    if shutil.which("ffmpeg") is None:
        raise SystemExit("ffmpeg not found. Install it and add it to the PATH.")
    ids = {clip_id for clip_id, _ in lines} | {"drive_start_sfx"}
    CLIPS.mkdir(parents=True, exist_ok=True)
    done = 0
    for f in sorted(folder.iterdir()):
        if f.suffix.lower() in (".txt", ".md") or f.stem not in ids:
            if f.is_file() and f.suffix.lower() not in (".txt", ".md"):
                print(f"[skipped] {f.name}: no line called '{f.stem}' (see --list)")
            continue
        target = CLIPS / f"{f.stem}.ogg"
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", str(f), "-af", "loudnorm=I=-18:TP=-2",
                        "-ac", "1", "-ar", "48000", "-c:a", "libopus", "-b:a", "32k", str(target)], check=True)
        print(f"[ok] {f.name} -> {target.relative_to(ROOT)}")
        done += 1
    print(f"{done} clip(s) converted. Rebuild the firmware to embed them.")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Generates the three BUDDY sound effects (OGG/Opus 48 kHz mono) into components/chipo_drive/clips/.

    python3 scripts/make_buddy_sfx.py

They are plain synthesized tones (no recordings, no copyright). The files are committed; this script
only exists so they can be regenerated or tuned. Needs numpy and ffmpeg with libopus.
"""
import subprocess, sys, tempfile, wave
from pathlib import Path
import numpy as np

SR = 48000
OUT = Path(__file__).resolve().parent.parent / "components" / "chipo_drive" / "clips"


def tone(freq, dur, vol=0.5, attack=0.004, release=0.05, harm=(1.0, 0.25, 0.1)):
    t = np.arange(int(SR * dur)) / SR
    y = sum(h * np.sin(2 * np.pi * freq * (i + 1) * t) for i, h in enumerate(harm))
    y /= sum(harm)
    env = np.minimum(1, t / attack) * np.minimum(1, (dur - t) / release)
    return (vol * y * env).astype(np.float32)


def silence(dur):
    return np.zeros(int(SR * dur), dtype=np.float32)


def save(name, samples):
    samples = np.clip(samples, -0.95, 0.95)
    with tempfile.TemporaryDirectory() as d:
        wav = Path(d) / "a.wav"
        with wave.open(str(wav), "wb") as w:
            w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
            w.writeframes((samples * 32767).astype("<i2").tobytes())
        OUT.mkdir(parents=True, exist_ok=True)
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", str(wav), "-c:a", "libopus", "-b:a", "32k",
                        "-ar", "48000", "-ac", "1", str(OUT / f"{name}.ogg")], check=True)
    print(f"{name}.ogg  {len(samples) / SR:.2f} s")


# short "ok": two rising notes
save("buddy_chime", np.concatenate([tone(880, 0.09), tone(1318, 0.16, release=0.1)]))
# timer finished: three beeps
beep = np.concatenate([tone(1760, 0.11, vol=0.55), silence(0.09)])
save("buddy_timer_done", np.concatenate([beep, beep, beep]))
# alarm: rising little melody, twice
mel = np.concatenate([tone(f, 0.16, vol=0.55) for f in (523, 659, 784, 1047)] + [silence(0.12)])
save("buddy_alarm", np.concatenate([mel, mel]))

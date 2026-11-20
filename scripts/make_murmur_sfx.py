#!/usr/bin/env python3
"""Generates CHIPO's offline "murmur" clips (cheerful made-up babble, OGG/Opus 48 kHz mono) into
components/chipo_drive/clips/murmur_1..4.ogg and oops.ogg ("uuuy", the fall).

    python3 scripts/make_murmur_sfx.py

Pure synthesis (a pulse train through vowel formant filters, then a little giggle): no recordings, no
words, nobody's voice. The files are committed; this only exists to regenerate or tune them.
Needs numpy, scipy and ffmpeg with libopus.
"""
import subprocess, tempfile, wave
from pathlib import Path
import numpy as np
from scipy.signal import lfilter

SR = 48000
OUT = Path(__file__).resolve().parent.parent / "components" / "chipo_drive" / "clips"
VOWELS = {"a": (800, 1150), "e": (500, 1900), "i": (320, 2300), "o": (500, 900), "u": (320, 800)}


def resonator(x, f, bw=110):
    r = np.exp(-np.pi * bw / SR)
    th = 2 * np.pi * f / SR
    return lfilter([1 - r], [1, -2 * r * np.cos(th), r * r], x)


def syllable(rng, f0, dur, vowel, glide):
    n = int(SR * dur)
    t = np.arange(n) / SR
    f = f0 * (1 + glide * t / dur) * (1 + 0.02 * np.sin(2 * np.pi * 18 * t))
    phase = 2 * np.pi * np.cumsum(f) / SR
    src = sum(np.sin(k * phase) / k for k in range(1, 14))
    f1, f2 = VOWELS[vowel]
    y = resonator(src, f1) + 0.8 * resonator(src, f2, 150)
    y /= np.max(np.abs(y)) + 1e-9
    burst = rng.standard_normal(int(SR * 0.012)) * np.linspace(1, 0, int(SR * 0.012)) * 0.25  # consonant pop
    y = np.concatenate([burst, y])
    n = len(y)
    env = np.minimum(1, np.arange(n) / (SR * 0.01)) * np.minimum(1, (n - np.arange(n)) / (SR * 0.03))
    return (y * env).astype(np.float32)


def make(seed):
    rng = np.random.default_rng(seed)
    parts = []
    for i in range(int(rng.integers(6, 9))):
        parts.append(syllable(rng, float(rng.uniform(330, 470)), float(rng.uniform(0.07, 0.15)),
                              str(rng.choice(list(VOWELS))), float(rng.uniform(-0.25, 0.35))))
        parts.append(np.zeros(int(SR * rng.uniform(0.0, 0.05)), dtype=np.float32))
    for i in range(4):  # giggle: quick alternating notes
        parts.append(syllable(rng, 520 + 70 * (i % 2), 0.065, "i", 0.1))
    y = np.concatenate(parts)
    return 0.6 * y / (np.max(np.abs(y)) + 1e-9)


def oops():
    """'uuuuy': a long falling slide (u to i) and a little thud at the end."""
    n = int(SR * 0.9)
    t = np.arange(n) / SR
    f = 560 * np.exp(-1.1 * t / 0.9 * 1.0) + 120  # slides down
    f *= 1 + 0.03 * np.sin(2 * np.pi * 7 * t)
    phase = 2 * np.pi * np.cumsum(f) / SR
    src = sum(np.sin(k * phase) / k for k in range(1, 12))
    mix = np.clip(t / 0.9, 0, 1)  # u -> i
    y = (1 - mix) * (resonator(src, 320) + 0.8 * resonator(src, 800, 150)) + mix * (resonator(src, 320) + 0.8 * resonator(src, 2300, 150))
    y /= np.max(np.abs(y)) + 1e-9
    y *= np.minimum(1, t / 0.02) * np.minimum(1, (0.9 - t) / 0.08)
    tt = np.arange(int(SR * 0.16)) / SR
    thud = np.sin(2 * np.pi * (140 - 60 * tt / 0.16) * tt) * np.exp(-tt * 28)  # bonk
    return 0.6 * np.concatenate([y, np.zeros(int(SR * 0.05)), 0.9 * thud]).astype(np.float32)


CLIPS = [(f"murmur_{k}", make(100 + k)) for k in range(1, 5)] + [("oops", oops())]
for name, samples in CLIPS:
    s = np.clip(samples, -0.95, 0.95)
    with tempfile.TemporaryDirectory() as d:
        wav = Path(d) / "a.wav"
        with wave.open(str(wav), "wb") as w:
            w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
            w.writeframes((s * 32767).astype("<i2").tobytes())
        OUT.mkdir(parents=True, exist_ok=True)
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", str(wav), "-c:a", "libopus", "-b:a", "32k",
                        "-ar", "48000", "-ac", "1", str(OUT / f"{name}.ogg")], check=True)
    print(f"{name}.ogg  {len(s) / SR:.2f} s")

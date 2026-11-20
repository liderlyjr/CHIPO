#!/usr/bin/env python3
"""Generates the BUDDY and FUN sound effects (OGG/Opus 48 kHz mono) into components/chipo_drive/clips/.

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


# ---- CHIPO FUN ---------------------------------------------------------------------------------
rng = np.random.default_rng(7)


def swell_chord(freqs, dur, vol=0.35):
    t = np.arange(int(SR * dur)) / SR
    y = sum(np.sin(2 * np.pi * f * t) + 0.3 * np.sin(2 * np.pi * 2 * f * t) for f in freqs) / (len(freqs) * 1.3)
    env = np.minimum(1, t / 0.08) * np.exp(-t * 1.6) * np.minimum(1, (dur - t) / 0.3)
    return (vol * y * env).astype(np.float32)


# start-up: a warm C major chord with a little rising arpeggio before it (original, not any real chime)
boot = np.concatenate([tone(523, 0.10, vol=0.4), tone(659, 0.10, vol=0.4), tone(784, 0.10, vol=0.4), swell_chord([262, 330, 392, 523], 1.7)])
save("fun_boot", boot)

# dice rattle: short noise ticks (band limited by a moving average), getting sparser
def tick(dur=0.012, vol=0.5):
    n = int(SR * dur)
    x = rng.standard_normal(n).astype(np.float32)
    x = np.convolve(x, np.ones(6) / 6, mode="same")
    return (vol * x * np.linspace(1, 0, n) ** 2).astype(np.float32)

roll = np.zeros(int(SR * 1.2), dtype=np.float32)
t0 = 0.0
gap = 0.03
while t0 < 1.1:
    i = int(t0 * SR)
    k = tick(vol=0.55 * (1.0 - t0 / 1.3))
    roll[i:i + len(k)] += k[: len(roll) - i]
    t0 += gap * (1 + rng.random() * 0.6)
    gap *= 1.07
save("fun_roll", roll)

# coin: a metallic ping with two inharmonic partials
t = np.arange(int(SR * 1.0)) / SR
ping = 0.4 * (np.sin(2 * np.pi * 2700 * t) * np.exp(-t * 7) + 0.6 * np.sin(2 * np.pi * 4130 * t) * np.exp(-t * 11))
ping *= np.minimum(1, t / 0.002)
save("fun_coin", np.concatenate([tick(0.02, 0.4), silence(0.25), ping.astype(np.float32)]))

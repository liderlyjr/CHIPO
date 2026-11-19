#!/usr/bin/env python3
"""Generates CHIPO's ambient sounds (rain, fire, soft noise) as short OGG/Opus pieces:

    components/chipo_drive/clips/ambient_<name>_<n>.ogg      n = 0 .. PIECES-1, 0.96 s each, 60 ms Opus frames

    python3 scripts/make_ambient_sfx.py

Why pieces: XiaoZhi's audio queue holds only 20 Opus packets (1.2 s) and AudioService::PlaySound blocks the caller
until everything is queued, so a long clip would freeze the main loop. 0.96 s = exactly 16 packets of 60 ms: CHIPO
queues one piece per second, and pieces are consecutive parts of ONE encoded stream (ffmpeg segment muxer), so the
decoder state carries over from one piece to the next. The loop (PIECES x 0.96 s = 8.64 s) is built to be seamless.

Pure synthesis, no recordings. Needs numpy, scipy and ffmpeg with libopus.
"""
import subprocess, tempfile, wave
from pathlib import Path
import numpy as np
from scipy.signal import butter, lfilter

SR = 48000
PIECES = 9
PIECE_S = 0.96
LOOP_S = PIECES * PIECE_S
OUT = Path(__file__).resolve().parent.parent / "components" / "chipo_drive" / "clips"
rng = np.random.default_rng(2026)


def band(x, lo, hi, order=2):
    b, a = butter(order, [lo / (SR / 2), hi / (SR / 2)], btype="band")
    return lfilter(b, a, x)


def low(x, hz, order=2):
    b, a = butter(order, hz / (SR / 2), btype="low")
    return lfilter(b, a, x)


def make_loop(raw, tail=0.5):
    """Seamless loop of LOOP_S seconds from `raw` (longer than LOOP_S + tail): the tail is cross-faded onto the head."""
    n = int(SR * LOOP_S)
    k = int(SR * tail)
    head = raw[:n].copy()
    fade = np.linspace(0, 1, k)
    head[:k] = head[:k] * fade + raw[n:n + k] * (1 - fade)
    return head


def wrap_events(n, times, shape, amps):
    """Place events (decaying shapes) on a circular timeline so the loop has no seam."""
    y = np.zeros(n)
    for t, a in zip(times, amps):
        i = int(t * SR)
        seg = shape * a
        end = min(n, i + len(seg))
        y[i:end] += seg[:end - i]
        if i + len(seg) > n:
            y[:i + len(seg) - n] += seg[end - i:]
    return y


def rain():
    n_raw = int(SR * (LOOP_S + 0.5))
    base = band(rng.standard_normal(n_raw), 1500, 9000) * 0.5
    base += low(rng.standard_normal(n_raw), 400) * 0.35  # distant rumble
    y = make_loop(base)
    n = len(y)
    drop = np.exp(-np.arange(int(SR * 0.02)) / (SR * 0.004)) * rng.standard_normal(int(SR * 0.02))
    drop = band(drop, 2500, 12000)
    times = rng.uniform(0, LOOP_S - 0.05, int(LOOP_S * 55))
    y += wrap_events(n, times, drop, rng.uniform(0.2, 1.0, len(times))) * 0.5
    return y


def fire():
    n_raw = int(SR * (LOOP_S + 0.5))
    rumble = low(np.cumsum(rng.standard_normal(n_raw)) * 0.002, 220)  # brown noise, low roar
    hb, ha = butter(2, 45 / (SR / 2), btype="high")  # no inaudible drift below 45 Hz (it would eat the loudness)
    rumble = lfilter(hb, ha, rumble)
    hiss = band(rng.standard_normal(n_raw), 3000, 9000) * 0.06
    y = make_loop(rumble * 5.0 + hiss * 2)
    n = len(y)
    pop = np.exp(-np.arange(int(SR * 0.03)) / (SR * 0.006)) * rng.standard_normal(int(SR * 0.03))
    pop = band(pop, 800, 7000)
    times = np.sort(rng.uniform(0, LOOP_S - 0.05, int(LOOP_S * 9)))
    amps = rng.uniform(0.15, 1.0, len(times)) ** 2
    y += wrap_events(n, times, pop, amps) * 0.9
    return y


def noise():
    n_raw = int(SR * (LOOP_S + 0.5))
    pink = rng.standard_normal(n_raw)
    b = [0.049922035, -0.095993537, 0.050612699, -0.004408786]  # classic pink-noise filter
    a = [1, -2.494956002, 2.017265875, -0.522189400]
    return make_loop(lfilter(b, a, pink))


def finish(y, rms=0.09):
    """Same loudness for every sound (RMS), with a soft limiter so crackles and drops cannot clip."""
    y = y - np.mean(y)
    y = np.tanh(y / np.std(y) / 2.0)
    return (rms * y / np.sqrt(np.mean(y ** 2))).astype(np.float32)


def save(name, samples):
    for old in OUT.glob(f"ambient_{name}_*.ogg"):
        old.unlink()
    with tempfile.TemporaryDirectory() as d:
        wav = Path(d) / "a.wav"
        with wave.open(str(wav), "wb") as w:
            w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
            w.writeframes((np.clip(samples, -0.95, 0.95) * 32767).astype("<i2").tobytes())
        OUT.mkdir(parents=True, exist_ok=True)
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", str(wav), "-c:a", "libopus", "-b:a", "28k",
                        "-application", "audio", "-frame_duration", "60", "-ar", "48000", "-ac", "1",
                        "-f", "segment", "-segment_time", str(PIECE_S), "-segment_format", "ogg",
                        "-reset_timestamps", "1", str(OUT / f"ambient_{name}_%d.ogg")], check=True)
    for extra in OUT.glob(f"ambient_{name}_*.ogg"):  # ffmpeg adds a stub with the last packet: not part of the loop
        if int(extra.stem.rsplit("_", 1)[1]) >= PIECES:
            extra.unlink()
    pieces = sorted(OUT.glob(f"ambient_{name}_*.ogg"))
    print(f"ambient_{name}: {len(pieces)} pieces, {sum(p.stat().st_size for p in pieces)} bytes")


for name, fn in (("rain", rain), ("fire", fire), ("noise", noise)):
    save(name, finish(fn()))

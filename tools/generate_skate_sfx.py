"""Original deterministic skate Foley. No recordings or third-party samples.
Sources: mono signed PCM16 WAV, 32000 Hz. Runtime: raw PCM16 LE .pcm slots.
Requires numpy. Run: uv run --with numpy python project/tools/generate_skate_sfx.py
"""

import argparse, hashlib, json, math, wave, zipfile
from pathlib import Path
import numpy as np

RATE = 32000
SEED = 810501
SURFACES = ("generic", "stone", "concrete", "wood", "metal", "dirt", "grass")
CUES = (
    "push",
    "pop",
    "land_soft",
    "land_medium",
    "land_hard",
    "grind_start",
    "grind_end",
    "slide_start",
    "slide_end",
    "bail_board",
    "bail_body",
    "pickup",
    "drop",
    "mount",
    "dismount",
    "rolling",
    "grind_loop",
    "slide_loop",
)
NS = "SkateHarkinian/Audio/Skate/"


def noise(n, seed, low, high, color=0):
    rng = np.random.default_rng(seed)
    x = rng.normal(0, 1, n)
    s = np.fft.rfft(x)
    f = np.fft.rfftfreq(n, 1 / RATE)
    # Smooth band limits and spectrum tilt. FFT defines a periodic filtered waveform.
    gain = (
        np.exp(-((f / max(high, 1)) ** 4))
        * (1 - np.exp(-((f / max(low, 1)) ** 4)))
        / (np.maximum(f, 30) / 100) ** color
    )
    s *= gain
    s[0] = 0
    x = np.fft.irfft(s, n)
    return x / (np.sqrt(np.mean(x * x)) + 1e-12)


def finish(x, peak, loop=False):
    x = np.asarray(x, dtype=float)
    x -= x.mean()
    if loop:
        # Match boundary value and slope over the last12ms using a Hermite correction.
        size = 384
        t = np.linspace(0, 1, size)
        value = x[0] - x[-1]
        slope = (x[1] - x[0]) - (x[-1] - x[-2])
        x[-size:] += value * (3 * t * t - 2 * t * t * t) + slope * (size - 1) * (t * t * t - t * t)
    else:
        fade = min(160, len(x) // 4)
        x[:fade] *= np.linspace(0, 1, fade)
        x[-fade:] *= np.linspace(1, 0, fade)
        x[0] = x[-1] = 0
    x = np.tanh(x * 0.6)
    x *= peak / (np.max(np.abs(x)) + 1e-12)
    return np.rint(np.clip(x, -0.98, 0.98) * 32767).astype("<i2")


def roll(surface, seed):
    n = RATE * 2
    t = np.arange(n) / RATE
    low, high, color = {
        "concrete": (45, 3200, 0.45),
        "wood": (55, 1600, 0.7),
        "metal": (100, 4400, 0.2),
        "dirt": (60, 2000, 0.7),
        "grass": (40, 650, 1),
    }[surface]
    x = noise(n, seed, low, high, color) * 0.55 + noise(n, seed + 1, 20, 220, 0.8) * 0.3
    x *= 0.82 + 0.13 * np.sin(2 * np.pi * 17 * t) + 0.05 * np.sin(2 * np.pi * 31 * t)
    if surface == "wood":
        x += 0.2 * np.sin(2 * np.pi * 180 * t) * (0.5 + 0.5 * np.sin(2 * np.pi * 13 * t)) + 0.12 * np.sin(
            2 * np.pi * 420 * t
        )
    if surface == "metal":
        x += 0.1 * np.sin(2 * np.pi * 950 * t) + 0.055 * np.sin(2 * np.pi * 1810 * t)
    return finish(x, {"concrete": 0.68, "wood": 0.64, "metal": 0.62, "dirt": 0.44, "grass": 0.28}[surface], True)


def impact(surface, strength, seed, duration=None):
    n = int(RATE * (duration or (0.12, 0.22, 0.36)[strength]))
    t = np.arange(n) / RATE
    damp = (0.022, 0.045, 0.075)[strength]
    freq = {"concrete": 150, "wood": 190, "metal": 360, "dirt": 95, "grass": 75}[surface]
    body = np.sin(2 * np.pi * freq * t) * np.exp(-t / damp) + 0.35 * np.sin(2 * np.pi * freq * 2.73 * t) * np.exp(
        -t / (damp * 0.6)
    )
    clack = noise(n, seed, 450, 6500 if surface == "metal" else 4500, 0.1) * np.exp(-t / (0.008 + 0.009 * strength))
    thump = np.sin(2 * np.pi * (75 - 10 * strength) * t) * np.exp(-t / (0.04 + 0.04 * strength))
    if surface == "wood":
        body += 0.4 * np.sin(2 * np.pi * 510 * t) * np.exp(-t / 0.05)
    if surface == "metal":
        body += 0.5 * np.sin(2 * np.pi * 1300 * t) * np.exp(-t / (0.025 + 0.02 * strength))
    soft = surface in ("dirt", "grass")
    x = body * 0.48 + clack * (0.3 if soft else 0.75) + thump * (0.18 + 0.15 * strength)
    if strength == 2:
        x += noise(n, seed + 1, 180, 2600, 0.4) * np.exp(-t / 0.13) * (0.07 + 0.08 * np.sin(2 * np.pi * 33 * t) ** 2)
    return finish(x, ((0.4, 0.62, 0.79)[strength]) * (0.58 if surface == "grass" else 0.8 if surface == "dirt" else 1))


def scrape(metal, seed, loop=False, end=False):
    duration = 2 if loop else 0.16 if end else 0.12
    n = int(RATE * duration)
    t = np.arange(n) / RATE
    x = noise(n, seed, 180 if metal else 70, 5000 if metal else 2700, 0.15 if metal else 0.8)
    x *= 0.68 + 0.21 * np.sin(2 * np.pi * 43 * t) + 0.11 * np.sin(2 * np.pi * 79 * t)
    if metal:
        x += 0.22 * np.sin(2 * np.pi * 1460 * t) + 0.15 * np.sin(2 * np.pi * 2780 * t)
    else:
        x += 0.18 * noise(n, seed + 1, 20, 240, 0.8)
    if not loop:
        x *= np.exp(-t / (0.045 if end else 0.022))
    return finish(x, 0.57 if loop else 0.65, loop)


def generate(out, archive):
    out.mkdir(parents=True, exist_ok=True)
    sounds = {}
    seq = 0

    def add(name, data):
        sounds[name] = data

    for i, s in enumerate(("concrete", "wood", "dirt", "grass", "metal")):
        add(f"roll_{s}_loop.wav", roll(s, SEED + i * 31))
    for i in range(2):
        n = int(RATE * 0.20)
        t = np.arange(n) / RATE
        x = noise(n, SEED + 100 + i, 100, 2200, 0.55) * (np.sin(np.pi * np.minimum(1, t / 0.18)) ** 2) + 0.24 * np.sin(
            2 * np.pi * 120 * t
        ) * np.exp(-t / 0.04)
        add(f"push_{i+1:02}.wav", finish(x, 0.43))
    for i in range(3):
        add(f"pop_{i+1:02}.wav", impact("wood", 1, SEED + 200 + i, 0.15 + i * 0.012))
    for j, s in enumerate(("concrete", "wood", "metal", "dirt", "grass")):
        for i, level in enumerate(("soft", "medium", "hard")):
            add(f"land_{s}_{level}.wav", impact(s, i, SEED + 300 + j * 13 + i))
    for kind, metal in [("grind", True), ("slide", False)]:
        for j, part in enumerate(("start", "loop", "end")):
            add(
                f"{kind}_{part}.wav",
                scrape(metal, SEED + 500 + (30 if metal else 0) + j, part == "loop", part == "end"),
            )
    for i in range(2):
        n = int(RATE * 0.55)
        x = np.zeros(n)
        for j, at in enumerate((0, 0.057, 0.17, 0.31)):
            hit = impact("wood" if j % 2 == 0 else "metal", 1, SEED + 600 + i * 10 + j, 0.14).astype(float) / 32767
            start = int(at * RATE)
            x[start : start + len(hit)] += hit * (1 - j * 0.18)
        add(f"bail_board_{i+1:02}.wav", finish(x, 0.69))
    for i, kind in enumerate(("soft", "hard")):
        n = int(RATE * (0.22 + i * 0.12))
        t = np.arange(n) / RATE
        x = np.sin(2 * np.pi * (72 - i * 12) * t) * np.exp(-t / (0.045 + i * 0.035)) + 0.35 * noise(
            n, SEED + 700 + i, 30, 500, 0.9
        ) * np.exp(-t / 0.045)
        add("bail_body_" + kind + ".wav", finish(x, 0.45 + i * 0.17))
    for i, kind in enumerate(("pickup", "drop", "mount", "dismount")):
        add("board_" + kind + ".wav", impact("wood", 0 if i == 0 else 1, SEED + 800 + i, 0.15 + i * 0.02))
    for name, pcm in sounds.items():
        with wave.open(str(out / name), "wb") as w:
            w.setparams((1, 2, RATE, len(pcm), "NONE", "not compressed"))
            w.writeframes(pcm.tobytes())
    slots = {}
    manifest = {
        "seed": SEED,
        "rate": RATE,
        "channels": 1,
        "bits": 16,
        "license": "Original project-generated synthetic Foley; no third-party samples",
        "generation": "FFT filtered deterministic noise, deck/truck resonances, impact envelopes, periodic scrape/rolling and Hermite loop seam correction",
        "sources": {},
        "slots": {},
    }
    for name, pcm in sounds.items():
        manifest["sources"][name] = {
            "frames": len(pcm),
            "seconds": len(pcm) / RATE,
            "peak": float(np.max(np.abs(pcm.astype(float))) / 32767),
            "rms": float(np.sqrt(np.mean((pcm.astype(float) / 32767) ** 2))),
            "sha256": hashlib.sha256(pcm.tobytes()).hexdigest(),
            "loopBoundaryDelta": int(pcm[0]) - int(pcm[-1]) if "loop" in name else None,
        }
    for surface in SURFACES:
        effective = "concrete" if surface in ("generic", "stone") else surface
        for cue in CUES:
            if cue == "rolling":
                names = [f"roll_{effective}_loop.wav"]
            elif cue.startswith("land_"):
                names = [f"land_{effective}_{cue[5:]}.wav"]
            elif cue == "push":
                names = ["push_01.wav", "push_02.wav"]
            elif cue == "pop":
                names = ["pop_01.wav", "pop_02.wav", "pop_03.wav"]
            elif cue == "bail_board":
                names = ["bail_board_01.wav", "bail_board_02.wav"]
            elif cue == "bail_body":
                names = ["bail_body_soft.wav", "bail_body_hard.wav"]
            elif cue in ("pickup", "drop", "mount", "dismount"):
                names = ["board_" + cue + ".wav"]
            else:
                names = [cue.replace("_loop", "_loop") + ".wav"]
            for variant, name in enumerate(names):
                path = NS + surface + "/" + cue + ("" if variant == 0 else f"_{variant+1:02}") + ".pcm"
                slots[path] = sounds[name].tobytes()
                manifest["slots"][path] = name
    archive.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for path, data in sorted(slots.items()):
            info = zipfile.ZipInfo(path, (2020, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            z.writestr(info, data)
        info = zipfile.ZipInfo(NS + "Manifest.json", (2020, 1, 1, 0, 0, 0))
        info.compress_type = zipfile.ZIP_DEFLATED
        z.writestr(info, json.dumps(manifest, sort_keys=True, indent=2).encode())
    (out / "manifest.json").write_text(json.dumps(manifest, sort_keys=True, indent=2), encoding="utf-8")
    print(f"{len(sounds)} original WAVs, {len(slots)} PCM slots, {archive.stat().st_size} archive bytes, seed={SEED}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    root = Path(__file__).resolve().parents[1]
    parser.add_argument("--out", type=Path, default=root / "assets/SkateAudio")
    parser.add_argument("--archive", type=Path, default=root / "resources/skateharkinian-audio.o2r")
    a = parser.parse_args()
    generate(a.out, a.archive)

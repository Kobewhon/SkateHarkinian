"""Read-only prepared-data verifier. No downloads, uploads or retail hash lists."""

import argparse, json
from pathlib import Path, PurePosixPath

REQUIRED = (
    "private/game.json",
    "private/stock/physics-skeletons.json",
    "private/stock/skater-collections.json",
    "private/stock/data/config/input.cfg",
    "private/stock/data/anim/OnBoard.abin",
    "private/stock/data/anim/OffBoard.abin",
    "private/stock/data/script/camera/Default_cameragraph.stategraph",
)


def verify(root):
    root = Path(root).resolve()
    errors = []

    def file(name):
        p = PurePosixPath(name)
        if not name or p.is_absolute() or any(x in (".", "..") for x in p.parts) or ":" in name or "\\" in name:
            errors.append("Unsafe/non-relative manifest path: " + name)
            return
        target = (root / name).resolve()
        if not target.is_relative_to(root):
            errors.append("Manifest path escapes assets root")
            return
        if not target.is_file() or target.stat().st_size == 0:
            errors.append("Missing/empty: " + name)

    for name in REQUIRED:
        file(name)
    try:
        m = json.loads((root / "private/game.json").read_text(encoding="utf-8"))
        if not isinstance(m, dict):
            raise ValueError("game.json must be an object")
        if m.get("version") != 1:
            errors.append("Unsupported game.json version (expected 1)")
        for key, ext in (("character_scene", ".glb"), ("action_graph", ".stategraph"), ("motion_graph", ".stategraph")):
            value = m.get(key)
            if not isinstance(value, str) or not value.endswith(ext):
                errors.append("Invalid manifest field: " + key)
            else:
                file(value)
        if not isinstance(m.get("initial_animation"), str) or not m["initial_animation"].strip():
            errors.append("Missing initial_animation")
    except (OSError, ValueError, TypeError) as e:
        errors.append("Cannot parse game.json: " + str(e))
    return errors


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("assets", type=Path)
    a = p.parse_args()
    errors = verify(a.assets)
    for e in errors:
        print("ERROR:", e)
    print(
        "Prepared data: "
        + (
            "FAILED; copy the complete assets folder and check conversion.log"
            if errors
            else "key files present; binary compatibility still requires matching runtime"
        )
    )
    raise SystemExit(1 if errors else 0)

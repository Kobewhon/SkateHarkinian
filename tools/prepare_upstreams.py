"""Fetch exact public upstream pins and apply original integration; never fetch game data."""

from pathlib import Path
import json, subprocess, shutil

root = Path(__file__).resolve().parents[1]
pins = json.loads((root / "UPSTREAM-PINS.json").read_text())


def git(*args, cwd=None):
    subprocess.run(["git", *args], cwd=cwd, check=True)


for name, label, patch, overlay in [
    ("Shipwright", "shipwright", "shipwright.patch", "overlay"),
    ("NativeSkate fork", "native", "native-skate.patch", "native-overlay"),
]:
    target = root / "upstream" / label
    pin = pins[name]
    target.parent.mkdir(exist_ok=True)
    if not target.exists():
        git("clone", "--no-checkout", pin["remote"], str(target))
        git("checkout", "--detach", pin["commit"], cwd=target)
    current = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=target).decode().strip()
    if current != pin["commit"]:
        dirty = subprocess.check_output(["git", "status", "--porcelain"], cwd=target)
        if dirty:
            raise SystemExit("Refusing to replace modified upstream checkout: " + label)
        git("checkout", "--detach", pin["commit"], cwd=target)
    if label == "shipwright":
        git("submodule", "update", "--init", "--recursive", cwd=target)
        for sub, key in [("libultraship", "libultraship"), ("torch", "Torch")]:
            actual = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=target / sub).decode().strip()
            if actual != pins[key]["commit"]:
                raise SystemExit("Submodule pin mismatch: " + sub)
    path = root / "patches" / patch
    check = subprocess.run(["git", "apply", "--check", str(path)], cwd=target, capture_output=True)
    if check.returncode == 0:
        git("apply", str(path), cwd=target)
    elif (
        subprocess.run(["git", "apply", "--reverse", "--check", str(path)], cwd=target, capture_output=True).returncode
        != 0
    ):
        raise SystemExit("Patch conflicts in " + label + "; keep your work and inspect manually")
    for src in (root / overlay).rglob("*"):
        if src.is_file():
            dest = target / src.relative_to(root / overlay)
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dest)
print("Pinned upstreams prepared. No game data fetched; no game launched.")

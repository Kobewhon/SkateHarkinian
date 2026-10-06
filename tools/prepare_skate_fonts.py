"""Build the generic Skate UI archive from the bundled OFL font; no network IO."""

from pathlib import Path
import zipfile, hashlib, json

r = Path(__file__).resolve().parents[1]
font = r / "resources/fonts/Nunito-Sans/NunitoSans-VF.ttf"
notice = r / "LICENSES/Nunito-Sans-OFL.txt"
assert (
    hashlib.sha256(font.read_bytes()).hexdigest() == json.loads((font.parent / "PROVENANCE.json").read_text())["sha256"]
)
with zipfile.ZipFile(r / "resources/skateharkinian-fonts.o2r", "w", zipfile.ZIP_DEFLATED) as z:
    for path, data in [
        ("SkateHarkinian/Fonts/SkateUI.ttf", font.read_bytes()),
        ("LICENSES/Nunito-Sans-OFL.txt", notice.read_bytes()),
    ]:
        i = zipfile.ZipInfo(path, (2020, 1, 1, 0, 0, 0))
        i.compress_type = zipfile.ZIP_DEFLATED
        z.writestr(i, data)
print("Verified and built OFL Skate UI font archive")

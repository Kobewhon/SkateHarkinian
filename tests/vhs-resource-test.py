"""Validate the original cassette archive without loading retail game data."""

import hashlib
import importlib.util
import json
import struct
import tempfile
import zipfile
from pathlib import Path
from xml.etree import ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
ARCHIVE = ROOT / "resources/skateharkinian-vhs.o2r"
PREFIX = "SkateHarkinian/VHS/"


def validate():
    with zipfile.ZipFile(ARCHIVE) as archive:
        assert archive.testzip() is None
        names = archive.namelist()
        assert len(names) == len(set(names))
        forbidden = ("tape.obj", "tape.mtl", "3C40831F", "5DA3156")
        assert not any(value in name for value in forbidden for name in names)
        provenance = json.loads(archive.read(PREFIX + "Manifest.json"))
        assert provenance["description"] == "Original SkateHarkinian procedural VHS cassette and artwork."
        vertices = ET.fromstring(archive.read(PREFIX + "Mesh/Vertices"))
        points = [(int(v.attrib["X"]), int(v.attrib["Y"]), int(v.attrib["Z"])) for v in vertices]
        assert len(points) == 84
        for axis, (lower, upper) in enumerate(zip(*provenance["modelBounds"])):
            assert min(p[axis] for p in points) == lower
            assert max(p[axis] for p in points) == upper
        display_list = ET.fromstring(archive.read(PREFIX + "Mesh/DisplayList"))
        assert len(list(display_list.iter("Triangle1"))) == 28
        textures = []
        for name in names:
            if not name.endswith(".meta"):
                continue
            metadata = json.loads(archive.read(name).rstrip(b"\0"))
            assert metadata["type"] == "Texture" and metadata["format"] == "Binary"
            data = archive.read(name[:-5])
            kind, width, height, flags, scale_x, scale_y, size = struct.unpack("<IIIIffI", data[:28])
            assert kind == 1 and flags == 1 and scale_x == scale_y == 1
            assert size == width * height * 4 and len(data) == 28 + size
            if "/HUD/" in name:
                assert (width, height) == (32, 32)
                textures.append(data)
            else:
                assert (width, height) in ((256, 128), (64, 64))
        assert len(textures) == 10 and len(set(textures)) == 10
        for command in display_list.iter("LoadTextureBlock"):
            resource = command.attrib.get("Texture", command.attrib.get("Path", ""))
            assert resource in names, command.attrib
    spec = importlib.util.spec_from_file_location("vhs_generator", ROOT / "tools/generate_vhs.py")
    generator = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(generator)
    with tempfile.TemporaryDirectory() as temporary:
        directory = Path(temporary)
        rebuilt = directory / "vhs.o2r"
        generator.build(rebuilt, directory / "art", ROOT / "resources/fonts/Nunito-Sans/NunitoSans-VF.ttf")
        assert hashlib.sha256(rebuilt.read_bytes()).digest() == hashlib.sha256(ARCHIVE.read_bytes()).digest()
    print(
        "PASS original VHS: deterministic archive, 28 triangles, bounds, 12 RAW RGBA32 textures, 10 distinct HUD states"
    )


if __name__ == "__main__":
    validate()

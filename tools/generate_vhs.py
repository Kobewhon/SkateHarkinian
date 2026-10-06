"""Build an original procedural VHS cassette, artwork and ten health HUD states.

This generator has no imported model, texture or prior-archive input. The cassette
is an independently authored chamfered shell; its materials are drawn with Pillow.
Only resource interfaces and dimensions follow the existing presentation contract.
"""

import argparse
import hashlib
import json
import math
import random
import struct
import zipfile
from pathlib import Path
from xml.etree import ElementTree as ET

from PIL import Image, ImageDraw, ImageFilter, ImageFont

NAMESPACE = "SkateHarkinian/VHS"
STATES = ("Empty", "Quarter", "Half", "ThreeQuarter", "Full")
PROVENANCE = "Original SkateHarkinian procedural VHS cassette and artwork."


def texture_bytes(image):
    image = image.convert("RGBA")
    width, height = image.size
    pixels = image.tobytes()
    return struct.pack("<IIIIffI", 1, width, height, 1, 1.0, 1.0, len(pixels)) + pixels


def artwork(font_path):
    face = Image.new("RGBA", (256, 128), (34, 41, 46, 255))
    draw = ImageDraw.Draw(face)
    draw.rounded_rectangle((3, 3, 252, 124), radius=8, outline=(99, 111, 119), width=2)
    draw.rounded_rectangle((15, 19, 240, 91), radius=9, fill=(18, 23, 29))
    for center in (72, 183):
        draw.ellipse((center - 31, 29, center + 31, 81), fill=(124, 144, 151))
        draw.ellipse((center - 26, 33, center + 26, 77), fill=(30, 39, 46))
        draw.ellipse((center - 21, 37, center + 21, 73), outline=(183, 200, 194), width=3)
        for angle in range(0, 360, 60):
            radians = math.radians(angle)
            end = (center + 15 * math.cos(radians), 55 + 12 * math.sin(radians))
            draw.line((center, 55, *end), fill=(207, 215, 207), width=3)
        draw.ellipse((center - 5, 51, center + 5, 59), fill=(16, 24, 31))
    draw.rounded_rectangle((109, 33, 146, 79), radius=3, fill=(228, 214, 175))
    draw.line((112, 42, 143, 42), fill=(201, 90, 63), width=4)
    draw.line((115, 55, 140, 55), fill=(76, 91, 100), width=2)
    draw.line((115, 63, 133, 63), fill=(76, 91, 100), width=2)
    draw.rounded_rectangle((23, 98, 232, 117), radius=2, fill=(217, 225, 205))
    font = ImageFont.truetype(str(font_path), 12)
    draw.text((128, 99), "SKATEHARKINIAN", font=font, anchor="mt", fill=(30, 49, 58))
    for x, y in ((12, 12), (243, 12), (12, 115), (243, 115)):
        draw.ellipse((x - 2, y - 2, x + 2, y + 2), fill=(155, 166, 167))
        draw.line((x - 1, y, x + 1, y), fill=(31, 39, 43))

    body = Image.new("RGBA", (64, 64))
    pixels = body.load()
    random_source = random.Random(822061)
    for y in range(64):
        for x in range(64):
            shade = random_source.randrange(-3, 4)
            pixels[x, y] = (35 + shade, 43 + shade, 49 + shade, 255)
    ImageDraw.Draw(body).line((0, 8, 63, 8), fill=(56, 66, 71), width=1)
    return face, body


def geometry():
    # Independent design, centered pivot: +X right, +Y top, +Z labelled face.
    perimeter = [
        (-560, -360),
        (560, -360),
        (600, -320),
        (600, 320),
        (560, 360),
        (-560, 360),
        (-600, 320),
        (-600, -320),
    ]
    triangles = []
    front = [(x, y, 90) for x, y in perimeter]
    back = [(x, y, -90) for x, y in perimeter]
    for index in range(1, 7):
        triangles.append(("CassetteFace", [front[0], front[index], front[index + 1]]))
        triangles.append(("CassetteShell", [back[0], back[index + 1], back[index]]))
    for index in range(8):
        following = (index + 1) % 8
        quad = [front[index], back[index], back[following], front[following]]
        triangles.append(("CassetteShell", quad[:3]))
        triangles.append(("CassetteShell", [quad[0], quad[2], quad[3]]))
    return triangles


def combine_terms():
    # The second texture cycle passes COMBINED through, avoiding a second tile.
    terms = {}
    for cycle in (0, 1):
        source = "G_CCMUX_TEXEL0" if cycle == 0 else "G_CCMUX_0"
        shade = "G_CCMUX_SHADE" if cycle == 0 else "G_CCMUX_0"
        result = "G_CCMUX_0" if cycle == 0 else "G_CCMUX_COMBINED"
        alpha = "G_ACMUX_TEXEL0" if cycle == 0 else "G_ACMUX_COMBINED"
        for name, value in (
            ("A", source),
            ("B", "G_CCMUX_0"),
            ("C", shade),
            ("D", result),
            ("Aa", "G_ACMUX_0"),
            ("Ab", "G_ACMUX_0"),
            ("Ac", "G_ACMUX_0"),
            ("Ad", alpha),
        ):
            terms[f"{name}{cycle}"] = value
    return terms


def mesh_resources(images):
    vertices = ET.Element("Vertex", Version="0")
    display_list = ET.Element("DisplayList", Version="0")
    ET.SubElement(display_list, "PipeSync")
    ET.SubElement(display_list, "ClearGeometryMode", G_CULL_FRONT="1", G_TEXTURE_GEN="1", G_TEXTURE_GEN_LINEAR="1")
    ET.SubElement(display_list, "SetGeometryMode", G_LIGHTING="1", G_CULL_BACK="1", G_ZBUFFER="1", G_SHADING_SMOOTH="1")
    triangles = geometry()
    vertex_offset = 0
    for material, image in images.items():
        faces = [points for name, points in triangles if name == material]
        width, height = image.size
        ET.SubElement(display_list, "PipeSync")
        ET.SubElement(display_list, "SetCycleType", G_CYC_2CYCLE="1")
        ET.SubElement(display_list, "SetRenderMode", Mode1="G_RM_FOG_SHADE_A", Mode2="G_RM_AA_ZB_OPA_SURF2")
        ET.SubElement(display_list, "SetTextureLUT", Mode="G_TT_NONE")
        ET.SubElement(display_list, "Texture", S="65535", T="65535", Level="0", Tile="0", On="1")
        ET.SubElement(display_list, "SetCombineLERP", **combine_terms())
        ET.SubElement(
            display_list,
            "LoadTextureBlock",
            Path=f"{NAMESPACE}/Textures/{material}",
            Format="0",
            Size="3",
            Width=str(width),
            Height=str(height),
            MaskS="0",
            MaskT="0",
            ShiftS="0",
            ShiftT="0",
            CMS_TXClamp="1",
            CMT_TXClamp="1",
        )
        for start in range(0, len(faces), 10):
            batch = faces[start : start + 10]
            ET.SubElement(
                display_list,
                "LoadVertices",
                Path=f"{NAMESPACE}/Mesh/Vertices",
                Count=str(3 * len(batch)),
                VertexBufferIndex="0",
                VertexOffset=str(vertex_offset),
            )
            for index, points in enumerate(batch):
                first = [points[1][axis] - points[0][axis] for axis in range(3)]
                second = [points[2][axis] - points[0][axis] for axis in range(3)]
                normal = [
                    first[1] * second[2] - first[2] * second[1],
                    first[2] * second[0] - first[0] * second[2],
                    first[0] * second[1] - first[1] * second[0],
                ]
                length = math.sqrt(sum(value * value for value in normal))
                assert length > 0
                normal = [round(value / length * 127) & 255 for value in normal]
                for x, y, z in points:
                    attributes = dict(
                        X=str(x),
                        Y=str(y),
                        Z=str(z),
                        S=str(round((x + 600) / 1200 * (width - 1) * 32)),
                        T=str(round((360 - y) / 720 * (height - 1) * 32)),
                        R=str(normal[0]),
                        G=str(normal[1]),
                        B=str(normal[2]),
                        A="255",
                    )
                    ET.SubElement(vertices, "Vtx", **attributes)
                ET.SubElement(
                    display_list,
                    "Triangle1",
                    V00=str(3 * index),
                    V01=str(3 * index + 1),
                    V02=str(3 * index + 2),
                    Flag0="0",
                )
            vertex_offset += len(batch) * 3
    ET.SubElement(display_list, "Texture", S="0", T="0", Level="0", Tile="0", On="0")
    ET.SubElement(display_list, "EndDisplayList")
    return ET.tostring(vertices), ET.tostring(display_list)


def build(output, artwork_root, font_path):
    face, shell = artwork(font_path)
    images = {"CassetteFace": face, "CassetteShell": shell}
    files = {}
    metadata = json.dumps(dict(format="Binary", type="Texture", version=1, isCustom=True)).encode() + b"\0"
    for name, image in images.items():
        path = f"{NAMESPACE}/Textures/{name}"
        files[path] = texture_bytes(image)
        files[path + ".meta"] = metadata
    files[f"{NAMESPACE}/Mesh/Vertices"], files[f"{NAMESPACE}/Mesh/DisplayList"] = mesh_resources(images)

    base_icon = Image.new("RGBA", (32, 32))
    base_icon.alpha_composite(face.resize((28, 16), Image.Resampling.LANCZOS), (2, 8))
    sheet = Image.new("RGBA", (320, 144), (21, 28, 35, 255))
    for defense in (False, True):
        for quarters, state in enumerate(STATES):
            icon = base_icon.copy()
            gain = 0.32 + 0.17 * quarters
            red, green, blue, alpha = icon.split()
            icon = Image.merge(
                "RGBA",
                tuple(channel.point(lambda value: round(value * gain)) for channel in (red, green, blue)) + (alpha,),
            )
            draw = ImageDraw.Draw(icon)
            for index in range(4):
                color = (236, 226, 186, 255) if index < quarters else (46, 54, 60, 255)
                draw.rectangle((5 + index * 6, 21, 8 + index * 6, 23), fill=color)
            if defense:
                outline = icon.getchannel("A").filter(ImageFilter.MaxFilter(3))
                layer = Image.new("RGBA", (32, 32), (188, 220, 239, 0))
                layer.putalpha(outline)
                layer.alpha_composite(icon)
                icon = layer
            group = "Defense" if defense else "Normal"
            path = f"{NAMESPACE}/HUD/{group}/{state}"
            files[path] = texture_bytes(icon)
            files[path + ".meta"] = metadata
            sheet.alpha_composite(icon.resize((64, 64), Image.Resampling.NEAREST), (quarters * 64, int(defense) * 72))
    hud_vertices = ET.Element("Vertex", Version="0")
    for x, y, s, t in ((-8, 8, 0, 0), (8, 8, 1024, 0), (-8, -8, 0, 1024), (8, -8, 1024, 1024)):
        ET.SubElement(
            hud_vertices, "Vtx", X=str(x), Y=str(y), Z="0", S=str(s), T=str(t), R="255", G="255", B="255", A="255"
        )
    files[f"{NAMESPACE}/HUD/Vertices"] = ET.tostring(hud_vertices)
    provenance = dict(
        description=PROVENANCE,
        creator="Kobewhon / SkateHarkinian",
        license="MIT (original geometry/artwork only)",
        geometry="independent chamfered shell; 28 triangles; no imported asset input",
        modelBounds=[[-600, -360, -90], [600, 360, 90]],
        pickupScale=0.01,
        hudDimensions=[32, 32],
        states=STATES,
        font="Nunito Sans OFL; bundled font itself retains OFL",
        materials={name: list(image.size) for name, image in images.items()},
    )
    files[f"{NAMESPACE}/Manifest.json"] = json.dumps(provenance, indent=2).encode("utf-8")
    files[f"{NAMESPACE}/HUD/Manifest.json"] = json.dumps(
        dict(
            provenance=PROVENANCE,
            states=STATES,
            dimensions=[32, 32],
            defense="light-blue outline",
            partial="four pips and brightness; health selection unchanged",
        ),
        indent=2,
    ).encode("utf-8")
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED) as archive:
        for name, data in sorted(files.items()):
            entry = zipfile.ZipInfo(name, (2026, 10, 6, 0, 0, 0))
            entry.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(entry, data)
    artwork_root.mkdir(parents=True, exist_ok=True)
    face.save(artwork_root / "CassetteFace.png")
    shell.save(artwork_root / "CassetteShell.png")
    sheet.save(artwork_root / "HUD-States.png")
    provenance["archiveSha256"] = hashlib.sha256(output.read_bytes()).hexdigest()
    (artwork_root / "PROVENANCE.json").write_text(json.dumps(provenance, indent=2), encoding="utf-8")
    print(PROVENANCE, "Archive:", output)


if __name__ == "__main__":
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, default=root / "resources/skateharkinian-vhs.o2r")
    parser.add_argument("--artwork", type=Path, default=root / "resources/vhs-original")
    parser.add_argument("--font", type=Path, default=root / "resources/fonts/Nunito-Sans/NunitoSans-VF.ttf")
    arguments = parser.parse_args()
    build(arguments.archive, arguments.artwork, arguments.font)

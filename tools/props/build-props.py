import json, math, struct, zipfile, random
from pathlib import Path
from PIL import Image, ImageDraw
from xml.etree import ElementTree as E
import argparse

root = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument("--meshes", type=Path, required=True)
p.add_argument("--archive", type=Path, default=root / "resources/skateharkinian-props.o2r")
p.add_argument("--previews", type=Path, default=root / "build/prop-materials")
a = p.parse_args()
a.archive.parent.mkdir(parents=True, exist_ok=True)
a.previews.mkdir(parents=True, exist_ok=True)
meshes = json.loads(a.meshes.read_text())
files = {}
mats = ["WornSteel", "PaintedSteel", "Plywood", "Concrete", "CopingSteel"]
rng = random.Random(810)
for mat in mats:
    im = Image.new("RGBA", (128, 128))
    pix = im.load()
    for y in range(128):
        for x in range(128):
            base = {
                "WornSteel": (137, 145, 148),
                "PaintedSteel": (38, 45, 51),
                "Plywood": (146, 110, 65),
                "Concrete": (142, 143, 139),
                "CopingSteel": (111, 121, 128),
            }[mat]
            noise = rng.randrange(-7, 8)
            grain = (
                5 * math.sin(y * 0.43 + 2 * math.sin(x * 0.049))
                if mat == "Plywood"
                else 4 * math.sin(y * 0.87) if "Steel" in mat else 0
            )
            pix[x, y] = tuple(max(0, min(255, int(c + noise + grain))) for c in base) + (255,)
    # Seeded restrained scratches/scuffs; tile is periodic at edges.
    draw = ImageDraw.Draw(im)
    for i in range(24):
        x = rng.randrange(128)
        y = rng.randrange(128)
        length = rng.randrange(5, 24)
        if "Steel" in mat:
            draw.line(
                (x, y, min(127, x + length), y),
                fill=(165, 174, 178, 255) if mat != "PaintedSteel" else (67, 72, 75, 255),
            )
        elif mat == "Plywood":
            draw.line((x, y, min(127, x + length), y), fill=(126, 93, 57, 255))
    path = "SkateHarkinian/Props/Materials/" + mat
    files[path] = struct.pack("<IIIIffI", 1, 128, 128, 1, 1.0, 1.0, 65536) + im.tobytes()
    files[path + ".meta"] = (
        json.dumps({"format": "Binary", "type": "Texture", "version": 1, "isCustom": True}).encode() + b"\0"
    )
    im.save(a.previews / (mat + ".png"))
attrs = {}
for cycle in (0, 1):
    terms = (
        [
            ("A", "G_CCMUX_TEXEL0"),
            ("B", "G_CCMUX_0"),
            ("C", "G_CCMUX_SHADE"),
            ("D", "G_CCMUX_0"),
            ("Aa", "G_ACMUX_0"),
            ("Ab", "G_ACMUX_0"),
            ("Ac", "G_ACMUX_0"),
            ("Ad", "G_ACMUX_TEXEL0"),
        ]
        if cycle == 0
        else [
            ("A", "G_CCMUX_0"),
            ("B", "G_CCMUX_0"),
            ("C", "G_CCMUX_0"),
            ("D", "G_CCMUX_COMBINED"),
            ("Aa", "G_ACMUX_0"),
            ("Ab", "G_ACMUX_0"),
            ("Ac", "G_ACMUX_0"),
            ("Ad", "G_ACMUX_COMBINED"),
        ]
    )
    for k, v in terms:
        attrs[k + str(cycle)] = v
manifest = []
for mesh in meshes[3:]:
    path = mesh["resource"].removesuffix("/DisplayList")
    verts = E.Element("Vertex", Version="0")
    dl = E.Element("DisplayList", Version="0")
    E.SubElement(dl, "PipeSync")
    E.SubElement(dl, "ClearGeometryMode", G_CULL_FRONT="1", G_TEXTURE_GEN="1", G_TEXTURE_GEN_LINEAR="1")
    E.SubElement(dl, "SetGeometryMode", G_LIGHTING="1", G_CULL_BACK="1", G_ZBUFFER="1", G_SHADING_SMOOTH="1")
    groups = {}
    for tri in mesh["triangles"]:
        p = [mesh["vertices"][i] for i in tri]
        a = [p[1][i] - p[0][i] for i in range(3)]
        b = [p[2][i] - p[0][i] for i in range(3)]
        n = [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]
        norm = math.sqrt(sum(t * t for t in n))
        assert norm > 0, mesh["key"]
        n = [v / norm for v in n]
        mat = (
            ("WornSteel" if sum(v[1] for v in p) / 3 > 12 else "PaintedSteel")
            if mesh["grinds"]
            else ("Concrete" if mesh["surface"] == 2 else "Plywood") if n[1] > 0.15 else "PaintedSteel"
        )
        # dominant-axis projection; 64 world-unit tile, grain aligned with +Z length.
        axis = max(range(3), key=lambda k: abs(n[k]))
        uvaxes = (2, 1) if axis == 0 else (2, 0) if axis == 1 else (0, 1)
        groups.setdefault(mat, []).append((p, n, uvaxes))
    offset = 0
    for mat, faces in groups.items():
        E.SubElement(dl, "PipeSync")
        E.SubElement(dl, "SetCycleType", G_CYC_2CYCLE="1")
        E.SubElement(dl, "SetRenderMode", Mode1="G_RM_FOG_SHADE_A", Mode2="G_RM_AA_ZB_OPA_SURF2")
        E.SubElement(dl, "SetTextureLUT", Mode="G_TT_NONE")
        E.SubElement(dl, "Texture", S="65535", T="65535", Level="0", Tile="0", On="1")
        E.SubElement(dl, "SetCombineLERP", **attrs)
        E.SubElement(
            dl,
            "LoadTextureBlock",
            Path="SkateHarkinian/Props/Materials/" + mat,
            Format="0",
            Size="3",
            Width="128",
            Height="128",
            MaskS="7",
            MaskT="7",
            ShiftS="0",
            ShiftT="0",
            CMS_TXWrap="1",
            CMT_TXWrap="1",
        )
        for start in range(0, len(faces), 10):
            batch = faces[start : start + 10]
            E.SubElement(
                dl,
                "LoadVertices",
                Path=path + "/Vertices",
                Count=str(len(batch) * 3),
                VertexBufferIndex="0",
                VertexOffset=str(offset),
            )
            for j, (points, n, uvaxes) in enumerate(batch):
                for p in points:
                    v = dict(zip(("X", "Y", "Z"), [str(round(x)) for x in p]))
                    v.update(
                        S=str(round(p[uvaxes[0]] * 64)),
                        T=str(round(p[uvaxes[1]] * 64)),
                        R=str(round(n[0] * 127) & 255),
                        G=str(round(n[1] * 127) & 255),
                        B=str(round(n[2] * 127) & 255),
                        A="255",
                    )
                    E.SubElement(verts, "Vtx", **v)
                E.SubElement(dl, "Triangle1", V00=str(j * 3), V01=str(j * 3 + 1), V02=str(j * 3 + 2), Flag0="0")
                offset += 3
    E.SubElement(dl, "Texture", S="0", T="0", Level="0", Tile="0", On="0")
    E.SubElement(dl, "EndDisplayList")
    files[path + "/Vertices"] = E.tostring(verts)
    files[path + "/DisplayList"] = E.tostring(dl)
    manifest.append(
        {
            **mesh,
            "materials": list(groups),
            "uv": "dominant projection; 64 world units per repeat; 5-bit fractional texels",
            "pivot": "bottom center, +Z forward",
        }
    )
files["SkateHarkinian/Props/Manifest.json"] = json.dumps(manifest, indent=2).encode()
with zipfile.ZipFile(a.archive, "w", zipfile.ZIP_DEFLATED) as z:
    for p, data in files.items():
        z.writestr(p, data)
print(f"{len(manifest)} prop definitions, 5 shared 128x128 RGBA32 materials; 327680 texture bytes")

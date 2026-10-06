"""Synthetic files only; no retail assets required."""

import sys, json, tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from verify_skate_data import verify, REQUIRED

with tempfile.TemporaryDirectory() as t:
    r = Path(t)
    assert verify(r)
    for name in REQUIRED:
        p = r / name
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(b"test")
    m = dict(
        version=1,
        character_scene="private/skater.glb",
        action_graph="private/action.stategraph",
        motion_graph="private/motion.stategraph",
        initial_animation="test",
    )
    for key in ("character_scene", "action_graph", "motion_graph"):
        (r / m[key]).write_bytes(b"synthetic")

    def write(v):
        (r / "private/game.json").write_text(json.dumps(v))

    write(m)
    assert not verify(r)
    for value in ("../escape.glb", "/outside.glb", "C:/outside.glb", "private\\outside.glb"):
        n = dict(m, character_scene=value)
        write(n)
        assert verify(r)
    write(dict(m, version=2))
    assert verify(r)
    write([])
    assert verify(r)
    write(m)
    (r / m["character_scene"]).write_bytes(b"")
    assert verify(r)
    print(
        "PASS missing/empty,valid,unsupported version,non-object,traversal/absolute/backslash paths; verifier is read-only"
    )

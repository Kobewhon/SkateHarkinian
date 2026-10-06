"""Compare status exports against a supplied golden DLL without launching SoH."""

import ctypes as c, argparse


class Packet(c.Structure):
    _fields_ = [("buttons", c.c_uint16), ("triggers", c.c_uint8 * 2), ("left", c.c_int16 * 2), ("right", c.c_int16 * 2)]


class Snapshot(c.Structure):
    _fields_ = [
        ("root", c.c_float * 16),
        ("board", c.c_float * 16),
        ("state_id", c.c_uint32),
        ("velocity", c.c_float * 3),
        ("tick", c.c_uint64),
        ("state", c.c_char * 64),
        ("core_intents", c.c_float * 7),
        ("body_pitch", c.c_float),
        ("body_yaw", c.c_float),
        ("trajectory", c.c_float * 3),
        ("trajectory_velocity", c.c_float * 3),
        ("ground_normal", c.c_float * 3),
        ("heading", c.c_float),
    ]


def load(path, assets):
    d = c.CDLL(str(path))
    d.sh_skate_create.argtypes = [c.c_char_p, c.POINTER(c.c_float), c.c_uint32, c.POINTER(c.c_float), c.c_float]
    d.sh_skate_create.restype = c.c_void_p
    d.sh_skate_step_samples.argtypes = [c.c_void_p, c.POINTER(Packet), c.c_uint32, c.POINTER(Snapshot)]
    d.sh_skate_error.restype = c.c_char_p
    d.sh_skate_destroy_checked.argtypes = [c.c_void_p]
    tri = (c.c_float * 18)(-500, 0, -500, 500, 0, 500, 500, 0, -500, -500, 0, -500, -500, 0, 500, 500, 0, 500)
    p = (c.c_float * 3)(0, 0, 0)
    h = d.sh_skate_create(str(assets).encode(), tri, 2, p, 0)
    assert h, d.sh_skate_error()
    return d, h


if __name__ == "__main__":
    from pathlib import Path

    a = argparse.ArgumentParser()
    a.add_argument("golden", type=Path)
    a.add_argument("candidate", type=Path)
    a.add_argument("assets", type=Path)
    v = a.parse_args()
    sessions = [load(x.resolve(), v.assets.resolve()) for x in [v.golden, v.candidate]]
    try:
        for tick in range(2000):
            p = Packet()
            phase = tick % 400
            if phase < 80:
                p.buttons = 0x1000
            elif phase < 120:
                p.left[0] = int((phase - 80) * 180)
            elif phase < 150:
                p.right[1] = -18000
            elif phase < 160:
                p.right[1] = 24000
            elif phase < 200:
                p.triggers[0] = 255
            elif phase == 220:
                p.buttons = 0x8000
            elif phase == 300:
                p.buttons = 0x4000
            out = []
            for d, h in sessions:
                snap = Snapshot()
                assert d.sh_skate_step_samples(h, c.byref(p), 1, c.byref(snap)), d.sh_skate_error()
                out.append(snap)
            for name, _ in Snapshot._fields_:
                left, right = getattr(out[0], name), getattr(out[1], name)
                if isinstance(left, c.Array):
                    left, right = list(left), list(right)
                assert left == right, (tick, name, left, right)
        print(
            "PASS 2000 matched golden/candidate status snapshots; complete fields, controller sequence, no SoH launch"
        )
    finally:
        for d, h in sessions:
            assert d.sh_skate_destroy_checked(h) == 1

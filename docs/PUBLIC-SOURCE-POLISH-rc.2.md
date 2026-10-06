# 8.2C-rc.2 source and asset polish

Build identity: `2026-10-06-ff0209e76-8.2C-rc.2`.

The human-verified rc.1 source, runtime and artifacts were checkpointed before editing. This candidate performs mechanical formatting, encoding repair and an original VHS visual replacement. Gameplay tuning, controls, tricks, LINE scoring and the human-accepted real-shield boards are preserved.

## Readability and encoding

Owned C++/headers/tests use the surrounding four-space, attached-brace style with a 120-column target. Owned Rust uses rustfmt with a 120-column target; Python uses Black. PowerShell regression and install-verification scripts use ordinary blocks. Native upstream formatting is confined to functions already changed by the integration; unrelated files are untouched. Host upstream formatting is restricted to integration ranges.

The C++ comparison preserves 79 existing owned token streams. The two intentional exceptions are the build identity and the cassette texture resource/dimension validator. Native formatting retains identifiers, literals and operators; Rust formatting may change redundant braces/trailing commas. No architecture or gameplay tuning rewrite was performed. Long remaining code lines are ten indivisible diagnostic/result string literals, not compressed implementation bodies.

Patches are generated from Git's raw UTF-8 bytes and checked against their exact pins. Unrelated Jalapeño/Dampé encoding changes do not appear in the Shipwright patch. Text uses UTF-8; installer scripts remain ASCII-compatible with Windows PowerShell 5.1. The original VHS integration files are now included in the public overlay, fixing the earlier export omission.

## Original cassette

**Original SkateHarkinian procedural VHS cassette and artwork.**

`tools/generate_vhs.py` independently authors a chamfered low-poly shell, reel windows, label, screws, plastic grain and normal/Double Defense HUD artwork with Pillow. It has no imported model, texture or old-archive input. Original artwork and provenance are in `resources/vhs-original/`; output is `resources/skateharkinian-vhs.o2r`.

The mesh contains 28 triangles and 84 vertex entries, bounds `[-600,-360,-90]` to `[600,360,90]`, centered pivot and existing 0.01 pickup draw scale. Face material is 256×128 and shell material 64×64, RAW RGBA32. Ten HUD state textures remain 32×32. Existing Mesh/HUD namespace interfaces are retained. The pickup loader validates the new resource names and dimensions. Health selection, collision, pickup logic and gameplay are unchanged.

Old and new model/material payload hashes are disjoint. The procedural generation is deterministic. Existing archive XML/resource interfaces are reused; retail-derived geometry/artwork is not. Final pickup and HUD visual acceptance requires a human check.

## Rights and publication

The owner selected MIT for SkateHarkinian-original code/assets, Copyright (c) 2026 Kobewhon. This license question is **RESOLVED**. Root LICENSE does not relicense upstream code, upstream-derived patch context, libraries, Nunito Sans, game resources or retail data.

The SK8-ENGINE pin `cb7968930f14dad38457e98720d1a274e469eec2` licensing/provenance question remains **OPEN**. Current main's GPLv3 does not establish this older revision's rights. See LICENSE-REVIEW.md before redistribution. Nunito Sans retains OFL; Pillow tooling retains its supplied notices. Actual OoT shield model references remain runtime references, without extracted shield assets in the export.

No retail game data, personal saves/configs, logs, build outputs, checkpoints or restricted font binary belongs in either public artifact. No game launch or publication is performed by this workflow.

# SkateHarkinian 8.2C public playtest release candidate

Build: `2026-10-06-ff0209e76-8.2C-rc.2`. Recommended tag: `v8.2C-rc.2`. Local publication-review handoff; not yet published.

The 8.2C gameplay baseline passed human testing. This candidate includes the developments since 8.1F and conservative release hardening.

## Highlights

- Expanded original skatepark ramp/rail library, authored grindable edges, tree trunk collision and improved ramp contact.
- Explicit vert transitions and coping handplants. Mini-ramp softlock fixed; repeated wall-to-wall regression coverage retained.
- Visible empty-hand and board-carry off-board jumps with stock Link jump voice. Improved board handling and supported prop push/carry.
- Default board plus actual Deku, Hylian and Mirror Shield decks with trucks and wheels. These are visual choices with identical board gameplay physics.
- No Bailing, Ollie Height, Board Speed, Push Acceleration and Air Control modifiers; optional one-shot Legacy FS 360 Pop Glitch. Defaults preserve normal tuning.
- Monotonic active LINE score and an authoritative 2.50-second link opportunity. Continuous scoring actions hold the LINE alive.
- Safer water recovery history and shoreline clearance; cleaner scene/grotto lifecycle and current-camera reacquisition.
- Consistent compact Skate Mode prompts, Skate Options, Object Dropper and score typography. The public candidate bundles OFL Nunito Sans; no Windows font installation is needed.

## Hardening

Read-only native snapshots avoid cloning the complete animation pose. Disabled developer performance scopes avoid clock/metric work. Temporary startup traces removed; recovery logging and regression tests preserved. The accepted shield renderer and gameplay systems remain unchanged. The handoff report records measured microbenchmarks, not FPS claims.

## Upgrade from 8.1F

Extract into a new folder and keep 8.1F for rollback. Supply your own OoT archive and locally prepared Skate 3 data; neither is in this download. Back up saves, start with fresh settings, and reapply bindings deliberately. Do not mix host/DLL/resource versions or copy the old font archive. Install the official x64 Visual C++ redistributable if required.

Read INSTALLATION.md and docs/SKATE3-DATA-SETUP.md, then run VERIFY-INSTALL.ps1. The verifier does not launch the game.

## Known limitations and review

Three synthetic Bank-to-Ledge lip-pop cases also fail on the golden baseline; they are not silently marked fixed. This is an early playtest. The new font requires a final human readability/layout check at 1080p, 1440p and 4K. The pinned upstream engine source/binary rights still require review; the original-work MIT decision is resolved. Upstream rights must be confirmed before either artifact is uploaded; see docs/LICENSE-REVIEW.md.

## Source and reports

The companion source archive contains integration, runtime wrapper, original resources, regression tests, patches and exact upstream pins. It excludes upstream game decompilation, private assets and build output. BUILDING.md explains reconstruction.

Use BUG-REPORT.md with build ID, scene/room, Link age, controller, cheats, selected board/prop and exact steps. Do not upload game data, personal saves or unredacted private logs. Development is AI-assisted; human gameplay testing remains the acceptance gate.

## rc.2 publication polish

Mechanical C++/Rust/tool readability formatting and UTF-8 repair; gameplay tuning and controls are unchanged from human-verified rc.1. VHS visuals are replaced with original procedural SkateHarkinian cassette geometry and artwork. Health/pickup mechanics are unchanged; human visual approval of the new cassette/HUD remains required.

The owner selected MIT for SkateHarkinian-original code/assets (Copyright (c) 2026 Kobewhon). Upstream and third-party work retain their own status. The exact SK8-ENGINE pin licensing question remains open; see docs/LICENSE-REVIEW.md.

# SkateHarkinian 8.2C — Public Playtest Release Candidate 2

Build: `2026-10-06-ff0209e76-8.2C-rc.2`  
Tag: `v8.2C-rc.2`

8.2C RC2 is the human-tested publication candidate following the 8.1F public playtest. It combines the new gameplay work from 8.2A–8.2C with conservative source/release hardening.

## What's new since 8.1F

### Skating and world interaction

- Expanded skatepark ramp/rail library and authored grindable edges on eligible props.
- Explicit vert transitions and coping handplants.
- Improved ramp contact/momentum behavior.
- Fixed the mini-ramp stale-contact/input softlock.
- Added tree-trunk collision without canopy blockers.
- Improved supported movable-prop carry/push ownership and cleanup.
- Preserved dynamic grind-provider capture/alignment/deletion behavior.

### Link and board presentation

- Visible off-board jumps now use stock Link jump animation as the base.
- Empty-hand and board-carry jumps have separate balance/landing treatment and meaningful limb motion.
- Stock Link jump voice plays once on takeoff.
- Adult/Child support and accepted carry transforms remain intact.

### Board Appearance

Four choices are available:

- Default Skateboard
- Deku Shield
- Hylian Shield
- Mirror Shield

The shield options use actual OoT shield model resources as cosmetic deck geometry, with SkateHarkinian trucks and four wheels. All four appearances use the same gameplay physics/collision/trick implementation.

### Gameplay modifiers

- No Bailing
- Ollie Height 0.5×–3×
- Board Speed 0.5×–3×
- Push Acceleration 0.5×–3×
- Air Control 0×–2×
- Legacy FS 360 Pop Glitch
- Reset defaults

Legacy FS360 recreates the requested FS360 → fresh airborne trigger → near-landing Triangle/Y → one-shot super-pop sequence while preserving rider/board ownership and adding no score bonus.

### LINE scoring

- Monotonic active LINE score.
- Authoritative 2.50-second real-time link opportunity.
- Continuous scoring actions hold the LINE alive.
- One-time banking.
- Frame-rate-independent behavior validated at 60/120/144/240 FPS.
- Deterministic F8/F9/scene handling.

### Recovery and lifecycle

- Safer water recovery with bounded dry-history sampling, clearance/stability checks, hysteresis and repeat-loop avoidance.
- Scene/grotto lifecycle cleanup and current-camera reacquisition.
- F8 still exits at the rider's current position.
- Session Marker remains explicit marker/camera state.
- Water recovery remains water-specific.
- F9 remains the canonical emergency-recovery seed.

### Object Dropper and Session Marker

- Camera-relative Object Dropper motion.
- Free elevated preview placement.
- Trigger/right-stick rotation with immediate camera-stick return.
- Dynamic Dropper rails participate in grind capture and cleanup.
- Session Marker stores semantic orbit state relative to the skater and remains same-scene only.

### UI and VHS health presentation

- Compact Skate Mode prompts, Skate Options, Object Dropper and score typography use bundled OFL Nunito Sans.
- FOT-Chiaro is not distributed.
- Original SkateHarkinian procedural VHS cassette geometry/art replaces the earlier imported cassette resource.
- Normal/Double Defense VHS HUD states and world pickup visuals passed final human testing.

### Release hardening

- Public readability pass over SkateHarkinian-owned C++/Rust/tools.
- UTF-8/mojibake repair.
- Cleaner upstream patch generation.
- Temporary startup timing traces removed.
- Lightweight read-only native status extraction avoids unnecessary full-pose copies.
- Disabled profiling scopes avoid timing/metric work unless profiling is enabled.
- Extensive portable regressions retained for score, mini-ramp, FS360, authored edges, trees, water, lifecycle/camera, jump, shields, Object Dropper, font and VHS resources.
- Added BUILDING, ARCHITECTURE, Skate 3 data setup, verifier, manifests and third-party notice documentation.
- SkateHarkinian-original work is MIT licensed.

## Upgrade from 8.1F

Extract RC2 into a **new folder** and keep 8.1F for rollback. Supply your own OoT archive and locally prepared Skate 3 data; neither is included. Back up saves, start with fresh settings and reapply bindings deliberately. Do not mix host/DLL/resource versions or copy the old font archive.

Read `INSTALLATION.md` and `docs/SKATE3-DATA-SETUP.md`, then run `VERIFY-INSTALL.ps1`. The verifier does not launch the game.

## Known limitation

Three synthetic Bank-to-Ledge 10 m/s lip-pop cases remain known failures at offsets 0.8, 0.1 and -0.1. They also fail on the accepted golden baseline. No broad grace workaround was added to hide them.

## Source and legal notes

The source repository contains SkateHarkinian integration/original code, original resources, regression tests, patches and exact upstream pins. It excludes private retail assets, upstream game decompilation and build output.

SkateHarkinian-original code/assets are MIT licensed. Upstream and third-party components retain their own licensing/status. The exact pinned SK8-ENGINE provenance/licensing question remains documented in `docs/LICENSE-REVIEW.md`.

Use `BUG-REPORT.md` with the build ID, scene/room, Link age, controller, cheats, selected board/prop and exact steps. Never upload game data, personal saves or unredacted private logs.

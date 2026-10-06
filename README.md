# SkateHarkinian 8.2C RC2

Build: `2026-10-06-ff0209e76-8.2C-rc.2`  
Recommended tag: `v8.2C-rc.2`

SkateHarkinian brings Skate-inspired riding, tricks, grinding, vert, spot-building and line scoring into Ocarina of Time through a custom Ship of Harkinian host and NativeSkate runtime.

**Windows x64 · early public playtest · source available**

This is not an official Nintendo, Electronic Arts or Harbour Masters release. Both Ocarina of Time and Skate 3 game data must be supplied by the user from legally obtained copies.

The 8.2C RC2 gameplay and final VHS/font presentation have passed human testing. RC2 also passed the release-hardening regression suite against the accepted 8.2C baseline.

## What's new in 8.2C since public 8.1F

### Skating, ramps and world interaction

- Expanded the original ramp/rail library with more skatepark pieces and authored grindable edges on eligible props.
- Added explicit vert transitions and coping handplants.
- Improved ramp contact, momentum handling and transition behavior without restoring the old oscillating ramp-grace experiment.
- Fixed the mini-ramp stale-contact/input softlock that could previously leave controls unresponsive.
- Added solid tree-trunk collision providers while keeping canopy space clear.
- Improved supported off-board prop manipulation, including movable prop ownership, carrying/pushing and cleanup.
- Preserved dynamic grind-provider cleanup across duplicate/delete/room/scene transitions.

### Link movement and board handling

- Added visible off-board jumping using stock Link jump animation as the rendered base.
- Added separate empty-hand and board-carry jump presentation with meaningful arm/leg participation.
- Added the stock Link jump voice on the takeoff edge.
- Preserved Adult/Child Link handling and board-carry transforms.
- Retained the historical FS 360 Pop Shuvit → grab → dismount crash/wedge fix.

### Board Appearance

Board Appearance now provides exactly four choices:

- **Default Skateboard**
- **Deku Shield**
- **Hylian Shield**
- **Mirror Shield**

The three shield choices use the actual OoT shield display-list resources from the user's prepared OoT data as the visible deck, with SkateHarkinian trucks and four wheels underneath. The old texture-based themed-deck experiment was retired.

All four choices share **one authoritative gameplay skateboard**. Board appearance does not change collision, dimensions, center of mass, pop, grinding, manuals, vert, handplants, tricks, score, mounting, carrying or recovery.

### Cheats / gameplay modifiers

Enhancements → SkateHarkinian includes:

- **No Bailing**
- **Ollie Height** — 0.5× to 3×, default 1×
- **Board Speed** — 0.5× to 3×, default 1×
- **Push Acceleration** — 0.5× to 3×, default 1×
- **Air Control** — 0× to 2×, default 1×
- **Legacy FS 360 Pop Glitch**
- Reset-to-default support

The Legacy FS360 option recreates the requested one-shot sequence: valid FS360 → fresh airborne trigger/grab → fresh Triangle/Y shortly before landing → extreme second pop. It does not add score and does not intentionally dismount the rider.

### LINE / scoring

- Active LINE score is monotonic while a line is alive.
- The authoritative trick-link opportunity is **2.50 seconds** in real time and is frame-rate independent.
- Eligible continuous scoring states such as grinds, manuals, grabs and handplants keep the line alive while active.
- Each completed line banks once.
- F8/F9/scene transitions discard or finalize state deterministically rather than double-banking.
- The HUD is a consumer of authoritative score state rather than its own score source.

### Recovery, water and lifecycle

- Water recovery now distinguishes merely dry ground from a genuinely safe recovery point.
- Added bounded dry-history sampling, shoreline clearance checks, stable-ground filtering and repeat-return avoidance.
- Added short post-return hysteresis so a dangerous shoreline point is not immediately re-recorded.
- Preserved the important separation between:
  - F8 exit at the rider's **current** position
  - Session Marker explicit position/camera state
  - water-specific safe dry recovery history
  - F9 canonical emergency recovery
- Improved grotto/scene lifecycle cleanup and current-camera reacquisition.
- Preserved F9 recovery and lifecycle self-heal protections.

### Object Dropper and Session Marker

- Object Dropper retains camera-relative movement and elevated free placement.
- Preview height remains editor-owned instead of being constantly snapped by gravity.
- Rotation uses trigger + right-stick controls with immediate camera-stick return after release.
- Dynamic Dropper rails participate in grind capture/alignment and clean up correctly.
- Session Marker stores semantic camera orbit state relative to the skater rather than a drifting raw camera transform.
- Marker load remains same-scene only.

### UI, font and VHS health presentation

- Skate Mode prompts, Skate Options, Object Dropper and score typography now use the public **Nunito Sans** UI resource under SIL OFL 1.1.
- The previously supplied FOT-Chiaro binary is not distributed because redistribution permission was not established.
- Replaced the earlier imported VHS artwork with a newly authored **original SkateHarkinian procedural VHS cassette and HUD artwork**.
- VHS world pickups and the normal/Double Defense HUD states passed the final human visual check.
- Ordinary heart actor/gameplay behavior remains stock; SkateHarkinian changes the intended VHS presentation layer.

### Release hardening and public source

- Cleaned and reformatted SkateHarkinian-owned C++/Rust/tools for public readability without a gameplay rewrite.
- Repaired UTF-8/mojibake issues and regenerated patches without unrelated encoding changes.
- Removed temporary startup timing/debug traces while keeping useful recovery/resource warnings.
- Reduced read-only native snapshot overhead by avoiding unnecessary full-pose cloning.
- Disabled developer timing-scope work when profiling is not enabled.
- Added portable regression tests for score, vert, jump, ramps, FS360, shields, water, lifecycle/camera, Object Dropper, trees, authored grind edges, fonts and VHS resources.
- Added `BUILDING.md`, architecture documentation, Skate 3 data preparation documentation, manifests, verifier scripts, third-party notices and exact upstream pins.
- SkateHarkinian-original code and original project-created assets are now explicitly licensed under **MIT**.

## Download and requirements

A matching player package for RC2 is prepared separately. You need:

- Windows x64
- a configured Player-1 controller
- legally prepared supported OoT data (`oot.o2r`)
- your own locally prepared Xbox 360 Skate 3 data
- Microsoft Visual C++ 2015–2022 x64 Redistributable if Windows reports missing runtime DLLs

Neither game's retail data is included in this repository or the prepared release artifacts.

## Quick start

1. Extract the matching player ZIP into a new writable folder.
2. Supply your own `oot.o2r` next to `soh.exe`.
3. Follow [Skate 3 data setup](docs/SKATE3-DATA-SETUP.md), copying the complete prepared `assets` folder into `skate-data/assets`.
4. Run `VERIFY-INSTALL.ps1`; it validates the install and does **not** launch the game.
5. Launch manually, configure Player 1, load a save and press **F8**.

See [installation](INSTALLATION.md), [controls](CONTROLS.md), [playtesting](PLAYTESTING.md), [known issues](KNOWN-ISSUES.md) and [8.2C release notes](docs/RELEASE-NOTES-8.2C.md).

Enhancements → SkateHarkinian contains Board Appearance, Cheats / Gameplay Modifiers and other skate settings. Hold L1/LB for Skate Options and Session Marker/Object Dropper commands. R1/RB is contextual: supported off-board prop manipulation, eligible vert handplant, or existing riding grab behavior.

## Source and contributors

[BUILDING.md](BUILDING.md) documents exact upstream reconstruction and Release builds. [Architecture](docs/ARCHITECTURE.md) explains system ownership and boundaries. This repository contains SkateHarkinian integration/original source, resources, tests and patches rather than redistributing full upstream game decompilation or retail assets.

## Known limitations

This is still an early playtest. Three synthetic Bank-to-Ledge 10 m/s lip-pop fixture cases remain known failures; they are documented rather than hidden by a broad grace workaround. See [KNOWN-ISSUES.md](KNOWN-ISSUES.md).

Report build ID, scene/room, Adult/Child Link, controller bindings, cheats, selected prop/board and reproducible steps using [BUG-REPORT.md](BUG-REPORT.md). Trim private paths from logs before posting and never attach game data or personal saves.

## Legal and development

SkateHarkinian-original code and original SkateHarkinian-created assets are licensed under the [MIT License](LICENSE), Copyright (c) 2026 Kobewhon.

MIT applies **only** to SkateHarkinian-original work. It does not relicense Ship of Harkinian/Harbour Masters code, libultraship, NativeSkate or Skate 3 engine upstream code, Rust dependencies, other libraries/components, Nunito Sans, Nintendo/OoT runtime resources, Skate 3 retail data, or patch context derived from upstream projects. These retain their own licenses or separate rights status.

The exact SK8-ENGINE pinned-revision licensing/provenance question remains documented in [docs/LICENSE-REVIEW.md](docs/LICENSE-REVIEW.md). Shield models are runtime references into user-provided OoT data and are not bundled. Nunito Sans remains under SIL OFL 1.1. See [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).

Development is AI-assisted. Human gameplay testing is the acceptance gate; automated tests supplement it rather than replacing it.

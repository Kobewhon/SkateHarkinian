# SkateHarkinian 8.2C release candidate

Build: `2026-10-06-ff0209e76-8.2C-rc.2`. Local publication-review artifacts; recommended tag `v8.2C-rc.2`.

Skateboarding in Ocarina of Time through a custom Ship of Harkinian host and NativeSkate runtime. Windows x64, early public playtest; not an official Nintendo, EA or Harbour Masters release.

The 8.2C gameplay baseline passed human verification. Phase 8.2D.1 prepares rc.2 for publication review. The new original VHS visuals still require human acceptance; gameplay equivalence is checked against rc.1. Nothing has been published automatically.

## Highlights

Ride, flick tricks, grind authored rails and prop edges, skate explicit vert transitions and handplant at eligible coping. Build spots with Object Dropper, move supported physics props off-board, set a Session Marker, and track a monotonic LINE with a 2.50-second link window. Recovery includes safe water return and F9.

Board Appearance offers Default Skateboard and real Deku, Hylian and Mirror Shield decks with trucks and four wheels. All share identical gameplay physics. Gameplay modifiers include No Bailing, Ollie Height, Board Speed, Push Acceleration, Air Control and the optional Legacy FS 360 Pop Glitch.

## Download and requirements

The player ZIP and source ZIP are prepared locally for review. The publication reviewer must attach both to the chosen GitHub release; this README does not imply an already published download.

You need Windows x64, a configured Player-1 controller, legally prepared supported OoT data (`oot.o2r`) and your own prepared Xbox 360 Skate 3 data. Neither game is included. Install the Microsoft Visual C++ 2015–2022 x64 Redistributable if Windows reports missing runtime DLLs.

## Quick start

1. Extract the player ZIP into a new writable folder.
2. Supply your own `oot.o2r` next to `soh.exe` using matching Ship of Harkinian extraction tooling.
3. Follow [Skate 3 data setup](docs/SKATE3-DATA-SETUP.md), copying the complete prepared `assets` folder into `skate-data/assets`.
4. Run `VERIFY-INSTALL.ps1`; it does not launch the game.
5. Launch manually, configure Player 1, load a save and press F8.

See [installation](INSTALLATION.md), [controls](CONTROLS.md), [playtesting](PLAYTESTING.md) and [release notes](docs/RELEASE-NOTES-8.2C.md).

Enhancements > SkateHarkinian contains Board Appearance, Cheats / Gameplay Modifiers and other skate settings. Hold L1/LB for Skate Options and Session Marker/Object Dropper commands. R1/RB is contextual: off-board supported prop manipulation, eligible vert handplant, or existing riding grab behavior.

## Source and contributors

[BUILDING.md](BUILDING.md) describes pinned upstream preparation and Release builds. [Architecture](docs/ARCHITECTURE.md) explains ownership. This source bundle contains original integration code and patches rather than redistributing upstream game decompilation or retail assets.

## Limitations and reports

This remains a playtest. Three synthetic Bank-to-Ledge lip-pop cases remain known failures; no broad grace workaround is enabled. See [known issues](KNOWN-ISSUES.md). Report build ID, scene/room, Adult/Child Link, controller bindings, cheats, selected prop/board and reproducible steps using [BUG-REPORT.md](BUG-REPORT.md). Trim private paths from logs before posting; never attach game data or personal saves.

## Legal and development

SkateHarkinian-original code and original SkateHarkinian-created assets are licensed under the [MIT License](LICENSE), Copyright (c) 2026 Kobewhon. The owner selected this license explicitly; the original-work license question is **resolved**.

MIT applies **only** to SkateHarkinian-original work. It does not relicense Ship of Harkinian/Harbour Masters code, libultraship, NativeSkate or Skate 3 engine upstream code, Rust dependencies, other libraries/components, Nunito Sans, Nintendo/OoT runtime resources, Skate 3 retail data, or patch context derived from upstream projects. These retain their separate licenses or unresolved rights status. The SK8-ENGINE pinned-revision licensing/provenance question remains **open**.

Both games and their assets belong to their respective owners. Shield resources are runtime references into user-provided OoT data, not bundled models. Nunito Sans is distributed under SIL OFL 1.1. FOT-Chiaro is excluded because the supplied copyright notice did not establish redistribution permission. See [third-party notices](THIRD-PARTY-NOTICES.md) and [publication review](docs/LICENSE-REVIEW.md).

Development is AI-assisted. Human gameplay testing establishes acceptance; synthetic tests supplement it and do not prove appearance or feel on their own.

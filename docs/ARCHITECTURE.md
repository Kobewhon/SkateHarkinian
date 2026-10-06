# Architecture

The SoH integration lives in `overlay/soh/soh/Enhancements/NativeSkating`; patches connect Player draw/update, camera, health/pickup and menu hooks. Upstream architecture stays intact.

`NativeSkateMode` owns lifecycle and the current scene generation. SDL standardized native packets and SoH logical utility/combat bindings enter contextual routing. Pause/dialogue/loading and Object Dropper take priority; Biped carry, vert handplant and riding actions are context-bound. F8/F9 remain outside transient trick masks.

The Rust `runtime` crate provides ABI 4 over a bounded physics worker request/reply channel. The pinned `skate-host` owns simulation. Native snapshots are read-only; full skeletal pose export is separate. Host world geometry and actor trunk colliders become generation-bound providers. Dynamic props and authored multi-path grinds use stable IDs and dirty transform updates; deletion removes every path. Explicit ramp metadata/controller owns vert contact, not a global wall slope allowance.

Board presentation consumes one gameplay board root. `NativeSkateBoardAppearance` resolves a visual deck resource plus per-style local calibration; trucks/wheels remain original components. Normal OoT standalone get-item shield display lists are independent draw instances, never equipment mutation.

The HUD reads sequenced native events into the authoritative monotonic LINE ledger. Its 2.50-second link interval uses native 60 Hz simulation ticks, not rendered-frame counts; eligible continuous actions hold the opportunity. The HUD never writes score authority.

Water recovery uses bounded stable dry samples and WaterBox clearance. Scene teardown clears transient ownership, references, providers, prop carry, vert and camera generation; destination initialization reacquires current objects. F9 is canonical emergency recovery, not the normal water/scene route.

Object Dropper stores authoritative object transforms; static collision and dynamic grinds track those transforms. Carried movable props suspend gravity/settle authority until release. Session Marker restores same-scene semantic position/camera and explicitly attached props.

Original props, synthesized PCM audio and VHS resources are mod archives. `NativeSkateHud` registers one shared cached `SkateUI.ttf` font at atlas initialization. Retail resources and private Skate data are supplied by the user and excluded from source/player archives.

The original cassette generator writes the existing VHS Mesh/HUD interfaces. Pickup and fractional health hooks live in overlay/soh/soh/Enhancements/SkateHarkinian; they do not infer gameplay physics from the new mesh.

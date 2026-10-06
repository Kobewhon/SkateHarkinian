# Changelog

## 8.2C-rc.2 — since public 8.1F

### Gameplay and world

- Expanded the original ramp/rail library and added authored grindable paths on eligible ledges, hubbas, stairs, funboxes and manual-pad style props.
- Added explicit vert transitions and eligible coping handplants.
- Improved ramp collision/contact and momentum handling.
- Fixed the mini-ramp stale-contact/input softlock.
- Added solid tree-trunk collision providers without canopy blockers.
- Improved supported movable-prop ownership, carry/push behavior and cleanup.
- Preserved dynamic grind-provider cleanup across duplication, deletion, room changes and scene teardown.

### Link movement and board handling

- Added visible off-board jumping using stock Link jump animation as the rendered base.
- Added separate empty-hand and board-carry balance/landing presentation with meaningful limb motion.
- Added one stock Link jump voice event on takeoff.
- Preserved Adult/Child handling and the historical FS360 grab/dismount wedge fix.

### Board Appearance

- Added four persistent board appearance choices: Default Skateboard, Deku Shield, Hylian Shield and Mirror Shield.
- Shield choices use real OoT shield model resources as the visible deck with SkateHarkinian trucks/wheels underneath.
- All visual choices retain the same authoritative NativeSkate gameplay board and physics.
- Retired the failed texture-based themed-deck experiment.

### Cheats / gameplay modifiers

- Added No Bailing.
- Added Ollie Height 0.5×–3×.
- Added Board Speed 0.5×–3×.
- Added Push Acceleration 0.5×–3×.
- Added Air Control 0×–2×.
- Added optional Legacy FS 360 Pop Glitch.
- Added reset-to-default support.
- Legacy FS360 uses a fresh FS360 → fresh airborne trigger → near-landing Triangle/Y sequence, one impulse, no intentional dismount and no score bonus.

### LINE and scoring

- Active LINE score is monotonic.
- Trick-link opportunity is authoritatively 2.50 seconds in real time and frame-rate independent.
- Eligible continuous scoring actions keep the line alive while active.
- Completed lines bank once.
- F8/F9/scene transitions discard/finalize score state deterministically.
- HUD scoring is a consumer of authoritative gameplay score state.

### Recovery, water, scenes and camera

- Added bounded safe-dry recovery history rather than treating every technically dry point as safe.
- Added shoreline clearance/stability filtering, repeat-return avoidance and post-return hysteresis.
- Kept F8 current-position exit, Session Marker, water recovery and F9 recovery as separate authoritative systems.
- Improved grotto/scene suspend/reseed handling and current-camera reacquisition.
- Preserved F9 emergency recovery and self-heal protections.

### Object Dropper and Session Marker

- Faster camera-relative Object Dropper movement.
- Elevated preview/free placement keeps editor-owned height.
- Trigger + right-stick rotation returns camera-stick ownership immediately when released.
- Dynamic Dropper rails work with grind capture/alignment and cleanup.
- Session Marker stores semantic yaw/pitch/distance/mode relative to the skater and remains same-scene only.

### UI, font and VHS

- Unified Skate Mode prompt, Skate Options, Object Dropper and score typography.
- Replaced the unverified-redistribution FOT-Chiaro binary with OFL Nunito Sans.
- Replaced previous imported VHS visuals with original procedural SkateHarkinian cassette geometry and artwork.
- Preserved normal/Double Defense HUD states and health behavior.
- Final RC2 font/VHS presentation passed human visual testing.

### Release hardening and public source

- Mechanically reformatted SkateHarkinian-owned C++/Rust/tool source for public readability without retuning gameplay.
- Repaired UTF-8/mojibake issues and regenerated upstream patches without unrelated encoding churn.
- Removed temporary startup timing/debug traces while retaining useful lifecycle/recovery/resource diagnostics.
- Read-only native status snapshots avoid cloning the full animation pose.
- Disabled developer timing-scope work when profiling is off.
- Added/retained regression coverage for score, vert, jump, ramps, mini-ramp, Legacy FS360, shields, water, lifecycle/camera, trees, Object Dropper, authored grind edges, font and VHS resources.
- Added reproducible BUILDING, architecture, data-setup, verifier, manifest, upstream-pin and third-party-license documentation.
- SkateHarkinian-original code/assets are now explicitly MIT licensed.

### Known limitation

Three automated Bank-to-Ledge 10 m/s lip-pop cases remain failures at offsets 0.8, 0.1 and -0.1, entering NativeBumped. These are unchanged from the accepted baseline and remain documented rather than being hidden by a broad oscillating grace workaround.

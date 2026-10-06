# Changelog

## 8.2C-rc.2 ? since public 8.1F

### Gameplay and presentation

Expanded original ramp/rail libraries and authored ledge, hubba, stair, funbox and manual-pad grind paths. Faster camera-relative Object Dropper movement; gravity and authoritative off-board manipulation for supported movable props. Ramp collision/momentum improvements, explicit vert transitions and coping handplants. Mini-ramp stale-contact/input softlock fixed. Stock Link jump animation now supplies the off-board rendered base with separate board-carry balance, landing compression and one takeoff voice event. Board/trick handling and body participation improved.

### Board Appearance and cheats

Default Skateboard, actual Deku Shield, Hylian Shield and Mirror Shield models as cosmetic decks with trucks/wheels. One persistent selector; identical physics and ownership. Broken deck texture experiments retired. No Bailing, Ollie Height, Board Speed, Push Acceleration, Air Control and Legacy FS 360 Pop Glitch; defaults preserve ordinary behavior. Legacy FS360 is a fresh FS360 → trigger → near-landing Triangle/Y sequence, one impulse, no dismount and no extra score bonus. Historical grab/dismount wedge safety retained.

### World, recovery and score

Solid tree trunk actor providers without canopy blockers; provider deletion/scene cleanup. Safer bounded water-anchor history, clearance, hysteresis and repeat-loop avoidance. Clean scene/grotto suspend/reseed and current-camera generation handling. Authoritative 2.50-second LINE opportunity, continuous-action lifetime, monotonic active score and once-only banking. Kokiri overhead walkway compatibility preserved.

### UI and release hardening

Consistent compact Skate Mode prompts, Skate Options, Object Dropper and scoring typography. Public candidate uses OFL Nunito Sans instead of the unverified-redistribution FOT-Chiaro binary. Cached shared font, one atlas registration, safe legacy-font fallback. Full-pose clones removed from read-only native status snapshots; diagnostic scope timing disabled outside explicit developer profiling. Portable source/build/data setup and verification tools, exclusion manifests and third-party review.

### Known limitation

Three automated Bank-to-Ledge 10 m/s lip-pop cases remain failures. Human-verified ramp improvements do not imply those specific synthetic cases pass. No failed oscillating grace experiment is restored.

## rc.2 publication polish

Mechanical C++/Rust/tool readability formatting and UTF-8 repair; gameplay tuning and controls are unchanged from human-verified rc.1. VHS visuals are replaced with original procedural SkateHarkinian cassette geometry and artwork. Health/pickup mechanics are unchanged; human visual approval of the new cassette/HUD remains required.

The owner selected MIT for SkateHarkinian-original code/assets (Copyright (c) 2026 Kobewhon). Upstream and third-party work retain their own status. The exact SK8-ENGINE pin licensing question remains open; see docs/LICENSE-REVIEW.md.

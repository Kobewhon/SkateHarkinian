# 🛹 SkateHarkinian

> **Early Public Playtest — 8.1F**  
> Skate-inspired native skating gameplay inside **The Legend of Zelda: Ocarina of Time / Ship of Harkinian**.

[![Status](https://img.shields.io/badge/status-early%20playtest-orange)](https://github.com/Kobewhon/SkateHarkinian/releases)
[![Platform](https://img.shields.io/badge/platform-Windows%20x64-blue)](#requirements)
[![Build](https://img.shields.io/badge/build-2026--10--04--ff0209e76--8.1F-purple)](#build-information)

SkateHarkinian turns Ocarina of Time into an experimental skating sandbox with native board physics, tricks, grinds, scoring, park-building tools, Session Markers, skateboard audio, and more.

**This is not a finished release.** It is an early public playtest intended for feedback, bug reports, weird edge cases, and people trying to break things.

## ⬇️ Download

### [Download the latest playtest from GitHub Releases](https://github.com/Kobewhon/SkateHarkinian/releases)

Current playtest asset:

`SkateHarkinian-Playtest-2026-10-04-ff0209e76-8.1F.zip`

> **Important:** SkateHarkinian does **not** include Ocarina of Time ROM/game data or Skate 3 game data. Testers must supply their own legally prepared data.

## ✨ Current features

- Native skateboard movement and physics
- Ollies, kickflips, heelflips, shuvits, grabs and body flips
- Grindable world geometry
- Dynamically placed grindable rails
- Skate 3-style scoring and **LINE** system
- Session Markers that restore position, facing, camera and attached props
- Object Dropper / skate-spot building tools
- 10 textured rails and 10 textured ramps
- Elevated/floating placement
- Invalid-placement red preview
- Surface-aware rolling, pop, landing and grind audio
- Biped/off-board movement and camera
- Contextual sword/brake behavior
- Water bail recovery to safe ground
- VHS-themed health HUD and recovery pickups
- Adult Link and Child Link support
- OoT Reloaded compatibility
- F9 emergency NativeSkate recovery

## 💻 Requirements

- **Windows x64**
- A supported, legally prepared `oot.o2r`
- Optional `oot-mq.o2r` for Master Quest
- Your own prepared Skate 3 / NativeSkate data in the expected `skate-data/assets` format
- A controller configured as **Player 1** in Ship of Harkinian
- Microsoft Visual C++ 2015–2022 x64 Redistributable if Windows reports missing runtime DLLs

A controller is strongly recommended. The native riding/trick system relies on dual-stick/flick inputs and does not currently have a complete keyboard substitute.

## 🚀 Quick installation

1. Download the playtest ZIP from **Releases**.
2. Extract it to a new writable folder, for example `C:\Games\SkateHarkinian\`.
3. Put your prepared `oot.o2r` next to `soh.exe`.
4. Copy your complete prepared NativeSkate `assets` folder to:
   `skate-data\assets\`
5. Run `VERIFY-INSTALL.ps1`.
6. Launch `soh.exe`.
7. Configure Player 1, load a save, and press **F8** to enter Skate Mode.

### Expected private-data layout

```text
SkateHarkinian/
├─ soh.exe
├─ soh.o2r
├─ oot.o2r
├─ skateharkinian_runtime.dll
├─ mods/
│  ├─ skateharkinian-audio.o2r
│  ├─ skateharkinian-props.o2r
│  └─ skateharkinian-vhs.o2r
└─ skate-data/
   └─ assets/
      └─ private/
         ├─ game.json
         ├─ skater.glb
         ├─ character-lighting.json
         ├─ stock/
         ├─ default_skater/
         └─ native-character/
```

For the full walkthrough, see **[Installation Guide](docs/INSTALLATION.md)**.

## 🎮 Quick controls

| Input | Action |
|---|---|
| **F8** | Toggle Skate Mode |
| **F9** | Emergency NativeSkate recovery |
| **LS** | Steer/carve |
| **Cross / A** | Right-foot push |
| **Square / X** | Left-foot push |
| **Triangle / Y** | Mount / off-board toggle when allowed |
| **L2 / LT** | Left grab in air |
| **R2 / RT** | Right grab in air |
| **RS** | Trick gestures / manual input |
| **Hold L1 / LB** | Skate utility overlay |
| **L1 + D-Pad Down** | Set Session Marker |
| **L1 + D-Pad Up** | Return to Session Marker |
| **L1 + Circle / B** | Object Dropper |

### Basic trick gestures

| Trick | Right-stick motion |
|---|---|
| Ollie | Down → Up |
| Nollie | Up → Down |
| Kickflip | Down → Up-Right |
| Heelflip | Down → Up-Left |
| Pop Shuvit | Down → Right → Up-Right |
| Frontside Pop Shuvit | Down → Left → Up-Left |
| 360 Flip | Left → Down-Left → Down → Up-Right |

The runtime contains more authored trick behavior and stance-dependent variations. See the **[Complete Controls](docs/CONTROLS.md)** for the source-derived control list.

## 🧱 Object Dropper

Hold **L1/LB + Circle/B** to enter/toggle the Object Dropper.

Highlights:

- Place ramps and grindable rails
- Move props relative to the live placement camera
- Raise/lower objects into the air
- Yaw with **L2/LT + Right Stick horizontal**
- Pitch with **R2/RT + Right Stick vertical**
- Duplicate objects
- Delete placed objects
- Attach selected props to Session Markers
- Invalid placements glow red
- Floating placement is intentionally supported

Current player content includes **10 rails, 10 ramps and VHS Tape**.

## 🧪 What I need testers to try

Please stress-test:

- General skating feel
- Trick consistency
- Grinds and grind capture
- Placed rails at different yaw/pitch angles
- Ramps at different speeds/angles
- Object Dropper placement/deletion/duplication
- Session Marker retries
- Scoring and LINE linking
- Skateboard audio and surface transitions
- Water bails
- Biped/off-board behavior
- Scene transitions
- Adult Link / Child Link
- F9 recovery
- Crashes, softlocks and anything that feels wrong

See **[Playtesting & Bug Reports](docs/PLAYTESTING.md)** before reporting an issue.

## 🐛 Reporting bugs

Use the repository's **Issues** tab and choose the SkateHarkinian bug-report template.

Helpful reports include:

- What happened
- What you were doing
- Scene/location
- Riding / Biped / Object Dropper state
- Object involved, if any
- Reproduction steps
- Expected vs actual behavior
- Screenshot/video
- Current `logs/Ship of Harkinian.log`
- Build ID

## ⚠️ Known limitations

- Props are temporary and room-scoped; there is no park-save system yet.
- Object registry maximum is 64, while solid props also compete for OoT's finite Dyna slots.
- Usable rails/ramps are currently static after placement.
- Native riding/trick inputs require a controller.
- SoH rebinding does not currently remap every raw native riding action.
- This is an early playtest and is **not claimed bug-free**.

## 🛠 Troubleshooting

**No skating:** verify `skateharkinian_runtime.dll` and the complete `skate-data/assets/private` data.

**Missing props/VHS/audio:** verify all three supplied `mods/skateharkinian-*.o2r` archives.

**No skateboard sound:** check **Enhancements → SkateHarkinian → Audio**, Skate Sounds, Skate Sound Volume, and normal SoH audio/output settings.

**Skate Mode gets stuck:** press **F9**.

**Install check:** run:

```powershell
powershell -ExecutionPolicy Bypass -File ".\VERIFY-INSTALL.ps1"
```

## 📦 Build information

**Build:** `2026-10-04-ff0209e76-8.1F`  
**Host:** custom Windows x64 host based on Ship of Harkinian 9.2.3, commit `ff0209e76`  
**Native runtime ABI:** 4

Playtest ZIP SHA-256:

```text
619e1a37b34219ffdc77b7bcc8f16c6be6b47a61baad5c98d124c1218f1ba0ad
```

## 📚 Documentation

- **[Installation Guide](docs/INSTALLATION.md)**
- **[Complete Controls](docs/CONTROLS.md)**
- **[Playtesting & Bug Reports](docs/PLAYTESTING.md)**
- **[GitHub Releases](https://github.com/Kobewhon/SkateHarkinian/releases)**

## Legal / project status

SkateHarkinian is an unofficial fan project and is not affiliated with or endorsed by Nintendo, Harbour Masters, Electronic Arts, or the rights holders of The Legend of Zelda or Skate.

No Ocarina of Time ROM/game dump, extracted Skate 3 game data, proprietary Skate 3 audio, personal saves, or personal configuration files are distributed in the playtest package.

Third-party notices for redistributed runtime components are included inside the playtest ZIP under `LICENSES/`.

Implementation has been AI-assisted, with live gameplay verification performed by human testing.

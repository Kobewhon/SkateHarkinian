# SkateHarkinian 8.1F — Installation Guide

This guide is for the **Windows x64 early public playtest**.

## 1. Download the playtest

Open the repository's [Releases](https://github.com/Kobewhon/SkateHarkinian/releases) page and download:

`SkateHarkinian-Playtest-2026-10-04-ff0209e76-8.1F.zip`

Extract the ZIP into a new writable folder, for example:

```text
C:\Games\SkateHarkinian\
```

Do not run the game directly from inside the ZIP.

## 2. Keep the supplied custom host

The extracted folder includes a custom `soh.exe`, `soh.o2r`, `skateharkinian_runtime.dll`, JSON files and SkateHarkinian mod archives.

**Do not replace the supplied `soh.exe` with stock Ship of Harkinian.**

The playtest relies on this custom host.

## 3. Add your own Ocarina of Time data

Copy your legally prepared supported:

`oot.o2r`

into the same folder as `soh.exe`.

If using Master Quest, also place:

`oot-mq.o2r`

next to `soh.exe`.

If you need to prepare OoT data, use the official Ship of Harkinian setup process. SkateHarkinian does not contain or extract a ROM for you.

## 4. Add your own prepared Skate 3 / NativeSkate data

Copy the **complete** `assets` folder from your already prepared NativeSkate / Skate 3 data into:

```text
SkateHarkinian\skate-data\assets\
```

The resulting layout must include:

```text
skate-data\assets\private\game.json
skate-data\assets\private\skater.glb
skate-data\assets\private\character-lighting.json
skate-data\assets\private\stock\
skate-data\assets\private\default_skater\
skate-data\assets\private\native-character\
```

Do **not** put this data in the `mods` folder.

SkateHarkinian does not distribute this game data.

## 5. Check the final folder structure

Your install should resemble:

```text
SkateHarkinian/
├─ CHANGELOG.md
├─ CONTROLS.md
├─ gamecontrollerdb.txt
├─ HASHES.json
├─ native-skate-surfaces.json
├─ native-skate-world.json
├─ PLAYTEST-NOTES.md
├─ README.md
├─ skateharkinian_runtime.dll
├─ soh.exe
├─ soh.o2r
├─ oot.o2r
├─ VERIFY-INSTALL.ps1
│
├─ mods/
│  ├─ skateharkinian-audio.o2r
│  ├─ skateharkinian-props.o2r
│  └─ skateharkinian-vhs.o2r
│
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

## 6. Verify the installation

Open PowerShell in the SkateHarkinian folder and run:

```powershell
powershell -ExecutionPolicy Bypass -File ".\VERIFY-INSTALL.ps1"
```

A complete install should report:

```text
PASS: package hashes and required data paths. Launch soh.exe manually.
```

The verifier checks package files/hashes and required private-data paths. It **does not launch the game**.

## 7. Launch

Run:

`soh.exe`

Then:

1. Configure a controller as **Player 1**.
2. Load a save.
3. Press **F8** to enter Skate Mode.

SkateHarkinian settings/help/build information are under:

**Enhancements → SkateHarkinian**

## 8. Optional OoT Reloaded

A compatible OoT Reloaded pack may be installed separately in `mods`.

For first-time troubleshooting, test with only the three supplied SkateHarkinian archives enabled.

## Troubleshooting

### No skating

Verify:

- `skateharkinian_runtime.dll`
- `native-skate-world.json`
- `native-skate-surfaces.json`
- the complete `skate-data/assets/private` data

Stock SoH alone is insufficient.

### Missing props, VHS visuals or audio

Verify:

```text
mods\skateharkinian-audio.o2r
mods\skateharkinian-props.o2r
mods\skateharkinian-vhs.o2r
```

### No skateboard sound

Check:

**Enhancements → SkateHarkinian → Audio**

and verify Skate Sounds / volume plus normal SoH audio-output settings.

### Skate Mode stops responding

Press **F9** for emergency NativeSkate recovery.

### Logs

The useful log is normally:

`logs/Ship of Harkinian.log`

relative to the application directory. Attach the current log and build ID when filing a bug report.

## Requirements recap

- Windows x64
- Prepared `oot.o2r`
- Prepared Skate 3 / NativeSkate private data
- Controller configured as Player 1
- Microsoft Visual C++ 2015–2022 x64 Redistributable if Windows reports missing runtime DLLs

No Python or Visual Studio is required to run the playtest.

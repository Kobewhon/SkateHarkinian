# Preparing your own Skate 3 data

SkateHarkinian contains open-source integration/runtime code and original mod resources. It does **not** contain the copyrighted Xbox 360 Skate 3 game data. You need your own legally obtained copy. No piracy/download mirrors are part of this guide.

## The two separate parts

**Open-source tooling:** this build uses the Skate crates embedded in [chasmlol/2010-rust-rewrite-mashup](https://github.com/chasmlol/2010-rust-rewrite-mashup), pinned to `0875efd7806f68217e8c48ab9b33b0964dbdcefe`, plus the supplied SkateHarkinian patches. Its own LICENSE is Apache-2.0. The embedded engine identifies [SK8-ENGINE/skate-3-rust-engine](https://github.com/SK8-ENGINE/skate-3-rust-engine) revision `cb7968930f14dad38457e98720d1a274e469eec2`. The older engine license/provenance gap is documented in LICENSE-REVIEW.md; do not assume the current engine branch is interchangeable.

SkateHarkinian uses the physics, action/motion graphs, animation evaluator and data readers, through its ABI 4 wrapper. It does not require MW2 to play SkateHarkinian. The fork's complete launcher does require MW2; use its standalone Skate converter rather than following its unrelated launcher/game installation flow.

**Retail data:** the converter reads your extracted Xbox 360 Skate 3 `default.xex` with the adjacent `data` directory. The tooling repository is not a source of those retail files.

This is the pinned Rust pipeline, **not** Alex's unrelated native recomp/ISO-selection release. Its [official pinned Skate guide](https://github.com/chasmlol/2010-rust-rewrite-mashup/blob/0875efd7806f68217e8c48ab9b33b0964dbdcefe/docs/SKATE.md) explicitly requires extracted data. Do not combine commands from those projects.

## 1. Obtain an extracted copy from your own media

Keep `default.xex` and `data/` in the same extracted game folder. For your own disc ISO, the pinned guide documents [XboxDev extract-xiso](https://github.com/XboxDev/extract-xiso):

```powershell
extract-xiso -x "<your-own-disc-image.iso>"
```

The converter does not accept an ISO directly. The official guide also describes extracting an owned Games on Demand container; use the documented upstream workflow for that format. It is not a folder containing only `default.xex`.

The pinned converter checks these files beside it:

```text
<your extracted game>/
  default.xex
  data/big/miscload.big
  data/big/miscboot.big
  data/big/db.big
  data/content/createacharacter.big
  ...the rest of your extracted game...
```

## 2. Obtain the matching converter

Use the standalone `skate/iw4l-skate-convert.exe` from the official mashup release matching the pinned pipeline. Do not download a pre-extracted asset folder from anyone. Release names can change: verify its included documentation and converter revision before using it.

For a reproducible developer build, check out both pinned repositories and follow the fork's `skate/converter/build.ps1 -SkateEngine <engine-checkout> -Out <output-folder>`. The script expects the engine at cb79689 and needs Python 3.13, PyInstaller, NumPy, Pillow and Rust. These are conversion-tool requirements, not required for ordinary players running the packaged game.

From a writable working folder, run the standalone converter with quoted paths:

```powershell
& "<converter-folder>/iw4l-skate-convert.exe" --xex "<your-extracted-game>/default.xex" --out "./skate-data"
```

Wait for successful completion. It writes `skate-data/assets` and a conversion log locally; it does not modify the source game. Keep the entire result, not just two files. Never run the converter with `--out` pointing at your original game folder: its output staging/rename replaces its output directory.

## 3. Install the prepared result

Copy the complete prepared **assets** folder into the SkateHarkinian folder, exactly one level below `skate-data`:

```text
SkateHarkinian/
  soh.exe
  skateharkinian_runtime.dll
  skate-data/
    assets/
      private/
        game.json
        skater.glb
        stock/
          physics-skeletons.json
          skater-collections.json
          data/
            config/input.cfg
            anim/OnBoard.abin
            anim/OffBoard.abin
            state/ActionGraph_OnBoard.stategraph
            state/MotionGraph_OnBoard.stategraph
            script/camera/Default_cameragraph.stategraph
            ...all other generated stock files...
```

`game.json` must be format version 1 and its `character_scene`, `action_graph` and `motion_graph` must name existing relative files within this assets root. The tree above matches the verified prepared manifest; the verifier follows those manifest fields rather than assuming every valid conversion has identical names.

`character-lighting.json`, `default_skater/`, `native-character/` and optional `custom/` output may also be generated. Keep them when present, but the current host does not independently require those optional directory names. Required data includes the complete stock settings/graphs/animation banks and referenced GLB. The verifier checks obvious absence/emptiness and key manifest references; it cannot prove every binary's compatibility with the runtime.

## 4. Verify without launching

Run `VERIFY-INSTALL.ps1` beside `soh.exe` or, for developers:

```powershell
python tools/verify_skate_data.py "<installation>/skate-data/assets"
```

The verifier reads locally, never uploads/downloads data, does not alter your copy and does not compare retail hashes against public lists. On success, manually launch the game, configure Player 1 and try F8. Compilation of the mod code does not require retail data; native integration tests and gameplay do.

## Troubleshooting

| Symptom | Check/fix |
|---|---|
| Wrong game/version, stategraph/animation parse failure | Use Xbox 360 Skate 3 and the exact pinned converter/runtime format; rerun matching conversion. Do not mix an unrelated recomp's output. |
| Missing default.xex | Extract your own media; keep the executable with its complete data folder. |
| Missing BIG/data files | Extraction is incomplete or the selected folder is wrong; supply the complete extracted game. |
| game.json or skater.glb missing | Conversion did not finish, or the assets directory was copied incorrectly. Check conversion.log and rerun to a new output directory. |
| NativeSkate data unavailable | Expected path is beside the executable at skate-data/assets/private, not in mods and not skate-data/assets/assets/private. Run the verifier. |
| Missing stock/graphs/animation | Copy the full prepared folder, not just GLB/manifest; confirm converter success. |
| Spaces in path | Quote both executable and arguments as shown. |
| Permission/read-only failure | Choose a writable folder outside Program Files; do not alter source media permissions unnecessarily. |
| Runtime DLL missing after extraction | Inspect antivirus quarantine; verify archive SHA-256 and use the matching host/DLL pair. Do not disable protection globally. |
| Runtime/prepared format mismatch | Use matching pinned tools and data; preserve the old working installation as rollback. |

## Do not share or upload

Never share the Skate 3 ISO, extracted data, default.xex, prepared private assets, retail-derived game.json/skater.glb, or stock/default_skater/native-character content. Generate these locally from your own copy. Do not attach them to issues, source ZIPs or release assets.

# Building the reviewed source bundle

## Prerequisites and pins

Windows x64; Visual Studio 2022 Build Tools v143 with Desktop development with C++ and Windows SDK; CMake >=3.26 (tested 3.31.6); Git; Python >=3.12 for preparation tools; Rust supporting edition 2024 and Bevy 0.19 (tested rustc 1.99.0). Resource generators additionally use NumPy/Pillow. Ordinary gameplay needs none of these developer tools.

Exact upstream URLs/commits are in UPSTREAM-PINS.json: Shipwright ff0209e76f01cf816806cd1d8521373744d1dc8e, libultraship 62e973aeb4a53ad4d22bb91e2d9373ecdfcd246c, Torch 2ab12fe9660aec04e02ee89fe81baed304a1a1d6 and the native mashup fork 0875efd7806f68217e8c48ab9b33b0964dbdcefe. Do not substitute an unrelated Skate recomp fork. Read docs/LICENSE-REVIEW.md before distributing a build.

## Prepare

Extract the source ZIP into a writable directory. In a VS 2022 x64 Native Tools/Developer PowerShell:

```powershell
python tools/prepare_upstreams.py
```

The script clones official upstreams into ignored upstream/, selects exact commits/submodules, applies patches and overlays. It stops on conflicting local work. Retail data is not downloaded. The archive deliberately does not redistribute the full upstream OoT decompilation or private recovered/game asset trees.

## Native runtime Release

```powershell
cargo build --release --locked --manifest-path runtime/Cargo.toml --target-dir build/native
cargo test --release --locked --manifest-path runtime/Cargo.toml --target-dir build/native --lib
```

Expected output: `build/native/release/skateharkinian_runtime.dll`, ABI 4. No retail files are needed to compile. For portable source-path diagnostics, optionally set `RUSTFLAGS` to `--remap-path-prefix=<absolute-source-root>=/src` before building; it changes embedded source locations, not gameplay. The wrapper allocates a 16 MB worker stack and adapts its floating-point environment; do not call Session::new on a default small host stack.

## Host Release

```powershell
cmake -S upstream/shipwright -B build/host -G "Visual Studio 17 2022" -A x64 -DVCPKG_TARGET_TRIPLET=x64-windows-static -DSOH_ROM_PATH= -DCMAKE_EXE_LINKER_FLAGS_RELEASE=/PDBALTPATH:soh.pdb
cmake --build build/host --config Release --target soh -- /m:3 /p:PostBuildEventUseInBuild=false
cmake --build build/host --config Release --target GenerateSohOtr -- /m:3 /p:PostBuildEventUseInBuild=false
```

Shipwright's build provisions/fetches its declared open-source dependencies through vcpkg/FetchContent. A preinstalled compatible vcpkg root can be supplied through the project-standard VCPKG_ROOT environment variable and CMAKE_TOOLCHAIN_FILE; do not point at another machine's absolute paths. The tested clean build used a fresh CMake directory and the existing declared x64-windows-static dependency installation; automatic provisioning also ran in that clean directory. Dependency caches are not old SkateHarkinian object files.

Upstream DefaultCXX.cmake sets output beside the source: expect `upstream/shipwright/x64/Release/soh.exe`, not build/host/Release. Release uses optimization/LTO; never distribute Debug executables. The GenerateSohOtr command builds the packer and produces `build/host/soh/soh.o2r` from host-owned resources; this is separate from the user's oot.o2r. Compiling the host and generating this archive do not need a ROM. We disable automatic post-build copying/extraction; prepare user game data separately for runtime use.

## Mod resources

Checked-in resources/*.o2r are original SkateHarkinian props, synthesized Foley, VHS and the OFL font; they can be copied without retail data. `python tools/prepare_skate_fonts.py` deterministically rebuilds the generic font archive with its notice. `python tools/generate_skate_sfx.py` uses a fixed seed to generate original WAV/PCM slots; use the tested archive when preserving an accepted audio baseline.

To rebuild props, compile tools/props/prop-export.cpp with the overlay NativeSkating directory on the include path, redirect its JSON into build/prop-meshes.json, then run:

```powershell
python tools/props/build-props.py --meshes build/prop-meshes.json --archive resources/skateharkinian-props.o2r
```

Rebuild the original procedural cassette and ten HUD states with `python tools/generate_vhs.py` (Python 3.10+ and Pillow). Its source artwork and provenance are in `resources/vhs-original/`. Run `python tests/vhs-resource-test.py` to check deterministic output, geometry bounds and texture formats. No shield assets are generated: get-item shield display lists resolve from legally prepared user OoT resources.

## Tests and benchmarks

```powershell
powershell -File tools/run_regressions.ps1
```

Optional parameters: -ImGuiRoot <host-build/_deps/imgui-src> for font atlas/layout testing; -RuntimeDll <fresh DLL> -AssetsRoot <user prepared assets> for the 2,000-cycle actual-mini-ramp integration fixture. Retail-dependent tests are not part of compilation and never launch soh.exe. The private final-render stock-frame oracle is excluded from source; the human-verified rendered baseline and asset-free final-limb/stock-base contracts are retained.

`snapshot_equivalence.py <golden-dll> <candidate-dll> <assets-root>` compares all native snapshot fields over 2,000 inputs. `cargo run --release --manifest-path runtime/Cargo.toml --example snapshot_benchmark -- <assets-root>` compares old full-pose extraction with the lightweight equivalent, counting allocator operations. tests/perf-benchmark.cpp compares enabled old timing work against disabled Scope work. Record machine/load/configuration and actual measurements; these are microbenchmarks, not whole-game FPS claims.

## Staging and distribution

Use a new writable staging/package directory. Copy the matching host/DLL, matching soh.o2r, gamecontrollerdb.txt, supplied world/surface configuration and all four mod archives into mods/. Include LICENSES, docs and VERIFY-INSTALL.ps1. Never copy personal config/saves/logs or private game data into a distributable ZIP. See SOURCE-MANIFEST.txt and docs/SKATE3-DATA-SETUP.md.

No build script launches the game or publishes GitHub. A competent reviewer must verify licensing/provenance and conduct the post-hardening human check before publication.

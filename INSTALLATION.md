# Installation and upgrading

Extract into a new writable folder. Keep the matching custom host, runtime DLL, `soh.o2r`, controller database, world/surface JSON and the four SkateHarkinian mod archives together. Do not mix an old host with a new runtime.

Prepare supported OoT data separately using matching Ship of Harkinian tooling, then place your own `oot.o2r` beside `soh.exe`; supply MQ data separately if needed. Follow docs/SKATE3-DATA-SETUP.md for the complete prepared `skate-data/assets` folder. Neither game is included.

Run VERIFY-INSTALL.ps1 before manually launching. Configure Player 1, load a save, press F8. F9 is emergency recovery. Enhancements > SkateHarkinian exposes Board Appearance and Cheats / Gameplay Modifiers.

From 8.1F: keep that installation as a rollback. Copy your game data into this new installation. Back up saves before copying them. Start with fresh configuration and only the supplied mods; reapply bindings/settings deliberately. Do not copy old executables/DLLs or old SkateHarkinian font archives. Add optional OoT Reloaded afterward to isolate failures.

Never share your game archives, prepared private assets, saves or personal configuration. Tests require no Windows font installation.

Windows x64 prerequisite: [Microsoft Visual C++ v14 redistributable](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist). Use the official x64 installer if a runtime DLL is reported missing.

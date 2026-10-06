# License scope and remaining publication review

This is a local handoff, not a blanket license declaration.

## RESOLVED: SkateHarkinian-original work

The owner explicitly selected the MIT License for SkateHarkinian-original code and original SkateHarkinian-created assets. Root LICENSE contains the standard MIT text with Copyright (c) 2026 Kobewhon. This question is resolved and is no longer a publication gate.

MIT applies only to original SkateHarkinian work. It does not relicense Ship of Harkinian/Harbour Masters code, libultraship, NativeSkate or Skate 3 upstream engine code, Rust dependencies, other libraries/components, Nunito Sans, Nintendo/OoT resources, Skate 3 retail data or patch context derived from upstream projects. A patch containing upstream context does not acquire MIT coverage for that context merely because it appears in this repository.

The rc.2 cassette geometry and artwork are newly procedural SkateHarkinian creations covered by MIT. The new provenance states: "Original SkateHarkinian procedural VHS cassette and artwork." The bundled Nunito Sans font used for labels remains under its own OFL; it is not relicensed.

## OPEN: pinned engine and upstream permission scope

Public redistribution of engine-derived source/binaries requires confirmed rights/permission for SK8-ENGINE revision **cb7968930f14dad38457e98720d1a274e469eec2**, or another independently verified licensing basis. The root MIT License does not resolve that question.

The pinned mashup fork has Apache-2.0 LICENSE/NOTICE. Its `skate` tree identifies SK8-ENGINE revision cb7968930f14dad38457e98720d1a274e469eec2; that revision has third-party notices but no top-level LICENSE. The current SK8-ENGINE main branch has GPLv3, which is not evidence that the older pin was licensed under those terms. The fork NOTICE also describes a narrower third-party scope than its later embedded Skate tree. Review/confirm this provenance before public redistribution.

The pinned Shipwright checkout has no checkout-wide license file. The owner previously confirmed custom-host redistribution permission, but the supplied checkout does not itself establish every source/binary right. The source bundle therefore ships SkateHarkinian integration and patches, with official upstream fetch instructions, not a blanket relicensing of Shipwright or the engine. Publication reviewer must confirm the permission scope and any corresponding-source obligations before uploading either ZIP.

libultraship and Torch include MIT licenses; Fast3D has its own retained notice. Other dependencies retain their original licenses. No claim of ownership over upstream code is made.

The supplied FOT-Chiaro copyright text says all rights reserved and contains no redistribution grant. It is excluded from both prepared public artifacts. Nunito Sans comes from Google's official fonts repository, revision 8b0a1d0f5983c89bc2b93f1b5fb55f9e252744b5, under SIL OFL 1.1 with its full notice.

Shield model references: `objects/object_gi_shield_1/gGiDekuShieldDL`, `objects/object_gi_shield_2/gGiHylianShieldDL`, `objects/object_gi_shield_3/gGiMirrorShieldDL` and its `gGiMirrorShieldSymbolDL`. These resolve through legally prepared user OoT data; no extracted shield binaries or textures are included.

Do not publish retail OoT/Skate data, saves/configs, private traces or checkpoints. The release reviewer must retain full notices and attach the matching source artifact beside any approved player binary artifact.

# Known issues

The human-verified 8.2C gameplay baseline includes mini ramps, rendered jumps and actual shield boards. The 8.2D font/cleanup candidate still needs a brief human comparison after rebuilding.

Three synthetic Bank-to-Ledge lip-pop cases at 10 m/s (offsets 0.8, 0.1 and -0.1) still enter NativeBumped rather than accept the authored pop. Ordinary ramp improvements passed human testing; these fixture failures remain documented and are not hidden by an oscillating grace workaround.

Native and SoH logical bindings differ; this is controller-first, not a complete native keyboard substitute. Some trick names depend on stance and prepared data. Optional graphics packs may need isolated compatibility checks. Font fallback uses the prior safe HUD path if the mod font resource is unavailable.

A genuinely unresponsive native worker is quarantined rather than unloading executing DLL code. Its module can remain resident until process exit. F9 remains available; a normal stationary/manual/handplant is not a fault.

Publication is pending license/provenance review described in docs/LICENSE-REVIEW.md.

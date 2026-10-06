# Known issues

The 8.2C RC2 gameplay baseline, rebuilt package, Nunito UI font and original VHS world/HUD presentation have passed the final human smoke/visual check.

## Bank-to-Ledge synthetic cases

Three synthetic Bank-to-Ledge lip-pop cases at 10 m/s (offsets 0.8, 0.1 and -0.1) still enter NativeBumped rather than accepting the authored pop. These exact failures also exist in the accepted golden baseline. Ordinary ramp improvements passed human testing, and the failures are intentionally documented instead of being hidden by the old oscillating grace workaround.

## Input scope

Native and SoH logical bindings differ. SkateHarkinian is controller-first and is not a complete native keyboard substitute. Some trick names depend on stance and prepared data.

## Optional graphics packs

Optional graphics/retexture packs may require isolated compatibility testing. The public UI font has a safe fallback path if the SkateHarkinian font resource is unavailable.

## Worker quarantine behavior

A genuinely unresponsive native worker is quarantined rather than unloading DLL code that may still be executing. Its module can remain resident until process exit. F9 remains available; normal stationary/manual/handplant states are not treated as worker faults.

## Publication/licensing review

SkateHarkinian-original work is MIT licensed. Separate upstream/provenance review for the pinned SK8-ENGINE revision remains documented in `docs/LICENSE-REVIEW.md`.

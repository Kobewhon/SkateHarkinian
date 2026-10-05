# Playtesting & Bug Reports

Build: **2026-10-04-ff0209e76-8.1F**

This is an **early public playtest**. The previous gameplay baseline was human-verified, but this build is not claimed bug-free.

## Please test

1. Launch from a fresh extracted folder.
2. Press F8 and enter Skate Mode.
3. Push, coast and carve.
4. Perform several tricks.
5. Test brake and drawn-sword tap/hold behavior.
6. Enter Biped mode and test the camera while walking/running.
7. Set and reload a Session Marker.
8. Enter/exit the Object Dropper.
9. Place and ride ramps.
10. Place and grind rails from both directions.
11. Yaw/pitch rails and verify grind alignment.
12. Delete placed objects.
13. Verify genuine invalid placements glow red.
14. Test elevated/floating placement and duplication.
15. Listen for roll/pop/land/grind audio and surface changes.
16. Trigger a water bail.
17. Press F9 and verify clean recovery.
18. Change room/scene.
19. Test Adult Link and Child Link if practical.
20. Quit/relaunch.

Also try grabs/catches/body flips, longer LINE combos, repeated marker attempts, and intentionally weird obstacle setups.

## Known limitations

- Temporary room-scoped props; no disk park persistence yet.
- Registry maximum 64.
- Solid props also use OoT's finite Dyna collision slots.
- Delete is immediate.
- Usable rails/ramps are static after placement.
- Native riding/flicks require a controller.
- SoH rebinding does not remap every raw native action.
- Practical object capacity varies by scene.

## Bug reports

Please use the repository's bug-report template.

A useful report contains:

- Build ID
- What happened
- What you were doing
- Location / scene
- Riding / Biped / Object Dropper state
- Object involved, if any
- Whether it reproduces
- Exact steps
- Expected behavior
- Actual behavior
- Screenshot/video
- Current `logs/Ship of Harkinian.log` if available

### Minimal template

```text
WHAT HAPPENED:

WHAT WERE YOU DOING:

LOCATION / SCENE:

BUILD ID:

SKATE MODE STATE: Riding / Biped / Object Dropper

OBJECT INVOLVED:

CAN YOU REPRODUCE IT:

STEPS:

EXPECTED:

ACTUAL:

SCREENSHOT / VIDEO:

LOG ATTACHED: Yes / No
```

## F9 recovery

F9 is an accepted emergency recovery feature.

If Skate Mode stops responding correctly, press **F9** and include that fact in your bug report. A water bail is normal gameplay and should not be reported as F9/self-heal unless something actually breaks.

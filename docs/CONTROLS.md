# Controls — SkateHarkinian 8.1F

**Labels assume default bindings.** PlayStation / Xbox: Cross/A, Circle/B, Square/X, Triangle/Y; L1/LB, R1/RB, L2/LT, R2/RT. LS/RS = left/right stick.

Utility commands, attack and most editor controls use SoH logical N64 bindings. Native riding buttons/sticks and raw Square/Triangle editor shortcuts use SDL standardized Player-1 actions. Remapping a SoH N64 button does not remap every native riding action.

## Skate Mode / recovery

| Input | Action |
|---|---|
| F8 | Toggle Skate Mode; exit at current position |
| F9 | Emergency NativeSkate recovery; temporary props clear |
| Configured pause | Pause ownership takes priority |

Normal OoT bindings apply when Skate Mode is off. There is no complete native keyboard riding/flick/dual-stick substitute: use a Player-1 controller.

## Riding

| Input | Action |
|---|---|
| LS | Steer/carve; horizontal also supplies eligible aerial body spin |
| Cross/A | Right-foot push |
| Square/X | Left-foot push |
| Logical attack, normally Circle/B; sword sheathed | Brake |
| Logical attack; sword drawn | Tap: attack on release; hold: brake after ~183 ms |
| Triangle/Y | Toggle off-board/mount when native state permits |
| L2/LT, R2/RT | Left/right air grab; ground crouch/grab follows native state |
| RS during grab | Tweak/board adjustment |
| R1/RB | Eligible world/coping grab |
| RS partial up/down | Manual/nose-manual request |
| LS strong directional sweep | Eligible powerslide/kick-turn request |

Riding RS owns tricks, not a camera. Grinding has no added universal button: ollie toward a valid rail with a suitable approach.

## Tricks

Use **RS** setup → quick sweep/flick. Recenter between separate gestures.

| Trick family | RS motion, regular reference orientation |
|---|---|
| Ollie | Down → Up |
| Nollie | Up → Down |
| Kickflip | Down → Up-Right |
| Heelflip | Down → Up-Left |
| Pop Shuvit | Down → Right → Up-Right |
| Frontside Pop Shuvit | Down → Left → Up-Left |
| 360 Pop Shuvit | Down-Left → Down → Right |
| Frontside 360 Pop Shuvit | Down-Right → Down → Left |
| 360 Flip | Left → Down-Left → Down → Up-Right |
| Hardflip | Down-Right → Down → Up-Right |
| Inward Heelflip | Down-Left → Down → Up-Left |
| Nollie Kickflip / Heelflip | Up → Down-Right / Down-Left |

Body flips use **LS near takeoff**:

- Small centered/down setup → Up requests FrontFlip
- Centered/up setup → Down requests BackFlip

Grabs use the triggers in the appropriate airborne state. Catch and landing remain native.

During native wipeout, fresh Cross/A or Square/X requests recovery. Both fully depressed native triggers + both stick clicks request a wipeout where permitted.

## Biped / off-board

| Input | Action |
|---|---|
| LS | Walk/run independently of camera |
| RS | Orbit/pitch normal camera, regardless of board visibility/carry or drawn sword |
| Cross/A held | Eligible off-board sprint |
| Square/X fresh | Jump |
| Triangle/Y | Mount/toggle when allowed |
| Fresh full L2/LT or R2/RT | Native board drop/retrieve or throw/retrieve request respectively |
| Logical attack | Contextual sword action |

Editor, pause, dialogue and special cameras take priority.

## Session Marker / utility

| Input | Action |
|---|---|
| Hold logical L1/LB | Utility overlay |
| L1/LB + D-Pad Down | Set/update position, facing and normal skate camera |
| L1/LB + D-Pad Up | Return; restore explicitly attached props and saved camera |
| L1/LB + logical attack, normally Circle/B | Enter/toggle Dropper; attack suppressed until released |
| Off-board near a prop: logical R held + marker-set chord | Attach nearest eligible prop while setting marker |

Set a marker before editor attachment. Only attached props reset.

## Object browser / root

| State/input | Action |
|---|---|
| Browser D-Pad Left/Right | Category |
| Browser D-Pad Up/Down | Object |
| Logical A, normally Cross/A | Select/enter placement |
| Logical B, normally Circle/B | Placement → browser → root → skating |
| Browser logical L, normally L1/LB | Duplicate last placed prop into candidate |
| Placed Objects raw Square/X fresh | Delete selected prop once per press |
| Root D-Pad Up/Down | Category |
| Root logical A | Browse |
| Root logical C-Left/C-Right | Undo last surviving placement / clear temporary objects |

Current player content: ten rails, ten ramps and VHS Tape. Debug objects require Developer Diagnostics and Show Developer Object Library.

## Placement / edit

| Input | Action |
|---|---|
| LS | Horizontal movement relative to live placement camera |
| RS without modifier | Camera orbit |
| L2/LT held + RS horizontal | Yaw |
| R2/RT held + RS vertical | Pitch |
| Both modifiers | X yaw + Y pitch; no roll |
| D-Pad Up/Down held | Raise/lower; manual air height is authoritative |
| D-Pad Right/Left held | Camera zoom in/out |
| Logical A | Commit valid candidate, return to browser |
| Logical B | Cancel preview; confirmed props remain |
| Logical L | Duplicate candidate; original edited prop remains |
| Raw Square/X | Explicit ground snap, resets pitch/roll |
| Edit existing: raw Triangle/Y held 0.5 s, or logical C-Up press | Delete selected existing prop |
| Edit existing: logical C-Right | Attach current pose to active marker |

Red preview blocks confirmation. Floating alone is valid. Static rails/ramps keep their committed transform. Copy preserves height/yaw/pitch.

### Trigger behavior note

The accepted editor uses live SDL raw trigger travel and SoH's global trigger-button threshold, normally 25%; it does not require literal full trigger travel. Rotation and full-stick camera suppression use the same held state. Releasing the trigger immediately restores camera control without an RS-recenter requirement.

## Notes

- Props are room-scoped and may clear during recovery/lifecycle teardown.
- No park-save files yet.
- Grinding depends on valid approach/geometry, not a dedicated grind button.
- Trick availability and naming can change with stance/fakie/native state.

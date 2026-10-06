# Controls - 8.2C release candidate

**Labels assume default bindings.** PlayStation / Xbox: Cross/A, Circle/B, Square/X, Triangle/Y; L1/LB, R1/RB, L2/LT, R2/RT. LS/RS = left/right stick.

Utility commands, attack and most editor controls use SoH logical N64 bindings. Native riding buttons/sticks and raw Square/Triangle editor shortcuts use SDL standardized Player-1 actions. Remapping a SoH N64 button does not remap every native riding action. No specific PlayStation device ID is required.

## Skate Mode / keyboard / recovery

| Input | Action |
|---|---|
| F8 | Toggle Skate Mode; exit at current position |
| F9 | Emergency NativeSkate recovery; temporary props clear |
| Configured pause | Pause ownership takes priority |

Normal OoT bindings apply when Skate Mode is off. Keyboard logical A/B/L/C-buttons and D-Pad follow your SoH configuration. There is no complete native keyboard riding/flick/dual-stick substitute: use a Player-1 controller. No undocumented keyboard bindings are implied.

## Riding

| Input | Action |
|---|---|
| LS | Steer/carve; horizontal also supplies eligible aerial body spin |
| Cross/A | Right-foot push |
| Square/X | Left-foot push |
| Logical attack, normally Circle/B; sword sheathed | Brake |
| Logical attack; sword drawn | Tap: attack on release; hold: brake after 11 native ticks (~183 ms) |
| Triangle/Y | Toggle off-board/mount when native state permits |
| L2/LT, R2/RT | Left/right air grab; ground crouch/grab follows native state |
| RS during grab | Tweak/board adjustment |
| R1/RB | Eligible world/coping grab |
| RS partial up/down | Manual/nose-manual request; native state owns engagement/balance |
| LS strong directional sweep | Eligible powerslide/kick-turn requests; angle/state determines activation |

Riding RS owns tricks, not a camera. Grinding has no added universal button: ollie toward a valid rail with a suitable approach. LS horizontal supplies balance; LS+RS horizontal supplies translation; RS vertical supplies up/down grind control. Native geometry/state determines the accepted grind.

## Tricks

Use **RS** setup → quick sweep/flick. Recenter between separate gestures. These directional descriptions come from authored patterns loaded by the actual runtime, not new fixed timing rules. Stance/fakie mirroring can change the resulting name.

| Trick family | RS motion, regular reference orientation |
|---|---|
| Ollie | Down → up |
| Nollie | Up → down |
| Kickflip | Down → up-right |
| Heelflip | Down → up-left |
| Pop shuvit | Down → right → up-right |
| Frontside pop shuvit | Down → left → up-left |
| 360 pop shuvit | Down-left → down → right |
| Frontside 360 pop shuvit | Down-right → down → left |
| 360 flip | Left → down-left → down → up-right |
| Hardflip | Down-right → down → up-right |
| Inward heelflip | Down-left → down → up-left |
| Nollie kickflip/heelflip | Up → down-right/down-left |

Body flips use **LS near takeoff**: small centered/down setup → up requests FrontFlip; centered/up setup → down requests BackFlip. Eligibility and the takeoff window belong to native action/motion graphs. Grabs use the triggers in the appropriate airborne state. Catch and landing remain native; no invented catch key. Flip-hold, late/fingerflip recognizers and stance variants are authored-data driven; no unverified universal shortcut is claimed for every named variant.

During native wipeout, fresh Cross/A or Square/X requests recovery. Both fully depressed native triggers + both stick clicks request a wipeout where permitted. Native physical full travel is distinct from the editor threshold below.

## BIPED / off-board

| Input | Action |
|---|---|
| LS | Walk/run independently of camera |
| RS | Orbit/pitch normal camera, regardless of board visibility/carry or drawn sword |
| Cross/A held | Eligible off-board sprint |
| Square/X fresh | Jump |
| Triangle/Y | Mount/toggle when allowed |
| Fresh full L2/LT or R2/RT | Native board drop/retrieve or throw/retrieve request respectively |
| Logical attack | Contextual sword action |

Editor, pause, dialogue and special cameras take priority. Board presentation does not gate biped camera.

## Session Marker / utility

| Input | Action |
|---|---|
| Hold logical L1/LB | Utility overlay |
| L1/LB + D-Pad Down | Set/update position, facing and normal skate camera |
| L1/LB + D-Pad Up | Return; restore explicitly attached props and saved camera |
| L1/LB + logical attack, normally Circle/B | Enter/toggle Dropper; attack suppressed until released |
| Off-board near a prop: logical R held + marker-set chord | Attach nearest eligible prop while setting marker |

Set a marker before editor attachment. Only attached props reset. Scene/room validity applies. Marker load preserves the existing active score-reset policy; banked totals follow existing LINE rules. F8, water anchor, F9 and scene spawn remain separate systems.

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

Current player content: ten rails, ten ramps and VHS Tape. Ledges/Props categories may be empty. Debug objects require Developer Diagnostics AND Show Developer Object Library.

## Placement / edit

| Input | Action |
|---|---|
| LS | Horizontal movement relative to LIVE placement camera |
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

Red preview blocks confirmation. Floating alone is valid. Static rails/ramps keep their committed transform. Copy preserves height/yaw/pitch. C-button bindings depend on SoH. RS motion suppresses generated C-button presses to prevent accidental commands. Logical C-Left also provides the legacy snap shortcut before manual height, without a rotation modifier.

**Source discrepancy:** accepted editor activation uses live SDL raw trigger travel 0–32767 and SoH's global trigger-button threshold, normally 25%; it does not use literal 0.95 normalized full travel. Rotation and full-stick camera suppression use the SAME held booleans. Release immediately restores camera control, without RS recenter. This working behavior is unchanged.

Editor exit retains its existing button-release handoff; that is not an RS recenter gate. Props are room-scoped and clear on unsafe teardown/recovery. No park save files.

## Source basis

Audited final NativeSkateInput, UtilityInput, ContextInput, Mode, ObjectDropper, ObjectEditor, marker/camera integration, SDL/Xbox mapping and runtime riding/trick/off-board/grind/wipeout producers. Gesture descriptions derive from the PAT data actually loaded by the runtime; no proprietary PAT files are redistributed. Logical bindings, raw SDL controls and authored eligibility are distinguished above.

## Additions since 8.1F

Off-board near a supported movable prop, hold R1/RB to acquire/push it; walking/turning moves its authoritative transform. Release R1 to drop and restore gravity. Editor/pause/dialogue/loading take priority. Riding R1 remains the existing world/coping action except at eligible explicit vert coping, where it requests a handplant. Failed eligibility must leave ordinary control intact.

Enhancements > SkateHarkinian > Board Appearance selects exactly one of Default Skateboard, Deku Shield, Hylian Shield or Mirror Shield. This is cosmetic, with no equipment/physics changes. Cheats / Gameplay Modifiers defaults are No Bailing OFF, Ollie Height/Board Speed/Push Acceleration 1.0x, Air Control 1.0x and Legacy FS 360 Pop Glitch OFF. Reset restores those defaults.

With Legacy FS 360 Pop Glitch ON: perform a valid FS 360 Pop Shuvit, press a fresh trigger airborne, then fresh Triangle/Y shortly before valid flat/near-flat landing (within approximately 0.16 seconds). Success consumes Triangle without dismount, applies one secondary pop and awards no extra bonus. Wrong trick/order/timing, grind/vert/handplant/water/loading cannot trigger it. Normal pop multiplier does not multiply the secondary impulse.

Object Dropper translation is about twice the old normal rate while retaining cubic small-stick control and camera-relative movement. No new universal fast modifier is claimed.

LINE link opportunity is exactly 2.50 seconds after the last qualifying event/action end; eligible grind/manual/grab/handplant/air action holds it alive. A line banks once after expiry, and active score does not decrease.

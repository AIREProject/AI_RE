# Shared Combat Damage, Enemy Reaction, and Aggro Swap Contract

## 1. Scope and status

This document defines the project-owned combat seam introduced by `M03-E09-T01`.
It covers Player/MAKO damage delivery, enemy Health and stagger reactions, the direct
`EnemyBase -> BossEnemy` hierarchy, Boss awareness/aggro, and the two-person Q aggro
swap. Backend, replication, boss phases, wild animals, inventory, storage, and
`ST_AIRECompanion_Local` changes are outside this task.

- Code baseline: 2026-08-07
- Task status: `Review` (`M03-E09-T01`) / implementation in progress (`M03-E09-T02A`)
- Completed locally: T01 C++ baseline and assets, one partial fallback PIE session, and
  T02A Boss physical-trace source plus static checks
- Still required: Unreal build, automation execution, Boss animation assets and assignment,
  moving-target/AI stability, physical Q input integration, and the remaining PIE/lifecycle gates

## 2. Runtime ownership

```text
Player trace / MAKO Ability / Enemy attack
-> FAIRECombatDamageRequest
-> UAIRECombatDamageSubsystem
-> UAIRECombatDamageGameplayEffect
-> UAIRECombatDamageExecution
-> target-declared Health / FlinchGauge / StunGauge attributes
```

The caller owns hit timing and creates an `ExecutionId`. The world subsystem owns
validation, same-affiliation rejection, and exact-once application. The target owns
its ASC and declares which attributes the shared execution may modify through
`IAIRECombatDamageTargetInterface`. The execution never branches on Player, MAKO,
or Boss classes. Attack definitions also carry `EAIRECombatTargetingMode`; this gate
implements actual damage fan-out only for `SingleTarget`. `Area` is retained as an
explicit classification seam and is rejected by current Player/MAKO validation and
does not expose a Boss Q-swap opportunity.

### 2.1 Request contract

`FAIRECombatDamageRequest` contains:

| Field | Rule |
|---|---|
| `Source`, `Target` | Valid, distinct actors in the same world; both participate in the combat target contract |
| `Damage` | Finite and non-negative |
| `StaggerValue` | Finite and non-negative |
| `ExecutionId` | Valid GUID created once per attack execution or combo step |
| `HitResult` | Optional presentation/context evidence; guarded by `bHasHitResult` |

Damage and stagger cannot both be zero. PlayerParty-to-PlayerParty and Enemy-to-Enemy
requests are rejected by affiliation, so Player and MAKO do not need caller-specific
friendly-fire branches.

An `ExecutionId` applies at most once to a given target. The same ID may apply once
to another target, preserving a future multi-target attack seam. The bounded ledger
keeps the most recent 4096 target/execution records and prunes destroyed targets.

### 2.2 Target contract

All combat targets provide a Health attribute and affiliation. Flinch and stun
attributes are optional. A damage-only request may still apply to a target without
stagger attributes; a stagger-only request to such a target is rejected explicitly.

- Player: global `HP`, affiliation `PlayerParty`
- MAKO: companion `Health`, affiliation `PlayerParty`
- Enemy: global `HP`, enemy reaction gauges, affiliation `Enemy`

## 3. Enemy composition and Boss baseline

`AAIREEnemyBase` is abstract. `AAIREBossEnemy` is its only concrete child in this
task and inherits it directly.

```text
AAIREEnemyBase
├─ UAIREEnemyVitalityComponent
├─ UAIREEnemyReactionComponent
├─ UAIREEnemyAttackComponent
├─ UAIREEnemyReactionAttributeSet
└─ UAIPerceptionStimuliSourceComponent

AAIREBossEnemy : AAIREEnemyBase
```

The vitality component owns Health initialization, the dead tag, ability
cancellation, and the exactly-once death event. The enemy character stops movement,
collision, perception, attack, and controller logic on death. `DeathRemovalDelay`
defaults to five seconds; zero leaves the corpse in the world. All baseline values
come from a validated `UAIREEnemyConfigDataAsset`; a missing or invalid assignment
uses that class's default object values. EndPlay and UnPossessed clear timers,
delegates, and ASC actor info.

The Boss baseline is:

| Value | Default |
|---|---:|
| Health / MaxHealth | 500 / 500 |
| Flinch threshold / duration | 50 / 0.45 s |
| Stun threshold / duration | 200 / 2.5 s |
| Melee damage / stagger | 25 / 25 |
| Surface attack range | 180 cm |
| Attack cooldown | 1.25 s |

Each stagger hit adds to both gauges. Stun has priority when both thresholds become
eligible on the same frame. Flinch consumes one threshold and preserves remainder;
stun clears both gauges when it ends. Stagger received while stunned is ignored.
Return-home reset clears reaction tags, timers, and gauges. Optional `FlinchMontage`
and `StunMontage` play once on state entry; transition, recovery, return, death, and
EndPlay stop only the active reaction montage. Missing assets do not affect logic.

## 4. Boss awareness, aggro, and attack

`AAIREEnemyAIController` owns perception and an optional `UStateTreeAIComponent`.
If an assigned StateTree starts, it owns the behavior graph. Without one, the C++
fallback loop remains playable:

```text
IdleUnaware -> Alerted -> EngagedChase -> EngagedAttack
                    lost -> Searching -> Returning -> IdleUnaware
                    hit  -> Flinching / Stunned
                    HP 0 -> Dead
```

When a StateTree is running, each non-terminal state Enter task must call
`ReportStateTreeAwarenessState` with `IdleUnaware`, `Alerted`, `EngagedChase`,
`EngagedAttack`, `Searching`, or `Returning`. This keeps
`GetCombatSnapshot().AwarenessState` synchronized while the C++ fallback Tick is
disabled. The reporting seam rejects calls when the enemy is dead or actively
reacting, and rejects attempts to report `Flinching`, `Stunned`, or `Dead`; those
states remain owned by the vitality/reaction runtime. After reaction recovery, the
next StateTree state Enter task reports the resumed non-terminal state.

Sight creates threat `1`. Applied damage adds threat at multiplier `1`, and threat
does not decay. Ordinary threat replacement still requires a lead of `10`, but it is
deferred while the current target is visible or within a `3.0 s` recent-sight grace
window. This prevents another party member briefly crossing the sight ray from
causing an immediate target turn. The explicit Q promotion bypasses that hold.
Damage remains valid short-lived engagement evidence for one second. Lost targets
use their last known location for a three-second search. Returning clears aggro and
ignores new aggro selection. Reaction gauges are reset only when return-home
completion is confirmed; the home leash defaults to 2500 cm.
`UAIREEnemyConfigDataAsset.HomeLeashRadius` is the per-enemy authority for that
distance, and zero disables distance-driven return while preserving explicit and
target-loss return paths. This allows arena bosses to sustain combat without changing
the default leash behavior of ordinary enemies.
Return movement and completion use the same center-to-home acceptance radius; the
navigation request does not add the controlled pawn radius. Once return completes,
sight tracking restarts and a visible eligible party target begins a fresh
IdleUnaware-to-Alerted engagement.

An attack snapshots its target, attack execution ID, selected pattern, play rate,
attack axis, and melee-trace settings for the full windup/recovery. Later aggro changes
affect the next attack and do not curve the active attack. Eligible data-asset patterns
are filtered by surface distance and current Health ratio, selected by weight, and avoid
the two most recently selected patterns while another eligible choice exists. If that
pool is exhausted, the immediately previous pattern remains excluded whenever at least
one alternative is eligible. On a
successfully played montage, `UAIREEnemyMeleeTraceAnimNotifyState` opens, updates, and
closes one or more indexed trace windows; montage end itself never applies damage. A
montage that never opens a window is a miss and emits a non-shipping diagnostic. If no
montage plays, the timed fallback runs one forward sphere sweep instead. The deprecated
`CommitActiveMeleeHit` seam also runs a spatial fallback sample and cannot commit from
target identity or distance alone.

Boss physical resolution is now implemented in T02A source. A configured bone/socket
pair sweeps the previous and current limb segments with a config-driven sphere. Missing
or invalid bones/sockets fall back to the snapshotted forward axis. A window may override
the socket pair for alternating-hand attacks and carries damage/stagger scales. Each
unique strike index receives a distinct damage-request execution ID, so one montage may
apply several exact-once contacts without defeating the shared subsystem ledger. Both
paths use the same collision channel, accept only the snapshotted target as hit evidence,
and attach the resulting `FHitResult` to `FAIRECombatDamageRequest`. Target loss,
reaction, return, death, interruption, and EndPlay close the trace window and clear
timers/delegates. The default fallback radius, reach from the capsule surface, and
channel are 35 cm, 180 cm, and `ECC_Pawn`.

An attack pattern may opt into `ForwardMoveDistance` and a desired
`ForwardMoveStopDistance`. A positive stop distance clamps the move from the
snapshotted target surface distance so it does not intentionally close past that
distance; zero preserves the full configured forward move for committed lunges that
rely on Character Movement collision to stop them.
`UAIREEnemyAttackMovementAnimNotifyState` may start this move before the independent
damage trace window; existing montages without that state retain first-trace-window
startup for compatibility. The move follows the already snapshotted attack axis and
uses montage play-rate-scaled duration. Character Movement owns the capsule sweep, so
walls and Pawns block it. Code-driven movement is rejected for montages that already
extract root motion, and reaction, death, return, target destruction, montage
interruption, and EndPlay remove any active movement source with the attack.

Per-pattern `ReuseCooldown` prevents a selected gap closer or signature attack from
being selected again before its own lockout expires, in addition to the shared attack
cooldown and recent-pattern avoidance. A Boss config may opt into combat approach
actions: far targets are approached at `CombatSprintSpeed`, and an eligible gap closer
may start from the outer pattern range. If the target is inside that outer range but no
pattern is eligible, the Boss closes directly to the preferred melee range instead of
alternating lateral steps. A code-driven gap closer temporarily requires one
non-gap-closing melee follow-up before another gap closer may start. The requirement
is owned by the attack selector and clears only after that non-gap attack starts or the
engagement is explicitly reset by return, death, unpossession, or EndPlay. Target swaps,
distance changes, and brief reaction interruptions do not bypass it. While the follow-up
is pending, the Boss closes continuously to the preferred melee range without artificial
pursuit pauses or lateral cooldown steps. During other shared attack cooldowns, at most
one bounded lateral reposition is requested.
Navigation is stopped only after an attack actually starts. Requests are reused while
active, restored to `MovementSpeed` on exit, and cleared on return, death, unpossession,
or EndPlay.
Damage authority remains solely with the montage trace window; sprint, lateral movement,
and attack movement do not commit damage by proximity.

The current Boss asset uses a `140 cm` preferred surface range. Its ordinary pattern
bands now overlap from `0` through `220 cm`, while `DashCross` retains its intentional
`220-700 cm`, `ForwardMoveDistance=1000`, `CooldownScale=1`, and `ReuseCooldown=0`
contract. These values only control entry and selection; the `100 cm` limb sweep still
owns actual hit evidence.

The project-owned Boss assets and component assignments are configured. A user-owned
Rider build and PIE session verified locomotion, sustained chase, gap-closer movement,
the required non-gap follow-up, death-pose retention, return/re-engagement, and MAKO
follow/return stability. `AIRE.Combat.Damage.SharedPipeline` passed on 2026-08-09.
`AIRE.Combat.Enemy.Attack.FallbackTrace` exposed two automation-fixture mismatches. The
cooldown-only patterns were corrected, and spatial fallback now invokes the public spatial
wrapper directly instead of trying to advance TimerManager repeatedly within one engine
frame. Gameplay was not changed. After the user rebuild, both `SharedPipeline` and the
corrected `FallbackTrace` passed on 2026-08-09. MAKO physical trace and
evade presentation remain in
`M03-E09-T02B` until its model, Skeleton, AnimBP, weapon mesh, and trace-socket contract
are fixed. The user-approved moving-target stability slice does modify MAKO source and
its basic weapon range, but not MAKO animation or model assets.

Attack entry and cancellation also require separate ranges in T02. Once an attack is
active, small target movement must not cause StateTree MoveTo and attack cancellation
to alternate. The target may miss the eventual sweep without tearing down and
restarting the same attack every frame. Chase reuses one active `MoveToActor` request
for the same target, and its reach test includes both the Boss and target collision
radii so it agrees with the attack component's surface-distance gate.

MAKO now keeps the selected sight target through a configurable `3.0 s` sight-loss
memory and a `200 cm` selected-target lose-sight distance. Its StateTree no longer
cancels an active basic attack or combat skill solely because the target crosses the
entry range: the Ability owns hit-time and next-combo-step range validation. A completed
fixed-point approach that is still out of range waits `0.5 s` before requesting another
approach, preventing continuous same-frame path re-issuance. The current basic melee
weapon surface range is `150 cm`. Movement retry and ability retry timers are independent,
and StateTree does not emit attack events while the corresponding GAS cooldown tag is
active. Follow and Return use capsule surface distance; Return latches at `600 cm` and
releases at `400 cm`, while Follow stops at `200 cm`.

## 5. Q aggro swap

`UAIREAggroSwapComponent` is owned by the project PlayerController. It resolves the
possessed pawn and exactly one other alive `PlayerParty` actor that has an
`UAIRECombatEvadeComponent`; it does not depend on concrete Player or MAKO classes.
It also resolves exactly one active `AAIREEnemyBase` melee opportunity. Zero active
opportunities do not consume cooldown. Multiple active opportunities are rejected
as ambiguous and also consume no cooldown.

A valid opportunity requires an active, non-area melee attack whose hit has not been
committed or cancelled. After full preflight, the 12-second cooldown is consumed
before the commit attempt and remains consumed if that attempt loses a race.

On success:

1. Cancel only the active attack's pending damage; its recovery continues.
2. Stop the old target's optional player primary action.
3. Cancel the old target's movement and GAS abilities.
4. Sweep both lateral directions and dash up to 300 cm over 0.25 seconds toward the
   farther clear side; an equal result chooses right.
5. Promote the other party actor to `current maximum threat + 25`.

The evade is code-driven swept movement. It does not teleport or exchange actor
positions and does not mutate root-motion mode. The optional montage must therefore
be authored in place with root motion disabled.

T01 exposes the PlayerController input seam but does not own the production Player
Input Action or Mapping Context. No `IA_AIREAggroSwap` or dedicated IMC is currently
assigned. The Player Combat/Input owner creates and maps physical Q in
`M03-E09-T03`; Enemy or MAKO asset work must not pre-empt that ownership.

## 6. Current Editor baseline and remaining ownership

The following LMK-owned Boss assets were created, compiled/saved where applicable,
and loaded in the partial PIE session:

- `/Game/Work/LMK/Enemy/Boss/BP_AIRE_Boss`
- `/Game/Work/LMK/Enemy/Boss/BP_EnemyAIController`
- `/Game/Work/LMK/Enemy/Boss/DA_Boss_Config`
- `/Game/Work/LMK/Enemy/Boss/ST_AIREEnemy_Combat`

The StateTree must continue to report only non-terminal awareness states through
`ReportStateTreeAwarenessState`. `Flinching`, `Stunned`, and `Dead` remain runtime
vitality/reaction states.

`M03-E09-T02A` owns Boss attack/flinch/stun/death presentation, Boss physical melee
sweeps, and the remaining Boss-only non-input PIE and lifecycle gates. `M03-E09-T02B`
owns MAKO moving-target attack stability, adoption of the physical sweep, and the
in-place MAKO evade montage after the MAKO asset contract is fixed. Root motion stays
disabled for the evade montage because `UAIRECombatEvadeComponent` owns the swept
capsule movement.

The selected T02A source assets are Paragon Crunch's mesh and `Crunch_Skeleton`, with
`Ability_Combo_01_Montage`, `HitReact_Front`, `Stunned_Start`/`Stunned_Loop`, and
`Death_A` as project-owned montage sources. The vendor assets remain untouched. The
project-owned `ABP_AIRE_Boss` and Attack/Flinch/Stun/Death montage copies still need to
be authored, compiled, saved, and assigned after the C++ build succeeds.

`M03-E09-T03` owns `IA_AIREAggroSwap`, the active Player IMC Q mapping,
PlayerController assignment, Player evade presentation, and the physical two-way
Player/MAKO swap gate. Existing Player mapping assets must remain unchanged until
that owner performs the integration.

Only the Boss derivative participates in this gate. WildAnimal remains out of scope.

## 7. Verification gate

Run the narrow automation tests `AIRE.Combat.Damage.SharedPipeline` and
`AIRE.Combat.Enemy.Attack.FallbackTrace`. The first uses the existing PlayerParty
combat fixture plus two real Boss instances to cover shared Health selection, Boss
Health/stagger, validation, exact-once, and death edges. The second covers Boss
fallback forward hit, rear/side miss, WorldStatic occlusion, cancellation, target
destruction, repeated same-strike resolution, and two distinct strikes within one attack.
It also covers fixed play-rate pattern selection and pattern-scaled fallback damage.
Neither test instantiates the production Player or MAKO classes. Verify those adapters
and all montage, socket-sweep, weighted selection, and AI behavior in PIE:

### 7.1 Partial PIE evidence recorded on 2026-08-06

- Boss, EnemyController, and the assigned StateTree spawned and ran; the Boss moved
  during chase.
- The observed window contained Chase 11, Attack 4, Flinching 1, Searching 7, and
  Returning 1 transitions. Idle, Alerted, Stunned, and Dead were not observed.
- Fallback Boss hits changed Player HP 100 to 75 and MAKO Health 100 to 50 to 25.
- Boss HP changed from 500 to 389; this window proves MAKO-side damage but does not
  separately prove the production Player-to-Boss adapter.
- Flinch gauge 25 and stun gauge 75 were observed after the recorded hits.
- PIE EndPlay stopped the StateTree and released the controller once. This is not
  evidence for the required three lifecycle repetitions.
- Searching and Chase alternated seven times on the Boss AI. The log did not include
  target identity or transition reason, so T02 must diagnose this before adding a
  blind debounce.

### 7.2 Remaining gate

- Player and MAKO damage reduce Boss Health through the same request path.
- MAKO and Boss melee damage commits only after the actual spatial sweep intersects
  the snapshotted target; rear, obstructed, and out-of-shape targets miss.
- A moving target inside the cancellation hysteresis does not cause MoveTo/Attack
  oscillation; a sweep miss completes one recovery before the next AI decision.
- Duplicate execution IDs do not apply twice to one target and may apply once to a different target.
- Player/MAKO friendly fire and Enemy/Enemy damage are rejected.
- Generic enemy defaults remain Flinch `50` and Stun `200`. The current Boss config uses
  Flinch `100` and Stun `300`, so MAKO's `25` basic-hit stagger does not interrupt every
  second hit while the `75` combat skill still contributes meaningful poise pressure.
- Sight, damage-only engagement, alert, chase, attack, search, return, and death transitions complete.
- With the StateTree running, each non-terminal Enter task updates the combined
  combat snapshot; an Enter report cannot overwrite active flinch, stun, or death.
- Active attacks keep their snapshotted target even if ordinary aggro changes during recovery.
- Q outside an opportunity consumes no cooldown; Q during one valid opportunity cancels damage, evades the old target, promotes the other target, and starts a 12-second cooldown.
- Two simultaneous enemy opportunities reject Q without consuming cooldown.
- Obstruction chooses the farther lateral side; equal clearance chooses right; both blocked sides remain collision-safe.
- Spawn/Possess/UnPossess/Destroy repeated three times leaves no Timer, Delegate, perception, StateTree, ASC, or focus residue.
- Backend and LLM remain unavailable without breaking the local combat loop.

T01 remains `Review`; it must not be closed as `Done` merely because T02/T03 now own
the remaining integration work. Close it after those tasks provide the original gate
evidence, or record an explicit scope reduction first. Unreal build and Editor
execution remain user-owned verification steps.

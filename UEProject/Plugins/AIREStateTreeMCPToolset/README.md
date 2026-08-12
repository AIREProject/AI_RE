# AIRE StateTree MCP Toolset

Editor-only UE 5.8 MCP tools for project-owned StateTree, UMG, and animation assets.

## Safety boundary

- Mutation calls accept only assets under `/Game/Work/LMK/`.
- Each mutation creates an Unreal Editor transaction for Undo support.
- Mutations mark the StateTree dirty but do not save it.
- `ValidateAndCompile` and `SaveStateTree` are separate explicit calls.
- `SaveStateTree` rejects assets that still require compilation.
- UMG tree creation and property editing use UE's built-in `UMGToolSet` and
  `EditorToolset.ObjectTools`.
- `AIREUMGMCPToolset` adds project-scoped slide/fade animation creation,
  inspection, explicit compilation, and saving.
- UMG mutations reject assets outside `/Game/Work/LMK/`.
- Combo montage mutations validate one indexed hit notify per section, add only
  missing combo window states, and reject assets outside `/Game/Work/LMK/`.
- Enemy melee montage mutations remove only the legacy `SaveAttack` and
  `ResetCombo` notifies, replace project-owned melee trace windows, and reject
  assets outside `/Game/Work/LMK/`. Each window carries a unique strike index,
  damage/stagger scales, and an optional per-strike socket pair.
- `ConfigureEnemyAttackMovementWindow` replaces only the project-owned attack
  movement state so a collision-aware gap closer can start independently from
  its damage trace window.
- Enemy attack tempo mutations replace only project-owned tempo states. They
  reject overlapping tempo windows and any overlap with an attack movement
  window, preserving trace timing, strike indices, and damage settings.
- `ConfigureMontageAnimationTrack` rebuilds a project-owned Montage from one or
  more compatible Animation Sequences, recalculates the real play length through
  the montage data controller, and can clear template Montage notifies.
- `InspectAnimationBoneMotion` samples selected bones on a compatible Skeletal
  Mesh and reports path length, peak speed, maximum reach, and forward-extension
  timing for contact-window and striking-hand decisions.
- `InspectAnimationBoneLocalRotations` compares selected animation bones against
  their mesh reference-pose rotations at explicit normalized times. Use it to
  distinguish finger curl from whole-arm movement during retarget validation.
- Frank-to-MAKO retarget tools create project-scoped IK Rig and IK Retargeter
  assets without overwriting existing retargeted animations. The default setup
  preserves the MAKO pelvis, spine, neck, and head reference pose while aligning
  limbs and fingers, uses one-to-one rotation for the spine, arms, and fingers,
  and exports trials under `Animations/Retarget/Fixed`.
- `CreateOrUpdateMakoWeaponSockets` copies the Frank left and right weapon-handle
  reference transforms onto MAKO mesh-only `weapon_l` and `weapon_r` sockets.
  It does not create or extract a separate weapon mesh.
- `RunBossCombatAutomationTests` starts only
  `AIRE.Combat.Damage.SharedPipeline` and
  `AIRE.Combat.Enemy.Attack.FallbackTrace`; completion remains explicit in the
  Output Log.
- Control Rig hierarchy sync accepts only project assets under `/Game/Work/LMK/`,
  replaces and removes bone hierarchy entries from the selected Skeletal Mesh,
  leaves curves and sockets untouched, and does not compile or save the asset.

## Workflow

1. Inspect the existing StateTree with the built-in read-only StateTree toolset.
2. Use `ListNodeTypes` to obtain an exact node struct path.
3. Add or move states, nodes, transitions, and property bindings.
4. Use `ListNodeProperties` before changing a node property with `SetNodePropertyText`.
5. Call `ValidateAndCompile` and resolve every reported error.
6. Call `SaveStateTree` only after the compiled structure has been inspected.

Named state and node creation is retry-safe: a matching sibling state or a matching named node is returned instead of duplicated.

## UMG workflow

1. Use `UMGToolSet.CreateWidgetBlueprint` and `AddWidget` to build the tree.
2. Use `EditorToolset.ObjectTools` to inspect and set exact widget and slot properties.
3. Use `AIREUMGMCPToolset.CreateOrReplaceSlideFadeAnimation` for horizontal
   translation and opacity tracks.
4. Inspect, compile, and save through explicit lifecycle calls.

## Combo montage workflow

1. Place one `AIRECompanionAttackHitAnimNotify` in each montage section and set
   its zero-based combo step index.
2. Use `InspectBasicAttackComboMontage` to verify marker, section, and notify layout.
3. For a combined animation sequence, use
   `ConfigureBasicAttackComboSectionsFromHits` to place named section boundaries
   between chronological hit notifies and reindex those notifies.
4. Use `ConfigureBasicAttackComboWindows` to add missing window states from each
   hit notify to the padded end of its section.
5. Inspect the result and save the montage with `AssetTools.save_assets`.

## Enemy melee montage workflow

1. Duplicate a template montage under `/Game/Work/LMK/`.
2. When the desired source is an Animation Sequence rather than a Montage, call
   `ConfigureMontageAnimationTrack` with the required Slot and sequence list.
3. Use `InspectAnimationBoneMotion` with the actual mesh and candidate limb bones,
   then preview-confirm the reported striking hand and contact interval.
4. Use `InspectEnemyMeleeTraceMontage` to inspect the existing notify layout.
5. Use `ConfigureEnemyMeleeTraceWindow` for a one-hit montage, or
   `ConfigureEnemyMeleeTraceWindows` for a multi-hit montage. Supply the
   preview-confirmed contact intervals and unique zero-based strike indices.
   Alternating-hand attacks can override the socket pair per strike.
   The mutation removes `SaveAttack` and `ResetCombo` and replaces only the
   project-owned `AIRE Enemy Melee Trace Window` states.
6. For a configured gap closer, use `ConfigureEnemyAttackMovementWindow` to
   place its movement interval independently from the contact interval.
7. Use `InspectEnemyAttackTempoMontage`, then
   `ConfigureEnemyAttackTempoWindows` to add a slow anticipation phase followed
   by a short fast strike phase. Do not overlap a tempo window with movement.
8. Inspect the result and save only the edited montage with `AssetTools.save_assets`.

## Control Rig hierarchy workflow

1. Duplicate the source Control Rig under `/Game/Work/LMK/` and assign the target
   Skeletal Mesh as its preview mesh.
2. Call `SyncControlRigBoneHierarchy` with the duplicated Control Rig Blueprint
   and target Skeletal Mesh.
3. Require `DiscrepancyCountAfter` to be zero before compiling the dependent
   Animation Blueprint.
4. Compile and save the Control Rig explicitly, then compile and save the
   dependent Animation Blueprint.

## Frank-to-MAKO retarget workflow

1. Call `CreateOrUpdateFrankToMakoRetargetSetup` with the Frank Dual source mesh
   and `SM_MAKO`. The created assets remain dirty until explicitly saved.
2. Call `InspectFrankToMakoRetargetSetup` and verify the intended target-pose
   offsets on `pelvis`, `spine_01`, `spine_05`, `neck_01`, and `head` before exporting.
3. Retarget only Idle and one movement clip first with
   `RetargetFrankAnimationsToMako`; inspect the generated `Fixed` assets.
4. If a small target-pose correction is still required, use
   `SetMakoTargetRetargetPose`, export a new prefixed trial, and compare again.
5. Use `AutoAlignMakoTargetRetargetPoseBones` to compare Direction, Mesh,
   Local Rotation Axes, or Global Rotation Axes alignment on selected target
   bones without saving the trial.
6. Save the setup and accepted animation assets explicitly after visual review.
7. Call `CreateOrUpdateMakoWeaponSockets` when the Frank weapon-handle reference
   transforms should seed MAKO `weapon_l` and `weapon_r`; save `SM_MAKO` only
   after a separate weapon mesh has been preview-attached and visually checked.

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

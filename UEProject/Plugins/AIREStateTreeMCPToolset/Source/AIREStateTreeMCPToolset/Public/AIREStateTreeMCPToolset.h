#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "AIREStateTreeMCPToolset.generated.h"

class UStateTree;
class UStateTreeState;

UENUM(BlueprintType)
enum class EAIREStateTreeEditableStateType : uint8
{
	State,
	Group
};

UENUM(BlueprintType)
enum class EAIREStateTreeNodeKind : uint8
{
	Evaluator,
	GlobalTask,
	StateTask,
	EnterCondition,
	TransitionCondition
};

UENUM(BlueprintType)
enum class EAIREStateTreeNodeDataTarget : uint8
{
	Node,
	InstanceData,
	ExecutionRuntimeData
};

UENUM(BlueprintType)
enum class EAIREStateTreeTransitionTrigger : uint8
{
	OnTick,
	OnStateCompleted,
	OnStateSucceeded,
	OnStateFailed
};

UENUM(BlueprintType)
enum class EAIREStateTreeTransitionPriority : uint8
{
	Low,
	Normal,
	Medium,
	High,
	Critical
};

USTRUCT(BlueprintType)
struct FAIREStateTreeMutationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|StateTree")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|StateTree")
	FString Message;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|StateTree")
	TObjectPtr<UStateTreeState> State = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|StateTree")
	FGuid NodeID;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|StateTree")
	FGuid TransitionID;
};

USTRUCT(BlueprintType)
struct FAIREStateTreeNodeTypeInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|StateTree")
	FString StructPath;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|StateTree")
	FString DisplayName;
};

USTRUCT(BlueprintType)
struct FAIREStateTreeNodePropertyInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|StateTree")
	FName PropertyName;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|StateTree")
	FString PropertyType;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|StateTree")
	FString ExportedValue;
};

USTRUCT(BlueprintType)
struct FAIREStateTreeBindingInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|StateTree")
	FGuid SourceStructID;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|StateTree")
	FString SourcePath;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|StateTree")
	FGuid TargetStructID;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|StateTree")
	FString TargetPath;
};

USTRUCT(BlueprintType)
struct FAIREStateTreeBindableStructInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|StateTree")
	bool bFound = false;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|StateTree")
	FGuid StructID;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|StateTree")
	FName DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|StateTree")
	FString StructPath;
};

USTRUCT(BlueprintType)
struct FAIREStateTreeCompileResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|StateTree")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "AIRE|StateTree")
	TArray<FString> Messages;
};

/**
 * Project-scoped StateTree editing tools exposed through UE 5.8 ToolsetRegistry.
 * All mutation calls reject assets outside /Game/Work/LMK/ and leave saving explicit.
 */
UCLASS(BlueprintType)
class AIRESTATETREEMCPTOOLSET_API UAIREStateTreeMCPToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/** Lists exact loaded UScriptStruct paths accepted by node creation tools. */
	UFUNCTION(meta = (AICallable), Category = "AIRE|StateTree|Query")
	static TArray<FAIREStateTreeNodeTypeInfo> ListNodeTypes(EAIREStateTreeNodeKind NodeKind, const FString& NameFilter = TEXT(""));

	/** Lists direct editable properties and export-text values for one node data area. */
	UFUNCTION(meta = (AICallable), Category = "AIRE|StateTree|Query")
	static TArray<FAIREStateTreeNodePropertyInfo> ListNodeProperties(
		UStateTree* StateTree,
		FGuid NodeID,
		EAIREStateTreeNodeDataTarget DataTarget);

	/** Returns all editor property bindings, including source and target struct IDs. */
	UFUNCTION(meta = (AICallable), Category = "AIRE|StateTree|Query")
	static TArray<FAIREStateTreeBindingInfo> GetPropertyBindings(UStateTree* StateTree);

	/** Finds a schema context by exact UObject type path and optional display-name hint. */
	UFUNCTION(meta = (AICallable), Category = "AIRE|StateTree|Query")
	static FAIREStateTreeBindableStructInfo FindContextData(
		UStateTree* StateTree,
		const FString& ObjectTypePath,
		const FString& ObjectNameHint = TEXT(""));

	/** Adds a retry-safe named root or child state and optionally inserts it at a sibling index. */
	UFUNCTION(meta = (AICallable), Category = "AIRE|StateTree|State")
	static FAIREStateTreeMutationResult AddState(
		UStateTree* StateTree,
		UStateTreeState* ParentState,
		FName StateName,
		EAIREStateTreeEditableStateType StateType,
		int32 InsertIndex = -1);

	/** Reparents or reorders a state. A null NewParentState moves it to the root. */
	UFUNCTION(meta = (AICallable), Category = "AIRE|StateTree|State")
	static FAIREStateTreeMutationResult MoveState(
		UStateTree* StateTree,
		UStateTreeState* State,
		UStateTreeState* NewParentState,
		int32 InsertIndex = -1);

	/** Detaches a state and its children from the tree. The operation supports Editor Undo. */
	UFUNCTION(meta = (AICallable), Category = "AIRE|StateTree|State")
	static FAIREStateTreeMutationResult RemoveState(UStateTree* StateTree, UStateTreeState* State);

	/** Adds an evaluator using an exact struct path returned by ListNodeTypes. */
	UFUNCTION(meta = (AICallable), Category = "AIRE|StateTree|Node")
	static FAIREStateTreeMutationResult AddEvaluator(
		UStateTree* StateTree,
		const FString& NodeStructPath,
		FName NodeName = NAME_None);

	/** Adds a global task using an exact struct path returned by ListNodeTypes. */
	UFUNCTION(meta = (AICallable), Category = "AIRE|StateTree|Node")
	static FAIREStateTreeMutationResult AddGlobalTask(
		UStateTree* StateTree,
		const FString& NodeStructPath,
		FName NodeName = NAME_None);

	/** Adds a task to a state using an exact struct path returned by ListNodeTypes. */
	UFUNCTION(meta = (AICallable), Category = "AIRE|StateTree|Node")
	static FAIREStateTreeMutationResult AddStateTask(
		UStateTree* StateTree,
		UStateTreeState* State,
		const FString& NodeStructPath,
		FName NodeName = NAME_None);

	/** Adds an enter condition to a state. */
	UFUNCTION(meta = (AICallable), Category = "AIRE|StateTree|Node")
	static FAIREStateTreeMutationResult AddEnterCondition(
		UStateTree* StateTree,
		UStateTreeState* State,
		const FString& NodeStructPath,
		FName NodeName = NAME_None);

	/** Adds a condition to a transition identified by its GUID. */
	UFUNCTION(meta = (AICallable), Category = "AIRE|StateTree|Node")
	static FAIREStateTreeMutationResult AddTransitionCondition(
		UStateTree* StateTree,
		UStateTreeState* State,
		FGuid TransitionID,
		const FString& NodeStructPath,
		FName NodeName = NAME_None);

	/** Removes an evaluator, task, enter condition, or transition condition by node GUID. */
	UFUNCTION(meta = (AICallable), Category = "AIRE|StateTree|Node")
	static FAIREStateTreeMutationResult RemoveNode(UStateTree* StateTree, FGuid NodeID);

	/** Sets one direct node property using Unreal export-text syntax; inspect it first. */
	UFUNCTION(meta = (AICallable), Category = "AIRE|StateTree|Node")
	static FAIREStateTreeMutationResult SetNodePropertyText(
		UStateTree* StateTree,
		FGuid NodeID,
		EAIREStateTreeNodeDataTarget DataTarget,
		FName PropertyName,
		const FString& ExportedValue);

	/** Adds a retry-safe goto transition or updates the matching transition priority. */
	UFUNCTION(meta = (AICallable), Category = "AIRE|StateTree|Transition")
	static FAIREStateTreeMutationResult AddGotoTransition(
		UStateTree* StateTree,
		UStateTreeState* SourceState,
		UStateTreeState* TargetState,
		EAIREStateTreeTransitionTrigger Trigger,
		EAIREStateTreeTransitionPriority Priority);

	/** Removes a transition from its source state by transition GUID. */
	UFUNCTION(meta = (AICallable), Category = "AIRE|StateTree|Transition")
	static FAIREStateTreeMutationResult RemoveTransition(
		UStateTree* StateTree,
		UStateTreeState* SourceState,
		FGuid TransitionID);

	/** Adds or replaces a property binding after resolving both paths against live data. */
	UFUNCTION(meta = (AICallable), Category = "AIRE|StateTree|Binding")
	static FAIREStateTreeMutationResult AddPropertyBinding(
		UStateTree* StateTree,
		FGuid SourceStructID,
		const FString& SourcePath,
		FGuid TargetStructID,
		const FString& TargetPath);

	/** Removes the binding whose target matches the supplied struct GUID and path. */
	UFUNCTION(meta = (AICallable), Category = "AIRE|StateTree|Binding")
	static FAIREStateTreeMutationResult RemovePropertyBinding(
		UStateTree* StateTree,
		FGuid TargetStructID,
		const FString& TargetPath);

	/** Validates and compiles without saving, returning compiler messages. */
	UFUNCTION(meta = (AICallable), Category = "AIRE|StateTree|Lifecycle")
	static FAIREStateTreeCompileResult ValidateAndCompile(UStateTree* StateTree);

	/** Saves a compiled StateTree; rejects assets that still require compilation. */
	UFUNCTION(meta = (AICallable), Category = "AIRE|StateTree|Lifecycle")
	static FAIREStateTreeMutationResult SaveStateTree(UStateTree* StateTree);
};

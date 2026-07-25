#include "AIREStateTreeMCPToolset.h"

#include "Editor.h"
#include "ScopedTransaction.h"
#include "PropertyBindingPath.h"
#include "StateTree.h"
#include "StateTreeCompilerLog.h"
#include "StateTreeConditionBase.h"
#include "StateTreeEditorData.h"
#include "StateTreeEditorNode.h"
#include "StateTreeEditingSubsystem.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreePropertyBindings.h"
#include "StateTreeState.h"
#include "StateTreeTaskBase.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "UObject/UObjectIterator.h"

#define LOCTEXT_NAMESPACE "AIREStateTreeMCPToolset"

namespace
{
	constexpr TCHAR AllowedAssetRoot[] = TEXT("/Game/Work/LMK/");
	constexpr int32 MaximumPropertyTextLength = 4096;

	struct FAIRELocatedStateTreeNode
	{
		FStateTreeEditorNode* Node = nullptr;
		TArray<FStateTreeEditorNode>* Container = nullptr;
		UObject* Owner = nullptr;
	};

	FAIREStateTreeMutationResult MakeFailure(const FString& Message)
	{
		FAIREStateTreeMutationResult Result;
		Result.Message = Message;
		return Result;
	}

	FAIREStateTreeMutationResult MakeSuccess(const FString& Message)
	{
		FAIREStateTreeMutationResult Result;
		Result.bSuccess = true;
		Result.Message = Message;
		return Result;
	}

	bool TryGetEditableData(
		UStateTree* StateTree,
		UStateTreeEditorData*& OutEditorData,
		FString& OutError)
	{
		OutEditorData = nullptr;
		if (!IsValid(StateTree))
		{
			OutError = TEXT("StateTree is null or invalid.");
			return false;
		}

		if (!StateTree->GetPathName().StartsWith(AllowedAssetRoot))
		{
			OutError = FString::Printf(
				TEXT("Asset '%s' is outside the allowed root '%s'."),
				*StateTree->GetPathName(),
				AllowedAssetRoot);
			return false;
		}

		// GetEditorData is a non-exported MinimalAPI Blueprint wrapper in UE 5.8.
		// The editor-only source object is public on UStateTree, so external editor modules cast it directly.
		OutEditorData = Cast<UStateTreeEditorData>(StateTree->EditorData.Get());
		if (!IsValid(OutEditorData))
		{
			OutError = TEXT("StateTree has no editable editor data.");
			return false;
		}

		return true;
	}

	bool DoesStateBelongToTree(
		const UStateTree* StateTree,
		const UStateTreeState* State)
	{
		return IsValid(StateTree)
			&& IsValid(State)
			&& State->GetTypedOuter<UStateTree>() == StateTree;
	}

	void FinalizeMutation(UStateTree* StateTree, UStateTreeEditorData* EditorData)
	{
		EditorData->ReparentStates();
		EditorData->FixDuplicateIDs();
		EditorData->UpdateBindings();
		EditorData->RemoveInvalidBindings();
		UStateTreeEditingSubsystem::ValidateStateTree(StateTree);
		UStateTreeEditingSubsystem::MarkAsModified(StateTree);
		StateTree->MarkPackageDirty();
	}

	TArray<TObjectPtr<UStateTreeState>>* GetStateContainer(
		UStateTreeEditorData* EditorData,
		UStateTreeState* ParentState)
	{
		return IsValid(ParentState) ? &ParentState->Children : &EditorData->SubTrees;
	}

	UStateTreeState* FindSiblingByName(
		const TArray<TObjectPtr<UStateTreeState>>& States,
		const FName StateName)
	{
		for (UStateTreeState* State : States)
		{
			if (IsValid(State) && State->Name == StateName)
			{
				return State;
			}
		}
		return nullptr;
	}

	void InsertStateAt(
		TArray<TObjectPtr<UStateTreeState>>& States,
		UStateTreeState* State,
		const int32 InsertIndex)
	{
		States.Remove(State);
		if (InsertIndex >= 0 && InsertIndex < States.Num())
		{
			States.Insert(State, InsertIndex);
		}
		else
		{
			States.Add(State);
		}
	}

	bool IsDescendantOf(const UStateTreeState* Candidate, const UStateTreeState* PossibleAncestor)
	{
		for (const UStateTreeState* Current = Candidate; IsValid(Current); Current = Current->Parent)
		{
			if (Current == PossibleAncestor)
			{
				return true;
			}
		}
		return false;
	}

	bool FindNodeInState(
		UStateTreeState* State,
		const FGuid NodeID,
		FAIRELocatedStateTreeNode& OutLocatedNode)
	{
		if (!IsValid(State))
		{
			return false;
		}

		TArray<TArray<FStateTreeEditorNode>*> Containers = {
			&State->EnterConditions,
			&State->Tasks,
			&State->Considerations
		};
		for (TArray<FStateTreeEditorNode>* Container : Containers)
		{
			if (FStateTreeEditorNode* Node = Container->FindByPredicate(
				[NodeID](const FStateTreeEditorNode& Candidate)
				{
					return Candidate.ID == NodeID;
				}))
			{
				OutLocatedNode.Node = Node;
				OutLocatedNode.Container = Container;
				OutLocatedNode.Owner = State;
				return true;
			}
		}

		for (FStateTreeTransition& Transition : State->Transitions)
		{
			if (FStateTreeEditorNode* Node = Transition.Conditions.FindByPredicate(
				[NodeID](const FStateTreeEditorNode& Candidate)
				{
					return Candidate.ID == NodeID;
				}))
			{
				OutLocatedNode.Node = Node;
				OutLocatedNode.Container = &Transition.Conditions;
				OutLocatedNode.Owner = State;
				return true;
			}
		}

		for (UStateTreeState* Child : State->Children)
		{
			if (FindNodeInState(Child, NodeID, OutLocatedNode))
			{
				return true;
			}
		}

		return false;
	}

	bool FindNode(
		UStateTreeEditorData* EditorData,
		const FGuid NodeID,
		FAIRELocatedStateTreeNode& OutLocatedNode)
	{
		TArray<TArray<FStateTreeEditorNode>*> GlobalContainers = {
			&EditorData->Evaluators,
			&EditorData->GlobalTasks
		};
		for (TArray<FStateTreeEditorNode>* Container : GlobalContainers)
		{
			if (FStateTreeEditorNode* Node = Container->FindByPredicate(
				[NodeID](const FStateTreeEditorNode& Candidate)
				{
					return Candidate.ID == NodeID;
				}))
			{
				OutLocatedNode.Node = Node;
				OutLocatedNode.Container = Container;
				OutLocatedNode.Owner = EditorData;
				return true;
			}
		}

		for (UStateTreeState* RootState : EditorData->SubTrees)
		{
			if (FindNodeInState(RootState, NodeID, OutLocatedNode))
			{
				return true;
			}
		}

		return false;
	}

	const UScriptStruct* GetRequiredBaseStruct(const EAIREStateTreeNodeKind NodeKind)
	{
		switch (NodeKind)
		{
		case EAIREStateTreeNodeKind::Evaluator:
			return FStateTreeEvaluatorBase::StaticStruct();
		case EAIREStateTreeNodeKind::GlobalTask:
		case EAIREStateTreeNodeKind::StateTask:
			return FStateTreeTaskBase::StaticStruct();
		case EAIREStateTreeNodeKind::EnterCondition:
		case EAIREStateTreeNodeKind::TransitionCondition:
			return FStateTreeConditionBase::StaticStruct();
		default:
			return nullptr;
		}
	}

	UScriptStruct* FindNodeStruct(
		const FString& NodeStructPath,
		const EAIREStateTreeNodeKind NodeKind,
		FString& OutError)
	{
		UScriptStruct* NodeStruct = FindObject<UScriptStruct>(nullptr, *NodeStructPath);
		if (!IsValid(NodeStruct))
		{
			OutError = FString::Printf(
				TEXT("Node struct '%s' is not loaded or does not exist."),
				*NodeStructPath);
			return nullptr;
		}

		const UScriptStruct* RequiredBaseStruct = GetRequiredBaseStruct(NodeKind);
		if (!IsValid(RequiredBaseStruct) || !NodeStruct->IsChildOf(RequiredBaseStruct))
		{
			OutError = FString::Printf(
				TEXT("Node struct '%s' is incompatible with node kind '%s'."),
				*NodeStructPath,
				*StaticEnum<EAIREStateTreeNodeKind>()->GetNameStringByValue(static_cast<int64>(NodeKind)));
			return nullptr;
		}

		return NodeStruct;
	}

	FAIREStateTreeMutationResult AddNodeToContainer(
		UStateTree* StateTree,
		UStateTreeEditorData* EditorData,
		TArray<FStateTreeEditorNode>& Container,
		UObject* NodeOuter,
		const EAIREStateTreeNodeKind NodeKind,
		const FString& NodeStructPath,
		const FName NodeName)
	{
		FString Error;
		UScriptStruct* NodeStruct = FindNodeStruct(NodeStructPath, NodeKind, Error);
		if (!IsValid(NodeStruct))
		{
			return MakeFailure(Error);
		}

		if (!NodeName.IsNone())
		{
			for (FStateTreeEditorNode& ExistingNode : Container)
			{
				if (ExistingNode.GetName() == NodeName && ExistingNode.Node.GetScriptStruct() == NodeStruct)
				{
					FAIREStateTreeMutationResult Result = MakeSuccess(TEXT("Matching node already exists; no mutation was applied."));
					Result.NodeID = ExistingNode.ID;
					return Result;
				}
			}
		}

		const FScopedTransaction Transaction(LOCTEXT("AddStateTreeNode", "Add AIRE StateTree Node"));
		StateTree->Modify();
		EditorData->Modify();
		NodeOuter->Modify();

		FStateTreeEditorNode& NewNode = Container.AddDefaulted_GetRef();
		NewNode.InitializeAs(NodeOuter, NodeStruct);
		if (!NodeName.IsNone())
		{
			NewNode.SetNodeName(NodeName);
		}

		const FGuid NewNodeID = NewNode.ID;
		FinalizeMutation(StateTree, EditorData);
		FAIREStateTreeMutationResult Result = MakeSuccess(TEXT("StateTree node added. Compile and save explicitly after all edits."));
		Result.NodeID = NewNodeID;
		return Result;
	}

	bool GetNodeData(
		FStateTreeEditorNode& Node,
		const EAIREStateTreeNodeDataTarget DataTarget,
		const UStruct*& OutStruct,
		void*& OutMemory,
		UObject*& OutOwnerObject)
	{
		OutStruct = nullptr;
		OutMemory = nullptr;
		OutOwnerObject = nullptr;

		switch (DataTarget)
		{
		case EAIREStateTreeNodeDataTarget::Node:
			OutStruct = Node.Node.GetScriptStruct();
			OutMemory = Node.Node.GetMutableMemory();
			break;
		case EAIREStateTreeNodeDataTarget::InstanceData:
			if (IsValid(Node.InstanceObject))
			{
				OutStruct = Node.InstanceObject->GetClass();
				OutMemory = Node.InstanceObject;
				OutOwnerObject = Node.InstanceObject;
			}
			else
			{
				OutStruct = Node.Instance.GetScriptStruct();
				OutMemory = Node.Instance.GetMutableMemory();
			}
			break;
		case EAIREStateTreeNodeDataTarget::ExecutionRuntimeData:
			if (IsValid(Node.ExecutionRuntimeDataObject))
			{
				OutStruct = Node.ExecutionRuntimeDataObject->GetClass();
				OutMemory = Node.ExecutionRuntimeDataObject;
				OutOwnerObject = Node.ExecutionRuntimeDataObject;
			}
			else
			{
				OutStruct = Node.ExecutionRuntimeData.GetScriptStruct();
				OutMemory = Node.ExecutionRuntimeData.GetMutableMemory();
			}
			break;
		default:
			break;
		}

		return IsValid(OutStruct) && OutMemory != nullptr;
	}

	EStateTreeTransitionTrigger ConvertTrigger(const EAIREStateTreeTransitionTrigger Trigger)
	{
		switch (Trigger)
		{
		case EAIREStateTreeTransitionTrigger::OnTick:
			return EStateTreeTransitionTrigger::OnTick;
		case EAIREStateTreeTransitionTrigger::OnStateCompleted:
			return EStateTreeTransitionTrigger::OnStateCompleted;
		case EAIREStateTreeTransitionTrigger::OnStateSucceeded:
			return EStateTreeTransitionTrigger::OnStateSucceeded;
		case EAIREStateTreeTransitionTrigger::OnStateFailed:
			return EStateTreeTransitionTrigger::OnStateFailed;
		default:
			return EStateTreeTransitionTrigger::None;
		}
	}

	EStateTreeTransitionPriority ConvertPriority(const EAIREStateTreeTransitionPriority Priority)
	{
		switch (Priority)
		{
		case EAIREStateTreeTransitionPriority::Low:
			return EStateTreeTransitionPriority::Low;
		case EAIREStateTreeTransitionPriority::Normal:
			return EStateTreeTransitionPriority::Normal;
		case EAIREStateTreeTransitionPriority::Medium:
			return EStateTreeTransitionPriority::Medium;
		case EAIREStateTreeTransitionPriority::High:
			return EStateTreeTransitionPriority::High;
		case EAIREStateTreeTransitionPriority::Critical:
			return EStateTreeTransitionPriority::Critical;
		default:
			return EStateTreeTransitionPriority::Normal;
		}
	}
}

TArray<FAIREStateTreeNodeTypeInfo> UAIREStateTreeMCPToolset::ListNodeTypes(
	const EAIREStateTreeNodeKind NodeKind,
	const FString& NameFilter)
{
	TArray<FAIREStateTreeNodeTypeInfo> Results;
	const UScriptStruct* RequiredBaseStruct = GetRequiredBaseStruct(NodeKind);
	if (!IsValid(RequiredBaseStruct))
	{
		return Results;
	}

	for (TObjectIterator<UScriptStruct> It; It; ++It)
	{
		UScriptStruct* Struct = *It;
		if (Struct == RequiredBaseStruct || !Struct->IsChildOf(RequiredBaseStruct))
		{
			continue;
		}

		const FString DisplayName = Struct->GetDisplayNameText().ToString();
		const bool bMatchesFilter = NameFilter.IsEmpty()
			|| DisplayName.Contains(NameFilter, ESearchCase::IgnoreCase)
			|| Struct->GetName().Contains(NameFilter, ESearchCase::IgnoreCase);
		if (!bMatchesFilter)
		{
			continue;
		}

		FAIREStateTreeNodeTypeInfo& Info = Results.AddDefaulted_GetRef();
		Info.StructPath = Struct->GetPathName();
		Info.DisplayName = DisplayName;
	}

	Results.Sort([](const FAIREStateTreeNodeTypeInfo& Left, const FAIREStateTreeNodeTypeInfo& Right)
	{
		return Left.DisplayName < Right.DisplayName;
	});
	return Results;
}

TArray<FAIREStateTreeNodePropertyInfo> UAIREStateTreeMCPToolset::ListNodeProperties(
	UStateTree* StateTree,
	const FGuid NodeID,
	const EAIREStateTreeNodeDataTarget DataTarget)
{
	TArray<FAIREStateTreeNodePropertyInfo> Results;
	UStateTreeEditorData* EditorData = nullptr;
	FString Error;
	if (!TryGetEditableData(StateTree, EditorData, Error))
	{
		return Results;
	}

	FAIRELocatedStateTreeNode LocatedNode;
	if (!FindNode(EditorData, NodeID, LocatedNode))
	{
		return Results;
	}

	const UStruct* DataStruct = nullptr;
	void* DataMemory = nullptr;
	UObject* OwnerObject = nullptr;
	if (!GetNodeData(*LocatedNode.Node, DataTarget, DataStruct, DataMemory, OwnerObject))
	{
		return Results;
	}

	for (TFieldIterator<FProperty> It(DataStruct); It; ++It)
	{
		const FProperty* Property = *It;
		if (Property->HasAnyPropertyFlags(CPF_Transient))
		{
			continue;
		}

		FAIREStateTreeNodePropertyInfo& Info = Results.AddDefaulted_GetRef();
		Info.PropertyName = Property->GetFName();
		Info.PropertyType = Property->GetCPPType();
		Property->ExportTextItem_Direct(
			Info.ExportedValue,
			Property->ContainerPtrToValuePtr<void>(DataMemory),
			nullptr,
			OwnerObject,
			PPF_None);
	}

	return Results;
}

TArray<FAIREStateTreeBindingInfo> UAIREStateTreeMCPToolset::GetPropertyBindings(UStateTree* StateTree)
{
	TArray<FAIREStateTreeBindingInfo> Results;
	UStateTreeEditorData* EditorData = nullptr;
	FString Error;
	if (!TryGetEditableData(StateTree, EditorData, Error))
	{
		return Results;
	}

	for (const FStateTreePropertyPathBinding& Binding : EditorData->GetPropertyEditorBindings()->GetBindings())
	{
		FAIREStateTreeBindingInfo& Info = Results.AddDefaulted_GetRef();
		Info.SourceStructID = Binding.GetSourcePath().GetStructID();
		Info.SourcePath = Binding.GetSourcePath().ToString();
		Info.TargetStructID = Binding.GetTargetPath().GetStructID();
		Info.TargetPath = Binding.GetTargetPath().ToString();
	}
	return Results;
}

FAIREStateTreeBindableStructInfo UAIREStateTreeMCPToolset::FindContextData(
	UStateTree* StateTree,
	const FString& ObjectTypePath,
	const FString& ObjectNameHint)
{
	FAIREStateTreeBindableStructInfo Result;
	UStateTreeEditorData* EditorData = nullptr;
	FString Error;
	if (!TryGetEditableData(StateTree, EditorData, Error))
	{
		return Result;
	}

	const UStruct* ObjectType = FindObject<UStruct>(nullptr, *ObjectTypePath);
	if (!IsValid(ObjectType))
	{
		return Result;
	}

	const FStateTreeBindableStructDesc Context = EditorData->FindContextData(ObjectType, ObjectNameHint);
	if (!Context.IsValid())
	{
		return Result;
	}

	Result.bFound = true;
	Result.StructID = Context.ID;
	Result.DisplayName = Context.Name;
	Result.StructPath = Context.Struct->GetPathName();
	return Result;
}

FAIREStateTreeMutationResult UAIREStateTreeMCPToolset::AddState(
	UStateTree* StateTree,
	UStateTreeState* ParentState,
	const FName StateName,
	const EAIREStateTreeEditableStateType StateType,
	const int32 InsertIndex)
{
	UStateTreeEditorData* EditorData = nullptr;
	FString Error;
	if (!TryGetEditableData(StateTree, EditorData, Error))
	{
		return MakeFailure(Error);
	}
	if (StateName.IsNone())
	{
		return MakeFailure(TEXT("StateName must not be empty."));
	}
	if (IsValid(ParentState) && !DoesStateBelongToTree(StateTree, ParentState))
	{
		return MakeFailure(TEXT("ParentState does not belong to the supplied StateTree."));
	}

	const EStateTreeStateType EngineStateType = StateType == EAIREStateTreeEditableStateType::Group
		? EStateTreeStateType::Group
		: EStateTreeStateType::State;
	TArray<TObjectPtr<UStateTreeState>>* States = GetStateContainer(EditorData, ParentState);
	if (UStateTreeState* ExistingState = FindSiblingByName(*States, StateName))
	{
		if (ExistingState->Type != EngineStateType)
		{
			return MakeFailure(TEXT("A sibling with the same name exists but has a different state type."));
		}
		FAIREStateTreeMutationResult Result = MakeSuccess(TEXT("Matching sibling state already exists; no mutation was applied."));
		Result.State = ExistingState;
		return Result;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddStateTreeState", "Add AIRE StateTree State"));
	StateTree->Modify();
	EditorData->Modify();
	if (IsValid(ParentState))
	{
		ParentState->Modify();
	}

	UStateTreeState* NewState = NewObject<UStateTreeState>(EditorData, NAME_None, RF_Transactional);
	NewState->Name = StateName;
	NewState->Type = EngineStateType;
	NewState->Parent = ParentState;
	InsertStateAt(*States, NewState, InsertIndex);
	FinalizeMutation(StateTree, EditorData);

	FAIREStateTreeMutationResult Result = MakeSuccess(TEXT("State added. Compile and save explicitly after all edits."));
	Result.State = NewState;
	return Result;
}

FAIREStateTreeMutationResult UAIREStateTreeMCPToolset::MoveState(
	UStateTree* StateTree,
	UStateTreeState* State,
	UStateTreeState* NewParentState,
	const int32 InsertIndex)
{
	UStateTreeEditorData* EditorData = nullptr;
	FString Error;
	if (!TryGetEditableData(StateTree, EditorData, Error))
	{
		return MakeFailure(Error);
	}
	if (!DoesStateBelongToTree(StateTree, State))
	{
		return MakeFailure(TEXT("State does not belong to the supplied StateTree."));
	}
	if (IsValid(NewParentState) && !DoesStateBelongToTree(StateTree, NewParentState))
	{
		return MakeFailure(TEXT("NewParentState does not belong to the supplied StateTree."));
	}
	if (State == NewParentState || IsDescendantOf(NewParentState, State))
	{
		return MakeFailure(TEXT("Moving the state would create a hierarchy cycle."));
	}

	TArray<TObjectPtr<UStateTreeState>>* Destination = GetStateContainer(EditorData, NewParentState);
	if (UStateTreeState* NameCollision = FindSiblingByName(*Destination, State->Name);
		IsValid(NameCollision) && NameCollision != State)
	{
		return MakeFailure(TEXT("The destination already has a child with the same state name."));
	}

	const FScopedTransaction Transaction(LOCTEXT("MoveStateTreeState", "Move AIRE StateTree State"));
	StateTree->Modify();
	EditorData->Modify();
	State->Modify();
	if (IsValid(State->Parent))
	{
		State->Parent->Modify();
	}
	if (IsValid(NewParentState))
	{
		NewParentState->Modify();
	}

	TArray<TObjectPtr<UStateTreeState>>* Source = GetStateContainer(EditorData, State->Parent);
	Source->Remove(State);
	InsertStateAt(*Destination, State, InsertIndex);
	State->Parent = NewParentState;
	FinalizeMutation(StateTree, EditorData);

	FAIREStateTreeMutationResult Result = MakeSuccess(TEXT("State moved. Compile and save explicitly after all edits."));
	Result.State = State;
	return Result;
}

FAIREStateTreeMutationResult UAIREStateTreeMCPToolset::RemoveState(
	UStateTree* StateTree,
	UStateTreeState* State)
{
	UStateTreeEditorData* EditorData = nullptr;
	FString Error;
	if (!TryGetEditableData(StateTree, EditorData, Error))
	{
		return MakeFailure(Error);
	}
	if (!DoesStateBelongToTree(StateTree, State))
	{
		return MakeFailure(TEXT("State does not belong to the supplied StateTree."));
	}

	const FScopedTransaction Transaction(LOCTEXT("RemoveStateTreeState", "Remove AIRE StateTree State"));
	StateTree->Modify();
	EditorData->Modify();
	State->Modify();
	if (IsValid(State->Parent))
	{
		State->Parent->Modify();
	}

	TArray<TObjectPtr<UStateTreeState>>* Source = GetStateContainer(EditorData, State->Parent);
	Source->Remove(State);
	State->Parent = nullptr;
	FinalizeMutation(StateTree, EditorData);
	return MakeSuccess(TEXT("State and its child hierarchy were detached. Compile and save explicitly after all edits."));
}

FAIREStateTreeMutationResult UAIREStateTreeMCPToolset::AddEvaluator(
	UStateTree* StateTree,
	const FString& NodeStructPath,
	const FName NodeName)
{
	UStateTreeEditorData* EditorData = nullptr;
	FString Error;
	if (!TryGetEditableData(StateTree, EditorData, Error))
	{
		return MakeFailure(Error);
	}
	return AddNodeToContainer(
		StateTree,
		EditorData,
		EditorData->Evaluators,
		EditorData,
		EAIREStateTreeNodeKind::Evaluator,
		NodeStructPath,
		NodeName);
}

FAIREStateTreeMutationResult UAIREStateTreeMCPToolset::AddGlobalTask(
	UStateTree* StateTree,
	const FString& NodeStructPath,
	const FName NodeName)
{
	UStateTreeEditorData* EditorData = nullptr;
	FString Error;
	if (!TryGetEditableData(StateTree, EditorData, Error))
	{
		return MakeFailure(Error);
	}
	return AddNodeToContainer(
		StateTree,
		EditorData,
		EditorData->GlobalTasks,
		EditorData,
		EAIREStateTreeNodeKind::GlobalTask,
		NodeStructPath,
		NodeName);
}

FAIREStateTreeMutationResult UAIREStateTreeMCPToolset::AddStateTask(
	UStateTree* StateTree,
	UStateTreeState* State,
	const FString& NodeStructPath,
	const FName NodeName)
{
	UStateTreeEditorData* EditorData = nullptr;
	FString Error;
	if (!TryGetEditableData(StateTree, EditorData, Error))
	{
		return MakeFailure(Error);
	}
	if (!DoesStateBelongToTree(StateTree, State))
	{
		return MakeFailure(TEXT("State does not belong to the supplied StateTree."));
	}
	return AddNodeToContainer(
		StateTree,
		EditorData,
		State->Tasks,
		State,
		EAIREStateTreeNodeKind::StateTask,
		NodeStructPath,
		NodeName);
}

FAIREStateTreeMutationResult UAIREStateTreeMCPToolset::AddEnterCondition(
	UStateTree* StateTree,
	UStateTreeState* State,
	const FString& NodeStructPath,
	const FName NodeName)
{
	UStateTreeEditorData* EditorData = nullptr;
	FString Error;
	if (!TryGetEditableData(StateTree, EditorData, Error))
	{
		return MakeFailure(Error);
	}
	if (!DoesStateBelongToTree(StateTree, State))
	{
		return MakeFailure(TEXT("State does not belong to the supplied StateTree."));
	}
	return AddNodeToContainer(
		StateTree,
		EditorData,
		State->EnterConditions,
		State,
		EAIREStateTreeNodeKind::EnterCondition,
		NodeStructPath,
		NodeName);
}

FAIREStateTreeMutationResult UAIREStateTreeMCPToolset::AddTransitionCondition(
	UStateTree* StateTree,
	UStateTreeState* State,
	const FGuid TransitionID,
	const FString& NodeStructPath,
	const FName NodeName)
{
	UStateTreeEditorData* EditorData = nullptr;
	FString Error;
	if (!TryGetEditableData(StateTree, EditorData, Error))
	{
		return MakeFailure(Error);
	}
	if (!DoesStateBelongToTree(StateTree, State))
	{
		return MakeFailure(TEXT("State does not belong to the supplied StateTree."));
	}

	FStateTreeTransition* Transition = State->Transitions.FindByPredicate(
		[TransitionID](const FStateTreeTransition& Candidate)
		{
			return Candidate.ID == TransitionID;
		});
	if (Transition == nullptr)
	{
		return MakeFailure(TEXT("TransitionID was not found on the supplied state."));
	}

	return AddNodeToContainer(
		StateTree,
		EditorData,
		Transition->Conditions,
		State,
		EAIREStateTreeNodeKind::TransitionCondition,
		NodeStructPath,
		NodeName);
}

FAIREStateTreeMutationResult UAIREStateTreeMCPToolset::RemoveNode(
	UStateTree* StateTree,
	const FGuid NodeID)
{
	UStateTreeEditorData* EditorData = nullptr;
	FString Error;
	if (!TryGetEditableData(StateTree, EditorData, Error))
	{
		return MakeFailure(Error);
	}

	FAIRELocatedStateTreeNode LocatedNode;
	if (!FindNode(EditorData, NodeID, LocatedNode) || LocatedNode.Container == nullptr)
	{
		return MakeFailure(TEXT("NodeID was not found."));
	}

	const FScopedTransaction Transaction(LOCTEXT("RemoveStateTreeNode", "Remove AIRE StateTree Node"));
	StateTree->Modify();
	EditorData->Modify();
	if (IsValid(LocatedNode.Owner))
	{
		LocatedNode.Owner->Modify();
	}
	LocatedNode.Container->RemoveAll(
		[NodeID](const FStateTreeEditorNode& Candidate)
		{
			return Candidate.ID == NodeID;
		});
	FinalizeMutation(StateTree, EditorData);
	return MakeSuccess(TEXT("Node removed. Compile and save explicitly after all edits."));
}

FAIREStateTreeMutationResult UAIREStateTreeMCPToolset::SetNodePropertyText(
	UStateTree* StateTree,
	const FGuid NodeID,
	const EAIREStateTreeNodeDataTarget DataTarget,
	const FName PropertyName,
	const FString& ExportedValue)
{
	UStateTreeEditorData* EditorData = nullptr;
	FString Error;
	if (!TryGetEditableData(StateTree, EditorData, Error))
	{
		return MakeFailure(Error);
	}
	if (PropertyName.IsNone())
	{
		return MakeFailure(TEXT("PropertyName must not be empty."));
	}
	if (ExportedValue.Len() > MaximumPropertyTextLength)
	{
		return MakeFailure(TEXT("ExportedValue exceeds the maximum supported length."));
	}

	FAIRELocatedStateTreeNode LocatedNode;
	if (!FindNode(EditorData, NodeID, LocatedNode))
	{
		return MakeFailure(TEXT("NodeID was not found."));
	}

	const UStruct* DataStruct = nullptr;
	void* DataMemory = nullptr;
	UObject* OwnerObject = nullptr;
	if (!GetNodeData(*LocatedNode.Node, DataTarget, DataStruct, DataMemory, OwnerObject))
	{
		return MakeFailure(TEXT("The selected node data target is unavailable."));
	}

	FProperty* Property = DataStruct->FindPropertyByName(PropertyName);
	if (Property == nullptr || Property->HasAnyPropertyFlags(CPF_Transient))
	{
		return MakeFailure(TEXT("Property was not found or is transient and cannot be edited."));
	}

	FScopedTransaction Transaction(LOCTEXT("SetStateTreeNodeProperty", "Set AIRE StateTree Node Property"));
	StateTree->Modify();
	EditorData->Modify();
	if (IsValid(LocatedNode.Owner))
	{
		LocatedNode.Owner->Modify();
	}
	if (IsValid(OwnerObject))
	{
		OwnerObject->Modify();
	}

	void* ValueAddress = Property->ContainerPtrToValuePtr<void>(DataMemory);
	FString PreviousValue;
	Property->ExportTextItem_Direct(
		PreviousValue,
		ValueAddress,
		nullptr,
		OwnerObject,
		PPF_None);
	if (Property->ImportText_Direct(*ExportedValue, ValueAddress, OwnerObject, PPF_None) == nullptr)
	{
		Property->ImportText_Direct(*PreviousValue, ValueAddress, OwnerObject, PPF_None);
		Transaction.Cancel();
		return MakeFailure(FString::Printf(
			TEXT("Could not import value '%s' into property '%s'."),
			*ExportedValue,
			*PropertyName.ToString()));
	}

	FinalizeMutation(StateTree, EditorData);
	return MakeSuccess(TEXT("Node property updated. Compile and save explicitly after all edits."));
}

FAIREStateTreeMutationResult UAIREStateTreeMCPToolset::AddGotoTransition(
	UStateTree* StateTree,
	UStateTreeState* SourceState,
	UStateTreeState* TargetState,
	const EAIREStateTreeTransitionTrigger Trigger,
	const EAIREStateTreeTransitionPriority Priority)
{
	UStateTreeEditorData* EditorData = nullptr;
	FString Error;
	if (!TryGetEditableData(StateTree, EditorData, Error))
	{
		return MakeFailure(Error);
	}
	if (!DoesStateBelongToTree(StateTree, SourceState)
		|| !DoesStateBelongToTree(StateTree, TargetState))
	{
		return MakeFailure(TEXT("SourceState and TargetState must belong to the supplied StateTree."));
	}

	const EStateTreeTransitionTrigger EngineTrigger = ConvertTrigger(Trigger);
	const EStateTreeTransitionPriority EnginePriority = ConvertPriority(Priority);
	for (FStateTreeTransition& ExistingTransition : SourceState->Transitions)
	{
		if (ExistingTransition.Trigger == EngineTrigger
			&& ExistingTransition.State.ID == TargetState->ID)
		{
			const FScopedTransaction Transaction(LOCTEXT("UpdateStateTreeTransition", "Update AIRE StateTree Transition"));
			StateTree->Modify();
			EditorData->Modify();
			SourceState->Modify();
			ExistingTransition.Priority = EnginePriority;
			const FGuid ExistingTransitionID = ExistingTransition.ID;
			FinalizeMutation(StateTree, EditorData);

			FAIREStateTreeMutationResult Result = MakeSuccess(TEXT("Matching transition already existed; its priority was updated."));
			Result.TransitionID = ExistingTransitionID;
			return Result;
		}
	}

	const FScopedTransaction Transaction(LOCTEXT("AddStateTreeTransition", "Add AIRE StateTree Transition"));
	StateTree->Modify();
	EditorData->Modify();
	SourceState->Modify();
	FStateTreeTransition& NewTransition = SourceState->AddTransition(
		EngineTrigger,
		EStateTreeTransitionType::GotoState,
		TargetState);
	NewTransition.Priority = EnginePriority;
	const FGuid NewTransitionID = NewTransition.ID;
	FinalizeMutation(StateTree, EditorData);

	FAIREStateTreeMutationResult Result = MakeSuccess(TEXT("Goto transition added. Compile and save explicitly after all edits."));
	Result.TransitionID = NewTransitionID;
	return Result;
}

FAIREStateTreeMutationResult UAIREStateTreeMCPToolset::RemoveTransition(
	UStateTree* StateTree,
	UStateTreeState* SourceState,
	const FGuid TransitionID)
{
	UStateTreeEditorData* EditorData = nullptr;
	FString Error;
	if (!TryGetEditableData(StateTree, EditorData, Error))
	{
		return MakeFailure(Error);
	}
	if (!DoesStateBelongToTree(StateTree, SourceState))
	{
		return MakeFailure(TEXT("SourceState does not belong to the supplied StateTree."));
	}

	const int32 ExistingIndex = SourceState->Transitions.IndexOfByPredicate(
		[TransitionID](const FStateTreeTransition& Candidate)
		{
			return Candidate.ID == TransitionID;
		});
	if (ExistingIndex == INDEX_NONE)
	{
		return MakeFailure(TEXT("TransitionID was not found on SourceState."));
	}

	const FScopedTransaction Transaction(LOCTEXT("RemoveStateTreeTransition", "Remove AIRE StateTree Transition"));
	StateTree->Modify();
	EditorData->Modify();
	SourceState->Modify();
	SourceState->Transitions.RemoveAt(ExistingIndex);
	FinalizeMutation(StateTree, EditorData);
	return MakeSuccess(TEXT("Transition removed. Compile and save explicitly after all edits."));
}

FAIREStateTreeMutationResult UAIREStateTreeMCPToolset::AddPropertyBinding(
	UStateTree* StateTree,
	const FGuid SourceStructID,
	const FString& SourcePath,
	const FGuid TargetStructID,
	const FString& TargetPath)
{
	UStateTreeEditorData* EditorData = nullptr;
	FString Error;
	if (!TryGetEditableData(StateTree, EditorData, Error))
	{
		return MakeFailure(Error);
	}

	FPropertyBindingDataView SourceView;
	FPropertyBindingDataView TargetView;
	if (!EditorData->GetBindingDataViewByID(SourceStructID, SourceView)
		|| !EditorData->GetBindingDataViewByID(TargetStructID, TargetView))
	{
		return MakeFailure(TEXT("SourceStructID or TargetStructID is not bindable in this StateTree."));
	}

	FPropertyBindingPath SourceBindingPath(SourceStructID);
	FPropertyBindingPath TargetBindingPath(TargetStructID);
	if (!SourceBindingPath.FromString(SourcePath) || !TargetBindingPath.FromString(TargetPath))
	{
		return MakeFailure(TEXT("SourcePath or TargetPath has invalid property-path syntax."));
	}

	if (!SourceBindingPath.UpdateSegmentsFromValue(SourceView, &Error))
	{
		return MakeFailure(FString::Printf(TEXT("Invalid source property path: %s"), *Error));
	}
	if (!TargetBindingPath.UpdateSegmentsFromValue(TargetView, &Error))
	{
		return MakeFailure(FString::Printf(TEXT("Invalid target property path: %s"), *Error));
	}

	const FScopedTransaction Transaction(LOCTEXT("AddStateTreePropertyBinding", "Add AIRE StateTree Property Binding"));
	StateTree->Modify();
	EditorData->Modify();
	EditorData->AddPropertyBinding(SourceBindingPath, TargetBindingPath);
	EditorData->OnPropertyBindingChanged(SourceBindingPath, TargetBindingPath);
	FinalizeMutation(StateTree, EditorData);
	return MakeSuccess(TEXT("Property binding added or replaced. Compile and save explicitly after all edits."));
}

FAIREStateTreeMutationResult UAIREStateTreeMCPToolset::RemovePropertyBinding(
	UStateTree* StateTree,
	const FGuid TargetStructID,
	const FString& TargetPath)
{
	UStateTreeEditorData* EditorData = nullptr;
	FString Error;
	if (!TryGetEditableData(StateTree, EditorData, Error))
	{
		return MakeFailure(Error);
	}

	FPropertyBindingPath TargetBindingPath(TargetStructID);
	if (!TargetBindingPath.FromString(TargetPath))
	{
		return MakeFailure(TEXT("TargetPath has invalid property-path syntax."));
	}
	FPropertyBindingDataView TargetView;
	if (!EditorData->GetBindingDataViewByID(TargetStructID, TargetView))
	{
		return MakeFailure(TEXT("TargetStructID is not bindable in this StateTree."));
	}
	if (!TargetBindingPath.UpdateSegmentsFromValue(TargetView, &Error))
	{
		return MakeFailure(FString::Printf(TEXT("Invalid target property path: %s"), *Error));
	}

	const FScopedTransaction Transaction(LOCTEXT("RemoveStateTreePropertyBinding", "Remove AIRE StateTree Property Binding"));
	StateTree->Modify();
	EditorData->Modify();
	EditorData->RemovePropertyBinding(TargetBindingPath);
	FinalizeMutation(StateTree, EditorData);
	return MakeSuccess(TEXT("Bindings targeting the supplied property path were removed."));
}

FAIREStateTreeCompileResult UAIREStateTreeMCPToolset::ValidateAndCompile(UStateTree* StateTree)
{
	FAIREStateTreeCompileResult Result;
	UStateTreeEditorData* EditorData = nullptr;
	FString Error;
	if (!TryGetEditableData(StateTree, EditorData, Error))
	{
		Result.Messages.Add(Error);
		return Result;
	}

	UStateTreeEditingSubsystem::ValidateStateTree(StateTree);
	FStateTreeCompilerLog CompilerLog;
	Result.bSuccess = UStateTreeEditingSubsystem::CompileStateTree(StateTree, CompilerLog);
	for (const TSharedRef<FTokenizedMessage>& Message : CompilerLog.ToTokenizedMessages())
	{
		Result.Messages.Add(Message->ToText().ToString());
	}
	if (Result.Messages.IsEmpty())
	{
		Result.Messages.Add(Result.bSuccess
			? TEXT("StateTree compiled successfully.")
			: TEXT("StateTree compilation failed without a compiler message."));
	}
	return Result;
}

FAIREStateTreeMutationResult UAIREStateTreeMCPToolset::SaveStateTree(UStateTree* StateTree)
{
	UStateTreeEditorData* EditorData = nullptr;
	FString Error;
	if (!TryGetEditableData(StateTree, EditorData, Error))
	{
		return MakeFailure(Error);
	}
	if (UStateTreeEditingSubsystem::NeedsRecompile(StateTree))
	{
		return MakeFailure(TEXT("StateTree requires a successful compile before it can be saved."));
	}
	if (GEditor == nullptr)
	{
		return MakeFailure(TEXT("Unreal Editor is unavailable."));
	}

	UEditorAssetSubsystem* AssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	if (!IsValid(AssetSubsystem) || !AssetSubsystem->SaveLoadedAsset(StateTree, true))
	{
		return MakeFailure(TEXT("The StateTree package could not be saved."));
	}
	return MakeSuccess(TEXT("StateTree saved successfully."));
}

#undef LOCTEXT_NAMESPACE

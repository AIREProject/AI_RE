#include "AIRECompanionTestMCPToolset.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Engine/World.h"
#include "K2Node_CallFunction.h"
#include "K2Node_InputKey.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Logging/TokenizedMessage.h"
#include "ScopedTransaction.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "AIRECompanionTestMCPToolset"

namespace
{
	const FString AllowedLevelPath = TEXT("/Game/Work/OBI/ThirdPerson/Lvl_ThirdPerson.Lvl_ThirdPerson");
	const FString FixtureMarker = TEXT("[AIRE_TEST_FIXTURE:M03-E02-T02]");

	struct FAIREBehaviorKeyBinding
	{
		FKey Key;
		const TCHAR* RequestName;
	};

	const FAIREBehaviorKeyBinding BehaviorKeyBindings[] =
	{
		{EKeys::One, TEXT("Work")},
		{EKeys::Two, TEXT("DirectCommand")},
		{EKeys::Four, TEXT("Survival")},
		{EKeys::Five, TEXT("Disabled")}
	};
	constexpr int32 GasFixtureNodeCount = 8;
	constexpr int32 ExpectedFixtureNodeCount = UE_ARRAY_COUNT(BehaviorKeyBindings) * 3 + 2 + GasFixtureNodeCount;

	FAIRECompanionTestFixtureResult MakeFixtureFailure(const FString& Message)
	{
		FAIRECompanionTestFixtureResult Result;
		Result.Message = Message;
		return Result;
	}

	bool TryGetLevelBlueprint(
		UWorld* LevelWorld,
		const bool bCreateIfMissing,
		ULevelScriptBlueprint*& OutBlueprint,
		UEdGraph*& OutEventGraph,
		FString& OutError)
	{
		OutBlueprint = nullptr;
		OutEventGraph = nullptr;

		if (!IsValid(LevelWorld) || LevelWorld->GetPathName() != AllowedLevelPath)
		{
			OutError = FString::Printf(TEXT("Only %s may be edited by this test tool."), *AllowedLevelPath);
			return false;
		}
		if (!IsValid(LevelWorld->PersistentLevel))
		{
			OutError = TEXT("The supplied world has no persistent level.");
			return false;
		}

		OutBlueprint = LevelWorld->PersistentLevel->GetLevelScriptBlueprint(!bCreateIfMissing);
		if (!IsValid(OutBlueprint))
		{
			OutError = TEXT("The level has no Level Script Blueprint.");
			return false;
		}

		OutEventGraph = FBlueprintEditorUtils::FindEventGraph(OutBlueprint);
		if (!IsValid(OutEventGraph))
		{
			OutError = TEXT("The Level Blueprint has no event graph.");
			return false;
		}

		return true;
	}

	int32 CountFixtureNodes(const UEdGraph& EventGraph)
	{
		int32 NodeCount = 0;
		for (const TObjectPtr<UEdGraphNode>& Node : EventGraph.Nodes)
		{
			if (IsValid(Node) && Node->NodeComment == FixtureMarker)
			{
				++NodeCount;
			}
		}
		return NodeCount;
	}

	int32 RemoveFixtureNodes(ULevelScriptBlueprint& Blueprint, UEdGraph& EventGraph)
	{
		TArray<UEdGraphNode*> NodesToRemove;
		for (const TObjectPtr<UEdGraphNode>& Node : EventGraph.Nodes)
		{
			if (IsValid(Node) && Node->NodeComment == FixtureMarker)
			{
				NodesToRemove.Add(Node);
			}
		}

		for (UEdGraphNode* Node : NodesToRemove)
		{
			FBlueprintEditorUtils::RemoveNode(&Blueprint, Node, true);
		}
		return NodesToRemove.Num();
	}

	UK2Node_InputKey* CreateInputKeyNode(UEdGraph& EventGraph, const FKey Key, const int32 NodeY)
	{
		FGraphNodeCreator<UK2Node_InputKey> NodeCreator(EventGraph);
		UK2Node_InputKey* Node = NodeCreator.CreateNode();
		Node->InputKey = Key;
		Node->bConsumeInput = false;
		Node->NodePosX = 0;
		Node->NodePosY = NodeY;
		Node->NodeComment = FixtureMarker;
		NodeCreator.Finalize();
		return Node;
	}

	UK2Node_CallFunction* CreateCallNode(
		UEdGraph& EventGraph,
		UFunction& Function,
		const int32 NodeX,
		const int32 NodeY)
	{
		FGraphNodeCreator<UK2Node_CallFunction> NodeCreator(EventGraph);
		UK2Node_CallFunction* Node = NodeCreator.CreateNode();
		Node->SetFromFunction(&Function);
		Node->NodePosX = NodeX;
		Node->NodePosY = NodeY;
		Node->NodeComment = FixtureMarker;
		NodeCreator.Finalize();
		return Node;
	}

	bool ConfigureRequestCall(
		const UEdGraphSchema_K2& Schema,
		UK2Node_CallFunction& CallNode,
		const FString& RequestName,
		const bool bIsRequested,
		FString& OutError)
	{
		UEdGraphPin* RequestPin = CallNode.FindPin(TEXT("Request"));
		UEdGraphPin* RequestedPin = CallNode.FindPin(TEXT("bIsRequested"));
		if (RequestPin == nullptr || RequestedPin == nullptr)
		{
			OutError = TEXT("The testing Blueprint function has unexpected parameter pins.");
			return false;
		}

		Schema.TrySetDefaultValue(*RequestPin, RequestName);
		Schema.TrySetDefaultValue(*RequestedPin, bIsRequested ? TEXT("true") : TEXT("false"));
		return true;
	}

	bool ConfigureDamageCall(
		const UEdGraphSchema_K2& Schema,
		UK2Node_CallFunction& CallNode,
		const float DamageAmount,
		FString& OutError)
	{
		UEdGraphPin* DamageAmountPin = CallNode.FindPin(TEXT("DamageAmount"));
		if (DamageAmountPin == nullptr)
		{
			OutError = TEXT("The damage testing function has no DamageAmount pin.");
			return false;
		}

		Schema.TrySetDefaultValue(*DamageAmountPin, FString::SanitizeFloat(DamageAmount));
		return true;
	}
}

FAIRECompanionTestFixtureResult UAIRECompanionTestMCPToolset::InspectBehaviorTestInputs(UWorld* LevelWorld)
{
	ULevelScriptBlueprint* Blueprint = nullptr;
	UEdGraph* EventGraph = nullptr;
	FString Error;
	if (!TryGetLevelBlueprint(LevelWorld, false, Blueprint, EventGraph, Error))
	{
		return MakeFixtureFailure(Error);
	}

	FAIRECompanionTestFixtureResult Result;
	Result.NodeCount = CountFixtureNodes(*EventGraph);
	Result.bSuccess = Result.NodeCount == ExpectedFixtureNodeCount;
	Result.Message = Result.bSuccess
		? TEXT("The complete behavior and Companion GAS input fixture is present.")
		: FString::Printf(TEXT("Expected %d fixture nodes but found %d."), ExpectedFixtureNodeCount, Result.NodeCount);
	return Result;
}

FAIRECompanionTestFixtureResult UAIRECompanionTestMCPToolset::ConfigureBehaviorTestInputs(UWorld* LevelWorld)
{
	if (!IsValid(LevelWorld) || LevelWorld->GetPathName() != AllowedLevelPath)
	{
		return MakeFixtureFailure(FString::Printf(TEXT("Only %s may be edited by this test tool."), *AllowedLevelPath));
	}
	if (LevelWorld->GetOutermost()->IsDirty())
	{
		return MakeFixtureFailure(TEXT("Save or discard existing level changes before configuring the test fixture."));
	}

	ULevelScriptBlueprint* Blueprint = nullptr;
	UEdGraph* EventGraph = nullptr;
	FString Error;
	if (!TryGetLevelBlueprint(LevelWorld, true, Blueprint, EventGraph, Error))
	{
		return MakeFixtureFailure(Error);
	}
	if (CountFixtureNodes(*EventGraph) == ExpectedFixtureNodeCount)
	{
		FAIRECompanionTestFixtureResult Result;
		Result.bSuccess = true;
		Result.NodeCount = ExpectedFixtureNodeCount;
		Result.Message = TEXT("The complete behavior test input fixture already exists; no mutation was applied.");
		return Result;
	}

	UClass* TestingLibraryClass = FindObject<UClass>(nullptr, TEXT("/Script/AI_RE.AIRECompanionTestingBlueprintLibrary"));
	if (!IsValid(TestingLibraryClass))
	{
		return MakeFixtureFailure(TEXT("AIRECompanionTestingBlueprintLibrary is not loaded."));
	}

	UFunction* SetRequestFunction = TestingLibraryClass->FindFunctionByName(TEXT("SetFirstCompanionTestBehaviorRequest"));
	UFunction* ClearRequestsFunction = TestingLibraryClass->FindFunctionByName(TEXT("ClearFirstCompanionTestBehaviorRequests"));
	UFunction* ApplyDamageFunction = TestingLibraryClass->FindFunctionByName(TEXT("ApplyDamageToFirstCompanion"));
	UFunction* ResetAttributesFunction = TestingLibraryClass->FindFunctionByName(TEXT("ResetFirstCompanionAttributes"));
	UFunction* LogAbilityStateFunction = TestingLibraryClass->FindFunctionByName(TEXT("LogFirstCompanionAbilityState"));
	const UEdGraphSchema_K2* Schema = Cast<UEdGraphSchema_K2>(EventGraph->GetSchema());
	if (!IsValid(SetRequestFunction)
		|| !IsValid(ClearRequestsFunction)
		|| !IsValid(ApplyDamageFunction)
		|| !IsValid(ResetAttributesFunction)
		|| !IsValid(LogAbilityStateFunction)
		|| !IsValid(Schema))
	{
		return MakeFixtureFailure(TEXT("Required testing functions or the K2 graph schema are unavailable."));
	}

	const FScopedTransaction Transaction(LOCTEXT("ConfigureBehaviorTestInputs", "Configure AIRE Companion Test Inputs"));
	LevelWorld->Modify();
	LevelWorld->PersistentLevel->Modify();
	Blueprint->Modify();
	EventGraph->Modify();
	RemoveFixtureNodes(*Blueprint, *EventGraph);

	for (int32 Index = 0; Index < UE_ARRAY_COUNT(BehaviorKeyBindings); ++Index)
	{
		const int32 NodeY = Index * 260;
		UK2Node_InputKey* InputNode = CreateInputKeyNode(*EventGraph, BehaviorKeyBindings[Index].Key, NodeY);
		UK2Node_CallFunction* PressedCall = CreateCallNode(*EventGraph, *SetRequestFunction, 360, NodeY - 50);
		UK2Node_CallFunction* ReleasedCall = CreateCallNode(*EventGraph, *SetRequestFunction, 360, NodeY + 80);

		if (!ConfigureRequestCall(*Schema, *PressedCall, BehaviorKeyBindings[Index].RequestName, true, Error)
			|| !ConfigureRequestCall(*Schema, *ReleasedCall, BehaviorKeyBindings[Index].RequestName, false, Error)
			|| !Schema->TryCreateConnection(InputNode->GetPressedPin(), PressedCall->GetExecPin())
			|| !Schema->TryCreateConnection(InputNode->GetReleasedPin(), ReleasedCall->GetExecPin()))
		{
			RemoveFixtureNodes(*Blueprint, *EventGraph);
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
			return MakeFixtureFailure(Error.IsEmpty() ? TEXT("Failed to connect a behavior test input node.") : Error);
		}
	}

	const int32 ClearNodeY = UE_ARRAY_COUNT(BehaviorKeyBindings) * 260;
	UK2Node_InputKey* ClearInputNode = CreateInputKeyNode(*EventGraph, EKeys::Zero, ClearNodeY);
	UK2Node_CallFunction* ClearCall = CreateCallNode(*EventGraph, *ClearRequestsFunction, 360, ClearNodeY);
	if (!Schema->TryCreateConnection(ClearInputNode->GetPressedPin(), ClearCall->GetExecPin()))
	{
		RemoveFixtureNodes(*Blueprint, *EventGraph);
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		return MakeFixtureFailure(TEXT("Failed to connect the clear-request input node."));
	}

	UK2Node_InputKey* LightDamageInputNode = CreateInputKeyNode(*EventGraph, EKeys::Six, 0);
	LightDamageInputNode->NodePosX = 900;
	UK2Node_CallFunction* LightDamageCall = CreateCallNode(*EventGraph, *ApplyDamageFunction, 1260, 0);
	if (!ConfigureDamageCall(*Schema, *LightDamageCall, 25.0f, Error)
		|| !Schema->TryCreateConnection(LightDamageInputNode->GetPressedPin(), LightDamageCall->GetExecPin()))
	{
		RemoveFixtureNodes(*Blueprint, *EventGraph);
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		return MakeFixtureFailure(Error.IsEmpty() ? TEXT("Failed to connect the light-damage input node.") : Error);
	}

	UK2Node_InputKey* FatalDamageInputNode = CreateInputKeyNode(*EventGraph, EKeys::Seven, 260);
	FatalDamageInputNode->NodePosX = 900;
	UK2Node_CallFunction* FatalDamageCall = CreateCallNode(*EventGraph, *ApplyDamageFunction, 1260, 260);
	if (!ConfigureDamageCall(*Schema, *FatalDamageCall, 200.0f, Error)
		|| !Schema->TryCreateConnection(FatalDamageInputNode->GetPressedPin(), FatalDamageCall->GetExecPin()))
	{
		RemoveFixtureNodes(*Blueprint, *EventGraph);
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		return MakeFixtureFailure(Error.IsEmpty() ? TEXT("Failed to connect the fatal-damage input node.") : Error);
	}

	UK2Node_InputKey* ResetAttributesInputNode = CreateInputKeyNode(*EventGraph, EKeys::Eight, 520);
	ResetAttributesInputNode->NodePosX = 900;
	UK2Node_CallFunction* ResetAttributesCall = CreateCallNode(*EventGraph, *ResetAttributesFunction, 1260, 520);
	if (!Schema->TryCreateConnection(ResetAttributesInputNode->GetPressedPin(), ResetAttributesCall->GetExecPin()))
	{
		RemoveFixtureNodes(*Blueprint, *EventGraph);
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		return MakeFixtureFailure(TEXT("Failed to connect the reset-attributes input node."));
	}

	UK2Node_InputKey* LogAbilityStateInputNode = CreateInputKeyNode(*EventGraph, EKeys::Nine, 780);
	LogAbilityStateInputNode->NodePosX = 900;
	UK2Node_CallFunction* LogAbilityStateCall = CreateCallNode(*EventGraph, *LogAbilityStateFunction, 1260, 780);
	if (!Schema->TryCreateConnection(LogAbilityStateInputNode->GetPressedPin(), LogAbilityStateCall->GetExecPin()))
	{
		RemoveFixtureNodes(*Blueprint, *EventGraph);
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		return MakeFixtureFailure(TEXT("Failed to connect the ability-state log input node."));
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	FAIRECompanionTestFixtureResult Result;
	Result.NodeCount = CountFixtureNodes(*EventGraph);
	Result.bSuccess = Result.NodeCount == ExpectedFixtureNodeCount;
	Result.Message = Result.bSuccess
		? TEXT("Behavior and Companion GAS test inputs configured. Compile and save explicitly.")
		: FString::Printf(TEXT("Fixture creation was incomplete: %d nodes were created."), Result.NodeCount);
	return Result;
}

FAIRECompanionTestFixtureResult UAIRECompanionTestMCPToolset::RemoveBehaviorTestInputs(UWorld* LevelWorld)
{
	ULevelScriptBlueprint* Blueprint = nullptr;
	UEdGraph* EventGraph = nullptr;
	FString Error;
	if (!TryGetLevelBlueprint(LevelWorld, false, Blueprint, EventGraph, Error))
	{
		return MakeFixtureFailure(Error);
	}

	const FScopedTransaction Transaction(LOCTEXT("RemoveBehaviorTestInputs", "Remove AIRE Companion Test Inputs"));
	LevelWorld->Modify();
	LevelWorld->PersistentLevel->Modify();
	Blueprint->Modify();
	EventGraph->Modify();

	FAIRECompanionTestFixtureResult Result;
	Result.NodeCount = RemoveFixtureNodes(*Blueprint, *EventGraph);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	Result.bSuccess = true;
	Result.Message = FString::Printf(TEXT("Removed %d fixture nodes. Compile and save explicitly."), Result.NodeCount);
	return Result;
}

FAIRECompanionTestFixtureResult UAIRECompanionTestMCPToolset::ValidateAndCompileLevelBlueprint(UWorld* LevelWorld)
{
	ULevelScriptBlueprint* Blueprint = nullptr;
	UEdGraph* EventGraph = nullptr;
	FString Error;
	if (!TryGetLevelBlueprint(LevelWorld, false, Blueprint, EventGraph, Error))
	{
		return MakeFixtureFailure(Error);
	}

	FCompilerResultsLog CompilerLog;
	FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipSave, &CompilerLog);

	FAIRECompanionTestFixtureResult Result;
	Result.NodeCount = CountFixtureNodes(*EventGraph);
	Result.bSuccess = CompilerLog.NumErrors == 0 && Blueprint->Status != BS_Error;
	for (const TSharedRef<FTokenizedMessage>& Message : CompilerLog.Messages)
	{
		Result.CompilerMessages.Add(Message->ToText().ToString());
	}
	Result.Message = Result.bSuccess ? TEXT("Level Blueprint compiled successfully.") : TEXT("Level Blueprint compilation failed.");
	return Result;
}

FAIRECompanionTestFixtureResult UAIRECompanionTestMCPToolset::SaveLevelBlueprint(UWorld* LevelWorld)
{
	ULevelScriptBlueprint* Blueprint = nullptr;
	UEdGraph* EventGraph = nullptr;
	FString Error;
	if (!TryGetLevelBlueprint(LevelWorld, false, Blueprint, EventGraph, Error))
	{
		return MakeFixtureFailure(Error);
	}
	if (Blueprint->Status == BS_Dirty || Blueprint->Status == BS_Error)
	{
		return MakeFixtureFailure(TEXT("The Level Blueprint requires a successful compile before saving."));
	}
	if (GEditor == nullptr)
	{
		return MakeFixtureFailure(TEXT("Unreal Editor is unavailable."));
	}

	UEditorAssetSubsystem* AssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	if (!IsValid(AssetSubsystem) || !AssetSubsystem->SaveLoadedAsset(LevelWorld, true))
	{
		return MakeFixtureFailure(TEXT("The level package could not be saved."));
	}

	FAIRECompanionTestFixtureResult Result;
	Result.bSuccess = true;
	Result.NodeCount = CountFixtureNodes(*EventGraph);
	Result.Message = TEXT("Level Blueprint saved successfully.");
	return Result;
}

#undef LOCTEXT_NAMESPACE

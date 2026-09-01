#if WITH_DEV_AUTOMATION_TESTS

/*
 * Command Gateway의 수명주기 계약을 실제 Transient World에서 검증합니다.
 * Clock Skew, 만료·중복 후보, 교체 명령, 늦게 도착한 Generation Callback,
 * WorkOrder 전환, 취소와 Shutdown을 한 흐름으로 실행해 명령이 정확히 한 번만
 * 활성화되고 종료되는지를 회귀 조건으로 고정합니다.
 */
#include "Command/AIRECompanionCommandGatewayComponent.h"

#include "AI_REHarvestGameplayTags.h"
#include "AI_REHarvestableResourceActor.h"
#include "AI_REHarvestableResourceComponent.h"
#include "AI_REItemActor.h"
#include "AI_REItemDataAsset.h"
#include "Chat/AIRECompanionChatComponent.h"
#include "Components/SphereComponent.h"
#include "Core/AIRECompanionCharacter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/StrongObjectPtr.h"
#include "Work/AIRECompanionWorkOrderComponent.h"

namespace
{
	FAIRECommandCandidate MakeCandidate(
		const FString& CommandId,
		const EAIRECommandType Type,
		const FString& RequestId = TEXT("request-1"))
	{
		const FDateTime NowUtc = FDateTime::UtcNow();
		FAIRECommandCandidate Candidate;
		Candidate.CommandId = CommandId;
		Candidate.RequestId = RequestId;
		Candidate.Type = Type;
		Candidate.Priority = EAIRECommandPriority::Normal;
		Candidate.IssuedAtUtc = NowUtc;
		Candidate.ExpiresAtUtc = NowUtc + FTimespan::FromSeconds(30.0);
		return Candidate;
	}

	void EmitCandidates(
		UAIRECompanionChatComponent& ChatComponent,
		TArray<FAIRECommandCandidate> Candidates)
	{
		FAIREChatResult Result;
		Result.RequestId = TEXT("request-1");
		Result.ResponseId = TEXT("response-1");
		Result.DisplayText = TEXT("ok");
		Result.CommandCandidates = MoveTemp(Candidates);
		ChatComponent.OnResponseReceived.Broadcast(Result);
	}

	void AddResourceQueryCollision(AAI_REHarvestableResourceActor& Resource)
	{
		USphereComponent* Collision = NewObject<USphereComponent>(&Resource);
		Resource.AddInstanceComponent(Collision);
		Collision->SetupAttachment(Resource.GetRootComponent());
		Collision->SetSphereRadius(50.0f);
		Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Collision->SetCollisionObjectType(ECC_WorldDynamic);
		Collision->SetCollisionResponseToAllChannels(ECR_Overlap);
		Collision->RegisterComponent();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAIRECompanionCommandGatewayLifecycleTest,
	"AIRE.Companion.Command.Gateway.Lifecycle",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

bool FAIRECompanionCommandGatewayLifecycleTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	if (!TestNotNull(TEXT("Engine is available"), GEngine))
	{
		return false;
	}

	const FName WorldName = MakeUniqueObjectName(
		nullptr,
		UWorld::StaticClass(),
		NAME_None,
		EUniqueObjectNameOptions::GloballyUnique);
	UWorld* TestWorld = UWorld::CreateWorld(
		EWorldType::Game,
		false,
		WorldName,
		GetTransientPackage());
	if (!TestNotNull(TEXT("Transient command world is created"), TestWorld))
	{
		return false;
	}
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(TestWorld);
	TestWorld->InitializeActorsForPlay(FURL());
	ON_SCOPE_EXIT
	{
		TestWorld->EndPlay(EEndPlayReason::Quit);
		GEngine->ShutdownWorldNetDriver(TestWorld);
		TestWorld->DestroyWorld(true);
		TestWorld->SetPhysicsScene(nullptr);
		GEngine->DestroyWorldContext(TestWorld);
	};

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AAIRECompanionCharacter* Companion =
		TestWorld->SpawnActor<AAIRECompanionCharacter>(
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			SpawnParameters);
	if (!TestNotNull(TEXT("Companion is spawned"), Companion))
	{
		return false;
	}
	TestWorld->BeginPlay();
	TestWorld->GetWorldSettings()->NotifyBeginPlay();

	UAIRECompanionChatComponent* ChatComponent = Companion->GetChatComponent();
	UAIRECompanionCommandGatewayComponent* Gateway =
		Companion->GetCommandGatewayComponent();
	if (!TestNotNull(TEXT("Chat component is available"), ChatComponent)
		|| !TestNotNull(TEXT("Command gateway is available"), Gateway))
	{
		return false;
	}

	FAIRECommandCandidate ToleratedClockSkew = MakeCandidate(
		TEXT("command-clock-skew-tolerated"),
		EAIRECommandType::HoldPosition);
	ToleratedClockSkew.IssuedAtUtc =
		FDateTime::UtcNow() + FTimespan::FromSeconds(15.0);
	ToleratedClockSkew.ExpiresAtUtc =
		ToleratedClockSkew.IssuedAtUtc + FTimespan::FromSeconds(30.0);
	EmitCandidates(*ChatComponent, {ToleratedClockSkew});
	TestTrue(
		TEXT("A candidate within the server clock tolerance becomes active"),
		Gateway->HasActiveDirectCommand());

	EmitCandidates(
		*ChatComponent,
		{MakeCandidate(TEXT("command-hold-1"), EAIRECommandType::HoldPosition)});
	const FAIREDirectCommandSnapshot FirstSnapshot =
		Gateway->GetDirectCommandSnapshot();
	TestTrue(TEXT("A valid Hold command becomes active"), FirstSnapshot.bIsActive);
	TestEqual(
		TEXT("The first active command owns its stable ID"),
		FirstSnapshot.CommandId,
		FString(TEXT("command-hold-1")));

	FAIRECommandCandidate ExcessiveClockSkew = MakeCandidate(
		TEXT("command-clock-skew-excessive"),
		EAIRECommandType::HoldPosition);
	ExcessiveClockSkew.IssuedAtUtc =
		FDateTime::UtcNow() + FTimespan::FromSeconds(45.0);
	ExcessiveClockSkew.ExpiresAtUtc =
		ExcessiveClockSkew.IssuedAtUtc + FTimespan::FromSeconds(30.0);
	EmitCandidates(*ChatComponent, {ExcessiveClockSkew});
	FAIREDirectCommandSnapshot Snapshot = Gateway->GetDirectCommandSnapshot();
	TestEqual(
		TEXT("A candidate beyond the server clock tolerance is rejected"),
		Snapshot.Generation,
		FirstSnapshot.Generation);

	EmitCandidates(
		*ChatComponent,
		{MakeCandidate(TEXT("command-hold-2"), EAIRECommandType::HoldPosition)});
	const FAIREDirectCommandSnapshot ReplacementSnapshot =
		Gateway->GetDirectCommandSnapshot();
	TestTrue(TEXT("A validated replacement remains active"), ReplacementSnapshot.bIsActive);
	TestEqual(
		TEXT("The replacement owns the active command"),
		ReplacementSnapshot.CommandId,
		FString(TEXT("command-hold-2")));
	TestTrue(
		TEXT("Replacement advances the command generation"),
		ReplacementSnapshot.Generation > FirstSnapshot.Generation);
	TestFalse(
		TEXT("A late completion from the replaced generation is ignored"),
		Gateway->ReportDirectCommandSucceeded(
			FirstSnapshot.CommandId,
			FirstSnapshot.Generation));

	EmitCandidates(
		*ChatComponent,
		{MakeCandidate(TEXT("command-hold-2"), EAIRECommandType::HoldPosition)});
	Snapshot = Gateway->GetDirectCommandSnapshot();
	TestEqual(
		TEXT("A duplicate does not replace the active intent"),
		Snapshot.Generation,
		ReplacementSnapshot.Generation);

	EmitCandidates(
		*ChatComponent,
		{
			MakeCandidate(TEXT("command-multiple-1"), EAIRECommandType::HoldPosition),
			MakeCandidate(TEXT("command-multiple-2"), EAIRECommandType::HoldPosition),
		});
	Snapshot = Gateway->GetDirectCommandSnapshot();
	TestEqual(
		TEXT("A multiple-candidate batch does not replace the active intent"),
		Snapshot.Generation,
		ReplacementSnapshot.Generation);

	FAIRECommandCandidate Expired = MakeCandidate(
		TEXT("command-expired"),
		EAIRECommandType::HoldPosition);
	Expired.IssuedAtUtc = FDateTime::UtcNow() - FTimespan::FromSeconds(2.0);
	Expired.ExpiresAtUtc = FDateTime::UtcNow() - FTimespan::FromSeconds(1.0);
	EmitCandidates(*ChatComponent, {Expired});
	Snapshot = Gateway->GetDirectCommandSnapshot();
	TestEqual(
		TEXT("An expired candidate does not replace the active intent"),
		Snapshot.Generation,
		ReplacementSnapshot.Generation);

	FAIRECommandCandidate GatherWithQuantity = MakeCandidate(
		TEXT("command-gather-quantity"),
		EAIRECommandType::GatherResource);
	GatherWithQuantity.GatherResource = EAIREGatherResourceKind::Wood;
	GatherWithQuantity.GatherQuantity = 1;
	GatherWithQuantity.bHasGatherQuantity = true;
	EmitCandidates(*ChatComponent, {GatherWithQuantity});
	Snapshot = Gateway->GetDirectCommandSnapshot();
	TestEqual(
		TEXT("Gather quantity does not mutate the active intent"),
		Snapshot.Generation,
		ReplacementSnapshot.Generation);

	FAIRECommandCandidate GatherWithoutTarget = MakeCandidate(
		TEXT("command-gather-no-target"),
		EAIRECommandType::GatherResource);
	GatherWithoutTarget.GatherResource = EAIREGatherResourceKind::Wood;
	EmitCandidates(*ChatComponent, {GatherWithoutTarget});
	Snapshot = Gateway->GetDirectCommandSnapshot();
	TestEqual(
		TEXT("Gather with no nearby wood preserves the active intent"),
		Snapshot.Generation,
		ReplacementSnapshot.Generation);

	TestTrue(
		TEXT("The active generation enters Running exactly through the task seam"),
		Gateway->ReportDirectCommandRunning(
			ReplacementSnapshot.CommandId,
			ReplacementSnapshot.Generation));
	TestTrue(
		TEXT("The active generation can reach one terminal result"),
		Gateway->ReportDirectCommandSucceeded(
			ReplacementSnapshot.CommandId,
			ReplacementSnapshot.Generation,
			EAIRECommandResultReason::LeaseCompleted));
	TestFalse(
		TEXT("A repeated terminal callback is ignored"),
		Gateway->ReportDirectCommandSucceeded(
			ReplacementSnapshot.CommandId,
			ReplacementSnapshot.Generation,
			EAIRECommandResultReason::LeaseCompleted));
	TestFalse(
		TEXT("Terminal completion clears the direct snapshot"),
		Gateway->HasActiveDirectCommand());

	AAI_REHarvestableResourceActor* Resource =
		TestWorld->SpawnActor<AAI_REHarvestableResourceActor>(
			FVector(200.0f, 0.0f, 0.0f),
			FRotator::ZeroRotator,
			SpawnParameters);
	if (!TestNotNull(TEXT("Gather resource is spawned"), Resource))
	{
		return false;
	}
	AddResourceQueryCollision(*Resource);
	TStrongObjectPtr<UAI_REItemDataAsset> RewardItem(
		NewObject<UAI_REItemDataAsset>());
	RewardItem->ItemId = FName(TEXT("AIRE.Test.GatherReward"));
	Resource->ItemActorClass = AAI_REItemActor::StaticClass();
	Resource->GetHarvestableResourceComponent()->SetResourceDefaults(
		AI_REHarvestGameplayTags::Resource_Wood,
		RewardItem.Get(),
		1,
		25.0f);

	FAIRECommandCandidate Gather = MakeCandidate(
		TEXT("command-gather"),
		EAIRECommandType::GatherResource);
	Gather.GatherResource = EAIREGatherResourceKind::Wood;
	EmitCandidates(*ChatComponent, {Gather});
	UAIRECompanionWorkOrderComponent* WorkOrderComponent =
		Companion->GetWorkOrderComponent();
	const FAIRECompanionWorkOrderSnapshot GatherSnapshot =
		WorkOrderComponent->GetWorkOrderSnapshot();
	TestTrue(
		TEXT("Gather creates a Harvesting WorkOrder"),
		GatherSnapshot.WorkOrderId.IsValid()
			&& GatherSnapshot.WorkType == EAIRECompanionWorkOrderType::Harvesting
			&& GatherSnapshot.TargetActor.Get() == Resource);
	TestTrue(
		TEXT("Gather enters Moving"),
		WorkOrderComponent->TryStartMoving(GatherSnapshot.WorkOrderId));
	TestTrue(
		TEXT("Gather enters Working"),
		WorkOrderComponent->TryStartWorking(GatherSnapshot.WorkOrderId));
	TestTrue(
		TEXT("Gather completes"),
		WorkOrderComponent->TryCompleteWorkOrder(GatherSnapshot.WorkOrderId));
	TestFalse(
		TEXT("Gather terminal transition is exact-once"),
		WorkOrderComponent->TryCompleteWorkOrder(GatherSnapshot.WorkOrderId));

	FAIRECommandCandidate GatherToCancel = MakeCandidate(
		TEXT("command-gather-cancelled"),
		EAIRECommandType::GatherResource);
	GatherToCancel.GatherResource = EAIREGatherResourceKind::Wood;
	EmitCandidates(*ChatComponent, {GatherToCancel});
	const FGuid GatherToCancelId =
		WorkOrderComponent->GetWorkOrderSnapshot().WorkOrderId;
	EmitCandidates(
		*ChatComponent,
		{MakeCandidate(TEXT("command-cancel-gather"), EAIRECommandType::CancelCurrent)});
	TestTrue(
		TEXT("CancelCurrent cancels Gather"),
		WorkOrderComponent->GetWorkOrderSnapshot().State
			== EAIRECompanionWorkOrderState::Cancelled);
	TestFalse(
		TEXT("Cancelled Gather ignores late completion"),
		WorkOrderComponent->TryCompleteWorkOrder(GatherToCancelId));

	EmitCandidates(
		*ChatComponent,
		{MakeCandidate(TEXT("command-shutdown"), EAIRECommandType::HoldPosition)});
	TestTrue(TEXT("A pre-shutdown command is active"), Gateway->HasActiveDirectCommand());
	Gateway->ShutdownGateway();
	TestFalse(TEXT("Shutdown clears the active command"), Gateway->HasActiveDirectCommand());
	EmitCandidates(
		*ChatComponent,
		{MakeCandidate(TEXT("command-late"), EAIRECommandType::HoldPosition)});
	TestFalse(
		TEXT("A late Chat callback after shutdown cannot reactivate the gateway"),
		Gateway->HasActiveDirectCommand());

	return true;
}

#endif

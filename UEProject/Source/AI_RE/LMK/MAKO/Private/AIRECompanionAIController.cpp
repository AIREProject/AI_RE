#include "AIRECompanionAIController.h"

#include "AIRECompanionCharacter.h"
#include "AIRECompanionThreatComponent.h"
#include "Components/StateTreeAIComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogAIRECompanionAIController, Log, All);

AAIRECompanionAIController::AAIRECompanionAIController()
{
	PrimaryActorTick.bCanEverTick = false;
	bAttachToPawn = true;
	bStartAILogicOnPossess = false;

	StateTreeAIComponent = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeAI"));
	check(StateTreeAIComponent);
	StateTreeAIComponent->SetStartLogicAutomatically(false);

	ThreatComponent = CreateDefaultSubobject<UAIRECompanionThreatComponent>(TEXT("Threat"));
	check(ThreatComponent);
	SetPerceptionComponent(*ThreatComponent);
}

void AAIRECompanionAIController::SetTestBehaviorRequest(
	const EAIRECompanionTestBehaviorRequest Request,
	const bool bIsRequested)
{
	if (Request == EAIRECompanionTestBehaviorRequest::None)
	{
		return;
	}

	if (bIsRequested)
	{
		EnumAddFlags(ActiveTestBehaviorRequests, Request);
	}
	else
	{
		EnumRemoveFlags(ActiveTestBehaviorRequests, Request);
	}
}

bool AAIRECompanionAIController::IsTestBehaviorRequestActive(
	const EAIRECompanionTestBehaviorRequest Request) const
{
	return Request != EAIRECompanionTestBehaviorRequest::None
		&& EnumHasAnyFlags(ActiveTestBehaviorRequests, Request);
}

void AAIRECompanionAIController::ClearTestBehaviorRequests()
{
	ActiveTestBehaviorRequests = EAIRECompanionTestBehaviorRequest::None;
}

UAIRECompanionThreatComponent* AAIRECompanionAIController::GetThreatComponent() const
{
	return ThreatComponent;
}

void AAIRECompanionAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	AAIRECompanionCharacter* PossessedCompanion = Cast<AAIRECompanionCharacter>(InPawn);
	if (!IsValid(PossessedCompanion))
	{
		UE_LOG(LogAIRECompanionAIController, Warning, TEXT("Companion AIController rejected incompatible pawn %s."), *GetNameSafe(InPawn));
		UnPossess();
		return;
	}

	ClearTestBehaviorRequests();
	CompanionCharacter = PossessedCompanion;
	ThreatComponent->StartThreatDetection(PossessedCompanion);
	StopMovement();
	StateTreeAIComponent->StartLogic();

	if (!StateTreeAIComponent->IsRunning())
	{
		UE_LOG(
			LogAIRECompanionAIController,
			Warning,
			TEXT("Companion AIController %s could not start its StateTree. Verify the StateTree asset assignment."),
			*GetNameSafe(this));
	}
}

void AAIRECompanionAIController::OnUnPossess()
{
	ResetCompanionState();
	Super::OnUnPossess();
}

void AAIRECompanionAIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ResetCompanionState();
	Super::EndPlay(EndPlayReason);
}

void AAIRECompanionAIController::ResetCompanionState()
{
	if (IsValid(ThreatComponent))
	{
		ThreatComponent->StopThreatDetection();
	}

	if (IsValid(StateTreeAIComponent))
	{
		StateTreeAIComponent->StopLogic(TEXT("Companion controller is releasing its pawn."));
	}

	StopMovement();
	ClearTestBehaviorRequests();
	CompanionCharacter.Reset();
}

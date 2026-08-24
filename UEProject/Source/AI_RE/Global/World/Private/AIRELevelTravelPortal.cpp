#include "AIRELevelTravelPortal.h"

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogAIRELevelTravelPortal, Log, All);

AAIRELevelTravelPortal::AAIRELevelTravelPortal()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerVolume"));
	TriggerVolume->SetBoxExtent(FVector(150.0f, 150.0f, 100.0f));
	TriggerVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerVolume->SetCollisionObjectType(ECC_WorldDynamic);
	TriggerVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerVolume->SetGenerateOverlapEvents(true);
	SetRootComponent(TriggerVolume);

	TriggerVolume->OnComponentBeginOverlap.AddDynamic(
		this,
		&AAIRELevelTravelPortal::HandleTriggerBeginOverlap);
	TriggerVolume->OnComponentEndOverlap.AddDynamic(
		this,
		&AAIRELevelTravelPortal::HandleTriggerEndOverlap);

	PortalEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("PortalEffect"));
	PortalEffect->SetupAttachment(TriggerVolume);
	PortalEffect->SetAutoActivate(true);
}

void AAIRELevelTravelPortal::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	ClearHoldTimer();
	Super::EndPlay(EndPlayReason);
}

void AAIRELevelTravelPortal::HandleTriggerBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	const int32 OtherBodyIndex,
	const bool bFromSweep,
	const FHitResult& SweepResult)
{
	(void)OverlappedComponent;
	(void)OtherComponent;
	(void)OtherBodyIndex;
	(void)bFromSweep;
	(void)SweepResult;

	if (bTravelRequested || !IsLocalPlayerPawn(OtherActor))
	{
		return;
	}

	if (HoldDuration <= 0.0f)
	{
		UE_LOG(
			LogAIRELevelTravelPortal,
			Warning,
			TEXT("Level portal hold duration must be greater than zero. Portal=%s"),
			*GetNameSafe(this));
		return;
	}

	FTimerManager& TimerManager = GetWorldTimerManager();
	if (TimerManager.IsTimerActive(HoldTimerHandle))
	{
		return;
	}

	TimerManager.SetTimer(
		HoldTimerHandle,
		this,
		&AAIRELevelTravelPortal::HandleHoldCompleted,
		HoldDuration,
		false);
}

void AAIRELevelTravelPortal::HandleTriggerEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	const int32 OtherBodyIndex)
{
	(void)OverlappedComponent;
	(void)OtherComponent;
	(void)OtherBodyIndex;

	if (!bTravelRequested && IsLocalPlayerPawn(OtherActor))
	{
		ClearHoldTimer();
	}
}

void AAIRELevelTravelPortal::HandleHoldCompleted()
{
	if (bTravelRequested)
	{
		return;
	}

	AActor* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!IsValid(PlayerPawn)
		|| !IsValid(TriggerVolume)
		|| !TriggerVolume->IsOverlappingActor(PlayerPawn))
	{
		return;
	}

	if (DestinationLevel.IsNull())
	{
		UE_LOG(
			LogAIRELevelTravelPortal,
			Warning,
			TEXT("Level portal destination is not configured. Portal=%s"),
			*GetNameSafe(this));
		return;
	}

	bTravelRequested = true;
	UGameplayStatics::OpenLevelBySoftObjectPtr(
		this,
		DestinationLevel,
		true,
		FString());
}

void AAIRELevelTravelPortal::ClearHoldTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HoldTimerHandle);
	}
}

bool AAIRELevelTravelPortal::IsLocalPlayerPawn(const AActor* Actor) const
{
	return IsValid(Actor)
		&& Actor == UGameplayStatics::GetPlayerPawn(this, 0);
}

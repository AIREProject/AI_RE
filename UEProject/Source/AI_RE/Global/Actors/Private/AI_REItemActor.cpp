#include "AI_REItemActor.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "AI_RECharacter.h"
#include "AI_REPlayerInventoryComponent.h"
#include "AIREHarvestRewardReceiver.h"
#include "../../OBI/Component/Public/AI_REItemDataAsset.h"
#include "Engine/World.h"
#include "TimerManager.h"

AAI_REItemActor::AAI_REItemActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComponent"));
	RootComponent = SphereComponent;
	SphereComponent->SetSphereRadius(100.f);
	SphereComponent->SetCollisionProfileName(TEXT("OverlapAllDynamic"));

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AAI_REItemActor::BeginPlay()
{
	Super::BeginPlay();

	if (!bHarvestAutoPickupEnabled)
	{
		return;
	}

	GetWorldTimerManager().SetTimer(
		HarvestAutoPickupTimerHandle,
		this,
		&AAI_REItemActor::PollHarvestAutoPickup,
		FMath::Max(0.05f, CompanionAutoPickupRetryInterval),
		true,
		FMath::Max(0.0f, CompanionAutoPickupDelay));
}

void AAI_REItemActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HarvestAutoPickupTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void AAI_REItemActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bPickupClaimed)
	{
		SetActorTickEnabled(false);
		return;
	}

	AActor* ReceiverActor = PickupPresentationTarget.Get();
	if (!IsValid(ReceiverActor) || CompanionPickupTravelDuration <= 0.0f)
	{
		Destroy();
		return;
	}

	PickupPresentationElapsedTime += FMath::Max(0.0f, DeltaSeconds);
	const float Alpha = FMath::Clamp(
		PickupPresentationElapsedTime / CompanionPickupTravelDuration,
		0.0f,
		1.0f);
	const float EasedAlpha = FMath::InterpEaseIn(0.0f, 1.0f, Alpha, 2.0f);
	const FVector TargetLocation =
		ReceiverActor->GetActorLocation()
		+ FVector(0.0f, 0.0f, CompanionPickupTargetHeight);
	SetActorLocation(
		FMath::Lerp(PickupPresentationStartLocation, TargetLocation, EasedAlpha),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	SetActorScale3D(FMath::Lerp(
		PickupPresentationStartScale,
		PickupPresentationStartScale * 0.1f,
		EasedAlpha));

	if (Alpha >= 1.0f)
	{
		Destroy();
	}
}

bool AAI_REItemActor::InitializeHarvestAutoPickup(
	const FGuid& InDeliveryId,
	AActor* InPreferredReceiver)
{
	if (!InDeliveryId.IsValid()
		|| !IsValid(InPreferredReceiver)
		|| !InPreferredReceiver->Implements<UAIREHarvestRewardReceiver>())
	{
		return false;
	}

	HarvestDeliveryId = InDeliveryId;
	PreferredAutoPickupReceiver = InPreferredReceiver;
	bHarvestAutoPickupEnabled = true;
	return true;
}

void AAI_REItemActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (ItemAsset && ItemAsset->WorldMesh)
	{
		MeshComponent->SetStaticMesh(ItemAsset->WorldMesh);
	}
}

void AAI_REItemActor::Interact_Implementation(AActor* Interactor)
{
	if (ItemAsset == nullptr || bPickupClaimed)
	{
		return;
	}

	if (AAI_RECharacter* PlayerChar = Cast<AAI_RECharacter>(Interactor))
	{
		if (UAI_REPlayerInventoryComponent* InvComp = PlayerChar->GetInventoryComponent())
		{
			const int32 ItemCountBeforePickup =
				InvComp->GetItemCount(ItemAsset->ItemId);
			bPickupClaimed = true;
			if (InvComp->AddItem(ItemAsset->ItemId, ItemCount))
			{
				Destroy();
				return;
			}

			// AddItem keeps any quantity that fitted even when the full request failed.
			// Leave only the uncollected quantity in the world to prevent duplication.
			const int32 AddedItemCount = FMath::Clamp(
				InvComp->GetItemCount(ItemAsset->ItemId) - ItemCountBeforePickup,
				0,
				ItemCount);
			ItemCount -= AddedItemCount;
			if (ItemCount <= 0)
			{
				Destroy();
				return;
			}
			bPickupClaimed = false;
		}
	}
}

void AAI_REItemActor::PollHarvestAutoPickup()
{
	if (!bHarvestAutoPickupEnabled
		|| bPickupClaimed
		|| !IsValid(ItemAsset)
		|| ItemAsset->ItemId.IsNone()
		|| ItemCount <= 0)
	{
		return;
	}
	if (!PreferredAutoPickupReceiver.IsValid())
	{
		GetWorldTimerManager().ClearTimer(HarvestAutoPickupTimerHandle);
		return;
	}
	if (!IsPreferredReceiverWithinRange())
	{
		return;
	}

	AActor* ReceiverActor = PreferredAutoPickupReceiver.Get();
	bPickupClaimed = true;
	const bool bCollected =
		IAIREHarvestRewardReceiver::Execute_TryReceiveHarvestReward(
			ReceiverActor,
			HarvestDeliveryId,
			ItemAsset->ItemId,
			ItemCount);
	if (!bCollected)
	{
		bPickupClaimed = false;
		return;
	}

	GetWorldTimerManager().ClearTimer(HarvestAutoPickupTimerHandle);
	if (!IsValid(ReceiverActor))
	{
		Destroy();
		return;
	}

	StartCompanionPickupPresentation(*ReceiverActor);
}

bool AAI_REItemActor::IsPreferredReceiverWithinRange() const
{
	const AActor* ReceiverActor = PreferredAutoPickupReceiver.Get();
	return IsValid(ReceiverActor)
		&& FVector::DistSquared(GetActorLocation(), ReceiverActor->GetActorLocation())
			<= FMath::Square(FMath::Max(0.0f, CompanionAutoPickupRadius));
}

void AAI_REItemActor::StartCompanionPickupPresentation(AActor& ReceiverActor)
{
	GetWorldTimerManager().ClearTimer(HarvestAutoPickupTimerHandle);
	PickupPresentationTarget = &ReceiverActor;
	PickupPresentationStartLocation = GetActorLocation();
	PickupPresentationStartScale = GetActorScale3D();
	PickupPresentationElapsedTime = 0.0f;
	SphereComponent->SetGenerateOverlapEvents(false);
	SetActorEnableCollision(false);
	MeshComponent->SetSimulatePhysics(false);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->AttachToComponent(
		SphereComponent,
		FAttachmentTransformRules::KeepWorldTransform);

	if (CompanionPickupTravelDuration <= 0.0f)
	{
		Destroy();
		return;
	}

	SetLifeSpan(CompanionPickupTravelDuration + 0.5f);
	SetActorTickEnabled(true);
}

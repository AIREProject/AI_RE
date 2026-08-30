#include "AI_REItemActor.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "AI_RECharacter.h"
#include "AI_REPlayerInventoryComponent.h"
#include "AIREHarvestRewardReceiver.h"
#include "../../OBI/Component/Public/AI_REItemDataAsset.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float WorldDropGravityAcceleration = 980.0f;
	constexpr float WorldDropMaximumFallSpeed = 2000.0f;
	constexpr float WorldDropSweepRadius = 12.0f;
}

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

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> DropEffectFinder(
		TEXT("/Game/VFX/DrapEffet/VFX/NE_drop_effects01.NE_drop_effects01"));
	if (DropEffectFinder.Succeeded())
	{
		DropHighlightEffect = DropEffectFinder.Object;
	}
}

void AAI_REItemActor::BeginPlay()
{
	Super::BeginPlay();

	// Only runtime-spawned drops settle. Level-authored item actors keep their
	// exact authored transform and existing collision setup.
	if (!HasAnyFlags(RF_WasLoaded))
	{
		bRuntimeDrop = true;
		bWorldDropSettling = true;
		WorldDropAngularVelocity = FRotator(
			FMath::FRandRange(-90.0f, 90.0f),
			FMath::FRandRange(-90.0f, 90.0f),
			FMath::FRandRange(-180.0f, 180.0f));
		SetActorTickEnabled(true);
		if (IsValid(DropHighlightEffect))
		{
			DropHighlightComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
				DropHighlightEffect,
				MeshComponent,
				NAME_None,
				FVector::ZeroVector,
				FRotator::ZeroRotator,
				EAttachLocation::SnapToTarget,
				true);
		}
	}

	if (!bRuntimeDrop && !bHarvestAutoPickupEnabled)
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

bool AAI_REItemActor::IsRuntimeDrop() const
{
	return bRuntimeDrop;
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

	if (bWorldDropSettling)
	{
		TickWorldDropSettling(DeltaSeconds);
		if (bWorldDropSettling && !bPickupClaimed)
		{
			return;
		}
	}

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

void AAI_REItemActor::TickWorldDropSettling(const float DeltaSeconds)
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		bWorldDropSettling = false;
		return;
	}

	const float SafeDeltaSeconds = FMath::Clamp(DeltaSeconds, 0.0f, 0.05f);
	WorldDropVerticalVelocity = FMath::Max(
		WorldDropVerticalVelocity
			- WorldDropGravityAcceleration * SafeDeltaSeconds,
		-WorldDropMaximumFallSpeed);

	const FVector StartLocation = GetActorLocation();
	const FVector EndLocation = StartLocation
		+ FVector(0.0f, 0.0f, WorldDropVerticalVelocity * SafeDeltaSeconds);
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AIREWorldDropSettling));
	QueryParams.AddIgnoredActor(this);

	FHitResult HitResult;
	const bool bHitGround = World->SweepSingleByObjectType(
		HitResult,
		StartLocation,
		EndLocation,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(WorldDropSweepRadius),
		QueryParams);
	if (bHitGround)
	{
		SetActorLocation(HitResult.Location, false, nullptr, ETeleportType::TeleportPhysics);
		WorldDropVerticalVelocity = 0.0f;
		bWorldDropSettling = false;
		if (!bPickupClaimed)
		{
			SetActorTickEnabled(false);
		}
		return;
	}

	SetActorLocation(EndLocation, false, nullptr, ETeleportType::TeleportPhysics);
	AddActorLocalRotation(WorldDropAngularVelocity * SafeDeltaSeconds);
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
				StartCompanionPickupPresentation(*PlayerChar);
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
				StartCompanionPickupPresentation(*PlayerChar);
				return;
			}
			bPickupClaimed = false;
		}
	}
}

void AAI_REItemActor::PollHarvestAutoPickup()
{
	if (bPickupClaimed
		|| !IsValid(ItemAsset)
		|| ItemAsset->ItemId.IsNone()
		|| ItemCount <= 0)
	{
		return;
	}
	if (bHarvestAutoPickupEnabled
		&& PreferredAutoPickupReceiver.IsValid()
		&& IsPreferredReceiverWithinRange())
	{
		AActor* ReceiverActor = PreferredAutoPickupReceiver.Get();
		bPickupClaimed = true;
		const bool bCollected =
			IAIREHarvestRewardReceiver::Execute_TryReceiveHarvestReward(
				ReceiverActor,
				HarvestDeliveryId,
				ItemAsset->ItemId,
				ItemCount);
		if (bCollected)
		{
			GetWorldTimerManager().ClearTimer(HarvestAutoPickupTimerHandle);
			if (!IsValid(ReceiverActor))
			{
				Destroy();
				return;
			}
			StartCompanionPickupPresentation(*ReceiverActor);
			return;
		}
		bPickupClaimed = false;
	}

	if (TryCollectForPlayer())
	{
		return;
	}
	if (bHarvestAutoPickupEnabled
		&& !PreferredAutoPickupReceiver.IsValid()
		&& !bRuntimeDrop)
	{
		GetWorldTimerManager().ClearTimer(HarvestAutoPickupTimerHandle);
	}
}

bool AAI_REItemActor::TryCollectForPlayer()
{
	AAI_RECharacter* PlayerCharacter = Cast<AAI_RECharacter>(
		UGameplayStatics::GetPlayerPawn(this, 0));
	if (!IsValid(PlayerCharacter)
		|| FVector::DistSquared(
			GetActorLocation(), PlayerCharacter->GetActorLocation())
			> FMath::Square(FMath::Max(0.0f, CompanionAutoPickupRadius)))
	{
		return false;
	}

	const int32 PreviousCount = ItemCount;
	Interact_Implementation(PlayerCharacter);
	return bPickupClaimed || ItemCount < PreviousCount;
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
	bWorldDropSettling = false;
	WorldDropVerticalVelocity = 0.0f;
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

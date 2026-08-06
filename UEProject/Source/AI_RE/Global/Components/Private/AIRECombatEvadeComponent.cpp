#include "AIRECombatEvadeComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AIRECombatDamageTargetInterface.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"

UAIRECombatEvadeComponent::UAIRECombatEvadeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

bool UAIRECombatEvadeComponent::TryStartLateralDash(AActor* ThreatActor)
{
	ACharacter* Character = OwnerCharacter.Get();
	if (!CanStartLateralDash(ThreatActor))
	{
		return false;
	}

	const FVector ToThreat = (ThreatActor->GetActorLocation()
		- Character->GetActorLocation()).GetSafeNormal2D();
	if (ToThreat.IsNearlyZero())
	{
		return false;
	}
	FVector RightDirection = FVector::CrossProduct(
		FVector::UpVector,
		ToThreat).GetSafeNormal();
	if (FVector::DotProduct(RightDirection, Character->GetActorRightVector())
		< 0.0f)
	{
		RightDirection *= -1.0f;
	}
	const FVector LeftDirection = -RightDirection;
	const FVector StartLocation = Character->GetActorLocation();
	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const float CapsuleRadius = IsValid(Capsule)
		? Capsule->GetScaledCapsuleRadius()
		: Character->GetSimpleCollisionRadius();
	const float CapsuleHalfHeight = IsValid(Capsule)
		? Capsule->GetScaledCapsuleHalfHeight()
		: CapsuleRadius;
	const FCollisionShape CollisionShape = FCollisionShape::MakeCapsule(
		CapsuleRadius,
		CapsuleHalfHeight);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AIREAggroSwapEvade), false);
	QueryParams.AddIgnoredActor(Character);
	auto MeasureClearance = [Character, &StartLocation, &CollisionShape,
		&QueryParams, this](const FVector& Direction)
	{
		FHitResult Hit;
		const bool bHit = GetWorld()->SweepSingleByChannel(
			Hit,
			StartLocation,
			StartLocation + Direction * DashDistance,
			Character->GetActorQuat(),
			IsValid(Character->GetCapsuleComponent())
				? Character->GetCapsuleComponent()->GetCollisionObjectType()
				: ECC_Pawn,
			CollisionShape,
			QueryParams);
		return bHit ? FMath::Max(0.0f, Hit.Distance) : DashDistance;
	};
	const float RightClearance = MeasureClearance(RightDirection);
	const float LeftClearance = MeasureClearance(LeftDirection);
	const bool bChooseRight = RightClearance >= LeftClearance;
	const FVector LateralDirection = bChooseRight
		? RightDirection
		: LeftDirection;
	ActiveDashDistance = bChooseRight ? RightClearance : LeftClearance;

	if (AController* Controller = Character->GetController())
	{
		Controller->StopMovement();
	}
	if (IAbilitySystemInterface* AbilitySystemOwner =
		Cast<IAbilitySystemInterface>(Character))
	{
		if (UAbilitySystemComponent* AbilitySystem =
			AbilitySystemOwner->GetAbilitySystemComponent())
		{
			AbilitySystem->CancelAllAbilities();
		}
	}
	if (UCharacterMovementComponent* Movement =
		Character->GetCharacterMovement())
	{
		PreviousMovementMode = Movement->MovementMode;
		PreviousCustomMovementMode = Movement->CustomMovementMode;
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}

	DashDirection = LateralDirection;
	ElapsedTime = 0.0f;
	MovedDistance = 0.0f;
	bEvading = true;
	SetComponentTickEnabled(true);
	if (IsValid(EvadeMontage))
	{
		Character->PlayAnimMontage(EvadeMontage);
	}
	OnEvadeStarted.Broadcast();
	return true;
}

bool UAIRECombatEvadeComponent::CanStartLateralDash(
	const AActor* ThreatActor) const
{
	const ACharacter* Character = OwnerCharacter.Get();
	if (bEvading
		|| !IsValid(Character)
		|| !IsValid(ThreatActor)
		|| ThreatActor == Character
		|| !FMath::IsFinite(DashDistance)
		|| !FMath::IsFinite(DashDuration)
		|| DashDistance <= 0.0f
		|| DashDuration <= 0.0f)
	{
		return false;
	}
	return !(ThreatActor->GetActorLocation()
		- Character->GetActorLocation()).GetSafeNormal2D().IsNearlyZero();
}

void UAIRECombatEvadeComponent::CancelEvade()
{
	if (bEvading)
	{
		FinishEvade();
	}
}

bool UAIRECombatEvadeComponent::IsEvading() const
{
	return bEvading;
}

void UAIRECombatEvadeComponent::BeginPlay()
{
	Super::BeginPlay();
	OwnerCharacter = Cast<ACharacter>(GetOwner());
	SetComponentTickEnabled(false);
}

void UAIRECombatEvadeComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	CancelEvade();
	OwnerCharacter.Reset();
	OnEvadeStarted.Clear();
	OnEvadeFinished.Clear();
	Super::EndPlay(EndPlayReason);
}

void UAIRECombatEvadeComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ACharacter* Character = OwnerCharacter.Get();
	if (!bEvading || !IsValid(Character))
	{
		FinishEvade();
		return;
	}

	ElapsedTime += FMath::Max(0.0f, DeltaTime);
	const float TargetDistance = ActiveDashDistance
		* FMath::Clamp(ElapsedTime / DashDuration, 0.0f, 1.0f);
	const float StepDistance = FMath::Max(0.0f, TargetDistance - MovedDistance);
	FHitResult HitResult;
	const FVector PreviousLocation = Character->GetActorLocation();
	Character->AddActorWorldOffset(
		DashDirection * StepDistance,
		true,
		&HitResult,
		ETeleportType::None);
	MovedDistance += FVector::Dist2D(
		PreviousLocation,
		Character->GetActorLocation());
	if (HitResult.bBlockingHit || ElapsedTime >= DashDuration)
	{
		FinishEvade();
	}
}

void UAIRECombatEvadeComponent::FinishEvade()
{
	if (!bEvading)
	{
		SetComponentTickEnabled(false);
		return;
	}
	bEvading = false;
	SetComponentTickEnabled(false);
	if (ACharacter* Character = OwnerCharacter.Get())
	{
		if (IsValid(EvadeMontage))
		{
			Character->StopAnimMontage(EvadeMontage);
		}
		if (UCharacterMovementComponent* Movement =
			Character->GetCharacterMovement())
		{
			if (AIRECombatDamageTarget::IsAlive(Character))
			{
				Movement->SetMovementMode(
					PreviousMovementMode,
					PreviousCustomMovementMode);
			}
			else
			{
				Movement->DisableMovement();
			}
		}
	}
	OnEvadeFinished.Broadcast();
}

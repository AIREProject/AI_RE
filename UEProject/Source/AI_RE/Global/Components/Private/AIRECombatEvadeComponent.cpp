#include "AIRECombatEvadeComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AIRECombatDamageTargetInterface.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"

DEFINE_LOG_CATEGORY_STATIC(LogAIRECombatEvade, Log, All);

bool FAIRECombatEvadePlan::IsValid() const
{
	return ::IsValid(ThreatActor.Get())
		&& !Direction.ContainsNaN()
		&& !Direction.GetSafeNormal2D().IsNearlyZero()
		&& FMath::IsFinite(AvailableDistance)
		&& AvailableDistance > 0.0f;
}

UAIRECombatEvadeComponent::UAIRECombatEvadeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

bool UAIRECombatEvadeComponent::TryStartLateralDash(AActor* ThreatActor)
{
	FAIRECombatEvadePlan Plan;
	return BuildLateralDashPlan(ThreatActor, FGuid(), Plan)
		&& TryStartLateralDashPlan(Plan);
}

bool UAIRECombatEvadeComponent::BuildLateralDashPlan(
	const AActor* ThreatActor,
	const FGuid& TriggerExecutionId,
	FAIRECombatEvadePlan& OutPlan) const
{
	OutPlan = FAIRECombatEvadePlan();
	const ACharacter* Character = OwnerCharacter.Get();
	if (!CanStartLateralDash(ThreatActor))
	{
		return false;
	}
	const UWorld* World = GetWorld();
	if (!IsValid(World))
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
	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(AIRECombatEvade),
		false);
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
	OutPlan.Side = bChooseRight
		? EAIRECombatEvadeSide::Right
		: EAIRECombatEvadeSide::Left;
	OutPlan.Direction = bChooseRight
		? RightDirection
		: LeftDirection;
	OutPlan.AvailableDistance = bChooseRight
		? RightClearance
		: LeftClearance;
	OutPlan.ThreatActor = const_cast<AActor*>(ThreatActor);
	OutPlan.TriggerExecutionId = TriggerExecutionId;
	return OutPlan.IsValid();
}

bool UAIRECombatEvadeComponent::TryStartLateralDashPlan(
	const FAIRECombatEvadePlan& Plan,
	UGameplayAbility* IgnoredAbility)
{
	ACharacter* Character = OwnerCharacter.Get();
	if (!CanStartLateralDash(Plan.ThreatActor.Get())
		|| !Plan.IsValid()
		|| Plan.AvailableDistance > DashDistance
		|| !FMath::IsNearlyEqual(
			Plan.Direction.SizeSquared2D(),
			1.0f,
			KINDA_SMALL_NUMBER))
	{
		return false;
	}

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
			AbilitySystem->CancelAbilities(
				nullptr,
				nullptr,
				IgnoredAbility);
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

	ActiveThreatActor = Plan.ThreatActor.Get();
	ActiveTriggerExecutionId = Plan.TriggerExecutionId;
	DashDirection = Plan.Direction;
	ActiveDashDistance = Plan.AvailableDistance;
	ElapsedTime = 0.0f;
	MovedDistance = 0.0f;
	bEvading = true;
	SetComponentTickEnabled(true);
	if (IsValid(EvadeMontage))
	{
		const FName MontageSection = Plan.Side
			== EAIRECombatEvadeSide::Right
			? TEXT("Evade_R")
			: TEXT("Evade_L");
		if (EvadeMontage->HasRootMotion())
		{
			UE_LOG(
				LogAIRECombatEvade,
				Error,
				TEXT("Evade montage must be in-place; presentation skipped to avoid duplicate movement. Owner=%s Montage=%s"),
				*GetNameSafe(Character),
				*GetNameSafe(EvadeMontage));
		}
		else if (!EvadeMontage->IsValidSectionName(MontageSection))
		{
			UE_LOG(
				LogAIRECombatEvade,
				Warning,
				TEXT("Evade montage is missing section %s; presentation skipped. Owner=%s Montage=%s"),
				*MontageSection.ToString(),
				*GetNameSafe(Character),
				*GetNameSafe(EvadeMontage));
		}
		else
		{
			Character->PlayAnimMontage(
				EvadeMontage,
				1.0f,
				MontageSection);
		}
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
		|| !IsValid(GetWorld())
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
		FinishEvade(true);
	}
	else
	{
		StopEvadePresentation();
	}
}

bool UAIRECombatEvadeComponent::IsEvading() const
{
	return bEvading;
}

bool UAIRECombatEvadeComponent::IsEvadingFrom(
	const AActor* ThreatActor,
	const FGuid& TriggerExecutionId) const
{
	return bEvading
		&& IsValid(ThreatActor)
		&& TriggerExecutionId.IsValid()
		&& ActiveThreatActor.Get() == ThreatActor
		&& ActiveTriggerExecutionId == TriggerExecutionId;
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
	if (!bEvading
		|| !IsValid(Character)
		|| !ActiveThreatActor.IsValid())
	{
		FinishEvade(true);
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
	if (HitResult.bBlockingHit)
	{
		FinishEvade(true);
	}
	else if (ElapsedTime >= DashDuration)
	{
		FinishEvade(false);
	}
}

void UAIRECombatEvadeComponent::FinishEvade(const bool bStopPresentation)
{
	if (!bEvading)
	{
		if (bStopPresentation)
		{
			StopEvadePresentation();
		}
		SetComponentTickEnabled(false);
		return;
	}
	bEvading = false;
	SetComponentTickEnabled(false);
	if (bStopPresentation)
	{
		StopEvadePresentation();
	}
	if (ACharacter* Character = OwnerCharacter.Get())
	{
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
	ActiveThreatActor.Reset();
	ActiveTriggerExecutionId.Invalidate();
	DashDirection = FVector::ZeroVector;
	ElapsedTime = 0.0f;
	MovedDistance = 0.0f;
	ActiveDashDistance = 0.0f;
	OnEvadeFinished.Broadcast();
}

void UAIRECombatEvadeComponent::StopEvadePresentation()
{
	if (ACharacter* Character = OwnerCharacter.Get();
		IsValid(Character) && IsValid(EvadeMontage))
	{
		Character->StopAnimMontage(EvadeMontage);
	}
}

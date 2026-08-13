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

bool UAIRECombatEvadeComponent::TryStartDirectionalDash(
	const FVector& WorldDirection)
{
	const ACharacter* Character = OwnerCharacter.Get();
	const FVector Direction = WorldDirection.GetSafeNormal2D();
	if (bEvading
		|| !IsValid(Character)
		|| !IsValid(GetWorld())
		|| WorldDirection.ContainsNaN()
		|| !FMath::IsFinite(DashDistance)
		|| !FMath::IsFinite(DashDuration)
		|| DashDistance <= 0.0f
		|| DashDuration <= 0.0f
		|| Direction.IsNearlyZero())
	{
		return false;
	}

	const float ForwardAmount = FVector::DotProduct(
		Direction,
		Character->GetActorForwardVector());
	const float RightAmount = FVector::DotProduct(
		Direction,
		Character->GetActorRightVector());
	const EAIRECombatEvadeSide PresentationSide =
		FMath::Abs(ForwardAmount) >= FMath::Abs(RightAmount)
			? (ForwardAmount >= 0.0f
				? EAIRECombatEvadeSide::Forward
				: EAIRECombatEvadeSide::Backward)
			: (RightAmount >= 0.0f
				? EAIRECombatEvadeSide::Right
				: EAIRECombatEvadeSide::Left);
	return TryStartDash(
		Direction,
		DashDistance,
		PresentationSide,
		nullptr,
		FGuid(),
		nullptr,
		DashDistance,
		false);
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
	return TryStartDash(
		Plan.Direction,
		Plan.AvailableDistance,
		Plan.Side,
		Plan.ThreatActor.Get(),
		Plan.TriggerExecutionId,
		IgnoredAbility,
		DashDistance,
		false);
}

bool UAIRECombatEvadeComponent::BuildThreatRetreatDashPlan(
	const AActor* ThreatActor,
	const FGuid& TriggerExecutionId,
	const float RetreatDistance,
	FAIRECombatEvadePlan& OutPlan) const
{
	OutPlan = FAIRECombatEvadePlan();
	const ACharacter* Character = OwnerCharacter.Get();
	if (bEvading
		|| !IsValid(Character)
		|| !IsValid(ThreatActor)
		|| ThreatActor == Character
		|| !IsValid(GetWorld())
		|| !FMath::IsFinite(RetreatDistance)
		|| RetreatDistance <= 0.0f)
	{
		return false;
	}

	const FVector Direction = (Character->GetActorLocation()
		- ThreatActor->GetActorLocation()).GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		return false;
	}
	const float ForwardAmount = FVector::DotProduct(
		Direction,
		Character->GetActorForwardVector());
	const float RightAmount = FVector::DotProduct(
		Direction,
		Character->GetActorRightVector());
	OutPlan.Side = FMath::Abs(ForwardAmount) >= FMath::Abs(RightAmount)
		? (ForwardAmount >= 0.0f
			? EAIRECombatEvadeSide::Forward
			: EAIRECombatEvadeSide::Backward)
		: (RightAmount >= 0.0f
			? EAIRECombatEvadeSide::Right
			: EAIRECombatEvadeSide::Left);
	OutPlan.Direction = Direction;
	OutPlan.AvailableDistance = RetreatDistance;
	OutPlan.ThreatActor = const_cast<AActor*>(ThreatActor);
	OutPlan.TriggerExecutionId = TriggerExecutionId;
	return OutPlan.IsValid();
}

bool UAIRECombatEvadeComponent::TryStartAggroSwapDashPlan(
	const FAIRECombatEvadePlan& Plan)
{
	if (!Plan.IsValid()
		|| !FMath::IsNearlyEqual(
			Plan.Direction.SizeSquared2D(),
			1.0f,
			KINDA_SMALL_NUMBER))
	{
		return false;
	}
	return TryStartDash(
		Plan.Direction,
		Plan.AvailableDistance,
		Plan.Side,
		Plan.ThreatActor.Get(),
		Plan.TriggerExecutionId,
		nullptr,
		Plan.AvailableDistance,
		true);
}

bool UAIRECombatEvadeComponent::TryStartDash(
	const FVector& Direction,
	const float AvailableDistance,
	const EAIRECombatEvadeSide PresentationSide,
	AActor* ThreatActor,
	const FGuid& TriggerExecutionId,
	UGameplayAbility* IgnoredAbility,
	const float MaximumDistance,
	const bool bAllowAirborne)
{
	ACharacter* Character = OwnerCharacter.Get();
	UCharacterMovementComponent* Movement = IsValid(Character)
		? Character->GetCharacterMovement()
		: nullptr;
	if (bEvading
		|| !IsValid(Character)
		|| !IsValid(Movement)
		|| (!bAllowAirborne && Movement->IsFalling())
		|| !IsValid(GetWorld())
		|| !FMath::IsFinite(DashDuration)
		|| DashDuration <= 0.0f
		|| Direction.ContainsNaN()
		|| Direction.IsNearlyZero()
		|| !FMath::IsFinite(AvailableDistance)
		|| !FMath::IsFinite(MaximumDistance)
		|| AvailableDistance <= 0.0f
		|| MaximumDistance <= 0.0f
		|| AvailableDistance > MaximumDistance)
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
	PreviousMovementMode = Movement->MovementMode;
	PreviousCustomMovementMode = Movement->CustomMovementMode;
	Movement->StopMovementImmediately();
	Movement->DisableMovement();

	ActiveThreatActor = ThreatActor;
	ActiveTriggerExecutionId = TriggerExecutionId;
	DashDirection = Direction;
	ActiveDashDistance = AvailableDistance;
	ElapsedTime = 0.0f;
	MovedDistance = 0.0f;
	bEvading = true;
	bRequiresActiveThreat = IsValid(ThreatActor);
	SetComponentTickEnabled(true);
	if (IsValid(EvadeMontage))
	{
		FName MontageSection;
		switch (PresentationSide)
		{
		case EAIRECombatEvadeSide::Forward:
			MontageSection = TEXT("Evade_F");
			break;
		case EAIRECombatEvadeSide::Backward:
			MontageSection = TEXT("Evade_B");
			break;
		case EAIRECombatEvadeSide::Right:
			MontageSection = TEXT("Evade_R");
			break;
		case EAIRECombatEvadeSide::Left:
		default:
			MontageSection = TEXT("Evade_L");
			break;
		}
		if (!EvadeMontage->IsValidSectionName(MontageSection))
		{
			const FName LateralFallbackSection = FVector::DotProduct(
				Direction,
				Character->GetActorRightVector()) >= 0.0f
				? TEXT("Evade_R")
				: TEXT("Evade_L");
			if (EvadeMontage->IsValidSectionName(LateralFallbackSection))
			{
				MontageSection = LateralFallbackSection;
			}
		}
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

float UAIRECombatEvadeComponent::MeasureClearance(
	const FVector& Direction) const
{
	const ACharacter* Character = OwnerCharacter.Get();
	const UWorld* World = GetWorld();
	if (!IsValid(Character)
		|| !IsValid(World)
		|| Direction.GetSafeNormal2D().IsNearlyZero())
	{
		return 0.0f;
	}
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
	FHitResult Hit;
	const FVector NormalizedDirection = Direction.GetSafeNormal2D();
	const bool bHit = World->SweepSingleByChannel(
		Hit,
		Character->GetActorLocation(),
		Character->GetActorLocation() + NormalizedDirection * DashDistance,
		Character->GetActorQuat(),
		IsValid(Capsule) ? Capsule->GetCollisionObjectType() : ECC_Pawn,
		CollisionShape,
		QueryParams);
	return bHit ? FMath::Max(0.0f, Hit.Distance) : DashDistance;
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
		|| (bRequiresActiveThreat && !ActiveThreatActor.IsValid()))
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
	FVector StepDelta = DashDirection * StepDistance;
	if (PreviousMovementMode != MOVE_Falling)
	{
		if (UCharacterMovementComponent* Movement =
			Character->GetCharacterMovement())
		{
			FFindFloorResult FloorResult;
			Movement->FindFloor(
				Character->GetActorLocation(),
				FloorResult,
				false);
			if (FloorResult.IsWalkableFloor())
			{
				const FVector GroundDirection = FVector::VectorPlaneProject(
					DashDirection,
					FloorResult.HitResult.ImpactNormal);
				const float HorizontalMagnitude = GroundDirection.Size2D();
				if (HorizontalMagnitude > UE_KINDA_SMALL_NUMBER)
				{
					StepDelta = GroundDirection
						* (StepDistance / HorizontalMagnitude);
				}
			}
		}
	}
	Character->AddActorWorldOffset(
		StepDelta,
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
		FinishEvade(true);
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
	bRequiresActiveThreat = false;
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

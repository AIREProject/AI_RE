#include "LocalAI/StateTree/AIRECompanionStateTree.h"

#include "Core/AIRECompanionAIController.h"
#include "Core/AIRECompanionCharacter.h"
#include "Core/AIRECompanionConfigDataAsset.h"
#include "Equipment/AIRECompanionEquipmentComponent.h"
#include "Inventory/AIRECompanionInventoryComponent.h"
#include "Policy/AIRECompanionLocalBehaviorPolicyComponent.h"
#include "Support/AIRECompanionSupportComponent.h"
#include "AbilitySystem/Core/AIRECompanionGameplayTags.h"
#include "Threat/AIRECompanionThreatComponent.h"
#include "Equipment/AIRECompanionWeaponDefinitionDataAsset.h"
#include "LocalAI/Threat/AIREThreatTargetInterface.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "StateTreeExecutionContext.h"

DEFINE_LOG_CATEGORY_STATIC(LogAIRECompanionStateTree, Log, All);

namespace
{
	constexpr float CombatApproachMargin = 50.0f;
	constexpr float CombatApproachAcceptanceRadius = 25.0f;
	constexpr float CombatRangeExitSlack = 25.0f;
	constexpr float CombatMovementRetryInterval = 0.5f;
	constexpr float CombatActivationRetryInterval = 0.1f;
	constexpr float SupportApproachMargin = 50.0f;
	constexpr float SupportApproachAcceptanceRadius = 25.0f;
	constexpr float SupportMovementRetryInterval = 0.5f;
	constexpr float SupportActivationRetryInterval = 0.1f;

	FVector CalculateCombatApproachLocation(
		const APawn& CompanionPawn,
		const AActor& TargetActor,
		const float AttackRange)
	{
		FVector TargetToCompanion =
			CompanionPawn.GetActorLocation() - TargetActor.GetActorLocation();
		TargetToCompanion.Z = 0.0f;
		if (!TargetToCompanion.Normalize())
		{
			TargetToCompanion = -TargetActor.GetActorForwardVector().GetSafeNormal2D();
		}

		const float DesiredSurfaceDistance = FMath::Max(
			0.0f,
			AttackRange - CombatApproachMargin);
		const float DesiredCenterDistance =
			CompanionPawn.GetSimpleCollisionRadius()
			+ TargetActor.GetSimpleCollisionRadius()
			+ DesiredSurfaceDistance;

		return TargetActor.GetActorLocation()
			+ TargetToCompanion * DesiredCenterDistance;
	}

	FVector CalculateSupportApproachLocation(
		const APawn& CompanionPawn,
		const AActor& TargetActor,
		const float SupportRange)
	{
		FVector TargetToCompanion =
			CompanionPawn.GetActorLocation()
			- TargetActor.GetActorLocation();
		TargetToCompanion.Z = 0.0f;
		if (!TargetToCompanion.Normalize())
		{
			TargetToCompanion =
				-TargetActor.GetActorForwardVector().GetSafeNormal2D();
		}

		const float DesiredSurfaceDistance = FMath::Max(
			0.0f,
			SupportRange - SupportApproachMargin);
		const float DesiredCenterDistance =
			CompanionPawn.GetSimpleCollisionRadius()
			+ TargetActor.GetSimpleCollisionRadius()
			+ DesiredSurfaceDistance;
		return TargetActor.GetActorLocation()
			+ TargetToCompanion * DesiredCenterDistance;
	}

	enum class EAIRECompanionBehaviorInput : uint16
	{
		HasPlayer = 1 << 0,
		ShouldFollow = 1 << 1,
		ShouldReturn = 1 << 2,
		Disabled = 1 << 3,
		Survival = 1 << 4,
		Combat = 1 << 5,
		DirectCommand = 1 << 6,
		Work = 1 << 7,
		Support = 1 << 8
	};
	ENUM_CLASS_FLAGS(EAIRECompanionBehaviorInput);

	void AddBehaviorInput(
		EAIRECompanionBehaviorInput& InputMask,
		const EAIRECompanionBehaviorInput Input,
		const bool bIsActive)
	{
		if (bIsActive)
		{
			EnumAddFlags(InputMask, Input);
		}
	}

	FString GetBehaviorStateName(const EAIRECompanionBehaviorState BehaviorState)
	{
		const UEnum* BehaviorStateEnum = StaticEnum<EAIRECompanionBehaviorState>();
		return IsValid(BehaviorStateEnum)
			? BehaviorStateEnum->GetNameStringByValue(static_cast<int64>(BehaviorState))
			: TEXT("Unknown");
	}
}

void FAIRECompanionContextEvaluator::TreeStart(FStateTreeExecutionContext& Context) const
{
	UpdateContext(Context);
}

void FAIRECompanionContextEvaluator::TreeStop(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.PlayerPawn = nullptr;
	InstanceData.ThreatTarget = nullptr;
	InstanceData.SupportTarget = nullptr;
	InstanceData.EquipmentComponent = nullptr;
	InstanceData.InventoryComponent = nullptr;
	InstanceData.SupportComponent = nullptr;
	InstanceData.AbilitySystemComponent = nullptr;
	InstanceData.DistanceToPlayer = 0.0f;
	InstanceData.MovementSpeed = 0.0f;
	InstanceData.bIsRunning = false;
	InstanceData.FollowStopDistance = 0.0f;
	InstanceData.ReturnStartDistance = 0.0f;
	InstanceData.CombatDistance = 0.0f;
	InstanceData.CombatCooldown = 0.0f;
	InstanceData.bHasPlayer = false;
	InstanceData.bShouldFollow = false;
	InstanceData.bShouldReturn = false;
	InstanceData.bIsDisabledRequested = false;
	InstanceData.bIsSurvivalRequested = false;
	InstanceData.bIsCombatRequested = false;
	InstanceData.bIsSupportRequested = false;
	InstanceData.bIsDirectCommandRequested = false;
	InstanceData.bIsWorkRequested = false;
	InstanceData.bBehaviorSelectionChanged = false;
	InstanceData.PreviousBehaviorInputMask = 0;
	InstanceData.bHasPreviousBehaviorInput = false;
	InstanceData.PreviousPlayerPawn.Reset();
	InstanceData.PreviousThreatTarget.Reset();
	InstanceData.PreviousSupportTarget.Reset();
}

void FAIRECompanionContextEvaluator::Tick(FStateTreeExecutionContext& Context, const float) const
{
	UpdateContext(Context);
}

void FAIRECompanionContextEvaluator::UpdateContext(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.PlayerPawn = nullptr;
	InstanceData.ThreatTarget = nullptr;
	InstanceData.SupportTarget = nullptr;
	InstanceData.EquipmentComponent = nullptr;
	InstanceData.InventoryComponent = nullptr;
	InstanceData.SupportComponent = nullptr;
	InstanceData.AbilitySystemComponent = nullptr;
	InstanceData.DistanceToPlayer = 0.0f;
	InstanceData.MovementSpeed = 0.0f;
	InstanceData.FollowStopDistance = 0.0f;
	InstanceData.ReturnStartDistance = 0.0f;
	InstanceData.CombatDistance = 0.0f;
	InstanceData.CombatCooldown = 0.0f;
	InstanceData.bHasPlayer = false;
	InstanceData.bShouldFollow = false;
	InstanceData.bShouldReturn = false;
	InstanceData.bIsDisabledRequested = false;
	InstanceData.bIsSurvivalRequested = false;
	InstanceData.bIsCombatRequested = false;
	InstanceData.bIsSupportRequested = false;
	InstanceData.bIsDirectCommandRequested = false;
	InstanceData.bIsWorkRequested = false;

	if (IsValid(InstanceData.CompanionController))
	{
		InstanceData.bIsDisabledRequested = InstanceData.CompanionController->IsTestBehaviorRequestActive(
			EAIRECompanionTestBehaviorRequest::Disabled);
		InstanceData.bIsSurvivalRequested = InstanceData.CompanionController->IsTestBehaviorRequestActive(
			EAIRECompanionTestBehaviorRequest::Survival);
		InstanceData.bIsDirectCommandRequested = InstanceData.CompanionController->IsTestBehaviorRequestActive(
			EAIRECompanionTestBehaviorRequest::DirectCommand);
		InstanceData.bIsWorkRequested = InstanceData.CompanionController->IsTestBehaviorRequestActive(
			EAIRECompanionTestBehaviorRequest::Work);

		const UAIRECompanionThreatComponent* ThreatComponent = InstanceData.CompanionController->GetThreatComponent();
		if (IsValid(ThreatComponent))
		{
			InstanceData.ThreatTarget = ThreatComponent->GetSelectedThreatTarget();
			InstanceData.bIsCombatRequested = ThreatComponent->IsCombatRequested();
		}
	}

	if (IsValid(InstanceData.CompanionCharacter))
	{
		InstanceData.EquipmentComponent =
			InstanceData.CompanionCharacter->GetEquipmentComponent();
		InstanceData.InventoryComponent =
			InstanceData.CompanionCharacter->GetInventoryComponent();
		InstanceData.SupportComponent =
			InstanceData.CompanionCharacter->GetSupportComponent();
		InstanceData.AbilitySystemComponent =
			InstanceData.CompanionCharacter->GetAbilitySystemComponent();
		if (IsValid(InstanceData.SupportComponent))
		{
			InstanceData.SupportTarget =
				InstanceData.SupportComponent->GetSupportTarget();
			InstanceData.bIsSupportRequested =
				InstanceData.SupportComponent->IsSupportRequested();
		}
		InstanceData.bIsDisabledRequested = InstanceData.bIsDisabledRequested
			|| InstanceData.CompanionCharacter->IsAbilitySystemDisabled();

		if (InstanceData.bIsCombatRequested
			&& InstanceData.bIsSupportRequested)
		{
			const UAIRECompanionLocalBehaviorPolicyComponent* PolicyComponent =
				InstanceData.CompanionCharacter
					->GetLocalBehaviorPolicyComponent();
			if (IsValid(PolicyComponent)
				&& PolicyComponent->GetLocalBehaviorPolicy().RolePreference
					== EAIRECompanionRolePreference::SupportPriority)
			{
				InstanceData.bIsCombatRequested = false;
			}
		}

		const UAIRECompanionConfigDataAsset* CompanionConfig = InstanceData.CompanionCharacter->GetCompanionConfig();
		if (IsValid(CompanionConfig))
		{
			InstanceData.FollowStopDistance = CompanionConfig->FollowStopDistance;
			InstanceData.ReturnStartDistance = CompanionConfig->ReturnStartDistance;

			const UWorld* World = InstanceData.CompanionCharacter->GetWorld();
			APawn* CurrentPlayerPawn = IsValid(World) ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
			if (IsValid(CurrentPlayerPawn))
			{
				InstanceData.PlayerPawn = CurrentPlayerPawn;
				InstanceData.DistanceToPlayer = FVector::Distance(
					InstanceData.CompanionCharacter->GetActorLocation(),
					CurrentPlayerPawn->GetActorLocation());
				InstanceData.bHasPlayer = true;
				InstanceData.bShouldFollow = InstanceData.DistanceToPlayer > InstanceData.FollowStopDistance;
				InstanceData.bShouldReturn = InstanceData.DistanceToPlayer > InstanceData.ReturnStartDistance;
			}

			if (InstanceData.bIsCombatRequested || InstanceData.bShouldReturn)
			{
				InstanceData.bIsRunning = true;
			}
			else if (InstanceData.bIsRunning)
			{
				InstanceData.bIsRunning =
					InstanceData.DistanceToPlayer > CompanionConfig->WalkResumeDistance;
			}
			else
			{
				InstanceData.bIsRunning =
					InstanceData.DistanceToPlayer >= CompanionConfig->RunStartDistance;
			}

			InstanceData.MovementSpeed = InstanceData.bIsRunning
				? CompanionConfig->MovementSpeed
				: CompanionConfig->WalkSpeed;
			if (UCharacterMovementComponent* MovementComponent =
					InstanceData.CompanionCharacter->GetCharacterMovement();
				IsValid(MovementComponent)
				&& !FMath::IsNearlyEqual(
					MovementComponent->MaxWalkSpeed,
					InstanceData.MovementSpeed))
			{
				MovementComponent->MaxWalkSpeed = InstanceData.MovementSpeed;
			}
		}
	}

	EAIRECompanionBehaviorInput BehaviorInputMask = static_cast<EAIRECompanionBehaviorInput>(0);
	AddBehaviorInput(BehaviorInputMask, EAIRECompanionBehaviorInput::HasPlayer, InstanceData.bHasPlayer);
	AddBehaviorInput(BehaviorInputMask, EAIRECompanionBehaviorInput::ShouldFollow, InstanceData.bShouldFollow);
	AddBehaviorInput(BehaviorInputMask, EAIRECompanionBehaviorInput::ShouldReturn, InstanceData.bShouldReturn);
	AddBehaviorInput(BehaviorInputMask, EAIRECompanionBehaviorInput::Disabled, InstanceData.bIsDisabledRequested);
	AddBehaviorInput(BehaviorInputMask, EAIRECompanionBehaviorInput::Survival, InstanceData.bIsSurvivalRequested);
	AddBehaviorInput(BehaviorInputMask, EAIRECompanionBehaviorInput::Combat, InstanceData.bIsCombatRequested);
	AddBehaviorInput(BehaviorInputMask, EAIRECompanionBehaviorInput::Support, InstanceData.bIsSupportRequested);
	AddBehaviorInput(BehaviorInputMask, EAIRECompanionBehaviorInput::DirectCommand, InstanceData.bIsDirectCommandRequested);
	AddBehaviorInput(BehaviorInputMask, EAIRECompanionBehaviorInput::Work, InstanceData.bIsWorkRequested);

	const uint16 CurrentBehaviorInputMask = static_cast<uint16>(BehaviorInputMask);
	const bool bPlayerPawnChanged = InstanceData.PreviousPlayerPawn.Get() != InstanceData.PlayerPawn.Get();
	const bool bThreatTargetChanged = InstanceData.PreviousThreatTarget.Get() != InstanceData.ThreatTarget.Get();
	const bool bSupportTargetChanged =
		InstanceData.PreviousSupportTarget.Get()
		!= InstanceData.SupportTarget.Get();
	InstanceData.bBehaviorSelectionChanged = !InstanceData.bHasPreviousBehaviorInput
		|| InstanceData.PreviousBehaviorInputMask != CurrentBehaviorInputMask
		|| bPlayerPawnChanged
		|| bThreatTargetChanged
		|| bSupportTargetChanged;
	InstanceData.PreviousBehaviorInputMask = CurrentBehaviorInputMask;
	InstanceData.bHasPreviousBehaviorInput = true;
	InstanceData.PreviousPlayerPawn = InstanceData.PlayerPawn;
	InstanceData.PreviousThreatTarget = InstanceData.ThreatTarget;
	InstanceData.PreviousSupportTarget = InstanceData.SupportTarget;
}

FAIREApplyCompanionMovementSettingsTask::FAIREApplyCompanionMovementSettingsTask()
{
	bShouldCallTick = false;
	bShouldStateChangeOnReselect = false;
}

EStateTreeRunStatus FAIREApplyCompanionMovementSettingsTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult&) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!IsValid(InstanceData.CompanionCharacter))
	{
		UE_LOG(LogAIRECompanionStateTree, Warning, TEXT("Cannot apply movement settings without a valid Companion Character."));
		return EStateTreeRunStatus::Failed;
	}

	UCharacterMovementComponent* MovementComponent = InstanceData.CompanionCharacter->GetCharacterMovement();
	if (!IsValid(MovementComponent))
	{
		UE_LOG(
			LogAIRECompanionStateTree,
			Warning,
			TEXT("Companion %s has no valid Character Movement Component."),
			*GetNameSafe(InstanceData.CompanionCharacter));
		return EStateTreeRunStatus::Failed;
	}

	if (!FMath::IsFinite(InstanceData.MovementSpeed) || InstanceData.MovementSpeed <= 0.0f)
	{
		UE_LOG(
			LogAIRECompanionStateTree,
			Warning,
			TEXT("Companion %s received invalid movement speed %.2f from its StateTree binding."),
			*GetNameSafe(InstanceData.CompanionCharacter),
			InstanceData.MovementSpeed);
		return EStateTreeRunStatus::Failed;
	}

	MovementComponent->MaxWalkSpeed = InstanceData.MovementSpeed;
	return EStateTreeRunStatus::Succeeded;
}

FAIRECompanionIdleTask::FAIRECompanionIdleTask()
{
	bShouldCallTick = false;
	bShouldStateChangeOnReselect = false;
}

EStateTreeRunStatus FAIRECompanionIdleTask::EnterState(
	FStateTreeExecutionContext&,
	const FStateTreeTransitionResult&) const
{
	return EStateTreeRunStatus::Running;
}

FAIRECompanionBehaviorDebugTask::FAIRECompanionBehaviorDebugTask()
{
	bShouldCallTick = false;
	bShouldStateChangeOnReselect = false;
}

EStateTreeRunStatus FAIRECompanionBehaviorDebugTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	UE_LOG(
		LogAIRECompanionStateTree,
		Log,
		TEXT("Companion behavior entered. State=%s Target=%s Priority=%s ChangeType=%s"),
		*GetBehaviorStateName(InstanceData.BehaviorState),
		*GetNameSafe(InstanceData.TargetActor),
		*StaticEnum<EStateTreeTransitionPriority>()->GetNameStringByValue(static_cast<int64>(Transition.Priority)),
		*StaticEnum<EStateTreeStateChangeType>()->GetNameStringByValue(static_cast<int64>(Transition.ChangeType)));
	return EStateTreeRunStatus::Succeeded;
}

void FAIRECompanionBehaviorDebugTask::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	UE_LOG(
		LogAIRECompanionStateTree,
		Log,
		TEXT("Companion behavior exited. State=%s Target=%s RunStatus=%s MoveCancellationBoundary=%s"),
		*GetBehaviorStateName(InstanceData.BehaviorState),
		*GetNameSafe(InstanceData.TargetActor),
		*StaticEnum<EStateTreeRunStatus>()->GetNameStringByValue(static_cast<int64>(Transition.CurrentRunStatus)),
		InstanceData.bOwnsMovementRequest ? TEXT("StateExit") : TEXT("None"));
}

FAIRECompanionEngageThreatTask::FAIRECompanionEngageThreatTask()
{
	bShouldCallTick = true;
	bShouldStateChangeOnReselect = false;
}

EStateTreeRunStatus FAIRECompanionEngageThreatTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult&) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!IsValid(InstanceData.CompanionController)
		|| !IsValid(InstanceData.EquipmentComponent)
		|| !IsValid(InstanceData.AbilitySystemComponent))
	{
		UE_LOG(
			LogAIRECompanionStateTree,
			Warning,
			TEXT("Engage Threat received invalid component bindings."));
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.ActiveTarget = InstanceData.ThreatTarget;
	InstanceData.RetryTimeRemaining = 0.0f;
	InstanceData.bMoveRequested = false;
	InstanceData.bSkillIntentBuffered = false;
	InstanceData.bSkillIntentEvaluatedForStep = false;
	InstanceData.bWasSkillCancelWindowOpen = false;
	InstanceData.bWasBasicAttackActive = false;
	UE_LOG(
		LogAIRECompanionStateTree,
		Log,
		TEXT("Companion threat engagement started. Companion=%s Target=%s"),
		*GetNameSafe(InstanceData.CompanionController->GetPawn()),
		*GetNameSafe(InstanceData.ThreatTarget));
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FAIRECompanionEngageThreatTask::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!IsValid(InstanceData.CompanionController)
		|| !IsValid(InstanceData.EquipmentComponent)
		|| !IsValid(InstanceData.AbilitySystemComponent))
	{
		CancelOwnedRequests(InstanceData);
		return EStateTreeRunStatus::Failed;
	}

	if (InstanceData.ActiveTarget.Get() != InstanceData.ThreatTarget)
	{
		CancelOwnedRequests(InstanceData);
		InstanceData.ActiveTarget = InstanceData.ThreatTarget;
		InstanceData.RetryTimeRemaining = 0.0f;
		InstanceData.bSkillIntentBuffered = false;
		InstanceData.bSkillIntentEvaluatedForStep = false;
		InstanceData.bWasSkillCancelWindowOpen = false;
		InstanceData.bWasBasicAttackActive = false;
	}

	APawn* CompanionPawn = InstanceData.CompanionController->GetPawn();
	AActor* TargetActor = InstanceData.ActiveTarget.Get();
	if (!IsValid(CompanionPawn) || !IsTargetUsable(TargetActor))
	{
		CancelOwnedRequests(InstanceData);
		return EStateTreeRunStatus::Running;
	}

	const UAIRECompanionWeaponDefinitionDataAsset* WeaponDefinition =
		InstanceData.EquipmentComponent->GetCurrentWeaponDefinition();
	if (!IsValid(WeaponDefinition))
	{
		CancelOwnedRequests(InstanceData);
		return EStateTreeRunStatus::Running;
	}
	const float AttackRange = WeaponDefinition->AttackRange;

	InstanceData.CompanionController->SetFocus(
		TargetActor,
		EAIFocusPriority::Gameplay);

	bool bAttackActive = InstanceData.AbilitySystemComponent->HasMatchingGameplayTag(
		AIRECompanionGameplayTags::StateActionAttacking);
	const bool bCombatSkillWasActive =
		InstanceData.AbilitySystemComponent->HasMatchingGameplayTag(
			AIRECompanionGameplayTags::StateActionAttackingSkill);
	const float ActiveAttackRange = bCombatSkillWasActive
		? WeaponDefinition->CombatSkill.AttackRange
		: AttackRange;
	const float AttackExitDistance =
		ActiveAttackRange + CombatRangeExitSlack;
	if (bAttackActive
		&& !IsTargetInRange(*CompanionPawn, *TargetActor, AttackExitDistance))
	{
		const FGameplayTag CombatAbilityTags[] =
		{
			AIRECompanionGameplayTags::AbilityCombatBasicAttack,
			AIRECompanionGameplayTags::AbilityCombatSkill
		};
		for (const FGameplayTag CombatAbilityTag : CombatAbilityTags)
		{
			const FGameplayAbilitySpecHandle AbilityHandle =
				InstanceData.EquipmentComponent->FindGrantedAbilityHandle(
					CombatAbilityTag);
			if (AbilityHandle.IsValid())
			{
				InstanceData.AbilitySystemComponent->CancelAbilityHandle(
					AbilityHandle);
			}
		}
		bAttackActive = InstanceData.AbilitySystemComponent->HasMatchingGameplayTag(
			AIRECompanionGameplayTags::StateActionAttacking);
		InstanceData.RetryTimeRemaining = 0.0f;
		InstanceData.bSkillIntentBuffered = false;
		InstanceData.bSkillIntentEvaluatedForStep = false;
		InstanceData.bWasSkillCancelWindowOpen = false;
		InstanceData.bWasBasicAttackActive = false;
	}

	InstanceData.RetryTimeRemaining = FMath::Max(
		0.0f,
		InstanceData.RetryTimeRemaining - DeltaTime);
	if (InstanceData.bMoveRequested
		&& InstanceData.CompanionController->GetMoveStatus()
			!= EPathFollowingStatus::Moving)
	{
		InstanceData.bMoveRequested = false;
	}

	if (!IsTargetInRange(*CompanionPawn, *TargetActor, AttackRange))
	{
		if (!bAttackActive
			&& !InstanceData.bMoveRequested
			&& InstanceData.RetryTimeRemaining <= 0.0f)
		{
			const FVector ApproachLocation = CalculateCombatApproachLocation(
				*CompanionPawn,
				*TargetActor,
				AttackRange);
			const EPathFollowingRequestResult::Type MoveResult =
				InstanceData.CompanionController->MoveToLocation(
					ApproachLocation,
					CombatApproachAcceptanceRadius,
					false,
					true,
					true,
					true,
					nullptr,
					true);
			InstanceData.bMoveRequested =
				MoveResult == EPathFollowingRequestResult::RequestSuccessful;
			if (MoveResult == EPathFollowingRequestResult::Failed)
			{
				UE_LOG(
					LogAIRECompanionStateTree,
					Warning,
					TEXT("Companion combat move failed. Companion=%s Target=%s ApproachLocation=%s AcceptanceRadius=%.2f"),
					*GetNameSafe(CompanionPawn),
					*GetNameSafe(TargetActor),
					*ApproachLocation.ToCompactString(),
					CombatApproachAcceptanceRadius);
				InstanceData.RetryTimeRemaining = CombatMovementRetryInterval;
			}
		}
		return EStateTreeRunStatus::Running;
	}

	if (InstanceData.bMoveRequested)
	{
		InstanceData.CompanionController->StopMovement();
		InstanceData.bMoveRequested = false;
	}

	const bool bBasicAttackActive =
		InstanceData.AbilitySystemComponent->HasMatchingGameplayTag(
			AIRECompanionGameplayTags::StateActionAttackingBasic);
	const bool bCombatSkillActive =
		InstanceData.AbilitySystemComponent->HasMatchingGameplayTag(
			AIRECompanionGameplayTags::StateActionAttackingSkill);
	const bool bSkillCancelWindowOpen =
		InstanceData.AbilitySystemComponent->HasMatchingGameplayTag(
			AIRECompanionGameplayTags::
				StateActionAttackingSkillCancelable);
	const FGameplayAbilitySpecHandle CombatSkillHandle =
		InstanceData.EquipmentComponent->FindGrantedAbilityHandle(
			AIRECompanionGameplayTags::AbilityCombatSkill);
	const bool bCanSelectCombatSkill =
		WeaponDefinition->CombatSkill.bEnabled
		&& CombatSkillHandle.IsValid();
	const auto ShouldSelectCombatSkill =
		[WeaponDefinition, bCanSelectCombatSkill]()
		{
			return bCanSelectCombatSkill
				&& FMath::FRand()
					< WeaponDefinition->CombatSkill.SelectionChance;
		};
	const auto RequestCombatSkill =
		[&InstanceData, CompanionPawn, TargetActor]()
		{
			FGameplayEventData SkillRequest;
			SkillRequest.EventTag =
				AIRECompanionGameplayTags::EventCombatSkillRequest;
			SkillRequest.Instigator = CompanionPawn;
			SkillRequest.Target = TargetActor;
			return InstanceData.AbilitySystemComponent->HandleGameplayEvent(
				AIRECompanionGameplayTags::EventCombatSkillRequest,
				&SkillRequest);
		};

	if (bCombatSkillActive)
	{
		return EStateTreeRunStatus::Running;
	}

	if (bBasicAttackActive)
	{
		if (!bSkillCancelWindowOpen
			&& InstanceData.bWasSkillCancelWindowOpen)
		{
			InstanceData.bSkillIntentBuffered = false;
			InstanceData.bSkillIntentEvaluatedForStep = false;
		}

		if (!InstanceData.bSkillIntentEvaluatedForStep)
		{
			InstanceData.bSkillIntentEvaluatedForStep = true;
			InstanceData.bSkillIntentBuffered =
				ShouldSelectCombatSkill();
		}

		if (bSkillCancelWindowOpen
			&& !InstanceData.bWasSkillCancelWindowOpen
			&& InstanceData.bSkillIntentBuffered)
		{
			const int32 ActivatedAbilityCount = RequestCombatSkill();
			UE_LOG(
				LogAIRECompanionStateTree,
				Verbose,
				TEXT("Companion buffered combat skill requested during combo. Target=%s ActivatedAbilities=%d"),
				*GetNameSafe(TargetActor),
				ActivatedAbilityCount);
			InstanceData.bSkillIntentBuffered = false;
		}

		InstanceData.bWasSkillCancelWindowOpen =
			bSkillCancelWindowOpen;
		InstanceData.bWasBasicAttackActive = true;
		return EStateTreeRunStatus::Running;
	}

	if (bAttackActive)
	{
		return EStateTreeRunStatus::Running;
	}

	if (InstanceData.bWasBasicAttackActive)
	{
		InstanceData.bWasBasicAttackActive = false;
		InstanceData.bSkillIntentEvaluatedForStep = false;
		InstanceData.bWasSkillCancelWindowOpen = false;
	}

	if (InstanceData.RetryTimeRemaining > 0.0f)
	{
		return EStateTreeRunStatus::Running;
	}

	FVector TargetDirection =
		TargetActor->GetActorLocation() - CompanionPawn->GetActorLocation();
	TargetDirection.Z = 0.0f;
	if (!TargetDirection.IsNearlyZero())
	{
		CompanionPawn->SetActorRotation(
			FRotator(
				0.0f,
				TargetDirection.Rotation().Yaw,
				0.0f));
	}

	const bool bShouldTryCombatSkill =
		InstanceData.bSkillIntentBuffered
		|| ShouldSelectCombatSkill();
	InstanceData.bSkillIntentBuffered = false;
	if (bShouldTryCombatSkill)
	{
		const int32 ActivatedAbilityCount = RequestCombatSkill();
		const bool bCombatSkillActiveAfterRequest =
			InstanceData.AbilitySystemComponent->HasMatchingGameplayTag(
				AIRECompanionGameplayTags::
					StateActionAttackingSkill);
		const bool bCombatSkillCommitted =
			ActivatedAbilityCount > 0
			&& bCombatSkillActiveAfterRequest;
		if (bCombatSkillCommitted)
		{
			UE_LOG(
				LogAIRECompanionStateTree,
				Verbose,
				TEXT("Companion combat skill requested. Target=%s ActivatedAbilities=%d RetryDelay=%.2f"),
				*GetNameSafe(TargetActor),
				ActivatedAbilityCount,
				CombatActivationRetryInterval);
			InstanceData.RetryTimeRemaining =
				CombatActivationRetryInterval;
			return EStateTreeRunStatus::Running;
		}
	}

	FGameplayEventData AttackRequest;
	AttackRequest.EventTag = AIRECompanionGameplayTags::EventAttackRequest;
	AttackRequest.Instigator = CompanionPawn;
	AttackRequest.Target = TargetActor;
	const int32 ActivatedAbilityCount =
		InstanceData.AbilitySystemComponent->HandleGameplayEvent(
		AIRECompanionGameplayTags::EventAttackRequest,
		&AttackRequest);
	if (ActivatedAbilityCount > 0)
	{
		UE_LOG(
			LogAIRECompanionStateTree,
			Verbose,
			TEXT("Companion attack requested. Target=%s ActivatedAbilities=%d RetryDelay=%.2f"),
			*GetNameSafe(TargetActor),
			ActivatedAbilityCount,
			CombatActivationRetryInterval);
	}
	else
	{
		UE_LOG(
			LogAIRECompanionStateTree,
			Log,
			TEXT("Companion attack request activated no abilities. Companion=%s Target=%s RetryDelay=%.2f"),
			*GetNameSafe(CompanionPawn),
			*GetNameSafe(TargetActor),
			CombatActivationRetryInterval);
	}
	InstanceData.RetryTimeRemaining = CombatActivationRetryInterval;
	return EStateTreeRunStatus::Running;
}

void FAIRECompanionEngageThreatTask::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	CancelOwnedRequests(InstanceData);
	UE_LOG(
		LogAIRECompanionStateTree,
		Log,
		TEXT("Companion threat engagement ended. Target=%s RunStatus=%s"),
		*GetNameSafe(InstanceData.ActiveTarget.Get()),
		*StaticEnum<EStateTreeRunStatus>()->GetNameStringByValue(
			static_cast<int64>(Transition.CurrentRunStatus)));
	InstanceData.ActiveTarget.Reset();
	InstanceData.RetryTimeRemaining = 0.0f;
	InstanceData.bSkillIntentBuffered = false;
	InstanceData.bSkillIntentEvaluatedForStep = false;
	InstanceData.bWasSkillCancelWindowOpen = false;
	InstanceData.bWasBasicAttackActive = false;
}

bool FAIRECompanionEngageThreatTask::IsTargetUsable(const AActor* TargetActor)
{
	return IsValid(TargetActor)
		&& TargetActor->GetClass()->ImplementsInterface(
			UAIREThreatTargetInterface::StaticClass())
		&& IAIREThreatTargetInterface::Execute_IsAliveThreatTarget(
			const_cast<AActor*>(TargetActor));
}

bool FAIRECompanionEngageThreatTask::IsTargetInRange(
	const APawn& CompanionPawn,
	const AActor& TargetActor,
	const float AttackRange)
{
	const float HorizontalDistance = FVector::Dist2D(
		CompanionPawn.GetActorLocation(),
		TargetActor.GetActorLocation());
	const float EffectiveDistance = FMath::Max(
		0.0f,
		HorizontalDistance
			- CompanionPawn.GetSimpleCollisionRadius()
			- TargetActor.GetSimpleCollisionRadius());
	return EffectiveDistance <= AttackRange;
}

void FAIRECompanionEngageThreatTask::CancelOwnedRequests(
	FInstanceDataType& InstanceData)
{
	if (IsValid(InstanceData.CompanionController))
	{
		InstanceData.CompanionController->StopMovement();
		InstanceData.CompanionController->ClearFocus(
			EAIFocusPriority::Gameplay);
	}
	InstanceData.bMoveRequested = false;

	if (!IsValid(InstanceData.EquipmentComponent)
		|| !IsValid(InstanceData.AbilitySystemComponent))
	{
		return;
	}

	const FGameplayAbilitySpecHandle AttackAbilityHandle =
		InstanceData.EquipmentComponent->FindGrantedAbilityHandle(
			AIRECompanionGameplayTags::AbilityCombatBasicAttack);
	if (AttackAbilityHandle.IsValid())
	{
		InstanceData.AbilitySystemComponent->CancelAbilityHandle(
			AttackAbilityHandle);
	}

	const FGameplayAbilitySpecHandle CombatSkillAbilityHandle =
		InstanceData.EquipmentComponent->FindGrantedAbilityHandle(
			AIRECompanionGameplayTags::AbilityCombatSkill);
	if (CombatSkillAbilityHandle.IsValid())
	{
		InstanceData.AbilitySystemComponent->CancelAbilityHandle(
			CombatSkillAbilityHandle);
	}

	InstanceData.bSkillIntentBuffered = false;
	InstanceData.bSkillIntentEvaluatedForStep = false;
	InstanceData.bWasSkillCancelWindowOpen = false;
	InstanceData.bWasBasicAttackActive = false;
}

FAIRECompanionEngageSupportTask::FAIRECompanionEngageSupportTask()
{
	bShouldCallTick = true;
	bShouldStateChangeOnReselect = false;
}

EStateTreeRunStatus FAIRECompanionEngageSupportTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult&) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!IsValid(InstanceData.CompanionController)
		|| !IsValid(InstanceData.SupportComponent)
		|| !IsValid(InstanceData.AbilitySystemComponent)
		|| !InstanceData.SupportComponent->IsSupportRequested()
		|| InstanceData.SupportComponent->GetSupportTarget()
			!= InstanceData.SupportTarget)
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.ActiveTarget = InstanceData.SupportTarget;
	InstanceData.RetryTimeRemaining = 0.0f;
	InstanceData.bMoveRequested = false;
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FAIRECompanionEngageSupportTask::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!IsValid(InstanceData.CompanionController)
		|| !IsValid(InstanceData.SupportComponent)
		|| !IsValid(InstanceData.AbilitySystemComponent))
	{
		CancelOwnedRequests(InstanceData);
		return EStateTreeRunStatus::Failed;
	}

	AActor* TargetActor = InstanceData.ActiveTarget.Get();
	APawn* CompanionPawn = InstanceData.CompanionController->GetPawn();
	if (!IsValid(TargetActor)
		|| !IsValid(CompanionPawn)
		|| InstanceData.SupportComponent->GetSupportTarget()
			!= TargetActor
		|| !InstanceData.SupportComponent->IsSupportRequested())
	{
		CancelOwnedRequests(InstanceData);
		return EStateTreeRunStatus::Running;
	}

	const float SupportRange =
		InstanceData.SupportComponent->GetSupportRange();
	if (!FMath::IsFinite(SupportRange) || SupportRange < 0.0f)
	{
		CancelOwnedRequests(InstanceData);
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.RetryTimeRemaining = FMath::Max(
		0.0f,
		InstanceData.RetryTimeRemaining - DeltaTime);
	if (InstanceData.bMoveRequested
		&& InstanceData.CompanionController->GetMoveStatus()
			!= EPathFollowingStatus::Moving)
	{
		InstanceData.bMoveRequested = false;
	}

	const bool bSupportAbilityActive =
		InstanceData.AbilitySystemComponent->HasMatchingGameplayTag(
			AIRECompanionGameplayTags::StateActionSupporting);
	if (!IsTargetInRange(
			*CompanionPawn,
			*TargetActor,
			SupportRange))
	{
		if (bSupportAbilityActive)
		{
			const FGameplayAbilitySpecHandle AbilityHandle =
				InstanceData.SupportComponent
					->FindSupportAbilityHandle();
			if (AbilityHandle.IsValid())
			{
				InstanceData.AbilitySystemComponent
					->CancelAbilityHandle(AbilityHandle);
			}
		}

		if (!InstanceData.bMoveRequested
			&& InstanceData.RetryTimeRemaining <= 0.0f)
		{
			const FVector ApproachLocation =
				CalculateSupportApproachLocation(
					*CompanionPawn,
					*TargetActor,
					SupportRange);
			const EPathFollowingRequestResult::Type MoveResult =
				InstanceData.CompanionController->MoveToLocation(
					ApproachLocation,
					SupportApproachAcceptanceRadius,
					false,
					true,
					true,
					true,
					nullptr,
					true);
			InstanceData.bMoveRequested =
				MoveResult
				== EPathFollowingRequestResult::RequestSuccessful;
			if (MoveResult == EPathFollowingRequestResult::Failed)
			{
				InstanceData.RetryTimeRemaining =
					SupportMovementRetryInterval;
			}
		}
		return EStateTreeRunStatus::Running;
	}

	if (InstanceData.bMoveRequested)
	{
		InstanceData.CompanionController->StopMovement();
		InstanceData.bMoveRequested = false;
	}

	InstanceData.CompanionController->SetFocus(
		TargetActor,
		EAIFocusPriority::Gameplay);
	if (bSupportAbilityActive
		|| InstanceData.RetryTimeRemaining > 0.0f)
	{
		return EStateTreeRunStatus::Running;
	}

	FGameplayEventData HealRequest;
	HealRequest.EventTag =
		AIRECompanionGameplayTags::EventSupportHealRequest;
	HealRequest.Instigator = CompanionPawn;
	HealRequest.Target = TargetActor;
	InstanceData.AbilitySystemComponent->HandleGameplayEvent(
		AIRECompanionGameplayTags::EventSupportHealRequest,
		&HealRequest);
	InstanceData.RetryTimeRemaining =
		SupportActivationRetryInterval;
	return EStateTreeRunStatus::Running;
}

void FAIRECompanionEngageSupportTask::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult&) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	CancelOwnedRequests(InstanceData);
	InstanceData.ActiveTarget.Reset();
	InstanceData.RetryTimeRemaining = 0.0f;
}

bool FAIRECompanionEngageSupportTask::IsTargetInRange(
	const APawn& CompanionPawn,
	const AActor& TargetActor,
	const float SupportRange)
{
	const float HorizontalDistance = FVector::Dist2D(
		CompanionPawn.GetActorLocation(),
		TargetActor.GetActorLocation());
	const float EffectiveDistance = FMath::Max(
		0.0f,
		HorizontalDistance
			- CompanionPawn.GetSimpleCollisionRadius()
			- TargetActor.GetSimpleCollisionRadius());
	return EffectiveDistance <= SupportRange;
}

void FAIRECompanionEngageSupportTask::CancelOwnedRequests(
	FInstanceDataType& InstanceData)
{
	if (IsValid(InstanceData.CompanionController))
	{
		InstanceData.CompanionController->StopMovement();
		InstanceData.CompanionController->ClearFocus(
			EAIFocusPriority::Gameplay);
	}
	InstanceData.bMoveRequested = false;

	if (IsValid(InstanceData.SupportComponent))
	{
		InstanceData.SupportComponent->CancelSupportRequest();
	}
}

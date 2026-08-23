#include "AIREEnemyReactionComponent.h"

#include "AbilitySystemComponent.h"
#include "AIRECombatGameplayTags.h"
#include "AIREEnemyReactionAttributeSet.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"

UAIREEnemyReactionComponent::UAIREEnemyReactionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UAIREEnemyReactionComponent::InitializeReaction(
	UAbilitySystemComponent* InAbilitySystem,
	const UAIREEnemyReactionAttributeSet* InAttributeSet)
{
	ShutdownReaction();
	if (!IsValid(InAbilitySystem)
		|| !IsValid(InAttributeSet)
		|| !InAbilitySystem->HasAttributeSetForAttribute(
			UAIREEnemyReactionAttributeSet::GetFlinchGaugeAttribute())
		|| !InAbilitySystem->HasAttributeSetForAttribute(
			UAIREEnemyReactionAttributeSet::GetStunGaugeAttribute()))
	{
		return false;
	}

	AbilitySystem = InAbilitySystem;
	AttributeSet = InAttributeSet;
	OwnerCharacter = Cast<ACharacter>(GetOwner());
	bOwnerDead = false;
	ResetGauges();
	FlinchChangedDelegateHandle = InAbilitySystem
		->GetGameplayAttributeValueChangeDelegate(
			UAIREEnemyReactionAttributeSet::GetFlinchGaugeAttribute())
		.AddUObject(this, &UAIREEnemyReactionComponent::HandleGaugeChanged);
	StunChangedDelegateHandle = InAbilitySystem
		->GetGameplayAttributeValueChangeDelegate(
			UAIREEnemyReactionAttributeSet::GetStunGaugeAttribute())
		.AddUObject(this, &UAIREEnemyReactionComponent::HandleGaugeChanged);
	return true;
}

void UAIREEnemyReactionComponent::ShutdownReaction()
{
	StopActiveReactionMontage();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EvaluationTimerHandle);
		World->GetTimerManager().ClearTimer(ReactionTimerHandle);
	}
	if (AbilitySystem.IsValid())
	{
		if (FlinchChangedDelegateHandle.IsValid())
		{
			AbilitySystem
				->GetGameplayAttributeValueChangeDelegate(
					UAIREEnemyReactionAttributeSet::GetFlinchGaugeAttribute())
				.Remove(FlinchChangedDelegateHandle);
		}
		if (StunChangedDelegateHandle.IsValid())
		{
			AbilitySystem
				->GetGameplayAttributeValueChangeDelegate(
					UAIREEnemyReactionAttributeSet::GetStunGaugeAttribute())
				.Remove(StunChangedDelegateHandle);
		}
		AbilitySystem->SetLooseGameplayTagCount(
			AIRECombatGameplayTags::StateFlinching,
			0);
		AbilitySystem->SetLooseGameplayTagCount(
			AIRECombatGameplayTags::StateStunned,
			0);
	}
	FlinchChangedDelegateHandle.Reset();
	StunChangedDelegateHandle.Reset();
	AbilitySystem.Reset();
	AttributeSet.Reset();
	OwnerCharacter.Reset();
	ReactionState = EAIREEnemyReactionState::None;
	bPendingFlinchEvaluation = false;
	OnReactionStateChanged.Clear();
	OnGroggyChanged.Clear();
}

void UAIREEnemyReactionComponent::ConfigureThresholds(
	const float InFlinchThreshold,
	const float InFlinchDuration,
	const float InStunThreshold,
	const float InStunDuration)
{
	if (FMath::IsFinite(InFlinchThreshold) && InFlinchThreshold > 0.0f)
	{
		FlinchThreshold = InFlinchThreshold;
	}
	if (FMath::IsFinite(InFlinchDuration) && InFlinchDuration > 0.0f)
	{
		FlinchDuration = InFlinchDuration;
	}
	if (FMath::IsFinite(InStunThreshold) && InStunThreshold > 0.0f)
	{
		StunThreshold = InStunThreshold;
	}
	if (FMath::IsFinite(InStunDuration) && InStunDuration > 0.0f)
	{
		StunDuration = InStunDuration;
	}
	BroadcastGroggyChanged();
}

FAIREEnemyReactionSnapshot
UAIREEnemyReactionComponent::GetReactionSnapshot() const
{
	FAIREEnemyReactionSnapshot Snapshot;
	Snapshot.State = ReactionState;
	Snapshot.FlinchThreshold = FlinchThreshold;
	Snapshot.StunThreshold = StunThreshold;
	if (AbilitySystem.IsValid())
	{
		Snapshot.FlinchGauge = AbilitySystem->GetNumericAttribute(
			UAIREEnemyReactionAttributeSet::GetFlinchGaugeAttribute());
		Snapshot.StunGauge = AbilitySystem->GetNumericAttribute(
			UAIREEnemyReactionAttributeSet::GetStunGaugeAttribute());
	}
	return Snapshot;
}

bool UAIREEnemyReactionComponent::IsAcceptingStagger() const
{
	return AbilitySystem.IsValid()
		&& !bOwnerDead
		&& ReactionState != EAIREEnemyReactionState::Stunned;
}

void UAIREEnemyReactionComponent::ResetForReturnHome()
{
	StopActiveReactionMontage();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EvaluationTimerHandle);
		World->GetTimerManager().ClearTimer(ReactionTimerHandle);
	}
	if (AbilitySystem.IsValid())
	{
		AbilitySystem->SetLooseGameplayTagCount(
			AIRECombatGameplayTags::StateFlinching,
			0);
		AbilitySystem->SetLooseGameplayTagCount(
			AIRECombatGameplayTags::StateStunned,
			0);
	}
	SetReactionState(EAIREEnemyReactionState::None);
	ResetGauges();
}

void UAIREEnemyReactionComponent::HandleOwnerDeath()
{
	bOwnerDead = true;
	bPendingFlinchEvaluation = false;
	StopActiveReactionMontage();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EvaluationTimerHandle);
		World->GetTimerManager().ClearTimer(ReactionTimerHandle);
	}
	if (AbilitySystem.IsValid())
	{
		AbilitySystem->SetLooseGameplayTagCount(
			AIRECombatGameplayTags::StateFlinching,
			0);
		AbilitySystem->SetLooseGameplayTagCount(
			AIRECombatGameplayTags::StateStunned,
			0);
	}
	SetReactionState(EAIREEnemyReactionState::None);
}

void UAIREEnemyReactionComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	ShutdownReaction();
	Super::EndPlay(EndPlayReason);
}

void UAIREEnemyReactionComponent::HandleGaugeChanged(
	const FOnAttributeChangeData& ChangeData)
{
	if (ChangeData.Attribute
		== UAIREEnemyReactionAttributeSet::GetStunGaugeAttribute())
	{
		BroadcastGroggyChanged();
	}
	if (!bResettingGauges && IsAcceptingStagger())
	{
		if (ReactionState == EAIREEnemyReactionState::Flinching)
		{
			bPendingFlinchEvaluation = true;
		}
		ScheduleReactionEvaluation();
	}
}

void UAIREEnemyReactionComponent::ScheduleReactionEvaluation()
{
	UWorld* World = GetWorld();
	if (!IsValid(World)
		|| World->GetTimerManager().IsTimerActive(EvaluationTimerHandle))
	{
		return;
	}
	EvaluationTimerHandle = World->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(
			this,
			&UAIREEnemyReactionComponent::EvaluateReactionGauges));
}

void UAIREEnemyReactionComponent::EvaluateReactionGauges()
{
	if (!IsAcceptingStagger() || !AbilitySystem.IsValid())
	{
		return;
	}

	const float StunGauge = AbilitySystem->GetNumericAttribute(
		UAIREEnemyReactionAttributeSet::GetStunGaugeAttribute());
	if (StunGauge >= StunThreshold)
	{
		StartStun();
		return;
	}

	if (ReactionState != EAIREEnemyReactionState::None)
	{
		return;
	}
	const float FlinchGauge = AbilitySystem->GetNumericAttribute(
		UAIREEnemyReactionAttributeSet::GetFlinchGaugeAttribute());
	if (FlinchGauge >= FlinchThreshold)
	{
		StartFlinch();
	}
}

void UAIREEnemyReactionComponent::StartFlinch()
{
	if (!AbilitySystem.IsValid()
		|| ReactionState != EAIREEnemyReactionState::None)
	{
		return;
	}

	const float CurrentGauge = AbilitySystem->GetNumericAttribute(
		UAIREEnemyReactionAttributeSet::GetFlinchGaugeAttribute());
	bResettingGauges = true;
	AbilitySystem->SetNumericAttributeBase(
		UAIREEnemyReactionAttributeSet::GetFlinchGaugeAttribute(),
		FMath::Max(0.0f, CurrentGauge - FlinchThreshold));
	bResettingGauges = false;
	bPendingFlinchEvaluation = false;
	AbilitySystem->SetLooseGameplayTagCount(
		AIRECombatGameplayTags::StateFlinching,
		1);
	SetReactionState(EAIREEnemyReactionState::Flinching);
	PlayReactionMontage(FlinchMontage);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ReactionTimerHandle,
			this,
			&UAIREEnemyReactionComponent::FinishReaction,
			FlinchDuration,
			false);
	}
}

void UAIREEnemyReactionComponent::StartStun()
{
	if (!AbilitySystem.IsValid()
		|| ReactionState == EAIREEnemyReactionState::Stunned)
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReactionTimerHandle);
	}
	AbilitySystem->SetLooseGameplayTagCount(
		AIRECombatGameplayTags::StateFlinching,
		0);
	bPendingFlinchEvaluation = false;
	AbilitySystem->SetLooseGameplayTagCount(
		AIRECombatGameplayTags::StateStunned,
		1);
	SetReactionState(EAIREEnemyReactionState::Stunned);
	PlayReactionMontage(StunMontage);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ReactionTimerHandle,
			this,
			&UAIREEnemyReactionComponent::FinishReaction,
			StunDuration,
			false);
	}
}

void UAIREEnemyReactionComponent::FinishReaction()
{
	if (!AbilitySystem.IsValid())
	{
		return;
	}
	const EAIREEnemyReactionState PreviousState = ReactionState;
	StopActiveReactionMontage();
	AbilitySystem->SetLooseGameplayTagCount(
		AIRECombatGameplayTags::StateFlinching,
		0);
	AbilitySystem->SetLooseGameplayTagCount(
		AIRECombatGameplayTags::StateStunned,
		0);
	SetReactionState(EAIREEnemyReactionState::None);
	if (PreviousState == EAIREEnemyReactionState::Stunned)
	{
		ResetGauges();
	}
	else if (bPendingFlinchEvaluation)
	{
		bPendingFlinchEvaluation = false;
		ScheduleReactionEvaluation();
	}
}

void UAIREEnemyReactionComponent::ResetGauges()
{
	if (!AbilitySystem.IsValid())
	{
		return;
	}
	bResettingGauges = true;
	AbilitySystem->SetNumericAttributeBase(
		UAIREEnemyReactionAttributeSet::GetFlinchGaugeAttribute(),
		0.0f);
	AbilitySystem->SetNumericAttributeBase(
		UAIREEnemyReactionAttributeSet::GetStunGaugeAttribute(),
		0.0f);
	bResettingGauges = false;
	bPendingFlinchEvaluation = false;
}

void UAIREEnemyReactionComponent::BroadcastGroggyChanged()
{
	const FAIREEnemyReactionSnapshot Snapshot = GetReactionSnapshot();
	OnGroggyChanged.Broadcast(
		Snapshot.StunGauge,
		Snapshot.StunThreshold);
}

void UAIREEnemyReactionComponent::SetReactionState(
	const EAIREEnemyReactionState NewState)
{
	if (ReactionState == NewState)
	{
		return;
	}
	const EAIREEnemyReactionState PreviousState = ReactionState;
	ReactionState = NewState;
	OnReactionStateChanged.Broadcast(PreviousState, ReactionState);
}

void UAIREEnemyReactionComponent::PlayReactionMontage(UAnimMontage* Montage)
{
	StopActiveReactionMontage();
	ACharacter* Character = OwnerCharacter.Get();
	if (IsValid(Character) && IsValid(Montage))
	{
		Character->PlayAnimMontage(Montage);
		ActiveReactionMontage = Montage;
	}
}

void UAIREEnemyReactionComponent::StopActiveReactionMontage()
{
	if (ACharacter* Character = OwnerCharacter.Get();
		IsValid(Character) && IsValid(ActiveReactionMontage))
	{
		Character->StopAnimMontage(ActiveReactionMontage);
	}
	ActiveReactionMontage = nullptr;
}

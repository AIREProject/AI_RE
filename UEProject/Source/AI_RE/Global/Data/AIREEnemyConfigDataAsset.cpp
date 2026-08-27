#include "AIREEnemyConfigDataAsset.h"

#include "Animation/AnimMontage.h"
#include "Misc/DataValidation.h"

bool UAIREEnemyConfigDataAsset::IsConfigurationValid(
	FText& OutValidationError) const
{
	OutValidationError = FText::GetEmpty();
	const bool bHasTraceStartSocket = !MeleeTrace.TraceStartSocket.IsNone();
	const bool bHasTraceEndSocket = !MeleeTrace.TraceEndSocket.IsNone();
	const bool bHasCompleteSocketPair =
		bHasTraceStartSocket == bHasTraceEndSocket;
	bool bValidAttackPatterns = true;
	TSet<FName> PatternIds;
	for (const FAIREEnemyAttackPattern& Pattern : AttackPatterns)
	{
		const bool bValidPattern = !Pattern.PatternId.IsNone()
			&& IsValid(Pattern.Montage)
			&& !PatternIds.Contains(Pattern.PatternId)
			&& FMath::IsFinite(Pattern.Weight)
			&& Pattern.Weight > 0.0f
			&& FMath::IsFinite(Pattern.MinRange)
			&& Pattern.MinRange >= 0.0f
			&& FMath::IsFinite(Pattern.MaxRange)
			&& Pattern.MaxRange >= Pattern.MinRange
			&& FMath::IsFinite(Pattern.MinHealthRatio)
			&& Pattern.MinHealthRatio >= 0.0f
			&& Pattern.MinHealthRatio <= 1.0f
			&& FMath::IsFinite(Pattern.MaxHealthRatio)
			&& Pattern.MaxHealthRatio >= Pattern.MinHealthRatio
			&& Pattern.MaxHealthRatio <= 1.0f
			&& FMath::IsFinite(Pattern.MinPlayRate)
			&& Pattern.MinPlayRate > 0.0f
			&& FMath::IsFinite(Pattern.MaxPlayRate)
			&& Pattern.MaxPlayRate >= Pattern.MinPlayRate
			&& FMath::IsFinite(Pattern.DamageScale)
			&& Pattern.DamageScale >= 0.0f
			&& FMath::IsFinite(Pattern.StaggerScale)
			&& Pattern.StaggerScale >= 0.0f
			&& (Pattern.DamageScale > 0.0f || Pattern.StaggerScale > 0.0f)
			&& FMath::IsFinite(Pattern.CooldownScale)
			&& Pattern.CooldownScale >= 0.0f
			&& FMath::IsFinite(Pattern.ReuseCooldown)
			&& Pattern.ReuseCooldown >= 0.0f
			&& FMath::IsFinite(Pattern.ForwardMoveDistance)
			&& Pattern.ForwardMoveDistance >= 0.0f
			&& FMath::IsFinite(Pattern.ForwardMoveStopDistance)
			&& Pattern.ForwardMoveStopDistance >= 0.0f
			&& Pattern.ForwardMoveStopDistance <= Pattern.MaxRange
			&& (Pattern.ForwardMoveDistance <= 0.0f
				|| !Pattern.Montage->HasRootMotion());
		if (!bValidPattern)
		{
			bValidAttackPatterns = false;
			break;
		}
		PatternIds.Add(Pattern.PatternId);
	}
	const bool bValid = FMath::IsFinite(MovementSpeed)
		&& MovementSpeed > 0.0f
		&& FMath::IsFinite(HomeLeashRadius)
		&& HomeLeashRadius >= 0.0f
		&& FMath::IsFinite(HomeWanderMinRadius)
		&& HomeWanderMinRadius >= 0.0f
		&& FMath::IsFinite(HomeWanderMaxRadius)
		&& HomeWanderMaxRadius >= HomeWanderMinRadius
		&& FMath::IsFinite(HomeWanderSpeed)
		&& HomeWanderSpeed > 0.0f
		&& FMath::IsFinite(HomeWanderWaitMin)
		&& HomeWanderWaitMin >= 0.0f
		&& FMath::IsFinite(HomeWanderWaitMax)
		&& HomeWanderWaitMax >= HomeWanderWaitMin
		&& FMath::IsFinite(HomeWanderAcceptanceRadius)
		&& HomeWanderAcceptanceRadius > 0.0f
		&& FMath::IsFinite(RetreatHealthRatio)
		&& RetreatHealthRatio >= 0.0f
		&& RetreatHealthRatio <= 1.0f
		&& FMath::IsFinite(CombatSprintSpeed)
		&& CombatSprintSpeed > 0.0f
		&& FMath::IsFinite(CombatSprintStartDistance)
		&& CombatSprintStartDistance > 0.0f
		&& FMath::IsFinite(TacticalApproachDistance)
		&& TacticalApproachDistance > 0.0f
		&& FMath::IsFinite(TacticalLateralOffset)
		&& TacticalLateralOffset > 0.0f
		&& FMath::IsFinite(TacticalMoveDuration)
		&& TacticalMoveDuration > 0.0f
		&& FMath::IsFinite(MaxHealth)
		&& MaxHealth > 0.0f
		&& FMath::IsFinite(InitialHealth)
		&& InitialHealth >= 0.0f
		&& InitialHealth <= MaxHealth
		&& FMath::IsFinite(DeathRemovalDelay)
		&& DeathRemovalDelay >= 0.0f
		&& FMath::IsFinite(FlinchThreshold)
		&& FlinchThreshold > 0.0f
		&& FMath::IsFinite(FlinchDuration)
		&& FlinchDuration > 0.0f
		&& FMath::IsFinite(StunThreshold)
		&& StunThreshold > 0.0f
		&& FMath::IsFinite(StunDuration)
		&& StunDuration > 0.0f
		&& FMath::IsFinite(AttackRange)
		&& AttackRange >= 0.0f
		&& FMath::IsFinite(AttackDamage)
		&& AttackDamage >= 0.0f
		&& FMath::IsFinite(AttackStaggerValue)
		&& AttackStaggerValue >= 0.0f
		&& (AttackDamage > 0.0f || AttackStaggerValue > 0.0f)
		&& FMath::IsFinite(AttackCooldownDuration)
		&& AttackCooldownDuration >= 0.0f
		&& FMath::IsFinite(AttackFallbackHitDelay)
		&& AttackFallbackHitDelay > 0.0f
		&& FMath::IsFinite(AttackFallbackRecoveryDuration)
		&& AttackFallbackRecoveryDuration > 0.0f
		&& bHasCompleteSocketPair
		&& FMath::IsFinite(MeleeTrace.TraceRadius)
		&& MeleeTrace.TraceRadius > 0.0f
		&& FMath::IsFinite(MeleeTrace.FallbackTraceDistance)
		&& MeleeTrace.FallbackTraceDistance >= MeleeTrace.TraceRadius
		&& MeleeTrace.TraceChannel.GetValue() >= ECC_WorldStatic
		&& MeleeTrace.TraceChannel.GetValue() < ECC_MAX
		&& bValidAttackPatterns;
	if (!bValid)
	{
		OutValidationError = NSLOCTEXT(
			"AIREEnemyConfig",
			"InvalidConfiguration",
			"Enemy combat values must be finite and satisfy their documented positive/non-negative ranges.");
	}
	return bValid;
}

#if WITH_EDITOR
EDataValidationResult UAIREEnemyConfigDataAsset::IsDataValid(
	FDataValidationContext& Context) const
{
	const EDataValidationResult SuperResult = Super::IsDataValid(Context);
	FText ValidationError;
	if (!IsConfigurationValid(ValidationError))
	{
		Context.AddError(ValidationError);
		return EDataValidationResult::Invalid;
	}
	return SuperResult == EDataValidationResult::Invalid
		? EDataValidationResult::Invalid
		: EDataValidationResult::Valid;
}
#endif

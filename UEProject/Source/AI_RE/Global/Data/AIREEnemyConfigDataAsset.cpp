#include "AIREEnemyConfigDataAsset.h"

#include "Misc/DataValidation.h"

bool UAIREEnemyConfigDataAsset::IsConfigurationValid(
	FText& OutValidationError) const
{
	OutValidationError = FText::GetEmpty();
	const bool bHasTraceStartSocket = !MeleeTrace.TraceStartSocket.IsNone();
	const bool bHasTraceEndSocket = !MeleeTrace.TraceEndSocket.IsNone();
	const bool bHasCompleteSocketPair =
		bHasTraceStartSocket == bHasTraceEndSocket;
	const bool bValid = FMath::IsFinite(MovementSpeed)
		&& MovementSpeed > 0.0f
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
		&& MeleeTrace.TraceChannel.GetValue() < ECC_MAX;
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

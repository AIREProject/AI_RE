#include "Core/AIRECompanionConfigDataAsset.h"

#include "Misc/DataValidation.h"

bool UAIRECompanionConfigDataAsset::IsConfigurationValid(FText& OutValidationError) const
{
	OutValidationError = FText::GetEmpty();

	if (!FMath::IsFinite(MovementSpeed) || MovementSpeed <= 0.0f)
	{
		OutValidationError = NSLOCTEXT("AIRECompanionConfig", "InvalidMovementSpeed", "Run Speed must be finite and greater than zero.");
		return false;
	}

	if (!FMath::IsFinite(WalkSpeed) || WalkSpeed <= 0.0f || WalkSpeed > MovementSpeed)
	{
		OutValidationError = NSLOCTEXT("AIRECompanionConfig", "InvalidWalkSpeed", "Walk Speed must be finite, greater than zero, and not exceed Run Speed.");
		return false;
	}

	if (!FMath::IsFinite(FollowStopDistance) || FollowStopDistance < 0.0f)
	{
		OutValidationError = NSLOCTEXT("AIRECompanionConfig", "InvalidFollowStopDistance", "Follow Stop Distance must be finite and non-negative.");
		return false;
	}

	if (!FMath::IsFinite(RunStartDistance) || RunStartDistance < 0.0f)
	{
		OutValidationError = NSLOCTEXT("AIRECompanionConfig", "InvalidRunStartDistance", "Run Start Distance must be finite and non-negative.");
		return false;
	}

	if (!FMath::IsFinite(WalkResumeDistance) || WalkResumeDistance < 0.0f)
	{
		OutValidationError = NSLOCTEXT("AIRECompanionConfig", "InvalidWalkResumeDistance", "Walk Resume Distance must be finite and non-negative.");
		return false;
	}

	if (!FMath::IsFinite(ReturnStartDistance) || ReturnStartDistance < 0.0f)
	{
		OutValidationError = NSLOCTEXT("AIRECompanionConfig", "InvalidReturnStartDistance", "Return Start Distance must be finite and non-negative.");
		return false;
	}

	if (!FMath::IsFinite(ThreatDetectionDistance) || ThreatDetectionDistance < 0.0f)
	{
		OutValidationError = NSLOCTEXT("AIRECompanionConfig", "InvalidThreatDetectionDistance", "Threat Detection Distance must be finite and non-negative.");
		return false;
	}

	if (!FMath::IsFinite(MaxChaseDistanceFromPlayer) || MaxChaseDistanceFromPlayer < 0.0f)
	{
		OutValidationError = NSLOCTEXT("AIRECompanionConfig", "InvalidMaxChaseDistance", "Max Chase Distance From Player must be finite and non-negative.");
		return false;
	}

	if (!FMath::IsFinite(MaxHealth) || MaxHealth <= 0.0f)
	{
		OutValidationError = NSLOCTEXT("AIRECompanionConfig", "InvalidMaxHealth", "Max Health must be finite and greater than zero.");
		return false;
	}

	if (!FMath::IsFinite(InitialHealth) || InitialHealth < 0.0f || InitialHealth > MaxHealth)
	{
		OutValidationError = NSLOCTEXT("AIRECompanionConfig", "InvalidInitialHealth", "Initial Health must be finite and between zero and Max Health.");
		return false;
	}

	if (!FMath::IsFinite(MaxStamina) || MaxStamina <= 0.0f)
	{
		OutValidationError = NSLOCTEXT("AIRECompanionConfig", "InvalidMaxStamina", "Max Stamina must be finite and greater than zero.");
		return false;
	}

	if (!FMath::IsFinite(InitialStamina) || InitialStamina < 0.0f || InitialStamina > MaxStamina)
	{
		OutValidationError = NSLOCTEXT("AIRECompanionConfig", "InvalidInitialStamina", "Initial Stamina must be finite and between zero and Max Stamina.");
		return false;
	}

	if (FollowStopDistance >= WalkResumeDistance
		|| WalkResumeDistance >= RunStartDistance)
	{
		OutValidationError = NSLOCTEXT(
			"AIRECompanionConfig",
			"InvalidWalkRunThresholds",
			"Movement distances must satisfy Follow Stop Distance < Walk Resume Distance < Run Start Distance.");
		return false;
	}

	if (FollowStopDistance >= ReturnStartDistance)
	{
		OutValidationError = NSLOCTEXT("AIRECompanionConfig", "InvalidFollowReturnThresholds", "Follow Stop Distance must be less than Return Start Distance.");
		return false;
	}

	return true;
}

#if WITH_EDITOR
EDataValidationResult UAIRECompanionConfigDataAsset::IsDataValid(FDataValidationContext& Context) const
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

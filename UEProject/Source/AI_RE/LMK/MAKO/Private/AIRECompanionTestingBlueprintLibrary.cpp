#include "AIRECompanionTestingBlueprintLibrary.h"

#include "AbilitySystemComponent.h"
#include "AIRECompanionAttributeSet.h"
#include "AIRECompanionCharacter.h"
#include "AIRECompanionDamageGameplayEffect.h"
#include "AIRECompanionGameplayTags.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogAIRECompanionTesting, Log, All);

namespace
{
	AAIRECompanionAIController* FindFirstCompanionController(const UObject* WorldContextObject)
	{
		if (!IsValid(GEngine) || !IsValid(WorldContextObject))
		{
			return nullptr;
		}

		UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
		if (!IsValid(World))
		{
			return nullptr;
		}

		for (TActorIterator<AAIRECompanionAIController> Iterator(World); Iterator; ++Iterator)
		{
			if (IsValid(*Iterator))
			{
				return *Iterator;
			}
		}

		return nullptr;
	}

	AAIRECompanionCharacter* FindFirstCompanionCharacter(const UObject* WorldContextObject)
	{
		if (!IsValid(GEngine) || !IsValid(WorldContextObject))
		{
			return nullptr;
		}

		UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
		if (!IsValid(World))
		{
			return nullptr;
		}

		for (TActorIterator<AAIRECompanionCharacter> Iterator(World); Iterator; ++Iterator)
		{
			if (IsValid(*Iterator))
			{
				return *Iterator;
			}
		}

		return nullptr;
	}
}

bool UAIRECompanionTestingBlueprintLibrary::SetFirstCompanionTestBehaviorRequest(
	const UObject* WorldContextObject,
	const EAIRECompanionTestBehaviorRequest Request,
	const bool bIsRequested)
{
	AAIRECompanionAIController* CompanionController = FindFirstCompanionController(WorldContextObject);
	if (!IsValid(CompanionController))
	{
		return false;
	}

	CompanionController->SetTestBehaviorRequest(Request, bIsRequested);
	return true;
}

bool UAIRECompanionTestingBlueprintLibrary::ClearFirstCompanionTestBehaviorRequests(
	const UObject* WorldContextObject)
{
	AAIRECompanionAIController* CompanionController = FindFirstCompanionController(WorldContextObject);
	if (!IsValid(CompanionController))
	{
		return false;
	}

	CompanionController->ClearTestBehaviorRequests();
	return true;
}

bool UAIRECompanionTestingBlueprintLibrary::ApplyDamageToFirstCompanion(
	const UObject* WorldContextObject,
	const float DamageAmount)
{
	if (!FMath::IsFinite(DamageAmount) || DamageAmount <= 0.0f)
	{
		return false;
	}

	AAIRECompanionCharacter* CompanionCharacter = FindFirstCompanionCharacter(WorldContextObject);
	UAbilitySystemComponent* AbilitySystem = IsValid(CompanionCharacter)
		? CompanionCharacter->GetAbilitySystemComponent()
		: nullptr;
	if (!IsValid(AbilitySystem))
	{
		return false;
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystem->MakeEffectContext();
	EffectContext.AddSourceObject(CompanionCharacter);
	FGameplayEffectSpecHandle EffectSpec = AbilitySystem->MakeOutgoingSpec(
		UAIRECompanionDamageGameplayEffect::StaticClass(),
		1.0f,
		EffectContext);
	if (!EffectSpec.IsValid())
	{
		return false;
	}

	EffectSpec.Data->SetSetByCallerMagnitude(AIRECompanionGameplayTags::DataDamage, -DamageAmount);
	const FActiveGameplayEffectHandle AppliedEffect = AbilitySystem->ApplyGameplayEffectSpecToSelf(*EffectSpec.Data.Get());
	return AppliedEffect.WasSuccessfullyApplied();
}

bool UAIRECompanionTestingBlueprintLibrary::ResetFirstCompanionAttributes(const UObject* WorldContextObject)
{
	AAIRECompanionCharacter* CompanionCharacter = FindFirstCompanionCharacter(WorldContextObject);
	return IsValid(CompanionCharacter) && CompanionCharacter->ResetAttributesToConfiguredDefaults();
}

bool UAIRECompanionTestingBlueprintLibrary::LogFirstCompanionAbilityState(const UObject* WorldContextObject)
{
	const AAIRECompanionCharacter* CompanionCharacter = FindFirstCompanionCharacter(WorldContextObject);
	if (!IsValid(CompanionCharacter))
	{
		return false;
	}

	const UAIRECompanionAttributeSet* Attributes = CompanionCharacter->GetCompanionAttributeSet();
	const UAbilitySystemComponent* AbilitySystem = CompanionCharacter->GetAbilitySystemComponent();
	if (!IsValid(Attributes) || !IsValid(AbilitySystem))
	{
		return false;
	}

	UE_LOG(
		LogAIRECompanionTesting,
		Log,
		TEXT("Companion GAS state. Companion=%s Health=%.2f/%.2f Stamina=%.2f/%.2f Dead=%s Disabled=%s"),
		*GetNameSafe(CompanionCharacter),
		Attributes->GetHealth(),
		Attributes->GetMaxHealth(),
		Attributes->GetStamina(),
		Attributes->GetMaxStamina(),
		AbilitySystem->HasMatchingGameplayTag(AIRECompanionGameplayTags::StateDisabledDead) ? TEXT("true") : TEXT("false"),
		AbilitySystem->HasMatchingGameplayTag(AIRECompanionGameplayTags::StateDisabled) ? TEXT("true") : TEXT("false"));
	return true;
}

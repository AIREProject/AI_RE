#include "AIRECompanionCharacter.h"

#include "AbilitySystemComponent.h"
#include "AIRECompanionAIController.h"
#include "AIRECompanionAttributeSet.h"
#include "AIRECompanionConfigDataAsset.h"
#include "AIRECompanionChatComponent.h"
#include "AIRECompanionEquipmentComponent.h"
#include "AIRECompanionGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogAIRECompanionCharacter, Log, All);

namespace
{
	constexpr TCHAR CompanionId[] = TEXT("MAKO");
}

AAIRECompanionCharacter::AAIRECompanionCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	AIControllerClass = AAIRECompanionAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	bUseControllerRotationYaw = false;

	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	check(MovementComponent);
	MovementComponent->bOrientRotationToMovement = true;
	MovementComponent->bUseControllerDesiredRotation = false;
	MovementComponent->RotationRate = FRotator(0.0f, 540.0f, 0.0f);

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
	check(AbilitySystemComponent);
	AbilitySystemComponent->SetIsReplicated(false);

	CompanionAttributeSet = CreateDefaultSubobject<UAIRECompanionAttributeSet>(TEXT("CompanionAttributes"));
	check(CompanionAttributeSet);

	EquipmentComponent = CreateDefaultSubobject<UAIRECompanionEquipmentComponent>(TEXT("Equipment"));
	check(EquipmentComponent);

	ChatComponent = CreateDefaultSubobject<UAIRECompanionChatComponent>(TEXT("Chat"));
	check(ChatComponent);
}

UAbilitySystemComponent* AAIRECompanionCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

const UAIRECompanionAttributeSet* AAIRECompanionCharacter::GetCompanionAttributeSet() const
{
	return CompanionAttributeSet;
}

bool AAIRECompanionCharacter::IsAbilitySystemDisabled() const
{
	return IsValid(AbilitySystemComponent)
		&& AbilitySystemComponent->HasMatchingGameplayTag(AIRECompanionGameplayTags::StateDisabled);
}

UAIRECompanionEquipmentComponent* AAIRECompanionCharacter::GetEquipmentComponent() const
{
	return EquipmentComponent;
}

UAIRECompanionChatComponent* AAIRECompanionCharacter::GetChatComponent() const
{
	return ChatComponent;
}

FString AAIRECompanionCharacter::GetCompanionId() const
{
	return CompanionId;
}

const UAIRECompanionConfigDataAsset* AAIRECompanionCharacter::GetCompanionConfig() const
{
	if (IsValid(CompanionConfig))
	{
		FText ValidationError;
		if (CompanionConfig->IsConfigurationValid(ValidationError))
		{
			return CompanionConfig;
		}
	}

	return GetDefault<UAIRECompanionConfigDataAsset>();
}

void AAIRECompanionCharacter::BeginPlay()
{
	Super::BeginPlay();

	check(AbilitySystemComponent);
	check(CompanionAttributeSet);
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	ResetAttributesToConfiguredDefaults();
	const UAIRECompanionConfigDataAsset* CompanionConfigData = GetCompanionConfig();
	if (IsValid(EquipmentComponent) && IsValid(CompanionConfigData))
	{
		EquipmentComponent->InitializeEquipment(
			AbilitySystemComponent,
			CompanionConfigData->CombatCooldown);
	}
	HealthChangedDelegateHandle = AbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UAIRECompanionAttributeSet::GetHealthAttribute())
		.AddUObject(this, &AAIRECompanionCharacter::HandleHealthChanged);

	if (!IsValid(CompanionConfig))
	{
		UE_LOG(LogAIRECompanionCharacter, Warning, TEXT("Companion %s has no configuration assigned. Using C++ defaults."), *GetNameSafe(this));
		return;
	}

	FText ValidationError;
	if (!CompanionConfig->IsConfigurationValid(ValidationError))
	{
		UE_LOG(
			LogAIRECompanionCharacter,
			Warning,
			TEXT("Companion %s has invalid configuration %s: %s Using C++ defaults."),
			*GetNameSafe(this),
			*GetNameSafe(CompanionConfig),
			*ValidationError.ToString());
	}
}

void AAIRECompanionCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(EquipmentComponent))
	{
		EquipmentComponent->ShutdownEquipment();
	}

	if (IsValid(AbilitySystemComponent))
	{
		if (HealthChangedDelegateHandle.IsValid())
		{
			AbilitySystemComponent
				->GetGameplayAttributeValueChangeDelegate(UAIRECompanionAttributeSet::GetHealthAttribute())
				.Remove(HealthChangedDelegateHandle);
			HealthChangedDelegateHandle.Reset();
		}

		AbilitySystemComponent->ClearActorInfo();
	}

	Super::EndPlay(EndPlayReason);
}

bool AAIRECompanionCharacter::ResetAttributesToConfiguredDefaults()
{
	if (!IsValid(AbilitySystemComponent) || !IsValid(CompanionAttributeSet))
	{
		return false;
	}

	const UAIRECompanionConfigDataAsset* CompanionConfigData = GetCompanionConfig();
	if (!IsValid(CompanionConfigData))
	{
		return false;
	}

	AbilitySystemComponent->SetNumericAttributeBase(
		UAIRECompanionAttributeSet::GetMaxHealthAttribute(),
		CompanionConfigData->MaxHealth);
	AbilitySystemComponent->SetNumericAttributeBase(
		UAIRECompanionAttributeSet::GetMaxStaminaAttribute(),
		CompanionConfigData->MaxStamina);
	AbilitySystemComponent->SetNumericAttributeBase(
		UAIRECompanionAttributeSet::GetHealthAttribute(),
		CompanionConfigData->InitialHealth);
	AbilitySystemComponent->SetNumericAttributeBase(
		UAIRECompanionAttributeSet::GetStaminaAttribute(),
		CompanionConfigData->InitialStamina);

	SynchronizeDeadState(CompanionAttributeSet->GetHealth());
	UE_LOG(
		LogAIRECompanionCharacter,
		Log,
		TEXT("Companion GAS attributes initialized. Companion=%s Health=%.2f/%.2f Stamina=%.2f/%.2f Disabled=%s"),
		*GetNameSafe(this),
		CompanionAttributeSet->GetHealth(),
		CompanionAttributeSet->GetMaxHealth(),
		CompanionAttributeSet->GetStamina(),
		CompanionAttributeSet->GetMaxStamina(),
		IsAbilitySystemDisabled() ? TEXT("true") : TEXT("false"));
	return true;
}

void AAIRECompanionCharacter::HandleHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	SynchronizeDeadState(ChangeData.NewValue);
	UE_LOG(
		LogAIRECompanionCharacter,
		Log,
		TEXT("Companion health changed. Companion=%s OldHealth=%.2f NewHealth=%.2f Dead=%s"),
		*GetNameSafe(this),
		ChangeData.OldValue,
		ChangeData.NewValue,
		ChangeData.NewValue <= 0.0f ? TEXT("true") : TEXT("false"));
}

void AAIRECompanionCharacter::SynchronizeDeadState(const float CurrentHealth)
{
	if (!IsValid(AbilitySystemComponent))
	{
		return;
	}

	AbilitySystemComponent->SetLooseGameplayTagCount(
		AIRECompanionGameplayTags::StateDisabledDead,
		CurrentHealth <= 0.0f ? 1 : 0);
}

#include "Core/AIRECompanionCharacter.h"

#include "AbilitySystemComponent.h"
#include "Core/AIRECompanionAIController.h"
#include "AbilitySystem/Core/Attributes/AIRECompanionAttributeSet.h"
#include "Core/AIRECompanionConfigDataAsset.h"
#include "Chat/AIRECompanionChatComponent.h"
#include "Equipment/AIRECompanionEquipmentComponent.h"
#include "Inventory/AIRECompanionInventoryComponent.h"
#include "Interaction/AIRECompanionInventoryInteractionComponent.h"
#include "AIREGameplayInventorySubsystem.h"
#include "Policy/AIRECompanionLocalBehaviorPolicyComponent.h"
#include "Support/AIRECompanionSupportComponent.h"
#include "Work/AIRECompanionStorageAutomationComponent.h"
#include "Work/AIRECompanionWorkOrderComponent.h"
#include "AbilitySystem/Core/AIRECompanionGameplayTags.h"
#include "Components/CapsuleComponent.h"
#include "Engine/GameInstance.h"
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

	LocalBehaviorPolicyComponent =
		CreateDefaultSubobject<UAIRECompanionLocalBehaviorPolicyComponent>(
			TEXT("LocalBehaviorPolicy"));
	check(LocalBehaviorPolicyComponent);

	EquipmentComponent = CreateDefaultSubobject<UAIRECompanionEquipmentComponent>(TEXT("Equipment"));
	check(EquipmentComponent);

	InventoryComponent = CreateDefaultSubobject<UAIRECompanionInventoryComponent>(TEXT("Inventory"));
	check(InventoryComponent);

	InventoryInteractionComponent =
		CreateDefaultSubobject<UAIRECompanionInventoryInteractionComponent>(
			TEXT("InventoryInteraction"));
	check(InventoryInteractionComponent);
	InventoryInteractionComponent->SetupAttachment(GetCapsuleComponent());

	SupportComponent = CreateDefaultSubobject<UAIRECompanionSupportComponent>(TEXT("Support"));
	check(SupportComponent);

	ChatComponent = CreateDefaultSubobject<UAIRECompanionChatComponent>(TEXT("Chat"));
	check(ChatComponent);

	WorkOrderComponent = CreateDefaultSubobject<UAIRECompanionWorkOrderComponent>(TEXT("WorkOrder"));
	check(WorkOrderComponent);

	StorageAutomationComponent =
		CreateDefaultSubobject<UAIRECompanionStorageAutomationComponent>(
			TEXT("StorageAutomation"));
	check(StorageAutomationComponent);
}

UAbilitySystemComponent* AAIRECompanionCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

bool AAIRECompanionCharacter::TryReceiveHarvestReward_Implementation(
	const FGuid DeliveryId,
	const FName ItemId,
	const int32 Count)
{
	FAIREInventoryContainerSnapshot MakoSnapshot;
	UGameInstance* GameInstance = GetGameInstance();
	UAIREGameplayInventorySubsystem* GameplayInventory =
		IsValid(GameInstance)
			? GameInstance->GetSubsystem<UAIREGameplayInventorySubsystem>()
			: nullptr;
	FAIREInventoryContainerSnapshot StorageSnapshot;
	if (!IsValid(InventoryComponent)
		|| !DeliveryId.IsValid()
		|| ItemId.IsNone()
		|| Count <= 0
		|| !InventoryComponent->GetInventorySnapshot(MakoSnapshot)
		|| !IsValid(GameplayInventory)
		|| !GameplayInventory->GetContainerSnapshot(
			UAIREGameplayInventorySubsystem::GetSharedStorageContainerId(),
			StorageSnapshot)
		|| MakoSnapshot.SessionId != StorageSnapshot.SessionId)
	{
		return false;
	}

	FAIREMakoWorkRewardRequest Request;
	Request.SessionId = MakoSnapshot.SessionId;
	Request.DeliveryId = DeliveryId;
	Request.ExpectedMakoRevision = MakoSnapshot.Revision;
	Request.ExpectedStorageRevision = StorageSnapshot.Revision;
	Request.Reward.ItemId = ItemId;
	Request.Reward.Count = Count;
	const FAIREInventoryWorkResult Result =
		InventoryComponent->TryStoreMakoWorkReward(Request);
	if (Result.Code == EAIREInventoryMutationCode::AlreadyApplied)
	{
		return true;
	}
	return Result.Code == EAIREInventoryMutationCode::Succeeded
		&& Result.Destination
			!= EAIREInventoryWorkResultDestination::WorldDrop;
}

void AAIRECompanionCharacter::Interact_Implementation(AActor* Interactor)
{
	if (IsValid(InventoryInteractionComponent))
	{
		IAI_REInteractableInterface::Execute_Interact(
			InventoryInteractionComponent,
			Interactor);
	}
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

UAIRECompanionInventoryComponent*
AAIRECompanionCharacter::GetInventoryComponent() const
{
	return InventoryComponent;
}

UAIRECompanionInventoryInteractionComponent*
AAIRECompanionCharacter::GetInventoryInteractionComponent() const
{
	return InventoryInteractionComponent;
}

UAIRECompanionSupportComponent*
AAIRECompanionCharacter::GetSupportComponent() const
{
	return SupportComponent;
}

UAIRECompanionLocalBehaviorPolicyComponent*
AAIRECompanionCharacter::GetLocalBehaviorPolicyComponent() const
{
	return LocalBehaviorPolicyComponent;
}

UAIRECompanionChatComponent* AAIRECompanionCharacter::GetChatComponent() const
{
	return ChatComponent;
}

UAIRECompanionWorkOrderComponent*
AAIRECompanionCharacter::GetWorkOrderComponent() const
{
	return WorkOrderComponent;
}

UAIRECompanionStorageAutomationComponent*
AAIRECompanionCharacter::GetStorageAutomationComponent() const
{
	return StorageAutomationComponent;
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
	const UAIRECompanionConfigDataAsset* CompanionConfigData =
		GetCompanionConfig();
	const bool bUseInventoryLoadout =
		IsValid(CompanionConfigData)
		&& !CompanionConfigData->InitialInventory.IsEmpty();
	if (IsValid(EquipmentComponent))
	{
		EquipmentComponent->InitializeEquipment(
			AbilitySystemComponent,
			!bUseInventoryLoadout);
	}
	const bool bInventoryInitialized =
		IsValid(InventoryComponent)
		&& InventoryComponent->InitializeInventory(
			CompanionConfigData,
			EquipmentComponent,
			AbilitySystemComponent);
	if (bUseInventoryLoadout && !bInventoryInitialized)
	{
		UE_LOG(
			LogAIRECompanionCharacter,
			Error,
			TEXT("Companion inventory loadout initialization failed. Companion=%s"),
			*GetNameSafe(this));
	}
	if (bInventoryInitialized
		&& IsValid(SupportComponent)
		&& IsValid(CompanionConfigData)
		&& !CompanionConfigData->InitialInventory.IsEmpty()
		&& !SupportComponent->InitializeSupport(
			CompanionConfigData,
			InventoryComponent,
			AbilitySystemComponent))
	{
		UE_LOG(
			LogAIRECompanionCharacter,
			Error,
			TEXT("Companion support initialization failed. Companion=%s"),
			*GetNameSafe(this));
	}
	if (bInventoryInitialized && IsValid(StorageAutomationComponent))
	{
		StorageAutomationComponent->InitializeAutomation(
			InventoryComponent,
			WorkOrderComponent,
			CompanionConfigData);
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
	if (IsValid(StorageAutomationComponent))
	{
		StorageAutomationComponent->ShutdownAutomation();
	}

	if (IsValid(WorkOrderComponent))
	{
		WorkOrderComponent->ShutdownWorkOrder();
	}

	if (IsValid(SupportComponent))
	{
		SupportComponent->ShutdownSupport();
	}

	if (IsValid(InventoryComponent))
	{
		InventoryComponent->ShutdownInventory();
	}

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

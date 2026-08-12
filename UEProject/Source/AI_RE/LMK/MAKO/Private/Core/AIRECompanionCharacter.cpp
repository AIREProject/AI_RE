#include "Core/AIRECompanionCharacter.h"

#include "AbilitySystemComponent.h"
#include "Core/AIRECompanionAIController.h"
#include "AbilitySystem/Combat/Abilities/AIRECompanionAutonomousEvadeAbility.h"
#include "AbilitySystem/Combat/Effects/AIRECompanionAutonomousEvadeGameplayEffects.h"
#include "AbilitySystem/Core/Attributes/AIRECompanionAttributeSet.h"
#include "Core/AIRECompanionConfigDataAsset.h"
#include "Chat/AIRECompanionChatComponent.h"
#include "Command/AIRECompanionCommandGatewayComponent.h"
#include "Equipment/AIRECompanionEquipmentComponent.h"
#include "Inventory/AIRECompanionInventoryComponent.h"
#include "Interaction/AIRECompanionInventoryInteractionComponent.h"
#include "AIREGameplayInventorySubsystem.h"
#include "AIRECombatGameplayTags.h"
#include "Policy/AIRECompanionLocalBehaviorPolicyComponent.h"
#include "Support/AIRECompanionSupportComponent.h"
#include "Work/AIRECompanionStorageAutomationComponent.h"
#include "Work/AIRECompanionWorkOrderComponent.h"
#include "AbilitySystem/Core/AIRECompanionGameplayTags.h"
#include "AIRECombatEvadeComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogAIRECompanionCharacter, Log, All);

namespace
{
	constexpr TCHAR CompanionId[] = TEXT("MAKO");
	const FName SoxMaterialSlotName(TEXT("UE_MIMI_BootCuff"));
	const FName ShoesMaterialSlotName(TEXT("UE_MIMI_Shoes"));
	const FName HairMaterialSlotName(TEXT("MIMI_Hair_Pink_v002"));
	const FName BackWeaponComponentNames[] =
	{
		FName(TEXT("holster_l")),
		FName(TEXT("holster_r"))
	};
	const FName HandWeaponComponentNames[] =
	{
		FName(TEXT("weapon_l")),
		FName(TEXT("weapon_r"))
	};
	const FName VisorComponentNames[] =
	{
		FName(TEXT("Visor"))
	};
	const TSoftObjectPtr<USkeletalMesh> DefaultUnhoodedHairMesh(
		FSoftObjectPath(TEXT("/Game/Work/LMK/Model/Hair/MAKO_HAIR.MAKO_HAIR")));
	const FName HoodMaterialSlotNames[] =
	{
		FName(TEXT("MIMI_Hood_CoolSlate_v003")),
		FName(TEXT("MIMI_HoodRim_AntiqueGold_v003")),
		FName(TEXT("MIMI_CrestGlowTeal_v003")),
		FName(TEXT("MIMI_CrestGold_v003")),
		FName(TEXT("MIMI_CrestSlate_v003"))
	};
	constexpr float StaminaRegenPeriod = 0.1f;
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

	UnhoodedHairSkeletalMeshComponent =
		CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("UnhoodedHair"));
	check(UnhoodedHairSkeletalMeshComponent);
	UnhoodedHairSkeletalMeshComponent->SetupAttachment(GetMesh());
	UnhoodedHairSkeletalMeshComponent->SetLeaderPoseComponent(GetMesh());
	UnhoodedHairSkeletalMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	UnhoodedHairSkeletalMeshComponent->SetGenerateOverlapEvents(false);
	UnhoodedHairSkeletalMeshComponent->SetCanEverAffectNavigation(false);
	UnhoodedHairSkeletalMeshComponent->SetVisibility(false, true);

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

	CommandGatewayComponent =
		CreateDefaultSubobject<UAIRECompanionCommandGatewayComponent>(
			TEXT("CommandGateway"));
	check(CommandGatewayComponent);

	WorkOrderComponent = CreateDefaultSubobject<UAIRECompanionWorkOrderComponent>(TEXT("WorkOrder"));
	check(WorkOrderComponent);

	StorageAutomationComponent =
		CreateDefaultSubobject<UAIRECompanionStorageAutomationComponent>(
			TEXT("StorageAutomation"));
	check(StorageAutomationComponent);

	CombatEvadeComponent =
		CreateDefaultSubobject<UAIRECombatEvadeComponent>(TEXT("CombatEvade"));
	check(CombatEvadeComponent);
}

UAbilitySystemComponent* AAIRECompanionCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

FGameplayAttribute AAIRECompanionCharacter::GetCombatHealthAttribute() const
{
	return UAIRECompanionAttributeSet::GetHealthAttribute();
}

EAIRECombatAffiliation AAIRECompanionCharacter::GetCombatAffiliation() const
{
	return EAIRECombatAffiliation::PlayerParty;
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

UAIRECompanionCommandGatewayComponent*
AAIRECompanionCharacter::GetCommandGatewayComponent() const
{
	return CommandGatewayComponent;
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

UAIRECombatEvadeComponent*
AAIRECompanionCharacter::GetCombatEvadeComponent() const
{
	return CombatEvadeComponent;
}

void AAIRECompanionCharacter::SetSoxAndShoesVisible(const bool bVisible)
{
	bSoxAndShoesVisible = bVisible;
	ApplySoxAndShoesVisibility();
}

bool AAIRECompanionCharacter::AreSoxAndShoesVisible() const
{
	return bSoxAndShoesVisible;
}

void AAIRECompanionCharacter::SetHoodVisible(const bool bVisible)
{
	bHoodVisible = bVisible;
	ApplyHoodVisibility();
}

bool AAIRECompanionCharacter::IsHoodVisible() const
{
	return bHoodVisible;
}

void AAIRECompanionCharacter::SetBackWeaponsVisible(const bool bVisible)
{
	bBackWeaponsVisible = bVisible;
	SetNamedStaticMeshComponentsVisible(BackWeaponComponentNames, bVisible);
}

bool AAIRECompanionCharacter::AreBackWeaponsVisible() const
{
	return bBackWeaponsVisible;
}

void AAIRECompanionCharacter::SetHandWeaponsVisible(const bool bVisible)
{
	bHandWeaponsVisible = bVisible;
	SetNamedStaticMeshComponentsVisible(HandWeaponComponentNames, bVisible);
}

bool AAIRECompanionCharacter::AreHandWeaponsVisible() const
{
	return bHandWeaponsVisible;
}

void AAIRECompanionCharacter::SetVisorVisible(const bool bVisible)
{
	bVisorVisible = bVisible;
	SetNamedStaticMeshComponentsVisible(VisorComponentNames, bVisible);
}

bool AAIRECompanionCharacter::IsVisorVisible() const
{
	return bVisorVisible;
}

void AAIRECompanionCharacter::SetCombatEquipmentActive(const bool bIsInCombat)
{
	SetBackWeaponsVisible(!bIsInCombat);
	SetHandWeaponsVisible(bIsInCombat);
	SetVisorVisible(bIsInCombat);
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

void AAIRECompanionCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplySoxAndShoesVisibility();
	ApplyHoodVisibility();
	ApplyCombatEquipmentVisibility();
}

void AAIRECompanionCharacter::BeginPlay()
{
	Super::BeginPlay();
	ApplySoxAndShoesVisibility();
	ApplyHoodVisibility();
	ApplyCombatEquipmentVisibility();

	check(AbilitySystemComponent);
	check(CompanionAttributeSet);
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	ResetAttributesToConfiguredDefaults();
	if (!InitializeAutonomousEvadeRuntime())
	{
		UE_LOG(
			LogAIRECompanionCharacter,
			Error,
			TEXT("Companion autonomous evade runtime initialization failed. Companion=%s"),
			*GetNameSafe(this));
	}
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
	InvulnerableStateChangedDelegateHandle = AbilitySystemComponent
		->RegisterGameplayTagEvent(
			AIRECombatGameplayTags::StateInvulnerable,
			EGameplayTagEventType::NewOrRemoved)
		.AddUObject(
			this,
			&AAIRECompanionCharacter::HandleInvulnerableStateChanged);
	UE_LOG(
		LogAIRECompanionCharacter,
		Log,
		TEXT("[MAKO HEALTH] Initialized Companion=%s Health=%.2f/%.2f"),
		*GetNameSafe(this),
		CompanionAttributeSet->GetHealth(),
		CompanionAttributeSet->GetMaxHealth());

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

void AAIRECompanionCharacter::ApplySoxAndShoesVisibility()
{
	USkeletalMeshComponent* MeshComponent = GetMesh();
	if (!IsValid(MeshComponent))
	{
		return;
	}

	const int32 SoxMaterialIndex =
		MeshComponent->GetMaterialIndex(SoxMaterialSlotName);
	const int32 ShoesMaterialIndex =
		MeshComponent->GetMaterialIndex(ShoesMaterialSlotName);
	if (SoxMaterialIndex == INDEX_NONE
		|| ShoesMaterialIndex == INDEX_NONE
		|| SoxMaterialIndex == ShoesMaterialIndex)
	{
		UE_LOG(
			LogAIRECompanionCharacter,
			Warning,
			TEXT("Companion mesh %s must provide distinct %s and %s footwear slots."),
			*GetNameSafe(MeshComponent->GetSkeletalMeshAsset()),
			*SoxMaterialSlotName.ToString(),
			*ShoesMaterialSlotName.ToString());
		return;
	}

	const int32 FootwearMaterialIndices[] =
	{
		SoxMaterialIndex,
		ShoesMaterialIndex
	};
	const int32 LODCount = MeshComponent->GetNumLODs();
	for (const int32 MaterialIndex : FootwearMaterialIndices)
	{
		for (int32 LODIndex = 0; LODIndex < LODCount; ++LODIndex)
		{
			MeshComponent->ShowMaterialSection(
				MaterialIndex,
				INDEX_NONE,
				bSoxAndShoesVisible,
				LODIndex);
		}
	}
}

void AAIRECompanionCharacter::ApplyHoodVisibility()
{
	USkeletalMeshComponent* MeshComponent = GetMesh();
	if (!IsValid(MeshComponent))
	{
		return;
	}

	if (IsValid(UnhoodedHairSkeletalMeshComponent)
		&& !IsValid(UnhoodedHairSkeletalMeshComponent->GetSkeletalMeshAsset()))
	{
		UnhoodedHairSkeletalMeshComponent->SetSkeletalMeshAsset(
			DefaultUnhoodedHairMesh.LoadSynchronous());
	}

	const bool bHasUnhoodedHair =
		IsValid(UnhoodedHairSkeletalMeshComponent)
		&& IsValid(UnhoodedHairSkeletalMeshComponent->GetSkeletalMeshAsset());
	if (IsValid(UnhoodedHairSkeletalMeshComponent))
	{
		UnhoodedHairSkeletalMeshComponent->SetLeaderPoseComponent(MeshComponent);
		UnhoodedHairSkeletalMeshComponent->SetVisibility(
			bHasUnhoodedHair && !bHoodVisible,
			true);
	}

	const int32 HairMaterialIndex =
		MeshComponent->GetMaterialIndex(HairMaterialSlotName);
	if (HairMaterialIndex != INDEX_NONE)
	{
		const bool bShowIntegratedHair = bHoodVisible || !bHasUnhoodedHair;
		const int32 LODCount = MeshComponent->GetNumLODs();
		for (int32 LODIndex = 0; LODIndex < LODCount; ++LODIndex)
		{
			MeshComponent->ShowMaterialSection(
				HairMaterialIndex,
				INDEX_NONE,
				bShowIntegratedHair,
				LODIndex);
		}
	}
	else if (bHasUnhoodedHair)
	{
		UE_LOG(
			LogAIRECompanionCharacter,
			Warning,
			TEXT("Companion mesh %s does not provide hair material slot %s."),
			*GetNameSafe(MeshComponent->GetSkeletalMeshAsset()),
			*HairMaterialSlotName.ToString());
	}

	bool bFoundHoodSlot = false;
	const int32 LODCount = MeshComponent->GetNumLODs();
	for (const FName& HoodMaterialSlotName : HoodMaterialSlotNames)
	{
		const int32 HoodMaterialIndex = MeshComponent->GetMaterialIndex(HoodMaterialSlotName);
		if (HoodMaterialIndex == INDEX_NONE)
		{
			continue;
		}

		bFoundHoodSlot = true;
		for (int32 LODIndex = 0; LODIndex < LODCount; ++LODIndex)
		{
			MeshComponent->ShowMaterialSection(
				HoodMaterialIndex,
				INDEX_NONE,
				bHoodVisible,
				LODIndex);
		}
	}

	if (!bFoundHoodSlot)
	{
		UE_LOG(
			LogAIRECompanionCharacter,
			Warning,
			TEXT("Companion mesh %s does not provide a hood material slot."),
			*GetNameSafe(MeshComponent->GetSkeletalMeshAsset()));
	}
}

void AAIRECompanionCharacter::ApplyCombatEquipmentVisibility()
{
	SetNamedStaticMeshComponentsVisible(
		BackWeaponComponentNames,
		bBackWeaponsVisible);
	SetNamedStaticMeshComponentsVisible(
		HandWeaponComponentNames,
		bHandWeaponsVisible);
	SetNamedStaticMeshComponentsVisible(
		VisorComponentNames,
		bVisorVisible);
}

void AAIRECompanionCharacter::SetNamedStaticMeshComponentsVisible(
	const TConstArrayView<FName> ComponentNames,
	const bool bVisible) const
{
	TInlineComponentArray<UStaticMeshComponent*> StaticMeshComponents(this);
	for (const FName ComponentName : ComponentNames)
	{
		UStaticMeshComponent* MatchingComponent = nullptr;
		const FString GeneratedComponentPrefix =
			ComponentName.ToString() + TEXT("_GEN_VARIABLE");
		for (UStaticMeshComponent* Component : StaticMeshComponents)
		{
			if (IsValid(Component)
				&& (Component->GetFName() == ComponentName
					|| Component->GetName().StartsWith(
						GeneratedComponentPrefix,
						ESearchCase::IgnoreCase)))
			{
				MatchingComponent = Component;
				break;
			}
		}

		if (!IsValid(MatchingComponent))
		{
			UE_LOG(
				LogAIRECompanionCharacter,
				Warning,
				TEXT("Companion %s does not provide static mesh component %s."),
				*GetNameSafe(this),
				*ComponentName.ToString());
			continue;
		}

		MatchingComponent->SetVisibility(bVisible, true);
		MatchingComponent->SetHiddenInGame(!bVisible, true);
	}
}

void AAIRECompanionCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ShutdownAutonomousEvadeRuntime();

	if (IsValid(CommandGatewayComponent))
	{
		CommandGatewayComponent->ShutdownGateway();
	}

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
		if (InvulnerableStateChangedDelegateHandle.IsValid())
		{
			AbilitySystemComponent->UnregisterGameplayTagEvent(
				InvulnerableStateChangedDelegateHandle,
				AIRECombatGameplayTags::StateInvulnerable,
				EGameplayTagEventType::NewOrRemoved);
			InvulnerableStateChangedDelegateHandle.Reset();
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

bool AAIRECompanionCharacter::InitializeAutonomousEvadeRuntime()
{
	ShutdownAutonomousEvadeRuntime();
	if (!IsValid(AbilitySystemComponent))
	{
		return false;
	}

	const UAIRECompanionConfigDataAsset* CompanionConfigData =
		GetCompanionConfig();
	if (!IsValid(CompanionConfigData)
		|| !CompanionConfigData->AutonomousEvade.IsValid())
	{
		return false;
	}

	FGameplayAbilitySpec EvadeAbilitySpec(
		UAIRECompanionAutonomousEvadeAbility::StaticClass(),
		1,
		INDEX_NONE,
		this);
	AutonomousEvadeAbilityHandle =
		AbilitySystemComponent->GiveAbility(EvadeAbilitySpec);
	if (!AutonomousEvadeAbilityHandle.IsValid())
	{
		return false;
	}

	FGameplayEffectContextHandle EffectContext =
		AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(this);
	FGameplayEffectSpecHandle RegenSpec =
		AbilitySystemComponent->MakeOutgoingSpec(
			UAIRECompanionStaminaRegenGameplayEffect::StaticClass(),
			1.0f,
			EffectContext);
	if (!RegenSpec.IsValid())
	{
		ShutdownAutonomousEvadeRuntime();
		return false;
	}
	RegenSpec.Data->SetSetByCallerMagnitude(
		AIRECompanionGameplayTags::DataStaminaRegenPerTick,
		CompanionConfigData->AutonomousEvade.StaminaRegenRate
			* StaminaRegenPeriod);
	StaminaRegenEffectHandle =
		AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(
			*RegenSpec.Data.Get());
	if (!StaminaRegenEffectHandle.WasSuccessfullyApplied())
	{
		StaminaRegenEffectHandle.Invalidate();
		ShutdownAutonomousEvadeRuntime();
		return false;
	}
	return true;
}

void AAIRECompanionCharacter::ShutdownAutonomousEvadeRuntime()
{
	if (!IsValid(AbilitySystemComponent))
	{
		AutonomousEvadeAbilityHandle = FGameplayAbilitySpecHandle();
		StaminaRegenEffectHandle.Invalidate();
		return;
	}

	if (AutonomousEvadeAbilityHandle.IsValid())
	{
		AbilitySystemComponent->CancelAbilityHandle(
			AutonomousEvadeAbilityHandle);
		AbilitySystemComponent->ClearAbility(
			AutonomousEvadeAbilityHandle);
		AutonomousEvadeAbilityHandle = FGameplayAbilitySpecHandle();
	}
	if (StaminaRegenEffectHandle.IsValid())
	{
		AbilitySystemComponent->RemoveActiveGameplayEffect(
			StaminaRegenEffectHandle);
		StaminaRegenEffectHandle.Invalidate();
	}
}

void AAIRECompanionCharacter::HandleHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	SynchronizeDeadState(ChangeData.NewValue);
	const float Delta = ChangeData.NewValue - ChangeData.OldValue;
	const TCHAR* ChangeType = Delta < -KINDA_SMALL_NUMBER
		? TEXT("Damage")
		: Delta > KINDA_SMALL_NUMBER
			? TEXT("Healing")
			: TEXT("Unchanged");
	const float MaxHealth = IsValid(CompanionAttributeSet)
		? CompanionAttributeSet->GetMaxHealth()
		: 0.0f;
	UE_LOG(
		LogAIRECompanionCharacter,
		Log,
		TEXT("[MAKO HEALTH] Companion=%s Change=%s Delta=%+.2f Health=%.2f/%.2f Previous=%.2f Dead=%s"),
		*GetNameSafe(this),
		ChangeType,
		Delta,
		ChangeData.NewValue,
		MaxHealth,
		ChangeData.OldValue,
		ChangeData.NewValue <= 0.0f ? TEXT("true") : TEXT("false"));
}

void AAIRECompanionCharacter::HandleInvulnerableStateChanged(
	const FGameplayTag,
	const int32 NewCount)
{
	UE_LOG(
		LogAIRECompanionCharacter,
		Log,
		TEXT("[MAKO EVADE] Invulnerable=%s Companion=%s TagCount=%d"),
		NewCount > 0 ? TEXT("ON") : TEXT("OFF"),
		*GetNameSafe(this),
		NewCount);
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
	if (CurrentHealth <= 0.0f)
	{
		if (AutonomousEvadeAbilityHandle.IsValid())
		{
			AbilitySystemComponent->CancelAbilityHandle(
				AutonomousEvadeAbilityHandle);
		}
		if (IsValid(CombatEvadeComponent))
		{
			CombatEvadeComponent->CancelEvade();
		}
	}
}

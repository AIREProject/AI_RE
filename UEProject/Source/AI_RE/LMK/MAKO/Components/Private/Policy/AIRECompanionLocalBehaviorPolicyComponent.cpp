#include "Policy/AIRECompanionLocalBehaviorPolicyComponent.h"

#include "Core/AIRECompanionCharacter.h"
#include "Core/AIRECompanionConfigDataAsset.h"

DEFINE_LOG_CATEGORY_STATIC(LogAIRECompanionPolicy, Log, All);

UAIRECompanionLocalBehaviorPolicyComponent::
	UAIRECompanionLocalBehaviorPolicyComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bWantsInitializeComponent = true;
	SetIsReplicatedByDefault(false);
}

FAIRECompanionLocalBehaviorPolicy
UAIRECompanionLocalBehaviorPolicyComponent::GetLocalBehaviorPolicy() const
{
	return CurrentPolicy;
}

bool UAIRECompanionLocalBehaviorPolicyComponent::SetLocalBehaviorPolicy(
	const FAIRECompanionLocalBehaviorPolicy NewPolicy)
{
	if (!bIsInitialized || !NewPolicy.IsValid())
	{
		return false;
	}

	if (CurrentPolicy == NewPolicy)
	{
		return true;
	}

	const FAIRECompanionLocalBehaviorPolicy PreviousPolicy = CurrentPolicy;
	CurrentPolicy = NewPolicy;
	OnLocalBehaviorPolicyChanged.Broadcast(PreviousPolicy, CurrentPolicy);

	UE_LOG(
		LogAIRECompanionPolicy,
		Log,
		TEXT("Local behavior policy changed. Companion=%s Engagement=%d Role=%d"),
		*GetNameSafe(GetOwner()),
		static_cast<int32>(CurrentPolicy.EngagementPolicy),
		static_cast<int32>(CurrentPolicy.RolePreference));
	return true;
}

void UAIRECompanionLocalBehaviorPolicyComponent::InitializeComponent()
{
	Super::InitializeComponent();

	CurrentPolicy = FAIRECompanionLocalBehaviorPolicy();
	if (const AAIRECompanionCharacter* Companion =
			Cast<AAIRECompanionCharacter>(GetOwner());
		IsValid(Companion))
	{
		if (const UAIRECompanionConfigDataAsset* CompanionConfig =
				Companion->GetCompanionConfig();
			IsValid(CompanionConfig))
		{
			CurrentPolicy.EngagementPolicy =
				CompanionConfig->DefaultEngagementPolicy;
			CurrentPolicy.RolePreference =
				CompanionConfig->DefaultRolePreference;
		}
	}

	bIsInitialized = CurrentPolicy.IsValid();
	ensureMsgf(
		bIsInitialized,
		TEXT("Companion local behavior policy initialized with invalid values."));
}

void UAIRECompanionLocalBehaviorPolicyComponent::UninitializeComponent()
{
	bIsInitialized = false;
	OnLocalBehaviorPolicyChanged.Clear();
	Super::UninitializeComponent();
}

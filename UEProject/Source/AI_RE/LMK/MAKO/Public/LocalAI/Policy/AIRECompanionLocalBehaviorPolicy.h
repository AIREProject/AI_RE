#pragma once

#include "CoreMinimal.h"
#include "AIRECompanionLocalBehaviorPolicy.generated.h"

UENUM(BlueprintType)
enum class EAIRECompanionEngagementPolicy : uint8
{
	HoldFire UMETA(DisplayName = "Hold Fire"),
	DefendPlayer UMETA(DisplayName = "Defend Player"),
	Aggressive UMETA(DisplayName = "Aggressive")
};

UENUM(BlueprintType)
enum class EAIRECompanionRolePreference : uint8
{
	Balanced UMETA(DisplayName = "Balanced"),
	SupportPriority UMETA(DisplayName = "Support Priority")
};

USTRUCT(BlueprintType)
struct AI_RE_API FAIRECompanionLocalBehaviorPolicy
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AIRE|Companion|Policy")
	EAIRECompanionEngagementPolicy EngagementPolicy =
		EAIRECompanionEngagementPolicy::Aggressive;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AIRE|Companion|Policy")
	EAIRECompanionRolePreference RolePreference =
		EAIRECompanionRolePreference::Balanced;

	bool IsValid() const
	{
		const bool bHasValidEngagementPolicy =
			EngagementPolicy == EAIRECompanionEngagementPolicy::HoldFire
			|| EngagementPolicy == EAIRECompanionEngagementPolicy::DefendPlayer
			|| EngagementPolicy == EAIRECompanionEngagementPolicy::Aggressive;
		const bool bHasValidRolePreference =
			RolePreference == EAIRECompanionRolePreference::Balanced
			|| RolePreference == EAIRECompanionRolePreference::SupportPriority;
		return bHasValidEngagementPolicy && bHasValidRolePreference;
	}

	bool operator==(const FAIRECompanionLocalBehaviorPolicy& Other) const
	{
		return EngagementPolicy == Other.EngagementPolicy
			&& RolePreference == Other.RolePreference;
	}

	bool operator!=(const FAIRECompanionLocalBehaviorPolicy& Other) const
	{
		return !(*this == Other);
	}
};

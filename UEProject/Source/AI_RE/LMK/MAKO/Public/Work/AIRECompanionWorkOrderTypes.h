#pragma once

#include "CoreMinimal.h"
#include "AIRECompanionWorkOrderTypes.generated.h"

class AActor;
class UDataTable;

UENUM(BlueprintType)
enum class EAIRECompanionWorkOrderType : uint8
{
	None UMETA(DisplayName = "None"),
	Crafting UMETA(DisplayName = "Crafting"),
	Harvesting UMETA(DisplayName = "Harvesting")
};

UENUM(BlueprintType)
enum class EAIRECompanionWorkOrderState : uint8
{
	None UMETA(DisplayName = "None"),
	Requested UMETA(DisplayName = "Requested"),
	Moving UMETA(DisplayName = "Moving"),
	Working UMETA(DisplayName = "Working"),
	PausedByCombat UMETA(DisplayName = "Paused By Combat"),
	Completed UMETA(DisplayName = "Completed"),
	Cancelled UMETA(DisplayName = "Cancelled"),
	Failed UMETA(DisplayName = "Failed")
};

USTRUCT(BlueprintType)
struct AI_RE_API FAIRECompanionWorkOrderRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AIRE|Companion|Work")
	EAIRECompanionWorkOrderType WorkType =
		EAIRECompanionWorkOrderType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Transient, Category = "AIRE|Companion|Work")
	TWeakObjectPtr<AActor> TargetActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Transient, Category = "AIRE|Companion|Work")
	TObjectPtr<UDataTable> RecipeTable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AIRE|Companion|Work")
	FName RecipeRowId = NAME_None;
};

USTRUCT(BlueprintType)
struct AI_RE_API FAIRECompanionWorkOrderSnapshot
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Work")
	FGuid WorkOrderId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "AIRE|Companion|Work")
	TWeakObjectPtr<AActor> TargetActor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Work")
	EAIRECompanionWorkOrderType WorkType =
		EAIRECompanionWorkOrderType::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "AIRE|Companion|Work")
	TObjectPtr<UDataTable> RecipeTable;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Work")
	FName RecipeRowId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Companion|Work")
	EAIRECompanionWorkOrderState State =
		EAIRECompanionWorkOrderState::None;
};

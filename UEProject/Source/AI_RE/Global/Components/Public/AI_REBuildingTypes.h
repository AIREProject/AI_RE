#pragma once

#include "CoreMinimal.h"
#include "AI_REBuildingTypes.generated.h"

class AAI_REBuildingPieceActor;
class UMaterialInterface;
class UTexture2D;

UENUM(BlueprintType)
enum class EAI_REBuildingPieceType : uint8
{
	Foundation,
	Wall,
	Roof
};

UENUM(BlueprintType)
enum class EAI_REBuildingMaterialType : uint8
{
	Wood,
	Stone,
	Iron
};

USTRUCT(BlueprintType)
struct AI_RE_API FAI_REBuildingMaterialProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI_RE|Building")
	EAI_REBuildingMaterialType MaterialType = EAI_REBuildingMaterialType::Wood;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI_RE|Building")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI_RE|Building")
	TObjectPtr<UMaterialInterface> MaterialOverride = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI_RE|Building", meta = (ClampMin = "1.0"))
	float MaxDurability = 500.0f;
};

USTRUCT(BlueprintType)
struct AI_RE_API FAI_REBuildingSnapPoint
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI_RE|Building")
	FName SnapId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI_RE|Building")
	EAI_REBuildingPieceType AcceptsPieceType = EAI_REBuildingPieceType::Wall;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI_RE|Building")
	FTransform LocalTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI_RE|Building")
	FTransform ChildLocalTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI_RE|Building", meta = (ClampMin = "0.0"))
	float SnapRadius = 120.0f;
};

USTRUCT(BlueprintType)
struct AI_RE_API FAI_REBuildingPieceDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI_RE|Building")
	EAI_REBuildingPieceType PieceType = EAI_REBuildingPieceType::Foundation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI_RE|Building")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI_RE|Building")
	TSubclassOf<AAI_REBuildingPieceActor> PieceClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI_RE|Building")
	TObjectPtr<UTexture2D> Icon = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI_RE|Building", meta = (ClampMin = "0.0"))
	float RotationStepDegrees = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI_RE|Building")
	TObjectPtr<UMaterialInterface> PreviewValidMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI_RE|Building")
	TObjectPtr<UMaterialInterface> PreviewInvalidMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI_RE|Building")
	FVector PlacementOverlapExtent = FVector(120.0f, 120.0f, 120.0f);
};

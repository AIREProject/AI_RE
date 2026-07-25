#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../../Components/Public/AI_REBuildingTypes.h"
#include "AI_REBuildingPieceActor.generated.h"

class UMaterialInterface;
class USceneComponent;
class UStaticMeshComponent;

UCLASS()
class AI_RE_API AAI_REBuildingPieceActor : public AActor
{
	GENERATED_BODY()
	
public:	
	AAI_REBuildingPieceActor();

	virtual void BeginPlay() override;
	virtual void Destroyed() override;

	UFUNCTION(BlueprintCallable, Category = "AI_RE|Building")
	void InitializeBuildingPiece(
		EAI_REBuildingPieceType InPieceType,
		EAI_REBuildingMaterialType InMaterialType,
		float InMaxDurability,
		UMaterialInterface* InMaterialOverride,
		AAI_REBuildingPieceActor* InParentPiece,
		FName InParentSnapId);

	UFUNCTION(BlueprintCallable, Category = "AI_RE|Building")
	void ApplyBuildingDamage(float DamageAmount);

	UFUNCTION(BlueprintCallable, Category = "AI_RE|Building")
	void DestroyPiece();

	UFUNCTION(BlueprintPure, Category = "AI_RE|Building")
	EAI_REBuildingPieceType GetPieceType() const { return PieceType; }

	UFUNCTION(BlueprintPure, Category = "AI_RE|Building")
	AAI_REBuildingPieceActor* GetParentPiece() const { return ParentPiece; }

	UFUNCTION(BlueprintCallable, Category = "AI_RE|Building")
	bool FindSnapPoint(FName SnapId, FAI_REBuildingSnapPoint& OutSnapPoint) const;

	UFUNCTION(BlueprintCallable, Category = "AI_RE|Building")
	bool FindNearestCompatibleSnapPoint(
		EAI_REBuildingPieceType DesiredPieceType,
		const FVector& QueryLocation,
		FAI_REBuildingSnapPoint& OutSnapPoint,
		FTransform& OutSnapTransform) const;

	UFUNCTION(BlueprintCallable, Category = "AI_RE|Building")
	bool IsSnapPointAvailable(FName SnapId) const;

	UFUNCTION(BlueprintCallable, Category = "AI_RE|Building")
	void ApplyPreviewMaterial(UMaterialInterface* PreviewMaterial);

	UFUNCTION(BlueprintCallable, Category = "AI_RE|Building")
	void ClearPreviewMaterial();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI_RE|Building")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI_RE|Building")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI_RE|Building")
	EAI_REBuildingPieceType DefaultPieceType = EAI_REBuildingPieceType::Foundation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI_RE|Building")
	TArray<FAI_REBuildingSnapPoint> SnapPoints;

	UPROPERTY(BlueprintReadOnly, Category = "AI_RE|Building")
	EAI_REBuildingPieceType PieceType = EAI_REBuildingPieceType::Foundation;

	UPROPERTY(BlueprintReadOnly, Category = "AI_RE|Building")
	EAI_REBuildingMaterialType MaterialType = EAI_REBuildingMaterialType::Wood;

	UPROPERTY(BlueprintReadOnly, Category = "AI_RE|Building")
	float CurrentDurability = 500.0f;

	UPROPERTY(BlueprintReadOnly, Category = "AI_RE|Building")
	float MaxDurability = 500.0f;

	UPROPERTY(BlueprintReadOnly, Category = "AI_RE|Building")
	TObjectPtr<AAI_REBuildingPieceActor> ParentPiece;

	UPROPERTY(BlueprintReadOnly, Category = "AI_RE|Building")
	FName ParentSnapId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "AI_RE|Building")
	TArray<TObjectPtr<AAI_REBuildingPieceActor>> ChildPieces;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> RuntimeMaterialOverride;
};

#pragma once

#include "CoreMinimal.h"
#include "AI_REBuildingTypes.h"
#include "Components/ActorComponent.h"
#include "AI_REPlayerBuildingPlacementComponent.generated.h"

class AAI_REBuildingPieceActor;

UCLASS(ClassGroup = (AI_RE), meta = (BlueprintSpawnableComponent))
class AI_RE_API UAI_REPlayerBuildingPlacementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAI_REPlayerBuildingPlacementComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "AI_RE|Building|Placement")
	void EnterPlacementMode(EAI_REBuildingPieceType PieceType, EAI_REBuildingMaterialType MaterialType);

	UFUNCTION(BlueprintCallable, Category = "AI_RE|Building|Placement")
	void CancelPlacement();

	UFUNCTION(BlueprintCallable, Category = "AI_RE|Building|Placement")
	void ConfirmPlacement();

	UFUNCTION(BlueprintCallable, Category = "AI_RE|Building|Placement")
	void RotatePreviewByWheel(float WheelDelta);

	UFUNCTION(BlueprintPure, Category = "AI_RE|Building|Placement")
	bool IsPlacementModeActive() const { return bPlacementModeActive; }

	UFUNCTION(BlueprintPure, Category = "AI_RE|Building|Placement")
	bool CanPlaceAtCurrentPreview() const { return bCanPlaceAtCurrentPreview; }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI_RE|Building|Placement")
	TArray<FAI_REBuildingPieceDefinition> PieceDefinitions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI_RE|Building|Placement")
	TArray<FAI_REBuildingMaterialProfile> MaterialProfiles;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI_RE|Building|Placement", meta = (ClampMin = "1.0"))
	float PlacementTraceDistance = 6000.0f;

	UFUNCTION(BlueprintImplementableEvent, Category = "AI_RE|Building|Placement", meta = (DisplayName = "On Placement Mode Changed"))
	void BP_OnPlacementModeChanged(bool bActive);

	UFUNCTION(BlueprintImplementableEvent, Category = "AI_RE|Building|Placement", meta = (DisplayName = "On Placement Preview Changed"))
	void BP_OnPlacementPreviewChanged(bool bCanPlace);

private:
	bool bPlacementModeActive = false;
	bool bCanPlaceAtCurrentPreview = false;
	float LastPolledMouseWheelAxis = 0.0f;
	float CurrentYawOffset = 0.0f;
	FTransform CurrentPlacementTransform = FTransform::Identity;
	
	EAI_REBuildingPieceType SelectedPieceType = EAI_REBuildingPieceType::Foundation;
	EAI_REBuildingMaterialType SelectedMaterialType = EAI_REBuildingMaterialType::Wood;
	
	FName CurrentParentSnapId = NAME_None;

	UPROPERTY(Transient)
	TObjectPtr<AAI_REBuildingPieceActor> CurrentParentPiece;

	UPROPERTY(Transient)
	TObjectPtr<AAI_REBuildingPieceActor> PreviewActor;

	// Singleplayer core logic (replaced Server RPCs)
	void PlacePieceLocal(EAI_REBuildingPieceType PieceType, EAI_REBuildingMaterialType MaterialType, FTransform PlacementTransform, AAI_REBuildingPieceActor* ParentPiece, FName ParentSnapId);
	
	const FAI_REBuildingPieceDefinition* FindPieceDefinition(EAI_REBuildingPieceType PieceType) const;
	const FAI_REBuildingMaterialProfile* FindMaterialProfile(EAI_REBuildingMaterialType MaterialType) const;
	
	void SetPlacementModeActive(bool bNewActive);
	void UpdatePlacementPreview();
	void DestroyPreviewActor();
	bool ResolvePlacementView(FVector& OutViewLocation, FVector& OutViewDirection) const;
};

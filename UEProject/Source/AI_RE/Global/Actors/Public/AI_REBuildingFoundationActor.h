#pragma once
#include "CoreMinimal.h"
#include "AI_REBuildingPieceActor.h"
#include "AI_REBuildingFoundationActor.generated.h"

UCLASS()
class AI_RE_API AAI_REBuildingFoundationActor : public AAI_REBuildingPieceActor
{
	GENERATED_BODY()
public:
	AAI_REBuildingFoundationActor();

	UFUNCTION(BlueprintCallable, Category = "AI_RE|Building")
	bool ResolveSnapPlacement(EAI_REBuildingPieceType InPieceType, const FVector& QueryLocation, float YawOffset, FTransform& OutPlacementTransform, FName& OutSnapId) const;

	UFUNCTION(BlueprintCallable, Category = "AI_RE|Building")
	bool ResolveSnapPlacementById(EAI_REBuildingPieceType InPieceType, FName SnapId, float YawOffset, FTransform& OutPlacementTransform) const;
};

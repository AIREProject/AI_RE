#include "AI_REBuildingFoundationActor.h"

AAI_REBuildingFoundationActor::AAI_REBuildingFoundationActor()
{
	DefaultPieceType = EAI_REBuildingPieceType::Foundation;
}

bool AAI_REBuildingFoundationActor::ResolveSnapPlacement(EAI_REBuildingPieceType InPieceType, const FVector& QueryLocation, float YawOffset, FTransform& OutPlacementTransform, FName& OutSnapId) const
{
	FAI_REBuildingSnapPoint FoundSnap;
	if (FindNearestCompatibleSnapPoint(InPieceType, QueryLocation, FoundSnap, OutPlacementTransform))
	{
		OutSnapId = FoundSnap.SnapId;
		return true;
	}
	return false;
}

bool AAI_REBuildingFoundationActor::ResolveSnapPlacementById(EAI_REBuildingPieceType InPieceType, FName SnapId, float YawOffset, FTransform& OutPlacementTransform) const
{
	FAI_REBuildingSnapPoint FoundSnap;
	if (FindSnapPoint(SnapId, FoundSnap) && FoundSnap.AcceptsPieceType == InPieceType)
	{
		OutPlacementTransform = FoundSnap.LocalTransform * GetActorTransform();
		return true;
	}
	return false;
}

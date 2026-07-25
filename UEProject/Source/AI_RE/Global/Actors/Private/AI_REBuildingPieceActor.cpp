#include "AI_REBuildingPieceActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"

AAI_REBuildingPieceActor::AAI_REBuildingPieceActor()
{
	PrimaryActorTick.bCanEverTick = false;
	// bReplicates = true; // Removed for singleplayer

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(SceneRoot);
}

void AAI_REBuildingPieceActor::BeginPlay()
{
	Super::BeginPlay();
	PieceType = DefaultPieceType;
}

void AAI_REBuildingPieceActor::Destroyed()
{
	// Local destroy logic
	Super::Destroyed();
}

void AAI_REBuildingPieceActor::InitializeBuildingPiece(
	EAI_REBuildingPieceType InPieceType,
	EAI_REBuildingMaterialType InMaterialType,
	float InMaxDurability,
	UMaterialInterface* InMaterialOverride,
	AAI_REBuildingPieceActor* InParentPiece,
	FName InParentSnapId)
{
	PieceType = InPieceType;
	MaterialType = InMaterialType;
	MaxDurability = InMaxDurability;
	CurrentDurability = MaxDurability;
	RuntimeMaterialOverride = InMaterialOverride;
	ParentPiece = InParentPiece;
	ParentSnapId = InParentSnapId;

	if (RuntimeMaterialOverride && MeshComponent)
	{
		for (int32 i = 0; i < MeshComponent->GetNumMaterials(); ++i)
		{
			MeshComponent->SetMaterial(i, RuntimeMaterialOverride);
		}
	}
}

void AAI_REBuildingPieceActor::ApplyBuildingDamage(float DamageAmount)
{
	CurrentDurability -= DamageAmount;
	if (CurrentDurability <= 0.0f)
	{
		DestroyPiece();
	}
}

void AAI_REBuildingPieceActor::DestroyPiece()
{
	for (AAI_REBuildingPieceActor* Child : ChildPieces)
	{
		if (IsValid(Child))
		{
			Child->DestroyPiece();
		}
	}
	Destroy();
}

bool AAI_REBuildingPieceActor::FindSnapPoint(FName SnapId, FAI_REBuildingSnapPoint& OutSnapPoint) const
{
	for (const auto& Pt : SnapPoints)
	{
		if (Pt.SnapId == SnapId)
		{
			OutSnapPoint = Pt;
			return true;
		}
	}
	return false;
}

bool AAI_REBuildingPieceActor::FindNearestCompatibleSnapPoint(
	EAI_REBuildingPieceType DesiredPieceType,
	const FVector& QueryLocation,
	FAI_REBuildingSnapPoint& OutSnapPoint,
	FTransform& OutSnapTransform) const
{
	bool bFound = false;
	float BestDistSq = MAX_flt;

	for (const auto& Pt : SnapPoints)
	{
		if (Pt.AcceptsPieceType == DesiredPieceType)
		{
			FVector SnapWorldLoc = GetActorTransform().TransformPosition(Pt.LocalTransform.GetLocation());
			float DistSq = FVector::DistSquared(SnapWorldLoc, QueryLocation);
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				OutSnapPoint = Pt;
				OutSnapTransform = Pt.LocalTransform * GetActorTransform();
				bFound = true;
			}
		}
	}
	return bFound;
}

bool AAI_REBuildingPieceActor::IsSnapPointAvailable(FName SnapId) const
{
	return true; // Simple logic for singleplayer stub
}

void AAI_REBuildingPieceActor::ApplyPreviewMaterial(UMaterialInterface* PreviewMaterial)
{
	if (MeshComponent && PreviewMaterial)
	{
		for (int32 i = 0; i < MeshComponent->GetNumMaterials(); ++i)
		{
			MeshComponent->SetMaterial(i, PreviewMaterial);
		}
	}
}

void AAI_REBuildingPieceActor::ClearPreviewMaterial()
{
	// Revert logic
}

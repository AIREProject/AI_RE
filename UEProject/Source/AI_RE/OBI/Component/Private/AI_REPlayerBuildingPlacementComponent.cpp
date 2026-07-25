#include "AI_REPlayerBuildingPlacementComponent.h"
#include "../../Global/Actors/Public/AI_REBuildingPieceActor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

UAI_REPlayerBuildingPlacementComponent::UAI_REPlayerBuildingPlacementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	// SetIsReplicatedByDefault(true); // Removed for singleplayer
}

void UAI_REPlayerBuildingPlacementComponent::BeginPlay()
{
	Super::BeginPlay();
	SetComponentTickEnabled(false);
}

void UAI_REPlayerBuildingPlacementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyPreviewActor();
	Super::EndPlay(EndPlayReason);
}

void UAI_REPlayerBuildingPlacementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bPlacementModeActive)
	{
		UpdatePlacementPreview();
	}
}

void UAI_REPlayerBuildingPlacementComponent::EnterPlacementMode(EAI_REBuildingPieceType PieceType, EAI_REBuildingMaterialType MaterialType)
{
	SelectedPieceType = PieceType;
	SelectedMaterialType = MaterialType;
	CurrentYawOffset = 0.0f;
	LastPolledMouseWheelAxis = 0.0f;
	CurrentParentPiece = nullptr;
	CurrentParentSnapId = NAME_None;
	SetPlacementModeActive(true);
}

void UAI_REPlayerBuildingPlacementComponent::CancelPlacement()
{
	SetPlacementModeActive(false);
}

void UAI_REPlayerBuildingPlacementComponent::ConfirmPlacement()
{
	if (!bPlacementModeActive || !bCanPlaceAtCurrentPreview)
	{
		return;
	}

	// Singleplayer direct placement call
	PlacePieceLocal(SelectedPieceType, SelectedMaterialType, CurrentPlacementTransform, CurrentParentPiece, CurrentParentSnapId);
}

void UAI_REPlayerBuildingPlacementComponent::RotatePreviewByWheel(float WheelDelta)
{
	if (!bPlacementModeActive || FMath::IsNearlyZero(WheelDelta))
	{
		return;
	}

	const float AppliedRotationStep = 90.0f;
	CurrentYawOffset = FMath::UnwindDegrees(CurrentYawOffset + FMath::Sign(WheelDelta) * AppliedRotationStep);
	UpdatePlacementPreview();
}

void UAI_REPlayerBuildingPlacementComponent::PlacePieceLocal(EAI_REBuildingPieceType PieceType, EAI_REBuildingMaterialType MaterialType, FTransform PlacementTransform, AAI_REBuildingPieceActor* ParentPiece, FName ParentSnapId)
{
	const FAI_REBuildingPieceDefinition* PieceDefinition = FindPieceDefinition(PieceType);
	if (!PieceDefinition || !PieceDefinition->PieceClass)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		// AAI_REBuildingPieceActor* NewPiece = World->SpawnActor<AAI_REBuildingPieceActor>(PieceDefinition->PieceClass, PlacementTransform, SpawnParams);
		UE_LOG(LogTemp, Warning, TEXT("Placed building piece locally! Type: %d"), (int32)PieceType);
	}
}

void UAI_REPlayerBuildingPlacementComponent::SetPlacementModeActive(bool bNewActive)
{
	bPlacementModeActive = bNewActive;
	SetComponentTickEnabled(bPlacementModeActive);

	if (!bPlacementModeActive)
	{
		bCanPlaceAtCurrentPreview = false;
		DestroyPreviewActor();
		BP_OnPlacementModeChanged(false);
	}
	else
	{
		UpdatePlacementPreview();
		BP_OnPlacementModeChanged(true);
	}
}

void UAI_REPlayerBuildingPlacementComponent::UpdatePlacementPreview()
{
	// Simplified Trace Logic for Preview Location
	FVector ViewLoc, ViewDir;
	if (ResolvePlacementView(ViewLoc, ViewDir))
	{
		FVector TraceEnd = ViewLoc + (ViewDir * PlacementTraceDistance);
		
		FRotator PlacementRot = FRotator::ZeroRotator;
		PlacementRot.Yaw = CurrentYawOffset;
		
		CurrentPlacementTransform = FTransform(PlacementRot, TraceEnd);
		bCanPlaceAtCurrentPreview = true;
	}
	
	BP_OnPlacementPreviewChanged(bCanPlaceAtCurrentPreview);
}

void UAI_REPlayerBuildingPlacementComponent::DestroyPreviewActor()
{
	if (PreviewActor)
	{
		// PreviewActor->Destroy();
		PreviewActor = nullptr;
	}
}

bool UAI_REPlayerBuildingPlacementComponent::ResolvePlacementView(FVector& OutViewLocation, FVector& OutViewDirection) const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	const APlayerController* PC = OwnerPawn ? Cast<APlayerController>(OwnerPawn->GetController()) : nullptr;
	
	if (PC)
	{
		FRotator ViewRot;
		PC->GetPlayerViewPoint(OutViewLocation, ViewRot);
		OutViewDirection = ViewRot.Vector();
		return true;
	}
	return false;
}

const FAI_REBuildingPieceDefinition* UAI_REPlayerBuildingPlacementComponent::FindPieceDefinition(EAI_REBuildingPieceType PieceType) const
{
	for (const auto& Def : PieceDefinitions)
	{
		if (Def.PieceType == PieceType) return &Def;
	}
	return nullptr;
}

const FAI_REBuildingMaterialProfile* UAI_REPlayerBuildingPlacementComponent::FindMaterialProfile(EAI_REBuildingMaterialType MaterialType) const
{
	for (const auto& Profile : MaterialProfiles)
	{
		if (Profile.MaterialType == MaterialType) return &Profile;
	}
	return nullptr;
}

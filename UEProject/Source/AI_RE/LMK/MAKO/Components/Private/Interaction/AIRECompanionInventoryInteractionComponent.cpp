#include "Interaction/AIRECompanionInventoryInteractionComponent.h"

#include "Core/AIRECompanionCharacter.h"
#include "Engine/World.h"
#include "Inventory/UI/AIREInventoryUIWorldSubsystem.h"

UAIRECompanionInventoryInteractionComponent::
	UAIRECompanionInventoryInteractionComponent(
		const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	InitSphereRadius(75.0f);
	BodyInstance.SetCollisionEnabled(ECollisionEnabled::QueryOnly, false);
	BodyInstance.SetObjectType(ECC_WorldDynamic);
	BodyInstance.SetResponseToAllChannels(ECR_Ignore);
	BodyInstance.SetResponseToChannel(ECC_Visibility, ECR_Block);
	SetGenerateOverlapEvents(false);
}

void UAIRECompanionInventoryInteractionComponent::Interact_Implementation(
	AActor* Interactor)
{
	if (!IsValid(Interactor))
	{
		return;
	}

	AAIRECompanionCharacter* Companion = Cast<AAIRECompanionCharacter>(
		GetOwner());
	if (!IsValid(Companion))
	{
		return;
	}

	UWorld* World = GetWorld();
	UAIREInventoryUIWorldSubsystem* InventoryUI = IsValid(World)
		? World->GetSubsystem<UAIREInventoryUIWorldSubsystem>()
		: nullptr;
	if (!IsValid(InventoryUI))
	{
		return;
	}

	InventoryUI->OpenCompanionInventory(Companion, Interactor);
}

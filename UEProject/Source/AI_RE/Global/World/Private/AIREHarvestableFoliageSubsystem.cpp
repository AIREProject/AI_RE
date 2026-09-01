#include "AIREHarvestableFoliageSubsystem.h"

#include "AIREHarvestableFoliageConfig.h"
#include "AIREHarvestableFoliageProxyActor.h"
#include "AI_REHarvestableResourceComponent.h"
#include "AI_REItemActor.h"
#include "AI_REItemDataAsset.h"
#include "Core/AIRECompanionCharacter.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "FoliageInstancedStaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "InstancedFoliageActor.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Work/AIRECompanionWorkOrderComponent.h"
#include "Work/AIRECompanionWorkOrderTypes.h"

namespace
{
constexpr float ScanInterval = 0.5f;
constexpr float ActivationRadius = 3000.0f;
constexpr float DeactivationRadius = 4000.0f;
constexpr int32 MaxActiveProxies = 48;
constexpr TCHAR ConfigPath[] =
	TEXT("/Game/Work/Global/Harvest/DA_AIREHarvestableFoliageConfig.DA_AIREHarvestableFoliageConfig");
constexpr TCHAR RewardPath[] =
	TEXT("/Game/Work/OBI/Datas/DA_PlantStem.DA_PlantStem");
constexpr TCHAR DropClassPath[] =
	TEXT("/Game/Work/OBI/Blueprints/BP_ItemActor.BP_ItemActor_C");
constexpr TCHAR ProxyClassPath[] =
	TEXT("/Game/Work/Global/Harvest/BP_AIREHarvestableFoliageProxy.BP_AIREHarvestableFoliageProxy_C");

const TCHAR* const DefaultMeshes[] = {
		TEXT("/Game/Work/OBI/Assets/Stylized_Tree_Pack/Meshes/Oak/SM_Stylized_Tree_Oak_03.SM_Stylized_Tree_Oak_03"),
		TEXT("/Game/Work/OBI/Assets/Stylized_Tree_Pack/Meshes/Oak/SM_Stylized_Tree_Oak_09.SM_Stylized_Tree_Oak_09"),
		TEXT("/Game/Work/OBI/Assets/Stylized_Tree_Pack/Meshes/Oak/SM_Stylized_Tree_Oak_10.SM_Stylized_Tree_Oak_10"),
	};
}

void UAIREHarvestableFoliageSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	Config = Cast<UAIREHarvestableFoliageConfig>(
		StaticLoadObject(
			UAIREHarvestableFoliageConfig::StaticClass(), nullptr, ConfigPath));
	if (IsValid(Config))
	{
		for (const FAIREHarvestableFoliageTypeConfig& Type : Config->Types)
		{
			if (UStaticMesh* Mesh = Type.SourceMesh.LoadSynchronous())
			{
				AllowedMeshes.Add(Mesh);
			}
		}
		RewardItem = Config->RewardItem;
		DroppedItemClass = Config->DroppedItemClass;
	}
	if (AllowedMeshes.IsEmpty())
	{
		for (const TCHAR* Path : DefaultMeshes)
		{
			if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, Path))
			{
				AllowedMeshes.Add(Mesh);
			}
		}
	}
	if (!IsValid(RewardItem))
	{
		RewardItem = LoadObject<UAI_REItemDataAsset>(nullptr, RewardPath);
	}
	if (!DroppedItemClass)
	{
		DroppedItemClass = LoadClass<AAI_REItemActor>(nullptr, DropClassPath);
	}
	ProxyActorClass = LoadClass<AAIREHarvestableFoliageProxyActor>(
		nullptr, ProxyClassPath);
	if (!ProxyActorClass)
	{
		ProxyActorClass = AAIREHarvestableFoliageProxyActor::StaticClass();
	}
	InWorld.GetTimerManager().SetTimer(
		ScanTimer,
		this,
		&UAIREHarvestableFoliageSubsystem::ScanNearbyInstances,
		ScanInterval,
		true,
		ScanInterval);
}

void UAIREHarvestableFoliageSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ScanTimer);
	}
	for (int32 Index = ActiveProxies.Num() - 1; Index >= 0; --Index)
	{
		RestoreProxy(Index);
	}
	Config = nullptr;
	AllowedMeshes.Reset();
	RewardItem = nullptr;
	DroppedItemClass = nullptr;
	ProxyActorClass = nullptr;
	Super::Deinitialize();
}

void UAIREHarvestableFoliageSubsystem::ScanNearbyInstances()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	TArray<FVector> Centers;
	AActor* ActiveWorkTarget = nullptr;
	if (APawn* Player = UGameplayStatics::GetPlayerPawn(World, 0))
	{
		Centers.Add(Player->GetActorLocation());
	}
	for (TActorIterator<AAIRECompanionCharacter> It(World); It; ++It)
	{
		Centers.Add(It->GetActorLocation());
		if (UAIRECompanionWorkOrderComponent* WorkOrder =
			It->GetWorkOrderComponent())
		{
			ActiveWorkTarget =
				WorkOrder->GetWorkOrderSnapshot().TargetActor.Get();
		}
		break;
	}
	if (Centers.IsEmpty())
	{
		return;
	}

	for (int32 Index = ActiveProxies.Num() - 1; Index >= 0; --Index)
	{
		FActiveProxy& Active = ActiveProxies[Index];
		AAIREHarvestableFoliageProxyActor* Proxy = Active.Proxy.Get();
		if (!IsValid(Proxy))
		{
			RestoreProxy(Index);
			continue;
		}
		const bool bWithinRange = Centers.ContainsByPredicate(
			[Proxy](const FVector& Center)
			{
				return FVector::DistSquared(Center, Proxy->GetActorLocation())
					<= FMath::Square(DeactivationRadius);
			});
		const UAI_REHarvestableResourceComponent* Resource =
			Proxy->GetHarvestableResourceComponent();
		const bool bWasDamaged = IsValid(Resource)
			&& Resource->GetCurrentHealth() < Resource->GetMaxHealth();
		if (!bWithinRange && !bWasDamaged && Proxy != ActiveWorkTarget)
		{
			RestoreProxy(Index);
		}
	}

	if (ActiveProxies.Num() >= MaxActiveProxies)
	{
		return;
	}

	for (TActorIterator<AInstancedFoliageActor> It(World); It; ++It)
	{
		TInlineComponentArray<UFoliageInstancedStaticMeshComponent*> Components;
		It->GetComponents(Components);
		for (UFoliageInstancedStaticMeshComponent* Component : Components)
		{
			if (!IsValid(Component)
				|| !AllowedMeshes.Contains(Component->GetStaticMesh()))
			{
				continue;
			}
			for (const FVector& Center : Centers)
			{
				const TArray<int32> Instances =
					Component->GetInstancesOverlappingSphere(
						Center, ActivationRadius, true);
				for (const int32 InstanceIndex : Instances)
				{
					if (ActiveProxies.Num() >= MaxActiveProxies)
					{
						return;
					}
					if (IsTracked(Component, InstanceIndex))
					{
						continue;
					}
					FTransform OriginalTransform;
					if (!Component->GetInstanceTransform(
							InstanceIndex, OriginalTransform, true))
					{
						continue;
					}

					AAIREHarvestableFoliageProxyActor* Proxy =
						World->SpawnActorDeferred<AAIREHarvestableFoliageProxyActor>(
							ProxyActorClass,
							OriginalTransform,
							nullptr,
							nullptr,
							ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
					if (!IsValid(Proxy))
					{
						continue;
					}
					Proxy->Configure(
						Component->GetStaticMesh(), RewardItem, DroppedItemClass);
					Proxy->FinishSpawning(OriginalTransform);

					FTransform HiddenTransform = OriginalTransform;
					HiddenTransform.SetScale3D(FVector::ZeroVector);
					Component->UpdateInstanceTransform(
						InstanceIndex, HiddenTransform, true, true, true);
					FActiveProxy& Active = ActiveProxies.AddDefaulted_GetRef();
					Active.Component = Component;
					Active.InstanceIndex = InstanceIndex;
					Active.OriginalTransform = OriginalTransform;
					Active.Proxy = Proxy;
				}
			}
		}
	}
}

bool UAIREHarvestableFoliageSubsystem::IsTracked(
	const UHierarchicalInstancedStaticMeshComponent* Component,
	const int32 InstanceIndex) const
{
	return ActiveProxies.ContainsByPredicate(
		[Component, InstanceIndex](const FActiveProxy& Active)
		{
			return Active.Component.Get() == Component
				&& Active.InstanceIndex == InstanceIndex;
		});
}

void UAIREHarvestableFoliageSubsystem::RestoreProxy(const int32 ActiveIndex)
{
	if (!ActiveProxies.IsValidIndex(ActiveIndex))
	{
		return;
	}
	const FActiveProxy Active = ActiveProxies[ActiveIndex];
	if (UHierarchicalInstancedStaticMeshComponent* Component =
		Active.Component.Get())
	{
		Component->UpdateInstanceTransform(
			Active.InstanceIndex,
			Active.OriginalTransform,
			true,
			true,
			true);
	}
	if (AAIREHarvestableFoliageProxyActor* Proxy = Active.Proxy.Get())
	{
		Proxy->Destroy();
	}
	ActiveProxies.RemoveAtSwap(ActiveIndex, 1, EAllowShrinking::No);
}

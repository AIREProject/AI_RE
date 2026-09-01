#if WITH_DEV_AUTOMATION_TESTS

#include "Work/AIRECompanionHarvestableResourceQuery.h"

#include "AI_REHarvestGameplayTags.h"
#include "AI_REHarvestableResourceActor.h"
#include "AI_REHarvestableResourceComponent.h"
#include "AI_REItemActor.h"
#include "AI_REItemDataAsset.h"
#include "AI_REWorkbenchGameplayTags.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

namespace
{
	AAI_REHarvestableResourceActor* SpawnResource(
		UWorld& World,
		const FVector& Location,
		const FGameplayTag ResourceTag,
		UAI_REItemDataAsset* RewardItem)
	{
		FActorSpawnParameters Parameters;
		Parameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AAI_REHarvestableResourceActor* Resource =
			World.SpawnActor<AAI_REHarvestableResourceActor>(
				Location,
				FRotator::ZeroRotator,
				Parameters);
		if (!IsValid(Resource))
		{
			return nullptr;
		}

		Resource->ItemActorClass = AAI_REItemActor::StaticClass();
		Resource->GetHarvestableResourceComponent()->SetResourceDefaults(
			ResourceTag,
			RewardItem,
			1,
			25.0f);
		UBoxComponent* Collision = NewObject<UBoxComponent>(Resource);
		Resource->AddInstanceComponent(Collision);
		Collision->SetupAttachment(Resource->GetRootComponent());
		Collision->SetBoxExtent(FVector(30.0f));
		Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Collision->SetCollisionObjectType(ECC_WorldDynamic);
		Collision->SetCollisionResponseToAllChannels(ECR_Block);
		Collision->RegisterComponent();
		UBoxComponent* SensorCollision = NewObject<UBoxComponent>(Resource);
		Resource->AddInstanceComponent(SensorCollision);
		SensorCollision->SetupAttachment(Resource->GetRootComponent());
		SensorCollision->SetBoxExtent(FVector(60.0f));
		SensorCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		SensorCollision->SetCollisionObjectType(ECC_WorldDynamic);
		SensorCollision->SetCollisionResponseToAllChannels(ECR_Overlap);
		SensorCollision->RegisterComponent();
		return Resource;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAIRECompanionHarvestableResourceQueryAutomationTest,
	"AIRE.Companion.Work.HarvestableResourceQuery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAIRECompanionHarvestableResourceQueryAutomationTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	if (!TestNotNull(TEXT("Engine is available"), GEngine))
	{
		return false;
	}

	UWorld* TestWorld = UWorld::CreateWorld(
		EWorldType::Game,
		false,
		MakeUniqueObjectName(
			nullptr,
			UWorld::StaticClass(),
			NAME_None,
			EUniqueObjectNameOptions::GloballyUnique),
		GetTransientPackage());
	if (!TestNotNull(TEXT("Query world is created"), TestWorld))
	{
		return false;
	}
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(TestWorld);
	TestWorld->InitializeActorsForPlay(FURL());
	ON_SCOPE_EXIT
	{
		TestWorld->EndPlay(EEndPlayReason::Quit);
		GEngine->ShutdownWorldNetDriver(TestWorld);
		TestWorld->DestroyWorld(true);
		TestWorld->SetPhysicsScene(nullptr);
		GEngine->DestroyWorldContext(TestWorld);
	};

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Origin = TestWorld->SpawnActor<AActor>(
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParameters);
	UAI_REItemDataAsset* RewardItem = NewObject<UAI_REItemDataAsset>(TestWorld);
	RewardItem->ItemId = FName(TEXT("AIRE.Test.Wood"));
	UAI_REItemDataAsset* RockRewardItem = NewObject<UAI_REItemDataAsset>(TestWorld);
	RockRewardItem->ItemId = FName(TEXT("Rock"));
	AAI_REHarvestableResourceActor* LegacyRock = SpawnResource(
		*TestWorld,
		FVector(75.0f, 0.0f, 0.0f),
		AI_REHarvestGameplayTags::Resource_IronOre,
		RockRewardItem);
	AAI_REHarvestableResourceActor* WrongTag = SpawnResource(
		*TestWorld,
		FVector(50.0f, 0.0f, 0.0f),
		AI_REWorkbenchGameplayTags::Workbench_Basic,
		RewardItem);
	AAI_REHarvestableResourceActor* InvalidReward = SpawnResource(
		*TestWorld,
		FVector(25.0f, 0.0f, 0.0f),
		AI_REHarvestGameplayTags::Resource_Wood,
		RewardItem);
	AAI_REHarvestableResourceActor* Destroyed = SpawnResource(
		*TestWorld,
		FVector(30.0f, 0.0f, 0.0f),
		AI_REHarvestGameplayTags::Resource_Wood,
		RewardItem);
	AAI_REHarvestableResourceActor* NearestWood = SpawnResource(
		*TestWorld,
		FVector(100.0f, 0.0f, 0.0f),
		AI_REHarvestGameplayTags::Resource_Wood,
		RewardItem);
	for (int32 Index = 2; Index <= 9; ++Index)
	{
		SpawnResource(
			*TestWorld,
			FVector(Index * 100.0f, 0.0f, 0.0f),
			AI_REHarvestGameplayTags::Resource_Wood,
			RewardItem);
	}
	AAI_REHarvestableResourceActor* Depleted = SpawnResource(
		*TestWorld,
		FVector(1000.0f, 0.0f, 0.0f),
		AI_REHarvestGameplayTags::Resource_Wood,
		RewardItem);
	AAI_REHarvestableResourceActor* Outside = SpawnResource(
		*TestWorld,
		FVector(5050.0f, 0.0f, 0.0f),
		AI_REHarvestGameplayTags::Resource_Wood,
		RewardItem);
	AAI_REHarvestableResourceActor* FallbackResource =
		TestWorld->SpawnActor<AAI_REHarvestableResourceActor>(
			FVector(30000.0f, 0.0f, 0.0f),
			FRotator::ZeroRotator,
			SpawnParameters);
	AActor* FilterOrigin = TestWorld->SpawnActor<AActor>(
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParameters);
	if (IsValid(FilterOrigin))
	{
		USceneComponent* FilterRoot = NewObject<USceneComponent>(FilterOrigin);
		FilterOrigin->AddInstanceComponent(FilterRoot);
		FilterOrigin->SetRootComponent(FilterRoot);
		FilterRoot->RegisterComponent();
		FilterOrigin->SetActorLocation(FVector(20000.0f, 0.0f, 0.0f));
	}
	for (int32 Index = 1; Index <= 8; ++Index)
	{
		SpawnResource(
			*TestWorld,
			FVector(20000.0f + Index * 100.0f, 0.0f, 0.0f),
			AI_REWorkbenchGameplayTags::Workbench_Basic,
			RewardItem);
	}
	AAI_REHarvestableResourceActor* WoodBehindWrongTags = SpawnResource(
		*TestWorld,
		FVector(22000.0f, 0.0f, 0.0f),
		AI_REHarvestGameplayTags::Resource_Wood,
		RewardItem);
	if (!TestNotNull(TEXT("Origin exists"), Origin)
		|| !TestNotNull(TEXT("Legacy rock exists"), LegacyRock)
		|| !TestNotNull(TEXT("Wrong tag exists"), WrongTag)
		|| !TestNotNull(TEXT("Invalid reward exists"), InvalidReward)
		|| !TestNotNull(TEXT("Destroyed resource exists"), Destroyed)
		|| !TestNotNull(TEXT("Nearest wood exists"), NearestWood)
		|| !TestNotNull(TEXT("Depleted resource exists"), Depleted)
		|| !TestNotNull(TEXT("Outside resource exists"), Outside)
		|| !TestNotNull(TEXT("Fallback resource exists"), FallbackResource)
		|| !TestNotNull(TEXT("Filter origin exists"), FilterOrigin)
		|| !TestNotNull(TEXT("Wood behind wrong tags exists"), WoodBehindWrongTags))
	{
		return false;
	}
	InvalidReward->ItemActorClass = nullptr;

	TestWorld->BeginPlay();
	Destroyed->Destroy();
	Depleted->GetHarvestableResourceComponent()->ApplyHarvestDamage(
		Depleted->GetHarvestableResourceComponent()->GetMaxHealth(),
		nullptr);
	TArray<TWeakObjectPtr<AAI_REHarvestableResourceActor>> Resources;
	TestTrue(
		TEXT("Bounded query executes"),
		FAIRECompanionHarvestableResourceQuery::CollectNearbyResources(
			*Origin,
			Resources));
	TestEqual(
		TEXT("Bounded query caps resources"),
		Resources.Num(),
		FAIRECompanionHarvestableResourceQuery::MaxNearbyResources);
	TestTrue(
		TEXT("Nearest wood ignores wrong tags"),
		FAIRECompanionHarvestableResourceQuery::FindNearestCompatible(
			*Origin,
			AI_REHarvestGameplayTags::Resource_Wood) == NearestWood);
	TestTrue(
		TEXT("Rock reward repairs the legacy rock tag for queries"),
		FAIRECompanionHarvestableResourceQuery::FindNearestCompatible(
			*Origin,
			AI_REHarvestGameplayTags::Resource_Rock) == LegacyRock);
	FVector HarvestInteractionLocation = FVector::ZeroVector;
	TestTrue(
		TEXT("Harvest interaction location is available"),
		NearestWood->TryGetHarvestInteractionLocation(
			Origin->GetActorLocation(),
			HarvestInteractionLocation));
	TestTrue(
		TEXT("Harvest interaction uses the nearest blocking collision surface"),
		FMath::IsNearlyEqual(
			FVector::Dist2D(
				NearestWood->GetActorLocation(),
				HarvestInteractionLocation),
			30.0));
	FVector FallbackInteractionLocation = FVector::ZeroVector;
	TestTrue(
		TEXT("Harvest interaction falls back when collision geometry is unavailable"),
		FallbackResource->TryGetHarvestInteractionLocation(
			FallbackResource->GetActorLocation() + FVector(-100.0f, 0.0f, 0.0f),
			FallbackInteractionLocation));
	TestTrue(
		TEXT("Legacy fallback keeps the configured interaction radius"),
		FMath::IsNearlyEqual(
			FVector::Dist2D(
				FallbackResource->GetActorLocation(),
				FallbackInteractionLocation),
			FallbackResource->HarvestInteractionRadius));
	TestTrue(
		TEXT("Tag filtering happens before the eight-resource cap"),
		FAIRECompanionHarvestableResourceQuery::FindNearestCompatible(
			*FilterOrigin,
			AI_REHarvestGameplayTags::Resource_Wood) == WoodBehindWrongTags);
	TestFalse(
		TEXT("Outside center radius is excluded"),
		Resources.ContainsByPredicate(
			[Outside](const TWeakObjectPtr<AAI_REHarvestableResourceActor>& Value)
			{
				return Value.Get() == Outside;
			}));
	TestFalse(
		TEXT("Depleted resource is excluded"),
		Resources.ContainsByPredicate(
			[Depleted](const TWeakObjectPtr<AAI_REHarvestableResourceActor>& Value)
			{
				return Value.Get() == Depleted;
			}));
	TestFalse(
		TEXT("Invalid reward resource is excluded"),
		Resources.ContainsByPredicate(
			[InvalidReward](
				const TWeakObjectPtr<AAI_REHarvestableResourceActor>& Value)
			{
				return Value.Get() == InvalidReward;
			}));
	TestFalse(
		TEXT("Destroyed resource is excluded"),
		Resources.ContainsByPredicate(
			[Destroyed](const TWeakObjectPtr<AAI_REHarvestableResourceActor>& Value)
			{
				return Value.Get() == Destroyed;
			}));
	Resources.Add(NearestWood);
	TestFalse(
		TEXT("Invalid radius is rejected and clears output"),
		FAIRECompanionHarvestableResourceQuery::CollectNearbyResources(
			*Origin,
			Resources,
			0.0f));
	TestTrue(TEXT("Rejected query clears output"), Resources.IsEmpty());
	return true;
}

#endif

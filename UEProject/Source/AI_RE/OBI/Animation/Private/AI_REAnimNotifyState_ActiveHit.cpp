// Copyright MixUpProject. All Rights Reserved.

#include "AI_REAnimNotifyState_ActiveHit.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayTagContainer.h"
#include "Components/SkeletalMeshComponent.h"

void UAI_REAnimNotifyState_ActiveHit::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (MeshComp && MeshComp->GetOwner())
	{
		AActor* Owner = MeshComp->GetOwner();
		FGameplayEventData Payload;
		Payload.Instigator = Owner;
		Payload.Target = Owner;

		// ActiveHit 시작 이벤트 발송 (구형)
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
			Owner,
			FGameplayTag::RequestGameplayTag(FName("Event.Combat.ActiveHit.Start"), false),
			Payload
		);

		FGameplayTag TraceTag = FGameplayTag::RequestGameplayTag(FName("Event.Attack.TraceBegin"), false);
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, FString::Printf(TEXT("[ActiveHit] NotifyBegin - Tag: %s Valid: %d"), *TraceTag.ToString(), TraceTag.IsValid()));

		// TraceBegin 이벤트 발송 (신형 트레이스)
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
			Owner,
			TraceTag,
			Payload
		);
	}
}

void UAI_REAnimNotifyState_ActiveHit::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	if (MeshComp && MeshComp->GetOwner())
	{
		AActor* Owner = MeshComp->GetOwner();
		FGameplayEventData Payload;
		Payload.Instigator = Owner;
		Payload.Target = Owner;

		// TraceSample 이벤트 발송 (매 프레임 궤적 샘플링)
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
			Owner,
			FGameplayTag::RequestGameplayTag(FName("Event.Attack.TraceSample"), false),
			Payload
		);
	}
}

void UAI_REAnimNotifyState_ActiveHit::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (MeshComp && MeshComp->GetOwner())
	{
		AActor* Owner = MeshComp->GetOwner();
		FGameplayEventData Payload;
		Payload.Instigator = Owner;
		Payload.Target = Owner;

		// ActiveHit 종료 이벤트 발송 (구형)
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
			Owner,
			FGameplayTag::RequestGameplayTag(FName("Event.Combat.ActiveHit.End"), false),
			Payload
		);

		// TraceEnd 이벤트 발송 (신형 트레이스)
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
			Owner,
			FGameplayTag::RequestGameplayTag(FName("Event.Attack.TraceEnd"), false),
			Payload
		);
	}
}

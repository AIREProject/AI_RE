// Copyright MixUpProject. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AI_REItemDataAsset.h"
#include "AI_REWeaponItemDataAsset.generated.h"

class UAIRECompanionWeaponDefinitionDataAsset;

/**
 * Player specific Weapon Item DataAsset.
 * Inherits from base ItemDataAsset and adds weapon-specific combat definition.
 */
UCLASS(BlueprintType)
class AI_RE_API UAI_REWeaponItemDataAsset : public UAI_REItemDataAsset
{
	GENERATED_BODY()

public:
	// 무기 전투 스펙 (대미지, 몽타주 등)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UAIRECompanionWeaponDefinitionDataAsset> WeaponDefinition;

	// 무기의 실제 외형 메시 (캐릭터 손에 들려질 모델링)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	TObjectPtr<class UStaticMesh> WeaponMesh;

	// 무기 장착 시 소켓을 기준으로 한 미세 조정 오프셋 (위치/회전/크기)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	FTransform AttachmentOffset;
};

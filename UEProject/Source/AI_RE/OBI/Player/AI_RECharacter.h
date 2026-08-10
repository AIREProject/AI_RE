// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AI_RECharacterBase.h"
#include "Logging/LogMacros.h"
#include "AI_REStatusComponent.h"
#include "AI_RECraftingTypes.h"
#include "AI_REPlayerCombatComponent.h"
#include "AI_RECharacter.generated.h"

class UAI_REMainUI;
class UAI_REPlayerCombatComponent;
class UAI_REPlayerInventoryComponent;
class UAI_REPlayerCraftingComponent;
class UAIRECombatEvadeComponent;
class USpringArmComponent;
class UCameraComponent;
class UInputAction;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A simple player-controllable third person character
 *  Implements a controllable orbiting camera
 */
UCLASS(abstract)
class AAI_RECharacter : public AAI_RECharacterBase
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;
	
protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> MouseLookAction;

	/** Interact Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UInputAction> InteractAction;

	/** Attack Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> AttackAction;

public:

	/** Constructor */
	AAI_RECharacter();	

protected:

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	/** Called for interaction input */
	void DoInteract(const FInputActionValue& Value);

	/** Called for attack input */
	void DoAttack();

public:

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

public:

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
	
// ------------------------- 아래에서 작업 진행 ----------------------------
	
protected:
	
	bool bIsSprint;
	
	UFUNCTION(BlueprintCallable, Category="Input")
	void StartSprint();
	
	UFUNCTION(BlueprintCallable, Category="Input")
	void StopSprint();
	
public:
	UFUNCTION(BlueprintCallable, Category="Input")
	void ToggleInventory();

	UFUNCTION(BlueprintCallable, Category="Input|QuickMenu")
	void UseQuickSlot(int32 SlotIndex);
	
	// 체력 테스트용 디버그 콘솔 커맨드
	UFUNCTION(Exec)
	void DebugTakeDamage(float DamageAmount);
	
protected:
	// UI
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<UAI_REMainUI> MainUIClass;
	// 실제로 생성되어서 화면에 떠 있는 위젯을 조종하기 위한 리모콘
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
	TObjectPtr<UAI_REMainUI> MainUIInstance;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<class UAI_REInventoryUI> InventoryUIClass;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
	TObjectPtr<class UAI_REInventoryUI> InventoryUIInstance;
	
	// 스프린트 (달리기) IA
	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> SprintAction;
	
	// 인벤토리 (토글) IA
	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> InventoryAction;
	
	// 퀵슬롯용 IA 배열 (에디터에서 IA_QuickSlot1 ~ 0 까지 순서대로 넣습니다)
	UPROPERTY(EditAnywhere, Category="Input|QuickMenu")
	TArray<TObjectPtr<UInputAction>> QuickSlotActions;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	TSubclassOf<class UAI_RECraftingUI> CraftingUIClass;

	UPROPERTY()
	TObjectPtr<class UAI_RECraftingUI> CraftingUIInstance;

	// Component
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class UAI_REPlayerInventoryComponent> InventoryComponent;

	// 크래프팅 매니저
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player Components")
	TObjectPtr<UAI_REPlayerCraftingComponent> CraftingComponent;

	// 전투/액션 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player Components") 
	TObjectPtr<UAI_REPlayerCombatComponent> CombatComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player Components")
	TObjectPtr<UAIRECombatEvadeComponent> CombatEvadeComponent;

	// 통합 타겟 스캐너 (분리됨)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player Components")
	TObjectPtr<class UAI_RETargetScannerComponent> TargetScannerComponent;

	// 무기 장착용 슬롯 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class UStaticMeshComponent> WeaponMeshComponent;

	virtual void BeginPlay() override;
	
public:
	// Crafting
	UFUNCTION(BlueprintCallable, Category = "Crafting")
	void OpenCraftingUI(EWorkbenchType WorkbenchType);

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	void CloseCraftingUI();

	
	// FOCEINLINE -> Function Call 방식이 아니라 사용 위치에서 코드를 받아 붙여넣어(inline) 실행
	FORCEINLINE TObjectPtr<UAI_REPlayerInventoryComponent> GetInventoryComponent() const { return InventoryComponent; }
	FORCEINLINE TObjectPtr<UAI_REPlayerCombatComponent> GetCombatComponent() const { return CombatComponent; }
	FORCEINLINE UAIRECombatEvadeComponent* GetCombatEvadeComponent() const { return CombatEvadeComponent; }
	FORCEINLINE class UAI_RETargetScannerComponent* GetTargetScannerComponent() const { return TargetScannerComponent; }
	FORCEINLINE class UStaticMeshComponent* GetWeaponMeshComponent() const { return WeaponMeshComponent; }
	
};


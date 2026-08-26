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

	/** Toggles the interaction target outline. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UInputAction> InteractionOutlineToggleAction;

	/** Craft Input Action (Bound to C) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UInputAction> CraftAction;

	/** Equip Input Action (Bound to E) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UInputAction> EquipAction;

	/** Attack Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> AttackAction;

	/** Toggle Scanner Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> ToggleScannerAction;

	/** Assign IA_AIREPlayerEvade and map it to Left Ctrl in the active Player IMC. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> EvadeAction;

public:

	/** Constructor */
	AAI_RECharacter();	

	virtual void Tick(float DeltaTime) override;

protected:

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);
	void StopMove(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	/** Called for interaction input */
	void DoInteract(const FInputActionValue& Value);

	/** Called for interaction outline toggle input. */
	void ToggleInteractionOutline(const FInputActionValue& Value);

	/** Called for craft input */
	void DoCraft(const FInputActionValue& Value);

	/** Called for equip input */
	void DoEquip(const FInputActionValue& Value);

	/** Called for attack input */
	void DoAttack();

	/** Toggles the combat target scanner */
	void ToggleScanner();

	/** Starts a dash in the movement direction captured when the input begins. */
	void DoEvade();

	FVector2D CurrentMovementInput = FVector2D::ZeroVector;

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
	
	/** Gets whether the player is currently in combat (locked on). */
	UFUNCTION(BlueprintPure, Category="AIRE|Combat")
	bool GetIsCombatState() const { return bIsCombatState; }

protected:
	/** TargetScanner에서 전달되는 전투 상태 변경 이벤트를 처리합니다. */
	UFUNCTION()
	void HandleCombatStateChanged(bool bIsCombat, AActor* Target);
	
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
	
	// 사망 (HP가 0이 되었을 때 내부적으로 호출)
	UFUNCTION(BlueprintCallable, Category="Combat")
	virtual void Die();

	// 사망 시 블루프린트에서 추가 연출을 넣을 수 있도록 이벤트 제공
	UFUNCTION(BlueprintImplementableEvent, Category="Combat")
	void OnPlayerDied();
	
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat")
	bool bIsDead = false;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 체력 변경 이벤트 콜백
	void HandleHealthChanged(const struct FOnAttributeChangeData& ChangeData);

	FDelegateHandle HealthChangedDelegateHandle;
	
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

	// 사망 시 재생할 애니메이션 몽타주
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TObjectPtr<class UAnimMontage> DeathMontage;

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

	// 전투 상태 및 카메라 타겟
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Combat")
	bool bIsCombatState = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AIRE|Combat")
	TWeakObjectPtr<AActor> CurrentCombatTarget;

	// 무기 장착용 슬롯 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class UStaticMeshComponent> WeaponMeshComponent;
	
public:
	// Crafting
	UFUNCTION(BlueprintCallable, Category = "Crafting")
	void OpenCraftingUI(EWorkbenchType WorkbenchType);

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	void CloseCraftingUI();

	UFUNCTION(BlueprintPure, Category = "UI")
	bool IsInventoryUIOpen() const;

	UFUNCTION(BlueprintPure, Category = "UI")
	bool IsCraftingUIOpen() const;

	
	// FOCEINLINE -> Function Call 방식이 아니라 사용 위치에서 코드를 받아 붙여넣어(inline) 실행
	FORCEINLINE TObjectPtr<UAI_REPlayerInventoryComponent> GetInventoryComponent() const { return InventoryComponent; }
	FORCEINLINE TObjectPtr<UAI_REPlayerCombatComponent> GetCombatComponent() const { return CombatComponent; }
	FORCEINLINE UAIRECombatEvadeComponent* GetCombatEvadeComponent() const { return CombatEvadeComponent; }
	FORCEINLINE class UAI_RETargetScannerComponent* GetTargetScannerComponent() const { return TargetScannerComponent; }
	FORCEINLINE class UStaticMeshComponent* GetWeaponMeshComponent() const { return WeaponMeshComponent; }
	
};


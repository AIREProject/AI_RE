// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "AI_RECharacter.generated.h"

class UAI_REMainUI;
class UAI_REPlayerCombatComponent;
class UAI_REPlayerInventoryComponent;
class UAI_REStatusComponent;
class UAI_RESkillComponent;
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
class AAI_RECharacter : public ACharacter
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
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

	/** Interact Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputAction* InteractAction;

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
	
private:
	
	bool bIsSprint;
	
	UFUNCTION(BlueprintCallable, Category="Input")
	void StartSprint();
	
	UFUNCTION(BlueprintCallable, Category="Input")
	void StopSprint();
	
	UFUNCTION(BlueprintCallable, Category="Input")
	void ToggleInventory();
	
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
	
	
	// 상태 컴포넌트 (체력, 스태미나, 배고픔 등 통합)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player Components")
	TObjectPtr<UAI_REStatusComponent> StatusComponent;
	
	// 스킬 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player Components")
	TObjectPtr<UAI_RESkillComponent> SkillComponent;
	// 인벤토리 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player Components")
	TObjectPtr<UAI_REPlayerInventoryComponent> InventoryComponent;
	// 전투/액션 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player Components") 
	TObjectPtr<UAI_REPlayerCombatComponent> CombatComponent;

public:
	
	virtual void BeginPlay() override;
	
	// FOCEINLINE -> Function Call 방식이 아니라 사용 위치에서 코드를 받아 붙여넣어(inline) 실행
	FORCEINLINE TObjectPtr<UAI_REStatusComponent> GetStatusComponent() const { return StatusComponent; }
	FORCEINLINE TObjectPtr<UAI_RESkillComponent> GetSkillComponent() const { return SkillComponent; }
	FORCEINLINE TObjectPtr<UAI_REPlayerInventoryComponent> GetInventoryComponent() const { return InventoryComponent; }
	FORCEINLINE TObjectPtr<UAI_REPlayerCombatComponent> GetCombatComponent() const { return CombatComponent; }
	
};


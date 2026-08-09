// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI_RECharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "AI_RE.h"
#include "AI_REMainUI.h"
#include "AI_REInventoryUI.h"
#include "AI_REStatusComponent.h"
#include "AI_REPlayerCombatComponent.h"
#include "AI_REPlayerInventoryComponent.h"
#include "AI_REPlayerCombatComponent.h"
#include "AbilitySystemComponent.h"
#include "AI_REAttributeSet.h"
#include "AI_REPlayerCraftingComponent.h"
#include "Blueprint/UserWidget.h"
#include "AI_RECraftingUI.h"
#include "Engine/Engine.h"
#include "AI_REInteractableInterface.h"
#include "AIRECombatEvadeComponent.h"
#include "Engine/OverlapResult.h"
#include "../Component/Public/AI_RETargetScannerComponent.h"

AAI_RECharacter::AAI_RECharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// Component Define
	InventoryComponent = CreateDefaultSubobject<UAI_REPlayerInventoryComponent>(TEXT("InventoryComponent"));
	CraftingComponent = CreateDefaultSubobject<UAI_REPlayerCraftingComponent>(TEXT("CraftingComponent"));
	CombatComponent = CreateDefaultSubobject<UAI_REPlayerCombatComponent>(TEXT("CombatComponent"));
	CombatEvadeComponent = CreateDefaultSubobject<UAIRECombatEvadeComponent>(TEXT("CombatEvade"));
	TargetScannerComponent = CreateDefaultSubobject<UAI_RETargetScannerComponent>(TEXT("TargetScannerComponent"));
	
	// 무기 장착용 메시 컴포넌트 생성 및 소켓에 부착
	WeaponMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMeshComponent"));
	WeaponMeshComponent->SetupAttachment(GetMesh(), FName("WeaponSocket_R"));
	WeaponMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 무기 자체의 물리 충돌은 끕니다.
	
	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void AAI_RECharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AAI_RECharacter::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AAI_RECharacter::Look);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AAI_RECharacter::Look);
		
		// Sprint 
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &AAI_RECharacter::StartSprint);
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &AAI_RECharacter::StopSprint);

		// Inventory
		if (InventoryAction)
		{
			EnhancedInputComponent->BindAction(InventoryAction, ETriggerEvent::Started, this, &AAI_RECharacter::ToggleInventory);
		}

		if (InteractAction)
		{
			EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &AAI_RECharacter::DoInteract);
		}

		if (AttackAction)
		{
			EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Started, this, &AAI_RECharacter::DoAttack);
		}

		// QuickSlots (1~0 키 바인딩)
		for (int32 i = 0; i < QuickSlotActions.Num(); ++i)
		{
			if (QuickSlotActions[i])
			{
				EnhancedInputComponent->BindAction(QuickSlotActions[i], ETriggerEvent::Started, this, &AAI_RECharacter::UseQuickSlot, i);
			}
		}
	}
	else
	{
		UE_LOG(LogAI_RE, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void AAI_RECharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	// route the input
	DoMove(MovementVector.X, MovementVector.Y);
}

void AAI_RECharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// route the input
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void AAI_RECharacter::DoMove(float Right, float Forward)
{
	if (GetController() != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void AAI_RECharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AAI_RECharacter::DoJumpStart()
{
	// signal the character to jump
	Jump();
}

void AAI_RECharacter::DoJumpEnd()
{
	// signal the character to stop jumping
	StopJumping();
}

// ------------------------------ 아래에서 작업 진행 (새로 추가된 함수) ------------------------------

void AAI_RECharacter::StartSprint()
{
	// 스태미나가 충분할 때만 달리기 허용
	if (AbilitySystemComponent && AbilitySystemComponent->GetNumericAttribute(UAI_REAttributeSet::GetSPAttribute()) >= 10.f)
	{
		GetCharacterMovement()->MaxWalkSpeed = 1000.f;
		bIsSprint = true;
	}
}

void AAI_RECharacter::StopSprint()
{
	GetCharacterMovement() -> MaxWalkSpeed = 500.f;
	bIsSprint = false;
}

void AAI_RECharacter::DebugTakeDamage(float DamageAmount)
{
	if (AbilitySystemComponent)
	{
		// GAS를 통해 다이렉트로 HP를 차감합니다 (서버 권한 등 무시하고 즉각 적용)
		AbilitySystemComponent->ApplyModToAttributeUnsafe(UAI_REAttributeSet::GetHPAttribute(), EGameplayModOp::Additive, -DamageAmount);
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, FString::Printf(TEXT("Debug: Took %f damage via GAS!"), DamageAmount));
	}
}

void AAI_RECharacter::UseQuickSlot(int32 SlotIndex)
{
	if (InventoryComponent)
	{
		// Quick slots are typically indexed starting from 100 in our component
		// Assuming SlotIndex passed from BP is 0, 1, 2, 3...
		// We map them to 100, 101, 102, 103...
		int32 MappedSlotIndex = SlotIndex >= 100 ? SlotIndex : 100 + SlotIndex;
		bool bSuccess = InventoryComponent->UseItem(MappedSlotIndex);
		
		if (!bSuccess)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, FString::Printf(TEXT("Quick Slot %d is empty or cannot be used!"), MappedSlotIndex));
		}
	}
}

void AAI_RECharacter::ToggleInventory()
{
	if (!InventoryUIInstance && InventoryUIClass)
	{
		InventoryUIInstance = CreateWidget<UAI_REInventoryUI>(GetWorld(), InventoryUIClass);
		if (InventoryUIInstance)
		{
			InventoryUIInstance->InitializeInventory(InventoryComponent);
		}
	}

	if (InventoryUIInstance)
	{
		APlayerController* PC = Cast<APlayerController>(GetController());
		
		if (InventoryUIInstance->IsInViewport())
		{
			InventoryUIInstance->RemoveFromParent();

			if (PC)
			{
				FInputModeGameOnly InputMode;
				PC->SetInputMode(InputMode);
				PC->SetShowMouseCursor(false);
			}
		}
		else
		{
			InventoryUIInstance->AddToViewport(10);

			if (PC)
			{
				FInputModeGameAndUI InputMode;
				InputMode.SetWidgetToFocus(InventoryUIInstance->TakeWidget());
				PC->SetInputMode(InputMode);
				PC->SetShowMouseCursor(true);
			}
		}
	}
}

void AAI_RECharacter::OpenCraftingUI(EWorkbenchType WorkbenchType)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	
	// 1. 크래프팅 위젯 생성
	if (!CraftingUIInstance && CraftingUIClass)
	{
		CraftingUIInstance = CreateWidget<UAI_RECraftingUI>(GetWorld(), CraftingUIClass);
	}

	if (CraftingUIInstance)
	{
		if (!CraftingUIInstance->IsInViewport())
		{
			// 크래프팅 UI 띄우기
			CraftingUIInstance->AddToViewport();
			
			CraftingUIInstance->InitializeCrafting(CraftingComponent, WorkbenchType);

			if (PC)
			{
				FInputModeGameAndUI InputMode;
				InputMode.SetWidgetToFocus(CraftingUIInstance->TakeWidget());
				PC->SetInputMode(InputMode);
				PC->SetShowMouseCursor(true);
			}
		}
		else
		{
			CloseCraftingUI();
		}
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Crafting UI Class is missing! Set it in BP_ThirdPersonCharacter."));
	}
}

void AAI_RECharacter::CloseCraftingUI()
{
	if (CraftingUIInstance && CraftingUIInstance->IsInViewport())
	{
		CraftingUIInstance->RemoveFromParent();
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			FInputModeGameOnly InputMode;
			PC->SetInputMode(InputMode);
			PC->SetShowMouseCursor(false);
		}
	}
}

void AAI_RECharacter::BeginPlay()
{
	Super::BeginPlay();

	// 가장 확실하게 인벤토리와 크래프팅 컴포넌트를 연결!
	if (CraftingComponent && InventoryComponent)
	{
		CraftingComponent->SetInventoryComponent(InventoryComponent);
	}
	
	if (MainUIClass != nullptr)
	{
		MainUIInstance = CreateWidget<UAI_REMainUI>(GetWorld(), MainUIClass);
		if (MainUIInstance)
		{
			MainUIInstance->AddToViewport();
			MainUIInstance->InitializeHUD(StatusComponent);
		}
	}
}

void AAI_RECharacter::DoInteract(const FInputActionValue& Value)
{
	if (CraftingUIInstance && CraftingUIInstance->IsInViewport())
	{
		CloseCraftingUI();
		return;
	}

	// TargetScannerComponent에서 캐싱된 상호작용 대상을 가져와 즉시 상호작용
	if (TargetScannerComponent)
	{
		if (AActor* Target = TargetScannerComponent->GetCachedInteractableTarget())
		{
			IAI_REInteractableInterface::Execute_Interact(Target, this);
			return;
		}
	}

	// 반경 내에 상호작용할 물건이 아무것도 없다면 맨손 제작(None) 메뉴를 엽니다!
	OpenCraftingUI(EWorkbenchType::None);
}

void AAI_RECharacter::DoAttack()
{
	if (CombatComponent)
	{
		CombatComponent->TryStartPrimaryAction();
	}
}

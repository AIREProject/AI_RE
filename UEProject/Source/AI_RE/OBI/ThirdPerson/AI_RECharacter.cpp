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
#include "AI_REPlayerCombatComponent.h"
#include "AI_REPlayerInventoryComponent.h"
#include "AI_REStatusComponent.h"
#include "AI_RESkillComponent.h"
#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "../../Global/Interfaces/Public/AI_REInteractableInterface.h"
#include "Engine/OverlapResult.h"

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
	
	StatusComponent = CreateDefaultSubobject<UAI_REStatusComponent>(TEXT("StatusComponent"));
	SkillComponent = CreateDefaultSubobject<UAI_RESkillComponent>(TEXT("SkillComponent"));
	InventoryComponent = CreateDefaultSubobject<UAI_REPlayerInventoryComponent>(TEXT("PlayerInventoryComponent"));
	CombatComponent = CreateDefaultSubobject<UAI_REPlayerCombatComponent>(TEXT("CombatComponent"));
	
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
	if (StatusComponent->CurrentSP > 10.f)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, TEXT("On Start Sprint"));
		GetCharacterMovement() -> MaxWalkSpeed = 1000.f;
		bIsSprint = true;
	}
}

void AAI_RECharacter::StopSprint()
{
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, TEXT("On Stop Sprint"));
	GetCharacterMovement() -> MaxWalkSpeed = 500.f;
	bIsSprint = false;
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

void AAI_RECharacter::BeginPlay()
{
	Super::BeginPlay();
	
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
	FVector Start = GetActorLocation();
	FVector End = Start + (GetActorForwardVector() * 200.0f); // 2미터 앞

	// 단순 충돌 체크를 위한 오버랩 감지 (실제 게임에선 SphereTrace 등을 씁니다)
	TArray<FOverlapResult> OverlapResults;
	FCollisionShape Sphere = FCollisionShape::MakeSphere(200.0f);
	
	bool bHit = GetWorld()->OverlapMultiByChannel(
		OverlapResults, 
		Start, 
		FQuat::Identity, 
		ECC_Visibility, 
		Sphere
	);

	if (bHit)
	{
		for (const FOverlapResult& Result : OverlapResults)
		{
			AActor* HitActor = Result.GetActor();
			if (HitActor && HitActor->Implements<UAI_REInteractableInterface>())
			{
				IAI_REInteractableInterface::Execute_Interact(HitActor, this);
				break; // 한 번에 하나만 상호작용
			}
		}
	}
}

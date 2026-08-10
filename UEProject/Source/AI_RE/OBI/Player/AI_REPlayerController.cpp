// Copyright Epic Games, Inc. All Rights Reserved.


#include "AI_REPlayerController.h"
#include "AIREAggroSwapComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "AI_RE.h"
#include "Widgets/Input/SVirtualJoystick.h"

AAI_REPlayerController::AAI_REPlayerController()
{
	AggroSwapComponent =
		CreateDefaultSubobject<UAIREAggroSwapComponent>(TEXT("AggroSwap"));
}

UAIREAggroSwapComponent*
AAI_REPlayerController::GetAggroSwapComponent() const
{
	return AggroSwapComponent;
}

void AAI_REPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// only spawn touch controls on local player controllers
	if (IsLocalPlayerController() && ShouldUseTouchControls())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogAI_RE, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}
}

void AAI_REPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (IsValid(AggroSwapAction))
	{
		if (UEnhancedInputComponent* EnhancedInput =
			Cast<UEnhancedInputComponent>(InputComponent))
		{
			EnhancedInput->BindAction(
				AggroSwapAction,
				ETriggerEvent::Started,
				this,
				&AAI_REPlayerController::HandleAggroSwapInput);
		}
	}

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Contexts
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
}

bool AAI_REPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

void AAI_REPlayerController::HandleAggroSwapInput()
{
	if (IsValid(AggroSwapComponent))
	{
		AggroSwapComponent->TryAggroSwap();
	}
}

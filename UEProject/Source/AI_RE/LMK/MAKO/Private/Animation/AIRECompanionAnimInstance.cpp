#include "Animation/AIRECompanionAnimInstance.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"

namespace
{
	constexpr float MinimumMovementSpeed = 3.0f;
}

void UAIRECompanionAnimInstance::NativeUpdateAnimation(const float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	APawn* Pawn = TryGetPawnOwner();
	if (!IsValid(Pawn))
	{
		GroundSpeed = 0.0f;
		MovementDirection = 0.0f;
		bShouldMove = false;
		bIsFalling = false;
		return;
	}

	const FVector Velocity = Pawn->GetVelocity();
	GroundSpeed = Velocity.Size2D();
	bShouldMove = GroundSpeed > MinimumMovementSpeed;

	if (bShouldMove)
	{
		const FVector LocalVelocity =
			Pawn->GetActorTransform().InverseTransformVectorNoScale(Velocity);
		MovementDirection = FMath::RadiansToDegrees(
			FMath::Atan2(LocalVelocity.Y, LocalVelocity.X));
	}
	else
	{
		MovementDirection = 0.0f;
	}

	const ACharacter* Character = Cast<ACharacter>(Pawn);
	const UCharacterMovementComponent* MovementComponent =
		IsValid(Character) ? Character->GetCharacterMovement() : nullptr;
	bIsFalling = IsValid(MovementComponent) && MovementComponent->IsFalling();
}

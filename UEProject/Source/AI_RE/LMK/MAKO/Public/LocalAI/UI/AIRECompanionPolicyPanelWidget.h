#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LocalAI/Policy/AIRECompanionLocalBehaviorPolicy.h"
#include "AIRECompanionPolicyPanelWidget.generated.h"

class AAIRECompanionCharacter;
class AActor;
class UAIRECompanionLocalBehaviorPolicyComponent;
class UButton;
class UTextBlock;
class UWidget;

enum class EAIRECompanionPolicyWheelSelection : uint8
{
	None,
	Balanced,
	SupportPriority,
	HoldFire,
	DefendPlayer,
	Aggressive
};

UCLASS(Abstract, Blueprintable)
class AI_RE_API UAIRECompanionPolicyPanelWidget
	: public UUserWidget
{
	GENERATED_BODY()

public:
	void BeginPolicySelection();
	bool CommitPolicySelection();
	void SetPanelOpen(bool bOpen);
	bool IsPanelOpen() const;
	void RefreshCompanionBinding();
	void BindCompanion(AAIRECompanionCharacter* Companion);
	void UnbindCompanion();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(
		const FGeometry& MyGeometry,
		float InDeltaTime) override;
	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

private:
	UFUNCTION()
	void HandleBalancedClicked();

	UFUNCTION()
	void HandleHoldFireClicked();

	UFUNCTION()
	void HandleDefendPlayerClicked();

	UFUNCTION()
	void HandleAggressiveClicked();

	UFUNCTION()
	void HandleSupportPriorityClicked();

	UFUNCTION()
	void HandlePolicyChanged(
		FAIRECompanionLocalBehaviorPolicy PreviousPolicy,
		FAIRECompanionLocalBehaviorPolicy CurrentPolicy);

	UFUNCTION()
	void HandleCompanionDestroyed(AActor* DestroyedActor);

	bool ApplySelection(EAIRECompanionPolicyWheelSelection Selection);
	void UpdateWheelSelection(const FGeometry& Geometry);
	FAIRECompanionLocalBehaviorPolicy GetPreviewPolicy(
		const FAIRECompanionLocalBehaviorPolicy& CurrentPolicy) const;
	void RefreshPolicyDisplay();
	void SetPolicyButtonsEnabled(bool bEnabled);

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> PolicyPanel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> CollapsedHint;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> CurrentPolicyText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BalancedButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> HoldFireButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> DefendPlayerButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> AggressiveButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> SupportPriorityButton;

	TWeakObjectPtr<AAIRECompanionCharacter> BoundCompanion;
	TWeakObjectPtr<UAIRECompanionLocalBehaviorPolicyComponent>
		BoundPolicyComponent;
	EAIRECompanionPolicyWheelSelection WheelSelection =
		EAIRECompanionPolicyWheelSelection::None;
	bool bPanelOpen = false;
};

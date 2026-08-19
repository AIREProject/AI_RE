#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AIRECompanionStatusWidget.generated.h"

class AAIRECompanionCharacter;
class AActor;
class UAbilitySystemComponent;
class UImage;
class UProgressBar;
class UTextBlock;
class UWidget;
struct FOnAttributeChangeData;

UCLASS(Abstract, Blueprintable)
class AI_RE_API UAIRECompanionStatusWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Status")
	void BindCompanion(AAIRECompanionCharacter* InCompanion);

	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Status")
	void UnbindCompanion();

	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Status")
	void RefreshStatus();

	UFUNCTION(BlueprintCallable, Category = "AIRE|Companion|Status")
	void RefreshDistance();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> StatusCardRoot;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> PortraitImage;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> CompanionNameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DistanceText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthProgressBar;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> StaminaProgressBar;

private:
	void HandleAttributeChanged(const FOnAttributeChangeData& ChangeData);

	UFUNCTION()
	void HandleCompanionDestroyed(AActor* DestroyedActor);

	void UnbindAttributeDelegates();
	void StartDistanceRefreshTimer();
	void StopDistanceRefreshTimer();
	void SetStatusCardVisibility(ESlateVisibility InVisibility);

	TWeakObjectPtr<AAIRECompanionCharacter> BoundCompanion;
	TWeakObjectPtr<UAbilitySystemComponent> BoundAbilitySystemComponent;

	FDelegateHandle HealthChangedDelegateHandle;
	FDelegateHandle MaxHealthChangedDelegateHandle;
	FDelegateHandle StaminaChangedDelegateHandle;
	FDelegateHandle MaxStaminaChangedDelegateHandle;
	FTimerHandle DistanceRefreshTimerHandle;
};

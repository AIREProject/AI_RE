#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AIREBossHUDWidget.generated.h"

class AAIREBossEnemy;
class AActor;
class UProgressBar;
class UTextBlock;
class UWidget;

UCLASS(Abstract, Blueprintable)
class AI_RE_API UAIREBossHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "AIRE|Enemy|Boss HUD")
	void BindBoss(AAIREBossEnemy* InBoss);

	UFUNCTION(BlueprintCallable, Category = "AIRE|Enemy|Boss HUD")
	void UnbindBoss();

	UFUNCTION(BlueprintCallable, Category = "AIRE|Enemy|Boss HUD")
	void RefreshHUD();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> BossHUDRoot;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> BossNameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthProgressBar;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> GroggyProgressBar;

private:
	UFUNCTION()
	void HandleHealthChanged(float OldHealth, float NewHealth);

	UFUNCTION()
	void HandleGroggyChanged(float CurrentGroggy, float MaxGroggy);

	UFUNCTION()
	void HandleBossDestroyed(AActor* DestroyedActor);

	void SetHUDVisibility(ESlateVisibility InVisibility);

	TWeakObjectPtr<AAIREBossEnemy> BoundBoss;
};

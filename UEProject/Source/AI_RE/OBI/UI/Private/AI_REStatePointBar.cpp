// Fill out your copyright notice in the Description page of Project Settings.


#include "AI_REStatePointBar.h"
#include "TimerManager.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"

void UAI_REStatePointBar::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bIsSmoothing)
	{
		CurrentPercent = FMath::FInterpTo(CurrentPercent, TargetPercent, InDeltaTime, 5.0f);
		
		if (StateBar && StateBar->GetDynamicMaterial())
		{
			StateBar->GetDynamicMaterial()->SetScalarParameterValue(FName("Percent"), CurrentPercent);
		}
		
		if (FMath::IsNearlyEqual(CurrentPercent, TargetPercent, 0.001f)) 
		{ 
			CurrentPercent = TargetPercent; 
			
			if (StateBar && StateBar->GetDynamicMaterial())
			{
				StateBar->GetDynamicMaterial()->SetScalarParameterValue(FName("Percent"), CurrentPercent);
			}
			
			bIsSmoothing = false;
		}
	}
}

void UAI_REStatePointBar::NativePreConstruct()
{
	Super::NativePreConstruct();
	
	// 1. 에디터에서 사용자가 커스텀 머티리얼을 슬롯에 넣었다면?
	if (StateBar && CustomBarMaterial)
	{
		// 2. 해당 머티리얼을 이미지 브러시로 덮어씌웁니다.
		StateBar->SetBrushFromMaterial(CustomBarMaterial);
	}
}

void UAI_REStatePointBar::SetPercentInstantly(float NewPercent)
{
	TargetPercent = NewPercent;
	CurrentPercent = NewPercent;
	bIsSmoothing = false;
	
	if (StateBar)
	{
		// 시작 시점에 확실하게 동적 머티리얼을 강제로 생성/가져와서 업데이트
		if (UMaterialInstanceDynamic* DynMat = StateBar->GetDynamicMaterial())
		{
			DynMat->SetScalarParameterValue(FName("Percent"), CurrentPercent);
		}
	}
}

void UAI_REStatePointBar::SetTargetPercent(float NewPercent)
{
	TargetPercent = NewPercent;
	bIsSmoothing = true;
}

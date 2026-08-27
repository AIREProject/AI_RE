#include "AbilitySystem/Combat/AIRECompanionCombatVFX.h"

#include "AIREBossEnemy.h"
#include "Equipment/AIRECompanionWeaponDefinitionDataAsset.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

namespace
{
FVector ResolveImpactNormal(
	const FHitResult& HitResult,
	const AActor* SourceActor,
	const AActor* TargetActor)
{
	FVector ImpactNormal = HitResult.ImpactNormal.GetSafeNormal();
	if (!ImpactNormal.IsNearlyZero())
	{
		return ImpactNormal;
	}

	ImpactNormal = HitResult.Normal.GetSafeNormal();
	if (!ImpactNormal.IsNearlyZero())
	{
		return ImpactNormal;
	}

	const FVector SourceFacingNormal =
		IsValid(SourceActor) && IsValid(TargetActor)
			? (SourceActor->GetActorLocation()
				- TargetActor->GetActorLocation()).GetSafeNormal()
			: FVector::ZeroVector;
	return SourceFacingNormal.IsNearlyZero()
		? FVector::UpVector
		: SourceFacingNormal;
}

FRotator ResolveSlashRotation(
	const FHitResult& HitResult,
	const FVector& ImpactNormal,
	const AActor* SourceActor,
	const AActor* TargetActor)
{
	// 실제 애니메이션 트레이스(무기가 이동한 궤적) 방향을 우선 사용
	FVector StrikeDirection = (HitResult.TraceEnd - HitResult.TraceStart).GetSafeNormal();
	
	// 트레이스 이동이 거의 없는 경우(Fallback 등) 기존 방식(적을 향하는 방향) 사용
	if (StrikeDirection.IsNearlyZero())
	{
		StrikeDirection = IsValid(SourceActor) && IsValid(TargetActor)
			? (TargetActor->GetActorLocation() - SourceActor->GetActorLocation())
				.GetSafeNormal()
			: FVector::ForwardVector;
	}
	StrikeDirection = FVector::VectorPlaneProject(
		StrikeDirection,
		ImpactNormal).GetSafeNormal();
	if (StrikeDirection.IsNearlyZero())
	{
		StrikeDirection = FVector::CrossProduct(
			ImpactNormal,
			FVector::UpVector).GetSafeNormal();
	}
	if (StrikeDirection.IsNearlyZero())
	{
		StrikeDirection = FVector::ForwardVector;
	}

	return FRotationMatrix::MakeFromXZ(
		StrikeDirection,
		ImpactNormal).Rotator();
}
}

void AIRECompanionCombatVFX::SpawnBossHitSlash(
	const UAIRECompanionWeaponDefinitionDataAsset* WeaponDefinition,
	const AActor* SourceActor,
	const AActor* TargetActor,
	const FHitResult& HitResult)
{
	const AAIREBossEnemy* Boss = Cast<AAIREBossEnemy>(TargetActor);
	UNiagaraSystem* SlashEffect = IsValid(WeaponDefinition)
		? WeaponDefinition->BossHitSlashEffect.LoadSynchronous()
		: nullptr;
	UWorld* World = IsValid(SourceActor) ? SourceActor->GetWorld() : nullptr;
	if (!IsValid(Boss) || !IsValid(SlashEffect) || !IsValid(World))
	{
		return;
	}

	const FVector ImpactNormal = ResolveImpactNormal(
		HitResult,
		SourceActor,
		TargetActor);
	const float SurfaceOffset = FMath::IsFinite(
		WeaponDefinition->BossHitSlashSurfaceOffset)
		? WeaponDefinition->BossHitSlashSurfaceOffset
		: 0.0f;
	const FVector EffectLocation = HitResult.ImpactPoint
		+ ImpactNormal * SurfaceOffset;
	const FRotator RotationOffset =
		WeaponDefinition->BossHitSlashRotationOffset.ContainsNaN()
			? FRotator::ZeroRotator
			: WeaponDefinition->BossHitSlashRotationOffset;
	const FQuat EffectRotation = ResolveSlashRotation(
			HitResult,
			ImpactNormal,
			SourceActor,
			TargetActor).Quaternion()
		* RotationOffset.Quaternion();
	const FVector EffectScale =
		WeaponDefinition->BossHitSlashScale.ContainsNaN()
			? FVector::OneVector
			: WeaponDefinition->BossHitSlashScale;

	UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		World,
		SlashEffect,
		EffectLocation,
		EffectRotation.Rotator(),
		EffectScale,
		true,
		true,
		ENCPoolMethod::AutoRelease,
		true);
}

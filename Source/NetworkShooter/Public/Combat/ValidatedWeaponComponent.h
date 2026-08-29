// Copyright Kang Hyungsoon. Portfolio source code.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ValidatedWeaponComponent.generated.h"

class UDamageType;

/**
 * 클라이언트가 보낸 발사 정보를 서버에서 검증한 뒤 피해를 적용합니다.
 * 피해량은 클라이언트 요청에 포함하지 않고 서버 설정값만 사용합니다.
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class NETWORKSHOOTER_API UValidatedWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UValidatedWeaponComponent();

	/** 로컬에서 예측한 히트스캔 결과를 서버에 보고합니다. */
	UFUNCTION(BlueprintCallable, Category="Combat|Validation")
	void ReportHitscan(
		AActor* Target,
		FVector TraceStart,
		FVector TraceEnd,
		double EstimatedServerHitTimeSeconds
	);

	/** 로컬에서 예측한 투사체 결과를 서버에 보고합니다. */
	UFUNCTION(BlueprintCallable, Category="Combat|Validation")
	void ReportProjectile(
		AActor* Target,
		FVector TraceStart,
		FVector InitialVelocity,
		double EstimatedServerHitTimeSeconds
	);

	/** 한 대상에게 향한 산탄 펠릿 끝점을 서버에 보고합니다. */
	UFUNCTION(BlueprintCallable, Category="Combat|Validation")
	void ReportShotgun(
		AActor* Target,
		FVector TraceStart,
		const TArray<FVector>& PelletTraceEnds,
		double EstimatedServerHitTimeSeconds
	);

private:
	UFUNCTION(Server, Reliable)
	void ServerReportHitscan(
		AActor* Target,
		FVector_NetQuantize TraceStart,
		FVector_NetQuantize TraceEnd,
		double HitServerTimeSeconds
	);

	UFUNCTION(Server, Reliable)
	void ServerReportProjectile(
		AActor* Target,
		FVector_NetQuantize TraceStart,
		FVector_NetQuantize100 InitialVelocity,
		double HitServerTimeSeconds
	);

	UFUNCTION(Server, Reliable)
	void ServerReportShotgun(
		AActor* Target,
		FVector_NetQuantize TraceStart,
		const TArray<FVector_NetQuantize>& PelletTraceEnds,
		double HitServerTimeSeconds
	);

	bool IsRequestPlausible(
		const AActor* Target,
		const FVector& TraceStart,
		double HitServerTimeSeconds
	) const;

	void ApplyValidatedDamage(AActor* Target, float DamageToApply) const;

	UPROPERTY(EditDefaultsOnly, Category="Combat|Damage", meta=(ClampMin="0.0"))
	float BodyDamage = 20.0f;

	UPROPERTY(EditDefaultsOnly, Category="Combat|Damage", meta=(ClampMin="0.0"))
	float CriticalDamage = 40.0f;

	UPROPERTY(EditDefaultsOnly, Category="Combat|Validation", meta=(ClampMin="0.25", ClampMax="3.0"))
	float MaximumRequestAgeSeconds = 1.25f;

	UPROPERTY(EditDefaultsOnly, Category="Combat|Validation", meta=(ClampMin="0.0", ClampMax="0.25"))
	float FutureTimeToleranceSeconds = 0.05f;

	UPROPERTY(EditDefaultsOnly, Category="Combat|Validation", meta=(ClampMin="100.0"))
	float MaximumTraceDistance = 20000.0f;

	UPROPERTY(EditDefaultsOnly, Category="Combat|Validation", meta=(ClampMin="100.0"))
	float MaximumProjectileSpeed = 30000.0f;

	UPROPERTY(EditDefaultsOnly, Category="Combat|Validation", meta=(ClampMin="1", ClampMax="32"))
	int32 MaximumPelletCount = 16;

	UPROPERTY(EditDefaultsOnly, Category="Combat|Damage")
	TSubclassOf<UDamageType> DamageTypeClass;
};

// Copyright Kang Hyungsoon. Portfolio source code.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ValidatedWeaponComponent.generated.h"

class UDamageType;

/** 같은 발사 시각으로 들어온 보고가 같은 무기에서 나왔는지 구분합니다. */
enum class EReportedShotKind : uint8
{
	Hitscan,
	Projectile,
	Shotgun
};

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
	/** 발사 간격 검사는 자동화 테스트에서 직접 확인합니다. */
	friend class FValidatedWeaponShotRateTest;

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

	/** 사수나 대상이 이미 쓰러졌으면 판정하지 않습니다. 체력이 없는 액터는 살아 있는 것으로 봅니다. */
	static bool IsActorAlive(const AActor* Actor);

	/** 서버가 알고 있는 조준 방향과 발사 방향의 차이를 확인합니다. */
	bool IsAimPlausible(const FVector& ShotDirection, float ExtraToleranceDegrees = 0.0f) const;

	/**
	 * 발사 간격을 확인하고 이번 발사를 기록합니다.
	 * 같은 발사 시각의 보고는 같은 종류의 발사이고 서로 다른 대상일 때만 MaxTargets까지 허용합니다.
	 */
	bool ConsumeShot(const AActor* Target, double HitServerTimeSeconds, int32 MaxTargets, EReportedShotKind ShotKind);

	/** 서버가 측정한 왕복 지연으로 되돌릴 수 있는 최대 시간을 계산합니다. */
	double GetMaximumRewindSeconds() const;

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

	/** 무기의 최소 발사 간격입니다. 0.1초는 분당 600발입니다. */
	UPROPERTY(EditDefaultsOnly, Category="Combat|Validation", meta=(ClampMin="0.01"))
	float MinimumShotIntervalSeconds = 0.1f;

	/** 발사 간격마다 허용하는 오차 비율입니다. 평균 연사 속도는 MinimumShotIntervalSeconds를 넘지 않습니다. */
	UPROPERTY(EditDefaultsOnly, Category="Combat|Validation", meta=(ClampMin="0.0", ClampMax="0.5"))
	float ShotIntervalTolerance = 0.15f;

	/** 같은 발사로 보고되는 시각의 허용 오차입니다. */
	UPROPERTY(EditDefaultsOnly, Category="Combat|Validation", meta=(ClampMin="0.0", ClampMax="0.01"))
	float SameShotToleranceSeconds = 0.001f;

	/** 산탄 한 발이 동시에 맞힐 수 있는 최대 대상 수입니다. */
	UPROPERTY(EditDefaultsOnly, Category="Combat|Validation", meta=(ClampMin="1", ClampMax="16"))
	int32 MaximumShotgunTargets = 4;

	/**
	 * 핑 외에 추가로 되돌릴 수 있는 시간입니다.
	 * 다른 플레이어의 보간 지연과 측정 오차를 포함합니다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Combat|Validation", meta=(ClampMin="0.0", ClampMax="0.5"))
	float RewindAllowanceSeconds = 0.15f;

	/** 서버가 아는 눈 위치에서 발사 시작점까지 허용하는 거리입니다. 이동 거리는 따로 더합니다. */
	UPROPERTY(EditDefaultsOnly, Category="Combat|Validation", meta=(ClampMin="0.0"))
	float MaximumTraceStartOffset = 150.0f;

	/** 서버 조준 방향과 발사 방향 사이에 허용하는 최대 각도입니다. */
	UPROPERTY(EditDefaultsOnly, Category="Combat|Validation", meta=(ClampMin="1.0", ClampMax="180.0"))
	float MaximumAimDeviationDegrees = 30.0f;

	/** 산탄 펠릿에 추가로 허용하는 퍼짐 각도입니다. */
	UPROPERTY(EditDefaultsOnly, Category="Combat|Validation", meta=(ClampMin="0.0", ClampMax="45.0"))
	float MaximumPelletSpreadDegrees = 15.0f;

	UPROPERTY(EditDefaultsOnly, Category="Combat|Damage")
	TSubclassOf<UDamageType> DamageTypeClass;

	double LastShotServerTimeSeconds = -1000.0;
	double ShotBudget = 1.0;
	EReportedShotKind LastShotKind = EReportedShotKind::Hitscan;
	TArray<TWeakObjectPtr<const AActor>> TargetsHitThisShot;
};

// Copyright Kang Hyungsoon. Portfolio source code.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Containers/List.h"
#include "Network/RewindTypes.h"
#include "RewindHistoryComponent.generated.h"

class UBoxComponent;
class UPrimitiveComponent;

/**
 * 서버에서 캐릭터 충돌 박스의 과거 상태를 기록하고 명중 시점으로 되돌려 판정합니다.
 * 기록은 최신 프레임을 머리에 두는 이중 연결 리스트로 관리합니다.
 */
UCLASS(ClassGroup=(Network), meta=(BlueprintSpawnableComponent))
class NETWORKSHOOTER_API URewindHistoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URewindHistoryComponent();

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction
	) override;

	/** 기록할 충돌 박스를 등록합니다. 치명 부위 여부는 판정 배율에 사용됩니다. */
	void RegisterHitBox(FName Name, UBoxComponent* HitBox, bool bCritical);

	/** 히트스캔 무기의 선분과 과거 충돌 박스가 만났는지 확인합니다. */
	FRewindCheckResult ConfirmHitscan(
		const FVector& TraceStart,
		const FVector& TraceEnd,
		double HitServerTimeSeconds
	);

	/** 발사 시작점과 초기 속도로 투사체 경로를 다시 계산합니다. */
	FRewindCheckResult ConfirmProjectile(
		const FVector& TraceStart,
		const FVector& InitialVelocity,
		double HitServerTimeSeconds
	);

	/** 한 대상에게 향한 산탄 펠릿의 일반·치명 부위 명중 수를 집계합니다. */
	FShotgunRewindResult ConfirmShotgun(
		const FVector& TraceStart,
		const TArray<FVector>& PelletTraceEnds,
		double HitServerTimeSeconds
	);

	UFUNCTION(BlueprintPure, Category="Network|Rewind")
	float GetRecordWindowSeconds() const { return RecordWindowSeconds; }

private:
	struct FRegisteredHitBox
	{
		TWeakObjectPtr<UBoxComponent> Component;
		bool bCritical = false;
	};

	void SaveCurrentFrame();
	FRewindFrame CaptureFrame(double ServerTimeSeconds) const;
	bool TrySampleFrame(double TargetServerTimeSeconds, FRewindFrame& OutFrame) const;
	FRewindFrame InterpolateFrames(
		const FRewindFrame& OlderFrame,
		const FRewindFrame& NewerFrame,
		double TargetServerTimeSeconds
	) const;

	void ApplyFrame(const FRewindFrame& Frame);
	void RestoreFrame(const FRewindFrame& Frame);
	void SetRegisteredHitBoxesForTrace(bool bEnabled);
	bool IsCriticalHitComponent(const UPrimitiveComponent* Component) const;
	FRewindCheckResult BuildResultFromHit(const FHitResult& HitResult) const;

	UPROPERTY(EditDefaultsOnly, Category="Network|Rewind", meta=(ClampMin="0.25", ClampMax="3.0"))
	float RecordWindowSeconds = 1.25f;

	UPROPERTY(EditDefaultsOnly, Category="Network|Rewind", meta=(ClampMin="10.0", ClampMax="120.0"))
	float ProjectileSimulationFrequency = 30.0f;

	TMap<FName, FRegisteredHitBox> RegisteredHitBoxes;
	TDoubleLinkedList<FRewindFrame> FrameHistory;
};

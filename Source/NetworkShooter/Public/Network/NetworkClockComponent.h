// Copyright Kang Hyungsoon. Portfolio source code.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NetworkClockComponent.generated.h"

/**
 * 왕복 시간을 이용해 클라이언트의 서버 시간 추정값을 계산합니다.
 * 소유권이 있는 PlayerController에 부착해야 RPC가 정상적으로 전달됩니다.
 */
UCLASS(ClassGroup=(Network), meta=(BlueprintSpawnableComponent))
class NETWORKSHOOTER_API UNetworkClockComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNetworkClockComponent();

	virtual void BeginPlay() override;

	/** 현재 클라이언트가 추정한 서버 시간을 반환합니다. */
	UFUNCTION(BlueprintPure, Category="Network|Clock")
	double GetEstimatedServerTimeSeconds() const;

	/** 최근 측정값을 평활화한 편도 지연 시간입니다. */
	UFUNCTION(BlueprintPure, Category="Network|Clock")
	float GetEstimatedOneWayLatencySeconds() const { return SmoothedOneWayLatencySeconds; }

	/** 디버그 화면에서 확인할 수 있도록 서버-클라이언트 시간 차이를 반환합니다. */
	UFUNCTION(BlueprintPure, Category="Network|Clock")
	float GetServerTimeOffsetSeconds() const { return SmoothedServerTimeOffsetSeconds; }

private:
	/** 클라이언트가 요청을 보낸 시각을 서버로 전달합니다. */
	UFUNCTION(Server, Unreliable)
	void ServerRequestTime(double ClientSendTimeSeconds);

	/** 서버 수신 시각을 클라이언트에 돌려보냅니다. */
	UFUNCTION(Client, Unreliable)
	void ClientReceiveTime(double ClientSendTimeSeconds, double ServerReceiveTimeSeconds);

	void RequestTimeSample();

	UPROPERTY(EditDefaultsOnly, Category="Network|Clock", meta=(ClampMin="0.5"))
	float SyncIntervalSeconds = 5.0f;

	UPROPERTY(EditDefaultsOnly, Category="Network|Clock", meta=(ClampMin="0.01", ClampMax="1.0"))
	float SmoothingFactor = 0.15f;

	UPROPERTY(VisibleInstanceOnly, Category="Network|Clock")
	float SmoothedServerTimeOffsetSeconds = 0.0f;

	UPROPERTY(VisibleInstanceOnly, Category="Network|Clock")
	float SmoothedOneWayLatencySeconds = 0.0f;

	bool bHasTimeSample = false;
	FTimerHandle SyncTimerHandle;
};

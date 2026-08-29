// Copyright Kang Hyungsoon. Portfolio source code.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "NetworkShooterPlayerController.generated.h"

class UNetworkClockComponent;

/** 서버 시간 동기화 컴포넌트를 소유하는 플레이어 컨트롤러입니다. */
UCLASS()
class NETWORKSHOOTER_API ANetworkShooterPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ANetworkShooterPlayerController();

	UFUNCTION(BlueprintPure, Category="Network|Clock")
	UNetworkClockComponent* GetNetworkClock() const { return NetworkClock; }

private:
	UPROPERTY(VisibleAnywhere, Category="Network|Clock")
	TObjectPtr<UNetworkClockComponent> NetworkClock;
};

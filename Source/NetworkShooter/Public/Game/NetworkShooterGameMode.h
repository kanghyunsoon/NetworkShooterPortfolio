// Copyright Kang Hyungsoon. Portfolio source code.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "NetworkShooterGameMode.generated.h"

/** 코드만으로 실행 가능한 기본 클래스 구성을 연결합니다. */
UCLASS()
class NETWORKSHOOTER_API ANetworkShooterGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ANetworkShooterGameMode();
};

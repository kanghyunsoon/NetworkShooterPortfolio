// Copyright Kang Hyungsoon. Portfolio source code.

#include "Player/NetworkShooterPlayerController.h"

#include "Network/NetworkClockComponent.h"

ANetworkShooterPlayerController::ANetworkShooterPlayerController()
{
	NetworkClock = CreateDefaultSubobject<UNetworkClockComponent>(TEXT("NetworkClock"));
}

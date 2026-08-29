// Copyright Kang Hyungsoon. Portfolio source code.

#include "Game/NetworkShooterGameMode.h"

#include "Character/NetworkShooterCharacter.h"
#include "Player/NetworkShooterPlayerController.h"

ANetworkShooterGameMode::ANetworkShooterGameMode()
{
	DefaultPawnClass = ANetworkShooterCharacter::StaticClass();
	PlayerControllerClass = ANetworkShooterPlayerController::StaticClass();
}

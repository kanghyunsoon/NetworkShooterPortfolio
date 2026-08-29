// Copyright Kang Hyungsoon. Portfolio source code.

#include "Network/NetworkClockComponent.h"

#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

UNetworkClockComponent::UNetworkClockComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UNetworkClockComponent::BeginPlay()
{
	Super::BeginPlay();

	const APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}

	RequestTimeSample();
	GetWorld()->GetTimerManager().SetTimer(
		SyncTimerHandle,
		this,
		&UNetworkClockComponent::RequestTimeSample,
		SyncIntervalSeconds,
		true
	);
}

double UNetworkClockComponent::GetEstimatedServerTimeSeconds() const
{
	if (!GetWorld())
	{
		return 0.0;
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		return GetWorld()->GetTimeSeconds();
	}

	return GetWorld()->GetTimeSeconds() + SmoothedServerTimeOffsetSeconds;
}

void UNetworkClockComponent::RequestTimeSample()
{
	if (GetWorld())
	{
		ServerRequestTime(GetWorld()->GetTimeSeconds());
	}
}

void UNetworkClockComponent::ServerRequestTime_Implementation(const double ClientSendTimeSeconds)
{
	if (GetWorld())
	{
		ClientReceiveTime(ClientSendTimeSeconds, GetWorld()->GetTimeSeconds());
	}
}

void UNetworkClockComponent::ClientReceiveTime_Implementation(
	const double ClientSendTimeSeconds,
	const double ServerReceiveTimeSeconds
)
{
	if (!GetWorld())
	{
		return;
	}

	const double ClientReceiveTimeSeconds = GetWorld()->GetTimeSeconds();
	const double RoundTripSeconds = FMath::Max(0.0, ClientReceiveTimeSeconds - ClientSendTimeSeconds);
	const double OneWaySeconds = RoundTripSeconds * 0.5;
	const double EstimatedServerNow = ServerReceiveTimeSeconds + OneWaySeconds;
	const float NewOffsetSeconds = static_cast<float>(EstimatedServerNow - ClientReceiveTimeSeconds);
	const float NewOneWaySeconds = static_cast<float>(OneWaySeconds);

	if (!bHasTimeSample)
	{
		SmoothedServerTimeOffsetSeconds = NewOffsetSeconds;
		SmoothedOneWayLatencySeconds = NewOneWaySeconds;
		bHasTimeSample = true;
		return;
	}

	SmoothedServerTimeOffsetSeconds = FMath::Lerp(
		SmoothedServerTimeOffsetSeconds,
		NewOffsetSeconds,
		SmoothingFactor
	);
	SmoothedOneWayLatencySeconds = FMath::Lerp(
		SmoothedOneWayLatencySeconds,
		NewOneWaySeconds,
		SmoothingFactor
	);
}

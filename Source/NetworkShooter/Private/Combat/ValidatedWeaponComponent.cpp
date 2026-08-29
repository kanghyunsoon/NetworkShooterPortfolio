// Copyright Kang Hyungsoon. Portfolio source code.

#include "Combat/ValidatedWeaponComponent.h"

#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Network/RewindHistoryComponent.h"

UValidatedWeaponComponent::UValidatedWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	DamageTypeClass = UDamageType::StaticClass();
}

void UValidatedWeaponComponent::ReportHitscan(
	AActor* Target,
	const FVector TraceStart,
	const FVector TraceEnd,
	const double EstimatedServerHitTimeSeconds
)
{
	ServerReportHitscan(Target, TraceStart, TraceEnd, EstimatedServerHitTimeSeconds);
}

void UValidatedWeaponComponent::ReportProjectile(
	AActor* Target,
	const FVector TraceStart,
	const FVector InitialVelocity,
	const double EstimatedServerHitTimeSeconds
)
{
	ServerReportProjectile(Target, TraceStart, InitialVelocity, EstimatedServerHitTimeSeconds);
}

void UValidatedWeaponComponent::ReportShotgun(
	AActor* Target,
	const FVector TraceStart,
	const TArray<FVector>& PelletTraceEnds,
	const double EstimatedServerHitTimeSeconds
)
{
	TArray<FVector_NetQuantize> QuantizedTraceEnds;
	const int32 Count = FMath::Min(PelletTraceEnds.Num(), MaximumPelletCount);
	QuantizedTraceEnds.Reserve(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		QuantizedTraceEnds.Add(PelletTraceEnds[Index]);
	}

	ServerReportShotgun(Target, TraceStart, QuantizedTraceEnds, EstimatedServerHitTimeSeconds);
}

bool UValidatedWeaponComponent::IsRequestPlausible(
	const AActor* Target,
	const FVector& TraceStart,
	const double HitServerTimeSeconds
) const
{
	if (!GetWorld() || !GetOwner() || !Target || Target == GetOwner())
	{
		return false;
	}

	const double ServerNow = GetWorld()->GetTimeSeconds();
	if (HitServerTimeSeconds < ServerNow - MaximumRequestAgeSeconds ||
		HitServerTimeSeconds > ServerNow + FutureTimeToleranceSeconds)
	{
		return false;
	}

	if (FVector::DistSquared(GetOwner()->GetActorLocation(), TraceStart) > FMath::Square(MaximumTraceDistance))
	{
		return false;
	}

	return Target->FindComponentByClass<URewindHistoryComponent>() != nullptr;
}

void UValidatedWeaponComponent::ApplyValidatedDamage(AActor* Target, const float DamageToApply) const
{
	if (!Target || DamageToApply <= 0.0f)
	{
		return;
	}

	const APawn* InstigatorPawn = Cast<APawn>(GetOwner());
	AController* InstigatorController = InstigatorPawn ? InstigatorPawn->GetController() : nullptr;
	UGameplayStatics::ApplyDamage(
		Target,
		DamageToApply,
		InstigatorController,
		GetOwner(),
		DamageTypeClass
	);
}

void UValidatedWeaponComponent::ServerReportHitscan_Implementation(
	AActor* Target,
	const FVector_NetQuantize TraceStart,
	const FVector_NetQuantize TraceEnd,
	const double HitServerTimeSeconds
)
{
	if (!IsRequestPlausible(Target, TraceStart, HitServerTimeSeconds))
	{
		return;
	}

	const FVector Direction = TraceEnd - TraceStart;
	if (Direction.SizeSquared() > FMath::Square(MaximumTraceDistance))
	{
		return;
	}

	URewindHistoryComponent* Rewind = Target->FindComponentByClass<URewindHistoryComponent>();
	const FRewindCheckResult Result = Rewind->ConfirmHitscan(TraceStart, TraceEnd, HitServerTimeSeconds);
	if (Result.bConfirmed)
	{
		ApplyValidatedDamage(Target, Result.bCritical ? CriticalDamage : BodyDamage);
	}
}

void UValidatedWeaponComponent::ServerReportProjectile_Implementation(
	AActor* Target,
	const FVector_NetQuantize TraceStart,
	const FVector_NetQuantize100 InitialVelocity,
	const double HitServerTimeSeconds
)
{
	if (!IsRequestPlausible(Target, TraceStart, HitServerTimeSeconds) ||
		InitialVelocity.SizeSquared() > FMath::Square(MaximumProjectileSpeed))
	{
		return;
	}

	URewindHistoryComponent* Rewind = Target->FindComponentByClass<URewindHistoryComponent>();
	const FRewindCheckResult Result = Rewind->ConfirmProjectile(
		TraceStart,
		InitialVelocity,
		HitServerTimeSeconds
	);
	if (Result.bConfirmed)
	{
		ApplyValidatedDamage(Target, Result.bCritical ? CriticalDamage : BodyDamage);
	}
}

void UValidatedWeaponComponent::ServerReportShotgun_Implementation(
	AActor* Target,
	const FVector_NetQuantize TraceStart,
	const TArray<FVector_NetQuantize>& PelletTraceEnds,
	const double HitServerTimeSeconds
)
{
	if (!IsRequestPlausible(Target, TraceStart, HitServerTimeSeconds) ||
		PelletTraceEnds.IsEmpty() ||
		PelletTraceEnds.Num() > MaximumPelletCount)
	{
		return;
	}

	TArray<FVector> CheckedTraceEnds;
	CheckedTraceEnds.Reserve(PelletTraceEnds.Num());
	for (const FVector_NetQuantize& TraceEnd : PelletTraceEnds)
	{
		if (FVector::DistSquared(TraceStart, TraceEnd) > FMath::Square(MaximumTraceDistance))
		{
			return;
		}
		CheckedTraceEnds.Add(TraceEnd);
	}

	URewindHistoryComponent* Rewind = Target->FindComponentByClass<URewindHistoryComponent>();
	const FShotgunRewindResult Result = Rewind->ConfirmShotgun(
		TraceStart,
		CheckedTraceEnds,
		HitServerTimeSeconds
	);

	const float TotalDamage =
		Result.BodyHitCount * BodyDamage +
		Result.CriticalHitCount * CriticalDamage;
	ApplyValidatedDamage(Target, TotalDamage);
}

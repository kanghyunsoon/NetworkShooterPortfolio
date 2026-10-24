// Copyright Kang Hyungsoon. Portfolio source code.

#include "Combat/ValidatedWeaponComponent.h"

#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
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
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!GetWorld() || !OwnerPawn || !Target || Target == OwnerPawn)
	{
		return false;
	}

	// 클라이언트가 고른 시각을 그대로 믿으면 대상이 가장 노출됐던 순간을 골라 보낼 수 있습니다.
	// 되돌릴 수 있는 시간은 서버가 잰 이 플레이어의 지연 시간으로 제한합니다.
	const double ServerNow = GetWorld()->GetTimeSeconds();
	const double RequestAgeSeconds = ServerNow - HitServerTimeSeconds;
	if (RequestAgeSeconds > GetMaximumRewindSeconds() ||
		HitServerTimeSeconds > ServerNow + FutureTimeToleranceSeconds)
	{
		return false;
	}

	// 발사 시작점은 서버가 아는 눈 위치 근처여야 합니다. 요청이 오는 동안 움직인 거리는 허용합니다.
	const double AllowedOffset =
		MaximumTraceStartOffset + OwnerPawn->GetVelocity().Size() * FMath::Max(0.0, RequestAgeSeconds);
	if (FVector::DistSquared(OwnerPawn->GetPawnViewLocation(), TraceStart) > FMath::Square(AllowedOffset))
	{
		return false;
	}

	return Target->FindComponentByClass<URewindHistoryComponent>() != nullptr;
}

double UValidatedWeaponComponent::GetMaximumRewindSeconds() const
{
	double RoundTripSeconds = 0.0;
	if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (const APlayerState* PlayerState = OwnerPawn->GetPlayerState())
		{
			RoundTripSeconds = PlayerState->GetPingInMilliseconds() / 1000.0;
		}
	}

	return FMath::Min(RoundTripSeconds + RewindAllowanceSeconds, static_cast<double>(MaximumRequestAgeSeconds));
}

bool UValidatedWeaponComponent::IsAimPlausible(const FVector& ShotDirection, const float ExtraToleranceDegrees) const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	const FVector Direction = ShotDirection.GetSafeNormal();
	if (!OwnerPawn || Direction.IsNearlyZero())
	{
		return false;
	}

	const FVector AimDirection = OwnerPawn->GetBaseAimRotation().Vector();
	const float MaxAngleDegrees = FMath::Min(MaximumAimDeviationDegrees + ExtraToleranceDegrees, 180.0f);
	return FVector::DotProduct(AimDirection, Direction) >= FMath::Cos(FMath::DegreesToRadians(MaxAngleDegrees));
}

bool UValidatedWeaponComponent::ConsumeShot(const AActor* Target, const double HitServerTimeSeconds, const int32 MaxTargets)
{
	// 같은 발사에서 여러 대상을 맞힌 보고는 허용하지만 같은 대상을 두 번 보고할 수는 없습니다.
	if (FMath::IsNearlyEqual(HitServerTimeSeconds, LastShotServerTimeSeconds, static_cast<double>(SameShotToleranceSeconds)))
	{
		TargetsHitThisShot.RemoveAll([](const TWeakObjectPtr<const AActor>& Hit) { return !Hit.IsValid(); });
		if (TargetsHitThisShot.Num() >= MaxTargets ||
			TargetsHitThisShot.ContainsByPredicate([Target](const TWeakObjectPtr<const AActor>& Hit) { return Hit.Get() == Target; }))
		{
			return false;
		}

		TargetsHitThisShot.Add(Target);
		return true;
	}

	// 발사 시각은 클라이언트가 보내지만 서버 시간 창 안에서만 받기 때문에,
	// 간격을 지키도록 하면 초당 보고 수가 무기 연사 속도를 넘지 못합니다.
	// 클라이언트는 프레임 단위로 발사하므로 간격마다 약간의 오차는 허용하고,
	// 모자란 만큼은 다음 발사로 넘겨서 평균 연사 속도는 넘지 못하게 합니다.
	const double ElapsedSeconds = HitServerTimeSeconds - LastShotServerTimeSeconds;
	if (ElapsedSeconds <= 0.0)
	{
		return false;
	}

	const double Budget = FMath::Min(ShotBudget + ElapsedSeconds / MinimumShotIntervalSeconds, 1.0);
	if (Budget < 1.0 - ShotIntervalTolerance)
	{
		return false;
	}

	ShotBudget = Budget - 1.0;
	LastShotServerTimeSeconds = HitServerTimeSeconds;
	TargetsHitThisShot.Reset();
	TargetsHitThisShot.Add(Target);
	return true;
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
	if (Direction.SizeSquared() > FMath::Square(MaximumTraceDistance) ||
		!IsAimPlausible(Direction) ||
		!ConsumeShot(Target, HitServerTimeSeconds, 1))
	{
		return;
	}

	URewindHistoryComponent* Rewind = Target->FindComponentByClass<URewindHistoryComponent>();
	const FRewindCheckResult Result = Rewind->ConfirmHitscan(TraceStart, TraceEnd, HitServerTimeSeconds, GetOwner());
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
		InitialVelocity.SizeSquared() > FMath::Square(MaximumProjectileSpeed) ||
		!IsAimPlausible(InitialVelocity) ||
		!ConsumeShot(Target, HitServerTimeSeconds, 1))
	{
		return;
	}

	URewindHistoryComponent* Rewind = Target->FindComponentByClass<URewindHistoryComponent>();
	const FRewindCheckResult Result = Rewind->ConfirmProjectile(
		TraceStart,
		InitialVelocity,
		HitServerTimeSeconds,
		GetOwner()
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
		if (FVector::DistSquared(TraceStart, TraceEnd) > FMath::Square(MaximumTraceDistance) ||
			!IsAimPlausible(TraceEnd - TraceStart, MaximumPelletSpreadDegrees))
		{
			return;
		}
		CheckedTraceEnds.Add(TraceEnd);
	}

	if (!ConsumeShot(Target, HitServerTimeSeconds, MaximumShotgunTargets))
	{
		return;
	}

	URewindHistoryComponent* Rewind = Target->FindComponentByClass<URewindHistoryComponent>();
	const FShotgunRewindResult Result = Rewind->ConfirmShotgun(
		TraceStart,
		CheckedTraceEnds,
		HitServerTimeSeconds,
		GetOwner()
	);

	const float TotalDamage =
		Result.BodyHitCount * BodyDamage +
		Result.CriticalHitCount * CriticalDamage;
	ApplyValidatedDamage(Target, TotalDamage);
}

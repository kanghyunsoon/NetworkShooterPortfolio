// Copyright Kang Hyungsoon. Portfolio source code.

#include "Network/RewindHistoryComponent.h"

#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"

namespace RewindCollision
{
	constexpr ECollisionChannel HitBoxChannel = ECC_GameTraceChannel1;
}

URewindHistoryComponent::URewindHistoryComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	SetIsReplicatedByDefault(false);
}

void URewindHistoryComponent::BeginPlay()
{
	Super::BeginPlay();

	// 기록은 서버에서만 사용하므로 클라이언트에서는 매 프레임 틱을 돌리지 않습니다.
	SetComponentTickEnabled(GetOwner() && GetOwner()->HasAuthority());
}

void URewindHistoryComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction
)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		SaveCurrentFrame();
	}
}

void URewindHistoryComponent::RegisterHitBox(
	const FName Name,
	UBoxComponent* HitBox,
	const bool bCritical
)
{
	if (!HitBox || Name.IsNone())
	{
		return;
	}

	FRegisteredHitBox Data;
	Data.Component = HitBox;
	Data.bCritical = bCritical;
	RegisteredHitBoxes.Add(Name, Data);

	// 평상시에는 전용 충돌 박스를 끄고 리와인드 판정 중에만 활성화합니다.
	HitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HitBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	HitBox->SetCollisionResponseToChannel(RewindCollision::HitBoxChannel, ECR_Block);
}

void URewindHistoryComponent::SaveCurrentFrame()
{
	if (!GetWorld() || RegisteredHitBoxes.IsEmpty())
	{
		return;
	}

	FrameHistory.AddHead(CaptureFrame(GetWorld()->GetTimeSeconds()));

	while (FrameHistory.GetHead() && FrameHistory.GetTail())
	{
		const double HistoryLength =
			FrameHistory.GetHead()->GetValue().ServerTimeSeconds -
			FrameHistory.GetTail()->GetValue().ServerTimeSeconds;

		if (HistoryLength <= RecordWindowSeconds)
		{
			break;
		}

		FrameHistory.RemoveNode(FrameHistory.GetTail());
	}
}

FRewindFrame URewindHistoryComponent::CaptureFrame(const double ServerTimeSeconds) const
{
	FRewindFrame Frame;
	Frame.ServerTimeSeconds = ServerTimeSeconds;
	Frame.HitBoxes.Reserve(RegisteredHitBoxes.Num());

	for (const TPair<FName, FRegisteredHitBox>& Pair : RegisteredHitBoxes)
	{
		const UBoxComponent* HitBox = Pair.Value.Component.Get();
		if (!HitBox)
		{
			continue;
		}

		FRewindHitBoxState State;
		State.Name = Pair.Key;
		State.Location = HitBox->GetComponentLocation();
		State.Rotation = HitBox->GetComponentRotation();
		State.Extent = HitBox->GetUnscaledBoxExtent();
		Frame.HitBoxes.Add(State);
	}

	return Frame;
}

bool URewindHistoryComponent::TrySampleFrame(
	const double TargetServerTimeSeconds,
	FRewindFrame& OutFrame
) const
{
	const TDoubleLinkedList<FRewindFrame>::TDoubleLinkedListNode* Head = FrameHistory.GetHead();
	const TDoubleLinkedList<FRewindFrame>::TDoubleLinkedListNode* Tail = FrameHistory.GetTail();
	if (!Head || !Tail)
	{
		return false;
	}

	const double NewestTime = Head->GetValue().ServerTimeSeconds;
	const double OldestTime = Tail->GetValue().ServerTimeSeconds;
	if (TargetServerTimeSeconds > NewestTime || TargetServerTimeSeconds < OldestTime)
	{
		return false;
	}

	const TDoubleLinkedList<FRewindFrame>::TDoubleLinkedListNode* Newer = Head;
	const TDoubleLinkedList<FRewindFrame>::TDoubleLinkedListNode* Older = Head;

	while (Older->GetNextNode() && Older->GetValue().ServerTimeSeconds > TargetServerTimeSeconds)
	{
		Newer = Older;
		Older = Older->GetNextNode();
	}

	if (FMath::IsNearlyEqual(Older->GetValue().ServerTimeSeconds, TargetServerTimeSeconds, 0.0001))
	{
		OutFrame = Older->GetValue();
		return true;
	}

	if (Newer == Older)
	{
		OutFrame = Newer->GetValue();
		return true;
	}

	OutFrame = InterpolateFrames(Older->GetValue(), Newer->GetValue(), TargetServerTimeSeconds);
	return true;
}

FRewindFrame URewindHistoryComponent::InterpolateFrames(
	const FRewindFrame& OlderFrame,
	const FRewindFrame& NewerFrame,
	const double TargetServerTimeSeconds
) const
{
	FRewindFrame Result;
	Result.ServerTimeSeconds = TargetServerTimeSeconds;
	Result.HitBoxes.Reserve(NewerFrame.HitBoxes.Num());

	const double TimeSpan = NewerFrame.ServerTimeSeconds - OlderFrame.ServerTimeSeconds;
	const float Alpha = TimeSpan > KINDA_SMALL_NUMBER
		? static_cast<float>((TargetServerTimeSeconds - OlderFrame.ServerTimeSeconds) / TimeSpan)
		: 0.0f;

	for (const FRewindHitBoxState& NewerState : NewerFrame.HitBoxes)
	{
		const FRewindHitBoxState* OlderState = OlderFrame.HitBoxes.FindByPredicate(
			[&NewerState](const FRewindHitBoxState& Candidate)
			{
				return Candidate.Name == NewerState.Name;
			}
		);

		if (!OlderState)
		{
			Result.HitBoxes.Add(NewerState);
			continue;
		}

		FRewindHitBoxState Interpolated;
		Interpolated.Name = NewerState.Name;
		Interpolated.Location = FMath::Lerp(OlderState->Location, NewerState.Location, Alpha);
		Interpolated.Rotation = FQuat::Slerp(
			OlderState->Rotation.Quaternion(),
			NewerState.Rotation.Quaternion(),
			Alpha
		).Rotator();
		Interpolated.Extent = FMath::Lerp(OlderState->Extent, NewerState.Extent, Alpha);
		Result.HitBoxes.Add(Interpolated);
	}

	return Result;
}

void URewindHistoryComponent::ApplyFrame(const FRewindFrame& Frame)
{
	for (const FRewindHitBoxState& State : Frame.HitBoxes)
	{
		const FRegisteredHitBox* Data = RegisteredHitBoxes.Find(State.Name);
		UBoxComponent* HitBox = Data ? Data->Component.Get() : nullptr;
		if (!HitBox)
		{
			continue;
		}

		HitBox->SetWorldLocationAndRotation(State.Location, State.Rotation, false, nullptr, ETeleportType::TeleportPhysics);
		HitBox->SetBoxExtent(State.Extent, false);
	}
}

void URewindHistoryComponent::RestoreFrame(const FRewindFrame& Frame)
{
	ApplyFrame(Frame);
	SetRegisteredHitBoxesForTrace(false);
}

void URewindHistoryComponent::SetRegisteredHitBoxesForTrace(const bool bEnabled)
{
	for (const TPair<FName, FRegisteredHitBox>& Pair : RegisteredHitBoxes)
	{
		if (UBoxComponent* HitBox = Pair.Value.Component.Get())
		{
			HitBox->SetCollisionEnabled(bEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
			HitBox->SetCollisionResponseToAllChannels(ECR_Ignore);
			HitBox->SetCollisionResponseToChannel(RewindCollision::HitBoxChannel, ECR_Block);
		}
	}
}

bool URewindHistoryComponent::IsCriticalHitComponent(const UPrimitiveComponent* Component) const
{
	for (const TPair<FName, FRegisteredHitBox>& Pair : RegisteredHitBoxes)
	{
		if (Pair.Value.Component.Get() == Component)
		{
			return Pair.Value.bCritical;
		}
	}

	return false;
}

FRewindCheckResult URewindHistoryComponent::BuildResultFromHit(const FHitResult& HitResult) const
{
	FRewindCheckResult Result;
	Result.bConfirmed = HitResult.bBlockingHit && HitResult.GetActor() == GetOwner();
	Result.bCritical = Result.bConfirmed && IsCriticalHitComponent(HitResult.GetComponent());
	return Result;
}

bool URewindHistoryComponent::IsBlockedByWorld(const FVector& Start, const FVector& End, const AActor* Shooter) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RewindLineOfSight), false);
	Params.AddIgnoredActor(GetOwner());
	if (Shooter)
	{
		Params.AddIgnoredActor(Shooter);
	}

	return GetWorld()->LineTraceTestByChannel(Start, End, ECC_Visibility, Params);
}

FRewindCheckResult URewindHistoryComponent::ConfirmHitscan(
	const FVector& TraceStart,
	const FVector& TraceEnd,
	const double HitServerTimeSeconds,
	const AActor* Shooter
)
{
	FRewindCheckResult Result;
	if (!GetOwner() || !GetOwner()->HasAuthority() || !GetWorld())
	{
		return Result;
	}

	FRewindFrame HistoricalFrame;
	if (!TrySampleFrame(HitServerTimeSeconds, HistoricalFrame))
	{
		return Result;
	}

	const FRewindFrame PresentFrame = CaptureFrame(GetWorld()->GetTimeSeconds());
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	const ECollisionEnabled::Type MeshCollision = Character && Character->GetMesh()
		? Character->GetMesh()->GetCollisionEnabled()
		: ECollisionEnabled::NoCollision;

	ApplyFrame(HistoricalFrame);
	SetRegisteredHitBoxesForTrace(true);
	if (Character && Character->GetMesh())
	{
		Character->GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	FHitResult HitResult;
	GetWorld()->LineTraceSingleByChannel(
		HitResult,
		TraceStart,
		TraceEnd,
		RewindCollision::HitBoxChannel
	);
	Result = BuildResultFromHit(HitResult);

	RestoreFrame(PresentFrame);
	if (Character && Character->GetMesh())
	{
		Character->GetMesh()->SetCollisionEnabled(MeshCollision);
	}

	if (Result.bConfirmed && IsBlockedByWorld(TraceStart, HitResult.ImpactPoint, Shooter))
	{
		Result = FRewindCheckResult();
	}

	return Result;
}

FRewindCheckResult URewindHistoryComponent::ConfirmProjectile(
	const FVector& TraceStart,
	const FVector& InitialVelocity,
	const double HitServerTimeSeconds,
	const AActor* Shooter
)
{
	FRewindCheckResult Result;
	if (!GetOwner() || !GetOwner()->HasAuthority() || !GetWorld())
	{
		return Result;
	}

	FRewindFrame HistoricalFrame;
	if (!TrySampleFrame(HitServerTimeSeconds, HistoricalFrame))
	{
		return Result;
	}

	const FRewindFrame PresentFrame = CaptureFrame(GetWorld()->GetTimeSeconds());
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	const ECollisionEnabled::Type MeshCollision = Character && Character->GetMesh()
		? Character->GetMesh()->GetCollisionEnabled()
		: ECollisionEnabled::NoCollision;

	ApplyFrame(HistoricalFrame);
	SetRegisteredHitBoxesForTrace(true);
	if (Character && Character->GetMesh())
	{
		Character->GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	FPredictProjectilePathParams PathParams;
	PathParams.StartLocation = TraceStart;
	PathParams.LaunchVelocity = InitialVelocity;
	PathParams.MaxSimTime = RecordWindowSeconds;
	PathParams.SimFrequency = ProjectileSimulationFrequency;
	PathParams.ProjectileRadius = 5.0f;
	PathParams.TraceChannel = RewindCollision::HitBoxChannel;
	PathParams.bTraceWithCollision = true;
	// 판정 대상 자신을 제외하면 경로가 대상 박스를 통과해 명중이 확정되지 않습니다. 제외할 쪽은 사수입니다.
	if (Shooter)
	{
		PathParams.ActorsToIgnore.Add(const_cast<AActor*>(Shooter));
	}

	FPredictProjectilePathResult PathResult;
	UGameplayStatics::PredictProjectilePath(this, PathParams, PathResult);
	Result = BuildResultFromHit(PathResult.HitResult);

	RestoreFrame(PresentFrame);
	if (Character && Character->GetMesh())
	{
		Character->GetMesh()->SetCollisionEnabled(MeshCollision);
	}

	if (Result.bConfirmed)
	{
		// 포물선 경로를 구간별로 나눠 명중 지점까지 지형에 막히지 않았는지 확인합니다.
		FVector SegmentStart = TraceStart;
		for (const FPredictProjectilePathPointData& Point : PathResult.PathData)
		{
			if (IsBlockedByWorld(SegmentStart, Point.Location, Shooter))
			{
				return FRewindCheckResult();
			}
			SegmentStart = Point.Location;
		}

		if (IsBlockedByWorld(SegmentStart, PathResult.HitResult.ImpactPoint, Shooter))
		{
			return FRewindCheckResult();
		}
	}

	return Result;
}

FShotgunRewindResult URewindHistoryComponent::ConfirmShotgun(
	const FVector& TraceStart,
	const TArray<FVector>& PelletTraceEnds,
	const double HitServerTimeSeconds,
	const AActor* Shooter
)
{
	FShotgunRewindResult Result;
	if (!GetOwner() || !GetOwner()->HasAuthority() || !GetWorld())
	{
		return Result;
	}

	FRewindFrame HistoricalFrame;
	if (!TrySampleFrame(HitServerTimeSeconds, HistoricalFrame))
	{
		return Result;
	}

	const FRewindFrame PresentFrame = CaptureFrame(GetWorld()->GetTimeSeconds());
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	const ECollisionEnabled::Type MeshCollision = Character && Character->GetMesh()
		? Character->GetMesh()->GetCollisionEnabled()
		: ECollisionEnabled::NoCollision;

	ApplyFrame(HistoricalFrame);
	SetRegisteredHitBoxesForTrace(true);
	if (Character && Character->GetMesh())
	{
		Character->GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	TArray<FVector, TInlineAllocator<16>> ConfirmedImpactPoints;
	TArray<bool, TInlineAllocator<16>> ConfirmedCritical;
	for (const FVector& PelletTraceEnd : PelletTraceEnds)
	{
		FHitResult HitResult;
		GetWorld()->LineTraceSingleByChannel(
			HitResult,
			TraceStart,
			PelletTraceEnd,
			RewindCollision::HitBoxChannel
		);

		const FRewindCheckResult PelletResult = BuildResultFromHit(HitResult);
		if (!PelletResult.bConfirmed)
		{
			continue;
		}

		ConfirmedImpactPoints.Add(HitResult.ImpactPoint);
		ConfirmedCritical.Add(PelletResult.bCritical);
	}

	RestoreFrame(PresentFrame);
	if (Character && Character->GetMesh())
	{
		Character->GetMesh()->SetCollisionEnabled(MeshCollision);
	}

	// 지형 검사는 현재 프레임으로 되돌린 뒤 합니다. 대상과 사수는 검사에서 제외됩니다.
	for (int32 Index = 0; Index < ConfirmedImpactPoints.Num(); ++Index)
	{
		if (IsBlockedByWorld(TraceStart, ConfirmedImpactPoints[Index], Shooter))
		{
			continue;
		}

		ConfirmedCritical[Index] ? ++Result.CriticalHitCount : ++Result.BodyHitCount;
	}

	return Result;
}

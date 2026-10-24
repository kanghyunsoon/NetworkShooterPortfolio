// Copyright Kang Hyungsoon. Portfolio source code.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Character/NetworkShooterCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/WorldSettings.h"
#include "Network/RewindHistoryComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRewindHistoryWorldTest,
	"NetworkShooter.Rewind.World",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

namespace RewindHistoryWorldTest
{
	void TickWorld(UWorld* World, const int32 Frames)
	{
		for (int32 Index = 0; Index < Frames; ++Index)
		{
			// 틱 매니저는 같은 GFrameCounter 안에서 틱 함수를 한 번만 실행합니다.
			// 테스트는 엔진 프레임 하나 안에서 월드를 여러 번 틱하므로 카운터를 직접 올립니다.
			++GFrameCounter;
			World->Tick(LEVELTICK_All, 1.0f / 60.0f);
		}
	}
}

bool FRewindHistoryWorldTest::RunTest(const FString& Parameters)
{
	using namespace RewindHistoryWorldTest;

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("RewindHistoryWorldTest"));
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	World->GetWorldSettings()->NotifyBeginPlay();

	const FVector PositionA(0.0, 0.0, 100.0);
	const FVector PositionB(0.0, 600.0, 100.0);

	ANetworkShooterCharacter* Target = World->SpawnActor<ANetworkShooterCharacter>(PositionA, FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("대상 캐릭터 생성"), Target))
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
		return false;
	}

	URewindHistoryComponent* Rewind = Target->GetRewindHistory();
	TestTrue(TEXT("서버는 최대 체력으로 시작"), FMath::IsNearlyEqual(Target->GetHealth(), Target->GetMaxHealth()));

	TickWorld(World, 10);
	const double TimeAtA = World->GetTimeSeconds();
	TickWorld(World, 2);
	Target->SetActorLocation(PositionB);
	TickWorld(World, 10);
	const double TimeAtB = World->GetTimeSeconds();

	// 과거 위치를 겨눈 사격은 그 시각으로 되돌리면 맞고, 현재 시각으로는 빗나갑니다.
	const FVector StartA = PositionA + FVector(-800.0, 0.0, 0.0);
	const FVector EndA = PositionA + FVector(800.0, 0.0, 0.0);
	TestTrue(TEXT("과거 위치 히트스캔 확정"), Rewind->ConfirmHitscan(StartA, EndA, TimeAtA, nullptr).bConfirmed);
	TestFalse(TEXT("과거 위치 히트스캔은 현재 시각에서 거부"), Rewind->ConfirmHitscan(StartA, EndA, TimeAtB, nullptr).bConfirmed);

	const FVector StartB = PositionB + FVector(-800.0, 0.0, 0.0);
	const FVector EndB = PositionB + FVector(800.0, 0.0, 0.0);
	TestTrue(TEXT("현재 위치 히트스캔 확정"), Rewind->ConfirmHitscan(StartB, EndB, TimeAtB, nullptr).bConfirmed);
	TestFalse(TEXT("기록 범위 밖 시각 거부"), Rewind->ConfirmHitscan(StartB, EndB, TimeAtB + 5.0, nullptr).bConfirmed);
	TestTrue(TEXT("판정 뒤 박스 위치 복구"), Rewind->ConfirmHitscan(StartB, EndB, TimeAtB, nullptr).bConfirmed);

	// 투사체 경로가 대상 자신을 제외하면 이 검사가 실패합니다.
	const FVector ProjectileStart = PositionB + FVector(-500.0, 0.0, 0.0);
	TestTrue(TEXT("대상 방향 투사체 확정"),
		Rewind->ConfirmProjectile(ProjectileStart, FVector(3000.0, 0.0, 0.0), TimeAtB, nullptr).bConfirmed);
	TestFalse(TEXT("반대 방향 투사체 거부"),
		Rewind->ConfirmProjectile(ProjectileStart, FVector(-3000.0, 0.0, 0.0), TimeAtB, nullptr).bConfirmed);

	// 리와인드 채널은 벽을 무시하므로 Visibility 검사가 벽 너머 명중을 막아야 합니다.
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (TestNotNull(TEXT("벽 메시"), Cube))
	{
		AStaticMeshActor* Wall = World->SpawnActor<AStaticMeshActor>(PositionB + FVector(-250.0, 0.0, 0.0), FRotator::ZeroRotator);
		UStaticMeshComponent* WallMesh = Wall->GetStaticMeshComponent();
		WallMesh->SetMobility(EComponentMobility::Movable);
		WallMesh->SetStaticMesh(Cube);
		WallMesh->SetCollisionProfileName(TEXT("BlockAll"));
		Wall->SetActorScale3D(FVector(0.5, 4.0, 4.0));
		TickWorld(World, 1);
		const double TimeWithWall = World->GetTimeSeconds();

		TestFalse(TEXT("벽 너머 히트스캔 거부"), Rewind->ConfirmHitscan(StartB, EndB, TimeWithWall, nullptr).bConfirmed);
		TestFalse(TEXT("벽 너머 투사체 거부"),
			Rewind->ConfirmProjectile(ProjectileStart, FVector(3000.0, 0.0, 0.0), TimeWithWall, nullptr).bConfirmed);

		const FVector SideStart = PositionB + FVector(0.0, -800.0, 0.0);
		const FVector SideEnd = PositionB + FVector(0.0, 800.0, 0.0);
		TestTrue(TEXT("열린 쪽 히트스캔 확정"), Rewind->ConfirmHitscan(SideStart, SideEnd, TimeWithWall, nullptr).bConfirmed);

		TArray<FVector> PelletTraceEnds;
		PelletTraceEnds.Add(SideEnd);
		PelletTraceEnds.Add(SideEnd + FVector(0.0, 0.0, 30.0));
		PelletTraceEnds.Add(SideEnd + FVector(0.0, 0.0, 600.0));
		const FShotgunRewindResult Shotgun = Rewind->ConfirmShotgun(SideStart, PelletTraceEnds, TimeWithWall, nullptr);
		TestEqual(TEXT("대상에 닿은 펠릿만 집계"), Shotgun.BodyHitCount + Shotgun.CriticalHitCount, 2);
	}

	// GameMode가 없는 테스트 월드라 ApplyDamage 대신 서버 피해 처리기에 직접 전달합니다.
	Target->OnTakeAnyDamage.Broadcast(Target, 1000.0f, GetDefault<UDamageType>(), nullptr, nullptr);
	TestTrue(TEXT("체력은 0 아래로 내려가지 않음"), FMath::IsNearlyZero(Target->GetHealth()));
	TestFalse(TEXT("체력 0이면 사망"), Target->IsAlive());

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

#endif

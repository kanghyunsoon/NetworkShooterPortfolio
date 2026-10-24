// Copyright Kang Hyungsoon. Portfolio source code.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Character/NetworkShooterCharacter.h"
#include "Combat/ValidatedWeaponComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/GameModeBase.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FValidatedWeaponServerReportTest,
	"NetworkShooter.Combat.ServerReport",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

namespace ValidatedWeaponServerReportTest
{
	void TickWorld(UWorld* World, const int32 Frames)
	{
		for (int32 Index = 0; Index < Frames; ++Index)
		{
			// 틱 매니저는 같은 GFrameCounter 안에서 틱 함수를 한 번만 실행합니다.
			++GFrameCounter;
			World->Tick(LEVELTICK_All, 1.0f / 60.0f);
		}
	}
}

bool FValidatedWeaponServerReportTest::RunTest(const FString& Parameters)
{
	using namespace ValidatedWeaponServerReportTest;

	// ApplyDamage는 서버 GameMode가 있어야 피해를 받으므로 GameMode까지 만든 월드에서 확인합니다.
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("ServerReportTest"));
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(World);
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	World->SetGameInstance(GameInstance);
	World->SetGameMode(FURL());
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();
	if (!TestNotNull(TEXT("서버 GameMode"), World->GetAuthGameMode()))
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
		return false;
	}

	const FVector TargetLocation(0.0, 600.0, 100.0);
	const FVector ShooterLocation(-800.0, 600.0, 100.0);
	ANetworkShooterCharacter* Target = World->SpawnActor<ANetworkShooterCharacter>(TargetLocation, FRotator::ZeroRotator);
	ANetworkShooterCharacter* Shooter = World->SpawnActor<ANetworkShooterCharacter>(ShooterLocation, FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("대상"), Target) || !TestNotNull(TEXT("사수"), Shooter))
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
		return false;
	}

	UValidatedWeaponComponent* Weapon = Shooter->GetValidatedWeapon();
	const float BodyDamage = Weapon->BodyDamage;
	TickWorld(World, 10);

	const FVector Eye = Shooter->GetPawnViewLocation();
	const FVector AimDirection = (TargetLocation - Eye).GetSafeNormal();
	const FVector TraceEnd = Eye + AimDirection * 1600.0;
	auto Now = [World]() { return World->GetTimeSeconds(); };
	float ExpectedHealth = Target->GetMaxHealth();

	// 정상 히트스캔은 서버 피해량만큼 체력을 줄입니다.
	Weapon->ServerReportHitscan_Implementation(Target, Eye, TraceEnd, Now());
	ExpectedHealth -= BodyDamage;
	TestEqual(TEXT("정상 히트스캔 피해"), Target->GetHealth(), ExpectedHealth);

	// 같은 발사 재전송, 발사 간격보다 빠른 발사는 거부합니다.
	Weapon->ServerReportHitscan_Implementation(Target, Eye, TraceEnd, Now());
	TestEqual(TEXT("같은 발사 재전송 거부"), Target->GetHealth(), ExpectedHealth);
	TickWorld(World, 1);
	Weapon->ServerReportHitscan_Implementation(Target, Eye, TraceEnd, Now());
	TestEqual(TEXT("연사 속도 초과 거부"), Target->GetHealth(), ExpectedHealth);

	// 핑 정보가 없으면 되돌릴 수 있는 시간은 기본 허용치뿐입니다.
	TickWorld(World, 7);
	Weapon->ServerReportHitscan_Implementation(Target, Eye, TraceEnd, Now() - 1.0);
	TestEqual(TEXT("오래된 요청 시각 거부"), Target->GetHealth(), ExpectedHealth);
	Weapon->ServerReportHitscan_Implementation(Target, Eye, TraceEnd, Now() + 0.5);
	TestEqual(TEXT("미래 요청 시각 거부"), Target->GetHealth(), ExpectedHealth);

	// 사수 눈 위치에서 먼 시작점과 조준 방향을 벗어난 사격은 거부합니다.
	const FVector FarStart = Eye + FVector(400.0, 0.0, 0.0);
	Weapon->ServerReportHitscan_Implementation(Target, FarStart, FarStart + AimDirection * 1600.0, Now());
	TestEqual(TEXT("먼 발사 시작점 거부"), Target->GetHealth(), ExpectedHealth);
	Weapon->ServerReportHitscan_Implementation(Target, Eye, Eye + FVector(0.0, 1600.0, 0.0), Now());
	TestEqual(TEXT("조준 방향 벗어난 사격 거부"), Target->GetHealth(), ExpectedHealth);
	Weapon->ServerReportHitscan_Implementation(Shooter, Eye, TraceEnd, Now());
	TestEqual(TEXT("자기 자신 대상 거부"), Shooter->GetHealth(), Shooter->GetMaxHealth());

	// 간격을 지킨 다음 발사는 다시 들어갑니다.
	TickWorld(World, 7);
	Weapon->ServerReportHitscan_Implementation(Target, Eye, TraceEnd, Now());
	ExpectedHealth -= BodyDamage;
	TestEqual(TEXT("간격을 지킨 다음 발사"), Target->GetHealth(), ExpectedHealth);

	// 벽이 막으면 리와인드 명중이어도 피해가 없습니다.
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	AStaticMeshActor* Wall = nullptr;
	if (TestNotNull(TEXT("벽 메시"), Cube))
	{
		Wall = World->SpawnActor<AStaticMeshActor>(FVector(-400.0, 600.0, 120.0), FRotator::ZeroRotator);
		UStaticMeshComponent* WallMesh = Wall->GetStaticMeshComponent();
		WallMesh->SetMobility(EComponentMobility::Movable);
		WallMesh->SetStaticMesh(Cube);
		WallMesh->SetCollisionProfileName(TEXT("BlockAll"));
		Wall->SetActorScale3D(FVector(0.5, 4.0, 4.0));
		TickWorld(World, 7);
		Weapon->ServerReportHitscan_Implementation(Target, Eye, TraceEnd, Now());
		TestEqual(TEXT("벽 너머 사격 거부"), Target->GetHealth(), ExpectedHealth);
		Wall->Destroy();
	}

	// 산탄은 대상에 닿은 펠릿 수만큼 피해를 줍니다.
	TickWorld(World, 7);
	TArray<FVector_NetQuantize> Pellets;
	Pellets.Add(Eye + (TargetLocation - Eye).GetSafeNormal() * 1600.0);
	Pellets.Add(Eye + (TargetLocation + FVector(0.0, 0.0, 25.0) - Eye).GetSafeNormal() * 1600.0);
	Pellets.Add(Eye + (TargetLocation + FVector(0.0, 0.0, 400.0) - Eye).GetSafeNormal() * 1600.0);
	Weapon->ServerReportShotgun_Implementation(Target, Eye, Pellets, Now());
	ExpectedHealth -= BodyDamage * 2.0f;
	TestEqual(TEXT("산탄 펠릿 피해 합산"), Target->GetHealth(), ExpectedHealth);

	// 투사체는 중력으로 떨어지는 만큼 조금 높게 겨눕니다.
	TickWorld(World, 7);
	const FVector ProjectileVelocity = (TargetLocation + FVector(0.0, 0.0, 30.0) - Eye).GetSafeNormal() * 3000.0;
	Weapon->ServerReportProjectile_Implementation(Target, Eye, ProjectileVelocity, Now());
	ExpectedHealth = FMath::Max(0.0f, ExpectedHealth - BodyDamage);
	TestEqual(TEXT("투사체 피해"), Target->GetHealth(), ExpectedHealth);
	TestFalse(TEXT("체력 0이면 사망"), Target->IsAlive());

	// 쓰러진 대상은 더 이상 판정하지 않고 발사 간격도 소비하지 않습니다.
	TickWorld(World, 7);
	Weapon->ServerReportHitscan_Implementation(Target, Eye, TraceEnd, Now());
	TestEqual(TEXT("쓰러진 대상 거부"), Target->GetHealth(), 0.0f);

	// 쓰러진 사수의 보고도 거부합니다. 같은 자리에 새 대상을 두려면 먼저 이전 대상을 없앱니다.
	Target->Destroy();
	TickWorld(World, 1);
	ANetworkShooterCharacter* SecondTarget = World->SpawnActor<ANetworkShooterCharacter>(TargetLocation, FRotator::ZeroRotator);
	if (TestNotNull(TEXT("새 대상"), SecondTarget))
	{
		TestTrue(TEXT("새 대상이 조준선 위에 있음"), SecondTarget->GetActorLocation().Equals(TargetLocation, 1.0));
		TickWorld(World, 7);
		Weapon->ServerReportHitscan_Implementation(SecondTarget, Eye, TraceEnd, Now());
		TestEqual(TEXT("새 대상은 맞음"), SecondTarget->GetHealth(), SecondTarget->GetMaxHealth() - BodyDamage);

		Shooter->OnTakeAnyDamage.Broadcast(Shooter, 1000.0f, GetDefault<UDamageType>(), nullptr, nullptr);
		TickWorld(World, 7);
		Weapon->ServerReportHitscan_Implementation(SecondTarget, Eye, TraceEnd, Now());
		TestEqual(TEXT("쓰러진 사수 거부"), SecondTarget->GetHealth(), SecondTarget->GetMaxHealth() - BodyDamage);
	}

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

#endif


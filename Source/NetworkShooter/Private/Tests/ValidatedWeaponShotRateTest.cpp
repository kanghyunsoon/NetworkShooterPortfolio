// Copyright Kang Hyungsoon. Portfolio source code.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/ValidatedWeaponComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FValidatedWeaponShotRateTest,
	"NetworkShooter.Combat.ShotRate",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FValidatedWeaponShotRateTest::RunTest(const FString& Parameters)
{
	UValidatedWeaponComponent* Weapon = NewObject<UValidatedWeaponComponent>();
	const AActor* TargetA = GetDefault<ACharacter>();
	const AActor* TargetB = GetDefault<APawn>();
	constexpr EReportedShotKind Hitscan = EReportedShotKind::Hitscan;
	constexpr EReportedShotKind Shotgun = EReportedShotKind::Shotgun;

	TestTrue(TEXT("첫 발사"), Weapon->ConsumeShot(TargetA, 10.0, 1, Hitscan));
	TestFalse(TEXT("같은 발사로 같은 대상 재보고"), Weapon->ConsumeShot(TargetA, 10.0, 1, Hitscan));
	TestFalse(TEXT("히트스캔 한 발로 두 번째 대상"), Weapon->ConsumeShot(TargetB, 10.0, 1, Hitscan));
	TestFalse(TEXT("발사 간격보다 빠름"), Weapon->ConsumeShot(TargetA, 10.05, 1, Hitscan));
	TestTrue(TEXT("발사 간격과 같음"), Weapon->ConsumeShot(TargetA, 10.1, 1, Hitscan));
	TestFalse(TEXT("이전 시각"), Weapon->ConsumeShot(TargetA, 9.0, 1, Hitscan));

	// 프레임 오차로 한 번 짧은 간격은 허용하고, 계속 짧으면 거부합니다.
	TestTrue(TEXT("휴식 뒤 발사"), Weapon->ConsumeShot(TargetA, 12.0, 1, Hitscan));
	TestTrue(TEXT("한 번 짧은 간격 허용"), Weapon->ConsumeShot(TargetA, 12.09, 1, Hitscan));
	TestFalse(TEXT("연속으로 짧은 간격 거부"), Weapon->ConsumeShot(TargetA, 12.18, 1, Hitscan));
	TestTrue(TEXT("정상 간격 복귀"), Weapon->ConsumeShot(TargetA, 12.30, 1, Hitscan));

	// 0.09초 간격으로 10초 동안 보내도 평균 0.1초 간격을 넘지 못합니다.
	int32 Accepted = 0;
	for (int32 Index = 1; Index <= 111; ++Index)
	{
		Accepted += Weapon->ConsumeShot(TargetA, 20.0 + Index * 0.09, 1, Hitscan) ? 1 : 0;
	}
	TestTrue(FString::Printf(TEXT("평균 연사 속도 제한 (%d발 허용)"), Accepted), Accepted <= 101);

	TestTrue(TEXT("산탄 대상 A"), Weapon->ConsumeShot(TargetA, 40.0, 2, Shotgun));
	TestTrue(TEXT("같은 산탄 발사의 대상 B"), Weapon->ConsumeShot(TargetB, 40.0, 2, Shotgun));
	TestFalse(TEXT("같은 산탄 발사로 대상 A 재보고"), Weapon->ConsumeShot(TargetA, 40.0, 2, Shotgun));

	TestTrue(TEXT("히트스캔 발사"), Weapon->ConsumeShot(TargetA, 50.0, 1, Hitscan));
	TestFalse(TEXT("같은 시각 산탄 보고로 대상 추가 거부"), Weapon->ConsumeShot(TargetB, 50.0, 4, Shotgun));

	TestFalse(TEXT("소유자 없는 조준 검사 거부"), Weapon->IsAimPlausible(FVector::ForwardVector));
	TestEqual(TEXT("핑 정보가 없으면 기본 허용 시간만 되돌림"),
		Weapon->GetMaximumRewindSeconds(), static_cast<double>(Weapon->RewindAllowanceSeconds));
	return true;
}

#endif

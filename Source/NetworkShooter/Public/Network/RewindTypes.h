// Copyright Kang Hyungsoon. Portfolio source code.

#pragma once

#include "CoreMinimal.h"
#include "RewindTypes.generated.h"

/** 한 시점의 충돌 박스 위치와 크기입니다. */
USTRUCT(BlueprintType)
struct NETWORKSHOOTER_API FRewindHitBoxState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FName Name = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector Location = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector Extent = FVector::ZeroVector;
};

/** 서버가 기록한 캐릭터의 한 프레임입니다. */
USTRUCT(BlueprintType)
struct NETWORKSHOOTER_API FRewindFrame
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	double ServerTimeSeconds = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TArray<FRewindHitBoxState> HitBoxes;
};

/** 서버 사이드 리와인드 판정 결과입니다. */
USTRUCT(BlueprintType)
struct NETWORKSHOOTER_API FRewindCheckResult
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bConfirmed = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bCritical = false;
};

/** 산탄 한 발을 구성하는 펠릿의 판정 집계입니다. */
USTRUCT(BlueprintType)
struct NETWORKSHOOTER_API FShotgunRewindResult
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 BodyHitCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 CriticalHitCount = 0;
};

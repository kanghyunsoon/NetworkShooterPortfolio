// Copyright Kang Hyungsoon. Portfolio source code.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "NetworkShooterCharacter.generated.h"

class UBoxComponent;
class URewindHistoryComponent;
class UValidatedWeaponComponent;

/** 리와인드 충돌 박스와 서버 권한 체력을 포함한 예제 캐릭터입니다. */
UCLASS()
class NETWORKSHOOTER_API ANetworkShooterCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ANetworkShooterCharacter();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="Character|Health")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category="Character|Health")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category="Network|Rewind")
	URewindHistoryComponent* GetRewindHistory() const { return RewindHistory; }

	UFUNCTION(BlueprintPure, Category="Combat|Validation")
	UValidatedWeaponComponent* GetValidatedWeapon() const { return ValidatedWeapon; }

protected:
	/** UI나 이펙트는 Blueprint에서 이 이벤트를 받아 갱신합니다. */
	UFUNCTION(BlueprintImplementableEvent, Category="Character|Health")
	void OnHealthChanged(float OldHealth, float NewHealth);

private:
	UFUNCTION()
	void HandleDamage(
		AActor* DamagedActor,
		float Damage,
		const UDamageType* DamageType,
		AController* InstigatedBy,
		AActor* DamageCauser
	);

	UFUNCTION()
	void OnRep_Health(float OldHealth);

	UPROPERTY(VisibleAnywhere, Category="Network|Rewind")
	TObjectPtr<URewindHistoryComponent> RewindHistory;

	UPROPERTY(VisibleAnywhere, Category="Combat|Validation")
	TObjectPtr<UValidatedWeaponComponent> ValidatedWeapon;

	UPROPERTY(VisibleAnywhere, Category="Network|Rewind")
	TObjectPtr<UBoxComponent> HeadHitBox;

	UPROPERTY(VisibleAnywhere, Category="Network|Rewind")
	TObjectPtr<UBoxComponent> TorsoHitBox;

	UPROPERTY(EditDefaultsOnly, Category="Character|Health", meta=(ClampMin="1.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(ReplicatedUsing=OnRep_Health, VisibleInstanceOnly, Category="Character|Health")
	float Health = 100.0f;
};

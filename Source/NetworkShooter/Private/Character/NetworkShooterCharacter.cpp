// Copyright Kang Hyungsoon. Portfolio source code.

#include "Character/NetworkShooterCharacter.h"

#include "Combat/ValidatedWeaponComponent.h"
#include "Components/BoxComponent.h"
#include "Net/UnrealNetwork.h"
#include "Network/RewindHistoryComponent.h"

ANetworkShooterCharacter::ANetworkShooterCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	RewindHistory = CreateDefaultSubobject<URewindHistoryComponent>(TEXT("RewindHistory"));
	ValidatedWeapon = CreateDefaultSubobject<UValidatedWeaponComponent>(TEXT("ValidatedWeapon"));

	HeadHitBox = CreateDefaultSubobject<UBoxComponent>(TEXT("HeadHitBox"));
	HeadHitBox->SetupAttachment(GetMesh(), TEXT("head"));
	HeadHitBox->SetBoxExtent(FVector(16.0f, 16.0f, 16.0f));
	HeadHitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	TorsoHitBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TorsoHitBox"));
	TorsoHitBox->SetupAttachment(GetMesh(), TEXT("spine_03"));
	TorsoHitBox->SetBoxExtent(FVector(24.0f, 18.0f, 35.0f));
	TorsoHitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ANetworkShooterCharacter::BeginPlay()
{
	Super::BeginPlay();

	Health = MaxHealth;
	RewindHistory->RegisterHitBox(TEXT("Head"), HeadHitBox, true);
	RewindHistory->RegisterHitBox(TEXT("Torso"), TorsoHitBox, false);

	if (HasAuthority())
	{
		OnTakeAnyDamage.AddDynamic(this, &ANetworkShooterCharacter::HandleDamage);
	}
}

void ANetworkShooterCharacter::HandleDamage(
	AActor* DamagedActor,
	const float Damage,
	const UDamageType* DamageType,
	AController* InstigatedBy,
	AActor* DamageCauser
)
{
	if (!HasAuthority() || Damage <= 0.0f || Health <= 0.0f)
	{
		return;
	}

	const float OldHealth = Health;
	Health = FMath::Clamp(Health - Damage, 0.0f, MaxHealth);
	OnHealthChanged(OldHealth, Health);
}

void ANetworkShooterCharacter::OnRep_Health(const float OldHealth)
{
	OnHealthChanged(OldHealth, Health);
}

void ANetworkShooterCharacter::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANetworkShooterCharacter, Health);
}

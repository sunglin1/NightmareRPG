// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "EnemyBase.generated.h"

UCLASS()
class NIGHTMARERPG_API AEnemyBase : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AEnemyBase();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Enemy Stats
	UPROPERTY(EditdefaultsOnly, BlueprintReadOnly, Category = "Enemy Stats")
	float MaxHP = 100.f;

	UPROPERTY(EditdefaultsOnly, BlueprintReadOnly, Category = "Enemy Stats")
	float CurrentHP;

	UPROPERTY(EditdefaultsOnly, BlueprintReadOnly, Category = "Enemy Stats")
	float AttackDamage = 10.f;

	UPROPERTY(EditdefaultsOnly, BlueprintReadOnly, Category = "Enemy State")
	bool IsDead = false;

	virtual void Die();

public:	
	virtual float TakeDamage(
		float DamageAmount, 
		struct FDamageEvent const& DamageEvent, 
		class AController* EventInstigator, 
		AActor* DamageCauser
	) override;

};

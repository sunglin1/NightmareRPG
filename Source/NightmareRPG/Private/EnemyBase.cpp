// Fill out your copyright notice in the Description page of Project Settings.


#include "EnemyBase.h"

// Sets default values
AEnemyBase::AEnemyBase()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AEnemyBase::BeginPlay()
{
	Super::BeginPlay();

	CurrentHP = MaxHP; // 체력 초기화
	
}

//적이 데미지를 받았을 때
float AEnemyBase::TakeDamage(
    float DamageAmount,
    FDamageEvent const& DamageEvent,
    AController* EventInstigator,
    AActor* DamageCauser)
{
	// 실제 데미지 계산
    if (IsDead || DamageAmount <= 0.f) 
        return 0.f;

    const float ActualDamage = FMath::Min(
        CurrentHP, DamageAmount
    );

    CurrentHP = FMath::Clamp( 
        CurrentHP - ActualDamage,
        0.f,
        MaxHP
    );

    if (CurrentHP <= 0.f) // 죽음 처리
    {
        Die();
    }

    return ActualDamage;
}

void AEnemyBase::Die()
{
    if (IsDead)
        return;

    IsDead = true;

	//timer 제거, 이동 중지 등 처리
    GetWorldTimerManager().ClearAllTimersForObject(this);

    if (GetController())
    {
        GetController()->StopMovement();
    }

    SetActorEnableCollision(false);

    SetLifeSpan(2.f);
}


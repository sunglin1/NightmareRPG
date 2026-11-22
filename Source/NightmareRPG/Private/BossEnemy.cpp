
#include "BossEnemy.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"

ABossEnemy::ABossEnemy()
{
    MaxHP = 1000.f;
    AttackDamage = 30.f;
    AttackRange = 200.f;
    AttackCooldown = 2.5f;
}

void ABossEnemy::BeginPlay()
{
    Super::BeginPlay();
}

void ABossEnemy::Attack(AActor* Target)
{
    if (!Target || IsDead)
    {
        return;
    }

    const float CurrentTime = GetWorld()->GetTimeSeconds();

    if (SpecialAttackMontage &&
        CurrentTime - LastSpecialAttackTime >= SpecialAttackCooldown)
    {
        SpecialAttack(Target);
        LastSpecialAttackTime = CurrentTime;
    }
    else
    {
        Super::Attack(Target);
    }
}


void ABossEnemy::SpecialAttack(AActor* Target)
{
    if (!Target || IsDead)
    {
        return;
    }

    // 특수 공격 애니메이션 재생
    if (SpecialAttackMontage)
    {
        PlayAnimMontage(SpecialAttackMontage);
    }

    // 공격 범위 확인
    const float Distance = FVector::Dist(
        GetActorLocation(),
        Target->GetActorLocation()
    );

    if (Distance > AttackRange)
    {
        return;
    }

    // 특수 공격 데미지 적용
    UGameplayStatics::ApplyDamage(
        Target,
        SpecialAttackDamage,
        GetController(),
        this,
        UDamageType::StaticClass()
    );

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("Boss Special Attack Damage: %.1f"),
        SpecialAttackDamage
    );
}


float ABossEnemy::TakeDamage(
    float DamageAmount,
    const FDamageEvent& DamageEvent,
    AController* EventInstigator,
    AActor* DamageCauser
)
{
    const float ActualDamage = Super::TakeDamage(
        DamageAmount,
        DamageEvent,
        EventInstigator,
        DamageCauser
    );

    if (IsDead || bHasTriggeredDaydream)
    {
        return ActualDamage;
    }

    if (MaxHP > 0.f &&
        CurrentHP / MaxHP <= DaydreamHPThreshold)
    {
        StartDaydreamPhase();
    }

    return ActualDamage;
}

void ABossEnemy::StartDaydreamPhase()
{
    if (bHasTriggeredDaydream || IsDead)
    {
        return;
    }

    bHasTriggeredDaydream = true;
    bIsDaydreamPhase = true;

    LucidityStacks = 0;
    bIsPlayerAwakened = false;

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("Boss Daydream Phase Started!")
    );

    OnDaydreamStarted();
    OnLucidityStackChanged(LucidityStacks);
}

void ABossEnemy::AddLucidityStack()
{
    // 백일몽 중이 아니거나 보스가 죽었다면 무시
    if (!bIsDaydreamPhase || IsDead)
    {
        return;
    }

    // 이미 자각 상태라면 추가 증가하지 않음
    if (bIsPlayerAwakened)
    {
        return;
    }

    LucidityStacks = FMath::Clamp(
        LucidityStacks + 1,
        0,
        FMath::Max(1, MaxLucidityStacks)
    );

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("Lucidity Stack: %d / %d"),
        LucidityStacks,
        MaxLucidityStacks
    );

    OnLucidityStackChanged(LucidityStacks);


    // 백일몽 시작 후 30초마다 자각 스택 증가
    GetWorldTimerManager().SetTimer(
        LucidityTimerHandle,
        this,
        &ABossEnemy::GainTimedLucidityStack,
        30.0f,
        true
    );

    // 최대 스택 달성
    if (LucidityStacks >= FMath::Max(1, MaxLucidityStacks))
    {
        bIsPlayerAwakened = true;

        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Lucidity Complete! Daydream Ending!")
        );

        OnPlayerAwakened();
        EndDaydreamPhase();
    }

}


void ABossEnemy::GainTimedLucidityStack()
{
    if (IsDead || !bIsDaydreamPhase || bIsPlayerAwakened)
    {
        GetWorldTimerManager().ClearTimer(LucidityTimerHandle);
        return;
    }

    AddLucidityStack();

    // 3스택이 되면 타이머 종료
    if (bIsPlayerAwakened)
    {
        GetWorldTimerManager().ClearTimer(LucidityTimerHandle);
    }
}


void ABossEnemy::EndDaydreamPhase()
{
    if (!bIsDaydreamPhase)
    {
        return;
    }

    bIsDaydreamPhase = false;

    // 30초마다 증가하는 스택 타이머 종료
    GetWorldTimerManager().ClearTimer(LucidityTimerHandle);

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("Boss Daydream Phase Ended!")
    );

    OnDaydreamEnded();
}

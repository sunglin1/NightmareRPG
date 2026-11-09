#include "EnemyBase.h"

#include "AIController.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "TimerManager.h"

AEnemyBase::AEnemyBase()
{
    PrimaryActorTick.bCanEverTick = false;
}

void AEnemyBase::BeginPlay()
{
    Super::BeginPlay();

    CurrentHP = MaxHP;
}

// 공격
void AEnemyBase::Attack(AActor* Target)
{
    if (!Target || IsDead)
    {
        return;
    }

    PlayRandomMontage(AttackMontages);

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("Enemy Attack! Damage: %f"),
        AttackDamage
    );
}

// 데미지를 받았을 때
float AEnemyBase::TakeDamage(
    float DamageAmount,
    FDamageEvent const& DamageEvent,
    AController* EventInstigator,
    AActor* DamageCauser
)
{
    if (IsDead || DamageAmount <= 0.f)
    {
        return 0.f;
    }

    const float ActualDamage = FMath::Min(CurrentHP, DamageAmount);

    CurrentHP = FMath::Clamp(
        CurrentHP - ActualDamage,
        0.f,
        MaxHP
    );

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("Enemy Hit! HP: %.1f / %.1f"),
        CurrentHP,
        MaxHP
    );

    if (CurrentHP <= 0.f)
    {
        // 사망 시 피격 애니메이션 대신 사망 애니메이션
        Die();
    }
    else
    {
        // 살아 있다면 피격 애니메이션 랜덤 재생
        PlayRandomMontage(HitMontages);
    }

    return ActualDamage;
}

// 사망
void AEnemyBase::Die()
{
    if (IsDead)
    {
        return;
    }

    IsDead = true;

    // 타이머 중지
    GetWorldTimerManager().ClearAllTimersForObject(this);

    // AI 이동 중지
    if (AAIController* AIController = Cast<AAIController>(GetController()))
    {
        AIController->StopMovement();
    }

    // 사망 애니메이션 재생
    float DeathDuration = 0.f;

    if (DeathMontage)
    {
        DeathDuration = PlayAnimMontage(DeathMontage);
    }

    // 죽은 적의 충돌 비활성화
    SetActorEnableCollision(false);

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("Enemy Died!")
    );

    // 사망 애니메이션이 끝난 후 제거
    const float DestroyDelay =
        DeathDuration > 0.f ? DeathDuration + 0.5f : 2.f;

    SetLifeSpan(DestroyDelay);
}

// 랜덤 애니메이션 재생
void AEnemyBase::PlayRandomMontage(
    const TArray<TObjectPtr<UAnimMontage>>& Montages
)
{
    if (Montages.IsEmpty())
    {
        return;
    }

    const int32 RandomIndex = FMath::RandRange(
        0,
        Montages.Num() - 1
    );

    UAnimMontage* SelectedMontage = Montages[RandomIndex].Get();

    if (SelectedMontage)
    {
        PlayAnimMontage(SelectedMontage);
    }
}
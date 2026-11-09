#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "EnemyBase.generated.h"

class UAnimMontage;

UCLASS()
class NIGHTMARERPG_API AEnemyBase : public ACharacter
{
    GENERATED_BODY()

public:
    AEnemyBase();

    // 적이 데미지를 받을 때
    virtual float TakeDamage(
        float DamageAmount,
        struct FDamageEvent const& DamageEvent,
        class AController* EventInstigator,
        AActor* DamageCauser
    ) override;

    // 공격
    virtual void Attack(AActor* Target);

    // AIController에서 사용할 Getter
    float GetAttackRange() const { return AttackRange; }
    float GetAttackCooldown() const { return AttackCooldown; }
    bool GetIsDead() const { return IsDead; }

protected:
    virtual void BeginPlay() override;

    // ===== Enemy Stats =====

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Stats")
    float MaxHP = 100.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy Stats")
    float CurrentHP = 100.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Stats")
    float AttackDamage = 10.f;

    // ===== Enemy Combat =====

    // 공격 가능한 거리
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Combat")
    float AttackRange = 120.f;

    // 공격 간격 (초)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Combat")
    float AttackCooldown = 1.5f;

    // ===== Enemy State =====

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy State")
    bool IsDead = false;

    // 랜덤 공격 애니메이션
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Animation")
    TArray<TObjectPtr<UAnimMontage>> AttackMontages;

    // 랜덤 피격 애니메이션
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Animation")
    TArray<TObjectPtr<UAnimMontage>> HitMontages;

    // 사망 애니메이션
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Animation")
    TObjectPtr<UAnimMontage> DeathMontage;

    virtual void Die();

    // 배열에서 랜덤 Montage를 재생하는 공통 함수
    void PlayRandomMontage(
        const TArray<TObjectPtr<UAnimMontage>>& Montages
    );
};
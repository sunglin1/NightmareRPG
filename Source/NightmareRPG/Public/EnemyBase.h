#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "EnemyBase.generated.h"

class UAnimMontage;
class UNiagaraSystem;
class USoundBase;
class UMaterialInterface;
class AEnemyBase;

// 템플릿 CombatEnemy.h의 FOnEnemyDied와 이름 충돌 방지
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemyBaseDied, AEnemyBase*, Enemy);

UCLASS()
class NIGHTMARERPG_API AEnemyBase : public ACharacter
{
    GENERATED_BODY()

public:
    AEnemyBase();

    // 적이 데미지를 받을 때 (Belica 사격은 ApplyPointDamage → FPointDamageEvent로 들어옴)
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
    void SetHasAggro(bool bNewAggro) { bHasAggro = bNewAggro; }

    // 공격/접촉 시 플레이어에게 주는 데미지
    float GetAttackDamage() const { return AttackDamage; }

    // 피격 경직 중이면 AI가 이동/공격을 멈춤
    bool IsStaggered() const;

    // 한 번이라도 피격되면 감지 거리 밖에서도 플레이어를 추적
    bool IsProvoked() const { return bProvoked; }

    // 사망 시 호출 (구역 진행 SectionProgression 등이 구독)
    UPROPERTY(BlueprintAssignable, Category = "Enemy State")
    FOnEnemyBaseDied OnEnemyDied;

protected:
    virtual void BeginPlay() override;
    

    // ===== Enemy Stats =====

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Stats")
    float MaxHP = 100.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy Stats")
    float CurrentHP = 100.f;

    // 공격 및 접촉 시 Belica에게 주는 데미지 (Belica 쪽 1.2초 피격 쿨다운 적용)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Stats")
    float AttackDamage = 20.f;

    // 머리(head/neck) 피격 시 데미지 배율
    // 1.0 = 부위와 관계없이 항상 10발(10 x 10 데미지)에 처치. 헤드샷 보너스를 원하면 BP에서 올림
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Stats")
    float HeadshotMultiplier = 1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy State")
    bool bHasAggro = false;

    // ===== Enemy Combat =====

    // 공격 가능한 거리
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Combat")
    float AttackRange = 120.f;

    // 공격 간격 (초)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Combat")
    float AttackCooldown = 1.5f;

    // ===== Hit Feedback =====

    // 피격 시 이동을 멈추는 경직 시간 (초)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Hit Feedback")
    float HitStaggerTime = 0.4f;

    // 사격 방향으로 밀려나는 힘
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Hit Feedback")
    float HitKnockback = 220.f;

    // 피격 지점에 생성되는 이펙트
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Hit Feedback")
    TObjectPtr<UNiagaraSystem> HitEffect;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Hit Feedback")
    TObjectPtr<USoundBase> HitSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Hit Feedback")
    TObjectPtr<USoundBase> DeathSound;

    // 사망 애니메이션이 끝난 뒤 시체가 남아 있는 시간 (초). 이후 액터 제거
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Hit Feedback")
    float CorpseLifetime = 1.5f;

    // 피격 시 메시 위에 덮어 씌우는 빨간 오버레이 머티리얼 (기본: /Game/Combat/Feedback/M_HitFlash)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Hit Feedback")
    TObjectPtr<UMaterialInterface> HitFlashMaterial;

    // 빨간 플래시 + 스케일 펄스 유지 시간 (초)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Hit Feedback")
    float HitFlashDuration = 0.1f;

    // 피격 순간 메시 스케일 배율 (1.0 = 펄스 없음)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Hit Feedback")
    float HitPulseScale = 1.08f;

    // ===== Enemy State =====

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy State")
    bool IsDead = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy State")
    bool bProvoked = false;

    // 랜덤 공격 애니메이션
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Animation")
    TArray<TObjectPtr<UAnimMontage>> AttackMontages;

    // 랜덤 피격 애니메이션
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Animation")
    TArray<TObjectPtr<UAnimMontage>> HitMontages;

    // 사망 애니메이션
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy Animation")
    TObjectPtr<UAnimMontage> DeathMontage;

    // ShotDirection: 사망 몽타주가 없을 때 래그돌에 가할 방향
    virtual void Die(const FVector& ShotDirection = FVector::ZeroVector);

    // 사망 몽타주 마지막 포즈에서 고정 (블렌드아웃으로 다시 일어서는 현상 방지)
    void FreezeCorpse();

    // 피격 플래시 시작 / 종료
    void StartHitFlash();
    void EndHitFlash();

    // 배열에서 랜덤 Montage를 재생하는 공통 함수
    void PlayRandomMontage(
        const TArray<TObjectPtr<UAnimMontage>>& Montages
    );

private:
    float StaggerEndTime = -1.f;
    FTimerHandle FreezeCorpseTimerHandle;
    FTimerHandle HitFlashTimerHandle;

    // 플래시 전 원래 오버레이 머티리얼 / 메시 스케일 (복원용)
    UPROPERTY()
    TObjectPtr<UMaterialInterface> SavedOverlayMaterial;
    FVector BaseMeshScale = FVector::OneVector;
    bool bHitFlashActive = false;
};

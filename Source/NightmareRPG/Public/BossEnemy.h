
#pragma once

#include "CoreMinimal.h"
#include "EnemyBase.h"
#include "BossEnemy.generated.h"

class UAnimMontage;

UCLASS()
class NIGHTMARERPG_API ABossEnemy : public AEnemyBase
{
    GENERATED_BODY()

public:
    ABossEnemy();

    virtual void Attack(AActor* Target) override;

    virtual float TakeDamage(
        float DamageAmount,
        const FDamageEvent& DamageEvent,
        AController* EventInstigator,
        AActor* DamageCauser
    ) override;

    // 방어 또는 회피 성공 시 호출
    UFUNCTION(BlueprintCallable, Category = "Boss Daydream")
    void AddLucidityStack();

    UFUNCTION(BlueprintPure, Category = "Boss Daydream")
    bool IsDaydreamActive() const
    {
        return bIsDaydreamPhase;
    }

    UFUNCTION(BlueprintPure, Category = "Boss Daydream")
    bool IsPlayerAwakened() const
    {
        return bIsPlayerAwakened;
    }

protected:
    virtual void BeginPlay() override;

    // 특수 공격
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss Combat")
    TObjectPtr<UAnimMontage> SpecialAttackMontage;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss Combat")
    float SpecialAttackDamage = 60.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss Combat")
    float SpecialAttackCooldown = 10.f;

    // 백일몽
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss Daydream")
    bool bIsDaydreamPhase = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss Daydream")
    float DaydreamHPThreshold = 0.5f;

    // 자각 스택
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss Daydream")
    int32 LucidityStacks = 0;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss Daydream")
    int32 MaxLucidityStacks = 3;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss Daydream")
    bool bIsPlayerAwakened = false;

    // 백일몽 시작 이벤트
    UFUNCTION(BlueprintImplementableEvent, Category = "Boss Daydream")
    void OnDaydreamStarted();

    // 스택 변경 이벤트
    UFUNCTION(BlueprintImplementableEvent, Category = "Boss Daydream")
    void OnLucidityStackChanged(int32 NewStacks);

    // 3스택 달성 이벤트
    UFUNCTION(BlueprintImplementableEvent, Category = "Boss Daydream")
    void OnPlayerAwakened();

	// 백일몽 종료 이벤트
	UFUNCTION(BlueprintImplementableEvent, Category = "Boss Daydream")
	void OnDaydreamEnded();


private:
    float LastSpecialAttackTime = 0.f;
    bool bHasTriggeredDaydream = false;

    void SpecialAttack(AActor* Target);
    void StartDaydreamPhase();

	FTimerHandle LucidityTimerHandle;

	void GainTimedLucidityStack();

	void EndDaydreamPhase();

};

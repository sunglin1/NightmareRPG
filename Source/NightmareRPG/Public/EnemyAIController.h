#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "EnemyAIController.generated.h"

UCLASS()
class NIGHTMARERPG_API AEnemyAIController : public AAIController
{
    GENERATED_BODY()

public:
    AEnemyAIController();

    // 매 프레임 실행
    virtual void Tick(float DeltaTime) override;

protected:
    virtual void BeginPlay() override;

    // 플레이어 감지 거리
    UPROPERTY(EditAnywhere, Category = "AI")
    float DetectionRange = 1000.f;

private:
    // 마지막 공격 시간
    float LastAttackTime = -1000.f;
};
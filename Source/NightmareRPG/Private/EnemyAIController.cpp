#include "EnemyAIController.h"
#include "EnemyBase.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"

// 생성자
AEnemyAIController::AEnemyAIController()
{
    // Tick 활성화
    PrimaryActorTick.bCanEverTick = true;
}

void AEnemyAIController::BeginPlay()
{
    Super::BeginPlay();
}

// 매 프레임 실행
void AEnemyAIController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 플레이어 찾기
    APawn* Player = UGameplayStatics::GetPlayerPawn(
        GetWorld(), 0
    );

    // AI가 조종 중인 적 가져오기
    AEnemyBase* Enemy = Cast<AEnemyBase>(GetPawn());

    // 플레이어가 없거나 적이 죽었다면 중단
    if (!Player || !Enemy || Enemy->GetIsDead())
    {
        StopMovement();
        return;
    }

    // 적과 플레이어 사이의 거리
    const float Distance = FVector::Dist(
        Enemy->GetActorLocation(),
        Player->GetActorLocation()
    );

    // 1. 공격 범위 안에 있을 때
    if (Distance <= Enemy->GetAttackRange())
    {
        // 이동 중지
        StopMovement();

        const float CurrentTime = GetWorld()->GetTimeSeconds();

        // 공격 쿨다운 확인
        if (CurrentTime - LastAttackTime >= Enemy->GetAttackCooldown())
        {
            // 공격 실행
            Enemy->Attack(Player);

            // 마지막 공격 시간 갱신
            LastAttackTime = CurrentTime;
        }
    }

    // 2. 공격 범위 밖이지만 감지 범위 안에 있을 때
    else if (Distance <= DetectionRange)
    {
        const EPathFollowingRequestResult::Type Result =
            MoveToActor(Player, 100.f);

        // 로그가 너무 많이 나오지 않도록 1초마다 출력
        static float LastLogTime = 0.f;
        const float CurrentTime = GetWorld()->GetTimeSeconds();

        if (CurrentTime - LastLogTime >= 1.f)
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("Chasing Player | Distance: %.1f | MoveResult: %d"),
                Distance,
                static_cast<int32>(Result)
            );

            LastLogTime = CurrentTime;
        }
    }

    // 3. 감지 범위 밖일 때
    else
    {
        StopMovement();
    }
}
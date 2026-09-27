// Fill out your copyright notice in the Description page of Project Settings.


#include "EnemyAIController.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"


void AEnemyAIController::BeginPlay()
{
    Super::BeginPlay();

    //매 프레임 Tick() 함수 호출
    PrimaryActorTick.bCanEverTick = true;
}

void AEnemyAIController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    APawn* Player = UGameplayStatics::GetPlayerPawn(
        GetWorld(), 0
    );

    APawn* Enemy = GetPawn();

    if (!Player || !Enemy)
        return;

    float Distance = FVector::Dist( // 플레이어와 적 거리 계산
        Enemy->GetActorLocation(),
        Player->GetActorLocation()
    );

    if (Distance <= DetectionRange) //범위 내에 플레이어 존재 시 150 거리 안으로 이동
    {
        MoveToActor(Player, 150.f);
    }
    else
    {
        StopMovement();
    }
}
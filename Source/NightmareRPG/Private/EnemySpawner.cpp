#include "EnemySpawner.h"
#include "Engine/World.h"

AEnemySpawner::AEnemySpawner()
{
    PrimaryActorTick.bCanEverTick = false;
}

void AEnemySpawner::BeginPlay()
{
    Super::BeginPlay();

    if (!EnemyClass)
    {
        UE_LOG(LogTemp, Error, TEXT("EnemyClass가 설정되지 않았습니다!"));
        return;
    }

    FActorSpawnParameters SpawnParams;

    SpawnParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    AActor* SpawnedEnemy = GetWorld()->SpawnActor<AActor>(
        EnemyClass,
        GetActorLocation(),
        GetActorRotation(),
        SpawnParams
    );
}
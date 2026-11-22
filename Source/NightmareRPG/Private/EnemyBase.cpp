#include "EnemyBase.h"

#include "AIController.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/DamageType.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AEnemyBase::AEnemyBase()
{
    PrimaryActorTick.bCanEverTick = false;

    // 기본 피격 이펙트 (BP에서 교체 가능)
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> DamageFX(
        TEXT("/Game/_Reference/Variant_Combat/VFX/NS_Damage.NS_Damage"));
    if (DamageFX.Succeeded())
    {
        HitEffect = DamageFX.Object;
    }

    // 기본 피격 플래시 (빨간 반투명 오버레이, BP에서 교체 가능)
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> HitFlashMat(
        TEXT("/Game/Combat/Feedback/M_HitFlash.M_HitFlash"));
    if (HitFlashMat.Succeeded())
    {
        HitFlashMaterial = HitFlashMat.Object;
    }
}

void AEnemyBase::BeginPlay()
{
    Super::BeginPlay();

    CurrentHP = MaxHP;

    // Belica의 사격/레이저 트레이스(ECC_Visibility)가 적을 맞출 수 있도록
    // 캡슐이 아닌 메시(피직스 에셋 바디)가 막도록 설정 → 본 이름(head 등)으로 부위 판정 가능
    if (USkeletalMeshComponent* MeshComp = GetMesh())
    {
        if (MeshComp->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
        {
            MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        }
        MeshComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

        BaseMeshScale = MeshComp->GetRelativeScale3D();
    }
}

// 공격
void AEnemyBase::Attack(AActor* Target)
{
    if (!Target || IsDead)
    {
        return;
    }

    // 기존 공격 애니메이션 재생
    PlayRandomMontage(AttackMontages);

    // 공격 범위 안의 대상에게 데미지 (Belica::TakeDamage에서 1.2초 쿨다운 처리)
    // 캡슐 반경만큼 여유를 둬서 몸이 맞닿은 상태에서도 확실히 적중
    const float Reach = AttackRange + 50.f;
    if (FVector::DistSquared(GetActorLocation(), Target->GetActorLocation()) <= FMath::Square(Reach))
    {
        UGameplayStatics::ApplyDamage(Target, AttackDamage, GetController(), this, UDamageType::StaticClass());
    }

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("Enemy Attack Damage: %.1f"),
        AttackDamage
    );
}

bool AEnemyBase::IsStaggered() const
{
    const UWorld* World = GetWorld();
    return World && World->GetTimeSeconds() < StaggerEndTime;
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

    // 사격 정보 (피격 위치 / 방향 / 본)
    FVector HitLocation = GetActorLocation();
    FVector HitNormal = FVector::UpVector;
    FVector ShotDirection = FVector::ZeroVector;
    bool bHeadshot = false;

    if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
    {
        const FPointDamageEvent& PointEvent = static_cast<const FPointDamageEvent&>(DamageEvent);
        HitLocation = PointEvent.HitInfo.ImpactPoint;
        HitNormal = PointEvent.HitInfo.ImpactNormal;
        ShotDirection = PointEvent.ShotDirection;

        const FName Bone = PointEvent.HitInfo.BoneName;
        bHeadshot = (Bone == TEXT("head") || Bone == TEXT("neck_01"));
        if (bHeadshot)
        {
            DamageAmount *= HeadshotMultiplier;
        }
    }

    // BP의 Event AnyDamage / PointDamage 도 호출되도록 부모 처리
    DamageAmount = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
    if (DamageAmount <= 0.f)
    {
        return 0.f;
    }

    const float ActualDamage = FMath::Min(CurrentHP, DamageAmount);

    CurrentHP = FMath::Clamp(
        CurrentHP - ActualDamage,
        0.f,
        MaxHP
    );

    bProvoked = true;

    // 피격 피드백: 빨간 플래시 + 스케일 펄스 (0.1초)
    StartHitFlash();

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("Enemy Hit%s! HP: %.1f / %.1f"),
        bHeadshot ? TEXT(" (HEADSHOT)") : TEXT(""),
        CurrentHP,
        MaxHP
    );

    // 피격 이펙트/사운드
    if (HitEffect)
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            this, HitEffect, HitLocation, HitNormal.Rotation());
    }
    if (HitSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, HitSound, HitLocation);
    }

    if (CurrentHP <= 0.f)
    {
        // 사망 시 피격 애니메이션 대신 사망 애니메이션
        Die(ShotDirection);
    }
    else
    {
        // 살아 있다면 피격 애니메이션 랜덤 재생 + 경직 + 넉백
        PlayRandomMontage(HitMontages);

        StaggerEndTime = GetWorld()->GetTimeSeconds() + HitStaggerTime;

        if (AAIController* AIController = Cast<AAIController>(GetController()))
        {
            AIController->StopMovement();
        }

        const FVector Push = ShotDirection.GetSafeNormal2D() * HitKnockback;
        if (!Push.IsNearlyZero())
        {
            LaunchCharacter(Push, true, false);
        }
    }

    return ActualDamage;
}

// 사망
void AEnemyBase::Die(const FVector& ShotDirection)
{
    if (IsDead)
    {
        return;
    }

    IsDead = true;

    // 타이머 중지 (처치 타격의 피격 플래시는 끝까지 재생되도록 다시 예약)
    GetWorldTimerManager().ClearAllTimersForObject(this);
    if (bHitFlashActive)
    {
        GetWorldTimerManager().SetTimer(
            HitFlashTimerHandle, this, &AEnemyBase::EndHitFlash, FMath::Max(HitFlashDuration, 0.01f), false);
    }

    // AI 정지 후 컨트롤러 분리/제거 (시체에 빈 AIController가 남지 않도록)
    if (AController* OwnerController = GetController())
    {
        if (AAIController* AIController = Cast<AAIController>(OwnerController))
        {
            AIController->StopMovement();
        }
        DetachFromControllerPendingDestroy();
        OwnerController->Destroy();
    }

    // 이동 컴포넌트를 먼저 정지 (충돌을 끈 뒤 바닥으로 떨어지는 현상 방지)
    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->StopMovementImmediately();
        Move->DisableMovement();
    }

    // 캡슐 충돌 해제 + 시체가 총알/레이저를 막지 않도록
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);

    if (DeathSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, DeathSound, GetActorLocation());
    }

    UE_LOG(
        LogTemp,
        Warning,
        TEXT("Enemy Died!")
    );

    // 사망 애니메이션 재생
    float DeathDuration = 0.f;

    if (DeathMontage)
    {
        DeathDuration = PlayAnimMontage(DeathMontage);
    }

    if (DeathDuration > 0.f)
    {
        // 블렌드아웃 직전에 마지막 포즈로 고정
        const float BlendOut = DeathMontage->BlendOut.GetBlendTime();
        const float FreezeAt = FMath::Max(DeathDuration - BlendOut - 0.05f, 0.05f);
        GetWorldTimerManager().SetTimer(
            FreezeCorpseTimerHandle, this, &AEnemyBase::FreezeCorpse, FreezeAt, false);
    }
    else
    {
        // 사망 몽타주가 없으면 래그돌
        GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
        GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
        GetMesh()->SetSimulatePhysics(true);
        GetMesh()->AddImpulse(ShotDirection.GetSafeNormal() * 3000.f, NAME_None, true);
    }

    // 사망 애니메이션 + 시체 유지 시간 후 제거
    OnEnemyDied.Broadcast(this);

    // (LifeSpan 0 = 영구 유지이므로 최소값 보장)
    SetLifeSpan(FMath::Max(DeathDuration + CorpseLifetime, 0.1f));
}

void AEnemyBase::StartHitFlash()
{
    USkeletalMeshComponent* MeshComp = GetMesh();
    if (!MeshComp)
    {
        return;
    }

    if (!bHitFlashActive)
    {
        SavedOverlayMaterial = MeshComp->GetOverlayMaterial();
        bHitFlashActive = true;
    }

    if (HitFlashMaterial)
    {
        MeshComp->SetOverlayMaterial(HitFlashMaterial);
    }
    MeshComp->SetRelativeScale3D(BaseMeshScale * HitPulseScale);

    // 연속 피격 시 타이머를 갱신해 플래시 유지
    GetWorldTimerManager().SetTimer(
        HitFlashTimerHandle, this, &AEnemyBase::EndHitFlash, FMath::Max(HitFlashDuration, 0.01f), false);
}

void AEnemyBase::EndHitFlash()
{
    if (!bHitFlashActive)
    {
        return;
    }
    bHitFlashActive = false;

    if (USkeletalMeshComponent* MeshComp = GetMesh())
    {
        MeshComp->SetOverlayMaterial(SavedOverlayMaterial);
        MeshComp->SetRelativeScale3D(BaseMeshScale);
    }
    SavedOverlayMaterial = nullptr;
}

void AEnemyBase::FreezeCorpse()
{
    if (USkeletalMeshComponent* MeshComp = GetMesh())
    {
        MeshComp->bPauseAnims = true;
    }
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

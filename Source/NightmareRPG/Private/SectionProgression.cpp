#include "SectionProgression.h"
#include "BelicaCharacter.h"
#include "EnemyBase.h"
#include "Animation/AnimSequence.h"
#include "Animation/SkeletalMeshActor.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "SectionProgression"

ASectionProgression::ASectionProgression()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false; // 문이 움직일 때만 Tick

	EntryTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("EntryTrigger"));
	EntryTrigger->SetBoxExtent(FVector(200.0f, 200.0f, 150.0f));
	EntryTrigger->SetCollisionProfileName(TEXT("Trigger"));
	EntryTrigger->SetLineThickness(3.0f);
	RootComponent = EntryTrigger;

	StartTitle = LOCTEXT("StartTitle", "OBJECTIVE");
	StartContent = LOCTEXT("StartContent", "Enter the abandoned bunker");
	EnterTitle = LOCTEXT("EnterTitle", "QUARANTINE ZONE");
	EnterContent = LOCTEXT("EnterContent", "Eliminate the hostiles ({Remaining}/{Total} remaining)");
	ClearedTitle = LOCTEXT("ClearedTitle", "SECTION CLEARED");
	ClearedContent = LOCTEXT("ClearedContent", "The bulkhead is unlocked. Proceed deeper into the bunker");
}

void ASectionProgression::BeginPlay()
{
	Super::BeginPlay();

	// 무효 항목 제거 후 사망 이벤트 구독
	SectionEnemies.RemoveAll([](const TObjectPtr<AEnemyBase>& E) { return !IsValid(E); });
	TotalEnemies = SectionEnemies.Num();
	for (AEnemyBase* Enemy : SectionEnemies)
	{
		Enemy->OnEnemyDied.AddDynamic(this, &ASectionProgression::HandleEnemyDied);
	}

	EntryTrigger->OnComponentBeginOverlap.AddDynamic(this, &ASectionProgression::OnEntryBegin);

	if (DoorToOpen && DoorToOpen->GetRootComponent())
	{
		DoorClosedTransform = DoorToOpen->GetActorTransform();
		const FTransform Delta(DoorOpenRotation, DoorOpenOffset);
		DoorOpenTransform = Delta * DoorClosedTransform; // 문 로컬 공간 기준 오프셋
	}

	// 플레이어 폰 빙의/HUD 생성 이후에 목표 적용
	GetWorldTimerManager().SetTimerForNextTick(this, &ASectionProgression::ApplyStartObjective);
}

ABelicaCharacter* ASectionProgression::GetBelica() const
{
	return Cast<ABelicaCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
}

int32 ASectionProgression::GetRemainingEnemies() const
{
	int32 Remaining = 0;
	for (const AEnemyBase* Enemy : SectionEnemies)
	{
		if (IsValid(Enemy) && !Enemy->GetIsDead())
		{
			++Remaining;
		}
	}
	return Remaining;
}

FText ASectionProgression::FormatObjective(const FText& Pattern) const
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("Remaining"), GetRemainingEnemies());
	Args.Add(TEXT("Total"), TotalEnemies);
	return FText::Format(Pattern, Args);
}

void ASectionProgression::ApplyStartObjective()
{
	if (bEntered || bCleared || StartTitle.IsEmpty())
	{
		return;
	}
	if (ABelicaCharacter* Belica = GetBelica())
	{
		Belica->SetObjective(StartTitle, FormatObjective(StartContent));
	}
}

void ASectionProgression::OnEntryBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bEntered || !OtherActor || OtherActor != GetBelica())
	{
		return;
	}

	bEntered = true;

	if (GetRemainingEnemies() == 0)
	{
		CompleteSection();
	}
	else
	{
		UpdateCombatObjective();
	}
}

void ASectionProgression::UpdateCombatObjective()
{
	if (bCleared)
	{
		return;
	}
	if (ABelicaCharacter* Belica = GetBelica())
	{
		Belica->SetObjective(EnterTitle, FormatObjective(EnterContent));
	}
}

void ASectionProgression::HandleEnemyDied(AEnemyBase* Enemy)
{
	if (bCleared)
	{
		return;
	}

	if (GetRemainingEnemies() == 0)
	{
		CompleteSection();
	}
	else if (bEntered)
	{
		UpdateCombatObjective(); // 남은 수 갱신
	}
}

void ASectionProgression::CompleteSection()
{
	if (bCleared)
	{
		return;
	}
	bCleared = true;

	UE_LOG(LogTemp, Log, TEXT("[%s] Section cleared (%d enemies)"), *GetName(), TotalEnemies);

	if (ABelicaCharacter* Belica = GetBelica())
	{
		Belica->SetObjective(ClearedTitle, FormatObjective(ClearedContent));
	}
	if (SectionClearedSound)
	{
		UGameplayStatics::PlaySound2D(this, SectionClearedSound);
	}

	if (DoorToOpen)
	{
		if (DoorOpenDelay > 0.0f)
		{
			GetWorldTimerManager().SetTimer(DoorTimerHandle, this, &ASectionProgression::OpenDoor, DoorOpenDelay, false);
		}
		else
		{
			OpenDoor();
		}
	}

	OnSectionCleared.Broadcast();
	BP_OnSectionCleared();
}

void ASectionProgression::OpenDoor()
{
	if (!IsValid(DoorToOpen))
	{
		return;
	}

	if (DoorOpenSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, DoorOpenSound, DoorToOpen->GetActorLocation());
	}

	// 1) 스켈레탈 메시 문 + 열림 애니메이션
	ASkeletalMeshActor* SkelDoor = Cast<ASkeletalMeshActor>(DoorToOpen);
	if (SkelDoor && DoorOpenAnim)
	{
		USkeletalMeshComponent* SkelComp = SkelDoor->GetSkeletalMeshComponent();
		SkelComp->PlayAnimation(DoorOpenAnim, false);
		GetWorldTimerManager().SetTimer(DoorTimerHandle, this, &ASectionProgression::FinishDoor,
			FMath::Max(DoorOpenAnim->GetPlayLength(), 0.05f), false);
		return;
	}

	// 2) 일반 문: 이동/회전 보간
	if (USceneComponent* Root = DoorToOpen->GetRootComponent())
	{
		Root->SetMobility(EComponentMobility::Movable);
	}
	DoorElapsed = 0.0f;
	bDoorAnimating = true;
	SetActorTickEnabled(true);
}

void ASectionProgression::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bDoorAnimating || !IsValid(DoorToOpen))
	{
		SetActorTickEnabled(false);
		return;
	}

	DoorElapsed += DeltaTime;
	const float Alpha = FMath::Clamp(DoorElapsed / DoorOpenTime, 0.0f, 1.0f);
	const float Eased = FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, 2.0f);

	FTransform Current;
	Current.Blend(DoorClosedTransform, DoorOpenTransform, Eased);
	DoorToOpen->SetActorTransform(Current);

	if (Alpha >= 1.0f)
	{
		bDoorAnimating = false;
		SetActorTickEnabled(false);
		FinishDoor();
	}
}

void ASectionProgression::FinishDoor()
{
	if (bDisableDoorCollisionWhenOpen && IsValid(DoorToOpen))
	{
		DoorToOpen->SetActorEnableCollision(false);
	}
}

#undef LOCTEXT_NAMESPACE

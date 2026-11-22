#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SectionProgression.generated.h"

class UBoxComponent;
class UAnimSequence;
class USoundBase;
class AEnemyBase;
class ABelicaCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSectionCleared);

// 데드 스페이스식 구역 진행:
//  - 진입 트리거에 들어오면 HUD 목표를 "위협 제거 (남은 수)"로 갱신
//  - 지정된 적이 모두 죽으면 목표 갱신 + 다음 문 개방 + OnSectionCleared 이벤트
UCLASS()
class NIGHTMARERPG_API ASectionProgression : public AActor
{
	GENERATED_BODY()

public:
	ASectionProgression();

	virtual void Tick(float DeltaTime) override;

	// BP에서 추가 연출(조명, 사운드, 체크포인트 등)을 붙일 수 있는 이벤트
	UPROPERTY(BlueprintAssignable, Category = "Progression")
	FOnSectionCleared OnSectionCleared;

	UFUNCTION(BlueprintPure, Category = "Progression")
	int32 GetRemainingEnemies() const;

	UFUNCTION(BlueprintPure, Category = "Progression")
	bool IsCleared() const { return bCleared; }

protected:
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Progression", meta = (DisplayName = "On Section Cleared"))
	void BP_OnSectionCleared();

	UFUNCTION()
	void OnEntryBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEnemyDied(AEnemyBase* Enemy);

	// ===== 구성 =====

	// 구역 진입 감지 볼륨
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Progression")
	UBoxComponent* EntryTrigger;

	// 이 구역에서 처치해야 하는 적들 (레벨에서 지정)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Progression")
	TArray<TObjectPtr<AEnemyBase>> SectionEnemies;

	// ===== 목표 텍스트 ({Remaining}, {Total} 사용 가능) =====

	// 레벨 시작 시 목표 (비워 두면 HUD 기본값 유지)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progression|Objective")
	FText StartTitle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progression|Objective")
	FText StartContent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progression|Objective")
	FText EnterTitle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progression|Objective")
	FText EnterContent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progression|Objective")
	FText ClearedTitle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progression|Objective")
	FText ClearedContent;

	// ===== 클리어 시 열리는 문 =====

	// 스켈레탈 메시 문이면 DoorOpenAnim 재생, 그 외에는 아래 오프셋만큼 이동/회전
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Progression|Door")
	TObjectPtr<AActor> DoorToOpen;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression|Door")
	TObjectPtr<UAnimSequence> DoorOpenAnim;

	// 문 기준 로컬 이동량 / 회전량 (스태틱 메시 문)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression|Door")
	FVector DoorOpenOffset = FVector(0.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression|Door")
	FRotator DoorOpenRotation = FRotator(0.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression|Door", meta = (ClampMin = "0.05"))
	float DoorOpenTime = 2.0f;

	// 클리어 후 문이 열리기 시작할 때까지 대기 (긴장감 연출)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression|Door")
	float DoorOpenDelay = 1.5f;

	// 열린 뒤 문 충돌 해제 (애니메이션 문은 충돌이 따라 움직이지 않을 수 있음)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression|Door")
	bool bDisableDoorCollisionWhenOpen = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression|Door")
	TObjectPtr<USoundBase> DoorOpenSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression")
	TObjectPtr<USoundBase> SectionClearedSound;

private:
	void ApplyStartObjective();
	void UpdateCombatObjective();
	void CompleteSection();
	void OpenDoor();
	void FinishDoor();
	ABelicaCharacter* GetBelica() const;
	FText FormatObjective(const FText& Pattern) const;

	bool bEntered = false;
	bool bCleared = false;
	int32 TotalEnemies = 0;

	// 스태틱 메시 문 보간
	bool bDoorAnimating = false;
	float DoorElapsed = 0.0f;
	FTransform DoorClosedTransform;
	FTransform DoorOpenTransform;

	FTimerHandle DoorTimerHandle;
};

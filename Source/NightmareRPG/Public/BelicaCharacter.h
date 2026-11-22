#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "BelicaCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UStaticMeshComponent;
class UInputMappingContext;
class UInputAction;
class UAnimMontage;
class UUserWidget;
class UCameraShakeBase;

UCLASS()
class NIGHTMARERPG_API ABelicaCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ABelicaCharacter();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

	// 적과 몸이 닿았을 때 접촉 데미지 (캡슐 충돌 / 오버랩 둘 다 처리)
	virtual void NotifyHit(class UPrimitiveComponent* MyComp, AActor* Other, class UPrimitiveComponent* OtherComp,
		bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit) override;
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;

	// 앉기/일어서기 시 카메라 Z 보간용 기본 높이 및 속도
	float BoomBaseZ = 65.0f;
	float BoomZInterpSpeed = 10.0f;

	// --- 입력 콜백 함수 ---
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void ToggleCrouch();
	void StartSprint();
	void StopSprint();
	void StartAim();
	void StopAim();
	void Fire();
	void Interact(); // F 키: 탄약 줍기
	void Reload();   // R 키: 장전

	// --- 스태미나 & 장전 내부 로직 ---
	UFUNCTION(BlueprintCallable, Category = "Stamina")
	bool ConsumeStamina(float Cost);

	UFUNCTION()
	void RecoverStamina();

	UFUNCTION()
	void OnReloadMontageEnded(UAnimMontage* Montage, bool bInterrupted);

public:
	// 탄약 상자(BP_AmmoPickup) 오버랩 시 호출용
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void SetOverlappingAmmo(AActor* AmmoActor) { OverlappingAmmo = AmmoActor; }

	// 범위를 벗어난 픽업이 현재 대상일 때만 해제 (여러 상자가 겹칠 때 다른 상자 참조를 지우지 않도록)
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ClearOverlappingAmmo(AActor* AmmoActor) { if (OverlappingAmmo == AmmoActor) OverlappingAmmo = nullptr; }

	// 예비 탄약 추가 (MaxReserveAmmo로 제한). 실제로 추가된 양 반환
	UFUNCTION(BlueprintCallable, Category = "Combat")
	int32 AddReserveAmmo(int32 Amount);

	// WBP_PlayerHUD의 기존 Text_ObjectiveTitle / Text_ObjectiveContent 텍스트만 교체 (위젯 에셋은 수정하지 않음)
	// HUD 생성 전에 호출되면 저장해 두었다가 BeginPlay에서 적용
	UFUNCTION(BlueprintCallable, Category = "UI")
	void SetObjective(const FText& Title, const FText& Content);

	// 적 공격/접촉 데미지 수신. DamageCooldown(1.2초) 내 중복 피격은 무시
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
		class AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetHealthPercent() const { return MaxHealth > 0.0f ? CurrentHealth / MaxHealth : 0.0f; }

	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsPlayerDead() const { return bIsPlayerDead; }

protected:
	// --- 컴포넌트 ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	UCameraComponent* FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	UStaticMeshComponent* LaserDot;

	// --- Enhanced Input 에셋 (BP 디테일 패널에서 할당) ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputMappingContext* DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* LookAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* JumpAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* CrouchAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* SprintAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* AimAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* FireAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* InteractAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* ReloadAction;

	// --- 애니메이션 & UI 에셋 ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	UAnimMontage* FireMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	UAnimMontage* ReloadMontage;

	// 1발당 데미지 (적 MaxHP 100 → 10발에 처치)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float WeaponDamage = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float WeaponRange = 10000.0f;

	// 벽/바닥 탄착 이펙트 (선택)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	class UNiagaraSystem* WorldImpactEffect = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UUserWidget> HUDWidgetClass;

	UPROPERTY(BlueprintReadOnly, Category = "UI")
	UUserWidget* PlayerHUDWidget;

	// --- 상태 및 탄약/스태미나 변수 (기존 WBP_PlayerHUD 호환용 이름 유지) ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	int32 CurrentAmmo = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	int32 MaxAmmo = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	int32 ReserveAmmo = 60;

	// 예비 탄약 최대치 (서바이벌 호러식 탄약 관리)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	int32 MaxReserveAmmo = 180;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	bool bIsReloading = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	bool vIsAiming = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
	bool IsSprint = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina")
	float CurrentStamina = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina")
	float MaxStamina = 100.0f;

	// --- 바이오하자드식 시선 처리 (AnimBP의 Aim Offset / 머리 본 회전에 사용) ---
	// 카메라(마우스) 방향과 캐릭터 정면 사이의 각도. AnimBP에서 Try Get Pawn Owner -> Cast 후 읽어 사용
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Look")
	float AimYaw = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Look")
	float AimPitch = 0.0f;

	// 머리가 돌아갈 수 있는 최대 좌우/상하 각도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	float MaxLookYaw = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	float MaxLookPitch = 45.0f;

	// 카메라가 이 각도보다 뒤쪽을 보면 시선을 정면으로 되돌림
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	float LookBehindThreshold = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	float LookInterpSpeed = 8.0f;

	// 카메라 상하 회전 제한
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	float CameraPitchMin = -60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	float CameraPitchMax = 50.0f;

	// 목표 갱신 시 재생할 효과음 (선택)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	class USoundBase* ObjectiveUpdatedSound = nullptr;

	// --- 적중 피드백 (WBP_PlayerHUD의 Crosshair / HitMarker 위젯 사용) ---
	// 적 적중 시 크로스헤어가 빨갛게 바뀌고 히트마커가 표시되는 시간 (초)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|HitFeedback")
	float HitMarkerDuration = 0.15f;

	// 히트마커가 처음 커졌다가 1.0으로 줄어드는 펄스 배율
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|HitFeedback")
	float HitMarkerPulseScale = 1.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|HitFeedback")
	FLinearColor CrosshairColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.9f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|HitFeedback")
	FLinearColor CrosshairHitColor = FLinearColor(1.0f, 0.05f, 0.05f, 1.0f);

	// 적중 시 재생할 2D 효과음 (선택, 프로젝트에 전용 사운드가 없으면 비워둠)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|HitFeedback")
	class USoundBase* HitMarkerSound = nullptr;

	// 적 적중 시 플레이어 카메라 흔들림 (기본: BP_CameraShake_Hit_Enemy, 약하게)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|HitFeedback")
	TSubclassOf<UCameraShakeBase> EnemyHitCameraShake;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|HitFeedback")
	float EnemyHitShakeScale = 0.35f;

	// 적중 시 크로스헤어 펄스 배율
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|HitFeedback")
	float CrosshairPulseScale = 1.25f;

	// --- 체력 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float MaxHealth = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
	float CurrentHealth = 100.0f;

	// 적에게 닿았을 때 받는 기본 데미지 (적 AttackDamage가 0이면 사용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float EnemyContactDamage = 20.0f;

	// 피격 후 무적 시간 (한 프레임에 여러 번 맞아 즉사하는 것을 방지)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float DamageCooldown = 1.2f;

	// 사망 후 레벨 재시작까지 대기 시간
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float DeathRestartDelay = 1.5f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
	bool bIsPlayerDead = false;

	// 피격 시 카메라 흔들림 (기본: BP_CameraShake_Hit_Player)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health")
	TSubclassOf<UCameraShakeBase> PlayerHurtCameraShake;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float PlayerHurtShakeScale = 0.6f;

	// 피격 효과음 (선택, 프로젝트에 전용 사운드가 없으면 비워둠)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health")
	class USoundBase* PlayerHurtSound = nullptr;

	// 화면 가장자리 빨간 피격 효과 (WBP_PlayerHUD의 DamageVignette 이미지) 페이드 시간
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float DamageVignetteDuration = 0.6f;

	// 피격 시 BP에서 추가 연출을 붙일 수 있는 이벤트
	UFUNCTION(BlueprintImplementableEvent, Category = "Health")
	void OnPlayerDamaged(float Damage, float NewHealth);

	UFUNCTION(BlueprintImplementableEvent, Category = "Health")
	void OnPlayerDied();

	// 적 적중 시 BP에서 추가 연출을 붙일 수 있는 이벤트
	UFUNCTION(BlueprintImplementableEvent, Category = "Combat")
	void OnEnemyHitConfirmed(AActor* Enemy, bool bKilled);

private:
	void ApplyObjectiveToHUD();

	// 적중 피드백 시작 / Tick에서 갱신
	void TriggerHitFeedback(bool bKilled);
	void UpdateHitFeedback(float DeltaTime);
	void SetCrosshairColor(const FLinearColor& Color);
	void SetCrosshairScale(float Scale);

	// 체력 UI / 피격 연출 / 사망
	void TryApplyEnemyContactDamage(AActor* Other);
	void UpdateHealthBar();
	void UpdateDamageVignette(float DeltaTime);
	void SetDamageVignetteAlpha(float Alpha);
	void HandlePlayerDeath();
	void RestartCurrentLevel();

	float LastDamageTime = -1000.0f;
	float DamageVignetteRemaining = 0.0f;
	FTimerHandle DeathRestartTimerHandle;

	float HitFeedbackRemaining = 0.0f;
	float HitPulseStartScale = 1.0f;

	FText ObjectiveTitle;
	FText ObjectiveContent;
	bool bHasObjective = false;

	UPROPERTY()
	AActor* OverlappingAmmo = nullptr;

	FTimerHandle StaminaRecoverTimerHandle;
};
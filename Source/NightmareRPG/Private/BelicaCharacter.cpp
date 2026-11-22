#include "BelicaCharacter.h"
#include "AmmoPickup.h"
#include "EnemyBase.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Camera/CameraShakeBase.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"
#include "Animation/AnimInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

ABelicaCharacter::ABelicaCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->MaxWalkSpeed = 330.0f;
	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->SetRelativeLocation(FVector(0.0f, 0.0f, 65.0f));
	CameraBoom->TargetArmLength = 180.0f;
	CameraBoom->SocketOffset = FVector(0.0f, 45.0f, 10.0f);
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->FieldOfView = 75.0f;
	FollowCamera->bUsePawnControlRotation = false;

	LaserDot = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LaserDot"));
	LaserDot->SetupAttachment(RootComponent);
	LaserDot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LaserDot->SetVisibility(false);

	// 기본 카메라 흔들림 (BP 디테일 패널에서 교체 가능)
	static ConstructorHelpers::FClassFinder<UCameraShakeBase> EnemyHitShakeClass(
		TEXT("/Game/_Reference/Variant_Combat/Blueprints/BP_CameraShake_Hit_Enemy"));
	if (EnemyHitShakeClass.Succeeded())
	{
		EnemyHitCameraShake = EnemyHitShakeClass.Class;
	}
	static ConstructorHelpers::FClassFinder<UCameraShakeBase> PlayerHurtShakeClass(
		TEXT("/Game/_Reference/Variant_Combat/Blueprints/BP_CameraShake_Hit_Player"));
	if (PlayerHurtShakeClass.Succeeded())
	{
		PlayerHurtCameraShake = PlayerHurtShakeClass.Class;
	}
}

void ABelicaCharacter::BeginPlay()
{
	Super::BeginPlay();

	/// 0. 카메라 컴포넌트 참조 및 3인칭 시점 위치/부착 복구
	if (!CameraBoom)
	{
		CameraBoom = FindComponentByClass<USpringArmComponent>();
		if (!CameraBoom)
		{
			CameraBoom = NewObject<USpringArmComponent>(this, TEXT("CameraBoom_Runtime"));
			CameraBoom->SetupAttachment(RootComponent);
			CameraBoom->RegisterComponent();
		}
	}
	if (CameraBoom)
	{
		CameraBoom->SetRelativeLocation(FVector(0.0f, 0.0f, 65.0f));
		CameraBoom->TargetArmLength = 180.0f;
		CameraBoom->SocketOffset = FVector(0.0f, 45.0f, 10.0f);
		CameraBoom->bUsePawnControlRotation = true;
	}

	if (!FollowCamera)
	{
		FollowCamera = FindComponentByClass<UCameraComponent>();
		if (!FollowCamera)
		{
			FollowCamera = NewObject<UCameraComponent>(this, TEXT("FollowCamera_Runtime"));
			FollowCamera->RegisterComponent();
		}
	}
	if (FollowCamera && CameraBoom)
	{
		FollowCamera->AttachToComponent(CameraBoom, FAttachmentTransformRules::SnapToTargetNotIncludingScale, USpringArmComponent::SocketName);
		FollowCamera->FieldOfView = 75.0f;
		FollowCamera->bUsePawnControlRotation = false;
	}
	if (!LaserDot)
	{
		LaserDot = Cast<UStaticMeshComponent>(GetDefaultSubobjectByName(TEXT("LaserDot")));
	}

	// 0-1. 체력 초기화
	CurrentHealth = MaxHealth;
	bIsPlayerDead = false;

	// 1. HUD 생성 및 뷰포트 추가
	if (HUDWidgetClass)
	{
		PlayerHUDWidget = CreateWidget<UUserWidget>(GetWorld(), HUDWidgetClass);
		if (PlayerHUDWidget)
		{
			PlayerHUDWidget->AddToViewport();
			// HUD 생성 전에 설정된 목표가 있으면 반영
			ApplyObjectiveToHUD();
			// 체력바 초기값 / 피격 비네트 숨김
			UpdateHealthBar();
			SetDamageVignetteAlpha(0.0f);
		}
	}

	// 2. Enhanced Input Mapping Context 등록
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		// 카메라 상하 회전 범위 제한 (바이오하자드식 숄더뷰)
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->ViewPitchMin = CameraPitchMin;
			PC->PlayerCameraManager->ViewPitchMax = CameraPitchMax;
		}

		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
			if (PauseMappingContext)
			{
				Subsystem->AddMappingContext(PauseMappingContext, 1);
			}
		}
	}

	// 3. 1초마다 스태미나 회복 타이머
	GetWorldTimerManager().SetTimer(
		StaminaRecoverTimerHandle, this, &ABelicaCharacter::RecoverStamina, 1.0f, true);
}

void ABelicaCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 1. 조준(ADS) 시 카메라 FOV(75 <-> 60) 및 암 길이(180 <-> 100) 부드럽게 보간
	if (FollowCamera && CameraBoom)
	{
		const float TargetFOV = vIsAiming ? 60.0f : 75.0f;
		const float TargetArm = vIsAiming ? 100.0f : 180.0f;
		FollowCamera->SetFieldOfView(FMath::FInterpTo(FollowCamera->FieldOfView, TargetFOV, DeltaTime, 12.0f));
		CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, TargetArm, DeltaTime, 12.0f);
	}

	// 1-1. 앉기/일어서기 시 SpringArm Z 오프셋 부드럽게 보간 (카메라 순간이동 방지)
	if (CameraBoom)
	{
		FVector Loc = CameraBoom->GetRelativeLocation();
		Loc.Z = FMath::FInterpTo(Loc.Z, BoomBaseZ, DeltaTime, BoomZInterpSpeed);
		CameraBoom->SetRelativeLocation(Loc);
	}

	// 1-2. 시선 처리: 마우스로 돌린 카메라 방향을 캐릭터 머리가 따라봄
	if (Controller)
	{
		const FRotator Delta = (Controller->GetControlRotation() - GetActorRotation()).GetNormalized();

		float TargetYaw = Delta.Yaw;
		float TargetPitch = Delta.Pitch;

		// 카메라가 캐릭터 뒤쪽을 보면 고개를 무리하게 꺾지 않고 정면으로 복귀
		if (FMath::Abs(TargetYaw) > LookBehindThreshold)
		{
			TargetYaw = 0.0f;
			TargetPitch = 0.0f;
		}

		TargetYaw = FMath::Clamp(TargetYaw, -MaxLookYaw, MaxLookYaw);
		TargetPitch = FMath::Clamp(TargetPitch, -MaxLookPitch, MaxLookPitch);

		AimYaw = FMath::FInterpTo(AimYaw, TargetYaw, DeltaTime, LookInterpSpeed);
		AimPitch = FMath::FInterpTo(AimPitch, TargetPitch, DeltaTime, LookInterpSpeed);
	}

	// 2. 조준 중일 때 weapon 소켓 -> 카메라 전방 5000 라인트레이스로 레이저 도트 위치 갱신
	if (vIsAiming && GetMesh() && FollowCamera && LaserDot)
	{
		const FVector Start = GetMesh()->GetSocketLocation(TEXT("weapon"));
		const FVector End = Start + (FollowCamera->GetForwardVector() * 5000.0f);

		FHitResult HitResult;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(this);

		const bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, Params);
		LaserDot->SetVisibility(bHit);
		if (bHit)
		{
			LaserDot->SetWorldLocation(HitResult.ImpactPoint);
		}
	}

	// 2-1. 적중 피드백 (히트마커 펄스 / 크로스헤어 색 복구)
	UpdateHitFeedback(DeltaTime);

	// 2-2. 피격 시 화면 가장자리 빨간 비네트 페이드아웃
	UpdateDamageVignette(DeltaTime);

	// 3. 질주 중 스태미나 소모 (초당 20)
	if (IsSprint)
	{
		if (!ConsumeStamina(20.0f * DeltaTime))
		{
			StopSprint();
		}
	}
}

void ABelicaCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	// 캡슐 중심이 내려간 만큼 상대 Z를 올려 월드 높이를 유지 -> Tick에서 서서히 보간
	if (CameraBoom)
	{
		CameraBoom->AddRelativeLocation(FVector(0.0f, 0.0f, HalfHeightAdjust));
	}
}

void ABelicaCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	if (CameraBoom)
	{
		CameraBoom->AddRelativeLocation(FVector(0.0f, 0.0f, -HalfHeightAdjust));
	}
}

void ABelicaCharacter::NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp,
	bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
	Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);
	TryApplyEnemyContactDamage(Other);
}

void ABelicaCharacter::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);
	TryApplyEnemyContactDamage(OtherActor);
}

void ABelicaCharacter::TryApplyEnemyContactDamage(AActor* Other)
{
	if (bIsPlayerDead)
	{
		return;
	}

	AEnemyBase* Enemy = Cast<AEnemyBase>(Other);
	if (!Enemy || Enemy->GetIsDead())
	{
		return;
	}

	// 쿨다운 중이면 데미지 이벤트 자체를 만들지 않음 (접촉 중에는 매 프레임 호출됨)
	if (GetWorld()->GetTimeSeconds() - LastDamageTime < DamageCooldown)
	{
		return;
	}

	const float Damage = Enemy->GetAttackDamage() > 0.0f ? Enemy->GetAttackDamage() : EnemyContactDamage;
	UGameplayStatics::ApplyDamage(this, Damage, Enemy->GetController(), Enemy, UDamageType::StaticClass());
}

float ABelicaCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	if (bIsPlayerDead || DamageAmount <= 0.0f)
	{
		return 0.0f;
	}

	// 1.2초 피격 쿨다운 (공격 + 접촉이 같은 프레임에 겹쳐도 한 번만 적용)
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastDamageTime < DamageCooldown)
	{
		return 0.0f;
	}

	DamageAmount = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (DamageAmount <= 0.0f)
	{
		return 0.0f;
	}

	LastDamageTime = Now;

	const float ActualDamage = FMath::Min(CurrentHealth, DamageAmount);
	CurrentHealth = FMath::Clamp(CurrentHealth - ActualDamage, 0.0f, MaxHealth);

	UE_LOG(LogTemp, Warning, TEXT("Belica Hit! HP: %.1f / %.1f"), CurrentHealth, MaxHealth);

	UpdateHealthBar();

	// 피격 연출: 화면 가장자리 빨간 비네트 + 카메라 흔들림 + 효과음
	DamageVignetteRemaining = FMath::Max(DamageVignetteDuration, 0.01f);
	SetDamageVignetteAlpha(1.0f);

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (PlayerHurtCameraShake)
		{
			PC->ClientStartCameraShake(PlayerHurtCameraShake, PlayerHurtShakeScale);
		}
	}
	if (PlayerHurtSound)
	{
		UGameplayStatics::PlaySound2D(this, PlayerHurtSound);
	}

	OnPlayerDamaged(ActualDamage, CurrentHealth);

	if (CurrentHealth <= 0.0f)
	{
		HandlePlayerDeath();
	}

	return ActualDamage;
}

void ABelicaCharacter::HandlePlayerDeath()
{
	if (bIsPlayerDead)
	{
		return;
	}
	bIsPlayerDead = true;

	UE_LOG(LogTemp, Warning, TEXT("Belica Died! Showing Game Over menu in %.1fs"), DeathRestartDelay);

	StopSprint();
	StopAim();
	bIsReloading = false;

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		DisableInput(PC);
	}

	// 사망 중에는 비네트를 유지
	DamageVignetteRemaining = 0.0f;
	SetDamageVignetteAlpha(0.85f);

	OnPlayerDied();

	//GetWorldTimerManager().SetTimer(
	//	DeathRestartTimerHandle, this, &ABelicaCharacter::RestartCurrentLevel,
	//	FMath::Max(DeathRestartDelay, 0.01f), false);
	// 위가 수정 전 아래가 수정 후

	GetWorldTimerManager().SetTimer(
		DeathRestartTimerHandle, this, &ABelicaCharacter::ShowGameOverMenu,
		FMath::Max(DeathRestartDelay, 0.01f), false);
}

void ABelicaCharacter::RestartCurrentLevel()
{
	// 현재 레벨(LV_2) 다시 로드. PIE 접두사(UEDPIE_0_)는 제거된 이름 사용
	const FString LevelName = UGameplayStatics::GetCurrentLevelName(this, /*bRemovePrefixString=*/true);
	UGameplayStatics::OpenLevel(this, FName(*LevelName));
}

void ABelicaCharacter::UpdateHealthBar()
{
	if (!PlayerHUDWidget)
	{
		return;
	}
	if (UProgressBar* HealthBar = Cast<UProgressBar>(PlayerHUDWidget->GetWidgetFromName(TEXT("HealthBar"))))
	{
		HealthBar->SetPercent(GetHealthPercent());
	}
}

void ABelicaCharacter::SetDamageVignetteAlpha(float Alpha)
{
	if (!PlayerHUDWidget)
	{
		return;
	}
	if (UImage* Vignette = Cast<UImage>(PlayerHUDWidget->GetWidgetFromName(TEXT("DamageVignette"))))
	{
		Vignette->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, FMath::Clamp(Alpha, 0.0f, 1.0f)));
	}
}

void ABelicaCharacter::UpdateDamageVignette(float DeltaTime)
{
	if (bIsPlayerDead || DamageVignetteRemaining <= 0.0f)
	{
		return;
	}

	DamageVignetteRemaining = FMath::Max(DamageVignetteRemaining - DeltaTime, 0.0f);
	const float Alpha = DamageVignetteRemaining / FMath::Max(DamageVignetteDuration, 0.01f);
	// 처음엔 강하게, 끝으로 갈수록 빠르게 사라지도록
	SetDamageVignetteAlpha(Alpha * Alpha);
}

void ABelicaCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (MoveAction)
			EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ABelicaCharacter::Move);
		if (LookAction)
			EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &ABelicaCharacter::Look);
		if (JumpAction)
		{
			EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
			EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		}
		if (CrouchAction)
			EnhancedInput->BindAction(CrouchAction, ETriggerEvent::Started, this, &ABelicaCharacter::ToggleCrouch);
		if (SprintAction)
		{
			EnhancedInput->BindAction(SprintAction, ETriggerEvent::Started, this, &ABelicaCharacter::StartSprint);
			EnhancedInput->BindAction(SprintAction, ETriggerEvent::Completed, this, &ABelicaCharacter::StopSprint);
		}
		if (AimAction)
		{
			EnhancedInput->BindAction(AimAction, ETriggerEvent::Started, this, &ABelicaCharacter::StartAim);
			EnhancedInput->BindAction(AimAction, ETriggerEvent::Completed, this, &ABelicaCharacter::StopAim);
		}
		if (FireAction)
			EnhancedInput->BindAction(FireAction, ETriggerEvent::Started, this, &ABelicaCharacter::Fire);
		if (InteractAction)
			EnhancedInput->BindAction(InteractAction, ETriggerEvent::Started, this, &ABelicaCharacter::Interact);
		if (ReloadAction)
			EnhancedInput->BindAction(ReloadAction, ETriggerEvent::Started, this, &ABelicaCharacter::Reload);
		if (PauseAction)
		{
			EnhancedInput->BindAction(
				PauseAction,
				ETriggerEvent::Started,
				this,
				&ABelicaCharacter::TogglePauseMenu
			);
		}
	}
}

void ABelicaCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D MovementVector = Value.Get<FVector2D>();
	if (Controller)
	{
		const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
		const FVector ForwardDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(ForwardDir, MovementVector.Y);
		AddMovementInput(RightDir, MovementVector.X);
	}
}

void ABelicaCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D LookAxis = Value.Get<FVector2D>();
	if (Controller)
	{
		AddControllerYawInput(LookAxis.X);
		AddControllerPitchInput(LookAxis.Y);
	}
}

void ABelicaCharacter::ToggleCrouch()
{
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Crouch();
	}
}

void ABelicaCharacter::StartSprint()
{
	if (ConsumeStamina(10.0f))
	{
		IsSprint = true;
		GetCharacterMovement()->MaxWalkSpeed = 500.0f;
	}
}

void ABelicaCharacter::StopSprint()
{
	IsSprint = false;
	GetCharacterMovement()->MaxWalkSpeed = 330.0f;
}

void ABelicaCharacter::StartAim()
{
	vIsAiming = true;
	bUseControllerRotationYaw = true;
	GetCharacterMovement()->bOrientRotationToMovement = false;
}

void ABelicaCharacter::StopAim()
{
	vIsAiming = false;
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	if (LaserDot)
	{
		LaserDot->SetVisibility(false);
	}
}

void ABelicaCharacter::Fire()
{
	if (bIsReloading || CurrentAmmo <= 0 || !FollowCamera)
	{
		return;
	}

	CurrentAmmo--;

	if (FireMontage)
	{
		PlayAnimMontage(FireMontage);
	}

	// 숄더뷰 카메라 기준 조준선. 카메라와 캐릭터 사이의 물체(뒤쪽 벽 등)에 맞지 않도록
	// 트레이스 시작점을 캐릭터 위치까지 앞으로 당김
	const FVector CamLoc = FollowCamera->GetComponentLocation();
	const FVector AimDir = FollowCamera->GetForwardVector();
	const float DistToCharacter = FMath::Max(FVector::DotProduct(GetActorLocation() - CamLoc, AimDir), 0.0f);
	const FVector Start = CamLoc + AimDir * DistToCharacter;
	const FVector End = CamLoc + AimDir * WeaponRange;

	FHitResult HitResult;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BelicaFire), /*bTraceComplex=*/false, this);

	if (!GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, Params))
	{
		return;
	}

	AActor* HitActor = HitResult.GetActor();
	if (!HitActor)
	{
		return;
	}

	// 적(AEnemyBase::TakeDamage)은 피격 위치/본 정보로 피격 리액션 및 헤드샷 판정
	const float Dealt = UGameplayStatics::ApplyPointDamage(
		HitActor, WeaponDamage, AimDir, HitResult, GetController(), this, UDamageType::StaticClass());

	// 적에게 실제로 데미지가 들어갔으면 HUD 적중 피드백 (크로스헤어 빨강 + 히트마커)
	if (AEnemyBase* Enemy = Cast<AEnemyBase>(HitActor))
	{
		if (Dealt > 0.0f)
		{
			const bool bKilled = Enemy->GetIsDead();
			TriggerHitFeedback(bKilled);
			OnEnemyHitConfirmed(Enemy, bKilled);
		}
	}

	// 적이 아닌 곳(벽/바닥)에 맞았을 때의 탄착 이펙트 (적 피격 이펙트는 EnemyBase에서 처리)
	if (!HitActor->IsA<AEnemyBase>() && WorldImpactEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this, WorldImpactEffect, HitResult.ImpactPoint, HitResult.ImpactNormal.Rotation());
	}
}

void ABelicaCharacter::Interact()
{
	if (!IsValid(OverlappingAmmo))
	{
		return;
	}

	// 예비 탄약이 가득 차 있으면 상자를 남겨둠
	if (ReserveAmmo >= MaxReserveAmmo)
	{
		return;
	}

	if (AAmmoPickup* Pickup = Cast<AAmmoPickup>(OverlappingAmmo))
	{
		Pickup->Collect(this);
	}
	else
	{
		// 레거시 BP 픽업 호환: AmmoAmount 변수를 리플렉션으로 읽음
		int32 PickupAmount = 30;
		if (FIntProperty* IntProp = FindFProperty<FIntProperty>(OverlappingAmmo->GetClass(), TEXT("AmmoAmount")))
		{
			PickupAmount = IntProp->GetPropertyValue_InContainer(OverlappingAmmo);
		}
		AddReserveAmmo(PickupAmount);
		OverlappingAmmo->Destroy();
	}
	OverlappingAmmo = nullptr;
}

void ABelicaCharacter::SetObjective(const FText& Title, const FText& Content)
{
	const bool bChanged = !bHasObjective || !ObjectiveTitle.EqualTo(Title) || !ObjectiveContent.EqualTo(Content);

	ObjectiveTitle = Title;
	ObjectiveContent = Content;
	bHasObjective = true;
	ApplyObjectiveToHUD();

	if (bChanged && ObjectiveUpdatedSound)
	{
		UGameplayStatics::PlaySound2D(this, ObjectiveUpdatedSound);
	}
}

void ABelicaCharacter::ApplyObjectiveToHUD()
{
	if (!bHasObjective || !PlayerHUDWidget)
	{
		return;
	}

	if (UTextBlock* TitleText = Cast<UTextBlock>(PlayerHUDWidget->GetWidgetFromName(TEXT("Text_ObjectiveTitle"))))
	{
		TitleText->SetText(ObjectiveTitle);
	}
	if (UTextBlock* ContentText = Cast<UTextBlock>(PlayerHUDWidget->GetWidgetFromName(TEXT("Text_ObjectiveContent"))))
	{
		ContentText->SetText(ObjectiveContent);
	}
	if (UWidget* Border = PlayerHUDWidget->GetWidgetFromName(TEXT("Border_Objective")))
	{
		Border->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void ABelicaCharacter::SetCrosshairColor(const FLinearColor& Color)
{
	if (!PlayerHUDWidget)
	{
		return;
	}
	if (UPanelWidget* Crosshair = Cast<UPanelWidget>(PlayerHUDWidget->GetWidgetFromName(TEXT("Crosshair"))))
	{
		for (int32 i = 0; i < Crosshair->GetChildrenCount(); ++i)
		{
			if (UImage* Img = Cast<UImage>(Crosshair->GetChildAt(i)))
			{
				Img->SetColorAndOpacity(Color);
			}
		}
	}
}

void ABelicaCharacter::SetCrosshairScale(float Scale)
{
	if (!PlayerHUDWidget)
	{
		return;
	}
	if (UWidget* Crosshair = PlayerHUDWidget->GetWidgetFromName(TEXT("Crosshair")))
	{
		Crosshair->SetRenderScale(FVector2D(Scale, Scale));
	}
}

void ABelicaCharacter::TriggerHitFeedback(bool bKilled)
{
	HitFeedbackRemaining = FMath::Max(HitMarkerDuration, 0.01f);
	SetCrosshairColor(CrosshairHitColor);
	SetCrosshairScale(CrosshairPulseScale);

	// 약한 카메라 흔들림
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (EnemyHitCameraShake)
		{
			PC->ClientStartCameraShake(EnemyHitCameraShake, bKilled ? EnemyHitShakeScale * 1.5f : EnemyHitShakeScale);
		}
	}

	if (PlayerHUDWidget)
	{
		if (UWidget* HitMarker = PlayerHUDWidget->GetWidgetFromName(TEXT("HitMarker")))
		{
			HitMarker->SetVisibility(ESlateVisibility::HitTestInvisible);
			// 처치 시 조금 더 크게 펄스
			HitPulseStartScale = bKilled ? HitMarkerPulseScale * 1.25f : HitMarkerPulseScale;
			HitMarker->SetRenderScale(FVector2D(HitPulseStartScale, HitPulseStartScale));
		}
	}

	if (HitMarkerSound)
	{
		UGameplayStatics::PlaySound2D(this, HitMarkerSound);
	}
}

void ABelicaCharacter::UpdateHitFeedback(float DeltaTime)
{
	if (HitFeedbackRemaining <= 0.0f)
	{
		return;
	}

	HitFeedbackRemaining -= DeltaTime;
	UWidget* HitMarker = PlayerHUDWidget ? PlayerHUDWidget->GetWidgetFromName(TEXT("HitMarker")) : nullptr;

	if (HitFeedbackRemaining <= 0.0f)
	{
		HitFeedbackRemaining = 0.0f;
		SetCrosshairColor(CrosshairColor);
		SetCrosshairScale(1.0f);
		if (HitMarker)
		{
			HitMarker->SetVisibility(ESlateVisibility::Hidden);
			HitMarker->SetRenderScale(FVector2D(1.0f, 1.0f));
		}
		return;
	}

	// 펄스: 시작 배율 → 1.0 으로 수축 (히트마커 + 크로스헤어)
	const float Alpha = HitFeedbackRemaining / FMath::Max(HitMarkerDuration, 0.01f);
	if (HitMarker)
	{
		const float Scale = FMath::Lerp(1.0f, HitPulseStartScale, Alpha);
		HitMarker->SetRenderScale(FVector2D(Scale, Scale));
	}
	SetCrosshairScale(FMath::Lerp(1.0f, CrosshairPulseScale, Alpha));
}

int32 ABelicaCharacter::AddReserveAmmo(int32 Amount)
{
	const int32 Added = FMath::Clamp(Amount, 0, FMath::Max(0, MaxReserveAmmo - ReserveAmmo));
	ReserveAmmo += Added;
	return Added;
}

void ABelicaCharacter::Reload()
{
	if (bIsReloading || CurrentAmmo >= MaxAmmo || ReserveAmmo <= 0)
	{
		return;
	}

	bIsReloading = true;

	// 질주 중이면 질주 해제 (장전 동작과 겹치지 않게)
	if (IsSprint)
	{
		StopSprint();
	}

	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (ReloadMontage && AnimInstance && PlayAnimMontage(ReloadMontage) > 0.0f)
	{
		FOnMontageEnded EndDelegate;
		EndDelegate.BindUObject(this, &ABelicaCharacter::OnReloadMontageEnded);
		AnimInstance->Montage_SetEndDelegate(EndDelegate, ReloadMontage);
	}
	else
	{
		// 몽타주가 없거나 재생 실패 시 bIsReloading이 영구히 true로 남지 않도록 즉시 완료
		OnReloadMontageEnded(nullptr, false);
	}
}

void ABelicaCharacter::OnReloadMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (!bInterrupted)
	{
		const int32 NeededAmmo = MaxAmmo - CurrentAmmo;
		const int32 AmmoToReload = FMath::Min(NeededAmmo, ReserveAmmo);
		CurrentAmmo += AmmoToReload;
		ReserveAmmo -= AmmoToReload;
	}

	bIsReloading = false;
}

bool ABelicaCharacter::ConsumeStamina(float Cost)
{
	if (CurrentStamina >= Cost)
	{
		CurrentStamina = FMath::Clamp(CurrentStamina - Cost, 0.0f, MaxStamina);
		return true;
	}
	return false;
}

void ABelicaCharacter::RecoverStamina()
{
	if (!IsSprint && CurrentStamina < MaxStamina)
	{
		CurrentStamina = FMath::Clamp(CurrentStamina + 5.0f, 0.0f, MaxStamina);
	}
}


void ABelicaCharacter::TogglePauseMenu()
{
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC || !PauseMenuWidgetClass)
    {
        return;
    }

    // 이미 일시정지 중이라면 게임 재개
    if (UGameplayStatics::IsGamePaused(this))
    {
        UGameplayStatics::SetGamePaused(this, false);

        PC->SetInputMode(FInputModeGameOnly());
        PC->bShowMouseCursor = false;

        if (IsValid(PauseMenuWidget))
        {
            PauseMenuWidget->RemoveFromParent();
            PauseMenuWidget = nullptr;
        }

        return;
    }

    // 일시정지 메뉴 생성
    if (!IsValid(PauseMenuWidget) || !PauseMenuWidget->IsInViewport())
    {
        PauseMenuWidget = CreateWidget<UUserWidget>(
            PC, PauseMenuWidgetClass
        );
    }

    if (!PauseMenuWidget)
    {
        return;
    }

    // 메뉴 표시
    if (!PauseMenuWidget->IsInViewport())
    {
        PauseMenuWidget->AddToViewport(10);
    }

    // 마우스 표시 및 UI 입력 활성화
    FInputModeGameAndUI InputMode;
    InputMode.SetWidgetToFocus(PauseMenuWidget->TakeWidget());
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);

    PC->SetInputMode(InputMode);
    PC->bShowMouseCursor = true;

    // 게임 일시정지
    UGameplayStatics::SetGamePaused(this, true);
}


void ABelicaCharacter::ShowGameOverMenu()
{
    APlayerController* PC = Cast<APlayerController>(GetController());

    if (!PC || !GameOverWidgetClass)
    {
        return;
    }

    if (IsValid(GameOverWidget) && GameOverWidget->IsInViewport())
    {
        return;
    }

    GameOverWidget = CreateWidget<UUserWidget>(
        PC, GameOverWidgetClass
    );

    if (!GameOverWidget)
    {
        return;
    }

    GameOverWidget->AddToViewport(20);

    FInputModeUIOnly InputMode;
    InputMode.SetWidgetToFocus(GameOverWidget->TakeWidget());
    InputMode.SetLockMouseToViewportBehavior(
        EMouseLockMode::DoNotLock
    );

    PC->SetInputMode(InputMode);
    PC->bShowMouseCursor = true;
}

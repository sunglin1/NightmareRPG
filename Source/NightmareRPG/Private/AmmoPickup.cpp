#include "AmmoPickup.h"
#include "BelicaCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

AAmmoPickup::AAmmoPickup()
{
	PrimaryActorTick.bCanEverTick = false;

	InteractSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractSphere"));
	InteractSphere->InitSphereRadius(150.0f);
	InteractSphere->SetCollisionProfileName(TEXT("Trigger"));
	RootComponent = InteractSphere;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	// 벙커 에셋팩의 탄약 상자 메시를 기본값으로 사용
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BoxMesh(
		TEXT("/Game/Abandoned_Bunker/Mesh/SM_Carteidge_box_1.SM_Carteidge_box_1"));
	if (BoxMesh.Succeeded())
	{
		Mesh->SetStaticMesh(BoxMesh.Object);
	}

	GlowLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("GlowLight"));
	GlowLight->SetupAttachment(RootComponent);
	GlowLight->SetRelativeLocation(FVector(0.0f, 0.0f, 40.0f));
	GlowLight->SetIntensity(800.0f);
	GlowLight->SetAttenuationRadius(200.0f);
	GlowLight->SetLightColor(FLinearColor(1.0f, 0.55f, 0.15f));
	GlowLight->SetCastShadows(false);
}

void AAmmoPickup::BeginPlay()
{
	Super::BeginPlay();

	InteractSphere->OnComponentBeginOverlap.AddDynamic(this, &AAmmoPickup::OnTriggerBegin);
	InteractSphere->OnComponentEndOverlap.AddDynamic(this, &AAmmoPickup::OnTriggerEnd);

	// 이미 범위 안에서 시작한 경우 처리
	TArray<AActor*> Overlapping;
	InteractSphere->GetOverlappingActors(Overlapping, ABelicaCharacter::StaticClass());
	for (AActor* Actor : Overlapping)
	{
		Cast<ABelicaCharacter>(Actor)->SetOverlappingAmmo(this);
	}
}

void AAmmoPickup::OnTriggerBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (ABelicaCharacter* Belica = Cast<ABelicaCharacter>(OtherActor))
	{
		if (!bCollected)
		{
			Belica->SetOverlappingAmmo(this);
		}
	}
}

void AAmmoPickup::OnTriggerEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	if (ABelicaCharacter* Belica = Cast<ABelicaCharacter>(OtherActor))
	{
		Belica->ClearOverlappingAmmo(this);
	}
}

bool AAmmoPickup::Collect(ABelicaCharacter* Collector)
{
	if (bCollected || !Collector)
	{
		return false;
	}

	bCollected = true;
	Collector->AddReserveAmmo(AmmoAmount);

	if (PickupSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, PickupSound, GetActorLocation());
	}

	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	Destroy();
	return true;
}

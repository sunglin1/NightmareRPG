#include "AmmoPickup.h"
#include "BelicaCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"

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

// ===== 상호작용 프롬프트 ("[F] 탄창 획득") =====
// 헤더 변경 없이(라이브 코딩 가능) WBP_PlayerHUD의 루트 패널에 텍스트 블록을 런타임으로 추가해 사용한다.
namespace AmmoPickupPrompt
{
	static const FName PromptWidgetName(TEXT("Text_InteractPrompt"));

	// 뷰포트에 떠 있는 플레이어 HUD (Text_Ammo 위젯을 가진 WBP_PlayerHUD)
	static UUserWidget* FindPlayerHUD(UObject* WorldContext)
	{
		TArray<UUserWidget*> Widgets;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(WorldContext, Widgets, UUserWidget::StaticClass(), true);
		for (UUserWidget* Widget : Widgets)
		{
			if (Widget && Widget->GetWidgetFromName(TEXT("Text_Ammo")))
			{
				return Widget;
			}
		}
		return nullptr;
	}

	static UTextBlock* GetOrCreatePrompt(UUserWidget* HUD)
	{
		if (!HUD)
		{
			return nullptr;
		}
		if (UTextBlock* Existing = Cast<UTextBlock>(HUD->GetWidgetFromName(PromptWidgetName)))
		{
			return Existing;
		}

		UPanelWidget* Root = HUD->WidgetTree ? Cast<UPanelWidget>(HUD->WidgetTree->RootWidget) : nullptr;
		if (!Root)
		{
			return nullptr;
		}

		UTextBlock* Text = HUD->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), PromptWidgetName);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = 22;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.86f, 0.62f)));
		Text->SetShadowOffset(FVector2D(1.5f, 1.5f));
		Text->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f));
		Text->SetJustification(ETextJustify::Center);
		Text->SetVisibility(ESlateVisibility::Collapsed);

		UPanelSlot* Slot = Root->AddChild(Text);
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
		{
			CanvasSlot->SetAnchors(FAnchors(0.5f, 0.68f));
			CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			CanvasSlot->SetPosition(FVector2D::ZeroVector);
			CanvasSlot->SetAutoSize(true);
			CanvasSlot->SetZOrder(50);
		}
		return Text;
	}

	static int32 ReadIntProperty(const ABelicaCharacter* Belica, const TCHAR* Name, int32 Fallback)
	{
		if (const FIntProperty* Prop = FindFProperty<FIntProperty>(ABelicaCharacter::StaticClass(), Name))
		{
			return Prop->GetPropertyValue_InContainer(Belica);
		}
		return Fallback;
	}

	static void Show(ABelicaCharacter* Belica)
	{
		if (UTextBlock* Text = GetOrCreatePrompt(FindPlayerHUD(Belica)))
		{
			const bool bFull = ReadIntProperty(Belica, TEXT("ReserveAmmo"), 0) >= ReadIntProperty(Belica, TEXT("MaxReserveAmmo"), MAX_int32);
			// "[F] 탄창 획득" / "예비 탄약이 가득 찼습니다"
			Text->SetText(bFull
				? FText::FromString(TEXT("예비 탄약이 가득 찼습니다"))
				: FText::FromString(TEXT("[F] 탄창 획득")));
			Text->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}

	static void Hide(ABelicaCharacter* Belica)
	{
		if (UUserWidget* HUD = FindPlayerHUD(Belica))
		{
			if (UWidget* Text = HUD->GetWidgetFromName(PromptWidgetName))
			{
				Text->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}

	// 아직 겹쳐 있는 다른(미획득) 픽업 찾기 - 여러 픽업이 겹칠 때 F 대상이 사라지지 않도록
	static AAmmoPickup* FindOtherOverlappingPickup(ABelicaCharacter* Belica, const AAmmoPickup* Exclude)
	{
		TArray<AActor*> Overlapping;
		Belica->GetOverlappingActors(Overlapping, AAmmoPickup::StaticClass());
		AAmmoPickup* Best = nullptr;
		float BestDistSq = TNumericLimits<float>::Max();
		for (AActor* Actor : Overlapping)
		{
			AAmmoPickup* Candidate = Cast<AAmmoPickup>(Actor);
			if (!Candidate || Candidate == Exclude || Candidate->IsActorBeingDestroyed() || !Candidate->GetActorEnableCollision())
			{
				continue;
			}
			const float DistSq = FVector::DistSquared(Candidate->GetActorLocation(), Belica->GetActorLocation());
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				Best = Candidate;
			}
		}
		return Best;
	}
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
		ABelicaCharacter* Belica = Cast<ABelicaCharacter>(Actor);
		Belica->SetOverlappingAmmo(this);
		AmmoPickupPrompt::Show(Belica);
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
			AmmoPickupPrompt::Show(Belica);
		}
	}
}

void AAmmoPickup::OnTriggerEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	if (ABelicaCharacter* Belica = Cast<ABelicaCharacter>(OtherActor))
	{
		Belica->ClearOverlappingAmmo(this);

		// 다른 픽업 범위 안에 아직 있으면 그 픽업을 F 대상으로 넘기고 프롬프트 유지
		if (AAmmoPickup* Other = AmmoPickupPrompt::FindOtherOverlappingPickup(Belica, this))
		{
			Belica->SetOverlappingAmmo(Other);
			AmmoPickupPrompt::Show(Belica);
		}
		else
		{
			AmmoPickupPrompt::Hide(Belica);
		}
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

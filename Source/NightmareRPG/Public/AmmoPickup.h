#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AmmoPickup.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class USoundBase;
class ABelicaCharacter;

// 탄약 상자: Belica가 범위 안에서 F(IA_Interact)를 누르면 예비 탄약을 보충하고 사라짐
UCLASS()
class NIGHTMARERPG_API AAmmoPickup : public AActor
{
	GENERATED_BODY()

public:
	AAmmoPickup();

	// Belica::Interact()에서 호출. 성공 시 true (탄약 지급 + 픽업 제거)
	UFUNCTION(BlueprintCallable, Category = "Ammo")
	bool Collect(ABelicaCharacter* Collector);

	int32 GetAmmoAmount() const { return AmmoAmount; }

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnTriggerBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnTriggerEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ammo")
	USphereComponent* InteractSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ammo")
	UStaticMeshComponent* Mesh;

	// 어두운 벙커에서 상자를 찾을 수 있도록 은은한 발광
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ammo")
	UPointLightComponent* GlowLight;

	// 지급할 예비 탄약 수 (기존 리플렉션 조회와 동일한 이름 유지)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo", meta = (ClampMin = "1"))
	int32 AmmoAmount = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo")
	USoundBase* PickupSound = nullptr;

private:
	bool bCollected = false;
};

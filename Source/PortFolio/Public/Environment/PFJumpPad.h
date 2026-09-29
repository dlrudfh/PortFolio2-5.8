#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PFJumpPad.generated.h"

class APFCharacter;
class UBoxComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;

// 지정 지점으로 발사하는 점프대
UCLASS()
class PORTFOLIO_API APFJumpPad : public AActor
{
	GENERATED_BODY()

public:
	APFJumpPad();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Jump Pad")
	TObjectPtr<UStaticMeshComponent> Platform;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Jump Pad")
	TObjectPtr<UBoxComponent> Trigger;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jump Pad", meta = (MakeEditWidget))
	FVector LandingOffset = FVector(5000.f, 0.f, 0.f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jump Pad", meta = (ClampMin = "0.8", ClampMax = "6.0"))
	float FlightDuration = 4.f;

private:
	void TryLaunch(APFCharacter* OtherCharacter);
	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex);

	TSet<TWeakObjectPtr<APFCharacter>> LaunchedCharacters;
};

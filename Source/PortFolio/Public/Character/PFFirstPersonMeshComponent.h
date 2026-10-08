#pragma once

#include "CoreMinimal.h"
#include "Components/SkeletalMeshComponent.h"
#include "PFFirstPersonMeshComponent.generated.h"

class UCameraComponent;

// 소유 플레이어의 팔, 무기 표시
UCLASS()
class PORTFOLIO_API UPFFirstPersonMeshComponent : public USkeletalMeshComponent
{
	GENERATED_BODY()

public:
	UPFFirstPersonMeshComponent();
	void InitializeView(USkeletalMeshComponent* Source, UCameraComponent* Camera, const FVector& Offset);
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	// 위치 보정 기준 카메라
	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> ViewCamera;
	FVector NeckOffset = FVector::ZeroVector;
};

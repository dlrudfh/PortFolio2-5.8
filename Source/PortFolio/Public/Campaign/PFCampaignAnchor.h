#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PFCampaignAnchor.generated.h"

UENUM(BlueprintType)
enum class EPFCampaignProp : uint8 { None, Control, Beacon, Core, Guide };

// 캠페인 배치 기준, 장치 외형
UCLASS()
class PORTFOLIO_API APFCampaignAnchor : public AActor
{
	GENERATED_BODY()
public:
	APFCampaignAnchor();
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;
	UPROPERTY(EditAnywhere, Category="Campaign") FName Id;
	UPROPERTY(EditAnywhere, Category="Campaign") EPFCampaignProp Prop = EPFCampaignProp::None;
	UPROPERTY(VisibleAnywhere, Category="Campaign") TObjectPtr<class UStaticMeshComponent> Visual;
	UPROPERTY(VisibleAnywhere, Category="Campaign") TObjectPtr<class UStaticMeshComponent> Ring;
	UPROPERTY(VisibleAnywhere, Category="Campaign") TObjectPtr<class URotatingMovementComponent> RingMovement;
	UPROPERTY(VisibleAnywhere, Category="Campaign") TObjectPtr<class UTextRenderComponent> Status;
	UPROPERTY(EditAnywhere, Category="Campaign") TObjectPtr<class UStaticMesh> MeshOverride;
	void SetActiveVisual(bool bActive);
private:
	bool bVisualInitialized = false;
	bool bActiveVisual = false;
};

// 고정 경로를 따라 이동하는 연출 정의
UCLASS()
class PORTFOLIO_API APFCampaignShot : public AActor
{
	GENERATED_BODY()
public:
	APFCampaignShot();
	UPROPERTY(EditAnywhere, Category="Campaign") FName Id;
	UPROPERTY(EditAnywhere, Category="Campaign", meta=(ClampMin="1", ClampMax="10")) float Duration = 4.f;
	UPROPERTY(EditAnywhere, Category="Campaign") float FieldOfView = 60.f;
	UPROPERTY(EditAnywhere, Category="Campaign") TObjectPtr<APFCampaignAnchor> LookAt;
	UPROPERTY(EditAnywhere, Category="Campaign") TObjectPtr<APFCampaignAnchor> EndLookAt;
	UPROPERTY(VisibleAnywhere, Category="Campaign") TObjectPtr<class USplineComponent> Path;
	void Evaluate(float Alpha, FVector& Location, FRotator& Rotation) const;
};

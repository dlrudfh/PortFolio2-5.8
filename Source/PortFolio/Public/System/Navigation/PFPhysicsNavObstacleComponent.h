#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Navigation/CrowdAgentInterface.h"
#include "PFPhysicsNavObstacleComponent.generated.h"

class UCrowdManager;
class UPrimitiveComponent;

// 물리 메시의 수동 Crowd 장애물 등록
UCLASS(ClassGroup = Navigation, meta = (BlueprintSpawnableComponent))
class PORTFOLIO_API UPFPhysicsNavObstacleComponent : public UActorComponent, public ICrowdAgentInterface
{
	GENERATED_BODY()

public:
	UPFPhysicsNavObstacleComponent();
	void SetObstacleMesh(UPrimitiveComponent* Mesh);
	UPrimitiveComponent* GetObstacleMesh() const;
	virtual FVector GetCrowdAgentLocation() const override;
	virtual FVector GetCrowdAgentVelocity() const override;
	virtual void GetCrowdAgentCollisions(float& CylinderRadius, float& CylinderHalfHeight) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void OnUnregister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void RegisterCrowdObstacle();

	// 물리 충돌 메시
	UPROPERTY(VisibleAnywhere, Category = Navigation)
	TObjectPtr<UPrimitiveComponent> ObstacleMesh;

	UPROPERTY(Transient)
	TWeakObjectPtr<UCrowdManager> RegisteredManager;

	float RegisteredRadius = -1.f;
	float RegisteredHalfHeight = -1.f;
};

#include "System/Navigation/PFPhysicsNavObstacleComponent.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "Navigation/CrowdManager.h"

UPFPhysicsNavObstacleComponent::UPFPhysicsNavObstacleComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.25f;
}

// 회피 위치, 크기의 기준 메시 지정
void UPFPhysicsNavObstacleComponent::SetObstacleMesh(UPrimitiveComponent* Mesh)
{
	ObstacleMesh = Mesh;
}

// 등록된 물리 메시 조회
UPrimitiveComponent* UPFPhysicsNavObstacleComponent::GetObstacleMesh() const
{
	return ObstacleMesh;
}

void UPFPhysicsNavObstacleComponent::BeginPlay()
{
	Super::BeginPlay();
	SetComponentTickEnabled(GetOwner()->HasAuthority());
	if (ObstacleMesh)
	{
		ObstacleMesh->SetCanEverAffectNavigation(false);
	}
	RegisterCrowdObstacle();
}

void UPFPhysicsNavObstacleComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UCrowdManager* Manager = RegisteredManager.Get())
	{
		Manager->UnregisterAgent(this);
	}
	RegisteredManager.Reset();
	Super::EndPlay(EndPlayReason);
}

void UPFPhysicsNavObstacleComponent::OnUnregister()
{
	if (UCrowdManager* Manager = RegisteredManager.Get())
	{
		Manager->UnregisterAgent(this);
	}
	RegisteredManager.Reset();
	Super::OnUnregister();
}

void UPFPhysicsNavObstacleComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	RegisterCrowdObstacle();
	if (UCrowdManager* Manager = RegisteredManager.Get())
	{
		float Radius;
		float HalfHeight;
		GetCrowdAgentCollisions(Radius, HalfHeight);
		if (!FMath::IsNearlyEqual(Radius, RegisteredRadius, 0.1f)
			|| !FMath::IsNearlyEqual(HalfHeight, RegisteredHalfHeight, 0.1f))
		{
			Manager->UpdateAgentParams(this);
			RegisteredRadius = Radius;
			RegisteredHalfHeight = HalfHeight;
		}
	}
}

// Crowd 초기화 이후 장애물 등록
void UPFPhysicsNavObstacleComponent::RegisterCrowdObstacle()
{
	if (!ObstacleMesh || !GetOwner()->HasAuthority())
	{
		return;
	}
	UCrowdManager* Manager = UCrowdManager::GetCurrent(GetWorld());
	if (Manager && RegisteredManager.Get() != Manager)
	{
		if (UCrowdManager* Previous = RegisteredManager.Get())
		{
			Previous->UnregisterAgent(this);
		}
		Manager->RegisterAgent(this);
		RegisteredManager = Manager;
		GetCrowdAgentCollisions(RegisteredRadius, RegisteredHalfHeight);
	}
}

FVector UPFPhysicsNavObstacleComponent::GetCrowdAgentLocation() const
{
	return ObstacleMesh
		? ObstacleMesh->Bounds.Origin - FVector(0, 0, ObstacleMesh->Bounds.BoxExtent.Z) : FVector::ZeroVector;
}

FVector UPFPhysicsNavObstacleComponent::GetCrowdAgentVelocity() const
{
	return ObstacleMesh ? ObstacleMesh->GetComponentVelocity() : FVector::ZeroVector;
}

void UPFPhysicsNavObstacleComponent::GetCrowdAgentCollisions(float& CylinderRadius, float& CylinderHalfHeight) const
{
	const FVector Extent = ObstacleMesh ? ObstacleMesh->Bounds.BoxExtent : FVector::ZeroVector;
	CylinderRadius = ObstacleMesh ? ObstacleMesh->Bounds.SphereRadius : 0.f;
	CylinderHalfHeight = Extent.Z;
}

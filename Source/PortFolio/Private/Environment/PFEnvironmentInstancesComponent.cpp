#include "Environment/PFEnvironmentInstancesComponent.h"

#include "AI/Navigation/NavCollisionBase.h"
#include "AI/Navigation/NavigationRelevantData.h"
#include "Engine/StaticMesh.h"

void UPFEnvironmentInstancesComponent::GetNavigationData(FNavigationRelevantData& Data) const
{
	Super::GetNavigationData(Data);
	const UStaticMesh* Mesh = GetStaticMesh();
	if (!Mesh || Mesh->IsCompiling())
	{
		return;
	}

	UNavCollisionBase* Collision = Mesh->GetNavCollision();
	if (Collision && Collision->HasSurfaceAreaClass() && !ShouldExportAsObstacle(*Collision))
	{
		// 기본 HISM에서 누락되는 표면 영역을 인스턴스 지형에 전달
		Collision->GetNavigationModifier(Data.Modifiers, FTransform::Identity);
	}
}

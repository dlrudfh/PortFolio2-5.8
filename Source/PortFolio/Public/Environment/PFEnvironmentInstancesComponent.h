#pragma once

#include "CoreMinimal.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "PFEnvironmentInstancesComponent.generated.h"

// 메시의 보행 표면 설정을 유지하는 환경 인스턴스
UCLASS()
class PORTFOLIO_API UPFEnvironmentInstancesComponent : public UHierarchicalInstancedStaticMeshComponent
{
	GENERATED_BODY()

public:
	virtual void GetNavigationData(FNavigationRelevantData& Data) const override;
};

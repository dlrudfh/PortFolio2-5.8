#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PFEnvironmentModule.generated.h"

class UHierarchicalInstancedStaticMeshComponent;

// 반복 지형 모듈 인스턴스
UCLASS()
class PORTFOLIO_API APFEnvironmentModule : public AActor
{
	GENERATED_BODY()

public:
	APFEnvironmentModule();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Environment)
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Instances;
};

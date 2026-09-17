#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PFNavigationSetup.generated.h"

class APFCharacter;

// NavMesh, 프로젝트 이동 링크 생성
UCLASS()
class PORTFOLIO_API APFNavigationSetup : public AActor
{
	GENERATED_BODY()

public:
	APFNavigationSetup();

	UFUNCTION(CallInEditor, Category = Navigation, meta = (DisplayName = "Build Navigation and Traversal Links", DisplayPriority = "1"))
	void PrepareStaticNavigation();

	UFUNCTION(CallInEditor, Category = Navigation, meta = (DisplayName = "Delete All Traversal Links", DisplayPriority = "2"))
	void DeleteAllTraversalLinks();

protected:
	// 점프 생성에 사용할 봇 기본 설정
	UPROPERTY(EditAnywhere, Category = Navigation)
	TSubclassOf<APFCharacter> BotClass;

	UPROPERTY(VisibleAnywhere, Category = Navigation)
	FString PreparationResult;
};

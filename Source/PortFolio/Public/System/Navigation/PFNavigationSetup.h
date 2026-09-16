#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PFNavigationSetup.generated.h"

class APFCharacter;

// Static NavMesh 설정 준비
UCLASS()
class PORTFOLIO_API APFNavigationSetup : public AActor
{
	GENERATED_BODY()

public:
	APFNavigationSetup();

	UFUNCTION(CallInEditor, Category = Navigation, meta = (DisplayName = "Prepare Static Navigation"))
	void PrepareStaticNavigation();

protected:
	// 점프 생성에 사용할 봇 기본 설정
	UPROPERTY(EditAnywhere, Category = Navigation)
	TSubclassOf<APFCharacter> BotClass;

	UPROPERTY(VisibleAnywhere, Category = Navigation)
	FString PreparationResult;
};

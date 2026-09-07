
#pragma once

#include "PortFolio/PortFolio.h"
#include "GameFramework/GameModeBase.h"
#include "PFGameMode.generated.h"

class UCapsuleComponent;
struct FCollisionQueryParams;

// 플레이어 생성, 교체 관리 클래스
UCLASS()
class PORTFOLIO_API APFGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
public:
	APFGameMode();
	static bool FindSpawnTransform(UWorld* World, const UCapsuleComponent* Capsule,
		const FCollisionQueryParams& QueryParams, int32& RemainingAttempts,
		bool bCheckBlockingCollision, FTransform& OutSpawnTransform);

	virtual void BeginPlay() override;

	virtual void PostLogin(APlayerController* NewPlayer) override;

	void RespawnPlayer(TWeakObjectPtr<APlayerController> PlayerController);

	virtual void ChangeCharacter(AController* Controller);

	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* Controller) override;

private:
	bool TryFindInitialPlayerSpawnTransform(APlayerController* NewPlayer, FTransform& OutSpawnTransform);
};

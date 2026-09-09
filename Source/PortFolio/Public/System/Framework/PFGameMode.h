
#pragma once

#include "PortFolio/PortFolio.h"
#include "GameFramework/GameModeBase.h"
#include "PFGameMode.generated.h"

class APFPlayerController;
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

	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void RestartPlayer(AController* NewPlayer) override;

	void RequestInitialSpawn(APFPlayerController* NewPlayer, ECHARACTER SelectedCharacter);

	void RespawnPlayer(TWeakObjectPtr<APlayerController> PlayerController);

	virtual void ChangeCharacter(AController* Controller);

	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* Controller) override;

private:
	bool UsesInitialSpawnFlow(AController* Controller) const;
	void TryStartInitialPlayer(APFPlayerController* NewPlayer);
	bool TryFindInitialPlayerSpawnTransform(APlayerController* NewPlayer, FTransform& OutSpawnTransform);
};

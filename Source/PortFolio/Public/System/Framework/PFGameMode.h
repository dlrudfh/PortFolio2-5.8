
#pragma once

#include "PortFolio/PortFolio.h"
#include "GameFramework/GameModeBase.h"
#include "PFGameMode.generated.h"

class APFPlayerController;
class APFCharacter;
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
		bool bCheckBlockingCollision, FTransform& OutSpawnTransform,
		const FVector* SpawnCenter = nullptr, float SpawnRadius = 0.f);

	virtual void BeginPlay() override;
	virtual void InitGameState() override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;

	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void RestartPlayer(AController* NewPlayer) override;

	void RequestInitialSpawn(APFPlayerController* NewPlayer, ECHARACTER SelectedCharacter);
	void ChangeTestMap();
	static bool ServerTravel(UWorld* World, const FString& URL);
	APFCharacter* SpawnEnemy(APFCharacter* ControlledPawn, UClass* EnemyClass);
	APFCharacter* SpawnCampaignEnemy(UClass* EnemyClass, const FTransform& GroundTransform);
	bool RespawnCampaignPlayer(APFPlayerController* Player);
	void RetryCampaignSpawns();
	bool IsCampaignPartyReady() const;

	void RespawnPlayer(TWeakObjectPtr<APlayerController> PlayerController);

	virtual void ChangeCharacter(class APFPlayerController* Controller);

	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* Controller) override;

private:
	bool UsesInitialSpawnFlow(AController* Controller) const;
	void TryStartInitialPlayer(APFPlayerController* NewPlayer);
	bool TryFindInitialPlayerSpawnTransform(APlayerController* NewPlayer, FTransform& OutSpawnTransform);

	// 맵 전환 대상, 패키징 참조
	UPROPERTY()
	TSoftObjectPtr<UWorld> TutorialMap;
	UPROPERTY()
	TSoftObjectPtr<UWorld> TestMap3;
	UPROPERTY()
	TArray<TSoftObjectPtr<UWorld>> AdditionalTestMaps;
	UPROPERTY()
	TArray<TSoftObjectPtr<class UPFCampaignDefinition>> CampaignDefinitions;
};

#pragma once

#include "PortFolio/PortFolio.h"
#include "GameFramework/GameModeBase.h"
#include "PFLobbyGameMode.generated.h"

class APFLobbyPlayerController;
class APFPlayerState;

// 캐릭터 선택 로비 관리
UCLASS()
class PORTFOLIO_API APFLobbyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	APFLobbyGameMode();
	virtual void InitGameState() override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	void SelectCharacter(APFLobbyPlayerController* Player, ECHARACTER Character);
	void SetReady(APFLobbyPlayerController* Player, bool bReady, int32 SettingsRevision);
	void SelectMap(APFLobbyPlayerController* Player, FName Map);
	void StartMatch(APFLobbyPlayerController* Player);
	void LeaveLobby(APFLobbyPlayerController* Player);

private:
	APFPlayerState* GetLobbyPlayer(APFLobbyPlayerController* Player) const;
	void ResetGuestReadiness();
	bool bClosingLobby = false;
};

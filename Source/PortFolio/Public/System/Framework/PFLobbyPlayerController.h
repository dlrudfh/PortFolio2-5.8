#pragma once

#include "PortFolio/PortFolio.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "PFLobbyPlayerController.generated.h"

// 로비 입력, 참가자 요청 전달
UCLASS()
class PORTFOLIO_API APFLobbyPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	UFUNCTION(Server, Reliable)
	void Server_SelectCharacter(ECHARACTER SelectedCharacter);
	UFUNCTION(Server, Reliable)
	void Server_SetReady(bool bReady, int32 SettingsRevision);
	UFUNCTION(Server, Reliable)
	void Server_SelectMap(FName Map);
	UFUNCTION(Server, Reliable)
	void Server_StartMatch();
	UFUNCTION(Server, Reliable)
	void Server_LeaveLobby();
	UFUNCTION(Client, Reliable)
	void Client_LeaveLobby();

protected:
	virtual void BeginPlay() override;
	virtual void OnRep_PlayerState() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void SynchronizeUsername();
	UFUNCTION(Server, Reliable)
	void Server_SetUsername(const FString& Username);
	FTimerHandle UsernameSyncTimerHandle;
};

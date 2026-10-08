#include "System/Framework/PFLobbyPlayerController.h"

#include "Engine/World.h"
#include "System/Framework/PFGameInstance.h"
#include "System/Framework/PFLobbyGameMode.h"
#include "System/Framework/PFPlayerState.h"
#include "UI/PFGameCursor.h"

void APFLobbyPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController()) return;
	FPFGameCursor::Install(GetWorld()->GetGameViewport());
	if (UPFGameInstance* GI = GetGameInstance<UPFGameInstance>())
	{
		GI->ApplyMenuVolume();
		GI->CreateTitle();
	}
	SynchronizeUsername();
	GetWorldTimerManager().SetTimer(UsernameSyncTimerHandle, this, &APFLobbyPlayerController::SynchronizeUsername, 1.f, true);
}

void APFLobbyPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	SynchronizeUsername();
}

void APFLobbyPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(UsernameSyncTimerHandle);
	if (IsLocalController())
		if (UPFGameInstance* GI = GetGameInstance<UPFGameInstance>()) GI->RemoveTitle();
	Super::EndPlay(EndPlayReason);
}

// 로컬 온라인 이름 동기화
void APFLobbyPlayerController::SynchronizeUsername()
{
	const APFPlayerState* State = GetPlayerState<APFPlayerState>();
	const UPFGameInstance* GI = GetGameInstance<UPFGameInstance>();
	if (!IsLocalController() || !State || !GI) return;
	const FString Username = GI->GetOnlineUsername();
	if (!Username.IsEmpty() && State->GetPlayerName() != Username) Server_SetUsername(Username);
}

// 서버 플레이어 이름 반영
void APFLobbyPlayerController::Server_SetUsername_Implementation(const FString& Username)
{
	FString Name = Username.TrimStartAndEnd();
	if (Name.IsEmpty() || Name.Len() > 128) return;
	if (APFPlayerState* State = GetPlayerState<APFPlayerState>()) State->SetPlayerName(Name);
}

// 서버에 캐릭터 선택 요청
void APFLobbyPlayerController::Server_SelectCharacter_Implementation(ECHARACTER SelectedCharacter)
{
	if (APFLobbyGameMode* Lobby = GetWorld()->GetAuthGameMode<APFLobbyGameMode>()) Lobby->SelectCharacter(this, SelectedCharacter);
}

// 서버에 준비 상태 요청
void APFLobbyPlayerController::Server_SetReady_Implementation(bool bReady, int32 SettingsRevision)
{
	if (APFLobbyGameMode* Lobby = GetWorld()->GetAuthGameMode<APFLobbyGameMode>()) Lobby->SetReady(this, bReady, SettingsRevision);
}

// 서버에 대전 맵 변경 요청
void APFLobbyPlayerController::Server_SelectMap_Implementation(FName Map)
{
	if (APFLobbyGameMode* Lobby = GetWorld()->GetAuthGameMode<APFLobbyGameMode>()) Lobby->SelectMap(this, Map);
}

// 서버에 게임 시작 요청
void APFLobbyPlayerController::Server_StartMatch_Implementation()
{
	if (APFLobbyGameMode* Lobby = GetWorld()->GetAuthGameMode<APFLobbyGameMode>()) Lobby->StartMatch(this);
}

// 서버에 로비 퇴장 요청
void APFLobbyPlayerController::Server_LeaveLobby_Implementation()
{
	if (APFLobbyGameMode* Lobby = GetWorld()->GetAuthGameMode<APFLobbyGameMode>()) Lobby->LeaveLobby(this);
}

// 세션을 정리하고 타이틀로 복귀
void APFLobbyPlayerController::Client_LeaveLobby_Implementation()
{
	if (UPFGameInstance* GI = GetGameInstance<UPFGameInstance>()) GI->ReturnToMainMenu();
}

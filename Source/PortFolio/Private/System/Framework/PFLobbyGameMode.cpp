#include "System/Framework/PFLobbyGameMode.h"

#include "Engine/World.h"
#include "GameFramework/GameSession.h"
#include "System/Framework/PFGameInstance.h"
#include "System/Framework/PFLobbyPlayerController.h"
#include "System/Framework/PFPlayerState.h"
#include "System/Framework/PFSessionGameState.h"

APFLobbyGameMode::APFLobbyGameMode()
{
	PlayerControllerClass = APFLobbyPlayerController::StaticClass();
	PlayerStateClass = APFPlayerState::StaticClass();
	GameStateClass = APFSessionGameState::StaticClass();
	DefaultPawnClass = nullptr;
	HUDClass = nullptr;
	bUseSeamlessTravel = true;
}

void APFLobbyGameMode::InitGameState()
{
	Super::InitGameState();
	APFSessionGameState* Session = GetGameState<APFSessionGameState>();
	const UPFGameInstance* GI = GetGameInstance<UPFGameInstance>();
	if (!Session || !GI) return;
	Session->Mode = GI->GetPendingSessionMode() == EPFSessionMode::Versus ? EPFSessionMode::Versus : EPFSessionMode::Story;
	Session->Capacity = FMath::Clamp(GI->GetPendingSessionCapacity(), 1, APFSessionGameState::GetMaxPlayers(Session->Mode));
	Session->RoomName = GI->GetSessionRoomName();
	Session->SelectedMap = TEXT("Map3");
	Session->Phase = EPFSessionPhase::Lobby;
	Session->bInitialized = true;
	Session->NotifySessionChanged();
}

void APFLobbyGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (!ErrorMessage.IsEmpty()) return;
	const APFSessionGameState* Session = GetGameState<APFSessionGameState>();
	if (!Session || !Session->bInitialized || Session->Phase != EPFSessionPhase::Lobby || bClosingLobby)
		ErrorMessage = TEXT("The session is no longer accepting players.");
	else if (GetNumPlayers() >= Session->Capacity)
		ErrorMessage = TEXT("The session is full.");
}

void APFLobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	APFSessionGameState* Session = GetGameState<APFSessionGameState>();
	APFPlayerState* Player = NewPlayer ? NewPlayer->GetPlayerState<APFPlayerState>() : nullptr;
	if (!Session || !Player) return;
	TSet<int32> OccupiedSlots;
	for (const APlayerState* Entry : Session->PlayerArray)
	{
		const APFPlayerState* Other = Cast<APFPlayerState>(Entry);
		if (Other && Other != Player && Other->GetLobbySlot() >= 0) OccupiedSlots.Add(Other->GetLobbySlot());
	}
	int32 Slot = INDEX_NONE;
	for (int32 Index = 0; Index < Session->Capacity; ++Index)
		if (!OccupiedSlots.Contains(Index)) { Slot = Index; break; }
	if (Slot == INDEX_NONE || Session->Phase != EPFSessionPhase::Lobby || bClosingLobby)
	{
		if (GameSession) GameSession->KickPlayer(NewPlayer, NSLOCTEXT("PFLobby", "JoinClosed", "This session is full or has already started."));
		return;
	}
	Player->SetLobbySlot(Slot);
	Player->SetLobbySelection(CHARACTER_TWINBLAST, Session->Mode == EPFSessionMode::Versus);
	Player->SetLobbyReady(false);
	if (NewPlayer->IsLocalController()) Session->HostPlayer = Player;
	Session->NotifySessionChanged();
}

void APFLobbyGameMode::Logout(AController* Exiting)
{
	APFSessionGameState* Session = GetGameState<APFSessionGameState>();
	APFPlayerState* Player = Exiting ? Exiting->GetPlayerState<APFPlayerState>() : nullptr;
	const bool bHostLeaving = Session && Session->IsHost(Player);
	if (Player)
	{
		Player->SetLobbyReady(false);
		Player->SetLobbySelection(Player->GetCharacter(), false);
		Player->SetLobbySlot(INDEX_NONE);
	}
	Super::Logout(Exiting);
	if (!Session) return;
	if (bHostLeaving && !GetWorld()->IsInSeamlessTravel())
	{
		bClosingLobby = true;
		Session->HostPlayer = nullptr;
		for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
			if (APFLobbyPlayerController* Guest = Cast<APFLobbyPlayerController>(It->Get()); Guest && Guest != Exiting)
				Guest->Client_LeaveLobby();
	}
	Session->NotifySessionChanged();
}

void APFLobbyGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
}

// 조작 가능한 로비 참가자 조회
APFPlayerState* APFLobbyGameMode::GetLobbyPlayer(APFLobbyPlayerController* Player) const
{
	const APFSessionGameState* Session = GetGameState<APFSessionGameState>();
	APFPlayerState* State = Player ? Player->GetPlayerState<APFPlayerState>() : nullptr;
	return !bClosingLobby && Session && Session->bInitialized && Session->Phase == EPFSessionPhase::Lobby
		&& IsValid(Player) && Player->GetWorld() == GetWorld() && IsValid(State)
		&& State->GetLobbySlot() >= 0 && State->GetLobbySlot() < Session->Capacity
		&& Session->PlayerArray.Contains(State) ? State : nullptr;
}

// 캐릭터 선택, 스토리 선택 해제
void APFLobbyGameMode::SelectCharacter(APFLobbyPlayerController* Player, ECHARACTER Character)
{
	APFPlayerState* State = GetLobbyPlayer(Player);
	APFSessionGameState* Session = GetGameState<APFSessionGameState>();
	if (!State || (Character != CHARACTER_TWINBLAST && Character != CHARACTER_KWANG)) return;
	const bool bSelected = Session->Mode == EPFSessionMode::Versus || !State->HasLobbySelection() || State->GetCharacter() != Character;
	if (Session->Mode == EPFSessionMode::Versus && State->HasLobbySelection() && State->GetCharacter() == Character) return;
	State->SetLobbySelection(Character, bSelected);
	Session->NotifySessionChanged();
}

// 게스트 준비 상태 반영
void APFLobbyGameMode::SetReady(APFLobbyPlayerController* Player, bool bReady, int32 SettingsRevision)
{
	APFPlayerState* State = GetLobbyPlayer(Player);
	APFSessionGameState* Session = GetGameState<APFSessionGameState>();
	if (!State || Session->IsHost(State)
		|| (bReady && (!State->HasLobbySelection() || SettingsRevision != Session->SettingsRevision))) return;
	State->SetLobbyReady(bReady);
	Session->NotifySessionChanged();
}

// 맵 변경 후 게스트 준비 해제
void APFLobbyGameMode::ResetGuestReadiness()
{
	APFSessionGameState* Session = GetGameState<APFSessionGameState>();
	if (!Session) return;
	for (APlayerState* Entry : Session->PlayerArray)
		if (APFPlayerState* Player = Cast<APFPlayerState>(Entry); Player && !Session->IsHost(Player))
			Player->SetLobbyReady(false);
}

// 호스트의 대전 맵 선택 반영
void APFLobbyGameMode::SelectMap(APFLobbyPlayerController* Player, FName Map)
{
	APFPlayerState* State = GetLobbyPlayer(Player);
	APFSessionGameState* Session = GetGameState<APFSessionGameState>();
	if (!State || !Session->IsHost(State) || Session->Mode != EPFSessionMode::Versus || Session->SelectedMap == Map) return;
	bool bAllowed = Map == TEXT("Tutorial");
	for (int32 Index = 3; Index <= 8 && !bAllowed; ++Index)
		bAllowed = Map == FName(FString::Printf(TEXT("Map%d"), Index));
	if (!bAllowed) return;
	Session->SelectedMap = Map;
	++Session->SettingsRevision;
	ResetGuestReadiness();
	Session->NotifySessionChanged();
}

// 준비된 전원을 게임으로 이동
void APFLobbyGameMode::StartMatch(APFLobbyPlayerController* Player)
{
	APFPlayerState* State = GetLobbyPlayer(Player);
	APFSessionGameState* Session = GetGameState<APFSessionGameState>();
	UPFGameInstance* GI = GetGameInstance<UPFGameInstance>();
	if (!State || !Session->IsHost(State) || !Session->CanStart() || !GI) return;
	Session->Phase = EPFSessionPhase::Starting;
	Session->NotifySessionChanged();
	if (!GI->BeginLobbyMatch(Session->Mode, Session->SelectedMap))
	{
		Session->Phase = EPFSessionPhase::Lobby;
		Session->NotifySessionChanged();
	}
}

// 준비 전 참가자 퇴장 처리
void APFLobbyGameMode::LeaveLobby(APFLobbyPlayerController* Player)
{
	APFPlayerState* State = GetLobbyPlayer(Player);
	APFSessionGameState* Session = GetGameState<APFSessionGameState>();
	if (!State || !Session->CanLeave(State)) return;
	if (Session->IsHost(State))
	{
		bClosingLobby = true;
		for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
			if (APFLobbyPlayerController* Guest = Cast<APFLobbyPlayerController>(It->Get()); Guest && Guest != Player)
				Guest->Client_LeaveLobby();
	}
	State->SetLobbySelection(State->GetCharacter(), false);
	State->SetLobbySlot(INDEX_NONE);
	Session->NotifySessionChanged();
	Player->Client_LeaveLobby();
}

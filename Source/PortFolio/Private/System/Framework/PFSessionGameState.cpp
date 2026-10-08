#include "System/Framework/PFSessionGameState.h"

#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "System/Framework/PFPlayerState.h"

// 현재 월드의 세션 상태 조회
APFSessionGameState* APFSessionGameState::Get(const UWorld* World)
{
	return World ? World->GetGameState<APFSessionGameState>() : nullptr;
}

// 스토리 플레이 여부 확인
bool APFSessionGameState::IsStory(const UWorld* World)
{
	const APFSessionGameState* State = Get(World);
	return State && State->bInitialized && State->Phase == EPFSessionPhase::Playing && State->Mode == EPFSessionMode::Story;
}

// 대전 플레이 여부 확인
bool APFSessionGameState::IsVersus(const UWorld* World)
{
	const APFSessionGameState* State = Get(World);
	return State && State->bInitialized && State->Phase == EPFSessionPhase::Playing && State->Mode == EPFSessionMode::Versus;
}

// 튜토리얼 플레이 여부 확인
bool APFSessionGameState::IsTraining(const UWorld* World)
{
	const APFSessionGameState* State = Get(World);
	return State && State->bInitialized && State->Phase == EPFSessionPhase::Playing && State->Mode == EPFSessionMode::Training;
}

// 모드별 최대 인원 조회
int32 APFSessionGameState::GetMaxPlayers(EPFSessionMode SessionMode)
{
	return SessionMode == EPFSessionMode::Training ? 1 : 4;
}

// 세션 호스트 확인
bool APFSessionGameState::IsHost(const APlayerState* Player) const
{
	return Player && Player == HostPlayer;
}

// 로비 시작 가능 여부 확인
bool APFSessionGameState::CanStart() const
{
	return Phase == EPFSessionPhase::Lobby && IsPartyReady();
}

// 인원, 캐릭터 구성, 준비 완료 확인
bool APFSessionGameState::IsPartyReady() const
{
	if (!bInitialized || !IsValid(HostPlayer)
		|| Capacity < 1 || Capacity > GetMaxPlayers(Mode)) return false;
	int32 Players = 0;
	int32 TwinBlasts = 0;
	int32 Kwangs = 0;
	bool bHasHost = false;
	TSet<int32> OccupiedSlots;
	for (const APlayerState* Entry : PlayerArray)
	{
		const APFPlayerState* Player = Cast<APFPlayerState>(Entry);
		if (!IsValid(Player) || Player->GetLobbySlot() == INDEX_NONE) continue;
		const int32 Slot = Player->GetLobbySlot();
		if (Slot < 0 || Slot >= Capacity || OccupiedSlots.Contains(Slot) || !Player->HasLobbySelection()) return false;
		OccupiedSlots.Add(Slot);
		++Players;
		bHasHost |= IsHost(Player);
		if (!IsHost(Player) && !Player->IsLobbyReady()) return false;
		if (Player->GetCharacter() == CHARACTER_TWINBLAST) ++TwinBlasts;
		else if (Player->GetCharacter() == CHARACTER_KWANG) ++Kwangs;
		else return false;
	}
	return bHasHost && Players == Capacity
		&& (Mode != EPFSessionMode::Story || Capacity == 1 || (TwinBlasts > 0 && Kwangs > 0));
}

// 준비 전 로비 퇴장 허용
bool APFSessionGameState::CanLeave(const APFPlayerState* Player) const
{
	return bInitialized && Phase == EPFSessionPhase::Lobby && IsValid(Player)
		&& Player->GetLobbySlot() >= 0 && Player->GetLobbySlot() < Capacity
		&& PlayerArray.Contains(Player) && !Player->IsLobbyReady();
}

// 서버 상태 변경 전파
void APFSessionGameState::NotifySessionChanged()
{
	if (!HasAuthority()) return;
	OnSessionStateChanged.Broadcast();
	ForceNetUpdate();
}

// 복제된 세션 상태 알림
void APFSessionGameState::OnRep_SessionState()
{
	OnSessionStateChanged.Broadcast();
}

void APFSessionGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APFSessionGameState, Mode);
	DOREPLIFETIME(APFSessionGameState, Phase);
	DOREPLIFETIME(APFSessionGameState, Capacity);
	DOREPLIFETIME(APFSessionGameState, SettingsRevision);
	DOREPLIFETIME(APFSessionGameState, SelectedMap);
	DOREPLIFETIME(APFSessionGameState, RoomName);
	DOREPLIFETIME(APFSessionGameState, HostPlayer);
	DOREPLIFETIME(APFSessionGameState, bInitialized);
}

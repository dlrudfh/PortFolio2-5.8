#include "Campaign/PFCampaignSubsystem.h"
#include "Character/PFCharacter.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GAS/Attributes/PFAttributeSet.h"
#include "GAS/Effects/PFGE_StatGameplayEffects.h"
#include "Kismet/GameplayStatics.h"
#include "System/Framework/PFSessionGameState.h"

// 게임 인스턴스의 캠페인 상태 조회
UPFCampaignSubsystem* UPFCampaignSubsystem::Get(const UObject* Context)
{
	const UWorld* World = Context ? Context->GetWorld() : nullptr;
	return World && World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UPFCampaignSubsystem>() : nullptr;
}

// 서버에서 확정한 캠페인 모드 확인
bool UPFCampaignSubsystem::IsCampaign(const UWorld* World)
{
	return APFSessionGameState::IsStory(World);
}

// 맵 이동과 재접속에 유지되는 플레이어 키
FString UPFCampaignSubsystem::PlayerKey(const APFPlayerState* Player)
{
	if (!Player) return FString();
	return Player->GetUniqueId().IsV1() && Player->GetUniqueId().IsValid() ? Player->GetUniqueId().GetV1()->ToString()
		: FString::Printf(TEXT("Local_%d"), Player->GetPlayerId());
}

// 새 캠페인 상태 초기화
void UPFCampaignSubsystem::ResetRun()
{
	Players.Reset();
	Records.Reset();
	CheckpointRecords.Reset();
	Deaths = 0;
	Elapsed = 0.;
	Chapter = INDEX_NONE;
	EnterChapter(3);
}

// 새 챕터의 복구 기준 준비
void UPFCampaignSubsystem::EnterChapter(int32 InChapter)
{
	if (Chapter == InChapter) return;
	Chapter = InChapter;
	CheckpointStep = 0;
	Checkpoint = TEXT("Start");
	CompletedEncounters.Reset();
	SupplyClaims.Reset();
	CheckpointRecords = Records;
}

// 플레이어 성장, 소지품, 시점 보관
void UPFCampaignSubsystem::CapturePlayer(APFPlayerState* Player)
{
	if (!Player || !Player->HasAuthority()) return;
	FPFCampaignPlayerSnapshot& Snapshot = Players.FindOrAdd(PlayerKey(Player));
	Snapshot.Character = Player->GetCharacter();
	Snapshot.Inventory = Player->GetInventorySlots();
	Snapshot.QuickSlots = Player->GetQuickSlotItemIDs();
	if (const APFCharacter* Character = Cast<APFCharacter>(Player->GetPawn())) Snapshot.View = Character->CaptureViewState();
	Snapshot.Stats = FPFStatValues(*Player->GetAttributeSet());
}

// 체크포인트의 플레이어 데이터 복원
bool UPFCampaignSubsystem::RestorePlayer(APFPlayerState* Player)
{
	const FPFCampaignPlayerSnapshot* Snapshot = Player ? Players.Find(PlayerKey(Player)) : nullptr;
	if (!Snapshot || !Player->HasAuthority()) return false;
	Player->SetCharacter(Snapshot->Character);
	Player->RestoreCampaignInventory(Snapshot->Inventory, Snapshot->QuickSlots);
	FPFStatValues RestoredStats = Snapshot->Stats;
	RestoredStats.Health = RestoredStats.MaxHealth;
	RestoredStats.Mana = RestoredStats.MaxMana;
	return FPFGE_StatGameplayEffects::InitializeStats(Player->GetAbilitySystemComponent(), RestoredStats);
}

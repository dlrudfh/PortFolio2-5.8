#include "System/Framework/PFTutorialManager.h"

#include "Character/Kwang/PFKwang.h"
#include "Character/PFCharacter.h"
#include "Character/TwinBlast/PFTwinBlast.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/PFGameplayTags.h"
#include "Props/PFChest.h"
#include "System/Framework/PFGameInstance.h"
#include "System/Framework/PFGameMode.h"
#include "System/Framework/PFPlayerController.h"
#include "System/Framework/PFPlayerState.h"
#include "System/Framework/PFSessionGameState.h"
#include "TimerManager.h"
#include "UI/HUD/PFStatWidget.h"
#include "UI/HUD/PFTutorialWidget.h"
#include "UI/Inventory/PFInventoryWidget.h"

APFTutorialManager::APFTutorialManager()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);
}

// 현재 월드의 튜토리얼 관리자 조회
APFTutorialManager* APFTutorialManager::Find(UWorld* World)
{
	if (!World) return nullptr;
	for (TActorIterator<APFTutorialManager> It(World); It; ++It)
	{
		if (!It->IsActorBeingDestroyed()) return *It;
	}
	return nullptr;
}

void APFTutorialManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!APFSessionGameState::IsTraining(GetWorld())) return;
	for (auto It = LocalProgress.CreateIterator(); It; ++It)
	{
		APFPlayerController* Controller = It.Key().Get();
		if (!Controller || Controller->GetWorld() != GetWorld())
		{
			if (Controller) Controller->DetachTutorial(this);
			It.RemoveCurrent();
		}
	}
	for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APFPlayerController* Controller = Cast<APFPlayerController>(It->Get());
		if (Controller && Controller->IsLocalController())
		{
			FLocalProgress& State = LocalProgress.FindOrAdd(Controller);
			UpdatePlayer(Controller, State, DeltaTime);
		}
	}
	if (HasAuthority())
	{
		for (auto It = CombatStates.CreateIterator(); It; ++It)
		{
			if (!It.Key().IsValid() || It.Key()->GetWorld() != GetWorld())
			{
				ReleaseCombat(It.Value());
				It.RemoveCurrent();
			}
		}
	}
}

void APFTutorialManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(TitleReturnTimer);
	for (const auto& Entry : LocalProgress)
	{
		if (APFPlayerController* Controller = Entry.Key.Get()) Controller->DetachTutorial(this);
	}
	for (auto& Entry : CombatStates) ReleaseCombat(Entry.Value);
	LocalProgress.Reset();
	CombatStates.Reset();
	Super::EndPlay(EndPlayReason);
}

// 로컬 플레이어의 안내 표시, 행동 관찰
void APFTutorialManager::UpdatePlayer(APFPlayerController* Controller, FLocalProgress& State, float DeltaTime)
{
	State.Controller = Controller;
	State.Guide = Controller->AttachTutorial(this);
	if (!State.Guide.IsValid()) return;
	if (State.Step == EStep::Finished)
	{
		State.Guide->SetVisibility(ESlateVisibility::HitTestInvisible);
		UpdateGuideText(State, nullptr);
		State.Guide->SetWorldMarker(nullptr, FVector::ZeroVector, false, FString());
		State.Guide->SetHighlight(nullptr, FString());
		return;
	}
	UPFInventoryWidget* Inventory = nullptr;
	UPFStatWidget* Stats = nullptr;
	bool bMenuOpen = false;
	Controller->GetTutorialWindows(Inventory, Stats, bMenuOpen);
	State.InventoryWindow = Inventory;
	State.StatWindow = Stats;
	APFCharacter* Character = Cast<APFCharacter>(Controller->GetPawn());
	APFPlayerState* PlayerState = Controller->GetPlayerState<APFPlayerState>();
	const bool bReady = Character && PlayerState && Character->IsPlayerCharacter() && !Character->IsDeadCharacter();
	if (bReady && !Character->IsLevelStartActive()) State.bIntroFinished = true;
	const bool bVisible = bReady && State.bIntroFinished && !bMenuOpen;
	State.Guide->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	if (!bVisible) return;

	if (State.ObservedCharacter.Get() != Character)
	{
		State.ObservedCharacter = Character;
		State.PreviousPosition = Character->GetActorLocation();
		State.InitialViewMode = static_cast<int32>(Character->GetCurrentControlMode());
		State.bPreviousViewFixed = Character->IsViewpointFixed();
		State.bWasAttacking = Character->HasStateTag(PFGameplayTags::Character_State_Attacking);
		if (State.Step == EStep::Move || State.Step == EStep::Jump) State.bInitialized = false;
	}
	if (!State.bInitialized) BeginStep(State, Character);
	if (!Character->IsLevelStartActive() && State.Step != EStep::Finished)
	{
		UpdateStep(State, Character, PlayerState, DeltaTime);
	}
	State.PreviousPosition = Character->GetActorLocation();
	State.bWasAttacking = Character->HasStateTag(PFGameplayTags::Character_State_Attacking);
	State.bPreviousViewFixed = Character->IsViewpointFixed();
	UpdateGuideText(State, Character);

	AActor* Marker = State.MarkerActor.Get();
	if (const APFCharacter* Target = Cast<APFCharacter>(Marker); Target && (Target->IsDeadCharacter() || Target->IsShrubConcealed())) Marker = nullptr;
	const bool bWindowOpen = (Inventory && Inventory->IsInventoryWindowVisible()) || (Stats && Stats->IsStatWindowVisible());
	State.Guide->SetWorldMarker(bWindowOpen ? nullptr : Marker, State.MarkerLocation,
		!bWindowOpen && State.bHasMovementTarget, State.MarkerCaption);
	State.Guide->SetHighlight(State.HighlightWidget.Get(), State.HighlightCaption);
}

// 직접 연 상자의 보상 생성, 획득 확인
void APFTutorialManager::NotifyChestState(APFPlayerController* Controller, FName ItemName, const FVector& ItemLocation, bool bCollected)
{
	FLocalProgress* State = LocalProgress.Find(Controller);
	if (!State || State->Step != EStep::Chest || ItemName.IsNone()) return;
	if (bCollected)
	{
		if (State->ChestRewards.Contains(ItemName)) CompleteStep(*State);
		return;
	}
	State->ChestRewards.Add(ItemName);
	State->StepFlags |= 1;
	State->MarkerActor.Reset();
	State->MarkerLocation = ItemLocation;
	State->bHasMovementTarget = true;
	State->MarkerCaption = TEXT("상자 아이템");
}

// 퀵슬롯 사용 전 수량 기록
void APFTutorialManager::NotifyQuickSlotUse(APFPlayerController* Controller, int32 ItemID, int32 CountBeforeUse)
{
	FLocalProgress* State = LocalProgress.Find(Controller);
	if (State && State->Step == EStep::Inventory && (State->StepFlags & 3) == 3 && CountBeforeUse > 0
		&& (ItemID == etoi(APFItem::ITEM_SHIELD) || ItemID == etoi(APFItem::ITEM_COIN)))
	{
		int32& Count = State->RequestedItemCounts.FindOrAdd(ItemID);
		Count = FMath::Max(Count, CountBeforeUse);
	}
}

// 서버에서 준비한 전투 대상, 처치 결과 반영
void APFTutorialManager::NotifyBotState(APFPlayerController* Controller, APFCharacter* Bot, bool bDefeated)
{
	FLocalProgress* State = LocalProgress.Find(Controller);
	if (!State || State->Step != EStep::KwangBot) return;
	if (bDefeated)
	{
		State->bTutorialBotDefeated = true;
		return;
	}
	if (IsValid(Bot) && Cast<APFKwang>(Bot) && !State->bTutorialBotDefeated)
	{
		State->TutorialBot = Bot;
		State->bBotSpawnFailed = false;
		State->TargetRefreshRemaining = 0.f;
	}
	else if (!State->TutorialBot.IsValid()) State->bBotSpawnFailed = true;
}

// 서버에서 플레이어별 광 봇 한 마리 준비
void APFTutorialManager::PrepareCombat(APFPlayerController* Controller)
{
	if (!HasAuthority() || !APFSessionGameState::IsTraining(GetWorld())
		|| !IsValid(Controller) || Controller->GetWorld() != GetWorld()) return;
	APFCharacter* Character = Cast<APFCharacter>(Controller->GetPawn());
	if (!Character || !Character->IsPlayerCharacter() || Character->IsDeadCharacter()
		|| Character->IsLevelStartActive() || Character->IsJumpPadFlightActive()) return;
	FCombatState& Combat = CombatStates.FindOrAdd(Controller);
	if (!Combat.bDefeated && !Combat.Bot.IsValid())
	{
		if (APFGameMode* GameMode = GetWorld()->GetAuthGameMode<APFGameMode>())
		{
			Combat.Bot = GameMode->SpawnEnemy(Character, APFKwang::StaticClass());
			if (Combat.Bot.IsValid()) Combat.Bot->OnCharacterDied.AddUObject(this, &APFTutorialManager::HandleBotDied);
		}
	}
	Controller->Client_NotifyTutorialBot(this, Combat.Bot.Get(), Combat.bDefeated);
}

// 서버의 전투 대상 사망 전달
void APFTutorialManager::HandleBotDied(APFCharacter* Bot)
{
	if (!HasAuthority()) return;
	for (auto& Entry : CombatStates)
	{
		if (Entry.Value.Bot.Get() == Bot)
		{
			Entry.Value.bDefeated = true;
			if (APFPlayerController* Controller = Entry.Key.Get()) Controller->Client_NotifyTutorialBot(this, Bot, true);
			return;
		}
	}
}

// 훈련 봇 구독, 액터 정리
void APFTutorialManager::ReleaseCombat(FCombatState& Combat)
{
	if (APFCharacter* Bot = Combat.Bot.Get())
	{
		Bot->OnCharacterDied.RemoveAll(this);
		if (HasAuthority()) Bot->Destroy();
	}
	Combat.Bot.Reset();
}

// 현재 단계의 관찰 기준 설정
void APFTutorialManager::BeginStep(FLocalProgress& State, APFCharacter* Character)
{
	State.bInitialized = true;
	State.StepFlags = 0;
	State.ActionTime = 0.f;
	State.TravelDistance = 0.f;
	State.TargetRefreshRemaining = 0.f;
	State.BotRequestRemaining = 0.f;
	State.RequestedItemCounts.Reset();
	State.ChestRewards.Reset();
	State.TutorialBot.Reset();
	State.bTutorialBotDefeated = false;
	State.bBotSpawnFailed = false;
	State.MarkerActor.Reset();
	State.HighlightWidget.Reset();
	State.bHasMovementTarget = false;
	State.PreviousPosition = Character->GetActorLocation();
	State.InitialViewMode = static_cast<int32>(Character->GetCurrentControlMode());
	State.bPreviousViewFixed = Character->IsViewpointFixed();
	State.bWasAttacking = Character->HasStateTag(PFGameplayTags::Character_State_Attacking);
	if (State.Step == EStep::Move)
	{
		State.bHasMovementTarget = FindMovementTarget(State, Character);
		State.MarkerCaption = TEXT("이동 목표");
	}
	else if (State.Step == EStep::Finished && !GetWorldTimerManager().IsTimerActive(TitleReturnTimer))
	{
		GetWorldTimerManager().SetTimer(TitleReturnTimer, this, &APFTutorialManager::ReturnToTitle, 5.f, false);
	}
}

// 실제 행동, 복제된 수치로 단계 진행
void APFTutorialManager::UpdateStep(FLocalProgress& State, APFCharacter* Character, APFPlayerState* PlayerState, float DeltaTime)
{
	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	const bool bAttacking = Character->HasStateTag(PFGameplayTags::Character_State_Attacking);
	const bool bUltimate = Character->HasStateTag(PFGameplayTags::Character_State_Ultimate);
	const bool bInventoryOpen = State.InventoryWindow.IsValid() && State.InventoryWindow->IsInventoryWindowVisible();
	const bool bStatsOpen = State.StatWindow.IsValid() && State.StatWindow->IsStatWindowVisible();
	const bool bGameplayOpen = !bInventoryOpen && !bStatsOpen;
	const float Moved = FVector::Dist2D(State.PreviousPosition, Character->GetActorLocation());
	switch (State.Step)
	{
	case EStep::Move:
		if (bGameplayOpen && Movement->IsMovingOnGround() && Moved < 150.f) State.TravelDistance += Moved;
		if (State.bHasMovementTarget
			? FVector::Dist2D(Character->GetActorLocation(), State.MarkerLocation) < 90.f
				&& FMath::Abs(Movement->GetActorFeetLocation().Z - State.MarkerLocation.Z) < 70.f
			: State.TravelDistance >= 500.f) CompleteStep(State);
		break;
	case EStep::Jump:
		if (bGameplayOpen && !Character->IsJumpPadFlightActive() && Movement->IsFalling() && Character->GetVelocity().Z > 100.f) State.StepFlags |= 1;
		if ((State.StepFlags & 1) && Movement->IsMovingOnGround()) CompleteStep(State);
		break;
	case EStep::Sprint:
		if (bGameplayOpen && Character->IsSprinting() && Movement->IsMovingOnGround() && Character->GetVelocity().Size2D() > 100.f) State.ActionTime += DeltaTime;
		if (State.ActionTime >= 1.f) CompleteStep(State);
		break;
	case EStep::View:
		if (static_cast<int32>(Character->GetCurrentControlMode()) != State.InitialViewMode) State.StepFlags |= 1;
		if (Character->GetCurrentControlMode() == TOPVIEW && Character->IsViewpointFixed() != State.bPreviousViewFixed) State.StepFlags |= 2;
		if ((State.StepFlags & 3) == 3) CompleteStep(State);
		break;
	case EStep::Attack:
		if (bGameplayOpen && bAttacking && !State.bWasAttacking && !bUltimate)
		{
			if (Cast<APFTwinBlast>(Character)) State.StepFlags |= 1;
			else if (Cast<APFKwang>(Character)) State.StepFlags |= 2;
		}
		if ((State.StepFlags & 3) == 3) CompleteStep(State);
		break;
	case EStep::Ultimate:
		if (bUltimate) State.StepFlags |= 1;
		if (bUltimate && bAttacking && !State.bWasAttacking) State.StepFlags |= 2;
		if ((State.StepFlags & 3) == 3 && !bUltimate && Cast<APFTwinBlast>(Character)) CompleteStep(State);
		break;
	case EStep::Chest:
		break;
	case EStep::Inventory:
	{
		if (bInventoryOpen) State.StepFlags |= 1;
		if (State.StepFlags & 1)
		{
			for (int32 ItemID : PlayerState->GetQuickSlotItemIDs())
			{
				if ((ItemID == etoi(APFItem::ITEM_SHIELD) || ItemID == etoi(APFItem::ITEM_COIN))
					&& PlayerState->GetInventoryItemCount(ItemID) > 0) State.StepFlags |= 2;
			}
		}
		bool bItemUsed = false;
		for (const TPair<int32, int32>& Request : State.RequestedItemCounts)
		{
			if (PlayerState->GetInventoryItemCount(Request.Key) < Request.Value && PlayerState->GetItemCooldownRemaining(Request.Key) > 0.f)
			{
				bItemUsed = true;
				break;
			}
		}
		if (bItemUsed) CompleteStep(State);
		break;
	}
	case EStep::KwangBot:
		if (State.bTutorialBotDefeated || (State.TutorialBot.IsValid() && State.TutorialBot->IsDeadCharacter())) CompleteStep(State);
		else if (!State.TutorialBot.IsValid() && bGameplayOpen)
		{
			State.BotRequestRemaining -= DeltaTime;
			if (State.BotRequestRemaining <= 0.f)
			{
				State.BotRequestRemaining = 3.f;
				if (State.Controller.IsValid()) State.Controller->RequestTutorialBot();
			}
		}
		break;
	case EStep::Stats:
		if (bStatsOpen)
		{
			State.ActionTime += DeltaTime;
			if (State.ActionTime >= .75f) State.StepFlags |= 1;
		}
		if ((State.StepFlags & 1) && !bStatsOpen) CompleteStep(State);
		break;
	default:
		break;
	}
	State.TargetRefreshRemaining -= DeltaTime;
	if (State.TargetRefreshRemaining <= 0.f)
	{
		State.TargetRefreshRemaining = 1.f;
		FindNearbyTarget(State, Character);
	}
}

// 완료 즉시 다음 단계 시작
void APFTutorialManager::CompleteStep(FLocalProgress& State)
{
	if (State.Step == EStep::Finished) return;
	APFCharacter* Character = State.Controller.IsValid() ? Cast<APFCharacter>(State.Controller->GetPawn()) : nullptr;
	if (!Character) return;
	State.Step = static_cast<EStep>(static_cast<uint8>(State.Step) + 1);
	BeginStep(State, Character);
}

// 현재 행동에 필요한 안내
void APFTutorialManager::UpdateGuideText(FLocalProgress& State, APFCharacter* Character)
{
	FString Title, Objective;
	State.HighlightWidget.Reset();
	const bool bInventoryOpen = State.InventoryWindow.IsValid() && State.InventoryWindow->IsInventoryWindowVisible();
	const bool bStatsOpen = State.StatWindow.IsValid() && State.StatWindow->IsStatWindowVisible();
	switch (State.Step)
	{
	case EStep::Move:
		Title = TEXT("기본 이동");
		Objective = State.bHasMovementTarget ? TEXT("W, A, S, D로 표시된 지점까지 이동하세요.") : TEXT("W, A, S, D로 5m 이동하세요.");
		break;
	case EStep::Jump:
		Title = TEXT("점프"); Objective = TEXT("Space를 눌러 점프하세요.");
		break;
	case EStep::Sprint:
		Title = TEXT("질주");
		if (Character->GetCurrentControlMode() == FPS) Objective = TEXT("V를 눌러 시점을 변경하세요.");
		else Objective = TEXT("마우스 우클릭을 눌러 달리기 모드를 활성화하고 1초 이상 달려보세요.");
		break;
	case EStep::View:
		Title = TEXT("카메라 조작");
		if (!(State.StepFlags & 1)) Objective = TEXT("V를 눌러 시점을 변경하세요.");
		else if (Character->GetCurrentControlMode() != TOPVIEW) Objective = TEXT("V를 눌러 탑뷰로 전환하세요.");
		else Objective = TEXT("왼쪽 Shift를 눌러 시점 고정 상태를 전환하세요.");
		break;
	case EStep::Attack:
		Title = TEXT("공격과 캐릭터 교체");
		if (State.StepFlags == 0) Objective = TEXT("마우스 좌클릭으로 일반공격을 해보세요.");
		else if (State.StepFlags & 1) Objective = Cast<APFKwang>(Character)
			? TEXT("마우스 좌클릭으로 일반공격을 해보세요.") : TEXT("C를 눌러 Kwang으로 교체하세요.");
		else Objective = Cast<APFTwinBlast>(Character)
			? TEXT("마우스 좌클릭으로 일반공격을 해보세요.") : TEXT("C를 눌러 Twinblast로 교체하세요.");
		if (Character->HasStateTag(PFGameplayTags::Character_State_Ultimate)
			&& (State.StepFlags == 0 || ((State.StepFlags & 2) && Cast<APFTwinBlast>(Character))))
		{
			Objective = TEXT("R을 눌러 궁극기를 해제하세요.");
		}
		break;
	case EStep::Ultimate:
		Title = TEXT("Twinblast 궁극기");
		if (!Cast<APFTwinBlast>(Character)) Objective = TEXT("C를 눌러 Twinblast로 교체하세요.");
		else if (!(State.StepFlags & 1)) Objective = TEXT("R을 눌러 궁극기를 활성화하세요.");
		else if (!(State.StepFlags & 2)) Objective = TEXT("마우스 좌클릭으로 궁극기를 발사하세요.");
		else Objective = TEXT("R을 눌러 궁극기를 해제하세요.");
		break;
	case EStep::Chest:
		Title = TEXT("상자와 아이템 획득");
		Objective = State.StepFlags & 1
			? TEXT("아이템을 획득해 보세요.")
			: TEXT("G를 눌러 상자를 오픈하세요.");
		break;
	case EStep::Inventory:
		Title = TEXT("인벤토리와 퀵슬롯");
		if (!(State.StepFlags & 1)) Objective = TEXT("I를 눌러 인벤토리를 여세요.");
		else if (!(State.StepFlags & 2)) Objective = TEXT("실드나 코인을 아래 퀵슬롯으로 드래그하세요.");
		else Objective = bInventoryOpen ? TEXT("I를 눌러 인벤토리를 닫으세요.") : TEXT("단축키를 눌러 아이템을 사용하세요.");
		if (State.InventoryWindow.IsValid() && (State.StepFlags & 1))
		{
			State.HighlightWidget = State.InventoryWindow->GetWidgetFromName(bInventoryOpen && !(State.StepFlags & 2) ? TEXT("InventoryWindow") : TEXT("QuickSlotWindow"));
			State.HighlightCaption.Reset();
		}
		break;
	case EStep::KwangBot:
		Title = TEXT("전투");
		if (!State.TutorialBot.IsValid() || State.TutorialBot->IsLevelStartActive()) Objective = TEXT("적과의 전투를 준비하세요.");
		else Objective = TEXT("적과의 전투에서 승리하세요.");
		break;
	case EStep::Stats:
		Title = TEXT("능력치 확인");
		Objective = bStatsOpen ? TEXT("능력치를 자유롭게 배분한 뒤, 다시 J를 눌러 창을 닫으세요.") : TEXT("J를 눌러 능력치 창을 여세요.");
		if (bStatsOpen)
		{
			State.HighlightWidget = State.StatWindow->GetWidgetFromName(TEXT("StatWindow"));
			State.HighlightCaption.Reset();
		}
		break;
	case EStep::Finished:
		State.Guide->SetGuide(FString::Printf(TEXT("튜토리얼 완료  %d / %d"), StepCount, StepCount),
			FString::Printf(TEXT("모든 튜토리얼을 완료했습니다. %d초 뒤 타이틀로 돌아갑니다."),
				FMath::Max(0, FMath::CeilToInt(GetWorldTimerManager().GetTimerRemaining(TitleReturnTimer)))), 1.f, true);
		return;
	}
	if ((State.Step == EStep::Jump || State.Step == EStep::Sprint) && Character->HasStateTag(PFGameplayTags::Character_State_Ultimate))
	{
		Objective = TEXT("R을 눌러 궁극기를 해제하세요.");
	}
	if (State.Step != EStep::Inventory && State.Step != EStep::Stats && (bInventoryOpen || bStatsOpen))
	{
		Objective = bInventoryOpen ? TEXT("I로 인벤토리를 닫고 안내를 계속하세요.") : TEXT("J로 능력치 창을 닫고 안내를 계속하세요.");
	}
	State.Guide->SetGuide(FString::Printf(TEXT("%d / %d  %s"), static_cast<int32>(State.Step) + 1, StepCount, *Title),
		Objective, static_cast<float>(static_cast<int32>(State.Step)) / StepCount, false);
}

// 완료 안내 후 타이틀 복귀
void APFTutorialManager::ReturnToTitle()
{
	if (UPFGameInstance* GameInstance = GetWorld()->GetGameInstance<UPFGameInstance>())
	{
		GameInstance->ReturnToMainMenu();
	}
}

// 가까운 평지에서 충돌 없는 이동 목표 탐색
bool APFTutorialManager::FindMovementTarget(FLocalProgress& State, APFCharacter* Character)
{
	UWorld* World = GetWorld();
	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const FVector Feet = Character->GetCharacterMovement()->GetActorFeetLocation();
	FCollisionQueryParams Query(SCENE_QUERY_STAT(TutorialMoveTarget), false, Character);
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius() * .9f,
		Capsule->GetScaledCapsuleHalfHeight() - 4.f);
	for (int32 DirectionIndex = 0; DirectionIndex < 8; ++DirectionIndex)
	{
		const FVector Direction = FRotator(0.f, Character->GetActorRotation().Yaw + DirectionIndex * 45.f, 0.f).Vector();
		FVector LastCenter = Character->GetActorLocation();
		bool bClear = true;
		for (int32 Segment = 1; Segment <= 4; ++Segment)
		{
			const FVector Point = Feet + Direction * (Segment * 100.f);
			FHitResult Ground, Obstacle;
			if (!World->LineTraceSingleByChannel(Ground, Point + FVector(0.f, 0.f, 70.f), Point - FVector(0.f, 0.f, 100.f), ECC_Visibility, Query)
				|| Ground.ImpactNormal.Z < .8f || FMath::Abs(Ground.ImpactPoint.Z - Feet.Z) > 35.f)
			{
				bClear = false;
				break;
			}
			const FVector Center = Ground.ImpactPoint + FVector(0.f, 0.f, Capsule->GetScaledCapsuleHalfHeight() + 2.f);
			if (World->SweepSingleByChannel(Obstacle, LastCenter, Center, FQuat::Identity, ECC_Pawn, Shape, Query))
			{
				bClear = false;
				break;
			}
			LastCenter = Center;
			State.MarkerLocation = Ground.ImpactPoint;
		}
		if (bClear) return true;
	}
	return false;
}

// 획득 가능한 상자 안내 표시
void APFTutorialManager::FindNearbyTarget(FLocalProgress& State, APFCharacter* Character)
{
	State.MarkerActor.Reset();
	if (State.Step != EStep::Chest || !State.ChestRewards.IsEmpty()) return;
	float NearestDistance = TNumericLimits<float>::Max();
	const APFPlayerState* PlayerState = Character->GetPlayerState<APFPlayerState>();
	for (TActorIterator<APFChest> It(GetWorld()); It; ++It)
	{
		if (It->CurrentState != etoi(APFChest::CHEST_CLOSED)) continue;
		if (!APFItem::GetDefinition(It->Reward) || (PlayerState
			&& PlayerState->GetInventoryItemCount(It->Reward) >= APFPlayerState::MaxInventoryStackCount)) continue;
		const float Distance = FVector::DistSquared(Character->GetActorLocation(), It->GetActorLocation());
		if (Distance < NearestDistance) { NearestDistance = Distance; State.MarkerActor = *It; }
	}
	State.MarkerCaption = TEXT("보급 상자");
}

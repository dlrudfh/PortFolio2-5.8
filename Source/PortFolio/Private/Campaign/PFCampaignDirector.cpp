#include "Campaign/PFCampaignDirector.h"
#include "Campaign/PFCampaignAnchor.h"
#include "Campaign/PFCampaignSubsystem.h"
#include "Campaign/PFCampaignEnemyController.h"
#include "Campaign/PFGA_CampaignShockwave.h"
#include "Character/Kwang/PFKwang.h"
#include "Character/TwinBlast/PFTwinBlast.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Materials/MaterialInterface.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GAS/Attributes/PFAttributeSet.h"
#include "GAS/Effects/PFGE_StatGameplayEffects.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Net/UnrealNetwork.h"
#include "Props/PFItem.h"
#include "Projectile/Projectile.h"
#include "System/Framework/PFGameInstance.h"
#include "System/Framework/PFGameMode.h"
#include "System/Framework/PFPlayerController.h"
#include "System/Subsystems/PFWorldSubsystem.h"

APFCampaignDirector::APFCampaignDirector()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
	SetNetUpdateFrequency(10.f);
}

// 현재 월드의 캠페인 관리자 조회
APFCampaignDirector* APFCampaignDirector::Find(const UWorld* World)
{
	UPFWorldSubsystem* Subsystem = World ? World->GetSubsystem<UPFWorldSubsystem>() : nullptr;
	return Subsystem ? Subsystem->GetCampaignDirector() : nullptr;
}

// 캠페인 진행 상태의 입력 제한
bool APFCampaignDirector::BlocksInput(const AController* Controller)
{
	const APFPlayerController* Player = Cast<APFPlayerController>(Controller);
	const APFCampaignDirector* Director = Controller ? Find(Controller->GetWorld()) : nullptr;
	return Director && (Director->Progress.Phase != EPFCampaignPhase::Playing || (Player && Player->IsCampaignWaiting()));
}

// 협동 플레이어의 아군 판정
bool APFCampaignDirector::AreFriendly(const APFCharacter* Source, const APFCharacter* Target)
{
	return IsValid(Source) && IsValid(Target) && Source->IsPlayerCharacter() && Target->IsPlayerCharacter() && Find(Source->GetWorld());
}

void APFCampaignDirector::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (UPFWorldSubsystem* Subsystem = GetWorld()->GetSubsystem<UPFWorldSubsystem>())
		Subsystem->RegisterCampaignDirector(this);
}

void APFCampaignDirector::BeginPlay()
{
	Super::BeginPlay();
	for (TActorIterator<APFCampaignAnchor> It(GetWorld()); It; ++It)
	{
		if (Anchors.Contains(It->Id) || It->Id.IsNone())
		{
			if (HasAuthority()) Fail(FString::Printf(TEXT("중복되거나 빈 배치 ID: %s"), *It->GetName()));
			continue;
		}
		Anchors.Add(It->Id, *It);
	}
	for (TActorIterator<APFCampaignShot> It(GetWorld()); It; ++It)
	{
		if (Shots.Contains(It->Id) || It->Id.IsNone())
		{
			if (HasAuthority()) Fail(TEXT("카메라 ID 중복 또는 누락"));
			continue;
		}
		Shots.Add(It->Id, *It);
	}
	if (!HasAuthority()) return;
	Run = UPFCampaignSubsystem::Get(this);
	const FString Map = UGameplayStatics::GetCurrentLevelName(this, true);
	const int32 Chapter = FCString::Atoi(*Map.RightChop(3));
	if (!Definition) Definition = LoadObject<UPFCampaignDefinition>(nullptr, *UPFCampaignDefinition::GetAssetPath(Chapter));
	if (!Run) { Fail(TEXT("캠페인 세션 상태 없음")); return; }
	if (Progress.Phase == EPFCampaignPhase::Fault || !ValidateLayout()) return;
	if (Definition->Chapter != Chapter) { Fail(TEXT("현재 맵과 캠페인 정의의 챕터 불일치")); return; }
	Run->EnterChapter(Chapter);
	Progress.Step = Run->CheckpointStep;
	ActiveCheckpoint = Run->Checkpoint;
	Completed = Run->CompletedEncounters;
	Claims = Run->SupplyClaims;
	Progress.SupplyClaims = Claims.Array();
	bReady = true;
}

// 이름으로 배치 기준 조회
APFCampaignAnchor* APFCampaignDirector::Anchor(FName Id) const
{
	const TObjectPtr<APFCampaignAnchor>* Value = Anchors.Find(Id);
	return Value ? Value->Get() : nullptr;
}

// 필수 배치와 챕터 정의 검사
bool APFCampaignDirector::ValidateLayout()
{
	if (!Definition || Definition->Steps.IsEmpty())
	{
		Fail(TEXT("캠페인 정의 에셋이 없거나 목표가 비어 있습니다."));
		return false;
	}
	if (Definition->Chapter == 8 && !GetMutableDefault<APFCampaignShockwaveMarker>()->TelegraphMaterial.LoadSynchronous())
	{
		Fail(TEXT("지휘관 범위 예고 머티리얼 누락"));
		return false;
	}
	for (const auto& Entry : Anchors)
		if (Entry.Value->Prop != EPFCampaignProp::None && !Entry.Value->Visual->GetStaticMesh())
		{
			Fail(FString::Printf(TEXT("장치 메시 누락: %s"), *Entry.Key.ToString()));
			return false;
		}
	TSet<FName> Required;
	TSet<FName> Checkpoints = { Definition->StartCheckpoint };
	TSet<FName> ShotIds;
	TSet<FName> EncounterIds;
	if (!Definition->IntroShot.IsNone()) ShotIds.Add(Definition->IntroShot);
	for (const FPFCampaignEncounter& Encounter : Definition->Encounters)
	{
		if (Encounter.Id.IsNone() || EncounterIds.Contains(Encounter.Id) || Encounter.Enemies.IsEmpty())
		{
			Fail(TEXT("전투 ID 또는 적 구성 오류"));
			return false;
		}
		EncounterIds.Add(Encounter.Id);
		Required.Add(Encounter.Trigger);
		for (const auto* Wave : { &Encounter.Enemies, &Encounter.Reinforcements })
			for (const FPFCampaignEnemy& Enemy : *Wave)
			{
				Required.Add(Enemy.Anchor);
				for (FName Point : Enemy.Patrol) Required.Add(Point);
			}
	}
	for (const FPFCampaignStep& Step : Definition->Steps)
	{
		if (Step.Type == EPFCampaignObjective::Defeat && Step.RequiredEncounters.IsEmpty())
		{
			Fail(TEXT("처치 목표의 필수 전투 누락"));
			return false;
		}
		Required.Add(Step.Target);
		for (FName Point : Step.Waypoints) Required.Add(Point);
		if (Step.bCheckpoint) Checkpoints.Add(Step.Checkpoint);
		if (!Step.Shot.IsNone()) ShotIds.Add(Step.Shot);
		for (FName Id : Step.RequiredEncounters)
			if (!EncounterIds.Contains(Id)) { Fail(TEXT("목표의 전투 참조 누락")); return false; }
	}
	for (const FPFCampaignSupply& Supply : Definition->Supplies) Required.Add(Supply.Anchor);
	for (FName Checkpoint : Checkpoints)
		for (int32 Index = 1; Index <= 4; ++Index)
			Required.Add(FName(FString::Printf(TEXT("%s_Spawn_%02d"), *Checkpoint.ToString(), Index)));
	for (FName Id : Required)
		if (!Anchor(Id)) { Fail(FString::Printf(TEXT("필수 배치 누락: %s"), *Id.ToString())); return false; }
	for (FName Id : ShotIds)
	{
		const TObjectPtr<APFCampaignShot>* Shot = Shots.Find(Id);
		if (!Shot || !IsValid(*Shot) || !(*Shot)->LookAt || (*Shot)->Duration <= 0.f
			|| (*Shot)->Path->GetNumberOfSplinePoints() < 2 || (*Shot)->Path->GetSplineLength() < 1.f)
		{
			Fail(FString::Printf(TEXT("카메라 경로 또는 주시 대상 누락: %s"), *Id.ToString()));
			return false;
		}
	}
	return true;
}

// 진행 불가 원인 표시
void APFCampaignDirector::Fail(const FString& Reason)
{
	Progress.Phase = EPFCampaignPhase::Fault;
	Progress.Error = FText::FromString(Reason);
	UE_LOG(LogTemp, Error, TEXT("Campaign Map=%s Step=%d: %s"), *GetWorld()->GetMapName(), Progress.Step, *Reason);
	ForceNetUpdate();
}

// 연결된 파티 조회
TArray<APFPlayerController*> APFCampaignDirector::Party() const
{
	TArray<APFPlayerController*> Result;
	for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		if (APFPlayerController* Player = Cast<APFPlayerController>(It->Get())) Result.Add(Player);
	Result.Sort([](const APFPlayerController& A, const APFPlayerController& B)
	{
		const APlayerState* AS = A.PlayerState;
		const APlayerState* BS = B.PlayerState;
		return AS && BS ? AS->GetPlayerId() < BS->GetPlayerId() : A.GetUniqueID() < B.GetUniqueID();
	});
	return Result;
}

// 서버 기준 시간
double APFCampaignDirector::ServerTime() const
{
	const AGameStateBase* State = GetWorld()->GetGameState();
	return State ? State->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}

// 지정 시작점의 NavMesh, 캡슐 충돌 확인
bool APFCampaignDirector::FindPlayerStart(APFPlayerController* Player, FTransform& Result)
{
	if (!HasAuthority() || !bReady || !Player) return false;
	const TArray<APFPlayerController*> Members = Party();
	const int32 Slot = Members.IndexOfByKey(Player);
	if (Slot < 0 || Slot >= 4) return false;
	APFGameMode* Mode = GetWorld()->GetAuthGameMode<APFGameMode>();
	UClass* PawnClass = Mode ? Mode->GetDefaultPawnClassForController(Player) : nullptr;
	const APFCharacter* Default = PawnClass ? PawnClass->GetDefaultObject<APFCharacter>() : nullptr;
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	const UCapsuleComponent* Capsule = Default ? Default->GetCapsuleComponent() : nullptr;
	if (!Nav || !Capsule) { Fail(TEXT("시작점 NavMesh 또는 캡슐 없음")); return false; }
	FCollisionQueryParams Query(SCENE_QUERY_STAT(CampaignStart), false, Player->GetPawn());
	for (int32 Offset = 0; Offset < 4; ++Offset)
	{
		APFCampaignAnchor* Start = Anchor(FName(FString::Printf(TEXT("%s_Spawn_%02d"), *ActiveCheckpoint.ToString(), (Slot + Offset) % 4 + 1)));
		FNavLocation Ground;
		if (!Nav->ProjectPointToNavigation(Start->GetActorLocation(), Ground, FVector(30.f, 30.f, 120.f))
			|| FVector::DistSquared2D(Ground.Location, Start->GetActorLocation()) > FMath::Square(30.f)) continue;
		const FVector Location = Ground.Location + FVector(0.f, 0.f, Capsule->GetScaledCapsuleHalfHeight() + 2.f);
		if (GetWorld()->OverlapBlockingTestByChannel(Location, FQuat::Identity, Capsule->GetCollisionObjectType(),
			FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()), Query,
			FCollisionResponseParams(Capsule->GetCollisionResponseToChannels()))) continue;
		Result = FTransform(Start->GetActorRotation(), Location);
		return true;
	}
	Fail(FString::Printf(TEXT("사용 가능한 지정 시작점 없음: %s, 슬롯 %d"), *ActiveCheckpoint.ToString(), Slot + 1));
	return false;
}

// 전투 중 참가자의 관전 대기
bool APFCampaignDirector::DeferJoiningPlayer(APFPlayerController* Player)
{
	if (!bReady) return true;
	if (Player->IsCampaignWaiting() || IsCombatActive() || Progress.Phase != EPFCampaignPhase::Playing)
	{
		Player->SetCampaignWaiting(true);
		return true;
	}
	return false;
}

// 플레이어 사망 집계
void APFCampaignDirector::HandlePlayerDeath(APFPlayerController* Player)
{
	if (!HasAuthority() || !Player || !Run || Player->IsCampaignWaiting()) return;
	Player->SetCampaignWaiting(true);
	++Run->Deaths;
	Progress.Deaths = Run->Deaths;
}

// 진행 중 전투 확인
bool APFCampaignDirector::IsCombatActive() const
{
	for (const auto& Entry : Encounters)
		if (Entry.Value.Remaining > 0) return true;
	return false;
}

// 연출, 복구 중 적 행동 제한
bool APFCampaignDirector::IsEncounterPaused() const
{
	return Progress.Phase != EPFCampaignPhase::Playing;
}

// 살아 있는 파티의 합류 확인
bool APFCampaignDirector::PartyReady(FName Target) const
{
	const APFCampaignAnchor* Point = Anchor(Target);
	int32 Alive = 0;
	for (APFPlayerController* Player : Party())
	{
		const APFCharacter* Character = Cast<APFCharacter>(Player->GetPawn());
		if (Player->IsCampaignWaiting() || !Character || Character->IsDeadCharacter()) continue;
		++Alive;
		if (!Player->HasClientLoadedCurrentWorld() || Character->IsLevelStartActive() || Character->IsJumpPadFlightActive()
			|| !Character->GetCharacterMovement()->IsMovingOnGround()
			|| FVector::DistSquared(Character->GetActorLocation(), Point->GetActorLocation()) > FMath::Square(1000.f)) return false;
	}
	return Alive > 0;
}

// 장치 거리, 가림, 조작 상태 확인
bool APFCampaignDirector::CanReach(const APFPlayerController* Player, const APFCampaignAnchor* Target) const
{
	const APFCharacter* Character = Player ? Cast<APFCharacter>(Player->GetPawn()) : nullptr;
	if (!Character || !Target || Character->IsDeadCharacter() || Character->IsLevelStartActive()
		|| Character->IsJumpPadFlightActive() || Player->IsCampaignWaiting()
		|| FVector::DistSquared(Character->GetActorLocation(), Target->GetActorLocation()) > FMath::Square(200.f)) return false;
	FHitResult Hit;
	const FCollisionQueryParams Query(SCENE_QUERY_STAT(CampaignInteract), true, Character);
	return !GetWorld()->LineTraceSingleByChannel(Hit, Character->GetPawnViewLocation(),
		Target->GetActorLocation() + FVector(0.f, 0.f, 60.f), ECC_Visibility, Query) || Hit.GetActor() == Target;
}

// 서버 장치 조작, 개인별 보급 지급
void APFCampaignDirector::Interact(APFPlayerController* Player)
{
	if (!HasAuthority() || !Definition || !Run || Progress.Phase != EPFCampaignPhase::Playing) return;
	APFPlayerState* State = Player ? Player->GetPlayerState<APFPlayerState>() : nullptr;
	if (!State) return;
	const FPFCampaignStep* Step = FindInteractionStep(Player);
	if (Step && PartyReady(Step->Target))
	{
		Advance();
		return;
	}
	FString Claim;
	const FPFCampaignSupply* Supply = FindSupply(Player, Claim);
	if (!Supply || !State->GrantCampaignSupply()) return;
	Claims.Add(Claim);
	Progress.SupplyClaims = Claims.Array();
	if (!Supply->Record.IsEmpty())
	{
		Run->Records.Add(FString::Printf(TEXT("%d/%s"), Definition->Chapter, *Supply->Anchor.ToString()));
		Progress.Radio = Supply->Record;
	}
}

// 지정 전투의 적 생성
bool APFCampaignDirector::SpawnWave(const FPFCampaignEncounter& Encounter, const TArray<FPFCampaignEnemy>& Wave, FEncounterRuntime& Runtime)
{
	APFGameMode* Mode = GetWorld()->GetAuthGameMode<APFGameMode>();
	if (!Mode) return false;
	for (const FPFCampaignEnemy& Entry : Wave)
	{
		APFCampaignAnchor* Point = Anchor(Entry.Anchor);
		APFCharacter* Enemy = Mode->SpawnCampaignEnemy(Entry.bKwang ? APFKwang::StaticClass() : APFTwinBlast::StaticClass(), Point->GetActorTransform());
		if (!Enemy)
		{
			Fail(FString::Printf(TEXT("적 생성 실패: %s/%s"), *Encounter.Id.ToString(), *Entry.Anchor.ToString()));
			return false;
		}
		Runtime.Enemies.Add(Enemy);
		++Runtime.Remaining;
		Enemy->OnCharacterDied.AddUObject(this, &APFCampaignDirector::EnemyDied);
		APFCampaignEnemyController* Controller = Cast<APFCampaignEnemyController>(Enemy->GetController());
		if (!Controller) { Fail(FString::Printf(TEXT("적 AI 연결 실패: %s"), *Entry.Anchor.ToString())); return false; }
		TArray<FVector> Patrol;
		for (FName Id : Entry.Patrol) Patrol.Add(Anchor(Id)->GetActorLocation());
		Controller->Configure(Point->GetActorLocation(), Patrol, this, Entry.bCommander, Entry.bFinalBoss);
		UPFAttributeSet* Stats = Enemy->GetAttributeSet();
		const float HealthScale = (Entry.bFinalBoss ? 4.f : Entry.bCommander ? 2.f : 1.f)
			* (1.f + (Entry.bCommander ? .65f : .35f) * (Runtime.Players - 1));
		FPFStatValues InitialStats(*Stats);
		InitialStats.MaxHealth *= HealthScale;
		InitialStats.Health = InitialStats.MaxHealth;
		InitialStats.Mana = InitialStats.MaxMana;
		InitialStats.AttackPower *= Entry.bFinalBoss ? 1.25f : 1.f;
		InitialStats.Coin = 0.f;
		InitialStats.StatPoint = 0.f;
		if (!FPFGE_StatGameplayEffects::InitializeStats(Enemy->GetAbilitySystemComponent(), InitialStats))
		{
			Fail(FString::Printf(TEXT("적 스탯 초기화 실패: %s"), *Entry.Anchor.ToString()));
			return false;
		}
		if (Entry.bFinalBoss) Progress.Boss = Enemy;
	}
	return true;
}

// 전투 시작 시 인원 고정
bool APFCampaignDirector::SpawnEncounter(const FPFCampaignEncounter& Encounter)
{
	FEncounterRuntime& Runtime = Encounters.Add(Encounter.Id);
	Runtime.Players = 0;
	for (APFPlayerController* Player : Party())
		if (!Player->IsCampaignWaiting()) ++Runtime.Players;
	Runtime.Players = FMath::Clamp(Runtime.Players, 1, 4);
	return SpawnWave(Encounter, Encounter.Enemies, Runtime);
}

// 전투 시작, 증원, 필수 처치 수 갱신
void APFCampaignDirector::UpdateEncounters(const TArray<APFPlayerController*>& Members)
{
	for (const FPFCampaignEncounter& Encounter : Definition->Encounters)
	{
		if (Completed.Contains(Encounter.Id)) continue;
		if (!Encounters.Contains(Encounter.Id) && Progress.Step >= Encounter.FirstStep && Progress.Step <= Encounter.LastStep)
		{
			for (APFPlayerController* Player : Members)
			{
				const APFCharacter* Character = Cast<APFCharacter>(Player->GetPawn());
				if (!Player->IsCampaignWaiting() && Character && !Character->IsDeadCharacter()
					&& FVector::DistSquared(Character->GetActorLocation(), Anchor(Encounter.Trigger)->GetActorLocation()) <= FMath::Square(Encounter.TriggerRadius))
				{
					if (!SpawnEncounter(Encounter)) return;
					break;
				}
			}
		}
		FEncounterRuntime* Runtime = Encounters.Find(Encounter.Id);
		if (!Runtime) continue;
		if (!Runtime->bReinforced && !Encounter.Reinforcements.IsEmpty())
		{
			for (int32 Index = 0; Index < Encounter.Enemies.Num(); ++Index)
			{
				if (!Encounter.Enemies[Index].bCommander) continue;
				const APFCharacter* Commander = Runtime->Enemies[Index].Get();
				const UPFAttributeSet* Stats = Commander ? Commander->GetAttributeSet() : nullptr;
				if (!Commander || (Stats && Stats->GetHealth() <= Stats->GetMaxHealth() * .5f))
				{
					Runtime->bReinforced = true;
					if (!SpawnWave(Encounter, Encounter.Reinforcements, *Runtime)) return;
				}
				break;
			}
		}
		for (const TWeakObjectPtr<APFCharacter>& Enemy : Runtime->Enemies)
			if (!Enemy.IsValid() && !CountedDeaths.Contains(Enemy))
			{
				Fail(FString::Printf(TEXT("사망 판정 없이 필수 적 제거: %s"), *Encounter.Id.ToString()));
				return;
			}
		if (Runtime->Remaining == 0) Completed.Add(Encounter.Id);
	}
	Progress.RemainingEnemies = 0;
	if (!Definition->Steps.IsValidIndex(Progress.Step)) return;
	const FPFCampaignStep& Step = Definition->Steps[Progress.Step];
	for (FName Id : Step.RequiredEncounters)
	{
		if (Completed.Contains(Id)) continue;
		const FEncounterRuntime* Runtime = Encounters.Find(Id);
		if (Runtime) Progress.RemainingEnemies += Runtime->Remaining;
		else
			for (const FPFCampaignEncounter& Encounter : Definition->Encounters)
				if (Encounter.Id == Id) Progress.RemainingEnemies += Encounter.Enemies.Num();
	}
	if (Step.Type == EPFCampaignObjective::Defeat && Progress.RemainingEnemies == 0) Advance();
}

// 사망한 적 한 번만 집계
void APFCampaignDirector::EnemyDied(APFCharacter* Enemy)
{
	if (!Enemy || CountedDeaths.Contains(Enemy)) return;
	CountedDeaths.Add(Enemy);
	for (auto& Entry : Encounters)
		if (Entry.Value.Enemies.Contains(Enemy))
		{
			Entry.Value.Remaining = FMath::Max(0, Entry.Value.Remaining - 1);
			break;
		}
}

// 캠페인 소유 전투 액터만 정리
void APFCampaignDirector::ClearEncounters()
{
	TSet<APFCharacter*> Sources;
	for (APFPlayerController* Player : Party())
		if (APFCharacter* Character = Cast<APFCharacter>(Player->GetPawn())) Sources.Add(Character);
	for (const auto& Entry : Encounters)
		for (TWeakObjectPtr<APFCharacter> Enemy : Entry.Value.Enemies)
			if (Enemy.IsValid()) Sources.Add(Enemy.Get());
	for (TActorIterator<AProjectile> It(GetWorld()); It; ++It)
		if (It->IsPoolActive() && Sources.Contains(It->GetSourceCharacter())) It->ReturnToPool();
	for (auto& Entry : Encounters)
		for (TWeakObjectPtr<APFCharacter> Enemy : Entry.Value.Enemies)
			if (Enemy.IsValid())
			{
				Enemy->OnCharacterDied.RemoveAll(this);
				Enemy->Destroy();
			}
	Encounters.Reset();
	CountedDeaths.Reset();
	Progress.Boss = nullptr;
}

// 체크포인트에서 생존자 회복, 관전자 복귀
bool APFCampaignDirector::RestoreParty(bool bRollback, bool bRespawnAll)
{
	APFGameMode* Mode = GetWorld()->GetAuthGameMode<APFGameMode>();
	if (!Mode) return false;
	const TArray<APFPlayerController*> Members = Party();
	for (APFPlayerController* Player : Members)
		if (!Player->HasClientLoadedCurrentWorld()) return false;
	for (APFPlayerController* Player : Members)
	{
		APFPlayerState* State = Player->GetPlayerState<APFPlayerState>();
		if (!State) return false;
		State->PrepareCampaignCheckpoint();
		if (State->GetAttributeSet()->GetMaxHealth() <= 0.f)
		{
			Fail(TEXT("체크포인트 플레이어 스탯 준비 실패"));
			return false;
		}
		const bool bHasSnapshot = Run->Players.Contains(Run->PlayerKey(State));
		if ((bRollback || !Player->GetPawn()) && bHasSnapshot && !Run->RestorePlayer(State))
		{
			Fail(TEXT("플레이어 체크포인트 데이터 복원 실패"));
			return false;
		}
		APFCharacter* Character = Cast<APFCharacter>(Player->GetPawn());
		if (bRespawnAll || bRollback || Player->IsCampaignWaiting() || !Character || Character->IsDeadCharacter())
		{
			if (!Mode->RespawnCampaignPlayer(Player)) { Fail(TEXT("체크포인트 복귀 실패")); return false; }
			if (const FPFCampaignPlayerSnapshot* Snapshot = Run->Players.Find(Run->PlayerKey(State)))
				Player->SetCampaignViewState(Snapshot->View);
		}
		Player->SetCampaignWaiting(false);
		RegisteredPlayers.Add(Player);
		if (!bHasSnapshot) Run->CapturePlayer(State);
	}
	return true;
}

// 플레이어와 현재 구간의 적 등장 시작
void APFCampaignDirector::StartStage()
{
	Progress.Phase = EPFCampaignPhase::Starting;
	if (!Definition->Steps.IsValidIndex(Progress.Step)) { FinishChapter(); return; }
	APFGameMode* Mode = GetWorld()->GetAuthGameMode<APFGameMode>();
	if (!Mode) { Fail(TEXT("캠페인 게임 모드 없음")); return; }
	if (!Mode->IsCampaignPartyReady()) return;
	ClearEncounters();
	if (!RestoreParty(bRollbackPending, true)) return;
	for (const FPFCampaignEncounter& Encounter : Definition->Encounters)
	{
		if (Completed.Contains(Encounter.Id) || Progress.Step < Encounter.FirstStep || Progress.Step > Encounter.LastStep) continue;
		if (!SpawnEncounter(Encounter)) return;
	}
	bRollbackPending = false;
	bStageSpawned = true;
	ForceNetUpdate();
}

// 파티와 목표의 복구 기준 확정
bool APFCampaignDirector::SaveCheckpoint(FName Id, int32 NextStep)
{
	APFGameMode* Mode = GetWorld()->GetAuthGameMode<APFGameMode>();
	if (!Mode || !Mode->IsCampaignPartyReady()) return false;
	ActiveCheckpoint = Id;
	if (!RestoreParty(false)) return false;
	ClearEncounters();
	Run->Checkpoint = Id;
	Run->CheckpointStep = NextStep;
	Run->CompletedEncounters = Completed;
	Run->SupplyClaims = Claims;
	Run->CheckpointRecords = Run->Records;
	for (APFPlayerController* Player : Party())
		if (Player->GetPawn()) Run->CapturePlayer(Player->GetPlayerState<APFPlayerState>());
	return true;
}

// 목표 완료, 연출과 다음 단계 연결
void APFCampaignDirector::Advance()
{
	if (!Definition->Steps.IsValidIndex(Progress.Step)) return;
	const FPFCampaignStep& Step = Definition->Steps[Progress.Step];
	if (!Step.Radio.IsEmpty()) Progress.Radio = Step.Radio;
	if (Step.bCheckpoint && !SaveCheckpoint(Step.Checkpoint, Progress.Step + 1)) return;
	if (!Step.Shot.IsNone())
	{
		StartShot(Step.Shot, true);
		return;
	}
	++Progress.Step;
	if (!Definition->Steps.IsValidIndex(Progress.Step)) FinishChapter();
	ForceNetUpdate();
}

// 파티 공통 카메라 시작
void APFCampaignDirector::StartShot(FName Id, bool bAdvanceAfter)
{
	Progress.Shot = Id;
	Progress.Phase = EPFCampaignPhase::Camera;
	Progress.PhaseStarted = ServerTime();
	bAdvanceAfterShot = bAdvanceAfter;
	Voters.Reset();
	for (APFPlayerController* Player : Party())
		if (APFCharacter* Character = Cast<APFCharacter>(Player->GetPawn()))
		{
			Character->ClearControlCommands();
			Character->GetCharacterMovement()->StopMovementImmediately();
		}
	ForceNetUpdate();
}

// 현재 카메라 경로 조회
APFCampaignShot* APFCampaignDirector::GetCurrentShot() const
{
	const TObjectPtr<APFCampaignShot>* Shot = Shots.Find(Progress.Shot);
	return Shot ? Shot->Get() : nullptr;
}

// 연출 종료, 정상 진행과 복구 구간 연결
void APFCampaignDirector::FinishShot()
{
	const FName FinishedShot = Progress.Shot;
	Progress.Shot = NAME_None;
	if (bAdvanceAfterShot)
	{
		bAdvanceAfterShot = false;
		++Progress.Step;
		Progress.Phase = EPFCampaignPhase::Playing;
		if (!Definition->Steps.IsValidIndex(Progress.Step)) FinishChapter();
		else if (FinishedShot == FName(TEXT("BossIntro")))
			for (const FPFCampaignEncounter& Encounter : Definition->Encounters)
				if (Encounter.Id == FName(TEXT("Boss")) && !Encounters.Contains(Encounter.Id) && !SpawnEncounter(Encounter)) return;
	}
	else
	{
		bStageSpawned = false;
		StartStage();
	}
	ForceNetUpdate();
}

// 연결된 참가자의 연출 건너뛰기 투표
void APFCampaignDirector::VoteSkip(APFPlayerController* Player)
{
	if (HasAuthority() && Progress.Phase == EPFCampaignPhase::Camera && Party().Contains(Player)) Voters.Add(Player);
}

// 전멸 구간 복구
void APFCampaignDirector::ResetCheckpoint()
{
	ClearEncounters();
	RouteProgress.Reset();
	Progress.Step = Run->CheckpointStep;
	Completed = Run->CompletedEncounters;
	Claims = Run->SupplyClaims;
	Progress.SupplyClaims = Claims.Array();
	Run->Records = Run->CheckpointRecords;
	ActiveCheckpoint = Run->Checkpoint;
	bStageSpawned = false;
	bRollbackPending = true;
	Progress.Phase = EPFCampaignPhase::Preparing;
	Progress.Radio = FText::FromString(TEXT("마지막 체크포인트에서 재시작합니다."));
	ForceNetUpdate();
}

// 챕터 이동 또는 캠페인 완료
void APFCampaignDirector::FinishChapter()
{
	if (Definition->Chapter == 8)
	{
		Progress.Records = Run->Records.Num();
		Progress.Deaths = Run->Deaths;
		Progress.Elapsed = Run->Elapsed;
		Progress.Phase = EPFCampaignPhase::Complete;
		ClearEncounters();
		return;
	}
	for (APFPlayerController* Player : Party())
	{
		if (!Player->HasClientLoadedCurrentWorld()) return;
		if (APFPlayerState* State = Player->GetPlayerState<APFPlayerState>())
		{
			State->PrepareCampaignCheckpoint();
			Run->CapturePlayer(State);
		}
		Player->CaptureCharacterState();
	}
	const int32 Next = Definition->Chapter == 3 ? 4 : Definition->Chapter == 4 ? 6 : 8;
	Progress.Phase = EPFCampaignPhase::Traveling;
	GetWorld()->GetAuthGameMode()->bUseSeamlessTravel = true;
	const bool bStarted = APFGameMode::ServerTravel(GetWorld(), FString::Printf(TEXT("/Game/ThirdPerson/Maps/Map%d?PFMode=Campaign?SeamlessTravel"), Next));
	if (!bStarted)
		Fail(TEXT("다음 챕터 이동 요청 실패"));
}

// 신규 참가자의 체크포인트 기준 보관
void APFCampaignDirector::UpdatePlayers(const TArray<APFPlayerController*>& Members)
{
	if (APFGameMode* Mode = GetWorld()->GetAuthGameMode<APFGameMode>()) Mode->RetryCampaignSpawns();
	Progress.Living = 0;
	Progress.Gathered = 0;
	const APFCampaignAnchor* Target = Definition->Steps.IsValidIndex(Progress.Step) ? Anchor(Definition->Steps[Progress.Step].Target) : nullptr;
	for (APFPlayerController* Player : Members)
	{
		APFCharacter* Character = Cast<APFCharacter>(Player->GetPawn());
		if (!Character || Character->IsDeadCharacter() || Player->IsCampaignWaiting()) continue;
		++Progress.Living;
		if (Target && Character->GetCharacterMovement()->IsMovingOnGround()
			&& FVector::DistSquared(Character->GetActorLocation(), Target->GetActorLocation()) <= FMath::Square(1000.f)) ++Progress.Gathered;
		if (!RegisteredPlayers.Contains(Player) && !Character->IsLevelStartActive())
		{
			RegisteredPlayers.Add(Player);
			Run->CapturePlayer(Player->GetPlayerState<APFPlayerState>());
		}
	}
}

void APFCampaignDirector::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	RefreshProps();
	if (!HasAuthority() || !bReady || Progress.Phase == EPFCampaignPhase::Fault || Progress.Phase == EPFCampaignPhase::Complete) return;
	Run->Elapsed += DeltaTime;
	Progress.Elapsed = Run->Elapsed;
	Progress.Deaths = Run->Deaths;
	Progress.Records = Run->Records.Num();
	const TArray<APFPlayerController*> Members = Party();
	UpdatePlayers(Members);
	if (Progress.Phase == EPFCampaignPhase::Fault) return;
	if (Progress.Phase == EPFCampaignPhase::Preparing)
	{
		APFGameMode* Mode = GetWorld()->GetAuthGameMode<APFGameMode>();
		if (!Mode || !Mode->IsCampaignPartyReady()) return;
		if (Progress.Step == 0 && !Definition->IntroShot.IsNone())
		{
			Progress.Radio = Definition->IntroRadio;
			StartShot(Definition->IntroShot, false);
			return;
		}
		else if (Definition->Steps.IsValidIndex(Progress.Step) && Definition->Steps.IsValidIndex(Progress.Step - 1))
		{
			const FPFCampaignStep& PreviousStep = Definition->Steps[Progress.Step - 1];
			if (!PreviousStep.Shot.IsNone())
			{
				if (!PreviousStep.Radio.IsEmpty()) Progress.Radio = PreviousStep.Radio;
				StartShot(PreviousStep.Shot, false);
				return;
			}
		}
		StartStage();
		return;
	}
	if (Progress.Phase == EPFCampaignPhase::Starting)
	{
		if (!bStageSpawned) { StartStage(); return; }
		if (Members.IsEmpty()) return;
		for (APFPlayerController* Player : Members)
		{
			if (Player->IsCampaignWaiting()) continue;
			const APFCharacter* Character = Cast<APFCharacter>(Player->GetPawn());
			if (!Character || Character->IsLevelStartActive() || !Player->HasClientLoadedCurrentWorld()) return;
		}
		for (const auto& Entry : Encounters)
			for (const TWeakObjectPtr<APFCharacter>& Enemy : Entry.Value.Enemies)
				if (Enemy.IsValid() && !Enemy->IsDeadCharacter() && Enemy->IsLevelStartActive()) return;
		Progress.Phase = EPFCampaignPhase::Playing;
		ForceNetUpdate();
		return;
	}
	if (Progress.Phase == EPFCampaignPhase::Resetting)
	{
		if (ServerTime() - Progress.PhaseStarted >= 5.) ResetCheckpoint();
		return;
	}
	if (Progress.Phase == EPFCampaignPhase::Camera)
	{
		Progress.Viewers = Members.Num();
		Progress.SkipVotes = 0;
		for (APFPlayerController* Player : Members) if (Voters.Contains(Player)) ++Progress.SkipVotes;
		const APFCampaignShot* Shot = GetCurrentShot();
		if (ServerTime() - Progress.PhaseStarted >= Shot->Duration
			|| (Progress.Viewers > 0 && Progress.SkipVotes == Progress.Viewers)) FinishShot();
		return;
	}
	if (Progress.Phase != EPFCampaignPhase::Playing) return;
	if (Progress.Living == 0 && !Members.IsEmpty())
	{
		Progress.Phase = EPFCampaignPhase::Resetting;
		Progress.PhaseStarted = ServerTime();
		ForceNetUpdate();
		return;
	}
	UpdateEncounters(Members);
	if (Progress.Phase != EPFCampaignPhase::Playing) return;
	if (!Definition->Steps.IsValidIndex(Progress.Step)) { FinishChapter(); return; }
	const FPFCampaignStep& Step = Definition->Steps[Progress.Step];
	if (Step.Type == EPFCampaignObjective::Reach)
	{
		for (APFPlayerController* Player : Members)
			if (!Player->IsCampaignWaiting() && Player->GetPawn()
				&& FVector::DistSquared(Player->GetPawn()->GetActorLocation(), Anchor(Step.Target)->GetActorLocation()) < FMath::Square(500.f))
			{
				Advance();
				break;
			}
	}
}

// 목표 장치와 안정화된 핵 표시
void APFCampaignDirector::RefreshProps()
{
	if (!Definition || (VisualDefinition.Get() == Definition.Get() && VisualStep == Progress.Step)) return;
	VisualDefinition = Definition;
	VisualStep = Progress.Step;
	const FName Target = Definition->Steps.IsValidIndex(Progress.Step) ? Definition->Steps[Progress.Step].Target : NAME_None;
	for (const auto& Entry : Anchors)
	{
		const bool bSupply = Definition->Supplies.ContainsByPredicate([&Entry](const FPFCampaignSupply& Supply) { return Supply.Anchor == Entry.Key; });
		const bool bRoute = Definition->Steps.IsValidIndex(Progress.Step) && Definition->Steps[Progress.Step].Waypoints.Contains(Entry.Key);
		Entry.Value->SetActiveVisual(bSupply || bRoute || Entry.Key == Target || (Entry.Value->Prop == EPFCampaignProp::Core && Progress.Step >= 5));
	}
}

// 현재 목표 문구
FText APFCampaignDirector::GetObjectiveText() const
{
	return Definition && Definition->Steps.IsValidIndex(Progress.Step) ? Definition->Steps[Progress.Step].Objective : FText::GetEmpty();
}

// 접근 가능한 진행 목표 조회
const FPFCampaignStep* APFCampaignDirector::FindInteractionStep(const APFPlayerController* Player) const
{
	if (!Definition->Steps.IsValidIndex(Progress.Step)) return nullptr;
	const FPFCampaignStep& Step = Definition->Steps[Progress.Step];
	return (Step.Type == EPFCampaignObjective::Interact || Step.Type == EPFCampaignObjective::Rally)
		&& CanReach(Player, Anchor(Step.Target)) ? &Step : nullptr;
}

// 개인별 미수령 보급품 조회
const FPFCampaignSupply* APFCampaignDirector::FindSupply(const APFPlayerController* Player, FString& Claim) const
{
	const APFPlayerState* State = Player ? Player->GetPlayerState<APFPlayerState>() : nullptr;
	const FString PlayerKey = UPFCampaignSubsystem::PlayerKey(State);
	for (const FPFCampaignSupply& Supply : Definition->Supplies)
	{
		Claim = FString::Printf(TEXT("%d/%s/%s"), Definition->Chapter, *Supply.Anchor.ToString(), *PlayerKey);
		const bool bClaimed = HasAuthority() ? Claims.Contains(Claim) : Progress.SupplyClaims.Contains(Claim);
		if (!bClaimed && CanReach(Player, Anchor(Supply.Anchor))) return &Supply;
	}
	return nullptr;
}

// 근처 장치 조작 안내
FText APFCampaignDirector::GetInteractionText(const APFPlayerController* Player) const
{
	if (!Definition || Progress.Phase != EPFCampaignPhase::Playing) return FText::GetEmpty();
	if (FindInteractionStep(Player))
		return FText::FromString(TEXT("G · 진행하려면 생존 동료가 모두 합류해야 합니다."));
	FString Claim;
	if (const FPFCampaignSupply* Supply = FindSupply(Player, Claim))
		return FText::FromString(Supply->Record.IsEmpty() ? TEXT("G · HP, MP 포션 보급") : TEXT("G · 보급품과 기록 조사"));
	return FText::GetEmpty();
}

// 미로의 실제 보행 경로를 따라 다음 모서리 안내
bool APFCampaignDirector::GetObjectiveLocation(APFPlayerController* Player, FVector& Result)
{
	if (!HasAuthority() || !Player || !Player->GetPawn() || !bReady || !Definition->Steps.IsValidIndex(Progress.Step)) return false;
	const FVector From = Player->GetPawn()->GetActorLocation();
	const FPFCampaignStep& Step = Definition->Steps[Progress.Step];
	APFCampaignAnchor* Target = Anchor(Step.Target);
	Result = Target->GetActorLocation();
	FIntPoint* Route = RouteProgress.Find(Player);
	if (!Route || Route->X != Progress.Step) Route = &RouteProgress.Add(Player, FIntPoint(Progress.Step, 0));
	while (Step.Waypoints.IsValidIndex(Route->Y))
	{
		APFCampaignAnchor* Point = Anchor(Step.Waypoints[Route->Y]);
		FHitResult Hit;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(CampaignRoute), false, Player->GetPawn());
		Query.AddIgnoredActor(Point);
		if (FVector::DistSquared2D(From, Point->GetActorLocation()) <= FMath::Square(400.f)
			&& !GetWorld()->LineTraceSingleByChannel(Hit, From, Point->GetActorLocation() + FVector(0.f, 0.f, 80.f), ECC_Visibility, Query))
		{
			++Route->Y;
			continue;
		}
		Result = Point->GetActorLocation();
		break;
	}
	if (Definition->Chapter == 8)
	{
		UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), From, Result);
		if (!Path || !Path->IsValid() || Path->IsPartial()) return false;
		for (int32 Index = 1; Index < Path->PathPoints.Num(); ++Index)
			if (FVector::DistSquared2D(From, Path->PathPoints[Index]) > FMath::Square(200.f))
			{
				Result = Path->PathPoints[Index];
				break;
			}
	}
	return true;
}

void APFCampaignDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority()) ClearEncounters();
	if (UPFWorldSubsystem* Subsystem = GetWorld()->GetSubsystem<UPFWorldSubsystem>())
		Subsystem->UnregisterCampaignDirector(this);
	Super::EndPlay(EndPlayReason);
}

void APFCampaignDirector::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APFCampaignDirector, Definition);
	DOREPLIFETIME(APFCampaignDirector, Progress);
}


#include "System/Framework/PFGameMode.h"
#include "System/Subsystems/PFGameInstanceSubsystem.h"
#include "Campaign/PFCampaignDirector.h"
#include "Campaign/PFCampaignSubsystem.h"
#include "Campaign/PFCampaignEnemyController.h"

#include "Projectile/Bullet.h"
#include "Character/TwinBlast/PFTwinBlast.h"
#include "System/Framework/PFPlayerState.h"
#include "System/Framework/PFGameInstance.h"
#include "System/Framework/PFSessionGameState.h"
#include "System/Subsystems/PFWorldSubsystem.h"
#include "System/Framework/PFPlayerController.h"
#include "System/Framework/PFEnemyAIController.h"
#include "System/Framework/PFTutorialManager.h"
#include "Character/PFCharacter.h"

#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameSession.h"
#include "AbilitySystemComponent.h"
#include "GAS/Attributes/PFAttributeSet.h"
#include "GAS/Effects/PFGE_StatGameplayEffects.h"
#include "GAS/PFGameplayTags.h"

APFGameMode::APFGameMode()
{
	for (int32 Chapter : { 3, 4, 6, 8 })
	{
		auto& Definition = CampaignDefinitions.Emplace_GetRef(FSoftObjectPath(UPFCampaignDefinition::GetAssetPath(Chapter)));
		Definition.ToSoftObjectPath().PostLoadPath(nullptr);
	}
	DefaultPawnClass = APFTwinBlast::StaticClass();
	PlayerControllerClass = APFPlayerController::StaticClass();
	PlayerStateClass = APFPlayerState::StaticClass();
	GameStateClass = APFSessionGameState::StaticClass();
	bUseSeamlessTravel = true;
	TutorialMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/ThirdPerson/Maps/Tutorial.Tutorial")));
	TestMap3 = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/ThirdPerson/Maps/Map3.Map3")));
	TutorialMap.ToSoftObjectPath().PostLoadPath(nullptr);
	TestMap3.ToSoftObjectPath().PostLoadPath(nullptr);
	for (int32 MapIndex = 4; MapIndex <= 8; ++MapIndex)
	{
		TSoftObjectPtr<UWorld>& Map = AdditionalTestMaps.Emplace_GetRef(FSoftObjectPath(
			FString::Printf(TEXT("/Game/ThirdPerson/Maps/Map%d.Map%d"), MapIndex, MapIndex)));
		Map.ToSoftObjectPath().PostLoadPath(nullptr);
	}
}

void APFGameMode::InitGameState()
{
	Super::InitGameState();
	APFSessionGameState* Session = GetGameState<APFSessionGameState>();
	if (!Session) return;
	const FString ModeOption = UGameplayStatics::ParseOption(OptionsString, TEXT("PFMode"));
	Session->Mode = ModeOption == TEXT("Campaign") ? EPFSessionMode::Story
		: ModeOption == TEXT("Training") ? EPFSessionMode::Training : EPFSessionMode::Versus;
	Session->Phase = UGameplayStatics::GetCurrentLevelName(this, true).Contains(TEXT("Title"))
		? EPFSessionPhase::Menu : EPFSessionPhase::Playing;
	Session->SelectedMap = FName(*UGameplayStatics::GetCurrentLevelName(this, true));
	if (const UPFGameInstance* Instance = GetGameInstance<UPFGameInstance>())
	{
		Session->Capacity = FMath::Clamp(Instance->GetPendingSessionCapacity(), 1, APFSessionGameState::GetMaxPlayers(Session->Mode));
		Session->RoomName = Instance->GetSessionRoomName();
	}
	Session->bInitialized = true;
	Session->ForceNetUpdate();
}

void APFGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	const UPFGameInstance* Instance = GetGameInstance<UPFGameInstance>();
	if (Instance && Instance->HasSessionMatchStarted()
		&& !UGameplayStatics::GetCurrentLevelName(this, true).Contains(TEXT("Title")))
	{
		ErrorMessage = TEXT("Match already started");
	}
}

void APFGameMode::PostLogin(APlayerController* NewPlayer)
{
	const UPFGameInstance* Instance = GetGameInstance<UPFGameInstance>();
	if (NewPlayer && !NewPlayer->IsLocalController() && Instance && Instance->HasSessionMatchStarted()
		&& !UGameplayStatics::GetCurrentLevelName(this, true).Contains(TEXT("Title")))
	{
		if (!GameSession || !GameSession->KickPlayer(NewPlayer,
			NSLOCTEXT("PFSession", "MatchAdmissionClosed", "This match has already started."))) NewPlayer->Destroy();
		return;
	}
	Super::PostLogin(NewPlayer);
}

void APFGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (UPFCampaignSubsystem::IsCampaign(GetWorld()) && !APFCampaignDirector::Find(GetWorld()))
		GetWorld()->SpawnActor<APFCampaignDirector>();

	auto* Pool = GetWorld()->GetSubsystem<UPFWorldSubsystem>();
	Pool->PreparePool(ABullet::StaticClass(), 10);

	// 훈련 모드의 튜토리얼 관리자 생성
	if (APFSessionGameState::IsTraining(GetWorld()) && !APFTutorialManager::Find(GetWorld()))
	{
		GetWorld()->SpawnActor<APFTutorialManager>();
	}
}

void APFGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	const UWorld* World = GetWorld();
	if (FPackageName::GetShortName(World->GetMapName()).Contains(TEXT("Title")))
	{
		return;
	}

	if (!UsesInitialSpawnFlow(NewPlayer))
	{
		Super::HandleStartingNewPlayer_Implementation(NewPlayer);
		return;
	}

	// 로그인 준비와 캐릭터 선택이 모두 끝난 뒤 최초 생성
	APFPlayerController* PlayerController = Cast<APFPlayerController>(NewPlayer);
	if (!PlayerController->GetPawn() && PlayerController->bInitialSpawnComplete)
	{
		PlayerController->bInitialSpawnComplete = false;
		PlayerController->bInitialSpawnInProgress = false;
		PlayerController->SetCampaignWaiting(false);
		if (APFPlayerState* State = PlayerController->GetPlayerState<APFPlayerState>())
		{
			PlayerController->InitialSpawnCharacter = State->GetCharacter();
			PlayerController->bInitialSpawnRequested = true;
		}
	}
	if (APFPlayerState* State = PlayerController->GetPlayerState<APFPlayerState>(); State && State->HasLobbySelection())
	{
		PlayerController->InitialSpawnCharacter = State->GetCharacter();
		PlayerController->bInitialSpawnRequested = true;
	}
	PlayerController->bMapPresentationReady = false;
	PlayerController->Client_PrepareMapPresentation(UPFCampaignSubsystem::IsCampaign(World));
	PlayerController->bInitialSpawnReady = true;
	TryStartInitialPlayer(PlayerController);
}

void APFGameMode::RestartPlayer(AController* NewPlayer)
{
	const UWorld* World = GetWorld();
	if (FPackageName::GetShortName(World->GetMapName()).Contains(TEXT("Title")))
	{
		return;
	}

	if (UsesInitialSpawnFlow(NewPlayer))
	{
		APFPlayerController* PlayerController = Cast<APFPlayerController>(NewPlayer);
		if (!PlayerController->bInitialSpawnComplete)
		{
			TryStartInitialPlayer(PlayerController);
			return;
		}
	}

	Super::RestartPlayer(NewPlayer);
}

// 지정 지면 위에 캠페인 적 생성
APFCharacter* APFGameMode::SpawnCampaignEnemy(UClass* EnemyClass, const FTransform& GroundTransform)
{
	if (!EnemyClass || !EnemyClass->IsChildOf(APFCharacter::StaticClass())
		|| !UPFCampaignSubsystem::IsCampaign(GetWorld())) return nullptr;
	const APFCharacter* Default = EnemyClass->GetDefaultObject<APFCharacter>();
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	FNavLocation Ground;
	if (!Nav || !Nav->ProjectPointToNavigation(GroundTransform.GetLocation(), Ground, FVector(30.f, 30.f, 120.f))
		|| FVector::DistSquared2D(Ground.Location, GroundTransform.GetLocation()) > FMath::Square(30.f)) return nullptr;
	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
	const FVector Location = Ground.Location + FVector(0.f, 0.f, Default->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.f);
	APFCharacter* Enemy = GetWorld()->SpawnActor<APFCharacter>(EnemyClass, Location, GroundTransform.Rotator(), Parameters);
	if (!Enemy) return nullptr;
	APFCampaignEnemyController* Controller = GetWorld()->SpawnActor<APFCampaignEnemyController>();
	if (!Controller) { Enemy->Destroy(); return nullptr; }
	Controller->Possess(Enemy);
	if (Controller->GetPawn() != Enemy || !Enemy->GetAttributeSet())
	{
		Controller->Destroy();
		Enemy->Destroy();
		return nullptr;
	}
	return Enemy;
}

// 준비가 끝난 캠페인 참가자의 최초 생성 재시도
void APFGameMode::RetryCampaignSpawns()
{
	for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		if (APFPlayerController* Player = Cast<APFPlayerController>(It->Get()); Player && !Player->bInitialSpawnComplete)
			TryStartInitialPlayer(Player);
}

// 캠페인 참가자의 맵 로딩, 캐릭터 선택 준비 확인
bool APFGameMode::IsCampaignPartyReady() const
{
	bool bHasPlayers = false;
	for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APFPlayerController* Player = Cast<APFPlayerController>(It->Get());
		if (!Player) return false;
		bHasPlayers = true;
		if (!Player->bInitialSpawnReady || !Player->bInitialSpawnRequested || !Player->bMapPresentationReady || Player->bInitialSpawnInProgress
			|| !Player->HasClientLoadedCurrentWorld() || !Player->GetPlayerState<APFPlayerState>()) return false;
	}
	return bHasPlayers;
}

// 체크포인트의 지정 위치로 플레이어 복귀
bool APFGameMode::RespawnCampaignPlayer(APFPlayerController* Player)
{
	if (!Player) return false;
	FTransform Start;
	if (!TryFindInitialPlayerSpawnTransform(Player, Start)) return false;
	Player->CaptureCharacterState();
	if (APawn* OldPawn = Player->GetPawn()) OldPawn->Destroy();
	RestartPlayerAtTransform(Player, Start);
	Player->bInitialSpawnComplete = IsValid(Player->GetPawn());
	Player->bInitialSpawnInProgress = false;
	Player->RestoreCharacterState();
	return Player->bInitialSpawnComplete;
}

// NavMesh 위에 AI 캐릭터 생성
APFCharacter* APFGameMode::SpawnEnemy(APFCharacter* ControlledPawn, UClass* EnemyClass)
{
	if (UPFCampaignSubsystem::IsCampaign(GetWorld())) return nullptr;
	UWorld* World = GetWorld();
	if (!IsValid(ControlledPawn) || ControlledPawn->GetWorld() != World
		|| !ControlledPawn->IsPlayerCharacter() || ControlledPawn->IsDeadCharacter() || ControlledPawn->IsJumpPadFlightActive())
	{
		return nullptr;
	}

	const FString LevelName = FPackageName::GetShortName(World->GetMapName());
	if (LevelName.Contains(TEXT("Title")))
	{
		return nullptr;
	}

	// 생성 조건 설정
	const APFCharacter* EnemyDefaultObject = EnemyClass ? EnemyClass->GetDefaultObject<APFCharacter>() : nullptr;
	const UCapsuleComponent* EnemyCapsule = EnemyDefaultObject ? EnemyDefaultObject->GetCapsuleComponent() : nullptr;
	const UCharacterMovementComponent* EnemyMovement = EnemyDefaultObject ? EnemyDefaultObject->GetCharacterMovement() : nullptr;
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!EnemyCapsule || !EnemyMovement || !Navigation)
	{
		PFLOG(Warning, TEXT("Enemy spawn failed: missing capsule, movement or navigation system"));
		return nullptr;
	}

	// 스폰 전후 발밑의 NavMesh 확인
	const auto IsOnSpawnNavMesh = [Navigation](const FVector& Feet,
		const UCharacterMovementComponent* Movement, const UCapsuleComponent* Capsule)
	{
		FNavAgentProperties AgentProperties = Movement->GetNavAgentPropertiesRef();
		if (Movement->ShouldUpdateNavAgentWithOwnersCollision())
		{
			AgentProperties.AgentRadius = Capsule->GetScaledCapsuleRadius();
			AgentProperties.AgentHeight = Capsule->GetScaledCapsuleHalfHeight() * 2.f;
		}
		const ARecastNavMesh* NavMesh = Cast<ARecastNavMesh>(Navigation->GetNavDataForProps(AgentProperties, Feet));
		constexpr float HorizontalTolerance = 1.f;
		const float HeightRange = Movement->MaxStepHeight + 6.f;
		FNavLocation NavLocation;
		return NavMesh && Navigation->ProjectPointToNavigation(Feet, NavLocation,
			FVector(HorizontalTolerance, HorizontalTolerance, HeightRange), NavMesh)
			&& FVector::DistSquared2D(Feet, NavLocation.Location) <= FMath::Square(HorizontalTolerance)
			&& FMath::Abs(NavLocation.Location.Z - Feet.Z) <= HeightRange;
	};

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemySpawn), false, ControlledPawn);
	QueryParams.AddIgnoredActor(ControlledPawn);
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Instigator = ControlledPawn;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

	// 공통 탐색 반경 내 위치를 찾아 생성
	const FVector SpawnCenter = ControlledPawn->GetActorLocation();
	const float SpawnRadius = GetDefault<APFEnemyAIController>()->GetTargetSearchRadius();
	int32 RemainingAttempts = 30;
	FTransform SpawnTransform;
	while (APFGameMode::FindSpawnTransform(World, EnemyCapsule, QueryParams, RemainingAttempts, false,
		SpawnTransform, &SpawnCenter, SpawnRadius))
	{
		const FVector CandidateFeet = SpawnTransform.GetLocation() - FVector(0.f, 0.f, EnemyCapsule->GetScaledCapsuleHalfHeight());
		if (!IsOnSpawnNavMesh(CandidateFeet, EnemyMovement, EnemyCapsule))
		{
			continue;
		}
		if (APFCharacter* SpawnedEnemy = World->SpawnActor<APFCharacter>(
			EnemyClass, SpawnTransform.GetLocation(), SpawnTransform.Rotator(), SpawnParameters))
		{
			// 충돌 보정된 실제 위치 확인
			const UCharacterMovementComponent* SpawnedMovement = SpawnedEnemy->GetCharacterMovement();
			if (FVector::DistSquared2D(SpawnedEnemy->GetActorLocation(), SpawnCenter) > FMath::Square(SpawnRadius)
				|| !IsOnSpawnNavMesh(SpawnedMovement->GetActorFeetLocation(),
				SpawnedMovement, SpawnedEnemy->GetCapsuleComponent()))
			{
				SpawnedEnemy->Destroy();
				continue;
			}
			SpawnedEnemy->SpawnDefaultController();
			if (!SpawnedEnemy->GetController())
			{
				SpawnedEnemy->Destroy();
				return nullptr;
			}
			PFLOG(Warning, TEXT("Enemy spawned: %s"), *SpawnedEnemy->GetClass()->GetName());
			return SpawnedEnemy;
		}
	}

	PFLOG(Warning, TEXT("Enemy spawn failed: no spawnable NavMesh ground found within %.0f cm"), SpawnRadius);
	return nullptr;
}

// PIE 이동 허용값 보존, 서버 맵 이동
bool APFGameMode::ServerTravel(UWorld* World, const FString& URL)
{
	IConsoleVariable* PIETravel = World->WorldType == EWorldType::PIE
		? IConsoleManager::Get().FindConsoleVariable(TEXT("net.AllowPIESeamlessTravel")) : nullptr;
	const int32 Previous = PIETravel ? PIETravel->GetInt() : 0;
	if (PIETravel) PIETravel->SetWithCurrentPriority(1);
	const bool bStarted = World->ServerTravel(URL, true);
	if (PIETravel) PIETravel->SetWithCurrentPriority(Previous);
	return bStarted;
}

// 테스트 맵 전환, 세션 연결 유지
void APFGameMode::ChangeTestMap()
{
	if (UPFCampaignSubsystem::IsCampaign(GetWorld())) return;
	UWorld* World = GetWorld();
	if (World->IsInSeamlessTravel() || !World->NextURL.IsEmpty())
	{
		return;
	}

	const FString CurrentMap = UGameplayStatics::GetCurrentLevelName(World, true);
	FString Destination;
	TArray<TSoftObjectPtr<UWorld>> Maps = { TutorialMap, TestMap3 };
	Maps.Append(AdditionalTestMaps);
	for (int32 Index = 0; Index < Maps.Num(); ++Index)
	{
		if (CurrentMap == Maps[Index].GetAssetName())
		{
			Destination = Maps[(Index + 1) % Maps.Num()].ToSoftObjectPath().GetLongPackageName();
			break;
		}
	}
	if (Destination.IsEmpty() || !FPackageName::DoesPackageExist(Destination))
	{
		return;
	}

	// 모든 플레이어의 입장 완료 후 카메라 상태 보관
	for (auto It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APFPlayerController* PlayerController = Cast<APFPlayerController>(It->Get());
		if (!PlayerController || !PlayerController->bInitialSpawnComplete
			|| !PlayerController->HasClientLoadedCurrentWorld())
		{
			return;
		}
		PlayerController->CaptureCharacterState();
	}

	const FString ModeOption = APFSessionGameState::IsVersus(World) ? TEXT("?PFMode=Versus") : FString();
	const bool bTravelStarted = ServerTravel(GetWorld(), Destination + ModeOption + TEXT("?SeamlessTravel"));
	if (!bTravelStarted)
	{
		PFLOG(Warning, TEXT("Test map travel failed: %s"), *Destination);
	}
}

// 최초 생성 대상 확인
bool APFGameMode::UsesInitialSpawnFlow(AController* Controller) const
{
	const UWorld* World = GetWorld();
	return IsValid(Cast<APFPlayerController>(Controller)) && World
		&& !FPackageName::GetShortName(World->GetMapName()).Contains(TEXT("Title"));
}

// 최초 선택 캐릭터 접수
void APFGameMode::RequestInitialSpawn(APFPlayerController* NewPlayer, ECHARACTER SelectedCharacter)
{
	if (!UsesInitialSpawnFlow(NewPlayer)
		|| NewPlayer->bInitialSpawnComplete || NewPlayer->bInitialSpawnInProgress)
	{
		return;
	}
	const APFPlayerState* State = NewPlayer->GetPlayerState<APFPlayerState>();
	if (State && State->HasLobbySelection()) SelectedCharacter = State->GetCharacter();
	if (SelectedCharacter != CHARACTER_TWINBLAST && SelectedCharacter != CHARACTER_KWANG)
	{
		return;
	}

	if (!NewPlayer->bInitialSpawnRequested)
	{
		NewPlayer->InitialSpawnCharacter = SelectedCharacter;
		NewPlayer->bInitialSpawnRequested = true;
	}
	TryStartInitialPlayer(NewPlayer);
}

// 선택 캐릭터 최초 생성
void APFGameMode::TryStartInitialPlayer(APFPlayerController* NewPlayer)
{
	if (!IsValid(NewPlayer)
		|| NewPlayer->bInitialSpawnComplete || NewPlayer->bInitialSpawnInProgress)
	{
		return;
	}
	if (IsValid(NewPlayer->GetPawn()))
	{
		NewPlayer->bInitialSpawnComplete = true;
		return;
	}
	if (!NewPlayer->bInitialSpawnReady || !NewPlayer->bInitialSpawnRequested || !NewPlayer->bMapPresentationReady)
	{
		return;
	}

	APFPlayerState* PlayerState = NewPlayer->GetPlayerState<APFPlayerState>();
	if (!IsValid(PlayerState) || bStartPlayersAsSpectators
		|| MustSpectate(NewPlayer) || !PlayerCanRestart(NewPlayer))
	{
		return;
	}

	NewPlayer->bInitialSpawnInProgress = true;
	if (PlayerState->HasLobbySelection()) NewPlayer->InitialSpawnCharacter = PlayerState->GetCharacter();
	PlayerState->SetCharacter(NewPlayer->InitialSpawnCharacter);
	PlayerState->EnsureInitialInventoryItems();
	if (UPFCampaignSubsystem::IsCampaign(GetWorld()))
	{
		APFCampaignDirector* Director = APFCampaignDirector::Find(GetWorld());
		if (!Director || Director->DeferJoiningPlayer(NewPlayer))
		{
			NewPlayer->bInitialSpawnInProgress = false;
			return;
		}
		PlayerState->PrepareCampaignCheckpoint();
		if (UPFCampaignSubsystem* Run = UPFCampaignSubsystem::Get(this); Run && Run->Players.Contains(Run->PlayerKey(PlayerState)))
		{
			if (!Run->RestorePlayer(PlayerState))
			{
				NewPlayer->bInitialSpawnInProgress = false;
				return;
			}
			NewPlayer->SetCampaignViewState(Run->Players.FindChecked(Run->PlayerKey(PlayerState)).View);
		}
	}

	// 맵별 시작 위치에 생성하고 실패하면 기본 시작 위치 사용
	FTransform SpawnTransform;
	if (TryFindInitialPlayerSpawnTransform(NewPlayer, SpawnTransform))
	{
		RestartPlayerAtTransform(NewPlayer, SpawnTransform);
	}
	if (!IsValid(NewPlayer->GetPawn()) && !UPFCampaignSubsystem::IsCampaign(GetWorld()))
	{
		Super::RestartPlayer(NewPlayer);
	}

	NewPlayer->bInitialSpawnComplete = IsValid(NewPlayer->GetPawn());
	NewPlayer->bInitialSpawnInProgress = false;
	if (NewPlayer->bInitialSpawnComplete)
	{
		NewPlayer->RestoreCharacterState();
	}
	if (!NewPlayer->bInitialSpawnComplete)
	{
		PFLOG(Warning, TEXT("Initial player spawn failed: %s"), *GetNameSafe(NewPlayer));
	}
}

// 플레이어 초기 생성 위치 탐색
bool APFGameMode::TryFindInitialPlayerSpawnTransform(APlayerController* NewPlayer, FTransform& OutSpawnTransform)
{
	if (UPFCampaignSubsystem::IsCampaign(GetWorld()))
	{
		APFCampaignDirector* Director = APFCampaignDirector::Find(GetWorld());
		return Director && Director->FindPlayerStart(Cast<APFPlayerController>(NewPlayer), OutSpawnTransform);
	}
	UWorld* World = GetWorld();
	if (!NewPlayer)
	{
		return false;
	}

	UClass* PlayerPawnClass = GetDefaultPawnClassForController(NewPlayer);
	const APFCharacter* PlayerDefaultObject = PlayerPawnClass ? Cast<APFCharacter>(PlayerPawnClass->GetDefaultObject()) : nullptr;
	const UCapsuleComponent* PlayerCapsule = PlayerDefaultObject ? PlayerDefaultObject->GetCapsuleComponent() : nullptr;
	if (!PlayerCapsule)
	{
		return false;
	}

	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(InitialPlayerSpawn), false, this);
	if (APFSessionGameState::IsTraining(World))
	{
		// 상자 사이 바닥의 고정 시작점
		const FVector GroundLocation(1544.f, 975.f, 5.f);
		OutSpawnTransform = FTransform(FRotator(0.f, -90.f, 0.f),
			GroundLocation + FVector(0.f, 0.f, PlayerCapsule->GetScaledCapsuleHalfHeight() + 2.f));
		return true;
	}
	int32 RemainingAttempts = 30;
	return FindSpawnTransform(World, PlayerCapsule, QueryParams, RemainingAttempts, true, OutSpawnTransform);
}

// 지면, 충돌을 고려한 생성 위치 탐색
bool APFGameMode::FindSpawnTransform(UWorld* World, const UCapsuleComponent* Capsule,
	const FCollisionQueryParams& QueryParams, int32& RemainingAttempts,
	bool bCheckBlockingCollision, FTransform& OutSpawnTransform,
	const FVector* SpawnCenter, float SpawnRadius)
{
	if (RemainingAttempts <= 0
		|| (SpawnCenter && SpawnRadius <= 0.f))
	{
		return false;
	}

	constexpr float BoundsEdgePadding = 10.f;
	constexpr float VerticalTracePadding = 1000.f;
	const float CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float CapsuleRadius = Capsule->GetScaledCapsuleRadius();

	// 생성 영역 액터 탐색
	AStaticMeshActor* SpawnAreaActor = nullptr;
	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		if (It->ActorHasTag(TEXT("PFRequireNavigableSpawn"))
			|| It->GetActorNameOrLabel().Equals(TEXT("SM_Cube"), ESearchCase::CaseSensitive))
		{
			SpawnAreaActor = *It;
			break;
		}
	}
	if (!SpawnAreaActor)
	{
		return false;
	}

	// 캡슐 크기를 고려한 생성 범위 계산
	const FBox SpawnAreaBounds = SpawnAreaActor->GetComponentsBoundingBox(true);
	float MinX = SpawnAreaBounds.Min.X + CapsuleRadius + BoundsEdgePadding;
	float MaxX = SpawnAreaBounds.Max.X - CapsuleRadius - BoundsEdgePadding;
	float MinY = SpawnAreaBounds.Min.Y + CapsuleRadius + BoundsEdgePadding;
	float MaxY = SpawnAreaBounds.Max.Y - CapsuleRadius - BoundsEdgePadding;
	if (SpawnCenter)
	{
		MinX = FMath::Max(MinX, static_cast<float>(SpawnCenter->X - SpawnRadius));
		MaxX = FMath::Min(MaxX, static_cast<float>(SpawnCenter->X + SpawnRadius));
		MinY = FMath::Max(MinY, static_cast<float>(SpawnCenter->Y - SpawnRadius));
		MaxY = FMath::Min(MaxY, static_cast<float>(SpawnCenter->Y + SpawnRadius));
	}
	if (!SpawnAreaBounds.IsValid || MinX >= MaxX || MinY >= MaxY)
	{
		return false;
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	static const ECollisionChannel WallCollisionChannel = GetCollisionChannel(PFCollisionChannelNames::Wall);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQueryParams.AddObjectTypesToQuery(WallCollisionChannel);

	// 무작위 위치의 지면 탐색
	while (RemainingAttempts > 0)
	{
		--RemainingAttempts;
		const float RandomX = FMath::FRandRange(MinX, MaxX);
		const float RandomY = FMath::FRandRange(MinY, MaxY);
		if (SpawnCenter && FVector::DistSquared2D(FVector(RandomX, RandomY, SpawnCenter->Z), *SpawnCenter)
			> FMath::Square(SpawnRadius))
		{
			continue;
		}
		const FVector TraceStart(RandomX, RandomY, SpawnAreaBounds.Max.Z + VerticalTracePadding);
		const FVector TraceEnd(RandomX, RandomY, SpawnAreaBounds.Min.Z - VerticalTracePadding);

		FHitResult GroundHit;
		if (!World->LineTraceSingleByObjectType(GroundHit, TraceStart, TraceEnd, ObjectQueryParams, QueryParams)
			|| GroundHit.ImpactNormal.Z < 0.5f)
		{
			continue;
		}

		const FVector SpawnLocation = GroundHit.ImpactPoint + FVector(0.f, 0.f, CapsuleHalfHeight + 2.f);
		if (SpawnAreaActor->ActorHasTag(TEXT("PFRequireNavigableSpawn")))
		{
			const UPrimitiveComponent* GroundComponent = GroundHit.GetComponent();
			if (!GroundComponent || !GroundComponent->ComponentHasTag(TEXT("PFMapSpawnSurface")))
			{
				continue;
			}
			UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
			FNavLocation NavLocation;
			if (!Navigation || !Navigation->ProjectPointToNavigation(GroundHit.ImpactPoint, NavLocation,
				FVector(CapsuleRadius, CapsuleRadius, 100.f))
				|| FVector::DistSquared2D(NavLocation.Location, GroundHit.ImpactPoint) > FMath::Square(CapsuleRadius)
				|| FMath::Abs(NavLocation.Location.Z - GroundHit.ImpactPoint.Z) > 100.f)
			{
				continue;
			}
		}
		// 생성 위치의 캡슐 충돌 확인
		if (bCheckBlockingCollision)
		{
			const FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight);
			const FCollisionResponseParams ResponseParams(Capsule->GetCollisionResponseToChannels());
			if (World->OverlapBlockingTestByChannel(SpawnLocation, FQuat::Identity, Capsule->GetCollisionObjectType(),
				CapsuleShape, QueryParams, ResponseParams))
			{
				continue;
			}
		}

		OutSpawnTransform = FTransform(FRotator::ZeroRotator, SpawnLocation);
		return true;
	}

	return false;
}

// 사망 플레이어 부활
void APFGameMode::RespawnPlayer(TWeakObjectPtr<APlayerController> PlayerController)
{
	if (UPFCampaignSubsystem::IsCampaign(GetWorld())) return;
	APlayerController* Controller = PlayerController.Get();
	if (!Controller)
	{
		return;
	}
	APFCharacter* DeadPlayer = Cast<APFCharacter>(Controller->GetPawn());
	APFPlayerState* PlayerState = Controller->GetPlayerState<APFPlayerState>();
	UAbilitySystemComponent* PlayerASC = PlayerState ? PlayerState->GetAbilitySystemComponent() : nullptr;
	UPFAttributeSet* PlayerAttributes = PlayerState ? PlayerState->GetAttributeSet() : nullptr;
	if (!DeadPlayer || !DeadPlayer->IsDeadCharacter() || !PlayerASC || !PlayerAttributes)
	{
		return;
	}

	FTransform SpawnTransform;
	const bool bHasSpawnTransform = TryFindInitialPlayerSpawnTransform(Controller, SpawnTransform);
	if (!DeadPlayer->Destroy())
	{
		return;
	}

	// 사망, 질주 해제와 체력, 마나 회복
	PlayerASC->SetLooseGameplayTagCount(PFGameplayTags::Character_State_Dead, 0, EGameplayTagReplicationState::TagOnly);
	PlayerASC->SetLooseGameplayTagCount(PFGameplayTags::Character_State_Sprinting, 0, EGameplayTagReplicationState::TagOnly);

	FPFGE_StatGameplayEffects::ApplyHeal(PlayerASC, PlayerAttributes->GetMaxHealth() - PlayerAttributes->GetHealth());
	FPFGE_StatGameplayEffects::ApplyManaRestore(PlayerASC, PlayerAttributes->GetMaxMana() - PlayerAttributes->GetMana());

	// 새 플레이어 생성
	if (bHasSpawnTransform)
	{
		RestartPlayerAtTransform(Controller, SpawnTransform);
	}
	if (!Controller->GetPawn())
	{
		RestartPlayer(Controller);
	}

	PlayerState->ForceNetUpdate();
}

// 선택 캐릭터 교체, 상태 복원
void APFGameMode::ChangeCharacter(APFPlayerController* Controller)
{
	if (!HasAuthority() || !IsValid(Controller) || !IsValid(Controller->GetPawn())) return;
	APFPlayerState* PS = Controller->GetPlayerState<APFPlayerState>();
	if (!PS) return;
	if (APFSessionGameState::IsStory(GetWorld()))
	{
		int32 Players = 0;
		int32 SameCharacterPlayers = 0;
		for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			const APFPlayerController* Player = Cast<APFPlayerController>(It->Get());
			const APFPlayerState* State = Player ? Player->GetPlayerState<APFPlayerState>() : nullptr;
			if (!State) continue;
			++Players;
			if (State->GetCharacter() == PS->GetCharacter()) ++SameCharacterPlayers;
		}
		if (Players >= 2 && SameCharacterPlayers <= 1) return;
	}
	// 이전 위치, 제어 상태 보관
	APawn* OldPawn = Controller->GetPawn();
	const FTransform RestartTransform = OldPawn->GetActorTransform();
	Controller->CaptureCharacterState();
	const FRotator SavedControlRotation = Controller->GetControlRotation();

	// 다음 캐릭터 선택
	ECHARACTER CharacterType = PS->GetCharacter();

	switch (CharacterType)
	{
	case CHARACTER_TWINBLAST:
		PS->SetCharacter(CHARACTER_KWANG);
		break;
	case CHARACTER_KWANG:
		PS->SetCharacter(CHARACTER_TWINBLAST);
		break;
	default:
		break;
	}

	OldPawn->Destroy();

	// 선택 캐릭터 생성, 상태 복원
	RestartPlayerAtTransform(Controller, RestartTransform);
	Controller->RestoreCharacterState();

	// 서버, 클라이언트 시선 복원
	Controller->SetControlRotation(SavedControlRotation);
	Controller->ClientSetRotation(SavedControlRotation, false);
}

UClass* APFGameMode::GetDefaultPawnClassForController_Implementation(AController* Controller)
{
	const APFPlayerState* State = IsValid(Controller) ? Controller->GetPlayerState<APFPlayerState>() : nullptr;
	const FPFCharacterDefinition* Definition = State ? UPFGameInstanceSubsystem::GetCharacterDefinition(State->GetCharacter()) : nullptr;
	UClass* PawnClass = Definition ? LoadClass<APawn>(nullptr, Definition->PawnPath) : nullptr;
	return PawnClass ? PawnClass : Super::GetDefaultPawnClassForController_Implementation(Controller);
}

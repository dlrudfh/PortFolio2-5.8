#include "System/Framework/PFGameInstance.h"
#include "Campaign/PFCampaignSubsystem.h"

#include "Props/PFItem.h"
#include "UI/Menu/PFTitle.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "GameDelegates.h"
#include "AudioDevice.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Guid.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "OnlineSubsystemNames.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	const FName HostHeartbeatKey(TEXT("PFHostHeartbeat"));
	const FName SessionPhaseKey(TEXT("PFSessionPhase"));
	constexpr double HostHeartbeatInterval = 5.0;
	constexpr double HostHeartbeatTimeout = 20.0;
	constexpr double HostValidationTimeout = 60.0;
}

UPFGameInstance::UPFGameInstance() : CreateSessionCompleteDelegate(FOnCreateSessionCompleteDelegate::CreateUObject(this, &UPFGameInstance::OnCreateSessionComplete)),
FindSessionsCompleteDelegate(FOnFindSessionsCompleteDelegate::CreateUObject(this, &UPFGameInstance::OnFindSessionsComplete)),
JoinSessionCompleteDelegate(FOnJoinSessionCompleteDelegate::CreateUObject(this, &UPFGameInstance::OnJoinSessionComplete)), CharacterType(CHARACTER_TWINBLAST)
{
	FString CharacterDataPath = TEXT("/Game/GameData/PFCharacterData.PFCharacterData");
	static ConstructorHelpers::FObjectFinder<UDataTable> DT_PFCHARACTER(*CharacterDataPath);
	PFCHECK(DT_PFCHARACTER.Succeeded());
	PFCharacterTable = DT_PFCHARACTER.Object;
	PFCHECK(PFCharacterTable->GetRowMap().Num() > 0);
}

void UPFGameInstance::Init()
{
	Super::Init();
	if (GConfig)
	{
		GConfig->GetFloat(TEXT("PortFolio.MenuSettings"), TEXT("Volume"), MenuVolume, GGameUserSettingsIni);
		GConfig->GetFloat(TEXT("PortFolio.MenuSettings"), TEXT("CameraSensitivity"), CameraSensitivity, GGameUserSettingsIni);
		MenuVolume = FMath::IsFinite(MenuVolume) ? FMath::Clamp(MenuVolume, 0.f, 100.f) : 100.f;
		CameraSensitivity = FMath::IsFinite(CameraSensitivity) ? FMath::Clamp(CameraSensitivity, 1.f, 100.f) : 50.f;
	}
	DisconnectHandle = FGameDelegates::Get().GetHandleDisconnectDelegate()
		.AddUObject(this, &UPFGameInstance::HandleSessionDisconnect);
	if (GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UPFGameInstance::HandleNetworkFailure);
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &UPFGameInstance::HandleTravelFailure);
	}
	MapLoadedHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UPFGameInstance::HandleMapLoaded);
	SessionRecoveryTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UPFGameInstance::TickSessionRecovery), 1.f);

    // 온라인 세션 인터페이스 연결
    IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
    if (OnlineSubsystem)
    {
        OnlineSessionInterface = OnlineSubsystem->GetSessionInterface();
        bSteamSessions = OnlineSubsystem->GetSubsystemName() == STEAM_SUBSYSTEM;
		if (OnlineSessionInterface.IsValid())
		{
			HostHeartbeatCompleteHandle = OnlineSessionInterface->AddOnUpdateSessionCompleteDelegate_Handle(
				FOnUpdateSessionCompleteDelegate::CreateUObject(this, &UPFGameInstance::OnHostHeartbeatComplete));
		}
		if (bSteamSessions)
		{
			if (UClass* DriverClass = LoadClass<UNetDriver>(nullptr, TEXT("/Script/SteamSockets.SteamSocketsNetDriver")))
			{
				UNetDriver* DriverDefaults = DriverClass->GetDefaultObject<UNetDriver>();
				DriverDefaults->ConnectionTimeout = 20.f;
				DriverDefaults->InitialConnectTimeout = 60.f;
			}
		}

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Yellow,
                FString::Printf(TEXT("Find Subsystem : %s"),
                    *OnlineSubsystem->GetSubsystemName().ToString()));
        }
    }

	RecoverPreviousLobby();
	if (OnlineSessionInterface.IsValid() && OnlineSessionInterface->GetNamedSession(NAME_GameSession))
	{
		BeginSessionCleanup();
	}
}

void UPFGameInstance::Shutdown()
{
	SaveMenuSettings();
	bShuttingDown = true;
	PendingSessionRequest = ESessionRequest::None;
	ClearSessionDelegates();
	FGameDelegates::Get().GetHandleDisconnectDelegate().Remove(DisconnectHandle);
	DisconnectHandle.Reset();
	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	}
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(MapLoadedHandle);
	FTSTicker::RemoveTicker(SessionRecoveryTickerHandle);
	SessionRecoveryTickerHandle.Reset();

	if (OnlineSessionInterface.IsValid())
	{
		OnlineSessionInterface->ClearOnUpdateSessionCompleteDelegate_Handle(HostHeartbeatCompleteHandle);
		HostHeartbeatCompleteHandle.Reset();
		// 진행 중인 세션 작업과 종료 요청의 충돌 방지
		if (!bHostHeartbeatPending && OnlineSessionInterface->GetNamedSession(NAME_GameSession)
			&& SessionOperation != ESessionOperation::Creating
			&& SessionOperation != ESessionOperation::Joining
			&& SessionOperation != ESessionOperation::Destroying)
		{
			OnlineSessionInterface->DestroySession(NAME_GameSession);
		}
	}
	FTSTicker::RemoveTicker(ExitTickerHandle);
	ExitTickerHandle.Reset();
	SessionSearch.Reset();
	OnlineSessionInterface.Reset();
	Super::Shutdown();
}

// 전체 음량 갱신
void UPFGameInstance::SetMenuVolume(float Value)
{
	const float NewVolume = FMath::RoundToFloat(FMath::Clamp(Value, 0.f, 100.f));
	bMenuSettingsDirty |= MenuVolume != NewVolume;
	MenuVolume = NewVolume;
	ApplyMenuVolume();
}

// 카메라 감도 갱신
void UPFGameInstance::SetCameraSensitivity(float Value)
{
	const float NewSensitivity = FMath::RoundToFloat(FMath::Clamp(Value, 1.f, 100.f));
	bMenuSettingsDirty |= CameraSensitivity != NewSensitivity;
	CameraSensitivity = NewSensitivity;
}

// 현재 월드의 전체 음량 적용
void UPFGameInstance::ApplyMenuVolume()
{
	if (UWorld* World = GetWorld())
	{
		if (FAudioDeviceHandle AudioDevice = World->GetAudioDevice())
		{
			AudioDevice->SetTransientPrimaryVolume(MenuVolume / 100.f);
		}
	}
}

// 로컬 설정 파일 저장
void UPFGameInstance::SaveMenuSettings()
{
	if (!GConfig || !bMenuSettingsDirty) return;
	GConfig->SetFloat(TEXT("PortFolio.MenuSettings"), TEXT("Volume"), MenuVolume, GGameUserSettingsIni);
	GConfig->SetFloat(TEXT("PortFolio.MenuSettings"), TEXT("CameraSensitivity"), CameraSensitivity, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
	bMenuSettingsDirty = false;
}

void UPFGameInstance::ReturnToMainMenu()
{
	if (bShuttingDown || bExitRequested || bReturningToTitle || !GetWorld())
	{
		return;
	}
	bReturningToTitle = true;
	PendingSessionRequest = ESessionRequest::None;
	bRestoreSessionInput = false;
	BeginSessionCleanup();
	Super::ReturnToMainMenu();
}

// 타이틀 위젯 해제
void UPFGameInstance::RemoveTitle()
{
	if (IsValid(TitleWidget)) TitleWidget->RemoveFromParent();
	TitleWidget = nullptr;
}

// 타이틀 UI 생성, 입력 설정
void UPFGameInstance::CreateTitle()
{
	UWorld* World = GetWorld();
	if (!World) return;
	
	FString MapName = World->GetMapName();
	if (!MapName.Contains(TEXT("Title")))
	{
		return;
	}
	bReturningToTitle = false;
	if (IsValid(TitleWidget))
	{
		if (TitleWidget->GetWorld() == World && TitleWidget->IsInViewport())
		{
			TitleWidget->ShowCharacterSelection();
			UpdateSessionUI();
			return;
		}
		RemoveTitle();
	}
	
	// 타이틀 위젯 생성
	TitleWidgetClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/GameData/UI/Title.Title_C"));

	if (TitleWidgetClass)
	{
		APlayerController* PC = GetFirstLocalPlayerController(World);
		if (PC)
		{
			TitleWidget = CreateWidget<UPFTitle>(PC, TitleWidgetClass);
			if (TitleWidget)
			{
				TitleWidget->AddToViewport(etoi(TITLE));
				TitleWidget->ShowCharacterSelection();
				UpdateSessionUI();

				// UI, 캐릭터 선택 입력 설정
				PC->bShowMouseCursor = true;
				PC->bEnableClickEvents = true;  
				PC->bEnableMouseOverEvents = true;

				FInputModeGameAndUI InputMode;
				InputMode.SetWidgetToFocus(TitleWidget->TakeWidget());
				InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
				PC->SetInputMode(InputMode);
			}
		}
	}
}

// 캐릭터 데이터 조회
FPFCharacterData* UPFGameInstance::GetPFCharacterData(int32 CharacterID)
{
	return PFCharacterTable->FindRow<FPFCharacterData>(*FString::FromInt(CharacterID), TEXT(""));
}

// 온라인 세션 생성 요청
void UPFGameInstance::CreateGameSession(const FString& SessionName, EPFSessionMode Mode, int32 Capacity)
{
	if ((Mode != EPFSessionMode::Story && Mode != EPFSessionMode::Versus)
		|| Capacity < 1 || Capacity > APFSessionGameState::GetMaxPlayers(Mode)
		|| SessionOperation != ESessionOperation::Idle || bCleanupRequested || bReturningToTitle)
	{
		return;
	}
	PendingSessionMode = Mode;
	PendingSessionCapacity = Capacity;
	RequestSession(ESessionRequest::Create, SessionName);
}

// 온라인 닉네임, 로컬 플레이어 이름 조회
FString UPFGameInstance::GetOnlineUsername() const
{
	if (IOnlineSubsystem* SteamSubsystem = IOnlineSubsystem::Get(STEAM_SUBSYSTEM))
	{
		const IOnlineIdentityPtr Identity = SteamSubsystem->GetIdentityInterface();
		if (Identity.IsValid() && Identity->GetLoginStatus(0) == ELoginStatus::LoggedIn)
		{
			const FString Nickname = Identity->GetPlayerNickname(0).TrimStartAndEnd();
			if (!Nickname.IsEmpty()) return Nickname.Left(128);
		}
	}
	const APlayerController* PlayerController = GetFirstLocalPlayerController();
	if (PlayerController && PlayerController->PlayerState)
	{
		const FString PlayerName = PlayerController->PlayerState->GetPlayerName().TrimStartAndEnd();
		if (!PlayerName.IsEmpty()) return PlayerName.Left(128);
	}
	return TEXT("Offline Player");
}

// 세션 없이 튜토리얼 진입 요청
void UPFGameInstance::StartTutorial()
{
	if (!GetWorld() || !GetWorld()->GetMapName().Contains(TEXT("Title"))) return;
	RequestSession(ESessionRequest::Tutorial, FString());
}

// 기존 세션 정리 후 요청 예약
bool UPFGameInstance::RequestSession(ESessionRequest Request, const FString& SessionName)
{
	const bool bTutorial = Request == ESessionRequest::Tutorial;
	if (!bTutorial) RecoverPreviousLobby();
	if ((!bTutorial && !bStartupLobbyRecovered) || bCleanupTimedOut)
	{
		SessionNotice = NSLOCTEXT("PFSession", "RecoveryPending", "Steam session recovery is still in progress. Try again shortly, or restart the game.");
		bRestoreSessionInput = true;
		UpdateSessionUI();
		return false;
	}
	if (bShuttingDown || bExitRequested || bCleanupRequested || bReturningToTitle
		|| SessionOperation != ESessionOperation::Idle)
	{
		return false;
	}

	const ULocalPlayer* LocalPlayer = GetFirstGamePlayer();
	if (!LocalPlayer || (!bTutorial && (!OnlineSessionInterface.IsValid() || !LocalPlayer->GetPreferredUniqueNetId().IsValid())))
	{
		PFLOG(Warning, TEXT("Session interface or local player is not ready"));
		SessionNotice = NSLOCTEXT("PFSession", "NotReady", "Online services are not ready. Check your Steam connection and try again.");
		bRestoreSessionInput = true;
		UpdateSessionUI();
		return false;
	}

	SessionNotice = FText::GetEmpty();
	RejectedLobbyIds.Empty();
	InputSessionName = SessionName;
	PendingSessionRequest = Request;
	bRestoreSessionInput = false;
	if (OnlineSessionInterface.IsValid() && OnlineSessionInterface->GetNamedSession(NAME_GameSession))
	{
		BeginSessionCleanup();
		return true;
	}

	return StartPendingSessionRequest();
}

// 예약된 세션 요청, 튜토리얼 이동 시작
bool UPFGameInstance::StartPendingSessionRequest()
{
	if (bShuttingDown || bExitRequested || bCleanupRequested || bReturningToTitle
		|| SessionOperation != ESessionOperation::Idle || PendingSessionRequest == ESessionRequest::None)
	{
		return false;
	}

	const bool bTutorial = PendingSessionRequest == ESessionRequest::Tutorial;
	const ULocalPlayer* LocalPlayer = GetFirstGamePlayer();
	if (!LocalPlayer || (!bTutorial && (!OnlineSessionInterface.IsValid() || !LocalPlayer->GetPreferredUniqueNetId().IsValid())))
	{
		FailSessionRequest();
		return false;
	}

	const ESessionRequest Request = PendingSessionRequest;
	PendingSessionRequest = ESessionRequest::None;
	ClearSessionDelegates();
	SessionSearch.Reset();
	Address.Empty();
	bLobbyReady = false;
	bSessionMatchStarted = false;
	bPublishingMatchStart = false;
	PendingGameplayTravelURL.Empty();

	if (bTutorial)
	{
		PendingSessionMode = EPFSessionMode::Training;
		PendingSessionCapacity = 1;
		SessionOperation = ESessionOperation::Traveling;
		SessionOperationStartedAt = FPlatformTime::Seconds();
		UpdateSessionUI();
		UGameplayStatics::OpenLevel(this, TEXT("/Game/ThirdPerson/Maps/Tutorial"), true, TEXT("PFMode=Training"));
		RemoveTitle();
		return true;
	}

	if (Request == ESessionRequest::Create)
	{
		// 공개 세션, 로비 설정
		FOnlineSessionSettings SessionSettings;
		SessionSettings.bIsLANMatch = false;
		SessionSettings.NumPublicConnections = PendingSessionCapacity;
		SessionSettings.bAllowJoinInProgress = true;
		SessionSettings.bAllowJoinViaPresence = true;
		SessionSettings.bShouldAdvertise = true;
		SessionSettings.bUsesPresence = true;
		SessionSettings.bUseLobbiesIfAvailable = true;
		SessionSettings.Set(FName("SessionName"), InputSessionName, EOnlineDataAdvertisementType::ViaOnlineService);
		SessionSettings.Set(FName("MapName"), FString(TEXT("/Game/ThirdPerson/Maps/Title")), EOnlineDataAdvertisementType::ViaOnlineService);
		SessionSettings.Set(FName("PFMode"), FString(PendingSessionMode == EPFSessionMode::Story ? TEXT("Campaign") : TEXT("Versus")), EOnlineDataAdvertisementType::ViaOnlineService);
		SessionSettings.Set(SessionPhaseKey, FString(TEXT("Creating")), EOnlineDataAdvertisementType::ViaOnlineService);
		SessionSettings.Set(HostHeartbeatKey, FGuid::NewGuid().ToString(), EOnlineDataAdvertisementType::ViaOnlineService);

		SessionOperation = ESessionOperation::Creating;
		SessionOperationStartedAt = FPlatformTime::Seconds();
		CreateSessionCompleteHandle = OnlineSessionInterface->AddOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegate);
		UpdateSessionUI();
		const bool bStarted = OnlineSessionInterface->CreateSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, SessionSettings);
		if (!bStarted && SessionOperation == ESessionOperation::Creating)
		{
			OnCreateSessionComplete(NAME_GameSession, false);
		}
		return bStarted;
	}

	SessionSearch = MakeShared<FOnlineSessionSearch>();
	SessionSearch->MaxSearchResults = 10000;
	SessionSearch->bIsLanQuery = false;
	SessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	SessionOperation = ESessionOperation::Finding;
	SessionOperationStartedAt = FPlatformTime::Seconds();
	FindSessionsCompleteHandle = OnlineSessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegate);
	UpdateSessionUI();
	const bool bStarted = OnlineSessionInterface->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(), SessionSearch.ToSharedRef());
	if (!bStarted && SessionOperation == ESessionOperation::Finding)
	{
		OnFindSessionsComplete(false);
	}
	return bStarted;
}

// 세션 생성 후 네트워크 로비 이동
void UPFGameInstance::OnCreateSessionComplete(FName SessionName, bool IsSucceeded)
{
	if (bShuttingDown || bExitDispatched || SessionName != NAME_GameSession
		|| SessionOperation != ESessionOperation::Creating)
	{
		return;
	}

	ClearSessionDelegates();
	SessionOperation = ESessionOperation::Idle;
	if (bCleanupRequested)
	{
		BeginSessionCleanup();
		return;
	}

	UWorld* World = GetWorld();
	if (!IsSucceeded || !World)
	{
		PFLOG(Warning, TEXT("Create session failed: %s"), *SessionName.ToString());
		FailSessionRequest();
		return;
	}

	if (const FNamedOnlineSession* Session = OnlineSessionInterface->GetNamedSession(NAME_GameSession))
	{
		SaveRecoveryLobby(Session->GetSessionIdStr());
	}
	SessionOperation = ESessionOperation::Traveling;
	SessionOperationStartedAt = FPlatformTime::Seconds();
	UpdateSessionUI();
	UGameplayStatics::OpenLevel(this, TEXT("/Game/ThirdPerson/Maps/Title"), true,
		TEXT("listen?game=/Script/PortFolio.PFLobbyGameMode?PFMode=Lobby"));

	if (SessionOperation == ESessionOperation::Traveling)
	{
		RemoveTitle();
	}
}

// 참가할 세션 검색 요청
bool UPFGameInstance::JoinGameSession(const FString& SessionName)
{
	return RequestSession(ESessionRequest::Join, SessionName);
}

// 참가 세션으로 이동
void UPFGameInstance::StartGame()
{
	if (bShuttingDown || bExitRequested || bCleanupRequested || bReturningToTitle
		|| SessionOperation != ESessionOperation::Idle)
	{
		return;
	}

	APlayerController* PlayerController = GetFirstLocalPlayerController();
	if (!PlayerController || Address.IsEmpty() || !OnlineSessionInterface.IsValid()
		|| !OnlineSessionInterface->GetNamedSession(NAME_GameSession))
	{
		PFLOG(Warning, TEXT("Join address or session is not ready"));
		FailSessionRequest();
		return;
	}

	const FNamedOnlineSession* Session = OnlineSessionInterface->GetNamedSession(NAME_GameSession);
	FString Phase;
	if (!Session->SessionSettings.Get(SessionPhaseKey, Phase) || Phase != TEXT("LobbyReady"))
	{
		RejectedLobbyIds.Add(JoiningLobbyId);
		PendingSessionRequest = ESessionRequest::Join;
		BeginSessionCleanup();
		return;
	}
	if (!IsSessionHostAvailable(*Session))
	{
		SessionNotice = NSLOCTEXT("PFSession", "HostLeft", "The host has disconnected. Please create or join another session.");
		ReturnToMainMenu();
		return;
	}

	SessionOperation = ESessionOperation::Traveling;
	SessionOperationStartedAt = FPlatformTime::Seconds();
	UpdateSessionUI();
	PlayerController->ClientTravel(Address, TRAVEL_Absolute);
	if (SessionOperation == ESessionOperation::Traveling)
	{
		RemoveTitle();
	}
}

// 로비 시작 확정, 신규 참가 차단
bool UPFGameInstance::BeginLobbyMatch(EPFSessionMode Mode, FName Map)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client || !bLobbyReady || bSessionMatchStarted
		|| bShuttingDown || bExitRequested || bCleanupRequested || bReturningToTitle
		|| SessionOperation != ESessionOperation::Idle || Mode != PendingSessionMode)
	{
		return false;
	}
	if (Mode == EPFSessionMode::Story)
	{
		Map = TEXT("Map3");
	}
	else if (Mode != EPFSessionMode::Versus)
	{
		return false;
	}
	const FName AllowedMaps[] = { TEXT("Map3"), TEXT("Map4"), TEXT("Map5"), TEXT("Map6"), TEXT("Map7"), TEXT("Map8"), TEXT("Tutorial") };
	bool bAllowedMap = false;
	for (const FName AllowedMap : AllowedMaps) bAllowedMap |= Map == AllowedMap;
	if (!bAllowedMap || !OnlineSessionInterface.IsValid()
		|| !OnlineSessionInterface->GetNamedSession(NAME_GameSession)) return false;

	bSessionMatchStarted = true;
	bLobbyReady = false;
	PendingGameplayTravelURL = FString::Printf(TEXT("/Game/ThirdPerson/Maps/%s?listen?game=/Script/PortFolio.PFGameMode?PFMode=%s"),
		*Map.ToString(), Mode == EPFSessionMode::Story ? TEXT("Campaign") : TEXT("Versus"));
	SessionOperation = ESessionOperation::Traveling;
	SessionOperationStartedAt = FPlatformTime::Seconds();
	UpdateSessionUI();
	PublishHostedSession();
	return true;
}

// 참가 차단 게시 후 게임 맵 이동
void UPFGameInstance::TravelToLobbyMatch()
{
	UWorld* World = GetWorld();
	if (!World || PendingGameplayTravelURL.IsEmpty() || bCleanupRequested || bReturningToTitle || bExitRequested) return;
	const APFSessionGameState* Session = APFSessionGameState::Get(World);
	if (!Session || Session->Phase != EPFSessionPhase::Starting || !Session->IsPartyReady())
	{
		SessionNotice = NSLOCTEXT("PFSession", "PartyChanged", "A player left before the game could start. Please create or join a session again.");
		ReturnToMainMenu();
		return;
	}
	if (PendingSessionMode == EPFSessionMode::Story)
	{
		if (UPFCampaignSubsystem* Run = GetSubsystem<UPFCampaignSubsystem>()) Run->ResetRun();
	}
	const FString TravelURL = PendingGameplayTravelURL;
	PendingGameplayTravelURL.Empty();
	SessionOperationStartedAt = FPlatformTime::Seconds();
	if (!World->ServerTravel(TravelURL, true))
	{
		SessionNotice = NSLOCTEXT("PFSession", "TravelFailed", "Could not connect to the game. Please create or join a session again.");
		ReturnToMainMenu();
	}
}

// 이름이 일치하는 세션 참가
void UPFGameInstance::OnFindSessionsComplete(bool IsSucceeded)
{
	if (bShuttingDown || bExitDispatched || SessionOperation != ESessionOperation::Finding)
	{
		return;
	}
	ClearSessionDelegates();
	SessionOperation = ESessionOperation::Idle;
	if (bCleanupRequested)
	{
		BeginSessionCleanup();
		return;
	}

	const ULocalPlayer* LocalPlayer = GetFirstGamePlayer();
	if (!IsSucceeded || !OnlineSessionInterface.IsValid() || !SessionSearch.IsValid()
		|| !LocalPlayer || !LocalPlayer->GetPreferredUniqueNetId().IsValid())
	{
		PFLOG(Warning, TEXT("Find sessions failed"));
		FailSessionRequest();
		return;
	}

	// 검색 결과에서 세션 이름 비교
	for (const FOnlineSessionSearchResult& SearchResult : SessionSearch->SearchResults)
	{
		FString SessionName;
		SearchResult.Session.SessionSettings.Get(FName("SessionName"), SessionName);
		if (InputSessionName != SessionName || !SearchResult.IsValid() || RejectedLobbyIds.Contains(SearchResult.GetSessionIdStr()))
		{
			continue;
		}
		FString Phase;
		if (!SearchResult.Session.SessionSettings.Get(SessionPhaseKey, Phase) || Phase != TEXT("LobbyReady")
			|| SearchResult.Session.NumOpenPublicConnections <= 0)
		{
			continue;
		}

		FString Heartbeat;
		if (bSteamSessions && (!SearchResult.Session.SessionSettings.Get(HostHeartbeatKey, Heartbeat) || Heartbeat.IsEmpty()))
		{
			continue;
		}

		FOnlineSessionSearchResult Result = SearchResult;
		Result.Session.SessionSettings.bUseLobbiesIfAvailable = true;
		Result.Session.SessionSettings.bUsesPresence = true;
		JoiningLobbyId = Result.Session.GetSessionIdStr();
		SaveRecoveryLobby(JoiningLobbyId);
		SessionOperation = ESessionOperation::Joining;
		SessionOperationStartedAt = FPlatformTime::Seconds();
		JoinSessionCompleteHandle = OnlineSessionInterface->AddOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegate);
		UpdateSessionUI();
		const bool bStarted = OnlineSessionInterface->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, Result);
		if (!bStarted && SessionOperation == ESessionOperation::Joining)
		{
			OnJoinSessionComplete(NAME_GameSession, EOnJoinSessionCompleteResult::UnknownError);
		}
		return;
	}

	PFLOG(Warning, TEXT("Session not found: %s"), *InputSessionName);
	FailSessionRequest();
}

// 참가 결과에 따라 타이틀 전환
void UPFGameInstance::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (bShuttingDown || bExitDispatched || SessionName != NAME_GameSession
		|| SessionOperation != ESessionOperation::Joining)
	{
		return;
	}
	ClearSessionDelegates();
	SessionOperation = ESessionOperation::Idle;
	SessionSearch.Reset();
	if (bCleanupRequested)
	{
		BeginSessionCleanup();
		return;
	}

	if (Result == EOnJoinSessionCompleteResult::Success && OnlineSessionInterface.IsValid()
		&& OnlineSessionInterface->GetResolvedConnectString(NAME_GameSession, Address) && !Address.IsEmpty())
	{
		const FNamedOnlineSession* Session = OnlineSessionInterface->GetNamedSession(NAME_GameSession);
		FString Heartbeat;
		if (!Session || (bSteamSessions && (!Session->SessionSettings.Get(HostHeartbeatKey, Heartbeat) || Heartbeat.IsEmpty())))
		{
			RejectedLobbyIds.Add(JoiningLobbyId);
			PendingSessionRequest = ESessionRequest::Join;
			BeginSessionCleanup();
			return;
		}
		SaveRecoveryLobby(Session->GetSessionIdStr());
		if (bSteamSessions)
		{
			LastHostHeartbeat = Heartbeat;
			LastHostHeartbeatReceivedAt = FPlatformTime::Seconds();
			SessionOperationStartedAt = LastHostHeartbeatReceivedAt;
			SessionOperation = ESessionOperation::ValidatingHost;
			UpdateSessionUI();
			return;
		}
		StartGame();
		return;
	}

	PFLOG(Warning, TEXT("Join session failed: %d"), static_cast<int32>(Result));
	RejectedLobbyIds.Add(JoiningLobbyId);
	PendingSessionRequest = ESessionRequest::Join;
	BeginSessionCleanup();
}

// 세션 정리 후 게임 종료
void UPFGameInstance::ExitGame()
{
	if (bShuttingDown || bExitRequested)
	{
		return;
	}
	bExitRequested = true;
	PendingSessionRequest = ESessionRequest::None;
	bRestoreSessionInput = false;
	if (!OnlineSessionInterface.IsValid()
		|| (!OnlineSessionInterface->GetNamedSession(NAME_GameSession)
			&& SessionOperation != ESessionOperation::Creating
			&& SessionOperation != ESessionOperation::Joining
			&& SessionOperation != ESessionOperation::Destroying))
	{
		FinishExitGame();
		return;
	}
	ExitTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UPFGameInstance::HandleExitTimeout), 5.f);
	BeginSessionCleanup();
}

// 연결 종료 시 해당 게임 인스턴스 정리
void UPFGameInstance::HandleSessionDisconnect(UWorld* World, UNetDriver* NetDriver)
{
	if (bShuttingDown || bExitDispatched || !GEngine)
	{
		return;
	}
	if (NetDriver && NetDriver->NetDriverName != NAME_GameNetDriver && NetDriver->NetDriverName != NAME_PendingNetDriver)
	{
		return;
	}

	const FWorldContext* Context = World
		? GEngine->GetWorldContextFromWorld(World)
		: (NetDriver ? GEngine->GetWorldContextFromPendingNetGameNetDriver(NetDriver) : nullptr);
	if (!Context || Context->OwningGameInstance != this || (World && World != GetWorld()))
	{
		return;
	}

	// 엔진이 타이틀 복귀를 결정한 연결만 정리
	PFLOG(Warning, TEXT("Cleaning session after disconnect"));
	if (SessionNotice.IsEmpty())
	{
		SessionNotice = NSLOCTEXT("PFSession", "Disconnected", "The connection has been lost. Please create or join a session again.");
	}
	PendingSessionRequest = ESessionRequest::None;
	bRestoreSessionInput = false;
	bReturningToTitle = true;
	BeginSessionCleanup();
}

// 네트워크 실패 안내 저장
void UPFGameInstance::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& Error)
{
	if (bShuttingDown || bExitRequested || !GEngine || !NetDriver) return;
	if (NetDriver->NetDriverName != NAME_GameNetDriver && NetDriver->NetDriverName != NAME_PendingNetDriver) return;
	const FWorldContext* Context = World ? GEngine->GetWorldContextFromWorld(World)
		: GEngine->GetWorldContextFromPendingNetGameNetDriver(NetDriver);
	if (!Context || Context->OwningGameInstance != this) return;
	if (NetDriver->GetNetMode() != NM_Client
		&& (FailureType == ENetworkFailure::ConnectionLost || FailureType == ENetworkFailure::ConnectionTimeout)) return;

	PFLOG(Warning, TEXT("Session network failure: %s"), *Error);
	SessionNotice = NSLOCTEXT("PFSession", "Disconnected", "The connection has been lost. Please create or join a session again.");
	UpdateSessionUI();
}

// 맵 이동 실패 안내 저장
void UPFGameInstance::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& Error)
{
	if (bShuttingDown || bExitRequested || !World || World->GetGameInstance() != this) return;
	PFLOG(Warning, TEXT("Session travel failure %d: %s"), static_cast<int32>(FailureType), *Error);
	SessionNotice = NSLOCTEXT("PFSession", "TravelFailed", "Could not connect to the game. Please create or join a session again.");
	ReturnToMainMenu();
}

// 맵 이동 완료 상태 반영
void UPFGameInstance::HandleMapLoaded(UWorld* World)
{
	if (!World || World->GetGameInstance() != this) return;
	if (SessionOperation == ESessionOperation::Traveling)
	{
		SessionOperation = ESessionOperation::Idle;
	}
	NextHostHeartbeatAt = 0.0;
	LastHostHeartbeatReceivedAt = FPlatformTime::Seconds();
	ObservedSessionWorld = World;
	if (World->GetMapName().Contains(TEXT("Title")))
	{
		bReturningToTitle = false;
		const bool bLobbyMap = World->URL.HasOption(TEXT("PFMode=Lobby"));
		if (bLobbyMap && World->GetNetMode() == NM_Standalone && !bCleanupRequested)
		{
			SessionNotice = NSLOCTEXT("PFSession", "ListenFailed", "Could not host the session. Please try again.");
			ReturnToMainMenu();
			return;
		}
		bLobbyReady = !bCleanupRequested && bLobbyMap
			&& World->GetNetMode() == NM_ListenServer;
		if (bLobbyReady) PublishHostedSession();
	}
	else RemoveTitle();
	UpdateSessionUI();
}

// 로컬 플레이어와 세션 소유자 비교
bool UPFGameInstance::IsLocalSessionHost(const FNamedOnlineSession& Session) const
{
	if (!Session.OwningUserId.IsValid()) return false;
	const ULocalPlayer* LocalPlayer = GetFirstGamePlayer();
	if (!LocalPlayer) return false;
	const FUniqueNetIdRepl LocalUserId = LocalPlayer->GetPreferredUniqueNetId();
	return LocalUserId.IsValid() && *LocalUserId == *Session.OwningUserId;
}

// 세션 호스트의 최근 생존 신호 확인
bool UPFGameInstance::IsSessionHostAvailable(const FNamedOnlineSession& Session) const
{
	if (!bSteamSessions || IsLocalSessionHost(Session)) return true;
	return Session.OwningUserId.IsValid() && !LastHostHeartbeat.IsEmpty()
		&& FPlatformTime::Seconds() - LastHostHeartbeatReceivedAt < HostHeartbeatTimeout;
}

// 호스트 생존 신호 갱신 완료
void UPFGameInstance::OnHostHeartbeatComplete(FName SessionName, bool bSucceeded)
{
	// 종료 후 도착한 콜백의 상태 변경 방지
	if (bShuttingDown || bExitDispatched || SessionName != NAME_GameSession || !bHostHeartbeatPending) return;
	bHostHeartbeatPending = false;
	const bool bCompletedMatchStart = bPublishingMatchStart;
	bPublishingMatchStart = false;
	if (!bSucceeded)
	{
		PFLOG(Warning, TEXT("Host heartbeat update failed"));
	}
	if (bCleanupRequested)
	{
		BeginSessionCleanup();
		return;
	}
	if (bCompletedMatchStart)
	{
		if (bSucceeded) TravelToLobbyMatch();
		else
		{
			SessionNotice = NSLOCTEXT("PFSession", "StartFailed", "Could not start the session. Please create or join a session again.");
			ReturnToMainMenu();
		}
	}
	else if (!PendingGameplayTravelURL.IsEmpty())
	{
		PublishHostedSession();
	}
}

// 세션 단계, 생존 신호 순차 게시
void UPFGameInstance::PublishHostedSession()
{
	if (bHostHeartbeatPending || bCleanupRequested || bReturningToTitle || bExitRequested || !OnlineSessionInterface.IsValid()) return;
	const FNamedOnlineSession* Session = OnlineSessionInterface->GetNamedSession(NAME_GameSession);
	if (!Session || !IsLocalSessionHost(*Session)) return;
	FOnlineSessionSettings Settings = Session->SessionSettings;
	const bool bCanJoin = bLobbyReady && !bSessionMatchStarted;
	Settings.bAllowJoinInProgress = bCanJoin;
	Settings.bAllowJoinViaPresence = bCanJoin;
	Settings.bShouldAdvertise = bCanJoin;
	const FString Phase = bSessionMatchStarted
		? (PendingGameplayTravelURL.IsEmpty() ? TEXT("Playing") : TEXT("Starting"))
		: (bLobbyReady ? TEXT("LobbyReady") : TEXT("Creating"));
	Settings.Set(SessionPhaseKey, Phase, EOnlineDataAdvertisementType::ViaOnlineService);
	if (UWorld* World = GetWorld())
	{
		Settings.Set(FName("MapName"), World->GetOutermost()->GetName(), EOnlineDataAdvertisementType::ViaOnlineService);
	}
	Settings.Set(HostHeartbeatKey, FGuid::NewGuid().ToString(), EOnlineDataAdvertisementType::ViaOnlineService);
	NextHostHeartbeatAt = FPlatformTime::Seconds() + HostHeartbeatInterval;
	bPublishingMatchStart = !PendingGameplayTravelURL.IsEmpty();
	bHostHeartbeatPending = true;
	if (!OnlineSessionInterface->UpdateSession(NAME_GameSession, Settings, true) && bHostHeartbeatPending)
	{
		OnHostHeartbeatComplete(NAME_GameSession, false);
	}
}

// 이전 실행에서 남긴 로컬 세션 기록 정리
void UPFGameInstance::RecoverPreviousLobby()
{
	if (bStartupLobbyRecovered) return;
	if (!bSteamSessions || GIsEditor)
	{
		bStartupLobbyRecovered = true;
		return;
	}
	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	const IOnlineIdentityPtr Identity = OnlineSubsystem ? OnlineSubsystem->GetIdentityInterface() : nullptr;
	if (!GConfig || !Identity.IsValid() || Identity->GetLoginStatus(0) != ELoginStatus::LoggedIn) return;
	const FUniqueNetIdPtr UserId = Identity->GetUniquePlayerId(0);
	if (!UserId.IsValid()) return;

	RecoverySettingsSection = FString::Printf(TEXT("PortFolio.SessionRecovery.%s"), *UserId->ToString());
	FString PreviousLobby;
	GConfig->GetString(*RecoverySettingsSection, TEXT("LobbyId"), PreviousLobby, GGameUserSettingsIni);
	if (!PreviousLobby.IsEmpty())
	{
		SessionNotice = NSLOCTEXT("PFSession", "RecoveryReset", "Previous session information was reset. Please create or join a session.");
		PFLOG(Warning, TEXT("Cleared previous local session record %s"), *PreviousLobby);
	}
	SaveRecoveryLobby(FString());
	bStartupLobbyRecovered = true;
	UpdateSessionUI();
}

// 재실행 복구용 로비 기록
void UPFGameInstance::SaveRecoveryLobby(const FString& LobbyId)
{
	if (!GConfig || !bSteamSessions || GIsEditor || RecoverySettingsSection.IsEmpty()) return;
	GConfig->SetString(*RecoverySettingsSection, TEXT("LobbyId"), *LobbyId, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

// 세션 대기 제한, 로비 호스트 감시
bool UPFGameInstance::TickSessionRecovery(float DeltaTime)
{
	(void)DeltaTime;
	if (bShuttingDown || bExitDispatched) return false;
	RecoverPreviousLobby();
	if (bExitRequested) return true;

	const double Now = FPlatformTime::Seconds();
	UWorld* World = GetWorld();
	if (World && ObservedSessionWorld.Get() != World)
	{
		ObservedSessionWorld = World;
		LastHostHeartbeatReceivedAt = Now;
		NextHostHeartbeatAt = 0.0;
	}
	if (const APFSessionGameState* State = APFSessionGameState::Get(World))
	{
		if (State->bInitialized && State->Phase != EPFSessionPhase::Menu)
		{
			PendingSessionMode = State->Mode;
			PendingSessionCapacity = State->Capacity;
			if (State->Phase == EPFSessionPhase::Starting || State->Phase == EPFSessionPhase::Playing)
			{
				bSessionMatchStarted = true;
			}
		}
	}
	if (World && bSessionMatchStarted && PendingGameplayTravelURL.IsEmpty()
		&& !World->GetMapName().Contains(TEXT("Title")) && !World->IsInSeamlessTravel()
		&& SessionOperation == ESessionOperation::Traveling)
	{
		SessionOperation = ESessionOperation::Idle;
		LastHostHeartbeatReceivedAt = Now;
		NextHostHeartbeatAt = 0.0;
		RemoveTitle();
	}
	if (bCleanupRequested)
	{
		if (SessionOperation == ESessionOperation::Destroying && OnlineSessionInterface.IsValid()
			&& !OnlineSessionInterface->GetNamedSession(NAME_GameSession))
		{
			OnDestroySessionComplete(NAME_GameSession, true);
			return true;
		}
		if (!bCleanupTimedOut && Now >= CleanupDeadline)
		{
			// 늦은 콜백이 새 세션을 지우지 않도록 기존 작업 유지
			bCleanupTimedOut = true;
			PendingSessionRequest = ESessionRequest::None;
			bRestoreSessionInput = true;
			SessionNotice = NSLOCTEXT("PFSession", "CleanupDelayed", "Steam session cleanup is delayed. Try again shortly, or restart the game.");
			PFLOG(Warning, TEXT("Session cleanup timed out, waiting for the outstanding operation before reuse"));
			UpdateSessionUI();
		}
		return true;
	}

	if ((SessionOperation == ESessionOperation::Creating || SessionOperation == ESessionOperation::Finding
		|| SessionOperation == ESessionOperation::Joining) && Now - SessionOperationStartedAt >= 30.0)
	{
		SessionNotice = NSLOCTEXT("PFSession", "RequestTimeout", "The session request timed out. Please try again.");
		FailSessionRequest();
		return true;
	}
	if (SessionOperation == ESessionOperation::Traveling && Now - SessionOperationStartedAt >= 60.0)
	{
		SessionNotice = NSLOCTEXT("PFSession", "TravelTimeout", "Connecting to the game timed out. Please try again.");
		ReturnToMainMenu();
		return true;
	}

	if (OnlineSessionInterface.IsValid()
		&& (SessionOperation == ESessionOperation::Idle || SessionOperation == ESessionOperation::Traveling
			|| SessionOperation == ESessionOperation::ValidatingHost))
	{
		const FNamedOnlineSession* Session = OnlineSessionInterface->GetNamedSession(NAME_GameSession);
		if (!Session)
		{
			if (SessionOperation == ESessionOperation::ValidatingHost)
			{
				FailSessionRequest();
			}
			return true;
		}
		if (IsLocalSessionHost(*Session))
		{
			if (!bHostHeartbeatPending && Now >= NextHostHeartbeatAt)
			{
				PublishHostedSession();
			}
			return true;
		}
		if (!bSteamSessions) return true;

		FString Heartbeat;
		if (Session->SessionSettings.Get(HostHeartbeatKey, Heartbeat) && !Heartbeat.IsEmpty() && Heartbeat != LastHostHeartbeat)
		{
			LastHostHeartbeat = Heartbeat;
			LastHostHeartbeatReceivedAt = Now;
			if (SessionOperation == ESessionOperation::ValidatingHost)
			{
				SessionOperation = ESessionOperation::Idle;
				StartGame();
				return true;
			}
		}
		if (SessionOperation == ESessionOperation::ValidatingHost)
		{
			if (Now - SessionOperationStartedAt >= HostValidationTimeout)
			{
				RejectedLobbyIds.Add(JoiningLobbyId);
				PendingSessionRequest = ESessionRequest::Join;
				BeginSessionCleanup();
			}
		}
		else if ((!World || World->GetNetMode() != NM_Client || !World->GetNetDriver()
			|| !World->GetNetDriver()->ServerConnection) && !IsSessionHostAvailable(*Session))
		{
			SessionNotice = NSLOCTEXT("PFSession", "HostLeft", "The host has disconnected. Please create or join another session.");
			ReturnToMainMenu();
		}
	}
	return true;
}

// 진행 중인 작업 완료 후 세션 제거
void UPFGameInstance::BeginSessionCleanup()
{
	if (bShuttingDown || bExitDispatched)
	{
		return;
	}
	if (!bCleanupRequested)
	{
		CleanupDeadline = FPlatformTime::Seconds() + 5.0;
		bCleanupTimedOut = false;
	}
	bCleanupRequested = true;
	bLobbyReady = false;
	PendingGameplayTravelURL.Empty();
	Address.Empty();
	UpdateSessionUI();

	if (bHostHeartbeatPending || SessionOperation == ESessionOperation::Creating
		|| SessionOperation == ESessionOperation::Finding
		|| SessionOperation == ESessionOperation::Joining
		|| SessionOperation == ESessionOperation::Destroying)
	{
		return;
	}

	ClearSessionDelegates();
	SessionSearch.Reset();
	SessionOperation = ESessionOperation::Idle;
	if (!OnlineSessionInterface.IsValid() || !OnlineSessionInterface->GetNamedSession(NAME_GameSession))
	{
		FinishSessionCleanup(true);
		return;
	}

	SessionOperation = ESessionOperation::Destroying;
	DestroySessionCompleteHandle = OnlineSessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
		FOnDestroySessionCompleteDelegate::CreateUObject(this, &UPFGameInstance::OnDestroySessionComplete));
	if (OnlineSessionInterface->GetSessionState(NAME_GameSession) == EOnlineSessionState::Destroying)
	{
		return;
	}

	const bool bStarted = OnlineSessionInterface->DestroySession(NAME_GameSession);
	if (!bStarted && SessionOperation == ESessionOperation::Destroying
		&& OnlineSessionInterface->GetSessionState(NAME_GameSession) != EOnlineSessionState::Destroying)
	{
		OnDestroySessionComplete(NAME_GameSession, false);
	}
}

// 세션 제거 결과 확인
void UPFGameInstance::OnDestroySessionComplete(FName SessionName, bool bSucceeded)
{
	if (bShuttingDown || bExitDispatched || SessionName != NAME_GameSession
		|| SessionOperation != ESessionOperation::Destroying)
	{
		return;
	}
	ClearSessionDelegates();
	SessionOperation = ESessionOperation::Idle;

	// 완료된 삭제 작업의 로컬 잔여 정보 정리
	if (!bSucceeded && OnlineSessionInterface.IsValid() && OnlineSessionInterface->GetNamedSession(SessionName))
	{
		PFLOG(Warning, TEXT("Destroy session failed, removing inactive local session"));
		OnlineSessionInterface->RemoveNamedSession(SessionName);
	}
	const bool bRemoved = !OnlineSessionInterface.IsValid() || !OnlineSessionInterface->GetNamedSession(SessionName);
	FinishSessionCleanup(bRemoved);
}

// 정리 완료 후 예약 요청, 입력 복원
void UPFGameInstance::FinishSessionCleanup(bool bSucceeded)
{
	if (bSucceeded && bCleanupTimedOut)
	{
		SessionNotice = NSLOCTEXT("PFSession", "CleanupRecovered", "The previous session has been cleared. You can create or join a session again.");
		bRestoreSessionInput = true;
	}
	bCleanupRequested = false;
	bCleanupTimedOut = false;
	CleanupDeadline = 0.0;
	LastHostHeartbeat.Empty();
	LastHostHeartbeatReceivedAt = 0.0;
	NextHostHeartbeatAt = 0.0;
	bLobbyReady = false;
	bSessionMatchStarted = false;
	bPublishingMatchStart = false;
	PendingGameplayTravelURL.Empty();
	if (bSucceeded)
	{
		SaveRecoveryLobby(FString());
	}
	SessionOperation = ESessionOperation::Idle;
	SessionSearch.Reset();
	Address.Empty();

	if (bExitRequested)
	{
		FinishExitGame();
		return;
	}
	if (!bSucceeded)
	{
		PFLOG(Warning, TEXT("Session cleanup incomplete"));
		PendingSessionRequest = ESessionRequest::None;
		bRestoreSessionInput = true;
	}
	else if (PendingSessionRequest != ESessionRequest::None)
	{
		StartPendingSessionRequest();
		return;
	}

	InputSessionName.Empty();
	UpdateSessionUI();
}

// 실패한 요청의 잔여 세션 정리
void UPFGameInstance::FailSessionRequest()
{
	if (SessionNotice.IsEmpty())
	{
		SessionNotice = NSLOCTEXT("PFSession", "RequestFailed", "Could not create or join the session. Check the session name and try again.");
	}
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Session request failed"));
	}
	PendingSessionRequest = ESessionRequest::None;
	bRestoreSessionInput = true;
	BeginSessionCleanup();
}

// 세션 완료 이벤트 구독 해제
void UPFGameInstance::ClearSessionDelegates()
{
	if (OnlineSessionInterface.IsValid())
	{
		OnlineSessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
		OnlineSessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
		OnlineSessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
		OnlineSessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteHandle);
	}
	CreateSessionCompleteHandle.Reset();
	FindSessionsCompleteHandle.Reset();
	JoinSessionCompleteHandle.Reset();
	DestroySessionCompleteHandle.Reset();
}

// 현재 타이틀에 세션 처리 상태 반영
void UPFGameInstance::UpdateSessionUI()
{
	if (!IsValid(TitleWidget) || TitleWidget->GetWorld() != GetWorld() || !TitleWidget->IsInViewport())
	{
		return;
	}
	TitleWidget->SetSessionNotice(SessionNotice);
	const bool bBusy = bReturningToTitle || bExitRequested
		|| (!bCleanupTimedOut && (bCleanupRequested || SessionOperation != ESessionOperation::Idle));
	const bool bEnteringGame = !bReturningToTitle && !bExitRequested && !bCleanupTimedOut
		&& (PendingSessionRequest == ESessionRequest::Create
			|| (!bCleanupRequested && (SessionOperation == ESessionOperation::Creating
				|| SessionOperation == ESessionOperation::Traveling)));
	TitleWidget->SetSessionBusy(bBusy, bEnteringGame);
	if (!bBusy && bRestoreSessionInput)
	{
		TitleWidget->ShowJoinFailed();
		bRestoreSessionInput = false;
	}
}

// 세션 대기 종료, 게임 종료 명령
void UPFGameInstance::FinishExitGame()
{
	if (bShuttingDown || bExitDispatched)
	{
		return;
	}
	bExitDispatched = true;
	ClearSessionDelegates();
	FTSTicker::RemoveTicker(ExitTickerHandle);
	ExitTickerHandle.Reset();
	if (APlayerController* PlayerController = GetFirstLocalPlayerController())
	{
		PlayerController->ConsoleCommand(TEXT("quit"));
	}
	else if (GEngine)
	{
		GEngine->Exec(GetWorld(), TEXT("QUIT"));
	}
	else
	{
		PFLOG(Warning, TEXT("Quit not dispatched: no local controller or engine"));
	}
}

// 세션 정리 응답 지연 시 게임 종료
bool UPFGameInstance::HandleExitTimeout(float DeltaTime)
{
	(void)DeltaTime;
	ExitTickerHandle.Reset();
	PFLOG(Warning, TEXT("Session cleanup exceeded exit timeout"));
	FinishExitGame();
	return false;
}

// 선택 캐릭터 저장
void UPFGameInstance::SetCharacterType(ECHARACTER SelectedCharacter)
{
	CharacterType = SelectedCharacter;
}

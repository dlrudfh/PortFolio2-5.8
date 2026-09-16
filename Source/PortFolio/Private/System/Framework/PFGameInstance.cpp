#include "System/Framework/PFGameInstance.h"

#include "Props/PFItem.h"
#include "UI/Menu/PFTitle.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameDelegates.h"

UPFGameInstance::UPFGameInstance() : CreateSessionCompleteDelegate(FOnCreateSessionCompleteDelegate::CreateUObject(this, &UPFGameInstance::OnCreateSessionComplete)),
FindSessionsCompleteDelegate(FOnFindSessionsCompleteDelegate::CreateUObject(this, &UPFGameInstance::OnFindSessionsComplete)),
JoinSessionCompleteDelegate(FOnJoinSessionCompleteDelegate::CreateUObject(this, &UPFGameInstance::OnJoinSessionComplete)), CharacterType(CHARACTER_TWINBLAST), IsFindSession(false)
{
	FString CharacterDataPath = TEXT("/Game/GameData/PFCharacterData.PFCharacterData");
	static ConstructorHelpers::FObjectFinder<UDataTable> DT_PFCHARACTER(*CharacterDataPath);
	PFCHECK(DT_PFCHARACTER.Succeeded());
	PFCharacterTable = DT_PFCHARACTER.Object;
	PFCHECK(PFCharacterTable->GetRowMap().Num() > 0);

	FString ItemDataPath = TEXT("/Game/GameData/PFItemData.PFItemData");
	static ConstructorHelpers::FObjectFinder<UDataTable> DT_PFITEM(*ItemDataPath);
	PFCHECK(DT_PFITEM.Succeeded());
	PFItemTable = DT_PFITEM.Object;
	PFCHECK(PFItemTable->GetRowMap().Num() > 0);
}

void UPFGameInstance::Init()
{
	Super::Init();
	DisconnectHandle = FGameDelegates::Get().GetHandleDisconnectDelegate()
		.AddUObject(this, &UPFGameInstance::HandleSessionDisconnect);

    // 온라인 세션 인터페이스 연결
    IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
    if (OnlineSubsystem)
    {
        OnlineSessionInterface = OnlineSubsystem->GetSessionInterface();

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Yellow,
                FString::Printf(TEXT("Find Subsystem : %s"),
                    *OnlineSubsystem->GetSubsystemName().ToString()));
        }
    }
}

void UPFGameInstance::Shutdown()
{
	bShuttingDown = true;
	PendingSessionRequest = ESessionRequest::None;
	ClearSessionDelegates();
	FGameDelegates::Get().GetHandleDisconnectDelegate().Remove(DisconnectHandle);
	DisconnectHandle.Reset();
	FTSTicker::RemoveTicker(ExitTickerHandle);
	ExitTickerHandle.Reset();
	SessionSearch.Reset();
	OnlineSessionInterface.Reset();
	Super::Shutdown();
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
			UpdateSessionUI();
			return;
		}
		TitleWidget->RemoveFromParent();
		TitleWidget = nullptr;
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

// 아이템 데이터 조회
FPFItemData* UPFGameInstance::GetPFItemData(int32 ItemID)
{
	return PFItemTable->FindRow<FPFItemData>(*FString::FromInt(ItemID), TEXT(""));
}

// 온라인 세션 생성 요청
void UPFGameInstance::CreateGameSession(const FString& SessionName)
{
	RequestSession(ESessionRequest::Create, SessionName);
}

// 기존 세션 정리 후 요청 예약
bool UPFGameInstance::RequestSession(ESessionRequest Request, const FString& SessionName)
{
	if (bShuttingDown || bExitRequested || bCleanupRequested || bReturningToTitle
		|| SessionOperation != ESessionOperation::Idle)
	{
		return false;
	}

	const ULocalPlayer* LocalPlayer = GetFirstGamePlayer();
	if (!OnlineSessionInterface.IsValid() || !LocalPlayer || !LocalPlayer->GetPreferredUniqueNetId().IsValid())
	{
		PFLOG(Warning, TEXT("Session interface or local player is not ready"));
		bRestoreSessionInput = true;
		UpdateSessionUI();
		return false;
	}

	InputSessionName = SessionName;
	PendingSessionRequest = Request;
	bRestoreSessionInput = false;
	if (OnlineSessionInterface->GetNamedSession(NAME_GameSession))
	{
		BeginSessionCleanup();
		return true;
	}

	return StartPendingSessionRequest();
}

// 예약된 세션 생성, 검색 시작
bool UPFGameInstance::StartPendingSessionRequest()
{
	if (bShuttingDown || bExitRequested || bCleanupRequested || bReturningToTitle
		|| SessionOperation != ESessionOperation::Idle || PendingSessionRequest == ESessionRequest::None)
	{
		return false;
	}

	const ULocalPlayer* LocalPlayer = GetFirstGamePlayer();
	if (!OnlineSessionInterface.IsValid() || !LocalPlayer || !LocalPlayer->GetPreferredUniqueNetId().IsValid())
	{
		FailSessionRequest();
		return false;
	}

	const ESessionRequest Request = PendingSessionRequest;
	PendingSessionRequest = ESessionRequest::None;
	ClearSessionDelegates();
	SessionSearch.Reset();
	Address.Empty();
	IsFindSession = false;

	if (Request == ESessionRequest::Create)
	{
		// 공개 세션, 로비 설정
		FOnlineSessionSettings SessionSettings;
		SessionSettings.bIsLANMatch = false;
		SessionSettings.NumPublicConnections = 4;
		SessionSettings.bAllowJoinInProgress = true;
		SessionSettings.bAllowJoinViaPresence = true;
		SessionSettings.bShouldAdvertise = true;
		SessionSettings.bUsesPresence = true;
		SessionSettings.bUseLobbiesIfAvailable = true;
		SessionSettings.Set(FName("SessionName"), InputSessionName, EOnlineDataAdvertisementType::ViaOnlineService);
		SessionSettings.Set(FName("MapName"), FString("/Game/ThirdPerson/Maps/Map2"), EOnlineDataAdvertisementType::ViaOnlineService);

		SessionOperation = ESessionOperation::Creating;
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
	FindSessionsCompleteHandle = OnlineSessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegate);
	UpdateSessionUI();
	const bool bStarted = OnlineSessionInterface->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(), SessionSearch.ToSharedRef());
	if (!bStarted && SessionOperation == ESessionOperation::Finding)
	{
		OnFindSessionsComplete(false);
	}
	return bStarted;
}

// 세션 생성 후 게임 맵 이동
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

	SessionOperation = ESessionOperation::Traveling;
	UpdateSessionUI();
	if (!World->ServerTravel(TEXT("/Game/ThirdPerson/Maps/Map2?listen")))
	{
		if (SessionOperation == ESessionOperation::Traveling)
		{
			FailSessionRequest();
		}
		return;
	}

	if (SessionOperation == ESessionOperation::Traveling && IsValid(TitleWidget))
	{
		TitleWidget->RemoveFromParent();
		TitleWidget = nullptr;
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

	SessionOperation = ESessionOperation::Traveling;
	UpdateSessionUI();
	PlayerController->ClientTravel(Address, TRAVEL_Absolute);
	if (SessionOperation == ESessionOperation::Traveling && IsValid(TitleWidget))
	{
		TitleWidget->RemoveFromParent();
		TitleWidget = nullptr;
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
	for (auto Result : SessionSearch->SearchResults)
	{
		FString SessionName;
		Result.Session.SessionSettings.Get(FName("SessionName"), SessionName);
		if (InputSessionName != SessionName || !Result.IsValid())
		{
			continue;
		}

		IsFindSession = true;
		Result.Session.SessionSettings.bUseLobbiesIfAvailable = true;
		Result.Session.SessionSettings.bUsesPresence = true;
		SessionOperation = ESessionOperation::Joining;
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
		UpdateSessionUI();
		if (IsValid(TitleWidget) && TitleWidget->GetWorld() == GetWorld() && TitleWidget->IsInViewport())
		{
			TitleWidget->ShowCharacterSelection();
		}
		return;
	}

	PFLOG(Warning, TEXT("Join session failed: %d"), static_cast<int32>(Result));
	FailSessionRequest();
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
	PendingSessionRequest = ESessionRequest::None;
	bRestoreSessionInput = false;
	bReturningToTitle = true;
	BeginSessionCleanup();
}

// 진행 중인 작업 완료 후 세션 제거
void UPFGameInstance::BeginSessionCleanup()
{
	if (bShuttingDown || bExitDispatched)
	{
		return;
	}
	bCleanupRequested = true;
	Address.Empty();
	IsFindSession = false;
	UpdateSessionUI();

	if (SessionOperation == ESessionOperation::Creating
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
	bCleanupRequested = false;
	SessionOperation = ESessionOperation::Idle;
	SessionSearch.Reset();
	Address.Empty();
	IsFindSession = false;

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
	const bool bBusy = bCleanupRequested || bReturningToTitle || bExitRequested
		|| SessionOperation != ESessionOperation::Idle;
	TitleWidget->SetSessionBusy(bBusy);
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

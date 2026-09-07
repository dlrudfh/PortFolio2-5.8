#include "System/Framework/PFGameInstance.h"

#include "Props/PFItem.h"
#include "UI/Menu/PFTitle.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/WidgetBlueprintLibrary.h"

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
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("CreateGameSession"));

	InputSessionName = SessionName;

	if (!OnlineSessionInterface.IsValid())
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("OnlineSessionInterface is not valid"));
		return;
	}

	// 기존 세션 제거 요청
	auto ExistingSession = OnlineSessionInterface->GetNamedSession(NAME_GameSession);

	if (ExistingSession)
	{
		OnlineSessionInterface->DestroySession(NAME_GameSession);
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("DestroySession"));
	}

	OnlineSessionInterface->AddOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegate);

	// 공개 세션, 로비 설정
	TSharedPtr<FOnlineSessionSettings> SessionSettings = MakeShareable(new FOnlineSessionSettings());
	SessionSettings->bIsLANMatch = false;
	SessionSettings->NumPublicConnections = 4;
	SessionSettings->bAllowJoinInProgress = true;
	SessionSettings->bAllowJoinViaPresence = true;
	SessionSettings->bShouldAdvertise = true;
	SessionSettings->bUsesPresence = true;
	SessionSettings->bUseLobbiesIfAvailable = true;
	SessionSettings->Set(FName("SessionName"), SessionName, EOnlineDataAdvertisementType::ViaOnlineService);
	SessionSettings->Set(FName("MapName"), FString("/Game/ThirdPerson/Maps/Map2"), EOnlineDataAdvertisementType::ViaOnlineService);

	ULocalPlayer* LocalPlayer = GetFirstGamePlayer();
	OnlineSessionInterface->CreateSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, *SessionSettings);
}

// 세션 생성 후 게임 맵 이동
void UPFGameInstance::OnCreateSessionComplete(FName SessionName, bool IsSucceeded)
{
	if (IsSucceeded)
	{
		GEngine->AddOnScreenDebugMessage(-1, 100.f, FColor::White, FString::Printf(TEXT("Create Session : %s"), *InputSessionName));

		UWorld* world = GetWorld();
		if (world)
		{
			world->ServerTravel(FString("/Game/ThirdPerson/Maps/Map2?listen"));
		}

		GEngine->AddOnScreenDebugMessage(-1, 100.f, FColor::Green, TEXT("Current State : Server"));
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("Create Session Failed : %s"), *SessionName.ToString()));
	}
}

// 참가할 세션 검색 요청
bool UPFGameInstance::JoinGameSession(const FString& SessionName)
{
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("JoinGameSession"));
	if (!OnlineSessionInterface.IsValid())
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("OnlineSessionInterface is not valid"));
		return false;
	}

	// 검색 상태 초기화, 로비 검색 설정
	InputSessionName = SessionName;
	IsFindSession = false;
	Address.Empty();

	OnlineSessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegate);

	SessionSearch = MakeShareable(new FOnlineSessionSearch());
	SessionSearch->MaxSearchResults = 10000;
	SessionSearch->bIsLanQuery = false;
	SessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);

	const ULocalPlayer* LocalPlayer = GetFirstGamePlayer();
	if (!LocalPlayer)
	{
		return false;
	}

	OnlineSessionInterface->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(), SessionSearch.ToSharedRef());

	return true;
}

// 참가 세션으로 이동
void UPFGameInstance::StartGame()
{
	ULocalPlayer* LP = GetFirstGamePlayer();
	if (!LP) return;

	APlayerController* PlayerController = LP->GetPlayerController(GetWorld());

	if (PlayerController)
	{
		if (Address.IsEmpty())
		{
			PFLOG(Warning, TEXT("Join address is not ready yet"));
			return;
		}
		PlayerController->ClientTravel(Address, TRAVEL_Absolute);
	}
}

// 이름이 일치하는 세션 참가
void UPFGameInstance::OnFindSessionsComplete(bool IsSucceeded)
{
	if (!OnlineSessionInterface.IsValid() || !SessionSearch.IsValid())
	{
		return;
	}

	// 검색 결과에서 세션 이름 비교
	for (auto Result : SessionSearch->SearchResults)
	{
		FString Id = Result.GetSessionIdStr();
		FString UserName = Result.Session.OwningUserName;
		FString SessionName;
		Result.Session.SessionSettings.Get(FName("SessionName"), SessionName);
		FString MapName;
		Result.Session.SessionSettings.Get(FName("MapName"), MapName);

		if (InputSessionName == SessionName)
		{
			if (IsSucceeded)
			{
				IsFindSession = true;
				GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Find Session Succeed"));
			}

			GEngine->AddOnScreenDebugMessage(-1, 100.f, FColor::White, FString::Printf(TEXT("Id : %s"), *Id));
			GEngine->AddOnScreenDebugMessage(-1, 100.f, FColor::White, FString::Printf(TEXT("UserName : %s"), *UserName));
			GEngine->AddOnScreenDebugMessage(-1, 100.f, FColor::White, FString::Printf(TEXT("Session Name : %s"), *SessionName));
			GEngine->AddOnScreenDebugMessage(-1, 100.f, FColor::White, FString::Printf(TEXT("MapName : %s"), *MapName));

			// 참가 완료 이벤트 연결, 참가 요청
			OnlineSessionInterface->AddOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegate);

			Result.Session.SessionSettings.bUseLobbiesIfAvailable = true;
			Result.Session.SessionSettings.bUsesPresence = true;

			const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
			if (LocalPlayer)
			{
				OnlineSessionInterface->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, Result);
			}

			return;
		}
	}

	if (!IsFindSession)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Find Session Failed"));
		if (TitleWidget)
		{
			TitleWidget->ShowJoinFailed();
		}
	}
}

// 참가 결과에 따라 타이틀 전환
void UPFGameInstance::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (!OnlineSessionInterface.IsValid())
	{
		return;
	}

	if (Result == EOnJoinSessionCompleteResult::Success &&
		OnlineSessionInterface->GetResolvedConnectString(NAME_GameSession, Address))
	{
		if (TitleWidget)
		{
			TitleWidget->ShowCharacterSelection();
		}
		return;
	}

	if (TitleWidget)
	{
		TitleWidget->ShowJoinFailed();
	}
}

// 게임 종료 처리 (미구현)
void UPFGameInstance::ExitGame()
{
}

// 선택 캐릭터 저장
void UPFGameInstance::SetCharacterType(ECHARACTER SelectedCharacter)
{
	CharacterType = SelectedCharacter;
}

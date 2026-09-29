
#pragma once

#include "PortFolio/PortFolio.h"

#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Blueprint/UserWidget.h"
#include "Containers/Ticker.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h"
#include "Interfaces/OnlineSessionInterface.h"

#include "PFGameInstance.generated.h"

// 캐릭터 데이터 행
USTRUCT(BlueprintType)
struct FPFCharacterData : public FTableRowBase
{
	GENERATED_BODY()

public:
	FPFCharacterData() : Level(1), MaxHP(10.f), MaxMP(10.f), Damage(1.f), CurExp(0) {}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CharacterData")
	int32 Level;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CharacterData")
	float MaxHP;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CharacterData")
	float MaxMP;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CharacterData")
	float Damage;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CharacterData")
	int32 CurExp;
};

// 아이템 데이터 행
USTRUCT(BlueprintType)
struct FPFItemData : public FTableRowBase
{
	GENERATED_BODY()

public:
	FPFItemData() : Name("Default"), Type("Default"), CurHP(0.f), MaxHP(0.f), CurMP(0.f), MaxMP(0.f), Damage(0.f), CurExp(0), Price(1){}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ItemData")
	FString Name;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ItemData")
	FString Type;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ItemData")
	float CurHP;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ItemData")
	float MaxHP;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ItemData")
	float CurMP;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ItemData")
	float MaxMP;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ItemData")
	float Damage;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ItemData")
	int32 CurExp;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ItemData")
	int32 Price;
};

// 게임 데이터, 세션 관리 클래스
UCLASS()
class PORTFOLIO_API UPFGameInstance : public UGameInstance
{
	GENERATED_BODY()
	
public:
	UPFGameInstance();
	virtual void Init() override;
	virtual void Shutdown() override;
	virtual void ReturnToMainMenu() override;
	void CreateTitle();
	float GetMenuVolume() const { return MenuVolume; }
	float GetCameraSensitivity() const { return CameraSensitivity; }
	void SetMenuVolume(float Value);
	void SetCameraSensitivity(float Value);
	void ApplyMenuVolume();
	void SaveMenuSettings();

	FPFCharacterData* GetPFCharacterData(int32 CharacterID);
	FPFItemData* GetPFItemData(int32 ItemID);
	UPROPERTY()
	TSubclassOf<UUserWidget> TitleWidgetClass;

	UFUNCTION(BlueprintCallable)
	void CreateGameSession(const FString& SessionName);
	UFUNCTION(BlueprintCallable)
	bool JoinGameSession(const FString& SessionName);
	UFUNCTION()
	void StartGame();
	UFUNCTION(BlueprintCallable)
	void ExitGame();
	void OnCreateSessionComplete(FName SessionName, bool IsSucceeded);
	void OnFindSessionsComplete(bool IsSucceeded);
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	
	// 온라인 세션 인터페이스
	IOnlineSessionPtr OnlineSessionInterface;

	// 세션 생성, 검색 완료 이벤트
	FOnCreateSessionCompleteDelegate CreateSessionCompleteDelegate;
	FOnFindSessionsCompleteDelegate FindSessionsCompleteDelegate;
	// 세션 검색 결과
	TSharedPtr<FOnlineSessionSearch> SessionSearch;
	FOnJoinSessionCompleteDelegate JoinSessionCompleteDelegate;

public:
	ECHARACTER GetCharacterType() { return CharacterType; };
	void SetCharacterType(ECHARACTER SelectedCharacter);

private:
	enum class ESessionOperation : uint8
	{
		Idle,
		Creating,
		Finding,
		Joining,
		ValidatingHost,
		Destroying,
		Traveling
	};

	enum class ESessionRequest : uint8
	{
		None,
		Create,
		Join
	};

	bool RequestSession(ESessionRequest Request, const FString& SessionName);
	bool StartPendingSessionRequest();
	void HandleSessionDisconnect(UWorld* World, class UNetDriver* NetDriver);
	void HandleNetworkFailure(UWorld* World, class UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& Error);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& Error);
	void HandleMapLoaded(UWorld* World);
	bool TickSessionRecovery(float DeltaTime);
	bool IsSessionHostAvailable(const FNamedOnlineSession& Session) const;
	void OnHostHeartbeatComplete(FName SessionName, bool bSucceeded);
	void RecoverPreviousLobby();
	void SaveRecoveryLobby(const FString& LobbyId);
	void BeginSessionCleanup();
	void OnDestroySessionComplete(FName SessionName, bool bSucceeded);
	void FinishSessionCleanup(bool bSucceeded);
	void FailSessionRequest();
	void ClearSessionDelegates();
	void UpdateSessionUI();
	void FinishExitGame();
	bool HandleExitTimeout(float DeltaTime);

	UPROPERTY()
	ECHARACTER CharacterType;
	// 캐릭터, 아이템 데이터 테이블
	UPROPERTY()
	class UDataTable* PFCharacterTable;
	UPROPERTY()
	class UDataTable* PFItemTable;
	// 타이틀 UI
	UPROPERTY()
	class UPFTitle* TitleWidget;
	UPROPERTY()
	bool IsFindSession;
	UPROPERTY()
	FString Address;

	FString InputSessionName;
	FText SessionNotice;
	FString RecoverySettingsSection;
	FString JoiningLobbyId;
	FString LastHostHeartbeat;
	TSet<FString> RejectedLobbyIds;

	float MenuVolume = 100.f;
	float CameraSensitivity = 50.f;
	bool bMenuSettingsDirty = false;

	// 세션 작업 완료 이벤트 핸들
	FDelegateHandle CreateSessionCompleteHandle;
	FDelegateHandle FindSessionsCompleteHandle;
	FDelegateHandle JoinSessionCompleteHandle;
	FDelegateHandle DestroySessionCompleteHandle;
	FDelegateHandle HostHeartbeatCompleteHandle;
	// 연결 종료 이벤트 핸들
	FDelegateHandle DisconnectHandle;
	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;
	FDelegateHandle MapLoadedHandle;
	// 세션 복구, 로비 호스트 감시
	FTSTicker::FDelegateHandle SessionRecoveryTickerHandle;
	// 게임 종료 대기 핸들
	FTSTicker::FDelegateHandle ExitTickerHandle;
	ESessionOperation SessionOperation = ESessionOperation::Idle;
	ESessionRequest PendingSessionRequest = ESessionRequest::None;
	bool bCleanupRequested = false;
	bool bRestoreSessionInput = false;
	bool bReturningToTitle = false;
	bool bExitRequested = false;
	bool bExitDispatched = false;
	bool bShuttingDown = false;
	bool bSteamSessions = false;
	bool bStartupLobbyRecovered = false;
	bool bCleanupTimedOut = false;
	bool bHostHeartbeatPending = false;
	double CleanupDeadline = 0.0;
	double SessionOperationStartedAt = 0.0;
	double LastHostHeartbeatReceivedAt = 0.0;
	double NextHostHeartbeatAt = 0.0;
};

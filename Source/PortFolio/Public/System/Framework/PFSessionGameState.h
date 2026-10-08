#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "PFSessionGameState.generated.h"

class APFPlayerState;

UENUM(BlueprintType)
enum class EPFSessionMode : uint8
{
	Story,
	Versus,
	Training
};

UENUM(BlueprintType)
enum class EPFSessionPhase : uint8
{
	Menu,
	Lobby,
	Starting,
	Playing
};

DECLARE_MULTICAST_DELEGATE(FOnSessionStateChanged);

// 세션 모드, 로비 상태
UCLASS()
class PORTFOLIO_API APFSessionGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	static APFSessionGameState* Get(const UWorld* World);
	static bool IsStory(const UWorld* World);
	static bool IsVersus(const UWorld* World);
	static bool IsTraining(const UWorld* World);
	static int32 GetMaxPlayers(EPFSessionMode SessionMode);
	bool IsHost(const APlayerState* Player) const;
	bool CanStart() const;
	bool IsPartyReady() const;
	bool CanLeave(const APFPlayerState* Player) const;
	void NotifySessionChanged();

	UPROPERTY(ReplicatedUsing = OnRep_SessionState, BlueprintReadOnly)
	EPFSessionMode Mode = EPFSessionMode::Training;
	UPROPERTY(ReplicatedUsing = OnRep_SessionState, BlueprintReadOnly)
	EPFSessionPhase Phase = EPFSessionPhase::Menu;
	UPROPERTY(ReplicatedUsing = OnRep_SessionState, BlueprintReadOnly)
	int32 Capacity = 1;
	UPROPERTY(ReplicatedUsing = OnRep_SessionState, BlueprintReadOnly)
	int32 SettingsRevision = 0;
	UPROPERTY(ReplicatedUsing = OnRep_SessionState, BlueprintReadOnly)
	FName SelectedMap = TEXT("Map3");
	UPROPERTY(ReplicatedUsing = OnRep_SessionState, BlueprintReadOnly)
	FString RoomName;
	UPROPERTY(ReplicatedUsing = OnRep_SessionState, BlueprintReadOnly)
	TObjectPtr<APFPlayerState> HostPlayer;
	UPROPERTY(ReplicatedUsing = OnRep_SessionState, BlueprintReadOnly)
	bool bInitialized = false;

	// 세션 상태 변경 알림
	FOnSessionStateChanged OnSessionStateChanged;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_SessionState();
};

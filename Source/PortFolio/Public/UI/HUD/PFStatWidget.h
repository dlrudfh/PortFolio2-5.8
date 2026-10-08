#pragma once

#include "PortFolio/PortFolio.h"

#include "Blueprint/UserWidget.h"
#include "PFStatWidget.generated.h"

enum class EPFStatUpgradeType : uint8;

// 플레이어 스탯 위젯
UCLASS()
class PORTFOLIO_API UPFStatWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void BindPlayerState(class APFPlayerState* PlayerState);
	void SetStatWindowVisible(bool bVisible);

	bool IsStatWindowVisible() const;

	void RefreshStats();

protected:
	virtual void NativeConstruct() override;

	virtual void NativeDestruct() override;

private:
	void UnbindPlayerState();
	void RefreshIdentity();
	void RequestStatIncrease(EPFStatUpgradeType UpgradeType);
	class UPFAttributeSet* GetCurrentAttributeSet() const;

	FString ResolveUsername() const;

	UFUNCTION()
	void HandleIncreaseHP();

	UFUNCTION()
	void HandleIncreaseMP();

	UFUNCTION()
	void HandleIncreaseDamage();

private:
	// 표시 정보, 스탯 변경 구독 대상
	TWeakObjectPtr<class APFPlayerState> BoundPlayerState;
	TWeakObjectPtr<class UPFAttributeSet> BoundAttributeSet;

	// 스탯 창 영역
	UPROPERTY()
	class UBorder* StatWindowBorder = nullptr;

	// 플레이어 정보 표시
	UPROPERTY()
	class UTextBlock* UsernameValueText = nullptr;

	UPROPERTY()
	class UTextBlock* CharacterValueText = nullptr;

	UPROPERTY()
	class UTextBlock* LevelValueText = nullptr;

	UPROPERTY()
	class UTextBlock* HPValueText = nullptr;

	UPROPERTY()
	class UTextBlock* MPValueText = nullptr;

	UPROPERTY()
	class UTextBlock* DamageValueText = nullptr;

	UPROPERTY()
	class UTextBlock* StatPointValueText = nullptr;

	// 스탯 강화 버튼
	UPROPERTY()
	class UButton* HPIncreaseButton = nullptr;

	UPROPERTY()
	class UButton* MPIncreaseButton = nullptr;

	UPROPERTY()
	class UButton* DamageIncreaseButton = nullptr;
};

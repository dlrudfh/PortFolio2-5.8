#pragma once

#include "PortFolio/PortFolio.h"

#include "Blueprint/UserWidget.h"
#include "PFStatWidget.generated.h"

// 플레이어 스탯 위젯
UCLASS()
class PORTFOLIO_API UPFStatWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetStatWindowVisible(bool bVisible);

	bool IsStatWindowVisible() const;

	void RefreshStats();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	virtual void NativeConstruct() override;

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	class UHorizontalBox* CreateInfoRow(
		const FString& RowName,
		const FText& Label,
		class UTextBlock*& OutValueText,
		class UButton*& OutIncreaseButton,
		bool bCanIncrease);

	class UPFAttributeSet* GetCurrentAttributeSet() const;

	FString ResolveUsername() const;

	UFUNCTION()
	void HandleIncreaseHP();

	UFUNCTION()
	void HandleIncreaseMP();

	UFUNCTION()
	void HandleIncreaseDamage();

private:
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

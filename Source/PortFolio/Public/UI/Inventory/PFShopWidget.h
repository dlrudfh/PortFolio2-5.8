#pragma once

#include "Blueprint/UserWidget.h"
#include "System/Framework/PFPlayerState.h"
#include "PFShopWidget.generated.h"

// 상점 블루프린트 연결, 구매 상태 표시
UCLASS()
class PORTFOLIO_API UPFShopWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPFShopWidget(const FObjectInitializer& ObjectInitializer);
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	void RefreshShop();
	void Purchase(int32 ItemID);
	void HandlePurchaseResult(int32 ItemID, EPFShopPurchaseResult Result);
	UFUNCTION()
	void BuyHealthPotion();
	UFUNCTION()
	void BuyManaPotion();
	UFUNCTION()
	void BuyShield();
	UFUNCTION()
	void CloseShop();

	// 디자이너에서 편집하는 상점 레이아웃
	UPROPERTY()
	TSubclassOf<UUserWidget> LayoutClass;
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> LayoutWidget;
	UPROPERTY(Transient)
	TArray<TObjectPtr<class UButton>> PurchaseButtons;
	UPROPERTY(Transient)
	TArray<TObjectPtr<class UTextBlock>> CountTexts;
	UPROPERTY(Transient)
	TArray<TObjectPtr<class UTextBlock>> PurchaseTexts;
	UPROPERTY(Transient)
	TObjectPtr<class UTextBlock> CoinText;
	UPROPERTY(Transient)
	TObjectPtr<class UTextBlock> ResultText;
	TWeakObjectPtr<APFPlayerState> BoundPlayerState;
	TWeakObjectPtr<UPFAttributeSet> BoundAttributes;
	bool bPurchasePending = false;
};

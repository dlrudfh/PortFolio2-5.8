#pragma once

#include "Blueprint/UserWidget.h"
#include "PFMenuWidget.generated.h"

// 게임 중 타이틀 복귀, 종료 메뉴
UCLASS()
class PORTFOLIO_API UPFMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	class UButton* CreateMenuButton(class UVerticalBox* Layout, FName Name, const FText& Label);
	UFUNCTION()
	void HandleGoToTitle();
	UFUNCTION()
	void HandleExitGame();
};

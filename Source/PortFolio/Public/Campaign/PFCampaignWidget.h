#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PFCampaignWidget.generated.h"

// 캠페인 목표, 무전, 파티 상태, 결과 화면
UCLASS()
class PORTFOLIO_API UPFCampaignWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPFCampaignWidget(const FObjectInitializer& ObjectInitializer);
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
private:
	UFUNCTION() void ReturnToTitle();

	// 디자이너 캠페인 에셋
	UPROPERTY() TSubclassOf<UUserWidget> LayoutClass;
	UPROPERTY(Transient) TObjectPtr<UUserWidget> LayoutWidget;
	UPROPERTY() TObjectPtr<UWidget> ObjectivePanel;
	UPROPERTY() TObjectPtr<UWidget> EnemyRow;
	UPROPERTY() TObjectPtr<UWidget> RadioPanel;
	UPROPERTY() TObjectPtr<UWidget> PromptPanel;
	UPROPERTY() TObjectPtr<UWidget> BossPanel;
	UPROPERTY() TObjectPtr<UWidget> ResultLayer;
	UPROPERTY() TObjectPtr<class UTextBlock> Title;
	UPROPERTY() TObjectPtr<class UTextBlock> StepText;
	UPROPERTY() TObjectPtr<class UTextBlock> Objective;
	UPROPERTY() TObjectPtr<class UTextBlock> EnemyCount;
	UPROPERTY() TObjectPtr<class UTextBlock> Status;
	UPROPERTY() TObjectPtr<class UTextBlock> Radio;
	UPROPERTY() TObjectPtr<class UTextBlock> Prompt;
	UPROPERTY() TObjectPtr<class UTextBlock> BossName;
	UPROPERTY() TObjectPtr<class UProgressBar> BossHealth;
	UPROPERTY() TObjectPtr<class UTextBlock> ResultChapter;
	UPROPERTY() TObjectPtr<class UTextBlock> ResultTime;
	UPROPERTY() TObjectPtr<class UTextBlock> ResultDeaths;
	UPROPERTY() TObjectPtr<class UTextBlock> ResultRecords;
	FText LastRadio;
	float RadioRemaining = 0.f;
};

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PFTutorialWidget.generated.h"

class AActor;
class UProgressBar;
class UTextBlock;

// 튜토리얼 안내, 목표 위치, UI 강조 표시
UCLASS()
class PORTFOLIO_API UPFTutorialWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPFTutorialWidget(const FObjectInitializer& ObjectInitializer);
	void SetGuide(const FString& Title, const FString& Objective, float Progress, bool bComplete);
	void SetWorldMarker(AActor* Target, const FVector& Location, bool bShowLocation, const FString& Caption);
	void SetHighlight(UWidget* Target, const FString& Caption);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void RefreshGuide();
	void UpdateTargetWidgets(const FGeometry& Geometry);

	// 디자이너 안내 에셋
	UPROPERTY()
	TSubclassOf<UUserWidget> LayoutClass;
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> LayoutWidget;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ObjectiveText;
	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> ProgressBar;
	UPROPERTY(Transient)
	TObjectPtr<UWidget> WorldMarker;
	UPROPERTY(Transient)
	TObjectPtr<UWidget> MarkerDiamond;
	UPROPERTY(Transient)
	TObjectPtr<UWidget> MarkerArrow;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MarkerText;
	UPROPERTY(Transient)
	TObjectPtr<UWidget> HighlightFrame;
	UPROPERTY(Transient)
	TObjectPtr<UWidget> HighlightHint;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HighlightText;

	// 관리자가 지정한 표시 대상
	TWeakObjectPtr<UWidget> HighlightWidget;
	TWeakObjectPtr<AActor> MarkerActor;
	FString GuideTitle;
	FString GuideObjective;
	FString HighlightCaption;
	FString MarkerCaption;
	FVector MarkerLocation = FVector::ZeroVector;
	float GuideProgress = 0.f;
	FVector2D LastTargetScreen = FVector2D::ZeroVector;
	int32 LastMarkerDistance = INDEX_NONE;
	bool bHighlightLayoutDirty = true;
	bool bShowComplete = false;
	bool bHasMarkerLocation = false;
};

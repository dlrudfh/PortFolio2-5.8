#pragma once

#include "PortFolio/PortFolio.h"

#include "Components/Image.h"
#include "PFCooldownOverlayWidget.generated.h"

// 아이템 쿨타임 마스크 위젯
UCLASS()
class PORTFOLIO_API UPFCooldownOverlayWidget : public UImage
{
	GENERATED_BODY()

public:
	void SetCooldownProgress(float InCooldownProgress);

	void SetOverlayColor(const FLinearColor& InOverlayColor);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void SynchronizeProperties() override;

private:
	void UpdateOverlay();

	UPROPERTY()
	float CooldownProgress = 0.f;

	UPROPERTY()
	FLinearColor OverlayColor = FLinearColor(0.f, 0.f, 0.f, 0.70f);
};

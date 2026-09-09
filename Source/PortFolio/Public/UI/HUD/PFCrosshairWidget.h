#pragma once

#include "PortFolio/PortFolio.h"
#include "Blueprint/UserWidget.h"
#include "PFCrosshairWidget.generated.h"

// 조준점 위젯
UCLASS()
class PORTFOLIO_API UPFCrosshairWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPFCrosshairWidget(const FObjectInitializer& ObjectInitializer);
	void SetCharacterTargeted(bool bTargeted);

protected:
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	// 조준점 텍스처
	UPROPERTY()
	class UTexture2D* CrosshairTexture = nullptr;
	bool bCharacterTargeted = false;
};

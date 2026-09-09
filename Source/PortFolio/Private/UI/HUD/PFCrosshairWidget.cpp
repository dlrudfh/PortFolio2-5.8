#include "UI/HUD/PFCrosshairWidget.h"

#include "Engine/Texture2D.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateLayoutTransform.h"
#include "Styling/SlateBrush.h"
#include "UObject/ConstructorHelpers.h"

UPFCrosshairWidget::UPFCrosshairWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UTexture2D> Crosshair(
		TEXT("/Game/GameData/UI/Crosshair.Crosshair"));
	if (Crosshair.Succeeded())
	{
		CrosshairTexture = Crosshair.Object;
	}
}

// 캐릭터 조준 상태 반영
void UPFCrosshairWidget::SetCharacterTargeted(bool bTargeted)
{
	if (bCharacterTargeted == bTargeted)
	{
		return;
	}

	bCharacterTargeted = bTargeted;
	InvalidateLayoutAndVolatility();
}

int32 UPFCrosshairWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 PaintLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	if (!CrosshairTexture)
	{
		return PaintLayer;
	}

	// 화면 중앙에 조준점 표시
	const FVector2D CrosshairSize(200.f, 200.f);
	const FVector2D CrosshairPosition = (AllottedGeometry.GetLocalSize() - CrosshairSize) * 0.5;
	const float Opacity = InWidgetStyle.GetColorAndOpacityTint().A;
	const FLinearColor CrosshairColor = bCharacterTargeted
		? FLinearColor(1.f, 0.f, 0.f, Opacity)
		: FLinearColor(1.f, 1.f, 1.f, Opacity);

	FSlateBrush CrosshairBrush;
	CrosshairBrush.SetResourceObject(CrosshairTexture);
	CrosshairBrush.DrawAs = ESlateBrushDrawType::Image;
	CrosshairBrush.ImageSize = CrosshairSize;

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		PaintLayer + 1,
		AllottedGeometry.ToPaintGeometry(CrosshairSize, FSlateLayoutTransform(CrosshairPosition)),
		&CrosshairBrush,
		ESlateDrawEffect::None,
		CrosshairColor);

	return PaintLayer + 1;
}

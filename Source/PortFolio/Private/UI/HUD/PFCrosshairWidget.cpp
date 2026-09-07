#include "UI/HUD/PFCrosshairWidget.h"

#include "Engine/Texture2D.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateLayoutTransform.h"
#include "Styling/SlateBrush.h"
#include "UObject/ConstructorHelpers.h"

UPFCrosshairWidget::UPFCrosshairWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UTexture2D> NormalCrosshair(TEXT("/Game/ParagonTwinblast/FX/Textures/Targeting/T_Circular_Reticules_Packed.T_Circular_Reticules_Packed"));
	if (NormalCrosshair.Succeeded())
	{
		NormalCrosshairTexture = NormalCrosshair.Object;
	}

	static ConstructorHelpers::FObjectFinder<UTexture2D> UltimateCrosshair(TEXT("/Game/ParagonTwinblast/FX/Textures/Tech/T_Techy_Holo_Reticle.T_Techy_Holo_Reticle"));
	if (UltimateCrosshair.Succeeded())
	{
		UltimateCrosshairTexture = UltimateCrosshair.Object;
	}
}

// 궁극기 조준점 전환
void UPFCrosshairWidget::SetUltimateCrosshair(bool bUltimate)
{
	bUltimateCrosshair = bUltimate;
	InvalidateLayoutAndVolatility();
}

int32 UPFCrosshairWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 PaintLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	UTexture2D* TargetTexture = bUltimateCrosshair ? UltimateCrosshairTexture : NormalCrosshairTexture;
	if (!TargetTexture)
	{
		return PaintLayer;
	}

	// 화면 중앙에 선택 조준점 그리기
	const FVector2D CrosshairSize = bUltimateCrosshair ? FVector2D(128.f, 64.f) : FVector2D(64.f, 64.f);
	const FVector2D CrosshairPosition = (AllottedGeometry.GetLocalSize() - CrosshairSize) * 0.5f;

	FSlateBrush CrosshairBrush;
	CrosshairBrush.SetResourceObject(TargetTexture);
	CrosshairBrush.DrawAs = ESlateBrushDrawType::Image;
	CrosshairBrush.ImageSize = CrosshairSize;

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		PaintLayer + 1,
		AllottedGeometry.ToPaintGeometry(CrosshairSize, FSlateLayoutTransform(CrosshairPosition)),
		&CrosshairBrush,
		ESlateDrawEffect::None,
		InWidgetStyle.GetColorAndOpacityTint());

	return PaintLayer + 1;
}

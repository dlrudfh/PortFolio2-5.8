#include "UI/Inventory/PFCooldownOverlayWidget.h"

#include "Rendering/DrawElements.h"
#include "Rendering/SlateResourceHandle.h"
#include "Widgets/Images/SImage.h"

namespace
{
	// 사각형 경계를 따라 그리는 쿨타임 마스크
	class SPFCooldownImage : public SImage
	{
	public:
		// 진행률, 마스크 색상 반영
		void SetCooldown(float InProgress, const FLinearColor& InColor)
		{
			if (Progress != InProgress || OverlayColor != InColor)
			{
				Progress = InProgress;
				OverlayColor = InColor;
				Invalidate(EInvalidateWidgetReason::Paint);
			}
		}

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
			FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override
		{
			const FSlateBrush* Brush = GetImageAttribute().Get();
			if (Progress <= 0.f || !Brush || Brush->DrawAs == ESlateBrushDrawType::NoDrawType) return Layer;

			const FLinearColor Tint = OverlayColor * Style.GetColorAndOpacityTint()
				* GetColorAndOpacityAttribute().Get().GetColor(Style) * Brush->GetTint(Style);
			const FColor Color = Tint.ToFColor(true);
			const FGeometry PaintGeometry = bFlipForRightToLeftFlowDirection && GSlateFlowDirection == EFlowDirection::RightToLeft
				? Geometry.MakeChild(FSlateRenderTransform(FScale2D(-1.f, 1.f))) : Geometry;
			const FVector2f Size(PaintGeometry.GetLocalSize());
			TArray<FSlateVertex> Vertices;
			TArray<SlateIndex> Indices;
			Vertices.Reserve(10);
			Indices.Reserve(24);
			const auto AddVertex = [&](FVector2f Point)
			{
				if (Brush->Mirroring == ESlateBrushMirrorType::Horizontal || Brush->Mirroring == ESlateBrushMirrorType::Both) Point.X = 1.f - Point.X;
				if (Brush->Mirroring == ESlateBrushMirrorType::Vertical || Brush->Mirroring == ESlateBrushMirrorType::Both) Point.Y = 1.f - Point.Y;
				Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
					PaintGeometry.GetAccumulatedRenderTransform(), Point * Size, FVector2f::ZeroVector, Color));
			};

			// 현재 경계부터 시계 방향으로 남은 영역 연결
			const FVector2f Center(.5f, .5f);
			const float Sweep = FMath::Clamp((1.f - Progress) * 360.f, 0.f, 359.999f);
			const float Angle = FMath::DegreesToRadians(Sweep - 90.f);
			const FVector2f Direction(FMath::Cos(Angle), FMath::Sin(Angle));
			AddVertex(Center);
			AddVertex(Center + Direction * (.5f / FMath::Max(FMath::Abs(Direction.X), FMath::Abs(Direction.Y))));
			static const FVector2f Boundary[] = {
				{1.f, 0.f}, {1.f, .5f}, {1.f, 1.f}, {.5f, 1.f},
				{0.f, 1.f}, {0.f, .5f}, {0.f, 0.f}, {.5f, 0.f}
			};
			for (int32 Index = FMath::FloorToInt(Sweep / 45.f); Index < UE_ARRAY_COUNT(Boundary); ++Index)
			{
				AddVertex(Boundary[Index]);
				Indices.Add(0);
				Indices.Add(static_cast<SlateIndex>(Vertices.Num() - 2));
				Indices.Add(static_cast<SlateIndex>(Vertices.Num() - 1));
			}
			const ESlateDrawEffect Effect = ShouldBeEnabled(bParentEnabled) ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
			FSlateDrawElement::MakeCustomVerts(Elements, Layer, FSlateResourceHandle(), Vertices, Indices, nullptr, 0, 0, Effect);
			return Layer;
		}

	private:
		float Progress = 0.f;
		FLinearColor OverlayColor = FLinearColor::Transparent;
	};
}

TSharedRef<SWidget> UPFCooldownOverlayWidget::RebuildWidget()
{
	FSlateBrush MaskBrush = GetBrush();
	MaskBrush.SetResourceObject(nullptr);
	MaskBrush.ImageSize = FVector2D(64.f);
	SetBrush(MaskBrush);
	MyImage = SNew(SPFCooldownImage).FlipForRightToLeftFlowDirection(ShouldFlipForRightToLeftFlowDirection());
	UpdateOverlay();
	return MyImage.ToSharedRef();
}

// 쿨타임 비율 반영
void UPFCooldownOverlayWidget::SetCooldownProgress(float InCooldownProgress)
{
	InCooldownProgress = FMath::Clamp(InCooldownProgress, 0.f, 1.f);
	if (CooldownProgress == InCooldownProgress) return;
	CooldownProgress = InCooldownProgress;
	UpdateOverlay();
}

// 마스크 색상 설정
void UPFCooldownOverlayWidget::SetOverlayColor(const FLinearColor& InOverlayColor)
{
	if (OverlayColor == InOverlayColor) return;
	OverlayColor = InOverlayColor;
	UpdateOverlay();
}

void UPFCooldownOverlayWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	UpdateOverlay();
}

// 생성된 Slate 마스크 갱신
void UPFCooldownOverlayWidget::UpdateOverlay()
{
	if (MyImage.IsValid())
	{
		StaticCastSharedPtr<SPFCooldownImage>(MyImage)->SetCooldown(CooldownProgress, OverlayColor);
	}
}

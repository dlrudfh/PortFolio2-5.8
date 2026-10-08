#include "UI/PFGameCursor.h"

#include "Engine/GameViewportClient.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateResourceHandle.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"

namespace
{
	const FLinearColor CursorIvory(0.97f, 0.91f, 0.72f, 1.f);
	const FLinearColor CursorGold(0.78f, 0.49f, 0.13f, 1.f);
	const FLinearColor CursorOutline(0.035f, 0.025f, 0.018f, 1.f);
	constexpr float CursorCanvasSize = 80.f;

	// 중앙 클릭 위치 기준 좌표 변환
	FVector2f CursorPoint(const FGeometry& Geometry, const FVector2f& Point)
	{
		const FVector2f Size(Geometry.GetLocalSize());
		return Size * .5f + Point * (Size.X / CursorCanvasSize);
	}

	// 커서 채움과 외곽선
	int32 DrawCursorShape(const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer,
		const TArray<FVector2f>& Contour, const FVector2f& Center, const FLinearColor& Color = CursorIvory)
	{
		TArray<FSlateVertex> Vertices;
		TArray<SlateIndex> Indices;
		Vertices.Reserve(Contour.Num() + 1);
		Indices.Reserve(Contour.Num() * 3);
		const FColor FillColor = Color.ToFColor(true);
		Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Geometry.GetAccumulatedRenderTransform(),
			CursorPoint(Geometry, Center), FVector2f::ZeroVector, FillColor));
		TArray<FVector2f> Outline;
		Outline.Reserve(Contour.Num() + 1);
		for (int32 Index = 0; Index < Contour.Num(); ++Index)
		{
			const FVector2f Point = CursorPoint(Geometry, Contour[Index]);
			Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Geometry.GetAccumulatedRenderTransform(),
				Point, FVector2f::ZeroVector, FillColor));
			Indices.Add(0);
			Indices.Add(static_cast<SlateIndex>(Index + 1));
			Indices.Add(static_cast<SlateIndex>((Index + 1) % Contour.Num() + 1));
			Outline.Add(Point);
		}
		FSlateDrawElement::MakeCustomVerts(Elements, Layer, FSlateResourceHandle(), Vertices, Indices, nullptr, 0, 0);
		const FVector2f FirstPoint = Outline[0];
		Outline.Add(FirstPoint);
		const float Scale = static_cast<float>(Geometry.GetLocalSize().X) / CursorCanvasSize;
		FSlateDrawElement::MakeLines(Elements, Layer + 1, Geometry.ToPaintGeometry(), Outline,
			ESlateDrawEffect::None, CursorOutline, true, 3.5f * Scale);
		FSlateDrawElement::MakeLines(Elements, Layer + 2, Geometry.ToPaintGeometry(), MoveTemp(Outline),
			ESlateDrawEffect::None, CursorGold, true, 1.2f * Scale);
		return Layer + 2;
	}

	// 커서 내부 금색 장식
	void DrawCursorDetail(const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer,
		TArray<FVector2f> Points)
	{
		for (FVector2f& Point : Points) Point = CursorPoint(Geometry, Point);
		const float Scale = static_cast<float>(Geometry.GetLocalSize().X) / CursorCanvasSize;
		FSlateDrawElement::MakeLines(Elements, Layer, Geometry.ToPaintGeometry(), MoveTemp(Points),
			ESlateDrawEffect::None, CursorGold, true, 1.2f * Scale);
	}

	// 대각선 칼날 커서
	class SPFGameCursor : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SPFGameCursor) {}
		SLATE_END_ARGS()

		// 커서 입력 통과 설정
		void Construct(const FArguments& InArgs)
		{
			SetCanTick(false);
			SetVisibility(EVisibility::HitTestInvisible);
		}

		virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override
		{
			return FVector2D(CursorCanvasSize, CursorCanvasSize);
		}

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& CullingRect,
			FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& WidgetStyle, bool bParentEnabled) const override
		{
			const TArray<FVector2f> Grip = {
				{22.f, 19.f}, {33.f, 30.f}, {30.f, 33.f}, {19.f, 22.f}
			};
			int32 CursorLayer = DrawCursorShape(AllottedGeometry, OutDrawElements, LayerId, Grip, {26.f, 26.f}, CursorOutline) + 1;
			const TArray<FVector2f> Blade = {
				{0.f, 0.f}, {15.f, 8.f}, {24.f, 18.f}, {18.f, 24.f}, {8.f, 15.f}
			};
			CursorLayer = DrawCursorShape(AllottedGeometry, OutDrawElements, CursorLayer, Blade, {14.f, 14.f}) + 1;
			DrawCursorDetail(AllottedGeometry, OutDrawElements, CursorLayer, {{3.f, 3.f}, {20.f, 20.f}});
			const TArray<FVector2f> Guard = {
				{12.f, 27.f}, {13.f, 23.f}, {23.f, 13.f}, {27.f, 12.f}, {26.f, 17.f}, {17.f, 26.f}
			};
			CursorLayer = DrawCursorShape(AllottedGeometry, OutDrawElements, CursorLayer + 1, Guard, {20.f, 20.f}, CursorGold) + 1;
			const TArray<FVector2f> Pommel = {
				{31.f, 28.f}, {36.f, 33.f}, {33.f, 36.f}, {28.f, 31.f}
			};
			CursorLayer = DrawCursorShape(AllottedGeometry, OutDrawElements, CursorLayer, Pommel, {32.f, 32.f}) + 1;
			DrawCursorDetail(AllottedGeometry, OutDrawElements, CursorLayer, {{24.f, 27.f}, {27.f, 24.f}});
			DrawCursorDetail(AllottedGeometry, OutDrawElements, CursorLayer, {{27.f, 30.f}, {30.f, 27.f}});
			return CursorLayer;
		}
	};
}

// 로컬 뷰포트 커서 등록
void FPFGameCursor::Install(UGameViewportClient* Viewport)
{
	if (!Viewport) return;
	const TSharedRef<SWidget> Cursor = SNew(SPFGameCursor);
	Viewport->SetSoftwareCursorWidget(EMouseCursor::Default, Cursor);
	Viewport->SetSoftwareCursorWidget(EMouseCursor::Hand, Cursor);
	Viewport->SetUseSoftwareCursorWidgets(true);
}

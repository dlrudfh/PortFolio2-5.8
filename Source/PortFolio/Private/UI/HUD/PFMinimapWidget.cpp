#include "UI/HUD/PFMinimapWidget.h"
#include "UI/PFWidgetHelpers.h"

#include "UI/HUD/PFMinimapAtlas.h"
#include "Character/PFCharacter.h"
#include "System/Framework/PFPlayerController.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Rendering/DrawElements.h"
#include "Rendering/RenderingCommon.h"
#include "Rendering/SlateLayoutTransform.h"
#include "Rendering/SlateRenderer.h"
#include "Rendering/SlateResourceHandle.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "UObject/ConstructorHelpers.h"

UPFMinimapWidget::UPFMinimapWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FClassFinder<UUserWidget> Layout(TEXT("/Game/GameData/UI/Minimap/WBP_Minimap"));
	LayoutClass = Layout.Class;
	AtlasAsset = TSoftObjectPtr<UPFMinimapAtlas>(FSoftObjectPath(TEXT("/Game/GameData/UI/Minimap/DA_MinimapAtlas.DA_MinimapAtlas")));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Outline(TEXT("/Game/GameData/UI/Minimap/M_MinimapOutline.M_MinimapOutline"));
	MapOutlineMaterial = Outline.Object;
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

TSharedRef<SWidget> UPFMinimapWidget::RebuildWidget()
{
	if (!LayoutWidget && LayoutClass)
	{
		LayoutWidget = CreateWidget<UUserWidget>(this, LayoutClass);
	}
	if (!LayoutWidget) return Super::RebuildWidget();
	MapPanel = LayoutWidget->GetWidgetFromName(TEXT("MinimapPanel"));
	MapSurface = LayoutWidget->GetWidgetFromName(TEXT("MapSurface"));
	MapNameText = PFWidgetHelpers::Find<UTextBlock>(LayoutWidget, TEXT("MapName"));
	if (UWidget* Preview = LayoutWidget->GetWidgetFromName(TEXT("MapPreview"))) Preview->SetVisibility(ESlateVisibility::Collapsed);
	return LayoutWidget->TakeWidget();
}

void UPFMinimapWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	if (bReleaseChildren && LayoutWidget) LayoutWidget->ReleaseSlateResources(true);
}

void UPFMinimapWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (IsDesignTime())
	{
		Atlas = AtlasAsset.LoadSynchronous();
		RefreshMap();
		PlayerPosition = PreviewPosition;
		bHasLocalCharacter = true;
	}
}

void UPFMinimapWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Atlas = AtlasAsset.LoadSynchronous();
	RefreshMap();
	CharacterRefreshRemaining = 0.f;
}

// 현재 레벨의 지도 연결
void UPFMinimapWidget::RefreshMap()
{
	MappedWorld = GetWorld();
	CurrentMap = IsDesignTime() ? PreviewMap : FName(*UGameplayStatics::GetCurrentLevelName(this, true));
	if (MapNameText) MapNameText->SetText(FText::FromName(CurrentMap));
	MapTexture = nullptr;
	if (!Atlas)
	{
		return;
	}
	for (const FPFMinimapEntry& Entry : Atlas->Maps)
	{
		if (Entry.LevelName == CurrentMap && Entry.Texture && Entry.WorldMax.X > Entry.WorldMin.X && Entry.WorldMax.Y > Entry.WorldMin.Y)
		{
			MapTexture = Entry.Texture;
			WorldMin = Entry.WorldMin;
			WorldMax = Entry.WorldMax;
			break;
		}
	}
	if (MapTexture && MapOutlineMaterial)
	{
		if (!MapOutlineInstance)
		{
			MapOutlineInstance = UMaterialInstanceDynamic::Create(MapOutlineMaterial, this);
		}
		if (MapOutlineInstance)
		{
			MapOutlineInstance->SetTextureParameterValue(TEXT("MapTexture"), MapTexture);
		}
	}
}

// 복제된 역할로 플레이어, 봇 목록 갱신
void UPFMinimapWidget::RefreshCharacters()
{
	TrackedCharacters.Reset();
	if (!GetWorld())
	{
		return;
	}
	for (TActorIterator<APFCharacter> It(GetWorld()); It; ++It)
	{
		if (!It->IsDeadCharacter() && (It->IsPlayerCharacter() || It->IsEnemyCharacter()))
		{
			TrackedCharacters.Add(*It);
		}
	}
}

void UPFMinimapWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (IsDesignTime())
	{
		return;
	}
	if (MappedWorld.Get() != GetWorld())
	{
		RefreshMap();
	}
	const APlayerController* Owner = GetOwningPlayer();
	const APFCharacter* Character = Owner ? Cast<APFCharacter>(Owner->GetPawn()) : nullptr;
	bHasLocalCharacter = Owner && Owner->IsLocalController() && IsValid(Character) && !Character->IsDeadCharacter();
	if (MapPanel) MapPanel->SetVisibility(MapTexture && bHasLocalCharacter ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (bHasLocalCharacter)
	{
		PlayerPosition = FVector2D(Character->GetActorLocation());
		CharacterRefreshRemaining -= InDeltaTime;
		if (CharacterRefreshRemaining <= 0.f)
		{
			RefreshCharacters();
			CharacterRefreshRemaining = .25f;
		}
	}
	if (TSharedPtr<SWidget> Widget = GetCachedWidget())
	{
		Widget->Invalidate(EInvalidateWidgetReason::Paint);
	}
}

int32 UPFMinimapWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	if (!MapTexture || !bHasLocalCharacter || !MapSurface)
	{
		return BaseLayer;
	}
	const FGeometry& SurfaceGeometry = MapSurface->GetCachedGeometry();
	const FVector2D MapPosition = AllottedGeometry.AbsoluteToLocal(SurfaceGeometry.LocalToAbsolute(FVector2D::ZeroVector));
	const FVector2D MapSize = AllottedGeometry.AbsoluteToLocal(SurfaceGeometry.LocalToAbsolute(SurfaceGeometry.GetLocalSize())) - MapPosition;
	if (MapSize.X < 16.f || MapSize.Y < 16.f)
	{
		return BaseLayer;
	}
	const FSlateBrush* White = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	const float Opacity = InWidgetStyle.GetColorAndOpacityTint().A;
	const auto Box = [&](int32 Layer, const FVector2D& Pos, const FVector2D& Size, const FLinearColor& Color)
	{
		FSlateDrawElement::MakeBox(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(Pos)), White, ESlateDrawEffect::None, Color);
	};

	// 디자이너 프레임 안의 지도 영역
	Box(BaseLayer + 2, MapPosition, MapSize, FLinearColor(.012f, .017f, .018f, Opacity));

	// 이미지 경계 밖은 빈 공간으로 표시
	const FVector2D Range(ViewRadius * 2.f);
	const FVector2D ViewMin = PlayerPosition - Range * .5;
	const FVector2D ViewMax = PlayerPosition + Range * .5;
	const FVector2D VisibleMin(FMath::Max(ViewMin.X, WorldMin.X), FMath::Max(ViewMin.Y, WorldMin.Y));
	const FVector2D VisibleMax(FMath::Min(ViewMax.X, WorldMax.X), FMath::Min(ViewMax.Y, WorldMax.Y));
	if (VisibleMax.X > VisibleMin.X && VisibleMax.Y > VisibleMin.Y)
	{
		const FVector2D Extent = WorldMax - WorldMin;
		const FVector2D ImagePosition = MapPosition + FVector2D((ViewMax.X - VisibleMax.X) / Range.X * MapSize.X, (ViewMax.Y - VisibleMax.Y) / Range.Y * MapSize.Y);
		const FVector2D ImageSize = (VisibleMax - VisibleMin) / Range * MapSize;
		FSlateBrush Brush;
		Brush.SetResourceObject(MapTexture);
		if (MapOutlineInstance) Brush.SetResourceObject(MapOutlineInstance);
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.Mirroring = ESlateBrushMirrorType::Horizontal;
		Brush.SetUVRegion(FBox2f(FVector2f((VisibleMin.X - WorldMin.X) / Extent.X, (WorldMax.Y - VisibleMax.Y) / Extent.Y), FVector2f((VisibleMax.X - WorldMin.X) / Extent.X, (WorldMax.Y - VisibleMin.Y) / Extent.Y)));
		FSlateDrawElement::MakeBox(OutDrawElements, BaseLayer + 3, AllottedGeometry.ToPaintGeometry(ImageSize, FSlateLayoutTransform(ImagePosition)), &Brush, ESlateDrawEffect::None, FLinearColor(1.f, 1.f, 1.f, Opacity));
	}

	// 주변 캐릭터 위치, 본인 위치
	const FVector2D Center = MapPosition + MapSize * .5;
	const FSlateRoundedBoxBrush DotBrush(FLinearColor::White);
	const auto Dot = [&](int32 Layer, const FVector2D& Point, float Radius, const FLinearColor& Color)
	{
		FSlateDrawElement::MakeBox(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(FVector2D((Radius + 1.5f) * 2.f), FSlateLayoutTransform(Point - FVector2D(Radius + 1.5f))), &DotBrush, ESlateDrawEffect::None, FLinearColor(.005f, .008f, .01f, Opacity));
		FSlateDrawElement::MakeBox(OutDrawElements, Layer + 1, AllottedGeometry.ToPaintGeometry(FVector2D(Radius * 2.f), FSlateLayoutTransform(Point - FVector2D(Radius))), &DotBrush, ESlateDrawEffect::None, Color);
	};
	const APawn* LocalPawn = GetOwningPlayerPawn();
	for (const TWeakObjectPtr<APFCharacter>& Entry : TrackedCharacters)
	{
		const APFCharacter* Character = Entry.Get();
		if (!IsValid(Character) || Character == LocalPawn || Character->IsDeadCharacter()
			|| (!Character->IsPlayerCharacter() && !Character->IsEnemyCharacter()))
		{
			continue;
		}
		const FVector2D Delta = FVector2D(Character->GetActorLocation()) - PlayerPosition;
		if (Delta.SizeSquared() > FMath::Square(ViewRadius))
		{
			continue;
		}
		FVector2D Point = Center - Delta / Range * MapSize;
		Point.X = FMath::Clamp(Point.X, MapPosition.X + 11.0, MapPosition.X + MapSize.X - 11.0);
		Point.Y = FMath::Clamp(Point.Y, MapPosition.Y + 11.0, MapPosition.Y + MapSize.Y - 11.0);
		Dot(BaseLayer + 6, Point, 5.f, Character->IsEnemyCharacter()
			? FLinearColor(.95f, .08f, .06f, Opacity) : FLinearColor(1.f, .8f, .04f, Opacity));
	}

	// 지도 좌표에 맞춘 본인 캐릭터의 정면 화살표
	const FVector Facing = LocalPawn ? FRotator(0.f, LocalPawn->GetActorRotation().Yaw, 0.f).Vector() : FVector(0.f, 1.f, 0.f);
	const FVector2D Forward = (-FVector2D(Facing) / Range * MapSize).GetSafeNormal();
	const FVector2D Right(-Forward.Y, Forward.X);
	const TArray<FVector2f> ArrowOutline = {
		FVector2f(Center + Forward * 14.),
		FVector2f(Center - Forward * 10. + Right * 9.),
		FVector2f(Center - Forward * 4.),
		FVector2f(Center - Forward * 10. - Right * 9.),
		FVector2f(Center + Forward * 14.)
	};
	TArray<FSlateVertex> ArrowVertices;
	ArrowVertices.Reserve(4);
	const FColor ArrowColor = FLinearColor(.08f, .72f, .35f, Opacity).ToFColor(true);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		ArrowVertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
			AllottedGeometry.GetAccumulatedRenderTransform(), ArrowOutline[Index], FVector2f::ZeroVector, ArrowColor));
	}
	const TArray<SlateIndex> ArrowIndices = { 0, 1, 2, 0, 2, 3 };
	FSlateDrawElement::MakeCustomVerts(OutDrawElements, BaseLayer + 8, FSlateResourceHandle(), ArrowVertices, ArrowIndices, nullptr, 0, 0);
	FSlateDrawElement::MakeLines(OutDrawElements, BaseLayer + 9, AllottedGeometry.ToPaintGeometry(), ArrowOutline,
		ESlateDrawEffect::None, FLinearColor(.005f, .008f, .01f, Opacity), true, 1.f);

	FVector Goal;
	const APFPlayerController* Player = Cast<APFPlayerController>(GetOwningPlayer());
	if (Player && Player->GetCampaignGuide(Goal))
	{
		const FVector2D Offset = -(FVector2D(Goal) - PlayerPosition) / Range * MapSize;
		const double Limit = MapSize.X * .5 - 14.;
		const double Scale = FMath::Min(1., Limit / FMath::Max(1., FMath::Max(FMath::Abs(Offset.X), FMath::Abs(Offset.Y))));
		FVector2D Point = Center + Offset * Scale;
		Point.Y = FMath::Max(Point.Y, MapPosition.Y + 14.);
		TArray<FVector2D> Shape;
		if (Scale < 1.)
		{
			const FVector2D Direction = Offset.GetSafeNormal();
			const FVector2D Perpendicular(-Direction.Y, Direction.X);
			Shape = { Point + Direction * 8., Point - Direction * 6. + Perpendicular * 6., Point - Direction * 6. - Perpendicular * 6., Point + Direction * 8. };
		}
		else Shape = { Point + FVector2D(0., -8.), Point + FVector2D(8., 0.), Point + FVector2D(0., 8.), Point + FVector2D(-8., 0.), Point + FVector2D(0., -8.) };
		FSlateDrawElement::MakeLines(OutDrawElements, BaseLayer + 10, AllottedGeometry.ToPaintGeometry(), Shape,
			ESlateDrawEffect::None, FLinearColor(.005f, .008f, .01f, Opacity), true, 5.f);
		FSlateDrawElement::MakeLines(OutDrawElements, BaseLayer + 11, AllottedGeometry.ToPaintGeometry(), Shape,
			ESlateDrawEffect::None, FLinearColor(.2f, .95f, 1.f, Opacity), true, 3.f);
	}
	return BaseLayer + 11;
}

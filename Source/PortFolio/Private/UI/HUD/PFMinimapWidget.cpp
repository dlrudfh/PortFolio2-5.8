#include "UI/HUD/PFMinimapWidget.h"

#include "UI/HUD/PFMinimapAtlas.h"
#include "Character/PFCharacter.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateLayoutTransform.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"

UPFMinimapWidget::UPFMinimapWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AtlasAsset = TSoftObjectPtr<UPFMinimapAtlas>(FSoftObjectPath(TEXT("/Game/GameData/UI/Minimap/DA_MinimapAtlas.DA_MinimapAtlas")));
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UPFMinimapWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (IsDesignTime())
	{
		Atlas = AtlasAsset.LoadSynchronous();
		RefreshMap();
		PlayerPosition = PreviewPosition;
		PlayerYaw = PreviewYaw;
		bHasLocalCharacter = true;
	}
}

void UPFMinimapWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Atlas = AtlasAsset.LoadSynchronous();
	RefreshMap();
}

// 현재 레벨의 지도 연결
void UPFMinimapWidget::RefreshMap()
{
	CurrentMap = IsDesignTime() ? PreviewMap : FName(*UGameplayStatics::GetCurrentLevelName(this, true));
	MapTexture = nullptr;
	if (!Atlas)
	{
		return;
	}
	const FString LevelName = CurrentMap.ToString();
	if (LevelName.Len() != 4 || !LevelName.StartsWith(TEXT("Map")) || LevelName[3] < TEXT('2') || LevelName[3] > TEXT('8'))
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
}

void UPFMinimapWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (IsDesignTime())
	{
		return;
	}
	if (CurrentMap != FName(*UGameplayStatics::GetCurrentLevelName(this, true)))
	{
		RefreshMap();
	}
	const APlayerController* Owner = GetOwningPlayer();
	const APFCharacter* Character = Owner ? Cast<APFCharacter>(Owner->GetPawn()) : nullptr;
	bHasLocalCharacter = Owner && Owner->IsLocalController() && IsValid(Character) && !Character->IsDeadCharacter();
	if (bHasLocalCharacter)
	{
		PlayerPosition = FVector2D(Character->GetActorLocation());
		PlayerYaw = Character->GetActorRotation().Yaw;
	}
	InvalidateLayoutAndVolatility();
}

int32 UPFMinimapWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	if (!MapTexture || !bHasLocalCharacter)
	{
		return BaseLayer;
	}
	const FVector2D Screen = AllottedGeometry.GetLocalSize();
	const float Side = static_cast<float>(FMath::Min(264.0, FMath::Min(Screen.X * .32, Screen.Y * .4)));
	if (Side < 96.f)
	{
		return BaseLayer;
	}
	const FVector2D Position(Screen.X - Side - 28.f, 28.f);
	const FVector2D MapPosition = Position + FVector2D(10.f, 30.f);
	const FVector2D MapSize(Side - 20.f, Side - 40.f);
	const FSlateBrush* White = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	const float Opacity = InWidgetStyle.GetColorAndOpacityTint().A;
	const FLinearColor Gold(.64f, .46f, .20f, Opacity);
	const auto Box = [&](int32 Layer, const FVector2D& Pos, const FVector2D& Size, const FLinearColor& Color)
	{
		FSlateDrawElement::MakeBox(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(Pos)), White, ESlateDrawEffect::None, Color);
	};

	// 석재 테두리, 금색 장식
	Box(BaseLayer + 1, Position, FVector2D(Side), FLinearColor(.025f, .03f, .033f, Opacity * .97f));
	Box(BaseLayer + 2, Position + FVector2D(3.f), FVector2D(Side - 6.f), Gold);
	Box(BaseLayer + 3, Position + FVector2D(5.f), FVector2D(Side - 10.f), FLinearColor(.065f, .07f, .073f, Opacity));
	Box(BaseLayer + 4, MapPosition, MapSize, FLinearColor(.016f, .022f, .027f, Opacity));
	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 12);
	FSlateDrawElement::MakeText(OutDrawElements, BaseLayer + 5, AllottedGeometry.ToPaintGeometry(FVector2D(50.f, 20.f), FSlateLayoutTransform(Position + FVector2D(Side * .5f - 5.f, 8.f))), TEXT("N"), Font, ESlateDrawEffect::None, Gold);

	// 이미지 경계 밖은 빈 공간으로 표시
	const double MapRange = FMath::Clamp(FMath::Max(WorldMax.X - WorldMin.X, WorldMax.Y - WorldMin.Y) * 1.4, 5000.0, static_cast<double>(ViewRange));
	const FVector2D Range(MapRange, MapRange * MapSize.Y / MapSize.X);
	const FVector2D ViewMin = PlayerPosition - Range * .5;
	const FVector2D ViewMax = PlayerPosition + Range * .5;
	const FVector2D VisibleMin(FMath::Max(ViewMin.X, WorldMin.X), FMath::Max(ViewMin.Y, WorldMin.Y));
	const FVector2D VisibleMax(FMath::Min(ViewMax.X, WorldMax.X), FMath::Min(ViewMax.Y, WorldMax.Y));
	if (VisibleMax.X > VisibleMin.X && VisibleMax.Y > VisibleMin.Y)
	{
		const FVector2D Extent = WorldMax - WorldMin;
		const FVector2D ImagePosition = MapPosition + FVector2D((VisibleMin.X - ViewMin.X) / Range.X * MapSize.X, (ViewMax.Y - VisibleMax.Y) / Range.Y * MapSize.Y);
		const FVector2D ImageSize = (VisibleMax - VisibleMin) / Range * MapSize;
		FSlateBrush Brush;
		Brush.SetResourceObject(MapTexture);
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.SetUVRegion(FBox2f(FVector2f((VisibleMin.X - WorldMin.X) / Extent.X, (WorldMax.Y - VisibleMax.Y) / Extent.Y), FVector2f((VisibleMax.X - WorldMin.X) / Extent.X, (WorldMax.Y - VisibleMin.Y) / Extent.Y)));
		FSlateDrawElement::MakeBox(OutDrawElements, BaseLayer + 5, AllottedGeometry.ToPaintGeometry(ImageSize, FSlateLayoutTransform(ImagePosition)), &Brush, ESlateDrawEffect::None, FLinearColor(1.f, 1.f, 1.f, Opacity));
	}

	// 소유 컨트롤러의 현재 캐릭터 방향
	const FVector2D Center = MapPosition + MapSize * .5;
	const float Yaw = FMath::DegreesToRadians(PlayerYaw);
	const FVector2D Forward(FMath::Cos(Yaw), -FMath::Sin(Yaw));
	const FVector2D Right(-Forward.Y, Forward.X);
	const TArray<FVector2D> Arrow = { Center + Forward * 10.f, Center - Forward * 7.f + Right * 6.f, Center - Forward * 3.f, Center - Forward * 7.f - Right * 6.f, Center + Forward * 10.f };
	FSlateDrawElement::MakeLines(OutDrawElements, BaseLayer + 6, AllottedGeometry.ToPaintGeometry(), Arrow, ESlateDrawEffect::None, FLinearColor(.01f, .015f, .02f, Opacity), true, 5.f);
	FSlateDrawElement::MakeLines(OutDrawElements, BaseLayer + 7, AllottedGeometry.ToPaintGeometry(), Arrow, ESlateDrawEffect::None, FLinearColor(.98f, .78f, .30f, Opacity), true, 2.f);
	return BaseLayer + 7;
}

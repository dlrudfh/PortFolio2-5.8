#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PFMinimapWidget.generated.h"

class UPFMinimapAtlas;
class UTexture2D;

// 북쪽 고정 로컬 캐릭터 미니맵
UCLASS()
class PORTFOLIO_API UPFMinimapWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPFMinimapWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	void RefreshMap();

	// 맵별 지형 이미지, 월드 좌표 범위
	UPROPERTY(EditDefaultsOnly, Category = Minimap)
	TSoftObjectPtr<UPFMinimapAtlas> AtlasAsset;

	UPROPERTY(EditAnywhere, Category = "Minimap|Preview", meta = (DesignerRebuild))
	FName PreviewMap = TEXT("Map3");

	UPROPERTY(EditAnywhere, Category = "Minimap|Preview", meta = (DesignerRebuild))
	FVector2D PreviewPosition = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Minimap|Preview", meta = (DesignerRebuild))
	float PreviewYaw = 90.f;

	UPROPERTY(Transient)
	TObjectPtr<UPFMinimapAtlas> Atlas;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> MapTexture;

	FName CurrentMap;
	FVector2D WorldMin = FVector2D::ZeroVector;
	FVector2D WorldMax = FVector2D::ZeroVector;
	FVector2D PlayerPosition = FVector2D::ZeroVector;
	float PlayerYaw = 0.f;
	bool bHasLocalCharacter = false;
	static constexpr float ViewRange = 25000.f;
};

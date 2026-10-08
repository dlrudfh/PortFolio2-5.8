#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PFMinimapWidget.generated.h"

class UPFMinimapAtlas;
class UTexture2D;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class APFCharacter;

// 북쪽 고정 로컬 캐릭터 미니맵
UCLASS()
class PORTFOLIO_API UPFMinimapWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPFMinimapWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	void RefreshMap();
	void RefreshCharacters();

	// 디자이너 미니맵 에셋
	UPROPERTY()
	TSubclassOf<UUserWidget> LayoutClass;
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> LayoutWidget;
	UPROPERTY(Transient)
	TObjectPtr<UWidget> MapPanel;
	UPROPERTY(Transient)
	TObjectPtr<UWidget> MapSurface;
	UPROPERTY(Transient)
	TObjectPtr<class UTextBlock> MapNameText;

	// 맵별 지형 이미지, 월드 좌표 범위
	UPROPERTY(EditDefaultsOnly, Category = Minimap)
	TSoftObjectPtr<UPFMinimapAtlas> AtlasAsset;

	UPROPERTY(EditAnywhere, Category = "Minimap|Preview", meta = (DesignerRebuild))
	FName PreviewMap = TEXT("Map3");

	UPROPERTY(EditAnywhere, Category = "Minimap|Preview", meta = (DesignerRebuild))
	FVector2D PreviewPosition = FVector2D::ZeroVector;

	UPROPERTY(Transient)
	TObjectPtr<UPFMinimapAtlas> Atlas;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> MapTexture;

	// 지도 윤곽선 재질
	UPROPERTY()
	TObjectPtr<UMaterialInterface> MapOutlineMaterial;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MapOutlineInstance;

	FName CurrentMap;
	TWeakObjectPtr<UWorld> MappedWorld;
	FVector2D WorldMin = FVector2D::ZeroVector;
	FVector2D WorldMax = FVector2D::ZeroVector;
	FVector2D PlayerPosition = FVector2D::ZeroVector;
	TArray<TWeakObjectPtr<APFCharacter>> TrackedCharacters;
	float CharacterRefreshRemaining = 0.f;
	bool bHasLocalCharacter = false;
	static constexpr float ViewRadius = 3000.f;
};

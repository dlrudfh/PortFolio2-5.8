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
	virtual void SynchronizeProperties() override;

private:
	void EnsureMaskTexture();

	void UpdateMaskTexture();

	TArray<FVector2D> BuildCooldownBoundaryPath() const;

	bool IsPointInsideCooldownMask(const FVector2D& NormalizedPoint, const TArray<FVector2D>& BoundaryPath) const;

	bool IsPointInTriangle(const FVector2D& Point, const FVector2D& A, const FVector2D& B, const FVector2D& C) const;

private:
	static constexpr int32 MaskTextureSize = 64;

	// 쿨타임 표시용 동적 텍스처
	UPROPERTY(Transient)
	class UTexture2D* MaskTexture = nullptr;

	UPROPERTY()
	float CooldownProgress = 0.f;

	UPROPERTY()
	FLinearColor OverlayColor = FLinearColor(0.f, 0.f, 0.f, 0.70f);

	// 마스크 픽셀 버퍼
	TArray<FColor> PixelBuffer;
};

#include "UI/Inventory/PFCooldownOverlayWidget.h"

#include "Engine/Texture2D.h"

namespace
{
	const FVector2D CooldownMaskCenter(0.5f, 0.5f);
	const FVector2D CooldownMaskStartPoint(0.5f, 0.f);

	// 시계 방향 마스크 경계점
	const TArray<FVector2D> CooldownBoundaryEndpoints =
	{
		FVector2D(1.f, 0.f),
		FVector2D(1.f, 0.5f),
		FVector2D(1.f, 1.f),
		FVector2D(0.5f, 1.f),
		FVector2D(0.f, 1.f),
		FVector2D(0.f, 0.5f),
		FVector2D(0.f, 0.f),
		FVector2D(0.5f, 0.f)
	};
}

// 쿨타임 비율 반영
void UPFCooldownOverlayWidget::SetCooldownProgress(float InCooldownProgress)
{
	InCooldownProgress = FMath::Clamp(InCooldownProgress, 0.f, 1.f);
	if (MaskTexture && CooldownProgress == InCooldownProgress)
	{
		return;
	}

	CooldownProgress = InCooldownProgress;

	UpdateMaskTexture();
}

// 마스크 색상 설정
void UPFCooldownOverlayWidget::SetOverlayColor(const FLinearColor& InOverlayColor)
{
	if (MaskTexture && OverlayColor == InOverlayColor)
	{
		return;
	}

	OverlayColor = InOverlayColor;

	UpdateMaskTexture();
}

void UPFCooldownOverlayWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	EnsureMaskTexture();
	UpdateMaskTexture();
}

// 쿨타임 마스크 텍스처 준비
void UPFCooldownOverlayWidget::EnsureMaskTexture()
{
	if (MaskTexture)
	{
		return;
	}

	// 동적 텍스처 생성, 브러시 연결
	MaskTexture = UTexture2D::CreateTransient(MaskTextureSize, MaskTextureSize, PF_B8G8R8A8);
	if (!MaskTexture)
	{
		return;
	}

	MaskTexture->SRGB = true;
	MaskTexture->Filter = TF_Bilinear;
	MaskTexture->AddressX = TA_Clamp;
	MaskTexture->AddressY = TA_Clamp;
	MaskTexture->UpdateResource();

	PixelBuffer.Init(FColor::Transparent, MaskTextureSize * MaskTextureSize);
	SetBrushFromTexture(MaskTexture, true);
	SetColorAndOpacity(FLinearColor::White);
}

// 쿨타임 마스크 픽셀 갱신
void UPFCooldownOverlayWidget::UpdateMaskTexture()
{
	EnsureMaskTexture();
	if (!MaskTexture)
	{
		return;
	}

	if (PixelBuffer.Num() != MaskTextureSize * MaskTextureSize)
	{
		PixelBuffer.Init(FColor::Transparent, MaskTextureSize * MaskTextureSize);
	}

	// 버퍼 초기화, 남은 구간 채우기
	for (FColor& Pixel : PixelBuffer)
	{
		Pixel = FColor::Transparent;
	}

	if (CooldownProgress > 0.f)
	{
		const TArray<FVector2D> BoundaryPath = BuildCooldownBoundaryPath();
		const FColor FillColor = OverlayColor.ToFColor(true);

		for (int32 Y = 0; Y < MaskTextureSize; ++Y)
		{
			for (int32 X = 0; X < MaskTextureSize; ++X)
			{
				const FVector2D NormalizedPoint(
					(static_cast<float>(X) + 0.5f) / static_cast<float>(MaskTextureSize),
					(static_cast<float>(Y) + 0.5f) / static_cast<float>(MaskTextureSize));

				if (IsPointInsideCooldownMask(NormalizedPoint, BoundaryPath))
				{
					PixelBuffer[(Y * MaskTextureSize) + X] = FillColor;
				}
			}
		}
	}

	// 픽셀 업로드 후 임시 메모리 해제
	static const FUpdateTextureRegion2D FullRegion(0, 0, 0, 0, MaskTextureSize, MaskTextureSize);
	const int32 DataSize = PixelBuffer.Num() * sizeof(FColor);
	uint8* RawData = new uint8[DataSize];
	FMemory::Memcpy(RawData, PixelBuffer.GetData(), DataSize);

	MaskTexture->UpdateTextureRegions(
		0,
		1,
		&FullRegion,
		MaskTextureSize * sizeof(FColor),
		sizeof(FColor),
		RawData,
		[](uint8* SrcData, const FUpdateTextureRegion2D*)
		{
			delete[] SrcData;
		});
}

// 남은 쿨타임의 경계 경로 계산
TArray<FVector2D> UPFCooldownOverlayWidget::BuildCooldownBoundaryPath() const
{
	TArray<FVector2D> BoundaryPath;
	BoundaryPath.Reserve(10);

	if (CooldownProgress <= 0.f)
	{
		return BoundaryPath;
	}

	// 진행 각도를 사각형 경계 좌표로 변환
	const float ElapsedAlpha = 1.f - CooldownProgress;
	const float SweepDegrees = FMath::Clamp(ElapsedAlpha * 360.f, 0.f, 359.999f);
	const float AngleRadians = FMath::DegreesToRadians(-90.f + SweepDegrees);
	const FVector2D Direction(FMath::Cos(AngleRadians), FMath::Sin(AngleRadians));
	const float DirectionScale = 0.5f / FMath::Max(FMath::Abs(Direction.X), FMath::Abs(Direction.Y));
	const FVector2D CurrentPoint = CooldownMaskCenter + (Direction * DirectionScale);
	const int32 SegmentIndex = FMath::Clamp(FMath::FloorToInt(SweepDegrees / 45.f), 0, CooldownBoundaryEndpoints.Num() - 1);

	// 현재 경계부터 남은 외곽 경로 구성
	BoundaryPath.Add(CurrentPoint);

	for (int32 EndpointIndex = SegmentIndex; EndpointIndex < CooldownBoundaryEndpoints.Num(); ++EndpointIndex)
	{
		if (BoundaryPath.Last().Equals(CooldownBoundaryEndpoints[EndpointIndex], KINDA_SMALL_NUMBER))
		{
			continue;
		}

		BoundaryPath.Add(CooldownBoundaryEndpoints[EndpointIndex]);
	}

	if (BoundaryPath.Num() == 1 || !BoundaryPath.Last().Equals(CooldownMaskStartPoint, KINDA_SMALL_NUMBER))
	{
		BoundaryPath.Add(CooldownMaskStartPoint);
	}

	return BoundaryPath;
}

// 마스크 내부 좌표 판정
bool UPFCooldownOverlayWidget::IsPointInsideCooldownMask(const FVector2D& NormalizedPoint, const TArray<FVector2D>& BoundaryPath) const
{
	if (BoundaryPath.Num() < 2)
	{
		return false;
	}

	for (int32 PathIndex = 0; PathIndex < BoundaryPath.Num() - 1; ++PathIndex)
	{
		if (IsPointInTriangle(NormalizedPoint, CooldownMaskCenter, BoundaryPath[PathIndex], BoundaryPath[PathIndex + 1]))
		{
			return true;
		}
	}

	return false;
}

// 삼각형 내부 좌표 판정
bool UPFCooldownOverlayWidget::IsPointInTriangle(const FVector2D& Point, const FVector2D& A, const FVector2D& B, const FVector2D& C) const
{
	const float Denominator = ((B.Y - C.Y) * (A.X - C.X)) + ((C.X - B.X) * (A.Y - C.Y));
	if (FMath::IsNearlyZero(Denominator))
	{
		return false;
	}

	const float Alpha = (((B.Y - C.Y) * (Point.X - C.X)) + ((C.X - B.X) * (Point.Y - C.Y))) / Denominator;
	const float Beta = (((C.Y - A.Y) * (Point.X - C.X)) + ((A.X - C.X) * (Point.Y - C.Y))) / Denominator;
	const float Gamma = 1.f - Alpha - Beta;
	const float Epsilon = -0.0001f;

	return Alpha >= Epsilon && Beta >= Epsilon && Gamma >= Epsilon;
}

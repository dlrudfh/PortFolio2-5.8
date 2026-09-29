#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PFMinimapAtlas.generated.h"

class UTexture2D;

USTRUCT(BlueprintType)
struct FPFMinimapEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName LevelName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UTexture2D> Texture = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector2D WorldMin = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector2D WorldMax = FVector2D::ZeroVector;
};

// 에디터 지형에서 추출한 정적 지도
UCLASS(BlueprintType)
class PORTFOLIO_API UPFMinimapAtlas : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FPFMinimapEntry> Maps;
};

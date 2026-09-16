#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PFCombatAimProvider.generated.h"

// 컨트롤러의 전투 조준점 제공
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UPFCombatAimProvider : public UInterface
{
	GENERATED_BODY()
};

// 발사 시점의 조준점 조회
class PORTFOLIO_API IPFCombatAimProvider
{
	GENERATED_BODY()

public:
	virtual bool TryGetCombatAim(FVector& OutAimPoint) = 0;
};

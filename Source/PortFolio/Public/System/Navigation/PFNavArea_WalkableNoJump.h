#pragma once

#include "CoreMinimal.h"
#include "NavAreas/NavArea_Default.h"
#include "PFNavArea_WalkableNoJump.generated.h"

// 자동 점프, 낙하 링크를 만들지 않는 보행 표면
UCLASS()
class PORTFOLIO_API UPFNavArea_WalkableNoJump : public UNavArea_Default
{
	GENERATED_BODY()
};

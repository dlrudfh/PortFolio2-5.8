#pragma once

#include "CoreMinimal.h"

class UGameViewportClient;

// 게임용 소프트웨어 커서
class PORTFOLIO_API FPFGameCursor
{
public:
	static void Install(UGameViewportClient* Viewport);
};

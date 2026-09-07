#pragma once
#include "PortFolio/PortFolio.h"

#include "UObject/Interface.h"

#include "PFPoolable.generated.h"

// 풀링 인터페이스 타입
UINTERFACE(MinimalAPI)
class UPFPoolable : public UInterface
{
	GENERATED_BODY()
};

// 풀링 동작 인터페이스
class PORTFOLIO_API IPFPoolable
{
	GENERATED_BODY()

public:
	virtual void SpawnFromPool() = 0;
	virtual void ReturnToPool() = 0;

	DECLARE_MULTICAST_DELEGATE_OneParam(FReturnToPoolDelegate, class AActor*);
	virtual FReturnToPoolDelegate& GetReturnDelegate() = 0;
};

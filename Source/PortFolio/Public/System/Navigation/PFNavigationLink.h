#pragma once

#include "CoreMinimal.h"
#include "Navigation/NavLinkProxy.h"
#include "NavLinkCustomComponent.h"
#include "PFNavigationLink.generated.h"

UENUM()
enum class EPFNavigationTraversal : uint8
{
	Jump,
	Drop
};

// 프로젝트 이동 링크의 실행 연결
UCLASS()
class PORTFOLIO_API UPFNavigationLinkComponent : public UNavLinkCustomComponent
{
	GENERATED_BODY()

public:
	virtual bool IsLinkPathfindingAllowed(const UObject* Querier) const override;
	virtual void OnLinkMoveFinished(UObject* PathComp) override;

protected:
	virtual void OnRegister() override;

private:
	void HandleMoveReached(UNavLinkCustomComponent* Link, UObject* PathComp, const FVector& Destination);
};

// 에디터에서 생성, 저장하는 단방향 이동 링크
UCLASS(NotBlueprintable)
class PORTFOLIO_API APFNavigationLink : public ANavLinkProxy
{
	GENERATED_BODY()

public:
	APFNavigationLink(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	void Configure(const FGuid& OwnerId, const FString& Key, const FVector& Start, const FVector& End,
		EPFNavigationTraversal Mode);
	EPFNavigationTraversal GetTraversal() const { return Traversal; }
	const FVector& GetStartFeet() const { return StartFeet; }
	const FVector& GetEndFeet() const { return EndFeet; }
	const FGuid& GetGeneratorId() const { return GeneratorId; }
	const FString& GetGenerationKey() const { return GenerationKey; }

private:
	UPROPERTY(VisibleAnywhere, Category = Navigation)
	EPFNavigationTraversal Traversal = EPFNavigationTraversal::Jump;

	UPROPERTY()
	FVector StartFeet = FVector::ZeroVector;

	UPROPERTY()
	FVector EndFeet = FVector::ZeroVector;

	UPROPERTY()
	FGuid GeneratorId;

	UPROPERTY()
	FString GenerationKey;
};

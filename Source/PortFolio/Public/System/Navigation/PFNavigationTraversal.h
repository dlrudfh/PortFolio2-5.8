#pragma once

#include "CoreMinimal.h"
#include "CollisionQueryParams.h"
#include "Engine/EngineTypes.h"

class ACharacter;
class UWorld;

struct FPFTraversalSettings
{
	float Radius = 0.f;
	float HalfHeight = 0.f;
	float MaxSpeed = 0.f;
	float JumpSpeed = 0.f;
	float Gravity = 0.f;
	float TerminalSpeed = 4000.f;
	float StepHeight = 0.f;
	float WalkableFloorZ = 0.f;
	bool bCanWalkOffLedges = false;
	ECollisionChannel Channel = ECC_Pawn;
	FCollisionResponseParams Responses;
	const AActor* IgnoredActor = nullptr;
};

struct FPFTraversalSolution
{
	FVector Velocity = FVector::ZeroVector;
	float Time = 0.f;
};

// 생성, 실행이 공유하는 이동 검증
namespace PFNavigationTraversal
{
	inline constexpr float FloorClearance = 2.f;
	PORTFOLIO_API FPFTraversalSettings GetSettings(const ACharacter& Character, const UWorld& World);
	PORTFOLIO_API bool Solve(const FVector& Start, const FVector& End, const FPFTraversalSettings& Settings,
		float VerticalSpeed, FPFTraversalSolution& OutSolution);
	PORTFOLIO_API FVector Evaluate(const FVector& Start, const FPFTraversalSolution& Solution,
		const FPFTraversalSettings& Settings, float Time);
	PORTFOLIO_API bool FindFloor(UWorld& World, const FVector& Point, const FPFTraversalSettings& Settings,
		float SearchHeight, FHitResult& OutHit);
	PORTFOLIO_API bool HasClearance(UWorld& World, const FVector& Feet, const FPFTraversalSettings& Settings);
	PORTFOLIO_API bool ValidateJump(UWorld& World, const FVector& Start, const FVector& End,
		const FPFTraversalSettings& Settings, FPFTraversalSolution& OutSolution);
	PORTFOLIO_API FVector GetDropVelocity(const FVector& Feet, const FVector& Destination, float MaxSpeed, float DeltaTime);
	PORTFOLIO_API bool ValidateDrop(UWorld& World, const FVector& Start, const FVector& End,
		const FPFTraversalSettings& Settings, float& OutDuration);
}

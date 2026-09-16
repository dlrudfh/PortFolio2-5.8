#pragma once

#include "PortFolio/PortFolio.h"
#include "PFCharacterControlTypes.generated.h"

UENUM(BlueprintType)
enum class EPFCharacterRole : uint8
{
	PLAYER,
	ENEMY,
	ROLE_END
};

USTRUCT(BlueprintType)
struct FPFCharacterAISettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "AI")
	float DesiredCombatDistance = 0.f;
	UPROPERTY(EditAnywhere, Category = "AI")
	float DistanceTolerance = 0.f;
	UPROPERTY(EditAnywhere, Category = "AI")
	float AttackRange = 100.f;
	UPROPERTY(EditAnywhere, Category = "AI")
	bool bRetreatWhenTooClose = false;
	UPROPERTY(EditAnywhere, Category = "AI")
	bool bRequiresLineOfSight = false;
};

USTRUCT()
struct FPFCharacterSharedStateSnapshot
{
	GENERATED_BODY()

	UPROPERTY()
	ECONTROLMODE ControlMode = TPS;
	UPROPERTY()
	bool bViewpointFixed = true;
	UPROPERTY()
	EPFDirection FinalDirection = IDLE;
	UPROPERTY()
	float SpringArmLength = 0.f;
	UPROPERTY()
	FVector SpringArmRelativeLocation = FVector::ZeroVector;
	UPROPERTY()
	FRotator SpringArmRelativeRotation = FRotator::ZeroRotator;
	UPROPERTY()
	float DesiredSpringArmLength = 0.f;
	UPROPERTY()
	FRotator DesiredSpringArmRotation = FRotator::ZeroRotator;
	UPROPERTY()
	float CameraFieldOfView = 90.f;
};

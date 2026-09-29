#pragma once

#include "PortFolio/PortFolio.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PFCharacterMovementComponent.generated.h"

// 카메라 회전과 이동 방향 회전 분리
UCLASS()
class PORTFOLIO_API UPFCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual void OnTeleported() override;

protected:
	virtual void PhysicsRotation(float DeltaTime) override;

private:
	friend class FSavedMove_PFCharacter;
	void GetRotationTarget(EPFDirection Direction, bool& bUseRelativeRotation, bool& bMovementRotation, float& TargetYaw) const;

	float CameraRelativeYaw = 0.f;
	bool bHasCameraRelativeYaw = false;
	EPFDirection RotationInputDirection = IDLE;
	TWeakObjectPtr<AController> RotationController;
};

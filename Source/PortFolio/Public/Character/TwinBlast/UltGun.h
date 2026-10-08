#pragma once

#include "PortFolio/PortFolio.h"
#include "GameFramework/Actor.h"
#include "Components/SkeletalMeshComponent.h"
#include "UltGun.generated.h"

// 궁극기 총 클래스
UCLASS(meta=(PrioritizeCategories="Mesh"))
class PORTFOLIO_API AUltGun : public AActor
{
	GENERATED_BODY()

public:
	AUltGun();

public:
	virtual void PostInitializeComponents() override;
	USkeletalMeshComponent* GetMesh();
	void PlayMontage(int NextIdx);
	void SetCurrentDir(EPFDirection Dir);
	void SetFirstPersonPresentation(USkeletalMeshComponent* FirstPersonParent);

protected:
	UFUNCTION()
	virtual void OnMontageEnd(UAnimMontage* Montage, bool bInterrupted);
	

protected:
	// 궁극기 총 메시
	UPROPERTY(VisibleAnywhere, Category = Mesh)
	USkeletalMeshComponent* MeshCom;

	// 소유 플레이어의 궁극기 총 표시
	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;
	bool bFirstPersonActive = false;
	EVisibilityBasedAnimTickOption SavedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPose;

	// 총 애니메이션 인스턴스
	UPROPERTY()
	class UPFAnimInstance* PFAnim;
};

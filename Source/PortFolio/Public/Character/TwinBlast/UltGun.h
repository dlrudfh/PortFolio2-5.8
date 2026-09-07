#pragma once

#include "PortFolio/PortFolio.h"
#include "GameFramework/Actor.h"
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
	void AttachToParent(USkeletalMeshComponent* ParentMesh, FName SocketName);
	void PlayMontage(int NextIdx);
	void SetCurrentDir(EPFDirection Dir);

protected:
	UFUNCTION()
	virtual void OnMontageEnd(UAnimMontage* Montage, bool bInterrupted);
	

protected:
	// 궁극기 총 메시
	UPROPERTY(VisibleAnywhere, Category = Mesh)
	USkeletalMeshComponent* MeshCom;

	// 총 애니메이션 인스턴스
	UPROPERTY()
	class UPFAnimInstance* PFAnim;
};

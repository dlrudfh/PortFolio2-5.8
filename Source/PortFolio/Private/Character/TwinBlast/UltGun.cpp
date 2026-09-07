

#include "Character/TwinBlast/UltGun.h"
#include "Particles/ParticleSystem.h"
#include "Animation/PFAnimInst_UltGun.h"
#include "Animation/PFAnimInst_TwinBlast.h"

using enum UPFAnimInst_UltGun::MTGIDX_UG;

AUltGun::AUltGun() : MeshCom(nullptr), PFAnim(nullptr)
{
	PrimaryActorTick.bCanEverTick = true;

	MeshCom = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MeshCom"));
	RootComponent = MeshCom;
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> ULTGUNMESH(TEXT("/Game/ParagonTwinblast/Characters/Heroes/TwinBlast/Meshes/TwinBlast_UltGun.TwinBlast_UltGun"));
	if (ULTGUNMESH.Succeeded())
	{
		MeshCom->SetSkeletalMesh(ULTGUNMESH.Object);
	}
	else
	{
		PFLOG(Warning, TEXT("UltGun Mesh Failed"));
	}
	SetActorScale3D(FVector(1.f, 1.f, 1.f));
	MeshCom->SetAnimationMode(EAnimationMode::AnimationBlueprint);

	static ConstructorHelpers::FClassFinder<UPFAnimInst_UltGun> ULTGUN_BLUEPRINT(TEXT("/Game/ParagonTwinblast/Characters/Heroes/TwinBlast/UltGun_Blueprint.UltGun_Blueprint_C"));
	if (ULTGUN_BLUEPRINT.Succeeded())
	{
		PFLOG(Warning, TEXT("UltGun BluePrint Succeed"));
		MeshCom->SetAnimInstanceClass(ULTGUN_BLUEPRINT.Class);
	}
	else
	{
		PFLOG(Warning, TEXT("UltGun BluePrint Failed"));
	}
}

// 종료된 총 몽타주 반영
void AUltGun::OnMontageEnd(UAnimMontage* Montage, bool bInterrupted)
{
	int MontageIdx = PFAnim->MontageEndTask(Montage);
}

void AUltGun::PostInitializeComponents()
{
	PFAnim = Cast<UPFAnimInst_UltGun>(MeshCom->GetAnimInstance());
	PFCHECK(nullptr != PFAnim);
	Super::PostInitializeComponents();
	if (!PFAnim)
	{
		PFLOG(Fatal, TEXT("UltGun AnimInst Failed"));
	}

	// 몽타주 종료 연결, 총 표시 초기화
	PFAnim->OnMontageEnded.AddDynamic(this, &AUltGun::OnMontageEnd);
	MeshCom->SetCollisionProfileName(TEXT("NoCollision"));
	MeshCom->SetVisibility(false);
}

// 총 메시 반환
USkeletalMeshComponent* AUltGun::GetMesh()
{
	return MeshCom;
}

// 부모 메시 소켓에 부착
void AUltGun::AttachToParent(USkeletalMeshComponent* ParentMesh, FName SocketName)
{
	MeshCom->AttachToComponent(ParentMesh, FAttachmentTransformRules::SnapToTargetIncludingScale, SocketName);
}

// 궁극기 총 몽타주 재생
void AUltGun::PlayMontage(int NextIdx)
{
	// 캐릭터 몽타주 번호를 총 몽타주 번호로 변환
	int32 UltGunMontageIndex = INDEX_NONE;

	if (NextIdx == etoi(UPFAnimInst_TwinBlast::MTGIDX_TB::ULTSTART))
	{
		UltGunMontageIndex = etoi(UPFAnimInst_UltGun::MTGIDX_UG::ULTSTART);
		MeshCom->SetVisibility(true);
	}
	else if (NextIdx == etoi(UPFAnimInst_TwinBlast::MTGIDX_TB::ULTEND))
	{
		UltGunMontageIndex = etoi(UPFAnimInst_UltGun::MTGIDX_UG::ULTEND);
	}
	else
	{
		PFLOG(Warning, TEXT("Unsupported TwinBlast montage index for UltGun: %d"), NextIdx);
		return;
	}

	PFAnim->PlayMontage(UltGunMontageIndex);
}

// 총 애니메이션 이동 방향 반영
void AUltGun::SetCurrentDir(EPFDirection Dir)
{
	PFAnim->SetCurrentDir(Dir);
}
;

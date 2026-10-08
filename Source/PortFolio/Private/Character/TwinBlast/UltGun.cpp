

#include "Character/TwinBlast/UltGun.h"
#include "Particles/ParticleSystem.h"
#include "Animation/PFAnimInst_UltGun.h"
#include "Animation/PFAnimInst_TwinBlast.h"

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
	PFAnim->MontageEndTask(Montage);
}

void AUltGun::PostInitializeComponents()
{
	PFAnim = Cast<UPFAnimInst_UltGun>(MeshCom->GetAnimInstance());
	checkf(PFAnim, TEXT("UltGun AnimInstance is required"));
	Super::PostInitializeComponents();

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


// 원본 총의 자세를 1인칭 팔 위치에 표시
void AUltGun::SetFirstPersonPresentation(USkeletalMeshComponent* FirstPersonParent)
{
	const bool bEnable = FirstPersonParent != nullptr;
	if (bEnable && !FirstPersonMesh)
	{
		FirstPersonMesh = NewObject<USkeletalMeshComponent>(this, TEXT("FirstPersonUltGun"));
		FirstPersonMesh->SetSkeletalMesh(MeshCom->GetSkeletalMeshAsset());
		FirstPersonMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		FirstPersonMesh->SetGenerateOverlapEvents(false);
		FirstPersonMesh->SetCanEverAffectNavigation(false);
		FirstPersonMesh->SetOnlyOwnerSee(true);
		FirstPersonMesh->SetCastShadow(false);
		FirstPersonMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
		FirstPersonMesh->SetVisibility(false);
		FirstPersonMesh->RegisterComponent();
		FirstPersonMesh->SetLeaderPoseComponent(MeshCom, true, false);
	}
	if (bEnable && FirstPersonMesh->GetAttachParent() != FirstPersonParent)
	{
		FirstPersonMesh->AttachToComponent(FirstPersonParent, FAttachmentTransformRules::SnapToTargetIncludingScale, TEXT("UltGunAttach"));
	}
	if (bEnable != bFirstPersonActive)
	{
		bFirstPersonActive = bEnable;
		MeshCom->SetOwnerNoSee(bEnable);
		MeshCom->SetFirstPersonPrimitiveType(bEnable ? EFirstPersonPrimitiveType::WorldSpaceRepresentation : EFirstPersonPrimitiveType::None);
		if (bEnable)
		{
			SavedAnimTickOption = MeshCom->VisibilityBasedAnimTickOption;
			MeshCom->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		}
		else
		{
			MeshCom->VisibilityBasedAnimTickOption = SavedAnimTickOption;
		}
	}
	if (FirstPersonMesh)
	{
		FirstPersonMesh->SetVisibility(bEnable && FirstPersonParent->IsVisible() && MeshCom->IsVisible());
	}
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

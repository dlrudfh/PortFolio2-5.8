#include "Character/PFFirstPersonMeshComponent.h"
#include "Camera/CameraComponent.h"

UPFFirstPersonMeshComponent::UPFFirstPersonMeshComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	PrimaryComponentTick.EndTickGroup = TG_PostUpdateWork;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
	SetOnlyOwnerSee(true);
	SetCastShadow(false);
	SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	SetBoundsScale(2.f);
	bSyncAttachParentLOD = false;
	SetForcedLOD(1);
}

// 원본 자세 공유, 카메라 갱신 후 위치 보정
void UPFFirstPersonMeshComponent::InitializeView(USkeletalMeshComponent* Source, UCameraComponent* Camera, const FVector& Offset)
{
	ViewCamera = Camera;
	NeckOffset = Offset;
	SetLeaderPoseComponent(Source, true, false);
	AddTickPrerequisiteComponent(Camera);
	if (Camera->GetAttachParent()) AddTickPrerequisiteComponent(Camera->GetAttachParent());
}

void UPFFirstPersonMeshComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	PrimaryComponentTick.EndTickGroup = TG_PostUpdateWork;
	USkinnedMeshComponent* Source = LeaderPoseComponent.Get();

	// 목의 이동을 상쇄하고 팔 동작, 조준 회전 유지
	const FTransform SourceTransform = Source->GetComponentTransform();
	const FVector NeckLocal = SourceTransform.InverseTransformPosition(Source->GetSocketLocation(TEXT("neck_01")));
	const FVector AnchorWorld = ViewCamera->GetComponentLocation()
		+ ViewCamera->GetComponentQuat().RotateVector(NeckOffset);
	FTransform ViewTransform = SourceTransform;
	ViewTransform.SetLocation(AnchorWorld - SourceTransform.TransformVector(NeckLocal));
	SetWorldTransform(ViewTransform);
}

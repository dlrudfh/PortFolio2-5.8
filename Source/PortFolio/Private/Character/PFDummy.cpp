#include "Character/PFDummy.h"
#include "System/Subsystems/PFGameInstanceSubsystem.h"

#include "UI/Menu/PFTitle.h"
#include "System/Framework/PFGameInstance.h"
#include "Animation/PFAnimInst_Kwang.h"
#include "Animation/PFAnimInst_TwinBlast.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

APFDummy::APFDummy() : DummyType(CHARACTER_END), PFAnim(nullptr)
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	GetCharacterMovement()->GravityScale = 0.f;

	GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -90.f));
}

void APFDummy::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// 캡슐, 메시 충돌 설정
	GetCapsuleComponent()->SetCollisionProfileName(TEXT("PFDummy"));
	GetCapsuleComponent()->SetGenerateOverlapEvents(true);

	GetMesh()->SetCollisionProfileName(TEXT("PFCharacter"));
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

// 캐릭터 메시, 애니메이션 설정
void APFDummy::SetMesh(ECHARACTER Type)
{
	if (DummyType == Type && PFAnim) return;
	if (PFAnim) PFAnim->OnMontageEnded.RemoveDynamic(this, &APFDummy::OnMontageEnd);
	DummyType = Type;
	UPFGameInstanceSubsystem::ApplyCharacterMesh(GetMesh(), Type, false);
	PFAnim = Cast<UPFAnimInstance>(GetMesh()->GetAnimInstance());
	PFCHECK(PFAnim);
	PFAnim->OnMontageEnded.AddDynamic(this, &APFDummy::OnMontageEnd);
}
void APFDummy::NotifyActorBeginCursorOver()
{
	Super::NotifyActorBeginCursorOver();
	bHovered = true;
	GetMesh()->SetRenderCustomDepth(bLobbySelected || bInteractive);
}

void APFDummy::NotifyActorEndCursorOver()
{
	Super::NotifyActorEndCursorOver();
	bHovered = false;
	GetMesh()->SetRenderCustomDepth(bLobbySelected);
}

void APFDummy::NotifyActorOnClicked(FKey ButtonPressed)
{
	Super::NotifyActorOnClicked(ButtonPressed);
	if (bInteractive && ButtonPressed == EKeys::LeftMouseButton && IsValid(Title))
		Title->HandleDummyClicked(this);
}

// 선택 연출 종료 후 선택 강조 유지
void APFDummy::OnMontageEnd(UAnimMontage* Montage, bool bInterrupted)
{
	GetMesh()->SetRenderCustomDepth(bLobbySelected || (bInteractive && bHovered));
}

// 로비 선택, 입력 상태 반영
void APFDummy::SetLobbyPresentation(bool bSelected, bool bCanInteract)
{
	bLobbySelected = bSelected;
	bInteractive = bCanInteract;
	GetMesh()->SetRenderCustomDepth(bLobbySelected || (bInteractive && bHovered));
}

// 선택 연출 재생
void APFDummy::PlaySelection()
{
	if (PFAnim) PFAnim->PlayMontage(0);
}

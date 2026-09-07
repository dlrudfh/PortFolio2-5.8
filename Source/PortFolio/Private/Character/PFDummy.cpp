#include "Character/PFDummy.h"

#include "UI/Menu/PFTitle.h"
#include "System/Framework/PFGameInstance.h"
#include "Animation/PFAnimInst_Kwang.h"
#include "Animation/PFAnimInst_TwinBlast.h"
#include "UObject/ConstructorHelpers.h"

APFDummy::APFDummy() : DummyType(CHARACTER_END), PFAnim(nullptr)
{
	PrimaryActorTick.bCanEverTick = false;

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
	DummyType = Type;

	switch (DummyType)
	{
	case CHARACTER_TWINBLAST:
	{
		USkeletalMesh* UMesh = LoadObject<USkeletalMesh>(
			nullptr,
			TEXT("/Game/ParagonTwinblast/Characters/Heroes/TwinBlast/Meshes/TwinBlast.TwinBlast")
		);

		UClass* AnimBP = LoadClass<UAnimInstance>(
			nullptr,
			TEXT("/Game/ParagonTwinblast/Characters/Heroes/TwinBlast/TwinBlast_Blueprint.TwinBlast_Blueprint_C")
		);

		if (UMesh) GetMesh()->SetSkeletalMesh(UMesh);
		if (AnimBP) GetMesh()->SetAnimInstanceClass(AnimBP);
		PFAnim = Cast<UPFAnimInst_TwinBlast>(GetMesh()->GetAnimInstance());
		PFCHECK(nullptr != PFAnim);
	}
		break;
	case CHARACTER_KWANG:
	{
		USkeletalMesh* UMesh = LoadObject<USkeletalMesh>(
			nullptr,
			TEXT("/Game/ParagonKwang/Characters/Heroes/Kwang/Meshes/Kwang_GDC.Kwang_GDC")
		);

		UClass* AnimBP = LoadClass<UAnimInstance>(
			nullptr,
			TEXT("/Game/ParagonKwang/Characters/Heroes/Kwang/Kwang_Blueprint.Kwang_Blueprint_C")
		);

		if (UMesh) GetMesh()->SetSkeletalMesh(UMesh);
		if (AnimBP) GetMesh()->SetAnimInstanceClass(AnimBP);
		PFAnim = Cast<UPFAnimInst_Kwang>(GetMesh()->GetAnimInstance());
		PFCHECK(nullptr != PFAnim);
	}
		break;
	default:
		break;
	}

	// 선택 연출 종료 이벤트 연결
	PFAnim->OnMontageEnded.AddDynamic(this, &APFDummy::OnMontageEnd);
}
void APFDummy::NotifyActorBeginCursorOver()
{
	Super::NotifyActorBeginCursorOver();
	// 마우스 오버 강조 표시
	GetMesh()->SetRenderCustomDepth(true);
}

void APFDummy::NotifyActorEndCursorOver()
{
	Super::NotifyActorEndCursorOver();
	// 강조 표시 해제
	GetMesh()->SetRenderCustomDepth(false);
}

void APFDummy::NotifyActorOnClicked(FKey ButtonPressed)
{
	Super::NotifyActorOnClicked(ButtonPressed);
	// 선택 연출 재생
	GetMesh()->SetRenderCustomDepth(true);
	PFAnim->PlayMontage(0); 
	
	// 선택 캐릭터 저장
	GetGameInstance<UPFGameInstance>()->SetCharacterType(DummyType);
}

// 선택 연출 종료 후 세션 시작
void APFDummy::OnMontageEnd(UAnimMontage* Montage, bool bInterrupted)
{
	Title->StartSession();
}

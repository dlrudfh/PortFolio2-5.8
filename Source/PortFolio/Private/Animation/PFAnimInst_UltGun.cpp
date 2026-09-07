#include "Animation/PFAnimInst_UltGun.h"

UPFAnimInst_UltGun::UPFAnimInst_UltGun() : UPFAnimInstance()
{
	LoadMontages();
}

void UPFAnimInst_UltGun::PlayMontage(int NextIdx)
{
	if (!Montages.IsValidIndex(NextIdx))
	{
		PFLOG(Warning, TEXT("Invalid UltGun montage index: %d (Num: %d)"), NextIdx, Montages.Num());
		return;
	}

	if (CurMtg == Montages[etoi(ULTSTART)] || CurMtg == Montages[etoi(ULTEND)])
	{
		return;
	}

	// 궁극기 전환 혼합 설정
	if(NextIdx == etoi(ULTSTART))
	{
		Set_Lerp(1.f, false, 10.f);
	}
	else if (NextIdx == etoi(ULTEND))
	{
		Set_Lerp(1.f, false, 10.f);
	}

	Montage_Stop(0.f);
	CurMtg = Montages[NextIdx];
	Montage_Play(CurMtg);
}

int UPFAnimInst_UltGun::MontageEndTask(UAnimMontage* Montage)
{
	int MtgIdx = etoi(MONTAGE_END);

	for (int idx = etoi(ULTSTART); idx < etoi(MONTAGE_END); ++idx)
	{
		if (Montage == Montages[idx])
		{
			MtgIdx = idx;
			break;
		}
	}

	CurMtg = nullptr;

	return MtgIdx;
}

void UPFAnimInst_UltGun::LoadMontages()
{
	// 궁극기 총 몽타주 로드
	Montages.SetNum(etoi(MONTAGE_END));

	static ConstructorHelpers::FObjectFinder<UAnimMontage> ULTSTART_MONTAGE(TEXT("/Game/ParagonTwinblast/Characters/Heroes/TwinBlast/Animations/Ability_Ultimate_Start_Montage.Ability_Ultimate_Start_Montage"));
	if (ULTSTART_MONTAGE.Succeeded())
	{
		Montages[etoi(ULTSTART)] = ULTSTART_MONTAGE.Object;
	}
	else
	{
		PFLOG(Fatal, TEXT("UltStart_Montage Failed"));
	}

	static ConstructorHelpers::FObjectFinder<UAnimMontage> ULTEND_MONTAGE(TEXT("/Game/ParagonTwinblast/Characters/Heroes/TwinBlast/Animations/Ability_Ultimate_End_Montage.Ability_Ultimate_End_Montage"));
	if (ULTEND_MONTAGE.Succeeded())
	{
		Montages[etoi(ULTEND)] = ULTEND_MONTAGE.Object;
	}
	else
	{
		PFLOG(Fatal, TEXT("UltEnd_Montage Failed"));
	}
}

// 궁극기 총 숨김
void UPFAnimInst_UltGun::AnimNotify_UltGunEnd()
{
	GetOwningComponent()->SetVisibility(false);
}

#include "Animation/PFAnimInst_UltGun.h"

UPFAnimInst_UltGun::UPFAnimInst_UltGun() : UPFAnimInstance()
{
	LoadMontages();
}

void UPFAnimInst_UltGun::PlayMontage(int NextIdx)
{
	if (CurMtg == Montages[etoi(ULTSTART)] || CurMtg == Montages[etoi(ULTEND)])
	{
		return;
	}

	// 궁극기 전환 혼합 설정
	if (NextIdx == etoi(ULTSTART) || NextIdx == etoi(ULTEND))
	{
		Set_Lerp(1.f, false, 10.f);
	}

	Montage_Stop(0.f);
	CurMtg = Montages[NextIdx];
	Montage_Play(CurMtg);
}

int UPFAnimInst_UltGun::MontageEndTask(UAnimMontage* Montage)
{
	const int32 MtgIdx = GetMontageIndex(Montage);
	CurMtg = nullptr;
	return MtgIdx;
}

void UPFAnimInst_UltGun::LoadMontages()
{
	static const TCHAR* const Paths[] =
	{
		TwinblastUltimateStart,
		TwinblastUltimateEnd
	};
	static_assert(UE_ARRAY_COUNT(Paths) == etoi(MONTAGE_END));
	Super::LoadMontages(Paths, 2);
}

// 궁극기 총 숨김
void UPFAnimInst_UltGun::AnimNotify_UltGunEnd()
{
	GetOwningComponent()->SetVisibility(false);
}

#include "Animation/PFAnimInst_Kwang.h"

#include "GAS/PFGameplayTags.h"
#include "Animation/AnimMontage.h"

UPFAnimInst_Kwang::UPFAnimInst_Kwang() : UPFAnimInstance()
{
	LoadMontages();
}

void UPFAnimInst_Kwang::PlayMontage(int NextIdx)
{
	if (!Montages.IsValidIndex(NextIdx) || !Montages[NextIdx])
	{
		return;
	}

	const bool bIsAttackMontage = NextIdx == etoi(ATTACKA) || NextIdx == etoi(ATTACKB)
		|| NextIdx == etoi(ATTACKC) || NextIdx == etoi(ATTACKD);
	if (bIsAttackMontage)
	{
		Montage_Play(Montages[NextIdx]);
		return;
	}

	CurMtg = GetCurrentActiveMontage();
	if (!CurMtg)
	{
		CurMtg = Montages[NextIdx];
	}
	if (!Montage_IsPlaying(CurMtg))
	{
		Montage_Play(CurMtg);
	}
}

void UPFAnimInst_Kwang::HandleMontageStarted(UAnimMontage* Montage)
{
	const int32 Index = Montages.IndexOfByKey(Montage);
	if (Index < etoi(ATTACKA) || Index > etoi(ATTACKD)) return;
	AttackEnd.Broadcast();
	TrackAttackMontage(Montage, TEXT("StartAttack"), TEXT("EndAttack"));
}

int UPFAnimInst_Kwang::MontageEndTask(UAnimMontage* Montage)
{
	const int32 MtgIdx = GetMontageIndex(Montage);

	// 새 공격이 이어지는 동안 종료 처리 보류
	if (MtgIdx >= etoi(ATTACKA) && MtgIdx <= etoi(ATTACKD))
	{
		if (!FinishAttackMontage())
		{
			return etoi(MONTAGE_END);
		}
	}
	CurMtg = GetCurrentActiveMontage();

	return MtgIdx;
}

void UPFAnimInst_Kwang::LoadMontages()
{
	static const TCHAR* const Paths[] =
	{
		TEXT("/Game/ParagonKwang/Characters/Heroes/Kwang/Animations/LevelStart_Montage.LevelStart_Montage"),
		TEXT("/Game/ParagonKwang/Characters/Heroes/Kwang/Animations/PrimaryAttack_A_Slow_Montage.PrimaryAttack_A_Slow_Montage"),
		TEXT("/Game/ParagonKwang/Characters/Heroes/Kwang/Animations/PrimaryAttack_B_Slow_Montage.PrimaryAttack_B_Slow_Montage"),
		TEXT("/Game/ParagonKwang/Characters/Heroes/Kwang/Animations/PrimaryAttack_C_Slow_Montage.PrimaryAttack_C_Slow_Montage"),
		TEXT("/Game/ParagonKwang/Characters/Heroes/Kwang/Animations/PrimaryAttack_D_Slow_Montage.PrimaryAttack_D_Slow_Montage")
	};
	static_assert(UE_ARRAY_COUNT(Paths) == etoi(MONTAGE_END));
	Super::LoadMontages(Paths, 0);
}

// 검 판정 시작 알림
void UPFAnimInst_Kwang::AnimNotify_StartAttack()
{
	AttackStart.Broadcast();
}

// 검 판정 종료 알림
void UPFAnimInst_Kwang::AnimNotify_EndAttack()
{
	AttackEnd.Broadcast();
}

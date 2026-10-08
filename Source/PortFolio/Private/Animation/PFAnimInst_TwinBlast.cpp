#include "Animation/PFAnimInst_TwinBlast.h"

#include "Animation/AnimMontage.h"
#include "GAS/PFGameplayTags.h"

UPFAnimInst_TwinBlast::UPFAnimInst_TwinBlast() : UPFAnimInstance()
{
	LoadMontages();
}

void UPFAnimInst_TwinBlast::PlayMontage(int NextIdx)
{
	if (!Montages.IsValidIndex(NextIdx) || !Montages[NextIdx])
	{
		return;
	}

	if (CurMtg == Montages[etoi(ULTSTART)] || CurMtg == Montages[etoi(ULTEND)] || CurMtg == Montages[etoi(LEVELSTART)])
	{
		return;
	}

	if (NextIdx == etoi(LEFTATTACK) || NextIdx == etoi(RIGHTATTACK))
	{
		Montage_Play(Montages[NextIdx]);
		return;
	}

	AttackMontageInstanceID = INDEX_NONE;
	Montage_Stop(0.f);
	CurMtg = Montages[NextIdx];
	Montage_Play(CurMtg);
}

void UPFAnimInst_TwinBlast::HandleMontageStarted(UAnimMontage* Montage)
{
	if (Montage != Montages[etoi(LEFTATTACK)] && Montage != Montages[etoi(RIGHTATTACK)]) return;
	TrackAttackMontage(Montage, TEXT("Shoot"), TEXT("RelaxShoot"));
}

int UPFAnimInst_TwinBlast::MontageEndTask(UAnimMontage* Montage)
{
	const int32 MtgIdx = GetMontageIndex(Montage);

	// 새 일반 공격이 이어지는 동안 이전 공격의 종료 처리 보류
	if (MtgIdx == etoi(LEFTATTACK) || MtgIdx == etoi(RIGHTATTACK))
	{
		if (!FinishAttackMontage())
		{
			return etoi(MONTAGE_END);
		}
	}

	if (MtgIdx == etoi(ULTSTART))
	{
		AnimNotify_ResetCombo();
	}

	CurMtg = GetCurrentActiveMontage();

	return MtgIdx;
}

// 궁극기 공격 몽타주 재생 여부 조회
bool UPFAnimInst_TwinBlast::IsUltimateAttackMontagePlaying() const
{
	return Montage_IsPlaying(Montages[etoi(ULTATTACK)]);
}

// 공격 자세에서 발사 알림
void UPFAnimInst_TwinBlast::AnimNotify_Shoot()
{
	if (!IsRelaxed)
	{
		SendAttackGameplayEvent(PFGameplayTags::Character_Event_Attack_Shoot);
	}
}

// 대기 자세에서 발사 알림
void UPFAnimInst_TwinBlast::AnimNotify_RelaxShoot()
{
	if (IsRelaxed)
	{
		SendAttackGameplayEvent(PFGameplayTags::Character_Event_Attack_Shoot);
		RelaxTime = 3.f;
	}
}

void UPFAnimInst_TwinBlast::LoadMontages()
{
	static const TCHAR* const Paths[] =
	{
		TEXT("/Game/ParagonTwinblast/Characters/Heroes/TwinBlast/Animations/LevelStart_Montage.LevelStart_Montage"),
		TEXT("/Game/ParagonTwinblast/Characters/Heroes/TwinBlast/Animations/DoubleShot_Fire_Lft_Montage.DoubleShot_Fire_Lft_Montage"),
		TEXT("/Game/ParagonTwinblast/Characters/Heroes/TwinBlast/Animations/DoubleShot_Fire_Rt_Montage.DoubleShot_Fire_Rt_Montage"),
		TwinblastUltimateStart,
		TEXT("/Game/ParagonTwinblast/Characters/Heroes/TwinBlast/Animations/Ability_Ultimate_Fire_Montage.Ability_Ultimate_Fire_Montage"),
		TwinblastUltimateEnd
	};
	static_assert(UE_ARRAY_COUNT(Paths) == etoi(MONTAGE_END));
	Super::LoadMontages(Paths, 5);
}

void UPFAnimInst_TwinBlast::AnimNotify_ResetCombo()
{
	Super::AnimNotify_ResetCombo();
	RelaxTime = 3.f;
}

#include "Animation/PFAnimInst_TwinBlast.h"

#include "GAS/PFGameplayTags.h"

UPFAnimInst_TwinBlast::UPFAnimInst_TwinBlast() : UPFAnimInstance()
{
	LoadMontages();
}

void UPFAnimInst_TwinBlast::PlayMontage(int NextIdx)
{
	if (CurMtg == Montages[etoi(ULTSTART)] || CurMtg == Montages[etoi(ULTEND)] || CurMtg == Montages[etoi(LEVELSTART)])
	{
		return;
	}
	// 일반 공격의 혼합, 자세 유지 시간 설정
	else if (NextIdx == etoi(LEFTATTACK) || NextIdx == etoi(RIGHTATTACK))
	{
		if (!IsSaveAttack())
		{
			Set_Lerp(0.f, true, 10.f);
		}
		RelaxTime = 3.f;
	}

	Montage_Stop(0.f);
	CurMtg = Montages[NextIdx];
	Montage_Play(CurMtg);
}

int UPFAnimInst_TwinBlast::MontageEndTask(UAnimMontage* Montage)
{
	int MtgIdx = etoi(MONTAGE_END);

	for (int idx = etoi(LEVELSTART); idx < etoi(MONTAGE_END); ++idx)
	{
		if (Montage == Montages[idx])
		{
			MtgIdx = idx;
			break;
		}
	}

	if (MtgIdx == etoi(ULTSTART))
	{
		AnimNotify_ResetCombo();
	}

	CurMtg = nullptr;

	return MtgIdx;
}

// 궁극기 공격 몽타주 재생 여부 조회
bool UPFAnimInst_TwinBlast::IsUltimateAttackMontagePlaying() const
{
	return Montages.IsValidIndex(etoi(ULTATTACK))
		&& Montages[etoi(ULTATTACK)]
		&& Montage_IsPlaying(Montages[etoi(ULTATTACK)]);
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
	// 트윈블라스트 몽타주 로드
	Montages.SetNum(etoi(MONTAGE_END));

	static ConstructorHelpers::FObjectFinder<UAnimMontage> LEVELSTART_MONTAGE(TEXT("/Game/ParagonTwinblast/Characters/Heroes/TwinBlast/Animations/LevelStart_Montage.LevelStart_Montage"));
	if (LEVELSTART_MONTAGE.Succeeded())
	{
		Montages[etoi(LEVELSTART)] = LEVELSTART_MONTAGE.Object;
	}
	else
	{
		PFLOG(Fatal, TEXT("LevelStart_Montage Failed"));
	}

	static ConstructorHelpers::FObjectFinder<UAnimMontage> LEFTATTACK_MONTAGE(TEXT("/Game/ParagonTwinblast/Characters/Heroes/TwinBlast/Animations/DoubleShot_Fire_Lft_Montage.DoubleShot_Fire_Lft_Montage"));
	if (LEFTATTACK_MONTAGE.Succeeded())
	{
		Montages[etoi(LEFTATTACK)] = LEFTATTACK_MONTAGE.Object;
	}
	else
	{
		PFLOG(Fatal, TEXT("LeftAttack_Montage Failed"));
	}

	static ConstructorHelpers::FObjectFinder<UAnimMontage> RIGHTATTACK_MONTAGE(TEXT("/Game/ParagonTwinblast/Characters/Heroes/TwinBlast/Animations/DoubleShot_Fire_Rt_Montage.DoubleShot_Fire_Rt_Montage"));
	if (RIGHTATTACK_MONTAGE.Succeeded())
	{
		Montages[etoi(RIGHTATTACK)] = RIGHTATTACK_MONTAGE.Object;
	}
	else
	{
		PFLOG(Fatal, TEXT("RightAttack_Montage Failed"));
	}

	static ConstructorHelpers::FObjectFinder<UAnimMontage> ULTSTART_MONTAGE(TEXT("/Game/ParagonTwinblast/Characters/Heroes/TwinBlast/Animations/Ability_Ultimate_Start_Montage.Ability_Ultimate_Start_Montage"));
	if (ULTSTART_MONTAGE.Succeeded())
	{
		Montages[etoi(ULTSTART)] = ULTSTART_MONTAGE.Object;
	}
	else
	{
		PFLOG(Fatal, TEXT("UltStart_Montage Failed"));
	}

	static ConstructorHelpers::FObjectFinder<UAnimMontage> ULTATTACK_MONTAGE(TEXT("/Game/ParagonTwinblast/Characters/Heroes/TwinBlast/Animations/Ability_Ultimate_Fire_Montage.Ability_Ultimate_Fire_Montage"));
	if (ULTATTACK_MONTAGE.Succeeded())
	{
		Montages[etoi(ULTATTACK)] = ULTATTACK_MONTAGE.Object;
	}
	else
	{
		PFLOG(Fatal, TEXT("UltAttack_Montage Failed"));
	}

	static ConstructorHelpers::FObjectFinder<UAnimMontage> ULTEND_MONTAGE(TEXT("/Game/ParagonTwinblast/Characters/Heroes/TwinBlast/Animations/Ability_Ultimate_End_Montage.Ability_Ultimate_End_Montage"));
	if (ULTEND_MONTAGE.Succeeded())
	{
		Montages[etoi(ULTEND)] = ULTEND_MONTAGE.Object;
	}
	else
	{
		PFLOG(Warning, TEXT("UltEnd_Montage Failed"));
	}
}

void UPFAnimInst_TwinBlast::AnimNotify_ResetCombo()
{
	Super::AnimNotify_ResetCombo();
	RelaxTime = 3.f;
}

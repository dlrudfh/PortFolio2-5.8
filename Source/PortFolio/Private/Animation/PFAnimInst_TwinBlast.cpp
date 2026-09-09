#include "Animation/PFAnimInst_TwinBlast.h"

#include "Animation/AnimMontage.h"
#include "Animation/ActiveMontageInstanceScope.h"
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
		// 기존 몽타주와 혼합하며 다음 일반 공격 재생
		AttackMontageInstanceID = INDEX_NONE;
		if (Montage_Play(Montages[NextIdx]) <= 0.f)
		{
			return;
		}
		CurMtg = Montages[NextIdx];
		FAnimMontageInstance* AttackInstance = GetActiveInstanceForMontage(CurMtg);
		AttackMontageInstanceID = AttackInstance ? AttackInstance->GetInstanceID() : INDEX_NONE;

		// 이전 공격 몽타주의 남은 발사, 콤보 노티파이 제외
		for (FAnimNotifyEventReference& EventReference : NotifyQueue.AnimNotifies)
		{
			const FAnimNotifyEvent* Notify = EventReference.GetNotify();
			const UE::Anim::FAnimNotifyMontageInstanceContext* MontageContext =
				EventReference.GetContextData<UE::Anim::FAnimNotifyMontageInstanceContext>();
			if (Notify && !Notify->Notify && !Notify->NotifyStateClass && MontageContext
				&& MontageContext->MontageInstanceID != AttackMontageInstanceID
				&& (Notify->NotifyName == TEXT("SaveAttack") || Notify->NotifyName == TEXT("ResetCombo")
					|| Notify->NotifyName == TEXT("Shoot") || Notify->NotifyName == TEXT("RelaxShoot")))
			{
				EventReference.SetNotify(nullptr);
			}
		}

		Set_Lerp(LerpVal, true, 10.f);
		RelaxTime = 3.f;
		return;
	}

	AttackMontageInstanceID = INDEX_NONE;
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

	// 새 일반 공격이 이어지는 동안 이전 공격의 종료 처리 보류
	if (MtgIdx == etoi(LEFTATTACK) || MtgIdx == etoi(RIGHTATTACK))
	{
		const FAnimMontageInstance* AttackInstance = GetMontageInstanceForID(AttackMontageInstanceID);
		if (AttackInstance && AttackInstance->IsValid()
			&& (AttackInstance->IsActive() || AttackInstance->GetWeight() > 0.f))
		{
			return etoi(MONTAGE_END);
		}
		AttackMontageInstanceID = INDEX_NONE;
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

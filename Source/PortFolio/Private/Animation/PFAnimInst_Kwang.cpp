#include "Animation/PFAnimInst_Kwang.h"

#include "GAS/PFGameplayTags.h"
#include "Animation/AnimMontage.h"
#include "Animation/ActiveMontageInstanceScope.h"

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
		// 이전 검 판정 종료, 새 공격 몽타주 재생
		AttackEnd.Broadcast();
		AttackMontageInstanceID = INDEX_NONE;
		if (Montage_Play(Montages[NextIdx]) <= 0.f)
		{
			return;
		}
		CurMtg = Montages[NextIdx];
		FAnimMontageInstance* AttackInstance = GetActiveInstanceForMontage(CurMtg);
		AttackMontageInstanceID = AttackInstance ? AttackInstance->GetInstanceID() : INDEX_NONE;

		// 이전 공격 몽타주의 남은 노티파이 제외
		for (FAnimNotifyEventReference& EventReference : NotifyQueue.AnimNotifies)
		{
			const FAnimNotifyEvent* Notify = EventReference.GetNotify();
			const UE::Anim::FAnimNotifyMontageInstanceContext* MontageContext =
				EventReference.GetContextData<UE::Anim::FAnimNotifyMontageInstanceContext>();
			if (Notify && !Notify->Notify && !Notify->NotifyStateClass && MontageContext
				&& MontageContext->MontageInstanceID != AttackMontageInstanceID
				&& (Notify->NotifyName == TEXT("SaveAttack") || Notify->NotifyName == TEXT("ResetCombo")
					|| Notify->NotifyName == TEXT("StartAttack") || Notify->NotifyName == TEXT("EndAttack")))
			{
				EventReference.SetNotify(nullptr);
			}
		}

		Set_Lerp(LerpVal, true, 10.f);
		RelaxTime = 3.f;
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

int UPFAnimInst_Kwang::MontageEndTask(UAnimMontage* Montage)
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

	// 새 공격이 이어지는 동안 종료 처리 보류
	if (MtgIdx >= etoi(ATTACKA) && MtgIdx <= etoi(ATTACKD))
	{
		const FAnimMontageInstance* AttackInstance = GetMontageInstanceForID(AttackMontageInstanceID);
		if (AttackInstance && AttackInstance->IsValid()
			&& (AttackInstance->IsActive() || AttackInstance->GetWeight() > 0.f))
		{
			return etoi(MONTAGE_END);
		}
		AttackMontageInstanceID = INDEX_NONE;
	}
	CurMtg = GetCurrentActiveMontage();

	return MtgIdx;
}

void UPFAnimInst_Kwang::LoadMontages()
{
	// 광 몽타주 로드
	Montages.SetNum(etoi(MONTAGE_END));

	static ConstructorHelpers::FObjectFinder<UAnimMontage> LEVELSTART_MONTAGE(TEXT("/Game/ParagonKwang/Characters/Heroes/Kwang/Animations/LevelStart_Montage.LevelStart_Montage"));
	if (LEVELSTART_MONTAGE.Succeeded())
	{
		Montages[etoi(LEVELSTART)] = LEVELSTART_MONTAGE.Object;
	}
	else
	{
		PFLOG(Warning, TEXT("LevelStart_Montage Failed"));
	}

	static ConstructorHelpers::FObjectFinder<UAnimMontage> ATTACKA_MONTAGE(TEXT("/Game/ParagonKwang/Characters/Heroes/Kwang/Animations/PrimaryAttack_A_Slow_Montage.PrimaryAttack_A_Slow_Montage"));
	if (ATTACKA_MONTAGE.Succeeded())
	{
		Montages[etoi(ATTACKA)] = ATTACKA_MONTAGE.Object;
	}
	else
	{
		PFLOG(Warning, TEXT("AttackA_Montage Failed"));
	}

	static ConstructorHelpers::FObjectFinder<UAnimMontage> ATTACKB_MONTAGE(TEXT("/Game/ParagonKwang/Characters/Heroes/Kwang/Animations/PrimaryAttack_B_Slow_Montage.PrimaryAttack_B_Slow_Montage"));
	if (ATTACKB_MONTAGE.Succeeded())
	{
		Montages[etoi(ATTACKB)] = ATTACKB_MONTAGE.Object;
	}
	else
	{
		PFLOG(Warning, TEXT("AttackB_Montage Failed"));
	}

	static ConstructorHelpers::FObjectFinder<UAnimMontage> ATTACKC_MONTAGE(TEXT("/Game/ParagonKwang/Characters/Heroes/Kwang/Animations/PrimaryAttack_C_Slow_Montage.PrimaryAttack_C_Slow_Montage"));
	if (ATTACKC_MONTAGE.Succeeded())
	{
		Montages[etoi(ATTACKC)] = ATTACKC_MONTAGE.Object;
	}
	else
	{
		PFLOG(Warning, TEXT("AttackC_Montage Failed"));
	}

	static ConstructorHelpers::FObjectFinder<UAnimMontage> ATTACKD_MONTAGE(TEXT("/Game/ParagonKwang/Characters/Heroes/Kwang/Animations/PrimaryAttack_D_Slow_Montage.PrimaryAttack_D_Slow_Montage"));
	if (ATTACKD_MONTAGE.Succeeded())
	{
		Montages[etoi(ATTACKD)] = ATTACKD_MONTAGE.Object;
	}
	else
	{
		PFLOG(Warning, TEXT("AttackD_Montage Failed"));
	}
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

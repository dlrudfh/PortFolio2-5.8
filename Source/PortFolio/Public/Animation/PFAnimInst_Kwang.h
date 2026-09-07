#pragma once

#include "Animation/PFAnimInstance.h"
#include "PFAnimInst_Kwang.generated.h"

DECLARE_MULTICAST_DELEGATE(FAttackStartDelegate);
DECLARE_MULTICAST_DELEGATE(FAttackEndDelegate);

// 광 애니메이션 클래스
UCLASS(meta=(PrioritizeCategories="Pawn Montage Lerp"))
class PORTFOLIO_API UPFAnimInst_Kwang : public UPFAnimInstance
{
	GENERATED_BODY()

public:
	enum class MTGIDX_K
	{
		LEVELSTART,
		ATTACKA,
		ATTACKB,
		ATTACKC, 
		ATTACKD,
		MONTAGE_END
	};
	using enum UPFAnimInst_Kwang::MTGIDX_K;

public:
	UPFAnimInst_Kwang();
	virtual void PlayMontage(int NextIdx) override;
	virtual int MontageEndTask(UAnimMontage* Montage) override;

private:
	// 현재 공격 몽타주 식별자
	int32 AttackMontageInstanceID = INDEX_NONE;

	virtual void LoadMontages() override;
	UFUNCTION()
	void AnimNotify_StartAttack();
	UFUNCTION()
	void AnimNotify_EndAttack();

public:
	// 검 판정 시작, 종료 이벤트
	FAttackStartDelegate AttackStart;
	FAttackEndDelegate AttackEnd;
};

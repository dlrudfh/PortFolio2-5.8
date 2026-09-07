#pragma once

#include "Animation/PFAnimInstance.h"
#include "PFAnimInst_TwinBlast.generated.h"

// 트윈블라스트 애니메이션 클래스
UCLASS(meta=(PrioritizeCategories="Pawn Montage Lerp"))
class PORTFOLIO_API UPFAnimInst_TwinBlast : public UPFAnimInstance
{
	GENERATED_BODY()

public:
	enum class MTGIDX_TB
	{
		LEVELSTART,
		LEFTATTACK,
		RIGHTATTACK,
		ULTSTART,
		ULTATTACK,
		ULTEND,
		MONTAGE_END
		
	}; using enum UPFAnimInst_TwinBlast::MTGIDX_TB;

public:
	UPFAnimInst_TwinBlast();
	virtual void PlayMontage(int NextIdx) override;
	virtual int MontageEndTask(UAnimMontage* Montage) override;
	bool IsUltimateAttackMontagePlaying() const;
	UFUNCTION()
	void AnimNotify_Shoot();
	UFUNCTION()
	void AnimNotify_RelaxShoot();

private:
	virtual void LoadMontages() override;
	virtual void AnimNotify_ResetCombo() override;
};

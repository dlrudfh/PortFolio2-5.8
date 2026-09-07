#pragma once

#include "Animation/PFAnimInstance.h"
#include "PFAnimInst_UltGun.generated.h"

// 궁극기 총 애니메이션 클래스
UCLASS(meta=(PrioritizeCategories="Pawn Montage Lerp"))
class PORTFOLIO_API UPFAnimInst_UltGun : public UPFAnimInstance
{
	GENERATED_BODY()

public:
	enum class MTGIDX_UG
	{
		ULTSTART,
		ULTEND,
		MONTAGE_END

	}; using enum UPFAnimInst_UltGun::MTGIDX_UG;

public:
	UPFAnimInst_UltGun();
	virtual void PlayMontage(int NextIdx) override;
	virtual int MontageEndTask(UAnimMontage* Montage) override;

private:
	virtual void LoadMontages() override;
	UFUNCTION()
	virtual void AnimNotify_UltGunEnd();
};

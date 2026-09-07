#pragma once

#include "PortFolio/PortFolio.h"
#include "Animation/AnimInstance.h"
#include "AlphaBlend.h"
#include "GameplayTagContainer.h"
#include "PFAnimInstance.generated.h"

DECLARE_MULTICAST_DELEGATE(FDeathEndDelegate);

// 공통 애니메이션 클래스
UCLASS(Abstract, meta=(PrioritizeCategories="Pawn Montage Lerp"))
class PORTFOLIO_API UPFAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UPFAnimInstance();
	virtual void PlayMontage(int NextIdx) PURE_VIRTUAL(UPFAnimInstance::PlayMontage);
	UFUNCTION(BlueprintPure, Category = "GAS|State", meta = (BlueprintThreadSafe))
	bool IsInAir() const;
	UFUNCTION(BlueprintPure, Category = "GAS|State", meta = (BlueprintThreadSafe))
	bool IsOnAttack() const;
	UFUNCTION(BlueprintPure, Category = "GAS|State", meta = (BlueprintThreadSafe))
	bool IsSaveAttack() const;
	UFUNCTION(BlueprintPure, Category = "GAS|State", meta = (BlueprintThreadSafe))
	bool IsUltimate() const;
	UFUNCTION(BlueprintPure, Category = "GAS|State", meta = (BlueprintThreadSafe))
	bool IsSprint() const;
	UFUNCTION(BlueprintPure, Category = "GAS|State", meta = (BlueprintThreadSafe))
	bool IsDead() const;
	UFUNCTION()
	virtual int MontageEndTask(UAnimMontage* Montage) PURE_VIRTUAL(UPFAnimInstance::MontageEndTask, return -1;);
	bool IsLevelStartMontage(const UAnimMontage* Montage) const;

	void SetFPS(bool FPS) { IsFPS = FPS; }
	void SetCurrentDir(EPFDirection Dir) { CurrentDir = Dir; }
	void ResetAttackCombo();
	void Dead();
	// 사망 연출 종료 이벤트
	FDeathEndDelegate DeathEnd;

	const int LEVELSTART_GLOBAL = 0;

protected:
	ENetRole CheckCharacterType();
	virtual void LoadMontages() PURE_VIRTUAL(UPFAnimInstance::LoadMontages);
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	UFUNCTION()
	void AnimNotify_SaveAttack();
	UFUNCTION()
	virtual void AnimNotify_ResetCombo();
	void SendAttackGameplayEvent(const FGameplayTag& EventTag);
	UFUNCTION()
	void AnimNotify_DeathEnd();

	void Set_Lerp(float TLerpVal = 0, bool TLerpIncrease = true, float TLerpCoefficient = 10.f);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Pawn, Meta = (AllowPrivateAccess = true))
	float RelaxTime = -1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Pawn, Meta = (AllowPrivateAccess = true))
	float CurrentPawnSpeed;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Pawn, Meta = (AllowPrivateAccess = true))
	bool IsLevelStart;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Pawn, Meta = (AllowPrivateAccess = true))
	bool IsRelaxed;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Pawn, Meta = (AllowPrivateAccess = true))
	bool IsFPS;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Pawn, Meta = (AllowPrivateAccess = true))
	EPFDirection CurrentDir;

	// 캐릭터 몽타주 목록
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = Montage, Meta = (AllowPrivateAccess = true))
	TArray<UAnimMontage*> Montages;
	UAnimMontage* CurMtg;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Lerp, Meta = (AllowPrivateAccess = true))
	float LerpVal = 0.f;
	// 애니메이션 혼합 보간
	FAlphaBlend LerpBlend;

private:
	void RefreshCachedStateTags();
	// 애니메이션 조회용 상태 태그 캐시
	UPROPERTY(Transient)
	FGameplayTagContainer CachedStateTags;
};

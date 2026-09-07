#pragma once

#include "PortFolio/PortFolio.h"

#include "GameplayEffect.h"
#include "ActiveGameplayEffectHandle.h"
#include "GAS/Attributes/PFAttributeSet.h"

#include "PFGE_StatGameplayEffects.generated.h"

// 스탯 초기화 효과
UCLASS()
class PORTFOLIO_API UPFGE_InitializeStats : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UPFGE_InitializeStats();
};

// 피해 효과
UCLASS()
class PORTFOLIO_API UPFGE_Damage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UPFGE_Damage();
};

// 실드 효과
UCLASS()
class PORTFOLIO_API UPFGE_Shield : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UPFGE_Shield();
};

// 아이템 쿨타임 효과
UCLASS()
class PORTFOLIO_API UPFGE_ItemCooldown : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UPFGE_ItemCooldown();
};

// 체력 회복 효과
UCLASS()
class PORTFOLIO_API UPFGE_Heal : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UPFGE_Heal();
};

// 마나 회복 효과
UCLASS()
class PORTFOLIO_API UPFGE_RestoreMana : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UPFGE_RestoreMana();
};

// 코인 획득 효과
UCLASS()
class PORTFOLIO_API UPFGE_AddCoin : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UPFGE_AddCoin();
};

// 경험치 획득 효과
UCLASS()
class PORTFOLIO_API UPFGE_AddExperience : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UPFGE_AddExperience();
};

// 마나 소모 효과
UCLASS()
class PORTFOLIO_API UPFGE_ManaCost : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UPFGE_ManaCost();
};

// 마나 재생 효과
UCLASS()
class PORTFOLIO_API UPFGE_ManaRegen : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UPFGE_ManaRegen();
};

// 최대 체력 강화 효과
UCLASS()
class PORTFOLIO_API UPFGE_MaxHealthUpgrade : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UPFGE_MaxHealthUpgrade();
};

// 최대 마나 강화 효과
UCLASS()
class PORTFOLIO_API UPFGE_MaxManaUpgrade : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UPFGE_MaxManaUpgrade();
};

// 공격력 강화 효과
UCLASS()
class PORTFOLIO_API UPFGE_AttackPowerUpgrade : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UPFGE_AttackPowerUpgrade();
};

// 스탯 효과 적용 헬퍼
struct PORTFOLIO_API FPFGE_StatGameplayEffects
{
	static bool InitializeStats(class UAbilitySystemComponent* TargetASC, float Level, float Experience,
		float Health, float MaxHealth, float Mana, float MaxMana, float AttackPower, float Coin, float StatPoint);
	static bool ApplyDamage(class UAbilitySystemComponent* SourceASC, class UAbilitySystemComponent* TargetASC,
		float AttackPower, float Damage, class AActor* Instigator, class AActor* EffectCauser,
		const UObject* SourceObject = nullptr, const class UGameplayAbility* SourceAbility = nullptr,
		const struct FHitResult* HitResult = nullptr);
	static bool ApplyShield(class UAbilitySystemComponent* TargetASC);
	static bool ApplyItemCooldown(class UAbilitySystemComponent* TargetASC, const FGameplayTag& CooldownTag, float Duration);
	static bool ApplyHeal(class UAbilitySystemComponent* TargetASC, float Amount);
	static bool ApplyManaRestore(class UAbilitySystemComponent* TargetASC, float Amount);
	static bool ApplyCoin(class UAbilitySystemComponent* TargetASC, float Amount);
	static bool ApplyExperience(class UAbilitySystemComponent* TargetASC, float Amount);
	static bool TryApplyManaCost(class UAbilitySystemComponent* TargetASC, const class UPFAttributeSet* AttributeSet, float Cost);
	static FActiveGameplayEffectHandle ApplyManaRegen(class UAbilitySystemComponent* TargetASC);
	static bool ApplyStatUpgrade(class UAbilitySystemComponent* TargetASC, const class UPFAttributeSet* AttributeSet,
		EPFStatUpgradeType UpgradeType);
};

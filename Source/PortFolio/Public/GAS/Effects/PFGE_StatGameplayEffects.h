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

// 코인 소모 효과
UCLASS()
class PORTFOLIO_API UPFGE_CoinCost : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UPFGE_CoinCost();
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

// 초기화, 복원할 스탯 수치
USTRUCT()
struct FPFStatValues
{
	GENERATED_BODY()

	FPFStatValues() = default;
	explicit FPFStatValues(const UPFAttributeSet& Attributes);
	explicit FPFStatValues(const struct FPFCharacterData& Data);

	UPROPERTY()
	float Level = 1.f;

	UPROPERTY()
	float Experience = 0.f;

	UPROPERTY()
	float Health = 10.f;

	UPROPERTY()
	float MaxHealth = 10.f;

	UPROPERTY()
	float Mana = 10.f;

	UPROPERTY()
	float MaxMana = 10.f;

	UPROPERTY()
	float AttackPower = 1.f;

	UPROPERTY()
	float Coin = 0.f;

	UPROPERTY()
	float StatPoint = 10.f;
};

// 스탯 효과 적용 헬퍼
struct PORTFOLIO_API FPFGE_StatGameplayEffects
{
	static bool InitializeStats(class UAbilitySystemComponent* TargetASC, const FPFStatValues& Values);
	static bool ApplyDamage(class UAbilitySystemComponent* SourceASC, class UAbilitySystemComponent* TargetASC,
		float AttackPower, float Damage, class AActor* Instigator, class AActor* EffectCauser,
		const UObject* SourceObject = nullptr, const class UGameplayAbility* SourceAbility = nullptr,
		const struct FHitResult* HitResult = nullptr);
	static bool ApplyShield(class UAbilitySystemComponent* TargetASC);
	static FActiveGameplayEffectHandle ApplyItemCooldown(class UAbilitySystemComponent* TargetASC, const FGameplayTag& CooldownTag, float Duration);
	static bool ApplyHeal(class UAbilitySystemComponent* TargetASC, float Amount);
	static bool ApplyManaRestore(class UAbilitySystemComponent* TargetASC, float Amount);
	static bool ApplyCoin(class UAbilitySystemComponent* TargetASC, float Amount);
	static bool TryApplyCoinCost(class UAbilitySystemComponent* TargetASC, float Cost);
	static bool ApplyExperience(class UAbilitySystemComponent* TargetASC, float Amount);
	static bool TryApplyManaCost(class UAbilitySystemComponent* TargetASC, const class UPFAttributeSet* AttributeSet, float Cost);
	static FActiveGameplayEffectHandle ApplyManaRegen(class UAbilitySystemComponent* TargetASC);
	static bool ApplyStatUpgrade(class UAbilitySystemComponent* TargetASC, const class UPFAttributeSet* AttributeSet,
		EPFStatUpgradeType UpgradeType);
};

#include "GAS/Effects/PFGE_StatGameplayEffects.h"

#include "Abilities/GameplayAbility.h"
#include "GAS/Attributes/PFAttributeSet.h"
#include "GAS/PFGameplayTags.h"
#include "NativeGameplayTags.h"
#include "System/Framework/PFGameInstance.h"

// 스탯, 전투 수치 전달용 태그
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_PF_Data_Level, "Data.Stat.Level");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_PF_Data_Experience, "Data.Stat.Experience");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_PF_Data_StatPoint, "Data.Stat.StatPoint");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_PF_Data_Coin, "Data.Stat.Coin");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_PF_Data_Health, "Data.Stat.Health");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_PF_Data_MaxHealth, "Data.Stat.MaxHealth");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_PF_Data_Mana, "Data.Stat.Mana");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_PF_Data_MaxMana, "Data.Stat.MaxMana");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_PF_Data_AttackPower, "Data.Stat.AttackPower");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_PF_Data_Damage, "Data.Combat.Damage");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_PF_Data_Heal, "Data.Combat.Heal");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_PF_Data_ManaRestore, "Data.Combat.ManaRestore");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_PF_Data_ManaCost, "Data.Combat.ManaCost");

namespace PFGE_StatGameplayEffectPrivate
{
	// 전달값 기반 스탯 변경 추가
	void AddSetByCallerModifier(UGameplayEffect& Effect, const FGameplayAttribute& Attribute,
		EGameplayModOp::Type Operation, const FGameplayTag& DataTag)
	{
		FSetByCallerFloat SetByCaller;
		SetByCaller.DataTag = DataTag;

		FGameplayModifierInfo& Modifier = Effect.Modifiers.AddDefaulted_GetRef();
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = Operation;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
	}

	// 고정값 스탯 변경 추가
	void AddConstantModifier(UGameplayEffect& Effect, const FGameplayAttribute& Attribute,
		EGameplayModOp::Type Operation, float Magnitude)
	{
		FGameplayModifierInfo& Modifier = Effect.Modifiers.AddDefaulted_GetRef();
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = Operation;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Magnitude));
	}

	// 효과 연출 Cue 연결
	void AddGameplayCue(UGameplayEffect& Effect, const FGameplayTag& CueTag)
	{
		FGameplayEffectCue& EffectCue = Effect.GameplayCues.AddDefaulted_GetRef();
		EffectCue.GameplayCueTags.AddTag(CueTag);
	}

	// 효과 Spec 생성
	FGameplayEffectSpecHandle MakeSpec(UAbilitySystemComponent* SourceASC, TSubclassOf<UGameplayEffect> EffectClass)
	{
		return SourceASC->MakeOutgoingSpec(EffectClass, 1.f, SourceASC->MakeEffectContext());
	}

	// 자신 또는 대상에게 효과 적용
	FActiveGameplayEffectHandle ApplySpec(UAbilitySystemComponent* SourceASC, UAbilitySystemComponent* TargetASC,
		const FGameplayEffectSpecHandle& SpecHandle)
	{
		return SourceASC == TargetASC
			? SourceASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get())
			: SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
	}

	// 양수 수치가 전달되는 획득, 회복 효과 적용
	bool ApplyAmount(UAbilitySystemComponent* TargetASC, TSubclassOf<UGameplayEffect> EffectClass,
		const FGameplayTag& DataTag, float Amount)
	{
		if (!TargetASC || Amount <= 0.f)
		{
			return false;
		}
		FGameplayEffectSpecHandle SpecHandle = MakeSpec(TargetASC, EffectClass);
		SpecHandle.Data->SetSetByCallerMagnitude(DataTag, Amount);
		return ApplySpec(TargetASC, TargetASC, SpecHandle).WasSuccessfullyApplied();
	}
}

UPFGE_InitializeStats::UPFGE_InitializeStats()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	using namespace PFGE_StatGameplayEffectPrivate;
	AddSetByCallerModifier(*this, UPFAttributeSet::GetCoinAttribute(), EGameplayModOp::Override, TAG_PF_Data_Coin);
	AddSetByCallerModifier(*this, UPFAttributeSet::GetMaxHealthAttribute(), EGameplayModOp::Override, TAG_PF_Data_MaxHealth);
	AddSetByCallerModifier(*this, UPFAttributeSet::GetMaxManaAttribute(), EGameplayModOp::Override, TAG_PF_Data_MaxMana);
	AddSetByCallerModifier(*this, UPFAttributeSet::GetAttackPowerAttribute(), EGameplayModOp::Override, TAG_PF_Data_AttackPower);
	AddSetByCallerModifier(*this, UPFAttributeSet::GetStatPointAttribute(), EGameplayModOp::Override, TAG_PF_Data_StatPoint);
	AddSetByCallerModifier(*this, UPFAttributeSet::GetLevelAttribute(), EGameplayModOp::Override, TAG_PF_Data_Level);
	AddSetByCallerModifier(*this, UPFAttributeSet::GetExperienceAttribute(), EGameplayModOp::Override, TAG_PF_Data_Experience);
	AddSetByCallerModifier(*this, UPFAttributeSet::GetHealthAttribute(), EGameplayModOp::Override, TAG_PF_Data_Health);
	AddSetByCallerModifier(*this, UPFAttributeSet::GetManaAttribute(), EGameplayModOp::Override, TAG_PF_Data_Mana);
}

UPFGE_Damage::UPFGE_Damage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	PFGE_StatGameplayEffectPrivate::AddSetByCallerModifier(
		*this, UPFAttributeSet::GetIncomingDamageAttribute(), EGameplayModOp::Additive, TAG_PF_Data_Damage);
}

UPFGE_Shield::UPFGE_Shield()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(3.f));
	PFGE_StatGameplayEffectPrivate::AddGameplayCue(*this, PFGameplayTags::GameplayCue_Item_Use_Shield);
}

UPFGE_ItemCooldown::UPFGE_ItemCooldown()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(1.f));
}

UPFGE_Heal::UPFGE_Heal()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	PFGE_StatGameplayEffectPrivate::AddSetByCallerModifier(
		*this, UPFAttributeSet::GetHealthAttribute(), EGameplayModOp::Additive, TAG_PF_Data_Heal);
}

UPFGE_RestoreMana::UPFGE_RestoreMana()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	PFGE_StatGameplayEffectPrivate::AddSetByCallerModifier(
		*this, UPFAttributeSet::GetManaAttribute(), EGameplayModOp::Additive, TAG_PF_Data_ManaRestore);
}

UPFGE_AddCoin::UPFGE_AddCoin()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	PFGE_StatGameplayEffectPrivate::AddSetByCallerModifier(
		*this, UPFAttributeSet::GetCoinAttribute(), EGameplayModOp::Additive, TAG_PF_Data_Coin);
}

UPFGE_CoinCost::UPFGE_CoinCost()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	PFGE_StatGameplayEffectPrivate::AddSetByCallerModifier(
		*this, UPFAttributeSet::GetCoinAttribute(), EGameplayModOp::Additive, TAG_PF_Data_Coin);
}

UPFGE_AddExperience::UPFGE_AddExperience()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	PFGE_StatGameplayEffectPrivate::AddSetByCallerModifier(
		*this, UPFAttributeSet::GetExperienceAttribute(), EGameplayModOp::Additive, TAG_PF_Data_Experience);
}

UPFGE_ManaCost::UPFGE_ManaCost()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	PFGE_StatGameplayEffectPrivate::AddSetByCallerModifier(
		*this, UPFAttributeSet::GetManaAttribute(), EGameplayModOp::Additive, TAG_PF_Data_ManaCost);
}

UPFGE_ManaRegen::UPFGE_ManaRegen()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	Period = FScalableFloat(1.f);
	bExecutePeriodicEffectOnApplication = false;
	PFGE_StatGameplayEffectPrivate::AddConstantModifier(
		*this, UPFAttributeSet::GetManaAttribute(), EGameplayModOp::Additive, 1.f);
}

UPFGE_MaxHealthUpgrade::UPFGE_MaxHealthUpgrade()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	PFGE_StatGameplayEffectPrivate::AddConstantModifier(*this, UPFAttributeSet::GetMaxHealthAttribute(), EGameplayModOp::Additive, 10.f);
	PFGE_StatGameplayEffectPrivate::AddConstantModifier(*this, UPFAttributeSet::GetHealthAttribute(), EGameplayModOp::Additive, 10.f);
	PFGE_StatGameplayEffectPrivate::AddConstantModifier(*this, UPFAttributeSet::GetStatPointAttribute(), EGameplayModOp::Additive, -1.f);
}

UPFGE_MaxManaUpgrade::UPFGE_MaxManaUpgrade()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	PFGE_StatGameplayEffectPrivate::AddConstantModifier(*this, UPFAttributeSet::GetMaxManaAttribute(), EGameplayModOp::Additive, 10.f);
	PFGE_StatGameplayEffectPrivate::AddConstantModifier(*this, UPFAttributeSet::GetManaAttribute(), EGameplayModOp::Additive, 10.f);
	PFGE_StatGameplayEffectPrivate::AddConstantModifier(*this, UPFAttributeSet::GetStatPointAttribute(), EGameplayModOp::Additive, -1.f);
}

UPFGE_AttackPowerUpgrade::UPFGE_AttackPowerUpgrade()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	PFGE_StatGameplayEffectPrivate::AddConstantModifier(*this, UPFAttributeSet::GetAttackPowerAttribute(), EGameplayModOp::Additive, 1.f);
	PFGE_StatGameplayEffectPrivate::AddConstantModifier(*this, UPFAttributeSet::GetStatPointAttribute(), EGameplayModOp::Additive, -1.f);
}

FPFStatValues::FPFStatValues(const UPFAttributeSet& Attributes)
	: Level(Attributes.GetLevel()),
	Experience(Attributes.GetExperience()),
	Health(Attributes.GetHealth()),
	MaxHealth(Attributes.GetMaxHealth()),
	Mana(Attributes.GetMana()),
	MaxMana(Attributes.GetMaxMana()),
	AttackPower(Attributes.GetAttackPower()),
	Coin(Attributes.GetCoin()),
	StatPoint(Attributes.GetStatPoint())
{
}

FPFStatValues::FPFStatValues(const FPFCharacterData& Data)
	: Level(static_cast<float>(Data.Level)), Experience(static_cast<float>(Data.CurExp)), Health(Data.MaxHP), MaxHealth(Data.MaxHP),
	Mana(Data.MaxMP), MaxMana(Data.MaxMP), AttackPower(Data.Damage)
{
}

// 초기 스탯 효과 적용
bool FPFGE_StatGameplayEffects::InitializeStats(UAbilitySystemComponent* TargetASC, const FPFStatValues& Values)
{
	if (!TargetASC) return false;
	FGameplayEffectSpecHandle SpecHandle = PFGE_StatGameplayEffectPrivate::MakeSpec(TargetASC, UPFGE_InitializeStats::StaticClass());
	// 초기 스탯 수치 전달
	SpecHandle.Data->SetSetByCallerMagnitude(TAG_PF_Data_Level, Values.Level);
	SpecHandle.Data->SetSetByCallerMagnitude(TAG_PF_Data_Experience, Values.Experience);
	SpecHandle.Data->SetSetByCallerMagnitude(TAG_PF_Data_Coin, Values.Coin);
	SpecHandle.Data->SetSetByCallerMagnitude(TAG_PF_Data_StatPoint, Values.StatPoint);
	SpecHandle.Data->SetSetByCallerMagnitude(TAG_PF_Data_Health, Values.Health);
	SpecHandle.Data->SetSetByCallerMagnitude(TAG_PF_Data_MaxHealth, Values.MaxHealth);
	SpecHandle.Data->SetSetByCallerMagnitude(TAG_PF_Data_Mana, Values.Mana);
	SpecHandle.Data->SetSetByCallerMagnitude(TAG_PF_Data_MaxMana, Values.MaxMana);
	SpecHandle.Data->SetSetByCallerMagnitude(TAG_PF_Data_AttackPower, Values.AttackPower);
	return PFGE_StatGameplayEffectPrivate::ApplySpec(TargetASC, TargetASC, SpecHandle).WasSuccessfullyApplied();
}

// 피해 출처를 포함한 피해 효과 적용
bool FPFGE_StatGameplayEffects::ApplyDamage(UAbilitySystemComponent* SourceASC, UAbilitySystemComponent* TargetASC,
	float AttackPower, float Damage, AActor* Instigator, AActor* EffectCauser,
	const UObject* SourceObject, const UGameplayAbility* SourceAbility, const FHitResult* HitResult)
{
	if (!TargetASC || Damage <= 0.f)
	{
		return false;
	}
	if (TargetASC->HasMatchingGameplayTag(PFGameplayTags::Character_State_Shield)
		|| TargetASC->HasMatchingGameplayTag(PFGameplayTags::Character_State_Invulnerable))
	{
		return false;
	}

	// 공격자, 피해 원인, 피격 정보 구성
	UAbilitySystemComponent* EffectiveSourceASC = SourceASC ? SourceASC : TargetASC;
	FGameplayEffectContextHandle EffectContext = EffectiveSourceASC->MakeEffectContext();
	AActor* ContextInstigator = IsValid(Instigator) ? Instigator : EffectiveSourceASC->GetAvatarActor();
	AActor* ContextEffectCauser = IsValid(EffectCauser) ? EffectCauser : ContextInstigator;
	EffectContext.AddInstigator(ContextInstigator, ContextEffectCauser);

	if (IsValid(SourceAbility))
	{
		EffectContext.SetAbility(SourceAbility);
	}
	if (IsValid(SourceObject))
	{
		EffectContext.AddSourceObject(SourceObject);
	}
	if (HitResult)
	{
		EffectContext.AddHitResult(*HitResult, true);
	}

	// 피해량을 효과 Spec으로 전달
	FGameplayEffectSpecHandle SpecHandle = EffectiveSourceASC->MakeOutgoingSpec(
		UPFGE_Damage::StaticClass(), 1.f, EffectContext);
	SpecHandle.Data->SetSetByCallerMagnitude(TAG_PF_Data_AttackPower, FMath::Max(0.f, AttackPower));
	SpecHandle.Data->SetSetByCallerMagnitude(TAG_PF_Data_Damage, Damage);
	return PFGE_StatGameplayEffectPrivate::ApplySpec(EffectiveSourceASC, TargetASC, SpecHandle).WasSuccessfullyApplied();
}

// 실드 효과, 상태 태그 적용
bool FPFGE_StatGameplayEffects::ApplyShield(UAbilitySystemComponent* TargetASC)
{
	if (!TargetASC) return false;
	FGameplayEffectSpecHandle SpecHandle = PFGE_StatGameplayEffectPrivate::MakeSpec(TargetASC, UPFGE_Shield::StaticClass());
	SpecHandle.Data->DynamicGrantedTags.AddTag(PFGameplayTags::Character_State_Shield);
	return PFGE_StatGameplayEffectPrivate::ApplySpec(TargetASC, TargetASC, SpecHandle).WasSuccessfullyApplied();
}

// 아이템 쿨타임 효과 적용
FActiveGameplayEffectHandle FPFGE_StatGameplayEffects::ApplyItemCooldown(UAbilitySystemComponent* TargetASC,
	const FGameplayTag& CooldownTag, float Duration)
{
	FGameplayEffectSpecHandle SpecHandle = PFGE_StatGameplayEffectPrivate::MakeSpec(
		TargetASC, UPFGE_ItemCooldown::StaticClass());
	SpecHandle.Data->SetDuration(Duration, true);
	SpecHandle.Data->DynamicGrantedTags.AddTag(CooldownTag);
	return PFGE_StatGameplayEffectPrivate::ApplySpec(TargetASC, TargetASC, SpecHandle);
}

// 체력 회복 효과 적용
bool FPFGE_StatGameplayEffects::ApplyHeal(UAbilitySystemComponent* TargetASC, float Amount)
{
	return PFGE_StatGameplayEffectPrivate::ApplyAmount(TargetASC, UPFGE_Heal::StaticClass(), TAG_PF_Data_Heal, Amount);
}

// 마나 회복 효과 적용
bool FPFGE_StatGameplayEffects::ApplyManaRestore(UAbilitySystemComponent* TargetASC, float Amount)
{
	return PFGE_StatGameplayEffectPrivate::ApplyAmount(TargetASC, UPFGE_RestoreMana::StaticClass(), TAG_PF_Data_ManaRestore, Amount);
}

// 코인 획득 효과 적용
bool FPFGE_StatGameplayEffects::ApplyCoin(UAbilitySystemComponent* TargetASC, float Amount)
{
	return PFGE_StatGameplayEffectPrivate::ApplyAmount(TargetASC, UPFGE_AddCoin::StaticClass(), TAG_PF_Data_Coin, Amount);
}

// 검증된 구매 비용 적용
bool FPFGE_StatGameplayEffects::TryApplyCoinCost(UAbilitySystemComponent* TargetASC, float Cost)
{
	FGameplayEffectSpecHandle SpecHandle = PFGE_StatGameplayEffectPrivate::MakeSpec(TargetASC, UPFGE_CoinCost::StaticClass());
	SpecHandle.Data->SetSetByCallerMagnitude(TAG_PF_Data_Coin, -Cost);
	return PFGE_StatGameplayEffectPrivate::ApplySpec(TargetASC, TargetASC, SpecHandle).WasSuccessfullyApplied();
}

// 경험치 획득 효과 적용
bool FPFGE_StatGameplayEffects::ApplyExperience(UAbilitySystemComponent* TargetASC, float Amount)
{
	return PFGE_StatGameplayEffectPrivate::ApplyAmount(TargetASC, UPFGE_AddExperience::StaticClass(), TAG_PF_Data_Experience, Amount);
}

// 잔여 마나 확인 후 소모
bool FPFGE_StatGameplayEffects::TryApplyManaCost(UAbilitySystemComponent* TargetASC, const UPFAttributeSet* AttributeSet, float Cost)
{
	if (!TargetASC || !AttributeSet || Cost < 0.f || AttributeSet->GetMana() < Cost)
	{
		return false;
	}
	if (Cost <= 0.f)
	{
		return true;
	}

	FGameplayEffectSpecHandle SpecHandle = PFGE_StatGameplayEffectPrivate::MakeSpec(TargetASC, UPFGE_ManaCost::StaticClass());
	SpecHandle.Data->SetSetByCallerMagnitude(TAG_PF_Data_ManaCost, -Cost);
	return PFGE_StatGameplayEffectPrivate::ApplySpec(TargetASC, TargetASC, SpecHandle).WasSuccessfullyApplied();
}

// 마나 재생 효과 적용
FActiveGameplayEffectHandle FPFGE_StatGameplayEffects::ApplyManaRegen(UAbilitySystemComponent* TargetASC)
{
	if (!TargetASC) return FActiveGameplayEffectHandle();
	const FGameplayEffectSpecHandle SpecHandle = PFGE_StatGameplayEffectPrivate::MakeSpec(TargetASC, UPFGE_ManaRegen::StaticClass());
	return PFGE_StatGameplayEffectPrivate::ApplySpec(TargetASC, TargetASC, SpecHandle);
}

// 선택한 스탯 강화 효과 적용
bool FPFGE_StatGameplayEffects::ApplyStatUpgrade(UAbilitySystemComponent* TargetASC, const UPFAttributeSet* AttributeSet,
	EPFStatUpgradeType UpgradeType)
{
	if (AttributeSet->GetStatPoint() < 1.f)
	{
		return false;
	}

	TSubclassOf<UGameplayEffect> UpgradeEffectClass;
	switch (UpgradeType)
	{
	case EPFStatUpgradeType::MaxHealth:
		UpgradeEffectClass = UPFGE_MaxHealthUpgrade::StaticClass();
		break;
	case EPFStatUpgradeType::MaxMana:
		UpgradeEffectClass = UPFGE_MaxManaUpgrade::StaticClass();
		break;
	case EPFStatUpgradeType::AttackPower:
		UpgradeEffectClass = UPFGE_AttackPowerUpgrade::StaticClass();
		break;
	default:
		return false;
	}

	const FGameplayEffectSpecHandle SpecHandle = PFGE_StatGameplayEffectPrivate::MakeSpec(TargetASC, UpgradeEffectClass);
	return PFGE_StatGameplayEffectPrivate::ApplySpec(TargetASC, TargetASC, SpecHandle).WasSuccessfullyApplied();
}

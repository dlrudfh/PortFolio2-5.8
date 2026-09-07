#include "GAS/Attributes/PFAttributeSet.h"

#include "Character/PFPlayer.h"
#include "GameplayEffectExtension.h"
#include "GAS/Effects/PFGE_StatGameplayEffects.h"
#include "Net/UnrealNetwork.h"

UPFAttributeSet::UPFAttributeSet()
	: Level(0.f)
	, Experience(0.f)
	, StatPoint(0.f)
	, Coin(0.f)
	, Health(0.f)
	, MaxHealth(0.f)
	, Mana(0.f)
	, MaxMana(0.f)
	, AttackPower(0.f)
	, IncomingDamage(0.f)
{
}

void UPFAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	// 스탯별 값 범위 제한
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
	else if (Attribute == GetManaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxMana());
	}
	else if (Attribute == GetLevelAttribute())
	{
		NewValue = FMath::Max(1.f, NewValue);
	}
	else if (Attribute == GetExperienceAttribute() || Attribute == GetStatPointAttribute()
		|| Attribute == GetCoinAttribute() || Attribute == GetAttackPowerAttribute()
		|| Attribute == GetMaxHealthAttribute() || Attribute == GetMaxManaAttribute())
	{
		NewValue = FMath::Max(0.f, NewValue);
	}
}

void UPFAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	const FGameplayAttribute& ChangedAttribute = Data.EvaluatedData.Attribute;
	// 누적 피해 소비, 체력 차감
	if (ChangedAttribute == GetIncomingDamageAttribute())
	{
		const float Damage = FMath::Max(0.f, GetIncomingDamage());
		SetIncomingDamage(0.f);

		if (Damage > 0.f)
		{
			const float PreviousHealth = GetHealth();
			SetHealth(FMath::Clamp(PreviousHealth - Damage, 0.f, GetMaxHealth()));
			OnHealthChanged.Broadcast();

			// 처치 경험치 지급, 사망 알림
			const AActor* OwningActor = GetOwningActor();
			if (PreviousHealth > 0.f && GetHealth() <= 0.f && OwningActor && OwningActor->HasAuthority())
			{
				UAbilitySystemComponent* KillerASC = Data.EffectSpec.GetContext().GetOriginalInstigatorAbilitySystemComponent();
				UAbilitySystemComponent* VictimASC = GetOwningAbilitySystemComponent();
				const float KillExperienceReward = static_cast<float>(FMath::Max(1, FMath::FloorToInt(GetLevel()))) * 100.f;
				const bool bPlayerKill = KillerASC && KillerASC != VictimASC && Cast<APFPlayer>(KillerASC->GetAvatarActor());
				if (bPlayerKill)
				{
					FPFGE_StatGameplayEffects::ApplyExperience(KillerASC, KillExperienceReward);
				}

				OnHealthIsZero.Broadcast();
			}
		}
		return;
	}

	// 스탯 범위 보정, 변경 알림
	if (ChangedAttribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
		OnHealthChanged.Broadcast();
	}
	else if (ChangedAttribute == GetManaAttribute())
	{
		SetMana(FMath::Clamp(GetMana(), 0.f, GetMaxMana()));
		OnManaChanged.Broadcast();
	}
	else if (ChangedAttribute == GetMaxHealthAttribute())
	{
		SetMaxHealth(FMath::Max(0.f, GetMaxHealth()));
		SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
		OnHealthChanged.Broadcast();
		OnStatsChanged.Broadcast();
	}
	else if (ChangedAttribute == GetMaxManaAttribute())
	{
		SetMaxMana(FMath::Max(0.f, GetMaxMana()));
		SetMana(FMath::Clamp(GetMana(), 0.f, GetMaxMana()));
		OnManaChanged.Broadcast();
		OnStatsChanged.Broadcast();
	}
	else if (ChangedAttribute == GetExperienceAttribute())
	{
		// 경험치에 따른 레벨, 스탯 포인트 정산
		float RemainingExperience = FMath::Max(0.f, GetExperience());
		int32 CurrentLevel = FMath::Max(1, FMath::FloorToInt(GetLevel()));
		const int32 PreviousLevel = CurrentLevel;
		float RequiredExperience = static_cast<float>(CurrentLevel) * 100.f;

		while (RemainingExperience >= RequiredExperience)
		{
			RemainingExperience -= RequiredExperience;
			++CurrentLevel;
			RequiredExperience = static_cast<float>(CurrentLevel) * 100.f;
		}

		const int32 GainedLevelCount = CurrentLevel - PreviousLevel;
		if (GainedLevelCount > 0)
		{
			SetStatPoint(GetStatPoint() + static_cast<float>(GainedLevelCount * 5));
		}

		SetLevel(static_cast<float>(CurrentLevel));
		SetExperience(RemainingExperience);
		OnStatsChanged.Broadcast();
	}
	else if (ChangedAttribute == GetLevelAttribute() || ChangedAttribute == GetStatPointAttribute()
		|| ChangedAttribute == GetCoinAttribute()
		|| ChangedAttribute == GetAttackPowerAttribute())
	{
		OnStatsChanged.Broadcast();
	}
}

void UPFAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// GAS 스탯 복제 등록
	DOREPLIFETIME_CONDITION_NOTIFY(UPFAttributeSet, Level, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPFAttributeSet, Experience, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPFAttributeSet, StatPoint, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPFAttributeSet, Coin, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPFAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPFAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPFAttributeSet, Mana, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPFAttributeSet, MaxMana, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPFAttributeSet, AttackPower, COND_None, REPNOTIFY_Always);
}

// 레벨 복제 반영, 변경 알림
void UPFAttributeSet::OnRep_Level(const FGameplayAttributeData& OldLevel)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPFAttributeSet, Level, OldLevel);
	OnStatsChanged.Broadcast();
}

// 경험치 복제 반영, 변경 알림
void UPFAttributeSet::OnRep_Experience(const FGameplayAttributeData& OldExperience)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPFAttributeSet, Experience, OldExperience);
	OnStatsChanged.Broadcast();
}

// 스탯 포인트 복제 반영, 변경 알림
void UPFAttributeSet::OnRep_StatPoint(const FGameplayAttributeData& OldStatPoint)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPFAttributeSet, StatPoint, OldStatPoint);
	OnStatsChanged.Broadcast();
}

// 코인 복제 반영, 변경 알림
void UPFAttributeSet::OnRep_Coin(const FGameplayAttributeData& OldCoin)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPFAttributeSet, Coin, OldCoin);
	OnStatsChanged.Broadcast();
}

// 체력 복제 반영, 변경 알림
void UPFAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPFAttributeSet, Health, OldHealth);
	OnHealthChanged.Broadcast();
}

// 최대 체력 복제 반영, 변경 알림
void UPFAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPFAttributeSet, MaxHealth, OldMaxHealth);
	OnHealthChanged.Broadcast();
	OnStatsChanged.Broadcast();
}

// 마나 복제 반영, 변경 알림
void UPFAttributeSet::OnRep_Mana(const FGameplayAttributeData& OldMana)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPFAttributeSet, Mana, OldMana);
	OnManaChanged.Broadcast();
}

// 최대 마나 복제 반영, 변경 알림
void UPFAttributeSet::OnRep_MaxMana(const FGameplayAttributeData& OldMaxMana)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPFAttributeSet, MaxMana, OldMaxMana);
	OnManaChanged.Broadcast();
	OnStatsChanged.Broadcast();
}

// 공격력 복제 반영, 변경 알림
void UPFAttributeSet::OnRep_AttackPower(const FGameplayAttributeData& OldAttackPower)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPFAttributeSet, AttackPower, OldAttackPower);
	OnStatsChanged.Broadcast();
}

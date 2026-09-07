#pragma once

#include "PortFolio/PortFolio.h"

#include "AttributeSet.h"
#include "AbilitySystemComponent.h"

#include "PFAttributeSet.generated.h"

UENUM(BlueprintType)
enum class EPFStatUpgradeType : uint8
{
	MaxHealth,
	MaxMana,
	AttackPower
};

DECLARE_MULTICAST_DELEGATE(FPFOnHealthIsZeroDelegate);
DECLARE_MULTICAST_DELEGATE(FPFOnHealthChangedDelegate);
DECLARE_MULTICAST_DELEGATE(FPFOnManaChangedDelegate);
DECLARE_MULTICAST_DELEGATE(FPFOnStatsChangedDelegate);

// GAS 캐릭터 스탯 클래스
UCLASS()
class PORTFOLIO_API UPFAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UPFAttributeSet();

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 성장 스탯
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Level, Category = "GAS|Stat")
	FGameplayAttributeData Level;
	ATTRIBUTE_ACCESSORS_BASIC(UPFAttributeSet, Level)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Experience, Category = "GAS|Stat")
	FGameplayAttributeData Experience;
	ATTRIBUTE_ACCESSORS_BASIC(UPFAttributeSet, Experience)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_StatPoint, Category = "GAS|Stat")
	FGameplayAttributeData StatPoint;
	ATTRIBUTE_ACCESSORS_BASIC(UPFAttributeSet, StatPoint)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Coin, Category = "GAS|Stat")
	FGameplayAttributeData Coin;
	ATTRIBUTE_ACCESSORS_BASIC(UPFAttributeSet, Coin)

	// 전투 스탯
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "GAS|Stat")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS_BASIC(UPFAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "GAS|Stat")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS_BASIC(UPFAttributeSet, MaxHealth)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Mana, Category = "GAS|Stat")
	FGameplayAttributeData Mana;
	ATTRIBUTE_ACCESSORS_BASIC(UPFAttributeSet, Mana)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxMana, Category = "GAS|Stat")
	FGameplayAttributeData MaxMana;
	ATTRIBUTE_ACCESSORS_BASIC(UPFAttributeSet, MaxMana)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_AttackPower, Category = "GAS|Stat")
	FGameplayAttributeData AttackPower;
	ATTRIBUTE_ACCESSORS_BASIC(UPFAttributeSet, AttackPower)

	// 피해 처리용 임시 값
	UPROPERTY(BlueprintReadOnly, Category = "GAS|Meta")
	FGameplayAttributeData IncomingDamage;
	ATTRIBUTE_ACCESSORS_BASIC(UPFAttributeSet, IncomingDamage)

	// 스탯 변경, 체력 소진 이벤트
	FPFOnHealthChangedDelegate OnHealthChanged;
	FPFOnManaChangedDelegate OnManaChanged;
	FPFOnStatsChangedDelegate OnStatsChanged;
	FPFOnHealthIsZeroDelegate OnHealthIsZero;

protected:
	UFUNCTION()
	void OnRep_Level(const FGameplayAttributeData& OldLevel);
	UFUNCTION()
	void OnRep_Experience(const FGameplayAttributeData& OldExperience);
	UFUNCTION()
	void OnRep_StatPoint(const FGameplayAttributeData& OldStatPoint);
	UFUNCTION()
	void OnRep_Coin(const FGameplayAttributeData& OldCoin);
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldHealth);
	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);
	UFUNCTION()
	void OnRep_Mana(const FGameplayAttributeData& OldMana);
	UFUNCTION()
	void OnRep_MaxMana(const FGameplayAttributeData& OldMaxMana);
	UFUNCTION()
	void OnRep_AttackPower(const FGameplayAttributeData& OldAttackPower);
};

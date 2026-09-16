#pragma once

#include "PortFolio/PortFolio.h"

#include "Animation/PFAnimInstance.h"
#include "GameplayTagContainer.h"
#include "AbilitySystemInterface.h"
#include "GameplayCueInterface.h"
#include "Character/PFCharacterControlTypes.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "GAS/Attributes/PFAttributeSet.h"
#include "ActiveGameplayEffectHandle.h"
#include "Abilities/GameplayAbility.h"
#include "Particles/ParticleSystemComponent.h"
#include "TimerManager.h"

#include "PFCharacter.generated.h"

class APFCharacter;
DECLARE_MULTICAST_DELEGATE_OneParam(FPFCharacterEvent, APFCharacter*);

class USoundBase;
class USoundConcurrency;

// 공통 캐릭터 클래스
UCLASS(Abstract, meta=(PrioritizeCategories="PFCharacter UI GAS"))
class PORTFOLIO_API APFCharacter : public ACharacter, public IAbilitySystemInterface, public IGameplayCueInterface
{
	GENERATED_BODY()

public:
	APFCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION()
	virtual void SetMesh() PURE_VIRTUAL(APFCharacter::SetMesh);
	UFUNCTION()
	virtual void SetParticle() PURE_VIRTUAL(APFCharacter::SetParticle);
	UFUNCTION()
	virtual void SetSound() PURE_VIRTUAL(APFCharacter::SetSound);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PostInitializeComponents() override;
	virtual void Tick(float DeltaTime) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void OnRep_PlayerState() override;
	virtual void OnRep_Controller() override;
	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode = 0) override;
	virtual void PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker) override;
	virtual void Jump() override;

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;
	bool ApplyAttackDamageTo(APFCharacter* Target, float Damage, AActor* Attacker,
		const UGameplayAbility* AttackAbility = nullptr, const FHitResult* HitResult = nullptr);

	UFUNCTION(BlueprintPure, Category = "Animation")
	virtual float GetAimPitch() const;

	float GetDamage() const;
	UPFAttributeSet* GetAttributeSet() const { return AttributeSet; }
	bool IsDeadCharacter() const;
	bool HasStateTag(const FGameplayTag& StateTag) const;
	virtual bool IsAttackCommandActive() const;
	EPFCharacterRole GetCharacterRole() const { return CharacterRole; }
	bool IsPlayerCharacter() const { return CharacterRole == EPFCharacterRole::PLAYER; }
	bool IsEnemyCharacter() const { return CharacterRole == EPFCharacterRole::ENEMY; }
	bool IsLocalPlayerCharacter() const { return IsPlayerCharacter() && IsLocallyControlled(); }
	const FPFCharacterAISettings& GetAISettings() const { return AISettings; }
	bool IsMovementBlocked() const;
	bool HasAirborneTag() const;
	bool IsSprinting() const;
	bool HasCrosshair() const { return bHasCrosshair; }
	bool IsViewpointFixed() const { return ViewpointFixed; }
	ECONTROLMODE GetCurrentControlMode() const { return CurrentControlMode; }
	class USpringArmComponent* GetCameraSpringArm() const { return SpringArm; }
	class UCameraComponent* GetFollowCamera() const { return Camera; }
	float GetTurnSpeed() const { return TurnSpeed; }
	float GetLookUpSpeed() const { return LookUpSpeed; }

	void SetAttackInputPressed(bool bPressed);
	void SetAIAttackCommand(bool bRequested, bool bExecute);
	bool TryGetAttackAim(FVector& OutAimPoint);
	void ClearControlCommands();
	void SetMovementInputDirection(EPFDirection NewDirection);
	void SetAIMovementDirection(EPFDirection NewDirection);
	void SetControlMode(ECONTROLMODE NewControlMode);
	void ToggleViewpointFixed();
	void ToggleSprint();
	UFUNCTION()
	void ActivateJumpAbility();
	void ActivateUltimateAbility();
	virtual bool CanSprint() const;

	bool GetHP(float Value);
	bool GetMP(float Value);
	bool GetShield();
	bool GetCoin(float Value);
	float GetMana() const;
	bool TryUseMana(float ManaCost);
	void PlayPickupNiagara(ENIAGARAID PickupNiagara);
	FPFCharacterSharedStateSnapshot CaptureViewState() const;
	void ApplyViewState(const FPFCharacterSharedStateSnapshot& Snapshot);

	FPFCharacterEvent OnCharacterReady;
	FPFCharacterEvent OnCharacterDied;
	FPFCharacterEvent OnCharacterLanded;
	FPFCharacterEvent OnDeathAnimationEnded;

	UFUNCTION()
	void OnLevelStartMontageStarted(UAnimMontage* Montage);

protected:
	virtual void InitAbilityActorInfo();
	void GivePlayerAbilities();
	void RefreshControlRole();
	UFUNCTION()
	void OnRep_CharacterRole();
	UFUNCTION()
	void OnRep_CurrentControlMode();
	UFUNCTION()
	void OnRep_ViewpointFixed();
	void HandleDeathAnimationEnd();
	virtual void ManageSpeed();
	virtual void SetDir();
	void PressAttackAbilityInput();
	void ReleaseAttackAbilityInput();
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_Niagara(ENIAGARAID NiagaraID);
	UFUNCTION()
	void GameplayCue_Item_Use_HP(EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters);
	UFUNCTION()
	void GameplayCue_Item_Use_MP(EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters);
	UFUNCTION()
	void GameplayCue_Item_Use_Shield(EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters);
	UFUNCTION()
	void GameplayCue_Item_Use_Coin(EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters);
	class UNiagaraComponent* SpawnAttachedNiagara(class UNiagaraSystem* NiagaraSystem);

	void GiveAttackAbility();
	FGameplayAbilitySpecHandle GetOrGiveAbility(TSubclassOf<UGameplayAbility> AbilityClass, int32 AbilityLevel);

	void AddTag(FName TagName, int Value);
	void SetTag(FName TagName, bool Value);
	virtual void SetBlockTags(bool bBlocked);
	void UpdateAirborneTag();
	void InitGASStats();
	void BindAttributeDelegates();
	void SetReplicatedStateTag(const FGameplayTag& StateTag, bool bEnabled);
	void BindStateTagEvents();
	void UnbindStateTagEvents();
	void DeadTagChanged(FGameplayTag StateTag, int32 NewCount);
	virtual void UltimateTagChanged(FGameplayTag StateTag, int32 NewCount);
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PostHitProcessing();
	virtual void PostHitProcessing();

	virtual void SetHPBar();
	void TickHPBar();
	virtual void Attack();
	virtual TSubclassOf<UGameplayAbility> GetAttackAbilityClass() const;

	void Dead();

	UFUNCTION()
	virtual void OnRep_FinalDir();
	UFUNCTION()
	void OnRep_LevelStartActive();

	UFUNCTION()
	virtual void OnMontageEnd(UAnimMontage* Montage, bool bInterrupted);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_CharacterRole)
	EPFCharacterRole CharacterRole = EPFCharacterRole::ROLE_END;
	UPROPERTY(EditDefaultsOnly, Category = "AI")
	FPFCharacterAISettings AISettings;
	UPROPERTY(Replicated)
	float ReplicatedAimPitch = 0.f;

	// 카메라, 스프링암
	UPROPERTY(VisibleAnywhere, Category = Camera)
	class USpringArmComponent* SpringArm;

	UPROPERTY(VisibleAnywhere, Category = Camera)
	class UCameraComponent* Camera;

	// 재생 중인 실드 이펙트
	UPROPERTY(Transient)
	class UNiagaraComponent* ShieldNiagaraCom = nullptr;

	// 아이템 사용 이펙트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Effect")
	class UNiagaraSystem* HPPotionUseNiagara = nullptr;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Effect")
	class UNiagaraSystem* MPPotionUseNiagara = nullptr;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Effect")
	class UNiagaraSystem* ShieldUseNiagara = nullptr;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Effect")
	class UNiagaraSystem* CoinUseNiagara = nullptr;


	UPROPERTY(ReplicatedUsing = OnRep_CurrentControlMode)
	ECONTROLMODE CurrentControlMode = DefaultControlMode;
	UPROPERTY(ReplicatedUsing = OnRep_ViewpointFixed)
	bool ViewpointFixed = true;
	UPROPERTY(EditAnywhere)
	float TurnSpeed = 1.f;
	UPROPERTY(EditAnywhere)
	float LookUpSpeed = 1.f;
	UPROPERTY(EditAnywhere)
	float SprintSpeed = 800.f;
	bool bHasCrosshair = false;
	float ArmLengthTo = 0.f;
	FRotator ArmRotationTo = FRotator::ZeroRotator;
	float ArmRotationSpeed = 10.f;
	EPFDirection MovementInputDirection = IDLE;

	// 점프, 궁극기 어빌리티 클래스
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS")
	TSubclassOf<UGameplayAbility> JumpAbilityClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS")
	TSubclassOf<UGameplayAbility> UltimateAbilityClass;
	// 입력 해제까지 보관할 공격 Spec 핸들
	FGameplayAbilitySpecHandle PressedAttackAbilityHandle;

	// AI 소유 어빌리티 시스템, 스탯
	UPROPERTY()
	UAbilitySystemComponent* OwnedASC;
	UPROPERTY()
	UPFAttributeSet* OwnedAttributeSet;

	// 머리 위 체력바
	UPROPERTY(VisibleAnywhere, Category = UI)
	class UWidgetComponent* OtherHPBar;
	bool bOtherHPBarBound = false;

	UPROPERTY(EditAnywhere)
	float WalkSpeed = 400.f;

	bool IsAttacking;
	// 태그 이벤트를 구독한 ASC
	TWeakObjectPtr<UAbilitySystemComponent> BoundStateASC;
	// 태그 이벤트 구독 핸들
	FDelegateHandle DeadStateTagHandle;
	FDelegateHandle UltimateStateTagHandle;

	// 캐릭터 애니메이션 인스턴스
	UPROPERTY()
	UPFAnimInstance* PFAnim;

	UPROPERTY(ReplicatedUsing = OnRep_FinalDir)
	EPFDirection FinalDir;
	UPROPERTY(ReplicatedUsing = OnRep_LevelStartActive)
	bool bLevelStartActive = false;

	// 연출용 파티클, 사운드
	UPROPERTY()
	TArray<class UParticleSystem*> Particles;
	UPROPERTY()
	TArray<class USoundBase*> Sounds;

	// 어빌리티, 효과, 태그 관리
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	class UAbilitySystemComponent* ASC;
	// GAS 스탯
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	UPFAttributeSet* AttributeSet;
	// 마나 재생 효과 핸들
	FActiveGameplayEffectHandle OwnedManaRegenEffectHandle;
	bool bOwnedGASStatsInitialized = false;
	bool bAttributeDelegatesBound = false;

	// 기본 공격 어빌리티 클래스
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS")
	TSubclassOf<UGameplayAbility> AttackAbilityClass;

};

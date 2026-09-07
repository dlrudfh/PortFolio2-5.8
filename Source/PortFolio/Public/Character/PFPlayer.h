#pragma once

#include "Character/PFCharacter.h"
#include "GameplayCueInterface.h"

#include "PFPlayer.generated.h"

class UUserWidget;

// 캐릭터 교체용 상태 정보
struct FPFCharacterSharedStateSnapshot
{
	ECONTROLMODE ControlMode = TPS;
	bool bViewpointFixed = true;
	EPFDirection FinalDirection = IDLE;
	float SpringArmLength = 0.f;
	FVector SpringArmRelativeLocation = FVector::ZeroVector;
	FRotator SpringArmRelativeRotation = FRotator::ZeroRotator;
	float DesiredSpringArmLength = 0.f;
	FRotator DesiredSpringArmRotation = FRotator::ZeroRotator;
	float CameraFieldOfView = 90.f;
};

// 플레이어 공통 클래스
UCLASS(Abstract, meta=(PrioritizeCategories="PFPlayer Camera UI Chest GAS"))
class PORTFOLIO_API APFPlayer : public APFCharacter, public IGameplayCueInterface
{
	GENERATED_BODY()

public:
	APFPlayer();

	UFUNCTION(Server, Reliable)
	void Server_SetControlMode(ECONTROLMODE NewControlMode);
	UFUNCTION()
	void OnRep_CurrentControlMode();

	UFUNCTION()
	void ActivateJumpAbility();
	void JumpStart();
	void JumpEnd();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode = 0) override;
	virtual void PostInitializeComponents() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	void SetChest(class APFChest* Chest = nullptr);

	virtual void GetHP(float Value);
	virtual void GetMP(float Value);
	virtual void GetShield();
	virtual void GetCoin(float Value);

	void PlayPickupNiagara(ENIAGARAID PickupNiagara);
	float GetMana() const;
	bool TryUseMana(float ManaCost);
	bool IsSprinting() const;
	virtual bool IsAttackCommandActive() const override;

	FPFCharacterSharedStateSnapshot CreateSharedStateSnapshot() const;
	void RestoreSharedStateSnapshot(const FPFCharacterSharedStateSnapshot& Snapshot);

	virtual void Jump() override;

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_Niagara(ENIAGARAID NiagaraID);

	const FVector& GetAimPoint() const { return AimPoint; }

protected:
	virtual void InitAbilityActorInfo() override;
	virtual void SetBlockTags(bool bBlocked) override;
	void GiveAbilities();

	virtual void ManageSpeed();
	virtual void SetDir();
	UFUNCTION(Server, Reliable)
	virtual void Server_SetDir();
	virtual void OnRep_FinalDir() override;

	UFUNCTION()
	void GameplayCue_Item_Use_HP(EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters);
	UFUNCTION()
	void GameplayCue_Item_Use_MP(EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters);
	UFUNCTION()
	void GameplayCue_Item_Use_Shield(EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters);
	UFUNCTION()
	void GameplayCue_Item_Use_Coin(EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters);

	class UNiagaraComponent* SpawnAttachedNiagara(class UNiagaraSystem* NiagaraSystem);

	virtual void SetHPBar() override;

	void UpDown(float NewAxisValue);
	UFUNCTION(Server, Reliable)
	void Server_UpDown(float NewAxisValue);

	void LeftRight(float NewAxisValue);
	UFUNCTION(Server, Reliable)
	void Server_LeftRight(float NewAxisValue);

	void LookUp(float NewAxisValue);
	void Turn(float NewAxisValue);

	void AttackStart();
	void AttackEnd();
	virtual void Attack() override;
	void PressAttackAbilityInput();
	void ReleaseAttackAbilityInput();

	UFUNCTION(Server, Unreliable)
	virtual void Server_UpdateAimPoint(FVector AimPoint_Client);
	FVector CalculateAimPoint() const;

	void Ultimate();
	UFUNCTION(Server, Reliable)
	virtual void Server_Ultimate();

	void Sprint();
	UFUNCTION(Server, Reliable)
	void Server_Sprint();

	void ViewpointFix();
	UFUNCTION(Server, Reliable)
	void Server_ViewpointFix();
	UFUNCTION()
	void OnRep_ViewpointFixed();

	void Interaction();
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_Interaction();

	void ChangeCharacter();
	UFUNCTION(Server, Reliable)
	void Server_ChangeCharacter();

	void OpenInventory();
	void ViewChange();
	void SpawnTestTwinblastEnemy();
	void SpawnTestKwangEnemy();
	UFUNCTION(Server, Reliable)
	void Server_SpawnTestEnemy(bool bSpawnTwinblast);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
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

	// 로컬 HUD, 조준점
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = UI)
	TSubclassOf<UUserWidget> SelfWidgetClass;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = UI)
	class UPFCharacterWidget* SelfHPBar;
	UPROPERTY(Transient)
	class UPFCrosshairWidget* CrosshairWidget = nullptr;
	bool bSelfHPBarBound = false;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentControlMode)
	ECONTROLMODE CurrentControlMode;

	UPROPERTY(EditAnywhere)
	float TurnSpeed = 1.f;
	UPROPERTY(EditAnywhere)
	float LookUpSpeed = 1.f;
	UPROPERTY(EditAnywhere)
	float SprintSpeed = 800.f;

	// 서버 공격용 조준점
	FVector AimPoint = FVector::ZeroVector;

	// 카메라 전환 목표
	float ArmLengthTo = 0.f;
	FRotator ArmRotationTo = FRotator::ZeroRotator;
	float ArmRotationSpeed = 0.f;

	bool JumpButtonHeld = false;
	UPROPERTY(ReplicatedUsing = OnRep_ViewpointFixed)
	bool ViewpointFixed;

	UPROPERTY(VisibleAnywhere, Category = "Chest")
	class APFChest* NearestChest;

	EPFDirection UpDownDir;
	EPFDirection LeftRightDir;

	// 점프 어빌리티 클래스
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS")
	TSubclassOf<UGameplayAbility> JumpAbilityClass;

	// 궁극기 어빌리티 클래스
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS")
	TSubclassOf<UGameplayAbility> UltimateAbilityClass;

	// 입력 해제까지 보관할 공격 Spec 핸들
	FGameplayAbilitySpecHandle PressedAttackAbilityHandle;
};

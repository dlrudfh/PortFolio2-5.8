#pragma once

#include "PortFolio/PortFolio.h"

#include "Animation/PFAnimInstance.h"
#include "GameFramework/Character.h"

#include "PFDummy.generated.h"

// 캐릭터 선택용 더미 클래스
UCLASS()
class PORTFOLIO_API APFDummy : public ACharacter
{
	GENERATED_BODY()

public:
	APFDummy();

	void SetMesh(ECHARACTER Type);
	void SetTitle(class UPFTitle* TempTitle) { Title = TempTitle; }
	ECHARACTER GetDummyType() const { return DummyType; }
	void SetLobbyPresentation(bool bSelected, bool bCanInteract);
	void PlaySelection();

	virtual void PostInitializeComponents() override;

	virtual void NotifyActorBeginCursorOver() override;
	virtual void NotifyActorEndCursorOver() override;
	virtual void NotifyActorOnClicked(FKey ButtonPressed) override;

protected:
	UFUNCTION()
	void OnMontageEnd(UAnimMontage* Montage, bool bInterrupted);

private:
	ECHARACTER DummyType;
	bool bLobbySelected = false;
	bool bInteractive = false;
	bool bHovered = false;

	// 선택 애니메이션 인스턴스
	UPROPERTY()
	UPFAnimInstance* PFAnim;

	// 타이틀 UI
	UPROPERTY()
	class UPFTitle* Title;
};

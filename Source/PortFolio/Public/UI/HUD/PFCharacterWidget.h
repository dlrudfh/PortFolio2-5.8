
#pragma once

#include "PortFolio/PortFolio.h"
#include "Blueprint/UserWidget.h"
#include "GAS/Attributes/PFAttributeSet.h"
#include "PFCharacterWidget.generated.h"

// 체력, 마나, 이름 표시 위젯
UCLASS()
class PORTFOLIO_API UPFCharacterWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void BindAttributeSet(class UPFAttributeSet* NewAttributeSet, bool IsLocal);
	void SetDisplayUsername(const FString& NewUsername);

protected:
	virtual void NativeConstruct() override;
	void UpdateAllWidget();
	void UpdateUsername();
	void UpdateResourceWidgets(class UProgressBar* Progress, class UTextBlock* Text, float Current, float Maximum);
	void UpdateHPWidget();
	void UpdateMPWidget();

private:
	// 표시할 GAS 스탯
	TWeakObjectPtr<UPFAttributeSet> CurrentAttributeSet;

	FString DisplayUsername;
	UPROPERTY(Transient)
	class USizeBox* UsernameBox = nullptr;
	UPROPERTY(Transient)
	class UTextBlock* UsernameText = nullptr;

	UPROPERTY()
	class UProgressBar* HPProgressBar;
	UPROPERTY()
	class UTextBlock* HPTextBlock;
	UPROPERTY()
	class UProgressBar* MPProgressBar;
	UPROPERTY()
	class UTextBlock* MPTextBlock;



	UPROPERTY()
	bool IsLocalPlayer = false;
};

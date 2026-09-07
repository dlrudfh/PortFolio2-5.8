
#pragma once

#include "PortFolio/PortFolio.h"
#include "Blueprint/UserWidget.h"
#include "GAS/Attributes/PFAttributeSet.h"
#include "PFCharacterWidget.generated.h"

// 체력, 마나 표시 위젯
UCLASS()
class PORTFOLIO_API UPFCharacterWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void BindAttributeSet(class UPFAttributeSet* NewAttributeSet, bool IsLocal);

protected:
	virtual void NativeConstruct() override;
	void UpdateAllWidget();
	void UpdateHPWidget();
	void UpdateMPWidget();

private:
	// 표시할 GAS 스탯
	TWeakObjectPtr<UPFAttributeSet> CurrentAttributeSet;

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


#pragma once

#include "PortFolio/PortFolio.h"
#include "Blueprint/UserWidget.h"
#include "PFTitle.generated.h"


// 타이틀, 캐릭터 선택 위젯
UCLASS()
class UPFTitle : public UUserWidget
{
	GENERATED_BODY()
	
public:
	UFUNCTION()
	void StartSession();
	void ShowCharacterSelection();
	void ShowJoinFailed();
	void SetSessionBusy(bool bBusy);

protected:
	virtual void NativeConstruct() override;

private:
	UFUNCTION()
	void CreateSessionNameInput();
	UFUNCTION()
	void JoinSessionNameInput();
	UFUNCTION()
	void InputComplete(const FText& Text, ETextCommit::Type CommitMethod);
	UFUNCTION()
	void ExitGame();

private:
	// 타이틀 메뉴 영역
	UPROPERTY()
	class UCanvasPanel* TitleUI;
	UPROPERTY()
	class UButton* CreateSessionButton;
	UPROPERTY()
	class UButton* JoinSessionButton;
	UPROPERTY()
	class UButton* ExitGameButton;
	// 세션 이름 입력창
	UPROPERTY()
	class UEditableTextBox* SessionNameInputBox;
	UPROPERTY()
	class UTextBlock* ChooseCharacter;

	bool IsCreateSession = true;
	bool bSessionBusy = false;
	FString SessionName;
};

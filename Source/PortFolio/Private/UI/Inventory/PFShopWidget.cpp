#include "UI/Inventory/PFShopWidget.h"
#include "UI/PFWidgetHelpers.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "InputCoreTypes.h"
#include "Props/PFItem.h"
#include "System/Framework/PFPlayerController.h"
#include "UObject/ConstructorHelpers.h"

namespace PFShopWidgetPrivate
{
	constexpr int32 Items[] = { static_cast<int32>(APFItem::EITEM::ITEM_HPPOTION),
		static_cast<int32>(APFItem::EITEM::ITEM_MPPOTION), static_cast<int32>(APFItem::EITEM::ITEM_SHIELD) };

	// 구매 결과 안내
	FText ResultMessage(EPFShopPurchaseResult Result)
	{
		switch (Result)
		{
		case EPFShopPurchaseResult::Success: return FText::FromString(TEXT("구매한 아이템을 인벤토리에 넣었습니다."));
		case EPFShopPurchaseResult::InsufficientFunds: return FText::FromString(TEXT("보유 코인이 부족합니다."));
		case EPFShopPurchaseResult::InventoryFull: return FText::FromString(TEXT("인벤토리 공간 또는 아이템 보유 한도를 확인해 주세요."));
		case EPFShopPurchaseResult::InvalidItem: return FText::FromString(TEXT("구매할 수 없는 아이템입니다."));
		case EPFShopPurchaseResult::Unavailable: return FText::FromString(TEXT("지금은 아이템을 구매할 수 없습니다."));
		default: return FText::FromString(TEXT("구매하지 못했습니다. 다시 시도해 주세요."));
		}
	}
}

UPFShopWidget::UPFShopWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FClassFinder<UUserWidget> Layout(TEXT("/Game/GameData/UI/Shop"));
	LayoutClass = Layout.Class;
	SetIsFocusable(true);
}

TSharedRef<SWidget> UPFShopWidget::RebuildWidget()
{
	if (!LayoutWidget && LayoutClass) LayoutWidget = CreateWidget<UUserWidget>(GetOwningPlayer(), LayoutClass);
	return LayoutWidget ? LayoutWidget->TakeWidget() : Super::RebuildWidget();
}

void UPFShopWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	if (bReleaseChildren && LayoutWidget) LayoutWidget->ReleaseSlateResources(true);
}

void UPFShopWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!LayoutWidget) return;
	CoinText = PFWidgetHelpers::Find<UTextBlock>(LayoutWidget, TEXT("ShopCoin"));
	ResultText = PFWidgetHelpers::Find<UTextBlock>(LayoutWidget, TEXT("ShopResult"));
	PurchaseButtons.Reset();
	CountTexts.Reset();
	PurchaseTexts.Reset();
	for (int32 Index = 0; Index < 3; ++Index)
	{
		PurchaseButtons.Add(PFWidgetHelpers::Find<UButton>(LayoutWidget, *FString::Printf(TEXT("BuyItem_%d"), Index)));
		CountTexts.Add(PFWidgetHelpers::Find<UTextBlock>(LayoutWidget, *FString::Printf(TEXT("OwnedItem_%d"), Index)));
		PurchaseTexts.Add(PFWidgetHelpers::Find<UTextBlock>(LayoutWidget, *FString::Printf(TEXT("BuyText_%d"), Index)));
		if (UTextBlock* Price = PFWidgetHelpers::Find<UTextBlock>(LayoutWidget, *FString::Printf(TEXT("PriceItem_%d"), Index)))
		{
			Price->SetText(FText::Format(FText::FromString(TEXT("{0}원")), FText::AsNumber(APFPlayerState::GetShopItemPrice(PFShopWidgetPrivate::Items[Index]))));
		}
	}
	if (PurchaseButtons[0]) PurchaseButtons[0]->OnClicked.AddUniqueDynamic(this, &UPFShopWidget::BuyHealthPotion);
	if (PurchaseButtons[1]) PurchaseButtons[1]->OnClicked.AddUniqueDynamic(this, &UPFShopWidget::BuyManaPotion);
	if (PurchaseButtons[2]) PurchaseButtons[2]->OnClicked.AddUniqueDynamic(this, &UPFShopWidget::BuyShield);
	if (UButton* Close = PFWidgetHelpers::Find<UButton>(LayoutWidget, TEXT("CloseShop")))
	{
		Close->OnClicked.AddUniqueDynamic(this, &UPFShopWidget::CloseShop);
	}

	BoundPlayerState = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<APFPlayerState>() : nullptr;
	if (BoundPlayerState.IsValid())
	{
		BoundPlayerState->OnInventoryChanged.AddUObject(this, &UPFShopWidget::RefreshShop);
		BoundPlayerState->OnShopPurchaseResult.AddUObject(this, &UPFShopWidget::HandlePurchaseResult);
		BoundAttributes = BoundPlayerState->GetAttributeSet();
		if (BoundAttributes.IsValid()) BoundAttributes->OnStatsChanged.AddUObject(this, &UPFShopWidget::RefreshShop);
	}
	bPurchasePending = false;
	RefreshShop();
}

void UPFShopWidget::NativeDestruct()
{
	if (BoundPlayerState.IsValid())
	{
		BoundPlayerState->OnInventoryChanged.RemoveAll(this);
		BoundPlayerState->OnShopPurchaseResult.RemoveAll(this);
	}
	if (BoundAttributes.IsValid()) BoundAttributes->OnStatsChanged.RemoveAll(this);
	BoundPlayerState.Reset();
	BoundAttributes.Reset();
	Super::NativeDestruct();
}

// 잔액, 보유 수량, 구매 버튼 갱신
void UPFShopWidget::RefreshShop()
{
	const APFPlayerState* State = BoundPlayerState.Get();
	if (CoinText)
	{
		const float Coins = BoundAttributes.IsValid() ? BoundAttributes->GetCoin() : 0.f;
		CoinText->SetText(FText::Format(FText::FromString(TEXT("보유 코인  {0}원")), FText::AsNumber(FMath::FloorToInt(Coins))));
	}
	for (int32 Index = 0; Index < PurchaseButtons.Num(); ++Index)
	{
		const int32 ItemID = PFShopWidgetPrivate::Items[Index];
		const EPFShopPurchaseResult Result = State ? State->GetShopPurchaseResult(ItemID) : EPFShopPurchaseResult::Unavailable;
		if (CountTexts[Index]) CountTexts[Index]->SetText(FText::Format(FText::FromString(TEXT("보유 {0}개")), FText::AsNumber(State ? State->GetInventoryItemCount(ItemID) : 0)));
		if (PurchaseButtons[Index])
		{
			PurchaseButtons[Index]->SetIsEnabled(!bPurchasePending && Result == EPFShopPurchaseResult::Success);
			PurchaseButtons[Index]->SetToolTipText(Result == EPFShopPurchaseResult::Success ? FText::GetEmpty() : PFShopWidgetPrivate::ResultMessage(Result));
		}
		if (PurchaseTexts[Index])
		{
			const TCHAR* Label = bPurchasePending ? TEXT("처리 중") : Result == EPFShopPurchaseResult::InsufficientFunds ? TEXT("코인 부족")
				: Result == EPFShopPurchaseResult::InventoryFull ? TEXT("보유 한도") : TEXT("구매");
			PurchaseTexts[Index]->SetText(FText::FromString(Label));
		}
	}
}

// 중복 클릭 차단, 구매 요청
void UPFShopWidget::Purchase(int32 ItemID)
{
	if (bPurchasePending || !BoundPlayerState.IsValid()) return;
	bPurchasePending = true;
	if (ResultText) ResultText->SetText(FText::FromString(TEXT("구매 요청 중...")));
	RefreshShop();
	BoundPlayerState->PurchaseShopItem(ItemID);
}

// 서버 구매 결과 표시
void UPFShopWidget::HandlePurchaseResult(int32 ItemID, EPFShopPurchaseResult Result)
{
	bPurchasePending = false;
	if (ResultText) ResultText->SetText(PFShopWidgetPrivate::ResultMessage(Result));
	RefreshShop();
}

// 체력포션 구매
void UPFShopWidget::BuyHealthPotion()
{
	Purchase(PFShopWidgetPrivate::Items[0]);
}

// 마나포션 구매
void UPFShopWidget::BuyManaPotion()
{
	Purchase(PFShopWidgetPrivate::Items[1]);
}

// 실드 구매
void UPFShopWidget::BuyShield()
{
	Purchase(PFShopWidgetPrivate::Items[2]);
}

// 상점 닫기
void UPFShopWidget::CloseShop()
{
	if (APFPlayerController* Controller = Cast<APFPlayerController>(GetOwningPlayer())) Controller->ToggleShop();
}

FReply UPFShopWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::B || InKeyEvent.GetKey() == EKeys::Escape)
	{
		if (!InKeyEvent.IsRepeat()) CloseShop();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

FReply UPFShopWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return FReply::Handled().SetUserFocus(TakeWidget());
}

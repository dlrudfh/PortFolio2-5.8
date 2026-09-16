#include "UI/Inventory/PFInventoryWidget.h"

#include "Character/PFCharacter.h"
#include "UI/Inventory/PFCooldownOverlayWidget.h"
#include "Props/PFItem.h"
#include "System/Framework/PFPlayerState.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"

namespace
{
	constexpr float InventorySlotFramePadding = 2.f;
	constexpr float InventorySlotImagePadding = 0.f;
	constexpr float InventorySlotVisualSize = 84.f - ((InventorySlotFramePadding + InventorySlotImagePadding) * 2.f);

	// 퀵슬롯 키 번호 문자열 반환
	FString GetQuickSlotLabel(int32 QuickSlotIndex)
	{
		return FString::Printf(TEXT("%d"), QuickSlotIndex + 1);
	}
}

// 아이템 아이콘 조회, 캐시
UTexture2D* UPFInventoryWidget::GetInventoryItemTexture(int32 ItemID)
{
	if (const TObjectPtr<UTexture2D>* CachedTexture = ItemIconCache.Find(ItemID))
	{
		return CachedTexture->Get();
	}

	const TCHAR* TexturePath = nullptr;
	switch (ItemID)
	{
	case etoi(APFItem::EITEM::ITEM_HPPOTION):
		TexturePath = TEXT("/Game/GameData/Images/Items/Hp.Hp");
		break;
	case etoi(APFItem::EITEM::ITEM_MPPOTION):
		TexturePath = TEXT("/Game/GameData/Images/Items/Mp.Mp");
		break;
	case etoi(APFItem::EITEM::ITEM_SHIELD):
		TexturePath = TEXT("/Game/GameData/Images/Items/Shield.Shield");
		break;
	case etoi(APFItem::EITEM::ITEM_COIN):
		TexturePath = TEXT("/Game/GameData/Images/Items/Coin.Coin");
		break;
	default:
		return nullptr;
	}

	UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, TexturePath);
	if (Texture)
	{
		ItemIconCache.Add(ItemID, Texture);
	}
	return Texture;
}

TSharedRef<SWidget> UPFInventoryWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		// 인벤토리 창, 제목 표시줄 구성
		RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("InventoryRoot"));
		WidgetTree->RootWidget = RootCanvas;

		InventoryBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InventoryWindow"));
		InventoryBorder->SetBrushColor(FLinearColor(0.02f, 0.03f, 0.05f, 0.92f));
		if (UCanvasPanelSlot* WindowSlot = RootCanvas->AddChildToCanvas(InventoryBorder))
		{
			WindowSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
			WindowSlot->SetAlignment(FVector2D::ZeroVector);
			WindowSlot->SetPosition(FVector2D(30.f, 30.f));
			WindowSlot->SetSize(FVector2D(436.f, 497.f));
		}

		UVerticalBox* LayoutBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("InventoryLayout"));
		InventoryBorder->SetContent(LayoutBox);

		TitleBarBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InventoryTitleBar"));
		TitleBarBorder->SetBrushColor(FLinearColor(0.12f, 0.18f, 0.22f, 1.f));
		if (UVerticalBoxSlot* TitleSlot = LayoutBox->AddChildToVerticalBox(TitleBarBorder))
		{
			TitleSlot->SetPadding(FMargin(8.f, 8.f, 8.f, 6.f));
			TitleSlot->SetHorizontalAlignment(HAlign_Fill);
		}

		TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("InventoryTitle"));
		TitleText->SetText(FText::FromString(TEXT("인벤토리")));
		TitleText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		TitleBarBorder->SetContent(TitleText);

		// 인벤토리 슬롯 배치
		InventoryGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("InventoryGrid"));
		if (UVerticalBoxSlot* GridSlot = LayoutBox->AddChildToVerticalBox(InventoryGrid))
		{
			GridSlot->SetPadding(FMargin(8.f, 4.f, 8.f, 8.f));
			GridSlot->SetHorizontalAlignment(HAlign_Fill);
			GridSlot->SetVerticalAlignment(VAlign_Fill);
		}

		InventorySlotBorders.SetNum(APFPlayerState::InventorySlotCount);
		InventorySlotImages.SetNum(APFPlayerState::InventorySlotCount);
		InventorySlotCountTexts.SetNum(APFPlayerState::InventorySlotCount);
		InventoryCooldownOverlays.SetNum(APFPlayerState::InventorySlotCount);
		InventoryCooldownTexts.SetNum(APFPlayerState::InventorySlotCount);

		for (int32 SlotIndex = 0; SlotIndex < APFPlayerState::InventorySlotCount; ++SlotIndex)
		{
			USizeBox* SlotSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("InventorySlotSize_%d"), SlotIndex));
			SlotSizeBox->SetWidthOverride(SlotSize);
			SlotSizeBox->SetHeightOverride(SlotSize);

			UBorder* SlotBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("InventorySlot_%d"), SlotIndex));
			SlotBorder->SetBrushColor(FLinearColor(0.28f, 0.31f, 0.35f, 1.f));
			SlotBorder->SetPadding(FMargin(InventorySlotFramePadding));
			SlotSizeBox->SetContent(SlotBorder);

			UBorder* InnerSlotBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("InventorySlotInner_%d"), SlotIndex));
			InnerSlotBorder->SetBrushColor(FLinearColor(0.09f, 0.11f, 0.14f, 1.f));
			SlotBorder->SetContent(InnerSlotBorder);

			UOverlay* SlotOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), *FString::Printf(TEXT("InventorySlotOverlay_%d"), SlotIndex));
			InnerSlotBorder->SetContent(SlotOverlay);

			UImage* SlotImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *FString::Printf(TEXT("InventorySlotImage_%d"), SlotIndex));
			SlotImage->SetVisibility(ESlateVisibility::HitTestInvisible);
			if (UOverlaySlot* ImageSlot = SlotOverlay->AddChildToOverlay(SlotImage))
			{
				ImageSlot->SetHorizontalAlignment(HAlign_Fill);
				ImageSlot->SetVerticalAlignment(VAlign_Fill);
				ImageSlot->SetPadding(FMargin(InventorySlotImagePadding));
			}

			UTextBlock* SlotCountText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("InventorySlotCount_%d"), SlotIndex));
			SlotCountText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			SlotCountText->SetJustification(ETextJustify::Right);
			SlotCountText->SetVisibility(ESlateVisibility::HitTestInvisible);
			if (UOverlaySlot* CountSlot = SlotOverlay->AddChildToOverlay(SlotCountText))
			{
				CountSlot->SetHorizontalAlignment(HAlign_Right);
				CountSlot->SetVerticalAlignment(VAlign_Bottom);
				CountSlot->SetPadding(FMargin(0.f, 0.f, 4.f, 2.f));
			}

			// 슬롯 쿨타임 마스크, 시간 표시
			UPFCooldownOverlayWidget* CooldownOverlay = WidgetTree->ConstructWidget<UPFCooldownOverlayWidget>(UPFCooldownOverlayWidget::StaticClass(), *FString::Printf(TEXT("InventoryCooldownOverlay_%d"), SlotIndex));
			CooldownOverlay->SetVisibility(ESlateVisibility::Hidden);
			if (UOverlaySlot* CooldownOverlaySlot = SlotOverlay->AddChildToOverlay(CooldownOverlay))
			{
				CooldownOverlaySlot->SetHorizontalAlignment(HAlign_Fill);
				CooldownOverlaySlot->SetVerticalAlignment(VAlign_Fill);
			}

			UTextBlock* CooldownText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("InventoryCooldownText_%d"), SlotIndex));
			CooldownText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			CooldownText->SetJustification(ETextJustify::Center);
			CooldownText->SetVisibility(ESlateVisibility::Hidden);
			if (UOverlaySlot* CooldownTextSlot = SlotOverlay->AddChildToOverlay(CooldownText))
			{
				CooldownTextSlot->SetHorizontalAlignment(HAlign_Center);
				CooldownTextSlot->SetVerticalAlignment(VAlign_Center);
			}

			if (UUniformGridSlot* GridPanelSlot = InventoryGrid->AddChildToUniformGrid(SlotSizeBox, SlotIndex / InventoryColumnCount, SlotIndex % InventoryColumnCount))
			{
				GridPanelSlot->SetHorizontalAlignment(HAlign_Center);
				GridPanelSlot->SetVerticalAlignment(VAlign_Center);
			}

			InventorySlotBorders[SlotIndex] = SlotBorder;
			InventorySlotImages[SlotIndex] = SlotImage;
			InventorySlotCountTexts[SlotIndex] = SlotCountText;
			InventoryCooldownOverlays[SlotIndex] = CooldownOverlay;
			InventoryCooldownTexts[SlotIndex] = CooldownText;
		}

		// 퀵슬롯 영역, 슬롯 구성
		QuickSlotBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("QuickSlotWindow"));
		QuickSlotBorder->SetBrushColor(FLinearColor(0.02f, 0.03f, 0.05f, 0.80f));
		if (UCanvasPanelSlot* QuickSlotCanvasSlot = RootCanvas->AddChildToCanvas(QuickSlotBorder))
		{
			QuickSlotCanvasSlot->SetAnchors(FAnchors(1.f, 0.f, 1.f, 0.f));
			QuickSlotCanvasSlot->SetAlignment(FVector2D(1.f, 0.f));
			QuickSlotCanvasSlot->SetPosition(FVector2D(-30.f, 30.f));
			QuickSlotCanvasSlot->SetSize(FVector2D((SlotSize * QuickSlotColumnCount) + 16.f, SlotSize + 16.f));
		}

		QuickSlotGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("QuickSlotGrid"));
		QuickSlotBorder->SetContent(QuickSlotGrid);

		QuickSlotBorders.SetNum(APFPlayerState::QuickSlotCount);
		QuickSlotImages.SetNum(APFPlayerState::QuickSlotCount);
		QuickSlotCountTexts.SetNum(APFPlayerState::QuickSlotCount);
		QuickSlotKeyTexts.SetNum(APFPlayerState::QuickSlotCount);
		QuickSlotCooldownOverlays.SetNum(APFPlayerState::QuickSlotCount);
		QuickSlotCooldownTexts.SetNum(APFPlayerState::QuickSlotCount);

		for (int32 QuickSlotIndex = 0; QuickSlotIndex < APFPlayerState::QuickSlotCount; ++QuickSlotIndex)
		{
			USizeBox* QuickSlotSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("QuickSlotSize_%d"), QuickSlotIndex));
			QuickSlotSizeBox->SetWidthOverride(SlotSize);
			QuickSlotSizeBox->SetHeightOverride(SlotSize);

			UBorder* QuickBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("QuickSlot_%d"), QuickSlotIndex));
			QuickBorder->SetBrushColor(FLinearColor(0.38f, 0.42f, 0.26f, 1.f));
			QuickBorder->SetPadding(FMargin(InventorySlotFramePadding));
			QuickSlotSizeBox->SetContent(QuickBorder);

			UBorder* QuickInnerBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("QuickSlotInner_%d"), QuickSlotIndex));
			QuickInnerBorder->SetBrushColor(FLinearColor(0.09f, 0.11f, 0.14f, 1.f));
			QuickBorder->SetContent(QuickInnerBorder);

			UOverlay* QuickOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), *FString::Printf(TEXT("QuickSlotOverlay_%d"), QuickSlotIndex));
			QuickInnerBorder->SetContent(QuickOverlay);

			UImage* QuickImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *FString::Printf(TEXT("QuickSlotImage_%d"), QuickSlotIndex));
			QuickImage->SetVisibility(ESlateVisibility::HitTestInvisible);
			if (UOverlaySlot* QuickImageSlot = QuickOverlay->AddChildToOverlay(QuickImage))
			{
				QuickImageSlot->SetHorizontalAlignment(HAlign_Fill);
				QuickImageSlot->SetVerticalAlignment(VAlign_Fill);
				QuickImageSlot->SetPadding(FMargin(InventorySlotImagePadding));
			}

			UTextBlock* QuickCountText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("QuickSlotCount_%d"), QuickSlotIndex));
			QuickCountText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			QuickCountText->SetJustification(ETextJustify::Right);
			QuickCountText->SetVisibility(ESlateVisibility::HitTestInvisible);
			if (UOverlaySlot* QuickCountSlot = QuickOverlay->AddChildToOverlay(QuickCountText))
			{
				QuickCountSlot->SetHorizontalAlignment(HAlign_Right);
				QuickCountSlot->SetVerticalAlignment(VAlign_Bottom);
				QuickCountSlot->SetPadding(FMargin(0.f, 0.f, 4.f, 2.f));
			}

			UPFCooldownOverlayWidget* QuickCooldownOverlay = WidgetTree->ConstructWidget<UPFCooldownOverlayWidget>(UPFCooldownOverlayWidget::StaticClass(), *FString::Printf(TEXT("QuickCooldownOverlay_%d"), QuickSlotIndex));
			QuickCooldownOverlay->SetVisibility(ESlateVisibility::Hidden);
			if (UOverlaySlot* QuickCooldownOverlaySlot = QuickOverlay->AddChildToOverlay(QuickCooldownOverlay))
			{
				QuickCooldownOverlaySlot->SetHorizontalAlignment(HAlign_Fill);
				QuickCooldownOverlaySlot->SetVerticalAlignment(VAlign_Fill);
			}

			UTextBlock* QuickCooldownText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("QuickCooldownText_%d"), QuickSlotIndex));
			QuickCooldownText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			QuickCooldownText->SetJustification(ETextJustify::Center);
			QuickCooldownText->SetVisibility(ESlateVisibility::Hidden);
			if (UOverlaySlot* QuickCooldownTextSlot = QuickOverlay->AddChildToOverlay(QuickCooldownText))
			{
				QuickCooldownTextSlot->SetHorizontalAlignment(HAlign_Center);
				QuickCooldownTextSlot->SetVerticalAlignment(VAlign_Center);
			}

			// 퀵슬롯 단축키 표시
			UTextBlock* QuickKeyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("QuickSlotKey_%d"), QuickSlotIndex));
			QuickKeyText->SetText(FText::FromString(GetQuickSlotLabel(QuickSlotIndex)));
			QuickKeyText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.92f, 0.45f, 1.f)));
			QuickKeyText->SetVisibility(ESlateVisibility::HitTestInvisible);
			if (UOverlaySlot* QuickKeySlot = QuickOverlay->AddChildToOverlay(QuickKeyText))
			{
				QuickKeySlot->SetHorizontalAlignment(HAlign_Left);
				QuickKeySlot->SetVerticalAlignment(VAlign_Top);
				QuickKeySlot->SetPadding(FMargin(-6.f, -8.f, 0.f, 0.f));
			}

			if (UUniformGridSlot* QuickGridSlot = QuickSlotGrid->AddChildToUniformGrid(QuickSlotSizeBox, 0, QuickSlotIndex))
			{
				QuickGridSlot->SetHorizontalAlignment(HAlign_Center);
				QuickGridSlot->SetVerticalAlignment(VAlign_Center);
			}

			QuickSlotBorders[QuickSlotIndex] = QuickBorder;
			QuickSlotImages[QuickSlotIndex] = QuickImage;
			QuickSlotCountTexts[QuickSlotIndex] = QuickCountText;
			QuickSlotKeyTexts[QuickSlotIndex] = QuickKeyText;
			QuickSlotCooldownOverlays[QuickSlotIndex] = QuickCooldownOverlay;
			QuickSlotCooldownTexts[QuickSlotIndex] = QuickCooldownText;
		}

		// 드래그 아이콘, 수량 표시 구성
		DragPreviewBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InventoryDragPreview"));
		DragPreviewBorder->SetBrushColor(FLinearColor(1.f, 1.f, 1.f, 0.f));
		DragPreviewBorder->SetVisibility(ESlateVisibility::Hidden);
		if (UCanvasPanelSlot* DragSlot = RootCanvas->AddChildToCanvas(DragPreviewBorder))
		{
			DragSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
			DragSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			DragSlot->SetSize(FVector2D(InventorySlotVisualSize, InventorySlotVisualSize));
		}

		UOverlay* DragPreviewOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("InventoryDragOverlay"));
		DragPreviewBorder->SetContent(DragPreviewOverlay);

		DragPreviewImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("InventoryDragImage"));
		DragPreviewImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UOverlaySlot* DragImageSlot = DragPreviewOverlay->AddChildToOverlay(DragPreviewImage))
		{
			DragImageSlot->SetHorizontalAlignment(HAlign_Fill);
			DragImageSlot->SetVerticalAlignment(VAlign_Fill);
		}

		DragPreviewCountText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("InventoryDragCount"));
		DragPreviewCountText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		DragPreviewCountText->SetJustification(ETextJustify::Right);
		DragPreviewCountText->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UOverlaySlot* DragCountSlot = DragPreviewOverlay->AddChildToOverlay(DragPreviewCountText))
		{
			DragCountSlot->SetHorizontalAlignment(HAlign_Right);
			DragCountSlot->SetVerticalAlignment(VAlign_Bottom);
			DragCountSlot->SetPadding(FMargin(0.f, 0.f, 4.f, 2.f));
		}
	}
	else
	{
		RootCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	}

	return Super::RebuildWidget();
}

void UPFInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);

	// 화면 오른쪽 위에 초기 배치
	if (UCanvasPanelSlot* WindowSlot = InventoryBorder ? Cast<UCanvasPanelSlot>(InventoryBorder->Slot) : nullptr)
	{
		const FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(this);
		const FVector2D WindowSize = WindowSlot->GetSize();
		if (ViewportSize.X > WindowSize.X + 30.f)
		{
			WindowSlot->SetPosition(FVector2D(ViewportSize.X - WindowSize.X - 30.f, 30.f));
		}
	}

	SetInventoryWindowVisible(false);
	BindPlayerState(GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<APFPlayerState>() : nullptr);
}

void UPFInventoryWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateInventoryCooldowns();
	UpdateQuickSlotCooldowns();
}

// 소지품 변경 이벤트 연결
void UPFInventoryWidget::BindPlayerState(APFPlayerState* NewPlayerState)
{
	if (CurrentPlayerState.Get() == NewPlayerState)
	{
		RefreshInventory();
		return;
	}

	// 이전 구독 해제, 새 PlayerState 연결
	CancelDrag();
	if (CurrentPlayerState.IsValid())
	{
		CurrentPlayerState->OnInventoryChanged.RemoveAll(this);
	}

	CurrentPlayerState = NewPlayerState;
	if (IsValid(NewPlayerState))
	{
		NewPlayerState->OnInventoryChanged.AddUObject(this, &UPFInventoryWidget::RefreshInventory);
	}
	RefreshInventory();
}

// 인벤토리, 퀵슬롯 표시 갱신
void UPFInventoryWidget::RefreshInventory()
{
	UpdateInventorySlotAppearance();
	UpdateQuickSlotAppearance();
}

// 인벤토리 창 표시 전환
void UPFInventoryWidget::SetInventoryWindowVisible(bool bVisible)
{
	if (!bVisible)
	{
		CancelDrag();
	}

	if (!InventoryBorder)
	{
		return;
	}

	InventoryBorder->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	if (bVisible)
	{
		RefreshInventory();
	}
}

// 인벤토리 창 표시 여부 조회
bool UPFInventoryWidget::IsInventoryWindowVisible() const
{
	return InventoryBorder && InventoryBorder->GetVisibility() == ESlateVisibility::Visible;
}

// 드래그한 창 위치를 화면 안으로 제한
void UPFInventoryWidget::UpdateInventoryWindowPosition(const FVector2D& ScreenSpacePosition)
{
	if (!RootCanvas || !InventoryBorder)
	{
		return;
	}

	if (UCanvasPanelSlot* WindowSlot = Cast<UCanvasPanelSlot>(InventoryBorder->Slot))
	{
		const FVector2D DesiredAbsolutePosition = ScreenSpacePosition - InventoryDragOffset;
		FVector2D LocalPosition = RootCanvas->GetCachedGeometry().AbsoluteToLocal(DesiredAbsolutePosition);
		const FVector2D CanvasSize = RootCanvas->GetCachedGeometry().GetLocalSize();
		const FVector2D WindowSize = WindowSlot->GetSize();

		LocalPosition.X = FMath::Clamp(LocalPosition.X, 0.f, FMath::Max(0.f, CanvasSize.X - WindowSize.X));
		LocalPosition.Y = FMath::Clamp(LocalPosition.Y, 0.f, FMath::Max(0.f, CanvasSize.Y - WindowSize.Y));
		WindowSlot->SetPosition(LocalPosition);
	}
}

// 드래그 아이콘 위치 갱신
void UPFInventoryWidget::UpdateDragPreviewPosition(const FVector2D& ScreenSpacePosition)
{
	if (!RootCanvas)
	{
		return;
	}

	if (UCanvasPanelSlot* DragSlot = DragPreviewBorder ? Cast<UCanvasPanelSlot>(DragPreviewBorder->Slot) : nullptr)
	{
		DragSlot->SetPosition(RootCanvas->GetCachedGeometry().AbsoluteToLocal(ScreenSpacePosition));
	}
}

// 마우스 위치의 인벤토리 슬롯 탐색
int32 UPFInventoryWidget::FindInventorySlotIndexAtScreenPosition(const FVector2D& ScreenSpacePosition) const
{
	for (int32 SlotIndex = 0; SlotIndex < InventorySlotBorders.Num(); ++SlotIndex)
	{
		if (InventorySlotBorders[SlotIndex] && InventorySlotBorders[SlotIndex]->GetCachedGeometry().IsUnderLocation(ScreenSpacePosition))
		{
			return SlotIndex;
		}
	}

	return INDEX_NONE;
}

// 마우스 위치의 퀵슬롯 탐색
int32 UPFInventoryWidget::FindQuickSlotIndexAtScreenPosition(const FVector2D& ScreenSpacePosition) const
{
	for (int32 QuickSlotIndex = 0; QuickSlotIndex < QuickSlotBorders.Num(); ++QuickSlotIndex)
	{
		if (QuickSlotBorders[QuickSlotIndex] && QuickSlotBorders[QuickSlotIndex]->GetCachedGeometry().IsUnderLocation(ScreenSpacePosition))
		{
			return QuickSlotIndex;
		}
	}

	return INDEX_NONE;
}

// 인벤토리 아이콘, 수량 갱신
void UPFInventoryWidget::UpdateInventorySlotAppearance()
{
	const TArray<FPFInventorySlot>* InventorySlots = CurrentPlayerState.IsValid() ? &CurrentPlayerState->GetInventorySlots() : nullptr;

	for (int32 SlotIndex = 0; SlotIndex < InventorySlotBorders.Num(); ++SlotIndex)
	{
		if (!InventorySlotBorders[SlotIndex]
			|| !InventorySlotImages.IsValidIndex(SlotIndex) || !InventorySlotImages[SlotIndex]
			|| !InventorySlotCountTexts.IsValidIndex(SlotIndex) || !InventorySlotCountTexts[SlotIndex]
			|| !InventoryCooldownOverlays.IsValidIndex(SlotIndex) || !InventoryCooldownOverlays[SlotIndex]
			|| !InventoryCooldownTexts.IsValidIndex(SlotIndex) || !InventoryCooldownTexts[SlotIndex])
		{
			continue;
		}

		const bool bHasItem = InventorySlots && InventorySlots->IsValidIndex(SlotIndex) && !(*InventorySlots)[SlotIndex].IsEmpty();
		const bool bIsDraggedSlot = bIsDraggingItem && DragSourceSlotIndex == SlotIndex;

		if (!bHasItem)
		{
			InventorySlotImages[SlotIndex]->SetBrushFromTexture(nullptr, true);
			InventorySlotImages[SlotIndex]->SetVisibility(ESlateVisibility::Hidden);
			InventorySlotCountTexts[SlotIndex]->SetText(FText::GetEmpty());
			InventoryCooldownOverlays[SlotIndex]->SetVisibility(ESlateVisibility::Hidden);
			InventoryCooldownTexts[SlotIndex]->SetVisibility(ESlateVisibility::Hidden);
			InventoryCooldownTexts[SlotIndex]->SetText(FText::GetEmpty());
			continue;
		}

		const int32 ItemID = (*InventorySlots)[SlotIndex].ItemID;
		InventorySlotImages[SlotIndex]->SetBrushFromTexture(GetInventoryItemTexture(ItemID), true);
		InventorySlotImages[SlotIndex]->SetVisibility(bIsDraggedSlot ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);

		if ((*InventorySlots)[SlotIndex].Count > 1)
		{
			InventorySlotCountTexts[SlotIndex]->SetText(FText::FromString(FString::Printf(TEXT("x%d"), (*InventorySlots)[SlotIndex].Count)));
		}
		else
		{
			InventorySlotCountTexts[SlotIndex]->SetText(FText::GetEmpty());
		}

		InventorySlotCountTexts[SlotIndex]->SetVisibility(bIsDraggedSlot ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
	}
	UpdateInventoryCooldowns();
}

// 퀵슬롯 아이콘, 수량 갱신
void UPFInventoryWidget::UpdateQuickSlotAppearance()
{
	const TArray<int32>* QuickSlotItemIDs = CurrentPlayerState.IsValid() ? &CurrentPlayerState->GetQuickSlotItemIDs() : nullptr;

	for (int32 QuickSlotIndex = 0; QuickSlotIndex < QuickSlotBorders.Num(); ++QuickSlotIndex)
	{
		if (!QuickSlotBorders[QuickSlotIndex]
			|| !QuickSlotImages.IsValidIndex(QuickSlotIndex) || !QuickSlotImages[QuickSlotIndex]
			|| !QuickSlotCountTexts.IsValidIndex(QuickSlotIndex) || !QuickSlotCountTexts[QuickSlotIndex]
			|| !QuickSlotCooldownOverlays.IsValidIndex(QuickSlotIndex) || !QuickSlotCooldownOverlays[QuickSlotIndex]
			|| !QuickSlotCooldownTexts.IsValidIndex(QuickSlotIndex) || !QuickSlotCooldownTexts[QuickSlotIndex])
		{
			continue;
		}

		const int32 QuickSlotItemID = QuickSlotItemIDs && QuickSlotItemIDs->IsValidIndex(QuickSlotIndex) ? (*QuickSlotItemIDs)[QuickSlotIndex] : RETURN_ERROR;
		if (QuickSlotItemID == RETURN_ERROR)
		{
			QuickSlotImages[QuickSlotIndex]->SetBrushFromTexture(nullptr, true);
			QuickSlotImages[QuickSlotIndex]->SetVisibility(ESlateVisibility::Hidden);
			QuickSlotCountTexts[QuickSlotIndex]->SetText(FText::GetEmpty());
			QuickSlotCooldownOverlays[QuickSlotIndex]->SetVisibility(ESlateVisibility::Hidden);
			QuickSlotCooldownTexts[QuickSlotIndex]->SetVisibility(ESlateVisibility::Hidden);
			QuickSlotCooldownTexts[QuickSlotIndex]->SetText(FText::GetEmpty());
			continue;
		}

		const int32 ItemCount = CurrentPlayerState->GetInventoryItemCount(QuickSlotItemID);
		QuickSlotImages[QuickSlotIndex]->SetBrushFromTexture(GetInventoryItemTexture(QuickSlotItemID), true);
		QuickSlotImages[QuickSlotIndex]->SetVisibility(ESlateVisibility::HitTestInvisible);
		QuickSlotCountTexts[QuickSlotIndex]->SetText(ItemCount > 1 ? FText::FromString(FString::Printf(TEXT("x%d"), ItemCount)) : FText::GetEmpty());
	}
	UpdateQuickSlotCooldowns();
}

// 인벤토리 쿨타임 표시 갱신
void UPFInventoryWidget::UpdateInventoryCooldowns()
{
	if (!IsInventoryWindowVisible())
	{
		return;
	}

	const TArray<FPFInventorySlot>* InventorySlots = CurrentPlayerState.IsValid() ? &CurrentPlayerState->GetInventorySlots() : nullptr;
	for (int32 SlotIndex = 0; SlotIndex < InventoryCooldownOverlays.Num(); ++SlotIndex)
	{
		if (!InventoryCooldownOverlays[SlotIndex]
			|| !InventoryCooldownTexts.IsValidIndex(SlotIndex) || !InventoryCooldownTexts[SlotIndex])
		{
			continue;
		}

		const bool bHasItem = InventorySlots && InventorySlots->IsValidIndex(SlotIndex) && !(*InventorySlots)[SlotIndex].IsEmpty();
		const bool bIsDraggedSlot = bIsDraggingItem && DragSourceSlotIndex == SlotIndex;
		const float CooldownRemaining = bHasItem && !bIsDraggedSlot
			? CurrentPlayerState->GetItemCooldownRemaining((*InventorySlots)[SlotIndex].ItemID) : 0.f;
		if (CooldownRemaining > 0.f)
		{
			InventoryCooldownOverlays[SlotIndex]->SetCooldownProgress(CooldownRemaining / APFPlayerState::ItemCooldownDuration);
			InventoryCooldownOverlays[SlotIndex]->SetVisibility(ESlateVisibility::HitTestInvisible);
			InventoryCooldownTexts[SlotIndex]->SetText(FText::AsNumber(FMath::CeilToInt(CooldownRemaining)));
			InventoryCooldownTexts[SlotIndex]->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			InventoryCooldownOverlays[SlotIndex]->SetVisibility(ESlateVisibility::Hidden);
			InventoryCooldownTexts[SlotIndex]->SetVisibility(ESlateVisibility::Hidden);
			InventoryCooldownTexts[SlotIndex]->SetText(FText::GetEmpty());
		}
	}
}

// 퀵슬롯 쿨타임 표시 갱신
void UPFInventoryWidget::UpdateQuickSlotCooldowns()
{
	if (!QuickSlotBorder || !QuickSlotBorder->IsVisible())
	{
		return;
	}

	const TArray<int32>* QuickSlotItemIDs = CurrentPlayerState.IsValid() ? &CurrentPlayerState->GetQuickSlotItemIDs() : nullptr;
	for (int32 QuickSlotIndex = 0; QuickSlotIndex < QuickSlotCooldownOverlays.Num(); ++QuickSlotIndex)
	{
		if (!QuickSlotCooldownOverlays[QuickSlotIndex]
			|| !QuickSlotCooldownTexts.IsValidIndex(QuickSlotIndex) || !QuickSlotCooldownTexts[QuickSlotIndex])
		{
			continue;
		}

		const int32 QuickSlotItemID = QuickSlotItemIDs && QuickSlotItemIDs->IsValidIndex(QuickSlotIndex)
			? (*QuickSlotItemIDs)[QuickSlotIndex] : RETURN_ERROR;
		const float CooldownRemaining = QuickSlotItemID != RETURN_ERROR
			? CurrentPlayerState->GetItemCooldownRemaining(QuickSlotItemID) : 0.f;
		if (CooldownRemaining > 0.f)
		{
			QuickSlotCooldownOverlays[QuickSlotIndex]->SetCooldownProgress(CooldownRemaining / APFPlayerState::ItemCooldownDuration);
			QuickSlotCooldownOverlays[QuickSlotIndex]->SetVisibility(ESlateVisibility::HitTestInvisible);
			QuickSlotCooldownTexts[QuickSlotIndex]->SetText(FText::AsNumber(FMath::CeilToInt(CooldownRemaining)));
			QuickSlotCooldownTexts[QuickSlotIndex]->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			QuickSlotCooldownOverlays[QuickSlotIndex]->SetVisibility(ESlateVisibility::Hidden);
			QuickSlotCooldownTexts[QuickSlotIndex]->SetVisibility(ESlateVisibility::Hidden);
			QuickSlotCooldownTexts[QuickSlotIndex]->SetText(FText::GetEmpty());
		}
	}
}

// 드래그할 아이템 아이콘, 수량 표시
void UPFInventoryWidget::RebuildDragPreview(int32 ItemID, int32 ItemCount)
{
	if (!DragPreviewBorder || !DragPreviewImage || !DragPreviewCountText)
	{
		return;
	}

	DragPreviewImage->SetBrushFromTexture(GetInventoryItemTexture(ItemID), true);
	DragPreviewImage->SetColorAndOpacity(FLinearColor::White);

	if (ItemCount > 1)
	{
		DragPreviewCountText->SetText(FText::FromString(FString::Printf(TEXT("x%d"), ItemCount)));
	}
	else
	{
		DragPreviewCountText->SetText(FText::GetEmpty());
	}

	DragPreviewBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
}

// 드래그 표시 초기화
void UPFInventoryWidget::ClearDragPreview()
{
	if (DragPreviewImage)
	{
		DragPreviewImage->SetBrushFromTexture(nullptr, true);
	}

	if (DragPreviewCountText)
	{
		DragPreviewCountText->SetText(FText::GetEmpty());
	}

	if (DragPreviewBorder)
	{
		DragPreviewBorder->SetVisibility(ESlateVisibility::Hidden);
	}
}

// 드래그 취소, 마우스 캡처 해제
void UPFInventoryWidget::CancelDrag()
{
	const bool bWasDragging = bIsDraggingInventory || bIsDraggingItem;
	bIsDraggingInventory = false;
	bIsDraggingItem = false;
	DragSourceSlotIndex = INDEX_NONE;
	ClearDragPreview();
	UpdateInventorySlotAppearance();

	// 이 위젯이 가진 마우스 캡처 해제
	if (bWasDragging && FSlateApplication::IsInitialized())
	{
		const TSharedPtr<SWidget> CapturedWidget = GetCachedWidget();
		if (CapturedWidget.IsValid())
		{
			FSlateApplication::Get().ForEachUser([&CapturedWidget](FSlateUser& User)
			{
				if (User.DoesWidgetHaveCursorCapture(CapturedWidget))
				{
					User.ReleaseCursorCapture();
				}
			}, true);
		}
	}
}

void UPFInventoryWidget::NativeDestruct()
{
	CancelDrag();
	// 소지품 변경 구독 해제
	if (CurrentPlayerState.IsValid())
	{
		CurrentPlayerState->OnInventoryChanged.RemoveAll(this);
	}
	CurrentPlayerState.Reset();
	Super::NativeDestruct();
}

void UPFInventoryWidget::NativeOnFocusLost(const FFocusEvent& InFocusEvent)
{
	CancelDrag();
	Super::NativeOnFocusLost(InFocusEvent);
}

void UPFInventoryWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	CancelDrag();
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);
}

// 창, 아이템 드래그 시작
FReply UPFInventoryWidget::HandleInventoryMouseButtonDown(const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	const FVector2D ScreenSpacePosition = InMouseEvent.GetScreenSpacePosition();

	// 제목 표시줄에서 창 드래그 시작
	if (IsInventoryWindowVisible() && TitleBarBorder && TitleBarBorder->GetCachedGeometry().IsUnderLocation(ScreenSpacePosition))
	{
		bIsDraggingInventory = true;
		InventoryDragOffset = ScreenSpacePosition - InventoryBorder->GetCachedGeometry().GetAbsolutePosition();
		return FReply::Handled().CaptureMouse(TakeWidget());
	}

	if (!CurrentPlayerState.IsValid() || !IsInventoryWindowVisible())
	{
		return FReply::Unhandled();
	}

	const int32 SlotIndex = FindInventorySlotIndexAtScreenPosition(ScreenSpacePosition);
	const TArray<FPFInventorySlot>& InventorySlots = CurrentPlayerState->GetInventorySlots();
	if (!InventorySlots.IsValidIndex(SlotIndex) || InventorySlots[SlotIndex].IsEmpty())
	{
		return FReply::Unhandled();
	}

	// 아이템 드래그 표시, 마우스 캡처
	bIsDraggingItem = true;
	DragSourceSlotIndex = SlotIndex;
	RebuildDragPreview(InventorySlots[SlotIndex].ItemID, InventorySlots[SlotIndex].Count);
	UpdateDragPreviewPosition(ScreenSpacePosition);
	UpdateInventorySlotAppearance();
	return FReply::Handled().CaptureMouse(TakeWidget());
}

FReply UPFInventoryWidget::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (FReply Reply = HandleInventoryMouseButtonDown(InMouseEvent); Reply.IsEventHandled())
	{
		return Reply;
	}

	return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UPFInventoryWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (FReply Reply = HandleInventoryMouseButtonDown(InMouseEvent); Reply.IsEventHandled())
	{
		return Reply;
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UPFInventoryWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
	}

	if (bIsDraggingInventory)
	{
		CancelDrag();
		return FReply::Handled();
	}

	// 놓은 위치에 따라 퀵슬롯 등록, 슬롯 이동
	if (bIsDraggingItem)
	{
		if (CurrentPlayerState.IsValid())
		{
			const FVector2D DropScreenPosition = InMouseEvent.GetScreenSpacePosition();
			const int32 QuickSlotIndex = FindQuickSlotIndexAtScreenPosition(DropScreenPosition);
			const TArray<FPFInventorySlot>& InventorySlots = CurrentPlayerState->GetInventorySlots();

			if (QuickSlotIndex != INDEX_NONE && InventorySlots.IsValidIndex(DragSourceSlotIndex) && !InventorySlots[DragSourceSlotIndex].IsEmpty())
			{
				CurrentPlayerState->AssignQuickSlot(QuickSlotIndex, InventorySlots[DragSourceSlotIndex].ItemID);
			}
			else
			{
				const int32 DropInventorySlotIndex = FindInventorySlotIndexAtScreenPosition(DropScreenPosition);
				if (DropInventorySlotIndex != INDEX_NONE && DropInventorySlotIndex != DragSourceSlotIndex)
				{
					CurrentPlayerState->MoveInventorySlot(DragSourceSlotIndex, DropInventorySlotIndex);
				}
			}
		}

		CancelDrag();
		RefreshInventory();
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UPFInventoryWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bIsDraggingInventory)
	{
		UpdateInventoryWindowPosition(InMouseEvent.GetScreenSpacePosition());
		return FReply::Handled();
	}

	if (bIsDraggingItem)
	{
		UpdateDragPreviewPosition(InMouseEvent.GetScreenSpacePosition());
		return FReply::Handled();
	}

	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UPFInventoryWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!IsInventoryWindowVisible() || InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !CurrentPlayerState.IsValid())
	{
		return Super::NativeOnMouseButtonDoubleClick(InGeometry, InMouseEvent);
	}

	const int32 SlotIndex = FindInventorySlotIndexAtScreenPosition(InMouseEvent.GetScreenSpacePosition());
	const TArray<FPFInventorySlot>& InventorySlots = CurrentPlayerState->GetInventorySlots();

	if (!InventorySlots.IsValidIndex(SlotIndex) || InventorySlots[SlotIndex].IsEmpty())
	{
		return Super::NativeOnMouseButtonDoubleClick(InGeometry, InMouseEvent);
	}

	APFCharacter* Character = Cast<APFCharacter>(GetOwningPlayerPawn());
	if (!Character)
	{
		return Super::NativeOnMouseButtonDoubleClick(InGeometry, InMouseEvent);
	}

	// 더블클릭한 아이템 사용
	CancelDrag();

	CurrentPlayerState->UseInventoryItem(SlotIndex, Character);
	RefreshInventory();
	return FReply::Handled();
}

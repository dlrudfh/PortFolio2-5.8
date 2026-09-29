#include "UI/Inventory/PFInventoryWidget.h"

#include "Character/PFCharacter.h"
#include "UI/Inventory/PFCooldownOverlayWidget.h"
#include "Props/PFItem.h"
#include "System/Framework/PFPlayerState.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"

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

void UPFInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);

	// 인벤토리, 퀵슬롯 위젯 연결
	RootCanvas = Cast<UCanvasPanel>(GetWidgetFromName(TEXT("InventoryRoot")));
	InventoryBorder = Cast<UBorder>(GetWidgetFromName(TEXT("InventoryWindow")));
	QuickSlotBorder = Cast<UBorder>(GetWidgetFromName(TEXT("QuickSlotWindow")));
	if (UCanvasPanelSlot* QuickSlot = QuickSlotBorder ? Cast<UCanvasPanelSlot>(QuickSlotBorder->Slot) : nullptr)
	{
		QuickSlot->SetAnchors(FAnchors(1.f, 1.f));
		QuickSlot->SetAlignment(FVector2D(1.f, 1.f));
		QuickSlot->SetPosition(FVector2D(-28.f, -28.f));
		QuickSlotBorder->SetRenderTransformPivot(FVector2D(1.f, 1.f));
	}
	TitleBarBorder = Cast<UBorder>(GetWidgetFromName(TEXT("InventoryTitleBar")));
	DragPreviewBorder = Cast<UBorder>(GetWidgetFromName(TEXT("InventoryDragPreview")));
	DragPreviewImage = Cast<UImage>(GetWidgetFromName(TEXT("InventoryDragImage")));
	DragPreviewCountText = Cast<UTextBlock>(GetWidgetFromName(TEXT("InventoryDragCount")));

	InventorySlotBorders.SetNum(APFPlayerState::InventorySlotCount);
	InventorySlotImages.SetNum(APFPlayerState::InventorySlotCount);
	InventorySlotCountTexts.SetNum(APFPlayerState::InventorySlotCount);
	InventoryCooldownOverlays.SetNum(APFPlayerState::InventorySlotCount);
	InventoryCooldownTexts.SetNum(APFPlayerState::InventorySlotCount);
	for (int32 SlotIndex = 0; SlotIndex < APFPlayerState::InventorySlotCount; ++SlotIndex)
	{
		InventorySlotBorders[SlotIndex] = Cast<UBorder>(GetWidgetFromName(*FString::Printf(TEXT("InventorySlot_%d"), SlotIndex)));
		InventorySlotImages[SlotIndex] = Cast<UImage>(GetWidgetFromName(*FString::Printf(TEXT("InventorySlotImage_%d"), SlotIndex)));
		InventorySlotCountTexts[SlotIndex] = Cast<UTextBlock>(GetWidgetFromName(*FString::Printf(TEXT("InventorySlotCount_%d"), SlotIndex)));
		InventoryCooldownOverlays[SlotIndex] = Cast<UPFCooldownOverlayWidget>(GetWidgetFromName(*FString::Printf(TEXT("InventoryCooldownOverlay_%d"), SlotIndex)));
		InventoryCooldownTexts[SlotIndex] = Cast<UTextBlock>(GetWidgetFromName(*FString::Printf(TEXT("InventoryCooldownText_%d"), SlotIndex)));
	}

	QuickSlotBorders.SetNum(APFPlayerState::QuickSlotCount);
	QuickSlotImages.SetNum(APFPlayerState::QuickSlotCount);
	QuickSlotCountTexts.SetNum(APFPlayerState::QuickSlotCount);
	QuickSlotCooldownOverlays.SetNum(APFPlayerState::QuickSlotCount);
	QuickSlotCooldownTexts.SetNum(APFPlayerState::QuickSlotCount);
	for (int32 QuickSlotIndex = 0; QuickSlotIndex < APFPlayerState::QuickSlotCount; ++QuickSlotIndex)
	{
		QuickSlotBorders[QuickSlotIndex] = Cast<UBorder>(GetWidgetFromName(*FString::Printf(TEXT("QuickSlot_%d"), QuickSlotIndex)));
		QuickSlotImages[QuickSlotIndex] = Cast<UImage>(GetWidgetFromName(*FString::Printf(TEXT("QuickSlotImage_%d"), QuickSlotIndex)));
		QuickSlotCountTexts[QuickSlotIndex] = Cast<UTextBlock>(GetWidgetFromName(*FString::Printf(TEXT("QuickSlotCount_%d"), QuickSlotIndex)));
		QuickSlotCooldownOverlays[QuickSlotIndex] = Cast<UPFCooldownOverlayWidget>(GetWidgetFromName(*FString::Printf(TEXT("QuickCooldownOverlay_%d"), QuickSlotIndex)));
		QuickSlotCooldownTexts[QuickSlotIndex] = Cast<UTextBlock>(GetWidgetFromName(*FString::Printf(TEXT("QuickCooldownText_%d"), QuickSlotIndex)));
	}

	SetInventoryWindowVisible(false);
	BindPlayerState(GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<APFPlayerState>() : nullptr);
}

void UPFInventoryWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateInventoryCooldowns();
	UpdateQuickSlotCooldowns();
	if (QuickSlotBorder)
	{
		const FVector2D ViewSize = MyGeometry.GetLocalSize();
		const FVector2D SlotSize = QuickSlotBorder->GetDesiredSize();
		if (ViewSize.X > 0.0 && ViewSize.Y > 0.0 && SlotSize.X > 0.0 && SlotSize.Y > 0.0)
		{
			const double Scale = FMath::Clamp(FMath::Min((ViewSize.X - 56.0) / SlotSize.X, (ViewSize.Y * .3) / SlotSize.Y), .25, 1.0);
			QuickSlotBorder->SetRenderScale(FVector2D(Scale));
		}
	}
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
		const FVector2D AnchorPosition = CanvasSize * WindowSlot->GetAnchors().Minimum;
		WindowSlot->SetPosition(LocalPosition - AnchorPosition + (WindowSize * WindowSlot->GetAlignment()));
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

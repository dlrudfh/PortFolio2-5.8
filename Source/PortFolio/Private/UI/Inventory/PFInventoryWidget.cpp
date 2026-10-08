#include "UI/Inventory/PFInventoryWidget.h"

#include "Character/PFCharacter.h"
#include "UI/Inventory/PFCooldownOverlayWidget.h"
#include "Props/PFItem.h"
#include "System/Framework/PFPlayerState.h"
#include "System/Framework/PFPlayerController.h"
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

	const FPFItemDefinition* Definition = APFItem::GetDefinition(ItemID);
	if (!Definition)
	{
		return nullptr;
	}

	UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, Definition->IconPath);
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
	checkf(RootCanvas && InventoryBorder && QuickSlotBorder && TitleBarBorder
		&& DragPreviewBorder && DragPreviewImage && DragPreviewCountText, TEXT("Required inventory widgets are missing"));

	BindSlotWidgets(InventorySlotWidgets, APFPlayerState::InventorySlotCount, TEXT("Inventory"));
	BindSlotWidgets(QuickSlotWidgets, APFPlayerState::QuickSlotCount, TEXT("Quick"));

	SetInventoryWindowVisible(false);
	BindPlayerState(GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<APFPlayerState>() : nullptr);
}

void UPFInventoryWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateSlotCooldowns();
	const FVector2D ViewSize = MyGeometry.GetLocalSize();
	const FVector2D SlotSize = QuickSlotBorder->GetDesiredSize();
	if (ViewSize.X > 0.0 && ViewSize.Y > 0.0 && SlotSize.X > 0.0 && SlotSize.Y > 0.0)
	{
		const double Scale = FMath::Clamp(FMath::Min((ViewSize.X - 56.0) / SlotSize.X, (ViewSize.Y * .3) / SlotSize.Y), .25, 1.0);
		QuickSlotBorder->SetRenderScale(FVector2D(Scale));
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
	UpdateSlotAppearance(false);
	UpdateSlotAppearance(true);
	UpdateSlotCooldowns();
}

// 인벤토리 창 표시 전환
void UPFInventoryWidget::SetInventoryWindowVisible(bool bVisible)
{
	if (!bVisible)
	{
		CancelDrag();
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
	return InventoryBorder->GetVisibility() == ESlateVisibility::Visible;
}

// 드래그한 창 위치를 화면 안으로 제한
void UPFInventoryWidget::UpdateInventoryWindowPosition(const FVector2D& ScreenSpacePosition)
{
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
	if (UCanvasPanelSlot* DragSlot = DragPreviewBorder ? Cast<UCanvasPanelSlot>(DragPreviewBorder->Slot) : nullptr)
	{
		DragSlot->SetPosition(RootCanvas->GetCachedGeometry().AbsoluteToLocal(ScreenSpacePosition));
	}
}

// 슬롯 이름으로 표시 요소 연결
void UPFInventoryWidget::BindSlotWidgets(TArray<FPFInventorySlotWidgets>& Slots, int32 Count, const TCHAR* Prefix)
{
	Slots.SetNum(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FPFInventorySlotWidgets& Widgets = Slots[Index];
		Widgets.DisplayedCooldownSeconds = INDEX_NONE;
		Widgets.Border = Cast<UBorder>(GetWidgetFromName(*FString::Printf(TEXT("%sSlot_%d"), Prefix, Index)));
		Widgets.Image = Cast<UImage>(GetWidgetFromName(*FString::Printf(TEXT("%sSlotImage_%d"), Prefix, Index)));
		Widgets.CountText = Cast<UTextBlock>(GetWidgetFromName(*FString::Printf(TEXT("%sSlotCount_%d"), Prefix, Index)));
		Widgets.CooldownOverlay = Cast<UPFCooldownOverlayWidget>(GetWidgetFromName(*FString::Printf(TEXT("%sCooldownOverlay_%d"), Prefix, Index)));
		Widgets.CooldownText = Cast<UTextBlock>(GetWidgetFromName(*FString::Printf(TEXT("%sCooldownText_%d"), Prefix, Index)));
		checkf(Widgets.Border && Widgets.Image && Widgets.CountText && Widgets.CooldownOverlay && Widgets.CooldownText, TEXT("Required inventory slot widgets are missing"));
	}
}

// 마우스 위치의 슬롯 탐색
int32 UPFInventoryWidget::FindSlotIndexAtScreenPosition(const TArray<FPFInventorySlotWidgets>& Slots, const FVector2D& ScreenSpacePosition) const
{
	return Slots.IndexOfByPredicate([&ScreenSpacePosition](const FPFInventorySlotWidgets& Widgets)
	{
		return Widgets.Border->GetCachedGeometry().IsUnderLocation(ScreenSpacePosition);
	});
}

// 슬롯의 아이템, 수량 조회
FPFInventorySlot UPFInventoryWidget::GetDisplayedSlot(int32 SlotIndex, bool bQuickSlots) const
{
	const APFPlayerState* PlayerState = CurrentPlayerState.Get();
	if (!PlayerState) return FPFInventorySlot();
	if (!bQuickSlots) return PlayerState->GetInventorySlots()[SlotIndex];

	const int32 ItemID = PlayerState->GetQuickSlotItemIDs()[SlotIndex];
	return FPFInventorySlot(ItemID, PlayerState->GetInventoryItemCount(ItemID));
}

// 슬롯 아이콘, 수량 갱신
void UPFInventoryWidget::UpdateSlotAppearance(bool bQuickSlots)
{
	const TArray<FPFInventorySlotWidgets>& Slots = bQuickSlots ? QuickSlotWidgets : InventorySlotWidgets;
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		const FPFInventorySlotWidgets& Widgets = Slots[Index];
		const FPFInventorySlot Item = GetDisplayedSlot(Index, bQuickSlots);
		const bool bHasItem = bQuickSlots ? Item.ItemID != RETURN_ERROR : !Item.IsEmpty();
		const bool bIsDraggedSlot = !bQuickSlots && bIsDraggingItem && DragSourceSlotIndex == Index;

		if (!bHasItem)
		{
			Widgets.Image->SetBrushFromTexture(nullptr, true);
			Widgets.Image->SetVisibility(ESlateVisibility::Hidden);
			Widgets.CountText->SetText(FText::GetEmpty());
			Widgets.CooldownOverlay->SetVisibility(ESlateVisibility::Hidden);
			Widgets.CooldownText->SetVisibility(ESlateVisibility::Hidden);
			continue;
		}

		Widgets.Image->SetBrushFromTexture(GetInventoryItemTexture(Item.ItemID), true);
		Widgets.Image->SetVisibility(bIsDraggedSlot ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
		Widgets.CountText->SetText(Item.Count > 1 ? FText::FromString(FString::Printf(TEXT("x%d"), Item.Count)) : FText::GetEmpty());
		if (!bQuickSlots) Widgets.CountText->SetVisibility(bIsDraggedSlot ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
	}
}

// 표시 중인 슬롯의 쿨타임 갱신
void UPFInventoryWidget::UpdateSlotCooldowns()
{
	const APFPlayerState* State = CurrentPlayerState.Get();
	TMap<int32, float, TInlineSetAllocator<4>> RemainingByItem;
	for (bool bQuickSlots : { false, true })
	{
		if (!bQuickSlots && !IsInventoryWindowVisible()) continue;
		TArray<FPFInventorySlotWidgets>& Slots = bQuickSlots ? QuickSlotWidgets : InventorySlotWidgets;
		for (int32 Index = 0; Index < Slots.Num(); ++Index)
		{
			FPFInventorySlotWidgets& Widgets = Slots[Index];
			int32 ItemID = RETURN_ERROR;
			if (State && bQuickSlots && State->GetQuickSlotItemIDs().IsValidIndex(Index)) ItemID = State->GetQuickSlotItemIDs()[Index];
			else if (State && !bQuickSlots && State->GetInventorySlots().IsValidIndex(Index))
			{
				const FPFInventorySlot& Item = State->GetInventorySlots()[Index];
				if (!Item.IsEmpty() && !(bIsDraggingItem && DragSourceSlotIndex == Index)) ItemID = Item.ItemID;
			}
			float Remaining = 0.f;
			if (ItemID != RETURN_ERROR)
			{
				float* Cached = RemainingByItem.Find(ItemID);
				if (!Cached) Cached = &RemainingByItem.Add(ItemID, State->GetItemCooldownRemaining(ItemID));
				Remaining = *Cached;
			}
			Widgets.CooldownOverlay->SetCooldownProgress(Remaining / APFPlayerState::ItemCooldownDuration);
			Widgets.CooldownOverlay->SetVisibility(Remaining > 0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
			Widgets.CooldownText->SetVisibility(Remaining > 0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
			const int32 Seconds = FMath::CeilToInt(Remaining);
			if (Widgets.DisplayedCooldownSeconds != Seconds)
			{
				Widgets.DisplayedCooldownSeconds = Seconds;
				Widgets.CooldownText->SetText(Seconds > 0 ? FText::AsNumber(Seconds) : FText::GetEmpty());
			}
		}
	}
}

// 드래그할 아이템 아이콘, 수량 표시
void UPFInventoryWidget::RebuildDragPreview(int32 ItemID, int32 ItemCount)
{
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
	DragPreviewImage->SetBrushFromTexture(nullptr, true);
	DragPreviewCountText->SetText(FText::GetEmpty());
	DragPreviewBorder->SetVisibility(ESlateVisibility::Hidden);
}

// 드래그 취소, 마우스 캡처 해제
void UPFInventoryWidget::CancelDrag()
{
	const bool bWasDragging = bIsDraggingInventory || bIsDraggingItem;
	bIsDraggingInventory = false;
	bIsDraggingItem = false;
	DragSourceSlotIndex = INDEX_NONE;
	ClearDragPreview();
	UpdateSlotAppearance(false);
	UpdateSlotCooldowns();

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
	const APFPlayerController* Player = Cast<APFPlayerController>(GetOwningPlayer());
	if (!Player || !Player->CanUseUIInput())
	{
		CancelDrag();
		return FReply::Handled();
	}
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	const FVector2D ScreenSpacePosition = InMouseEvent.GetScreenSpacePosition();

	// 제목 표시줄에서 창 드래그 시작
	if (IsInventoryWindowVisible() && TitleBarBorder->GetCachedGeometry().IsUnderLocation(ScreenSpacePosition))
	{
		bIsDraggingInventory = true;
		InventoryDragOffset = ScreenSpacePosition - InventoryBorder->GetCachedGeometry().GetAbsolutePosition();
		return FReply::Handled().CaptureMouse(TakeWidget());
	}

	if (!CurrentPlayerState.IsValid() || !IsInventoryWindowVisible())
	{
		return FReply::Unhandled();
	}

	const int32 SlotIndex = FindSlotIndexAtScreenPosition(InventorySlotWidgets, ScreenSpacePosition);
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
	UpdateSlotAppearance(false);
	UpdateSlotCooldowns();
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
	const APFPlayerController* Player = Cast<APFPlayerController>(GetOwningPlayer());
	if (!Player || !Player->CanUseUIInput())
	{
		CancelDrag();
		return FReply::Handled();
	}
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
			const int32 QuickSlotIndex = FindSlotIndexAtScreenPosition(QuickSlotWidgets, DropScreenPosition);
			const TArray<FPFInventorySlot>& InventorySlots = CurrentPlayerState->GetInventorySlots();

			if (QuickSlotIndex != INDEX_NONE && InventorySlots.IsValidIndex(DragSourceSlotIndex) && !InventorySlots[DragSourceSlotIndex].IsEmpty())
			{
				CurrentPlayerState->AssignQuickSlot(QuickSlotIndex, InventorySlots[DragSourceSlotIndex].ItemID);
			}
			else
			{
				const int32 DropInventorySlotIndex = FindSlotIndexAtScreenPosition(InventorySlotWidgets, DropScreenPosition);
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
	const APFPlayerController* Player = Cast<APFPlayerController>(GetOwningPlayer());
	if (!Player || !Player->CanUseUIInput())
	{
		CancelDrag();
		return FReply::Handled();
	}
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
	const APFPlayerController* Player = Cast<APFPlayerController>(GetOwningPlayer());
	if (!Player || !Player->CanUseUIInput())
	{
		CancelDrag();
		return FReply::Handled();
	}
	if (!IsInventoryWindowVisible() || InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !CurrentPlayerState.IsValid())
	{
		return Super::NativeOnMouseButtonDoubleClick(InGeometry, InMouseEvent);
	}

	const int32 SlotIndex = FindSlotIndexAtScreenPosition(InventorySlotWidgets, InMouseEvent.GetScreenSpacePosition());
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

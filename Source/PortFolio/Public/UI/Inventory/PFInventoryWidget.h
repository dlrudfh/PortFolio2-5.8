#pragma once

#include "PortFolio/PortFolio.h"

#include "Blueprint/UserWidget.h"
#include "PFInventoryWidget.generated.h"

class UTexture2D;

// 인벤토리, 퀵슬롯 위젯
UCLASS()
class PORTFOLIO_API UPFInventoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void BindPlayerState(class APFPlayerState* NewPlayerState);

	void RefreshInventory();

	void SetInventoryWindowVisible(bool bVisible);

	bool IsInventoryWindowVisible() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnFocusLost(const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	static constexpr int32 InventoryColumnCount = 5;

	static constexpr int32 QuickSlotColumnCount = 4;

	static constexpr float SlotSize = 84.f;

	void UpdateInventoryWindowPosition(const FVector2D& ScreenSpacePosition);

	void UpdateDragPreviewPosition(const FVector2D& ScreenSpacePosition);

	int32 FindInventorySlotIndexAtScreenPosition(const FVector2D& ScreenSpacePosition) const;

	int32 FindQuickSlotIndexAtScreenPosition(const FVector2D& ScreenSpacePosition) const;

	void UpdateInventorySlotAppearance();

	void UpdateQuickSlotAppearance();
	void UpdateInventoryCooldowns();
	void UpdateQuickSlotCooldowns();

	void RebuildDragPreview(int32 ItemID, int32 ItemCount);

	void ClearDragPreview();
	void CancelDrag();
	UTexture2D* GetInventoryItemTexture(int32 ItemID);

	FReply HandleInventoryMouseButtonDown(const FPointerEvent& InMouseEvent);

private:
	// 소지품 데이터를 구독한 PlayerState
	TWeakObjectPtr<class APFPlayerState> CurrentPlayerState;

	// 아이템 아이콘 캐시
	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UTexture2D>> ItemIconCache;

	// 창, 퀵슬롯 배치 영역
	UPROPERTY()
	class UCanvasPanel* RootCanvas = nullptr;

	UPROPERTY()
	class UBorder* InventoryBorder = nullptr;

	UPROPERTY()
	class UBorder* QuickSlotBorder = nullptr;

	UPROPERTY()
	class UBorder* TitleBarBorder = nullptr;

	UPROPERTY()
	class UTextBlock* TitleText = nullptr;

	UPROPERTY()
	class UUniformGridPanel* InventoryGrid = nullptr;

	UPROPERTY()
	class UUniformGridPanel* QuickSlotGrid = nullptr;

	// 인벤토리 슬롯 표시 요소
	UPROPERTY()
	TArray<class UBorder*> InventorySlotBorders;

	UPROPERTY()
	TArray<class UImage*> InventorySlotImages;

	UPROPERTY()
	TArray<class UTextBlock*> InventorySlotCountTexts;

	// 인벤토리 쿨타임 표시
	UPROPERTY()
	TArray<class UPFCooldownOverlayWidget*> InventoryCooldownOverlays;

	UPROPERTY()
	TArray<class UTextBlock*> InventoryCooldownTexts;

	// 퀵슬롯 표시 요소
	UPROPERTY()
	TArray<class UBorder*> QuickSlotBorders;

	UPROPERTY()
	TArray<class UImage*> QuickSlotImages;

	UPROPERTY()
	TArray<class UTextBlock*> QuickSlotCountTexts;

	// 퀵슬롯 쿨타임 표시
	UPROPERTY()
	TArray<class UPFCooldownOverlayWidget*> QuickSlotCooldownOverlays;

	UPROPERTY()
	TArray<class UTextBlock*> QuickSlotCooldownTexts;

	UPROPERTY()
	TArray<class UTextBlock*> QuickSlotKeyTexts;

	// 드래그 중인 아이템 표시
	UPROPERTY()
	class UBorder* DragPreviewBorder = nullptr;

	UPROPERTY()
	class UImage* DragPreviewImage = nullptr;

	UPROPERTY()
	class UTextBlock* DragPreviewCountText = nullptr;

	bool bIsDraggingInventory = false;

	bool bIsDraggingItem = false;

	int32 DragSourceSlotIndex = INDEX_NONE;

	FVector2D InventoryDragOffset = FVector2D::ZeroVector;
};

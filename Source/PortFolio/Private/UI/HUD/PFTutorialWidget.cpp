#include "UI/HUD/PFTutorialWidget.h"
#include "UI/PFWidgetHelpers.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"

UPFTutorialWidget::UPFTutorialWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FClassFinder<UUserWidget> Layout(TEXT("/Game/GameData/UI/Tutorial/WBP_Tutorial"));
	LayoutClass = Layout.Class;
}

TSharedRef<SWidget> UPFTutorialWidget::RebuildWidget()
{
	if (!LayoutWidget && LayoutClass && GetOwningPlayer())
	{
		LayoutWidget = CreateWidget<UUserWidget>(GetOwningPlayer(), LayoutClass);
	}
	if (!LayoutWidget) return Super::RebuildWidget();

	TitleText = PFWidgetHelpers::Find<UTextBlock>(LayoutWidget, TEXT("TitleText"), true);
	ObjectiveText = PFWidgetHelpers::Find<UTextBlock>(LayoutWidget, TEXT("ObjectiveText"), true);
	ProgressBar = PFWidgetHelpers::Find<UProgressBar>(LayoutWidget, TEXT("ProgressBar"), true);
	WorldMarker = PFWidgetHelpers::Find<UWidget>(LayoutWidget, TEXT("WorldMarker"), true);
	MarkerDiamond = PFWidgetHelpers::Find<UWidget>(LayoutWidget, TEXT("MarkerDiamond"), true);
	MarkerArrow = PFWidgetHelpers::Find<UWidget>(LayoutWidget, TEXT("MarkerArrow"), true);
	MarkerText = PFWidgetHelpers::Find<UTextBlock>(LayoutWidget, TEXT("MarkerText"), true);
	HighlightFrame = PFWidgetHelpers::Find<UWidget>(LayoutWidget, TEXT("HighlightFrame"), true);
	HighlightHint = PFWidgetHelpers::Find<UWidget>(LayoutWidget, TEXT("HighlightHint"), true);
	HighlightText = PFWidgetHelpers::Find<UTextBlock>(LayoutWidget, TEXT("HighlightText"), true);
	checkf(Cast<UCanvasPanelSlot>(WorldMarker->Slot) && Cast<UCanvasPanelSlot>(HighlightFrame->Slot)
		&& Cast<UCanvasPanelSlot>(HighlightHint->Slot), TEXT("Tutorial markers require canvas slots"));
	WorldMarker->SetVisibility(ESlateVisibility::Collapsed);
	HighlightFrame->SetVisibility(ESlateVisibility::Collapsed);
	HighlightHint->SetVisibility(ESlateVisibility::Collapsed);
	LastTargetScreen = FVector2D::ZeroVector;
	LastMarkerDistance = INDEX_NONE;
	bHighlightLayoutDirty = true;
	RefreshGuide();
	return LayoutWidget->TakeWidget();
}

void UPFTutorialWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	if (bReleaseChildren && LayoutWidget) LayoutWidget->ReleaseSlateResources(true);
}

// 관리자가 정한 안내 내용 반영
void UPFTutorialWidget::SetGuide(const FString& Title, const FString& Objective, float Progress, bool bComplete)
{
	bShowComplete = bComplete;
	if (GuideTitle == Title && GuideObjective == Objective && GuideProgress == Progress) return;
	GuideTitle = Title;
	GuideObjective = Objective;
	GuideProgress = Progress;
	if (TitleText) RefreshGuide();
}

// 월드 마커 대상 설정
void UPFTutorialWidget::SetWorldMarker(AActor* Target, const FVector& Location, bool bShowLocation, const FString& Caption)
{
	MarkerActor = Target;
	MarkerLocation = Location;
	bHasMarkerLocation = bShowLocation;
	if (MarkerCaption != Caption) LastMarkerDistance = INDEX_NONE;
	MarkerCaption = Caption;
}

// 기존 UI의 강조 대상 설정
void UPFTutorialWidget::SetHighlight(UWidget* Target, const FString& Caption)
{
	HighlightWidget = Target;
	if (HighlightCaption != Caption) bHighlightLayoutDirty = true;
	HighlightCaption = Caption;
}

void UPFTutorialWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!LayoutWidget) return;
	UpdateTargetWidgets(MyGeometry);
}

// 디자이너 패널의 문구, 진행도 반영
void UPFTutorialWidget::RefreshGuide()
{
	PFWidgetHelpers::SetTextIfChanged(TitleText, FText::FromString(GuideTitle));
	PFWidgetHelpers::SetTextIfChanged(ObjectiveText, FText::FromString(GuideObjective));
	ProgressBar->SetPercent(GuideProgress);
}

// 디자이너 마커, 강조 위젯의 위치 갱신
void UPFTutorialWidget::UpdateTargetWidgets(const FGeometry& Geometry)
{
	const FVector2D Screen = Geometry.GetLocalSize();
	const bool bScreenReady = !bShowComplete && Screen.X >= 100.f && Screen.Y >= 100.f;
	const bool bHighlightSelected = bScreenReady && HighlightWidget.IsValid() && HighlightWidget->IsVisible();
	const bool bShowHighlight = bHighlightSelected && !HighlightWidget->GetCachedGeometry().GetLocalSize().IsNearlyZero();
	APlayerController* Controller = GetOwningPlayer();
	const bool bShowMarker = bScreenReady && !bHighlightSelected && (bHasMarkerLocation || MarkerActor.IsValid())
		&& Controller && Controller->GetPawn();
	const bool bResize = LastTargetScreen != Screen;
	LastTargetScreen = Screen;
	const bool bPrepassHighlight = bResize || bHighlightLayoutDirty || !HighlightHint->IsVisible();
	const bool bPrepassMarker = bResize || !WorldMarker->IsVisible();
	WorldMarker->SetVisibility(bShowMarker ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	HighlightFrame->SetVisibility(bShowHighlight ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	HighlightHint->SetVisibility(bShowHighlight && !HighlightCaption.IsEmpty()
		? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	if (bShowHighlight)
	{
		const FGeometry& TargetGeometry = HighlightWidget->GetCachedGeometry();
		UCanvasPanelSlot* FrameSlot = Cast<UCanvasPanelSlot>(HighlightFrame->Slot);
		UCanvasPanelSlot* HintSlot = Cast<UCanvasPanelSlot>(HighlightHint->Slot);

		const FVector2D Min = Geometry.AbsoluteToLocal(TargetGeometry.LocalToAbsolute(FVector2D::ZeroVector)) - FVector2D(4.f);
		const FVector2D Max = Geometry.AbsoluteToLocal(TargetGeometry.LocalToAbsolute(TargetGeometry.GetLocalSize())) + FVector2D(4.f);
		FrameSlot->SetPosition(Min);
		FrameSlot->SetSize(Max - Min);
		if (HighlightCaption.IsEmpty()) return;
		if (bHighlightLayoutDirty)
		{
			PFWidgetHelpers::SetTextIfChanged(HighlightText, FText::FromString(HighlightCaption));
			bHighlightLayoutDirty = false;
		}
		if (bPrepassHighlight) HighlightHint->ForceLayoutPrepass();
		const FVector2D CaptionSize = HighlightHint->GetDesiredSize();
		FVector2D Position(Max.X + 12.f, Min.Y);
		if (Position.X + CaptionSize.X > Screen.X - 12.f) Position = FVector2D(Min.X - CaptionSize.X - 12.f, Min.Y);
		if (Position.X < 12.f) Position = FVector2D(Min.X, Max.Y + 12.f);
		Position.X = FMath::Clamp(Position.X, 12.0, FMath::Max(12.0, Screen.X - CaptionSize.X - 12.0));
		Position.Y = FMath::Clamp(Position.Y, 12.0, FMath::Max(12.0, Screen.Y - CaptionSize.Y - 12.0));
		HintSlot->SetPosition(Position);
		return;
	}
	if (!bShowMarker) return;
	UCanvasPanelSlot* MarkerSlot = Cast<UCanvasPanelSlot>(WorldMarker->Slot);

	const FVector Target = MarkerActor.IsValid() ? MarkerActor->GetActorLocation() + FVector(0.f, 0.f, 100.f) : MarkerLocation + FVector(0.f, 0.f, 45.f);
	FVector2D Point;
	const bool bProjected = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(Controller, Target, Point, true);
	const bool bOnScreen = bProjected && Point.X >= 36.f && Point.X <= Screen.X - 36.f && Point.Y >= 210.f && Point.Y <= Screen.Y - 80.f;
	if (!bOnScreen)
	{
		FVector CameraLocation;
		FRotator CameraRotation;
		Controller->GetPlayerViewPoint(CameraLocation, CameraRotation);
		const FVector CameraDirection = CameraRotation.UnrotateVector(Target - CameraLocation);
		FVector2D Direction = bProjected ? Point - Screen * .5f : FVector2D(CameraDirection.Y, -CameraDirection.Z);
		if (!Direction.Normalize()) Direction = FVector2D(0.f, 1.f);
		Point = Screen * .5f + Direction * FMath::Min(Screen.X, Screen.Y);
		Point.Y = FMath::Clamp(Point.Y, FMath::Min(210.0, Screen.Y * .4), Screen.Y - 80.0);
		MarkerArrow->SetRenderTransformAngle(FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X)) + 90.f);
	}
	const bool bMarkerShapeChanged = MarkerDiamond->IsVisible() != bOnScreen;
	MarkerDiamond->SetVisibility(bOnScreen ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	MarkerArrow->SetVisibility(bOnScreen ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	const int32 Distance = FMath::RoundToInt(FVector::Distance(Controller->GetPawn()->GetActorLocation(), Target) / 100.f);
	const bool bTextChanged = Distance != LastMarkerDistance;
	if (bTextChanged)
	{
		LastMarkerDistance = Distance;
		PFWidgetHelpers::SetTextIfChanged(MarkerText, FText::FromString(FString::Printf(TEXT("%s  %dm"), *MarkerCaption, Distance)));
	}
	if (bPrepassMarker || bTextChanged || bMarkerShapeChanged) WorldMarker->ForceLayoutPrepass();
	const double HalfWidth = WorldMarker->GetDesiredSize().X * .5;
	Point.X = FMath::Clamp(Point.X, HalfWidth + 12.0, FMath::Max(HalfWidth + 12.0, Screen.X - HalfWidth - 12.0));
	MarkerSlot->SetPosition(Point);
}

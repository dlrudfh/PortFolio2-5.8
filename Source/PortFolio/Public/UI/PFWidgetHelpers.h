#pragma once

#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"

namespace PFWidgetHelpers
{
	inline void SetTextIfChanged(UTextBlock* Widget, const FText& Text)
	{
		if (Widget->GetText().EqualTo(Text)) return;
		Widget->SetText(Text);
	}

	template<typename WidgetType>
	WidgetType* Find(UUserWidget* Layout, FName Name, bool bRequired = false)
	{
		WidgetType* Widget = Cast<WidgetType>(Layout->GetWidgetFromName(Name));
		checkf(!bRequired || Widget, TEXT("Required widget missing: %s / %s"), *Layout->GetName(), *Name.ToString());
		return Widget;
	}
}

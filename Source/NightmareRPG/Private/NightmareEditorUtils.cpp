#include "NightmareEditorUtils.h"

#if WITH_EDITOR
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#endif

int32 UNightmareEditorUtils::EnsureWidgetVariableGuids(UObject* WidgetBlueprint)
{
	int32 Added = 0;
#if WITH_EDITOR
	UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(WidgetBlueprint);
	if (!WidgetBP || !WidgetBP->WidgetTree)
	{
		return 0;
	}

	WidgetBP->Modify();
	WidgetBP->WidgetTree->ForEachWidget([WidgetBP, &Added](UWidget* Widget)
	{
		if (Widget && !WidgetBP->WidgetVariableNameToGuidMap.Contains(Widget->GetFName()))
		{
			WidgetBP->OnVariableAdded(Widget->GetFName());
			++Added;
		}
	});
#endif
	return Added;
}

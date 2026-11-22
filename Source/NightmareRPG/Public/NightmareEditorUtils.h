#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NightmareEditorUtils.generated.h"

/**
 * 에디터 전용 유틸리티 (Python 에디터 스크립트에서 호출).
 * 패키지 빌드에서는 아무 동작도 하지 않음.
 */
UCLASS()
class NIGHTMARERPG_API UNightmareEditorUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 스크립트로 위젯 트리에 추가한 위젯은 WidgetVariableNameToGuidMap 항목이 없어
	 * 컴파일 시 ensure가 발생함. 디자이너에서 위젯을 추가할 때와 동일하게 GUID를 등록.
	 * @return 새로 GUID를 등록한 위젯 수
	 */
	UFUNCTION(BlueprintCallable, Category = "Nightmare|Editor", meta = (DevelopmentOnly))
	static int32 EnsureWidgetVariableGuids(UObject* WidgetBlueprint);
};

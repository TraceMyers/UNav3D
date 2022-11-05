#pragma once

#include "CoreMinimal.h"
#include "Editor/Blutility/Classes/EditorUtilityWidget.h"
#include "UNavUI.generated.h"

// NOTE: requires editor utility scripting plugin
class ADraw;

UCLASS(BlueprintType)
class UNAV3D_API UNavUI : public UEditorUtilityWidget
{
	GENERATED_BODY()

public:
	UNavUI();
	~UNavUI();

	virtual void PostLoad() override;

	UFUNCTION(BlueprintCallable)
	void HideAndShowAllStaticMeshes();

	UFUNCTION(BlueprintCallable)
	void HideAndShowAllNavMeshes();

	UPROPERTY()
	ADraw* Draw;

private:

	
};
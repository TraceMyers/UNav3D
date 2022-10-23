#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DebugMarker.generated.h"

/*
 * For visualizing points in space in-editor
 */

UCLASS()
class UNAV3D_API ADebugMarker : public AActor {
	GENERATED_BODY()
	
public:	
	ADebugMarker();
	// For allowing Tick in Editor
	virtual bool ShouldTickIfViewportsOnly() const override;
	virtual void Tick(float DeltaTime) override;
	
	static void Spawn(UWorld* World, const FVector& Location, float LifeSpan=1.0f, float Scale=1.0f);
	
	UPROPERTY(BlueprintReadWrite, VisibleDefaultsOnly)
	UStaticMeshComponent* MeshCmp;

protected:
	virtual void BeginPlay() override;

private:
	
	// using a counter instead of SetLifeSpan() because we can't call it in constructor and BeginPlay doesn't run in editor
	float Ctr;

};

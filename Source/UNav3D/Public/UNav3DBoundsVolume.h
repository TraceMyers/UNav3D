#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UNav3DBoundsVolume.generated.h"

class GeometryProcessor;
class UBoxComponent;

UCLASS()
class UNAV3D_API AUNav3DBoundsVolume : public AActor {
	GENERATED_BODY()
	
public:	

	AUNav3DBoundsVolume();
	virtual void Tick(float DeltaTime) override;
	TArray<AActor*> GetOverlappingStaticMeshes();
	
	UPROPERTY(BlueprintReadWrite, VisibleDefaultsOnly)
	UStaticMeshComponent* BoundsMesh;
	UPROPERTY(BlueprintReadWrite, VisibleDefaultsOnly)
	UBoxComponent* BoundsBox;
	UPROPERTY()
	TArray<AActor*> OverlappingStaticMeshes;

protected:

	virtual void BeginPlay() override;

private:


};

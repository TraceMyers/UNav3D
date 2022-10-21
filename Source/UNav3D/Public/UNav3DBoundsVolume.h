#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UNav3DBoundsVolume.generated.h"

UCLASS()
class UNAV3D_API AUNav3DBoundsVolume : public AActor {
	GENERATED_BODY()
	
public:	

	AUNav3DBoundsVolume();
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(BlueprintReadWrite, VisibleDefaultsOnly)
	UStaticMeshComponent* BoundsMesh;

	FVector GetMaximum();
	FVector GetMinimum();

protected:

	virtual void BeginPlay() override;

private:	


};

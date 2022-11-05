#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "Draw.generated.h"

struct UNavMesh;

UCLASS()
class UNAV3D_API ADraw : public AActor {
	GENERATED_BODY()
	
public:	
	ADraw();
	
	void Init();
	
	void HideAndShowAllNavMeshes();

	UPROPERTY()
	UMaterial* TriMaterial;
	UPROPERTY(VisibleAnywhere)
	UProceduralMeshComponent* NavMeshes;
	
protected:
	
	virtual void BeginPlay() override;

private:

	TArray<TArray<AStaticMeshActor*>> ActorMemory;

	void AddNavMesh(UNavMesh& NMesh);
	
};

#pragma once

#include "CoreMinimal.h"
#include "BoundingBox.h"
#include "Engine/StaticMeshActor.h"
#include "VertexCapture.generated.h"

// Used for debugging, to check if a vertex is inside its bbox
UCLASS()
class AVertexCapture : public AStaticMeshActor
{
	GENERATED_BODY()
	
public:
	
	AVertexCapture();
	virtual void Tick(float DeltaTime) override;

	BoundingBox& GetBBox();

protected:
	
	virtual void BeginPlay() override;

private:

	BoundingBox Box;

};

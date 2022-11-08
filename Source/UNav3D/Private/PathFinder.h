#pragma once

#include "CoreMinimal.h"

class UNAV3D_API FPathFinder : public FRunnable {

public:

	FPathFinder();
	virtual ~FPathFinder() override;
	
	bool StartThread();
	void StopThread();
	
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;

private:

	TUniquePtr<FRunnableThread> Thread;
	bool IsThreadRun;

};

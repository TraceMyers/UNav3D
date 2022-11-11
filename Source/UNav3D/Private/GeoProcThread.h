#pragma once

#include "CoreMinimal.h"
#include "GeometryProcessor.h"
#include "HAL/RunnableThread.h"

class FGeoProcThread : FRunnable {
	
public:

	enum GEOPROC_THREAD_TASK {
		GEOPROC_NONE,
		GEOPROC_REFORM,
		GEOPROC_SIMPLIFY
	};

	static const int GEOPROC_THREAD_FAIL = UINT32_MAX;
	
	FGeoProcThread();
	virtual ~FGeoProcThread() override;
	
	bool StartThread(GEOPROC_THREAD_TASK Task, FThreadSafeBool* TaskFinished, FCriticalSection* Mutex);
	void StopThread();

	void InitSimplify(TArray<TArray<Tri*>>* Batches, TriMesh* TMesh);
	void InitReformTMesh(TArray<TriMesh*>* TMeshes, UNavMesh* NMesh);
	
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;

private:

	FRunnableThread* Thread;
	GeometryProcessor GeoProc;
	FCriticalSection* Mutex;
	FThreadSafeBool IsThreadRun;
	
	GeometryProcessor::GEOPROC_RESPONSE Response;
	FThreadSafeBool* TaskFinished;
	GEOPROC_THREAD_TASK Task;

	TArray<TArray<Tri*>>* Batches;
	TriMesh* TMesh;
	TArray<TriMesh*>* TMeshGroup;
	UNavMesh* NMesh;
};
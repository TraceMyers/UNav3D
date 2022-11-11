#include "GeoProcThread.h"

FGeoProcThread::FGeoProcThread() :
	Thread(nullptr),
	Mutex(nullptr),
	Response(GeometryProcessor::GEOPROC_SUCCESS),
	IsThreadRun(false),
	TaskFinished(nullptr),
	Task(GEOPROC_NONE),
	TMeshGroup(nullptr),
	NMesh(nullptr)
{}

FGeoProcThread::~FGeoProcThread() {
	StopThread();
}

bool FGeoProcThread::StartThread(GEOPROC_THREAD_TASK _Task, FThreadSafeBool* _TaskFinished, FCriticalSection* _Mutex) {
	if (Thread == nullptr) {
		Task = _Task;
		TaskFinished = _TaskFinished;
		Mutex = _Mutex;
		IsThreadRun = true;
		Thread = FRunnableThread::Create(this, TEXT("Grid Generator"));
		if (Thread != nullptr) {
			return true;
		}
	}
	return false;
}

void FGeoProcThread::StopThread() {
	if (Thread != nullptr) {
		Thread->Kill();
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}	
	IsThreadRun = false;
}

void FGeoProcThread::InitSimplify(TArray<TArray<Tri*>>* _Batches, TriMesh* _TMesh) {
	Batches = _Batches;
	TMesh = _TMesh;
}

void FGeoProcThread::InitReformTMesh(TArray<TriMesh*>* Group, UNavMesh* _NMesh) {
	TMeshGroup = Group;
	NMesh = _NMesh;
}

#pragma endregion

bool FGeoProcThread::Init() {
	return true;
}

uint32 FGeoProcThread::Run() {
	uint32 RetVal = 0;
	
	switch(Task) {
	case GEOPROC_REFORM:
		if (TMeshGroup == nullptr || Mutex == nullptr || NMesh == nullptr) {
			RetVal = -1;
		}
		else {
			GeoProc.ReformTriMesh(TMeshGroup, Mutex, &IsThreadRun, NMesh);
			TMeshGroup = nullptr;
			NMesh = nullptr;
		}
		break;
	default:
		;
	}

	Mutex = nullptr;
	IsThreadRun = false;
	TaskFinished->AtomicSet(true);
	return RetVal;
}

void FGeoProcThread::Stop() {
	IsThreadRun = false;
}
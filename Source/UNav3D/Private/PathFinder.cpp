#include "PathFinder.h"


FPathFinder::FPathFinder() :
	IsThreadRun(false)
{}

FPathFinder::~FPathFinder() {
	StopThread();
}

bool FPathFinder::StartThread() {
	if (!Thread.IsValid()) {
		Thread = TUniquePtr<FRunnableThread>(FRunnableThread::Create(this, TEXT("Grid Generator")));
		if (Thread.IsValid()) {
			return true;
		}
	}
	return false;
}

void FPathFinder::StopThread() {
	if (Thread.IsValid()) {
		FRunnableThread* RawThread = Thread.Get();
		RawThread->Kill();
		RawThread->WaitForCompletion();
		Thread.Reset();
	}	
}

#pragma endregion

bool FPathFinder::Init() {
	return true;
}

uint32 FPathFinder::Run() {
	
	return 0;
}

void FPathFinder::Stop() {
	// code goes here
	FRunnable::Stop();
}
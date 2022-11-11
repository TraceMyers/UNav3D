#include "DataProcessing.h"
#include "Misc/MessageDialog.h"
#include "ToolMenus.h"
#include "UNav3DBoundsVolume.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ScopedSlowTask.h"
#include "Debug.h"
#include "TriMesh.h"
#include "Data.h"
#include <thread>
#include <chrono>
#include "GeoProcThread.h"

#define LOCTEXT_NAMESPACE "UNav3D"

namespace {

	constexpr int TOTAL_RESET_TASK_CT = 3;
	
	constexpr int MAX_THREAD_CT = 8;
	constexpr int WAIT_SUCCESS = 0;
	constexpr int WAIT_FAILURE = -1;
	constexpr int MAX_START_THREAD_ATTEMPTS = 300;
	
	int MaxRunningThreadCt = 4;

	FCriticalSection DataProcMutex;
	GeometryProcessor GProc;
	FGeoProcThread* GProcThreads[MAX_THREAD_CT] {nullptr};
	FThreadSafeBool IsGProcTAvail[MAX_THREAD_CT] {true};
	
}

namespace {

	bool TryStartThread(FGeoProcThread::GEOPROC_THREAD_TASK Task, int ThreadIndex) {
		constexpr int WAIT_MS = 10;

		FThreadSafeBool* PP = IsGProcTAvail;
		
		for (int AttemptCt = 0; AttemptCt < MAX_START_THREAD_ATTEMPTS; AttemptCt++) {
			if (GProcThreads[ThreadIndex]->StartThread(Task, &IsGProcTAvail[ThreadIndex], &DataProcMutex)) {
				return true;	
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_MS));
		}
		return false;
	}

	void GProcThreadsStop() {
		for (int i = 0; i < MAX_THREAD_CT; i++) {
			if (GProcThreads[i] != nullptr) {
				GProcThreads[i]->StopThread();
				IsGProcTAvail[i].AtomicSet(true);
			}
		}
	}

	int GProcWaitForAll(float WaitSeconds) {
		constexpr int WAIT_MS = 100;
		constexpr float WAIT_DELTA = WAIT_MS * 1e-3f;
		
		for (float WaitCtr = 0.0f; WaitCtr < WaitSeconds; WaitCtr += WAIT_DELTA) {
			bool AllAvail = true;
			for (int i = 0; i < MaxRunningThreadCt; i++) {
				if (!IsGProcTAvail[i]) {
					AllAvail = false;
					break;;
				}
			}
			if (AllAvail) {
				return WAIT_SUCCESS;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_MS));
		}
		return WAIT_FAILURE;
	}

	int GProcWaitForAvailable(float WaitSeconds) {
		constexpr int WAIT_MS = 10;
		constexpr float WAIT_DELTA = WAIT_MS * 1e-3f;
		
		for (float WaitCtr = 0.0f; WaitCtr < WaitSeconds; WaitCtr += WAIT_DELTA) {
			for (int i = 0; i < MaxRunningThreadCt; i++) {
				if (IsGProcTAvail[i]) {
					GProcThreads[i]->StopThread();
					return i;
				}
			}
            std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_MS));
		}
		return WAIT_FAILURE;
	}

	// Finds bounds volume in editor viewport; returns false if number of bounds volumes is not 1
	bool SetBoundsVolume(const UWorld* World) {
		TArray<AActor*> FoundActors;
		UGameplayStatics::GetAllActorsOfClass(
			World,
			AUNav3DBoundsVolume::StaticClass(),
			FoundActors
		);
		const int BoundsVolumeCt = FoundActors.Num();
		if (BoundsVolumeCt == 0) {
			UNAV_GENERR("No bounds volumes found.")
			return false;
		}
		if (BoundsVolumeCt > 1) {
			UNAV_GENERR("UNav3D Currently only supports one Navigation Volume.")
			return false;
		}
		Data::BoundsVolume = Cast<AUNav3DBoundsVolume>(FoundActors[0]);
		return true;
	}

	// TODO: Make an error log for more informative errors
	bool BatchTris(const TriMesh& TMesh, TArray<TArray<Tri*>>& Batches) {
		constexpr static uint16 BATCH_SZ = 2048;
		uint16 MeshBatch = 1;
		if ((float)TMesh.Grid.Num() / BATCH_SZ >= (float)UINT16_MAX) {
			UNAV_GENERR("Batch size too small for one or more of the meshes.")
			return false;
		}
		int StartTriIndex = 0;
		while (StartTriIndex >= 0) {
			const int BatchCt = GProc.GetMeshBatch(
				Batches, TMesh, StartTriIndex, 2048, MeshBatch++
			);
			if (BatchCt == 0) {
				UNAV_GENERR("Got a batch count of zero, which might indicate a problem with a mesh.")
				return false;
			}
		}	
		return true;
	}

	// Takes meshes found inside bounds volume and populates TriMeshes with their data
	// This must run in the game thread, since access to mesh data is only allowed there
	bool PopulateTriMeshes(TArray<TriMesh>& TMeshes) {
		Data::BoundsVolume->GetOverlappingMeshes(TMeshes);
		if (TMeshes.Num() == 0) {
			UNAV_GENERR("No static mesh actors found inside the bounds volume.")
			return false;
		}

		{
			// populating the bounds volume tmesh for intersection testing in GeomProcessor.PopulateNavMeshes()
			if (GProc.PopulateTriMesh(Data::BoundsVolumeTMesh) != GeometryProcessor::GEOPROC_SUCCESS) {
				UNAV_GENERR("Bounds volume mesh was not populated correctly.")
				return false;
			}
		}

		// getting geometry data and populating the TriMeshes with it
		for (int i = 0; i < TMeshes.Num(); i++) {
			TriMesh& TMesh = TMeshes[i];
			GeometryProcessor::GEOPROC_RESPONSE Response = GProc.PopulateTriMesh(TMesh);
			if (Response == GeometryProcessor::GEOPROC_HIGH_INDEX) {
				UNAV_GENERR("One of the meshes was not processed correctly due to a bad index coming from the index buffer")
			}
			else if (Response == GeometryProcessor::GEOPROC_ALLOC_FAIL) {
				UNAV_GENERR("The Geometry Processor failed to allocate enough space for a mesh.")
			}
	#ifdef UNAV_DBG
			UNavDbg::PrintTriMesh(TMesh);
	#endif

			TArray<TArray<Tri*>> Batches;
			if (!BatchTris(TMesh, Batches)) {
				return false;
			}
			// TODO: thread simplify
		}
		
		return true;
	}

	bool ReformTriMeshes(TArray<TArray<TriMesh*>>& Groups) {
		Data::NMeshes.Init(UNavMesh(), Groups.Num());
		for (int i = 0; i < Groups.Num(); i++) {
			const int Avail = GProcWaitForAvailable(200.0f);
			if (Avail < 0) {
				return false;	
			}
			GProcThreads[Avail]->InitReformTMesh(&Groups[i], &Data::NMeshes[i]);
			IsGProcTAvail[Avail].AtomicSet(false);
			if (!TryStartThread(FGeoProcThread::GEOPROC_REFORM, Avail)) {
				IsGProcTAvail[Avail].AtomicSet(true);
			}
		}
		if (GProcWaitForAll(200.0f) == WAIT_SUCCESS) {
			return true;
		}
		// FThreadSafeBool B(true);
		// for (int i = 0; i <Groups.Num(); i++) {
		// 	GProc.ReformTriMesh(&Groups[i], &DataProcMutex, &B, &Data::NMeshes[i]);
		// }
		return true;
	}

	// Advance the progress bar
	void EnterProgressFrame(FScopedSlowTask& Task, const char* msg) {
		// Task.EnterProgressFrame *requests* a UI update, which only works if there's sufficient time between calls
		// so, we can get stuck on a long stage without new information unless we wait a little
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		Task.EnterProgressFrame( 
			1.0f,
			FText::Format(LOCTEXT("UNav3D", "UNav3D working on... {0}"), FText::FromString(msg))
		);
	}

#ifdef UNAV_DEV
	// Sets the bounds volumes of all vertex captures in the world and adds them to the TArray in Data
	void InitVertexCaptures(const UWorld* World) {
		TArray<AActor*> VertexCaptures;
		UGameplayStatics::GetAllActorsOfClass(World, AVertexCapture::StaticClass(), VertexCaptures);
		for (const auto VCA : VertexCaptures) {
			AVertexCapture* VC = Cast<AVertexCapture>(VCA);
			if (VC != nullptr) {
				Geometry::SetBoundingBox(VC->GetBBox(), VC);
				UNavDbg::DrawBoundingBox(World, VC->GetBBox());
				Data::VertexCaptures.Add(VC);
			}
		}
	}
#endif
	
}

// ---------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------

bool DataProcessing::Init() {
	Cleanup();
	for (int i = 0; i < MaxRunningThreadCt; i++) {
		GProcThreads[i] = new FGeoProcThread();
		if (GProcThreads[i] == nullptr) {
			Cleanup();
			return false;			
		}
	}
	return true;
}

void DataProcessing::Cleanup() {
	for (int i = 0; i < MAX_THREAD_CT; i++) {
		if (GProcThreads[i] != nullptr) {
			GProcThreads[i]->StopThread();
			delete GProcThreads[i];
			GProcThreads[i] = nullptr;
		}
	}	
}

void DataProcessing::TotalReload() {
	Data::Reset();	
	GProcThreadsStop();
	
	if (GEditor == nullptr || GEditor->GetEditorWorldContext().World() == nullptr) {
		UNAV_GENERR("GEditor or World Unavailable")
		return;
	}
	const UWorld* World = GEditor->GetEditorWorldContext().World();

	// If we find a single UNav3DBoundsVolume in the editor world, we're good
	if (!SetBoundsVolume(World)) {
		return;
	}

#ifdef UNAV_DEV
	// vertex captures can be placed in the world so triangles with matching vertices can be stopped on in debugging
	InitVertexCaptures(World);
#endif
	
	// starting up progress bar
	FScopedSlowTask Task(
		TOTAL_RESET_TASK_CT, LOCTEXT("Unav3D", "UNav3D working on...")
	);
	Task.MakeDialog(false);
	
	EnterProgressFrame(Task, "getting and simplifying static mesh data");
	if (!PopulateTriMeshes(Data::TMeshes)) {
		return;
	}

	return;
	EnterProgressFrame(Task, "grouping meshes by intersection");
	TArray<TArray<TriMesh*>> TMeshGroups;
	GProc.GroupTriMeshes(Data::TMeshes, TMeshGroups);
	
	EnterProgressFrame(Task, "reforming meshes");
	if (!ReformTriMeshes(TMeshGroups)) {
		return;
	}

#ifdef UNAV_DBG
	UNavDbg::DrawSavedLines(World);
#endif
}

#undef LOCTEXT_NAMESPACE

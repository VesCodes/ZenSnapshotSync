#pragma once

#include <Containers/Array.h>
#include <Containers/Map.h>
#include <Containers/Ticker.h>

#include "ZenSnapshotSyncTypes.h"

class FAsyncTaskNotification;
class FZenSnapshotSyncModule;
class UToolMenu;

struct FZenSnapshotSyncTask
{
	FZenSnapshotSyncHandle Handle;
	TUniquePtr<FAsyncTaskNotification> Notification;
};

class FZenSnapshotSyncToolbar
{
public:
	using ThisClass = FZenSnapshotSyncToolbar;

	ZENSNAPSHOTSYNC_API FZenSnapshotSyncToolbar();
	ZENSNAPSHOTSYNC_API ~FZenSnapshotSyncToolbar();

private:
	void RegisterMenus();
	void MakeSnapshotSyncMenu(UToolMenu* Menu);

	bool CanSyncSnapshot(const FZenSnapshotDescriptor* SnapshotDescriptor) const;
	void SyncSnapshot(const FZenSnapshotDescriptor* SnapshotDescriptor);

	bool TickSnapshotSyncTasks(float DeltaTime);
	void CancelSnapshotSyncTasks();

	FZenSnapshotSyncModule* SnapshotSyncModule = nullptr;
	TArray<FZenSnapshotDescriptor> LatestSnapshotDescriptors;
	TMap<FString, FZenSnapshotSyncTask> SnapshotSyncTasks;
	FTSTicker::FDelegateHandle SnapshotSyncTickHandle;
};

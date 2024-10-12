#pragma once

#include <ZenServerHttp.h>
#include <Experimental/ZenServerInterface.h>
#include <Modules/ModuleManager.h>

#include "ZenSnapshotSyncTypes.h"

class FZenSnapshotSyncToolbar;

class FZenSnapshotSyncModule : public IModuleInterface
{
public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FQuerySnapshotsMulticastDelegate, TArray<FZenSnapshotDescriptor>& SnapshotDescriptors);
	using FQuerySnapshotsDelegate = FQuerySnapshotsMulticastDelegate::FDelegate;

	virtual void StartupModule() override;

	ZENSNAPSHOTSYNC_API static bool ReadSnapshotDescriptorJson(FStringView SnapshotDescriptorJson, TArray<FZenSnapshotDescriptor>& SnapshotDescriptors);
	ZENSNAPSHOTSYNC_API static bool ReadSnapshotDescriptorFile(const TCHAR* SnapshotDescriptorFilePath, TArray<FZenSnapshotDescriptor>& SnapshotDescriptors);

	ZENSNAPSHOTSYNC_API FZenSnapshotSyncHandle RequestSnapshotSync(const FZenSnapshotDescriptor& SnapshotDescriptor) const;
	ZENSNAPSHOTSYNC_API FZenSnapshotSyncHandle RequestSnapshotSyncFromFile(FStringView TargetPlatform, FStringView Directory, FStringView FileName) const;
	ZENSNAPSHOTSYNC_API FZenSnapshotSyncHandle RequestSnapshotSyncFromCloud(FStringView TargetPlatform, FStringView Host, FStringView Namespace, FStringView Bucket, FStringView Key) const;
	ZENSNAPSHOTSYNC_API FZenSnapshotSyncHandle RequestSnapshotSyncFromZen(FStringView TargetPlatform, FStringView Host, FStringView Project, FStringView Oplog) const;
	ZENSNAPSHOTSYNC_API bool QuerySnapshotSyncStatus(FZenSnapshotSyncHandle& Handle) const;
	ZENSNAPSHOTSYNC_API bool CancelSnapshotSync(FZenSnapshotSyncHandle& Handle) const;

	ZENSNAPSHOTSYNC_API FDelegateHandle RegisterQuerySnapshotsCallback(FQuerySnapshotsDelegate&& Callback);
	ZENSNAPSHOTSYNC_API void UnregisterQuerySnapshotsCallback(FDelegateHandle CallbackHandle);

	ZENSNAPSHOTSYNC_API bool CanQuerySnapshots() const;
	ZENSNAPSHOTSYNC_API void QuerySnapshots(TArray<FZenSnapshotDescriptor>& SnapshotDescriptors) const;

private:
	static FUtf8StringView GetResponseBufferAsString(const TArray64<uint8>& ResponseBuffer);

	FZenSnapshotSyncHandle RequestSnapshotSync(FStringView TargetPlatform, FCbObjectView Params) const;

	UE::Zen::FScopeZenService ZenService;
	TUniquePtr<UE::Zen::FZenHttpRequestPool> RequestPool;
	TSharedPtr<FZenSnapshotSyncToolbar> Toolbar = nullptr;
	FQuerySnapshotsMulticastDelegate OnQuerySnapshots;
};

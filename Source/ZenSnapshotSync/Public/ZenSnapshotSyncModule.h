#pragma once

#include <ZenServerHttp.h>
#include <Experimental/ZenServerInterface.h>
#include <Modules/ModuleManager.h>

struct FZenSnapshotSyncHandle
{
	ZENSNAPSHOTSYNC_API bool IsValid() const;

	ZENSNAPSHOTSYNC_API bool IsCompleted() const;
	ZENSNAPSHOTSYNC_API const FString& GetError() const;

	ZENSNAPSHOTSYNC_API const FString& GetState() const;
	ZENSNAPSHOTSYNC_API float GetStateProgress() const;
	ZENSNAPSHOTSYNC_API const TArray<FString>& GetMessages() const;

private:
	friend class FZenSnapshotSyncModule;

	FString JobId;

	bool bCompleted = false;
	FString Error;

	FString State;
	float StateProgress = 0.0f;
	TArray<FString> Messages;
};

class FZenSnapshotSyncModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;

	ZENSNAPSHOTSYNC_API static bool ReadSnapshotDescriptor(const TCHAR* SnapshotDescriptorFilePath, TArray<TSharedPtr<FJsonObject>>& OutSnapshots);

	ZENSNAPSHOTSYNC_API FZenSnapshotSyncHandle RequestSnapshotSync(const TSharedPtr<FJsonObject>& Snapshot) const;
	ZENSNAPSHOTSYNC_API FZenSnapshotSyncHandle RequestSnapshotSyncFromFile(FStringView TargetPlatform, FStringView Directory, FStringView FileName) const;
	ZENSNAPSHOTSYNC_API FZenSnapshotSyncHandle RequestSnapshotSyncFromCloud(FStringView TargetPlatform, FStringView Host, FStringView Namespace, FStringView Bucket, FStringView Key) const;
	ZENSNAPSHOTSYNC_API FZenSnapshotSyncHandle RequestSnapshotSyncFromZen(FStringView TargetPlatform, FStringView Host, FStringView Project, FStringView Oplog) const;
	ZENSNAPSHOTSYNC_API bool QuerySnapshotSyncStatus(FZenSnapshotSyncHandle& Handle) const;
	ZENSNAPSHOTSYNC_API bool CancelSnapshotSync(const FZenSnapshotSyncHandle& Handle) const;

private:
	static FUtf8StringView GetResponseBufferAsString(const TArray64<uint8>& ResponseBuffer);

	FZenSnapshotSyncHandle RequestSnapshotSync(FStringView TargetPlatform, FCbObjectView Params) const;

	UE::Zen::FScopeZenService ZenService;
	TUniquePtr<UE::Zen::FZenHttpRequestPool> RequestPool;
};

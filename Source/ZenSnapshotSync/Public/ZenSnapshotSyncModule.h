#pragma once

#include <ZenServerHttp.h>
#include <Experimental/ZenServerInterface.h>
#include <Modules/ModuleManager.h>

struct FZenSnapshotDescriptor
{
	ZENSNAPSHOTSYNC_API const FString& GetName() const;
	ZENSNAPSHOTSYNC_API const FString& GetTargetPlatform() const;

private:
	friend class FZenSnapshotSyncModule;

	FString Name;
	FString TargetPlatform;
	TSharedPtr<FJsonObject> Object = nullptr;
};

struct FZenSnapshotSyncHandle
{
	ZENSNAPSHOTSYNC_API bool IsValid() const;
	ZENSNAPSHOTSYNC_API bool IsComplete() const;
	ZENSNAPSHOTSYNC_API bool IsError() const;
	ZENSNAPSHOTSYNC_API const FString& GetErrorMessage() const;
	ZENSNAPSHOTSYNC_API const FString& GetState() const;
	ZENSNAPSHOTSYNC_API float GetStateProgress() const;

private:
	friend class FZenSnapshotSyncModule;

	FString JobId;
	bool bComplete = false;
	FString ErrorMessage;
	FString State;
	float StateProgress = 0.0f;
};

class FZenSnapshotSyncModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;

	ZENSNAPSHOTSYNC_API static bool ReadSnapshotDescriptorJson(FStringView SnapshotDescriptorJson, TArray<FZenSnapshotDescriptor>& SnapshotDescriptors);
	ZENSNAPSHOTSYNC_API static bool ReadSnapshotDescriptorFile(const TCHAR* SnapshotDescriptorFilePath, TArray<FZenSnapshotDescriptor>& SnapshotDescriptors);

	ZENSNAPSHOTSYNC_API FZenSnapshotSyncHandle RequestSnapshotSync(const FZenSnapshotDescriptor& SnapshotDescriptor) const;
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

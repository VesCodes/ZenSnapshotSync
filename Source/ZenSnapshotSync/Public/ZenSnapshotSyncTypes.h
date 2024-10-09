#pragma once

#include <Containers/UnrealString.h>
#include <Templates/SharedPointer.h>

class FJsonObject;

struct FZenSnapshotDescriptor
{
	ZENSNAPSHOTSYNC_API const FString& GetName() const;
	ZENSNAPSHOTSYNC_API const FString& GetTargetPlatform() const;

private:
	friend class FZenSnapshotSyncModule;

	FString Name;
	FString TargetPlatform;
	TSharedPtr<class FJsonObject> Object = nullptr;
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

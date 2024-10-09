#include "ZenSnapshotSyncTypes.h"

const FString& FZenSnapshotDescriptor::GetName() const
{
	return Name;
}

const FString& FZenSnapshotDescriptor::GetTargetPlatform() const
{
	return TargetPlatform;
}

bool FZenSnapshotSyncHandle::IsValid() const
{
	return !JobId.IsEmpty();
}

bool FZenSnapshotSyncHandle::IsComplete() const
{
	return bComplete;
}

bool FZenSnapshotSyncHandle::IsError() const
{
	return !ErrorMessage.IsEmpty();
}

const FString& FZenSnapshotSyncHandle::GetErrorMessage() const
{
	return ErrorMessage;
}

const FString& FZenSnapshotSyncHandle::GetState() const
{
	return State;
}

float FZenSnapshotSyncHandle::GetStateProgress() const
{
	return StateProgress;
}

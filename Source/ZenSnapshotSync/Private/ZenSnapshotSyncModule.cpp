#include "ZenSnapshotSyncModule.h"

#include <Logging/StructuredLog.h>
#include <Misc/App.h>
#include <Misc/FileHelper.h>
#include <Misc/Paths.h>
#include <Serialization/CompactBinaryWriter.h>
#include <Serialization/JsonReader.h>
#include <Serialization/JsonSerializer.h>

DEFINE_LOG_CATEGORY_STATIC(LogZenSnapshotSync, Log, All);

IMPLEMENT_MODULE(FZenSnapshotSyncModule, ZenSnapshotSync);

bool FZenSnapshotSyncHandle::IsValid() const
{
	return !JobId.IsEmpty();
}

bool FZenSnapshotSyncHandle::IsCompleted() const
{
	return bCompleted;
}

const FString& FZenSnapshotSyncHandle::GetError() const
{
	return Error;
}

const FString& FZenSnapshotSyncHandle::GetState() const
{
	return State;
}

float FZenSnapshotSyncHandle::GetStateProgress() const
{
	return StateProgress;
}

const TArray<FString>& FZenSnapshotSyncHandle::GetMessages() const
{
	return Messages;
}

void FZenSnapshotSyncModule::StartupModule()
{
	RequestPool = MakeUnique<UE::Zen::FZenHttpRequestPool>(ZenService.GetInstance().GetURL());
}

bool FZenSnapshotSyncModule::ReadSnapshotDescriptor(const TCHAR* SnapshotDescriptorFilePath, TArray<TSharedPtr<FJsonObject>>& OutSnapshots)
{
	FString SnapshotDescriptorJson;
	if (!FFileHelper::LoadFileToString(SnapshotDescriptorJson, SnapshotDescriptorFilePath))
	{
		UE_LOGFMT(LogZenSnapshotSync, Error, "Failed to read snapshot descriptor file '{File}'", SnapshotDescriptorFilePath);
		return false;
	}

	TSharedPtr<FJsonObject> SnapshotDescriptor;

	const TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<>::Create(SnapshotDescriptorJson);
	if (!FJsonSerializer::Deserialize(JsonReader, SnapshotDescriptor) || !SnapshotDescriptor.IsValid())
	{
		UE_LOGFMT(LogZenSnapshotSync, Error, "Failed to deserialize snapshot descriptor file '{File}'", SnapshotDescriptorFilePath);
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>> Snapshots = SnapshotDescriptor->GetArrayField(TEXT("snapshots"));

	OutSnapshots.Reserve(Snapshots.Num());
	for (const TSharedPtr<FJsonValue>& SnapshotValue : Snapshots)
	{
		const TSharedPtr<FJsonObject>* Snapshot;
		if (SnapshotValue->TryGetObject(Snapshot))
		{
			OutSnapshots.Add(*Snapshot);
		}
	}

	return true;
}

FZenSnapshotSyncHandle FZenSnapshotSyncModule::RequestSnapshotSync(const TSharedPtr<FJsonObject>& Snapshot) const
{
	if (Snapshot.IsValid())
	{
		const FString SnapshotType = Snapshot->GetStringField(TEXT("type"));
		const FString TargetPlatform = Snapshot->GetStringField(TEXT("targetplatform"));

		if (SnapshotType == TEXT("file"))
		{
			const FString Directory = Snapshot->GetStringField(TEXT("directory"));
			const FString FileName = Snapshot->GetStringField(TEXT("filename"));

			return RequestSnapshotSyncFromFile(TargetPlatform, Directory, FileName);
		}

		if (SnapshotType == TEXT("cloud"))
		{
			const FString Host = Snapshot->GetStringField(TEXT("host"));
			const FString Namespace = Snapshot->GetStringField(TEXT("namespace"));
			const FString Bucket = Snapshot->GetStringField(TEXT("bucket"));
			const FString Key = Snapshot->GetStringField(TEXT("key"));

			return RequestSnapshotSyncFromCloud(TargetPlatform, Host, Namespace, Bucket, Key);
		}

		if (SnapshotType == TEXT("zen"))
		{
			const FString Host = Snapshot->GetStringField(TEXT("host"));
			const FString Project = Snapshot->GetStringField(TEXT("projectid"));
			const FString Oplog = Snapshot->GetStringField(TEXT("oplogid"));

			return RequestSnapshotSyncFromZen(TargetPlatform, Host, Project, Oplog);
		}
	}

	return FZenSnapshotSyncHandle();
}

FZenSnapshotSyncHandle FZenSnapshotSyncModule::RequestSnapshotSyncFromFile(FStringView TargetPlatform, FStringView Directory, FStringView FileName) const
{
	FCbWriter ParamsWriter;
	ParamsWriter.BeginObject();
	ParamsWriter.BeginObject("file");
	ParamsWriter.AddString("path", Directory);
	ParamsWriter.AddString("name", FileName);
	ParamsWriter.EndObject();
	ParamsWriter.EndObject();

	FCbFieldIterator Params = ParamsWriter.Save();
	return RequestSnapshotSync(TargetPlatform, Params.AsObjectView());
}

FZenSnapshotSyncHandle FZenSnapshotSyncModule::RequestSnapshotSyncFromCloud(FStringView TargetPlatform, FStringView Host, FStringView Namespace, FStringView Bucket, FStringView Key) const
{
	FCbWriter ParamsWriter;
	ParamsWriter.BeginObject();
	ParamsWriter.BeginObject("cloud");
	ParamsWriter.AddString("url", Host);
	ParamsWriter.AddString("namespace", Namespace);
	ParamsWriter.AddString("bucket", Bucket);
	ParamsWriter.AddString("key", Key);
	ParamsWriter.EndObject();
	ParamsWriter.EndObject();

	FCbFieldIterator Params = ParamsWriter.Save();
	return RequestSnapshotSync(TargetPlatform, Params.AsObjectView());
}

FZenSnapshotSyncHandle FZenSnapshotSyncModule::RequestSnapshotSyncFromZen(FStringView TargetPlatform, FStringView Host, FStringView Project, FStringView Oplog) const
{
	FCbWriter ParamsWriter;
	ParamsWriter.BeginObject();
	ParamsWriter.BeginObject("zen");
	ParamsWriter.AddString("url", Host);
	ParamsWriter.AddString("project", Project);
	ParamsWriter.AddString("oplog", Oplog);
	ParamsWriter.EndObject();
	ParamsWriter.EndObject();

	FCbFieldIterator Params = ParamsWriter.Save();
	return RequestSnapshotSync(TargetPlatform, Params.AsObjectView());
}

FZenSnapshotSyncHandle FZenSnapshotSyncModule::RequestSnapshotSync(FStringView TargetPlatform, FCbObjectView Params) const
{
	using namespace UE::Zen;

	const FString ProjectId = FApp::GetZenStoreProjectId();
	const FStringView OplogId = TargetPlatform;

	TStringBuilder<128> RequestUri;
	FZenScopedRequestPtr Request(RequestPool.Get());
	IFileManager& FileManager = IFileManager::Get();

	// Ensure project exists
	RequestUri << TEXTVIEW("/prj/") << ProjectId;

	FZenHttpRequest::Result Result = Request->PerformBlockingDownload(RequestUri, nullptr, EContentType::CbObject);
	if (Result != FZenHttpRequest::Result::Success || Request->GetResponseCode() != 200)
	{
		FCbWriter PayloadWriter;
		PayloadWriter.BeginObject();
		PayloadWriter.AddString("id", ProjectId);
		PayloadWriter.AddString("root", FileManager.ConvertToAbsolutePathForExternalAppForRead(*FPaths::RootDir()));
		PayloadWriter.AddString("engine", FileManager.ConvertToAbsolutePathForExternalAppForRead(*FPaths::EngineDir()));
		PayloadWriter.AddString("project", FileManager.ConvertToAbsolutePathForExternalAppForRead(*FPaths::ProjectDir()));
		PayloadWriter.AddString("projectfile", FileManager.ConvertToAbsolutePathForExternalAppForRead(*FPaths::GetProjectFilePath()));
		PayloadWriter.EndObject();

		FCbFieldIterator Payload = PayloadWriter.Save();
		Request->Reset();

		Result = Request->PerformBlockingPost(RequestUri, Payload.AsObjectView());
		if (Result != FZenHttpRequest::Result::Success || Request->GetResponseCode() != 201)
		{
			UE_LOGFMT(LogZenSnapshotSync, Error, "Failed to create project '{ProjectId}' ({ResponseCode})", ProjectId, Request->GetResponseCode());
			return FZenSnapshotSyncHandle();
		}
	}

	// Create project store file
	const FString ProjectStoreFilePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Saved"), TEXT("Cooked"), TargetPlatform, TEXT("ue.projectstore"));
	{
		TUniquePtr<FArchive> ProjectStoreFile(FileManager.CreateFileWriter(*ProjectStoreFilePath));
		if (!ProjectStoreFile.IsValid())
		{
			UE_LOGFMT(LogZenSnapshotSync, Error, "Failed to create project store file '{File}' ({ErrorCode})", ProjectStoreFilePath, FPlatformMisc::GetLastError());
			return FZenSnapshotSyncHandle();
		}

		const TSharedRef<TJsonWriter<UTF8CHAR>> Writer = TJsonWriterFactory<UTF8CHAR>::Create(ProjectStoreFile.Get());
		Writer->WriteObjectStart();
		Writer->WriteObjectStart(TEXT("zenserver"));
		Writer->WriteValue(TEXT("projectid"), ProjectId);
		Writer->WriteValue(TEXT("oplogid"), OplogId);
		Writer->WriteObjectEnd();
		Writer->WriteObjectEnd();
		Writer->Close();
	}

	// Ensure oplog exists
	RequestUri << TEXTVIEW("/oplog/") << OplogId;
	Request->Reset();

	Result = Request->PerformBlockingDownload(RequestUri, nullptr, EContentType::CbObject);
	if (Result != FZenHttpRequest::Result::Success || Request->GetResponseCode() != 200)
	{
		FCbWriter PayloadWriter;
		PayloadWriter.BeginObject();
		PayloadWriter.AddString("gcpath", FileManager.ConvertToAbsolutePathForExternalAppForRead(*ProjectStoreFilePath));
		PayloadWriter.EndObject();

		FCbFieldIterator Payload = PayloadWriter.Save();
		Request->Reset();

		Result = Request->PerformBlockingPost(RequestUri, Payload.AsObjectView());
		if (Result != FZenHttpRequest::Result::Success || Request->GetResponseCode() != 201)
		{
			UE_LOGFMT(LogZenSnapshotSync, Error, "Failed to create oplog '{OplogId}' ({ResponseCode})", OplogId, Request->GetResponseCode());
			return FZenSnapshotSyncHandle();
		}
	}

	// Request oplog import
	RequestUri << TEXTVIEW("/rpc");
	Request->Reset();

	FCbWriter PayloadWriter;
	PayloadWriter.BeginObject();
	PayloadWriter.AddString("method", "import");
	PayloadWriter.AddObject("params", Params);
	PayloadWriter.EndObject();

	FCbFieldIterator Payload = PayloadWriter.Save();

	Result = Request->PerformBlockingPost(RequestUri, Payload.AsObjectView());
	if (Result != FZenHttpRequest::Result::Success || Request->GetResponseCode() != 202)
	{
		UE_LOGFMT(LogZenSnapshotSync, Error, "Failed to import oplog '{Oplog}' ({ResponseCode})", TargetPlatform, Request->GetResponseCode());
		return FZenSnapshotSyncHandle();
	}

	FZenSnapshotSyncHandle Handle;
	Handle.JobId = FString(GetResponseBufferAsString(Request->GetResponseBuffer()));

	return MoveTemp(Handle);
}

bool FZenSnapshotSyncModule::QuerySnapshotSyncStatus(FZenSnapshotSyncHandle& Handle) const
{
	using namespace UE::Zen;

	if (Handle.JobId.IsEmpty() || Handle.bCompleted)
	{
		return false;
	}

	TStringBuilder<128> RequestUri;
	FZenScopedRequestPtr Request(RequestPool.Get());

	RequestUri << TEXTVIEW("/admin/jobs/") << Handle.JobId;

	const FZenHttpRequest::Result Result = Request->PerformBlockingDownload(RequestUri, nullptr, EContentType::CbObject);
	if (Result != FZenHttpRequest::Result::Success || Request->GetResponseCode() != 200)
	{
		Handle.Error = FString(GetResponseBufferAsString(Request->GetResponseBuffer()));

		UE_LOGFMT(LogZenSnapshotSync, Error, "Failed to query job '{JobId}' ({ResponseCode})", Handle.JobId, Request->GetResponseCode());
		return false;
	}

	const FCbObjectView Response = Request->GetResponseAsObject();
	const FUtf8StringView Status = Response["Status"].AsString();

	if (Status == "Complete")
	{
		Handle.bCompleted = true;
	}
	else if (Status == "Aborted")
	{
		Handle.bCompleted = true;

		Handle.Error = FString(Response["AbortReason"].AsString());
		if (Handle.Error.IsEmpty())
		{
			Handle.Error = TEXT("Aborted");
		}
	}

	Handle.State = FString(Response["CurrentOp"].AsString());
	Handle.StateProgress = Response["CurrentOpPercentComplete"].AsUInt32() / 100.0f;

	for (FCbFieldView Message : Response["Messages"].AsArrayView())
	{
		Handle.Messages.Emplace(Message.AsString());
	}

	return !Handle.bCompleted;
}

bool FZenSnapshotSyncModule::CancelSnapshotSync(const FZenSnapshotSyncHandle& Handle) const
{
	using namespace UE::Zen;

	if (Handle.JobId.IsEmpty() || Handle.bCompleted)
	{
		return false;
	}

	TStringBuilder<128> RequestUri;
	FZenScopedRequestPtr Request(RequestPool.Get());

	RequestUri << TEXTVIEW("/admin/jobs/") << Handle.JobId;

	const FZenHttpRequest::Result Result = Request->PerformBlockingDelete(RequestUri);
	if (Result != FZenHttpRequest::Result::Success || Request->GetResponseCode() != 200)
	{
		UE_LOGFMT(LogZenSnapshotSync, Error, "Failed to cancel job '{JobId}' ({ResponseCode})", Handle.JobId, Request->GetResponseCode());
		return false;
	}

	return true;
}

FUtf8StringView FZenSnapshotSyncModule::GetResponseBufferAsString(const TArray64<uint8>& ResponseBuffer)
{
	return FUtf8StringView(reinterpret_cast<const UTF8CHAR*>(ResponseBuffer.GetData()), ResponseBuffer.Num());
}

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

void FZenSnapshotSyncModule::StartupModule()
{
	RequestPool = MakeUnique<UE::Zen::FZenHttpRequestPool>(ZenService.GetInstance().GetURL());
}

bool FZenSnapshotSyncModule::ReadSnapshotDescriptorJson(FStringView SnapshotDescriptorJson, TArray<FZenSnapshotDescriptor>& SnapshotDescriptors)
{
	TSharedPtr<FJsonObject> SnapshotDescriptorRootObject;

	const TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<>::CreateFromView(SnapshotDescriptorJson);
	if (!FJsonSerializer::Deserialize(JsonReader, SnapshotDescriptorRootObject) || !SnapshotDescriptorRootObject.IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>> SnapshotDescriptorValues = SnapshotDescriptorRootObject->GetArrayField(TEXT("snapshots"));
	SnapshotDescriptors.Reserve(SnapshotDescriptors.Num() + SnapshotDescriptorValues.Num());

	for (const TSharedPtr<FJsonValue>& SnapshotDescriptorValue : SnapshotDescriptorValues)
	{
		const TSharedPtr<FJsonObject>& SnapshotDescriptorObject = SnapshotDescriptorValue->AsObject();
		if (SnapshotDescriptorObject.IsValid())
		{
			FZenSnapshotDescriptor& SnapshotDescriptor = SnapshotDescriptors.Emplace_GetRef();
			SnapshotDescriptor.Name = SnapshotDescriptorObject->GetStringField(TEXT("name"));
			SnapshotDescriptor.TargetPlatform = SnapshotDescriptorObject->GetStringField(TEXT("targetplatform"));
			SnapshotDescriptor.Object = SnapshotDescriptorObject;
		}
	}

	return true;
}

bool FZenSnapshotSyncModule::ReadSnapshotDescriptorFile(const TCHAR* SnapshotDescriptorFilePath, TArray<FZenSnapshotDescriptor>& SnapshotDescriptors)
{
	FString SnapshotDescriptorJson;
	if (!FFileHelper::LoadFileToString(SnapshotDescriptorJson, SnapshotDescriptorFilePath))
	{
		UE_LOGFMT(LogZenSnapshotSync, Error, "Failed to read snapshot descriptor file '{File}'", SnapshotDescriptorFilePath);
		return false;
	}

	return ReadSnapshotDescriptorJson(SnapshotDescriptorJson, SnapshotDescriptors);
}

FZenSnapshotSyncHandle FZenSnapshotSyncModule::RequestSnapshotSync(const FZenSnapshotDescriptor& SnapshotDescriptor) const
{
	if (SnapshotDescriptor.Object.IsValid())
	{
		const FString SnapshotType = SnapshotDescriptor.Object->GetStringField(TEXT("type"));

		if (SnapshotType == TEXT("file"))
		{
			const FString Directory = SnapshotDescriptor.Object->GetStringField(TEXT("directory"));
			const FString FileName = SnapshotDescriptor.Object->GetStringField(TEXT("filename"));

			return RequestSnapshotSyncFromFile(SnapshotDescriptor.GetTargetPlatform(), Directory, FileName);
		}

		if (SnapshotType == TEXT("cloud"))
		{
			const FString Host = SnapshotDescriptor.Object->GetStringField(TEXT("host"));
			const FString Namespace = SnapshotDescriptor.Object->GetStringField(TEXT("namespace"));
			const FString Bucket = SnapshotDescriptor.Object->GetStringField(TEXT("bucket"));
			const FString Key = SnapshotDescriptor.Object->GetStringField(TEXT("key"));

			return RequestSnapshotSyncFromCloud(SnapshotDescriptor.GetTargetPlatform(), Host, Namespace, Bucket, Key);
		}

		if (SnapshotType == TEXT("zen"))
		{
			const FString Host = SnapshotDescriptor.Object->GetStringField(TEXT("host"));
			const FString Project = SnapshotDescriptor.Object->GetStringField(TEXT("projectid"));
			const FString Oplog = SnapshotDescriptor.Object->GetStringField(TEXT("oplogid"));

			return RequestSnapshotSyncFromZen(SnapshotDescriptor.GetTargetPlatform(), Host, Project, Oplog);
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

	if (ProjectId.IsEmpty() || OplogId.IsEmpty())
	{
		return FZenSnapshotSyncHandle();
	}

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
		UE_LOGFMT(LogZenSnapshotSync, Error, "Failed to import oplog '{OplogId}' ({ResponseCode})", OplogId, Request->GetResponseCode());
		return FZenSnapshotSyncHandle();
	}

	FZenSnapshotSyncHandle Handle;
	Handle.JobId = FString(GetResponseBufferAsString(Request->GetResponseBuffer()));

	return MoveTemp(Handle);
}

bool FZenSnapshotSyncModule::QuerySnapshotSyncStatus(FZenSnapshotSyncHandle& Handle) const
{
	using namespace UE::Zen;

	if (!Handle.IsValid() || Handle.IsComplete() || Handle.IsError())
	{
		return false;
	}

	TStringBuilder<128> RequestUri;
	FZenScopedRequestPtr Request(RequestPool.Get());

	RequestUri << TEXTVIEW("/admin/jobs/") << Handle.JobId;

	const FZenHttpRequest::Result Result = Request->PerformBlockingDownload(RequestUri, nullptr, EContentType::CbObject);
	if (Result != FZenHttpRequest::Result::Success || Request->GetResponseCode() != 200)
	{
		Handle.ErrorMessage = FString(GetResponseBufferAsString(Request->GetResponseBuffer()));
		return false;
	}

	const FCbObjectView Response = Request->GetResponseAsObject();
	const FUtf8StringView Status = Response["Status"].AsString();

	Handle.State = FString(Response["CurrentOp"].AsString());
	Handle.StateProgress = Response["CurrentOpPercentComplete"].AsUInt32() / 100.0f;

	if (Status == "Complete")
	{
		Handle.bComplete = true;
		return false;
	}

	if (Status == "Aborted")
	{
		const FUtf8StringView AbortReason = Response["AbortReason"].AsString();
		Handle.ErrorMessage = AbortReason.IsEmpty() ? TEXT("Aborted") : FString(AbortReason);
		return false;
	}

	return true;
}

bool FZenSnapshotSyncModule::CancelSnapshotSync(const FZenSnapshotSyncHandle& Handle) const
{
	using namespace UE::Zen;

	if (!Handle.IsValid() || Handle.IsComplete() || Handle.IsError())
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

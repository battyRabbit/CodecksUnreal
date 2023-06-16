// Copyright(C) battyRabbit UG (limited liability). All Rights Reserved.

#include "Requests/CodecksUserReportRequest.h"

#include "CodecksUnreal.h"
#include "Settings/CodecksSettings.h"

#include <HttpModule.h>
#include <ImageUtils.h>
#include <ImageWrapperHelper.h>

#include <Interfaces/IHttpResponse.h>

#include <Tasks/Task.h>
#include <Tasks/Pipe.h>

UCodecksUserReportRequest::UCodecksUserReportRequest()
{}

void UCodecksUserReportRequest::BeginDestroy()
{
	UObject::BeginDestroy();
	FScreenshotRequest::OnScreenshotCaptured().RemoveAll(this);
	GEngine->GameViewport->OnScreenshotCaptured().RemoveAll(this);
}

void UCodecksUserReportRequest::BuildRequest()
{
	HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetVerb("POST");
	HttpRequest->SetHeader("Content-Type", "application/json");

	FStringFormatNamedArguments Formatter;
	const UCodecksSettings* CodecksSettings = GetDefault<UCodecksSettings>();

	//~ BATTY_TODO(Hati) Can we validate more than empty?
	if (CodecksSettings->GetReportToken().IsEmpty())
	{
		Fail(CodecksRequestErrors::NoRequestToken);
		return;
	}

	const FString Endpoint = CodecksSettings->GetApiUrl() / "user-report/v1/create-report?token={REPORT_TOKEN}";

	Formatter.Add("REPORT_TOKEN", GetReportToken());
	HttpRequest->SetURL(FString::Format(*Endpoint, Formatter));

	const TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();
	JsonObject << *this;

	FString ContentString;
	const TSharedRef<TJsonWriter<TCHAR>> JsonWriter = TJsonWriterFactory<>::Create(&ContentString);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), JsonWriter);

	if (ContentString.IsEmpty())
	{
		Fail(CodecksRequestErrors::NoContent);
		return;
	}

	HttpRequest->SetContentAsString(ContentString);
}

FString UCodecksUserReportRequest::GetReportToken() const
{
	const UCodecksSettings* CodecksSettings = GetDefault<UCodecksSettings>();
	return CodecksSettings->GetReportToken();
}

void UCodecksUserReportRequest::AttachIntermediateScreenshot(bool bShowUI)
{
	const FString DateName = FDateTime::Now().ToString(TEXT("%Y-%m-%d %H-%M-%S"));
	const FString Filename = DateName + ".png";

	int32 NewScreenshot = AttachedFiles.Add(FAttachedFile{Filename});

	ScreenshotPipe.Launch(TEXT("Codecks_FetchScreenshot"), [this, NewScreenshot, bShowUI]() {
		UE::Tasks::FTaskEvent WaitForScreenshot(UE_SOURCE_LOCATION);

		const FDelegateHandle Delegate = GEngine->GameViewport->OnScreenshotCaptured().AddWeakLambda(this, [this, NewScreenshot, &WaitForScreenshot](int32 Width, int32 Height, const TArray<FColor>& Colors) {
			Async(EAsyncExecution::TaskGraph, [this, Width, Height, NewScreenshot, Colors, &WaitForScreenshot]() {
				TArray64<uint8> CompressedBitmap;
				FImageUtils::PNGCompressImageArray(Width, Height, TArrayView64<const FColor>(Colors.GetData(), Colors.Num()), CompressedBitmap);

				if (AttachedFiles.IsValidIndex(NewScreenshot))
				{
					AttachedFiles[NewScreenshot].Binary = MoveTemp(CompressedBitmap);
					AttachedFiles[NewScreenshot].ContentType = "image/png";
				}

				AsyncTask(ENamedThreads::GameThread, [&WaitForScreenshot]() { WaitForScreenshot.Trigger(); });
			});
		});

		FScreenshotRequest::RequestScreenshot(/*bShowUI=*/bShowUI);

		WaitForScreenshot.BusyWait();

		GEngine->GameViewport->OnScreenshotCaptured().Remove(Delegate);
	});
}

void UCodecksUserReportRequest::AddRequestData(const TSharedPtr<FJsonObject>& JsonObject)
{
	check(JsonObject);

	if (Content.IsEmpty())
	{
		Fail(CodecksRequestErrors::NoContent);
		return;
	}

	JsonObject->SetStringField("content", Content);

	if (Severity != ECodecksUserReportSeverity::None)
	{
		FString EnumString;
		switch (Severity)
		{
		case ECodecksUserReportSeverity::Critical:
			EnumString = "critical";
			break;
		case ECodecksUserReportSeverity::High:
			EnumString = "high";
			break;
		case ECodecksUserReportSeverity::Low:
			EnumString = "low";
			break;
		case ECodecksUserReportSeverity::None:
		default:
			EnumString = "";
		}
		JsonObject->SetStringField("severity", EnumString.ToLower());
	}

	JsonObject->SetStringField("userEmail", UserEmail);

	TArray<TSharedPtr<FJsonValue>> JsonFilenames;
	auto AddFilenamesForAttachments = [&JsonFilenames](const TArrayView<FAttachedFile>& Files) {
		for (const FAttachedFile& File : Files)
		{
			JsonFilenames.Add(MakeShared<FJsonValueString>(File.Filename));
		}
	};

	AddFilenamesForAttachments(AttachedFiles);

	JsonObject->SetArrayField("fileNames", JsonFilenames);
}

void UCodecksUserReportRequest::AttachFile(const FString& Filename, const FString& FileContents)
{
	AttachFile(Filename, TArray64<uint8>(reinterpret_cast<const uint8*>(GetData(FileContents)), FileContents.GetAllocatedSize()), "text/plain");
}

void UCodecksUserReportRequest::AttachFile(const FString& Filename, const TArrayView64<uint8>& Binary, FString ContentType)
{
	FAttachedFile& AttachedFile = AttachedFiles.AddDefaulted_GetRef();
	AttachedFile.Filename = Filename;
	AttachedFile.Binary = Binary;
	AttachedFile.ContentType = ContentType;
}

void UCodecksUserReportRequest::AttachFile(const FString& Filename, TArray64<uint8>&& Binary, FString ContentType)
{
	AttachFile(Filename, TArrayView64<uint8>(MoveTemp(Binary)), ContentType);
}

bool UCodecksUserReportRequest::IsOk() const
{
	return RequestState < ECodecksRequestState::Failed;
}

FName UCodecksUserReportRequest::Error() const
{
	return RequestState_Error;
}

void UCodecksUserReportRequest::CreateReport()
{
	CreateReport({});
}

void UCodecksUserReportRequest::CreateReport(const TFunction<void(UCodecksUserReportRequest* Request)>& UpdateCall)
{
	if (IsActive())
	{
		return;
	}

	// Build request
	BuildRequest();

	if (!IsOk())
	{
		UpdateCall(this);
		return;
	}

	Succeed(ECodecksRequestState::Initialized);
	UpdateCall(this);

	HttpRequest->OnProcessRequestComplete().BindWeakLambda(this, [this, UpdateCall](FHttpRequestPtr /*Request*/, FHttpResponsePtr Response, bool /*bConnectedSuccessfully*/) {
		TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();

		Succeed(ECodecksRequestState::ReportSubmitted);
		UpdateCall(this);

		const TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(Response->GetContentAsString());
		FJsonSerializer::Deserialize(JsonReader, JsonObject);

		const TArray<TSharedPtr<FJsonValue>>* UploadUrls = nullptr;
		JsonObject->TryGetArrayField("uploadUrls", UploadUrls);

		this->UploadAttachments(UploadUrls, UpdateCall, /*OnComplete=*/ [JsonObject, UpdateCall, this]() {
			FJsonObjectWrapper WrappedJson;
			WrappedJson.JsonObject = JsonObject;
			OnResponse.Broadcast(WrappedJson);

			// Can only succeed if not failed yet (maybe upload gone wrong)
			if (RequestState < ECodecksRequestState::Succeeded)
			{
				Succeed(ECodecksRequestState::Succeeded);
			}

			UpdateCall(this);
		});
	});

	// Calculate total bytes
	TotalBytesToSend = HttpRequest->GetContentLength();

	HttpRequest->OnRequestProgress().BindWeakLambda(this, [this](FHttpRequestPtr Request, int32 BytesSent, int32 /*BytesReceived*/) {
		const float Progress = static_cast<float>(BytesSent) / TotalBytesToSend;
		OnProgress.Broadcast(Progress);
	});

	// Kick off request on mainthread
	if (!HttpRequest->ProcessRequest())
	{
		Fail(CodecksRequestErrors::UnableToProcess);
	}
}

void UCodecksUserReportRequest::Succeed(ECodecksRequestState NextState)
{
	if (ensureAlways(RequestState < NextState))
	{
		RequestState = NextState;

		if (RequestState >= ECodecksRequestState::Succeeded)
		{
			HttpRequest.Reset();
		}
	}
	else
	{ }
}

void UCodecksUserReportRequest::ResetState()
{
	RequestState = ECodecksRequestState::Initializing;
	RequestState_Error = FName();
}

void UCodecksUserReportRequest::UploadAttachments(const TArray<TSharedPtr<FJsonValue>>* UploadUrls, const TFunction<void(UCodecksUserReportRequest* Request)>& UpdateCall, const TFunction<void()>& OnComplete)
{
	// Schedule onto taskgraph, to not block any execution

	Succeed(ECodecksRequestState::UploadingFiles);
	UpdateCall(this);

	Async(EAsyncExecution::TaskGraph, [this, UploadUrls, UpdateCall, OnComplete]() {

		// Wait for remaining intermediate screenshots
		ScreenshotPipe.WaitUntilEmpty();

		UE::Tasks::FPipe UploadPipe(UE_SOURCE_LOCATION);

		if (UploadUrls)
		{
			for (const TSharedPtr<FJsonValue>& UploadUrl : *UploadUrls)
			{
				TSharedPtr<FJsonObject> UploadObject = UploadUrl->AsObject();

				const FString UploadFilename = UploadObject->GetStringField("fileName");
				const FString UploadURL = UploadObject->GetStringField("url");

				// Those get copied to the final request
				const TSharedPtr<FJsonObject> UploadMetaFields = UploadObject->GetObjectField("fields");

				// Has data for file?
				if (FAttachedFile* AttachedFile = AttachedFiles.FindByPredicate([&UploadFilename](const FAttachedFile& A) { return A.Filename.Compare(UploadFilename) == 0; }))
				{
					// Proceed building multipart/form-data
					FString CLRF("\r\n");
					FString Dash("--");

					TArray<uint8> Bytes;
					TArray<uint8>::SizeType EstimatedSize = 64;
					FMemoryWriter Writer(Bytes, false, false);
					Writer.Seek(0);

					const FString FormBoundary = FString::FromInt(FDateTime::Now().GetTicks());

					// Header
					const TSharedRef<IHttpRequest> UploadRequest = FHttpModule::Get().CreateRequest();
					UploadRequest->SetVerb("POST");
					UploadRequest->SetURL(UploadURL);
					UploadRequest->SetHeader("Content-Type", "multipart/form-data; boundary=" + FormBoundary);

					FString FormStartBoundary = CLRF + Dash + FormBoundary + CLRF;

					// Set all meta fields according to AWS

					// Add content-type field dynamically as it is not part of metafields
					UploadMetaFields->Values.Add("Content-Type", MakeShared<FJsonValueString>(AttachedFile->ContentType));
					{
						for (TTuple<FString, TSharedPtr<FJsonValue>> Field : UploadMetaFields->Values)
						{
							Writer.Serialize(const_cast<ANSICHAR*>(StringCast<ANSICHAR>(*FormStartBoundary).Get()), FormStartBoundary.Len());

							FString header("Content-Disposition: form-data; name=\"" + Field.Key + "\"");
							Writer.Serialize(const_cast<ANSICHAR*>(StringCast<ANSICHAR>(*header).Get()), header.Len());
							Writer.Serialize(const_cast<ANSICHAR*>(StringCast<ANSICHAR>(*CLRF).Get()), CLRF.Len());

							// Also add content-type here, because you have to for S3
							FString ContentType = "Content-Type: text/plain; encoding=utf8";
							Writer.Serialize(const_cast<ANSICHAR*>(StringCast<ANSICHAR>(*ContentType).Get()), ContentType.Len());
							Writer.Serialize(const_cast<ANSICHAR*>(StringCast<ANSICHAR>(*CLRF).Get()), CLRF.Len());

							// Empty line between form-data header and content **IS** important
							Writer.Serialize(const_cast<ANSICHAR*>(StringCast<ANSICHAR>(*CLRF).Get()), CLRF.Len());

							FString sectionData = Field.Value->AsString();
							Writer.Serialize(const_cast<ANSICHAR*>(StringCast<ANSICHAR>(*sectionData).Get()), sectionData.Len());
						}
					}

					// Serialize binary data from file
					{
						Writer.Serialize(const_cast<ANSICHAR*>(StringCast<ANSICHAR>(*FormStartBoundary).Get()), FormStartBoundary.Len());

						// Format the file the way S3 wants it (in a form, with the file contents being a field called "file")
						const FString FileHeader("Content-Disposition: form-data; name=\"file\"; filename=\"" + AttachedFile->Filename + "\"");
						Writer.Serialize(const_cast<ANSICHAR*>(StringCast<ANSICHAR>(*FileHeader).Get()), FileHeader.Len());
						Writer.Serialize(const_cast<ANSICHAR*>(StringCast<ANSICHAR>(*CLRF).Get()), CLRF.Len());

						FString ContentType("Content-Type: " + AttachedFile->ContentType);
						Writer.Serialize(const_cast<ANSICHAR*>(StringCast<ANSICHAR>(*ContentType).Get()), ContentType.Len());
						Writer.Serialize(const_cast<ANSICHAR*>(StringCast<ANSICHAR>(*CLRF).Get()), CLRF.Len());

						// Empty line between form-data header and content **IS** important
						Writer.Serialize(const_cast<ANSICHAR*>(StringCast<ANSICHAR>(*CLRF).Get()), CLRF.Len());

						Writer.Serialize(AttachedFile->Binary.GetData(), AttachedFile->Binary.Num());
					}

					FString FormEndBoundary = CLRF + Dash + FormBoundary + Dash + CLRF;
					Writer.Serialize(const_cast<ANSICHAR*>(StringCast<ANSICHAR>(*FormEndBoundary).Get()), FormEndBoundary.Len());

					Writer.Close();

					UploadRequest->SetContent(Bytes);
					TotalBytesToSend += UploadRequest->GetContentLength();

					auto UploadFile = [this, UploadRequest, UploadFilename]() {
						UE::Tasks::FTaskEvent WaitForUpload(UE_SOURCE_LOCATION);

						UploadRequest->OnProcessRequestComplete().BindLambda([&WaitForUpload, UploadFilename, this](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully) {
							UE_LOG(LogCodecksUnreal, Log, TEXT("Uploading %s complete..."), *UploadFilename);
							if (Response->GetResponseCode() > 300)
							{
								UE_LOG(LogCodecksUnreal, Warning, TEXT("%d"), Response->GetResponseCode());
								UE_LOG(LogCodecksUnreal, Warning, TEXT("%s"), *Response->GetContentAsString());
							}

							WaitForUpload.Trigger();
							TotalBytesSent += Request->GetContentLength();
						});

						uint32 BytesSentTillNow = TotalBytesSent;

						UploadRequest->OnRequestProgress().BindLambda([this, BytesSentTillNow](FHttpRequestPtr Request, int32 BytesSent, int32 /*BytesReceived*/) {
							const float Progress = static_cast<float>(BytesSentTillNow + BytesSent) / TotalBytesToSend;
							AsyncTask(ENamedThreads::GameThread, [Progress, &ProgressDelegate(OnProgress)] {
								ProgressDelegate.Broadcast(Progress);
							});
						});

						// Enable to dump files into Saved/Inspection/Filename for debugging
#if 0 // DebugCodecksUploadAsFile
						FString Testfile = FPaths::ProjectSavedDir() / TEXT("Inspection") / UploadFilename + TEXT(".txt");
						FFileHelper::SaveArrayToFile(Bytes, *Testfile);
#endif // DebugCodecksUploadAsFile

						UploadRequest->ProcessRequest();
						WaitForUpload.Wait();
					};

					UploadPipe.Launch(TEXT("Codecks_UploadFile"), MoveTemp(UploadFile));
				}
			}
		}

		// Sequential upload
		UploadPipe.WaitUntilEmpty();

		AsyncTask(ENamedThreads::GameThread, [OnComplete]() {
			OnComplete();
		});
	});
}

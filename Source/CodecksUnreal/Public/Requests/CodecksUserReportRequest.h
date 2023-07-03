// Copyright(C) battyRabbit UG (limited liability). All Rights Reserved.

#pragma once

#include <CoreMinimal.h>

#include <Dom/JsonObject.h>
#include <JsonObjectWrapper.h>
#include <Tasks/Pipe.h>
#include <UObject/Object.h>

#include "CodecksUserReportRequest.generated.h"

class IHttpRequest;

UENUM(BlueprintType)
enum class ECodecksUserReportSeverity : uint8
{
	Critical,
	High,
	Low,
	None
};

UENUM(BlueprintType)
enum class ECodecksRequestState : uint8
{
	Initializing,
	Initialized,
	ReportSubmitted,
	UploadingFiles,
	Succeeded,

	Failed
};

namespace CodecksRequestErrors
{
	const FName NoRequestToken = FName("NO_REPORT_TOKEN");
	const FName NoConnection = FName("NO_CONNECTION");
	const FName ErrorResponse = FName("HTTP_ERROR");
	const FName UnableToProcess = FName("UNABLE_TO_PROCESS");
	const FName NoContent = FName("NO_CONTENT");
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCodecksUnrealUserRequestProgressDelegate, float, Progress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCodecksUnrealUserRequestResponseDelegate, FJsonObjectWrapper, Response);

/**
 * Actual implementation of a user report request for the Service
 * Indirection here so it is easier to change while the process can stay the same
 */
UCLASS(BlueprintType, ClassGroup=(CodecksUnreal))
class CODECKSUNREAL_API UCodecksUserReportRequest : public UObject
{
	GENERATED_BODY()

public:
	UCodecksUserReportRequest();

	virtual void BeginDestroy() override;

	virtual void BuildRequest();

	void SetContent(FString InContent) { Content = MoveTemp(InContent); }

	// May override to read token from somewhere else or some app state
	virtual FString GetReportToken() const;

	ECodecksRequestState GetRequestState() const { return RequestState; }

	/**
	 * Appends a string to the existing content.
	 * This is useful to append meta-information to the content automatically.
	 * I.e. Buildversion, bugit output and other meta information.
	 */
	UFUNCTION(BlueprintCallable)
	void AppendContent(const FString& Appendex) { Content += Appendex; }

	/**
	 * Creates an intermediate screenshot in memory and attaches it.
	 */
	UFUNCTION(BlueprintCallable)
	void AttachIntermediateScreenshot(bool bShowUI = false);

	void AddRequestData(const TSharedPtr<FJsonObject>& JsonObject);

	UFUNCTION(BlueprintCallable)
	bool IsActive() const { return RequestState > ECodecksRequestState::Initializing || HttpRequest.IsValid(); }

	UFUNCTION(BlueprintCallable)
	void AttachFile(const FString& Filename, const FString& FileContents);
	void AttachFile(const FString& Filename, TArray64<uint8>&& Binary, FString ContentType);
	void AttachFile(const FString& Filename, const TArrayView64<uint8>& Binary, FString ContentType);

	bool IsOk() const;
	FName Error() const;

	UFUNCTION(BlueprintCallable)
	void SetEmail(FString InEmail) { UserEmail = MoveTemp(InEmail); }

	UFUNCTION(BlueprintCallable)
	void SetSeverity(ECodecksUserReportSeverity InSeverity) { Severity = InSeverity; }

	virtual void CreateReport();
	virtual void CreateReport(const TFunction<void(UCodecksUserReportRequest* Request)>& UpdateCall);

	friend FJsonObject& operator<<(FJsonObject& Json, UCodecksUserReportRequest& Request);

protected:
	void Fail(const FName& FailureToken)
	{
		RequestState = ECodecksRequestState::Failed;
		RequestState_Error = FailureToken;
	}

	void Succeed(ECodecksRequestState NextState);

	void ResetState();

	void UploadAttachments(const TArray<TSharedPtr<FJsonValue>>* UploadUrls, const TFunction<void(UCodecksUserReportRequest* Request)>& UpdateCall, const TFunction<void()>& OnComplete);

	TSharedPtr<IHttpRequest> HttpRequest;

	UPROPERTY(BlueprintAssignable)
	FCodecksUnrealUserRequestProgressDelegate OnProgress;

	UPROPERTY(BlueprintAssignable)
	FCodecksUnrealUserRequestResponseDelegate OnResponse;

	UPROPERTY(BlueprintReadOnly)
	ECodecksRequestState RequestState = ECodecksRequestState::Initializing;

	/**
	 * Internal error keys for requests
	 *
	 * Note this can be used to localize the state a custom function to convert FName to FText
	 * A good way of doing so would be to have a StringTable of all error messages with the valid FNames as keys,
	 * @see CodecksRequestErrors for a list of error tokens
	 */
	UPROPERTY(BlueprintReadOnly)
	FName RequestState_Error;

	UPROPERTY(BlueprintReadWrite, meta=(ExposeOnSpawn))
	FString Content;

	UPROPERTY(BlueprintReadWrite, meta=(ExposeOnSpawn))
	ECodecksUserReportSeverity Severity = ECodecksUserReportSeverity::None;

	UPROPERTY(BlueprintReadWrite, meta=(ExposeOnSpawn))
	FString UserEmail;

	struct FAttachedFile
	{
		FString Filename;
		TArray64<uint8> Binary;
		FString ContentType;
	};

	UPROPERTY(Transient)
	uint32 TotalBytesToSend = 0;
	UPROPERTY(Transient)
	uint32 TotalBytesSent = 0;

	TArray<FAttachedFile> AttachedFiles;

	UE::Tasks::FPipe ScreenshotPipe = UE::Tasks::FPipe(UE_SOURCE_LOCATION);
};

inline const TSharedPtr<FJsonObject>& operator<<(const TSharedPtr<FJsonObject>& Json, UCodecksUserReportRequest::ThisClass& Request)
{
	Request.AddRequestData(Json);
	return Json;
}

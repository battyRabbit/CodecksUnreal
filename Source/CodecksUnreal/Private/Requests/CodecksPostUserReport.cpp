// Copyright(C) battyRabbit UG (limited liability). All Rights Reserved.

#include "Requests/CodecksPostUserReport.h"

UCodecksPostUserReport* UCodecksPostUserReport::PostUserReport(UObject* AppContext, UCodecksUserReportRequest* Request)
{
	if (!Request)
	{
		FFrame::KismetExecutionMessage(TEXT("Missing request"), ELogVerbosity::Error);
		return nullptr;
	}

	UCodecksPostUserReport* Task = NewObject<UCodecksPostUserReport>(AppContext);
	Task->Request = Request;
	Task->RegisterWithGameInstance(AppContext);

	return Task;
}

void UCodecksPostUserReport::Activate()
{
	Super::Activate();

	Request->CreateReport([this](UCodecksUserReportRequest* Update) {
		switch (Update->GetRequestState())
		{
		case ECodecksRequestState::Initialized:
		case ECodecksRequestState::ReportSubmitted:
		case ECodecksRequestState::UploadingFiles:
			OnUpdate(Update);
			break;
		case ECodecksRequestState::Succeeded:
			OnComplete(Update);
			break;
		case ECodecksRequestState::Failed:
			OnFailure(Update);
			break;
		default: ;
		}
	});
}

void UCodecksPostUserReport::Cancel()
{
	Teardown();
	Super::Cancel();
}

void UCodecksPostUserReport::OnUpdate(UCodecksUserReportRequest* UpdatedRequest)
{
	OnUpdated.Broadcast(UpdatedRequest);
}

void UCodecksPostUserReport::OnFailure(UCodecksUserReportRequest* FailedRequest)
{
	OnCompleted.Broadcast(FailedRequest);

	Teardown();
}

void UCodecksPostUserReport::OnComplete(UCodecksUserReportRequest* CompletedRequest)
{
	OnCompleted.Broadcast(CompletedRequest);

	Teardown();
}

void UCodecksPostUserReport::Teardown()
{
	SetReadyToDestroy();
}

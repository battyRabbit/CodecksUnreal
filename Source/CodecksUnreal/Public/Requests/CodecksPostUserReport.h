// Copyright(C) battyRabbit UG (limited liability). All Rights Reserved.

#pragma once

#include <CoreMinimal.h>

#include "CodecksUserReportRequest.h"

#include <Engine/CancellableAsyncAction.h>

#include "CodecksPostUserReport.generated.h"

class UCodecksUserReportRequest;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCodecksUnrealUserRequestUpdateDelegate, UCodecksUserReportRequest*, ActiveRequest);


UCLASS(ClassGroup=(CodecksUnreal), meta=(ExposedAsyncProxy))
class UCodecksPostUserReport : public UCancellableAsyncAction
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta=(BlueprintInternalUseOnly=true, WorldContext="AppContext", HidePin="AppContext", Keywords="codecks, user, report, bug"))
	static UCodecksPostUserReport* PostUserReport(UObject* AppContext, UCodecksUserReportRequest* Request);

	virtual void Activate() override;
	virtual void Cancel() override;

	UPROPERTY(BlueprintAssignable)
	FCodecksUnrealUserRequestUpdateDelegate OnUpdated;

	UPROPERTY(BlueprintAssignable)
	FCodecksUnrealUserRequestUpdateDelegate OnCompleted;

protected:
	virtual void OnUpdate(UCodecksUserReportRequest* UpdatedRequest);
	virtual void OnFailure(UCodecksUserReportRequest* FailedRequest);
	virtual void OnComplete(UCodecksUserReportRequest* CompletedRequest);

	virtual void Teardown();

	UPROPERTY()
	UCodecksUserReportRequest* Request;
};

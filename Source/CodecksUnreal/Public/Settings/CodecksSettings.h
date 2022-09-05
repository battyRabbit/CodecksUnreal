// Copyright(C) battyRabbit UG (limited liability). All Rights Reserved.

#pragma once

#include <CoreMinimal.h>

#include "CodecksSettings.generated.h"

class UCodecksUserReportService;

UCLASS(NotBlueprintType, DefaultConfig, Config=Game, ClassGroup=(CodecksUnreal), DisplayName="Codecks Unreal")
class CODECKSUNREAL_API UCodecksSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UCodecksSettings();

	FString GetReportToken() const { return ReportToken; }

	FString GetApiUrl() const {return CodecksApiURL; }

protected:
	/**
	 * @brief Token get by codecks organization settings or generated(and injected) during build process.
	 */
	UPROPERTY(Config, EditAnywhere)
	FString ReportToken;

	/**
	 * Current backend api
	 */
	UPROPERTY(Config, EditAnywhere)
	FString CodecksApiURL;

};

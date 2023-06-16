// Copyright(C) Sebastian Krause. All Rights Reserved.

#include <CoreMinimal.h>

#include "Requests/CodecksUserReportRequest.h"


// Just here so we don't need to friend / give access to AttachedFiles
class UCodecksUserReportRequest_Internal : public UCodecksUserReportRequest
{
public:
	using UCodecksUserReportRequest::AttachedFiles;
};

BEGIN_DEFINE_SPEC(FCodecksUnrealReportRequest, "CodecksUnreal.Attachments", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	TObjectPtr<UCodecksUserReportRequest_Internal> Request = nullptr;
END_DEFINE_SPEC(FCodecksUnrealReportRequest)

void FCodecksUnrealReportRequest::Define()
{
	BeforeEach([this]
	{
		Request = NewObject<UCodecksUserReportRequest_Internal>(GetTransientPackage());
	});

	Describe("Attach Files", [this]()
	{
		It("Attached file content contains all data with UTF8 support", [this]()
		{
			static const FString Filename = "test.log";
			static const FString Content = TEXT("JBhMAMLMUNLs6uy5cw7iWBoXo3SFI5SP狗ジャパニーズ");
			Request->AttachFile(Filename, Content);
			const auto& AttachedFile = Request->AttachedFiles[0];

			TestEqual("Filename matches", AttachedFile.Filename, Filename);
			const FString Backported = reinterpret_cast<const TCHAR*>(AttachedFile.Binary.GetData());
			TestEqual("File content matches", Backported, Content);
		});
	});

	AfterEach([this]
	{
		Request = nullptr;
	});
}



#pragma once

#include "CoreMinimal.h"

class FCodecksUnrealEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};

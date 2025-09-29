// Copyright ySion

#pragma once

#include "Modules/ModuleManager.h"

class FDaxSystemModule : public IModuleInterface {
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "DaxSetAsset.h"
#include "IAssetTools.h"
#include "DaxSystemFactories.generated.h"

UCLASS()
class DAXSYSTEMEDITOR_API UDaxSetFactory : public UFactory {
    GENERATED_BODY()

public:
    UDaxSetFactory() {
        bCreateNew = true;
        bEditAfterNew = true;
        SupportedClass = UDaxSetAsset::StaticClass();
    }

    virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
                                      EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override {
        return NewObject<UDaxSetAsset>(InParent, Class, Name, Flags);
    }

    virtual FText GetDisplayName() const override {
        return NSLOCTEXT("DaxSetFactory", "DaxSetFactoryName", "Dax Set");
    }

    virtual uint32 GetMenuCategories() const override {
        return EAssetTypeCategories::Misc;
    }

    virtual bool ShouldShowInNewMenu() const override { return true; }
};
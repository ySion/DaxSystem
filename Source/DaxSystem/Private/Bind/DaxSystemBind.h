#pragma once

#include "CoreMinimal.h"
#include "DaxSystem/Private/DaxSet.h"
#include "DaxSystemBind.generated.h"

UCLASS()
class UDaxVisitorBind : public UBlueprintFunctionLibrary {
    GENERATED_BODY()
public:
    
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "DaxSystem", meta = (NotInAngelscript))
    static FDaxVisitor gn(const FDaxVisitor& Visitor, const FName& Name) {
        return const_cast<FDaxVisitor&>(Visitor).MakeVisitorByName(Name);
    }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "DaxSystem", meta = (NotInAngelscript))
    static FDaxVisitor gs(const FDaxVisitor& Visitor, const FString& Path) {
        return const_cast<FDaxVisitor&>(Visitor).MakeVisitorByName(FName(Path));
    }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "DaxSystem", meta = (NotInAngelscript))
    static FDaxVisitor gi(const FDaxVisitor& Visitor, int Index) {
        return const_cast<FDaxVisitor&>(Visitor).MakeVisitorByIndex(Index);
    }

    UFUNCTION(BlueprintCallable, Category = "DaxSystem", meta = (NotInAngelscript))
    static FInstancedStruct TryGetValue(const FDaxVisitor& Visitor, const UScriptStruct* ValueType) {
        auto Temp = const_cast<FDaxVisitor&>(Visitor).TryGetValue(ValueType);
        FInstancedStruct TempInstanced;
        TempInstanced.InitializeAs(Temp.GetScriptStruct(), const_cast<uint8*>(Temp.GetMemory()));
        return TempInstanced;
    }
    
    UFUNCTION(BlueprintCallable, Category = "DaxSystem", meta = (NotInAngelscript))
    static FDaxResultDetail TrySetValue(const FDaxVisitor& Visitor, const FInstancedStruct& Value) {
        return const_cast<FDaxVisitor&>(Visitor).TrySetValue(Value);
    }

    UFUNCTION(BlueprintCallable, Category = "DaxSystem", meta = (NotInAngelscript))
    static FDaxResultDetail EnsureAndSetValue(const FDaxVisitor& Visitor, const FInstancedStruct& Value) {
        return const_cast<FDaxVisitor&>(Visitor).EnsureAndSetValue(Value);
    }
};


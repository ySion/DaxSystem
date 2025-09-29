#pragma once

#include "CoreMinimal.h"
#include "DaxSet.h"
#include "Components/ActorComponent.h"
#include "DaxComponent.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DAXSYSTEM_API UDaxComponent : public UActorComponent {
    GENERATED_BODY()

public:
    UDaxComponent();

    UPROPERTY(Replicated)
    FDaxSet DataSet;

    virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
    
    UFUNCTION(BlueprintCallable)
    FDaxVisitor GetVisitor() const { return DataSet.GetVisitor(); }

    UFUNCTION(BlueprintCallable)
    FDaxVisitor GetVisitorFromPath(const FString& Path) const { return DataSet.GetVisitorFromPath(Path); }

    UFUNCTION(BlueprintCallable)
    FString GetString() const { return DataSet.GetString(); }

    UFUNCTION(BlueprintCallable)
    FString GetStringDebug() const { return DataSet.GetStringDebug(); }

    UFUNCTION(BlueprintCallable)
    int GetNodeNum() const { return DataSet.GetNodeNum(); }

    UFUNCTION(BlueprintCallable)
    bool BindOnChanged(const FDaxVisitor& Position, const int32 Depth, UObject* Target, const FName& FuncName);
    
    UFUNCTION(BlueprintCallable)
    void UnbindOnChanged(const FDaxVisitor& Position);

    UFUNCTION(BlueprintCallable)
    void UnbindAllFor(UObject* TargetObject);

    UFUNCTION(BlueprintCallable)
    void MarkDirty();
};
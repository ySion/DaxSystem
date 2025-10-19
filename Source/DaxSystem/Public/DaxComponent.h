#pragma once

#include "CoreMinimal.h"
#include "DaxSystem/Private/DaxSet.h"
#include "Components/ActorComponent.h"
#include "DaxComponent.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DAXSYSTEM_API UDaxComponent : public UActorComponent {
    GENERATED_BODY()

public:
    UDaxComponent();
    
    // 为批量 PushModel 提供缓冲：仅置位，由 Subsystem 统一 Flush
    UPROPERTY(Transient)
    bool bPendingDirty = false;
    
    UPROPERTY(Replicated)
    FDaxSet DataSet {};

    virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

    // 调整注册时机：在 OnRegister/OnUnregister 与 Subsystem 交互
    virtual void OnRegister() override;
    virtual void OnUnregister() override;
    
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

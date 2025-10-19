#include "DaxSystem/Public/DaxComponent.h"
#include "DaxSystem/Public/DaxSubsystem.h"
#include "Net/UnrealNetwork.h"

UDaxComponent::UDaxComponent() {
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
    DataSet.ParentComponent = this;
}

void UDaxComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const {
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    FDoRepLifetimeParams SharedParams{};
    SharedParams.bIsPushBased = true;
    DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, DataSet, SharedParams);
}

void UDaxComponent::OnRegister() {
    Super::OnRegister();
    if (UWorld* W = GetWorld()) {
        if (UDaxSubsystem* Sys = W->GetSubsystem<UDaxSubsystem>()) {
            Sys->RegisterComponent(this);
        }
    }
}

void UDaxComponent::OnUnregister() {
    if (UWorld* W = GetWorld()) {
        if (UDaxSubsystem* Sys = W->GetSubsystem<UDaxSubsystem>()) {
            Sys->UnregisterComponent(this);
        }
    }
    Super::OnUnregister();
}

bool UDaxComponent::BindOnChanged(const FDaxVisitor& Position, const int32 Depth, UObject* Target, const FName& FuncName) {
    FDaxOnChangedDynamic Delegate;
    Delegate.BindUFunction(Target, FuncName);
    if (!Delegate.IsBound()) return false;
    return DataSet.BindOnChanged(Position, Depth, Delegate);
}

void UDaxComponent::UnbindOnChanged(const FDaxVisitor& Position) {
    return DataSet.UnbindOnChanged(Position);
}

void UDaxComponent::UnbindAllFor(UObject* TargetObject) {
    return DataSet.UnbindAllFor(TargetObject);
}

void UDaxComponent::MarkDirty() {
    // 改为仅置位，由 Subsystem 统一 Flush，降低 Push 次数
    bPendingDirty = true;
}
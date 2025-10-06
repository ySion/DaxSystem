#include "DaxComponent.h"

#include "DaxSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"

UDaxComponent::UDaxComponent() {
    PrimaryComponentTick.bCanEverTick = false;
    const auto W = GetWorld();
    if (!W) return;
    
    auto Sys = W->GetSubsystem<UDaxSubsystem>();
    if (!Sys) return;
    
    Sys->RegisterComponent(this);
    SetIsReplicatedByDefault(true);
    DataSet.ParentComponent = this;
}

void UDaxComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const {
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    FDoRepLifetimeParams SharedParams{};
    SharedParams.bIsPushBased = true;
    DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, DataSet, SharedParams);
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
    MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, DataSet, this);
}
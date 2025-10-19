#include "DaxSystem/Public/DaxSubsystem.h"
#include "Net/Core/PushModel/PushModel.h"

bool UDaxSubsystem::ShouldCreateSubsystem(UObject* Outer) const {
    if (!IsValid(Outer)) return false;
    const UWorld* World = Cast<UWorld>(Outer);
    if (!World) return false;

    const EWorldType::Type WT = World->WorldType;
    const bool bCreateCondition = (WT == EWorldType::Game) || (WT == EWorldType::PIE) || (WT == EWorldType::GamePreview);
    if (!bCreateCondition) return false;
    return true;
}

void UDaxSubsystem::Initialize(FSubsystemCollectionBase& Collection) {
    Super::Initialize(Collection);
    PreActorTickHandle = FWorldDelegates::OnWorldPreActorTick.AddUObject(this, &UDaxSubsystem::HandleWorldPreActorTick);
}

void UDaxSubsystem::Deinitialize() {
    if (PreActorTickHandle.IsValid()) {
        FWorldDelegates::OnWorldPreActorTick.Remove(PreActorTickHandle);
        PreActorTickHandle.Reset();
    }
    Super::Deinitialize();
}

void UDaxSubsystem::HandleWorldPreActorTick(UWorld* InWorld, ELevelTick TickType, float DeltaSeconds) {
    if (InWorld == GetWorld() && InWorld != nullptr) {
        EarlyTick(DeltaSeconds);
    }
}

void UDaxSubsystem::EarlyTick(float DeltaSeconds) {
    // 当前阶段仅做批量 Push 和帧变更集合清理，监听事件尚未实现
    CollectEvent();
    DispatchEvent();
}

void UDaxSubsystem::RegisterComponent(UDaxComponent* Component) {
    if (!IsValid(Component) || Component->IsBeingDestroyed()) return;
    DaxComponentTable.AddUnique(Component);
    UE_LOGFMT(DataXSystem, Log, "DaxSubsystem RegisterComponent: {0}, Parent: {1}", *Component->GetName(), Component->GetOwner() ? *Component->GetOwner()->GetName() : TEXT("null"));
}

void UDaxSubsystem::UnregisterComponent(UDaxComponent* Component) {
    if (!Component) return;
    DaxComponentTable.RemoveSwap(Component);
    UE_LOGFMT(DataXSystem, Log, "DaxSubsystem UnregisterComponent: {0}", Component ? *Component->GetName() : TEXT("null"));
}

void UDaxSubsystem::CollectEvent() {
    // 预留：后续在此做多线程筛选/聚合（监听系统未启用）
}

void UDaxSubsystem::DispatchEvent() {
    // 批量 Flush PushModel + 清理本帧变更集合
    DaxComponentTable.RemoveAll([](UDaxComponent* Comp) {
        if (!IsValid(Comp) || Comp->IsBeingDestroyed()) return true;
        if (Comp->bPendingDirty) {
            MARK_PROPERTY_DIRTY_FROM_NAME(UDaxComponent, DataSet, Comp);
            Comp->bPendingDirty = false;
        }
        Comp->DataSet.ClearFrameChangedNodes();
        return false;
    });
}
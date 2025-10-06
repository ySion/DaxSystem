#include "DaxSubsystem.h"

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

    ParallelFor(DaxComponentTable.Num(), [&](int32 Index) {
        auto Ptr = DaxComponentTable[Index];
        if (!IsValid(Ptr)) return;
    });
    CollectEvent();
    DispatchEvent();
}

void UDaxSubsystem::RegisterComponent(UDaxComponent* Component) {
    if (!IsValid(Component) || Component->IsBeingDestroyed()) return;
    DaxComponentTable.AddUnique(Component);
    UE_LOGFMT(DataXSystem, Log, "DaxSubsystem RegisterComponent: {0}, Parent: {1}", *Component->GetName(), Component->GetOwner() ? *Component->GetOwner()->GetName() : TEXT("null"));
}

void UDaxSubsystem::CollectEvent() {
    
}

void UDaxSubsystem::DispatchEvent() {
    
}
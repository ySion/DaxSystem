#pragma once

#include "CoreMinimal.h"
#include "DaxComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "DaxSubsystem.generated.h"

UCLASS(BlueprintType)
class DAXSYSTEM_API UDaxSubsystem : public UWorldSubsystem {
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override; // ★ 决定是否创建
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    void EarlyTick(float DeltaSeconds);

    void RegisterComponent(UDaxComponent* Component);
    void UnregisterComponent(UDaxComponent* Component);
    
    UFUNCTION(BlueprintCallable)
    TArray<UDaxComponent*> GetDaxComponentTable() const { return DaxComponentTable; }

protected:
    void HandleWorldPreActorTick(UWorld* InWorld, ELevelTick TickType, float DeltaSeconds);

    void CollectEvent();
    void DispatchEvent();

private:
    FDelegateHandle PreActorTickHandle;

    TArray<UDaxComponent*> DaxComponentTable{};
};

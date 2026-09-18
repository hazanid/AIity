#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Simulation/AIityBridgeProtocol.h"
#include "Simulation/AIitySimulationClock.h"
#include "Simulation/AIityWorldState.h"
#include "Persistence/AIityWorldStore.h"
#include "AIityWorldSubsystem.generated.h"

UCLASS()
class AIITY_API UAIityWorldSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

	const AIity::FWorldState& GetCommittedState() const { return CommittedState; }
	void SubmitMovementReceipt(const AIity::FMovementReceipt& Receipt);
	bool TogglePause();
	bool RetryPersistence();
	void MarkStartupFailure(const FString& Error);
	void SetSpeed(int32 NewSpeed);
	bool IsPaused() const { return Clock.IsPaused(); }
	bool IsReady() const { return Clock.IsReady(); }
	bool HasPersistenceFailure() const { return Clock.HasFailure(); }
	int32 GetSpeed() const { return Clock.GetSpeed(); }
	float GetSimulationDelta(float RealSeconds) const { return Clock.ScaleDelta(RealSeconds); }
	float GetMovementSpeedScale(float RealSeconds) const { return Clock.MovementSpeedScale(RealSeconds); }
	const FString& GetStatus() const { return Status; }
	bool SaveNow();

private:
	TUniquePtr<FAIityWorldStore> Store;
	AIity::FWorldState CommittedState;
	AIity::FBridgeReceiptQueue PendingReceipts;
	AIity::FSimulationClock Clock;
	float Accumulator = 0.0f;
	FString Status = TEXT("Starting");
	bool bShuttingDown = false;

	bool CommitAndPublish(const AIity::FCandidateTick& Candidate);
	void EnterPersistenceFailure(const FString& Error);
};

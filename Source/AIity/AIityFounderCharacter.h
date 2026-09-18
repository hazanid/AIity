#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AIityFounderCharacter.generated.h"

class UStaticMeshComponent;

UCLASS()
class AIITY_API AAIityFounderCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AAIityFounderCharacter();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	void InitializeFounder(uint64 InFounderId, const FString& InName, int32 ColorIndex);
	void ResetBridgeExecution();
	uint64 GetFounderId() const { return FounderId; }

private:
	UPROPERTY()
	UStaticMeshComponent* Body;

	UPROPERTY()
	UStaticMeshComponent* Head;

	uint64 FounderId = 0;
	uint64 ActiveActionId = 0;
	uint64 ActiveEpoch = 0;
	float MovementSeconds = 0.0f;
	bool bTerminalSent = false;
};

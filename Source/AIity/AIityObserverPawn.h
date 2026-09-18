#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SpectatorPawn.h"
#include "AIityObserverPawn.generated.h"

UCLASS()
class AIITY_API AAIityObserverPawn : public ASpectatorPawn
{
	GENERATED_BODY()

public:
	AAIityObserverPawn();
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
	bool bNeedsInitialView = true;
	void LookYaw(float Value);
	void LookPitch(float Value);
};

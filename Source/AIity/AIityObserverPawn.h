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
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
};

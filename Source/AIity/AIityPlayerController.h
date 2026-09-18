#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "AIityPlayerController.generated.h"

UCLASS()
class AIITY_API AAIityPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AAIityPlayerController();
	virtual void SetupInputComponent() override;
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;
	virtual void PlayerTick(float DeltaTime) override;
	uint64 GetSelectedFounderId() const { return SelectedFounderId; }
	bool IsFollowing() const { return bFollowing; }

private:
	uint64 SelectedFounderId = 1;
	bool bFollowing = false;

	void SelectNextFounder();
	void SelectPreviousFounder();
	void SelectRelativeFounder(int32 Direction);
	void ToggleFollow();
	void TogglePause();
	void RetryPersistence();
	void SpeedOne();
	void SpeedTwo();
	void SpeedFour();
};

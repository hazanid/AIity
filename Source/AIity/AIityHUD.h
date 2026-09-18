#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "AIityHUD.generated.h"

class UFont;

UCLASS()
class AIITY_API AAIityHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	float DrawWrappedText(const FString& Text, float X, float Y, float MaxWidth,
		const FLinearColor& Color, UFont* Font, float Scale, float MaxY);
};

#include "AIityHUD.h"

#include "AIityGameMode.h"
#include "AIityPlayerController.h"
#include "AIityWorldSubsystem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Presentation/AIityPresentationHelpers.h"

namespace
{
const TCHAR* ActionLabel(AIity::EActionKind Action)
{
	switch (Action)
	{
	case AIity::EActionKind::GatherFood: return TEXT("Gathering food");
	case AIity::EActionKind::GatherWater: return TEXT("Gathering water");
	case AIity::EActionKind::Eat: return TEXT("Eating");
	case AIity::EActionKind::Drink: return TEXT("Drinking");
	case AIity::EActionKind::Rest: return TEXT("Resting");
	default: return TEXT("Choosing");
	}
}
}

float AAIityHUD::DrawWrappedText(const FString& Text, float X, float Y, float MaxWidth,
	const FLinearColor& Color, UFont* Font, float Scale, float MaxY, bool bDraw)
{
	if (Text.IsEmpty() || MaxWidth <= 0.0f || !Font)
	{
		return Y;
	}
	float SampleWidth = 0.0f;
	float SampleHeight = 0.0f;
	GetTextSize(TEXT("Ag"), SampleWidth, SampleHeight, Font, Scale);
	const float LineAdvance = FMath::Max(12.0f * Scale, SampleHeight + 3.0f * Scale);
	TArray<FString> Lines;
	TArray<FString> Words;
	Text.ParseIntoArrayWS(Words);
	FString Line;
	for (const FString& Word : Words)
	{
		const FString Candidate = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
		float CandidateWidth = 0.0f;
		float CandidateHeight = 0.0f;
		GetTextSize(Candidate, CandidateWidth, CandidateHeight, Font, Scale);
		if (CandidateWidth <= MaxWidth)
		{
			Line = Candidate;
			continue;
		}
		if (!Line.IsEmpty())
		{
			Lines.Add(Line);
		}
		Line.Reset();

		float WordWidth = 0.0f;
		GetTextSize(Word, WordWidth, CandidateHeight, Font, Scale);
		if (WordWidth <= MaxWidth)
		{
			Line = Word;
			continue;
		}
		FString Chunk;
		for (int32 Index = 0; Index < Word.Len(); ++Index)
		{
			const FString CandidateChunk = Chunk + Word.Mid(Index, 1);
			float ChunkWidth = 0.0f;
			GetTextSize(CandidateChunk, ChunkWidth, CandidateHeight, Font, Scale);
			if (ChunkWidth > MaxWidth && !Chunk.IsEmpty())
			{
				Lines.Add(Chunk);
				Chunk = Word.Mid(Index, 1);
			}
			else
			{
				Chunk = CandidateChunk;
			}
		}
		Line = Chunk;
	}
	if (!Line.IsEmpty())
	{
		Lines.Add(Line);
	}
	for (int32 Index = 0; Index < Lines.Num() && Y + LineAdvance <= MaxY; ++Index)
	{
		FString VisibleLine = Lines[Index];
		if (Index + 1 < Lines.Num() && Y + 2.0f * LineAdvance > MaxY)
		{
			// Keep the last visible line inside the panel and disclose omitted text.
			float Width = 0.0f;
			float Height = 0.0f;
			do
			{
				GetTextSize(VisibleLine + TEXT("..."), Width, Height, Font, Scale);
				if (Width <= MaxWidth || VisibleLine.IsEmpty()) break;
				VisibleLine.LeftChopInline(1);
			} while (true);
			VisibleLine += TEXT("...");
		}
		if (bDraw) DrawText(VisibleLine, Color, X, Y, Font, Scale);
		Y += LineAdvance;
	}
	return Y;
}

float AAIityHUD::GetFontScale(UFont* Font, float PixelHeight) const
{
	float Width = 0.0f;
	float Height = 0.0f;
	GetTextSize(TEXT("Ag"), Width, Height, Font, 1.0f);
	return PixelHeight / FMath::Max(1.0f, Height);
}

void AAIityHUD::DrawHUD()
{
	Super::DrawHUD();
	const UAIityWorldSubsystem* Simulation = GetWorld()->GetSubsystem<UAIityWorldSubsystem>();
	const AAIityPlayerController* Controller = Cast<AAIityPlayerController>(PlayerOwner);
	if (!Simulation || !Controller || !Canvas || !GEngine)
	{
		return;
	}
	const AIity::FWorldState& State = Simulation->GetCommittedState();
	AIity::FHudLayout Layout = AIity::BuildHudLayout(Canvas->ClipX, Canvas->ClipY);
	UFont* BodyFont = GEngine->GetSmallFont();
	UFont* HeadingFont = GEngine->GetLargeFont();
	const float BodyScale = GetFontScale(BodyFont, Layout.BodyTextHeight);
	const float HeadingScale = GetFontScale(HeadingFont, Layout.HeadingTextHeight);
	const AAIityGameMode* GameMode = GetWorld()->GetAuthGameMode<AAIityGameMode>();
	const FString Status = GameMode && !GameMode->GetStartupFailure().IsEmpty()
		? GameMode->GetStartupFailure() : Simulation->GetStatus();
	const float StatusContentHeight = DrawWrappedText(TEXT("STATUS"), 0, 0,
		Layout.ContentWidth, FLinearColor::White, BodyFont, BodyScale, MAX_flt, false)
		+ 3.0f * Layout.Scale + DrawWrappedText(Status, 0, 0, Layout.ContentWidth,
			FLinearColor::White, BodyFont, BodyScale, MAX_flt, false);
	Layout = AIity::BuildHudLayout(Canvas->ClipX, Canvas->ClipY, StatusContentHeight);
	const float PanelHeight = Canvas->ClipY - Layout.Margin * 2.0f;
	const float X = Layout.Margin + Layout.Padding;
	const float StatusTop = Canvas->ClipY - Layout.Margin - Layout.StatusHeight;
	const float BodyBottom = StatusTop - Layout.Padding;
	DrawRect(FLinearColor(0.025f, 0.035f, 0.025f, 0.9f),
		Layout.Margin, Layout.Margin, Layout.PanelWidth, PanelHeight);
	const FLinearColor Primary(0.92f, 0.86f, 0.68f);
	const FLinearColor Muted(0.68f, 0.72f, 0.62f);
	const FLinearColor Failure(1.0f, 0.55f, 0.42f);
	float Y = Layout.Margin + Layout.Padding;
	Y = DrawWrappedText(TEXT("AIITY  /  RIVER VALLEY"), X, Y, Layout.ContentWidth,
		Primary, HeadingFont, HeadingScale, BodyBottom);
	Y += 5.0f * Layout.Scale;
	Y = DrawWrappedText(FString::Printf(TEXT("%s  |  %dx  |  tick %llu"),
		Simulation->IsPaused() ? TEXT("PAUSED") : TEXT("RUNNING"),
		Simulation->GetSpeed(), static_cast<unsigned long long>(State.Tick)),
		X, Y, Layout.ContentWidth, Muted, BodyFont, BodyScale, BodyBottom);
	Y += 5.0f * Layout.Scale;
	Y = DrawWrappedText(
		TEXT("[ / ] founder  ·  click select  ·  F follow  ·  SPACE pause  ·  R retry  ·  1/2/4 speed"),
		X, Y, Layout.ContentWidth, Muted, BodyFont, BodyScale, BodyBottom);
	Y += 12.0f * Layout.Scale;

	const AIity::FFounder* Selected = nullptr;
	for (const AIity::FFounder& Founder : State.Founders)
	{
		if (Founder.Id == Controller->GetSelectedFounderId())
		{
			Selected = &Founder;
			break;
		}
	}
	if (Selected)
	{
		Y = DrawWrappedText(FString::Printf(TEXT("SELECTED / %s"),
			UTF8_TO_TCHAR(Selected->Name.c_str())), X, Y, Layout.ContentWidth,
			Primary, HeadingFont, HeadingScale,
			FMath::Min(BodyBottom, Y + 2.0f * (Layout.HeadingTextHeight + 3.0f * HeadingScale)));
		Y += 5.0f * Layout.Scale;
		Y = DrawWrappedText(FString::Printf(TEXT("Age %d  ·  %s  ·  food %d  ·  water %d"),
			Selected->AgeYears, UTF8_TO_TCHAR(Selected->Sex.c_str()), Selected->Food, Selected->Water),
			X, Y, Layout.ContentWidth, Primary, BodyFont, BodyScale, BodyBottom);
		Y = DrawWrappedText(FString::Printf(TEXT("Hunger %d  ·  Thirst %d  ·  Fatigue %d"),
			Selected->Needs.Hunger, Selected->Needs.Thirst, Selected->Needs.Fatigue),
			X, Y, Layout.ContentWidth, Primary, BodyFont, BodyScale, BodyBottom);
		Y = DrawWrappedText(FString::Printf(TEXT("Stamina %d  ·  Health %d"),
			Selected->Needs.Stamina, Selected->Needs.Health),
			X, Y, Layout.ContentWidth, Primary, BodyFont, BodyScale, BodyBottom);
		Y = DrawWrappedText(FString::Printf(TEXT("Action: %s%s"), ActionLabel(Selected->Action.Kind),
			Controller->IsFollowing() ? TEXT("  ·  FOLLOWING") : TEXT("")),
			X, Y, Layout.ContentWidth, Muted, BodyFont, BodyScale, BodyBottom);
		Y = DrawWrappedText(FString::Printf(TEXT("Prefers: %s"),
			UTF8_TO_TCHAR(Selected->Personality.PreferredActivity.c_str())),
			X, Y, Layout.ContentWidth, Muted, BodyFont, BodyScale, BodyBottom);
		Y += 12.0f * Layout.Scale;
	}
	else
	{
		Y = DrawWrappedText(TEXT("SELECTED / no founder"), X, Y, Layout.ContentWidth,
			Failure, HeadingFont, HeadingScale, BodyBottom);
	}

	Y = DrawWrappedText(TEXT("RECENT EVENTS / NEWEST FIRST"), X, Y, Layout.ContentWidth,
		Primary, BodyFont, BodyScale, BodyBottom);
	Y += 4.0f * Layout.Scale;
	const int32 First = FMath::Max(0, static_cast<int32>(State.History.size()) - 8);
	for (int32 Index = static_cast<int32>(State.History.size()) - 1; Index >= First && Y < BodyBottom; --Index)
	{
		const AIity::FEvent& Event = State.History[Index];
		Y = DrawWrappedText(FString::Printf(TEXT("· %s"), UTF8_TO_TCHAR(Event.Message.c_str())),
			X, Y, Layout.ContentWidth, Muted, BodyFont, BodyScale, BodyBottom);
	}

	const bool bFailure = Status.Contains(TEXT("fail"), ESearchCase::IgnoreCase) ||
		Status.Contains(TEXT("error"), ESearchCase::IgnoreCase);
	DrawRect(bFailure ? FLinearColor(0.24f, 0.045f, 0.025f, 0.96f)
		: FLinearColor(0.045f, 0.07f, 0.045f, 0.96f),
		Layout.Margin, StatusTop, Layout.PanelWidth, Layout.StatusHeight);
	float StatusY = StatusTop + Layout.Padding;
	StatusY = DrawWrappedText(TEXT("STATUS"), X, StatusY, Layout.ContentWidth,
		bFailure ? Failure : Primary, BodyFont, BodyScale, Canvas->ClipY - Layout.Margin);
	DrawWrappedText(Status, X, StatusY + 3.0f * Layout.Scale, Layout.ContentWidth,
		bFailure ? Failure : Muted, BodyFont, BodyScale, Canvas->ClipY - Layout.Margin);
}

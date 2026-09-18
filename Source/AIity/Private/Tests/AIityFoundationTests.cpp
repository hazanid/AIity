#if WITH_DEV_AUTOMATION_TESTS

#include "AIityFounderCharacter.h"
#include "AIityGameMode.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Misc/AutomationTest.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Persistence/AIityWorldStore.h"
#include "SQLitePreparedStatement.h"
#include "Simulation/AIityPersistencePolicy.h"
#include "Simulation/AIityRules.h"
#include "Simulation/AIitySerialization.h"
#include "Simulation/AIitySimulationClock.h"

#include <limits>

namespace
{
int32 TotalFood(const AIity::FWorldState& State)
{
	int32 Total = 0;
	for (const AIity::FFounder& Founder : State.Founders)
	{
		Total += Founder.Food;
	}
	for (const AIity::FResourceNode& Resource : State.Resources)
	{
		if (Resource.Kind == AIity::EResourceKind::Food)
		{
			Total += Resource.Amount;
		}
	}
	return Total;
}

int32 ResourceAmount(const AIity::FWorldState& State, uint64 ResourceId)
{
	for (const AIity::FResourceNode& Resource : State.Resources)
	{
		if (Resource.Id == ResourceId)
		{
			return Resource.Amount;
		}
	}
	return -1;
}

bool DeleteTestWorldFamily(const FString& Path)
{
	TArray<FString> Matches;
	IFileManager::Get().FindFiles(Matches, *(Path + TEXT("*")), true, false);
	for (const FString& Match : Matches)
	{
		const FString FilePath = FPaths::Combine(FPaths::GetPath(Path), Match);
		if (!IFileManager::Get().Delete(*FilePath, false, true))
		{
			return false;
		}
	}
	return true;
}

bool Require(FAutomationTestBase& Test, const TCHAR* What, bool bSucceeded, const FString& Error)
{
	if (Test.TestTrue(What, bSucceeded))
	{
		return true;
	}
	if (!Error.IsEmpty())
	{
		Test.AddError(FString::Printf(TEXT("%s: %s"), What, *Error));
	}
	return false;
}

bool ExecuteRaw(FSQLiteDatabase& Database, const TCHAR* Sql, FString& Error)
{
	Error.Reset();
	if (Database.Execute(Sql))
	{
		return true;
	}
	Error = Database.GetLastError();
	return false;
}

struct FFamilySnapshot
{
	bool bPrimaryExists = false;
	bool bWalExists = false;
	bool bShmExists = false;
	bool bJournalExists = false;
	TArray<uint8> Primary;
	TArray<uint8> Wal;
	TArray<uint8> Shm;
	TArray<uint8> Journal;
};

bool SnapshotFamily(const FString& Path, FFamilySnapshot& Snapshot)
{
	IPlatformFile& Files = FPlatformFileManager::Get().GetPlatformFile();
	Snapshot.bPrimaryExists = Files.FileExists(*Path);
	Snapshot.bWalExists = Files.FileExists(*(Path + TEXT("-wal")));
	Snapshot.bShmExists = Files.FileExists(*(Path + TEXT("-shm")));
	Snapshot.bJournalExists = Files.FileExists(*(Path + TEXT("-journal")));
	return (!Snapshot.bPrimaryExists || FFileHelper::LoadFileToArray(Snapshot.Primary, *Path)) &&
		(!Snapshot.bWalExists || FFileHelper::LoadFileToArray(Snapshot.Wal, *(Path + TEXT("-wal")))) &&
		(!Snapshot.bShmExists || FFileHelper::LoadFileToArray(Snapshot.Shm, *(Path + TEXT("-shm")))) &&
		(!Snapshot.bJournalExists ||
			FFileHelper::LoadFileToArray(Snapshot.Journal, *(Path + TEXT("-journal"))));
}

UWorld* CreatePlacementTestWorld()
{
	const FName WorldName(*FString::Printf(
		TEXT("AIityPlacementTest_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	UWorld* World = UWorld::CreateWorld(
		EWorldType::GamePreview, false, WorldName, GetTransientPackage());
	if (!World)
	{
		return nullptr;
	}
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::GamePreview);
	World->AddToRoot();
	WorldContext.SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	return World;
}

void DestroyPlacementTestWorld(UWorld* World)
{
	if (!World)
	{
		return;
	}
	if (World->AreActorsInitialized())
	{
		for (AActor* Actor : FActorRange(World))
		{
			if (Actor)
			{
				Actor->RouteEndPlay(EEndPlayReason::LevelTransition);
			}
		}
	}
	GEngine->ShutdownWorldNetDriver(World);
	World->DestroyWorld(true);
	World->SetPhysicsScene(nullptr);
	GEngine->DestroyWorldContext(World);
	World->RemoveFromRoot();
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAIityRulesFoundationTest, "AIity.Rules.FoundationConservation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAIityRulesFoundationTest::RunTest(const FString& Parameters)
{
	const AIity::FWorldState Initial = AIity::FRules::CreateFoundationWorld(0xA117ULL);
	const AIity::FCandidateTick First = AIity::FRules::BuildCandidate(Initial, {});
	TestEqual(TEXT("Exactly ten founders exist"), static_cast<int32>(First.State.Founders.size()), 10);
	int32 Women = 0;
	int32 Men = 0;
	for (const AIity::FFounder& Item : First.State.Founders)
	{
		Women += Item.Sex == "woman" ? 1 : 0;
		Men += Item.Sex == "man" ? 1 : 0;
	}
	TestEqual(TEXT("Five founders are women"), Women, 5);
	TestEqual(TEXT("Five founders are men"), Men, 5);

	AIity::FWorldState ExactResource = Initial;
	for (AIity::FResourceNode& Resource : ExactResource.Resources)
	{
		Resource.X = ExactResource.Founders[0].X;
		Resource.Y = ExactResource.Founders[0].Y;
	}
	const AIity::FCandidateTick ExactStarted = AIity::FRules::BuildCandidate(ExactResource, {});
	const AIity::FFounder& ExactFounder = ExactStarted.State.Founders[0];
	const AIity::FAction ExactAction = ExactFounder.Action;
	TestTrue(TEXT("Exact durable resource XY still awaits engine arrival"),
		ExactAction.AwaitingMovement);
	const int32 ExactAmountBefore = ResourceAmount(ExactStarted.State, ExactAction.TargetId);
	const int32 ExactFoodBefore = ExactFounder.Food;
	const int32 ExactWaterBefore = ExactFounder.Water;
	const AIity::FCandidateTick DisplacedWithoutReceipt =
		AIity::FRules::BuildCandidate(ExactStarted.State, {});
	TestEqual(TEXT("Presentation displacement without arrival grants no goods"),
		ResourceAmount(DisplacedWithoutReceipt.State, ExactAction.TargetId), ExactAmountBefore);
	TestEqual(TEXT("No arrival preserves carried food"),
		DisplacedWithoutReceipt.State.Founders[0].Food, ExactFoodBefore);
	TestEqual(TEXT("No arrival preserves carried water"),
		DisplacedWithoutReceipt.State.Founders[0].Water, ExactWaterBefore);
	const AIity::FMovementReceipt ExactArrival{ExactAction.Id, ExactFounder.Id,
		ExactAction.Id, ExactAction.Epoch, AIity::EMovementOutcome::Arrived,
		ExactAction.TargetX, ExactAction.TargetY};
	AIity::FMovementReceipt OutsideArrival = ExactArrival;
	OutsideArrival.X += 151;
	const AIity::FCandidateTick Outside =
		AIity::FRules::BuildCandidate(ExactStarted.State, {OutsideArrival});
	TestTrue(TEXT("Out-of-range arrival does not consume the receipt identity"),
		Outside.ConsumedReceiptIds.empty());
	TestTrue(TEXT("Out-of-range arrival keeps gathering awaiting movement"),
		Outside.State.Founders[0].Action.AwaitingMovement);
	TestEqual(TEXT("Out-of-range arrival consumes no resource"),
		ResourceAmount(Outside.State, ExactAction.TargetId), ExactAmountBefore);
	TestEqual(TEXT("Out-of-range arrival grants no food"),
		Outside.State.Founders[0].Food, ExactFoodBefore);
	TestEqual(TEXT("Out-of-range arrival grants no water"),
		Outside.State.Founders[0].Water, ExactWaterBefore);
	const AIity::FCandidateTick ExactCompleted =
		AIity::FRules::BuildCandidate(ExactStarted.State, {ExactArrival});
	TestEqual(TEXT("Valid actual arrival consumes one resource"),
		ResourceAmount(ExactCompleted.State, ExactAction.TargetId), ExactAmountBefore - 1);
	TestEqual(TEXT("Valid actual arrival grants one carried good"),
		ExactCompleted.State.Founders[0].Food + ExactCompleted.State.Founders[0].Water,
		ExactFoodBefore + ExactWaterBefore + 1);
	const AIity::FCandidateTick ExactDuplicate =
		AIity::FRules::BuildCandidate(ExactCompleted.State, {ExactArrival});
	TestEqual(TEXT("Duplicate arrival cannot grant a second effect"),
		ResourceAmount(ExactDuplicate.State, ExactAction.TargetId), ExactAmountBefore - 1);

	AIity::FWorldState LegacyGather = First.State;
	LegacyGather.Founders[0].Action.AwaitingMovement = false;
	LegacyGather.Founders[0].X = LegacyGather.Founders[0].Action.TargetX;
	LegacyGather.Founders[0].Y = LegacyGather.Founders[0].Action.TargetY;
	const uint64 LegacyEpoch = LegacyGather.Founders[0].Action.Epoch;
	const auto LegacyTransitionIsSafe =
		[&](const AIity::FCandidateTick& Transition)
		{
			const AIity::FFounder& TransitionFounder = Transition.State.Founders[0];
			const AIity::FAction TransitionAction = TransitionFounder.Action;
			const int32 AmountBefore =
				ResourceAmount(Transition.State, TransitionAction.TargetId);
			const int32 GoodsBefore = TransitionFounder.Food + TransitionFounder.Water;
			const AIity::FCandidateTick NoReceipt =
				AIity::FRules::BuildCandidate(Transition.State, {});
			AIity::FMovementReceipt Receipt{
				TransitionAction.Id, TransitionFounder.Id, TransitionAction.Id, LegacyEpoch,
				AIity::EMovementOutcome::Arrived,
				TransitionAction.TargetX, TransitionAction.TargetY};
			const AIity::FCandidateTick Stale =
				AIity::FRules::BuildCandidate(Transition.State, {Receipt});
			Receipt.Epoch = TransitionAction.Epoch;
			const AIity::FCandidateTick Fresh =
				AIity::FRules::BuildCandidate(Transition.State, {Receipt});
			const AIity::FCandidateTick Duplicate =
				AIity::FRules::BuildCandidate(Fresh.State, {Receipt});
			return Transition.State.Tick == LegacyGather.Tick + 1 &&
				Transition.State.LogicalSeconds == LegacyGather.LogicalSeconds &&
				TransitionAction.Id == LegacyGather.Founders[0].Action.Id &&
				TransitionAction.AwaitingMovement &&
				TransitionAction.Epoch == LegacyGather.ExecutionEpoch + 1 &&
				ResourceAmount(NoReceipt.State, TransitionAction.TargetId) == AmountBefore &&
				NoReceipt.State.Founders[0].Food + NoReceipt.State.Founders[0].Water == GoodsBefore &&
				ResourceAmount(Stale.State, TransitionAction.TargetId) == AmountBefore &&
				Stale.State.Founders[0].Food + Stale.State.Founders[0].Water == GoodsBefore &&
				ResourceAmount(Fresh.State, TransitionAction.TargetId) == AmountBefore - 1 &&
				Fresh.State.Founders[0].Food + Fresh.State.Founders[0].Water == GoodsBefore + 1 &&
				ResourceAmount(Duplicate.State, TransitionAction.TargetId) == AmountBefore - 1;
		};
	TestTrue(TEXT("Resume safely reissues an old non-awaiting gather"),
		LegacyTransitionIsSafe(AIity::FRules::BuildResumeCandidate(LegacyGather)));
	TestTrue(TEXT("Retry safely reissues an old non-awaiting gather"),
		LegacyTransitionIsSafe(AIity::FRules::BuildRetryCandidate(LegacyGather)));

	const uint64 UnsignedMaximum = std::numeric_limits<uint64>::max();
	std::string RangeError;
	AIity::FWorldState FullRangeText = Initial;
	FullRangeText.Seed = UnsignedMaximum;
	FullRangeText.RandomState = UnsignedMaximum;
	TestTrue(TEXT("Seed and PRNG retain full uint64 text range"),
		AIity::FRules::Validate(FullRangeText, RangeError));
	AIity::FWorldState InvalidSignedState = Initial;
	InvalidSignedState.Tick = UnsignedMaximum;
	TestFalse(TEXT("Tick outside SQLite signed range is rejected"),
		AIity::FRules::Validate(InvalidSignedState, RangeError));
	InvalidSignedState = Initial;
	InvalidSignedState.LogicalSeconds = UnsignedMaximum;
	TestFalse(TEXT("LogicalSeconds outside SQLite signed range is rejected"),
		AIity::FRules::Validate(InvalidSignedState, RangeError));
	InvalidSignedState = Initial;
	InvalidSignedState.History.push_back(
		AIity::FEvent{UnsignedMaximum, 0, 0, "invalid", "out of signed range"});
	TestFalse(TEXT("Loaded Event outside SQLite signed range is rejected"),
		AIity::FRules::Validate(InvalidSignedState, RangeError));
	AIity::FCandidateTick InvalidEnvelope = First;
	InvalidEnvelope.Events[0].AgentId = UnsignedMaximum;
	TestFalse(TEXT("Candidate Event outside SQLite signed range is rejected"),
		AIity::FRules::ValidatePersistenceEnvelope(InvalidEnvelope, RangeError));
	InvalidEnvelope = First;
	InvalidEnvelope.ConsumedReceiptIds.push_back(UnsignedMaximum);
	TestFalse(TEXT("Candidate receipt outside SQLite signed range is rejected"),
		AIity::FRules::ValidatePersistenceEnvelope(InvalidEnvelope, RangeError));

	const int32 FoodBefore = TotalFood(First.State);
	const AIity::FFounder& Founder = First.State.Founders[0];
	const AIity::FMovementReceipt Arrival{Founder.Action.Id, Founder.Id, Founder.Action.Id, Founder.Action.Epoch,
		AIity::EMovementOutcome::Arrived, Founder.Action.TargetX, Founder.Action.TargetY};
	const AIity::FCandidateTick Completed = AIity::FRules::BuildCandidate(First.State, {Arrival});
	TestEqual(TEXT("Gather transfers finite food without creating it"), TotalFood(Completed.State), FoodBefore);

	AIity::FWorldState Empty = First.State;
	for (AIity::FResourceNode& Resource : Empty.Resources)
	{
		Resource.Amount = 0;
	}
	const AIity::FCandidateTick Unavailable = AIity::FRules::BuildCandidate(Empty, {});
	for (const AIity::FResourceNode& Resource : Unavailable.State.Resources)
	{
		TestTrue(TEXT("Resource never becomes negative"), Resource.Amount >= 0);
	}
	AIity::FWorldState Starving = Initial;
	Starving.Founders[0].Needs.Hunger = 1000;
	Starving.Founders[0].Food = 0;
	const int32 HealthBefore = Starving.Founders[0].Needs.Health;
	const AIity::FCandidateTick Starved = AIity::FRules::BuildCandidate(Starving, {});
	TestTrue(TEXT("Starvation has a bounded health cost"), Starved.State.Founders[0].Needs.Health < HealthBefore);
	const FString FirstSerialized = UTF8_TO_TCHAR(AIity::SerializeWorld(First.State).c_str());
	const FString RepeatedSerialized = UTF8_TO_TCHAR(
		AIity::SerializeWorld(AIity::FRules::BuildCandidate(Initial, {}).State).c_str());
	TestEqual(TEXT("Seeded candidate serialization is exactly repeatable"),
		FirstSerialized, RepeatedSerialized);

	AIity::FWorldState Depleted = Initial;
	for (AIity::FResourceNode& Resource : Depleted.Resources)
	{
		Resource.Amount = 0;
	}
	for (int32 Tick = 0; Tick < 1000; ++Tick)
	{
		Depleted = AIity::FRules::BuildCandidate(Depleted, {}).State;
	}
	TestTrue(TEXT("Display Event buffer stays bounded during sustained depletion"), Depleted.History.size() <= 64);
	TestTrue(TEXT("Bounded WorldState serialization does not grow with full history"),
		AIity::SerializeWorld(Depleted).size() < 32000);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAIityFounderPlacementTest,
	"AIity.Bridge.FounderPlacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAIityFounderPlacementTest::RunTest(const FString& Parameters)
{
	UWorld* World = CreatePlacementTestWorld();
	if (!TestNotNull(TEXT("Isolated transient placement world exists"), World))
	{
		return false;
	}
	TestFalse(TEXT("Placement fixture never begins play or opens a production save"),
		World->HasBegunPlay());

	AIity::FWorldState Separated = AIity::FRules::CreateFoundationWorld(0xA117ULL);
	const std::string SeparatedBytes = AIity::SerializeWorld(Separated);
	TestTrue(TEXT("Separated founder group places at durable XY"),
		AAIityGameMode::ReconcileFounderActors(*World, Separated));
	int32 SeparatedCount = 0;
	TMap<uint64, AAIityFounderCharacter*> SeparatedActors;
	TMap<uint64, FVector> SeparatedLocations;
	for (TActorIterator<AAIityFounderCharacter> It(World); It; ++It)
	{
		++SeparatedCount;
		SeparatedActors.Add(It->GetFounderId(), *It);
		SeparatedLocations.Add(It->GetFounderId(), It->GetActorLocation());
		const AIity::FFounder* Expected = nullptr;
		for (const AIity::FFounder& Founder : Separated.Founders)
		{
			if (Founder.Id == It->GetFounderId())
			{
				Expected = &Founder;
				break;
			}
		}
		TestNotNull(TEXT("Placed founder keeps a stable ID"), Expected);
		if (Expected)
		{
			TestEqual(TEXT("Separated founder keeps durable X"),
				FMath::RoundToInt(It->GetActorLocation().X), static_cast<int64>(Expected->X));
			TestEqual(TEXT("Separated founder keeps durable Y"),
				FMath::RoundToInt(It->GetActorLocation().Y), static_cast<int64>(Expected->Y));
		}
	}
	TestEqual(TEXT("All separated founders exist"), SeparatedCount, 10);
	TestTrue(TEXT("Separated placement does not mutate WorldState"),
		AIity::SerializeWorld(Separated) == SeparatedBytes);

	AIity::FWorldState MinimumCoordinates = Separated;
	MinimumCoordinates.Founders[0].X = std::numeric_limits<int32>::lowest();
	const std::string MinimumCoordinateBytes =
		AIity::SerializeWorld(MinimumCoordinates);
	TestFalse(TEXT("INT32_MIN engine coordinate fails preflight"),
		AAIityGameMode::ReconcileFounderActors(*World, MinimumCoordinates));
	AIity::FWorldState MaximumCoordinates = Separated;
	MaximumCoordinates.Founders[0].Y = std::numeric_limits<int32>::max();
	const std::string MaximumCoordinateBytes =
		AIity::SerializeWorld(MaximumCoordinates);
	TestFalse(TEXT("INT32_MAX engine coordinate fails preflight"),
		AAIityGameMode::ReconcileFounderActors(*World, MaximumCoordinates));
	int32 PreservedActorCount = 0;
	for (TActorIterator<AAIityFounderCharacter> It(World); It; ++It)
	{
		++PreservedActorCount;
		TestTrue(TEXT("Coordinate rejection keeps existing actor identity"),
			SeparatedActors.FindRef(It->GetFounderId()) == *It);
		TestTrue(TEXT("Coordinate rejection keeps existing actor location"),
			SeparatedLocations.FindRef(It->GetFounderId()).Equals(
				It->GetActorLocation(), 0.0));
		TestTrue(TEXT("Coordinate rejection keeps existing actor collision"),
			It->GetActorEnableCollision());
	}
	TestEqual(TEXT("Coordinate rejection keeps the complete existing group"),
		PreservedActorCount, 10);
	TestTrue(TEXT("Minimum coordinate rejection does not mutate serialized WorldState"),
		AIity::SerializeWorld(MinimumCoordinates) == MinimumCoordinateBytes);
	TestTrue(TEXT("Maximum coordinate rejection does not mutate serialized WorldState"),
		AIity::SerializeWorld(MaximumCoordinates) == MaximumCoordinateBytes);

	AIity::FWorldState Clustered = Separated;
	for (AIity::FFounder& Founder : Clustered.Founders)
	{
		Founder.X = 0;
		Founder.Y = 0;
	}
	for (AIity::FResourceNode& Resource : Clustered.Resources)
	{
		Resource.X = 0;
		Resource.Y = 0;
	}
	const std::string ClusteredBytes = AIity::SerializeWorld(Clustered);
	TestTrue(TEXT("Co-located durable founders receive bounded presentation offsets"),
		AAIityGameMode::ReconcileFounderActors(*World, Clustered));
	TArray<AAIityFounderCharacter*> ClusteredActors;
	TSet<uint64> ClusteredIds;
	bool bHasSavedLocation = false;
	uint64 DisplacedFounderId = 0;
	for (TActorIterator<AAIityFounderCharacter> It(World); It; ++It)
	{
		ClusteredActors.Add(*It);
		ClusteredIds.Add(It->GetFounderId());
		bHasSavedLocation |= FMath::IsNearlyZero(It->GetActorLocation().X) &&
			FMath::IsNearlyZero(It->GetActorLocation().Y);
		if (!FMath::IsNearlyZero(It->GetActorLocation().X) ||
			!FMath::IsNearlyZero(It->GetActorLocation().Y))
		{
			DisplacedFounderId = It->GetFounderId();
		}
		UCapsuleComponent* Capsule = It->GetCapsuleComponent();
		TestNotNull(TEXT("Placed founder has a capsule"), Capsule);
		if (Capsule)
		{
			TestTrue(TEXT("Founder capsule keeps collision enabled"),
				Capsule->GetCollisionEnabled() != ECollisionEnabled::NoCollision);
			TestEqual(TEXT("Founder capsule blocks other Pawns"),
				Capsule->GetCollisionResponseToChannel(ECC_Pawn), ECR_Block);
			TestEqual(TEXT("Founder capsule blocks world geometry"),
				Capsule->GetCollisionResponseToChannel(ECC_WorldStatic), ECR_Block);
		}
	}
	TestEqual(TEXT("Clustered placement restores all ten founders"), ClusteredActors.Num(), 10);
	TestEqual(TEXT("Clustered placement preserves all ten stable IDs"), ClusteredIds.Num(), 10);
	TestTrue(TEXT("Saved XY is tried before nearby offsets"), bHasSavedLocation);
	for (int32 Left = 0; Left < ClusteredActors.Num(); ++Left)
	{
		for (int32 Right = Left + 1; Right < ClusteredActors.Num(); ++Right)
		{
			const float MinimumDistance =
				ClusteredActors[Left]->GetCapsuleComponent()->GetScaledCapsuleRadius() +
				ClusteredActors[Right]->GetCapsuleComponent()->GetScaledCapsuleRadius();
			TestTrue(TEXT("Placed blocking capsules do not overlap"),
				FVector::DistSquared2D(
					ClusteredActors[Left]->GetActorLocation(),
					ClusteredActors[Right]->GetActorLocation()) >=
					FMath::Square(MinimumDistance));
		}
	}
	TestTrue(TEXT("Clustered placement does not repair durable XY"),
		AIity::SerializeWorld(Clustered) == ClusteredBytes);
	TestTrue(TEXT("Clustered placement physically displaces at least one founder"),
		DisplacedFounderId != 0);
	const AIity::FCandidateTick DisplacedStarted =
		AIity::FRules::BuildCandidate(Clustered, {});
	const AIity::FFounder* DisplacedFounder = nullptr;
	for (const AIity::FFounder& Founder : DisplacedStarted.State.Founders)
	{
		if (Founder.Id == DisplacedFounderId)
		{
			DisplacedFounder = &Founder;
			break;
		}
	}
	TestNotNull(TEXT("Displaced stable ID remains in WorldState"), DisplacedFounder);
	if (DisplacedFounder)
	{
		const AIity::FAction DisplacedAction = DisplacedFounder->Action;
		const int32 AmountBefore =
			ResourceAmount(DisplacedStarted.State, DisplacedAction.TargetId);
		const int32 GoodsBefore = DisplacedFounder->Food + DisplacedFounder->Water;
		const AIity::FCandidateTick NoReceipt =
			AIity::FRules::BuildCandidate(DisplacedStarted.State, {});
		TestEqual(TEXT("Physically displaced founder gets no remote goods"),
			ResourceAmount(NoReceipt.State, DisplacedAction.TargetId), AmountBefore);
		const AIity::FMovementReceipt Arrival{
			DisplacedAction.Id, DisplacedFounder->Id, DisplacedAction.Id,
			DisplacedAction.Epoch, AIity::EMovementOutcome::Arrived,
			DisplacedAction.TargetX, DisplacedAction.TargetY};
		const AIity::FCandidateTick Arrived =
			AIity::FRules::BuildCandidate(DisplacedStarted.State, {Arrival});
		TestEqual(TEXT("Actual arrival gives displaced founder one resource effect"),
			ResourceAmount(Arrived.State, DisplacedAction.TargetId), AmountBefore - 1);
		bool bCheckedArrivedFounder = false;
		for (const AIity::FFounder& Founder : Arrived.State.Founders)
		{
			if (Founder.Id == DisplacedFounder->Id)
			{
				TestEqual(TEXT("Actual arrival gives displaced founder one carried good"),
					Founder.Food + Founder.Water, GoodsBefore + 1);
				bCheckedArrivedFounder = true;
				break;
			}
		}
		TestTrue(TEXT("Arrived founder remains addressable by stable ID"),
			bCheckedArrivedFounder);
	}

	for (AAIityFounderCharacter* Character : ClusteredActors)
	{
		Character->SetActorEnableCollision(false);
		Character->Destroy();
	}
	FActorSpawnParameters BlockerParameters;
	BlockerParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACharacter* Blocker = World->SpawnActor<ACharacter>(
		FVector(0.0f, 0.0f, 110.0f), FRotator::ZeroRotator, BlockerParameters);
	if (TestNotNull(TEXT("Exhaustion blocker exists"), Blocker))
	{
		Blocker->GetCapsuleComponent()->SetCapsuleSize(1000.0f, 500.0f, true);
		Blocker->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Blocker->GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Block);
		Blocker->GetCapsuleComponent()->UpdateOverlaps();
	}
	AIity::FWorldState Exhausted = Separated;
	for (int32 Index = 0; Index < 9; ++Index)
	{
		Exhausted.Founders[Index].X = 5000 + Index * 200;
		Exhausted.Founders[Index].Y = 0;
	}
	Exhausted.Founders[9].X = 0;
	Exhausted.Founders[9].Y = 0;
	const std::string ExhaustedBytes = AIity::SerializeWorld(Exhausted);
	TestFalse(TEXT("Bounded search fails when one founder has no candidate"),
		AAIityGameMode::ReconcileFounderActors(*World, Exhausted));
	int32 PartialFounderCount = 0;
	for (TActorIterator<AAIityFounderCharacter> It(World); It; ++It)
	{
		++PartialFounderCount;
	}
	TestEqual(TEXT("Failed placement cleans every actor created by that attempt"),
		PartialFounderCount, 0);
	TestTrue(TEXT("Failed placement does not destroy unrelated actors"),
		IsValid(Blocker));
	TestTrue(TEXT("Failed placement leaves serialized WorldState unchanged"),
		AIity::SerializeWorld(Exhausted) == ExhaustedBytes);

	DestroyPlacementTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAIityPersistenceSignedStorageRangeTest,
	"AIity.Persistence.SignedStorageRange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAIityPersistenceSignedStorageRangeTest::RunTest(const FString& Parameters)
{
	const FString Path = FPaths::Combine(
		FPaths::ProjectSavedDir(), TEXT("Tests/SignedStorageRange.sqlite"));
	if (!TestTrue(TEXT("Prior signed-range fixture is removed"), DeleteTestWorldFamily(Path)))
	{
		return false;
	}
	FString Error;
	FAIityWorldStore Store;
	if (!Require(*this, TEXT("Signed-range store opens"), Store.Open(Path, Error), Error))
	{
		return false;
	}
	AIity::FCandidateTick Initial;
	Initial.State = AIity::FRules::CreateFoundationWorld(913);
	if (!Require(*this, TEXT("Signed-range baseline commits"),
			Store.Commit(Initial, Error), Error))
	{
		return false;
	}

	const uint64 UnsignedMaximum = std::numeric_limits<uint64>::max();
	const AIity::FCandidateTick Valid =
		AIity::FRules::BuildCandidate(Initial.State, {});
	AIity::FCandidateTick Invalid = Valid;
	Invalid.State.Tick = UnsignedMaximum;
	TestFalse(TEXT("Store rejects unsigned Tick before transaction"),
		Store.Commit(Invalid, Error));
	Invalid = Valid;
	Invalid.State.LogicalSeconds = UnsignedMaximum;
	TestFalse(TEXT("Store rejects unsigned LogicalSeconds before transaction"),
		Store.Commit(Invalid, Error));
	Invalid = Valid;
	Invalid.Events[0].Id = UnsignedMaximum;
	TestFalse(TEXT("Store rejects unsigned Event ID before transaction"),
		Store.Commit(Invalid, Error));
	Invalid = Valid;
	Invalid.Events[0].Tick = UnsignedMaximum;
	TestFalse(TEXT("Store rejects unsigned Event Tick before transaction"),
		Store.Commit(Invalid, Error));
	Invalid = Valid;
	Invalid.Events[0].AgentId = UnsignedMaximum;
	TestFalse(TEXT("Store rejects unsigned Event agent before transaction"),
		Store.Commit(Invalid, Error));
	Invalid = Valid;
	Invalid.ConsumedReceiptIds.push_back(UnsignedMaximum);
	TestFalse(TEXT("Store rejects unsigned receipt ID before transaction"),
		Store.Commit(Invalid, Error));

	AIity::FWorldState Loaded;
	bool bFound = false;
	if (!Require(*this, TEXT("Baseline reloads after all range rejections"),
			Store.LoadLatest(Loaded, bFound, Error), Error))
	{
		return false;
	}
	TestTrue(TEXT("Baseline still exists after all range rejections"), bFound);
	TestTrue(TEXT("Range rejection preserves exact current WorldState"),
		AIity::SerializeWorld(Loaded) == AIity::SerializeWorld(Initial.State));
	Store.Close();

	FScopedSqliteDatabase RawSession;
	if (!Require(*this, TEXT("Range fixture opens read-only"),
			RawSession.OpenExclusive(*Path, ESQLiteDatabaseOpenMode::ReadOnly, Error), Error))
	{
		return false;
	}
	{
		FSQLitePreparedStatement Query = RawSession.Database.PrepareStatement(
			TEXT("SELECT "
				"(SELECT tick FROM current_state WHERE id=1),"
				"(SELECT COUNT(*) FROM world_events),"
				"(SELECT COUNT(*) FROM consumed_receipts),"
				"(SELECT COUNT(*) FROM world_checkpoints);"),
			ESQLitePreparedStatementFlags::Persistent);
		if (!TestTrue(TEXT("Range preservation query is valid"), Query.IsValid()) ||
			!TestEqual(TEXT("Range preservation query returns one row"),
				Query.Step(), ESQLitePreparedStatementStepResult::Row))
		{
			AddError(RawSession.Database.GetLastError());
			return false;
		}
		int64 Tick = -1;
		int64 EventCount = -1;
		int64 ReceiptCount = -1;
		int64 CheckpointCount = -1;
		TestTrue(TEXT("Range preservation values read"),
			Query.GetColumnValueByIndex(0, Tick) &&
			Query.GetColumnValueByIndex(1, EventCount) &&
			Query.GetColumnValueByIndex(2, ReceiptCount) &&
			Query.GetColumnValueByIndex(3, CheckpointCount));
		TestEqual(TEXT("Invalid candidates preserve current state row"), Tick, static_cast<int64>(0));
		TestEqual(TEXT("Invalid candidates add no Events"), EventCount, static_cast<int64>(0));
		TestEqual(TEXT("Invalid candidates consume no receipts"), ReceiptCount, static_cast<int64>(0));
		TestEqual(TEXT("Invalid candidates add no Checkpoints"), CheckpointCount, static_cast<int64>(1));
	}
	TestTrue(TEXT("Range fixture closes after query finalizes"),
		RawSession.Database.Close());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAIityPersistenceRecoveryTest, "AIity.Persistence.AtomicCommitAndReopen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAIityPersistenceRecoveryTest::RunTest(const FString& Parameters)
{
	const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Tests/FoundationRecovery.sqlite"));
	IPlatformFile& Files = FPlatformFileManager::Get().GetPlatformFile();
	if (!TestTrue(TEXT("Prior scoped recovery fixtures are removed"), DeleteTestWorldFamily(Path)))
	{
		return false;
	}

	FString Error;
	FAIityWorldStore Store;
	if (!Require(*this, TEXT("SQLite world opens"), Store.Open(Path, Error), Error))
	{
		return false;
	}
	AIity::FCandidateTick Initial;
	Initial.State = AIity::FRules::CreateFoundationWorld(77);
	if (!Require(*this, TEXT("Initial state commits"), Store.Commit(Initial, Error), Error))
	{
		return false;
	}
	const AIity::FCandidateTick Durable = AIity::FRules::BuildCandidate(Initial.State, {});
	Store.SetFailCommitForTesting(true);
	if (!TestFalse(TEXT("Injected failure rolls back a transaction after writing candidate state"),
		Store.Commit(Durable, Error)))
	{
		return false;
	}
	bool bFound = false;
	AIity::FWorldState StillInitial;
	if (!Require(*this, TEXT("Last committed state remains readable after rollback"),
		Store.LoadLatest(StillInitial, bFound, Error), Error) ||
		!TestTrue(TEXT("Rollback leaves a committed state"), bFound))
	{
		return false;
	}
	TestEqual(TEXT("Failed candidate was not published"), StillInitial.Tick, Initial.State.Tick);
	if (!Require(*this, TEXT("Candidate commits before publication"), Store.Commit(Durable, Error), Error))
	{
		return false;
	}
	const AIity::FCandidateTick Interrupted = AIity::FRules::BuildCandidate(Durable.State, {});
	if (!Require(*this, TEXT("A database-safe backup is created"), Store.CheckpointAndBackup(Error), Error) ||
		!Require(*this, TEXT("A second backup retains the prior validated copy"),
			Store.CheckpointAndBackup(Error), Error))
	{
		return false;
	}
	for (const FString& Published : {Path + TEXT(".backup"), Path + TEXT(".backup.previous")})
	{
		if (!TestFalse(TEXT("Published backup has no WAL dependency"),
				Files.FileExists(*(Published + TEXT("-wal")))) ||
			!TestFalse(TEXT("Published backup has no rollback-journal dependency"),
				Files.FileExists(*(Published + TEXT("-journal")))))
		{
			return false;
		}
	}

	TArray<uint8> BackupBefore;
	TArray<uint8> BackupAfter;
	TArray<uint8> PreviousBefore;
	TArray<uint8> PreviousAfter;
	if (!TestTrue(TEXT("Validated backup can be read"),
			FFileHelper::LoadFileToArray(BackupBefore, *(Path + TEXT(".backup")))) ||
		!TestTrue(TEXT("Prior validated backup can be read"),
			FFileHelper::LoadFileToArray(PreviousBefore, *(Path + TEXT(".backup.previous")))))
	{
		return false;
	}
	const FString BackupWalPath = Path + TEXT(".backup-wal");
	TArray<uint8> BackupWalBefore;
	TArray<uint8> BackupWalAfter;
	TArray<uint8> BackupAfterBlockedPublish;
	if (!TestTrue(TEXT("Existing backup WAL blocker is written"),
			FFileHelper::SaveStringToFile(TEXT("backup WAL blocker"), *BackupWalPath)) ||
		!TestTrue(TEXT("Existing backup WAL blocker is captured"),
			FFileHelper::LoadFileToArray(BackupWalBefore, *BackupWalPath)) ||
		!TestFalse(TEXT("Existing backup WAL blocks publication"),
			Store.CheckpointAndBackup(Error)) ||
		!TestTrue(TEXT("Blocked backup primary remains readable"),
			FFileHelper::LoadFileToArray(BackupAfterBlockedPublish, *(Path + TEXT(".backup")))) ||
		!TestTrue(TEXT("Blocked backup WAL remains readable"),
			FFileHelper::LoadFileToArray(BackupWalAfter, *BackupWalPath)) ||
		!TestTrue(TEXT("Blocked backup primary is unchanged"),
			BackupBefore == BackupAfterBlockedPublish) ||
		!TestTrue(TEXT("Blocked backup WAL is unchanged"), BackupWalBefore == BackupWalAfter) ||
		!TestTrue(TEXT("Owned backup WAL blocker is removed"), Files.DeleteFile(*BackupWalPath)))
	{
		return false;
	}
	const FString PreviousJournalPath = Path + TEXT(".backup.previous-journal");
	TArray<uint8> PreviousJournalBefore;
	TArray<uint8> PreviousJournalAfter;
	TArray<uint8> PreviousAfterBlockedPublish;
	if (!TestTrue(TEXT("Existing previous-backup journal blocker is written"),
			FFileHelper::SaveStringToFile(
				TEXT("previous backup journal blocker"), *PreviousJournalPath)) ||
		!TestTrue(TEXT("Existing previous-backup journal blocker is captured"),
			FFileHelper::LoadFileToArray(PreviousJournalBefore, *PreviousJournalPath)) ||
		!TestFalse(TEXT("Existing previous-backup journal blocks publication"),
			Store.CheckpointAndBackup(Error)) ||
		!TestTrue(TEXT("Blocked previous-backup primary remains readable"),
			FFileHelper::LoadFileToArray(
				PreviousAfterBlockedPublish, *(Path + TEXT(".backup.previous")))) ||
		!TestTrue(TEXT("Blocked previous-backup journal remains readable"),
			FFileHelper::LoadFileToArray(PreviousJournalAfter, *PreviousJournalPath)) ||
		!TestTrue(TEXT("Blocked previous-backup primary is unchanged"),
			PreviousBefore == PreviousAfterBlockedPublish) ||
		!TestTrue(TEXT("Blocked previous-backup journal is unchanged"),
			PreviousJournalBefore == PreviousJournalAfter) ||
		!TestTrue(TEXT("Owned previous-backup journal blocker is removed"),
			Files.DeleteFile(*PreviousJournalPath)))
	{
		return false;
	}
	TArray<FString> UnexpectedTemporaryBackups;
	IFileManager::Get().FindFiles(
		UnexpectedTemporaryBackups, *(Path + TEXT(".backup*.tmp*")), true, true);
	if (!TestEqual(TEXT("Destination preflight creates no temporary backup"),
		UnexpectedTemporaryBackups.Num(), 0))
	{
		return false;
	}
	Store.SetFailBackupPublishForTesting(true);
	if (!TestFalse(TEXT("Injected backup publication failure is reported"),
		Store.CheckpointAndBackup(Error)))
	{
		return false;
	}
	if (!TestTrue(TEXT("Prior backup remains readable"),
			FFileHelper::LoadFileToArray(BackupAfter, *(Path + TEXT(".backup")))) ||
		!TestTrue(TEXT("Previous backup remains readable"),
			FFileHelper::LoadFileToArray(PreviousAfter, *(Path + TEXT(".backup.previous")))))
	{
		return false;
	}
	TestTrue(TEXT("Prior validated backup is unchanged on failure"), BackupBefore == BackupAfter);
	TestTrue(TEXT("Previous validated backup is unchanged on failure"), PreviousBefore == PreviousAfter);
	Store.SetFailBackupAfterPreviousForTesting(true);
	if (!TestFalse(TEXT("Injected interruption after previous publication is reported"),
		Store.CheckpointAndBackup(Error)))
	{
		return false;
	}
	TArray<uint8> BackupAfterBoundaryFailure;
	if (!TestTrue(TEXT("Current backup remains recognized after boundary interruption"),
		FFileHelper::LoadFileToArray(BackupAfterBoundaryFailure, *(Path + TEXT(".backup")))))
	{
		return false;
	}
	TestTrue(TEXT("Current backup is unchanged after boundary interruption"),
		BackupBefore == BackupAfterBoundaryFailure);

	FAIityWorldStore SecondWriter;
	if (!TestFalse(TEXT("A second writer cannot open the same world"), SecondWriter.Open(Path, Error)))
	{
		return false;
	}

	Store.SetMinimumFreeBytesForTesting(MAX_uint64);
	if (!TestFalse(TEXT("Configured disk reserve pauses before another candidate write"),
		Store.Commit(Interrupted, Error)))
	{
		return false;
	}
	Store.SetMinimumFreeBytesForTesting(0);
	if (!Require(*this, TEXT("Retry probes the production writable transaction path"),
		Store.CheckWritable(Error), Error))
	{
		return false;
	}
	Store.Close();

	for (const TCHAR* Sidecar : {TEXT("-wal"), TEXT("-shm")})
	{
		const FString SidecarPath = Path + Sidecar;
		if (Files.FileExists(*SidecarPath) &&
			!TestTrue(TEXT("Closed clean fixture sidecar can be removed"), Files.DeleteFile(*SidecarPath)))
		{
			return false;
		}
		if (!TestFalse(TEXT("Clean reopen fixture has no SQLite sidecar"),
			Files.FileExists(*SidecarPath)))
		{
			return false;
		}
	}
	FPlatformProcess::Sleep(0.01f);
	FAIityWorldStore Reopened;
	bFound = false;
	AIity::FWorldState Loaded;
	if (!Require(*this, TEXT("Database reopens cleanly without WAL or SHM sidecars"),
			Reopened.Open(Path, Error), Error) ||
		!Require(*this, TEXT("Latest committed state loads"),
			Reopened.LoadLatest(Loaded, bFound, Error), Error) ||
		!TestTrue(TEXT("A state was found"), bFound))
	{
		return false;
	}
	if (!TestTrue(TEXT("Reopened state retains all founders"), Loaded.Founders.size() == 10))
	{
		return false;
	}
	TestEqual(TEXT("Uncommitted tick is absent"), Loaded.Tick, Durable.State.Tick);
	TestNotEqual(TEXT("Interrupted candidate was not published"), Loaded.Tick, Interrupted.State.Tick);
	TestEqual(TEXT("No offline wall time is applied"), Loaded.LogicalSeconds, Durable.State.LogicalSeconds);
	const AIity::FCandidateTick Resume = AIity::FRules::BuildResumeCandidate(Loaded);
	TestEqual(TEXT("Reopen advances execution epoch exactly once"),
		Resume.State.ExecutionEpoch, Loaded.ExecutionEpoch + 1);
	TestEqual(TEXT("Reopen does not advance simulation time"),
		Resume.State.LogicalSeconds, Loaded.LogicalSeconds);
	if (!Require(*this, TEXT("Reopen transition commits"), Reopened.Commit(Resume, Error), Error))
	{
		return false;
	}
	const AIity::FFounder& ResumedFounder = Resume.State.Founders[0];
	TestEqual(TEXT("Active movement is reissued under new epoch"),
		ResumedFounder.Action.Epoch, Resume.State.ExecutionEpoch);
	const AIity::FMovementReceipt FreshReceipt{ResumedFounder.Action.Id, ResumedFounder.Id,
		ResumedFounder.Action.Id, ResumedFounder.Action.Epoch, AIity::EMovementOutcome::Arrived,
		ResumedFounder.Action.TargetX, ResumedFounder.Action.TargetY};
	const AIity::FCandidateTick MovementCompleted = AIity::FRules::BuildCandidate(Resume.State, {FreshReceipt});
	if (!Require(*this, TEXT("Fresh resumed movement can commit"),
		Reopened.Commit(MovementCompleted, Error), Error))
	{
		return false;
	}
	Reopened.Close();

	TArray<uint8> BeforeUnsupportedOpen;
	TArray<uint8> AfterUnsupportedOpen;
	FScopedSqliteDatabase RawSession;
	FSQLiteDatabase& RawDatabase = RawSession.Database;
	if (!Require(*this, TEXT("Schema test database opens"),
			RawSession.OpenExclusive(*Path, ESQLiteDatabaseOpenMode::ReadWriteCreate, Error), Error) ||
		!Require(*this, TEXT("Schema test writes a newer version"),
			ExecuteRaw(RawDatabase,
				TEXT("UPDATE world_meta SET value=999 WHERE key='schema_version';"), Error), Error) ||
		!Require(*this, TEXT("Future schema fixture is checkpointed"),
			ExecuteRaw(RawDatabase, TEXT("PRAGMA wal_checkpoint(TRUNCATE);"), Error), Error) ||
		!TestTrue(TEXT("Future schema fixture closes"), RawDatabase.Close()))
	{
		return false;
	}
	TestTrue(TEXT("Future-schema bytes can be captured"), FFileHelper::LoadFileToArray(BeforeUnsupportedOpen, *Path));
	FAIityWorldStore Unsupported;
	if (!TestFalse(TEXT("A newer schema is rejected without migration"), Unsupported.Open(Path, Error)))
	{
		return false;
	}
	TestTrue(TEXT("Schema error is clear"), Error.Contains(TEXT("unsupported")));
	TestTrue(TEXT("Rejected future schema remains readable"), FFileHelper::LoadFileToArray(AfterUnsupportedOpen, *Path));
	TestTrue(TEXT("Compatibility check leaves future schema unchanged"), BeforeUnsupportedOpen == AfterUnsupportedOpen);

	const FString MissingSchemaPath = Path + TEXT(".missing-schema");
	Files.DeleteFile(*MissingSchemaPath);
	if (!Require(*this, TEXT("Missing-schema fixture opens"),
			RawSession.OpenExclusive(*MissingSchemaPath,
				ESQLiteDatabaseOpenMode::ReadWriteCreate, Error), Error) ||
		!Require(*this, TEXT("Missing-schema fixture has unrelated data"),
			ExecuteRaw(RawDatabase, TEXT("CREATE TABLE unrelated(value TEXT);"), Error), Error) ||
		!TestTrue(TEXT("Missing-schema fixture closes"), RawDatabase.Close()))
	{
		return false;
	}
	TArray<uint8> MissingSchemaBefore;
	TArray<uint8> MissingSchemaAfter;
	TestTrue(TEXT("Missing-schema fixture bytes can be captured"),
		FFileHelper::LoadFileToArray(MissingSchemaBefore, *MissingSchemaPath));
	FAIityWorldStore MissingSchema;
	if (!TestFalse(TEXT("Existing database without world schema is rejected"),
		MissingSchema.Open(MissingSchemaPath, Error)))
	{
		return false;
	}
	TestTrue(TEXT("Rejected missing-schema database remains readable"),
		FFileHelper::LoadFileToArray(MissingSchemaAfter, *MissingSchemaPath));
	TestTrue(TEXT("Missing-schema database is unchanged"), MissingSchemaBefore == MissingSchemaAfter);

	if (!Require(*this, TEXT("Schema test database reopens"),
			RawSession.OpenExclusive(*Path, ESQLiteDatabaseOpenMode::ReadWriteCreate, Error), Error) ||
		!Require(*this, TEXT("Schema test restores supported version"),
			ExecuteRaw(RawDatabase,
				TEXT("UPDATE world_meta SET value=2 WHERE key='schema_version';"), Error), Error) ||
		!Require(*this, TEXT("Recoverable corruption keeps known schema but breaks current WorldState"),
			ExecuteRaw(RawDatabase,
				TEXT("UPDATE current_state SET state_text='malformed';"), Error), Error) ||
		!Require(*this, TEXT("Schema restore is checkpointed"),
			ExecuteRaw(RawDatabase, TEXT("PRAGMA wal_checkpoint(TRUNCATE);"), Error), Error) ||
		!TestTrue(TEXT("Malformed-state fixture closes"), RawDatabase.Close()))
	{
		return false;
	}
	Files.DeleteFile(*(Path + TEXT("-wal")));
	Files.DeleteFile(*(Path + TEXT("-shm")));
	TestTrue(TEXT("Damaged WAL fixture is created"),
		FFileHelper::SaveStringToFile(TEXT(""), *(Path + TEXT("-wal"))));
	TestTrue(TEXT("Damaged SHM fixture is created"),
		FFileHelper::SaveStringToFile(TEXT(""), *(Path + TEXT("-shm"))));
	if (!Require(*this, TEXT("Newest backup opens for malformed-state fixture"),
			RawSession.OpenExclusive(*(Path + TEXT(".backup")),
				ESQLiteDatabaseOpenMode::ReadWrite, Error), Error) ||
		!Require(*this, TEXT("Newest backup receives mismatched Tick metadata"),
			ExecuteRaw(RawDatabase, TEXT("UPDATE current_state SET tick=tick+1;"), Error), Error) ||
		!TestTrue(TEXT("Mismatched backup fixture closes"), RawDatabase.Close()))
	{
		return false;
	}
	FAIityWorldStore InterruptedRecovery;
	InterruptedRecovery.SetFailRecoveryAfterMarkerForTesting(true);
	if (!TestFalse(TEXT("Injected recovery interruption leaves durable intent"),
		InterruptedRecovery.Open(Path, Error)))
	{
		return false;
	}
	TestTrue(TEXT("Recovery marker remains after interruption"),
		Files.FileExists(*(Path + TEXT(".recovery"))));
	if (!TestFalse(TEXT("Recovering primary has no WAL dependency"),
			Files.FileExists(*(Path + TEXT(".recovering-wal")))) ||
		!TestFalse(TEXT("Recovering primary has no rollback-journal dependency"),
			Files.FileExists(*(Path + TEXT(".recovering-journal")))))
	{
		return false;
	}
	FAIityWorldStore Recovered;
	if (!Require(*this, TEXT("Next open finishes recognized interrupted recovery"),
		Recovered.Open(Path, Error), Error))
	{
		return false;
	}
	bFound = false;
	if (!Require(*this, TEXT("Recovered WorldState loads"),
			Recovered.LoadLatest(Loaded, bFound, Error), Error) ||
		!TestTrue(TEXT("Recovered state exists"), bFound))
	{
		return false;
	}
	Recovered.Close();
	TArray<FString> DamagedFiles;
	IFileManager::Get().FindFiles(DamagedFiles, *(Path + TEXT(".damaged.*")), true, false);
	TestTrue(TEXT("Damaged original is preserved beside recovered world"), DamagedFiles.Num() > 0);
	TArray<FString> DamagedWalFiles;
	IFileManager::Get().FindFiles(DamagedWalFiles, *(Path + TEXT(".damaged.*-wal")), true, false);
	TestTrue(TEXT("Damaged WAL is preserved with primary"), DamagedWalFiles.Num() > 0);
	TArray<FString> DamagedShmFiles;
	IFileManager::Get().FindFiles(DamagedShmFiles, *(Path + TEXT(".damaged.*-shm")), true, false);
	TestTrue(TEXT("Damaged SHM is preserved with primary"), DamagedShmFiles.Num() > 0);

	const FString UnknownPath = Path + TEXT(".unknown");
	if (!TestTrue(TEXT("Unknown primary fixture is written"),
			FFileHelper::SaveStringToFile(TEXT("not a database"), *UnknownPath)) ||
		!TestTrue(TEXT("Unknown WAL fixture is written"),
			FFileHelper::SaveStringToFile(TEXT("unknown wal"), *(UnknownPath + TEXT("-wal")))) ||
		!TestTrue(TEXT("Unknown SHM fixture is written"),
			FFileHelper::SaveStringToFile(TEXT("unknown shm"), *(UnknownPath + TEXT("-shm")))) ||
		!TestTrue(TEXT("Unknown primary fixture has an older valid backup"),
			Files.CopyFile(*(UnknownPath + TEXT(".backup")), *(Path + TEXT(".backup.previous")))))
	{
		return false;
	}
	TArray<uint8> UnknownBefore;
	TArray<uint8> UnknownAfter;
	TArray<uint8> UnknownWalBefore;
	TArray<uint8> UnknownWalAfter;
	TArray<uint8> UnknownShmBefore;
	TArray<uint8> UnknownShmAfter;
	TArray<uint8> UnknownBackupBefore;
	TArray<uint8> UnknownBackupAfter;
	if (!TestTrue(TEXT("Unknown primary bytes can be captured"),
			FFileHelper::LoadFileToArray(UnknownBefore, *UnknownPath)) ||
		!TestTrue(TEXT("Unknown WAL bytes can be captured"),
			FFileHelper::LoadFileToArray(UnknownWalBefore, *(UnknownPath + TEXT("-wal")))) ||
		!TestTrue(TEXT("Unknown SHM bytes can be captured"),
			FFileHelper::LoadFileToArray(UnknownShmBefore, *(UnknownPath + TEXT("-shm")))) ||
		!TestTrue(TEXT("Unknown backup bytes can be captured"),
			FFileHelper::LoadFileToArray(UnknownBackupBefore, *(UnknownPath + TEXT(".backup")))))
	{
		return false;
	}
	FAIityWorldStore Unknown;
	if (!TestFalse(TEXT("Unreadable identity fails closed without backup replacement"),
		Unknown.Open(UnknownPath, Error)))
	{
		return false;
	}
	if (!TestTrue(TEXT("Unknown primary remains readable"),
			FFileHelper::LoadFileToArray(UnknownAfter, *UnknownPath)) ||
		!TestTrue(TEXT("Unknown WAL remains readable"),
			FFileHelper::LoadFileToArray(UnknownWalAfter, *(UnknownPath + TEXT("-wal")))) ||
		!TestTrue(TEXT("Unknown SHM remains readable"),
			FFileHelper::LoadFileToArray(UnknownShmAfter, *(UnknownPath + TEXT("-shm")))) ||
		!TestTrue(TEXT("Unknown backup remains readable"),
			FFileHelper::LoadFileToArray(UnknownBackupAfter, *(UnknownPath + TEXT(".backup")))))
	{
		return false;
	}
	TestTrue(TEXT("Unknown primary is unchanged"), UnknownBefore == UnknownAfter);
	TestTrue(TEXT("Failed inspection leaves unknown WAL unchanged"), UnknownWalBefore == UnknownWalAfter);
	TestTrue(TEXT("Failed inspection leaves unknown SHM unchanged"), UnknownShmBefore == UnknownShmAfter);
	TestTrue(TEXT("Failed inspection leaves unknown backup unchanged"),
		UnknownBackupBefore == UnknownBackupAfter);

	const FString MissingPrimaryPath = Path + TEXT(".missing-primary");
	Files.DeleteFile(*MissingPrimaryPath);
	TestTrue(TEXT("Missing primary fixture has save-family evidence"),
		Files.CopyFile(*(MissingPrimaryPath + TEXT(".backup")), *(Path + TEXT(".backup.previous"))));
	FAIityWorldStore MissingPrimary;
	if (!TestFalse(TEXT("Missing primary with save-family evidence never initializes"),
		MissingPrimary.Open(MissingPrimaryPath, Error)))
	{
		return false;
	}
	TestFalse(TEXT("Missing primary remains absent"), Files.FileExists(*MissingPrimaryPath));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAIityPersistenceWalFamilyPreservationTest,
	"AIity.Persistence.ReadOnlyWalFamilyPreservation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAIityPersistenceWalFamilyPreservationTest::RunTest(const FString& Parameters)
{
	const FString FixtureRoot = FPaths::Combine(FPaths::ProjectSavedDir(),
		TEXT("Tests/WalFamily"), FGuid::NewGuid().ToString(EGuidFormats::Digits));
	IPlatformFile& Files = FPlatformFileManager::Get().GetPlatformFile();
	if (!TestFalse(TEXT("Fresh WAL fixture does not already exist"),
			Files.DirectoryExists(*FixtureRoot)) ||
		!TestTrue(TEXT("Fresh WAL fixture directory is created"),
			Files.CreateDirectoryTree(*FixtureRoot)))
	{
		return false;
	}

	const FString SeedPath = FPaths::Combine(FixtureRoot, TEXT("Seed.sqlite"));
	const FString ProbePath = FPaths::Combine(FixtureRoot, TEXT("Probe.sqlite"));
	FString Error;
	FScopedSqliteDatabase SeedSession;
	FSQLiteDatabase& Seed = SeedSession.Database;
	if (!Require(*this, TEXT("Future-schema seed opens"),
			SeedSession.OpenExclusive(*SeedPath, ESQLiteDatabaseOpenMode::ReadWriteCreate, Error), Error) ||
		!Require(*this, TEXT("Schema 2 identity is created before WAL"),
			ExecuteRaw(Seed,
				TEXT("CREATE TABLE world_meta (key TEXT PRIMARY KEY, value INTEGER NOT NULL);"),
				Error), Error) ||
		!Require(*this, TEXT("Schema 2 identity is committed before WAL"),
			ExecuteRaw(Seed,
				TEXT("INSERT INTO world_meta(key,value) VALUES('schema_version',2);"), Error),
			Error) ||
		!Require(*this, TEXT("Future-schema seed enables WAL"),
			ExecuteRaw(Seed, TEXT("PRAGMA journal_mode=WAL;"), Error), Error) ||
		!Require(*this, TEXT("Future-schema seed disables automatic checkpointing"),
			ExecuteRaw(Seed, TEXT("PRAGMA wal_autocheckpoint=0;"), Error), Error) ||
		!Require(*this, TEXT("Future-schema WAL transaction begins"),
			ExecuteRaw(Seed, TEXT("BEGIN IMMEDIATE TRANSACTION;"), Error), Error) ||
		!Require(*this, TEXT("Schema 999 is written into WAL"),
			ExecuteRaw(Seed,
				TEXT("UPDATE world_meta SET value=999 WHERE key='schema_version';"), Error),
			Error) ||
		!Require(*this, TEXT("Schema 999 WAL transaction commits"),
			ExecuteRaw(Seed, TEXT("COMMIT;"), Error), Error))
	{
		return false;
	}

	const int64 WalSize = Files.FileSize(*(SeedPath + TEXT("-wal")));
	if (!TestTrue(TEXT("Committed future-schema WAL is nonempty"), WalSize > 0) ||
		!TestTrue(TEXT("Quiescent probe primary is copied"),
			Files.CopyFile(*ProbePath, *SeedPath)))
	{
		return false;
	}
	for (const TCHAR* Sidecar : {TEXT("-wal"), TEXT("-shm")})
	{
		const FString Source = SeedPath + Sidecar;
		if (Files.FileExists(*Source) &&
			!TestTrue(TEXT("Every present quiescent sidecar is copied"),
				Files.CopyFile(*(ProbePath + Sidecar), *Source)))
		{
			return false;
		}
	}
	if (!TestTrue(TEXT("Seed closes after the complete idle family is copied"), Seed.Close()))
	{
		return false;
	}

	FFamilySnapshot ProbeBefore;
	if (!TestTrue(TEXT("Future-schema probe family is captured"),
		SnapshotFamily(ProbePath, ProbeBefore)) ||
		!TestTrue(TEXT("Future-schema probe retains a nonempty WAL"),
			ProbeBefore.bWalExists && ProbeBefore.Wal.Num() > 0))
	{
		return false;
	}
	FAIityWorldStore ProbeStore;
	const bool bProbeOpened = ProbeStore.Open(ProbePath, Error);
	const bool bRejected = TestFalse(TEXT("Uncheckpointed future schema is rejected"), bProbeOpened);
	ProbeStore.Close();
	const bool bRejectedAs999 = TestTrue(TEXT("Uncheckpointed WAL is read as unsupported schema 999"),
		!bProbeOpened && Error.Contains(TEXT("schema 999")) && Error.Contains(TEXT("unsupported")));
	FFamilySnapshot ProbeAfter;
	const bool bProbeReadable = TestTrue(TEXT("Future-schema probe family remains readable"),
		SnapshotFamily(ProbePath, ProbeAfter));
	const bool bPrimaryPresence = TestTrue(TEXT("Future-schema primary presence is unchanged"),
		ProbeBefore.bPrimaryExists == ProbeAfter.bPrimaryExists);
	const bool bWalPresence = TestTrue(TEXT("Future-schema WAL presence is unchanged"),
		ProbeBefore.bWalExists == ProbeAfter.bWalExists);
	const bool bShmPresence = TestTrue(TEXT("Future-schema SHM presence is unchanged"),
		ProbeBefore.bShmExists == ProbeAfter.bShmExists);
	const bool bJournalPresence = TestTrue(TEXT("Future-schema journal presence is unchanged"),
		ProbeBefore.bJournalExists == ProbeAfter.bJournalExists);
	const bool bPrimaryBytes = TestTrue(TEXT("Future-schema primary bytes are unchanged"),
		ProbeBefore.Primary == ProbeAfter.Primary);
	const bool bWalBytes = TestTrue(TEXT("Future-schema WAL bytes are unchanged"),
		ProbeBefore.Wal == ProbeAfter.Wal);
	const bool bShmBytes = TestTrue(TEXT("Future-schema SHM bytes are unchanged"),
		ProbeBefore.Shm == ProbeAfter.Shm);
	const bool bJournalBytes = TestTrue(TEXT("Future-schema journal bytes are unchanged"),
		ProbeBefore.Journal == ProbeAfter.Journal);
	if (!bRejected || !bRejectedAs999 || !bProbeReadable || !bPrimaryPresence ||
		!bWalPresence || !bShmPresence || !bJournalPresence || !bPrimaryBytes ||
		!bWalBytes || !bShmBytes || !bJournalBytes)
	{
		return false;
	}

	const FString ZeroPath = FPaths::Combine(FixtureRoot, TEXT("Zero.sqlite"));
	{
		TUniquePtr<IFileHandle> ZeroPrimary(Files.OpenWrite(*ZeroPath));
		if (!TestTrue(TEXT("Zero-page primary fixture is written"), ZeroPrimary.IsValid()))
		{
			return false;
		}
	}
	if (!TestTrue(TEXT("Zero-page WAL fixture is written"),
			FFileHelper::SaveStringToFile(TEXT("zero-page WAL sentinel"),
				*(ZeroPath + TEXT("-wal")))))
	{
		return false;
	}
	FFamilySnapshot ZeroBefore;
	if (!TestTrue(TEXT("Zero-page family is captured"), SnapshotFamily(ZeroPath, ZeroBefore)) ||
		!TestTrue(TEXT("Zero-page fixture has primary and nonempty WAL"),
			ZeroBefore.bPrimaryExists && ZeroBefore.Primary.IsEmpty() &&
			ZeroBefore.bWalExists && ZeroBefore.Wal.Num() > 0))
	{
		return false;
	}
	FAIityWorldStore ZeroStore;
	const bool bZeroOpened = ZeroStore.Open(ZeroPath, Error);
	const bool bZeroRejected = TestFalse(TEXT("Zero-page primary with WAL fails closed"), bZeroOpened);
	ZeroStore.Close();
	FFamilySnapshot ZeroAfter;
	const bool bZeroReadable = TestTrue(TEXT("Zero-page family remains readable"),
		SnapshotFamily(ZeroPath, ZeroAfter));
	const bool bZeroPrimaryPresence = TestTrue(TEXT("Zero-page primary presence is unchanged"),
		ZeroBefore.bPrimaryExists == ZeroAfter.bPrimaryExists);
	const bool bZeroWalPresence = TestTrue(TEXT("Zero-page WAL presence is unchanged"),
		ZeroBefore.bWalExists == ZeroAfter.bWalExists);
	const bool bZeroShmPresence = TestTrue(TEXT("Zero-page SHM presence is unchanged"),
		ZeroBefore.bShmExists == ZeroAfter.bShmExists);
	const bool bZeroJournalPresence = TestTrue(TEXT("Zero-page journal presence is unchanged"),
		ZeroBefore.bJournalExists == ZeroAfter.bJournalExists);
	const bool bZeroPrimaryBytes = TestTrue(TEXT("Zero-page primary bytes are unchanged"),
		ZeroBefore.Primary == ZeroAfter.Primary);
	const bool bZeroWalBytes = TestTrue(TEXT("Zero-page WAL bytes are unchanged"),
		ZeroBefore.Wal == ZeroAfter.Wal);
	const bool bZeroShmBytes = TestTrue(TEXT("Zero-page SHM bytes are unchanged"),
		ZeroBefore.Shm == ZeroAfter.Shm);
	const bool bZeroJournalBytes = TestTrue(TEXT("Zero-page journal bytes are unchanged"),
		ZeroBefore.Journal == ZeroAfter.Journal);
	if (!bZeroRejected || !bZeroReadable || !bZeroPrimaryPresence || !bZeroWalPresence ||
		!bZeroShmPresence || !bZeroJournalPresence || !bZeroPrimaryBytes ||
		!bZeroWalBytes || !bZeroShmBytes || !bZeroJournalBytes)
	{
		return false;
	}

	const FString JournalPath = FPaths::Combine(FixtureRoot, TEXT("Journal.sqlite"));
	if (!TestTrue(TEXT("Rollback-journal primary fixture is copied"),
			Files.CopyFile(*JournalPath, *SeedPath)) ||
		!TestTrue(TEXT("Rollback-journal fixture is written"),
			FFileHelper::SaveStringToFile(TEXT("untrusted rollback journal"),
				*(JournalPath + TEXT("-journal")))))
	{
		return false;
	}
	FFamilySnapshot JournalBefore;
	if (!TestTrue(TEXT("Rollback-journal family is captured"),
		SnapshotFamily(JournalPath, JournalBefore)))
	{
		return false;
	}
	FAIityWorldStore JournalStore;
	const bool bJournalOpened = JournalStore.Open(JournalPath, Error);
	const bool bJournalRejected = TestFalse(
		TEXT("Existing primary with rollback journal is rejected"), bJournalOpened);
	JournalStore.Close();
	const bool bJournalError = TestTrue(TEXT("Rollback-journal rejection is explicit"),
		!bJournalOpened && Error.Contains(TEXT("rollback journal")));
	FFamilySnapshot JournalAfter;
	const bool bJournalReadable = TestTrue(TEXT("Rollback-journal family remains readable"),
		SnapshotFamily(JournalPath, JournalAfter));
	const bool bJournalPrimaryBytes = TestTrue(TEXT("Rollback-journal primary is unchanged"),
		JournalBefore.Primary == JournalAfter.Primary);
	const bool bRollbackJournalBytes = TestTrue(TEXT("Rollback journal is unchanged"),
		JournalBefore.Journal == JournalAfter.Journal);
	const bool bRollbackJournalPresence = TestTrue(TEXT("Rollback journal remains present"),
		JournalBefore.bJournalExists == JournalAfter.bJournalExists);
	if (!bJournalRejected || !bJournalError || !bJournalReadable ||
		!bJournalPrimaryBytes || !bRollbackJournalBytes || !bRollbackJournalPresence)
	{
		return false;
	}

	const FString MissingJournalPath = FPaths::Combine(
		FixtureRoot, TEXT("MissingJournal.sqlite"));
	if (!TestTrue(TEXT("Missing-primary rollback journal is written"),
		FFileHelper::SaveStringToFile(TEXT("orphan rollback journal"),
			*(MissingJournalPath + TEXT("-journal")))))
	{
		return false;
	}
	FFamilySnapshot MissingJournalBefore;
	if (!TestTrue(TEXT("Missing-primary journal family is captured"),
		SnapshotFamily(MissingJournalPath, MissingJournalBefore)))
	{
		return false;
	}
	FAIityWorldStore MissingJournalStore;
	const bool bMissingJournalOpened = MissingJournalStore.Open(MissingJournalPath, Error);
	const bool bMissingJournalRejected = TestFalse(
		TEXT("Missing primary with rollback journal never initializes"), bMissingJournalOpened);
	MissingJournalStore.Close();
	FFamilySnapshot MissingJournalAfter;
	const bool bMissingJournalReadable = TestTrue(
		TEXT("Missing-primary journal family remains readable"),
		SnapshotFamily(MissingJournalPath, MissingJournalAfter));
	const bool bMissingPrimaryStillAbsent = TestFalse(
		TEXT("Rollback journal is save-family evidence and primary stays absent"),
		MissingJournalAfter.bPrimaryExists);
	const bool bMissingJournalBytes = TestTrue(
		TEXT("Missing-primary rollback journal is unchanged"),
		MissingJournalBefore.Journal == MissingJournalAfter.Journal);
	const bool bMissingJournalPresence = TestTrue(
		TEXT("Missing-primary rollback journal remains present"),
		MissingJournalBefore.bJournalExists == MissingJournalAfter.bJournalExists);
	if (!bMissingJournalRejected || !bMissingJournalReadable || !bMissingPrimaryStillAbsent ||
		!bMissingJournalBytes || !bMissingJournalPresence)
	{
		return false;
	}

	const FString DependentPath = FPaths::Combine(FixtureRoot, TEXT("Dependent.sqlite"));
	FAIityWorldStore DependentSetup;
	AIity::FCandidateTick DependentInitial;
	DependentInitial.State = AIity::FRules::CreateFoundationWorld(901);
	if (!Require(*this, TEXT("Dependent-backup fixture opens"),
			DependentSetup.Open(DependentPath, Error), Error) ||
		!Require(*this, TEXT("Dependent-backup fixture commits"),
			DependentSetup.Commit(DependentInitial, Error), Error) ||
		!Require(*this, TEXT("Dependent-backup fixture creates a backup"),
			DependentSetup.CheckpointAndBackup(Error), Error))
	{
		return false;
	}
	DependentSetup.Close();
	FScopedSqliteDatabase DependentRawSession;
	FSQLiteDatabase& DependentRaw = DependentRawSession.Database;
	if (!Require(*this, TEXT("Dependent-backup primary opens for corruption"),
			DependentRawSession.OpenExclusive(
				*DependentPath, ESQLiteDatabaseOpenMode::ReadWrite, Error), Error) ||
		!Require(*this, TEXT("Dependent-backup primary becomes recoverable"),
			ExecuteRaw(DependentRaw,
				TEXT("UPDATE current_state SET state_text='malformed';"), Error), Error) ||
		!Require(*this, TEXT("Dependent-backup corruption is checkpointed"),
			ExecuteRaw(DependentRaw, TEXT("PRAGMA wal_checkpoint(TRUNCATE);"), Error), Error) ||
		!TestTrue(TEXT("Dependent-backup raw fixture closes"), DependentRaw.Close()) ||
		!TestTrue(TEXT("Dependent backup WAL dependency is written"),
			FFileHelper::SaveStringToFile(TEXT("untrusted backup WAL"),
				*(DependentPath + TEXT(".backup-wal")))))
	{
		return false;
	}
	FFamilySnapshot DependentPrimaryBefore;
	FFamilySnapshot DependentBackupBefore;
	if (!TestTrue(TEXT("Dependent primary is captured"),
			SnapshotFamily(DependentPath, DependentPrimaryBefore)) ||
		!TestTrue(TEXT("Dependent backup family is captured"),
			SnapshotFamily(DependentPath + TEXT(".backup"), DependentBackupBefore)))
	{
		return false;
	}
	FAIityWorldStore DependentStore;
	const bool bDependentOpened = DependentStore.Open(DependentPath, Error);
	const bool bDependentRejected = TestFalse(
		TEXT("Backup with WAL dependency cannot authorize recovery"), bDependentOpened);
	DependentStore.Close();
	const bool bDependentError = TestTrue(TEXT("Dependent backup leaves no validated recovery source"),
		!bDependentOpened && Error.Contains(TEXT("no validated backup")));
	FFamilySnapshot DependentPrimaryAfter;
	FFamilySnapshot DependentBackupAfter;
	const bool bDependentReadable = TestTrue(TEXT("Rejected dependent family remains readable"),
		SnapshotFamily(DependentPath, DependentPrimaryAfter) &&
		SnapshotFamily(DependentPath + TEXT(".backup"), DependentBackupAfter));
	const bool bDependentPrimary = TestTrue(TEXT("Rejected dependent primary is unchanged"),
		DependentPrimaryBefore.Primary == DependentPrimaryAfter.Primary);
	const bool bDependentBackup = TestTrue(TEXT("Rejected dependent backup is unchanged"),
		DependentBackupBefore.Primary == DependentBackupAfter.Primary);
	const bool bDependentWal = TestTrue(TEXT("Rejected dependent backup WAL is unchanged"),
		DependentBackupBefore.bWalExists == DependentBackupAfter.bWalExists &&
		DependentBackupBefore.Wal == DependentBackupAfter.Wal);
	if (!bDependentRejected || !bDependentError || !bDependentReadable || !bDependentPrimary ||
		!bDependentBackup || !bDependentWal)
	{
		return false;
	}

	const FString OrphanPath = FPaths::Combine(FixtureRoot, TEXT("Orphan.sqlite"));
	FAIityWorldStore OrphanStore;
	AIity::FCandidateTick OrphanInitial;
	OrphanInitial.State = AIity::FRules::CreateFoundationWorld(902);
	if (!Require(*this, TEXT("Orphan-sidecar fixture opens"),
			OrphanStore.Open(OrphanPath, Error), Error) ||
		!Require(*this, TEXT("Orphan-sidecar fixture commits"),
			OrphanStore.Commit(OrphanInitial, Error), Error))
	{
		return false;
	}
	const FString OrphanSidecar = OrphanPath + TEXT(".backup.previous-wal");
	TArray<uint8> OrphanBefore;
	TArray<uint8> OrphanAfter;
	if (!TestTrue(TEXT("Orphan previous-backup WAL is written"),
			FFileHelper::SaveStringToFile(TEXT("orphan previous WAL"), *OrphanSidecar)) ||
		!TestTrue(TEXT("Orphan previous-backup WAL is captured"),
			FFileHelper::LoadFileToArray(OrphanBefore, *OrphanSidecar)) ||
		!TestFalse(TEXT("Orphan previous-backup WAL blocks publication"),
			OrphanStore.CheckpointAndBackup(Error)) ||
		!TestFalse(TEXT("Blocked orphan fixture creates no backup primary"),
			Files.FileExists(*(OrphanPath + TEXT(".backup")))) ||
		!TestFalse(TEXT("Blocked orphan fixture creates no previous-backup primary"),
			Files.FileExists(*(OrphanPath + TEXT(".backup.previous")))) ||
		!TestTrue(TEXT("Orphan previous-backup WAL remains readable"),
			FFileHelper::LoadFileToArray(OrphanAfter, *OrphanSidecar)) ||
		!TestTrue(TEXT("Orphan previous-backup WAL is unchanged"),
			OrphanBefore == OrphanAfter))
	{
		return false;
	}
	OrphanStore.Close();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAIityPersistenceCheckpointBoundaryTest,
	"AIity.Persistence.ReopenCheckpointBoundaries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAIityPersistenceCheckpointBoundaryTest::RunTest(const FString& Parameters)
{
	const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Tests/CheckpointBoundary.sqlite"));
	if (!TestTrue(TEXT("Prior scoped boundary fixtures are removed"), DeleteTestWorldFamily(Path)))
	{
		return false;
	}

	FString Error;
	FAIityWorldStore Store;
	if (!Require(*this, TEXT("Boundary world opens"), Store.Open(Path, Error), Error))
	{
		return false;
	}
	AIity::FCandidateTick Initial;
	Initial.State = AIity::FRules::CreateFoundationWorld(300);
	if (!Require(*this, TEXT("Tick zero commits and checkpoints"), Store.Commit(Initial, Error), Error))
	{
		return false;
	}
	Store.Close();

	bool bFound = false;
	AIity::FWorldState State;
	if (!Require(*this, TEXT("Tick zero reopens"), Store.Open(Path, Error), Error) ||
		!Require(*this, TEXT("Tick zero loads"), Store.LoadLatest(State, bFound, Error), Error) ||
		!TestTrue(TEXT("Tick zero state exists"), bFound))
	{
		return false;
	}
	State.Tick = 298;
	AIity::FCandidateTick Setup;
	Setup.State = State;
	if (!Require(*this, TEXT("Boundary fixture reaches Tick 298"), Store.Commit(Setup, Error), Error))
	{
		return false;
	}
	Store.Close();

	bFound = false;
	if (!Require(*this, TEXT("Tick 298 reopens"), Store.Open(Path, Error), Error) ||
		!Require(*this, TEXT("Tick 298 loads"), Store.LoadLatest(State, bFound, Error), Error) ||
		!TestTrue(TEXT("Tick 298 state exists"), bFound))
	{
		return false;
	}
	const AIity::FCandidateTick Reopen299 = AIity::FRules::BuildResumeCandidate(State);
	TestEqual(TEXT("First reopen is Tick 299"), Reopen299.State.Tick, static_cast<uint64>(299));
	if (!Require(*this, TEXT("Tick 299 commits"), Store.Commit(Reopen299, Error), Error))
	{
		return false;
	}
	Store.Close();

	bFound = false;
	if (!Require(*this, TEXT("Tick 299 reopens"), Store.Open(Path, Error), Error) ||
		!Require(*this, TEXT("Tick 299 loads"), Store.LoadLatest(State, bFound, Error), Error) ||
		!TestTrue(TEXT("Tick 299 state exists"), bFound))
	{
		return false;
	}
	const AIity::FCandidateTick Reopen300 = AIity::FRules::BuildResumeCandidate(State);
	TestEqual(TEXT("Second reopen is Tick 300"), Reopen300.State.Tick, static_cast<uint64>(300));
	TestEqual(TEXT("Reopens preserve LogicalSeconds"),
		Reopen300.State.LogicalSeconds, Initial.State.LogicalSeconds);
	if (!Require(*this, TEXT("Tick 300 commits and checkpoints"),
		Store.Commit(Reopen300, Error), Error))
	{
		return false;
	}
	Store.Close();

	FScopedSqliteDatabase RawSession;
	FSQLiteDatabase& RawDatabase = RawSession.Database;
	if (!Require(*this, TEXT("Checkpoint database opens read-only"),
		RawSession.OpenExclusive(*Path, ESQLiteDatabaseOpenMode::ReadOnly, Error), Error))
	{
		return false;
	}
	int64 Count = 0;
	{
		FSQLitePreparedStatement Query = RawDatabase.PrepareStatement(
			TEXT("SELECT COUNT(*) FROM world_checkpoints WHERE tick IN (0,300);"),
			ESQLitePreparedStatementFlags::Persistent);
		if (!TestTrue(TEXT("Checkpoint query is valid"), Query.IsValid()))
		{
			AddError(RawDatabase.GetLastError());
			return false;
		}
		if (!TestEqual(TEXT("Ticks 0 and 300 are retained Checkpoints"),
			Query.Step(), ESQLitePreparedStatementStepResult::Row))
		{
			AddError(RawDatabase.GetLastError());
			return false;
		}
		if (!TestTrue(TEXT("Checkpoint count reads"), Query.GetColumnValueByIndex(0, Count)))
		{
			AddError(RawDatabase.GetLastError());
			return false;
		}
	}
	TestEqual(TEXT("Exactly both boundary Checkpoints exist"), Count, static_cast<int64>(2));
	TestTrue(TEXT("Read-only inspection closes after statements finalize"), RawDatabase.Close());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAIityBridgeClockTest, "AIity.Bridge.ClockContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAIityBridgeClockTest::RunTest(const FString& Parameters)
{
	AIity::FSimulationClock Clock;
	TestEqual(TEXT("Uninitialized clock is fail closed"), Clock.ScaleDelta(1.0f), 0.0f);
	TestFalse(TEXT("Uninitialized clock cannot resume"), Clock.TogglePause());
	Clock.MarkReady();
	TestTrue(TEXT("Ready world starts paused"), Clock.IsPaused());
	TestTrue(TEXT("Owner can resume a ready world"), Clock.TogglePause());
	TestEqual(TEXT("One-times clock uses real delta"), Clock.ScaleDelta(0.25f), 0.25f);
	TestTrue(TEXT("Four-times speed is accepted"), Clock.SetSpeed(4));
	TestEqual(TEXT("Four-times clock scales all simulation work"), Clock.ScaleDelta(0.25f), 1.0f);
	TestTrue(TEXT("Owner can pause"), Clock.TogglePause());
	TestEqual(TEXT("Paused clock freezes movement and timeout delta"), Clock.ScaleDelta(1.0f), 0.0f);
	Clock.MarkFailed();
	TestFalse(TEXT("Failed persistence cannot be toggled back to running"), Clock.TogglePause());
	TestEqual(TEXT("Failed persistence remains frozen"), Clock.ScaleDelta(1.0f), 0.0f);
	Clock.MarkRetryReady();
	TestTrue(TEXT("Successful retry stays paused"), Clock.IsPaused());
	TestFalse(TEXT("Successful retry clears failure"), Clock.HasFailure());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAIityBridgeReceiptTest, "AIity.Bridge.TerminalMovementReceipt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAIityBridgeReceiptTest::RunTest(const FString& Parameters)
{
	const AIity::FCandidateTick First = AIity::FRules::BuildCandidate(AIity::FRules::CreateFoundationWorld(91), {});
	const AIity::FFounder& Founder = First.State.Founders[0];
	const int32 FoodBefore = TotalFood(First.State);
	const AIity::FMovementReceipt Arrival{Founder.Action.Id, Founder.Id, Founder.Action.Id, Founder.Action.Epoch,
		AIity::EMovementOutcome::Arrived, Founder.Action.TargetX, Founder.Action.TargetY};
	const AIity::FCandidateTick Accepted = AIity::FRules::BuildCandidate(First.State, {Arrival});
	const AIity::FCandidateTick Duplicate = AIity::FRules::BuildCandidate(Accepted.State, {Arrival});
	TestEqual(TEXT("Accepted arrival conserves resources"), TotalFood(Accepted.State), FoodBefore);
	TestEqual(TEXT("Duplicate terminal receipt has no effect"), TotalFood(Duplicate.State), TotalFood(Accepted.State));
	TestEqual(TEXT("Duplicate is not consumed twice"), static_cast<int32>(Duplicate.ConsumedReceiptIds.size()), 0);

	AIity::FMovementReceipt Stale = Arrival;
	Stale.ReceiptId++;
	Stale.Epoch--;
	const AIity::FCandidateTick Rejected = AIity::FRules::BuildCandidate(First.State, {Stale});
	TestEqual(TEXT("Stale receipt grants no resource"), TotalFood(Rejected.State), FoodBefore);

	const AIity::FCandidateTick Resume = AIity::FRules::BuildResumeCandidate(First.State);
	Stale = Arrival;
	AIity::FMovementReceipt Fresh = Arrival;
	Fresh.Epoch = Resume.State.ExecutionEpoch;
	const AIity::FCandidateTick StaleThenFresh = AIity::FRules::BuildCandidate(Resume.State, {Stale, Fresh});
	TestEqual(TEXT("Invalid old epoch does not consume the fresh receipt identity"),
		static_cast<int32>(StaleThenFresh.ConsumedReceiptIds.size()), 1);
	TestEqual(TEXT("Fresh resumed movement receipt is consumed"), StaleThenFresh.ConsumedReceiptIds[0], Fresh.ReceiptId);
	return true;
}

#endif

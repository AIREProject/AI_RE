#include "LocalAI/Combat/AIRECompanionAutonomousEvadePolicy.h"

FAIRECompanionAutonomousEvadeDecision
FAIRECompanionAutonomousEvadePolicy::Evaluate(
	const FGuid& ExecutionId,
	const float SelectionChance,
	const float ReactionDelayMin,
	const float ReactionDelayMax)
{
	FAIRECompanionAutonomousEvadeDecision Decision;
	if (!ExecutionId.IsValid()
		|| !FMath::IsFinite(SelectionChance)
		|| !FMath::IsFinite(ReactionDelayMin)
		|| !FMath::IsFinite(ReactionDelayMax)
		|| ReactionDelayMin < 0.0f
		|| ReactionDelayMax < ReactionDelayMin)
	{
		return Decision;
	}

	FRandomStream RandomStream(static_cast<int32>(GetTypeHash(ExecutionId)));
	Decision.bSelected = RandomStream.FRand()
		< FMath::Clamp(SelectionChance, 0.0f, 1.0f);
	if (Decision.bSelected)
	{
		Decision.ReactionDelay = RandomStream.FRandRange(
			ReactionDelayMin,
			ReactionDelayMax);
	}
	return Decision;
}

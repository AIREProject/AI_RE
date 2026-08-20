#pragma once

#include "CoreMinimal.h"

struct AI_RE_API FAIRECompanionAutonomousEvadeDecision
{
	bool bSelected = false;
	float ReactionDelay = 0.0f;
};

/** Stateless deterministic policy used once for each enemy execution. */
struct AI_RE_API FAIRECompanionAutonomousEvadePolicy
{
	static FAIRECompanionAutonomousEvadeDecision Evaluate(
		const FGuid& ExecutionId,
		float SelectionChance,
		float ReactionDelayMin,
		float ReactionDelayMax);
};

#include "AIREGameplayGuidanceSubsystem.h"

void UAIREGameplayGuidanceSubsystem::QueueDeathReturnGuidance()
{
	bDeathReturnGuidancePending = true;
}

bool UAIREGameplayGuidanceSubsystem::ConsumeDeathReturnGuidance()
{
	if (!bDeathReturnGuidancePending)
	{
		return false;
	}

	bDeathReturnGuidancePending = false;
	return true;
}

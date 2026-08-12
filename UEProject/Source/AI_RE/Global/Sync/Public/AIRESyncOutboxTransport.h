#pragma once

#include "CoreMinimal.h"
#include "AIRESyncOutboxTypes.h"

using FAIRESyncOutboxTransportCallback =
	TFunction<void(const FAIRESyncOutboxTransportResult&)>;

class AI_RE_API IAIRESyncOutboxTransport
{
public:
	virtual ~IAIRESyncOutboxTransport() = default;

	/** The transport must invoke Completion on the Game Thread. */
	virtual bool StartAttempt(
		const FAIRESyncOutboxTransportAttempt& Attempt,
		FAIRESyncOutboxTransportCallback Completion) = 0;

	virtual void CancelAttempt(const FGuid& AttemptToken) = 0;
};

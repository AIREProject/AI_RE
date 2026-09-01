#pragma once

#include "CoreMinimal.h"
#include "Chat/Contracts/AIREChatTypes.h"
#include "Command/AIRECompanionCommandTypes.h"
#include "Components/ActorComponent.h"
#include "Work/AIRECompanionWorkOrderTypes.h"
#include "AIRECompanionCommandGatewayComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FAIRECompanionCommandResultSignature,
	const FAIRECommandResult&,
	Result);

class UDataTable;

/**
 * 외부 대화 서비스가 제안한 행동을 Unreal Gameplay 명령으로 변환하는 검증 경계입니다.
 *
 * 명령의 식별자, 요청 상관관계, 수명, 지원 범위를 검증한 뒤 DirectCommand,
 * 전투 또는 WorkOrder로 전달합니다. 외부 AI는 후보만 제안하고 실제 상태 변경과
 * 행동 우선순위는 게임 클라이언트가 소유하도록 책임을 분리했습니다.
 *
 * 처리된 CommandId와 Generation을 함께 관리해 중복 후보와 늦게 도착한
 * 비동기 완료 신호가 같은 명령을 두 번 종료하지 못하게 합니다.
 */
UCLASS(ClassGroup = AIRE, meta = (BlueprintSpawnableComponent))
class AI_RE_API UAIRECompanionCommandGatewayComponent final
	: public UActorComponent
{
	GENERATED_BODY()

public:
	UAIRECompanionCommandGatewayComponent();

	UFUNCTION(BlueprintPure, Category = "AIRE|Companion|Command")
	FAIREDirectCommandSnapshot GetDirectCommandSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "AIRE|Companion|Command")
	bool HasActiveDirectCommand() const;

	bool CanAdvertiseCraftItem(
		const FAIREWorldContextV1& WorldContext) const;

	bool ReportDirectCommandRunning(const FString& CommandId, int64 Generation);
	bool ReportDirectCommandSucceeded(
		const FString& CommandId,
		int64 Generation,
		EAIRECommandResultReason Reason = EAIRECommandResultReason::None);
	bool ReportDirectCommandFailed(
		const FString& CommandId,
		int64 Generation,
		EAIRECommandResultReason Reason);
	bool ReportDirectCommandPreempted(const FString& CommandId, int64 Generation);

	void ShutdownGateway();

	UPROPERTY(BlueprintAssignable, Category = "AIRE|Companion|Command")
	FAIRECompanionCommandResultSignature OnCommandResult;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleChatResponse(const FAIREChatResult& Result);

	UFUNCTION()
	void HandleChatFailure(const FAIREChatError& Error);

	UFUNCTION()
	void HandleWorkOrderChanged(
		FAIRECompanionWorkOrderSnapshot PreviousSnapshot,
		FAIRECompanionWorkOrderSnapshot CurrentSnapshot);

	void EvaluateActiveCommand();
	bool ValidateCandidate(
		const FAIRECommandCandidate& Candidate,
		const FString& ExpectedRequestId,
		EAIRECommandResultReason& OutReason) const;
	bool HasHigherPriorityBehavior() const;
	bool TryExecuteCandidate(const FAIRECommandCandidate& Candidate);
	bool TryExecuteDirectCommand(const FAIRECommandCandidate& Candidate);
	bool TryExecuteCancelCurrent(const FAIRECommandCandidate& Candidate);
	bool TryExecuteAttack(const FAIRECommandCandidate& Candidate);
	bool TryExecuteGatherResource(const FAIRECommandCandidate& Candidate);
	bool TryExecuteLocalGatherFallback(
		const FString& RequestId,
		const FString& SubmittedUserMessage);
	bool TryExecuteLocalCraftFallback(
		const FString& RequestId,
		const FString& SubmittedUserMessage);
	bool TryExecuteCraftItem(const FAIRECommandCandidate& Candidate);
	void RejectCandidate(
		const FAIRECommandCandidate& Candidate,
		EAIRECommandResultReason Reason);
	void CompleteActiveCommand(
		EAIRECommandResultStatus Status,
		EAIRECommandResultReason Reason);
	void CancelActiveCommand(EAIRECommandResultReason Reason);
	void BroadcastResult(
		const FAIRECommandCandidate& Candidate,
		EAIRECommandResultStatus Status,
		EAIRECommandResultReason Reason);
	void RememberProcessedCommand(const FString& CommandId);
	bool IsProcessedCommand(const FString& CommandId) const;
	void ClearEvaluationTimer();

	UPROPERTY(Transient)
	FAIRECommandCandidate ActiveCandidate;

	UPROPERTY(Transient)
	FAIREDirectCommandSnapshot DirectCommandSnapshot;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> ActiveAttackTarget;

	UPROPERTY(EditDefaultsOnly, Category = "AIRE|Companion|Command")
	TObjectPtr<UDataTable> CraftingRecipeTable;

	FGuid ActiveWorkOrderId;

	TSet<FString> ProcessedCommandIds;
	TArray<FString> ProcessedCommandOrder;
	FTimerHandle EvaluationTimerHandle;
	int64 Generation = 0;
	bool bHasActiveCommand = false;
	bool bActiveCommandRunning = false;
	bool bIsEndingPlay = false;
};

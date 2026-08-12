# Durable Sync Outbox

## 목적

`UAIRESyncOutboxSubsystem`은 서버가 아직 확인하지 않은 UE Snapshot/Event operation을
GameInstance와 프로세스 재시작을 넘어 보존하는 endpoint-neutral queue입니다. 실제 Game State
JSON serializer, HTTP path, 인증과 Inventory dirty checkpoint는 AX-I10의 책임입니다.

Outbox는 at-least-once replay와 immutable operation identity를 보장하지만 exactly-once network
delivery를 주장하지 않습니다. 서버는 같은 `operation_id`에 대한 멱등성을 별도로 제공해야 합니다.

## Operation 계약

- 현재 scope는 `AIRE_OPEN / demo-slot-1 / mako`만 허용합니다.
- operation ID는 유효한 UUID이며 재시도 중 바꾸지 않습니다.
- body는 caller가 만든 canonical·redacted UTF-8 bytes입니다. Outbox는 JSON 의미를 해석하지 않고
  플랫폼별 crypto 구현에 의존하지 않는 내부 one-shot SHA-256으로 lowercase hex hash를 계산합니다.
- Snapshot은 ASCII stable `coalescing_key`가 필수이고 Event는 key를 사용하지 않습니다.
- credential, Authorization header, device token과 대화 원문을 body에 넣지 않는 책임은 caller에
  있습니다. Outbox는 opaque body나 그 내용을 일반 로그에 출력하지 않습니다.

동일 operation ID와 schema/kind/scope/key/body hash를 다시 enqueue하면 기존 상태를 반환합니다.
같은 ID를 다른 값에 사용하면 queue를 변경하지 않고 `Conflict`로 거부합니다.

## 상태와 순서

```text
enqueue -> Pending -> durable InFlight -> transport
                       |       |
                       |       +-> timeout/failure/cancel -> Pending
                       +-> matching ID/hash ack -> durable Acked
                                                  -> durable compact -> removed

load: InFlight -> Pending
      Acked    -> non-replay + compact
```

- 전역에서 한 operation만 InFlight입니다. Pending은 enqueue sequence 순으로 수동 dispatch합니다.
- dispatch 전 InFlight 상태를 먼저 저장합니다. dirty 또는 save-in-flight 상태에서는 전송을
  시작하지 않습니다.
- transport callback은 Game Thread 계약이며 lifecycle epoch, attempt token, operation ID와 body
  hash가 현재 attempt와 모두 일치해야 상태를 변경합니다.
- timeout과 transport failure는 자동 retry를 시작하지 않고 같은 ID/hash의 Pending으로 되돌립니다.
- Pending cancel은 제거합니다. InFlight cancel은 transport를 취소하고 Pending으로 보존합니다.
- deinitialize와 `FlushBestEffort`는 파일이나 네트워크 완료를 기다리지 않습니다.

## Snapshot coalescing

같은 scope/key의 아직 전송되지 않은 가장 최근 Pending Snapshot만 새 Snapshot으로 대체합니다.
Event 또는 전송을 시작한 InFlight/Acked entry를 넘어 coalesce하지 않으며 Event는 합치거나
제거하지 않습니다.
새 operation ID/body/hash/sequence가 authoritative하고 superseded ID는 tombstone으로 저장하지
않습니다. enqueue 결과만 어떤 ID가 대체됐는지 돌려줍니다.

## 영속성 및 한도

Outbox는 Inventory SaveGame과 분리된 다음 두 슬롯을 번갈아 사용합니다.

- `AIRE.SyncOutbox.Primary`
- `AIRE.SyncOutbox.Previous`

load 시 두 envelope의 format, generation, canonical scope, entry state/order/identity/hash와 전체
직렬화 크기를 먼저 검증하고 가장 높은 유효 generation 전체만 채택합니다. 부분 복원, hash 자동
수정과 last-write-wins merge는 하지 않습니다. 최신 슬롯이 무효이면 이전 유효 generation을
사용하고 둘 다 무효이면 빈 안전 상태로 시작합니다.

기본 한도는 body 256 KiB, entry 128개, SaveGame memory serialization 기준 envelope 2 MiB입니다.
Snapshot coalescing을 먼저 적용한 뒤에도 한도를 넘으면 기존 queue를 변경하지 않고 거부합니다.

Ack는 두 번의 durable write를 사용합니다. 첫 write가 Acked를 보존하고, 두 번째 write가 compact
결과를 보존한 뒤에만 메모리에서도 제거합니다. compact write가 성공할 때까지 Acked identity를
남겨 같은 ID의 enqueue를 계속 중복으로 판정합니다. 첫 write 뒤 프로세스가 종료되어도 load된
Acked는 재전송하지 않습니다.

## 검증

Automation prefix는 `AIRE.Sync.Outbox.*`입니다. 정상 enqueue, duplicate/conflict, coalescing/Event
barrier, body/count/envelope bound, SaveGame validation, InFlight/Acked recovery, 잘못된 ack,
timeout/cancel과 late callback을 deterministic fake transport로 검증합니다. SHA-256은 empty, `abc`,
56-byte padding 경계의 표준 known-answer vector로 별도 검증합니다.

Codex는 Unreal 프로젝트를 build·launch하지 않습니다. 최종 Gate는 사용자가 Rider/UBT build,
Session Frontend Automation, 종료·재실행 및 map 전환 PIE로 확인합니다.

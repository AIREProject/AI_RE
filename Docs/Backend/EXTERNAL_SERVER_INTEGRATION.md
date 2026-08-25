# External AI Companion Server Integration

## 현재 결정

AI_RE 저장소 안에 Backend를 구현하지 않습니다. UE/Web은 워크스페이스의 별도
`AIRE_SERVER/` 저장소가 제공하는 Backend를 사용합니다.

- Base URL: <https://traip.mtvs2026.work>
- Swagger UI: <https://traip.mtvs2026.work/docs>
- OpenAPI: <https://traip.mtvs2026.work/openapi.json>
- Source contract: `AIRE_SERVER/app/`의 FastAPI model과 route
- 운영 설명: `AIRE_SERVER/docs/api-endpoints.md`

과거 문서에 남은 `ai_companion_server`, `/v1/companion/*`와 AI_RE 내부 Backend는 현행 채택
대상이 아닙니다.

## 계약 권위

1. 배포 `/openapi.json`: 지금 배포된 HTTP path와 DTO
2. `AIRE_SERVER/app/`: 채택 Backend의 model, route와 validation 구현
3. `AIRE_SERVER/tests/`: 오류, 멱등성, scope와 상태 전이의 실행 근거
4. `AIRE_SERVER/docs/`: 운영 설명과 요청 예시
5. [`AI_RE/Contracts/`](../../Contracts/README.md): 권위 위치와 검증 결과를 안내하는 registry

두 최상위 권위가 다르면 클라이언트가 양쪽 형식을 추측 지원하지 않습니다. 차이를 기록하고
Backend source 또는 배포를 먼저 정합화합니다.

## 2026-08-25 배포 정합성

배포 OpenAPI와 현재 `AIRE_SERVER`의 `app.openapi()` 결과를 canonical JSON으로 비교했습니다.

| 항목 | 결과 |
|---|---|
| 구조 비교 | 완전 일치 |
| canonical SHA-256 | `7ccee10375121be87a9512bafa2caf44d17f5f4435d91f8eae16e114d071bc4d` |
| path template | 57개 |
| component schema | 121개 |

따라서 과거에 남아 있던 Context v1, `Command.CraftItem`, Game State, Event, Command Result,
Memory와 Memory Candidate의 “배포 전 목표 계약” 표기는 더 이상 현재 사실이 아닙니다. 이들은
배포 OpenAPI에 노출됩니다.

이 비교는 schema 배포 여부만 확인합니다. 모든 endpoint의 정상·오류 runtime smoke, 실제 LLM,
UE PIE와 모바일 UI 검증을 완료했다는 뜻은 아닙니다.

같은 날 제한된 read-only smoke에서 `/health`는 `status=ok`, `/ready`는 DB revision `0017`과
LLM `ready`를 반환했습니다. Game State, Memory, Memory Candidate와 Offline Task GET도 HTTP
200을 반환했고, malformed pairing body는 HTTP 400 `InvalidRequest` ErrorEnvelope로
정규화됐습니다. 쓰기·충돌·재전송과 전체 오류 matrix는 아직 별도 검증 항목입니다.

## 현재 제품 endpoint

### 상태와 대화

| Method | Path | 역할 |
|---|---|---|
| `GET` | `/health` | 프로세스와 설정 확인 |
| `GET` | `/ready` | DB revision과 LLM readiness 확인 |
| `POST` | `/api/v1/chat` | UE/Web Chat |
| `POST` | `/api/v1/situations` | UE 상황 기반 선제 대사 |

기본 transport는 HTTP Chat입니다. source에는 호환용 `WS /api/v1/chat`도 있지만 OpenAPI는
WebSocket을 표현하지 않으므로 현재 UE/Web 신규 구현의 계약 근거로 사용하지 않습니다.

### Device와 Offline Task

| 영역 | Path |
|---|---|
| Device | `/api/v1/devices`, `/api/v1/devices/me`, `/api/v1/devices/{device_id}` |
| 등록·pairing | `/api/v1/devices/register-game`, `/pairing-codes`, `/pair` |
| Offline Task | `/api/v1/tasks`, `/api/v1/tasks/{task_id}`와 `start/complete/claim/collect` |

현재 제품은 고정 GameClient/WebClient role, 단일 Profile, Save Slot `demo-slot-1`, Companion
`mako` 기준입니다. 실제 bearer credential을 저장소, fixture, URL 또는 로그에 넣지 않습니다.

### 게임 동기화와 실행 기록

| Method | Path | 역할 |
|---|---|---|
| `GET/PUT` | `/api/v1/game-state` | 마지막 승인 Inventory Snapshot 조회·저장 |
| `POST` | `/api/v1/events` | allowlist Game Event 저장 |
| `POST` | `/api/v1/command-results` | Command Candidate 실행 상태 전이 저장 |

Game State는 조회용 서버 복사본이며 gameplay 실행 권위가 아닙니다. PUT은 `operation_id`,
`state_version`, 원문 body hash와 base version 규칙을 따릅니다.

Event는 `event_id`, Command Result는 `operation_id`로 멱등합니다. 두 요청은 body ID와 같은
`X-Request-ID` 및 전송한 raw body의 lowercase SHA-256인 `X-Content-SHA256`을 요구합니다.
Command Result는 canonical Chat에 저장된 같은 scope/session의 Candidate만 참조할 수 있습니다.

### Memory와 후보 검토

| Method | Path | 역할 |
|---|---|---|
| `GET` | `/api/v1/memories` | Active Memory 목록 |
| `GET/PATCH/DELETE` | `/api/v1/memories/{memory_id}` | 상세·정정·고정·삭제 |
| `POST` | `/api/v1/memories/search` | scope 검색 |
| `POST` | `/api/v1/memories/reset` | scope reset |
| `GET` | `/api/v1/memory-candidates` | PendingReview 목록 |
| `GET/PATCH` | `/api/v1/memory-candidates/{candidate_id}` | 상세·Approve/Reject |

Memory 응답의 `sources[]`는 내부 source ID나 원문 없이 type, mode와 발생 시각만 제공합니다.
`last_used_at`과 `use_count`도 현재 배포 schema에 포함됩니다. 후보 응답은 confidence, Provider와
내부 source ID를 노출하지 않습니다. 같은 후보 결정을 다시 보내는 것은 멱등이고 다른 결정은
충돌입니다.

Memory Candidate 배포 계약 blocker는 해소되었습니다. Web의 feature flag 활성화는 별도의 실제
모바일 UI와 runtime 연동 검증을 거쳐 결정하며, OpenAPI 노출만으로 검증 완료 처리하지 않습니다.

## 핵심 DTO 경계

### Chat

- `surface=game`은 strict `GameContextV1`을 요구합니다.
- `surface=mobile`은 `game_context`를 생략하거나 `null`로 보냅니다. `{}`는 허용하지 않습니다.
- Context v1은 location, threat, nearby resources, workstation, current work와 inventories를
  대사 facts로만 제공합니다.
- `allowed_commands`는 최대 16개, 응답 `command_candidates`는 최대 4개입니다.
- 현재 `CommandType`은 Follow, HoldPosition, ReturnToPlayer, EngageTarget, DistractTarget,
  MoveToLocation, CancelCurrent, GatherResource, Attack, Switch와 CraftItem입니다.
- Context나 Memory reference는 Command 권한이나 Recipe 사실을 변경하지 않습니다.

### Event와 Command Result

- Event type은 Combat Started/Ended, Danger Detected, Rescue Completed, Discovery Found,
  Companion Returned allowlist만 지원합니다.
- Command Result 최초 상태는 `Accepted | Rejected | Expired`입니다.
- 후속 전이는 `Accepted → Running → Succeeded | Failed | Cancelled | Expired`만 허용합니다.
- scope, Candidate request/type, 만료와 terminal 이후 전이를 fail-closed로 검증합니다.

정확한 field, enum, 길이와 상한은 배포 OpenAPI를 사용합니다. 이 문서는 DTO 전체를 복제하지
않습니다.

## 오류 계약과 OpenAPI 표시 차이

현재 Backend 구현은 validation 오류를 포함한 HTTP 실패를 다음 `ErrorEnvelope`로 정규화합니다.

```json
{
  "request_id": "request-1",
  "error": {
    "code": "InvalidRequest",
    "message": "Request validation failed.",
    "retryable": false,
    "details": {}
  }
}
```

다만 현재 배포 OpenAPI는 FastAPI 기본 `422 HTTPValidationError`만 다수 operation에 표시하고,
실제 custom `400/401/403/404/409/410/413/500/503/504 ErrorEnvelope` 응답을 schema로 충분히
기술하지 않습니다. 이는 runtime 구현과 OpenAPI 설명 사이의 문서화 gap입니다.

클라이언트는 현재 source와 runtime으로 확인한 ErrorEnvelope를 fail-closed로 파싱하되,
OpenAPI만 보고 개별 오류 code 지원을 추측하지 않습니다. 실제 응답이 Envelope와 다르면 정상
응답으로 바꾸지 말고 계약 불일치로 보고합니다.

## UE/Web 책임 경계

- UE PC가 현재 gameplay state와 Command 최종 실행 권위를 가집니다.
- 모든 Command Candidate는 UE Command Gateway에서 target, 상태, 만료와 capability를 다시
  검증합니다.
- Mobile/Web은 Command Candidate나 Game State 응답으로 UE gameplay를 직접 변경하지 않습니다.
- Backend/LLM timeout, unavailable, malformed response와 late callback은 Inventory, StateTree,
  GAS 또는 현재 Command를 임의 변경하지 않습니다.
- Backend 장애 중에도 로컬 이동·전투·생존과 이미 승인된 로컬 행동을 유지합니다.
- GameWorld와 RealWorld 시간을 섞지 않습니다.

## 레거시 계약 처리

`AI_RE/Contracts/openapi.yaml`, `schemas/`와 `fixtures/`는 과거 구현 추적용입니다. 현재 배포 57개
path 중 일부만 포함하고 제거된 `/api/v1/system/capabilities`도 남아 있으므로 client 생성이나
지원 판정에 사용하지 않습니다. 자세한 경계와 갱신 절차는
[`Contracts/README.md`](../../Contracts/README.md)를 따릅니다.

## 통합 검증 체크리스트

- 배포 OpenAPI와 source OpenAPI canonical 비교
- 정상 및 malformed DTO runtime smoke
- 인증 role과 다른 profile/save slot/companion scope 거부
- timeout, cancellation, duplicate, expiry와 late callback
- Event/Command Result raw-body hash와 멱등 재전송
- Game State version conflict와 stale overwrite 거부
- Memory 수정·삭제·reset 및 후보 같은/다른 결정 재전송
- Backend unavailable 중 UE 로컬 AI 유지
- 실제 UE build/Automation/PIE와 모바일 브라우저 확인

Unreal build, Automation과 PIE는 사용자가 수행합니다.

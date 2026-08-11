# External AI Companion Server Integration

## 결정

2026-07-27부터 기존 `AI_RE` Backend 구현은 사용하지 않습니다. Backend 구현은
워크스페이스의 별도 Git 저장소 `ai_companion_server/`를 채택하고, 실제 서비스 호출은
다음 배포 서버를 사용합니다.

- Base URL: [https://traip.mtvs2026.work](https://traip.mtvs2026.work)
- Swagger UI: [https://traip.mtvs2026.work/docs](https://traip.mtvs2026.work/docs)
- OpenAPI: [https://traip.mtvs2026.work/openapi.json](https://traip.mtvs2026.work/openapi.json)

파트너는 당분간 Backend, DB, LLM Provider와 배포 코드를 개발하거나 운영하지 않습니다.
파트너의 범위는 UE/Web 클라이언트 연동, 외부 응답 검증, timeout·오류·미접속 처리와
로컬 Companion fallback입니다.

## 계약 우선순위

1. 배포 서버 `/openapi.json`: 지금 호출 가능한 런타임 API의 기준
2. `ai_companion_server/docs/current/`: 채택한 Backend의 목표 동작과 클라이언트 계약
3. `ai_companion_server` 현재 코드와 테스트: 목표 계약의 구현 사실
4. `AI_RE/Contracts/`: 계약 재조정 전까지 레거시 참고 자료

배포 OpenAPI와 채택 서버 계약이 다르면 클라이언트에서 임의로 추측하거나 양쪽 형식을
동시에 지원하지 않습니다. 차이를 기록하고 Backend 담당자의 배포 또는 계약 결정을 기다립니다.

## 2026-07-27 확인 결과

로컬 `ai_companion_server` Build 1은 다음 API를 구현합니다.

```text
POST /v1/companion/message
POST /v1/companion/event
```

현재 배포 Swagger는 다음 API를 노출합니다.

```text
GET    /health
GET    /api/v1/system/capabilities
POST   /api/v1/chat
POST   /api/v1/devices/register-game
POST   /api/v1/devices/pairing-codes
POST   /api/v1/devices/pair
GET    /api/v1/devices
GET    /api/v1/devices/me
DELETE /api/v1/devices/me
DELETE /api/v1/devices/{device_id}
```

`GET /health`는 확인 시점에 `status=ok`, `schema_version=1`,
`ai_mode=companion`을 반환했습니다. 그러나 배포 Swagger에는
`/v1/companion/message`와 `/v1/companion/event`가 없으므로 두 서버 계약이 현재
동일하다고 간주할 수 없습니다.

## 2026-08-10 AX-I02 HTTP Chat 재확인

배포 OpenAPI는 `POST /api/v1/chat`와 현재 `ChatRequest`/`ChatResponse`를 제공합니다.
따라서 AX-I02의 HTTP Chat tactical subset에서는 기존 path/DTO blocker가 해소되었습니다.
이 확인은 Chat 요청·응답 범위에만 적용하며, 클라이언트는 배포 OpenAPI에 맞춰 호출합니다.

`ErrorEnvelope` custom 오류 형식은 배포 OpenAPI에 명시되어 있지 않습니다. AX-I02
클라이언트는 현재 `ai_server` code snapshot의 후보 계약을 기준으로 오류 응답을
fail-closed 검증하고, 실제 런타임 응답이 그 계약과 다르면 계약 불일치로 보고합니다.

`Event`, `Command`, Command Result 및 전체 M04 계약 Gate는 여전히 해결되지 않았습니다.
이 항목들의 DTO·인증·매핑은 별도 계약 확인 전까지 확정하지 않습니다.

## 2026-08-11 AX-I03 Command Candidate Prototype

배포 OpenAPI의 현재 `ChatRequest`는 중복 없는 `allowed_commands`를 최대 16개까지 받고,
`ChatResponse.command_candidates`는 최대 4개를 반환합니다. 배포 `CommandType` enum은
Follow, HoldPosition, ReturnToPlayer, EngageTarget, DistractTarget, MoveToLocation,
CancelCurrent, GatherResource, Attack, Switch 열 가지입니다. 워크스페이스에는 기존 문서가
가리키는 별도 `ai_companion_server/`가 없으므로 이 확인에는 배포 OpenAPI와 현재
`AIRE_SERVER/app` 코드만 사용했습니다. `AIRE_SERVER/old/`와 과거 `docs/current/`는
이 범위의 런타임 권위가 아닙니다.

AX-I03 UE Prototype은 LLM Command 선택 경험을 먼저 검증하기 위해 Game Chat마다 열 가지를
모두 고정 광고합니다. 이는 실행 가능한 capability만 광고하는 최종 계약이 아니라 의도적인
예외이며, 모든 Candidate는 UE Command Gateway에서 다시 검증합니다. 현재 로컬 실행 범위는
Follow, HoldPosition, ReturnToPlayer, active WorkOrder Cancel과 UE-selected Threat Attack입니다.
GatherResource와 EngageTarget, DistractTarget, MoveToLocation, Switch는 명시적인
`UnsupportedExecution`으로 끝나며 gameplay mutation을 만들지 않습니다.

Editor MCP로 `ST_AIRECompanion_Local`에 production DirectCommand Task와 property binding,
`IdleNearPlayer`를 구성하고 Compile·Save했습니다. 사용자는 실제 Game Chat PIE에서 Hold lease,
Follow 200cm 정지·재추적과 Combat 선점을 확인했습니다. 직접 ReturnToPlayer 후보,
active WorkOrder Cancel/재개, Level 종료·Owner 파괴와 전체 unsupported Command 실서버 흐름은
아직 검증되지 않았으므로 이 Prototype은 `Review`이며 전체 통합 완료로 보지 않습니다.

현재 Backend brain은 복귀 발화를 `Command.Switch`로 생성하지만 UE는 이를
`ReturnToPlayer`로 추측 변환하지 않습니다. 또한 Attack의 `parameters.target_id`는 안정
World Entity Identity가 구현되기 전까지 선택 Threat와 대조할 수 없으므로 거부합니다.
Command Result의 Backend 전송은 여전히 계약과 endpoint가 없어 로컬 결과로만 유지합니다.
이 Prototype은 Event·Command Result를 포함한 전체 M04 Gate를 해제하지 않습니다.

## 2026-08-10 AX-W01 Web 배포 기준

AX-W01 WebApp은 pairing 및 브라우저 credential 저장 없이 `AIRE_WEB` 고정 신원으로
`POST /api/v1/chat`을 호출합니다. 로컬 개발과 GPT Sites 배포 모두 브라우저에서는
same-origin `/api/*`를 사용하며, 개발 Vite proxy와 Sites Worker가 각각
`https://traip.mtvs2026.work`로 전달합니다.

2026-08-10 당시 private GPT Sites 배포의 루트 HTML, JavaScript asset 및 `/health` Backend
proxy가 HTTP 200으로 확인됐습니다. 이 배포 확인은 Mobile Chat 정상 응답과 전체 실패
매트릭스의 실제 휴대폰 검증을 대신하지 않습니다.

## 2026-08-11 AX-W02 Offline Task Web 배포

배포 OpenAPI의 `POST /api/v1/tasks`, `GET /api/v1/tasks`와 로컬 `AIRE_SERVER`의 Offline
Task DTO가 일치함을 확인하고, WebApp에 Gathering/Crafting 생성과 Task 목록 조회를
추가했습니다. WebClient는 GameClient 전용 start/complete/claim 및 `/collect`를 호출하지
않으며, `Completed`와 `Claimed`를 UE Inventory 지급 완료로 표시하지 않습니다.

사용자 승인에 따라 GPT Sites 접근 정책은 public으로 전환된 현재 상태를 유지합니다.
2026-08-11 배포 뒤 루트 HTML, 배포 JavaScript와 `/health` same-origin proxy가 확인됐고,
배포 JavaScript는 사용자 성공 빌드 산출물과 SHA-256이 일치했습니다. 이 public 운영은
고정 `AIRE_WEB` 신원을 사용하는 단일-player demo 경계이며 production 인증을 제공하지
않습니다.

실제 모바일 Gathering/Crafting 생성·목록 확인, 동일 request ID 멱등성, 네 status filter와
401/403·timeout·network·invalid/malformed JSON UI 매트릭스는 아직 Review 항목입니다.

## 연동 Gate

UE/Web 클라이언트의 새 서버 연동을 완료하려면 Backend 담당자가 다음 중 하나를 확정해야 합니다.

1. `ai_companion_server` Build 1 계약을 배포 서버에 반영
2. 배포 서버 계약을 목표 계약으로 채택하고 `ai_companion_server/docs/current/`를 갱신

Gate가 해결되기 전에도 Base URL 설정, 비동기 요청 수명주기, timeout, 취소, 오류 표시와
Backend 미접속 시 로컬 AI 유지 작업은 진행할 수 있습니다. 요청·응답 DTO와 Command 매핑은
계약 확정 후 구현합니다.

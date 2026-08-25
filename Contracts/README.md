# AI_RE Contract Registry

이 디렉터리는 UE/Web 클라이언트가 채택 Backend 계약을 찾는 진입점입니다. 2026-08-25
기준으로 실제 호출 계약은 이 저장소의 과거 계약 파일이 아니라 다음 두 권위를 따릅니다.

1. 배포 런타임: <https://traip.mtvs2026.work/openapi.json>
2. 채택 Backend source: 워크스페이스 `AIRE_SERVER/app/`의 FastAPI model과 route

운영 설명과 예시는 `AIRE_SERVER/docs/api-endpoints.md`, AI_RE의 클라이언트 경계는
[`Docs/Backend/EXTERNAL_SERVER_INTEGRATION.md`](../Docs/Backend/EXTERNAL_SERVER_INTEGRATION.md)를
따릅니다.

## 2026-08-25 정합성 확인

- 배포 OpenAPI와 현재 `AIRE_SERVER`가 생성한 OpenAPI는 구조적으로 완전히 같습니다.
- canonical JSON SHA-256은
  `7ccee10375121be87a9512bafa2caf44d17f5f4435d91f8eae16e114d071bc4d`입니다.
- 계약에는 path template 57개와 component schema 121개가 있습니다.
- 제품 경로에는 Chat, Situation, Device, Offline Task, Game State, Event, Command Result,
  Memory, Memory Candidate, Health와 Readiness가 포함됩니다.
- `/api/v1/system/capabilities`는 현재 배포 계약에 없습니다.

이 확인은 OpenAPI 구조의 일치를 뜻합니다. 모든 endpoint의 정상·오류 runtime smoke나 UE/Web
실기기 검증을 대신하지 않습니다.

## 현재 제품 API 범위

| 영역 | 경로 |
|---|---|
| 상태 | `GET /health`, `GET /ready` |
| 대화 | `POST /api/v1/chat`, `POST /api/v1/situations` |
| Device | `/api/v1/devices/*` |
| Offline Task | `/api/v1/tasks*` |
| 게임 동기화 | `GET/PUT /api/v1/game-state` |
| 실행 기록 | `POST /api/v1/events`, `POST /api/v1/command-results` |
| 기억 | `/api/v1/memories*`, `/api/v1/memory-candidates*` |
| 운영자 | `/api/v1/admin/*` |

HTTP Chat이 기본 transport입니다. 호환 WebSocket은 source에 `WS /api/v1/chat`으로 남아 있지만
OpenAPI에는 WebSocket 경로가 표현되지 않습니다.

## 이 디렉터리의 과거 산출물

다음 파일은 2026-07-27 이전 Backend용 **레거시 참고 자료**입니다. 현재 client 생성,
validator 구현, endpoint 지원 판정 또는 테스트 기대값의 권위로 사용하지 않습니다.

- `openapi.yaml`
- `schemas/*.schema.json`
- `fixtures/**`

특히 레거시 `openapi.yaml`은 현재 57개 배포 path 중 일부만 담고 있고, 배포에서 제거된
`/api/v1/system/capabilities`도 포함합니다. `schemas/`와 `fixtures/`의 Chat, Event, Memory,
AIService 및 WebSocket 형식 역시 현재 FastAPI DTO와 섞어 쓰면 안 됩니다.

레거시 파일은 과거 구현과 handoff의 추적성을 보존하기 위해 남깁니다. 새 공유 fixture가
필요하면 배포 OpenAPI와 `AIRE_SERVER` 테스트 fixture에서 현재 DTO를 먼저 확정한 뒤 별도
현재 계약 파일로 추가합니다.

## 클라이언트 규칙

- 외부 응답의 필수 field, type, enum과 지원 `schema_version`을 fail-closed로 검증합니다.
- 모르는 optional field는 gameplay 실행 경로에 노출하지 않고 무시합니다.
- Command Candidate는 UE Command Gateway의 재검증 전까지 실행하지 않습니다.
- Event와 Command Result 재전송은 각각 `event_id`와 `operation_id` 멱등 규칙을 지킵니다.
- Mobile/Web은 UE gameplay state를 직접 변경하지 않습니다.
- Backend 장애 시 로컬 StateTree/GAS/Command Gateway 동작을 유지합니다.
- fixture에는 token, credential, 실제 대화 원문을 넣지 않습니다.

## 갱신 절차

1. 배포 `/openapi.json`을 내려받습니다.
2. 현재 `AIRE_SERVER`의 `app.openapi()` 결과와 canonical JSON을 비교합니다.
3. 차이가 있으면 배포와 source 중 어느 쪽을 갱신할지 먼저 결정합니다.
4. 일치한 뒤 이 README와 External Server Integration 문서의 확인 날짜·차이 목록을 갱신합니다.
5. 관련 UE USTRUCT, Web validator와 현재 fixture를 함께 검증합니다.

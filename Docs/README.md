# AI_RE Shared Documentation

이 디렉터리는 팀이 합의하고 공동으로 유지하는 설계 문서를 관리합니다.

개인화된 분석, 임시 구상과 작업 체크리스트는 `.agents/`에서 관리하며, 팀 구현 계약은 이 디렉터리의 문서를 기준으로 합니다.

## 문서 목록

### Backend / AI

- [AI Backend Architecture](Backend/AI_BACKEND_ARCHITECTURE.md) — A/B/C 디바이스 구조, FastAPI 기반 Backend와 AIService 협업 계약
- [AIService Handoff Guide](Backend/AI_SERVICE_HANDOFF.md) — AI 파트 입력·출력 계약, 책임 경계와 Local Runtime Adapter 인계 Gate
- [AI Developer Quickstart](Backend/AI_DEVELOPER_QUICKSTART.md) — 처음 참여한 AI 개발자의 독립 검증, Adapter 교체와 Runtime·모델 계획 가이드
- [WebSocket Chat Protocol Handoff](Backend/WEBSOCKET_CHAT_PROTOCOL_HANDOFF.md) — UE·WebClient 공용 첫 메시지 인증, Chat Frame과 연결 수명주기 계약

### Development / Test

- [Local Baseline Test Guide](Development/LOCAL_BASELINE_TEST.md) — Backend와 WebApp 로컬 실행, 휴대폰 접속, 초기 기준선 범위

## 문서 원칙

1. 공유 구현에 영향을 주는 계약은 코드 변경보다 먼저 갱신합니다.
2. 확정된 구조와 담당 파트가 결정할 항목을 구분합니다.
3. 특정 LLM 모델이나 Runtime에 Backend 도메인을 결합하지 않습니다.
4. API와 데이터 계약은 향후 `Contracts/`의 OpenAPI/JSON Schema를 실행 가능한 기준으로 사용합니다.

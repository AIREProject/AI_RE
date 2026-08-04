# AI_RE Shared Documentation

이 디렉터리는 팀이 합의하고 공동으로 유지하는 설계 문서를 관리합니다.

개인화된 분석, 임시 구상과 작업 체크리스트는 Git 저장소 바깥의 워크스페이스 `.agents/`에서 관리하며, 팀 구현 계약은 이 디렉터리의 문서를 기준으로 합니다.

## 현재 저장소 기준선

- 2026-07-27 재시작 저장소에는 Backend 구현 디렉터리가 없으며, `Docs/Backend/`에는 외부 서버 연동 기준만 둡니다.
- Backend 구현은 워크스페이스의 별도 Git 저장소 `ai_companion_server/`를 채택합니다.
- 배포 Backend는 [https://api.mtvs2026.work](https://api.mtvs2026.work)를 사용합니다.
- 파트너는 Backend를 개발·운영하지 않고 UE/Web 클라이언트 연동을 담당합니다.
- 기존 [`Contracts/`](../Contracts/README.md)는 새 서버와 대조가 끝날 때까지 레거시 참고 자료입니다.

## 문서 목록

### Contracts

- [External Server Integration](Backend/EXTERNAL_SERVER_INTEGRATION.md) — 채택 서버, 계약 우선순위, 현재 배포 불일치와 책임 경계
- [Legacy Contracts](../Contracts/README.md) — 이전 Backend용 OpenAPI, JSON Schema, Fixture

### Development / Test

- [Local Baseline Test Guide](Development/LOCAL_BASELINE_TEST.md) — 재시작 전 Backend·WebApp 기준선의 보존 기록. 현재 Backend 검증 절차로 사용하지 않음

### Unreal Engine

- [Companion GAS Combat Architecture and Verification](UE/COMPANION_GAS_COMBAT.md) — MAKO GAS 전투 책임, 무기별 Data Asset, 콤보·신규 무기 확장과 Editor/PIE 검증 절차
- [Gameplay Inventory and Shared Warehouse](UE/GAMEPLAY_INVENTORY.md) — MAKO 20칸·Equipment 1칸, 공유 창고 50칸과 원자적 로컬 mutation 계약

## 문서 원칙

1. 공유 구현에 영향을 주는 계약은 코드 변경보다 먼저 갱신합니다.
2. 확정된 구조와 담당 파트가 결정할 항목을 구분합니다.
3. 특정 LLM 모델이나 Runtime에 Backend 도메인을 결합하지 않습니다.
4. 현재 호출 가능 여부는 배포 서버 OpenAPI, 목표 동작은 `ai_companion_server/docs/current/`를 기준으로 하고 차이를 명시적으로 관리합니다.

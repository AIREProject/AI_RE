# AI_RE Shared Documentation

이 디렉터리는 팀이 합의하고 공동으로 유지하는 설계 문서를 관리합니다.

개인화된 분석, 임시 구상과 작업 체크리스트는 Git 저장소 바깥의 워크스페이스 `.agents/`에서 관리하며, 팀 구현 계약은 이 디렉터리의 문서를 기준으로 합니다.

## 현재 저장소 기준선

- 2026-07-27 재시작 저장소에는 Backend 구현 디렉터리가 없으며, `Docs/Backend/`에는 외부 서버 연동 기준만 둡니다.
- 현행 Backend 구현 권위는 워크스페이스의 별도 Git 저장소 `AIRE_SERVER/`와 배포
  OpenAPI입니다. 2026-08-25 확인에서 두 OpenAPI는 구조적으로 완전히 일치합니다.
- 배포 Backend는 [https://traip.mtvs2026.work](https://traip.mtvs2026.work)를 사용합니다.
- 2026-08-11 사용자 결정으로 별도 Backend 작업자는 없으며, 필요한 Backend·LLM 변경과
  UE/Web 통합을 파트너가 직접 수행합니다.
- [`Contracts/`](../Contracts/README.md)는 현행 계약의 권위 위치와 검증 결과를 안내합니다.
  그 안의 기존 `openapi.yaml`, `schemas/`, `fixtures/`는 명시적인 레거시 참고 자료입니다.

## 문서 목록

### Contracts

- [External Server Integration](Backend/EXTERNAL_SERVER_INTEGRATION.md) — 채택 서버, 계약 우선순위, 현재 배포 정합성과 책임 경계
- [Contract Registry](../Contracts/README.md) — 현행 런타임 권위, 정합성 결과와 레거시 산출물 경계

### Development / Test

- [Local Baseline Test Guide](Development/LOCAL_BASELINE_TEST.md) — 재시작 전 Backend·WebApp 기준선의 보존 기록. 현재 Backend 검증 절차로 사용하지 않음

### Unreal Engine

- [Companion GAS Combat Architecture and Verification](UE/COMPANION_GAS_COMBAT.md) — MAKO GAS 전투 책임, 무기별 Data Asset, 콤보·신규 무기 확장과 Editor/PIE 검증 절차
- [Shared Combat Damage, Enemy Reaction, and Aggro Swap](UE/COMBAT_DAMAGE_STAGGER_CONTRACT.md) — Player·MAKO·Enemy 공용 피해/스태거 실행, Boss AI와 Q 어그로 스왑 계약
- [Gameplay Inventory and Shared Storage](UE/GAMEPLAY_INVENTORY.md) — MAKO 20칸·Equipment 1칸, 공유 보관함 50칸과 원자적 로컬 mutation 계약
- [World Time and Day/Night](UE/WORLD_TIME_DAY_NIGHT.md) — 싱글플레이 게임 시간 권위와 낮밤 조명

## 문서 원칙

1. 공유 구현에 영향을 주는 계약은 코드 변경보다 먼저 갱신합니다.
2. 확정된 구조와 담당 파트가 결정할 항목을 구분합니다.
3. 특정 LLM 모델이나 Runtime에 Backend 도메인을 결합하지 않습니다.
4. 현재 호출 가능 여부는 배포 서버 OpenAPI, 목표 동작은 `AIRE_SERVER/`의 현행 문서·코드·테스트를 기준으로 하고 차이를 명시적으로 관리합니다.

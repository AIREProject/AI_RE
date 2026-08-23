# AIRE World Time and Day/Night

## 목적

싱글플레이 월드의 게임 시간을 하나의 로컬 권위로 유지하고 낮밤 조명에 사용합니다.
InGame Chat의 시간 Context는 별도 요구사항에 따라 PC 현실 로컬 시각을 사용합니다.

## 구조

| 타입 | 책임 |
|---|---|
| `UAIREWorldTimeSubsystem` | 게임 시간 진행, Day 증가, 낮밤 판정과 변경 이벤트 |
| `AAIREDayNightVisualController` | 지정된 태양·달·SkyLight의 회전·밝기·색 보간 |

멀티플레이용 GameState, 시간 복제, 전용 GameMode는 사용하지 않습니다. Visual Controller는
월드를 검색해 라이트를 추측하지 않고 레벨에서 지정된 참조만 제어합니다.

## 시간 규칙

- 현실 1,200초가 게임 24시간입니다.
- 시간은 1초 간격으로 갱신되며 현실 1초마다 게임 1.2분 진행합니다.
- 새 월드는 Day 1, 09:00에서 시작합니다.
- 24:00을 통과하면 시간을 00:00으로 순환시키고 Day를 1 증가시킵니다.
- 낮은 05:00 이상 23:00 미만, 밤은 23:00 이상 또는 05:00 미만입니다.
- 저장/로드와 종료 중 현실 경과 반영은 현재 범위에 없습니다.

## 조명 규칙

- 새벽 보간: 04:00~06:00
- 완전한 낮: 06:00~22:00
- 황혼 보간: 22:00~24:00
- 완전한 밤: 00:00~04:00
- 태양 기준점: 일출 05:00, 정점 14:00, 일몰 23:00, 최저점 02:00
- 달 기준점: 지평선 23:00, 야간 정점 02:00, 지평선 05:00
- 자동 노출: `MainLevel_Top`의 Unbound Post Process Volume에서 Min/Max EV100을 모두 `0`으로 고정
- 첫 시간 적용은 즉시 처리하고 이후 회전·밝기·색은 Tick에서 보간합니다.

기본 밝기·색은 PalWorld Prototype의 실제 Controller instance 값을 계승합니다. 낮/밤의
태양 밝기는 `10`/`0.02`, 달은 `0`/`0.15`, SkyLight는 `0.8`/`0.05`입니다. 맵별 시각
튜닝은 Controller instance property로 조정하며 Subsystem의 시간 규칙은 변경하지 않습니다.
노출은 Controller가 보간하지 않고 맵의 Post Process Volume이 고정합니다.

## MainLevel_Top 배치

Unreal MCP 연결 후 다음 순서로 구성합니다.

1. 기존 Movable `DirectionalLight`를 태양으로, 기존 Movable `SkyLight`를 하늘 보조광으로 사용합니다.
2. Movable Directional Light `DL_Moon`을 추가하되 Atmosphere Sun Light는 끕니다. 태양만
   Atmosphere Sun Light Index `0`을 사용하고 Forward Shading Priority는 태양 `1`, 달 `0`으로
   구분합니다.
3. `AAIREDayNightVisualController`를 한 개 추가해 태양, 달, SkyLight 참조를 직접 지정합니다.
4. Unbound `AIRE_DayNightPostProcess`를 추가하고 Auto Exposure Min/Max EV100을 `0`으로
   고정합니다.
5. 월드 파티션 전역 액터는 Spatial Loading을 비활성화하고 맵을 저장합니다.

달은 대기 원반과 하늘 산란에 참여하지 않고 방향광만 제공합니다. SkyAtmosphere, 비활성
Height Fog, Volumetric Cloud와 고정 노출은 PalWorld 맵의 실측값을 사용해 같은 렌더링
기반을 맞춥니다.

현재 다른 플레이맵, 동적 날씨, 별·달 메시, UI 시계, 수면과 시간 건너뛰기,
AI·스폰의 낮밤 행동은 범위에 포함하지 않습니다.

## 검증

- Automation: `AIRE.World.Time.Progression`
- Console: `aire.Time.ToggleDayNight`
- Console: `aire.Time.SetTime HH:MM`
- Console: `aire.Time.TogglePause`

`SetTime`은 현실 시계와 같은 24시간제 `HH:MM` 형식을 사용합니다. 유효 범위는
`00:00`~`23:59`입니다. 일출 후보는 04:45~06:15, 일몰 후보는 21:45~23:30 범위에서
15분 단위로 비교합니다. 먼저 `aire.Time.TogglePause`로 시간 진행을 멈춘 뒤 `SetTime`을
반복하면 같은 구도에서 시간을 비교할 수 있습니다. 기존 decimal-hour 형식의
`aire.Time.SetHour`는 개발 호환용으로 유지하지만 신규 사용에는 권장하지 않습니다.

Codex는 Unreal 빌드나 Editor/PIE를 실행하지 않습니다. 사용자가 최신 source를 빌드한 뒤
Automation과 `MainLevel_Top` PIE에서 조명 전환, 자정 그림자, 종료·재진입 초기화를 확인해야
합니다.

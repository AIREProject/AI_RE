# Gameplay Inventory와 공유 창고 계약

- 관련 Milestone: `M03 Companion Local AI`
- 관련 Task: `M03-E08-T01`
- 현재 상태: Review, 사용자 UBT·월드 창고 Snapshot 스모크 통과; 자동화·Transfer·두 번째 MAKO 무기 검증 대기

## 1. 책임과 수명

`UAIREGameplayInventorySubsystem`은 한 GameInstance Session 동안 다음 로컬 값을
소유합니다.

- `AIRE.Inventory.MAKO`: 일반 Item Stack 20칸과 별도 Weapon Equipment 1칸
- `AIRE.Inventory.SharedWarehouse`: Player·MAKO 공용 occupied Stack 50칸
- Container별 revision, 성공한 mutation ID와 장착 예약

Subsystem은 Actor, ASC 또는 Equipment Component를 저장하지 않습니다. Item 상태에는
안정 `ItemId`, Slot Index와 Count만 저장하고 정적 Definition과 MaxStack은 기존
`UAI_REItemSubsystem`에서 조회합니다. GameInstance 수명은 SaveGame 영속성이 아닙니다.

## 2. Snapshot과 변경

Snapshot은 Session ID, Container ID, revision, capacity, occupied Stack과 MAKO의
equipped·pending·transition 상태를 값 복사로 반환합니다. 반환 배열을 호출자가 바꿔도
Subsystem 상태는 변경되지 않습니다.

Add·Remove·Move·Transfer는 Session ID, mutation GUID와 expected revision을 검증하고
전체 변경을 먼저 계산합니다. 전량을 처리할 수 없으면 아무 상태도 바꾸지 않습니다.
성공한 mutation 재요청은 `AlreadyApplied`를 반환하고 revision이나 Delegate를 다시
증가시키지 않습니다. Container 간 Transfer는 양쪽 값을 먼저 commit한 뒤 변경된 각
Container의 revision과 Delegate를 한 번씩 갱신합니다.

명시적 Session reset은 새 Session ID를 만들고 Container, revision, 장착 예약과
in-session mutation 기록을 초기화합니다. SaveGame과 영속 중복 방지는 후속
`M03-E08-T03` 책임입니다.

## 3. MAKO Equipment

장착 무기는 일반 20칸을 사용하지 않습니다. 교체 요청은 새 무기의 일반 Slot을 잠그고
pending으로 표시하지만 Item을 미리 이동하지 않습니다. Equipment 비동기 성공 뒤에만
새 무기를 Equipment Slot으로 옮기고 이전 무기를 잠근 Slot으로 원자 교환합니다.

새 무기 장착이 실패하면 Item 배치를 유지한 채 이전 무기의 Equipment 런타임을
복구합니다. 복구도 실패하면 마지막 유효 equipped Item과 수량은 보존하고
`RecoveryFailed`를 노출합니다. Component 종료, Session reset과 늦은 Callback은
Session·mutation 불일치 상태를 변경하지 않습니다.

`UAIRECompanionInventoryComponent`는 Healing과 기존 호출자를 위한 Has·Add·Consume·Equip
façade 및 Equipment/ASC Callback 수명만 유지합니다. Item 수량과 equipped·pending Item
ID는 Subsystem과 중복 소유하지 않습니다.

## 4. 월드 창고 접근점

`AAIRESharedWarehouseActor`는 공유 창고의 물리적 월드 접근점입니다. 기존 Player 상호작용
Trace가 사용하는 `IAI_REInteractableInterface`를 구현하며, Visibility 전용 상호작용
Collision과 Blueprint에서 설정할 Static Mesh를 제공합니다.

Actor는 Item·revision을 소유하지 않습니다. 상호작용할 때 Gameplay Inventory Subsystem의
공유 창고 Snapshot을 조회하고 `On Warehouse Opened` Blueprint Event로 전달합니다. 실제
창고 UI 생성·Binding·Transfer 입력은 `M03-E08-T02`에서 이 Event에 연결합니다. Crafting
책임이 섞이지 않도록 `AAI_REWorkBenchBase`를 상속하지 않습니다.

2026-08-04 사용자 PIE 스모크에서 Player 상호작용 시 빈 공유 창고의 `Revision=0`,
`Capacity=50`, `ContainerId=AIRE.Inventory.SharedWarehouse`가 Blueprint Event로 전달되는
것을 확인했습니다. 예치·인출과 변경 후 revision 갱신은 T02 UI 연결 뒤 검증합니다.

## 5. Player와 테스트 경계

Player 개인 Inventory와 Quick Slot은 기존 Actor Component가 계속 소유합니다. 공유
창고 Transfer는 일반 Slot만 대상으로 post-state를 먼저 준비하고 Player·창고를 함께
commit합니다. 기존 `AddItem`의 부분 성공 동작, `UseItem`, Crafting과 UI는 바꾸지 않습니다.

Work 구현 전에는 기존 MAKO Config의 기본 쌍검·회복 앰플과 Testing Blueprint helper의
명시적 seed를 검증 입력으로 사용합니다. 실제 Work 결과는 후속 E07 통합에서 같은
검증된 Add mutation을 한 번만 호출합니다. 두 번째 MAKO Weapon 자산이 없으므로 실제
다중 무기 async 교체·복구 검증 전까지 T01은 `Review`입니다.

## 6. 외부 Import 경계

T01의 Startup Import Candidate는 UE 내부 C++ 값이며 Backend DTO가 아닙니다. local format,
profile/save/companion scope, Session·Container, base revision, candidate/operation ID와 전체
Add·Remove batch를 검증한 뒤 한 번만 적용합니다. 현재 Backend에는 Inventory·Snapshot·
Offline settlement 계약이 없으므로 HTTP, JSON 필드와 재시도 정책은 확정하지 않습니다.
실제 연동은 Backend 계약과 배포 OpenAPI가 일치한 뒤 `M03-E08-T04`에서 구현합니다.

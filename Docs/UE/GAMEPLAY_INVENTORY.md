# Gameplay Inventory와 Shared Storage(공유 보관함) 계약

- 관련 Milestone: `M03 Companion Local AI`
- 관련 Task: `M03-E08-T01`, `M03-E07-T02`, `M03-E08-T02`, `M03-E08-T05`
- 현재 상태: M03-E08-T01·M03-E07-T02·M03-E08-T02·M03-E08-T05 Review.
  최신 UI·물리적 Shared Storage Work C++의 사용자 UBT와 세 Inventory WBP 및
  `BP_AIRESharedStorage` Compile/Save는 통과했습니다. 리네임 이후 PIE 정상·실패 경로,
  Storage Rule·Preferred Storage 연결과 두 번째 MAKO 무기 검증은 대기 중입니다.

## 1. 책임과 수명

`UAIREGameplayInventorySubsystem`은 한 GameInstance Session 동안 다음 로컬 값을
소유합니다.

- `AIRE.Inventory.MAKO`: 일반 Item Stack 20칸과 별도 Weapon Equipment 1칸
- `AIRE.Inventory.SharedStorage`: Player·MAKO 공용 occupied Stack 50칸
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

## 4. 월드 Shared Storage 접근점

`AAIRESharedStorageActor`는 Shared Storage의 물리적 월드 접근점입니다. 기존 Player 상호작용
Trace가 사용하는 `IAI_REInteractableInterface`를 구현하며, Visibility 전용 상호작용
Collision과 Blueprint에서 설정할 Static Mesh를 제공합니다.

Actor는 Item·revision을 소유하지 않습니다. 상호작용할 때 Gameplay Inventory Subsystem의
Shared Storage Snapshot을 조회하고 월드당 하나인 `UAIREInventoryUIWorldSubsystem`에 자신과
Player Interactor를 명시적으로 전달해 Storage Panel을 엽니다. `On Storage Opened`
Blueprint Event를 호출하며, Crafting 책임이 섞이지 않도록
`AAI_REWorkBenchBase`를 상속하지 않습니다.

MAKO의 물리적 Shared Storage 작업은 Actor의 조정 가능한 `CompanionInteractionPoint` Transform을
목표로 사용합니다. 이 Scene Component는 접근 위치와 작업 방향만 제공하며 Inventory나
WorkOrder 상태를 소유하지 않습니다.

2026-08-04 사용자 PIE 스모크에서 Player 상호작용 시 빈 Shared Storage의 `Revision=0`,
`Capacity=50`, `ContainerId=AIRE.Inventory.SharedStorage`가 Blueprint Event로 전달되는
것을 확인했습니다. 예치·인출과 변경 후 revision 갱신은 T02 UI 연결 뒤 검증합니다.

## 5. Inventory UI 경계

Inventory UI는 두 개의 독립 화면입니다. Player가 공유 보관함 Actor와 상호작용하면
`Player Inventory + 공유 보관함`만 표시하고, Player가 MAKO와 상호작용하면
`MAKO Inventory 20칸 + Equipment 1칸`만 표시합니다. Shared Storage 화면에 MAKO Inventory를
합치거나 MAKO 화면에 Storage Transfer 입력을 추가하지 않습니다.

정식 명칭은 한국어 `공유 보관함`, 영문 `Shared Storage`입니다. C++ 계약은
`AAIRESharedStorageActor`, `UAIREStorageInventoryPanelWidget`, `StorageTransfer`,
`UAIRECompanionStorageAutomationComponent`, `PreferredStorage`,
`AIRE.Inventory.SharedStorage`, `TryTransferPlayerStorage`를 사용합니다. 기존
`AIRE.Inventory.SharedWarehouse`는 저장된 legacy ID를 정규화하는 exact alias 호환에만
남깁니다. 기존 이름으로 저장된 C++ Class와 Asset 참조는 `CoreRedirects` 및 Editor asset
redirector로 이전하며, 신규 코드·UI 문구·자산 이름에는 정식 Storage 명칭만 사용합니다.

`UAIREInventoryUIWorldSubsystem`은 월드당 열린 Inventory 창 하나, Escape 닫기, 이동·시점
입력 억제와 cursor/input mode 복구를 담당합니다. Panel과 Slot Widget의 입력·drag/drop·
mutation·Delegate binding은 C++ 책임입니다. WBP는 BindWidget 이름에 맞춘 배치·색상·
폰트·크기와 `SlotWidgetClass` 지정만 소유하며 Graph에서 Inventory 상태를 변경하지 않습니다.

Player와 Shared Storage 사이의 일반 drag는 최신 Source Stack 전체를 이동합니다. `Shift+drag`는
WBP 수량 선택기를 열어 정확한 수량을 입력받습니다. 반대편 Grid에 drop한 경우만 처리하고
목적 Slot은 지정하지 않으며, 기존 Stack 병합 뒤 첫 빈 Slot을 사용하는 자동 배치를 유지합니다.
요청 수량 전체를 수용하지 못하면 양쪽 상태를 모두 보존합니다. 구형 Deposit·Withdraw 버튼과
항상 노출된 수량 입력은 사용하지 않습니다.

Storage Panel은 Player `OnInventoryChanged`와 Shared Storage `OnContainerChanged`를,
MAKO Panel은 `OnInventoryChanged`와 `OnWeaponEquipResult`를 구독합니다. 같은 프레임 신호는
다음 Tick 한 번의 Refresh로 합치며, UI는 mutation 성공 전에 수량을 바꾸거나 실제 배열·
Session·revision을 소유하지 않습니다. Conflict·Session 변경에서는 자동 재시도하지 않고
최신 Snapshot과 실제 Player Inventory를 다시 표시합니다.

## 6. Player와 테스트 경계

Player 개인 Inventory와 Quick Slot은 기존 Actor Component가 계속 소유합니다.
Shared Storage Transfer는 일반 Slot만 대상으로 post-state를 먼저 준비하고 Player·Shared Storage를 함께
commit합니다. 기존 `AddItem`의 부분 성공 동작, Quick Slot과 `UseItem`은 바꾸지 않습니다.

Player 제작은 `Player Inventory → Shared Storage`, MAKO 제작은
`MAKO Inventory → Shared Storage` 우선순위로 각 로컬 Inventory와 Shared Storage를 하나의 재료
풀처럼 사용합니다. Recipe의 중복 재료 행은 Item ID별로 합산하고, 로컬 수량을 먼저
사용한 뒤 부족분만 Shared Storage에서 사용합니다. 모든 재료 제거와 결과 배치의 post-state가 유효할
때만 한 번에 commit합니다. 합산 재료가 부족하거나 허용된 결과 목적지가 수용할 수 없으면
재료·결과·revision·mutation ledger를 모두 보존합니다. Player 결과는 기존처럼 Player
Inventory에 배치합니다.

MAKO 제작 완료는 `FAIREMakoCraftWorkRequest`로 재료 전체와 결과를 먼저 검증한 뒤
`TryCompleteMakoCraftWork`에서 한 번에 commit합니다. `WorkOrderId`를 mutation ID로
사용하므로 재호출은 `AlreadyApplied`를 반환하고 재료 소비·결과 지급·revision·Event를
반복하지 않습니다. 결과는 MAKO Inventory, Shared Storage, 허용된 World Drop 순서로
귀결됩니다.

제작 중 Shared Storage 접근은 Recipe 한 건을 정산하는 논리적 예외이며 물리적
`StorageTransfer` WorkOrder를 생성하지 않습니다. 현재 재료 부족은 로컬 실패로 끝나며
Backend·LLM 호출을 하지 않습니다. 부족 재료 요청을 LLM으로 확장하는 기능은 후속 범위입니다.

채집 보상은 보상 발생마다 생성한 `DeliveryId`를 가진 실제 World Item Actor로 먼저
생성합니다. MAKO가 채집한 Item은 짧은 지연 뒤 지정 반경 안의 해당 MAKO에게 자동 획득을
시도하고, 성공하면 MAKO 방향으로 흡수되는 표현 뒤 제거됩니다. 획득 시점에
`FAIREMakoWorkRewardRequest`와 같은 `DeliveryId`로 `TryStoreMakoWorkReward`를 호출하므로
중복 Overlap이나 재시도가 수량을 반복 지급하지 않습니다.

MAKO Inventory가 수용하지 못하면 Shared Storage를 사용합니다. 두 컨테이너가 모두 수용하지
못한 `WorldDrop` 결과는 applied ledger에 기록하지 않으므로 Item은 바닥에 남고, MAKO가
범위 안에 있는 동안 공간이 생기면 같은 `DeliveryId`로 다시 획득할 수 있습니다. Player의
기존 수동 `Interact` 획득은 유지하며 먼저 성공한 경로만 Item을 claim합니다. 이 근접
자동 획득은 AI Perception이나 이동 명령이 아니며, 먼 Item을 찾아 이동하는 행동은 후속
WorkOrder가 소유합니다. 채집 보상의 `MAKO → Shared Storage` 직접 fallback은 정확한 보상 정산
예외입니다. MAKO의 일반 자동 예치·인출은 `M03-E08-T05`의 `StorageTransfer` WorkOrder로
Shared Storage 접근점까지 실제 이동한 뒤 수행하므로 두 경로를 같은 행동으로 취급하지 않습니다.

`UAIRECompanionStorageAutomationComponent`는 Config의 순서가 보장된 Storage Rule을
`최대 초과 예치 → 최소 미달 인출` 순서로 평가합니다. 명시적으로 설정된 `PreferredStorage`만
사용하고 Tick 검색은 하지 않습니다. 한 WorkOrder는 현재 Source Stack 하나만 옮기며,
Equipped Item은 보유량에는 포함하지만 Equipped·Pending Item은 Transfer Source로 사용하지
않습니다. 실행기는 도착·방향 정렬·선택적 Montage·고정 작업 시간을 마친 뒤 최신 MAKO와
Shared Storage Snapshot, Session과 Rule을 다시 확인하고 `WorkOrderId`를 mutation ID로 사용해
`TryTransferItem`을 호출합니다. 전투가 선점하면 기존 `PausedByCombat → Requested` 계약으로
이동 시간과 작업 시간을 처음부터 다시 시작합니다.

두 번째 MAKO Weapon 자산이 없으므로 실제 다중 무기 async 교체·복구 검증 전까지
T01은 `Review`입니다.

2026-08-04 사용자 결정으로 제작·채집 결과 수량의 화면 확인은 `M03-E08-T02`
Inventory·Shared Storage UI 검증에 포함합니다. 이 이관은 E07의 원자 mutation과 exact-once
계약을 변경하지 않으며 UI는 Snapshot을 표시할 뿐 WorkOrder나 보상 수량을 직접
수정하지 않습니다.

## 7. 외부 Import 경계

T01의 Startup Import Candidate는 UE 내부 C++ 값이며 Backend DTO가 아닙니다. local format,
profile/save/companion scope, Session·Container, base revision, candidate/operation ID와 전체
Add·Remove batch를 검증한 뒤 한 번만 적용합니다. 현재 Backend에는 Inventory·Snapshot·
Offline settlement 계약이 없으므로 HTTP, JSON 필드와 재시도 정책은 확정하지 않습니다.
실제 연동은 Backend 계약과 배포 OpenAPI가 일치한 뒤 `M03-E08-T04`에서 구현합니다.

# Gameplay Inventory와 Shared Storage(공유 보관함) 계약

- 관련 Milestone: `M03 Companion Local AI`
- 관련 Task: `M03-E08-T01`, `M03-E07-T02`, `M03-E08-T02`, `M03-E08-T03`, `M03-E08-T05`, `M03-E08-T06`
- 현재 상태: M03-E08-T01·M03-E07-T02 Review, M03-E08-T05 Planned,
  M03-E08-T02·M03-E08-T06 In Progress, M03-E08-T03 Review
  (SaveGame 계약·구현 범위 반영 완료, 사용자 Build/Automation/PIE 대기).
  최신 UI와 기존 물리적 Shared Storage Work C++의 사용자 UBT, 세 Inventory WBP 및
  `BP_AIRESharedStorage` Compile/Save는 통과했습니다. 리네임 이후 PIE 정상·실패 경로,
  새 논리 Storage Rule과 두 번째 MAKO 무기 검증은 대기 중입니다.

## 1. 책임과 수명

`UAIREGameplayInventorySubsystem`은 한 GameInstance Session 동안 다음 로컬 값을
소유합니다.

- `AIRE.Inventory.MAKO`: 일반 Item Stack 20칸과 별도 Weapon Equipment 1칸
- `AIRE.Inventory.SharedStorage`: Player·MAKO 공용 occupied Stack 50칸
- Container별 revision, 성공한 mutation ID와 장착 예약

Player 일반 Inventory 30칸은 기존 `UAI_REPlayerInventoryComponent`가 계속 소유합니다.
Component는 일반 Inventory revision과 별도 Weapon Equipment 1칸의 안정 Item ID를
소유하고, Gameplay Inventory Subsystem은 MAKO↔Player 직접 Transfer와 Player 장착 요청의
Session·mutation 중복 방지 경계를 제공합니다. Player Quick Slot 100~109는 이 직접
Transfer와 Equipment Slot에 포함하지 않습니다.

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

MAKO 일반 20칸은 `UAI_REItemSubsystem`에서 확인되는 일반 Item도 보관할 수 있습니다.
MAKO 전용 Definition 요구는 Equipment 장착 호환성에만 적용하며, Player에서 옮긴 일반
재료 Item을 SaveGame 검증이 거부해서 이전 세대로 되돌리는 일이 없어야 합니다.

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

### Player Equipment

Player Weapon Equipment는 일반 30칸 및 Quick Slot과 분리된 1칸입니다. Player 일반
Inventory의 `UAI_REWeaponItemDataAsset` Stack 1개를 Equipment Slot에 drop하면 최신 Player
revision·Session·mutation ID와 Source Slot을 검증합니다. `UAI_REPlayerCombatComponent`가
Weapon Definition을 받아들인 뒤에만 새 무기를 Equipment Item ID로 commit하고, 이전
장착 무기는 같은 Source Slot으로 원자 교환합니다. 런타임 장착이 거부되면 Item 배열과
Equipment Item ID는 모두 유지합니다.

Player Combat Component는 장착 성공 시 기존 Weapon Ability·Mesh·Linked Anim Layer를
정리하고 새 Definition의 Ability·Mesh·Montage·Layer를 적용합니다. Montage 비동기 Load는
Component 종료 또는 다음 장착에서 취소하며 Inventory commit을 늦게 변경하지 않습니다.
Player 장착은 Player 전용 `UAI_REWeaponItemDataAsset`만 허용합니다. MAKO 전용 Weapon은
Player 일반 Inventory에 보관할 수 있지만 Player Equipment Slot에는 장착할 수 없으며,
반대 방향도 같은 호환성 규칙을 적용합니다.

## 4. 월드 Shared Storage 접근점

`AAIRESharedStorageActor`는 Shared Storage의 물리적 월드 접근점입니다. 기존 Player 상호작용
Trace가 사용하는 `IAI_REInteractableInterface`를 구현하며, Visibility 전용 상호작용
Collision과 Blueprint에서 설정할 Static Mesh를 제공합니다.

Actor는 Item·revision을 소유하지 않습니다. 상호작용할 때 Gameplay Inventory Subsystem의
Shared Storage Snapshot을 조회하고 월드당 하나인 `UAIREInventoryUIWorldSubsystem`에 자신과
Player Interactor를 명시적으로 전달해 Storage Panel을 엽니다. `On Storage Opened`
Blueprint Event를 호출하며, Crafting 책임이 섞이지 않도록
`AAI_REWorkBenchBase`를 상속하지 않습니다.

2026-08-13 사용자 결정으로 MAKO 자동 예치·인출은 이 Actor까지 이동하지 않고 Inventory
Subsystem 안에서 논리적으로 수행합니다. `CompanionInteractionPoint`는 기존 자산 호환을 위해
남길 수 있지만 MAKO 자동 Transfer의 실행 조건이나 목표로 사용하지 않습니다.

2026-08-04 사용자 PIE 스모크에서 Player 상호작용 시 빈 Shared Storage의 `Revision=0`,
`Capacity=50`, `ContainerId=AIRE.Inventory.SharedStorage`가 Blueprint Event로 전달되는
것을 확인했습니다. 예치·인출과 변경 후 revision 갱신은 T02 UI 연결 뒤 검증합니다.

## 5. Inventory UI 경계

Inventory UI는 상호작용 대상에 따라 두 화면을 사용합니다. Player가 공유 보관함 Actor와
상호작용하면 기존 `Player Inventory + 공유 보관함` 화면을 표시합니다. Player가 MAKO와
상호작용하면 하나의 합성 modal에서 왼쪽 `MAKO Inventory 20칸 + Equipment 1칸`, 오른쪽
`Player Inventory 30칸 + Equipment 1칸`을 함께 표시합니다. 합성 modal은 Shared Storage나
StorageTransfer WorkOrder를 표시하거나 실행하지 않습니다.

정식 명칭은 한국어 `공유 보관함`, 영문 `Shared Storage`입니다. C++ 계약은
`AAIRESharedStorageActor`, `UAIREStorageInventoryPanelWidget`, `StorageTransfer`,
`UAIRECompanionStorageAutomationComponent`,
`AIRE.Inventory.SharedStorage`, `TryTransferPlayerStorage`를 사용합니다. 기존
`AIRE.Inventory.SharedWarehouse`는 저장된 legacy ID를 정규화하는 exact alias 호환에만
남깁니다. 기존 이름으로 저장된 C++ Class와 Asset 참조는 `CoreRedirects` 및 Editor asset
redirector로 이전하며, 신규 코드·UI 문구·자산 이름에는 정식 Storage 명칭만 사용합니다.

`UAIREInventoryUIWorldSubsystem`은 월드당 열린 Inventory 창 하나, Escape 닫기, 이동·시점
입력 억제와 cursor/input mode 복구를 담당합니다. Panel과 Slot Widget의 입력·drag/drop·
mutation·Delegate binding은 C++ 책임입니다. WBP는 BindWidget 이름에 맞춘 배치·색상·
폰트·크기와 `SlotWidgetClass` 지정만 소유하며 Graph에서 Inventory 상태를 변경하지 않습니다.

`WBP_AIRECompanionInventoryPanel`의 합성 화면 필수 BindWidget은 `MakoGrid`,
`EquipmentGrid`, `PlayerGrid`, `PlayerEquipmentGrid`, `QuantityPicker`,
`QuantitySpinBox`, `QuantityConfirmButton`, `QuantityCancelButton`, `CloseButton`,
`StatusText`입니다. 기존 `EquipButton`은 사용하지 않으며 Equipment 장착은 Slot drop으로만
요청합니다.

Player와 Shared Storage 사이의 일반 drag는 최신 Source Stack 전체를 이동합니다. `Shift+drag`는
WBP 수량 선택기를 열어 정확한 수량을 입력받습니다. 반대편 Grid에 drop한 경우만 처리하고
목적 Slot은 지정하지 않으며, 기존 Stack 병합 뒤 첫 빈 Slot을 사용하는 자동 배치를 유지합니다.
요청 수량 전체를 수용하지 못하면 양쪽 상태를 모두 보존합니다. 구형 Deposit·Withdraw 버튼과
항상 노출된 수량 입력은 사용하지 않습니다.

MAKO 합성 modal의 MAKO↔Player 일반 Grid drag도 같은 전체 Stack·`Shift+drag` 정확 수량·
자동 Stack 병합·첫 빈 Slot 규칙을 사용합니다. 요청은 Player revision과 MAKO revision,
Session·mutation ID 및 최신 Source Stack을 검증하고, 전량 수용할 수 있을 때만 양쪽을
한 번에 commit합니다. Full·stale revision·invalid Source·중복 mutation에서는 Item을
부분 이동하지 않습니다. Equipment Slot은 일반 Transfer Source 또는 Destination이 아니며,
MAKO 일반 Slot→MAKO Equipment와 Player 일반 Slot→Player Equipment drop만 장착 요청으로
처리합니다. 상대 Inventory의 Weapon을 바로 Equipment Slot으로 drop하는 이동+장착 결합은
지원하지 않습니다.

Storage Panel은 Player `OnInventoryChanged`와 Shared Storage `OnContainerChanged`를,
MAKO 합성 Panel은 MAKO·Player `OnInventoryChanged`와 각 `OnWeaponEquipResult`를 구독합니다. 같은 프레임 신호는
다음 Tick 한 번의 Refresh로 합치며, UI는 mutation 성공 전에 수량을 바꾸거나 실제 배열·
Session·revision을 소유하지 않습니다. Conflict·Session 변경에서는 자동 재시도하지 않고
최신 Snapshot과 실제 Player Inventory를 다시 표시합니다.

합성 Panel은 Item 아이콘·이름·Stack 수량과 Equipment 상태만 표시합니다. Item 상세 카드,
무게 수치·무게 제한·무게 Progress Bar는 구현하거나 표시하지 않습니다.

## 6. Player와 테스트 경계

Player 개인 Inventory와 Quick Slot은 기존 Actor Component가 계속 소유합니다.
Shared Storage Transfer는 일반 Slot만 대상으로 post-state를 먼저 준비하고 Player·Shared Storage를 함께
commit합니다. 기존 `AddItem`의 부분 성공 동작, Quick Slot과 `UseItem`은 바꾸지 않습니다.

Player Inventory revision은 일반 Item 변경, Shared Storage/MAKO 원자 Transfer와 Player
Equipment commit에서 증가합니다. MAKO↔Player 직접 Transfer는 기존 Actor Component의
exact prepare/commit 경계를 사용하며 Player Quick Slot과 장착 Item을 Source로 사용하지
않습니다. 정상 Item은 양쪽 일반 Inventory에 보관할 수 있지만 실제 장착은 각 소유자의
호환 Weapon Data Asset만 허용합니다.

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
예외입니다. MAKO의 일반 자동 예치·인출은 `M03-E08-T05`의 논리 Transfer 정책으로 수행하며,
Shared Storage 접근점 이동이나 `StorageTransfer` WorkOrder를 생성하지 않습니다.

`UAIRECompanionStorageAutomationComponent`는 Config의 순서가 보장된 Storage Rule을
`최대 초과 예치 → 최소 미달 인출` 순서로 평가합니다. Actor나 접근점을 찾지 않고 Inventory
Subsystem의 canonical Shared Storage Container를 직접 사용합니다. 한 평가는 현재 Source Stack 하나만 옮기며, Equipped Item은
보유량에는 포함하지만 Equipped·Pending Item은 Transfer Source로 사용하지 않습니다. 다음 Tick에
병합된 평가가 최신 MAKO와 Shared Storage Snapshot, Session, 양쪽 revision과 Rule을 다시 확인하고
새 mutation ID로 `TryTransferItem`을 호출합니다. Transfer는 MAKO 행동 상태를 점유하지 않고,
전량을 수용할 수 없거나 입력이 stale이면 양쪽 상태를 모두 보존합니다.

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

## 8. AX-I07 로컬 SaveGame 경계

`M03-E08-T03`은 Backend 동기화가 아닌 Player·MAKO·Shared Storage의 로컬 영속성을
다룹니다. SaveGame은 다음 canonical scope를 사용합니다.

- `profile_id=AIRE_OPEN`, `save_slot_id=demo-slot-1`, `companion_id=mako`
- `save_format_version=2`, `item_content_version=1`, `UserIndex=0`을 사용합니다. 콘텐츠
  호환성이 깨질 때 persistence 코드 상수를 수동 증가합니다.
- `local_format_version`과 현재 빌드의 `content_version`을 envelope에 함께 기록하고,
  둘 중 하나라도 지원되지 않으면 해당 세대를 무효로 처리합니다.
- 두 Container의 안정 ID·capacity·revision·Slot/ItemId/Count와 MAKO의 안정
  `EquippedItemId`만 값으로 저장합니다. Actor·ASC·Component·Equipment 포인터와
  Item Definition 경로는 저장하지 않습니다.
- MAKO 일반 Stack은 Player에서 전달된 일반 Item을 포함해 현재 Item catalog에 존재하고
  MaxStack을 만족하면 유효합니다. MAKO Equipment만 MAKO 호환 Weapon Definition을 요구합니다.
- Player는 기존 `UAI_REPlayerInventoryComponent`가 계속 소유하며, 일반 Inventory
  capacity 30, 일반 Slot 0~29, Quick Slot 100~109의 Slot/ItemId/Count, revision과 안정된
  Player `EquippedWeaponItemId`를 같은 envelope 세대에 값으로 저장합니다. 저장된 Player
  값은 비동기 load 검증 뒤 Player Component가 등록될 때 한 번 적용하고 Combat Component에
  runtime weapon restore를 요청합니다.
- Player 필드가 추가된 format 2는 format 1과 호환되지 않습니다. 부분 migration 없이
  format 1 세대를 무효로 처리하고 다른 유효 세대로 fallback하거나 빈 안전 상태를 사용합니다.
- MAKO `Equipping`·`Recovering` transition과 pending/previous/reserved callback 상태는
  직렬화하지 않습니다. `RecoveryFailed`도 transition은 복원하지 않고 마지막 안정
  `EquippedItemId`만 저장하며, active transition 중 dirty 요청은 전환이 끝날 때까지
  새 세대 저장을 미룹니다.

SaveGame은 `AIRE.Inventory.Local.Primary`와 `AIRE.Inventory.Local.Previous` 두 개의 교대
세대 슬롯을 사용합니다. 각 envelope의 단조 증가
`generation`과 payload 검증 결과를 함께 확인하고, 유효한 세대 중 가장 높은 generation을
선택합니다. 최신 세대가 손상·구버전·scope mismatch·content mismatch·검증 실패면 다른
세대로 fallback합니다. 두 슬롯이 모두 존재하지 않을 때만 load 결과 확정 뒤 Config seed를
한 번 적용하며, 이때 AX-I06 첫 제작 검증 재료인 `IronIngot×3`, `WoodHandle×1`도 Shared
Storage에 지급합니다. 이 초기 상태는 새 세대로 저장하고, 유효한 복원본에는 seed를 다시
합치지 않습니다. 하나라도 존재하지만 두 세대가 모두 유효하지 않으면 seed 없이 빈 안전
상태로 준비합니다.

영속 중복 방지 ledger는 mutation result, Work result, import candidate와 import operation을
각각 최대 256개로 제한하고 deterministic eviction을 사용합니다. 저장된 안정 ID는 다음
실행에서도 `AlreadyApplied` 판정에 사용하며, 재전송은 수량·revision·Delegate를 다시
변경하지 않습니다. `WorldDrop`처럼 성공한 보상 ledger에 기록하지 않는 기존 계약은
그대로 유지합니다.

성공한 로컬 mutation·Work·Import·Equipment commit은 dirty 상태를 만들고, 저장 요청은
single-flight로 병합합니다. 진행 중인 저장이 있으면 최신 값만 다음 저장으로 합치며,
실패·취소·종료 중 callback이 현재 또는 새 Session을 되돌리지 않습니다. 저장 실패는
이전 유효 세대를 보존하고 Local Work·Inventory·Combat를 중단시키지 않습니다.

Backend HTTP, Outbox, Offline Settlement와 모바일 상태 조회는 포함하지 않으며, 실제 외부
정산은 후속 계약 Gate 이후 별도 Task에서 다룹니다. SaveGame 자체는 PC 로컬 저장이지만,
Offline Task 동기화가 안전하게 끝난 뒤에는 이 저장 세대의 Player·MAKO·Shared Storage를
`PUT /api/v1/game-state`로 업로드합니다.

## 9. AX-P01 제한 Offline Task 결과 적용 경계

AX-P01은 기존 Task API가 확정한 `PlantStem` Gathering과 `ShoddyBandage` Crafting 결과만
AX-I07 저장 경계에 적용합니다. GameInstance Offline Task Subsystem이 목록과 상태 전이 DTO를
검증하고, Gameplay Inventory Subsystem은 비용·보상과 문자열 `task_id` ledger를 MAKO 우선·
Shared Storage fallback 규칙으로 한 번에 계산한 뒤 전량 성공할 때만 commit합니다.

Web `ShoddyBandage` 제작은 Backend가 먼저 최신 서버 Snapshot의 재료를 예약 차감합니다.
UE의 비용 적용은 제작 가능 여부를 다시 결정하는 권위가 아니라 서버 결정을 local SaveGame에
동일하게 반영하는 단계입니다. 미완료 제작 예약이 하나라도 있으면 Offline Task Subsystem은
차감 전 local Snapshot을 업로드하지 않습니다. 모든 완료 Task의 apply/save/claim이 끝난 뒤
서버의 현재 `state_version`을 GET하고 다음 version을 `X-Base-State-Version`과 함께 PUT합니다.

Offline Task ledger는 최대 256개이며 SaveGame format 2 envelope에 함께 저장됩니다. 같은
Task ID 재적용은 `AlreadyApplied`이며 수량과 revision을 바꾸지 않습니다. 저장 완료 delegate는
실제 비동기 SaveGame 성공/실패 결과를 coordinator에 전달하며, 기존 Task `claim`은 성공한
generation 뒤에만 가능합니다. `Claimed`는 별도 Settlement receipt가 아니라 서버 Task 상태입니다.
이 제한 경계는 범용 AX-I08 Outbox나 AX-I11/I12 전체 Settlement 계약을 대신하지 않습니다.

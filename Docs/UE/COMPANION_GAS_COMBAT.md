# Companion GAS Combat Architecture and Verification

## 1. 문서 목적

이 문서는 MAKO Companion의 현재 GAS 전투 구조, 무기별 Data Asset 소유권,
쌍검 콤보, 로컬 전투·지원 정책과 신규 무기 확장 규칙, 코드 구조 변경 후 필요한
Unreal Editor 설정과 PIE 검증 절차를 정의합니다.

- 관련 Milestone: `M03 Companion Local AI`
- 관련 Task: `M03-E03-T02`, `M03-E03-T03`, `M03-E03-T04`, `M03-E03-T05`,
  `M03-E06-T01`, `M03-E06-T02`, `M03-E09-T01`, `M03-E09-T02`
- 코드 기준일: 2026-08-10
- 현재 검증 상태: T05A 기본 공격 콤보와 T05B 전투 스킬 검증 완료.
  T05C 인벤토리·무기 장착·소모품 회복 구현과 현재 자산으로 가능한 UBT·Editor·PIE
  검증 완료. M03-E08-T01 Gameplay Inventory Subsystem은 Review이며 사용자 UBT·Editor·PIE,
  실제 플레이어 GAS Health 연동과 다중 무기 검증 대기. M03-E09-T02B MAKO 실제 무기
  Trace·자율 회피 source와 project-owned 자산 연결은 완료했고 사용자 PIE 스모크에서
  자율 회피를 확인했습니다. 마지막 표현·디버그 source 변경 뒤 재빌드, 신규 자동화,
  공격 범위 envelope 튜닝과 상세 PIE/lifecycle 검증이 남아 `Review`입니다.

Player·MAKO·Enemy가 공유하는 피해·스태거 실행과 Boss/Q 어그로 스왑 계약은
[`COMBAT_DAMAGE_STAGGER_CONTRACT.md`](COMBAT_DAMAGE_STAGGER_CONTRACT.md)를 따릅니다.

## 2. 현재 코드 구조

```text
UEProject/Source/AI_RE/LMK/MAKO/
├─ Public/
│  ├─ Core/
│  ├─ LocalAI/
│  │  ├─ Policy/
│  │  ├─ StateTree/
│  │  ├─ Support/
│  │  ├─ Threat/
│  │  └─ UI/
│  ├─ AbilitySystem/
│  │  ├─ Core/
│  │  │  └─ Attributes/
│  │  ├─ Combat/
│  │  │  ├─ Abilities/
│  │  │  └─ Effects/
│  │  └─ Support/
│  │     ├─ Abilities/
│  │     └─ Effects/
│  ├─ Inventory/
│  ├─ Equipment/
│  ├─ Animation/
│  └─ Testing/
├─ Private/
│  └─ Public과 동일한 기능 구조
└─ Components/
   ├─ Public/
   │  ├─ Equipment/
   │  ├─ Inventory/
   │  ├─ Policy/
   │  ├─ Support/
   │  └─ Threat/
   └─ Private/
      ├─ Equipment/
      ├─ Inventory/
      ├─ Policy/
      ├─ Support/
      └─ Threat/
```

### 2.1 책임 경계

| 영역 | 책임 |
|---|---|
| Companion Character | ASC·AttributeSet·기능 컴포넌트 조립과 생명주기 |
| StateTree | 행동 우선순위, Target 접근, 공격 요청과 취소 |
| Threat Component | 적대 Target 감지·선택·소실 정리 |
| Local Behavior Policy Component | 지속 교전 정책과 역할 선호의 단일 런타임 원본, 변경 Delegate |
| Gameplay Inventory Subsystem | GameInstance 수명의 MAKO Item 20칸·Equipment 1칸, 공유 창고와 원자적 mutation |
| Inventory Component | Gameplay Inventory façade와 Equipment/ASC 비동기 Callback 수명 |
| Equipment Component | 현재 무기, 비동기 자산, Ability Handle과 Anim Layer 수명 |
| Support Component | 명시적 지원 요청, 회복 Target과 지원 AbilitySet 수명 |
| Gameplay Ability | 공격·회복 검증, Cooldown Commit, Montage·Timer와 GE 실행 |
| Shared Combat Damage | Target 선언형 Health·선택적 스태거 Attribute 검증과 exact-once 피해 실행 |
| Gameplay Effect | 공격별 Cooldown과 회복, 공용 Combat Damage Execution |
| AttributeSet | Health·MaxHealth·Stamina·MaxStamina |
| Companion Item Definition | Consumable 회복 데이터 또는 Weapon Definition 참조 |
| Weapon Definition | 무기별 전투 수치, AbilitySet과 공격 표현 |
| AbilitySet | 무기가 부여할 Ability 클래스와 Level |

별도 `CombatManager` 또는 전지적인 `CombatComponent`는 두지 않습니다. StateTree,
Equipment와 GAS가 이미 각자의 상태와 취소 경계를 소유하므로 이를 한 객체에 다시
복제하지 않습니다.

## 3. GAS 실행 흐름

```text
Companion Character BeginPlay
→ ASC ActorInfo 초기화
→ Attribute 기본값 초기화
→ Equipment가 Weapon Definition의 Soft Reference 비동기 로드
→ AbilitySet의 각 Ability로 FGameplayAbilitySpec 생성
→ Weapon Definition을 AbilitySpec SourceObject로 연결
→ Ability 역할에 맞는 CooldownDuration을 SetByCaller 값으로 등록
→ GiveAbility
→ StateTree가 Event.Companion.Attack.Request 전송
→ Primary Attack Ability 활성화
→ Target·거리·Cooldown 검증 후 Target 방향 정렬
→ 기본 공격 Cooldown GE를 실행당 한 번 적용
→ Montage Hit·Combo Window Notify 또는 시간 기반 fallback으로 Step Event 전송
→ 다음 Step 직전 Target·거리 재검증 후 Target 방향 재정렬
→ 공용 Combat Damage Request를 Target ASC에 exact-once 적용
→ Health가 0이면 State.Companion.Disabled.Dead 적용
→ Threat 해제, Ability 취소, Return/Follow 복귀
```

### 3.1 StateTree와 GAS 경계

StateTree는 Health나 Stamina를 직접 변경하지 않습니다. StateTree는 현재
Weapon Definition의 `AttackRange`를 사용해 접근하고 Gameplay Event로 공격 의도만
전달합니다.

Ability는 자신의 AbilitySpec `SourceObject`에서 Weapon Definition을 가져옵니다.
공격 거리는 Event의 임의 숫자를 신뢰하지 않고 Weapon Definition의 `AttackRange`를
다시 읽어 최종 검증합니다.

실제 공격 제한은 GAS Cooldown이 소유합니다. StateTree는 해당 Cooldown Tag가 활성화된
동안 Gameplay Event를 다시 보내지 않으며, 짧은 재검사 간격은 Cooldown의 두 번째 원본이
아닙니다. 이동 재시도와 Ability 활성화 재검사는 서로 다른 타이머를 사용합니다.

## 4. Data Asset 계약

### 4.1 Companion Config

Companion Config가 소유하는 값은 다음과 같습니다.

- 이동 속도와 Walk/Run 전환 거리
- Follow/Return 거리
- Threat 감지 거리, 선택 Target의 추가 소실 거리·시야 기억 시간, 플레이어 방어 반경과 최대 추격 거리
- 기본 교전 정책과 역할 선호
- 초기 Health·Stamina와 Max 값

기존 `CombatDistance`와 `CombatCooldown`은 기존 StateTree 자산 호환을 위해
deprecated 프로퍼티로 남아 있지만 런타임에서는 사용하지 않습니다.

### 4.2 Weapon Definition

각 무기 Data Asset은 다음 값을 소유합니다.

| 필드 | 의미 |
|---|---|
| `WeaponTag` | 표시 이름과 무관한 안정적인 무기 정체성 |
| `AbilitySet` | 장착 시 부여할 Ability 목록 |
| `AttackMontage` | Primary Attack 표현 |
| `ComboSteps` | 선택적 Montage Section·Damage 순서; 빈 배열은 기존 단발 계약 |
| `LinkedAnimLayerClass` | 무기별 Linked Anim Layer |
| `Damage` | 기본 공격 피해 |
| `StaggerValue` | 기본 공격이 적 반응 Gauge에 누적하는 값 |
| `StaminaCost` | 기존 `.uasset` 호환용 deprecated 필드; 런타임에서 사용하지 않음 |
| `AttackRange` | 충돌 반경을 제외한 유효 표면 거리 |
| `CooldownDuration` | GAS Cooldown 지속 시간 |
| `FallbackHitDelay` | Montage가 없을 때 Hit 발생 지연 |
| `FallbackRecoveryDuration` | fallback 공격 종료 지연 |
| `LeftTraceSockets` / `RightTraceSockets` | 좌·우 칼날의 project-owned base/tip socket pair |
| `TraceCapsuleRadius` / `TraceCapsuleHalfHeight` / `TraceChannel` | MAKO 무기 캡슐의 반경·반높이와 collision channel |
| `ComboSteps.TraceSide` / `TraceSocketOverride` | 단계별 선행 칼날과 선택적 socket pair override |
| `HarvestAttackRange` | Combat sweep과 분리된 직접 채집 판정 거리 |
| `CombatSkill` | 선택적 스킬 활성화·Montage·Damage·Cooldown·Range·선택 확률·fallback 시간 |

무기마다 별도 C++ Data Asset 클래스를 만들지 않습니다. 동일한
`UAIRECompanionWeaponDefinitionDataAsset` 클래스의 자산 인스턴스로 차이를 표현합니다.

```text
DA_AIRE_Weapon_DualSword
DA_AIRE_Weapon_GreatSword
DA_AIRE_Weapon_Bow
```

### 4.3 AbilitySet

AbilitySet 항목은 `AbilityClass`와 `AbilityLevel`을 가집니다.

검증 규칙:

- 빈 AbilitySet 금지
- AbilityClass 누락 금지
- Level 1 미만 금지
- `UAIRECompanionGameplayAbility` 비파생 클래스 금지
- 동일 Ability 클래스 중복 금지

한 Weapon AbilitySet에는 기본 공격 역할 Ability와 전투 스킬 역할 Ability를 각각
최대 하나만 둡니다. 두 Ability는 독립 Request Tag를 사용합니다.

## 5. 현재 기본 근접 공격

`UAIRECompanionMeleeAttackAbility`는 다음을 소유합니다.

- Disabled 상태 차단
- Weapon Definition과 근접 무기 검증
- 적대·생존 Target 검증
- 공격 거리 검증과 Target 방향 정렬
- 실행당 Cooldown Commit, 공격 Stamina 무비용
- Montage 재생 또는 fallback
- Hit Event 한 번 소비
- 공용 Damage/Stagger Request 적용
- 취소, Target 소실과 늦은 Notify 무효화

Equipment가 무기 해제·교체·사망 시 부여된 Ability Handle을 취소하고 회수합니다.

## 6. 기본 공격 콤보

콤보 단계는 StateTree가 아니라 하나의 Combo Ability가 소유합니다.

```text
StateTree Attack.Request 1회
→ Melee Combo Ability
→ Combo Step 1
→ Combo Window
→ Combo Step 2
→ Combo Window
→ Finisher
→ Ability 종료
```

별도 AttackSet 자산을 추가하지 않고 Weapon Definition 내부의
`FAIREWeaponComboStepDefinition` 배열을 사용합니다.

```text
MontageSection
Damage
StaggerValue
```

Combo Window는 초 단위 Data Asset 값보다 Montage의 Anim Notify State로 정의합니다.
Ability는 현재 Step, Hit 소비 여부, Combo Window와 다음 Step 예약 상태를 소유합니다.
Hit Notify와 Combo Window Notify State는 Weapon Definition 배열과 일치하는 0-based
`ComboStepIndex`를 Gameplay Event의 `EventMagnitude`로 전달합니다. Ability는 현재
실행과 Step이 일치하지 않는 늦은 Event를 무시합니다.

Montage Section이 끝나 Blend Out을 예약하기 전에 Ability는 현재 Section의
`NextSection`을 다음 Combo Section으로 미리 연결합니다. Combo Window End에서 다음
Step의 실행 조건을 다시 검증한 뒤 Ability의 현재 Step을 전환하고, 이어지는 Section의
다음 연결을 준비합니다. 이 계약은 Section 종료 뒤 `MontageJumpToSection`을 호출해
1타가 반복되거나 전환 모션이 끊기는 경로를 사용하지 않습니다.

AI Companion은 다음 조건을 만족할 때 다음 Step으로 자동 진행할 수 있습니다.

- Target이 생존하고 적대 상태를 유지함
- Target이 현재 공격 거리 안에 있음
- StateTree 전투 상태가 여전히 유효함
- 무기 교체·사망·전투 취소가 발생하지 않음

기본 공격은 Stamina를 검사하거나 차감하지 않습니다. 기본 공격 Cooldown은 Combo 실행
시작 시 한 번만 적용합니다. 다음 Step의 Target 조건이 실패하면 현재 Ability를
안전하게 종료합니다.

Montage가 없거나 로드되지 않으면 기존 `FallbackHitDelay`와
`FallbackRecoveryDuration`을 Step마다 재사용해 제한된 자동 Combo 수명주기를
실행합니다. `ComboSteps`가 비어 있으면 기존 단발 Montage와 fallback이 유지됩니다.

### 6.1 Hit 판정과 후속 Trace 경계

Anim Notify는 실제 명중을 확정하지 않고 Hit 판정을 시도할 타이밍만 전달합니다.
현재 T05A는 선택된 Threat Target의 생존·적대성·거리를 Ability에서 검증한 뒤
공용 Combat Damage Subsystem에 Request를 전달합니다. 기본 근접 Weapon의 현재 표면
공격 거리는 `150 cm`입니다.

2026-08-06 이동하는 Boss 대상 PIE에서 이 임시 경계가 Move/Attack 진동과 실제 궤적을
확인하지 않는 피해 판정을 만든다는 결함이 확인됐습니다. M03-E09-T02A는 Boss에서 실제
근접 공간 판정 경계를 먼저 확립합니다. 이후 사용자 승인으로 MAKO 이동 표적 안정화
source만 먼저 보완했습니다. Threat는 선택 Target에 `200 cm` 소실 거리와 `3.0 s` 시야
기억을 적용합니다. 플레이어와 함께 싸우는 Companion은 감지 거리와 시야 차폐를
유지하되 방향과 무관하게 주변 적을 최초 획득합니다. StateTree는 진행 중인 공격을
단순 거리 이탈만으로 취소하지 않습니다.
현재 Step의 hit 거리와 다음 Combo Step 진입 거리는 Ability가 계속 검증하므로, 이탈한
공격은 피해 없이 현재 표현을 마친 뒤 다음 연계를 종료합니다. 완료된 고정 지점 접근이
여전히 사거리 밖이면 `0.5 s` 판단 간격 뒤 새 접근을 요청합니다.

M03-E09-T02B source는 Boss와 MAKO가 공간 판정만 공유하는
`FAIRECombatMeleeTraceResolver`를 채택했습니다. 기본 공격의 각 Combo Step과 Combat Skill
활성화는 Target, `ExecutionId`, Damage, Stagger, targeting mode, trace radius/channel과
좌·우 socket pair를 snapshot합니다. NotifyState의 Begin/Tick/End는 무기 base socket에서
칼날 축과 중앙에 맞춘 `반경 35 cm / 반높이 160 cm` 캡슐을 프레임 사이 6개 substep으로
sweep합니다. Boss의 요청은 기존 sphere shape를 명시적으로 유지합니다. `NoHit`과
`Blocked`는 다음 sample을 허용하고, `Invalid`·`TargetHit`만 strike를 종료합니다. `TargetHit`만 `FHitResult`를 포함한
공용 Damage Request를 commit합니다.

기본 socket 계약은 `weapon_l/r`에서 `weapon_trace_tip_l/r`까지이며, Combo Step과 Skill이
선행 칼날 또는 완전한 socket pair override를 선택합니다. Socket 누락은 explicit invalid
miss와 validation 오류이며 거리 기반 combat hit로 대체하지 않습니다. 기존 point Notify와
Montage가 없는 시간 fallback은 one-shot spatial sample로 유지합니다. Harvest만 직접 채집
hit를 유지하고 활성화와 판정 시점 모두 별도 `75 cm` 표면 거리를 검증합니다. 현재 진행 중인
Combat strike는 Threat 변경이나 사거리 이탈로 snapshot target을 교체하거나 취소하지 않습니다.

이때도 StateTree, Combo Step, 단계별 Damage, 실행당 Cooldown과 `ExecutionId` exact-once
계약은 유지합니다. 공격 진입 거리와 더 넓은 취소 거리를 분리해 작은 Target 이동으로
MoveTo와 Attack이 반복 취소되지 않게 하며, 판정 frame에 Shape를 벗어난 공격은 한 번의
miss recovery로 종료합니다. 기본 공격은 현재 선택 Target 하나만 단계당 한 번 피해를
받도록 제한합니다.

같은 Combo 실행 규칙을 사용하는 무기는 Combo Ability를 재사용하고 Weapon Definition의
Montage Section과 Step 수치만 다르게 구성합니다. 두 번째 이상의 시스템에서 Combo Step
구성을 공유해야 할 때만 별도 AttackSet Data Asset 추출을 검토합니다.

### 6.2 전투 스킬 삽입 계약

`UAIRECompanionCombatSkillAbility`는 기본 공격과 독립된 Request·Hit·Started·Ended와
Cooldown Tag를 사용합니다. Weapon Definition의 `CombatSkill`은 활성화 여부, 선택적
Montage, Damage, Cooldown, Range, SelectionChance와 fallback 시간만 소유하며
Stamina 비용은 소유하지 않습니다.

StateTree는 기본 공격 전, 각 Combo Window와 콤보 종료 후에 `SelectionChance`를 한 번만
평가해 스킬 의도를 보냅니다. 비취소 구간에 선택된 의도는 하나만 버퍼링하고 가장 가까운
Window에서 처리합니다. 스킬 활성화가 Cooldown·Target·상태 검증으로 실패하면 콤보 중에는
현재 기본 공격을 유지하고, 기본 공격 전에는 기본 공격 Request로 fallback합니다.

```text
기본1 → 기본2 → 스킬 → 기본3 → 기본4
기본1 → 스킬 → 기본2 → 기본3 → 기본4
스킬 → 기본1 → 기본2 → 기본3 → 기본4
기본1 → 기본2 → 기본3 → 기본4 → 스킬
```

스킬이 Commit에 성공한 뒤에만 `Skill Started`를 보내며, 기본 공격 Ability는 종료하지
않고 다음 Step을 보존합니다. 스킬이 정상 종료해 `Skill Ended`를 보내면 보존한 Step부터
같은 기본 공격 실행을 재개하므로 기본 공격 Cooldown을 다시 적용하지 않습니다.

스킬 Hit는 실행당 최대 한 번만 공용 Damage/Stagger Request로 적용합니다. Montage가 없으면 스킬
전용 fallback Hit·Recovery Timer를 사용합니다. 취소·Unequip·Disabled·사망·Target
파괴 시 두 Ability의 Montage Task, Event Task, Timer, Window Tag, 버퍼와 transient
참조를 정리하고 늦은 Event를 무시합니다.

지상 기본 공격과 전투 스킬은 Stamina를 소비하지 않습니다. T02B 자율 회피만 `25`를
소비하며, 성공한 dash 뒤 `1.5 s` 동안 회복을 막고 이후 `15/s`로 최대 `100`까지
회복합니다. Q AggroSwap 회피는 무료입니다.

### 6.3 자율 회피 계약

StateTree의 Engage Threat Task만 자율 회피 여부를 결정합니다. 현재 선택 Threat에서
MAKO를 겨냥한 열린 `SingleTarget` 공격 snapshot을 읽고, 각 `ExecutionId`를 한 번만
GUID-seeded deterministic stream으로 평가합니다. 기본 선택 확률은 `50%`, 선택된 반응
지연은 `0.15-0.28 s`입니다. 지연 종료 시 동일 opportunity, Stamina, `5 s` cooldown과
`100 cm` 최소 lateral clearance를 다시 검증합니다. 실패·만료·차단은 재시도하지 않으며
회피 중 새 요청은 queue하지 않습니다.

`UAIRECompanionAutonomousEvadeAbility`는 현재 melee 장착과 snapshot을 다시 확인한 뒤
공격/스킬 Ability를 취소하고 준비된 계획을 실행합니다. 실제 이동은
`UAIRECombatEvadeComponent`만 소유합니다. Character capsule을 좌우 `300 cm` sweep해 더
넓은 방향을 고르고 동률이면 오른쪽을 선택한 뒤, 선택한 실제 거리만 `0.25 s` 동안 swept
movement로 이동합니다. `Evade_L/R` Montage section은 in-place 표현만 담당하며 Root
Motion이 있으면 재생하지 않습니다. Montage interrupt만으로 code-driven dash를 중단하지
않습니다. 현재 `MK_AM_Evade`는 각 section을 `2.0` 배속으로 `0.375 s` 표현하고, 실제
capsule dash는 계속 `0.25 s`입니다. 정상 dash 종료는 Montage를 자르지 않지만 충돌,
명시적 cancel, death, unequip, unpossess와 EndPlay는 즉시 표현을 중단합니다.

dash 시작 성공 뒤에만 Stamina/cooldown/regen-block을 확정합니다. `+0.05 s`부터 최대
`0.12 s` 동안 `State.Combat.Invulnerable`을 부여하며 조기 충돌·cancel·death에서는 즉시
제거합니다. Montage 재생 길이는 이 고정 `+0.05-0.17 s` window를 변경하지 않습니다.
이 시간에 닿은 Damage/Stagger 요청은 `TargetInvulnerable`로 terminal 기록되어
무적 종료 뒤 같은 `(ExecutionId, Target)` duplicate가 피해를 적용할 수 없습니다. Q가
동일 Threat와 ExecutionId의 자율 dash와 겹치면 이동을 재시작하지 않고 현재 dash를
재사용하며, strike cancel·aggro promotion·Q cooldown만 수행합니다. 다른 Execution이면
거부합니다.

cancel, target destruction, death, unpossess, unequip과 EndPlay는 pending decision,
timer, delegate, transient tag/effect와 active movement context를 정리합니다.

### 6.4 MAKO 인벤토리·지원 회복 계약

MAKO 일반 인벤토리는 `UAIREGameplayInventorySubsystem`의 20칸 Container이며 이번
범위에서는 `Consumable`과 `Weapon`만 소유합니다. 장착 무기는 일반 Stack과 분리된
Equipment 1칸에 저장합니다. 존재하지 않거나 Weapon이 아닌 Item, 공격 중 교체 요청은
거부합니다. 새 무기 Slot은 비동기 장착 동안 잠그고, 성공한 뒤에만 이전 Equipment와
원자 교환합니다. 새 무기 로드 또는 AbilitySet 부여가 실패하면 Item 배치를 유지한 채
이전 무기와 Ability Handle을 복구합니다. 상세 mutation·revision·공유 창고 계약은
[`GAMEPLAY_INVENTORY.md`](GAMEPLAY_INVENTORY.md)를 따릅니다.

회복은 마법이 아니라 MAKO가 자기 인벤토리의 응급 회복 앰플을 아군에게 사용하는
행동입니다.

```text
RequestSupport(Target)
→ 회복 대상 계약·ASC·앰플 보유 검증
→ StateTree가 Support 거리까지 접근
→ 회복 Ability가 처치 Timer 시작
→ Target·아이템 재검증
→ Healing GE Spec 준비
→ Cooldown Commit
→ 앰플 1개 소비
→ min(회복량, MaxHealth - Health)만 GE로 적용
→ 요청 완료
```

State Exit, Combat 선점, Target 파괴 또는 MAKO 사망으로 처치가 취소되면 Cooldown,
소비와 회복이 발생하지 않습니다. GE 적용이 예상 밖으로 실패하면 소비한 Item을
복구하고 실패를 기록합니다. 자동 Health 임계치 감지와 자동 소비는 이 Task에 포함하지
않으며 `RequestSupport(Target)` 호출만 지원합니다.

기존 `DefaultWeaponDefinition`은 초기 인벤토리가 없는 기존 자산만을 위한 deprecated
fallback입니다. 신규 Companion Config는 초기 무기 Item과
`DefaultEquippedWeaponItemId`를 사용합니다.

## 7. 신규 무기 추가

### 7.1 동일한 단발 근접 무기

다음 자산만 추가합니다.

1. 구체적인 `Weapon.Companion.Melee.*` Gameplay Tag
2. Weapon Definition Data Asset
3. AbilitySet Data Asset 생성 또는 기존 AbilitySet 재사용
4. Attack Montage
5. Linked Anim Layer

기존 기본 근접 Ability를 재사용하면 Character와 StateTree C++ 수정이 필요하지 않습니다.

### 7.2 다른 콤보 근접 무기

동일 Combo Ability를 사용하고 Weapon Definition의 Combo Step 데이터와 Montage만
교체합니다. 실행 규칙이 실제로 다를 때만 별도 Ability 클래스를 만듭니다.

### 7.3 원거리 무기

원거리 Weapon Definition과 Ranged Attack Ability를 추가합니다. 기본적인 접근은
`AttackRange`를 재사용할 수 있지만 다음 요구가 생기면 별도 이동 정책이 필요합니다.

- 최소·선호 공격 거리
- Line of Sight
- 후퇴와 거리 벌리기
- Projectile 또는 Hitscan
- 탄약과 재장전

원거리 확장 전에는 StateTree가 특정 근접 Ability를 찾지 않도록
`Ability.Companion.Combat.PrimaryAttack` 같은 공통 태그를 도입하고, 근접·원거리
세부 태그를 하위 분류로 두는 것을 권장합니다.

## 8. 이번 구조 변경 후 Editor 설정

### 8.1 필수 설정

#### `DA_MAKO_Weapon_BasicMelee`

기존 Companion Config에 커스텀 전투 값이 있었다면 Weapon Definition으로 복사합니다.

기본 기준:

```text
AttackRange = 150 cm
CooldownDuration = 1.5 s
Damage = 25
CombatSkill.bEnabled = true
CombatSkill.Damage = 45
CombatSkill.CooldownDuration = 4 s
CombatSkill.SelectionChance = 0.45
```

다음 참조도 유지해야 합니다.

- `AbilitySet = DA_MAKO_AbilitySet_BasicMelee`
- 승인된 `AttackMontage`
- 승인된 `LinkedAnimLayerClass`

변경 후 Data Asset을 Save합니다.

#### `DA_MAKO_AbilitySet_BasicMelee`

- `UAIRECompanionMeleeAttackAbility`가 존재하는지 확인합니다.
- `UAIRECompanionCombatSkillAbility`가 존재하는지 확인합니다.
- 기본 공격 역할과 전투 스킬 역할 Ability가 각각 하나 이하인지 확인합니다.
- Ability Level이 1 이상인지 확인합니다.

#### `BP_MAKO`

- Equipment Component의 `DefaultWeaponDefinition`이
  `DA_MAKO_Weapon_BasicMelee`를 가리키는지 확인합니다.
- Character·ASC·AttributeSet 클래스나 컴포넌트 추가 설정은 필요하지 않습니다.

#### `ST_AIRECompanion_Local`

`EngageThreat` Task의 필수 Binding:

- Companion Controller
- Equipment Component
- Ability System Component
- Threat Target

기존 `CombatDistance`와 `CombatCooldown` Binding은 제거합니다. 프로퍼티는 기존 자산을
안전하게 열기 위한 deprecated 호환 필드일 뿐 런타임에서 사용되지 않습니다.

StateTree를 Compile한 뒤 Save합니다.

### 8.2 변경하지 않아도 되는 설정

- StateTree 상태 순서와 전역 Transition
- Threat Component Sight 설정
- Companion Config의 이동·Threat·초기 Attribute 값
- Base Anim Blueprint와 Linked Anim Layer 연결 방식
- Combat Target fixture의 ASC 설정

#### 추적·귀환 안정화 계약

- Follow와 Return 판단은 Character capsule의 표면 거리를 사용합니다.
- Follow는 `FollowStopDistance=200 cm`에서 evaluator가 이동을 종료합니다. Follow MoveTo의
  성공 반경은 이 값에 binding하지 않아 같은 frame의 완료·재진입 loop를 만들지 않습니다.
- Return은 `ReturnStartDistance=600 cm`에서 latch하고 `ReturnStopDistance=400 cm`에서
  해제합니다. Follow·Return의 판단 소유자는 StateTree evaluator 하나입니다.
- 이동 재시도와 Ability 재시도 timer는 분리하며, 해당 GAS cooldown tag가 활성인 동안
  StateTree는 같은 공격 Gameplay Event를 반복 발행하지 않습니다.

### 8.3 기본 공격 콤보 설정

1. `AttackMontage` 하나에 실행 순서대로 Montage Section을 만듭니다.
2. Weapon Definition의 `ComboSteps`에 같은 순서로 Section 이름과 Damage를 입력합니다.
3. MAKO project-owned Skeleton에 `weapon_trace_tip_l/r`을 보이는 칼날 끝에 추가하고,
   Weapon Definition의 기본 좌·우 socket pair, `TraceSide` 또는 override를 실제 선행
   칼날과 맞춥니다.
4. 각 Section의 유효 칼날 구간에 `AIRE Companion Melee Trace Window` NotifyState를
   배치하고 Mode `BasicAttack`, `ComboStepIndex`를 배열의 0-based Index와 맞춥니다.
   기존 `AIRE Companion Attack Hit`은 호환 one-shot sample로만 유지합니다.
5. 다음 단계로 전환할 Section에는 `AIRE Companion Combo Window` Notify State를
   배치하고 같은 `ComboStepIndex`를 입력합니다. Notify State의 End 위치가 실제 다음
   Section으로 전환할 지점입니다.
6. 마지막 Step에는 다음 단계가 없으므로 Combo Window가 필요하지 않습니다.
7. Data Validation 후 Weapon Definition, Montage와 관련 Anim Blueprint를
   Compile·Save합니다.

Notify는 Trace 명중을 확정하지 않습니다. NotifyState와 시간 fallback은 같은 공용 공간
resolver를 호출하고, 같은 단계의 `ExecutionId`는 snapshotted Target에 terminal 결과를
최대 한 번만 commit합니다. Trace window의 첫 miss는 window를 소비하지 않습니다.

### 8.4 전투 스킬 설정

1. Weapon Definition의 `CombatSkill.bEnabled`를 켜고 Damage 45, Cooldown 4초,
   Range와 SelectionChance를 설정합니다.
2. `DA_MAKO_AbilitySet_BasicMelee`에 `UAIRECompanionCombatSkillAbility`를 추가합니다.
3. 스킬 Montage의 유효 칼날 구간에 `AIRE Companion Melee Trace Window` NotifyState를
   Mode `CombatSkill`로 배치하고 Weapon Definition의 `TraceSide`/override를 선행
   칼날과 맞춥니다. 기존 Combat Skill Hit Notify는 호환 one-shot sample입니다.
   Montage가 없으면 spatial fallback 시간이 사용됩니다.
4. 선택 경로를 고정해 검증할 때 SelectionChance를 1.0 또는 0.0으로 설정하고, 검증 후
   승인값 0.45로 복원합니다.
5. Weapon Definition, Ability Set, Montage, 관련 Anim Blueprint와 StateTree를
   Compile·Save합니다.

### 8.5 자율 회피 설정

1. Companion Config의 `AutonomousEvade`가 `0.5`, `0.15-0.28 s`, `5 s`, `100 cm`,
   Stamina `25`, regen delay/rate `1.5 s`/`15`, invulnerability `+0.05 s`/`0.12 s`인지
   확인합니다.
2. `MK_AM_Evade`에 in-place `Evade_L/R` section을 만들고 Root Motion을 비활성화한 뒤
   `UAIRECombatEvadeComponent`의 Evade Montage에 연결합니다.
3. Weapon Definition의 Linked Anim Layer는 검증 대상 `MK_ABP_MAKO_DualLayers`로
   연결하고 공격·스킬·Harvest·회피 전체를 회귀 검증합니다.
4. project-owned 자산만 Compile·Save하고 vendor animation/Skeleton은 수정하지 않습니다.

2026-08-10 Editor MCP readback/save 기준으로 실제 편집 대상은
`/Game/Work/LMK/Blueprints/AI/BP_MAKO`이며 `/AI/MAKO/BP_MAKO`는 구형 redirector입니다.
`SM_MAKO`의 `weapon_trace_tip_l/r`, `MK_AM_Combo01_Montage`의 네 Basic trace window,
`MK_AM_ChargeAttack`의 Combat Skill trace window, `DA_MAKO_Weapon_BasicMelee`의
`MK_ABP_MAKO_DualLayers`, `MK_AM_Evade`의 in-place `Evade_L/R`, 그리고 `BP_MAKO`의
Evade Montage 할당은 project-owned 자산에 개별 저장했습니다. 사용자는 PIE 스모크에서
자율 회피 동작을 확인했지만, 이후 추가된 presentation lifetime 분리와 진단 로그 source는
재빌드 전이며 공격 시작 거리와 새 `35 × 160 cm` capsule envelope의 최종 승인이 남았습니다.

PIE에서 `aire.Combat.MeleeTrace.Debug 1`을 사용하면 공용 trace가 cyan `NoHit`, green
`TargetHit`, red `Blocked`로 표시됩니다. Output Log는 `[MAKO ATTACK]`, `[MAKO HEALTH]`,
`[MAKO EVADE]`, `[ENEMY ATTACK]`을 필터링해 strike/ExecutionId, Health, 무적 ON/OFF와
`TargetInvulnerable` 결과를 확인합니다. 검증 뒤 debug cvar는 `0`으로 복원합니다.

### 8.6 인벤토리·지원 회복 설정

다음 자산을 생성하고 저장합니다.

1. `DA_AIRE_Item_BasicDualSword`
   - Item Type: `Weapon`
   - Max Stack: `1`
   - Weapon Definition: 승인된 기본 쌍검 Weapon Definition
2. `DA_AIRE_Item_EmergencyHealingAmpoule`
   - Item Type: `Consumable`
   - Healing Amount: `25`
   - Treatment Duration: `0.5`
   - Support Range: `200`
   - Cooldown Duration: `5`
3. `DA_AIRE_AbilitySet_Support`
   - `UAIRECompanionUseHealingItemAbility` 하나만 등록
4. Companion Config
   - 초기 인벤토리: 응급 회복 앰플 3개, 기본 쌍검 1개
   - 기본 장착 무기 ItemId: 기본 쌍검 ItemId
   - 기본 회복 소모품 ItemId: 응급 회복 앰플 ItemId
   - Support Ability Set: `DA_AIRE_AbilitySet_Support`

`ST_AIRECompanion_Local`에는 Combat 다음, DirectCommand 이전 우선순위로 Support
상태를 추가합니다. Evaluator의 Inventory Component, Support Component,
Support Target과 `bIsSupportRequested`를 `Engage Support` Task에 Binding하고
StateTree를 Compile·Save합니다. 전체 우선순위는 다음과 같습니다.

```text
Disabled > Survival > Combat > Support > DirectCommand
> Work > Return/Follow > Idle
```

회복용 ASC Target fixture는 `Health=50`, `MaxHealth=100`, `bIsHostile=false`로
설정합니다.

### 8.6 로컬 전투·지원 정책과 데모 패널

정책의 런타임 단일 원본은 Character의
`UAIRECompanionLocalBehaviorPolicyComponent`입니다. 기본값은 Companion Config의
`Aggressive + Balanced`이며, 같은 Character를 다시 Possess하면 현재 값을 유지하고
새 Character는 Config 기본값으로 시작합니다. 정책은 StateTree State나 Gameplay Tag로
복제하지 않으며 SaveGame·Replication·Backend 동기화도 하지 않습니다.

교전 축은 다음 규칙을 사용합니다.

| 교전 정책 | Combat 요청과 Target 허용 규칙 |
|---|---|
| `HoldFire` | 신규 Combat 요청을 억제하고 현재 Target을 해제합니다. 인식 중인 유효 적대 목록은 유지하므로 다른 정책으로 복귀하면 즉시 재선택할 수 있습니다. |
| `DefendPlayer` | 감지 거리 안이면서 플레이어로부터 `DefendPlayerRadius` 이내인 Target만 허용합니다. 기본 반경은 600cm입니다. |
| `Aggressive` | 감지 거리 안이면서 플레이어로부터 `MaxChaseDistanceFromPlayer` 이내인 Target을 허용합니다. 기본 상한은 1500cm입니다. |

거리 경계는 포함하며 Target 순위는 기존과 같이 MAKO에서 가장 가까운 유효 Target을
우선합니다. Threat Component는 정책 변경 Delegate에서 현재 Target을 동기적으로
재검증합니다. `HoldFire` 전환 뒤 Ability의 늦은 Hit 검증도
`IsCombatRequested()` 실패로 차단됩니다.

역할 선호는 유효한 Combat 요청과 일회성 `RequestSupport(Target)` 요청이 동시에 있을
때만 StateTree Evaluator가 적용합니다.

```text
Balanced:
Disabled > Survival > Combat > Support > DirectCommand > Work > Return/Follow > Idle

SupportPriority:
Disabled > Survival > Support > Combat > DirectCommand > Work > Return/Follow > Idle
```

데모 입력은 독립 Policy HUD가 `P` 키의 Press·Release를 사용합니다. `P`를 누르고 있는
동안 이동·시점 입력을 억제하고 마우스 커서를 중앙에 배치하며, 방향을 고른 뒤 `P`를
놓는 시점에 선택한 축 하나만 적용합니다. 중앙 데드존에서 놓으면 취소되며, Release를
잃어 패널이 남은 경우 다음 `P` Press가 취소·복구합니다. 기존 숫자 퀵슬롯과 Chat HUD
입력은 변경하지 않습니다.

원형 UI는 두 독립 축을 한 화면에 배치합니다.

| 반원 | 방향 | 변경 값 |
|---|---|---|
| 위쪽 | 좌 / 우 | `Balanced` / `SupportPriority` |
| 아래쪽 | 좌 / 중앙 / 우 | `HoldFire` / `DefendPlayer` / `Aggressive` |

한 번의 제스처는 선택한 축만 바꾸고 다른 축은 현재 값을 보존하므로 `3 × 2`의 모든
조합을 최대 두 번의 입력으로 만들 수 있습니다. 현재 교전 값과 역할 값은 각 반원의
활성 섹터로 동시에 표시하고, 커서가 가리키는 값은 별도 강조와 중앙의 전체 정책
미리보기로 표시합니다. `SupportPriority` 선택은 Support 요청을 만들지 않습니다.

WBP 경로는
`/Game/Work/LMK/UI/Policy/WBP_AIRECompanionPolicyPanel`이며 부모 클래스는
`UAIRECompanionPolicyPanelWidget`입니다. WBP는 레이아웃만 소유하고 정책 로직이나
StateTree 상태 선택 Blueprint Graph를 두지 않습니다. 외부 PNG·아이콘·UI Material은
사용하지 않으며 원형 섹터 렌더링과 각도 판정은 C++, 텍스트·버튼 배치는 WBP가
소유합니다. C++ `BindWidget` 계약 이름은 `PolicyPanel`, `CollapsedHint`,
`CurrentPolicyText`, `StatusText`, `BalancedButton`, `SupportPriorityButton`,
`HoldFireButton`, `DefendPlayerButton`, `AggressiveButton`입니다.

## 9. 검증 체크리스트

### 9.1 빌드·자산 기준선

- [ ] Rider에서 새 폴더 구조가 정상 인식된다.
- [ ] UnrealBuildTool 빌드가 성공한다.
- [ ] 이동한 C++ 클래스가 Blueprint에서 Missing Class로 표시되지 않는다.
- [ ] `DA_MAKO_Weapon_BasicMelee`가 오류 없이 열리고 Save된다.
- [ ] `DA_MAKO_AbilitySet_BasicMelee` Data Validation이 성공한다.
- [ ] `ST_AIRECompanion_Local`이 Compile·Save된다.
- [ ] `BP_MAKO`와 관련 Anim Blueprint가 Compile·Save된다.

### 9.2 무기 DA 소유권

- [ ] Companion Config의 deprecated 전투 값을 변경해도 공격 거리·쿨다운이 변하지 않는다.
- [ ] Weapon Definition의 `AttackRange`를 늘리면 더 먼 거리에서 공격을 시작한다.
- [ ] Weapon Definition의 `AttackRange`를 줄이면 새 거리까지 접근한 후 공격한다.
- [ ] Weapon Definition의 `CooldownDuration`을 늘리면 공격 간격이 증가한다.
- [ ] 검증 후 `AttackRange`와 `CooldownDuration`을 승인값으로 복원한다.

권장 임시 비교값:

```text
AttackRange: 150 → 300 → 150
CooldownDuration: 1.5 → 3.0 → 1.5
```

T02B 종료 시점의 `150 cm`는 구현 기준선이지 최종 체감 승인값이 아닙니다. 공격 진입이
보이는 칼날 궤적보다 지나치게 이르거나 늦지 않은지, `35 cm` capsule radius와
`160 cm` half-height가 의도하지 않은 측면·후방 hit를 만들지 않는지를 함께 비교한 뒤
세 값을 확정해야 합니다.

2026-08-12 PIE 피드백의 기본 공격 접근 거리 즉시 비교값은 `80 cm`입니다. 이 값은
`DA_MAKO_Weapon_BasicMelee.AttackRange`에서 변경하며 Data Asset 저장 후 다음 PIE부터
반영됩니다. Combat Skill은 `CombatSkill.AttackRange`만 별도이지만 현재 trace capsule
크기를 기본 공격과 공유하므로, 후속 Task에서 스킬 전용 shape·radius·half-height와
`MK_AM_ChargeAttack` NotifyState 범위를 함께 분리·튜닝합니다.

### 9.3 Stamina·Cooldown

- [ ] 단일 기본 공격과 4단 Combo 전체에서 Stamina가 변하지 않는다.
- [ ] 스킬 정상·실패·fallback 실행에서 Stamina가 변하지 않는다.
- [ ] Stamina가 0이어도 Target·Cooldown 조건이 유효하면 기본 공격과 스킬이 시작된다.
- [ ] 기본 공격 Cooldown 중 추가 기본 공격이 시작되지 않는다.
- [ ] 스킬 Cooldown 중 추가 스킬이 시작되지 않는다.
- [ ] Cooldown 종료 후 같은 Target을 다시 공격한다.
- [ ] Cooldown 실패가 StateTree나 게임 스레드를 멈추지 않는다.

Target이 먼저 사망해 검증이 끝나지 않으면 테스트 Target의 Health를 임시로 높이거나
Target을 Reset한 뒤 반복합니다.

### 9.4 Trace·Hit·Damage

- [ ] Target Health 100, Damage 25 기준으로 정확히 네 번의 Hit 후 사망한다.
- [ ] 정지/이동 Target의 실제 blade sweep 교차에서만 피해가 적용된다.
- [ ] 후방·측면·사거리 밖 intentional miss와 WorldStatic/Pawn 차폐는 피해를 적용하지 않는다.
- [ ] Trace window의 첫 `NoHit` 뒤 후속 sample이 명중할 수 있다.
- [ ] 첫 `Blocked` 뒤에도 후속 sample이 계속되며 `TargetHit`만 피해를 exact-once 적용한다.
- [ ] Hit 전에 Ability를 취소하면 피해가 적용되지 않는다.
- [ ] 취소 후 늦게 도착한 Hit Event가 피해를 적용하지 않는다.
- [ ] Target 사망 후 추가 피해가 적용되지 않는다.

### 9.5 접근·방향·취소

- [ ] `AttackRange` 밖에서는 Target에게 접근한다.
- [ ] 유효 거리 안에 들어온 뒤 이동을 멈추고 공격한다.
- [ ] 기본 공격·각 Combo Step·스킬 시작 시 선택 Target 방향을 바라본다.
- [ ] 공격 중 Target이 거리 밖으로 이동해도 진행 중 strike를 강제 취소하지 않고 표현과
  spatial miss를 마친 뒤 다음 연계 진입에서 정리한다.
- [ ] Target 파괴·EndPlay 시 Ability, Focus와 이동 요청이 정리된다.
- [ ] Threat가 해제되면 ReturnToPlayer 또는 FollowPlayer로 복귀한다.

### 9.6 장비 수명주기

- [ ] Equip 시 Ability가 한 번만 부여된다.
- [ ] Unequip 시 진행 중 Ability와 Ability Handle이 제거된다.
- [ ] 같은 무기를 반복 Equip·Unequip해도 Ability가 누적되지 않는다.
- [ ] 무기 교체 중 늦은 비동기 Callback이 이전 무기를 장착하지 않는다.
- [ ] Dead Tag 진입 시 Ability와 Linked Anim Layer가 해제된다.
- [ ] Health Reset 후 마지막 Weapon Definition이 다시 장착된다.
- [ ] 반복 Spawn·Possess·UnPossess·Destroy 후 Delegate와 Handle이 남지 않는다.

### 9.7 표현과 fallback

- [ ] Attack/Skill Montage의 Trace NotifyState 경로로 피해가 적용된다.
- [ ] Linked Anim Layer가 기본 Locomotion을 유지한다.
- [ ] Montage가 없는 테스트 Weapon에서도 시간 기반 fallback 공격이 종료된다.
- [ ] fallback 취소 후 Timer가 남아 늦은 피해를 주지 않는다.
- [ ] Unequip 후 이전 Linked Anim Layer가 남지 않는다.

### 9.8 로컬 AI 회귀

- [ ] Backend와 LLM 없이 Idle·Follow·Return이 동작한다.
- [ ] Threat 감지 후 Combat이 Locomotion보다 우선한다.
- [ ] Target 종료 후 로컬 추적 루프로 복귀한다.
- [ ] Work 또는 낮은 우선순위 행동이 Combat 종료 후 유효하면 재선택된다.
- [ ] 네트워크 기능 실패가 StateTree·GAS 전투를 중단하지 않는다.

### 9.9 기본 공격 콤보

- [ ] 빈 `ComboSteps`에서 기존 단발 Montage와 fallback이 유지된다.
- [ ] 설정한 Section 순서와 Step별 Damage가 정확히 적용되고 Stamina가 변하지 않는다.
- [ ] 기본 공격 Cooldown은 전체 Combo 실행당 한 번만 적용된다.
- [ ] 중복 Hit Notify는 같은 Step에 추가 피해를 만들지 않는다.
- [ ] 이전 Step Index의 늦은 Hit·Window Event는 무시된다.
- [ ] Stamina가 0이어도 진행하고 다음 Step 직전 Target 소실·교체·거리 이탈 시 안전하게 종료된다.
- [ ] State Exit·Unequip·Disabled·사망·Destroy 후 늦은 Event와 Timer가 실행을 되살리지 않는다.
- [ ] Montage가 없는 Combo fallback이 Step별 피해 후 종료되고 취소 시 Timer를 남기지 않는다.

2026-07-28 사용자 검증 증거:

- `AM_Combo01_Montage`의 `Attack_01`~`Attack_04`가 0-based Step 0~3 순서로
  연속 재생되고 각 단계 Hit가 소비되는 정상 경로를 확인했습니다.
- Section 종료 전 다음 Section을 미리 연결하는 방식으로 1타 반복과 Blend Out 끊김을
  해결했습니다.
- T05B 회귀 시험에서 Damage·Stamina 불변·Cooldown과 실패·취소·fallback·반복
  수명주기를 함께 확인했습니다.

### 9.10 전투 스킬 연계

- [ ] SelectionChance 1.0에서 기본1·2, 기본2·3, 기본3·4 사이에 스킬이 삽입된다.
- [ ] 스킬 종료 후 정확한 다음 Combo Step부터 재개된다.
- [ ] 기본 공격 전과 마지막 Step 종료 후에도 스킬이 실행된다.
- [ ] 비취소 구간 요청은 가장 가까운 Window에서 한 번만 실행된다.
- [ ] 같은 Window의 반복 Tick과 중복 Request가 스킬을 중복 실행하지 않는다.
- [ ] 스킬 Cooldown·Target 무효·거리 실패 시 현재 Combo 유지 또는 기본 공격 fallback이 동작한다.
- [ ] 스킬 Hit는 실행당 최대 한 번만 피해를 적용한다.
- [ ] State Exit·Target 파괴·Unequip·Disabled·사망 후 늦은 Hit·Ended가 피해나 재개를 만들지 않는다.
- [ ] Equip·Unequip과 정상·실패·취소를 3회 이상 반복해 Handle·Timer·Delegate가 누적되지 않는다.

### 9.11 자율 회피

- [ ] 선택 Threat의 열린 `SingleTarget` Execution마다 deterministic roll을 한 번만 수행한다.
- [ ] 50% 선택과 `0.15-0.28 s` 지연이 고정 seed에서 재현되고 실패·만료는 재시도하지 않는다.
- [ ] Stamina `24.99`는 거부되고 `25`는 성공 후 0이 되며, `1.5 s` 뒤 `15/s`로 회복해
  100에서 clamp된다.
- [ ] 성공 시 `5 s` cooldown이 적용되고 시작 실패에는 비용과 cooldown이 없다.
- [ ] 좌우 clearance 중 넓은 쪽, 동률 오른쪽, 최소 `100 cm`, `300 cm/0.25 s` swept
  movement와 Root Motion 비중복을 확인한다.
- [ ] `+0.05 s` 전에는 피해, `+0.05-0.17 s`에는 `TargetInvulnerable`, 이후에는 정상 피해가
  적용되며 무적 접촉 duplicate는 늦은 피해를 만들지 않는다.
- [ ] 같은 Execution의 Q는 dash를 재사용하고 추가 Stamina를 쓰지 않으며 다른 Execution은
  거부한다.
- [ ] cancel·target destroy·death·unpossess·unequip·EndPlay를 세 번 반복해 timer, delegate,
  tag/effect와 movement context가 남지 않는다.

### 9.12 인벤토리·무기 교체·지원 회복

- [x] 앰플 3개에서 회복 성공 후 2개가 남고 Health가 50에서 75로 증가한다.
- [ ] Health 90에서 회복해도 100을 넘지 않고 앰플은 정확히 하나만 소비된다.
- [x] Item 없음, 적대·사망·MaxHealth·ASC 없는 Target 요청이 즉시 거부된다.
- [x] 처치 중 Combat·Target 파괴·MAKO 사망 시 Item과 Health가 변하지 않는다.
- [ ] 처치 중 State Exit 시 Item과 Health가 변하지 않는다.
- [ ] 비전투 중 두 Weapon Item을 교체하면 이전 Ability가 회수되고 새 Ability가 한 번만 부여된다.
- [ ] 무기 로드 실패·잘못된 Item 타입·미보유 무기·공격 중 교체가 안전하게 거부된다.
- [ ] 무기 장착 실패 뒤 이전 무기와 Ability Handle, 장착 ItemId가 복구된다.
- [x] 사망 시 무기·지원 Ability Handle이 회수되고 부활 시 각각 한 번만 재부여된다.
- [ ] 포션 성공·취소, 무기 교체, 사망·부활, Spawn·Destroy를 각각 3회 이상 반복해
      Handle·Timer·Delegate·Item 수량이 누적되지 않는다.
- [x] Backend 없이 Idle·Follow·Combat·Support·Return이 동작하고 Combat이 Support를 선점한다.

> 2026-07-30 사용자 PIE 검증 기준입니다. 다중 무기 자산이 아직 없어 비전투 교체,
> 공격 중 교체 거부와 장착 실패 복구는 실제 두 번째 무기를 추가할 때 검증합니다.
> 실제 플레이어 ASC·GAS Health 연동 전까지 지원 회복은 ASC 아군 fixture 기준입니다.

### 9.13 로컬 전투·지원 정책과 데모 UI

- [x] `AI_REEditor Win64 Development` UHT·UBT 빌드가 성공한다.
- [x] WBP가 커스텀 C++ 부모와 필수 `BindWidget` 계약으로 Compile·Save된다.
- [ ] `P` Hold·방향 선택·Release를 반복하고 취소할 때 이동·시점·커서 상태가 복구된다.
- [ ] `HoldFire` 전환 즉시 현재 공격이 취소되고 같은 적을 인식한 채 신규 Combat 요청이 억제된다.
- [ ] `DefendPlayer`가 플레이어 기준 600cm 포함 경계 안 Target만 허용한다.
- [ ] `Aggressive`가 감지 거리와 플레이어 기준 1500cm 추격 상한을 모두 지킨다.
- [ ] Combat·Support 동시 요청에서 Balanced는 Combat, SupportPriority는 Support를 선택한다.
- [ ] Disabled·Survival은 두 역할 선호에서 항상 Combat·Support를 선점한다.
- [ ] 위 2개 역할·아래 3개 교전 선택으로 여섯 조합을 만들고 각 입력이 한 축만 변경한다.
- [ ] 연속 축 전환과 Target 소실 뒤 늦은 Ability·이동·Timer가 발생하지 않는다.
- [ ] Spawn·Possess·UnPossess·Destroy 반복 뒤 정책·Delegate·Widget·Input Component가 누적되지 않는다.
- [ ] Backend·LLM 없이 전체 경로가 동작한다.

> 2026-08-03 사용자 PIE 스모크에서 원형 UI 표시와 정책 입력의 대략적인 정상 동작을
> 확인했습니다. WBP는 재빌드된 C++ 부모 기준 19개 Widget, compiler message 0개로
> Compile·Save되었습니다. 거리 포함 경계, Combat·Support 동시 요청 우선순위와 반복
> 수명주기 항목은 별도 상세 검증 전이므로 열어 둡니다.

### 9.14 자율 동행과 외부 DirectCommand Prototype

- [x] 평상시에는 `ReturnStartDistance=600cm`를 넘을 때만 복귀를 시작하고
  `ReturnStopDistance=400cm` 안에서 자율 행동으로 돌아간다.
- [x] 상위 행동과 Work가 없으면 플레이어 기준 150~350cm NavMesh 안에서 3~6초 대기와
  제한 배회를 반복하며, 목적지 탐색은 최대 8회로 제한된다.
- [x] `Command.Follow`가 `FollowStopDistance=200cm` 안에서 정지하고 lease 동안 재추적한다.
- [x] Hold와 Follow DirectCommand를 Combat이 선점한다.
- [ ] DirectCommand 종료 후 기존 WorkOrder가 유효하면 재개된다.
- [ ] Candidate 만료·중복·교체·Nav 실패·Player 소실과 State Exit에서 이동 요청과 늦은
  Callback이 남지 않는다.
- [ ] CancelCurrent는 active WorkOrder ID만 취소하고 Attack은 현재 UE-selected
  hostile/alive Threat만 관찰한다.
- [ ] Gather와 Engage/Distract/Move/Switch는 성공으로 가장하지 않고 로컬
  `UnsupportedExecution`으로 종료된다.
- [ ] Backend·LLM이 없거나 Candidate가 거부되어도 Return·배회·Combat·Work 로컬 루프가
  계속 동작한다.

`ST_AIRECompanion_Local`의 기존 Follow 거리 조건은 기본 동행 입력으로 사용하지 않습니다.
Follow는 검증된 DirectCommand lease에서만 사용하고, 기본 동행은 Return hysteresis와
IdleNearPlayer가 담당합니다. 외부 priority 문자열은 진단값일 뿐 StateTree 우선순위를
변경하지 않습니다.

활성 DirectCommand 동안에는 자율 Return latch를 끕니다. 따라서 HoldPosition은 플레이어가
600cm 밖으로 이동해도 lease 동안 위치를 유지하고, lease 종료 다음 tick에 현재 거리를 다시
평가해 600cm 밖이면 400cm 안까지 복귀합니다. PIE에서
`aire.Companion.DistanceDebug 1`을 실행하면 MAKO 머리 위에 플레이어 표면 거리,
600→400cm Return 경계, 현재 로컬 선택, Command intent와 남은 lease가 표시됩니다.
검증 뒤에는 `aire.Companion.DistanceDebug 0`으로 끕니다.

> 2026-08-11: Config, Context Evaluator, production DirectCommand Task,
> IdleNearPlayer NavMesh Task와 Gateway 연동 C++ source를 구현하고 `git diff --check`를
> 통과했습니다. 같은 날 Editor MCP로 `ST_AIRECompanion_Local`에 production
> DirectCommand Task와 `IdleNearPlayer` State·Task·HasPlayer 조건 및 evaluator binding을
> 적용하고 해당 asset만 Compile·Save한 뒤 재컴파일 성공을 확인했습니다. 이후 사용자가
> 최신 source PIE에서
> 600→400cm 복귀, 제한 배회, Hold lease, Follow 200cm 정지·재추적과 Combat 선점을
> 확인했습니다. WorkOrder 진입 수단이 없어 Work 재개는 후속 통합으로 이관했고,
> Level 종료·Owner 파괴 검증은 보류했으므로 나머지 수명주기 체크는 열어 둡니다.

## 10. 통과 기준과 후속 정리

이번 구조 변경은 다음 조건이 모두 만족되어야 검증 완료로 간주합니다.

1. UBT 빌드와 관련 Blueprint·StateTree Compile 성공
2. Weapon Definition의 Range·Cooldown 변경이 런타임에 반영
3. 공격 Stamina 불변·GAS Cooldown·공용 Damage/Stagger 회귀 통과
4. 취소·사망·무기 교체·Target 소실 수명주기 통과
5. Idle·Follow·Combat·Return 로컬 루프 회귀 통과
6. 인벤토리 원자성, 지원 취소 무소비, 과회복 방지와 무기 실패 복구 통과
7. 로컬 정책의 거리·우선순위·취소 수명주기와 Policy WBP 입력 경로 통과
8. Player·MAKO 공격이 같은 Damage/Stagger 계약으로 Boss Health와 반응 Gauge에 적용

검증 후 별도 정리 Task에서 deprecated `CombatDistance`·`CombatCooldown` 프로퍼티와
기존 StateTree Binding 호환 필드를 제거할 수 있습니다. `M03-E09-T01`에서 기존
MAKO Health 전용 Damage GE 결합은 Target 선언형 공용 Combat Damage Execution으로
교체되었습니다. 이후 공격 타입·방어·저항도 이 Target 계약을 확장합니다.

T05C는 실제 플레이어가 ASC·GAS Health·회복 대상 인터페이스를 제공하고 사용자
UBT·Editor·PIE 검증이 끝날 때까지 `Review`로 유지합니다. 인벤토리 UI,
플레이어와 MAKO 간 전달, 월드 획득, Drop, Crafting, SaveGame과 Replication은 후속
범위입니다.

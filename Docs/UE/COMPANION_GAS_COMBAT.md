# Companion GAS Combat Architecture and Verification

## 1. 문서 목적

이 문서는 MAKO Companion의 현재 GAS 전투 구조, 무기별 Data Asset 소유권,
쌍검 콤보와 신규 무기 확장 규칙, 코드 구조 변경 후 필요한 Unreal Editor 설정과
PIE 검증 절차를 정의합니다.

- 관련 Milestone: `M03 Companion Local AI`
- 관련 Task: `M03-E03-T02`, `M03-E03-T03`, `M03-E03-T04`, 후속 `M03-E03-T05`
- 코드 기준일: 2026-07-28
- 현재 검증 상태: T05A 정상 4단 콤보 PIE 1차 수용, 실패·취소·fallback 경계 검증 대기

## 2. 현재 코드 구조

```text
UEProject/Source/AI_RE/LMK/MAKO/
├─ Public/
│  ├─ Core/
│  ├─ LocalAI/
│  │  ├─ StateTree/
│  │  └─ Threat/
│  ├─ AbilitySystem/
│  │  ├─ Core/
│  │  │  └─ Attributes/
│  │  └─ Combat/
│  │     ├─ Abilities/
│  │     └─ Effects/
│  ├─ Equipment/
│  ├─ Animation/
│  └─ Testing/
├─ Private/
│  └─ Public과 동일한 기능 구조
└─ Components/
   ├─ Public/
   │  ├─ Equipment/
   │  └─ Threat/
   └─ Private/
      ├─ Equipment/
      └─ Threat/
```

### 2.1 책임 경계

| 영역 | 책임 |
|---|---|
| Companion Character | ASC·AttributeSet·기능 컴포넌트 조립과 생명주기 |
| StateTree | 행동 우선순위, Target 접근, 공격 요청과 취소 |
| Threat Component | 적대 Target 감지·선택·소실 정리 |
| Equipment Component | 현재 무기, 비동기 자산, Ability Handle과 Anim Layer 수명 |
| Gameplay Ability | 공격 검증, 비용·쿨다운 Commit, Montage·Hit·Damage 실행 |
| Gameplay Effect | Stamina 비용, Cooldown, Health 피해 |
| AttributeSet | Health·MaxHealth·Stamina·MaxStamina |
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
→ StaminaCost·CooldownDuration을 SetByCaller 값으로 등록
→ GiveAbility
→ StateTree가 Event.Companion.Attack.Request 전송
→ Primary Attack Ability 활성화
→ Target·거리·각도·Stamina·Cooldown 검증
→ 기본 공격 Cooldown GE를 실행당 한 번 적용
→ 현재 Combo Step의 Stamina Cost GE 적용
→ Montage Hit·Combo Window Notify 또는 시간 기반 fallback으로 Step Event 전송
→ 다음 Step 직전 Target·거리·각도·Stamina 재검증
→ Damage GE를 Target ASC에 적용
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

실제 공격 제한은 GAS Cooldown이 소유합니다. StateTree의 짧은 재시도 간격은 매 Tick
활성화 요청을 막는 throttle일 뿐 Cooldown의 두 번째 원본이 아닙니다.

## 4. Data Asset 계약

### 4.1 Companion Config

Companion Config가 소유하는 값은 다음과 같습니다.

- 이동 속도와 Walk/Run 전환 거리
- Follow/Return 거리
- Threat 감지 거리와 최대 추격 거리
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
| `ComboSteps` | 선택적 Montage Section·Damage·Stamina Cost 순서; 빈 배열은 기존 단발 계약 |
| `LinkedAnimLayerClass` | 무기별 Linked Anim Layer |
| `Damage` | 기본 공격 피해 |
| `StaminaCost` | 기본 공격 비용 |
| `AttackRange` | 충돌 반경을 제외한 유효 표면 거리 |
| `CooldownDuration` | GAS Cooldown 지속 시간 |
| `AttackHalfAngleDegrees` | 전방 기준 유효 공격 반각 |
| `FallbackHitDelay` | Montage가 없을 때 Hit 발생 지연 |
| `FallbackRecoveryDuration` | fallback 공격 종료 지연 |

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

동일한 `Event.Companion.Attack.Request`를 Trigger로 사용하는 Primary Attack Ability는
한 Weapon AbilitySet에 하나만 둡니다. 기본 공격과 콤보 Ability를 동시에 넣으면 같은
Event에 둘 다 활성화를 시도할 수 있습니다.

## 5. 현재 기본 근접 공격

`UAIRECompanionMeleeAttackAbility`는 다음을 소유합니다.

- Disabled 상태 차단
- Weapon Definition과 근접 무기 검증
- 적대·생존 Target 검증
- 공격 거리와 공격 각도 검증
- Stamina Cost와 Cooldown Commit
- Montage 재생 또는 fallback
- Hit Event 한 번 소비
- Damage GE 적용
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
StaminaCost
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
- Target이 현재 공격 거리와 각도 안에 있음
- 다음 Step 비용을 낼 Stamina가 있음
- StateTree 전투 상태가 여전히 유효함
- 무기 교체·사망·전투 취소가 발생하지 않음

Stamina Cost는 각 Step 시작 직전에 기존 Cost GE로 적용하고, 기본 공격 Cooldown은
Combo 실행 시작 시 한 번만 적용합니다. 다음 Step 조건이 실패하면 아직 실행하지 않은
Step의 비용은 소비하지 않고 현재 Ability를 안전하게 종료합니다.

Montage가 없거나 로드되지 않으면 기존 `FallbackHitDelay`와
`FallbackRecoveryDuration`을 Step마다 재사용해 제한된 자동 Combo 수명주기를
실행합니다. `ComboSteps`가 비어 있으면 기존 단발 Montage와 fallback이 유지됩니다.

### 6.1 Hit 판정과 후속 Trace 경계

Anim Notify는 실제 명중을 확정하지 않고 Hit 판정을 시도할 타이밍만 전달합니다.
현재 T05A는 선택된 Threat Target의 생존·적대성·거리·각도를 Ability에서 검증한 뒤
Damage GE를 적용합니다.

실제 Weapon Mesh와 Blade Socket 계약이 추가되면 같은 Ability 내부 Hit 판정 경계를
Socket 이전·현재 위치 기반 Sphere 또는 Capsule Sweep으로 교체합니다. 이때도
StateTree, Combo Step, 단계별 Cost·Damage와 실행당 Cooldown 계약은 유지하며,
기본 공격은 현재 선택 Target 하나만 단계당 한 번 피해를 받도록 제한합니다.

같은 Combo 실행 규칙을 사용하는 무기는 Combo Ability를 재사용하고 Weapon Definition의
Montage Section과 Step 수치만 다르게 구성합니다. 두 번째 이상의 시스템에서 Combo Step
구성을 공유해야 할 때만 별도 AttackSet Data Asset 추출을 검토합니다.

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

#### `DA_AIRE_Weapon_BasicMelee`

기존 Companion Config에 커스텀 전투 값이 있었다면 Weapon Definition으로 복사합니다.

기본 기준:

```text
AttackRange = 150 cm
CooldownDuration = 1.5 s
Damage = 25
StaminaCost = 20
AttackHalfAngleDegrees = 30
```

다음 참조도 유지해야 합니다.

- `AbilitySet = DA_AIRE_AbilitySet_BasicMelee`
- 승인된 `AttackMontage`
- 승인된 `LinkedAnimLayerClass`

변경 후 Data Asset을 Save합니다.

#### `DA_AIRE_AbilitySet_BasicMelee`

- `UAIRECompanionMeleeAttackAbility`가 존재하는지 확인합니다.
- 같은 `Attack.Request`를 받는 Primary Attack Ability가 두 개 이상 없는지 확인합니다.
- Ability Level이 1 이상인지 확인합니다.

#### `BP_MAKO`

- Equipment Component의 `DefaultWeaponDefinition`이
  `DA_AIRE_Weapon_BasicMelee`를 가리키는지 확인합니다.
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

### 8.3 기본 공격 콤보 설정

1. `AttackMontage` 하나에 실행 순서대로 Montage Section을 만듭니다.
2. Weapon Definition의 `ComboSteps`에 같은 순서로 Section 이름, Damage와
   Stamina Cost를 입력합니다.
3. 각 Section의 판정 프레임에 `AIRE Companion Attack Hit` Notify를 배치하고
   `ComboStepIndex`를 배열의 0-based Index와 맞춥니다.
4. 다음 단계로 전환할 Section에는 `AIRE Companion Combo Window` Notify State를
   배치하고 같은 `ComboStepIndex`를 입력합니다. Notify State의 End 위치가 실제 다음
   Section으로 전환할 지점입니다.
5. 마지막 Step에는 다음 단계가 없으므로 Combo Window가 필요하지 않습니다.
6. Data Validation 후 Weapon Definition, Montage와 관련 Anim Blueprint를
   Compile·Save합니다.

T05A의 Hit Notify는 Trace 명중을 확정하지 않습니다. 현재 선택 Target의 생존·적대성·
거리·각도를 Ability가 다시 검증해 피해를 적용하며, 실제 Weapon Socket Trace는
Weapon Mesh 계약이 추가된 뒤 연결합니다.

## 9. 검증 체크리스트

### 9.1 빌드·자산 기준선

- [ ] Rider에서 새 폴더 구조가 정상 인식된다.
- [ ] UnrealBuildTool 빌드가 성공한다.
- [ ] 이동한 C++ 클래스가 Blueprint에서 Missing Class로 표시되지 않는다.
- [ ] `DA_AIRE_Weapon_BasicMelee`가 오류 없이 열리고 Save된다.
- [ ] `DA_AIRE_AbilitySet_BasicMelee` Data Validation이 성공한다.
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

### 9.3 Cost·Cooldown

- [ ] 공격 1회마다 Stamina가 Weapon Definition의 `StaminaCost`만큼 감소한다.
- [ ] Stamina가 비용보다 적으면 공격 Ability가 시작되지 않는다.
- [ ] Cooldown 중 추가 공격이 시작되지 않는다.
- [ ] Cooldown 종료 후 같은 Target을 다시 공격한다.
- [ ] Cost·Cooldown 실패가 StateTree나 게임 스레드를 멈추지 않는다.

Target이 먼저 사망해 비용 검증이 끝나지 않으면 테스트 Target의 Health를 임시로 높이거나
Target을 Reset한 뒤 반복합니다.

### 9.4 Hit·Damage

- [ ] Target Health 100, Damage 25 기준으로 정확히 네 번의 Hit 후 사망한다.
- [ ] 한 Ability 실행에서 Hit Notify가 중복되어도 피해는 한 번만 적용된다.
- [ ] Hit 전에 Ability를 취소하면 피해가 적용되지 않는다.
- [ ] 취소 후 늦게 도착한 Hit Event가 피해를 적용하지 않는다.
- [ ] Target 사망 후 추가 피해가 적용되지 않는다.

### 9.5 접근·각도·취소

- [ ] `AttackRange` 밖에서는 Target에게 접근한다.
- [ ] 유효 거리 안에 들어온 뒤 이동을 멈추고 공격한다.
- [ ] 공격 반각 밖이면 Target 방향으로 회전한 뒤 공격한다.
- [ ] 공격 중 Target이 거리 밖으로 이동하면 Ability와 이동 요청이 정리된다.
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

- [ ] Attack Montage와 Hit Notify 경로로 피해가 적용된다.
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
- [ ] 설정한 Section 순서와 Step별 Damage·Stamina Cost가 정확히 적용된다.
- [ ] 기본 공격 Cooldown은 전체 Combo 실행당 한 번만 적용된다.
- [ ] 중복 Hit Notify는 같은 Step에 추가 피해를 만들지 않는다.
- [ ] 이전 Step Index의 늦은 Hit·Window Event는 무시된다.
- [ ] 다음 Step 직전 Stamina 부족, Target 소실·교체, 거리·각도 이탈 시 안전하게 종료된다.
- [ ] State Exit·Unequip·Disabled·사망·Destroy 후 늦은 Event와 Timer가 실행을 되살리지 않는다.
- [ ] Montage가 없는 Combo fallback이 Step별 비용·피해 후 종료되고 취소 시 Timer를 남기지 않는다.

2026-07-28 PIE 1차 수용 증거:

- `AM_Combo01_Montage`의 `Attack_01`~`Attack_04`가 0-based Step 0~3 순서로
  연속 재생되고 각 단계 Hit가 소비되는 정상 경로를 확인했습니다.
- Section 종료 전 다음 Section을 미리 연결하는 방식으로 1타 반복과 Blend Out 끊김을
  해결했습니다.
- 위 체크리스트 항목은 Damage·Stamina·Cooldown과 실패·취소·fallback을 함께 요구하므로
  부분 확인만으로 완료 표시하지 않습니다.

## 10. 통과 기준과 후속 정리

이번 구조 변경은 다음 조건이 모두 만족되어야 검증 완료로 간주합니다.

1. UBT 빌드와 관련 Blueprint·StateTree Compile 성공
2. Weapon Definition의 Range·Cooldown 변경이 런타임에 반영
3. Stamina Cost·GAS Cooldown·Damage GE 회귀 통과
4. 취소·사망·무기 교체·Target 소실 수명주기 통과
5. Idle·Follow·Combat·Return 로컬 루프 회귀 통과

검증 후 별도 정리 Task에서 deprecated `CombatDistance`·`CombatCooldown` 프로퍼티와
기존 StateTree Binding 호환 필드를 제거할 수 있습니다. 실제 적 연동 전에는 현재
Damage GE가 `UAIRECompanionAttributeSet::Health`에 결합된 점을 해결하고 공용 Combat
Attribute 또는 Target 피해 계약을 확정해야 합니다.

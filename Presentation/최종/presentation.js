(() => {
  const deck = document.getElementById("deck");

  const appendixPages = [
    { category: "market", categoryTitle: "시장 · 사업성", title: "시장과 타깃", headline: "캐릭터와의 관계가 게임 밖에서도 이어지는 경험", bullets: ["관계 몰입형 플레이어", "게임 밖에서도 연결을 원하는 사용자", "행동하는 AI를 탐색하는 초기 수용층"], cards: [["2030", "$41.39B", "대화형 AI 시장 전망"], ["성장률", "23.7%", "2025–2030 CAGR"], ["핵심 가치", "관계의 연속성", "플레이와 일상의 연결"]], source: "Grand View Research · Conversational AI Market · 2025" },
    { category: "market", categoryTitle: "시장 · 사업성", title: "경쟁 환경과 포지셔닝", headline: "대화·행동·기억을 하나의 캐릭터 경험으로", bullets: ["대화 서비스는 관계 형성에 강점", "게임 AI는 상황 인식과 행동에 강점", "AI : RE는 두 경험을 멀티플랫폼으로 연결"], cards: [["대화형 AI", "관계", "대화 · 음성 · 기억"], ["게임 AI", "행동", "인식 · 계획 · 실행"], ["AI : RE", "연결", "관계 · 행동 · 멀티접점"]], source: "Character.AI · NVIDIA ACE · Inworld" },
    { category: "market", categoryTitle: "시장 · 사업성", title: "수익 구조", headline: "게임에서 시작해 동료 서비스와 IP로 확장", bullets: ["PC 게임과 캐릭터·에피소드 DLC", "선택형 동료 서비스 구독", "공식 IP 캐릭터와 B2B 프레임워크"], cards: [["게임", "콘텐츠", "본편 · 캐릭터 · 에피소드"], ["서비스", "구독", "대화 · 기억 · 음성"], ["B2B", "IP", "캐릭터 동료 프레임워크"]], source: "Character.AI c.ai+ 가격은 비교 기준으로만 사용" },
    { category: "market", categoryTitle: "시장 · 사업성", title: "확장 로드맵", headline: "게임 검증 이후 접점과 형태를 단계적으로 확장", bullets: ["현재: 게임 안의 동료 경험 검증", "다음: Web·Discord·Kakao·Voice 고도화", "장기: Physical AI와 공식 IP 적용"], cards: [["NOW", "GAME", "플레이 · 관계 · 예외 검증"], ["NEXT", "SERVICE", "음성 · 멀티모달 · 운영"], ["EXPAND", "PHYSICAL AI", "로봇 · IP · B2B"]], source: "Physical AI는 장기 확장 방향이며 현재 제품 범위와 구분" },

    { category: "gameplay", categoryTitle: "게임플레이 구현", title: "게임플레이 전체 지도", headline: "생존·전투·작업을 동료 행동으로 연결", bullets: ["따라오기와 자율 생존", "전투·회피·지원", "채집·제작·보관·저장"], cards: [["생존", "FOLLOW", "위협 감지 · 회피"], ["전투", "COMBAT", "공격 · 지원 · 회복"], ["작업", "WORK", "채집 · 제작 · 운반"]], source: "Unreal Vertical Slice 구현 범위" },
    { category: "gameplay", categoryTitle: "게임플레이 구현", title: "동료 행동과 전투", headline: "우선순위 판단과 능력 실행을 분리", bullets: ["StateTree가 상황별 행동 우선순위 결정", "GAS가 비용·재사용·효과 처리", "실행 직전 거리·자원·대상을 다시 확인"], cards: [["판단", "StateTree", "생존 → 전투 → 명령 → 작업"], ["실행", "GAS", "비용 · 쿨다운 · 효과"], ["보호", "재검증", "거리 · 자원 · 대상"]], source: "StateTree · Gameplay Ability System" },
    { category: "gameplay", categoryTitle: "게임플레이 구현", title: "작업과 아이템 순환", headline: "요청부터 제작 결과까지 하나의 작업 흐름으로", bullets: ["채집·제작을 WorkOrder로 관리", "전투 중단 후 남은 작업 재개", "플레이어·MAKO·공용 창고 사이 원자적 이동"], cards: [["요청", "WORK ORDER", "종류 · 대상 · 수량"], ["진행", "INTERRUPT", "중단 · 재개 · 실패"], ["결과", "INVENTORY", "소비 · 생산 · 이동"]], source: "WorkOrder · Inventory · Storage" },
    { category: "gameplay", categoryTitle: "게임플레이 구현", title: "저장과 월드 상태", headline: "게임 상태와 현실 시간을 분리해 안전하게 복원", bullets: ["최신 저장본과 이전 세대 저장본 유지", "중복 적용을 막는 작업 기록", "GameWorld와 RealWorld 시간 분리"], cards: [["복원", "SAVE", "Primary · Previous"], ["중복 방지", "LEDGER", "이미 적용된 결과 확인"], ["시간", "TWO CLOCKS", "게임 · 현실 분리"]], source: "SaveGame · World Time · Applied Operation Ledger" },

    { category: "ai", categoryTitle: "AI 시스템 구조", title: "로컬 행동과 대화 AI", headline: "빠른 게임 행동과 깊은 대화를 역할별로 분리", bullets: ["Unreal은 이동·전투·작업을 즉시 처리", "서버 AI는 대화·의도·장기 맥락 처리", "게임 사실은 제한된 Context로만 전달"], cards: [["게임", "LOCAL", "StateTree · GAS · WorkOrder"], ["대화", "SERVER AI", "LangGraph · LLM · Memory"], ["연결", "CONTEXT", "위치 · 위협 · 작업 · 시간"]], source: "Hybrid AI architecture" },
    { category: "ai", categoryTitle: "AI 시스템 구조", title: "행동 안전성과 장애 대응", headline: "AI의 제안은 게임 규칙을 통과해야만 실행", bullets: ["행동 후보를 현재 게임 상태로 재검증", "잘못된 대상·거리·시간의 요청 거부", "서버 장애 시 로컬 생존·전투는 유지"], cards: [["제안", "CANDIDATE", "행동 종류 · 대상"], ["확인", "GAME RULES", "상태 · 거리 · 유효 시간"], ["대체", "LOCAL FALLBACK", "따라오기 · 전투 · 생존"]], source: "External response validation · Local fallback" },
    { category: "ai", categoryTitle: "AI 시스템 구조", title: "대화 처리 흐름", headline: "질문의 성격에 맞는 처리 경로로 분기", bullets: ["명령·제작법·적·세계관·일반 대화 구분", "필요한 게임 정보와 기억만 결합", "최종 응답은 안전 검사를 거쳐 반환"], cards: [["분류", "INTENT", "질문과 요청의 성격"], ["처리", "LANGGRAPH", "경로별 Context 조합"], ["응답", "SANITIZE", "근거 · 수치 · 권한 확인"]], source: "LangGraph intent-specific nodes" },

    { category: "memory", categoryTitle: "장기기억 · 환각 · 망각", title: "기억 저장과 검색", headline: "출처가 있는 기억만 제한적으로 회상", bullets: ["사용자 발화와 검증된 사건을 원본으로 보관", "후보 검토 후 승인된 기억만 활성화", "동일 사용자·세이브·동료 범위에서 상위 3개만 검색"], cards: [["저장", "SOURCE", "Message · Game Event"], ["검토", "CANDIDATE", "승인 · 보류 · 거부"], ["검색", "TOP 3", "최대 360자"]], source: "Source-backed Memory · Bounded RAG" },
    { category: "memory", categoryTitle: "장기기억 · 환각 · 망각", title: "환각 제어", headline: "저장 단계와 응답 단계에서 두 번 제한", bullets: ["출처 없는 생성 문장은 기억으로 승격하지 않음", "유사·모순 후보는 검토 대기 상태로 격리", "응답의 출처·숫자·날짜·부정·권한을 재확인"], cards: [["1차", "저장 검사", "출처 · 범위 · 중복"], ["2차", "응답 검사", "인용 · 수치 · 부정"], ["실패", "안전 응답", "기억 사용을 취소"]], source: "환각 제거가 아니라 가능성을 단계별로 제한" },
    { category: "memory", categoryTitle: "장기기억 · 환각 · 망각", title: "망각과 사용자 제어", headline: "자동 삭제 대신 회상 순위와 사용자 선택으로 관리", bullets: ["오래된 기억은 30일 반감기로 순위 감쇠", "반복 회상 보너스는 5회까지만 적용", "검색·정정·고정·삭제·초기화·후보 검토 제공"], cards: [["감쇠", "30 DAYS", "시간에 따른 순위 조정"], ["편향 방지", "5 USES", "반복 보너스 상한"], ["통제", "MEMORY API", "정정 · 고정 · 삭제"]], source: "Soft Forgetting · User Memory Control" },
    { category: "memory", categoryTitle: "장기기억 · 환각 · 망각", title: "삭제와 재생성 방지", headline: "삭제된 기억은 검색에서도 재처리에서도 제외", bullets: ["삭제 즉시 검색·Prompt·Cache에서 제외", "Tombstone으로 같은 원본의 재생성 차단", "마지막 참조가 사라지면 원본도 정리 대상으로 전환"], cards: [["1", "ARCHIVE", "활성 기억에서 제외"], ["2", "TOMBSTONE", "재증류 차단"], ["3", "PURGE", "참조 없는 원본 정리"]], source: "Delete without resurrection" },

    { category: "platform", categoryTitle: "멀티플랫폼 연동", title: "플랫폼별 기능 범위", headline: "같은 MAKO를 접점별 역할에 맞게 제공", bullets: ["Unreal: 플레이와 행동", "Web·Discord: 대화·기억·작업", "KakaoTalk: 현재 대화 연동"], cards: [["UNREAL", "PLAY", "대화 · 기억 · 작업 · 행동"], ["WEB / DISCORD", "MANAGE", "대화 · 기억 · 작업"], ["KAKAO TALK", "CHAT", "대화 연동"]], source: "KakaoTalk 대화 연동 완료 · 기능 범위는 구분 표기" },
    { category: "platform", categoryTitle: "멀티플랫폼 연동", title: "Web·Discord·KakaoTalk", headline: "하나의 서버를 세 가지 일상 접점에 연결", bullets: ["Mobile Web은 대화·기억 관리·작업 요청", "Discord는 명령 기반의 빠른 접근", "KakaoTalk은 Open Builder 대화 연동"], cards: [["WEB", "화면", "대화 · 기억 · 작업"], ["DISCORD", "명령", "채팅 · 검색 · 요청"], ["KAKAO", "메신저", "대화 Adapter"]], source: "Kakao SkillPayload · Callback response" },
    { category: "platform", categoryTitle: "멀티플랫폼 연동", title: "오프라인 작업과 동기화", headline: "서버에서 진행하고 PC가 최종 적용", bullets: ["작업 시간은 서버가 관리", "완료 결과는 Outbox로 재전송 가능", "PC가 인벤토리에 반영하고 저장한 뒤 확정"], cards: [["서버", "PROGRESS", "RealWorld 시간"], ["전달", "OUTBOX", "재시도 · 중복 방지"], ["PC", "APPLY", "인벤토리 · 저장 · 확정"]], source: "Offline Task E2E는 별도 검증 상태로 관리" },

    { category: "production", categoryTitle: "제작 과정 · 검증", title: "생성형 AI 활용", headline: "사람이 방향을 정하고 AI가 반복을 가속", bullets: ["캐릭터 레퍼런스와 콘셉트 탐색", "UI 시안과 발표 시각 자료 제작", "코드 구현·검증 과정의 보조"], cards: [["MODEL", "캐릭터", "레퍼런스 탐색"], ["VISUAL", "화면", "UI · 이미지 시안"], ["CODE", "개발", "구현 · 테스트 보조"]], source: "최종 선택과 검증은 팀이 수행" },
    { category: "production", categoryTitle: "제작 과정 · 검증", title: "아트 콘셉트", headline: "세계와 캐릭터가 같은 감정선을 공유하도록 설계", bullets: ["OUTOPIA는 인공 구조물과 침식된 자연이 공존하는 근미래 폐허", "PLAYER는 현실적인 생존자, MAKO는 신비한 동료로 대비", "절제된 색감과 거대한 실루엣으로 고독과 동행을 함께 표현"], cards: [["WORLD", "OUTOPIA", "폐허 · 자연 침식 · 거대 구조물"], ["PLAYER", "SURVIVOR", "현실성 · 생존 장비 · 무게감"], ["MAKO", "COMPANION", "신비로움 · 대비 · 온기"]], source: "World concept · Character design reference" },
    { category: "production", categoryTitle: "제작 과정 · 검증", title: "구현 및 검증 상태", headline: "완료된 구현과 남은 통합 검증을 분리", bullets: ["Backend 전체 835개 테스트 통과", "Web·Discord·Kakao 대화 연동 구현", "Local LLM 회상 정확도와 Web→UE E2E는 별도 확인"], cards: [["완료", "835 TESTS", "Backend full gate"], ["구현", "3 SURFACES", "Web · Discord · Kakao"], ["추가 확인", "E2E", "Memory recall · UE apply"]], source: "2026-08-23 프로젝트 검증 기록" },
    { category: "production", categoryTitle: "제작 과정 · 검증", title: "근거와 출처", headline: "시장 자료와 구현 근거를 구분해 관리", bullets: ["시장: Grand View Research", "사례: NVIDIA ACE · Character.AI", "구현: OpenAPI · 프로젝트 문서 · 테스트 결과"], cards: [["MARKET", "외부 자료", "규모 · 성장률"], ["CASE", "공식 사례", "대화 · 게임 AI"], ["PROJECT", "내부 근거", "계약 · 코드 · 테스트"]], source: "시장조사 메모 · share.gemini.google/Ma15pK0tJXLb" }
  ];

  const architecture = document.querySelector(".slide--multiplatform");
  architecture.dataset.scope = "main";
  architecture.dataset.title = "기술 아키텍처";
  architecture.className = "slide slide--multiplatform";
  architecture.innerHTML =
    '<div class="slide-accent"></div><div class="slide-inner slide-inner--multiplatform">' +
    '<header class="slide-header slide-header--compact multiplatform-header"><div><span class="kicker">TECH FRAMEWORK</span><h2>하나의 MAKO를 연결하는 기술 프레임워크</h2></div></header>' +
    '<div class="architecture architecture--matrix architecture--multiplatform">' +
    '<div class="architecture__column architecture__column--frontend"><span class="architecture__label">FRONTEND</span>' +
    '<article class="architecture__ue"><strong>PC</strong><p>게임 플레이 · 이동 · 전투 · 작업</p></article>' +
    '<article class="architecture__mobile"><strong>MOBILE</strong><p class="architecture__surfaces">WEB / DISCORD / KAKAO TALK</p><p>게임 밖 대화 · 기억 관리 · 요청</p></article></div>' +
    '<div class="architecture__bridge" aria-hidden="true"><span>REQUEST</span><div class="architecture__bridge-track"><i>→</i><i>←</i></div><strong>RESPONSE</strong></div>' +
    '<div class="architecture__column architecture__column--service"><span class="architecture__label">BACKEND</span>' +
    '<article class="architecture__backend"><strong>SERVER</strong><p>사용자 연결 · 데이터 관리 · 맥락 유지</p></article><div class="architecture__service-link" aria-hidden="true">↕</div>' +
    '<article class="architecture__llm"><strong>AI</strong><p>대화 생성 · 기억 회상 · 상황 이해</p></article></div></div></div><div class="slide-rule"></div>';

  document.querySelector(".slide--local-ai")?.removeAttribute("data-scope");
  document.querySelector(".slide--generative-ai")?.removeAttribute("data-scope");

  const mobile = document.querySelector(".slide--mobile-final");
  mobile.dataset.scope = "main";

  const vision = document.querySelector(".slide--final-polish");
  vision.dataset.scope = "main";
  vision.dataset.title = "Next Vision";
  vision.innerHTML =
    '<div class="slide-accent"></div><div class="slide-inner slide-inner--final-polish">' +
    '<header class="slide-header slide-header--compact polish-header"><div><span class="kicker">NEXT VISION</span><h2>게임 속 동료를,<br>더 많은 일상으로</h2></div></header>' +
    '<div class="todo-board vision-board" aria-label="AI RE의 확장 방향">' +
    '<article><span aria-hidden="true"></span><em>01</em><div><strong>게임 콘텐츠</strong><p>캐릭터 · 에피소드 DLC</p></div></article>' +
    '<article><span aria-hidden="true"></span><em>02</em><div><strong>플랫폼 확장</strong><p>Web · Discord · Kakao · Voice</p></div></article>' +
    '<article><span aria-hidden="true"></span><em>03</em><div><strong>현실로 확장</strong><p>Physical AI · 공식 IP · B2B</p></div></article></div>' +
    '<p class="polish-statement vision-statement"><strong>게임 콘텐츠</strong> → 동료 서비스 → <strong>Physical AI · IP</strong></p></div><div class="slide-rule"></div>';

  const oldAppendix = [...document.querySelectorAll('[data-scope="appendix"]')];
  oldAppendix.forEach((slide, index) => {
    if (index >= appendixPages.length) {
      slide.removeAttribute("data-scope");
      return;
    }
    const page = appendixPages[index];
    slide.className = "slide appendix-slide appendix-explain-slide";
    slide.dataset.scope = "appendix";
    slide.dataset.category = page.category;
    slide.dataset.categoryTitle = page.categoryTitle;
    slide.dataset.title = page.title;
    const paragraphs = page.bullets.map((item) => "<p>" + item + "</p>").join("");
    const facts = page.cards.map((fact) =>
      '<div class="appendix-fact"><span>' + fact[0] + '</span><strong>' + fact[1] + '</strong><p>' + fact[2] + '</p></div>'
    ).join("");
    slide.innerHTML =
      '<div class="slide-accent"></div><div class="slide-inner appendix-shell appendix-explain">' +
      '<header class="appendix-document__header"><h2>' + page.title + '</h2><p>' + page.headline + '</p></header>' +
      '<div class="appendix-document__body"><div class="appendix-prose">' + paragraphs + '</div>' +
      '<div class="appendix-facts">' + facts + '</div></div><p class="source-line">' + page.source + '</p></div><div class="slide-rule"></div>';
  });

  const mainOrder = [
    ".slide--cover",
    ".slide--overview-focus",
    ".slide--differentiation",
    ".slide--gameplay-video",
    ".slide--gameplay-loop",
    ".slide--character-design",
    ".slide--multiplatform",
    ".slide--mobile-final",
    ".team-slide",
    ".slide--final-polish",
    ".slide--closing"
  ];
  const mainSlides = mainOrder.map((selector) => document.querySelector(selector)).filter(Boolean);
  const appendixSlides = [...document.querySelectorAll('[data-scope="appendix"]')];
  const slides = [...mainSlides, ...appendixSlides];
  slides.forEach((slide) => deck.appendChild(slide));
  document.querySelectorAll(".slide:not([data-scope])").forEach((slide) => {
    slide.classList.remove("is-active", "is-before");
    slide.setAttribute("aria-hidden", "true");
  });

  const menu = document.getElementById("slideMenu");
  const menuList = document.getElementById("menuList");
  const menuToggle = document.getElementById("menuToggle");
  const menuClose = document.getElementById("menuClose");
  const menuScrim = document.getElementById("menuScrim");
  const prevButton = document.getElementById("prevButton");
  const nextButton = document.getElementById("nextButton");
  const currentNumber = document.getElementById("currentNumber");
  const totalNumber = document.getElementById("totalNumber");
  const progressBar = document.getElementById("progressBar");
  const chatDemo = document.querySelector(".chat-demo");
  const chatViewport = document.querySelector(".chat-demo__viewport");

  let currentIndex = 0;
  let touchStartX = null;
  let chatDemoTimers = [];
  let cursorIdleTimer = null;
  const formatNumber = (value) => String(value).padStart(2, "0");

  const labelFor = (slide) => slide.dataset.scope === "appendix"
    ? "A" + formatNumber(appendixSlides.indexOf(slide) + 1)
    : formatNumber(mainSlides.indexOf(slide) + 1);

  slides.forEach((slide) => slide.setAttribute("aria-label", labelFor(slide) + ". " + slide.dataset.title));

  const isPresentationFullscreen = () => document.fullscreenElement !== null ||
    (Math.abs(window.innerWidth - window.screen.width) <= 8 && Math.abs(window.innerHeight - window.screen.height) <= 8);

  const scheduleCursorHide = () => {
    window.clearTimeout(cursorIdleTimer);
    document.body.classList.remove("is-cursor-idle");
    if (!isPresentationFullscreen()) return;
    cursorIdleTimer = window.setTimeout(() => document.body.classList.add("is-cursor-idle"), 800);
  };

  const stopChatDemo = () => {
    chatDemoTimers.forEach((timer) => window.clearTimeout(timer));
    chatDemoTimers = [];
    chatDemo?.querySelectorAll("[data-chat-delay]").forEach((item) => item.classList.remove("is-visible"));
  };

  const scrollChatToBottom = (behavior = "smooth") => window.requestAnimationFrame(() => {
    chatViewport?.scrollTo({ top: chatViewport.scrollHeight, behavior });
  });

  const startChatDemo = () => {
    if (!chatDemo || !chatViewport) return;
    stopChatDemo();
    chatViewport.scrollTop = 0;
    chatDemo.querySelectorAll("[data-chat-delay]").forEach((item) => {
      const showDelay = Number(item.dataset.chatDelay);
      const hideDelay = Number(item.dataset.chatHide);
      chatDemoTimers.push(window.setTimeout(() => {
        item.classList.add("is-visible");
        scrollChatToBottom();
        chatDemoTimers.push(window.setTimeout(scrollChatToBottom, 560));
      }, showDelay));
      if (Number.isFinite(hideDelay)) {
        chatDemoTimers.push(window.setTimeout(() => item.classList.remove("is-visible"), hideDelay));
      }
    });
  };

  const setMenuOpen = (open) => {
    menu.classList.toggle("is-open", open);
    menuScrim.classList.toggle("is-open", open);
    menu.setAttribute("aria-hidden", String(!open));
    menuToggle.setAttribute("aria-expanded", String(open));
    if (open) menu.querySelector(".menu-item.is-active")?.focus();
  };

  const updateSlides = () => {
    slides.forEach((slide, index) => {
      slide.classList.toggle("is-active", index === currentIndex);
      slide.classList.toggle("is-before", index < currentIndex);
      slide.setAttribute("aria-hidden", String(index !== currentIndex));
    });
    [...menuList.querySelectorAll(".menu-item")].forEach((item) => {
      const index = Number(item.dataset.index);
      item.classList.toggle("is-active", index === currentIndex);
      item.setAttribute("aria-current", index === currentIndex ? "page" : "false");
    });
    const slide = slides[currentIndex];
    const isAppendix = slide.dataset.scope === "appendix";
    currentNumber.textContent = labelFor(slide);
    totalNumber.textContent = isAppendix ? "A" + formatNumber(appendixSlides.length) : formatNumber(mainSlides.length);
    progressBar.style.width = ((currentIndex + 1) / slides.length * 100) + "%";
    prevButton.disabled = currentIndex === 0;
    nextButton.disabled = currentIndex === slides.length - 1;
    document.body.classList.toggle("is-appendix", isAppendix);
    document.title = slide.dataset.title + " | TRAIP AI : RE";
    if (slide.contains(chatDemo)) startChatDemo(); else stopChatDemo();
  };

  const goToSlide = (index) => {
    const nextIndex = Math.max(0, Math.min(index, slides.length - 1));
    if (nextIndex === currentIndex) return;
    currentIndex = nextIndex;
    updateSlides();
  };

  const addMenuGroup = (title, groupSlides) => {
    if (!groupSlides.length) return;
    const heading = document.createElement("span");
    heading.className = "menu-group-title";
    heading.textContent = title;
    menuList.appendChild(heading);
    groupSlides.forEach((slide) => {
      const index = slides.indexOf(slide);
      const button = document.createElement("button");
      button.type = "button";
      button.className = "menu-item" + (slide.dataset.scope === "appendix" ? " menu-item--appendix" : "");
      button.dataset.index = String(index);
      button.innerHTML = "<span>" + labelFor(slide) + "</span><strong>" + slide.dataset.title + "</strong>";
      button.addEventListener("click", () => { goToSlide(index); setMenuOpen(false); });
      menuList.appendChild(button);
    });
  };

  addMenuGroup("MAIN · 7 MIN", mainSlides);
  [
    ["시장 · 사업성", "market"],
    ["게임플레이 구현", "gameplay"],
    ["AI 시스템 구조", "ai"],
    ["장기기억 · 환각 · 망각", "memory"],
    ["멀티플랫폼 연동", "platform"],
    ["제작 과정 · 검증", "production"]
  ].forEach(([title, key]) => addMenuGroup(title, appendixSlides.filter((slide) => slide.dataset.category === key)));

  prevButton.addEventListener("click", () => goToSlide(currentIndex - 1));
  nextButton.addEventListener("click", () => goToSlide(currentIndex + 1));
  menuToggle.addEventListener("click", () => setMenuOpen(!menu.classList.contains("is-open")));
  menuClose.addEventListener("click", () => setMenuOpen(false));
  menuScrim.addEventListener("click", () => setMenuOpen(false));
  window.addEventListener("resize", scheduleCursorHide);
  document.addEventListener("fullscreenchange", scheduleCursorHide);
  document.addEventListener("mousemove", scheduleCursorHide, { passive: true });

  document.addEventListener("keydown", (event) => {
    const isMenuOpen = menu.classList.contains("is-open");
    if (event.key === "Escape" && isMenuOpen) { event.preventDefault(); setMenuOpen(false); return; }
    if (event.key.toLowerCase() === "m") { event.preventDefault(); setMenuOpen(!isMenuOpen); return; }
    if (isMenuOpen) return;
    if (["ArrowRight", "ArrowDown", "PageDown", " "].includes(event.key)) { event.preventDefault(); goToSlide(currentIndex + 1); }
    else if (["ArrowLeft", "ArrowUp", "PageUp", "Backspace"].includes(event.key)) { event.preventDefault(); goToSlide(currentIndex - 1); }
    else if (event.key === "Home") { event.preventDefault(); goToSlide(0); }
    else if (event.key === "End") { event.preventDefault(); goToSlide(mainSlides.length - 1); }
  });

  document.addEventListener("touchstart", (event) => { touchStartX = event.changedTouches[0]?.clientX ?? null; }, { passive: true });
  document.addEventListener("touchend", (event) => {
    if (touchStartX === null || menu.classList.contains("is-open")) return;
    const touchEndX = event.changedTouches[0]?.clientX ?? touchStartX;
    const delta = touchEndX - touchStartX;
    touchStartX = null;
    if (Math.abs(delta) >= 50) goToSlide(currentIndex + (delta < 0 ? 1 : -1));
  }, { passive: true });

  updateSlides();
  scheduleCursorHide();
})();
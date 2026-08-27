(() => {
  const deck = document.getElementById("deck");

  const marketFit = document.createElement("section");
  marketFit.id = "market-fit";
  marketFit.className = "slide slide--market-fit";
  marketFit.dataset.scope = "main";
  marketFit.dataset.title = "시장성";
  marketFit.innerHTML =
    '<div class="slide-inner market-fit">' +
    '<header class="market-fit__header"><span class="kicker">MARKET FIT</span><h2>생존게임과 AI 캐릭터 챗의 교차점</h2></header>' +
    '<div class="market-fit__charts"><section class="market-fit__survival">' +
    '<div class="market-fit__section-title"><strong>생존게임의 대중적 인기</strong><span>대표 타이틀 판매 성과</span></div>' +
    '<div class="market-fit__bars market-fit__bars--sales"><div class="market-fit__bar"><div><strong>PALWORLD<small>공식 플레이어</small></strong><span>2,500만 명+</span></div><i style="--bar:100"></i></div>' +
    '<div class="market-fit__bar"><div><strong>ARK<small>공식 판매</small></strong><span>2,000만 장+</span></div><i style="--bar:80"></i></div>' +
    '<div class="market-fit__bar"><div><strong>ENSHROUDED<small>공식 플레이어</small></strong><span>400만 명</span></div><i style="--bar:16"></i></div></div>' +
    '<p class="market-fit__caption">대형 생존게임은 수백만~수천만 규모의 수요를 증명</p></section>' +
    '<section class="market-fit__chat"><div class="market-fit__section-title"><strong>국내 AI 캐릭터 챗</strong><span>ZETA · CRACK · 2024.04 → 2026.03</span></div>' +
    '<div class="market-fit__curve"><svg viewBox="0 0 760 230" role="img" aria-label="AI 캐릭터 챗 MAU와 총 사용시간 성장 추세"><line x1="32" y1="190" x2="728" y2="190"></line><line x1="32" y1="34" x2="32" y2="190"></line>' +
    '<path class="market-fit__curve-line market-fit__curve-line--mau" d="M32 182 C190 180 310 164 430 132 S620 75 728 54"></path><path class="market-fit__curve-line market-fit__curve-line--time" d="M32 185 C205 184 342 176 468 138 S650 48 728 24"></path>' +
    '<circle class="market-fit__curve-dot market-fit__curve-dot--mau" cx="32" cy="182" r="5"></circle><circle class="market-fit__curve-dot market-fit__curve-dot--mau" cx="728" cy="54" r="6"></circle><circle class="market-fit__curve-dot market-fit__curve-dot--time" cx="32" cy="185" r="5"></circle><circle class="market-fit__curve-dot market-fit__curve-dot--time" cx="728" cy="24" r="6"></circle></svg>' +
    '<div class="market-fit__curve-labels"><span>2024.04</span><span>2026.03</span></div><div class="market-fit__curve-legend"><p><i></i><strong>MAU</strong><span>16만 → 196만</span><em>12배</em></p><p><i></i><strong>총 사용시간</strong><span>110만h → 6,310만h</span><em>57배</em></p></div></div>' +
    '<p class="market-fit__overlap"><strong>62.5%</strong><span><b>제타 이용자 중 모바일 게임 헤비 유저</b><em>100명 중 약 63명이 모바일 게임 이용량 상위 10%에 해당</em></span></p></section></div>' +
    '<div class="market-fit__logic"><span><strong>생존게임</strong>반복되는 공동 사건</span><i>→</i><span><strong>동료</strong>경험이 관계로 축적</span><i>→</i><span><strong>LLM</strong>정해진 대사 밖의 대화</span>' +
    '<div class="market-fit__logic-actions"><button type="button" data-slide-target="market-evidence-stats">통계 근거 보기</button><button type="button" data-slide-target="market-evidence-pragmata">시장 사례 보기</button></div></div></div><div class="slide-rule"></div>';
  deck.appendChild(marketFit);
  const marketEvidencePages = [
    {
      title: "행동하는 동료의 시장 신호",
      modifier: "pragmata",
      html:
        '<div class="market-evidence__wash"></div><div class="slide-inner market-evidence">' +
        '<header class="market-evidence__header"><span>PRAGMATA · CAPCOM</span><h2>신규 IP도 동료와 함께하는 플레이로 시장을 만들었다</h2><p>프래그마타는 인간과 안드로이드 동료의 협력 플레이를 핵심 경험으로 내세운 SF 액션 어드벤처입니다.</p></header>' +
        '<div class="pragmata-market-body"><div class="pragmata-market-data">' +
        '<div class="market-timeline" aria-label="프래그마타 누적 판매 추이">' +
        '<div><em>2 DAYS</em><strong>1,000,000</strong><span>전 세계 판매</span></div><i></i>' +
        '<div><em>16 DAYS</em><strong>2,000,000</strong><span>전 세계 판매</span></div><i></i>' +
        '<div class="market-timeline__latest"><em>FY Q1</em><strong>2,510,000</strong><span>누적 판매</span></div></div>' +
        '<p class="market-evidence__meaning"><strong>2026.04.17 출시 · 완전 신규 IP</strong><span>CAPCOM은 새로운 게임성과 함께 캐릭터·세계관을 초기 판매 동력으로 평가했습니다.</span></p></div>' +
        '<figure class="market-reference market-reference--pragmata"><img src="./assets/market-pragmata-keyart.png" alt="프래그마타의 휴와 안드로이드 동료 다이애나 공식 키아트"><figcaption>HUGH × DIANA · OFFICIAL KEY ART · ©CAPCOM</figcaption></figure></div>' +
        '<p class="market-evidence__sources"><a href="https://www.capcom.co.jp/ir/english/news/html/e260420.html" target="_blank">CAPCOM · 1M / 2 days</a><span>·</span><a href="https://www.capcom.co.jp/ir/news/pdf/260507.pdf" target="_blank">CAPCOM · 2M / 16 days</a><span>·</span><a href="https://www.capcom.co.jp/ir/english/data/result_2025.html" target="_blank">CAPCOM FY2026 Q1 · 2.51M</a></p></div><div class="slide-rule"></div>'
    },
    {
      title: "출시 직후 상위권에 오른 라이자챗",
      modifier: "ryza",
      html:
        '<div class="market-evidence__wash"></div><div class="slide-inner market-evidence">' +
        '<header class="market-evidence__header"><span>RYZACHAT:AI · MARKET PROOF</span><h2>출시 직후 일본 App Store 무료 앱 종합 3위</h2><p>사전등록 급증으로 서버 증설까지 이어진, 관계형 캐릭터 서비스의 실제 시장 진입 사례입니다.</p></header>' +
        '<div class="ryza-market-body"><figure class="market-reference market-reference--ryza"><img src="./assets/market-ryzachat-demo.jpg" alt="라이자챗 AI의 캐릭터 대화와 응답 UI가 보이는 공식 데모 구동 화면"><figcaption>캐릭터 대화 · 음성 · 응답 UI · RYZACHAT:AI 공식 데모</figcaption></figure>' +
        '<div class="ryza-market-data"><div class="ryza-interest"><em>MARKET SIGNAL · 2026.08.26</em><strong>출시 직후 무료 앱 종합 3위</strong><span>사전등록 급증 → 서버 증설 → 일본 출시</span></div>' +
        '<div class="ryza-economics__pricing">' +
        '<div><span>MONTHLY</span><strong>¥980</strong><em>월 구독</em></div>' +
        '<div><span>ANNUAL</span><strong>¥6,000</strong><em>연 구독</em></div>' +
        '<div><span>TEXT</span><strong>45</strong><em>월 기본 대화 횟수</em></div>' +
        '<div><span>IAP</span><strong>¥190–3,900</strong><em>추가 대화 토큰</em></div>' +
        '<div><span>SKIN</span><strong>¥1,850</strong><em>의상 단품</em></div></div></div></div>' +
        '<div class="ryza-proof-banner"><strong>관심 → 유입 → 결제</strong><span>출시 직후 상위권 진입 · 월 구독 · 추가 대화 토큰 · 의상 아이템</span></div>' +
        '<p class="market-evidence__sources"><a href="https://ryzachat-ai.go-spiral.ai/" target="_blank">RyzaChat:AI 공식 · 화면</a><span>·</span><a href="https://apps.apple.com/jp/iphone/charts" target="_blank">Apple · 일본 무료 앱 순위</a><span>·</span><a href="https://www.4gamer.net/games/029/G102959/20260825010/" target="_blank">4Gamer · 출시</a><span>·</span><a href="https://dengekionline.com/article/202608/85461" target="_blank">전격온라인 · 가격</a></p></div><div class="slide-rule"></div>'
    },
    {
      title: "PRAGMATA + RYZACHAT = AIRE",
      modifier: "aire",
      html:
        '<div class="market-evidence__wash"></div><div class="slide-inner market-evidence">' +
        '<header class="market-evidence__header"><h2><span>PRAGMATA</span><i>+</i><span>RYZACHAT</span><i>=</i><strong>AIRE</strong></h2></header>' +
        '<div class="aire-market-flow">' +
        '<section><strong>PRAGMATA</strong><h3>함께 행동하는 동료</h3></section>' +
        '<b>+</b><section><strong>RYZACHAT</strong><h3>게임 밖에서도 이어지는 관계</h3></section>' +
        '<b>=</b><section><strong>AIRE</strong><h3>게임 안과 밖의 같은 동료</h3></section></div>' +
        '<div class="aire-market-rule"><strong>게임 안의 행동과 게임 밖의 관계를 하나의 동료로 연결</strong></div>' +
        '<p class="market-evidence__sources">해석 근거 · CAPCOM 공식 판매자료 · RYZACHAT 공식 기능 · AI : RE 현재 구현 기조</p></div><div class="slide-rule"></div>'
    },
    {
      title: "시장성 수치 출처",
      modifier: "stats",
      html:
        '<div class="market-evidence__wash"></div><div class="slide-inner market-evidence market-evidence--stats-content">' +
        '<header class="market-evidence__header"><span>MARKET FIT · SOURCE MAP</span><h2>시장성 수치 출처</h2><p>앱별 MAU 비교와 제타의 MAU·총 사용시간을 함께 배치해 시장 규모와 이용 깊이를 확인할 수 있습니다.</p></header>' +
        '<div class="stats-chart-grid">' +
        '<figure class="stats-chart-card"><figcaption><strong>AI 채팅 주요 앱 MAU 추이</strong><span>2024.04 → 2026.03 · 월별 · 단위: 만 명</span></figcaption><img src="./assets/market-ai-chat-mau-apps.png" alt="제타, 크랙, 러비더비, 케이브덕, 보리챗의 2024년 4월부터 2026년 3월까지 월간 활성 이용자 추이 그래프"><a href="https://www.igaworksblog.com/post/entertainment-platform-analysis" target="_blank" rel="noreferrer">iGAWorks · Mobile Index 분석 원문 ↗</a></figure>' +
        '<figure class="stats-chart-card stats-chart-card--zeta"><figcaption><strong>제타 MAU &amp; 총 사용시간</strong><span>2022.05 → 2026.03 · 월별</span></figcaption><img src="./assets/market-zeta-mau-usage.png" alt="제타의 월간 활성 이용자와 총 사용시간을 함께 보여주는 이중축 그래프"><a href="https://www.igaworksblog.com/post/entertainment-platform-analysis" target="_blank" rel="noreferrer">iGAWorks · Mobile Index 분석 원문 ↗</a></figure>' +
        '</div>' +
        '<section class="stats-source-panel stats-source-panel--survival stats-source-panel--compact"><header><span>SURVIVAL GAMES</span><em>본문 왼쪽 막대의 기준</em></header><div class="stats-source-list">' +
        '<article><div class="stats-source-copy"><h3>PALWORLD</h3><p>출시 1개월 · 총 플레이어</p></div><div class="stats-source-metric"><strong>2,500만+</strong><em>명</em></div><a href="https://palworld.co.jp/en/?target=about" target="_blank" rel="noreferrer">Palworld Entertainment 공식 원문 ↗</a></article>' +
        '<article><div class="stats-source-copy"><h3>ARK: SURVIVAL EVOLVED</h3><p>2020.12 공식 발표 · 판매 수량</p></div><div class="stats-source-metric"><strong>2,000만+</strong><em>장</em></div><a href="https://www.studiowildcard.com/news/category/Press%2BReleases" target="_blank" rel="noreferrer">Studio Wildcard 공식 원문 ↗</a></article>' +
        '<article><div class="stats-source-copy"><h3>ENSHROUDED</h3><p>2025.04 공식 발표 · 누적 플레이어</p></div><div class="stats-source-metric"><strong>400만+</strong><em>명</em></div><a href="https://enshrouded.com/en-US/news/4m-flameborn-update-6-teaser" target="_blank" rel="noreferrer">Keen Games 공식 원문 ↗</a></article>' +
        '</div></section>' +
        '<p class="stats-source-note"><strong>지표 정의를 구분했습니다.</strong> 판매 수량·플레이어 수·모바일 이용 지표는 서로 다른 기준이며, 본문 그래프는 각 출처의 정의를 유지합니다.</p>' +
        '<p class="market-evidence__sources"><a href="https://palworld.co.jp/en/?target=about" target="_blank" rel="noreferrer">PALWORLD</a><span>·</span><a href="https://www.studiowildcard.com/news/category/Press%2BReleases" target="_blank" rel="noreferrer">ARK</a><span>·</span><a href="https://enshrouded.com/en-US/news/4m-flameborn-update-6-teaser" target="_blank" rel="noreferrer">ENSHROUDED</a><span>·</span><a href="https://www.igaworksblog.com/post/entertainment-platform-analysis" target="_blank" rel="noreferrer">iGAWorks · Mobile Index</a></p></div><div class="slide-rule"></div>'
    }
  ];

  marketEvidencePages.forEach((page, index) => {
    const slide = document.createElement("section");
    slide.id = "market-evidence-" + page.modifier;
    slide.className = "slide market-evidence-slide market-evidence--" + page.modifier;
    slide.dataset.scope = "appendix";
    slide.dataset.appendixOrder = String(index + 5);
    slide.dataset.category = "market-evidence";
    slide.dataset.categoryTitle = "최신 시장 근거";
    slide.dataset.title = page.title;
    slide.innerHTML = page.html;
    slide.insertAdjacentHTML("beforeend", '<button type="button" class="appendix-return market-evidence__return" data-slide-target="market-fit">← 본문 07</button>');
    deck.appendChild(slide);
  });
  const mainOrder = [
    ".slide--cover",
    ".slide--overview-focus",
    ".slide--differentiation",
    ".slide--gameplay-video",
    ".slide--gameplay-loop",
    ".slide--character-design",
    ".slide--market-fit",
    ".slide--multiplatform",
    ".slide--memory-platform",
    ".slide--final-polish",
    ".slide--closing"
  ];
  const mainSlides = mainOrder.map((selector) => document.querySelector(selector)).filter(Boolean);
  const legacyAppendixSlides = [...document.querySelectorAll('[data-scope="appendix"]')]
    .filter((slide) => !slide.dataset.appendixOrder);
  legacyAppendixSlides.forEach((slide) => slide.remove());
  const appendixSlides = [...document.querySelectorAll('[data-scope="appendix"]')]
    .sort((a, b) => (Number(a.dataset.appendixOrder) || 1000) - (Number(b.dataset.appendixOrder) || 1000));
  const numberedAppendixSlides = appendixSlides.filter((slide) => slide.dataset.appendixIndex !== "false");
  const slides = [...mainSlides, ...appendixSlides];
  slides.forEach((slide) => {
    slide.dataset.print = "include";
    deck.appendChild(slide);
  });
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
  const gameplayVideoSlide = document.querySelector(".slide--gameplay-video");
  const gameplayVideo = gameplayVideoSlide?.querySelector("video");

  let currentIndex = 0;
  let touchStartX = null;
  let chatDemoTimers = [];
  let cursorIdleTimer = null;
  const formatNumber = (value) => String(value).padStart(2, "0");

  const labelFor = (slide) => slide.dataset.scope === "appendix"
    ? slide.dataset.appendixIndex === "false"
      ? "INDEX"
      : "A" + formatNumber(numberedAppendixSlides.indexOf(slide) + 1)
    : formatNumber(mainSlides.indexOf(slide) + 1);

  slides.forEach((slide) => slide.setAttribute("aria-label", labelFor(slide) + ". " + slide.dataset.title));

  const isBrowserFullscreen = () => Math.abs(window.innerWidth - window.screen.width) <= 8 &&
    Math.abs(window.innerHeight - window.screen.height) <= 8;

  const isPresentationFullscreen = () => document.fullscreenElement !== null || isBrowserFullscreen();

  const scheduleCursorHide = () => {
    window.clearTimeout(cursorIdleTimer);
    document.body.classList.remove("is-cursor-idle");
    if (!isPresentationFullscreen()) return;
    cursorIdleTimer = window.setTimeout(() => document.body.classList.add("is-cursor-idle"), 800);
  };

  const syncBrowserFullscreenLayout = () => {
    if (!isBrowserFullscreen()) document.body.classList.remove("is-gameplay-video-browser-fullscreen");
    scheduleCursorHide();
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

  const resetGameplayVideo = () => {
    if (!gameplayVideo) return;
    gameplayVideo.pause();
    const resetTime = () => { gameplayVideo.currentTime = 0; };
    if (gameplayVideo.readyState >= 1) resetTime();
    else gameplayVideo.addEventListener("loadedmetadata", resetTime, { once: true });
  };

  const requestGameplayVideoFullscreen = () => {
    if (!gameplayVideo || document.fullscreenElement === gameplayVideo) return;
    if (isBrowserFullscreen()) {
      document.body.classList.add("is-gameplay-video-browser-fullscreen");
      scheduleCursorHide();
      return;
    }
    const request = gameplayVideo.requestFullscreen?.bind(gameplayVideo) || gameplayVideo.webkitRequestFullscreen?.bind(gameplayVideo);
    if (!request) return;
    try {
      const result = request();
      result?.catch?.(() => {});
    } catch (_) {}
  };

  const leaveGameplayVideo = () => {
    if (!gameplayVideo) return;
    gameplayVideo.pause();
    document.body.classList.remove("is-gameplay-video-browser-fullscreen");
    if (document.fullscreenElement === gameplayVideo) document.exitFullscreen?.().catch?.(() => {});
    scheduleCursorHide();
  };

  const updateSlides = ({ requestVideoFullscreen = false } = {}) => {
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
    totalNumber.textContent = isAppendix ? "A" + formatNumber(numberedAppendixSlides.length) : formatNumber(mainSlides.length);
    progressBar.style.width = ((currentIndex + 1) / slides.length * 100) + "%";
    prevButton.disabled = currentIndex === 0;
    nextButton.disabled = currentIndex === slides.length - 1;
    document.body.classList.toggle("is-appendix", isAppendix);
    document.title = slide.dataset.title + " | TRAIP AI : RE";
    if (slide.contains(chatDemo)) startChatDemo(); else stopChatDemo();
    if (slide === gameplayVideoSlide) {
      resetGameplayVideo();
      if (requestVideoFullscreen) requestGameplayVideoFullscreen();
    } else {
      leaveGameplayVideo();
    }
  };

  const goToSlide = (index) => {
    const nextIndex = Math.max(0, Math.min(index, slides.length - 1));
    if (nextIndex === currentIndex) return;
    currentIndex = nextIndex;
    updateSlides({ requestVideoFullscreen: slides[nextIndex] === gameplayVideoSlide });
  };

  document.addEventListener("click", (event) => {
    const trigger = event.target.closest("[data-slide-target]");
    if (!trigger) return;
    const target = document.getElementById(trigger.dataset.slideTarget);
    const targetIndex = slides.indexOf(target);
    if (targetIndex < 0) return;
    event.preventDefault();
    goToSlide(targetIndex);
    setMenuOpen(false);
  });

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
    ["기존 본문 자료", "preserved-main"],
    ["시장 · 사업성", "market"],
    ["게임플레이 구현", "gameplay"],
    ["AI 시스템 구조", "ai"],
    ["장기기억 · 환각 · 망각", "memory"],
    ["멀티플랫폼 연동", "platform"],
    ["제작 과정 · 검증", "production"],
    ["최신 시장 근거", "market-evidence"]
  ].forEach(([title, key]) => addMenuGroup(title, appendixSlides.filter((slide) => slide.dataset.category === key)));

  prevButton.addEventListener("click", () => goToSlide(currentIndex - 1));
  nextButton.addEventListener("click", () => goToSlide(currentIndex + 1));
  menuToggle.addEventListener("click", () => setMenuOpen(!menu.classList.contains("is-open")));
  menuClose.addEventListener("click", () => setMenuOpen(false));
  menuScrim.addEventListener("click", () => setMenuOpen(false));
  window.addEventListener("resize", syncBrowserFullscreenLayout);
  document.addEventListener("fullscreenchange", scheduleCursorHide);
  document.addEventListener("mousemove", scheduleCursorHide, { passive: true });

  document.addEventListener("keydown", (event) => {
    const isMenuOpen = menu.classList.contains("is-open");
    if (event.key === "Escape" && isMenuOpen) { event.preventDefault(); setMenuOpen(false); return; }
    if (event.key === "Escape" && document.body.classList.contains("is-gameplay-video-browser-fullscreen")) {
      event.preventDefault();
      leaveGameplayVideo();
      return;
    }
    if (event.key.toLowerCase() === "m") { event.preventDefault(); setMenuOpen(!isMenuOpen); return; }
    if (isMenuOpen) return;
    if (event.key === " " && slides[currentIndex] === gameplayVideoSlide && gameplayVideo) {
      event.preventDefault();
      if (gameplayVideo.paused) gameplayVideo.play().catch(() => {});
      else gameplayVideo.pause();
      return;
    }
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

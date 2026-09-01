(() => {
  const api = window.AIREPresentation;
  if (!api) return;

  const params = new URLSearchParams(window.location.search);
  const isPresenter = params.get("presenter") === "1";
  const isAudience = params.get("audience") === "1";

  const createSessionId = () => {
    if (window.crypto?.randomUUID) return window.crypto.randomUUID();
    return `${Date.now()}-${Math.random().toString(16).slice(2)}`;
  };

  const ensureSessionId = () => {
    let sessionId = params.get("session");
    if (sessionId) return sessionId;
    sessionId = createSessionId();
    params.set("session", sessionId);
    const nextUrl = `${window.location.pathname}?${params.toString()}${window.location.hash}`;
    window.history.replaceState(null, "", nextUrl);
    return sessionId;
  };

  const enterPresenterMode = () => {
    const nextParams = new URLSearchParams(window.location.search);
    nextParams.delete("audience");
    nextParams.set("presenter", "1");
    nextParams.set("session", createSessionId());
    window.location.assign(`${window.location.pathname}?${nextParams.toString()}`);
  };

  if (!isPresenter && !isAudience) {
    const presenterButton = document.createElement("button");
    presenterButton.type = "button";
    presenterButton.className = "presenter-entry";
    presenterButton.textContent = "발표자 모드";
    presenterButton.title = "발표자 모드 열기 (P)";
    presenterButton.addEventListener("click", enterPresenterMode);
    document.body.appendChild(presenterButton);
    document.addEventListener("keydown", (event) => {
      if (event.key.toLowerCase() !== "p" || event.ctrlKey || event.altKey || event.metaKey) return;
      event.preventDefault();
      enterPresenterMode();
    });
    return;
  }

  const sessionId = ensureSessionId();
  const channel = "BroadcastChannel" in window
    ? new BroadcastChannel(`aire-presentation-${sessionId}`)
    : null;

  const postMessage = (type, payload = {}) => {
    channel?.postMessage({ type, payload, sessionId, sender: isPresenter ? "presenter" : "audience" });
  };

  const buildVideoState = () => {
    const video = api.gameplayVideo;
    if (!video) return null;
    return {
      currentTime: Number.isFinite(video.currentTime) ? video.currentTime : 0,
      paused: video.paused,
      playbackRate: video.playbackRate
    };
  };

  const applyAudienceState = async (state) => {
    if (!state || !Number.isInteger(state.index)) return;
    api.goToSlide(state.index);

    const video = api.gameplayVideo;
    if (!video || !state.video) return;
    video.muted = true;
    video.playbackRate = state.video.playbackRate || 1;
    if (Math.abs(video.currentTime - state.video.currentTime) > 0.35) {
      try {
        video.currentTime = state.video.currentTime;
      } catch (_) {}
    }
    if (state.video.paused) {
      video.pause();
      return;
    }
    try {
      await video.play();
    } catch (error) {
      postMessage("media-error", { message: error instanceof Error ? error.message : String(error) });
    }
  };

  if (isAudience) {
    document.title = "청중 화면 | TRAIP AI : RE";
    if (api.gameplayVideo) {
      api.gameplayVideo.muted = true;
      api.gameplayVideo.setAttribute("aria-hidden", "true");
    }

    const launchOverlay = document.createElement("div");
    launchOverlay.className = "audience-launch";
    launchOverlay.innerHTML = `
      <div class="audience-launch__panel">
        <span>TRAIP · AI : RE</span>
        <h1>청중 화면 준비 완료</h1>
        <p>이 창이 두 번째 모니터에 있는지 확인한 뒤 전체화면을 시작하세요.</p>
        <button type="button">전체화면 시작</button>
        <small>전체화면이 지원되지 않으면 이 안내를 닫고 F11을 누르세요.</small>
      </div>`;
    document.body.appendChild(launchOverlay);

    const launchButton = launchOverlay.querySelector("button");
    launchButton.addEventListener("click", async () => {
      try {
        if (document.documentElement.requestFullscreen) {
          await document.documentElement.requestFullscreen({ navigationUI: "hide" });
        }
      } catch (_) {
        postMessage("fullscreen-error");
      }
      launchOverlay.classList.add("is-hidden");
      postMessage("fullscreen", { active: Boolean(document.fullscreenElement) });
      window.setTimeout(() => window.opener?.focus(), 250);
    });

    document.addEventListener("fullscreenchange", () => {
      postMessage("fullscreen", { active: Boolean(document.fullscreenElement) });
      if (!document.fullscreenElement) launchOverlay.classList.remove("is-hidden");
    });

    channel?.addEventListener("message", (event) => {
      const message = event.data;
      if (message?.sessionId !== sessionId || message.sender !== "presenter") return;
      if (message.type === "state") applyAudienceState(message.payload);
    });

    window.setInterval(() => {
      postMessage("heartbeat", {
        index: api.getCurrentIndex(),
        fullscreen: Boolean(document.fullscreenElement)
      });
    }, 1000);
    postMessage("ready", { index: api.getCurrentIndex() });
    return;
  }

  const formatClock = (milliseconds) => {
    const totalSeconds = Math.max(0, Math.floor(milliseconds / 1000));
    const minutes = Math.floor(totalSeconds / 60);
    const seconds = totalSeconds % 60;
    return `${String(minutes).padStart(2, "0")}:${String(seconds).padStart(2, "0")}`;
  };

  const shell = document.createElement("div");
  shell.className = "presenter-shell";
  shell.innerHTML = `
    <header class="presenter-header">
      <div><span>TEAM TRAIP</span><strong>AI : RE 발표자 콘솔</strong></div>
      <div class="presenter-header__actions">
        <span class="presenter-connection" id="presenterConnection" data-state="offline">청중 화면 미연결</span>
        <button type="button" id="openAudienceButton">청중 화면 열기</button>
        <button type="button" id="exitPresenterButton">일반 화면</button>
      </div>
    </header>
    <div class="presenter-grid">
      <section class="presenter-preview presenter-preview--current">
        <header><span>CURRENT</span><strong id="presenterCurrentTitle">현재 슬라이드</strong></header>
        <div class="presenter-current-frame" id="presenterCurrentFrame"></div>
      </section>
      <section class="presenter-preview presenter-preview--next">
        <header><span>NEXT</span><strong id="presenterNextTitle">다음 슬라이드</strong></header>
        <div class="presenter-next-frame"><div class="presenter-next-deck" id="presenterNextDeck"></div></div>
      </section>
      <section class="presenter-notes">
        <header>
          <div><span>SPEAKER NOTES</span><strong id="presenterNotesTitle">대본 불러오는 중</strong></div>
          <em id="presenterNotesTarget">--:--</em>
        </header>
        <div class="presenter-notes__warning" id="presenterNotesWarning" hidden></div>
        <div class="presenter-notes__body" id="presenterNotesBody" aria-live="polite"></div>
      </section>
    </div>
    <footer class="presenter-footer">
      <div class="presenter-timer">
        <div><span>전체 경과</span><strong id="totalElapsed">00:00</strong><small id="totalTarget">목표 07:40</small></div>
        <div id="slideTimerCard"><span>현재 슬라이드</span><strong id="slideElapsed">00:00</strong><small id="slideTarget">목표 --:--</small></div>
      </div>
      <div class="presenter-timer__actions">
        <button type="button" id="timerToggle">시작 <kbd>T</kbd></button>
        <button type="button" id="timerReset">초기화 <kbd>Shift+R</kbd></button>
      </div>
      <div class="presenter-navigation" id="presenterNavigation"></div>
    </footer>`;
  document.body.appendChild(shell);

  const currentFrame = document.getElementById("presenterCurrentFrame");
  currentFrame.appendChild(document.getElementById("deck"));
  document.getElementById("presenterNavigation").appendChild(document.querySelector(".deck-footer"));

  const currentTitle = document.getElementById("presenterCurrentTitle");
  const nextTitle = document.getElementById("presenterNextTitle");
  const nextDeck = document.getElementById("presenterNextDeck");
  const notesTitle = document.getElementById("presenterNotesTitle");
  const notesTarget = document.getElementById("presenterNotesTarget");
  const notesBody = document.getElementById("presenterNotesBody");
  const notesWarning = document.getElementById("presenterNotesWarning");
  const connection = document.getElementById("presenterConnection");
  const totalElapsed = document.getElementById("totalElapsed");
  const slideElapsed = document.getElementById("slideElapsed");
  const slideTarget = document.getElementById("slideTarget");
  const slideTimerCard = document.getElementById("slideTimerCard");
  const timerToggle = document.getElementById("timerToggle");

  let notes = [];
  let notesLoadWarning = "";
  let audienceWindow = null;
  let audienceLastSeenAt = 0;
  let audienceFullscreen = false;
  let timerRunning = false;
  let totalElapsedBase = 0;
  let slideElapsedBase = 0;
  let timerMark = performance.now();
  let activeSlideTargetMs = null;
  let lastMediaBroadcastAt = 0;

  const appendInlineText = (container, text) => {
    const parts = text.split(/(\*\*[^*]+\*\*)/g).filter(Boolean);
    parts.forEach((part) => {
      if (part.startsWith("**") && part.endsWith("**")) {
        const strong = document.createElement("strong");
        strong.textContent = part.slice(2, -2);
        container.appendChild(strong);
      } else {
        container.appendChild(document.createTextNode(part));
      }
    });
  };

  const renderNoteBody = (body) => {
    notesBody.replaceChildren();
    const lines = body.split(/\r?\n/).map((line) => line.trim()).filter(Boolean);
    lines.forEach((line) => {
      const paragraph = document.createElement("p");
      if (/^\[.+\]$/.test(line)) paragraph.className = "presenter-note-cue";
      appendInlineText(paragraph, line);
      notesBody.appendChild(paragraph);
    });
  };

  const parseNotes = (markdown) => {
    const headingPattern = /^##\s+(\d+)\.\s+(.+?)\s+—\s+(?:약\s*)?(\d+):(\d+)\s*$/gm;
    const headings = [...markdown.matchAll(headingPattern)];
    return headings.map((match, index) => {
      const bodyStart = match.index + match[0].length;
      const bodyEnd = headings[index + 1]?.index ?? markdown.length;
      const body = markdown.slice(bodyStart, bodyEnd)
        .split(/\r?\n/)
        .filter((line) => line.trim() !== "---")
        .join("\n")
        .trim();
      const minutes = Number(match[3]);
      const seconds = Number(match[4]);
      return {
        number: Number(match[1]),
        title: match[2].trim(),
        durationSeconds: minutes * 60 + seconds,
        body
      };
    });
  };

  const loadNotes = async () => {
    try {
      const response = await fetch("./대본.md", { cache: "no-store" });
      if (!response.ok) throw new Error(`HTTP ${response.status}`);
      notes = parseNotes(await response.text());
      if (notes.length !== api.mainSlides.length) {
        notesLoadWarning = `대본 ${notes.length}개와 본문 슬라이드 ${api.mainSlides.length}개의 개수가 다릅니다.`;
      } else if (notes.some((note, index) => note.number !== index + 1)) {
        notesLoadWarning = "대본 번호가 1부터 순서대로 이어지지 않습니다.";
      }
      const totalTargetSeconds = notes.reduce((sum, note) => sum + note.durationSeconds, 0);
      document.getElementById("totalTarget").textContent = `목표 ${formatClock(totalTargetSeconds * 1000)}`;
    } catch (error) {
      notes = [];
      notesLoadWarning = `대본을 불러오지 못했습니다: ${error instanceof Error ? error.message : String(error)}`;
    }
    renderPresenterState();
  };

  const sanitizeClone = (slide) => {
    const clone = slide.cloneNode(true);
    clone.removeAttribute("id");
    clone.querySelectorAll("[id]").forEach((element) => element.removeAttribute("id"));
    clone.classList.remove("is-before");
    clone.classList.add("is-active");
    clone.setAttribute("aria-hidden", "false");
    clone.querySelectorAll("button, a, input, video").forEach((element) => {
      element.setAttribute("tabindex", "-1");
      element.setAttribute("aria-hidden", "true");
    });
    clone.querySelectorAll("video").forEach((video) => {
      video.removeAttribute("controls");
      video.preload = "metadata";
      video.muted = true;
    });
    return clone;
  };

  const renderPresenterState = () => {
    const index = api.getCurrentIndex();
    const currentSlide = api.slides[index];
    const nextSlide = api.slides[index + 1];
    currentTitle.textContent = `${api.labelFor(currentSlide)} · ${currentSlide.dataset.title}`;
    nextTitle.textContent = nextSlide ? `${api.labelFor(nextSlide)} · ${nextSlide.dataset.title}` : "발표 종료";
    nextDeck.replaceChildren();
    if (nextSlide) nextDeck.appendChild(sanitizeClone(nextSlide));

    const mainIndex = api.mainSlides.indexOf(currentSlide);
    const currentNote = mainIndex >= 0 ? notes[mainIndex] : null;
    if (currentNote) {
      notesTitle.textContent = `${currentNote.number}. ${currentNote.title}`;
      notesTarget.textContent = formatClock(currentNote.durationSeconds * 1000);
      activeSlideTargetMs = currentNote.durationSeconds * 1000;
      renderNoteBody(currentNote.body);
    } else {
      notesTitle.textContent = mainIndex >= 0 ? "대본을 불러오는 중" : "등록된 대본 없음";
      notesTarget.textContent = "--:--";
      activeSlideTargetMs = null;
      renderNoteBody(mainIndex >= 0 && !notesLoadWarning ? "대본을 불러오고 있습니다." : "이 슬라이드에 연결된 발표 대본이 없습니다.");
    }
    slideTarget.textContent = activeSlideTargetMs === null ? "목표 --:--" : `목표 ${formatClock(activeSlideTargetMs)}`;
    notesWarning.hidden = !notesLoadWarning;
    notesWarning.textContent = notesLoadWarning;
  };

  const getTimerValues = () => {
    const liveDelta = timerRunning ? performance.now() - timerMark : 0;
    return {
      total: totalElapsedBase + liveDelta,
      slide: slideElapsedBase + liveDelta
    };
  };

  const commitTimerDelta = () => {
    if (!timerRunning) return;
    const now = performance.now();
    const delta = now - timerMark;
    totalElapsedBase += delta;
    slideElapsedBase += delta;
    timerMark = now;
  };

  const toggleTimer = () => {
    if (timerRunning) {
      commitTimerDelta();
      timerRunning = false;
      timerToggle.innerHTML = "계속 <kbd>T</kbd>";
    } else {
      timerMark = performance.now();
      timerRunning = true;
      timerToggle.innerHTML = "일시정지 <kbd>T</kbd>";
    }
  };

  const resetTimer = () => {
    totalElapsedBase = 0;
    slideElapsedBase = 0;
    timerMark = performance.now();
    timerRunning = false;
    timerToggle.innerHTML = "시작 <kbd>T</kbd>";
  };

  const updateTimerDisplay = () => {
    const values = getTimerValues();
    totalElapsed.textContent = formatClock(values.total);
    slideElapsed.textContent = formatClock(values.slide);
    slideTimerCard.classList.toggle("is-over-time", activeSlideTargetMs !== null && values.slide > activeSlideTargetMs);
    window.requestAnimationFrame(updateTimerDisplay);
  };

  const broadcastState = () => {
    postMessage("state", {
      index: api.getCurrentIndex(),
      video: buildVideoState()
    });
  };

  const setConnectionState = () => {
    const connected = Date.now() - audienceLastSeenAt < 3500;
    connection.dataset.state = connected ? "online" : "offline";
    if (!connected) {
      connection.textContent = audienceWindow?.closed ? "청중 화면 닫힘" : "청중 화면 미연결";
      return;
    }
    connection.textContent = audienceFullscreen ? "청중 화면 · 전체화면" : "청중 화면 · 연결됨";
  };

  const openAudience = async () => {
    if (audienceWindow && !audienceWindow.closed) {
      audienceWindow.focus();
      broadcastState();
      return;
    }

    const audienceParams = new URLSearchParams();
    audienceParams.set("audience", "1");
    audienceParams.set("session", sessionId);
    const audienceUrl = `${window.location.pathname}?${audienceParams.toString()}`;
    audienceWindow = window.open("about:blank", `aire-audience-${sessionId}`, "popup=yes,width=1280,height=720");
    if (!audienceWindow) {
      connection.dataset.state = "error";
      connection.textContent = "팝업 차단됨 · 허용 후 재시도";
      return;
    }

    try {
      if ("getScreenDetails" in window) {
        const details = await window.getScreenDetails();
        const targetScreen = details.screens.find((screen) => screen !== details.currentScreen);
        if (targetScreen) {
          audienceWindow.moveTo(targetScreen.availLeft, targetScreen.availTop);
          audienceWindow.resizeTo(targetScreen.availWidth, targetScreen.availHeight);
        } else {
          connection.textContent = "보조 화면 없음 · 창을 수동 배치하세요";
        }
      } else {
        connection.textContent = "창을 두 번째 모니터로 수동 배치하세요";
      }
    } catch (_) {
      connection.textContent = "화면 권한 없음 · 창을 수동 배치하세요";
    }
    audienceWindow.location.replace(audienceUrl);
  };

  channel?.addEventListener("message", (event) => {
    const message = event.data;
    if (message?.sessionId !== sessionId || message.sender !== "audience") return;
    if (["ready", "heartbeat", "fullscreen", "media-error", "fullscreen-error"].includes(message.type)) {
      audienceLastSeenAt = Date.now();
    }
    if (message.type === "ready") broadcastState();
    if (message.type === "heartbeat" || message.type === "fullscreen") {
      audienceFullscreen = Boolean(message.payload.fullscreen ?? message.payload.active);
    }
    if (message.type === "media-error") {
      connection.dataset.state = "error";
      connection.textContent = "청중 영상 재생 차단됨";
    }
    if (message.type === "fullscreen-error") {
      connection.dataset.state = "error";
      connection.textContent = "전체화면 실패 · 청중 창에서 F11";
    }
    setConnectionState();
  });

  document.addEventListener("aire:slidechange", () => {
    commitTimerDelta();
    slideElapsedBase = 0;
    timerMark = performance.now();
    renderPresenterState();
    broadcastState();
  });

  if (api.gameplayVideo) {
    ["play", "pause", "seeked", "ratechange"].forEach((eventName) => {
      api.gameplayVideo.addEventListener(eventName, broadcastState);
    });
    api.gameplayVideo.addEventListener("timeupdate", () => {
      const now = performance.now();
      if (now - lastMediaBroadcastAt < 400) return;
      lastMediaBroadcastAt = now;
      broadcastState();
    });
  }

  document.getElementById("openAudienceButton").addEventListener("click", openAudience);
  document.getElementById("exitPresenterButton").addEventListener("click", () => {
    const normalParams = new URLSearchParams(window.location.search);
    normalParams.delete("presenter");
    normalParams.delete("session");
    const query = normalParams.toString();
    window.location.assign(`${window.location.pathname}${query ? `?${query}` : ""}`);
  });
  timerToggle.addEventListener("click", toggleTimer);
  document.getElementById("timerReset").addEventListener("click", resetTimer);

  document.addEventListener("keydown", (event) => {
    if (event.ctrlKey || event.altKey || event.metaKey) return;
    if (event.key.toLowerCase() === "t") {
      event.preventDefault();
      event.stopImmediatePropagation();
      toggleTimer();
    } else if (event.key.toLowerCase() === "r" && event.shiftKey) {
      event.preventDefault();
      event.stopImmediatePropagation();
      resetTimer();
    }
  }, true);

  window.setInterval(() => {
    setConnectionState();
    if (audienceWindow && !audienceWindow.closed) broadcastState();
  }, 2000);

  renderPresenterState();
  loadNotes();
  updateTimerDisplay();
})();

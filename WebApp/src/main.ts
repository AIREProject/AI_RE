import "./style.css";

import {
  ApiClientError,
  createMobileChat,
  type MobileChatRequest,
} from "./api/client";
import { apiBaseUrl, companionId, saveSlotId } from "./config";

type ChatUiState = "idle" | "sending" | "success" | "error";

function createStableId(prefix: string): string {
  if (typeof crypto.randomUUID === "function") {
    return `${prefix}-${crypto.randomUUID()}`;
  }

  const bytes = crypto.getRandomValues(new Uint8Array(16));
  bytes[6] = ((bytes[6] ?? 0) & 0x0f) | 0x40;
  bytes[8] = ((bytes[8] ?? 0) & 0x3f) | 0x80;
  const hex = Array.from(bytes, (byte) => byte.toString(16).padStart(2, "0"));
  const uuid = [
    hex.slice(0, 4).join(""),
    hex.slice(4, 6).join(""),
    hex.slice(6, 8).join(""),
    hex.slice(8, 10).join(""),
    hex.slice(10, 16).join(""),
  ].join("-");
  return `${prefix}-${uuid}`;
}

function requireElement<T extends Element>(selector: string): T {
  const element = document.querySelector<T>(selector);
  if (element === null) {
    throw new Error(`Required element was not found: ${selector}`);
  }
  return element;
}

function formatCurrentTime(): string {
  return new Intl.DateTimeFormat("ko-KR", {
    hour: "2-digit",
    minute: "2-digit",
  }).format(new Date());
}

function createRealWorldTimeContext(now: Date): MobileChatRequest["time_context"] {
  const hour = now.getHours();
  return {
    source: "RealWorld",
    day: now.getDate(),
    hour,
    period: hour < 12 ? "AM" : "PM",
  };
}

const app = requireElement<HTMLElement>("#app");

app.innerHTML = `
  <div id="companion-app" class="companion-app" data-chat-state="idle">
    <header class="app-header">
      <div class="companion-avatar" aria-hidden="true">
        <span>MA</span>
        <span class="status-dot" data-state="connected"></span>
      </div>
      <div class="header-copy">
        <p class="companion-label">나의 동료</p>
        <h1>MAKO</h1>
        <p class="connection-status" data-state="connected">모바일 대화 준비됨</p>
      </div>
    </header>

    <nav class="app-tabs" aria-label="동료 메뉴">
      <button class="tab-button active" type="button" data-view="chat" aria-selected="true">
        <svg viewBox="0 0 24 24" aria-hidden="true"><path d="M21 15a4 4 0 0 1-4 4H8l-5 3V7a4 4 0 0 1 4-4h10a4 4 0 0 1 4 4Z"/></svg>
        대화
      </button>
      <button class="tab-button" type="button" data-view="memory" aria-selected="false">
        <svg viewBox="0 0 24 24" aria-hidden="true"><path d="M12 21a9 9 0 1 0-9-9v7l2.5-2.5M8 12h8M12 8v8"/></svg>
        기억
      </button>
    </nav>

    <main id="chat-view" class="app-view active" data-view-panel="chat">
      <section id="chat-messages" class="chat-messages" aria-label="대화 내용" aria-live="polite">
        <div class="date-divider"><span>오늘</span></div>
        <article class="message companion-message">
          <div class="message-bubble">
            <p>여기서 나눈 이야기는 다음 모험에서도 이어져. 오늘은 어떤 하루였어?</p>
          </div>
          <time>방금</time>
        </article>
      </section>

      <section class="composer-area">
        <p class="time-context"><span aria-hidden="true">◷</span>현실 시간 기준으로 대화해요</p>
        <div class="suggestion-list" aria-label="추천 대화">
          <button type="button" class="suggestion-chip">철 도끼 제작 방법</button>
          <button type="button" class="suggestion-chip">나무 100개만 캐줘</button>
        </div>
        <form id="chat-form" class="chat-composer" data-state="idle" aria-busy="false">
          <label class="sr-only" for="chat-input">MAKO에게 메시지 보내기</label>
          <textarea id="chat-input" rows="1" maxlength="2000" placeholder="MAKO에게 이야기하기"></textarea>
          <button id="send-button" class="send-button" type="submit" aria-label="메시지 보내기">
            <svg viewBox="0 0 24 24" aria-hidden="true"><path d="m22 2-7 20-4-9-9-4Z"/><path d="M22 2 11 13"/></svg>
          </button>
        </form>
        <p id="composer-notice" class="composer-notice" data-state="idle" aria-live="polite" hidden></p>
      </section>
    </main>

    <main id="memory-view" class="app-view" data-view-panel="memory" hidden>
      <section class="memory-intro">
        <span class="memory-mark" aria-hidden="true">✦</span>
        <p class="section-kicker">함께 쌓은 기억</p>
        <h2>MAKO가 기억하고 있는 이야기</h2>
        <p>직접 들려준 이야기만 저장 후보가 되며, 언제든 확인하고 지울 수 있어요.</p>
      </section>
      <section class="empty-card">
        <div class="empty-icon" aria-hidden="true">✧</div>
        <h3>아직 저장된 기억이 없어요</h3>
        <p>대화를 이어가면 소중한 이야기가 이곳에 나타나요.</p>
        <span class="development-badge">Memory API 연결 예정</span>
      </section>
    </main>
  </div>
`;

const companionApp = requireElement<HTMLElement>("#companion-app");
const chatMessages = requireElement<HTMLElement>("#chat-messages");
const chatForm = requireElement<HTMLFormElement>("#chat-form");
const chatInput = requireElement<HTMLTextAreaElement>("#chat-input");
const sendButton = requireElement<HTMLButtonElement>("#send-button");
const composerNotice = requireElement<HTMLElement>("#composer-notice");

const sessionId = createStableId("session");
let isSending = false;

function setChatState(state: ChatUiState, message?: string): void {
  companionApp.dataset.chatState = state;
  chatForm.dataset.state = state;
  chatForm.setAttribute("aria-busy", String(state === "sending"));
  composerNotice.dataset.state = state;

  if (message === undefined) {
    composerNotice.textContent = "";
    composerNotice.hidden = true;
    return;
  }
  composerNotice.textContent = message;
  composerNotice.hidden = false;
}

function appendMessage(text: string, kind: "user" | "companion"): void {
  const message = document.createElement("article");
  message.className = `message ${kind}-message`;
  const bubble = document.createElement("div");
  bubble.className = "message-bubble";
  const content = document.createElement("p");
  content.textContent = text;
  bubble.append(content);
  const time = document.createElement("time");
  time.textContent = formatCurrentTime();
  message.append(bubble, time);
  chatMessages.append(message);
  chatMessages.scrollTop = chatMessages.scrollHeight;
}

function chatFailureMessage(error: ApiClientError): string {
  switch (error.kind) {
    case "timeout":
      return "MAKO의 응답 시간이 초과됐어요. 자동으로 다시 보내지는 않았어요.";
    case "network":
      return "Backend에 연결할 수 없어요. 네트워크 상태를 확인해 주세요.";
    case "invalid-json":
      return "Backend 응답을 JSON으로 읽을 수 없어요.";
    case "invalid-success-response":
      return "Backend의 성공 응답 형식이나 요청 식별자가 올바르지 않아요.";
    case "invalid-error-response":
      return "Backend의 오류 응답 형식이나 요청 식별자가 올바르지 않아요.";
    case "unauthorized":
      return "Web 인증이 거부됐어요. Backend의 AIRE_WEB 설정을 확인해 주세요.";
    case "forbidden":
      return "현재 Web 신원에는 이 대화 요청 권한이 없어요.";
    case "server-error": {
      const publicMessage = error.publicMessage === null ? "" : ` ${error.publicMessage}`;
      return `Backend가 대화 요청을 처리하지 못했어요 (${error.code}).${publicMessage}`;
    }
  }
}

document.querySelectorAll<HTMLButtonElement>(".tab-button").forEach((button) => {
  button.addEventListener("click", () => {
    const target = button.dataset.view;
    if (target === undefined) {
      return;
    }
    document.querySelectorAll<HTMLButtonElement>(".tab-button").forEach((tab) => {
      const active = tab === button;
      tab.classList.toggle("active", active);
      tab.setAttribute("aria-selected", String(active));
    });
    document.querySelectorAll<HTMLElement>("[data-view-panel]").forEach((panel) => {
      const active = panel.dataset.viewPanel === target;
      panel.classList.toggle("active", active);
      panel.hidden = !active;
    });
  });
});

document.querySelectorAll<HTMLButtonElement>(".suggestion-chip").forEach((button) => {
  button.addEventListener("click", () => {
    chatInput.value = button.textContent?.trim() ?? "";
    chatInput.dispatchEvent(new Event("input"));
    chatInput.focus();
  });
});

chatInput.addEventListener("input", () => {
  chatInput.style.height = "auto";
  chatInput.style.height = `${Math.min(chatInput.scrollHeight, 120)}px`;
  if (!isSending) {
    setChatState("idle");
  }
});

chatInput.addEventListener("keydown", (event) => {
  if (event.key === "Enter" && !event.shiftKey) {
    event.preventDefault();
    chatForm.requestSubmit();
  }
});

chatForm.addEventListener("submit", (event) => {
  event.preventDefault();
  if (isSending) {
    return;
  }

  const message = chatInput.value.trim();
  if (message.length === 0) {
    return;
  }

  isSending = true;
  sendButton.disabled = true;
  chatInput.disabled = true;
  setChatState("sending", "MAKO가 답을 준비하고 있어요…");
  appendMessage(message, "user");
  chatInput.value = "";
  chatInput.style.height = "auto";

  const request: MobileChatRequest = {
    request_id: createStableId("request"),
    schema_version: 1,
    session_id: sessionId,
    save_slot_id: saveSlotId,
    companion_id: companionId,
    message_id: createStableId("message"),
    user_message: message,
    surface: "mobile",
    time_context: createRealWorldTimeContext(new Date()),
    allowed_commands: [],
  };

  void createMobileChat(apiBaseUrl, request)
    .then((response) => {
      appendMessage(response.display_text, "companion");
      setChatState("success");
    })
    .catch((error: unknown) => {
      if (error instanceof ApiClientError) {
        setChatState("error", chatFailureMessage(error));
        return;
      }
      setChatState("error", "대화 요청 중 알 수 없는 오류가 발생했어요.");
    })
    .finally(() => {
      isSending = false;
      sendButton.disabled = false;
      chatInput.disabled = false;
      chatInput.focus();
    });
});

setChatState("idle");

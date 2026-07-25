import "./style.css";

import {
  ApiClientError,
  createOfflineChat,
  getCurrentDevice,
  pairWebDevice,
  revokeCurrentDevice,
  type OfflineChatRequest,
} from "./api/client";
import {
  clearDeviceCredentials,
  loadDeviceCredentials,
  saveDeviceCredentials,
  type DeviceCredentials,
} from "./auth/credentials";
import { apiBaseUrl, companionId, saveSlotId } from "./config";

type ConnectionState = "checking" | "connected" | "disconnected";
type PairingState =
  | "idle"
  | "pairing"
  | "success"
  | "expired"
  | "used"
  | "invalid"
  | "backend-unavailable"
  | "storage-failed";

interface PairingFragment {
  recognized: boolean;
  code: string | null;
}

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

function consumePairingFragment(): PairingFragment {
  const prefix = "#/pair/";
  if (!window.location.hash.startsWith(prefix)) {
    return { recognized: false, code: null };
  }

  const candidate = window.location.hash.slice(prefix.length);
  window.history.replaceState(
    window.history.state,
    "",
    `${window.location.pathname}${window.location.search}`,
  );
  return {
    recognized: true,
    code: /^[0-9]{8}$/.test(candidate) ? candidate : null,
  };
}

const app = requireElement<HTMLElement>("#app");

app.innerHTML = `
  <section id="pairing-shell" class="pairing-shell" aria-labelledby="pairing-title">
    <div class="pairing-card">
      <div class="pairing-avatar" aria-hidden="true">AI</div>
      <p class="section-kicker">AIRE WEB CONNECTION</p>
      <h1 id="pairing-title">동료와 연결하기</h1>
      <p id="pairing-status" class="pairing-status" data-state="idle" aria-live="polite">
        게임에 표시된 QR을 열거나 8자리 Pairing Code를 입력해 주세요.
      </p>
      <form id="pairing-form" class="pairing-form">
        <label for="pairing-code">Pairing Code</label>
        <input
          id="pairing-code"
          name="pairing-code"
          type="text"
          inputmode="numeric"
          autocomplete="one-time-code"
          maxlength="8"
          pattern="[0-9]{8}"
          placeholder="8자리 숫자"
        />
        <button id="pairing-submit" type="submit">연결하기</button>
      </form>
      <button id="retry-authentication" class="secondary-button" type="button" hidden>
        다시 확인하기
      </button>
      <p class="pairing-help">Pairing Code는 5분 동안 한 번만 사용할 수 있어요.</p>
    </div>
  </section>

  <div id="companion-app" class="companion-app" hidden>
    <header class="app-header">
      <div class="companion-avatar" aria-hidden="true">
        <span>AI</span>
        <span id="status-dot" class="status-dot" data-state="checking"></span>
      </div>
      <div class="header-copy">
        <p class="companion-label">나의 동료</p>
        <h1>AIRE</h1>
        <p id="connection-status" class="connection-status" aria-live="polite">연결 확인 중</p>
      </div>
      <button id="refresh-connection" class="icon-button" type="button" aria-label="연결 상태 다시 확인">
        <svg viewBox="0 0 24 24" aria-hidden="true"><path d="M20 12a8 8 0 1 1-2.34-5.66M20 4v6h-6"/></svg>
      </button>
      <button id="disconnect-device" class="icon-button disconnect-button" type="button" aria-label="기기 연결 해제">
        <svg viewBox="0 0 24 24" aria-hidden="true"><path d="M10 17l5-5-5-5M15 12H3M14 3h5a2 2 0 0 1 2 2v14a2 2 0 0 1-2 2h-5"/></svg>
      </button>
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
      <p id="auth-notice" class="auth-notice" data-state="success" aria-live="polite" hidden></p>
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
          <button type="button" class="suggestion-chip">오늘 있었던 일 이야기하기</button>
          <button type="button" class="suggestion-chip">다음 모험 얘기하기</button>
        </div>
        <form id="chat-form" class="chat-composer">
          <label class="sr-only" for="chat-input">AIRE에게 메시지 보내기</label>
          <textarea id="chat-input" rows="1" maxlength="1000" placeholder="AIRE에게 이야기하기"></textarea>
          <button id="send-button" class="send-button" type="submit" aria-label="메시지 보내기">
            <svg viewBox="0 0 24 24" aria-hidden="true"><path d="m22 2-7 20-4-9-9-4Z"/><path d="M22 2 11 13"/></svg>
          </button>
        </form>
        <p id="composer-notice" class="composer-notice" hidden></p>
      </section>
    </main>

    <main id="memory-view" class="app-view" data-view-panel="memory" hidden>
      <section class="memory-intro">
        <span class="memory-mark" aria-hidden="true">✦</span>
        <p class="section-kicker">함께 쌓은 기억</p>
        <h2>AIRE가 기억하고 있는 이야기</h2>
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

const pairingShell = requireElement<HTMLElement>("#pairing-shell");
const pairingStatus = requireElement<HTMLElement>("#pairing-status");
const pairingForm = requireElement<HTMLFormElement>("#pairing-form");
const pairingInput = requireElement<HTMLInputElement>("#pairing-code");
const pairingSubmit = requireElement<HTMLButtonElement>("#pairing-submit");
const retryAuthentication = requireElement<HTMLButtonElement>(
  "#retry-authentication",
);
const companionApp = requireElement<HTMLElement>("#companion-app");
const connectionStatus = requireElement<HTMLElement>("#connection-status");
const statusDot = requireElement<HTMLElement>("#status-dot");
const refreshButton = requireElement<HTMLButtonElement>("#refresh-connection");
const disconnectButton = requireElement<HTMLButtonElement>("#disconnect-device");
const authNotice = requireElement<HTMLElement>("#auth-notice");
const chatMessages = requireElement<HTMLElement>("#chat-messages");
const chatForm = requireElement<HTMLFormElement>("#chat-form");
const chatInput = requireElement<HTMLTextAreaElement>("#chat-input");
const sendButton = requireElement<HTMLButtonElement>("#send-button");
const composerNotice = requireElement<HTMLElement>("#composer-notice");

const sessionId = createStableId("session");
const initialFragment = consumePairingFragment();
let currentCredentials: DeviceCredentials | null = null;
let pendingFragmentCode = initialFragment.code;
let isPairing = false;
let isSending = false;

function setConnectionState(state: ConnectionState, label: string): void {
  statusDot.dataset.state = state;
  connectionStatus.dataset.state = state;
  connectionStatus.textContent = label;
}

function setPairingState(state: PairingState, message: string): void {
  pairingShell.hidden = false;
  companionApp.hidden = true;
  pairingStatus.dataset.state = state;
  pairingStatus.textContent = message;
  const busy = state === "pairing";
  pairingInput.disabled = busy;
  pairingSubmit.disabled = busy;
  retryAuthentication.hidden = state !== "backend-unavailable";
}

function showChat(credentials: DeviceCredentials, pairedNow: boolean): void {
  currentCredentials = credentials;
  pairingShell.hidden = true;
  companionApp.hidden = false;
  setConnectionState("connected", "C PC 인증됨");
  authNotice.hidden = !pairedNow;
  if (pairedNow) {
    authNotice.textContent = "Pairing이 완료됐어요. 다음 접속부터 자동으로 연결할게요.";
  }
}

function showPairingIdle(message?: string): void {
  currentCredentials = null;
  setPairingState(
    "idle",
    message ?? "게임에 표시된 QR을 열거나 8자리 Pairing Code를 입력해 주세요.",
  );
  pairingInput.focus();
}

function handleUnauthorized(message: string): void {
  clearDeviceCredentials();
  showPairingIdle(message);
}

function formatCurrentTime(): string {
  return new Intl.DateTimeFormat("ko-KR", {
    hour: "2-digit",
    minute: "2-digit",
  }).format(new Date());
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

function showComposerNotice(message: string): void {
  composerNotice.textContent = message;
  composerNotice.hidden = false;
}

function pairingErrorMessage(error: ApiClientError): [PairingState, string] {
  switch (error.code) {
    case "ExpiredPairingCode":
      return ["expired", "Pairing Code가 만료됐어요. 게임에서 새 Code를 발급해 주세요."];
    case "UsedPairingCode":
      return ["used", "이미 사용된 Pairing Code예요. 새 Code를 발급해 주세요."];
    case "InvalidPairingCode":
    case "InvalidRequest":
      return ["invalid", "올바르지 않은 Pairing Code예요. 8자리 숫자를 확인해 주세요."];
    default:
      return [
        "backend-unavailable",
        error.retryable
          ? "Backend가 응답하지 않아요. 잠시 후 다시 시도해 주세요."
          : "Backend 응답을 확인할 수 없어요. 연결 상태를 확인해 주세요.",
      ];
  }
}

async function pairWithCode(pairingCode: string): Promise<void> {
  if (isPairing) {
    return;
  }
  isPairing = true;
  pairingInput.value = "";
  setPairingState("pairing", "동료와 안전하게 연결하고 있어요…");

  try {
    const response = await pairWebDevice(
      apiBaseUrl,
      pairingCode,
      createStableId("request-pair"),
    );
    const credentials: DeviceCredentials = {
      schema_version: 1,
      profile_id: response.profile_id,
      device_id: response.device.device_id,
      device_token: response.device_token,
    };
    try {
      saveDeviceCredentials(credentials);
    } catch {
      setPairingState(
        "storage-failed",
        "브라우저 저장소를 사용할 수 없어 연결 정보를 보관하지 못했어요.",
      );
      return;
    }
    pendingFragmentCode = null;
    pairingStatus.dataset.state = "success";
    showChat(credentials, true);
  } catch (error: unknown) {
    if (error instanceof ApiClientError) {
      const [state, message] = pairingErrorMessage(error);
      setPairingState(state, message);
    } else {
      setPairingState(
        "backend-unavailable",
        "C PC Backend에 연결할 수 없어요. 네트워크를 확인해 주세요.",
      );
    }
  } finally {
    isPairing = false;
  }
}

async function restoreAuthentication(
  credentials: DeviceCredentials,
): Promise<void> {
  setPairingState("pairing", "저장된 연결을 확인하고 있어요…");
  try {
    const response = await getCurrentDevice(
      apiBaseUrl,
      credentials.device_token,
      createStableId("request-device-me"),
    );
    const normalized: DeviceCredentials = {
      schema_version: 1,
      profile_id: response.profile_id,
      device_id: response.device_id,
      device_token: credentials.device_token,
    };
    saveDeviceCredentials(normalized);
    pendingFragmentCode = null;
    showChat(normalized, false);
  } catch (error: unknown) {
    if (error instanceof ApiClientError && error.code === "UnauthorizedDevice") {
      clearDeviceCredentials();
      if (pendingFragmentCode !== null) {
        await pairWithCode(pendingFragmentCode);
        return;
      }
      if (initialFragment.recognized) {
        setPairingState(
          "invalid",
          "Pairing 링크가 올바르지 않아요. 새 QR을 열거나 Code를 직접 입력해 주세요.",
        );
        return;
      }
      showPairingIdle("저장된 연결이 만료되거나 폐기됐어요. 다시 Pairing해 주세요.");
      return;
    }

    setPairingState(
      "backend-unavailable",
      "저장된 연결 정보는 유지했어요. C PC Backend 연결을 확인해 주세요.",
    );
  }
}

async function startAuthentication(): Promise<void> {
  const stored = loadDeviceCredentials();
  if (stored !== null) {
    await restoreAuthentication(stored);
    return;
  }
  if (pendingFragmentCode !== null) {
    await pairWithCode(pendingFragmentCode);
    return;
  }
  if (initialFragment.recognized) {
    setPairingState(
      "invalid",
      "Pairing 링크가 올바르지 않아요. 새 QR을 열거나 Code를 직접 입력해 주세요.",
    );
    return;
  }
  showPairingIdle();
}

async function checkConnection(): Promise<void> {
  const credentials = currentCredentials;
  if (credentials === null) {
    await startAuthentication();
    return;
  }
  refreshButton.disabled = true;
  setConnectionState("checking", "인증 확인 중");
  try {
    const response = await getCurrentDevice(
      apiBaseUrl,
      credentials.device_token,
      createStableId("request-device-me"),
    );
    const normalized: DeviceCredentials = {
      schema_version: 1,
      profile_id: response.profile_id,
      device_id: response.device_id,
      device_token: credentials.device_token,
    };
    saveDeviceCredentials(normalized);
    currentCredentials = normalized;
    setConnectionState("connected", "C PC 인증됨");
  } catch (error: unknown) {
    if (error instanceof ApiClientError && error.code === "UnauthorizedDevice") {
      handleUnauthorized("기기 연결이 폐기됐어요. 다시 Pairing해 주세요.");
      return;
    }
    setConnectionState("disconnected", "C PC에 연결할 수 없음");
  } finally {
    refreshButton.disabled = false;
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
    chatInput.focus();
  });
});

pairingInput.addEventListener("input", () => {
  pairingInput.value = pairingInput.value.replace(/[^0-9]/g, "").slice(0, 8);
});

pairingForm.addEventListener("submit", (event) => {
  event.preventDefault();
  const pairingCode = pairingInput.value;
  if (!/^[0-9]{8}$/.test(pairingCode)) {
    setPairingState("invalid", "Pairing Code는 8자리 숫자여야 해요.");
    return;
  }
  void pairWithCode(pairingCode);
});

retryAuthentication.addEventListener("click", () => {
  void startAuthentication();
});

chatInput.addEventListener("input", () => {
  chatInput.style.height = "auto";
  chatInput.style.height = `${Math.min(chatInput.scrollHeight, 120)}px`;
  composerNotice.hidden = true;
});

chatForm.addEventListener("submit", (event) => {
  event.preventDefault();
  const credentials = currentCredentials;
  if (isSending || credentials === null) {
    return;
  }
  const message = chatInput.value.trim();
  if (message.length === 0) {
    return;
  }

  isSending = true;
  sendButton.disabled = true;
  chatInput.disabled = true;
  composerNotice.hidden = true;
  appendMessage(message, "user");
  chatInput.value = "";
  chatInput.style.height = "auto";
  const request: OfflineChatRequest = {
    schema_version: 1,
    request_id: createStableId("request"),
    save_slot_id: saveSlotId,
    companion_id: companionId,
    session_id: sessionId,
    interaction_mode: "Offline",
    message_id: createStableId("message"),
    user_message: message,
    time_context: {
      source: "RealWorld",
      observed_at: new Date().toISOString(),
      timezone: Intl.DateTimeFormat().resolvedOptions().timeZone || "UTC",
    },
    recent_event_ids: [],
  };

  void createOfflineChat(apiBaseUrl, credentials.device_token, request)
    .then((response) => {
      appendMessage(response.display_text, "companion");
    })
    .catch((error: unknown) => {
      if (error instanceof ApiClientError) {
        if (error.code === "UnauthorizedDevice") {
          handleUnauthorized("기기 연결이 폐기됐어요. 다시 Pairing해 주세요.");
          return;
        }
        const retryHint = error.retryable ? " 잠시 후 다시 시도해 주세요." : "";
        showComposerNotice(`대화 요청에 실패했어요 (${error.code}).${retryHint}`);
        return;
      }
      showComposerNotice("C PC와 대화 요청을 주고받지 못했어요.");
    })
    .finally(() => {
      isSending = false;
      sendButton.disabled = false;
      chatInput.disabled = false;
      if (currentCredentials !== null) {
        chatInput.focus();
      }
    });
});

refreshButton.addEventListener("click", () => {
  void checkConnection();
});

disconnectButton.addEventListener("click", () => {
  const credentials = currentCredentials;
  if (credentials === null) {
    return;
  }
  disconnectButton.disabled = true;
  authNotice.hidden = false;
  authNotice.dataset.state = "checking";
  authNotice.textContent = "기기 연결을 해제하고 있어요…";
  void revokeCurrentDevice(
    apiBaseUrl,
    credentials.device_token,
    createStableId("request-revoke-me"),
  )
    .then((response) => {
      if (response.device_id !== credentials.device_id) {
        throw new ApiClientError("InvalidRevocationResponse", false);
      }
      clearDeviceCredentials();
      showPairingIdle("기기 연결을 해제했어요. 다시 연결하려면 Pairing해 주세요.");
    })
    .catch((error: unknown) => {
      if (error instanceof ApiClientError && error.code === "UnauthorizedDevice") {
        handleUnauthorized("이미 폐기된 연결이에요. 다시 Pairing해 주세요.");
        return;
      }
      authNotice.hidden = false;
      authNotice.dataset.state = "error";
      authNotice.textContent = "연결 해제에 실패했어요. 저장된 연결 정보는 유지했어요.";
    })
    .finally(() => {
      disconnectButton.disabled = false;
    });
});

void startAuthentication();

import "./style.css";

import {
  ApiClientError,
  createMobileChat,
  type MobileChatRequest,
} from "./api/client";
import {
  createOfflineTask,
  deleteOfflineTask,
  listOfflineTasks,
  OfflineTaskApiError,
  type CreateOfflineTaskRequest,
  type OfflineTaskStatus,
  type OfflineTaskType,
  type OfflineTaskView,
} from "./api/offlineTasks";
import {
  decideMemoryCandidate,
  deleteMemory,
  getMemoryCandidate,
  getMemory,
  listMemoryCandidates,
  listMemories,
  MemoryApiError,
  resetMemories,
  searchMemories,
  updateMemory,
  type MemoryCandidateView,
  type MemoryType,
  type MemorySourceView,
  type MemoryView,
} from "./api/memories";
import {
  apiBaseUrl,
  companionId,
  memoryEnabled,
  memoryReviewEnabled,
  saveSlotId,
} from "./config";

type ChatUiState = "idle" | "sending" | "success" | "cancelled" | "error";
type TaskUiState = "idle" | "loading" | "success" | "warning" | "error";
type CreatableOfflineTaskType = Extract<
  OfflineTaskType,
  "Gathering" | "Crafting"
>;

interface TaskItemOption {
  id: string;
  label: string;
}

interface ActiveChatRequest {
  controller: AbortController;
}

const taskItemOptions: Record<
  CreatableOfflineTaskType,
  readonly TaskItemOption[]
> = {
  Gathering: [
    { id: "PlantStem", label: "나무" },
  ],
  Crafting: [{ id: "ShoddyBandage", label: "엉성한 붕대" }],
};

const taskItemLabels = new Map<string, string>(
  Object.values(taskItemOptions)
    .flat()
    .map((item): [string, string] => [item.id, item.label]),
);

const taskTypeLabels: Record<OfflineTaskType, string> = {
  Gathering: "채집",
  Crafting: "제작",
  Scouting: "탐색 (생성 미지원)",
};

const taskStatusLabels: Record<OfflineTaskStatus, string> = {
  Pending: "Pending · UE 동기화 대기",
  InProgress: "InProgress · 서버 시간 계산 중",
  Completed: "Completed · UE 적용 대기",
  Claimed: "Claimed · 서버 처리 종료",
};

function taskStatusLabel(task: OfflineTaskView): string {
  if (task.status !== "InProgress" || task.progress_quantity === null) {
    return taskStatusLabels[task.status];
  }
  if (task.progress_quantity === 0) {
    return "작업 중 · 아직 완성 0개";
  }
  if (
    task.quantity !== null &&
    task.progress_quantity >= task.quantity
  ) {
    return `수령 가능 · ${task.progress_quantity}개 준비`;
  }
  return `수령 가능 · 지금 ${task.progress_quantity}개`;
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
        <p class="connection-status" data-state="connected">마코랑 이야기할 준비 됐어</p>
      </div>
    </header>

    <nav class="app-tabs" aria-label="동료 메뉴">
      <button class="tab-button active" type="button" data-view="chat" aria-selected="true">
        <svg viewBox="0 0 24 24" aria-hidden="true"><path d="M21 15a4 4 0 0 1-4 4H8l-5 3V7a4 4 0 0 1 4-4h10a4 4 0 0 1 4 4Z"/></svg>
        대화
      </button>
      <button class="tab-button" type="button" data-view="tasks" aria-selected="false">
        <svg viewBox="0 0 24 24" aria-hidden="true"><path d="M9 5h10v14H5V9Z"/><path d="M9 5v4H5M9 13h6M9 16h4"/></svg>
        작업
      </button>
      <button id="memory-tab" class="tab-button" type="button" data-view="memory" aria-selected="false">
        <svg viewBox="0 0 24 24" aria-hidden="true"><path d="M12 21a9 9 0 1 0-9-9v7l2.5-2.5M8 12h8M12 8v8"/></svg>
        기억
      </button>
    </nav>

    <main id="chat-view" class="app-view active" data-view-panel="chat">
      <section id="chat-messages" class="chat-messages" aria-label="대화 내용" aria-live="polite">
        <div class="date-divider"><span>오늘</span></div>
        <article class="message companion-message">
          <div class="message-bubble">
            <p>오, 왔네. 오늘은 뭐부터 얘기해볼까?</p>
          </div>
          <time>방금</time>
        </article>
      </section>

      <section class="composer-area">
        <p class="time-context"><span aria-hidden="true">◷</span>현실 시간 기준으로 대화해요</p>
        <div class="suggestion-list" aria-label="추천 대화">
          <button type="button" class="suggestion-chip">오늘 좀 어땠어?</button>
          <button type="button" class="suggestion-chip">돌 도끼 제작 방법을 알려줘</button>
          <button type="button" class="suggestion-chip">나무 30개만 캐놔줘</button>
        </div>
        <form id="chat-form" class="chat-composer" data-state="idle" aria-busy="false">
          <label class="sr-only" for="chat-input">MAKO에게 메시지 보내기</label>
          <textarea id="chat-input" rows="1" maxlength="2000" placeholder="MAKO에게 이야기하기"></textarea>
          <button id="send-button" class="send-button" type="submit" aria-label="메시지 보내기">
            <svg viewBox="0 0 24 24" aria-hidden="true"><path d="m22 2-7 20-4-9-9-4Z"/><path d="M22 2 11 13"/></svg>
          </button>
          <button
            id="cancel-chat-button"
            class="cancel-chat-button"
            type="button"
            aria-label="대화 요청 취소"
            hidden
            disabled
          >
            <svg viewBox="0 0 24 24" aria-hidden="true"><path d="M6 6l12 12M18 6 6 18"/></svg>
          </button>
        </form>
        <p id="composer-notice" class="composer-notice" data-state="idle" aria-live="polite" hidden></p>
      </section>
    </main>

    <main id="tasks-view" class="app-view task-view" data-view-panel="tasks" hidden>
      <div class="task-scroll">
        <section class="task-intro">
          <p class="section-kicker">오프라인 작업</p>
          <h2>MAKO에게 할 일을 맡겨요</h2>
          <p>제작 재료는 서버 Inventory에서 먼저 예약하고, 완성품은 게임 접속 시 안전하게 받아요.</p>
        </section>

        <section class="task-create-card" aria-labelledby="task-create-title">
          <div class="task-section-heading">
            <div>
              <p class="task-section-label">새 요청</p>
              <h3 id="task-create-title">작업 만들기</h3>
            </div>
          </div>
          <form id="task-form" class="task-form" data-state="idle" aria-busy="false" novalidate>
            <label for="task-type">종류</label>
            <select id="task-type">
              <option value="Gathering">Gathering · 채집</option>
              <option value="Crafting">Crafting · 제작</option>
              <option value="Scouting" disabled>Scouting · 준비 중</option>
            </select>

            <label for="task-item">아이템</label>
            <select id="task-item"></select>

            <label for="task-quantity">수량</label>
            <input
              id="task-quantity"
              type="number"
              inputmode="numeric"
              min="1"
              max="50"
              step="1"
              value="1"
              aria-describedby="task-quantity-hint"
            />
            <p id="task-quantity-hint" class="field-hint">서버 시간 정책 기준 · 수량 1~50</p>

            <button id="task-create-button" class="primary-task-button" type="submit">
              작업 요청하기
            </button>
          </form>
          <p id="task-create-notice" class="task-notice" data-state="idle" aria-live="polite" hidden></p>
        </section>

        <section class="task-list-section" aria-labelledby="task-list-title">
          <div class="task-section-heading task-list-heading">
            <div>
              <p class="task-section-label">서버 기록</p>
              <h3 id="task-list-title">현재 작업</h3>
            </div>
            <button id="task-refresh-button" class="task-refresh-button" type="button">
              새로고침
            </button>
          </div>

          <label class="status-filter-label" for="task-status-filter">상태 필터</label>
          <select id="task-status-filter" class="status-filter">
            <option value="Active" selected>진행 중 · Claimed 숨김</option>
            <option value="">전체 기록</option>
            <option value="Pending">Pending</option>
            <option value="InProgress">InProgress</option>
            <option value="Completed">Completed</option>
            <option value="Claimed">Claimed</option>
          </select>

          <p class="task-boundary-notice">
            새로고침 시 서버 시간 기준 완성 수량을 확인해요. UE를 켜면 표시된 정수 수량을
            확정하고 남은 예약량은 종료해요.
          </p>
          <div id="task-list" class="task-list" data-state="idle" aria-live="polite" aria-busy="false">
            <p class="task-list-message">작업 탭을 열면 목록을 불러와요.</p>
          </div>
        </section>
      </div>
    </main>

    <main id="memory-view" class="app-view" data-view-panel="memory" hidden>
      <div class="memory-scroll">
        <section class="memory-intro">
          <span class="memory-mark" aria-hidden="true">✦</span>
          <p class="section-kicker">함께 쌓은 기억</p>
          <h2>MAKO가 기억하고 있는 이야기</h2>
          <p>직접 들려준 이야기만 저장 후보가 되며, 언제든 확인하고 지울 수 있어요.</p>
        </section>

        <section class="memory-toolbar" aria-label="기억 검색 및 관리">
          <form id="memory-search-form" class="memory-search-form" novalidate>
            <label class="sr-only" for="memory-search-input">기억 검색</label>
            <input
              id="memory-search-input"
              type="search"
              maxlength="2000"
              placeholder="기억에서 찾아보기"
              autocomplete="off"
            />
            <button id="memory-search-button" class="memory-search-button" type="submit">검색</button>
          </form>
          <div class="memory-toolbar-actions">
            <button id="memory-refresh-button" class="memory-secondary-button" type="button">새로고침</button>
            <button id="memory-reset-button" class="memory-danger-button" type="button">모든 기억 잊기</button>
          </div>
        </section>

        <p id="memory-notice" class="memory-notice" data-state="idle" aria-live="polite" hidden></p>
        <section id="memory-candidate-section" class="memory-candidate-section" aria-labelledby="memory-candidate-heading" hidden>
          <div class="memory-candidate-heading">
            <div>
              <p class="memory-section-label">검토할 기억</p>
              <h3 id="memory-candidate-heading">저장 전에 한 번 확인해 주세요</h3>
            </div>
            <button id="memory-candidate-refresh-button" class="memory-secondary-button" type="button">새로고침</button>
          </div>
          <p>비슷하거나 확인이 필요한 이야기예요. 승인하면 MAKO가 다음 대화부터 참고할 수 있어요.</p>
          <p id="memory-candidate-notice" class="memory-notice memory-candidate-notice" data-state="idle" aria-live="polite" hidden></p>
          <div id="memory-candidate-list" class="memory-candidate-list" data-state="idle" aria-live="polite" aria-busy="false">
            <section class="memory-state-card">
              <div class="empty-icon" aria-hidden="true">✧</div>
              <h3>검토할 기억을 불러오는 중이에요</h3>
            </section>
          </div>
          <section id="memory-candidate-detail" class="memory-detail memory-candidate-detail" aria-labelledby="memory-candidate-detail-title" hidden>
            <div class="memory-detail-heading">
              <div>
                <p class="memory-section-label">검토할 기억</p>
                <h3 id="memory-candidate-detail-title">후보 상세</h3>
              </div>
              <button id="memory-candidate-detail-close" class="memory-close-button" type="button">닫기</button>
            </div>
            <div id="memory-candidate-detail-content"></div>
          </section>
        </section>
        <div id="memory-list" class="memory-list" data-state="idle" aria-live="polite" aria-busy="false">
          <section class="memory-state-card">
            <div class="empty-icon" aria-hidden="true">✧</div>
            <h3>기억 탭을 열면 목록을 불러와요</h3>
            <p>서버에 저장된 기억만 이곳에 표시돼요.</p>
          </section>
        </div>

        <section id="memory-detail" class="memory-detail" aria-labelledby="memory-detail-title" hidden>
          <div class="memory-detail-heading">
            <div>
              <p class="memory-section-label">선택한 기억</p>
              <h3 id="memory-detail-title">기억 상세</h3>
            </div>
            <button id="memory-detail-close" class="memory-close-button" type="button">닫기</button>
          </div>
          <div id="memory-detail-content"></div>
        </section>
      </div>
    </main>
  </div>
`;

const companionApp = requireElement<HTMLElement>("#companion-app");
const chatMessages = requireElement<HTMLElement>("#chat-messages");
const chatForm = requireElement<HTMLFormElement>("#chat-form");
const chatInput = requireElement<HTMLTextAreaElement>("#chat-input");
const sendButton = requireElement<HTMLButtonElement>("#send-button");
const cancelChatButton = requireElement<HTMLButtonElement>("#cancel-chat-button");
const composerNotice = requireElement<HTMLElement>("#composer-notice");
const suggestionList = requireElement<HTMLElement>(".suggestion-list");
const taskForm = requireElement<HTMLFormElement>("#task-form");
const taskTypeSelect = requireElement<HTMLSelectElement>("#task-type");
const taskItemSelect = requireElement<HTMLSelectElement>("#task-item");
const taskQuantityInput = requireElement<HTMLInputElement>("#task-quantity");
const taskQuantityHint = requireElement<HTMLElement>("#task-quantity-hint");
const taskCreateButton = requireElement<HTMLButtonElement>("#task-create-button");
const taskCreateNotice = requireElement<HTMLElement>("#task-create-notice");
const taskStatusFilter = requireElement<HTMLSelectElement>("#task-status-filter");
const taskRefreshButton = requireElement<HTMLButtonElement>("#task-refresh-button");
const taskList = requireElement<HTMLElement>("#task-list");
const memoryTab = requireElement<HTMLButtonElement>("#memory-tab");
const memorySearchForm = requireElement<HTMLFormElement>("#memory-search-form");
const memorySearchInput = requireElement<HTMLInputElement>("#memory-search-input");
const memorySearchButton = requireElement<HTMLButtonElement>("#memory-search-button");
const memoryRefreshButton = requireElement<HTMLButtonElement>("#memory-refresh-button");
const memoryResetButton = requireElement<HTMLButtonElement>("#memory-reset-button");
const memoryNotice = requireElement<HTMLElement>("#memory-notice");
const memoryCandidateSection = requireElement<HTMLElement>("#memory-candidate-section");
const memoryCandidateRefreshButton = requireElement<HTMLButtonElement>("#memory-candidate-refresh-button");
const memoryCandidateNotice = requireElement<HTMLElement>("#memory-candidate-notice");
const memoryCandidateList = requireElement<HTMLElement>("#memory-candidate-list");
const memoryCandidateDetail = requireElement<HTMLElement>("#memory-candidate-detail");
const memoryCandidateDetailTitle = requireElement<HTMLElement>("#memory-candidate-detail-title");
const memoryCandidateDetailClose = requireElement<HTMLButtonElement>("#memory-candidate-detail-close");
const memoryCandidateDetailContent = requireElement<HTMLElement>("#memory-candidate-detail-content");
const memoryList = requireElement<HTMLElement>("#memory-list");
const memoryDetail = requireElement<HTMLElement>("#memory-detail");
const memoryDetailTitle = requireElement<HTMLElement>("#memory-detail-title");
const memoryDetailClose = requireElement<HTMLButtonElement>("#memory-detail-close");
const memoryDetailContent = requireElement<HTMLElement>("#memory-detail-content");

memoryTab.hidden = !memoryEnabled;
memoryTab.disabled = !memoryEnabled;
memoryCandidateSection.hidden = !memoryReviewEnabled;

const sessionId = createStableId("session");
let activeChatRequest: ActiveChatRequest | null = null;
let isCreatingTask = false;
let isListingTasks = false;
let deletingTaskId: string | null = null;
let isDraggingSuggestions = false;
let didDragSuggestions = false;
let suppressSuggestionClick = false;
let suggestionDragStartX = 0;
let suggestionDragStartScrollLeft = 0;
let memoryItems: MemoryView[] = [];
let memoryCandidateItems: MemoryCandidateView[] = [];
let selectedMemory: MemoryView | null = null;
let selectedMemoryCandidate: MemoryCandidateView | null = null;
let isMemoryLoaded = false;
let isMemoryCandidatesLoaded = false;
let isListingMemories = false;
let isListingMemoryCandidates = false;
let isLoadingMemoryCandidateDetail = false;
let memoryMutation: "update" | "delete" | "reset" | "candidate" | null = null;
let memoryLastAction: "list" | "search" = "list";

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

function setChatControls(isSending: boolean): void {
  sendButton.disabled = isSending;
  chatInput.disabled = isSending;
  cancelChatButton.hidden = !isSending;
  cancelChatButton.disabled = !isSending;
}

function finishChatRequest(
  request: ActiveChatRequest,
  state?: ChatUiState,
  message?: string,
): boolean {
  if (activeChatRequest !== request) {
    return false;
  }

  activeChatRequest = null;
  setChatControls(false);
  if (state !== undefined) {
    setChatState(state, message);
  }
  chatInput.focus();
  return true;
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
    case "cancelled":
      return "요청 대기를 취소했어요. Backend에서는 이미 처리됐을 수 있으며 자동으로 다시 보내지지 않아요.";
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
      if (error.code === "InventorySnapshotRequired") {
        return "저장된 게임 Inventory가 없어요. 게임에서 인벤토리를 한 번 동기화한 뒤 다시 요청해 주세요.";
      }
      if (error.code === "InsufficientCraftingMaterials") {
        return "서버에 저장된 Inventory의 나무가 부족해서 제작을 시작하지 못했어요.";
      }
      const publicMessage = error.publicMessage === null ? "" : ` ${error.publicMessage}`;
      return `Backend가 대화 요청을 처리하지 못했어요 (${error.code}).${publicMessage}`;
    }
  }
}

function isCreatableOfflineTaskType(
  value: string,
): value is CreatableOfflineTaskType {
  return value === "Gathering" || value === "Crafting";
}

function selectedTaskStatus(): OfflineTaskStatus | undefined {
  const value = taskStatusFilter.value;
  switch (value) {
    case "Pending":
    case "InProgress":
    case "Completed":
    case "Claimed":
      return value;
    default:
      return undefined;
  }
}

function syncTaskControls(): void {
  const isTaskMutationPending = isCreatingTask || deletingTaskId !== null;
  taskTypeSelect.disabled = isTaskMutationPending;
  taskItemSelect.disabled = isTaskMutationPending;
  taskQuantityInput.disabled = isTaskMutationPending;
  taskCreateButton.disabled = isTaskMutationPending || isListingTasks;
  taskStatusFilter.disabled = isTaskMutationPending || isListingTasks;
  taskRefreshButton.disabled = isTaskMutationPending || isListingTasks;
}

function setTaskCreateState(state: TaskUiState, message?: string): void {
  taskForm.dataset.state = state;
  taskForm.setAttribute("aria-busy", String(state === "loading"));
  taskCreateNotice.dataset.state = state;

  if (message === undefined) {
    taskCreateNotice.textContent = "";
    taskCreateNotice.hidden = true;
    return;
  }
  taskCreateNotice.textContent = message;
  taskCreateNotice.hidden = false;
}

function setTaskListMessage(state: TaskUiState, message: string): void {
  taskList.dataset.state = state;
  taskList.setAttribute("aria-busy", String(state === "loading"));
  const content = document.createElement("p");
  content.className = "task-list-message";
  content.textContent = message;
  taskList.replaceChildren(content);
}

function populateTaskItems(taskType: CreatableOfflineTaskType): void {
  const options = taskItemOptions[taskType].map((item) => {
    const option = document.createElement("option");
    option.value = item.id;
    option.textContent = `${item.label} · ${item.id}`;
    return option;
  });
  taskItemSelect.replaceChildren(...options);
}

function updateTaskQuantityPolicy(taskType: CreatableOfflineTaskType): void {
  taskQuantityInput.max = "50";
  updateTaskDurationHint(taskType);
}

function updateTaskDurationHint(taskType: CreatableOfflineTaskType): void {
  const taskLabel = taskType === "Gathering" ? "채집" : "제작";
  taskQuantityHint.textContent =
    `${taskLabel} 시간은 현재 Backend 정책으로 계산 · 수량 1~50`;
}

function validateTaskQuantity(
  taskType: CreatableOfflineTaskType,
): number | null {
  const rawQuantity = taskQuantityInput.value.trim();
  if (rawQuantity.length === 0) {
    setTaskCreateState("error", "수량을 입력해 주세요.");
    return null;
  }

  if (!/^[0-9]+$/.test(rawQuantity)) {
    setTaskCreateState("error", "수량은 숫자로 된 정수만 입력해 주세요.");
    return null;
  }

  const quantity = Number(rawQuantity);
  if (!Number.isSafeInteger(quantity) || quantity < 1) {
    setTaskCreateState("error", "수량은 1 이상의 정수여야 해요.");
    return null;
  }
  if (quantity > 50) {
    setTaskCreateState("error", "작업 수량은 1~50개만 요청할 수 있어요.");
    return null;
  }
  return quantity;
}

function taskFailureMessage(
  error: OfflineTaskApiError,
  operation: "create" | "list" | "delete",
): string {
  const subject =
    operation === "create"
      ? "작업 요청"
      : operation === "list"
        ? "작업 목록 조회"
        : "예약 삭제";
  switch (error.kind) {
    case "timeout":
      if (operation === "create") {
        return "작업 요청 응답 시간이 초과됐어요. 자동 재전송하지 않았으니 목록을 먼저 확인해 주세요.";
      }
      if (operation === "delete") {
        return "예약 삭제 응답 시간이 초과됐어요. 목록을 새로고침해 삭제 여부를 확인해 주세요.";
      }
      return "작업 목록 응답 시간이 초과됐어요. 자동으로 다시 조회하지 않았어요.";
    case "network":
      return `${subject} 중 Backend에 연결할 수 없어요. 네트워크 상태를 확인해 주세요.`;
    case "invalid-json":
      return `${subject} 응답을 JSON으로 읽을 수 없어요.`;
    case "invalid-success-response":
      return `${subject} 성공 응답 형식이나 요청 식별자가 올바르지 않아요.`;
    case "invalid-error-response":
      return `${subject} 오류 응답 형식이나 요청 식별자가 올바르지 않아요.`;
    case "unauthorized":
      return `${subject}에 필요한 Web 인증이 거부됐어요.`;
    case "forbidden":
      return `현재 Web 신원에는 ${subject} 권한이 없어요.`;
    case "server-error":
      if (operation === "create" && error.code === "InventorySnapshotRequired") {
        return "저장된 게임 Inventory가 없어요. 게임에서 인벤토리를 한 번 동기화한 뒤 다시 요청해 주세요.";
      }
      if (
        operation === "create" &&
        error.code === "InsufficientCraftingMaterials"
      ) {
        return "서버에 저장된 Inventory의 나무가 부족해요. 엉성한 붕대 1개당 나무 2개가 필요해요.";
      }
      if (operation === "delete" && error.code === "OfflineTaskNotFound") {
        return "이미 삭제됐거나 현재 프로필의 예약이 아니에요. 목록을 새로고침해 주세요.";
      }
      if (
        operation === "delete" &&
        error.code === "OfflineTaskTransitionNotAllowed"
      ) {
        return "이미 완료 처리 중인 작업은 삭제할 수 없어요. 목록을 새로고침해 주세요.";
      }
      return `Backend가 ${subject}을 처리하지 못했어요 (${error.code}).`;
  }
}

function formatTaskStartedAt(value: string): string {
  return new Intl.DateTimeFormat("ko-KR", {
    dateStyle: "short",
    timeStyle: "short",
  }).format(new Date(value));
}

function formatTaskQuantity(value: number | null): string {
  return value === null ? "—" : `${value}개`;
}

function appendTaskMetric(
  container: HTMLDListElement,
  label: string,
  value: string,
): void {
  const group = document.createElement("div");
  const term = document.createElement("dt");
  term.textContent = label;
  const description = document.createElement("dd");
  description.textContent = value;
  group.append(term, description);
  container.append(group);
}

function renderTaskList(tasks: OfflineTaskView[]): void {
  if (tasks.length === 0) {
    setTaskListMessage("success", "조건에 맞는 서버 Task가 없어요.");
    return;
  }

  const cards = tasks.map((task) => {
    const card = document.createElement("article");
    card.className = "task-card";

    const header = document.createElement("div");
    header.className = "task-card-header";
    const titleGroup = document.createElement("div");
    const type = document.createElement("p");
    type.className = "task-card-type";
    type.textContent = taskTypeLabels[task.task_type];
    const title = document.createElement("h4");
    const itemLabel =
      task.item_id === null
        ? "아이템 미지정"
        : (taskItemLabels.get(task.item_id) ?? task.item_id);
    title.textContent = itemLabel;
    titleGroup.append(type, title);

    const status = document.createElement("span");
    status.className = "task-status-badge";
    status.dataset.status = task.status;
    status.textContent =
      task.status === "Claimed" && task.result_quantity === 0
        ? "Claimed · 결과 0개로 종료"
        : taskStatusLabel(task);
    const actions = document.createElement("div");
    actions.className = "task-card-actions";
    actions.append(status);
    if (task.status === "Pending" || task.status === "InProgress") {
      const deleteButton = document.createElement("button");
      deleteButton.className = "task-delete-button";
      deleteButton.type = "button";
      deleteButton.textContent = "예약 삭제";
      deleteButton.disabled = deletingTaskId !== null;
      deleteButton.setAttribute("aria-label", `${itemLabel} 예약 삭제`);
      deleteButton.addEventListener("click", () => {
        void removeOfflineTask(task, deleteButton);
      });
      actions.append(deleteButton);
    }
    header.append(titleGroup, actions);

    const taskId = document.createElement("code");
    taskId.className = "task-id";
    taskId.textContent = `Task ID ${task.task_id}`;

    const metrics = document.createElement("dl");
    metrics.className = "task-metrics";
    appendTaskMetric(metrics, "목표", formatTaskQuantity(task.quantity));
    appendTaskMetric(metrics, "진행", formatTaskQuantity(task.progress_quantity));
    appendTaskMetric(metrics, "결과", formatTaskQuantity(task.result_quantity));
    const startedAt = document.createElement("time");
    startedAt.className = "task-started-at";
    startedAt.dateTime = task.started_at;
    startedAt.textContent = `시작 기록 ${formatTaskStartedAt(task.started_at)}`;

    card.append(header, taskId, metrics, startedAt);
    return card;
  });

  taskList.dataset.state = "success";
  taskList.setAttribute("aria-busy", "false");
  taskList.replaceChildren(...cards);
}

async function removeOfflineTask(
  task: OfflineTaskView,
  deleteButton: HTMLButtonElement,
): Promise<void> {
  if (deletingTaskId !== null) {
    return;
  }
  if (task.status !== "Pending" && task.status !== "InProgress") {
    setTaskListMessage("error", "완료 처리된 작업은 삭제할 수 없어요.");
    return;
  }
  const confirmation = task.task_type === "Crafting"
    ? "이 제작 예약을 삭제할까요? 서버에 예약한 재료는 Inventory로 돌려놔요."
    : "이 예약 작업을 삭제할까요? 진행된 시간은 복구되지 않아요.";
  if (!window.confirm(confirmation)) {
    return;
  }

  deletingTaskId = task.task_id;
  deleteButton.disabled = true;
  deleteButton.textContent = "삭제 중…";
  syncTaskControls();

  try {
    await deleteOfflineTask(
      apiBaseUrl,
      task.task_id,
      createStableId("task-delete"),
    );
    deletingTaskId = null;
    syncTaskControls();
    await loadOfflineTaskList();
  } catch (error: unknown) {
    if (error instanceof OfflineTaskApiError) {
      setTaskListMessage("error", taskFailureMessage(error, "delete"));
    } else {
      setTaskListMessage("error", "예약 삭제 중 알 수 없는 오류가 발생했어요.");
    }
  } finally {
    deletingTaskId = null;
    syncTaskControls();
  }
}

async function loadOfflineTaskList(
  expectedTaskId?: string,
): Promise<"found" | "missing" | "failed"> {
  if (isListingTasks) {
    return "failed";
  }

  isListingTasks = true;
  syncTaskControls();
  setTaskListMessage("loading", "서버 Task 목록을 불러오고 있어요…");

  try {
    const response = await listOfflineTasks(
      apiBaseUrl,
      saveSlotId,
      createStableId("task-list"),
      selectedTaskStatus(),
    );
    const visibleTasks =
      taskStatusFilter.value === "Active"
        ? response.tasks.filter((task) => task.status !== "Claimed")
        : response.tasks;
    renderTaskList(visibleTasks);
    if (expectedTaskId === undefined) {
      return "found";
    }
    return visibleTasks.some((task) => task.task_id === expectedTaskId)
      ? "found"
      : "missing";
  } catch (error: unknown) {
    if (error instanceof OfflineTaskApiError) {
      setTaskListMessage("error", taskFailureMessage(error, "list"));
    } else {
      setTaskListMessage("error", "작업 목록 조회 중 알 수 없는 오류가 발생했어요.");
    }
    return "failed";
  } finally {
    isListingTasks = false;
    syncTaskControls();
  }
}

async function submitOfflineTask(): Promise<void> {
  if (isCreatingTask || isListingTasks) {
    return;
  }

  const selectedType = taskTypeSelect.value;
  if (!isCreatableOfflineTaskType(selectedType)) {
    setTaskCreateState("error", "현재는 Gathering과 Crafting만 요청할 수 있어요.");
    return;
  }

  const allowedItems = taskItemOptions[selectedType];
  const selectedItem = taskItemSelect.value;
  if (!allowedItems.some((item) => item.id === selectedItem)) {
    setTaskCreateState("error", "지원되는 아이템을 선택해 주세요.");
    return;
  }

  const quantity = validateTaskQuantity(selectedType);
  if (quantity === null) {
    return;
  }

  isCreatingTask = true;
  syncTaskControls();
  setTaskCreateState("loading", "서버에 작업을 요청하고 있어요…");

  const request: CreateOfflineTaskRequest = {
    request_id: createStableId("task-create"),
    save_slot_id: saveSlotId,
    task_type: selectedType,
    item_id: selectedItem,
    quantity,
  };

  try {
    const response = await createOfflineTask(apiBaseUrl, request);
    setTaskCreateState("success", "작업이 등록됐어요. 전체 목록에서 확인하고 있어요…");
    taskStatusFilter.value = "";
    const reconciliation = await loadOfflineTaskList(response.task.task_id);
    if (reconciliation === "found") {
      setTaskCreateState("success", "작업을 등록했고 전체 목록에서 확인했어요.");
    } else if (reconciliation === "missing") {
      setTaskCreateState(
        "warning",
        "작업 등록은 완료됐지만 전체 목록에서 같은 Task를 확인하지 못했어요.",
      );
    } else {
      setTaskCreateState(
        "warning",
        "작업 등록은 완료됐지만 목록 갱신에 실패했어요. 새로고침으로 확인해 주세요.",
      );
    }
  } catch (error: unknown) {
    if (error instanceof OfflineTaskApiError) {
      setTaskCreateState("error", taskFailureMessage(error, "create"));
    } else {
      setTaskCreateState("error", "작업 요청 중 알 수 없는 오류가 발생했어요.");
    }
  } finally {
    isCreatingTask = false;
    syncTaskControls();
  }
}

async function reconcileChatCreatedTask(taskId: string): Promise<void> {
  taskStatusFilter.value = "";
  setTaskCreateState("loading", "채팅에서 등록한 작업을 목록에서 확인하고 있어요…");
  const reconciliation = await loadOfflineTaskList(taskId);
  if (reconciliation === "found") {
    setTaskCreateState("success", "채팅에서 요청한 작업을 현재 작업 목록에 등록했어요.");
  } else if (reconciliation === "missing") {
    setTaskCreateState(
      "warning",
      "채팅 작업은 등록됐지만 현재 목록에서 같은 Task를 확인하지 못했어요.",
    );
  } else {
    setTaskCreateState(
      "warning",
      "채팅 작업은 등록됐지만 목록 갱신에 실패했어요. 작업 탭에서 새로고침해 주세요.",
    );
  }
}

type MemoryListState = "idle" | "loading" | "success" | "empty" | "no-results" | "error";

function memoryTypeLabel(memoryType: string): string {
  const labels: Record<string, string> = {
    ProfileFact: "프로필 기억",
    Preference: "취향 기억",
    Promise: "약속 기억",
    Episode: "함께 겪은 일",
  };
  return labels[memoryType] ?? "저장된 기억";
}

function formatMemoryDate(value: string): string {
  return new Intl.DateTimeFormat("ko-KR", {
    dateStyle: "medium",
    timeStyle: "short",
  }).format(new Date(value));
}

function memorySourceLabel(source: MemorySourceView): string {
  if (source.source_type === "Message" && source.source_mode === "RealWorld") {
    return "모바일에서 직접 공유한 기억";
  }
  if (source.source_type === "Message" && source.source_mode === "GameWorld") {
    return "게임에서 직접 공유한 기억";
  }
  if (source.source_type === "Event" && source.source_mode === "GameWorld") {
    return "게임에서 함께 겪은 기억";
  }
  if (source.source_type === "Legacy" && source.source_mode === "LegacyUnknown") {
    return "이전 대화에서 가져온 기억";
  }
  return "함께 쌓은 기억";
}

function memoryFailureMessage(
  error: MemoryApiError,
  operation:
    | "list"
    | "search"
    | "detail"
    | "update"
    | "delete"
    | "reset"
    | "candidate-list"
    | "candidate-detail"
    | "candidate-decision",
): string {
  const subject: Record<typeof operation, string> = {
    list: "기억 목록 조회",
    search: "기억 검색",
    detail: "기억 상세 조회",
    update: "기억 수정",
    delete: "기억 삭제",
    reset: "기억 초기화",
    "candidate-list": "검토할 기억 조회",
    "candidate-detail": "후보 기억 상세 조회",
    "candidate-decision": "후보 기억 처리",
  };
  switch (error.kind) {
    case "timeout":
      return `${subject[operation]} 응답 시간이 초과됐어요. 자동으로 다시 시도하지 않았어요.`;
    case "network":
      return `${subject[operation]} 중 Backend에 연결할 수 없어요.`;
    case "invalid-json":
      return `${subject[operation]} 응답을 JSON으로 읽을 수 없어요.`;
    case "invalid-success-response":
      return `${subject[operation]} 성공 응답 형식이나 요청 식별자가 올바르지 않아요.`;
    case "invalid-error-response":
      return `${subject[operation]} 오류 응답 형식이나 요청 식별자가 올바르지 않아요.`;
    case "unauthorized":
      return `${subject[operation]}에 필요한 Web 인증이 거부됐어요.`;
    case "forbidden":
      return `현재 Web 신원에는 ${subject[operation]} 권한이 없어요.`;
    case "server-error":
      if (error.code === "MemoryNotFound") {
        return "이미 삭제됐거나 현재 프로필의 기억이 아니에요. 목록을 새로고침해 주세요.";
      }
      return `Backend가 ${subject[operation]}을 처리하지 못했어요.`;
  }
}

function setMemoryNotice(
  state: "idle" | "error",
  message?: string,
  retry?: () => void,
): void {
  memoryNotice.dataset.state = state;
  memoryNotice.replaceChildren();
  if (message === undefined) {
    memoryNotice.hidden = true;
    return;
  }

  const text = document.createElement("span");
  text.textContent = message;
  memoryNotice.append(text);
  if (retry !== undefined) {
    const button = document.createElement("button");
    button.type = "button";
    button.className = "memory-inline-retry";
    button.textContent = "다시 시도";
    button.addEventListener("click", retry);
    memoryNotice.append(button);
  }
  memoryNotice.hidden = false;
}

function createMemoryStateCard(
  title: string,
  description: string,
  retry?: () => void,
): HTMLElement {
  const card = document.createElement("section");
  card.className = "memory-state-card";
  const icon = document.createElement("div");
  icon.className = "empty-icon";
  icon.setAttribute("aria-hidden", "true");
  icon.textContent = "✧";
  const heading = document.createElement("h3");
  heading.textContent = title;
  const body = document.createElement("p");
  body.textContent = description;
  card.append(icon, heading, body);
  if (retry !== undefined) {
    const button = document.createElement("button");
    button.type = "button";
    button.className = "memory-retry-button";
    button.textContent = "다시 시도";
    button.addEventListener("click", retry);
    card.append(button);
  }
  return card;
}

function setMemoryListMessage(
  state: MemoryListState,
  title: string,
  description: string,
  retry?: () => void,
): void {
  memoryList.dataset.state = state;
  memoryList.setAttribute("aria-busy", String(state === "loading"));
  memoryList.replaceChildren(createMemoryStateCard(title, description, retry));
}

function setMemoryCandidateNotice(
  state: "idle" | "success" | "error",
  message?: string,
  retry?: () => void,
): void {
  memoryCandidateNotice.dataset.state = state;
  memoryCandidateNotice.replaceChildren();
  if (message === undefined) {
    memoryCandidateNotice.hidden = true;
    return;
  }
  const text = document.createElement("span");
  text.textContent = message;
  memoryCandidateNotice.append(text);
  if (retry !== undefined) {
    const button = document.createElement("button");
    button.type = "button";
    button.className = "memory-inline-retry";
    button.textContent = "다시 시도";
    button.addEventListener("click", retry);
    memoryCandidateNotice.append(button);
  }
  memoryCandidateNotice.hidden = false;
}

function setMemoryCandidateListMessage(
  state: MemoryListState,
  title: string,
  description: string,
  retry?: () => void,
): void {
  memoryCandidateList.dataset.state = state;
  memoryCandidateList.setAttribute("aria-busy", String(state === "loading"));
  memoryCandidateList.replaceChildren(createMemoryStateCard(title, description, retry));
}

function appendMemorySourceSummary(container: HTMLElement, sources: MemorySourceView[]): void {
  const sourceList = document.createElement("ul");
  sourceList.className = "memory-source-list";
  if (sources.length === 0) {
    const item = document.createElement("li");
    item.textContent = "출처 정보가 없는 기억";
    sourceList.append(item);
  } else {
    sources.slice(0, 3).forEach((source) => {
      const item = document.createElement("li");
      const label = document.createElement("span");
      label.textContent = memorySourceLabel(source);
      const date = document.createElement("time");
      date.dateTime = source.occurred_at;
      date.textContent = formatMemoryDate(source.occurred_at);
      item.append(label, date);
      sourceList.append(item);
    });
    if (sources.length > 3) {
      const more = document.createElement("li");
      more.className = "memory-source-more";
      more.textContent = `출처 ${sources.length - 3}개 더 있음`;
      sourceList.append(more);
    }
  }
  container.append(sourceList);
}

function appendMemoryMeta(container: HTMLElement, memory: MemoryView): void {
  const meta = document.createElement("div");
  meta.className = "memory-card-meta";
  const created = document.createElement("time");
  created.dateTime = memory.created_at;
  created.textContent = `저장 ${formatMemoryDate(memory.created_at)}`;
  meta.append(created);
  if (memory.corrected) {
    const corrected = document.createElement("span");
    corrected.className = "memory-corrected-badge";
    corrected.textContent = "내용을 바로잡음";
    meta.append(corrected);
  }
  const usage = document.createElement("span");
  usage.textContent = `대화에서 ${memory.use_count ?? 0}회 참고`;
  meta.append(usage);
  if (memory.last_used_at !== null && memory.last_used_at !== undefined) {
    const lastUsed = document.createElement("time");
    lastUsed.dateTime = memory.last_used_at;
    lastUsed.textContent = `마지막 참고 ${formatMemoryDate(memory.last_used_at)}`;
    meta.append(lastUsed);
  }
  container.append(meta);
}

function syncMemoryControls(): void {
  const disabled =
    isListingMemories || isListingMemoryCandidates || memoryMutation !== null;
  memorySearchInput.disabled = disabled;
  memorySearchButton.disabled = disabled;
  memoryRefreshButton.disabled = disabled;
  memoryResetButton.disabled = disabled;
  memoryDetailClose.disabled = isLoadingMemoryDetail || memoryMutation !== null;
  memoryCandidateRefreshButton.disabled = disabled;
  memoryCandidateDetailClose.disabled =
    isLoadingMemoryCandidateDetail || memoryMutation !== null;
}

function updateMemoryItem(updated: MemoryView): void {
  memoryItems = memoryItems.map((memory) =>
    memory.memory_id === updated.memory_id ? updated : memory,
  );
  selectedMemory = updated;
}

function candidateReviewReasonLabel(reason: string): string {
  const labels: Record<string, string> = {
    LowConfidence: "내용을 정확히 기억할지 확인이 필요해요.",
    TextSimilarity: "이미 저장된 기억과 문장이 비슷해요.",
    SemanticSimilarity: "이미 저장된 기억과 의미가 비슷해요.",
    PossibleConflict: "기존 기억과 내용이 다를 수 있어요.",
  };
  return labels[reason] ?? "저장하기 전에 내용을 확인해 주세요.";
}

function candidateSourceModeLabel(sourceMode: MemoryCandidateView["source_mode"]): string {
  if (sourceMode === "RealWorld") {
    return "모바일에서 직접 공유한 이야기";
  }
  if (sourceMode === "GameWorld") {
    return "게임에서 직접 공유하거나 함께 겪은 일";
  }
  return "이전 대화에서 가져온 이야기";
}

function appendMemoryCandidateMeta(
  container: HTMLElement | DocumentFragment,
  candidate: MemoryCandidateView,
): void {
  const meta = document.createElement("div");
  meta.className = "memory-card-meta";
  const occurred = document.createElement("time");
  occurred.dateTime = candidate.occurred_at;
  occurred.textContent = `이야기한 때 ${formatMemoryDate(candidate.occurred_at)}`;
  const source = document.createElement("span");
  source.textContent = candidateSourceModeLabel(candidate.source_mode);
  meta.append(occurred, source);
  container.append(meta);
}

function renderMemoryCandidateList(items: MemoryCandidateView[]): void {
  if (items.length === 0) {
    setMemoryCandidateListMessage(
      "empty",
      "검토할 기억이 없어요",
      "확인이 필요한 이야기가 생기면 여기에 보여드릴게요.",
    );
    return;
  }

  memoryCandidateList.dataset.state = "success";
  memoryCandidateList.setAttribute("aria-busy", "false");
  const cards = items.map((candidate) => {
    const card = document.createElement("article");
    card.className = "memory-card memory-candidate-card";
    const header = document.createElement("div");
    header.className = "memory-card-header";
    const titleGroup = document.createElement("div");
    const type = document.createElement("p");
    type.className = "memory-card-type";
    type.textContent = memoryTypeLabel(candidate.memory_type);
    const title = document.createElement("h3");
    title.textContent = "저장 전 확인이 필요한 이야기";
    titleGroup.append(type, title);
    header.append(titleGroup);
    const text = document.createElement("p");
    text.className = "memory-card-text";
    text.textContent = candidate.text;
    const reason = document.createElement("p");
    reason.className = "memory-candidate-reason";
    reason.textContent = candidateReviewReasonLabel(candidate.review_reason);
    const actions = document.createElement("div");
    actions.className = "memory-card-actions";
    const detailButton = document.createElement("button");
    detailButton.className = "memory-primary-button";
    detailButton.type = "button";
    detailButton.textContent = "검토하기";
    detailButton.addEventListener("click", () => {
      void openMemoryCandidateDetail(candidate.candidate_id);
    });
    actions.append(detailButton);
    card.append(header, text);
    appendMemoryCandidateMeta(card, candidate);
    card.append(reason, actions);
    return card;
  });
  memoryCandidateList.replaceChildren(...cards);
}

function renderMemoryList(items: MemoryView[], action: "list" | "search"): void {
  if (items.length === 0) {
    if (action === "search") {
      setMemoryListMessage(
        "no-results",
        "검색 결과가 없어요",
        "다른 단어로 검색하거나 검색어를 지워 전체 기억을 볼 수 있어요.",
        () => {
          memorySearchInput.value = "";
          void loadMemories("list");
        },
      );
      return;
    }
    setMemoryListMessage(
      "empty",
      "아직 저장된 기억이 없어요",
      "대화를 이어가면 소중한 이야기가 이곳에 나타나요.",
    );
    return;
  }

  memoryList.dataset.state = "success";
  memoryList.setAttribute("aria-busy", "false");
  const cards = items.map((memory) => {
    const card = document.createElement("article");
    card.className = "memory-card";

    const header = document.createElement("div");
    header.className = "memory-card-header";
    const titleGroup = document.createElement("div");
    const type = document.createElement("p");
    type.className = "memory-card-type";
    type.textContent = memoryTypeLabel(memory.memory_type);
    const title = document.createElement("h3");
    title.textContent = memory.pinned ? "오래 기억하는 이야기" : "저장된 이야기";
    titleGroup.append(type, title);
    const pinned = document.createElement("span");
    pinned.className = "memory-pin-badge";
    pinned.textContent = memory.pinned ? "고정됨" : "";
    pinned.hidden = !memory.pinned;
    header.append(titleGroup, pinned);

    const text = document.createElement("p");
    text.className = "memory-card-text";
    text.textContent = memory.text;
    appendMemoryMeta(card, memory);
    appendMemorySourceSummary(card, memory.sources);

    const actions = document.createElement("div");
    actions.className = "memory-card-actions";
    const detailButton = document.createElement("button");
    detailButton.className = "memory-secondary-button";
    detailButton.type = "button";
    detailButton.textContent = "자세히";
    detailButton.addEventListener("click", () => {
      void openMemoryDetail(memory.memory_id);
    });
    const pinButton = document.createElement("button");
    pinButton.className = "memory-secondary-button";
    pinButton.type = "button";
    pinButton.textContent = memory.pinned ? "고정 해제" : "오래 기억하기";
    pinButton.addEventListener("click", () => {
      void toggleMemoryPinned(memory, pinButton);
    });
    const forgetButton = document.createElement("button");
    forgetButton.className = "memory-delete-button";
    forgetButton.type = "button";
    forgetButton.textContent = "이 기억 잊기";
    forgetButton.addEventListener("click", () => {
      void forgetMemory(memory, forgetButton);
    });
    actions.append(detailButton, pinButton, forgetButton);
    card.append(header, text, actions);
    return card;
  });
  memoryList.replaceChildren(...cards);
}

function setMemoryDetailMessage(
  title: string,
  description: string,
  retry?: () => void,
): void {
  memoryDetailContent.replaceChildren(
    createMemoryStateCard(title, description, retry),
  );
}

function renderMemoryDetail(memory: MemoryView): void {
  memoryDetailTitle.textContent = memoryTypeLabel(memory.memory_type);
  const content = document.createDocumentFragment();
  const text = document.createElement("p");
  text.className = "memory-detail-text";
  text.textContent = memory.text;
  content.append(text);

  const meta = document.createElement("div");
  meta.className = "memory-detail-meta";
  appendMemoryMeta(meta, memory);
  content.append(meta);

  const sourceHeading = document.createElement("h4");
  sourceHeading.className = "memory-detail-subtitle";
  sourceHeading.textContent = "어디에서 온 기억인가요?";
  content.append(sourceHeading);
  const sourceContainer = document.createElement("div");
  appendMemorySourceSummary(sourceContainer, memory.sources);
  content.append(sourceContainer);

  const form = document.createElement("form");
  form.className = "memory-correction-form";
  form.noValidate = true;
  const correctionHeading = document.createElement("h4");
  correctionHeading.className = "memory-detail-subtitle";
  correctionHeading.textContent = "내용 바로잡기";
  const correctionLabel = document.createElement("label");
  correctionLabel.textContent = "기억할 내용";
  const correctionInput = document.createElement("textarea");
  correctionInput.rows = 3;
  correctionInput.maxLength = 4000;
  correctionInput.required = true;
  correctionInput.value = memory.text;
  correctionLabel.append(correctionInput);
  const reasonLabel = document.createElement("label");
  reasonLabel.textContent = "정정 이유 (필수)";
  const reasonInput = document.createElement("input");
  reasonInput.type = "text";
  reasonInput.maxLength = 512;
  reasonInput.required = true;
  reasonInput.placeholder = "예: 지금은 이렇게 기억해 줘";
  reasonLabel.append(reasonInput);
  const correctionActions = document.createElement("div");
  correctionActions.className = "memory-form-actions";
  const correctionButton = document.createElement("button");
  correctionButton.className = "memory-primary-button";
  correctionButton.type = "submit";
  correctionButton.textContent = "내용 저장";
  const formNotice = document.createElement("p");
  formNotice.className = "memory-form-notice";
  formNotice.hidden = true;
  correctionActions.append(correctionButton);
  form.append(correctionHeading, correctionLabel, reasonLabel, correctionActions, formNotice);
  form.addEventListener("submit", (event) => {
    event.preventDefault();
    if (correctionInput.value.trim().length === 0) {
      formNotice.textContent = "기억할 내용을 입력해 주세요.";
      formNotice.hidden = false;
      return;
    }
    if (reasonInput.value.trim().length === 0) {
      formNotice.textContent = "정정 이유를 입력해 주세요.";
      formNotice.hidden = false;
      return;
    }
    void submitMemoryCorrection(
      memory,
      correctionInput.value.trim(),
      reasonInput.value.trim(),
      correctionButton,
      formNotice,
    );
  });
  content.append(form);

  const actions = document.createElement("div");
  actions.className = "memory-detail-actions";
  const pinButton = document.createElement("button");
  pinButton.className = "memory-secondary-button";
  pinButton.type = "button";
  pinButton.textContent = memory.pinned ? "고정 해제" : "오래 기억하기";
  pinButton.addEventListener("click", () => {
    void toggleMemoryPinned(memory, pinButton);
  });
  const forgetButton = document.createElement("button");
  forgetButton.className = "memory-delete-button";
  forgetButton.type = "button";
  forgetButton.textContent = "이 기억 잊기";
  forgetButton.addEventListener("click", () => {
    void forgetMemory(memory, forgetButton);
  });
  actions.append(pinButton, forgetButton);
  content.append(actions);
  memoryDetailContent.replaceChildren(content);
}

function setMemoryCandidateDetailMessage(
  title: string,
  description: string,
  retry?: () => void,
): void {
  memoryCandidateDetailContent.replaceChildren(
    createMemoryStateCard(title, description, retry),
  );
}

function renderMemoryCandidateDetail(candidate: MemoryCandidateView): void {
  memoryCandidateDetailTitle.textContent = memoryTypeLabel(candidate.memory_type);
  const content = document.createDocumentFragment();
  const text = document.createElement("p");
  text.className = "memory-detail-text";
  text.textContent = candidate.text;
  content.append(text);
  appendMemoryCandidateMeta(content, candidate);

  const reviewReason = document.createElement("p");
  reviewReason.className = "memory-candidate-reason";
  reviewReason.textContent = candidateReviewReasonLabel(candidate.review_reason);
  content.append(reviewReason);

  const form = document.createElement("form");
  form.className = "memory-correction-form";
  form.noValidate = true;
  const heading = document.createElement("h4");
  heading.className = "memory-detail-subtitle";
  heading.textContent = "저장할 내용 확인";

  const typeLabel = document.createElement("label");
  typeLabel.textContent = "기억 종류";
  const typeInput = document.createElement("select");
  const memoryTypes: readonly MemoryType[] = [
    "ProfileFact",
    "Preference",
    "Episode",
    "Promise",
    "RelationshipEvidence",
  ];
  memoryTypes.forEach((memoryType) => {
    const option = document.createElement("option");
    option.value = memoryType;
    option.textContent = memoryTypeLabel(memoryType);
    option.selected = memoryType === candidate.memory_type;
    typeInput.append(option);
  });
  typeLabel.append(typeInput);

  const importanceLabel = document.createElement("label");
  importanceLabel.textContent = "중요도 (1~10)";
  const importanceInput = document.createElement("input");
  importanceInput.type = "number";
  importanceInput.min = "1";
  importanceInput.max = "10";
  importanceInput.step = "1";
  importanceInput.value = "6";
  importanceInput.required = true;
  importanceLabel.append(importanceInput);

  const correctedLabel = document.createElement("label");
  correctedLabel.textContent = "다르게 기억할 내용 (선택)";
  const correctedInput = document.createElement("textarea");
  correctedInput.rows = 3;
  correctedInput.maxLength = 4000;
  correctedInput.placeholder = "비워 두면 위의 직접 공유한 내용을 그대로 저장해요.";
  correctedLabel.append(correctedInput);

  const pinnedLabel = document.createElement("label");
  pinnedLabel.className = "memory-checkbox-label";
  const pinnedInput = document.createElement("input");
  pinnedInput.type = "checkbox";
  pinnedLabel.append(pinnedInput, document.createTextNode("오래 기억하기"));

  const reasonLabel = document.createElement("label");
  reasonLabel.textContent = "처리 이유 (필수)";
  const reasonInput = document.createElement("input");
  reasonInput.type = "text";
  reasonInput.maxLength = 512;
  reasonInput.required = true;
  reasonInput.placeholder = "예: 내용을 확인하고 저장함";
  reasonLabel.append(reasonInput);

  const actions = document.createElement("div");
  actions.className = "memory-form-actions";
  const approveButton = document.createElement("button");
  approveButton.className = "memory-primary-button";
  approveButton.type = "button";
  approveButton.textContent = "승인하고 저장";
  const rejectButton = document.createElement("button");
  rejectButton.className = "memory-delete-button";
  rejectButton.type = "button";
  rejectButton.textContent = "저장하지 않기";
  const notice = document.createElement("p");
  notice.className = "memory-form-notice";
  notice.hidden = true;
  actions.append(approveButton, rejectButton);
  form.append(
    heading,
    typeLabel,
    importanceLabel,
    correctedLabel,
    pinnedLabel,
    reasonLabel,
    actions,
    notice,
  );

  const submitDecision = (decision: "Approve" | "Reject"): void => {
    const reason = reasonInput.value.trim();
    if (reason.length === 0) {
      setMemoryInlineError(notice, "처리 이유를 입력해 주세요.");
      return;
    }
    const importance = Number(importanceInput.value);
    if (
      decision === "Approve" &&
      (!Number.isInteger(importance) || importance < 1 || importance > 10)
    ) {
      setMemoryInlineError(notice, "중요도는 1부터 10 사이의 정수여야 해요.");
      return;
    }
    void submitMemoryCandidateDecision(
      candidate,
      decision,
      {
        memoryType: typeInput.value as MemoryType,
        importance,
        pinned: pinnedInput.checked,
        correctedText: correctedInput.value.trim(),
        reason,
      },
      approveButton,
      rejectButton,
      notice,
    );
  };
  approveButton.addEventListener("click", () => submitDecision("Approve"));
  rejectButton.addEventListener("click", () => submitDecision("Reject"));
  content.append(form);
  memoryCandidateDetailContent.replaceChildren(content);
}

interface MemoryCandidateDecisionInput {
  memoryType: MemoryType;
  importance: number;
  pinned: boolean;
  correctedText: string;
  reason: string;
}

async function openMemoryCandidateDetail(candidateId: string): Promise<void> {
  if (isLoadingMemoryCandidateDetail || memoryMutation !== null) {
    return;
  }
  isLoadingMemoryCandidateDetail = true;
  syncMemoryControls();
  memoryCandidateDetail.hidden = false;
  setMemoryCandidateDetailMessage("후보 기억을 불러오는 중…", "최신 내용을 확인하고 있어요.");
  try {
    const candidate = await getMemoryCandidate(
      apiBaseUrl,
      candidateId,
      saveSlotId,
      companionId,
      createStableId("memory-candidate-detail"),
    );
    memoryCandidateItems = memoryCandidateItems.map((item) =>
      item.candidate_id === candidate.candidate_id ? candidate : item,
    );
    selectedMemoryCandidate = candidate;
    renderMemoryCandidateDetail(candidate);
  } catch (error: unknown) {
    const message =
      error instanceof MemoryApiError
        ? memoryFailureMessage(error, "candidate-detail")
        : "후보 기억 상세 조회 중 알 수 없는 오류가 발생했어요.";
    setMemoryCandidateDetailMessage(
      "후보 기억을 불러오지 못했어요",
      message,
      () => void openMemoryCandidateDetail(candidateId),
    );
  } finally {
    isLoadingMemoryCandidateDetail = false;
    syncMemoryControls();
  }
}

async function loadMemoryCandidates(): Promise<void> {
  if (!memoryReviewEnabled || isListingMemoryCandidates || memoryMutation !== null) {
    return;
  }
  isListingMemoryCandidates = true;
  isMemoryCandidatesLoaded = true;
  syncMemoryControls();
  setMemoryCandidateNotice("idle");
  setMemoryCandidateListMessage(
    "loading",
    "검토할 기억을 불러오는 중…",
    "확인이 필요한 기억 후보를 확인하고 있어요.",
  );
  try {
    const response = await listMemoryCandidates(
      apiBaseUrl,
      saveSlotId,
      companionId,
      createStableId("memory-candidate-list"),
    );
    memoryCandidateItems = response.candidates;
    renderMemoryCandidateList(memoryCandidateItems);
  } catch (error: unknown) {
    const message =
      error instanceof MemoryApiError
        ? memoryFailureMessage(error, "candidate-list")
        : "검토할 기억을 불러오는 중 알 수 없는 오류가 발생했어요.";
    setMemoryCandidateNotice("error", message, () => void loadMemoryCandidates());
    setMemoryCandidateListMessage(
      "error",
      "검토할 기억을 불러오지 못했어요",
      message,
      () => void loadMemoryCandidates(),
    );
  } finally {
    isListingMemoryCandidates = false;
    syncMemoryControls();
  }
}

async function submitMemoryCandidateDecision(
  candidate: MemoryCandidateView,
  decision: "Approve" | "Reject",
  input: MemoryCandidateDecisionInput,
  approveButton: HTMLButtonElement,
  rejectButton: HTMLButtonElement,
  notice: HTMLElement,
): Promise<void> {
  if (memoryMutation !== null) {
    return;
  }
  memoryMutation = "candidate";
  approveButton.disabled = true;
  rejectButton.disabled = true;
  approveButton.textContent = decision === "Approve" ? "저장 중…" : "승인하고 저장";
  rejectButton.textContent = decision === "Reject" ? "처리 중…" : "저장하지 않기";
  notice.hidden = true;
  syncMemoryControls();
  try {
    const response = await decideMemoryCandidate(
      apiBaseUrl,
      candidate.candidate_id,
      saveSlotId,
      companionId,
      decision === "Approve"
        ? {
            decision,
            memory_type: input.memoryType,
            importance: input.importance,
            pinned: input.pinned,
            corrected_text: input.correctedText.length === 0 ? null : input.correctedText,
            reason: input.reason,
          }
        : { decision, reason: input.reason },
      createStableId("memory-candidate-decision"),
    );
    memoryCandidateItems = memoryCandidateItems.filter(
      (item) => item.candidate_id !== candidate.candidate_id,
    );
    selectedMemoryCandidate = null;
    memoryCandidateDetail.hidden = true;
    renderMemoryCandidateList(memoryCandidateItems);
    if (response.memory !== null && isMemoryLoaded && memoryLastAction === "list") {
      memoryItems = [
        response.memory,
        ...memoryItems.filter((item) => item.memory_id !== response.memory?.memory_id),
      ];
      renderMemoryList(memoryItems, "list");
    }
    setMemoryCandidateNotice(
      "success",
      decision === "Approve" ? "기억을 저장했어요." : "이 이야기는 저장하지 않았어요.",
    );
  } catch (error: unknown) {
    setMemoryInlineError(
      notice,
      error instanceof MemoryApiError
        ? memoryFailureMessage(error, "candidate-decision")
        : "후보 기억 처리 중 알 수 없는 오류가 발생했어요.",
    );
    approveButton.disabled = false;
    rejectButton.disabled = false;
    approveButton.textContent = "승인하고 저장";
    rejectButton.textContent = "저장하지 않기";
  } finally {
    memoryMutation = null;
    syncMemoryControls();
  }
}

let isLoadingMemoryDetail = false;

async function openMemoryDetail(memoryId: string): Promise<void> {
  if (isLoadingMemoryDetail || memoryMutation !== null) {
    return;
  }
  isLoadingMemoryDetail = true;
  syncMemoryControls();
  memoryDetail.hidden = false;
  setMemoryDetailMessage("기억을 불러오는 중…", "서버에서 최신 내용을 확인하고 있어요.");
  try {
    const memory = await getMemory(apiBaseUrl, memoryId, createStableId("memory-detail"));
    if (memory.save_slot_id !== saveSlotId || memory.companion_id !== companionId) {
      setMemoryDetailMessage("기억을 표시할 수 없어요", "현재 Web 범위에 속한 기억이 아니에요.");
      return;
    }
    updateMemoryItem(memory);
    renderMemoryDetail(memory);
  } catch (error: unknown) {
    const message =
      error instanceof MemoryApiError
        ? memoryFailureMessage(error, "detail")
        : "기억 상세 조회 중 알 수 없는 오류가 발생했어요.";
    setMemoryDetailMessage(
      "기억 상세를 불러오지 못했어요",
      message,
      () => void openMemoryDetail(memoryId),
    );
  } finally {
    isLoadingMemoryDetail = false;
    syncMemoryControls();
  }
}

async function loadMemories(action: "list" | "search" = memoryLastAction): Promise<void> {
  if (isListingMemories || memoryMutation !== null) {
    return;
  }
  const query = memorySearchInput.value.trim();
  if (action === "search" && query.length === 0) {
    action = "list";
  }
  isListingMemories = true;
  memoryLastAction = action;
  isMemoryLoaded = true;
  syncMemoryControls();
  setMemoryNotice("idle");
  setMemoryListMessage("loading", "기억을 불러오는 중…", "서버에 저장된 기억을 확인하고 있어요.");
  try {
    const response =
      action === "search"
        ? await searchMemories(
            apiBaseUrl,
            { save_slot_id: saveSlotId, companion_id: companionId, query, limit: 20 },
            createStableId("memory-search"),
          )
        : await listMemories(
            apiBaseUrl,
            saveSlotId,
            companionId,
            createStableId("memory-list"),
          );
    memoryItems = response.memories;
    renderMemoryList(memoryItems, action);
  } catch (error: unknown) {
    const message =
      error instanceof MemoryApiError
        ? memoryFailureMessage(error, action)
        : "기억 목록을 불러오는 중 알 수 없는 오류가 발생했어요.";
    setMemoryNotice("error", message, () => void loadMemories(memoryLastAction));
    setMemoryListMessage(
      "error",
      "기억을 불러오지 못했어요",
      message,
      () => void loadMemories(memoryLastAction),
    );
  } finally {
    isListingMemories = false;
    syncMemoryControls();
  }
}

function setMemoryInlineError(notice: HTMLElement, message: string): void {
  notice.textContent = message;
  notice.dataset.state = "error";
  notice.hidden = false;
}

async function submitMemoryCorrection(
  memory: MemoryView,
  correctedText: string,
  correctionReason: string,
  button: HTMLButtonElement,
  notice: HTMLElement,
): Promise<void> {
  if (memoryMutation !== null) {
    return;
  }
  memoryMutation = "update";
  button.disabled = true;
  button.textContent = "저장 중…";
  syncMemoryControls();
  notice.hidden = true;
  try {
    const updated = await updateMemory(
      apiBaseUrl,
      memory.memory_id,
      { corrected_text: correctedText, correction_reason: correctionReason },
      createStableId("memory-correction"),
    );
    updateMemoryItem(updated);
    setMemoryNotice("idle");
    renderMemoryList(memoryItems, memoryLastAction);
    renderMemoryDetail(updated);
  } catch (error: unknown) {
    setMemoryInlineError(
      notice,
      error instanceof MemoryApiError
        ? memoryFailureMessage(error, "update")
        : "기억 수정 중 알 수 없는 오류가 발생했어요.",
    );
    button.disabled = false;
    button.textContent = "내용 저장";
  } finally {
    memoryMutation = null;
    syncMemoryControls();
  }
}

async function toggleMemoryPinned(memory: MemoryView, button: HTMLButtonElement): Promise<void> {
  if (memoryMutation !== null) {
    return;
  }
  memoryMutation = "update";
  button.disabled = true;
  button.textContent = "저장 중…";
  syncMemoryControls();
  try {
    const updated = await updateMemory(
      apiBaseUrl,
      memory.memory_id,
      { pinned: !memory.pinned },
      createStableId("memory-pin"),
    );
    updateMemoryItem(updated);
    setMemoryNotice("idle");
    renderMemoryList(memoryItems, memoryLastAction);
    if (!memoryDetail.hidden && selectedMemory?.memory_id === updated.memory_id) {
      renderMemoryDetail(updated);
    }
  } catch (error: unknown) {
    const message =
      error instanceof MemoryApiError
        ? memoryFailureMessage(error, "update")
        : "기억 고정 상태를 바꾸는 중 알 수 없는 오류가 발생했어요.";
    setMemoryNotice("error", message);
    button.disabled = false;
    button.textContent = memory.pinned ? "고정 해제" : "오래 기억하기";
  } finally {
    memoryMutation = null;
    syncMemoryControls();
  }
}

async function forgetMemory(memory: MemoryView, button: HTMLButtonElement): Promise<void> {
  if (memoryMutation !== null) {
    return;
  }
  if (!window.confirm("이 기억을 잊을까요? 이후 대화에서 다시 회상하지 않아요.")) {
    return;
  }
  memoryMutation = "delete";
  button.disabled = true;
  button.textContent = "삭제 중…";
  syncMemoryControls();
  try {
    await deleteMemory(
      apiBaseUrl,
      memory.memory_id,
      "user-request",
      createStableId("memory-delete"),
    );
    memoryItems = memoryItems.filter((item) => item.memory_id !== memory.memory_id);
    setMemoryNotice("idle");
    if (selectedMemory?.memory_id === memory.memory_id) {
      selectedMemory = null;
      memoryDetail.hidden = true;
    }
    renderMemoryList(memoryItems, memoryLastAction);
  } catch (error: unknown) {
    setMemoryNotice(
      "error",
      error instanceof MemoryApiError
        ? memoryFailureMessage(error, "delete")
        : "기억 삭제 중 알 수 없는 오류가 발생했어요.",
    );
    button.disabled = false;
    button.textContent = "이 기억 잊기";
  } finally {
    memoryMutation = null;
    syncMemoryControls();
  }
}

async function resetMemoryDataAfterConfirmation(): Promise<void> {
  if (memoryMutation !== null) {
    return;
  }
  if (!window.confirm("모든 기억을 잊을까요? 이 작업은 되돌릴 수 없어요.")) {
    return;
  }
  if (!window.confirm("정말 모든 기억을 초기화할까요? 확인을 누르면 즉시 처리돼요.")) {
    return;
  }
  memoryMutation = "reset";
  syncMemoryControls();
  setMemoryListMessage("loading", "모든 기억을 초기화하는 중…", "서버에서 삭제를 처리하고 있어요.");
  try {
    await resetMemories(
      apiBaseUrl,
      { save_slot_id: saveSlotId, companion_id: companionId, reason: "user-request-reset" },
      createStableId("memory-reset"),
    );
    memoryItems = [];
    selectedMemory = null;
    memoryDetail.hidden = true;
    memoryLastAction = "list";
    memorySearchInput.value = "";
    setMemoryNotice("idle");
    renderMemoryList([], "list");
  } catch (error: unknown) {
    const message =
      error instanceof MemoryApiError
        ? memoryFailureMessage(error, "reset")
        : "모든 기억을 초기화하는 중 알 수 없는 오류가 발생했어요.";
    setMemoryNotice("error", message, () => void resetMemoryDataAfterConfirmation());
    setMemoryListMessage(
      "error",
      "기억을 초기화하지 못했어요",
      message,
      () => void resetMemoryDataAfterConfirmation(),
    );
  } finally {
    memoryMutation = null;
    syncMemoryControls();
  }
}

document.querySelectorAll<HTMLButtonElement>(".tab-button").forEach((button) => {
  button.addEventListener("click", () => {
    const target = button.dataset.view;
    if (target === undefined) {
      return;
    }
    if (target === "memory" && !memoryEnabled) {
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
    if (target === "tasks") {
      void loadOfflineTaskList();
    }
    if (target === "memory" && !isMemoryLoaded) {
      void loadMemories("list");
    }
    if (target === "memory" && memoryReviewEnabled && !isMemoryCandidatesLoaded) {
      void loadMemoryCandidates();
    }
  });
});

document.querySelectorAll<HTMLButtonElement>(".suggestion-chip").forEach((button) => {
  button.addEventListener("click", () => {
    chatInput.value = button.textContent?.trim() ?? "";
    chatInput.dispatchEvent(new Event("input"));
    chatInput.focus();
  });
});

suggestionList.addEventListener("pointerdown", (event) => {
  if (event.pointerType !== "mouse" || event.button !== 0) {
    return;
  }

  isDraggingSuggestions = true;
  didDragSuggestions = false;
  suggestionDragStartX = event.clientX;
  suggestionDragStartScrollLeft = suggestionList.scrollLeft;
});

suggestionList.addEventListener("pointermove", (event) => {
  if (!isDraggingSuggestions) {
    return;
  }

  const dragDistance = event.clientX - suggestionDragStartX;
  if (Math.abs(dragDistance) > 4) {
    if (!didDragSuggestions) {
      suggestionList.setPointerCapture(event.pointerId);
      suggestionList.classList.add("is-dragging");
    }
    didDragSuggestions = true;
    suggestionList.scrollLeft = suggestionDragStartScrollLeft - dragDistance;
    event.preventDefault();
  }
});

suggestionList.addEventListener("pointerup", (event) => {
  if (!isDraggingSuggestions) {
    return;
  }

  suppressSuggestionClick = didDragSuggestions;
  isDraggingSuggestions = false;
  suggestionList.classList.remove("is-dragging");
  if (suggestionList.hasPointerCapture(event.pointerId)) {
    suggestionList.releasePointerCapture(event.pointerId);
  }
  window.setTimeout(() => {
    suppressSuggestionClick = false;
  }, 0);
});

suggestionList.addEventListener("pointercancel", () => {
  isDraggingSuggestions = false;
  didDragSuggestions = false;
  suggestionList.classList.remove("is-dragging");
});

suggestionList.addEventListener(
  "click",
  (event) => {
    if (!suppressSuggestionClick) {
      return;
    }

    event.preventDefault();
    event.stopPropagation();
    suppressSuggestionClick = false;
  },
  true,
);

taskTypeSelect.addEventListener("change", () => {
  const selectedType = taskTypeSelect.value;
  if (!isCreatableOfflineTaskType(selectedType)) {
    return;
  }
  populateTaskItems(selectedType);
  updateTaskQuantityPolicy(selectedType);
  if (!isCreatingTask) {
    setTaskCreateState("idle");
  }
});

taskQuantityInput.addEventListener("input", () => {
  const selectedType = taskTypeSelect.value;
  if (isCreatableOfflineTaskType(selectedType)) {
    updateTaskDurationHint(selectedType);
  }
  if (!isCreatingTask) {
    setTaskCreateState("idle");
  }
});

taskForm.addEventListener("submit", (event) => {
  event.preventDefault();
  void submitOfflineTask();
});

taskStatusFilter.addEventListener("change", () => {
  void loadOfflineTaskList();
});

taskRefreshButton.addEventListener("click", () => {
  void loadOfflineTaskList();
});

memorySearchForm.addEventListener("submit", (event) => {
  event.preventDefault();
  void loadMemories("search");
});

memoryRefreshButton.addEventListener("click", () => {
  void loadMemories(memorySearchInput.value.trim().length > 0 ? "search" : "list");
});

memoryCandidateRefreshButton.addEventListener("click", () => {
  void loadMemoryCandidates();
});

memoryResetButton.addEventListener("click", () => {
  void resetMemoryDataAfterConfirmation();
});

memoryDetailClose.addEventListener("click", () => {
  if (!isLoadingMemoryDetail && memoryMutation === null) {
    memoryDetail.hidden = true;
    selectedMemory = null;
  }
});

memoryCandidateDetailClose.addEventListener("click", () => {
  if (!isLoadingMemoryCandidateDetail && memoryMutation === null) {
    memoryCandidateDetail.hidden = true;
    selectedMemoryCandidate = null;
  }
});

chatInput.addEventListener("input", () => {
  chatInput.style.height = "auto";
  chatInput.style.height = `${Math.min(chatInput.scrollHeight, 120)}px`;
  if (activeChatRequest === null) {
    setChatState("idle");
  }
});

chatInput.addEventListener("keydown", (event) => {
  if (event.key === "Enter" && !event.shiftKey) {
    event.preventDefault();
    chatForm.requestSubmit();
  }
});

cancelChatButton.addEventListener("click", () => {
  const request = activeChatRequest;
  if (request === null) {
    return;
  }

  const didFinish = finishChatRequest(
    request,
    "cancelled",
    "요청 대기를 취소했어요. Backend에서는 이미 처리됐을 수 있으며 자동으로 다시 보내지지 않아요.",
  );
  if (didFinish) {
    request.controller.abort();
  }
});

chatForm.addEventListener("submit", (event) => {
  event.preventDefault();
  if (activeChatRequest !== null) {
    return;
  }

  const message = chatInput.value.trim();
  if (message.length === 0) {
    return;
  }

  const activeRequest: ActiveChatRequest = {
    controller: new AbortController(),
  };
  activeChatRequest = activeRequest;
  setChatControls(true);
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
    allowed_commands: ["Command.GatherResource", "Command.CraftItem"],
  };

  void createMobileChat(apiBaseUrl, request, activeRequest.controller.signal)
    .then((response) => {
      if (activeChatRequest !== activeRequest) {
        return;
      }
      appendMessage(response.display_text, "companion");
      if (response.offline_task_id !== null) {
        setChatState("success", "작업 요청을 등록했어요. 작업 목록에서 확인하고 있어요.");
        void reconcileChatCreatedTask(response.offline_task_id);
      } else {
        setChatState("success");
      }
    })
    .catch((error: unknown) => {
      if (activeChatRequest !== activeRequest) {
        return;
      }
      if (error instanceof ApiClientError) {
        setChatState("error", chatFailureMessage(error));
        return;
      }
      setChatState("error", "대화 요청 중 알 수 없는 오류가 발생했어요.");
    })
    .finally(() => {
      finishChatRequest(activeRequest);
    });
});

populateTaskItems("Gathering");
updateTaskQuantityPolicy("Gathering");
syncTaskControls();
setTaskCreateState("idle");
setChatControls(false);
setChatState("idle");

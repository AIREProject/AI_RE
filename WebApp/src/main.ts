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
import { apiBaseUrl, companionId, saveSlotId } from "./config";

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
  Gathering: [{ id: "PlantStem", label: "나무" }],
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
        <p class="connection-status" data-state="connected">모바일 대화 준비됨</p>
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
          <button type="button" class="suggestion-chip">오늘 나에게 해주고 싶은 말이 있어?</button>
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
          <p>현실 시간이 지난 뒤 게임을 켜면 완성된 수량을 UE Inventory에 적용해요.</p>
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
  if (!window.confirm("이 예약 작업을 삭제할까요? 진행된 시간은 복구되지 않아요.")) {
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
    if (target === "tasks") {
      void loadOfflineTaskList();
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
    allowed_commands: [],
  };

  void createMobileChat(apiBaseUrl, request, activeRequest.controller.signal)
    .then((response) => {
      if (activeChatRequest !== activeRequest) {
        return;
      }
      appendMessage(response.display_text, "companion");
      setChatState("success");
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

import { apiRequestTimeoutMs, webBearer } from "../config";

const stableIdPattern = /^[A-Za-z0-9][A-Za-z0-9._:-]*$/;
const offlineTaskTypes = new Set<string>([
  "Gathering",
  "Crafting",
  "Scouting",
]);
const offlineTaskStatuses = new Set<string>([
  "Pending",
  "InProgress",
  "Completed",
  "Claimed",
]);

export type OfflineTaskType = "Gathering" | "Crafting" | "Scouting";
export type OfflineTaskStatus =
  | "Pending"
  | "InProgress"
  | "Completed"
  | "Claimed";

export interface CreateOfflineTaskRequest {
  request_id: string;
  save_slot_id: string;
  task_type: OfflineTaskType;
  item_id: string | null;
  quantity: number | null;
}

export interface OfflineTaskView {
  task_id: string;
  save_slot_id: string;
  item_id: string | null;
  task_type: OfflineTaskType;
  status: OfflineTaskStatus;
  started_at: string;
  quantity: number | null;
  result_quantity: number | null;
  progress_quantity: number | null;
}

export interface OfflineTaskResponse {
  request_id: string;
  task: OfflineTaskView;
}

export interface OfflineTaskListResponse {
  request_id: string;
  tasks: OfflineTaskView[];
}

interface ErrorEnvelope {
  request_id: string;
  error: {
    code: string;
    message: string;
    retryable: boolean;
    details: Record<string, unknown>;
  };
}

export type OfflineTaskFailureKind =
  | "timeout"
  | "network"
  | "invalid-json"
  | "invalid-success-response"
  | "invalid-error-response"
  | "unauthorized"
  | "forbidden"
  | "server-error";

export class OfflineTaskApiError extends Error {
  public readonly name = "OfflineTaskApiError";

  constructor(
    public readonly kind: OfflineTaskFailureKind,
    public readonly code: string,
    public readonly retryable: boolean,
    public readonly status: number | null = null,
  ) {
    super(code);
  }
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function isNonEmptyString(
  value: unknown,
  maximumLength: number,
): value is string {
  return (
    typeof value === "string" &&
    value.length > 0 &&
    value.length <= maximumLength
  );
}

function isStableId(value: unknown): value is string {
  return isNonEmptyString(value, 128) && stableIdPattern.test(value);
}

function isNullableStableId(value: unknown): value is string | null {
  return value === null || isStableId(value);
}

function isNullableSafeInteger(value: unknown): value is number | null {
  return value === null || (
    typeof value === "number" &&
    Number.isSafeInteger(value) &&
    value >= 0
  );
}

function isIsoDateString(value: unknown): value is string {
  return typeof value === "string" && !Number.isNaN(Date.parse(value));
}

function isOfflineTaskType(value: unknown): value is OfflineTaskType {
  return typeof value === "string" && offlineTaskTypes.has(value);
}

function isOfflineTaskStatus(value: unknown): value is OfflineTaskStatus {
  return typeof value === "string" && offlineTaskStatuses.has(value);
}

function isOfflineTaskView(
  value: unknown,
  expectedSaveSlotId: string,
): value is OfflineTaskView {
  return (
    isRecord(value) &&
    isStableId(value.task_id) &&
    value.save_slot_id === expectedSaveSlotId &&
    isNullableStableId(value.item_id) &&
    isOfflineTaskType(value.task_type) &&
    isOfflineTaskStatus(value.status) &&
    isIsoDateString(value.started_at) &&
    isNullableSafeInteger(value.quantity) &&
    isNullableSafeInteger(value.result_quantity) &&
    isNullableSafeInteger(value.progress_quantity)
  );
}

function isOfflineTaskResponse(
  value: unknown,
  request: CreateOfflineTaskRequest,
): value is OfflineTaskResponse {
  if (!isRecord(value) || value.request_id !== request.request_id) {
    return false;
  }

  const task = value.task;
  if (!isOfflineTaskView(task, request.save_slot_id)) {
    return false;
  }

  return (
    task.task_type === request.task_type &&
    task.item_id === request.item_id &&
    task.quantity === request.quantity
  );
}

function isOfflineTaskListResponse(
  value: unknown,
  requestId: string,
  saveSlotId: string,
): value is OfflineTaskListResponse {
  if (!isRecord(value) || value.request_id !== requestId) {
    return false;
  }

  const tasks = value.tasks;
  return (
    Array.isArray(tasks) &&
    tasks.every((task: unknown) => isOfflineTaskView(task, saveSlotId))
  );
}

function isErrorEnvelope(value: unknown): value is ErrorEnvelope {
  return (
    isRecord(value) &&
    isStableId(value.request_id) &&
    isRecord(value.error) &&
    isNonEmptyString(value.error.code, 128) &&
    isNonEmptyString(value.error.message, 512) &&
    typeof value.error.retryable === "boolean" &&
    isRecord(value.error.details)
  );
}

async function readJson(response: Response): Promise<unknown> {
  try {
    return await response.json();
  } catch {
    throw new OfflineTaskApiError(
      "invalid-json",
      "InvalidJsonResponse",
      false,
      response.status,
    );
  }
}

function requireResponseRequestId(
  response: Response,
  requestId: string,
  expectedSuccessStatus = 200,
): void {
  if (response.headers.get("X-Request-ID") !== requestId) {
    throw new OfflineTaskApiError(
      response.status === expectedSuccessStatus
        ? "invalid-success-response"
        : "invalid-error-response",
      response.status === expectedSuccessStatus
        ? "InvalidOfflineTaskResponse"
        : "InvalidErrorResponse",
      false,
      response.status,
    );
  }
}

function throwResponseError(
  response: Response,
  body: unknown,
  requestId: string,
): never {
  if (!isErrorEnvelope(body) || body.request_id !== requestId) {
    throw new OfflineTaskApiError(
      "invalid-error-response",
      "InvalidErrorResponse",
      false,
      response.status,
    );
  }

  const kind: OfflineTaskFailureKind =
    response.status === 401
      ? "unauthorized"
      : response.status === 403
        ? "forbidden"
        : "server-error";
  throw new OfflineTaskApiError(
    kind,
    body.error.code,
    body.error.retryable,
    response.status,
  );
}

export async function createOfflineTask(
  apiBaseUrl: string,
  request: CreateOfflineTaskRequest,
): Promise<OfflineTaskResponse> {
  const controller = new AbortController();
  const timeoutHandle = setTimeout(() => controller.abort(), apiRequestTimeoutMs);

  try {
    const response = await fetch(`${apiBaseUrl}/api/v1/tasks`, {
      method: "POST",
      headers: {
        Accept: "application/json",
        Authorization: `Bearer ${webBearer}`,
        "Content-Type": "application/json",
        "X-Request-ID": request.request_id,
      },
      body: JSON.stringify(request),
      signal: controller.signal,
    });
    const body = await readJson(response);
    requireResponseRequestId(response, request.request_id);

    if (response.status !== 200) {
      throwResponseError(response, body, request.request_id);
    }
    if (!isOfflineTaskResponse(body, request)) {
      throw new OfflineTaskApiError(
        "invalid-success-response",
        "InvalidOfflineTaskResponse",
        false,
        response.status,
      );
    }
    return body;
  } catch (error: unknown) {
    if (error instanceof OfflineTaskApiError) {
      throw error;
    }
    if (controller.signal.aborted) {
      throw new OfflineTaskApiError("timeout", "RequestTimeout", false);
    }
    throw new OfflineTaskApiError("network", "NetworkFailure", false);
  } finally {
    clearTimeout(timeoutHandle);
  }
}

export async function listOfflineTasks(
  apiBaseUrl: string,
  saveSlotId: string,
  requestId: string,
  status?: OfflineTaskStatus,
): Promise<OfflineTaskListResponse> {
  const controller = new AbortController();
  const timeoutHandle = setTimeout(() => controller.abort(), apiRequestTimeoutMs);
  const query = new URLSearchParams({ save_slot_id: saveSlotId });
  if (status !== undefined) {
    query.set("status", status);
  }

  try {
    const response = await fetch(`${apiBaseUrl}/api/v1/tasks?${query.toString()}`, {
      method: "GET",
      headers: {
        Accept: "application/json",
        Authorization: `Bearer ${webBearer}`,
        "X-Request-ID": requestId,
      },
      signal: controller.signal,
    });
    const body = await readJson(response);
    requireResponseRequestId(response, requestId);

    if (response.status !== 200) {
      throwResponseError(response, body, requestId);
    }
    if (!isOfflineTaskListResponse(body, requestId, saveSlotId)) {
      throw new OfflineTaskApiError(
        "invalid-success-response",
        "InvalidOfflineTaskListResponse",
        false,
        response.status,
      );
    }
    return body;
  } catch (error: unknown) {
    if (error instanceof OfflineTaskApiError) {
      throw error;
    }
    if (controller.signal.aborted) {
      throw new OfflineTaskApiError("timeout", "RequestTimeout", false);
    }
    throw new OfflineTaskApiError("network", "NetworkFailure", false);
  } finally {
    clearTimeout(timeoutHandle);
  }
}

export async function deleteOfflineTask(
  apiBaseUrl: string,
  taskId: string,
  requestId: string,
): Promise<void> {
  const controller = new AbortController();
  const timeoutHandle = setTimeout(() => controller.abort(), apiRequestTimeoutMs);

  try {
    const response = await fetch(
      `${apiBaseUrl}/api/v1/tasks/${encodeURIComponent(taskId)}`,
      {
        method: "DELETE",
        headers: {
          Accept: "application/json",
          Authorization: `Bearer ${webBearer}`,
          "X-Request-ID": requestId,
        },
        signal: controller.signal,
      },
    );
    requireResponseRequestId(response, requestId, 204);

    if (response.status === 204) {
      return;
    }
    const body = await readJson(response);
    throwResponseError(response, body, requestId);
  } catch (error: unknown) {
    if (error instanceof OfflineTaskApiError) {
      throw error;
    }
    if (controller.signal.aborted) {
      throw new OfflineTaskApiError("timeout", "RequestTimeout", false);
    }
    throw new OfflineTaskApiError("network", "NetworkFailure", false);
  } finally {
    clearTimeout(timeoutHandle);
  }
}

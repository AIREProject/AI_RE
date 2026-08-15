import { chatTimeoutMs, webBearer } from "../config";

const stableIdPattern = /^[A-Za-z0-9][A-Za-z0-9._:-]*$/;
const commandTypes = new Set<string>([
  "Command.Follow",
  "Command.HoldPosition",
  "Command.ReturnToPlayer",
  "Command.EngageTarget",
  "Command.DistractTarget",
  "Command.MoveToLocation",
  "Command.CancelCurrent",
  "Command.GatherResource",
  "Command.Attack",
  "Command.Switch",
]);
const commandPriorities = new Set<string>([
  "Low",
  "Normal",
  "High",
  "Critical",
]);

export interface MobileChatRequest {
  request_id: string;
  schema_version: 1;
  session_id: string;
  save_slot_id: string;
  companion_id: string;
  message_id: string;
  user_message: string;
  surface: "mobile";
  time_context: {
    source: "RealWorld";
    day: number;
    hour: number;
    period: string;
  };
  allowed_commands: [];
}

export interface CommandCandidate {
  command_id: string;
  request_id: string;
  type: string;
  target_id: string | null;
  priority: "Low" | "Normal" | "High" | "Critical";
  issued_at: string;
  expires_at: string;
  parameters: Record<string, unknown>;
}

export interface ChatResponse {
  request_id: string;
  message_id: string | null;
  session_id: string;
  save_slot_id: string;
  companion_id: string;
  response_id: string;
  display_text: string;
  command_candidates: CommandCandidate[];
  offline_task_id: string | null;
  ai_metadata: {
    provider: string;
    model_version: string;
    prompt_version: string;
  };
}

export interface ErrorEnvelope {
  request_id: string;
  error: {
    code: string;
    message: string;
    retryable: boolean;
    details: Record<string, unknown>;
  };
}

export type ChatFailureKind =
  | "cancelled"
  | "timeout"
  | "network"
  | "invalid-json"
  | "invalid-success-response"
  | "invalid-error-response"
  | "unauthorized"
  | "forbidden"
  | "server-error";

export class ApiClientError extends Error {
  public readonly name = "ApiClientError";

  constructor(
    public readonly kind: ChatFailureKind,
    public readonly code: string,
    public readonly retryable: boolean,
    public readonly status: number | null = null,
    public readonly publicMessage: string | null = null,
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
  return (
    isNonEmptyString(value, 128) &&
    stableIdPattern.test(value)
  );
}

function isNullableStableId(value: unknown): value is string | null {
  return value === null || isStableId(value);
}

function isJsonValue(value: unknown): boolean {
  if (
    value === null ||
    typeof value === "string" ||
    typeof value === "boolean"
  ) {
    return true;
  }
  if (typeof value === "number") {
    return Number.isFinite(value);
  }
  if (Array.isArray(value)) {
    return value.every(isJsonValue);
  }
  if (isRecord(value)) {
    return Object.values(value).every(isJsonValue);
  }
  return false;
}

function isIsoDateString(value: unknown): value is string {
  return typeof value === "string" && !Number.isNaN(Date.parse(value));
}

function isCommandCandidate(
  value: unknown,
  requestId: string,
): value is CommandCandidate {
  if (!isRecord(value) || !isRecord(value.parameters)) {
    return false;
  }
  if (Object.keys(value.parameters).length > 16 || !isJsonValue(value.parameters)) {
    return false;
  }
  if (
    !isNonEmptyString(value.command_id, 128) ||
    value.request_id !== requestId ||
    typeof value.type !== "string" ||
    !commandTypes.has(value.type) ||
    !isNullableStableId(value.target_id) ||
    typeof value.priority !== "string" ||
    !commandPriorities.has(value.priority) ||
    !isIsoDateString(value.issued_at) ||
    !isIsoDateString(value.expires_at)
  ) {
    return false;
  }

  return Date.parse(value.expires_at) > Date.parse(value.issued_at);
}

function isChatResponse(
  value: unknown,
  request: MobileChatRequest,
): value is ChatResponse {
  if (
    !isRecord(value) ||
    !isRecord(value.ai_metadata) ||
    !Array.isArray(value.command_candidates)
  ) {
    return false;
  }
  if (
    value.command_candidates.length > 4 ||
    !value.command_candidates.every((candidate) =>
      isCommandCandidate(candidate, request.request_id),
    )
  ) {
    return false;
  }

  return (
    value.request_id === request.request_id &&
    value.message_id === request.message_id &&
    value.session_id === request.session_id &&
    value.save_slot_id === request.save_slot_id &&
    value.companion_id === request.companion_id &&
    isStableId(value.response_id) &&
    isNonEmptyString(value.display_text, 4000) &&
    isNullableStableId(value.offline_task_id) &&
    isNonEmptyString(value.ai_metadata.provider, 64) &&
    isNonEmptyString(value.ai_metadata.model_version, 128) &&
    isNonEmptyString(value.ai_metadata.prompt_version, 128)
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
    throw new ApiClientError(
      "invalid-json",
      "InvalidJsonResponse",
      false,
      response.status,
    );
  }
}

function requireResponseRequestId(response: Response, requestId: string): void {
  if (response.headers.get("X-Request-ID") !== requestId) {
    throw new ApiClientError(
      response.status === 200
        ? "invalid-success-response"
        : "invalid-error-response",
      response.status === 200
        ? "InvalidChatResponse"
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
    throw new ApiClientError(
      "invalid-error-response",
      "InvalidErrorResponse",
      false,
      response.status,
    );
  }

  const kind: ChatFailureKind =
    response.status === 401
      ? "unauthorized"
      : response.status === 403
        ? "forbidden"
        : "server-error";
  throw new ApiClientError(
    kind,
    body.error.code,
    body.error.retryable,
    response.status,
    body.error.message,
  );
}

export async function createMobileChat(
  apiBaseUrl: string,
  request: MobileChatRequest,
  externalSignal?: AbortSignal,
): Promise<ChatResponse> {
  const controller = new AbortController();
  let abortKind: "cancelled" | "timeout" | null = null;
  const abortRequest = (kind: "cancelled" | "timeout"): void => {
    if (abortKind !== null) {
      return;
    }
    abortKind = kind;
    controller.abort();
  };
  const handleExternalAbort = (): void => abortRequest("cancelled");
  const timeoutHandle = setTimeout(
    () => abortRequest("timeout"),
    chatTimeoutMs,
  );
  let hasExternalAbortListener = false;

  if (externalSignal?.aborted === true) {
    handleExternalAbort();
  } else if (externalSignal !== undefined) {
    externalSignal.addEventListener("abort", handleExternalAbort, { once: true });
    hasExternalAbortListener = true;
  }

  try {
    const response = await fetch(`${apiBaseUrl}/api/v1/chat`, {
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
    if (!isChatResponse(body, request)) {
      throw new ApiClientError(
        "invalid-success-response",
        "InvalidChatResponse",
        false,
        response.status,
      );
    }
    return body;
  } catch (error: unknown) {
    if (abortKind === "cancelled") {
      throw new ApiClientError("cancelled", "RequestCancelled", false);
    }
    if (abortKind === "timeout") {
      throw new ApiClientError("timeout", "RequestTimeout", false);
    }
    if (error instanceof ApiClientError) {
      throw error;
    }
    throw new ApiClientError("network", "NetworkFailure", false);
  } finally {
    clearTimeout(timeoutHandle);
    if (hasExternalAbortListener) {
      externalSignal?.removeEventListener("abort", handleExternalAbort);
    }
  }
}

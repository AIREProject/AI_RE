import { apiRequestTimeoutMs, webBearer } from "../config";

const stableIdPattern = /^[A-Za-z0-9][A-Za-z0-9._:-]*$/;
const memorySourceTypes = new Set(["Message", "Event", "Legacy"]);
const memorySourceModes = new Set(["RealWorld", "GameWorld", "LegacyUnknown"]);
const memoryTypes = new Set([
  "ProfileFact",
  "Preference",
  "Episode",
  "Promise",
  "RelationshipEvidence",
]);

export type MemorySourceType = "Message" | "Event" | "Legacy";
export type MemorySourceMode = "RealWorld" | "GameWorld" | "LegacyUnknown";

export interface MemorySourceView {
  source_type: MemorySourceType;
  source_mode: MemorySourceMode;
  occurred_at: string;
}

export interface MemoryView {
  memory_id: string;
  save_slot_id: string;
  companion_id: string;
  memory_type: string;
  text: string;
  // The API validates importance for contract integrity, but the Web UI never renders it.
  importance: number;
  pinned: boolean;
  corrected: boolean;
  created_at: string;
  last_used_at?: string | null;
  use_count?: number;
  sources: MemorySourceView[];
}

export interface MemoryListResponse {
  request_id: string;
  memories: MemoryView[];
}

export interface SearchMemoriesRequest {
  save_slot_id: string;
  companion_id: string;
  query: string;
  limit?: number;
}

export interface UpdateMemoryRequest {
  corrected_text?: string;
  correction_reason?: string;
  pinned?: boolean;
}

export interface MemoryResetRequest {
  save_slot_id: string;
  companion_id: string;
  reason: string;
}

export interface MemoryResetResponse {
  request_id: string;
  archived_count: number;
}

export type MemoryCandidateDecision = "Approve" | "Reject";

export interface MemoryCandidateView {
  candidate_id: string;
  save_slot_id: string;
  companion_id: string;
  memory_type: MemoryType;
  text: string;
  source_mode: MemorySourceMode;
  occurred_at: string;
  review_reason: string;
  created_at: string;
}

export type MemoryType =
  | "ProfileFact"
  | "Preference"
  | "Episode"
  | "Promise"
  | "RelationshipEvidence";

export interface MemoryCandidateListResponse {
  request_id: string;
  candidates: MemoryCandidateView[];
}

export interface ApproveMemoryCandidateRequest {
  decision: "Approve";
  memory_type: MemoryType;
  importance: number;
  pinned: boolean;
  corrected_text: string | null;
  reason: string;
}

export interface RejectMemoryCandidateRequest {
  decision: "Reject";
  reason: string;
}

export type DecideMemoryCandidateRequest =
  | ApproveMemoryCandidateRequest
  | RejectMemoryCandidateRequest;

interface LegacyDecideMemoryCandidateRequest {
  decision: MemoryCandidateDecision;
  memory_type?: MemoryType;
  importance?: number;
  pinned?: boolean;
  corrected_text?: string | null;
  reason?: string | null;
}

export interface DecideMemoryCandidateResponse {
  request_id: string;
  candidate_id: string;
  decision: MemoryCandidateDecision;
  memory: MemoryView | null;
}

export type MemoryApiFailureKind =
  | "timeout"
  | "network"
  | "invalid-json"
  | "invalid-success-response"
  | "invalid-error-response"
  | "unauthorized"
  | "forbidden"
  | "server-error";

export class MemoryApiError extends Error {
  public readonly name = "MemoryApiError";

  constructor(
    public readonly kind: MemoryApiFailureKind,
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

function isNonEmptyString(value: unknown, maximumLength: number): value is string {
  return typeof value === "string" && value.length > 0 && value.length <= maximumLength;
}

function isStableId(value: unknown): value is string {
  return isNonEmptyString(value, 128) && stableIdPattern.test(value);
}

function isIsoDateString(value: unknown): value is string {
  return typeof value === "string" && !Number.isNaN(Date.parse(value));
}

function isMemorySource(value: unknown): value is MemorySourceView {
  return (
    isRecord(value) &&
    typeof value.source_type === "string" &&
    memorySourceTypes.has(value.source_type) &&
    typeof value.source_mode === "string" &&
    memorySourceModes.has(value.source_mode) &&
    isIsoDateString(value.occurred_at)
  );
}

function isMemoryView(
  value: unknown,
  expectedSaveSlotId?: string,
  expectedCompanionId?: string,
): value is MemoryView {
  if (!isRecord(value)) {
    return false;
  }

  const sources = value.sources;
  return (
    isStableId(value.memory_id) &&
    isStableId(value.save_slot_id) &&
    (expectedSaveSlotId === undefined || value.save_slot_id === expectedSaveSlotId) &&
    isStableId(value.companion_id) &&
    (expectedCompanionId === undefined || value.companion_id === expectedCompanionId) &&
    typeof value.memory_type === "string" &&
    memoryTypes.has(value.memory_type) &&
    isNonEmptyString(value.text, 4000) &&
    typeof value.importance === "number" &&
    Number.isSafeInteger(value.importance) &&
    value.importance >= 1 &&
    value.importance <= 10 &&
    typeof value.pinned === "boolean" &&
    typeof value.corrected === "boolean" &&
    isIsoDateString(value.created_at) &&
    (value.last_used_at === undefined ||
      value.last_used_at === null ||
      isIsoDateString(value.last_used_at)) &&
    (value.use_count === undefined ||
      (typeof value.use_count === "number" &&
        Number.isSafeInteger(value.use_count) &&
        value.use_count >= 0)) &&
    (sources === undefined || (Array.isArray(sources) && sources.every(isMemorySource)))
  );
}

function normalizeMemory(value: MemoryView): MemoryView {
  return {
    ...value,
    // Older deployed responses omit the additive sources field. Treating that as
    // an empty list keeps the client backwards-compatible without exposing IDs.
    sources: value.sources ?? [],
    last_used_at: value.last_used_at ?? null,
    use_count: value.use_count ?? 0,
  };
}

function isMemoryCandidateView(
  value: unknown,
  expectedSaveSlotId?: string,
  expectedCompanionId?: string,
): value is MemoryCandidateView {
  return (
    isRecord(value) &&
    isStableId(value.candidate_id) &&
    isStableId(value.save_slot_id) &&
    (expectedSaveSlotId === undefined || value.save_slot_id === expectedSaveSlotId) &&
    isStableId(value.companion_id) &&
    (expectedCompanionId === undefined || value.companion_id === expectedCompanionId) &&
    typeof value.memory_type === "string" &&
    memoryTypes.has(value.memory_type) &&
    isNonEmptyString(value.text, 4000) &&
    typeof value.source_mode === "string" &&
    memorySourceModes.has(value.source_mode) &&
    isIsoDateString(value.occurred_at) &&
    isStableId(value.review_reason) &&
    isIsoDateString(value.created_at)
  );
}

function isDecideMemoryCandidateRequest(
  value: LegacyDecideMemoryCandidateRequest,
): value is DecideMemoryCandidateRequest {
  if (!isNonEmptyString(value.reason, 512)) {
    return false;
  }
  if (value.decision === "Reject") {
    return true;
  }
  return (
    value.decision === "Approve" &&
    typeof value.memory_type === "string" &&
    memoryTypes.has(value.memory_type) &&
    typeof value.importance === "number" &&
    Number.isSafeInteger(value.importance) &&
    value.importance >= 1 &&
    value.importance <= 10 &&
    typeof value.pinned === "boolean" &&
    (value.corrected_text === null ||
      (typeof value.corrected_text === "string" && value.corrected_text.length <= 4000))
  );
}

function isMemoryListResponse(
  value: unknown,
  requestId: string,
  expectedSaveSlotId: string,
  expectedCompanionId: string,
): value is MemoryListResponse {
  if (!isRecord(value) || value.request_id !== requestId || !Array.isArray(value.memories)) {
    return false;
  }
  return value.memories.every((memory) =>
    isMemoryView(memory, expectedSaveSlotId, expectedCompanionId),
  );
}

function isMemoryResetResponse(value: unknown, requestId: string): value is MemoryResetResponse {
  return (
    isRecord(value) &&
    value.request_id === requestId &&
    typeof value.archived_count === "number" &&
    Number.isSafeInteger(value.archived_count) &&
    value.archived_count >= 0
  );
}

function isMemoryCandidateListResponse(
  value: unknown,
  requestId: string,
  expectedSaveSlotId: string,
  expectedCompanionId: string,
): value is MemoryCandidateListResponse {
  return (
    isRecord(value) &&
    value.request_id === requestId &&
    Array.isArray(value.candidates) &&
    value.candidates.every((candidate) =>
      isMemoryCandidateView(candidate, expectedSaveSlotId, expectedCompanionId),
    )
  );
}

function isDecideMemoryCandidateResponse(
  value: unknown,
  requestId: string,
  candidateId: string,
  decision: MemoryCandidateDecision,
): value is DecideMemoryCandidateResponse {
  return (
    isRecord(value) &&
    value.request_id === requestId &&
    value.candidate_id === candidateId &&
    value.decision === decision &&
    (value.memory === null || isMemoryView(value.memory))
  );
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
    throw new MemoryApiError(
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
    throw new MemoryApiError(
      response.status === expectedSuccessStatus
        ? "invalid-success-response"
        : "invalid-error-response",
      response.status === expectedSuccessStatus
        ? "InvalidMemoryResponse"
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
    throw new MemoryApiError(
      "invalid-error-response",
      "InvalidErrorResponse",
      false,
      response.status,
    );
  }

  const kind: MemoryApiFailureKind =
    response.status === 401
      ? "unauthorized"
      : response.status === 403
        ? "forbidden"
        : "server-error";
  throw new MemoryApiError(
    kind,
    body.error.code,
    body.error.retryable,
    response.status,
    body.error.message,
  );
}

async function requestJson<T>(
  apiBaseUrl: string,
  path: string,
  requestId: string,
  init: RequestInit,
  validate: (body: unknown) => body is T,
): Promise<T> {
  const controller = new AbortController();
  const timeoutHandle = setTimeout(() => controller.abort(), apiRequestTimeoutMs);

  try {
    const response = await fetch(`${apiBaseUrl}${path}`, {
      ...init,
      headers: {
        Accept: "application/json",
        Authorization: `Bearer ${webBearer}`,
        "X-Request-ID": requestId,
        ...init.headers,
      },
      signal: controller.signal,
    });
    const body = await readJson(response);
    requireResponseRequestId(response, requestId);
    if (response.status !== 200) {
      throwResponseError(response, body, requestId);
    }
    if (!validate(body)) {
      throw new MemoryApiError(
        "invalid-success-response",
        "InvalidMemoryResponse",
        false,
        response.status,
      );
    }
    return body;
  } catch (error: unknown) {
    if (error instanceof MemoryApiError) {
      throw error;
    }
    if (controller.signal.aborted) {
      throw new MemoryApiError("timeout", "RequestTimeout", false);
    }
    throw new MemoryApiError("network", "NetworkFailure", false);
  } finally {
    clearTimeout(timeoutHandle);
  }
}

export async function listMemories(
  apiBaseUrl: string,
  saveSlotId: string,
  companionId: string,
  requestId: string,
): Promise<MemoryListResponse> {
  const query = new URLSearchParams({
    save_slot_id: saveSlotId,
    companion_id: companionId,
  });
  const response = await requestJson<MemoryListResponse>(
    apiBaseUrl,
    `/api/v1/memories?${query.toString()}`,
    requestId,
    { method: "GET" },
    (body): body is MemoryListResponse =>
      isMemoryListResponse(body, requestId, saveSlotId, companionId),
  );
  return {
    ...response,
    memories: response.memories.map(normalizeMemory),
  };
}

export async function searchMemories(
  apiBaseUrl: string,
  request: SearchMemoriesRequest,
  requestId: string,
): Promise<MemoryListResponse> {
  const response = await requestJson<MemoryListResponse>(
    apiBaseUrl,
    "/api/v1/memories/search",
    requestId,
    {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ ...request, limit: request.limit ?? 20 }),
    },
    (body): body is MemoryListResponse =>
      isMemoryListResponse(
        body,
        requestId,
        request.save_slot_id,
        request.companion_id,
      ),
  );
  return {
    ...response,
    memories: response.memories.map(normalizeMemory),
  };
}

export async function getMemory(
  apiBaseUrl: string,
  memoryId: string,
  requestId: string,
): Promise<MemoryView> {
  const response = await requestJson<MemoryView>(
    apiBaseUrl,
    `/api/v1/memories/${encodeURIComponent(memoryId)}`,
    requestId,
    { method: "GET" },
    (body): body is MemoryView => isMemoryView(body),
  );
  return normalizeMemory(response);
}

export async function updateMemory(
  apiBaseUrl: string,
  memoryId: string,
  request: UpdateMemoryRequest,
  requestId: string,
): Promise<MemoryView> {
  const response = await requestJson<MemoryView>(
    apiBaseUrl,
    `/api/v1/memories/${encodeURIComponent(memoryId)}`,
    requestId,
    {
      method: "PATCH",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(request),
    },
    (body): body is MemoryView => isMemoryView(body),
  );
  return normalizeMemory(response);
}

export async function deleteMemory(
  apiBaseUrl: string,
  memoryId: string,
  reason: string,
  requestId: string,
): Promise<void> {
  const query = new URLSearchParams({ reason });
  const controller = new AbortController();
  const timeoutHandle = setTimeout(() => controller.abort(), apiRequestTimeoutMs);

  try {
    const response = await fetch(
      `${apiBaseUrl}/api/v1/memories/${encodeURIComponent(memoryId)}?${query.toString()}`,
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
    if (error instanceof MemoryApiError) {
      throw error;
    }
    if (controller.signal.aborted) {
      throw new MemoryApiError("timeout", "RequestTimeout", false);
    }
    throw new MemoryApiError("network", "NetworkFailure", false);
  } finally {
    clearTimeout(timeoutHandle);
  }
}

export async function resetMemories(
  apiBaseUrl: string,
  request: MemoryResetRequest,
  requestId: string,
): Promise<MemoryResetResponse> {
  return requestJson<MemoryResetResponse>(
    apiBaseUrl,
    "/api/v1/memories/reset",
    requestId,
    {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(request),
    },
    (body): body is MemoryResetResponse => isMemoryResetResponse(body, requestId),
  );
}

export async function listMemoryCandidates(
  apiBaseUrl: string,
  saveSlotId: string,
  companionId: string,
  requestId: string,
): Promise<MemoryCandidateListResponse> {
  const query = new URLSearchParams({
    save_slot_id: saveSlotId,
    companion_id: companionId,
  });
  return requestJson<MemoryCandidateListResponse>(
    apiBaseUrl,
    `/api/v1/memory-candidates?${query.toString()}`,
    requestId,
    { method: "GET" },
    (body): body is MemoryCandidateListResponse =>
      isMemoryCandidateListResponse(body, requestId, saveSlotId, companionId),
  );
}

export async function getMemoryCandidate(
  apiBaseUrl: string,
  candidateId: string,
  expectedSaveSlotId: string,
  expectedCompanionId: string,
  requestId: string,
): Promise<MemoryCandidateView> {
  const query = new URLSearchParams({
    save_slot_id: expectedSaveSlotId,
    companion_id: expectedCompanionId,
  });
  return requestJson<MemoryCandidateView>(
    apiBaseUrl,
    `/api/v1/memory-candidates/${encodeURIComponent(candidateId)}?${query.toString()}`,
    requestId,
    { method: "GET" },
    (body): body is MemoryCandidateView =>
      isMemoryCandidateView(body, expectedSaveSlotId, expectedCompanionId),
  );
}

export async function decideMemoryCandidate(
  apiBaseUrl: string,
  candidateId: string,
  saveSlotId: string,
  companionId: string,
  request: DecideMemoryCandidateRequest,
  requestId: string,
): Promise<DecideMemoryCandidateResponse> {
  if (!isDecideMemoryCandidateRequest(request)) {
    throw new MemoryApiError(
      "invalid-success-response",
      "InvalidMemoryCandidateDecision",
      false,
    );
  }
  const query = new URLSearchParams({
    save_slot_id: saveSlotId,
    companion_id: companionId,
  });
  return requestJson<DecideMemoryCandidateResponse>(
    apiBaseUrl,
    `/api/v1/memory-candidates/${encodeURIComponent(candidateId)}?${query.toString()}`,
    requestId,
    {
      method: "PATCH",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(request),
    },
    (body): body is DecideMemoryCandidateResponse =>
      isDecideMemoryCandidateResponse(body, requestId, candidateId, request.decision),
  ).then((response) => ({
    ...response,
    memory: response.memory === null ? null : normalizeMemory(response.memory),
  }));
}

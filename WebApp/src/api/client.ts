export interface OfflineChatRequest {
  schema_version: 1;
  request_id: string;
  save_slot_id: string;
  companion_id: string;
  session_id: string;
  interaction_mode: "Offline";
  message_id: string;
  user_message: string;
  time_context: {
    source: "RealWorld";
    observed_at: string;
    timezone: string;
  };
  recent_event_ids: string[];
}

export interface ChatResponse {
  schema_version: 1;
  request_id: string;
  response_id: string;
  display_text: string;
  interaction_mode: "InGame" | "Offline";
  command_candidates: unknown[];
  memory_candidates: unknown[];
  ai_metadata: {
    provider: string;
    model_version: string;
    prompt_version: string;
  };
}

export interface DeviceTokenResponse {
  schema_version: 1;
  request_id: string;
  profile_id: string;
  device: {
    device_id: string;
    role: "WebClient";
    created_at: string;
    last_used_at?: string | null;
    revoked_at?: string | null;
  };
  device_token: string;
}

export interface DeviceSelfResponse {
  schema_version: 1;
  request_id: string;
  profile_id: string;
  device_id: string;
  role: "WebClient";
  status: "Active";
}

export interface DeviceRevocationResponse {
  schema_version: 1;
  request_id: string;
  device_id: string;
  status: "Revoked";
}

interface ErrorEnvelope {
  schema_version: 1;
  request_id: string;
  error: {
    code: string;
    message: string;
    retryable: boolean;
  };
}

export class ApiClientError extends Error {
  constructor(
    public readonly code: string,
    public readonly retryable: boolean,
  ) {
    super(code);
  }
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null;
}

function isNonEmptyString(value: unknown): value is string {
  return typeof value === "string" && value.length > 0;
}

function isOptionalNullableString(value: unknown): boolean {
  return value === undefined || typeof value === "string" || value === null;
}

function isChatResponse(value: unknown): value is ChatResponse {
  if (!isRecord(value) || !isRecord(value.ai_metadata)) {
    return false;
  }
  return (
    value.schema_version === 1 &&
    typeof value.request_id === "string" &&
    typeof value.response_id === "string" &&
    typeof value.display_text === "string" &&
    (value.interaction_mode === "InGame" || value.interaction_mode === "Offline") &&
    Array.isArray(value.command_candidates) &&
    Array.isArray(value.memory_candidates) &&
    typeof value.ai_metadata.provider === "string" &&
    typeof value.ai_metadata.model_version === "string" &&
    typeof value.ai_metadata.prompt_version === "string"
  );
}

function isDeviceTokenResponse(value: unknown): value is DeviceTokenResponse {
  if (!isRecord(value) || !isRecord(value.device)) {
    return false;
  }
  return (
    value.schema_version === 1 &&
    isNonEmptyString(value.request_id) &&
    isNonEmptyString(value.profile_id) &&
    isNonEmptyString(value.device.device_id) &&
    value.device.role === "WebClient" &&
    typeof value.device.created_at === "string" &&
    isOptionalNullableString(value.device.last_used_at) &&
    isOptionalNullableString(value.device.revoked_at) &&
    typeof value.device_token === "string" &&
    value.device_token.length >= 32
  );
}

function isDeviceSelfResponse(value: unknown): value is DeviceSelfResponse {
  return (
    isRecord(value) &&
    value.schema_version === 1 &&
    isNonEmptyString(value.request_id) &&
    isNonEmptyString(value.profile_id) &&
    isNonEmptyString(value.device_id) &&
    value.role === "WebClient" &&
    value.status === "Active"
  );
}

function isDeviceRevocationResponse(
  value: unknown,
): value is DeviceRevocationResponse {
  return (
    isRecord(value) &&
    value.schema_version === 1 &&
    isNonEmptyString(value.request_id) &&
    isNonEmptyString(value.device_id) &&
    value.status === "Revoked"
  );
}

function isErrorEnvelope(value: unknown): value is ErrorEnvelope {
  return (
    isRecord(value) &&
    value.schema_version === 1 &&
    typeof value.request_id === "string" &&
    isRecord(value.error) &&
    typeof value.error.code === "string" &&
    typeof value.error.message === "string" &&
    typeof value.error.retryable === "boolean"
  );
}

async function readJson(response: Response): Promise<unknown> {
  try {
    return await response.json();
  } catch {
    throw new ApiClientError("InvalidJsonResponse", false);
  }
}

function requireSuccessfulRequest(
  response: Response,
  body: unknown,
  requestId: string,
): void {
  if (!response.ok) {
    if (
      isErrorEnvelope(body) &&
      body.request_id === requestId &&
      response.headers.get("X-Request-ID") === requestId
    ) {
      throw new ApiClientError(body.error.code, body.error.retryable);
    }
    throw new ApiClientError("InvalidErrorResponse", false);
  }

  if (response.headers.get("X-Request-ID") !== requestId) {
    throw new ApiClientError("InvalidRequestIdResponse", false);
  }
}

export async function pairWebDevice(
  apiBaseUrl: string,
  pairingCode: string,
  requestId: string,
): Promise<DeviceTokenResponse> {
  const response = await fetch(`${apiBaseUrl}/api/v1/devices/pair`, {
    method: "POST",
    headers: {
      Accept: "application/json",
      "Content-Type": "application/json",
      "X-Request-ID": requestId,
    },
    body: JSON.stringify({
      schema_version: 1,
      request_id: requestId,
      pairing_code: pairingCode,
    }),
  });
  const body = await readJson(response);
  requireSuccessfulRequest(response, body, requestId);
  if (!isDeviceTokenResponse(body) || body.request_id !== requestId) {
    throw new ApiClientError("InvalidPairingResponse", false);
  }
  return body;
}

export async function getCurrentDevice(
  apiBaseUrl: string,
  deviceToken: string,
  requestId: string,
): Promise<DeviceSelfResponse> {
  const response = await fetch(`${apiBaseUrl}/api/v1/devices/me`, {
    headers: {
      Accept: "application/json",
      Authorization: `Bearer ${deviceToken}`,
      "X-Request-ID": requestId,
    },
  });
  const body = await readJson(response);
  requireSuccessfulRequest(response, body, requestId);
  if (!isDeviceSelfResponse(body) || body.request_id !== requestId) {
    throw new ApiClientError("InvalidDeviceResponse", false);
  }
  return body;
}

export async function revokeCurrentDevice(
  apiBaseUrl: string,
  deviceToken: string,
  requestId: string,
): Promise<DeviceRevocationResponse> {
  const response = await fetch(`${apiBaseUrl}/api/v1/devices/me`, {
    method: "DELETE",
    headers: {
      Accept: "application/json",
      Authorization: `Bearer ${deviceToken}`,
      "X-Request-ID": requestId,
    },
  });
  const body = await readJson(response);
  requireSuccessfulRequest(response, body, requestId);
  if (!isDeviceRevocationResponse(body) || body.request_id !== requestId) {
    throw new ApiClientError("InvalidRevocationResponse", false);
  }
  return body;
}

export async function createOfflineChat(
  apiBaseUrl: string,
  deviceToken: string,
  request: OfflineChatRequest,
): Promise<ChatResponse> {
  const response = await fetch(`${apiBaseUrl}/api/v1/chat`, {
    method: "POST",
    headers: {
      Accept: "application/json",
      Authorization: `Bearer ${deviceToken}`,
      "Content-Type": "application/json",
      "X-Request-ID": request.request_id,
    },
    body: JSON.stringify(request),
  });
  const body = await readJson(response);
  requireSuccessfulRequest(response, body, request.request_id);
  if (!isChatResponse(body) || body.request_id !== request.request_id) {
    throw new ApiClientError("InvalidChatResponse", false);
  }
  return body;
}

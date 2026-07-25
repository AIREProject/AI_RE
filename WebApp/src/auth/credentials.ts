export const DEVICE_CREDENTIALS_STORAGE_KEY =
  "aire.web_device_credentials.v1";

export interface DeviceCredentials {
  schema_version: 1;
  profile_id: string;
  device_id: string;
  device_token: string;
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null;
}

function isDeviceCredentials(value: unknown): value is DeviceCredentials {
  return (
    isRecord(value) &&
    Object.keys(value).length === 4 &&
    value.schema_version === 1 &&
    typeof value.profile_id === "string" &&
    value.profile_id.length > 0 &&
    typeof value.device_id === "string" &&
    value.device_id.length > 0 &&
    typeof value.device_token === "string" &&
    value.device_token.length >= 32
  );
}

export function loadDeviceCredentials(): DeviceCredentials | null {
  const stored = localStorage.getItem(DEVICE_CREDENTIALS_STORAGE_KEY);
  if (stored === null) {
    return null;
  }

  try {
    const parsed: unknown = JSON.parse(stored);
    if (isDeviceCredentials(parsed)) {
      return parsed;
    }
  } catch {
    // Invalid browser state is discarded below.
  }

  clearDeviceCredentials();
  return null;
}

export function saveDeviceCredentials(credentials: DeviceCredentials): void {
  localStorage.setItem(
    DEVICE_CREDENTIALS_STORAGE_KEY,
    JSON.stringify(credentials),
  );
}

export function clearDeviceCredentials(): void {
  localStorage.removeItem(DEVICE_CREDENTIALS_STORAGE_KEY);
}

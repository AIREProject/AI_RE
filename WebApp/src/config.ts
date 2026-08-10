const rawApiBaseUrl = import.meta.env.VITE_API_BASE_URL as string | undefined;

export const apiBaseUrl = (rawApiBaseUrl ?? "").replace(/\/$/, "");
export const webBearer = "AIRE_WEB";
export const saveSlotId = "demo-slot-1";
export const companionId = "mako";
export const chatTimeoutMs = 35_000;

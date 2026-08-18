const rawApiBaseUrl = import.meta.env.VITE_API_BASE_URL as string | undefined;
const rawMemoryEnabled = import.meta.env.VITE_MEMORY_ENABLED as string | undefined;

export const apiBaseUrl = (rawApiBaseUrl ?? "").replace(/\/$/, "");
export const webBearer = "AIRE_WEB";
export const saveSlotId = "demo-slot-1";
export const companionId = "mako";
export const apiRequestTimeoutMs = 35_000;
export const chatTimeoutMs = apiRequestTimeoutMs;
export const memoryEnabled = rawMemoryEnabled === "true";

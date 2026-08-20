const rawApiBaseUrl = import.meta.env.VITE_API_BASE_URL as string | undefined;
const rawMemoryEnabled = import.meta.env.VITE_MEMORY_ENABLED as string | undefined;

export const apiBaseUrl = (rawApiBaseUrl ?? "").replace(/\/$/, "");
export const webBearer = "AIRE_WEB";
export const saveSlotId = "demo-slot-1";
export const companionId = "mako";
export const apiRequestTimeoutMs = 35_000;
export const chatTimeoutMs = apiRequestTimeoutMs;
// 배포 Backend가 Memory API를 제공하므로 기본은 활성화한다. 명시적으로 false를 줄 때만
// 계약 전환·장애 대응을 위해 탭을 숨긴다.
export const memoryEnabled = rawMemoryEnabled !== "false";

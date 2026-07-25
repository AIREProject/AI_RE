const rawApiBaseUrl = import.meta.env.VITE_API_BASE_URL as string | undefined;
const rawSaveSlotId = import.meta.env.VITE_SAVE_SLOT_ID as string | undefined;
const rawCompanionId = import.meta.env.VITE_COMPANION_ID as string | undefined;

export const apiBaseUrl = (rawApiBaseUrl ?? "").replace(/\/$/, "");
export const saveSlotId = rawSaveSlotId ?? "save-01";
export const companionId = rawCompanionId ?? "companion-01";

# AI_RE WebApp

Framework-free Vite + strict TypeScript Mobile Chat for MAKO.

The product path uses the fixed public Web identity `AIRE_WEB` with the canonical
scope `AIRE_OPEN / demo-slot-1 / mako`. It does not use pairing links, browser
device credentials, login, token refresh, revoke, or account/save/companion
selectors.

## Local run

```powershell
npm.cmd install
npm.cmd run dev
```

The development server listens on the LAN and proxies `/health` and `/api` to
`https://traip.mtvs2026.work`. Set only `VITE_API_BASE_URL` when an explicit API
origin is needed; save slot, companion, and Web bearer values are fixed in the
application.

Opening or refreshing the page enters Chat immediately and creates a new
browser-session `session_id`. Each submit creates new request and message IDs.
Chat failures are displayed without automatic retry, and the Memory tab remains
a placeholder until a user Memory API is available.

## Verification

```powershell
npm.cmd run typecheck
npm.cmd run build
```

On an actual mobile browser, verify one successful MAKO response, immediate Chat
entry after refresh, duplicate-submit blocking, and visible errors for 401, 403,
timeout, network failure, invalid JSON, and malformed response data. A failure
must never append a companion message or automatically resend the user message.

## GPT Sites hosting

The production build emits static assets under `dist/client` and adds the
Cloudflare Worker entry point required by GPT Sites. The Worker serves the
Vite application and proxies same-origin `/health` and `/api/*` requests to
`https://traip.mtvs2026.work`, so the hosted client keeps the same relative API
boundary as local development.

The current private deployment is available at
`https://aire-mako-chat.lunau1f320.chatgpt.site`. Access policy and future
versions are managed through GPT Sites; secrets and runtime credentials must
not be added to `.openai/hosting.json`.

# AI_RE WebApp

Framework-free Vite + strict TypeScript Mobile Chat and Offline Task UI for MAKO.

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

## Offline Tasks

The Task tab uses `POST /api/v1/tasks` and
`GET /api/v1/tasks?save_slot_id=demo-slot-1`. It creates only the following
seed-backed demo requests:

- Gathering: `Branch`, `Stone`; required integer quantity from 1 through 50
- Crafting: `ShoddyBandage`, `Rope`, `Axe_Stone`, `Pickaxe_Stone`; required
  positive safe-integer quantity

Scouting cannot be selected for creation. Existing Scouting tasks are still
validated and displayed when returned by the list API.

The list refreshes only when the Task tab is opened, the status filter changes,
the refresh button is pressed, or a create request succeeds. There is no polling
or automatic retry. Create and list responses are accepted only after runtime
validation from `unknown`, including request-ID correlation and canonical save
slot scope.

`Pending`, `InProgress`, `Completed`, and `Claimed` are Backend Task states.
Neither `Completed` nor `Claimed` proves that UE Inventory rewards were applied.
The WebApp never calls the GameClient-only `/start`, `/complete`, or `/claim`
routes, and AX-W02 does not call the `/collect` prototype.

## Verification

```powershell
npm.cmd run typecheck
npm.cmd run build
```

On an actual mobile browser, verify one successful MAKO response, immediate Chat
entry after refresh, duplicate-submit blocking, and visible errors for 401, 403,
timeout, network failure, invalid JSON, and malformed response data. A failure
must never append a companion message or automatically resend the user message.

For Offline Tasks, verify both Gathering and Crafting creation, then confirm the
returned Task ID appears in the unfiltered list. Send the same create body and
`X-Request-ID` twice with a controlled HTTP client and confirm both responses use
the same Task ID and only one list entry exists. Also verify empty lists, all four
status filters, duplicate-submit blocking, and rejection of blank, zero,
negative, fractional, and Gathering quantities above 50.

Exercise create and list failures separately for 401, 403, timeout, network
failure, invalid JSON, and malformed response data. Error-envelope `details` and
authentication values must never appear in the UI or console. A create timeout
must not retry automatically; refresh the list before deciding whether to submit
a new request.

## GPT Sites hosting

The production build emits static assets under `dist/client` and adds the
Cloudflare Worker entry point required by GPT Sites. The Worker serves the
Vite application and proxies same-origin `/health` and `/api/*` requests to
`https://traip.mtvs2026.work`, so the hosted client keeps the same relative API
boundary as local development.

The current public deployment is available at
`https://aire-mako-chat.lunau1f320.chatgpt.site`. Public access was explicitly
approved for the fixed-identity single-player demo on 2026-08-11. Access policy
and future versions are managed through GPT Sites; secrets and runtime
credentials must not be added to `.openai/hosting.json`.

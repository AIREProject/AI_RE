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
`https://traip.mtvs2026.work` when `VITE_DEV_API_PROXY_TARGET` is unset or empty.
To use a local Backend instead, start it on port 8000 and run:

```powershell
$env:VITE_DEV_API_PROXY_TARGET = "http://127.0.0.1:8000"
npm.cmd run dev
```

The development proxy target is trimmed and trailing slashes are removed before
the same target is applied to both routes. `VITE_API_BASE_URL` has a different
purpose: when set, the browser calls that explicit API origin instead of the
same-origin Vite proxy. Save slot, companion, and Web bearer values are fixed in
the application.

A successful `/health` response confirms HTTP connectivity and server
configuration only. It does not verify database migrations or queries, or an
actual LLM request.

Opening or refreshing the page enters Chat immediately and creates a new
browser-session `session_id`. Each submit creates new request and message IDs.
Chat failures are displayed without automatic retry, and the Memory tab remains
a placeholder until a user Memory API is available.

While a Chat request is waiting, the user can cancel the browser-side wait. The
already displayed user message remains, no companion response is appended for
that cancelled request, and the input controls become available immediately.
Cancellation does not guarantee that Backend processing or persistence was
rolled back because the server may already have received the request. Cancelled
or timed-out Chat requests are never retried automatically; a later explicit
submit creates new request and message IDs.

## Offline Tasks

The Task tab uses `POST /api/v1/tasks` and
`GET /api/v1/tasks?save_slot_id=demo-slot-1`. It creates only the following
seed-backed demo requests:

- Gathering: `PlantStem` (`나무`), quantity 1 through 50
- Crafting: `ShoddyBandage`, quantity 1 through 50

Scouting cannot be selected for creation. Existing Scouting tasks are still
validated and displayed when returned by the list API.

The list refreshes only when the Task tab is opened, the status filter changes,
the refresh button is pressed, or a create request succeeds. There is no polling
or automatic retry. Create and list responses are accepted only after runtime
validation from `unknown`, including request-ID correlation and canonical save
slot scope.

Quantity tasks start InProgress immediately. Each explicit list refresh displays
the server-calculated integer `progress_quantity`; the browser does not estimate
time locally or poll. If at least one unit is ready, launching UE finalizes that
displayed-at-sync quantity and closes the uncompleted remainder.

The default active view hides Claimed history. Selecting the full-history or
Claimed filter exposes terminal records. A Claimed task with result quantity
zero is labeled explicitly and is not presented as an Inventory reward.

Pending and InProgress cards expose an explicit reservation delete action using
`DELETE /api/v1/tasks/{task_id}`. The action asks for confirmation and refreshes
the list after a 204 response. Completed and Claimed cards cannot be deleted.
Delete timeout does not retry automatically because the server may already have
applied it; refresh the list to reconcile the result.

The WebApp does not present a client-owned duration. The deployed Backend owns
the elapsed-time policy. `Pending` waits for UE synchronization, `Completed`
waits for UE Inventory application, and `Claimed` is the existing server Task
state reached after UE confirms its SaveGame write. It is not a separate
settlement receipt. The WebApp never calls GameClient-only start, complete, or
claim routes and does not poll.

If synchronization happens before the first unit is complete, the target
Backend keeps the task InProgress with no result quantity. UE does not mutate
Inventory or claim it and a later launch or explicit synchronization evaluates
the elapsed time again.

The deployed Backend snapshots its current Admin policy into each new Task. The
default policies are 5 seconds per `PlantStem` Gathering unit and 10 seconds per
`ShoddyBandage` Crafting unit. Policy changes affect only subsequently created
Tasks. The browser does not invent or cache a replacement duration.

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
negative, fractional, and quantities above 50 for both task types.

Verify that Pending and InProgress reservations can be deleted, while Completed
and Claimed cards have no delete button. Confirm cancellation can remove the old
blocking reservation and that a delete timeout directs the user to refresh
instead of automatically resending the request.

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
boundary as local development. `VITE_DEV_API_PROXY_TARGET` configures only the
Vite development server; it is not injected as a production API origin and does
not change the Sites Worker target.

The current public deployment is available at
`https://aire-mako-chat.lunau1f320.chatgpt.site`. Public access was explicitly
approved for the fixed-identity single-player demo on 2026-08-11. Access policy
and future versions are managed through GPT Sites; secrets and runtime
credentials must not be added to `.openai/hosting.json`.

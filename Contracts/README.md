# AI_RE Contracts

This directory is the executable source of truth for data exchanged between the Backend, WebApp, Unreal Engine, and AIService.

## Layout

- `openapi.yaml`: HTTP API contract.
- `schemas/`: reusable JSON Schema definitions.
- `fixtures/`: normal and failure examples used by every implementation.
- `schemas/websocket-chat-frame.schema.json`: WebSocket Auth와 Chat Frame envelope.

## Rules

- Breaking semantic changes increment `schema_version`.
- Unknown optional fields may be ignored, but unsupported required schema versions must be rejected.
- Display names, Unreal class names, and array indexes are not stable protocol identifiers.
- `InGame` uses `GameWorld` time and `Offline` uses `RealWorld` time.
- Command and Memory candidates are untrusted until validated by the owning system.
- AIService Request v2 exposes only the current Message ID and Backend-verified
  Event IDs as valid Memory Candidate sources.
- Fixtures must not contain credentials or real user conversation data.

## Identity and development authentication

- There is no multi-user `user_id`. C owns one local `profile_id` and scopes data by `save_slot_id` and `companion_id`.
- C resolves `profile_id`, `device_id`, and the Device role from the bearer token.
- A WebClient may inspect and revoke only its authenticated identity through `/api/v1/devices/me`; it cannot supply another Device ID.
- Client-provided `profile_id` and `device_id` are optional claims. When present, C compares them with the authenticated identity and rejects a mismatch.
- The local GameClient development token is accepted only by `register-game` as a bootstrap credential.
- Registered GameClient and paired WebClient tokens are validated from persisted HMAC-SHA256 hashes. Raw tokens and Pairing Codes are never stored.
- Tokens never appear in Contracts fixtures, logs, URLs, or committed configuration.

## Request correlation and idempotency

- Clients may send `X-Request-ID`; when a body also has `request_id`, the values must match.
- When the header is absent, a body `request_id` becomes the effective correlation ID; requests without either receive a Backend UUID.
- The effective value is returned in `X-Request-ID`, error envelopes and structured request logs.
- Repeating the same authenticated Profile plus `request_id` and identical content returns the stored Chat response.
- Reusing that key with different content returns `DuplicateRequest` without creating messages.

## Initial scope

The contract covers health, capabilities, persistent Device Pairing, Chat, Event, Command candidates, Memory candidates, the AIService boundary, and the shared error envelope.

## WebSocket Chat

- Native GameClients connect to `/api/v1/game/chat` with a bearer token in the
  WebSocket handshake `Authorization` header.
- Browser WebClients connect to `/api/v1/mobile/chat` and send
  `{ "type": "auth", "token": "..." }` as the first frame.
- Chat messages use `chat`, `chat_response`, and `error` envelopes whose payloads
  reuse the existing Chat Request, Chat Response, and Error Envelope schemas.
- Unknown frame types are ignored for forward compatibility.
- HTTP `POST /api/v1/chat` remains available during migration.

# Contract Fixtures

| Fixture | Expected result |
|---|---|
| `chat/offline-request.valid.json` | Valid Offline Chat request using RealWorld time |
| `chat/ingame-request.valid.json` | Valid InGame Chat request using GameWorld time |
| `chat/response.valid.json` | Valid normalized response with no direct game execution |
| `chat/request.invalid-missing-time-context.json` | Invalid because `time_context` is required |
| `commands/follow.valid.json` | Valid Follow command candidate |
| `events/combat-started.valid.json` | Valid meaningful game event |
| `memory/user-shared-event.valid.json` | Valid source-backed memory candidate |
| `errors/ai-service-unavailable.valid.json` | Valid stable unavailable error |
| `errors/request-too-large.valid.json` | Valid request size limit error |
| `errors/duplicate-request.valid.json` | Valid conflicting idempotency key error |
| `ai-service/request.valid.json` | Valid provider-independent AIService v2 normal input without authentication data |
| `ai-service/result.valid.json` | Valid normalized general-conversation Mock output |
| `ai-service/request.command.valid.json` | Valid InGame input with one allowed Command |
| `ai-service/result.command.valid.json` | Valid allowed Command Candidate Mock output |
| `ai-service/request.memory.valid.json` | Valid Offline input with a current Message source |
| `ai-service/result.memory.valid.json` | Valid source-backed Memory Candidate Mock output |
| `devices/me.valid.json` | Valid active WebClient self identity without credentials |
| `devices/revoke-me.valid.json` | Valid WebClient self-revocation result |

Semantic checks such as command expiry, duplicate IDs, target validity, and source existence are application validations in addition to JSON Schema validation.

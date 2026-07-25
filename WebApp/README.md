# AI_RE WebApp

Framework-free Vite + strict TypeScript foundation for the B phone WebApp. It verifies connectivity to the mock Backend without committing the project to a UI framework before the Offline Chat task.

## Local run

```powershell
Copy-Item .env.example .env
npm.cmd install
npm.cmd run dev
```

Vite listens on the LAN and proxies `/health` and `/api` to the local Backend during development.

Open a `#/pair/{8-digit-code}` link or enter the Pairing Code in the WebApp.
The resulting WebClient credential is stored under
`aire.web_device_credentials.v1` and validated through `/api/v1/devices/me` on
later visits. Device Tokens must never be added to Vite environment variables.

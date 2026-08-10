import { defineConfig } from "vite";

export default defineConfig({
  build: {
    outDir: "dist/client",
  },
  server: {
    host: "0.0.0.0",
    proxy: {
      "/health": {
        target: "https://traip.mtvs2026.work",
        changeOrigin: true,
      },
      "/api": {
        target: "https://traip.mtvs2026.work",
        changeOrigin: true,
      },
    },
  },
});

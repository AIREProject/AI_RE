import { defineConfig } from "vite";

export default defineConfig({
  server: {
    host: "0.0.0.0",
    proxy: {
      "/health": {
        target: "https://api.mtvs2026.work",
        changeOrigin: true,
      },
      "/api": {
        target: "https://api.mtvs2026.work",
        changeOrigin: true,
      },
    },
  },
});

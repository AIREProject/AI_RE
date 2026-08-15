import { defineConfig, loadEnv } from "vite";

const deployedBackendOrigin = "https://traip.mtvs2026.work";

export default defineConfig(({ mode }) => {
  const env = loadEnv(mode, process.cwd(), "VITE_");
  const configuredProxyTarget = env.VITE_DEV_API_PROXY_TARGET?.trim();
  const proxyTarget = (configuredProxyTarget || deployedBackendOrigin).replace(
    /\/+$/,
    "",
  );

  return {
    build: {
      outDir: "dist/client",
    },
    server: {
      host: "0.0.0.0",
      proxy: {
        "/health": {
          target: proxyTarget,
          changeOrigin: true,
        },
        "/api": {
          target: proxyTarget,
          changeOrigin: true,
        },
      },
    },
  };
});

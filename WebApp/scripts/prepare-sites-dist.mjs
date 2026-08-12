import { copyFile, cp, mkdir, rm } from "node:fs/promises";
import { resolve } from "node:path";

const root = resolve(import.meta.dirname, "..");
const clientDirectory = resolve(root, "dist", "client");
const deploymentDirectory = resolve(root, "dist");
const serverDirectory = resolve(root, "dist", "server");
const metadataDirectory = resolve(root, "dist", ".openai");

await mkdir(serverDirectory, { recursive: true });
await mkdir(metadataDirectory, { recursive: true });
await rm(resolve(deploymentDirectory, "assets"), {
  recursive: true,
  force: true,
});
await cp(
  resolve(clientDirectory, "assets"),
  resolve(deploymentDirectory, "assets"),
  { recursive: true },
);
await copyFile(
  resolve(clientDirectory, "index.html"),
  resolve(deploymentDirectory, "index.html"),
);
await copyFile(
  resolve(root, "sites", "worker.js"),
  resolve(serverDirectory, "index.js"),
);
await copyFile(
  resolve(root, ".openai", "hosting.json"),
  resolve(metadataDirectory, "hosting.json"),
);

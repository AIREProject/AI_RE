import { copyFile, mkdir } from "node:fs/promises";
import { resolve } from "node:path";

const root = resolve(import.meta.dirname, "..");
const serverDirectory = resolve(root, "dist", "server");
const metadataDirectory = resolve(root, "dist", ".openai");

await mkdir(serverDirectory, { recursive: true });
await mkdir(metadataDirectory, { recursive: true });
await copyFile(
  resolve(root, "sites", "worker.js"),
  resolve(serverDirectory, "index.js"),
);
await copyFile(
  resolve(root, ".openai", "hosting.json"),
  resolve(metadataDirectory, "hosting.json"),
);

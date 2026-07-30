import { cp, mkdir } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";

const scriptDirectory = path.dirname(fileURLToPath(import.meta.url));
const webAppDirectory = path.resolve(scriptDirectory, "..");
const sourceDirectory = path.resolve(webAppDirectory, "..", "PresentationWeb");
const targetDirectory = path.join(webAppDirectory, "public", "presentation");
const presentationEntries = [
  "index.html",
  "styles.css",
  "presentation.js",
  "assets",
];

await mkdir(targetDirectory, { recursive: true });

for (const entry of presentationEntries) {
  await cp(
    path.join(sourceDirectory, entry),
    path.join(targetDirectory, entry),
    { recursive: true, force: true },
  );
}

console.log(`Presentation synced to ${targetDirectory}`);

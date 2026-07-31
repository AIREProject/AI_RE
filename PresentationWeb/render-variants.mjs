import fs from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";
import puppeteer from "puppeteer-core";

const scriptDirectory = path.dirname(fileURLToPath(import.meta.url));
const outputDirectory = path.join(scriptDirectory, "output", "pdf");
const baseUrl = process.env.AIRE_PRESENTATION_URL ?? "http://127.0.0.1:5173";

const variants = [
  {
    route: "/presentation/index.html",
    output: "AI_RE_Presentation_current_stable.pdf",
  },
  {
    route: "/presentation/variants/pdf-emphasis/index.html",
    output: "AI_RE_Presentation_pdf_emphasis.pdf",
  },
];

const printStyles = `
  @page { size: 1920px 1080px; margin: 0; }
  *, *::before, *::after {
    animation: none !important;
    transition: none !important;
  }
  body, html {
    margin: 0;
    padding: 0;
    height: auto;
    overflow: visible !important;
  }
  .deck {
    position: static !important;
    width: 100vw !important;
    height: auto !important;
    overflow: visible !important;
    display: block !important;
  }
  .slide {
    position: relative !important;
    visibility: visible !important;
    opacity: 1 !important;
    transform: none !important;
    page-break-after: always !important;
    break-after: page !important;
    width: 100vw !important;
    height: 100vh !important;
    z-index: 1 !important;
  }
  .slide:last-of-type {
    page-break-after: auto !important;
    break-after: auto !important;
  }
  .menu-toggle, .slide-menu, .menu-scrim, .phone-modal, .deck-controls {
    display: none !important;
  }
  .phone-screen iframe {
    display: none !important;
  }
  .phone-print-preview {
    position: absolute;
    inset: 0;
    z-index: 2;
    display: flex !important;
    flex-direction: column;
    align-items: center;
    padding: 7.4cqw 1.2cqw 1.8cqw;
    background: #f7f2e9;
    color: #27232c;
    text-align: center;
  }
  .phone-print-preview__mark {
    display: grid;
    width: 3.4cqw;
    height: 3.4cqw;
    margin-bottom: 1cqw;
    place-items: center;
    border-radius: 1cqw;
    background: linear-gradient(135deg, #e4a845, #c87524);
    color: white;
    font: 800 0.9cqw/1 serif;
    box-shadow: 0 0.5cqw 1.1cqw rgba(177, 105, 29, 0.24);
  }
  .phone-print-preview small {
    color: #8a5b28;
    font-size: 0.38cqw;
    font-weight: 800;
    letter-spacing: 0.1em;
  }
  .phone-print-preview strong {
    margin-top: 0.45cqw;
    font-size: 1cqw;
  }
  .phone-print-preview p {
    margin: 0.7cqw 0 1.6cqw;
    color: #7b7478;
    font-size: 0.43cqw;
  }
  .phone-print-preview label {
    color: #7b7478;
    font-size: 0.36cqw;
    font-weight: 800;
    letter-spacing: 0.12em;
  }
  .phone-print-preview__input,
  .phone-print-preview__button {
    box-sizing: border-box;
    width: 100%;
    margin-top: 0.4cqw;
    padding: 0.6cqw;
    border-radius: 0.65cqw;
    font-size: 0.72cqw;
    font-weight: 800;
  }
  .phone-print-preview__input {
    border: 1px solid #e7dfd6;
    background: white;
    color: #8c8588;
  }
  .phone-print-preview__button {
    background: linear-gradient(90deg, #efa635, #dd8824);
    color: white;
  }
`;

await fs.mkdir(outputDirectory, { recursive: true });

const browser = await puppeteer.launch({
  executablePath: "C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe",
  headless: true,
});

try {
  const page = await browser.newPage();
  await page.setViewport({ width: 1920, height: 1080 });

  for (const variant of variants) {
    console.log(`Loading ${variant.route}`);
    const response = await page.goto(new URL(variant.route, baseUrl).href, {
      waitUntil: "domcontentloaded",
      timeout: 60000,
    });
    if (!response?.ok()) {
      throw new Error(`Failed to load ${variant.route}: HTTP ${response?.status() ?? "unknown"}`);
    }
    await page.addStyleTag({ content: printStyles });
    await new Promise((resolve) => setTimeout(resolve, 1500));
    await page.pdf({
      path: path.join(outputDirectory, variant.output),
      printBackground: true,
      landscape: true,
      preferCSSPageSize: true,
    });
    console.log(`Rendered ${variant.output}`);
  }
} finally {
  await browser.close();
}

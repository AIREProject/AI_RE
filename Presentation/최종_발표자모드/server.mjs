import { createReadStream } from "node:fs";
import { stat } from "node:fs/promises";
import { createServer } from "node:http";
import path from "node:path";
import { fileURLToPath } from "node:url";

const host = "127.0.0.1";
const port = Number.parseInt(process.env.AIRE_PRESENTATION_PORT ?? "4173", 10);
const root = path.dirname(fileURLToPath(import.meta.url));

const mimeTypes = new Map([
  [".css", "text/css; charset=utf-8"],
  [".html", "text/html; charset=utf-8"],
  [".jpeg", "image/jpeg"],
  [".jpg", "image/jpeg"],
  [".js", "text/javascript; charset=utf-8"],
  [".json", "application/json; charset=utf-8"],
  [".md", "text/markdown; charset=utf-8"],
  [".mp4", "video/mp4"],
  [".pdf", "application/pdf"],
  [".png", "image/png"],
  [".pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation"]
]);

const sendText = (response, statusCode, message) => {
  const body = Buffer.from(message, "utf8");
  response.writeHead(statusCode, {
    "Content-Type": "text/plain; charset=utf-8",
    "Content-Length": body.length,
    "Cache-Control": "no-store"
  });
  response.end(body);
};

const resolveRequestPath = (requestUrl) => {
  const url = new URL(requestUrl, `http://${host}:${port}`);
  const decodedPath = decodeURIComponent(url.pathname);
  const requestedPath = decodedPath === "/" ? "/index.html" : decodedPath;
  const absolutePath = path.resolve(root, `.${requestedPath}`);
  const relativePath = path.relative(root, absolutePath);
  if (relativePath.startsWith("..") || path.isAbsolute(relativePath)) return null;
  return absolutePath;
};

const parseRange = (rangeHeader, fileSize) => {
  const match = /^bytes=(\d*)-(\d*)$/.exec(rangeHeader ?? "");
  if (!match) return null;

  let start;
  let end;
  if (match[1] === "") {
    const suffixLength = Number.parseInt(match[2], 10);
    if (!Number.isFinite(suffixLength) || suffixLength <= 0) return null;
    start = Math.max(0, fileSize - suffixLength);
    end = fileSize - 1;
  } else {
    start = Number.parseInt(match[1], 10);
    end = match[2] === "" ? fileSize - 1 : Number.parseInt(match[2], 10);
  }

  if (!Number.isFinite(start) || !Number.isFinite(end) || start < 0 || start >= fileSize || end < start) {
    return null;
  }
  return { start, end: Math.min(end, fileSize - 1) };
};

const server = createServer(async (request, response) => {
  if (request.method !== "GET" && request.method !== "HEAD") {
    response.setHeader("Allow", "GET, HEAD");
    sendText(response, 405, "Method Not Allowed");
    return;
  }

  let filePath;
  try {
    filePath = resolveRequestPath(request.url ?? "/");
  } catch (_) {
    sendText(response, 400, "Bad Request");
    return;
  }
  if (!filePath) {
    sendText(response, 403, "Forbidden");
    return;
  }

  let fileStat;
  try {
    fileStat = await stat(filePath);
  } catch (_) {
    sendText(response, 404, "Not Found");
    return;
  }
  if (!fileStat.isFile()) {
    sendText(response, 404, "Not Found");
    return;
  }

  const extension = path.extname(filePath).toLowerCase();
  const contentType = mimeTypes.get(extension) ?? "application/octet-stream";
  const range = parseRange(request.headers.range, fileStat.size);
  const noCache = [".html", ".css", ".js", ".md"].includes(extension);
  const commonHeaders = {
    "Accept-Ranges": "bytes",
    "Cache-Control": noCache ? "no-store" : "public, max-age=3600",
    "Content-Type": contentType
  };

  if (request.headers.range && !range) {
    response.writeHead(416, {
      ...commonHeaders,
      "Content-Range": `bytes */${fileStat.size}`
    });
    response.end();
    return;
  }

  if (range) {
    const contentLength = range.end - range.start + 1;
    response.writeHead(206, {
      ...commonHeaders,
      "Content-Length": contentLength,
      "Content-Range": `bytes ${range.start}-${range.end}/${fileStat.size}`
    });
    if (request.method === "HEAD") {
      response.end();
      return;
    }
    createReadStream(filePath, { start: range.start, end: range.end }).pipe(response);
    return;
  }

  response.writeHead(200, { ...commonHeaders, "Content-Length": fileStat.size });
  if (request.method === "HEAD") {
    response.end();
    return;
  }
  createReadStream(filePath).pipe(response);
});

server.on("error", (error) => {
  console.error(`발표 서버를 시작하지 못했습니다: ${error.message}`);
  process.exitCode = 1;
});

server.listen(port, host, () => {
  console.log(`AI : RE 발표 서버: http://${host}:${port}/`);
  console.log(`발표자 모드: http://${host}:${port}/?presenter=1`);
  console.log("종료하려면 Ctrl+C를 누르세요.");
});

const backendOrigin = "https://traip.mtvs2026.work";

function isBackendRoute(pathname) {
  return pathname === "/health" || pathname.startsWith("/api/");
}

async function proxyBackend(request) {
  const incomingUrl = new URL(request.url);
  const backendUrl = new URL(incomingUrl.pathname + incomingUrl.search, backendOrigin);
  const headers = new Headers(request.headers);
  headers.delete("host");

  return fetch(
    new Request(backendUrl, {
      method: request.method,
      headers,
      body: request.body,
      redirect: "manual",
    }),
  );
}

export default {
  async fetch(request, env) {
    const url = new URL(request.url);

    if (isBackendRoute(url.pathname)) {
      return proxyBackend(request);
    }

    const response = await env.ASSETS.fetch(request);
    if (response.status !== 404 || request.method !== "GET") {
      return response;
    }

    const acceptsHtml = request.headers.get("accept")?.includes("text/html") ?? false;
    if (!acceptsHtml) {
      return response;
    }

    return env.ASSETS.fetch(
      new Request(new URL("/index.html", request.url), {
        headers: request.headers,
      }),
    );
  },
};

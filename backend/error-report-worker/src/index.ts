/**
 * WinKit error-report Worker.
 *
 * WinKit posts a diagnostic report to POST /report. This Worker validates it,
 * rate-limits by IP, and stores it in Workers KV — it does not talk to GitHub
 * or any other third party. A separate, authenticated GET /reports (and
 * GET /reports/:errorId) lets you pull stored reports later from your own
 * site/tooling using PULL_API_KEY.
 *
 * No GitHub token or other third-party credential is needed by this Worker.
 */

export interface Env {
  REPORTS: KVNamespace;
  PULL_API_KEY: string;
}

interface DiagnosticReport {
  errorId: string;
  winKitVersion: string;
  windowsVersion: string;
  architecture: string;
  component: string;
  exceptionType: string;
  exceptionMessage: string;
  stackTrace: string;
  timestamp: string;
}

const ERROR_ID_PATTERN = /^WK-[0-9A-F]{8}$/;
const MAX_FIELD_LENGTH: Record<keyof DiagnosticReport, number> = {
  errorId: 16,
  winKitVersion: 32,
  windowsVersion: 128,
  architecture: 32,
  component: 128,
  exceptionType: 256,
  exceptionMessage: 2000,
  stackTrace: 8000,
  timestamp: 40
};
const MAX_BODY_BYTES = 32 * 1024;

const RATE_LIMIT_MAX_REQUESTS = 10;
const RATE_LIMIT_WINDOW_SECONDS = 60 * 60; // 1 hour
const REPORT_RETENTION_SECONDS = 60 * 60 * 24 * 90; // 90 days
const REPORT_KEY_PREFIX = "report:";
const RATE_LIMIT_KEY_PREFIX = "ratelimit:";

export default {
  async fetch(request: Request, env: Env): Promise<Response> {
    const url = new URL(request.url);

    try {
      if (request.method === "POST" && url.pathname === "/report") {
        return await handleSubmitReport(request, env);
      }

      if (request.method === "GET" && url.pathname === "/reports") {
        return await handleListReports(request, env, url);
      }

      if (request.method === "GET" && url.pathname.startsWith("/reports/")) {
        return await handleGetReport(request, env, url.pathname.slice("/reports/".length));
      }

      return jsonResponse({ error: "Not found" }, 404);
    } catch (err) {
      return jsonResponse({ error: "Internal error" }, 500);
    }
  }
};

async function handleSubmitReport(request: Request, env: Env): Promise<Response> {
  const contentLength = Number(request.headers.get("content-length") ?? "0");
  if (contentLength > MAX_BODY_BYTES) {
    return jsonResponse({ error: "Payload too large" }, 413);
  }

  const clientIp = request.headers.get("cf-connecting-ip") ?? "unknown";
  const rateLimited = await isRateLimited(env, clientIp);
  if (rateLimited) {
    return jsonResponse({ error: "Too many reports from this address. Try again later." }, 429);
  }

  let body: unknown;
  try {
    body = await request.json();
  } catch {
    return jsonResponse({ error: "Malformed JSON" }, 400);
  }

  const validation = validateReport(body);
  if (!validation.ok) {
    return jsonResponse({ error: validation.error }, 400);
  }

  const report = validation.report;
  const key = REPORT_KEY_PREFIX + report.errorId;

  // Never overwrite an existing report under the same error ID.
  const existing = await env.REPORTS.get(key);
  if (existing !== null) {
    return jsonResponse({ error: "A report with this error ID already exists" }, 409);
  }

  const stored = { ...report, receivedAt: new Date().toISOString(), sourceIp: clientIp };
  await env.REPORTS.put(key, JSON.stringify(stored), { expirationTtl: REPORT_RETENTION_SECONDS });

  return jsonResponse({ received: true, errorId: report.errorId }, 201);
}

async function handleListReports(request: Request, env: Env, url: URL): Promise<Response> {
  if (!isAuthorized(request, env)) {
    return jsonResponse({ error: "Unauthorized" }, 401);
  }

  const cursor = url.searchParams.get("cursor") ?? undefined;
  const limitParam = Number(url.searchParams.get("limit") ?? "50");
  const limit = Number.isFinite(limitParam) ? Math.min(Math.max(limitParam, 1), 100) : 50;

  const list = await env.REPORTS.list({ prefix: REPORT_KEY_PREFIX, cursor, limit });
  const reports = await Promise.all(
    list.keys.map(async (k) => {
      const value = await env.REPORTS.get(k.name);
      return value ? JSON.parse(value) : null;
    })
  );

  return jsonResponse({
    reports: reports.filter((r) => r !== null),
    cursor: list.list_complete ? null : list.cursor
  });
}

async function handleGetReport(request: Request, env: Env, errorId: string): Promise<Response> {
  if (!isAuthorized(request, env)) {
    return jsonResponse({ error: "Unauthorized" }, 401);
  }

  const value = await env.REPORTS.get(REPORT_KEY_PREFIX + errorId);
  if (value === null) {
    return jsonResponse({ error: "Not found" }, 404);
  }

  return jsonResponse(JSON.parse(value));
}

function isAuthorized(request: Request, env: Env): boolean {
  const header = request.headers.get("authorization") ?? "";
  const expected = `Bearer ${env.PULL_API_KEY}`;
  return env.PULL_API_KEY.length > 0 && header === expected;
}

async function isRateLimited(env: Env, clientIp: string): Promise<boolean> {
  const window = Math.floor(Date.now() / 1000 / RATE_LIMIT_WINDOW_SECONDS);
  const key = `${RATE_LIMIT_KEY_PREFIX}${clientIp}:${window}`;

  const current = Number((await env.REPORTS.get(key)) ?? "0");
  if (current >= RATE_LIMIT_MAX_REQUESTS) {
    return true;
  }

  await env.REPORTS.put(key, String(current + 1), { expirationTtl: RATE_LIMIT_WINDOW_SECONDS });
  return false;
}

type ValidationResult =
  | { ok: true; report: DiagnosticReport }
  | { ok: false; error: string };

function validateReport(body: unknown): ValidationResult {
  if (typeof body !== "object" || body === null) {
    return { ok: false, error: "Body must be a JSON object" };
  }

  const record = body as Record<string, unknown>;
  const fields: (keyof DiagnosticReport)[] = [
    "errorId", "winKitVersion", "windowsVersion", "architecture",
    "component", "exceptionType", "exceptionMessage", "stackTrace", "timestamp"
  ];

  const report: Partial<DiagnosticReport> = {};
  for (const field of fields) {
    const value = record[field];
    if (typeof value !== "string" || value.length === 0) {
      return { ok: false, error: `Field '${field}' is required and must be a non-empty string` };
    }
    if (value.length > MAX_FIELD_LENGTH[field]) {
      return { ok: false, error: `Field '${field}' exceeds the maximum allowed length` };
    }
    report[field] = value;
  }

  if (!ERROR_ID_PATTERN.test(report.errorId!)) {
    return { ok: false, error: "Field 'errorId' must match WK-XXXXXXXX (8 uppercase hex characters)" };
  }

  if (Number.isNaN(Date.parse(report.timestamp!))) {
    return { ok: false, error: "Field 'timestamp' must be a valid ISO 8601 date" };
  }

  return { ok: true, report: report as DiagnosticReport };
}

function jsonResponse(body: unknown, status = 200): Response {
  return new Response(JSON.stringify(body), {
    status,
    headers: { "content-type": "application/json" }
  });
}

# WinKit error-report Worker

Receives diagnostic reports submitted by WinKit's "Send to Developers" dialog,
validates and rate-limits them, and stores them in Workers KV. It does **not**
talk to GitHub or any other third party — no GitHub token or other
third-party credential is needed here. A separate authenticated endpoint lets
you pull stored reports later from your own site or tooling.

## Endpoints

- `POST /report` — public, rate-limited (10/hour per IP), validated. Stores
  the report keyed by its `errorId`. Returns `{ "received": true, "errorId": "..." }`.
- `GET /reports` — requires `Authorization: Bearer <PULL_API_KEY>`. Lists
  stored reports, newest keys first is not guaranteed (KV list order), supports
  `?limit=` (max 100, default 50) and `?cursor=` for pagination.
- `GET /reports/:errorId` — requires the same bearer token. Returns one report.

Reports are retained for 90 days (`REPORT_RETENTION_SECONDS` in `src/index.ts`),
then expire automatically via KV's `expirationTtl`. Adjust that constant if you
need longer retention before your pull site is ready.

## One-time setup

1. Install the Cloudflare CLI (from this directory):
   ```
   npm install
   ```

2. Log in to Cloudflare:
   ```
   npx wrangler login
   ```

3. Create the KV namespace:
   ```
   npx wrangler kv namespace create REPORTS
   ```
   This prints an `id`. Paste it into `wrangler.toml` in place of
   `REPLACE_WITH_YOUR_KV_NAMESPACE_ID`.

4. Set the pull API key (choose your own long random string — this is what
   your future pull site will send as the bearer token):
   ```
   npx wrangler secret put PULL_API_KEY
   ```

## Deploy

```
npx wrangler deploy
```

This prints the Worker's URL, e.g. `https://winkit-error-reports.<your-subdomain>.workers.dev`.

Update `ReportEndpoint` in
`src/WinKit.Infrastructure/ErrorReportingService.cs` (WinKit's client-side
code) from the `REPLACE-WITH-YOUR-WORKER-URL` placeholder to
`https://<that-url>/report`, then rebuild WinKit.

## Local testing

```
npx wrangler dev
```

Then, from another terminal:

```
curl -X POST http://localhost:8787/report -H "content-type: application/json" -d "{\"errorId\":\"WK-DEADBEEF\",\"winKitVersion\":\"1.0.1\",\"windowsVersion\":\"Windows 11 Pro (Build 26200)\",\"architecture\":\"X64\",\"component\":\"Test\",\"exceptionType\":\"System.Exception\",\"exceptionMessage\":\"test\",\"stackTrace\":\"at Test()\",\"timestamp\":\"2026-08-23T00:00:00Z\"}"

curl http://localhost:8787/reports -H "Authorization: Bearer <your PULL_API_KEY>"
```

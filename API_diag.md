# Diagnostics module API (`mod_diag.h`, commands `diag_*`)

All args are strings. Every command returns a JSON value, or the string `error:<message>` (protocol-level problems only: busy, unknown check id). Commands **block** until done, so call them without awaiting on the UI thread; the page receives progress through events. `diag_cancel*` must be dispatched on a different worker thread than the running scan/fix.

Source of truth for the page behaviour: C# `DiagnosticsViewModel` / `DiagnosticCheckItemViewModel` / `DashboardViewModel`. The Diagnostics page auto-runs a full scan when opened (`InitializeAsync -> RunAsync`).

## Commands

| cmd | args | returns |
|---|---|---|
| `diag_list` | none | `{"checks":[{"id","name","category","categoryLabel","hasFixer"}...], "englishUi":bool, "uiCulture":"en-US"}` in run order (below). |
| `diag_run_all` | none | Runs all 11 checks sequentially (20 s timeout each), blocking. Returns `{"cancelled":bool,"results":[Result...],"summary":Summary\|null}`. `summary` is null when cancelled (the stored summary is NOT updated on cancel; `results` then holds only the finished checks). Error `error:A diagnostics scan is already running.` / `error:A fix is currently running.` if busy. |
| `diag_run_one` | `checkId` (check id; not `id`, which is the request envelope's own key) | `{"cancelled":bool,"result":Result\|null}`. Same timeout/error wrapping, does not touch the summary, emits no events. `error:Unknown check: x` for bad id. |
| `diag_fix` | `checkId` (check id) | `FixResult` (below). Fails with `success:false,message:"No automatic fix is available for this check."` if the id has no fixer. `error:` if a scan or another fix is running. |
| `diag_cancel` | `target` = `scan`\|`fix`\|`all` (default `all`) | `{"cancelledScan":bool,"cancelledFix":bool}` (true = a cancel was requested for something that was running). |
| `diag_cancel_scan` / `diag_cancel_fix` | none | same shape, shortcuts. |
| `diag_summary` | none | `Summary` (below). |
| `diag_status` | none | `{"scanRunning":bool,"fixRunning":bool,"isAdmin":bool}` |

### Result object
```
{ "checkId":"disk-space", "name":"Disk Space", "category":"Storage", "categoryLabel":"Storage",
  "status":"Passed|Informational|Warning|Failed|NotApplicable", "statusLabel":"PASSED|INFO|WARNING|FAILED|N/A",
  "statusColor":"success|accent|warning|error|muted", "severity":"None|Low|Medium|High",
  "sortPriority":0-4, "isIssue":bool,
  "summary":"...", "technicalDetail":string|null, "recommendedAction":string|null,
  "fixAction": null | { "label","kind":"Fix"|"Review","confirmationMessage","isDestructive":bool,"requiresElevation":bool,
                        "reviewTarget": null|"startup-manager"|"services-manager"|"cleanup" } }
```
`technicalDetail` may contain `\r\n` line breaks (render monospace, wrapped, inside a collapsed "Technical details" expander that is hidden when null). `recommendedAction` is a muted line under the summary, hidden when null.

### FixResult
```
{ "success":bool, "cancelled":bool, "value":string|null, "userMessage":string|null, "technicalDetail":string|null,
  "message":string, "note":string|null }
```
- `success:true`: `message` = summary text (C# `Value`), `note` = optional extra warning text (C# `UserMessage`, e.g. "N item(s) were in use ... skipped").
- `success:false`: `message` = user-facing error (C# `UserMessage`), `technicalDetail` = detail for the error dialog.
- `cancelled:true`: user cancelled; `success:false`.

### Summary object (`diag_summary`, `diag_run_all.summary`, event `diag_summary_changed`)
```
{ "hasRun":bool, "totalChecks":int|null, "issueCount":int|null, "ranAt":"ISO-8601 local with offset"|null,
  "ranAtUnixMs":int|null, "fromPreviousSession":bool, "statusIsHealthy":bool|null, "statusMessage":string }
```
Persisted to `%LOCALAPPDATA%\WinKit\diagnostics-summary.json` after every completed (non-cancelled) full scan and reloaded lazily, so the Dashboard has a "last known" value after restart (`fromPreviousSession:true`; the C# app was in-memory only). `issueCount` = number of results with status Warning or Failed. Dashboard mapping (identical to C#): `hasRun:false` -> neutral state, text "Diagnostics haven't been run yet."; 0 issues -> healthy, "Your system is running normally."; 1 -> "1 issue needs your attention."; n -> "n issues need your attention.". The Dashboard must never trigger a scan; "Run diagnostics" navigates to the Diagnostics page.

## Events (via `PushEvent`)
| event | data |
|---|---|
| `diag_check_started` | `{"checkId","name","index":0-based,"total":11}` |
| `diag_progress` | `{"index","total":11,"completed":n,"result":Result}` - one per finished check, in run order. Append to the list as they arrive. |
| `diag_summary_changed` | `Summary` (only after a full uncancelled scan) |
| `diag_finished` | `{"cancelled":bool,"completed":n}` (always, after scan ends, before the command returns) |
| `diag_fix_progress` | `{"checkId","message"}` - short status line; show in the item's muted `StatusMessage` slot. Clear when `diag_fix_finished` arrives. |
| `diag_fix_output` | `{"checkId","line"}` - streamed raw DISM/SFC console lines (only Windows Integrity fix). Optional live log; DISM progress bars arrive as many lines. |
| `diag_fix_finished` | `{"checkId","success":bool,"cancelled":bool}` |

## Check order (as registered; run-all order) and category labels
1 `windows-integrity` "Windows Integrity" (Windows Integrity) - fixer
2 `windows-update` "Windows Update" (Windows Update) - fixer
3 `drivers` "Drivers" (Drivers) - fixer (opens Device Manager)
4 `startup` "Startup Programs" (Startup) - Review only
5 `services` "Services" (Services) - Review only
6 `disk-space` "Disk Space" (Storage) - Review only
7 `temp-files` "Temporary Files" (Storage) - fixer
8 `disk-health` "Disk Health" (Storage) - no action
9 `internet-connectivity` "Internet Connectivity" (Network) - no action
10 `dns-resolution` "DNS Resolution" (Network) - fixer
11 `network-adapter` "Network Adapter" (Network) - no action

Fixer exists for: windows-integrity, windows-update, drivers, temp-files, dns-resolution. A button is offered only when the result's `fixAction` is non-null (conditions are inside each check; e.g. DNS only when failing, Drivers only on Warning, Integrity only on Failed).

## UI behaviour the page must replicate
**Layout:** header "Diagnostics" / "Analyze your system for configuration, integrity, performance, and maintenance issues."; buttons: `Run Diagnostics` (primary, disabled while a fix runs) and `Cancel` (secondary, only visible while scanning -> `diag_cancel` target `scan`).
**While scanning:** card "Running diagnostics..." + muted "(N checks completed)" (N = results received). Results list is cleared at scan start and filled by `diag_progress`. All fix buttons are disabled while scanning.
**Summary card** (visible only when a scan has finished: `hasRun && !running`): if issues == 0 -> "No issues found. Your system is running normally." else "N issue(s) found". Under it muted: "P passed · W warnings · F failed" (counts of status Passed / Warning / Failed; Informational and N/A are counted in none). Checkbox "Show only issues" (filters to `isIssue`), and a secondary button "Review Issues" (sets that checkbox on) shown only when issues > 0. `IsHealthy` = ran && !running && issues == 0.
**List:** grouped by `categoryLabel` (group header muted text), items sorted ascending by `sortPriority` (Failed 0, Warning 1, Informational 2, Passed 3, N/A 4; stable, ties keep run order). Group order follows the first item in the sorted list (so a group containing a Failed item floats to the top).
**Item card:** title = `name`, status badge = `statusLabel` coloured by `statusColor` (success=Brush.Success, warning=Brush.Warning, error=Brush.Error, accent=Brush.Accent for INFO, muted=Brush.TextMuted for N/A); `summary` (secondary text); `recommendedAction` (muted, if any); busy status message (muted, if any); "Technical details" expander with monospace `technicalDetail` (if any); on the right a secondary button with `fixAction.label` (only if `fixAction` non-null).
**Clicking the action button** (disabled while any fix is running; enabled only when no scan is running):
1. Confirm dialog: title `"<label> - <name>"`, message `confirmationMessage` + (`requiresElevation` ? `" Windows may prompt for administrator permission."` : `""`), confirm button text = `label`, destructive style if `isDestructive`. Cancel -> nothing.
2. `kind == "Review"`: no backend call; navigate by `reviewTarget`: `startup-manager` -> System Tools page, Startup Manager tab; `services-manager` -> System Tools, Services Manager tab; `cleanup` -> Cleanup page.
3. `kind == "Fix"`: set item busy (button turns into `Cancel` -> `diag_cancel` target `fix`, always enabled; other action buttons and "Run Diagnostics" disabled), call `diag_fix {id}` and show `diag_fix_progress` messages on the item. On result:
   - success: activity log (Success) `"<name>: <message>"`; success toast title `"<name> fixed"`, description `message`; if `note` non-empty a second warning toast titled "Note" with `note`.
   - failure (not cancelled): activity log (Error) `"<name>: fix failed"`; error dialog title `"<label> failed"`, text `message`, expandable detail `technicalDetail`.
   - cancelled: activity log (Info) `"<name>: fix cancelled"`.
   - In every case clear busy state, then **re-run the full scan** (`diag_run_all`).
**Activity log (UI-owned) after a scan:** 0 issues -> Success "Diagnostics completed - no issues found"; else Warning "Diagnostics completed - N issue(s) found"; cancelled -> Info "Diagnostics scan cancelled".

## Behavioural notes
- Per-check timeout 20 s -> Informational result "This check took too long to respond and was skipped." Internal error -> Informational "This check could not complete due to an unexpected error." (detail = error text).
- Windows Integrity check/fix require an English installed UI language (else NotApplicable / fix fails with explanation).
- `diag_fix` on Integrity and Windows Update triggers a UAC prompt (the Integrity fix can run up to 30 min per command; streamed via `diag_fix_output`). Declining UAC yields a failure result.
- Temp-files fix deletes the user temp dir and `%SystemRoot%\Temp` contents; message `"Removed N file(s), freeing X GB."`, with a `note` if items were skipped.
- Drivers, Disk Health and the Services third-party-security lookups use PowerShell/CIM (about 1-3 s each); Disk Health may be N/A when not elevated.

Note (windows-integrity): DISM only runs elevated. Without administrator rights the check returns `Informational` with a "Check as Administrator" fix; that fixer runs `DISM /CheckHealth` elevated first and only continues to RestoreHealth + SFC if corruption is found. The outcome is remembered for the rest of the session, so later non-elevated scans show it instead of the informational result.

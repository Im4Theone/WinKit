# API_cleanup.md - mod_cleanup.h (port of WinKit.Cleanup)

Args are strings. Call failures return `error:<message>`. Commands block (worker thread). `opId` = caller-chosen id for events/cancel.

## Commands (4)
| cmd | args | returns |
|---|---|---|
| `cleanup_categories` | - | Static array of CategoryInfo (no scanning, instant). |
| `cleanup_scan` | `opId` | ScanResult. Streams `cleanup_scan_category`. |
| `cleanup_run` | `categories` (comma/space separated ids, e.g. `UserTemp,ThumbnailCache`; case-insensitive; pass in display/scan order, duplicates ignored), `opId` | RunResult. Streams `cleanup_progress`, `cleanup_category_done`. Errors: `error:No categories selected.` / `error:Unknown category.` |
| `cleanup_cancel` | `opId` | `true` if a running scan/run with that opId was signalled. (Extra: the C# UI has no cancel button; the executor honours it between categories and between files.) |

## Shapes
**Category ids** (enum names): `UserTemp`, `WindowsTemp`, `ThumbnailCache`, `RecycleBin`, `WindowsUpdateCache` - this is also the scan/display order.

**CategoryInfo**: `{"category","displayName","description","locations":[path],"defaultSelected":bool,"needsAdmin":bool}`

**ScanResult**: `{"cancelled":bool,"isElevated":bool,"categories":[Scan...]}`;
**Scan** = CategoryInfo fields + `"sizeBytes"` (int64), `"fileCount"` (RecycleBin: item count clamped to int max), `"sizeGb"` (= sizeBytes/1024^3, 6 decimals).

**RunResult**: `{"success":true,"userMessage":string|null,"technicalDetail":null,"cancelled":bool,"summary":{"bytesFreed","filesRemoved","filesSkipped","gbFreed"},"perCategory":[{"category","bytesFreed","filesRemoved","filesSkipped"}...]}`
`userMessage` is non-null when something was skipped: elevated -> `"N item(s) were in use and were skipped."`, not elevated -> `"N item(s) were in use or protected and were skipped. To remove protected system files, go to Settings > Advanced > Run as Administrator."`

## Events
| event | payload |
|---|---|
| `cleanup_scan_category` | `{"opId","scan":Scan}` - emitted as each category finishes scanning |
| `cleanup_progress` | `{"opId","category","message"}` - category start. Messages: `Removing user temporary files...`, `Removing Windows temporary files...`, `Clearing thumbnail cache...`, `Emptying the Recycle Bin...`, `Removing Windows Update cache...` |
| `cleanup_category_done` | `{"opId","result":{"category","bytesFreed","filesRemoved","filesSkipped"}}` |

## Behaviour notes (what the native code does; identical to C# unless marked)
- Locations: UserTemp = %TEMP% (GetTempPath, long form, trailing `\`); WindowsTemp = %SystemRoot%\Temp (fallback C:\Windows); ThumbnailCache = %LOCALAPPDATA%\Microsoft\Windows\Explorer, only top-level `thumbcache_*.db` / `iconcache_*.db`; RecycleBin = shell recycle bin (all drives; location text `Recycle Bin`); WindowsUpdateCache = %SystemRoot%\SoftwareDistribution\Download.
- Scan: recursive file count/size, reparse points (junctions/symlinks) skipped, inaccessible folders ignored. No age thresholds exist in the C# (everything in these folders is a candidate).
- Delete: contents of the root are removed, never the root itself; files get attributes reset then deleted; in-use / access-denied files are counted in `filesSkipped`, never forced; empty sub-folders are removed afterwards. Recycle bin: size queried, then SHEmptyRecycleBin (no UI); `bytesFreed` increases by the queried size only if the empty succeeded (bin counts do not touch filesRemoved).
- Added safety (not in C#): directory junctions/symlinks are never followed (only the link is removed, counted as 1 removed); roots that are a drive root or a system/user root (user profile, Windows, System32, Program Files, ProgramData) are refused (e.g. a hostile TEMP=C:\).
- Admin: `WindowsTemp` and `WindowsUpdateCache` are the categories where protected files typically get skipped without elevation (`needsAdmin:true`, informational; C# only reports it through the skipped message). The C# has no "risky" flag; the only category with special treatment is RecycleBin (not pre-selected).

## UI behaviours to replicate (CleanupViewModel)
- Scan automatically on page open; also a Rescan button; show scanning state. After a rescan keep the user's previous checkbox state per category; for new categories pre-select everything **except RecycleBin** (`defaultSelected`).
- Footer total = sum of `sizeGb` of selected categories (display `0.##` style: up to 2 decimals, trailing zeros trimmed).
- Clean button: do nothing if nothing selected. Confirm dialog: title `Clean selected items`, message `This will remove {sum sizeGb, 0.##} GB across {n} categor{y|ies}. This can't be undone.`, confirm button `Clean up`, destructive style.
- Send selected categories in list order to `cleanup_run`. On success: activity log (Success) `{gbFreed 0.##} GB of files removed`; Success toast title `Cleanup completed`, description `{gbFreed 0.##} GB of temporary files removed.`; if `userMessage` non-empty also a Warning toast title `Some items were skipped`, description = `userMessage`. Then always rescan. (Failure dialog `Cleanup failed` / `Unknown error.` only applies if the call returns an error.)

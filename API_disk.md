# API_disk - drives, folder sizes and battery (`mod_disk.h`)

Commands: `disk_drives`, `disk_scan`, `disk_cancel`, `hw_health`. All are read-only.

| cmd | args | returns |
|---|---|---|
| `disk_drives` | - | Array of `{letter:"C:", path:"C:\\", label, fileSystem, type:"Fixed"\|"Removable"\|"Network", totalBytes, freeBytes}`. CD drives and drives without media are skipped. |
| `disk_scan` | `path` (absolute folder, or `@user` for the profile folder), `opId` | `{path, parent:string\|null, totalBytes, fileCount, unreadable, itemCount, cancelled:false, items:[{name,isDir,link,bytes,files}]}` with `items` sorted largest first and capped at 300 (`itemCount` is the real number). `{cancelled:true}` after `disk_cancel`. Error for a missing folder, a relative path or a file. |
| `disk_cancel` | `opId` | `true` if a scan with that id was running. |
| `hw_health` | - | `{battery:{present:false}}` or `{battery:{present:true, percent:int\|null, onAc, charging, remainingMinutes:int\|null, designMwh, fullMwh, wearPercent, health:"Good"\|"Fair"\|"Worn", cycleCount?, chemistry?}}`. Capacity fields are absent (and `wearPercent` null) when the battery does not report absolute capacity. |

Event: `disk_progress` `{opId, files, bytes, done, total}` about four times a second while a scan runs (`done`/`total` count the top-level folders).

Scan rules: sizes are file sizes (not size on disk). Junctions and symbolic links are listed with `link:true` and counted as 0, so nothing is counted twice and nothing outside the folder is read. OneDrive online-only files (recall attributes) are skipped because they use no space on this drive. Folders Windows refuses to open add to `unreadable`. Paths over 240 characters use the `\\?\` prefix. Up to 8 worker threads measure the sub-folders; the cancel flag is checked at every folder.

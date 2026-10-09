# API_system.md - mod_system.h (WinKit.System port)

All args are strings (`"true"`/`"false"`/`"1"` for bools, decimal for numbers). Every command returns a JSON value.
Argument-level problems (missing/invalid arg) come back as `error:<message>` (the bridge's error channel).
Commands marked **OpResult** return an OperationResult object, ALWAYS (success or failure, never `error:`):

```
{"success":true|false, "message":string|null, "detail":string|null}
```
`message` = C# `UserMessage` (show as the dialog body), `detail` = C# `TechnicalDetail` (show under "details / copy"). On failure the C# UI shows an error dialog `ShowError(<title>, message ?? "Unknown error.", detail)`; titles are given per command below.
All lists are already sorted exactly as the C# service sorted them; the UI may re-sort on column click.

## Processes (ProcessManagerService)
| cmd | args | returns |
|---|---|---|
| `proc_list` | - | array of `{id:int, name:string, filePath:string\|null, workingSetBytes:number, cpuPercent:number(2 dp, 0-100, share of ALL logical cores), threadCount:int, canBeEnded:bool}` sorted by cpuPercent desc, then workingSetBytes desc. **Takes ~0.4 s** (it samples CPU time over 300 ms like the C#), so do not poll faster than ~1 s and skip a tick while one is in flight. `name` has `.exe` stripped; pid 0 is "Idle". `filePath` null when access denied. `canBeEnded` is false for WinKit's own pid and for pids 0 (Idle) and 4 (System). Working set and CPU time come from the system-wide process table (NtQuerySystemInformation), so protected services report real values even without administrator rights. |
| `proc_end` | `pid` | **OpResult**. Dialog title on failure: "Couldn't end process". Messages: "That process has already exited." / "Unable to end that process. It may require administrator access." (detail = Win32 text). Also refuses WinKit's own pid ("Unable to end that process."). |

UI behaviour to replicate:
- Confirm before `proc_end`: title "End process", message `End "<name>" (PID <id>)? Unsaved work in this process will be lost.`, confirm button "End process", destructive style. Disable/hide End when `canBeEnded` is false. After ending, refresh unless frozen.
- **"Freeze" is purely a UI feature**: it stops the 1 s auto-refresh timer (button text toggles "Freeze"/"Unfreeze"); unfreezing restarts the timer and refreshes immediately. It does NOT suspend any process; there is no backend command. The C# also only auto-refreshes (every 1 s) while the System Tools page is visible.
- No "protected process" list exists in the C#; the only safety rule is `canBeEnded` + the confirmation.
- Memory column: bytes formatted with 1024 units "B, KB, MB, GB, TB", format `{size:0.#} {unit}`. CPU column `{0:0.0}%`.

## Services (ServicesManagerService)
| cmd | args | returns |
|---|---|---|
| `svc_list` | - | array of `{name, displayName, status, startupType, canStart:bool, canStop:bool}` sorted by displayName (case-insensitive). Only Win32 services (no kernel drivers). `status` in `Stopped, StartPending, StopPending, Running, ContinuePending, PausePending, Paused`. `startupType` in `Boot, System, Automatic, Manual, Disabled, Unknown` (delayed-auto shows `Automatic`; `Unknown` = config not readable). `canStart` = status is Stopped. `canStop` = Running AND service accepts stop. |
| `svc_start` | `name` (service key name, not display name) | **OpResult** (waits up to 10 s for Running). Failure title "Couldn't start service". |
| `svc_stop` | `name` | **OpResult** (waits up to 10 s for Stopped). Failure title "Couldn't stop service". |

Failure messages: `Unable to start|stop "<name>". This may require administrator access.` (detail = system text) or `Timed out waiting for "<name>" to start|stop.`
UI: `svc_start` has no confirmation; `svc_stop` confirms first: title "Stop service", message `Stop "<displayName>"? Some features of Windows or other apps may stop working until it's started again.`, confirm "Stop service", destructive. Refresh the list after either (success or failure). Show Start button only if `canStart`, Stop only if `canStop`.
Not in the C# (therefore NOT ported): restart, changing startup type.

## Startup (StartupManagerService)
| cmd | args | returns |
|---|---|---|
| `startup_list` | - | array of `{name, command:string, scope, isEnabled:bool, requiresElevationToChange:bool}` sorted by name (case-insensitive). `scope` is one of `CurrentUserRegistry, AllUsersRegistry, CurrentUserFolder, AllUsersFolder`. `requiresElevationToChange` = scope starts with AllUsers. Sources, in order: HKCU Run, HKLM Run, entries previously disabled by WinKit (isEnabled=false, from backup file), per-user Startup folder, common Startup folder (all files except hidden/system files such as desktop.ini; `command` = full file path; `*.disabled` files are isEnabled=false and `name` is the file name without `.disabled` and extension). |
| `startup_set` | `scope`, `name`, `command` (exactly as listed), `enabled` (the NEW state, i.e. `!entry.isEnabled`) | **OpResult**. Failure title "Couldn't change startup item". |

Behaviour (same as C#): disabling a registry entry deletes the value from the Run key and stores `{name,command,machine}` in `%LOCALAPPDATA%\WinKit\disabled-startup.json` (same format as the C# app, so both apps share it); enabling re-creates the value (REG_SZ) and removes it from the backup. Folder entries are renamed by appending / removing `.disabled`. Access denied => message "This startup item requires administrator access to change. Restart WinKit as Administrator." Missing shortcut => "That shortcut could not be found." No confirmation prompt in the C# UI (toggle then refresh). The UI may show an "admin" badge for `requiresElevationToChange`.

## Installed apps (InstalledAppsService)
| cmd | args | returns |
|---|---|---|
| `apps_list` | - | array of `{name, publisher:string\|null, version:string\|null, installDate:"yyyy-MM-dd"\|null, estimatedSizeMb:number\|null, uninstallCommand:string\|null}`; from HKLM Uninstall (64-bit view), HKLM WOW6432Node, HKCU; entries without DisplayName or with SystemComponent=1 skipped; de-duplicated by name (case-insens., first wins); sorted by name. |
| `apps_uninstall` | `name` (as listed) | **OpResult**. Failure title "Couldn't start uninstaller". Looks the app up again by name and launches its UninstallString via ShellExecute (quoted path split exactly like the C#, not through cmd.exe). Messages: "This application does not provide an uninstaller." / `Unable to start the uninstaller for "<name>".` / "That application is no longer installed." |

UI: confirm first - title "Uninstall application", message `This opens the uninstaller for "<name>". WinKit doesn't remove anything itself — you'll finish the uninstall in the app's own wizard.`, confirm "Continue", destructive. On success show a busy indicator on that row for 2 s (C# does `Task.Delay(2s)`), then clear it. There is **no "clean uninstall"/leftover cleanup** in the C# - only the vendor uninstaller is launched. Size display: `estimatedSizeMb >= 1024 ? "{mb/1024:0.#} GB" : "{mb:0} MB"`, empty when null.

## Environment variables (EnvironmentVariablesService)
| cmd | args | returns |
|---|---|---|
| `env_list` | - | array of `{name, value, scope:"User"\|"Machine"}` read from the registry (HKCU\Environment, HKLM\...\Session Manager\Environment; REG_EXPAND_SZ values are returned expanded). Sorted User first then Machine, each by name (case-insens.). |
| `env_set` | `scope` ("User" default \| "Machine"), `name`, `value` | **OpResult**. Failure title "Couldn't set variable". |
| `env_delete` | `scope`, `name` | **OpResult**. Failure title "Couldn't delete variable". |

Rules: blank name => "Variable name cannot be empty." (the C# UI silently does nothing when NewName is blank - check before calling). **An empty `value` deletes the variable** (.NET semantics). Names containing `=` or >= 255 chars are rejected (message "Unable to change the variable.", detail explains). Machine scope without admin => "Changing a system environment variable requires administrator access." Existing REG_EXPAND_SZ variables keep their type on write (small deviation, avoids breaking Path). A `WM_SETTINGCHANGE` "Environment" broadcast is sent after changes. UI: add form (name, value, scope dropdown defaulting to User); after a successful add clear the inputs and refresh. Delete confirms: title "Delete environment variable", message `Delete "<name>"? Applications that rely on it may stop working correctly.`, confirm "Delete", destructive; refresh afterwards.

## Hosts file (HostsFileService)
| cmd | args | returns |
|---|---|---|
| `hosts_get` | - | `{path:string, content:string, exists:bool}`; content is "" when the file is missing. |
| `hosts_entries` | - | array of `{ipAddress, hostName, isEnabled}` (parsed like C# `ParseEntriesAsync`: a leading `#` means disabled and is stripped; lines without a valid IP + host are skipped). The C# UI itself does not use it; the page is a raw text editor. |
| `hosts_save` | `content` (full file text) | **OpResult**. Failure title "Couldn't save hosts file". Writes UTF-8 without BOM, overwriting in place. |

Errors: access denied => "Editing the hosts file requires administrator access. Restart WinKit as Administrator."; other => "Unable to save the hosts file." UI: confirm before save - title "Save hosts file", message "This changes how your computer resolves domain names to IP addresses system-wide. Only continue if you're sure about these changes.", confirm "Save", destructive. On success show a success notification "Hosts file saved". The C# has **no backup feature** (nothing is backed up) so none is provided. Tip: WPF text boxes use CRLF; if the editor yields bare `\n`, Windows still reads it fine.

## System information (SystemInfoService + SystemInformationViewModel)
| cmd | args | returns |
|---|---|---|
| `sysinfo_get` | - | see below. Takes ~0.5-2 s (WMI queries for CPU/GPU). |
| `sysinfo_uptime` | - | `{uptimeMs:number, uptimeText:string}` - cheap; the C# page refreshes the uptime text every 1 s (the UI can also just tick `uptimeMs` locally). |

`sysinfo_get`:
```
{ machineName, architecture:"64-bit"|"32-bit", windowsVersion:"Windows 11 Pro 23H2 (Build 22631)",
  uptimeMs, uptimeText, isAdmin:bool,
  cpu:{name, usagePercent (0-100, WMI LoadPercentage), coreCount (logical)},
  memory:{totalGb, usedGb, usagePercent},
  gpu:{name, dedicatedMemoryGb:number|null},          // first Win32_VideoController; AdapterRAM is 32-bit so capped at 4 GB (same as C#)
  storage:[{name:"C:", totalGb, freeGb, usedGb, usagePercent}],   // fixed, ready drives only
  network:{adapterName ("Not connected" if none), isConnected, ipAddress:string|null } }  // fastest Up non-loopback/non-tunnel adapter, first IPv4
```
Formatting: `uptimeText` = `Nd Hh Mm Ss` (>=1 day) / `Hh Mm Ss` / `Mm Ss`. Memory shown as `{totalGb:0.#} GB`; dashboard percentages `{0:0}%`. Page rows: Machine name, Architecture, Windows version, Processor, Logical cores, Memory, GPU, Network adapter, IP address, System uptime.

## Command list (17)
`proc_list proc_end svc_list svc_start svc_stop startup_list startup_set apps_list apps_uninstall env_list env_set env_delete hosts_get hosts_entries hosts_save sysinfo_get sysinfo_uptime`

## Native notes
Everything is native Win32 (toolhelp, SCM, registry, ShellExecute, GetAdaptersAddresses, GlobalMemoryStatusEx) except CPU/GPU info, which uses in-process WMI COM (no PowerShell). Needs admin for: HKLM Run changes, common Startup folder, Machine env vars, hosts file, stopping/starting most services, ending protected processes.

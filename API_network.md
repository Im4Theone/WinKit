# API_network.md - mod_network.h (port of WinKit.Network)

All args are strings. All commands block until done (worker thread). Failures of the *call itself* (bad args) return `error:<message>`.
JSON key names are camelCase. `opId` = caller-chosen unique string used for events and cancel (may be empty if you never cancel).
Needs `WSAStartup` (done internally), ICMP via iphlpapi (no admin required).

## Commands

| cmd | args | returns |
|---|---|---|
| `ping_run` | `host` (default `8.8.8.8`, trimmed), `count` (default `4`, clamped 1..1000; C# UI always passes 4), `opId` | PingResult (below). Streams `ping_reply`. |
| `ping_cancel` | `opId` | `true` if a running ping with that opId was signalled, else `false`. The running `ping_run` then returns with `cancelled:true`. |
| `trace_run` | `host` (default `8.8.8.8`), `maxHops` (default `30`, clamped 1..255), `opId` | TraceResult. Streams `trace_hop`. |
| `trace_cancel` | `opId` | `true`/`false` as above. |
| `dns_lookup` | `host` (default `example.com`) | OperationResult with `value` (below). |
| `ipcfg_get` | - | JSON array of Adapter, sorted: up adapters first (stable), loopback excluded. |
| `wifi_info` | `rescan` ("true" asks the adapter to scan and waits 2.5 s) | `{available:false, reason}` or `{available:true, adapters:[{description,state,connected,connection:null\|{ssid,bssid,signal(0-100),rssiDbm\|null,phy,channel,band,frequencyMhz,rxMbps,txMbps,security,profile,weakSecurity,othersOnChannel},networks:[{ssid,signal,secure,security,connected,saved,channel?,band?}],profiles:[names]}]}`. Uses wlanapi.dll, loaded at run time. Never returns passwords. |
| `ports_list` | - | JSON array of PortEntry: every TCP connection and UDP socket (IPv4 and IPv6) with the process that owns it. Error if none of the four tables could be read. |
| `netmaint_flushdns` | - | OperationResult. `ipconfig /flushdns`, NOT elevated. |
| `netmaint_dhcprenew` | - | OperationResult. `ipconfig /renew`, UAC "runas" prompt. |
| `netmaint_dhcprelease` | - | OperationResult. `ipconfig /release`, UAC prompt. |
| `netmaint_winsockreset` | - | OperationResult. `netsh winsock reset`, UAC prompt. |
| `netmaint_tcpipreset` | - | OperationResult. `netsh int ip reset`, UAC prompt. |

(13 commands.) `ping_run` also accepts `timeoutMs` (200-4000, default 4000) and `intervalMs` (0-1000, default 1000); the Connection test uses 1500 and 200. Elevated commands show the UAC dialog each time and block until the child exits (no timeout). If UAC is declined the result is a failure with `technicalDetail` "The operation was canceled by the user." Maintenance commands cannot be cancelled (C# passes CancellationToken.None).

## JSON shapes

**PingReply** (one probe): `{"sequence":1,"status":"Success","statusCode":0,"rttMs":23,"address":"8.8.8.8"}`
- `status` = .NET `IPStatus` name: `Success`, `TimedOut`, `TtlExpired`, `DestinationHostUnreachable`, `DestinationNetworkUnreachable`, `Unknown` (name resolution failed / send failed), ... ; `statusCode` = numeric IPStatus (0, 11010, 11013, ..., -1 for Unknown).
- `rttMs` = round trip in ms if `Success`, else `0` (same as C#). `address` = replying address or `null`.

**PingResult**: `{"host","cancelled":bool,"sent","received","lost","minMs","maxMs","avgMs"(2 decimals; the three are null if received==0),"replies":[PingReply...]}` (summary stats are an addition; C# UI only lists replies).
- Probe behaviour (identical to C#): 32-byte payload of 'a', TTL 64, timeout 4000 ms, 1000 ms pause between probes (not after the last), name re-resolved per probe, first A/AAAA address used (IPv4 and IPv6 supported).

**TracerouteHop**: `{"hop":3,"address":"10.0.0.1"|null,"hostName":"router.lan"|null,"rttMs":12|null,"timedOut":false,"reachedDestination":false}`
- Timed-out hop: `address/hostName/rttMs` null, `timedOut:true`. Hop number 0 with `timedOut:true` = host could not be resolved (trace stops; this is a normal result, not an error). Reverse DNS per hop (null if none). One probe per hop, DontFragment, 4000 ms timeout, 32-byte payload, stops when destination reached or after `maxHops`.
- **TraceResult**: `{"host","resolved":bool,"destination":"ip"|null,"reached":bool,"cancelled":bool,"hops":[TracerouteHop...]}`

**OperationResult** (maintenance / dns): `{"success":true|false,"userMessage":string|null,"technicalDetail":string|null}`; dns success adds `"value":{"hostName":"example.com","addresses":["93.184...","2606:..."]}` (no technicalDetail key on success).
- On failure show `userMessage` in an error dialog titled as below, with `technicalDetail` as expandable "details" (nullable).
- DNS failures: `Could not resolve "<host>".` (technicalDetail = Windows resolver message), or `No records found for "<host>".` Only A/AAAA records (IPv4+IPv6) - the C# does not query other record types. `hostName` = canonical name; for an IP-literal input a reverse lookup is performed (PTR) and failure to reverse-resolve is reported as "Could not resolve".

**Adapter** (`ipcfg_get` array element): `{"name","description","macAddress":"AA-BB-CC-DD-EE-FF"|null,"ipv4Addresses":[],"ipv6Addresses":[] (link-local include "%scopeId"),"gateways":[] (v4+v6),"dnsServers":[],"dhcpEnabled":bool (IPv4 DHCP),"isUp":bool}`

**PortEntry** (`ports_list` array element): `{"proto":"TCP"|"UDP","local":"0.0.0.0","localPort":135,"remote":"" (empty for listeners and UDP),"remotePort":0,"state":"Listening"|"Established"|"Time wait"|...,"pid":1816,"process":"svchost" (no .exe; empty when no process owns it, pid 0)}`. UDP sockets are reported as `Listening`. Unsorted; the page sorts and filters.

## Events (via PushEvent; payload is `data`)
| event | payload |
|---|---|
| `ping_reply` | `{"opId":"..","reply":PingReply}` - one per probe, as soon as it completes |
| `trace_hop` | `{"opId":"..","hop":TracerouteHop}` - one per hop (including the hop-0 resolve failure) |

The final command result repeats everything, so a UI can ignore events and just render the result.

## UI behaviours to replicate (from the C# view models)
- Ping / Traceroute pages: default host text `8.8.8.8`; Run does nothing if host is blank/whitespace or already running; host is trimmed; clears the previous results on Run; Stop button visible only while running -> `ping_cancel` / `trace_cancel` (cancelling is silent, keep already-shown rows). Ping always count 4. Table rows appended in arrival order.
- DNS Lookup page: default host `example.com`; ignored if blank or running; clears result + error on start; success shows host name + list of addresses; failure shows `userMessage` as inline error text.
- IP Configuration: Refresh button; shows loading state while `ipcfg_get` runs; list replaced on each refresh.
- Maintenance: every action first shows a confirmation dialog; ignore click if already busy; while running disable all buttons. On success: record activity entry `<successTitle>` (Success) and show a Success toast titled `<successTitle>` (no description). On failure: error dialog titled `"<Title> failed"`, message `userMessage` (fallback "Unknown error."), details `technicalDetail`.

| cmd | Dialog title | Message | Confirm button | Destructive style | successTitle | failure userMessage |
|---|---|---|---|---|---|---|
| netmaint_flushdns | Flush DNS | This clears your computer's cached DNS lookups. | Flush DNS | no | DNS cache flushed | Unable to flush the DNS cache. |
| netmaint_dhcprenew | Renew DHCP lease | This requests a new IP address from your network's DHCP server. | Renew | no | DHCP lease renewed | Unable to renew the DHCP lease. |
| netmaint_dhcprelease | Release DHCP lease | This releases your current IP address. You'll briefly lose network connectivity. | Release | yes | DHCP lease released | Unable to release the DHCP lease. |
| netmaint_winsockreset | Reset Winsock | This resets the Windows network stack. Your computer may need to be restarted. | Reset Winsock | yes | Winsock reset | Unable to reset Winsock. |
| netmaint_tcpipreset | Reset TCP/IP | This resets the TCP/IP stack to its default configuration. Your computer may need to be restarted. | Reset TCP/IP | yes | TCP/IP stack reset | Unable to reset the TCP/IP stack. |

Differences from C#: for elevated commands with a non-zero exit code `technicalDetail` is `"Exit code N."` (C# gave empty stderr); flush-DNS failure detail is the combined console output.

// mod_diag.h - WinKit diagnostics engine (11 checks, 5 fixers, summary store). Header-only.
// Port of WinKit.Diagnostics (C#). See API_diag.md for the command / event contract.
#pragma once
#include "common.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <windns.h>
#include <shellapi.h>
#include <objbase.h>
#include <memory>
#include <cstring>

#ifdef _MSC_VER
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "dnsapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ole32.lib")
#endif

#ifndef GAA_FLAG_INCLUDE_GATEWAYS
#define GAA_FLAG_INCLUDE_GATEWAYS 0x0080
#endif

namespace diag_detail {

// ======================================================================================
// Model
// ======================================================================================
struct FixAct {
    bool has = false;
    ws label, kind, confirm;           // kind: "Fix" | "Review"
    bool destructive = false, elevation = false;
};

struct Res {
    ws id, name, category;             // category = DiagnosticCategory enum name
    ws status = L"Passed";             // Passed | Informational | Warning | Failed | NotApplicable
    ws severity = L"None";             // None | Low | Medium | High
    ws summary, detail, rec;           // detail / rec: empty == null
    FixAct fix;
};

struct Info { const wchar_t *id, *name, *cat; };
inline const Info *Infos() {
    static const Info t[] = {
        { L"windows-integrity",      L"Windows Integrity",     L"WindowsIntegrity" },
        { L"windows-update",         L"Windows Update",        L"WindowsUpdate" },
        { L"drivers",                L"Drivers",               L"Drivers" },
        { L"startup",                L"Startup Programs",      L"Startup" },
        { L"services",               L"Services",              L"Services" },
        { L"disk-space",             L"Disk Space",            L"Storage" },
        { L"temp-files",             L"Temporary Files",       L"Storage" },
        { L"disk-health",            L"Disk Health",           L"Storage" },
        { L"internet-connectivity",  L"Internet Connectivity", L"Network" },
        { L"dns-resolution",         L"DNS Resolution",        L"Network" },
        { L"network-adapter",        L"Network Adapter",       L"Network" },
    };
    return t;
}
static const size_t kCheckCount = 11;

inline Res Mk(const wchar_t *id) {
    Res r;
    for (size_t i = 0; i < kCheckCount; i++)
        if (wcscmp(Infos()[i].id, id) == 0) { r.id = Infos()[i].id; r.name = Infos()[i].name; r.category = Infos()[i].cat; break; }
    return r;
}

inline ws CategoryLabel(const ws &c) {
    if (c == L"WindowsIntegrity") return L"Windows Integrity";
    if (c == L"WindowsUpdate") return L"Windows Update";
    return c;                          // Drivers, Startup, Services, Storage, Network
}
inline ws StatusLabel(const ws &s) {
    if (s == L"Passed") return L"PASSED";
    if (s == L"Warning") return L"WARNING";
    if (s == L"Failed") return L"FAILED";
    if (s == L"Informational") return L"INFO";
    if (s == L"NotApplicable") return L"N/A";
    return L"";
}
inline ws StatusColor(const ws &s) {
    if (s == L"Passed") return L"success";
    if (s == L"Warning") return L"warning";
    if (s == L"Failed") return L"error";
    if (s == L"Informational") return L"accent";
    return L"muted";
}
inline int SortPriority(const ws &s) {
    if (s == L"Failed") return 0;
    if (s == L"Warning") return 1;
    if (s == L"Informational") return 2;
    if (s == L"Passed") return 3;
    if (s == L"NotApplicable") return 4;
    return 5;
}
inline bool IsIssue(const ws &s) { return s == L"Warning" || s == L"Failed"; }

inline ws ReviewTarget(const ws &checkId) {
    if (checkId == L"startup") return L"startup-manager";
    if (checkId == L"services") return L"services-manager";
    if (checkId == L"disk-space") return L"cleanup";
    return L"";
}

inline ws OptStr(const ws &s) { return s.empty() ? ws(L"null") : JStr(s); }

inline ws ResJson(const Res &r) {
    JObj o;
    o.S(L"checkId", r.id).S(L"name", r.name).S(L"category", r.category).S(L"categoryLabel", CategoryLabel(r.category))
     .S(L"status", r.status).S(L"statusLabel", StatusLabel(r.status)).S(L"statusColor", StatusColor(r.status))
     .S(L"severity", r.severity).N(L"sortPriority", SortPriority(r.status)).B(L"isIssue", IsIssue(r.status))
     .S(L"summary", r.summary).R(L"technicalDetail", OptStr(r.detail)).R(L"recommendedAction", OptStr(r.rec));
    if (r.fix.has) {
        ws rt = r.fix.kind == L"Review" ? ReviewTarget(r.id) : ws();
        JObj f;
        f.S(L"label", r.fix.label).S(L"kind", r.fix.kind).S(L"confirmationMessage", r.fix.confirm)
         .B(L"isDestructive", r.fix.destructive).B(L"requiresElevation", r.fix.elevation).R(L"reviewTarget", OptStr(rt));
        o.R(L"fixAction", f.Str());
    } else o.Null(L"fixAction");
    return o.Str();
}

// ======================================================================================
// Small helpers
// ======================================================================================
inline ws FmtTrim(double v, int digits) {               // .NET "0.#" / "0.##" with invariant '.'
    wchar_t b[64]; swprintf(b, 64, L"%.*f", digits, v);
    ws s = b;
    if (s.find(L'.') != ws::npos) { while (!s.empty() && s.back() == L'0') s.pop_back(); if (!s.empty() && s.back() == L'.') s.pop_back(); }
    if (s == L"-0" || s.empty()) s = L"0";
    return s;
}
inline ws FormatSysMsg(DWORD code) {
    wchar_t b[512] = {};
    DWORD n = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code, 0, b, 511, nullptr);
    ws s = n ? Trim(b) : ws();
    if (s.empty()) s = L"Win32 error " + Num(code) + L".";
    return s;
}
inline ws Join(const std::vector<ws> &v, const ws &sep) {
    ws o; for (size_t i = 0; i < v.size(); i++) { if (i) o += sep; o += v[i]; } return o;
}
static const wchar_t *kNL = L"\r\n";

struct ComInit {
    bool ok = false;
    ComInit() { HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE); ok = (hr == S_OK || hr == S_FALSE); }
    ~ComInit() { if (ok) CoUninitialize(); }
};
struct CsLock {
    CRITICAL_SECTION *c; explicit CsLock(CRITICAL_SECTION *cs) : c(cs) { EnterCriticalSection(c); } ~CsLock() { LeaveCriticalSection(c); }
};

// ---- EnglishOutputGuard -------------------------------------------------------------
inline bool EnglishExpected() { return PRIMARYLANGID(GetSystemDefaultUILanguage()) == LANG_ENGLISH; }
inline ws UiCultureName() {
    wchar_t b[96] = {};
    if (LCIDToLocaleName(MAKELCID(GetSystemDefaultUILanguage(), SORT_DEFAULT), b, 85, 0) > 0) return b;
    return L"unknown";
}

// ---- services (SCM) -----------------------------------------------------------------
struct SvcInfo { bool ok = false; DWORD state = 0; bool hasStart = false; DWORD startType = 0; };
inline SvcInfo QuerySvc(const wchar_t *name) {
    SvcInfo r;
    SC_HANDLE m = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!m) return r;
    SC_HANDLE s = OpenServiceW(m, name, SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG);
    if (!s) s = OpenServiceW(m, name, SERVICE_QUERY_STATUS);
    if (s) {
        SERVICE_STATUS st = {};
        if (QueryServiceStatus(s, &st)) { r.ok = true; r.state = st.dwCurrentState; }
        DWORD need = 0;
        QueryServiceConfigW(s, nullptr, 0, &need);
        if (need >= sizeof(QUERY_SERVICE_CONFIGW) && need < 65536) {
            std::vector<char> buf(need + 16);
            if (QueryServiceConfigW(s, (LPQUERY_SERVICE_CONFIGW)buf.data(), (DWORD)buf.size(), &need)) {
                r.hasStart = true; r.startType = ((LPQUERY_SERVICE_CONFIGW)buf.data())->dwStartType;
            }
        }
        CloseServiceHandle(s);
    }
    CloseServiceHandle(m);
    return r;
}
inline ws StateName(DWORD s) {
    switch (s) {
    case SERVICE_STOPPED: return L"Stopped"; case SERVICE_START_PENDING: return L"StartPending";
    case SERVICE_STOP_PENDING: return L"StopPending"; case SERVICE_RUNNING: return L"Running";
    case SERVICE_CONTINUE_PENDING: return L"ContinuePending"; case SERVICE_PAUSE_PENDING: return L"PausePending";
    case SERVICE_PAUSED: return L"Paused"; default: return L"Unknown";
    }
}
inline ws StartName(DWORD t) {
    switch (t) { case 0: return L"Boot"; case 1: return L"System"; case 2: return L"Automatic"; case 3: return L"Manual"; case 4: return L"Disabled"; default: return L"Unknown"; }
}

// ---- registry -----------------------------------------------------------------------
inline bool KeyExists(HKEY root, const ws &sub) {
    HKEY k; if (RegOpenKeyExW(root, sub.c_str(), 0, KEY_READ | KEY_WOW64_64KEY, &k) != ERROR_SUCCESS) return false;
    RegCloseKey(k); return true;
}
inline bool ValueExists(HKEY root, const ws &sub, const ws &name) {
    HKEY k; if (RegOpenKeyExW(root, sub.c_str(), 0, KEY_READ | KEY_WOW64_64KEY, &k) != ERROR_SUCCESS) return false;
    DWORD type = 0, sz = 0; LONG r = RegQueryValueExW(k, name.c_str(), nullptr, &type, nullptr, &sz);
    RegCloseKey(k); return r == ERROR_SUCCESS || r == ERROR_MORE_DATA;
}
// names of string values (REG_SZ / REG_EXPAND_SZ) under a key
inline std::vector<ws> RegStringValueNames(HKEY root, const ws &sub) {
    std::vector<ws> v; HKEY k;
    if (RegOpenKeyExW(root, sub.c_str(), 0, KEY_READ, &k) != ERROR_SUCCESS) return v;
    std::vector<wchar_t> n(16384);
    for (DWORD i = 0;; i++) {
        DWORD nl = (DWORD)n.size(), type = 0;
        LONG r = RegEnumValueW(k, i, n.data(), &nl, nullptr, &type, nullptr, nullptr);
        if (r == ERROR_NO_MORE_ITEMS) break;
        if (r != ERROR_SUCCESS) continue;
        if (type == REG_SZ || type == REG_EXPAND_SZ) v.push_back(ws(n.data(), nl));
    }
    RegCloseKey(k); return v;
}

// ---- file system --------------------------------------------------------------------
// Paths of 240+ characters need the \\?\ prefix for the Win32 file APIs (temp folders can nest deeper than MAX_PATH).
inline ws Lp(const ws &p) {
    if (p.size() < 240 || p.compare(0, 4, L"\\\\?\\") == 0) return p;
    if (p.compare(0, 2, L"\\\\") == 0) return L"\\\\?\\UNC\\" + p.substr(2);
    return L"\\\\?\\" + p;
}
inline long long ScanDirBytes(const ws &root0, volatile LONG *abort) {
    long long total = 0;
    ws root = root0; while (root.size() > 3 && root.back() == L'\\') root.pop_back();
    if (!DirExists(root)) return 0;
    std::vector<ws> stack; stack.push_back(root);
    while (!stack.empty()) {
        if (abort && *abort) return total;
        ws dir = stack.back(); stack.pop_back();
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileExW(Lp(dir + L"\\*").c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
        if (h == INVALID_HANDLE_VALUE) continue;
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
                stack.push_back(dir + L"\\" + fd.cFileName);
            } else total += ((long long)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    return total;
}
// A temp folder we are willing to scan and empty. GetTempPath falls back to the profile folder when %TEMP% is missing,
// and a drive root or well-known system folder must never be treated as "temporary files".
inline ws NormDirLower(ws p) { while (p.size() > 3 && (p.back() == L'\\' || p.back() == L'/')) p.pop_back(); return Lower(p); }
inline bool SafeTempRoot(const ws &root) {
    ws n = NormDirLower(root);
    if (n.size() <= 3) return false;
    if (!(n[1] == L':' || StartsWith(n, L"\\\\"))) return false;
    static const wchar_t *const bad[] = { L"USERPROFILE", L"ProgramFiles", L"ProgramFiles(x86)", L"ProgramData", L"SystemRoot", L"windir", L"SystemDrive", L"HOMEDRIVE" };
    for (const wchar_t *b : bad) { ws v = EnvVar(b); if (!v.empty() && NormDirLower(v) == n) return false; }
    wchar_t sys[MAX_PATH] = {}; if (GetSystemDirectoryW(sys, MAX_PATH) && NormDirLower(sys) == n) return false;
    return true;
}
inline ws RawTempDir() { wchar_t b[MAX_PATH + 2] = {}; DWORD n = GetTempPathW(MAX_PATH + 1, b); return n ? ws(b, n) : ws(); }
inline ws UserTempDir() { ws p = RawTempDir(); return SafeTempRoot(p) ? p : ws(); }      // "" = nothing safe to scan or delete
inline ws WindowsTempDir() { ws r = EnvVar(L"SystemRoot"); if (r.empty()) r = L"C:\\Windows"; return r + L"\\Temp"; }

// ---- IP address formatting (no winsock init needed) -----------------------------------
inline ws FmtSockAddr(const SOCKET_ADDRESS &sa) {
    if (!sa.lpSockaddr) return L"";
    if (sa.lpSockaddr->sa_family == AF_INET && sa.iSockaddrLength >= (int)sizeof(sockaddr_in)) {
        const unsigned char *p = (const unsigned char *)&((const sockaddr_in *)sa.lpSockaddr)->sin_addr;
        wchar_t b[32]; swprintf(b, 32, L"%u.%u.%u.%u", p[0], p[1], p[2], p[3]); return b;
    }
    if (sa.lpSockaddr->sa_family == AF_INET6 && sa.iSockaddrLength >= (int)sizeof(sockaddr_in6)) {
        const sockaddr_in6 *s6 = (const sockaddr_in6 *)sa.lpSockaddr;
        const unsigned char *p = (const unsigned char *)&s6->sin6_addr;
        unsigned w[8]; for (int i = 0; i < 8; i++) w[i] = (p[2 * i] << 8) | p[2 * i + 1];
        int bs = -1, bl = 0;
        for (int i = 0; i < 8;) { if (w[i]) { i++; continue; } int j = i; while (j < 8 && !w[j]) j++; if (j - i > bl) { bl = j - i; bs = i; } i = j; }
        if (bl < 2) bs = -1;
        ws o;
        for (int i = 0; i < 8; i++) {
            if (i == bs) { o += L"::"; i += bl - 1; continue; }
            if (!o.empty() && o.back() != L':') o += L":";
            wchar_t b[8]; swprintf(b, 8, L"%x", w[i]); o += b;
        }
        if (s6->sin6_scope_id) o += L"%" + Num(s6->sin6_scope_id);
        return o;
    }
    return L"";
}

// ======================================================================================
// Checks.  Each runs on its own worker thread; *abort becomes non-zero on timeout / cancel.
// ======================================================================================
typedef Res (*CheckFn)(volatile LONG *abort);

// DISM only runs elevated. When the user lets the Repair/Check action run it as administrator, the outcome is remembered for
// the rest of this session so the next (non-elevated) scan can still show it instead of "needs administrator access".
struct ElevIntegrity { CRITICAL_SECTION cs; bool has = false, clean = false; ws output; ElevIntegrity() { InitializeCriticalSection(&cs); } };
inline ElevIntegrity &ElevIntegrityState() { static ElevIntegrity s; return s; }
inline void RememberIntegrity(bool clean, const ws &output) {
    ElevIntegrity &e = ElevIntegrityState(); CsLock l(&e.cs); e.has = true; e.clean = clean; e.output = output;
}
inline void SetRepairFix(Res &r) {
    r.fix.has = true; r.fix.label = L"Repair"; r.fix.kind = L"Fix"; r.fix.elevation = true;
    r.fix.confirm = L"This runs DISM (RestoreHealth) and the System File Checker to repair Windows system files. "
                    L"It can take 10-20 minutes, needs an internet connection, and requires administrator permission. "
                    L"Don't turn off your PC while it runs.";
}

inline Res Check_WindowsIntegrity(volatile LONG *) {
    Res r = Mk(L"windows-integrity");
    if (!EnglishExpected()) {
        r.status = L"NotApplicable";
        r.summary = L"This check needs an English-language Windows installation to reliably read DISM's output.";
        r.detail = L"Installed UI culture: " + UiCultureName() + L".";
        return r;
    }
    DWORD ec = (DWORD)-1;
    ws output = RunCapture(L"dism.exe /Online /Cleanup-Image /CheckHealth", &ec, 18000);
    auto inconclusive = [&](const ws &d) {
        r.status = L"Informational"; r.summary = L"Could not determine Windows component store health."; r.detail = d; return r;
    };
    if (ec == (DWORD)-1 && output.empty()) return inconclusive(L"dism.exe could not be started.");
    if (ec == 0 && ContainsI(output, L"No component store corruption detected")) {
        r.status = L"Passed"; r.summary = L"No known corruption in the Windows component store."; return r;
    }
    // DISM refuses to run without administrator rights ("Error: 740"). That says nothing about the health of the system,
    // so it must not be reported as corruption.
    if (ec != 0 && (ContainsI(output, L"Error: 740") || ContainsI(output, L"elevated permissions"))) {
        bool has = false, clean = false; ws prev;
        { ElevIntegrity &e = ElevIntegrityState(); CsLock l(&e.cs); has = e.has; clean = e.clean; prev = e.output; }
        if (has && clean) {
            r.status = L"Passed"; r.summary = L"No known corruption in the Windows component store.";
            r.detail = L"Verified with administrator rights earlier in this session.";
            return r;
        }
        if (has) {
            r.status = L"Failed"; r.severity = L"High";
            r.summary = L"The Windows component store is flagged as corrupted.";
            r.detail = prev; r.rec = L"Run DISM RestoreHealth and System File Checker to repair Windows system files.";
            SetRepairFix(r); return r;
        }
        r.status = L"Informational"; r.severity = L"Low";
        r.summary = L"Checking the Windows component store requires administrator access.";
        r.detail = Trim(output);
        r.rec = L"Run the check as administrator, or start WinKit as Administrator (Settings > Advanced).";
        r.fix.has = true; r.fix.label = L"Check as Administrator"; r.fix.kind = L"Fix"; r.fix.elevation = true;
        r.fix.confirm = L"This runs DISM's quick component store check (CheckHealth) with administrator rights. It only reads the "
                        L"store's status and usually takes under a minute. If it finds corruption, a repair is offered afterwards.";
        return r;
    }
    if (ec != 0 && Trim(output).empty())
        return inconclusive(L"dism.exe exited with code " + SNum((int)ec) + L" and produced no output.");
    r.status = L"Failed"; r.severity = L"High";
    r.summary = L"The Windows component store is flagged as corrupted.";
    r.detail = output;
    r.rec = L"Run DISM RestoreHealth and System File Checker to repair Windows system files.";
    SetRepairFix(r);
    return r;
}

inline Res Check_WindowsUpdate(volatile LONG *) {
    Res r = Mk(L"windows-update");
    SvcInfo s = QuerySvc(L"wuauserv");
    bool disabled = false; ws sdetail;
    if (s.ok) {
        sdetail = L"wuauserv: start type = " + (s.hasStart ? StartName(s.startType) : ws(L"Unknown")) + L", status = " + StateName(s.state) + L".";
        disabled = s.hasStart && s.startType == SERVICE_DISABLED;
    } else sdetail = L"Could not query the wuauserv service.";
    bool reboot = KeyExists(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Component Based Servicing\\RebootPending")
               || KeyExists(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\WindowsUpdate\\Auto Update\\RebootRequired")
               || ValueExists(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Session Manager", L"PendingFileRenameOperations");
    if (disabled) {
        r.status = L"Failed"; r.severity = L"High"; r.summary = L"The Windows Update service is disabled.";
        r.detail = sdetail; r.rec = L"Enable and start the Windows Update service.";
        r.fix.has = true; r.fix.label = L"Enable"; r.fix.kind = L"Fix"; r.fix.elevation = true;
        r.fix.confirm = L"This sets the Windows Update service back to its default startup type and starts it. It requires administrator permission.";
        return r;
    }
    if (reboot) {
        r.status = L"Informational"; r.severity = L"Low"; r.summary = L"A restart is required to finish applying recent changes.";
        r.detail = L"A pending-reboot flag is set. This is most often left by Windows Update, but can also be left by driver or application installers.";
        r.rec = L"Restart your PC when convenient.";
        return r;
    }
    r.status = L"Passed"; r.summary = L"Windows Update is configured normally and no restart is pending."; return r;
}

inline const wchar_t *KnownDriverCode(int c) {
    switch (c) {
    case 1: return L"device is not configured correctly";
    case 3: return L"driver may be corrupted, or the system is low on resources";
    case 10: return L"device cannot start";
    case 18: return L"drivers need to be reinstalled";
    case 22: return L"device is disabled";
    case 28: return L"drivers are not installed";
    case 31: return L"device is not working properly";
    case 37: return L"driver returned a failure";
    case 39: return L"driver may be corrupted or missing";
    case 43: return L"device reported a failure and was stopped by Windows";
    case 45: return L"device is not currently connected";
    default: return nullptr;
    }
}

inline Res Check_Drivers(volatile LONG *) {
    Res r = Mk(L"drivers");
    static const wchar_t *script =
        LR"PS(try{ Get-CimInstance -ClassName Win32_PnPEntity -Filter 'ConfigManagerErrorCode <> 0' -ErrorAction Stop | ForEach-Object { $n = if($null -eq $_.Name){'Unknown device'}else{[string]$_.Name}; 'DEV:' + [int]$_.ConfigManagerErrorCode + '|' + $n }; 'DONE' }catch{ 'ERR:' + $_.Exception.Message })PS";
    DWORD ec = 0; ws out = RunPowerShell(script, &ec, 18000);
    std::vector<ws> benign, problems; ws err; bool done = false;
    for (const ws &l : Lines(out)) {
        ws t = Trim(l);
        if (StartsWith(t, L"DEV:")) {
            size_t bar = t.find(L'|'); if (bar == ws::npos) continue;
            int code = _wtoi(t.substr(4, bar - 4).c_str()); ws name = t.substr(bar + 1);
            const wchar_t *kd = KnownDriverCode(code);
            ws desc = kd ? ws(kd) : L"reporting configuration error code " + SNum(code);
            ws line = name + L" - " + desc + L" (code " + SNum(code) + L")";
            if (code == 22 || code == 45) benign.push_back(line); else problems.push_back(line);
        } else if (StartsWith(t, L"ERR:")) err = Trim(t.substr(4));
        else if (t == L"DONE") done = true;
    }
    if (!done && err.empty()) err = L"The WMI query did not complete (PowerShell exit code " + SNum((int)ec) + L").";
    if (!err.empty()) {
        r.status = L"Informational"; r.summary = L"Could not query device status via WMI."; r.detail = err; return r;
    }
    if (!problems.empty()) {
        r.status = L"Warning"; r.severity = L"Medium";
        r.summary = problems.size() == 1 ? ws(L"1 device is reporting a configuration problem.")
                                         : SNum((long long)problems.size()) + L" devices are reporting configuration problems.";
        ws d; for (auto &l : problems) d += l + kNL;
        if (!benign.empty()) d += L"Also disabled or not connected: " + Pl((long long)benign.size(), L"device", L"devices") + L"." + kNL;
        r.detail = Trim(d);
        r.rec = L"Open Device Manager to review the affected devices and update or reinstall their drivers.";
        r.fix.has = true; r.fix.label = L"Open Device Manager"; r.fix.kind = L"Fix";
        r.fix.confirm = L"This opens the built-in Windows Device Manager so you can review the affected devices.";
        return r;
    }
    if (!benign.empty()) {
        r.status = L"Informational";
        r.summary = benign.size() == 1 ? ws(L"1 device is disabled or not currently connected.")
                                       : SNum((long long)benign.size()) + L" devices are disabled or not currently connected.";
        r.detail = Join(benign, kNL); return r;
    }
    r.status = L"Passed"; r.summary = L"All devices are working correctly."; return r;
}

inline bool OrdLess(const ws &a, const ws &b) { return CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_LESS_THAN; }
inline ws FileNoExt(const ws &f) { size_t d = f.find_last_of(L'.'); return d == ws::npos || d == 0 ? f : f.substr(0, d); }

inline void AddFolderStartup(std::vector<ws> &names, const ws &dir) {
    if (dir.empty() || !DirExists(dir)) return;
    WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if ((fd.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) || _wcsicmp(fd.cFileName, L"desktop.ini") == 0) continue;   // folder metadata, not a program
        ws f = fd.cFileName;
        if (f.size() >= 9 && _wcsicmp(f.c_str() + f.size() - 9, L".disabled") == 0) continue;   // disabled entry
        names.push_back(FileNoExt(f));
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

inline Res Check_Startup(volatile LONG *) {
    Res r = Mk(L"startup");
    const ws run = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    std::vector<ws> en;
    for (auto &n : RegStringValueNames(HKEY_CURRENT_USER, run)) en.push_back(n);
    for (auto &n : RegStringValueNames(HKEY_LOCAL_MACHINE, run)) en.push_back(n);
    ws appdata = EnvVar(L"APPDATA"), pd = EnvVar(L"ProgramData");
    if (!appdata.empty()) AddFolderStartup(en, appdata + L"\\Microsoft\\Windows\\Start Menu\\Programs\\Startup");
    if (!pd.empty()) AddFolderStartup(en, pd + L"\\Microsoft\\Windows\\Start Menu\\Programs\\StartUp");
    std::stable_sort(en.begin(), en.end(), OrdLess);
    const size_t kWarn = 8;
    if (en.size() > kWarn) {
        r.status = L"Warning"; r.severity = L"Low";
        r.summary = SNum((long long)en.size()) + L" programs are set to launch at startup.";
        r.detail = Join(en, kNL);
        r.rec = L"Review your startup programs and disable the ones you don't need running all the time.";
        r.fix.has = true; r.fix.label = L"Review Startup Items"; r.fix.kind = L"Review";
        r.fix.confirm = L"This opens Startup Manager, where you can disable individual entries.";
        return r;
    }
    r.status = L"Passed";
    r.summary = en.empty() ? ws(L"No programs launch at startup.") : en.size() == 1 ? ws(L"1 program launches at startup - a normal amount.")
                                                                    : SNum((long long)en.size()) + L" programs launch at startup - a normal amount.";
    return r;
}

// number of products in root\SecurityCenter2 for a class; -1 if unavailable (treated as "covered")
inline int SecurityCenterCount(const wchar_t *cls) {
    ws script = ws(L"try{ $c=@(Get-CimInstance -Namespace root\\SecurityCenter2 -ClassName ") + cls + L" -ErrorAction Stop).Count; 'COUNT:' + $c }catch{ 'ERR:' + $_.Exception.Message }";
    ws out = RunPowerShell(script, nullptr, 12000);
    for (const ws &l : Lines(out)) { ws t = Trim(l); if (StartsWith(t, L"COUNT:")) return _wtoi(t.c_str() + 6); }
    return -1;
}

inline Res Check_Services(volatile LONG *) {
    Res r = Mk(L"services");
    struct Base { const wchar_t *svc, *disp, *impact; };
    static const Base baseline[] = {
        { L"BFE", L"Base Filtering Engine", L"Windows Firewall and other network filtering will stop working." },
        { L"Dnscache", L"DNS Client", L"Name resolution may become slow or unreliable for most apps." },
        { L"Dhcp", L"DHCP Client", L"Automatic IP address configuration will not work." },
    };
    std::vector<ws> problems; bool critical = false;
    for (const Base &b : baseline) {
        SvcInfo s = QuerySvc(b.svc);
        if (!s.ok || s.state == SERVICE_RUNNING) continue;
        problems.push_back(ws(b.disp) + L" (" + b.svc + L") is " + StateName(s.state) + L". " + b.impact);
        critical = true;
    }
    SvcInfo mps = QuerySvc(L"MpsSvc");
    bool fwCovered = (mps.ok && mps.state == SERVICE_RUNNING) || SecurityCenterCount(L"FirewallProduct") != 0;
    if (!fwCovered) { problems.push_back(L"Windows Firewall is stopped and no third-party firewall product was detected."); critical = true; }
    SvcInfo wd = QuerySvc(L"WinDefend");
    bool avCovered = (wd.ok && wd.state == SERVICE_RUNNING) || SecurityCenterCount(L"AntiVirusProduct") != 0;
    if (!avCovered) problems.push_back(L"Windows Defender is stopped and no third-party antivirus product was detected.");

    if (problems.empty()) { r.status = L"Passed"; r.summary = L"Core Windows services are running normally."; return r; }
    r.status = critical ? L"Failed" : L"Warning"; r.severity = critical ? L"High" : L"Medium";
    r.summary = problems.size() == 1 ? ws(L"1 important service needs attention.")
                                     : SNum((long long)problems.size()) + L" important services need attention.";
    r.detail = Join(problems, kNL);
    r.rec = L"Open Services in System Tools to review and start the affected services.";
    r.fix.has = true; r.fix.label = L"Review Services"; r.fix.kind = L"Review";
    r.fix.confirm = L"This opens the Services tool, where you can start the affected services yourself.";
    return r;
}

inline Res Check_DiskSpace(volatile LONG *) {
    Res r = Mk(L"disk-space");
    ws sys = SysDir();
    ws root = (sys.size() >= 3 && sys[1] == L':') ? sys.substr(0, 2) + L"\\" : ws(L"C:\\");
    ULARGE_INTEGER avail = {}, total = {}, freeb = {};
    if (!GetDiskFreeSpaceExW(root.c_str(), &avail, &total, &freeb)) {
        r.status = L"Informational"; r.summary = L"Could not read free space for drive " + root + L"."; return r;
    }
    double freePct = total.QuadPart == 0 ? 100.0 : (double)avail.QuadPart * 100.0 / (double)total.QuadPart;
    double freeGb = (double)avail.QuadPart / 1024.0 / 1024.0 / 1024.0;
    double totalGb = (double)total.QuadPart / 1024.0 / 1024.0 / 1024.0;
    ws detail = root + L" - " + FmtTrim(freeGb, 1) + L" GB free of " + FmtTrim(totalGb, 1) + L" GB (" + FmtTrim(freePct, 1) + L"% free).";
    auto review = [&]() {
        r.fix.has = true; r.fix.label = L"Open Cleanup"; r.fix.kind = L"Review";
        r.fix.confirm = L"This opens the Cleanup tool so you can review and remove reclaimable files.";
    };
    if (freeGb < 10.0) {
        r.status = L"Failed"; r.severity = L"High";
        r.summary = L"The system drive (" + root + L") is critically low on space."; r.detail = detail;
        r.rec = L"Free up disk space before it affects Windows Update, app installs, or general stability.";
        review(); return r;
    }
    if (freeGb < 25.0) {
        r.status = L"Warning"; r.severity = L"Medium";
        r.summary = L"The system drive (" + root + L") is running low on space."; r.detail = detail;
        r.rec = L"Consider freeing up disk space soon."; review(); return r;
    }
    r.status = L"Passed"; r.summary = L"The system drive has " + FmtTrim(freeGb, 1) + L" GB free (" + FmtTrim(freePct, 1) + L"%)."; r.detail = detail;
    return r;
}

inline Res Check_TempFiles(volatile LONG *abort) {
    Res r = Mk(L"temp-files");
    long long bytes = ScanDirBytes(UserTempDir(), abort) + ScanDirBytes(WindowsTempDir(), abort);
    double gb = (double)bytes / 1024.0 / 1024.0 / 1024.0;
    if (bytes > 2LL * 1024 * 1024 * 1024) {
        r.status = L"Warning"; r.severity = L"Low";
        r.summary = FmtTrim(gb, 1) + L" GB of temporary files have accumulated.";
        r.rec = L"Clean up temporary files to reclaim disk space.";
        r.fix.has = true; r.fix.label = L"Clean Up"; r.fix.kind = L"Fix";
        r.fix.confirm = L"This removes " + FmtTrim(gb, 1) + L" GB of user and Windows temporary files. This can't be undone.";
        return r;
    }
    r.status = L"Passed"; r.summary = L"Temporary files are at a normal level (" + FmtTrim(gb, 1) + L" GB)."; return r;
}

inline Res Check_DiskHealth(volatile LONG *) {
    Res r = Mk(L"disk-health");
    auto na = [&](const ws &d) {
        r.status = L"NotApplicable"; r.summary = L"SMART failure prediction isn't available for this drive/controller."; r.detail = d; return r;
    };
    static const wchar_t *script =
        LR"PS(try{ $r=@(Get-CimInstance -Namespace root\wmi -ClassName MSStorageDriver_FailurePredictStatus -ErrorAction Stop); 'COUNT:' + $r.Count; foreach($d in $r){ $n = if($null -eq $d.InstanceName){'Unknown drive'}else{[string]$d.InstanceName}; 'DRV:' + [string]$d.PredictFailure + '|' + $n } }catch{ 'ERR:' + $_.Exception.Message })PS";
    DWORD ec = 0; ws out = RunPowerShell(script, &ec, 18000);
    int count = -1; ws err; std::vector<ws> failing;
    for (const ws &l : Lines(out)) {
        ws t = Trim(l);
        if (StartsWith(t, L"COUNT:")) count = _wtoi(t.c_str() + 6);
        else if (StartsWith(t, L"ERR:")) err = Trim(t.substr(4));
        else if (StartsWith(t, L"DRV:")) {
            size_t bar = t.find(L'|'); if (bar == ws::npos) continue;
            if (Lower(t.substr(4, bar - 4)) == L"true") failing.push_back(t.substr(bar + 1));
        }
    }
    if (!err.empty()) {
        if (ContainsI(err, L"access") && ContainsI(err, L"denied")) return na(L"SMART failure prediction isn't accessible on this system: " + err);
        return na(L"SMART failure prediction isn't available on this system: " + err);
    }
    if (count < 0) return na(L"SMART failure prediction isn't available on this system: the WMI query did not complete (PowerShell exit code " + SNum((int)ec) + L").");
    if (count == 0) return na(L"No drives reported SMART failure-prediction status.");
    if (!failing.empty()) {
        r.status = L"Failed"; r.severity = L"High";
        r.summary = L"A drive is reporting a SMART failure-prediction warning."; r.detail = Join(failing, kNL);
        r.rec = L"Back up your data immediately and plan to replace this drive. WinKit cannot repair failing hardware automatically.";
        return r;
    }
    r.status = L"Passed"; r.summary = L"No SMART failure prediction detected."; return r;
}

// .NET System.Net.NetworkInformation.IPStatus names
inline ws IpStatusName(DWORD c) {
    switch (c) {
    case 0: return L"Success"; case 11001: return L"BufferTooSmall"; case 11002: return L"DestinationNetworkUnreachable";
    case 11003: return L"DestinationHostUnreachable"; case 11004: return L"DestinationProtocolUnreachable";
    case 11005: return L"DestinationPortUnreachable"; case 11006: return L"NoResources"; case 11007: return L"BadOption";
    case 11008: return L"HardwareError"; case 11009: return L"PacketTooBig"; case 11010: return L"TimedOut";
    case 11012: return L"BadRoute"; case 11013: return L"TtlExpired"; case 11014: return L"TtlReassemblyTimeExceeded";
    case 11015: return L"ParameterProblem"; case 11016: return L"SourceQuench"; case 11018: return L"BadDestination";
    case 11040: return L"DestinationUnreachable"; case 11041: return L"TimeExceeded"; case 11042: return L"BadHeader";
    case 11043: return L"UnrecognizedNextHeader"; case 11044: return L"IcmpError"; case 11045: return L"DestinationScopeMismatch";
    default: return L"Unknown";
    }
}

// Non-blocking TCP connect to an IPv4 address (network byte order as used by IcmpSendEcho). True if it connected in time.
inline bool TcpReach(unsigned long addrNbo, unsigned short port, int timeoutMs, unsigned long &elapsedMs) {
    static const bool wsa = []() { WSADATA d; return WSAStartup(MAKEWORD(2, 2), &d) == 0; }();
    if (!wsa) return false;
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return false;
    u_long nb = 1; ioctlsocket(s, FIONBIO, &nb);
    sockaddr_in sa; memset(&sa, 0, sizeof(sa)); sa.sin_family = AF_INET; sa.sin_port = htons(port); sa.sin_addr.s_addr = addrNbo;
    ULONGLONG t0 = GetTickCount64(); bool ok = false;
    if (connect(s, (sockaddr *)&sa, sizeof(sa)) == 0) ok = true;
    else if (WSAGetLastError() == WSAEWOULDBLOCK) {
        fd_set wr, ex; FD_ZERO(&wr); FD_ZERO(&ex); FD_SET(s, &wr); FD_SET(s, &ex);
        timeval tv; tv.tv_sec = timeoutMs / 1000; tv.tv_usec = (timeoutMs % 1000) * 1000;
        if (select(0, nullptr, &wr, &ex, &tv) > 0 && FD_ISSET(s, &wr)) {
            int err = 0, len = sizeof(err);
            ok = getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&err, &len) == 0 && err == 0;
        }
    }
    elapsedMs = (unsigned long)(GetTickCount64() - t0);
    closesocket(s);
    return ok;
}

inline Res Check_Internet(volatile LONG *) {
    Res r = Mk(L"internet-connectivity");
    const ws target = L"1.1.1.1";
    ws status = L"Unknown"; unsigned long rtt = 0; bool ok = false;
    HANDLE h = IcmpCreateFile();
    if (h != INVALID_HANDLE_VALUE) {
        char payload[32]; memset(payload, 'a', sizeof(payload));
        IP_OPTION_INFORMATION opt; memset(&opt, 0, sizeof(opt)); opt.Ttl = 64;
        std::vector<char> rep(sizeof(ICMP_ECHO_REPLY) + sizeof(payload) + 8 + 128);
        DWORD n = IcmpSendEcho(h, 0x01010101u, payload, (WORD)sizeof(payload), &opt, rep.data(), (DWORD)rep.size(), 4000);
        if (n > 0) {
            const ICMP_ECHO_REPLY *er = (const ICMP_ECHO_REPLY *)rep.data();
            status = IpStatusName((DWORD)er->Status);
            if (er->Status == 0) { ok = true; rtt = (unsigned long)er->RoundTripTime; }
        } else status = IpStatusName(GetLastError());
        IcmpCloseHandle(h);
    }
    if (ok) { r.status = L"Passed"; r.summary = L"Reached " + target + L" in " + Num(rtt) + L" ms."; return r; }
    // Many networks drop ICMP echo while the internet works fine, so before reporting a failure try a real TCP connection.
    unsigned long tcpMs = 0;
    if (TcpReach(0x01010101u, 443, 3000, tcpMs)) {
        r.status = L"Passed"; r.summary = L"Reached " + target + L" in " + Num(tcpMs) + L" ms.";
        r.detail = L"Ping to " + target + L" returned " + status + L", but a TCP connection to port 443 succeeded (ping is probably blocked on this network).";
        return r;
    }
    r.status = L"Failed"; r.severity = L"Medium"; r.summary = L"Your computer could not reach the internet.";
    r.detail = L"Ping to " + target + L" returned " + status + L", and a TCP connection to port 443 failed.";
    r.rec = L"Check your Wi-Fi or ethernet connection, or contact your network administrator.";
    return r;
}

inline ws DnsErrText(DWORD st) {
    if (st == 9003) return L"No such host is known.";
    if (st == 9501) return L"The requested name is valid, but no data of the requested type was found.";
    if (st == 1460 || st == 9002 || st == 1722 || st == 9701)
        return L"This is usually a temporary error during hostname resolution and means that the local server did not receive a response from an authoritative server.";
    return FormatSysMsg(st);
}

inline Res Check_Dns(volatile LONG *) {
    Res r = Mk(L"dns-resolution");
    const wchar_t *host = L"www.microsoft.com";
    auto query = [&](WORD type, DWORD &status) {
        PDNS_RECORD rec = nullptr; int n = 0;
        status = (DWORD)DnsQuery_W(host, type, DNS_QUERY_STANDARD, nullptr, &rec, nullptr);
        if (status == 0 && rec) { for (PDNS_RECORD p = rec; p; p = p->pNext) if (p->wType == type) n++; }
        if (rec) DnsRecordListFree(rec, DnsFreeRecordList);
        return n;
    };
    DWORD s4 = 0, s6 = 0;
    int n = query(DNS_TYPE_A, s4);
    if (n == 0) n = query(DNS_TYPE_AAAA, s6);
    if (n > 0) { r.status = L"Passed"; r.summary = L"DNS lookups are working."; return r; }
    ws detail = (s4 == 0 && s6 == 0) ? L"DNS lookup for " + ws(host) + L" returned no addresses." : DnsErrText(s4 ? s4 : s6);
    r.status = L"Failed"; r.severity = L"Medium"; r.summary = L"DNS lookups are failing."; r.detail = detail;
    r.rec = L"Flush the DNS cache, or check your DNS server configuration.";
    r.fix.has = true; r.fix.label = L"Flush DNS Cache"; r.fix.kind = L"Fix";
    r.fix.confirm = L"This clears your computer's cached DNS lookups.";
    return r;
}

struct Adapter { ws name; bool up = false; std::vector<ws> v4, gw, dns; };
inline ws DescribeAdapter(const Adapter &a) {
    return a.name + L": IPv4 " + Join(a.v4, L", ") + L"; Gateway " + Join(a.gw, L", ") + L"; DNS " + Join(a.dns, L", ");
}
inline std::vector<Adapter> EnumAdapters() {
    std::vector<Adapter> out;
    ULONG len = 15000; std::vector<char> buf;
    ULONG flags = GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST;
    ULONG rc = ERROR_BUFFER_OVERFLOW;
    for (int i = 0; i < 4 && rc == ERROR_BUFFER_OVERFLOW; i++) {
        buf.assign(len + 64, 0);
        rc = GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, (PIP_ADAPTER_ADDRESSES)buf.data(), &len);
    }
    if (rc != NO_ERROR) return out;
    for (PIP_ADAPTER_ADDRESSES p = (PIP_ADAPTER_ADDRESSES)buf.data(); p; p = p->Next) {
        if (p->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        Adapter a; a.name = p->FriendlyName ? p->FriendlyName : L""; a.up = p->OperStatus == IfOperStatusUp;
        for (auto u = p->FirstUnicastAddress; u; u = u->Next)
            if (u->Address.lpSockaddr && u->Address.lpSockaddr->sa_family == AF_INET) a.v4.push_back(FmtSockAddr(u->Address));
        for (auto g = p->FirstGatewayAddress; g; g = g->Next) { ws s = FmtSockAddr(g->Address); if (!s.empty()) a.gw.push_back(s); }
        for (auto d = p->FirstDnsServerAddress; d; d = d->Next) { ws s = FmtSockAddr(d->Address); if (!s.empty()) a.dns.push_back(s); }
        out.push_back(a);
    }
    std::stable_sort(out.begin(), out.end(), [](const Adapter &x, const Adapter &y) { return x.up && !y.up; });
    return out;
}

inline Res Check_NetworkAdapter(volatile LONG *) {
    Res r = Mk(L"network-adapter");
    std::vector<Adapter> active;
    for (auto &a : EnumAdapters()) if (a.up) active.push_back(a);
    if (active.empty()) {
        r.status = L"Failed"; r.severity = L"High"; r.summary = L"No active network adapter was found.";
        r.rec = L"Check that Wi-Fi or ethernet is enabled and connected."; return r;
    }
    const Adapter *primary = nullptr;
    for (auto &a : active) if (!a.gw.empty()) { primary = &a; break; }
    if (!primary) {
        r.status = L"Warning"; r.severity = L"Medium"; r.summary = L"The active network adapter has no default gateway configured.";
        std::vector<ws> d; for (auto &a : active) d.push_back(DescribeAdapter(a));
        r.detail = Join(d, kNL);
        r.rec = L"Check your IP configuration in Network Tools > IP Configuration, or reconnect to your network."; return r;
    }
    r.status = L"Passed"; r.summary = primary->name + L" is connected with a valid IP configuration."; r.detail = DescribeAdapter(*primary);
    return r;
}

inline CheckFn CheckFnAt(size_t i) {
    static const CheckFn t[kCheckCount] = {
        Check_WindowsIntegrity, Check_WindowsUpdate, Check_Drivers, Check_Startup, Check_Services, Check_DiskSpace,
        Check_TempFiles, Check_DiskHealth, Check_Internet, Check_Dns, Check_NetworkAdapter };
    return t[i];
}

// ======================================================================================
// Fixers
// ======================================================================================
struct FixRes {
    bool ok = false, cancelled = false;
    ws value, userMessage, technical;       // empty == null
};
inline FixRes FixOk(const ws &v, const ws &note = L"") { FixRes f; f.ok = true; f.value = v; f.userMessage = note; return f; }
inline FixRes FixFail(const ws &m, const ws &tech = L"") { FixRes f; f.ok = false; f.userMessage = m; f.technical = tech; return f; }
inline FixRes FixCancelled() { FixRes f; f.cancelled = true; return f; }

inline void FixProgress(const ws &checkId, const ws &msg) {
    PushEvent(L"diag_fix_progress", JObj().S(L"checkId", checkId).S(L"message", msg).Str());
}
inline void FixOutput(const ws &checkId, const ws &line) {
    PushEvent(L"diag_fix_output", JObj().S(L"checkId", checkId).S(L"line", line).Str());
}

// ---- DnsFlush -----------------------------------------------------------------------
inline FixRes Fix_Dns(const ws &id, volatile LONG *) {
    FixProgress(id, L"Flushing DNS cache...");
    DWORD ec = (DWORD)-1; ws out = RunCapture(L"ipconfig.exe /flushdns", &ec, 30000);
    if (ec == 0) return FixOk(L"DNS cache flushed.");
    return FixFail(L"Unable to flush the DNS cache.", ec == (DWORD)-1 ? ws(L"ipconfig.exe could not be started.") : Trim(out));
}

// ---- OpenDeviceManager --------------------------------------------------------------
inline FixRes Fix_Drivers(const ws &, volatile LONG *) {
    ComInit com;
    HINSTANCE h = ShellExecuteW(nullptr, L"open", L"devmgmt.msc", nullptr, nullptr, SW_SHOWNORMAL);
    INT_PTR rc = (INT_PTR)h;
    if (rc > 32) return FixOk(L"Device Manager opened.");
    return FixFail(L"Could not open Device Manager.", FormatSysMsg((DWORD)rc));
}

// ---- TempFiles ----------------------------------------------------------------------
struct DelStats { long long bytes = 0; int removed = 0, skipped = 0; };
inline void TryDeleteFile(const ws &path, long long len, DelStats &s) {
    SetFileAttributesW(Lp(path).c_str(), FILE_ATTRIBUTE_NORMAL);
    if (DeleteFileW(Lp(path).c_str())) { s.bytes += len; s.removed++; } else s.skipped++;
}
inline void DeleteDirRecursive(const ws &dir, DelStats &s, int depth) {
    std::vector<std::pair<ws, long long>> files; std::vector<ws> subs, links;
    WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(Lp(dir + L"\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) { s.skipped++; return; }
    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        ws full = dir + L"\\" + fd.cFileName;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) links.push_back(full);   // never follow junctions/symlinks
            else subs.push_back(full);
        } else files.push_back(std::make_pair(full, ((long long)fd.nFileSizeHigh << 32) | fd.nFileSizeLow));
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    for (auto &f : files) TryDeleteFile(f.first, f.second, s);
    for (auto &l : links) RemoveDirectoryW(Lp(l).c_str());
    if (depth < 100) for (auto &d : subs) DeleteDirRecursive(d, s, depth + 1);
    RemoveDirectoryW(Lp(dir).c_str());          // only succeeds when empty
}
inline void DeleteDirContents(const ws &root0, DelStats &s) {
    ws root = root0; while (root.size() > 3 && root.back() == L'\\') root.pop_back();
    if (!DirExists(root)) return;
    WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(Lp(root + L"\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    std::vector<WIN32_FIND_DATAW> entries;
    do { if (wcscmp(fd.cFileName, L".") && wcscmp(fd.cFileName, L"..")) entries.push_back(fd); } while (FindNextFileW(h, &fd));
    FindClose(h);
    for (auto &e : entries) {
        ws full = root + L"\\" + e.cFileName;
        if (e.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (e.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) RemoveDirectoryW(Lp(full).c_str());
            else DeleteDirRecursive(full, s, 0);
        } else TryDeleteFile(full, ((long long)e.nFileSizeHigh << 32) | e.nFileSizeLow, s);
    }
}
inline FixRes Fix_TempFiles(const ws &id, volatile LONG *cancel) {
    DelStats s;
    if (*cancel) return FixCancelled();
    FixProgress(id, L"Removing user temporary files...");
    DeleteDirContents(UserTempDir(), s);
    if (*cancel) return FixCancelled();
    FixProgress(id, L"Removing Windows temporary files...");
    DeleteDirContents(WindowsTempDir(), s);
    double gb = (double)s.bytes / 1024.0 / 1024.0 / 1024.0;
    ws value = L"Removed " + Pl(s.removed, L"file", L"files") + L", freeing " + FmtTrim(gb, 2) + L" GB.";
    ws note;
    if (s.skipped > 0)
        note = SkippedNote(s.skipped, IsAdmin());
    return FixOk(value, note);
}

// ---- elevated process helper (ShellExecuteEx "runas") -------------------------------
struct ElevRun { bool started = false, timedOut = false, cancelled = false; DWORD exitCode = (DWORD)-1; ws error; };
template <class Poll>
inline ElevRun RunElevated(const ws &exe, const ws &params, ULONGLONG timeoutMs, volatile LONG *cancel, Poll poll) {
    ElevRun r; ComInit com;
    SHELLEXECUTEINFOW sei; memset(&sei, 0, sizeof(sei));
    sei.cbSize = sizeof(sei); sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    sei.lpVerb = L"runas"; sei.lpFile = exe.c_str(); sei.lpParameters = params.c_str(); sei.nShow = SW_HIDE;
    if (!ShellExecuteExW(&sei)) { r.error = FormatSysMsg(GetLastError()); return r; }
    r.started = true;
    if (!sei.hProcess) { r.exitCode = 0; return r; }
    ULONGLONG t0 = GetTickCount64();
    for (;;) {
        DWORD w = WaitForSingleObject(sei.hProcess, 400);
        poll();
        if (w == WAIT_OBJECT_0) { DWORD ec = 0; GetExitCodeProcess(sei.hProcess, &ec); r.exitCode = ec; break; }
        if (*cancel) { TerminateProcess(sei.hProcess, 1); r.cancelled = true; break; }       // may fail for an elevated child; we stop waiting regardless
        if (GetTickCount64() - t0 > timeoutMs) { TerminateProcess(sei.hProcess, 1); r.timedOut = true; break; }
    }
    CloseHandle(sei.hProcess);
    return r;
}

// ---- captured-output file (cmd /c "x > file 2>&1") ------------------------------------
inline ws DecodeCapturedRaw(const std::string &b) {
    if (b.empty()) return L"";
    size_t sample = std::min<size_t>(b.size(), 200), zeros = 0;
    for (size_t i = 1; i < sample; i += 2) if (b[i] == 0) zeros++;
    bool u16 = zeros > sample / 4;
    if (b.size() >= 2 && (unsigned char)b[0] == 0xFF && (unsigned char)b[1] == 0xFE) u16 = true;
    ws t;
    if (u16) { size_t n = b.size() / 2; t.resize(n); if (n) memcpy(&t[0], b.data(), n * 2); }
    else t = FromCp(b, CP_UTF8);
    ws o; o.reserve(t.size());
    for (wchar_t c : t) if (c != 0 && c != 0xFEFF) o += c;
    return o;
}
inline ws ReadCaptured(const ws &path) {
    std::string b; if (!ReadFileBytes(path, b)) return L"";
    return Trim(DecodeCapturedRaw(b));
}
struct Tail {
    ws path, checkId; size_t emitted = 0;
    void Poll(bool final) {
        std::string b; if (!ReadFileBytes(path, b) || b.empty()) return;
        ws t = DecodeCapturedRaw(b), cur; std::vector<ws> segs;
        for (wchar_t c : t) { if (c == L'\r' || c == L'\n') { segs.push_back(cur); cur.clear(); } else cur += c; }
        if (final && !cur.empty()) segs.push_back(cur);
        for (; emitted < segs.size(); emitted++) { ws l = Trim(segs[emitted]); if (!l.empty()) FixOutput(checkId, l); }
    }
};
inline ws TempLogPath(const wchar_t *tag) {
    GUID g; ws id;
    if (SUCCEEDED(CoCreateGuid(&g))) { wchar_t b[40]; swprintf(b, 40, L"%08lx%04x%04x%02x%02x%02x%02x%02x%02x%02x%02x", (unsigned long)g.Data1, g.Data2, g.Data3, g.Data4[0], g.Data4[1], g.Data4[2], g.Data4[3], g.Data4[4], g.Data4[5], g.Data4[6], g.Data4[7]); id = b; }
    else id = Num(GetTickCount64()) + Num(GetCurrentProcessId());
    return RawTempDir() + L"winkit-" + tag + L"-" + id + L".log";
}

struct CapRun { int exitCode = -1; ws output; bool timedOut = false, cancelled = false; };
inline CapRun RunElevatedCaptured(const ws &checkId, const ws &exeName, const ws &exeArgs, volatile LONG *cancel) {
    const ULONGLONG kTimeout = 30ULL * 60 * 1000;
    CapRun c;
    ws tag = exeName; size_t dot = tag.find_last_of(L'.'); if (dot != ws::npos) tag = tag.substr(0, dot);
    ws tmp = TempLogPath(tag.c_str());
    Tail tail; tail.path = tmp; tail.checkId = checkId;
    ws params = L"/c \"" + exeName + L" " + exeArgs + L" > \"" + tmp + L"\" 2>&1\"";
    ElevRun er = RunElevated(L"cmd.exe", params, kTimeout, cancel, [&]() { tail.Poll(false); });
    tail.Poll(true);
    c.output = ReadCaptured(tmp);
    DeleteFileW(tmp.c_str());
    if (!er.started) { c.exitCode = -1; c.output = er.error; return c; }       // e.g. UAC declined
    c.timedOut = er.timedOut; c.cancelled = er.cancelled; c.exitCode = (int)er.exitCode;
    return c;
}
inline ws Truncate4000(const ws &t) { return t.size() <= 4000 ? t : t.substr(0, 4000) + kNL + L"... (truncated)"; }

// ---- WindowsIntegrity (DISM RestoreHealth + SFC) ---------------------------------------
inline FixRes Fix_Integrity(const ws &id, volatile LONG *cancel) {
    if (!EnglishExpected())
        return FixFail(L"This repair needs an English-language Windows installation to reliably read DISM/SFC output.",
                       L"Installed UI culture: " + UiCultureName() + L".");
    FixProgress(id, L"Checking the Windows component store...");
    CapRun chk = RunElevatedCaptured(id, L"dism.exe", L"/Online /Cleanup-Image /CheckHealth", cancel);
    if (chk.cancelled || *cancel) return FixCancelled();
    if (chk.timedOut) return FixFail(L"The check timed out and was stopped.", L"dism.exe did not finish within 30 minutes.");
    if (chk.exitCode == -1) return FixFail(L"The elevation request was cancelled or failed.", chk.output);
    if (chk.exitCode == 0 && ContainsI(chk.output, L"No component store corruption detected")) {
        RememberIntegrity(true, chk.output);
        return FixOk(L"No corruption found in the Windows component store.");
    }
    RememberIntegrity(false, chk.output);
    FixProgress(id, L"Running DISM /RestoreHealth (this can take several minutes)...");
    CapRun dism = RunElevatedCaptured(id, L"dism.exe", L"/Online /Cleanup-Image /RestoreHealth", cancel);
    if (dism.cancelled || *cancel) return FixCancelled();
    if (dism.timedOut) return FixFail(L"The repair timed out and was stopped.", L"dism.exe did not finish within 30 minutes.");
    if (dism.exitCode != 0) return FixFail(L"DISM RestoreHealth failed. System File Checker was not run.", Truncate4000(dism.output));
    FixProgress(id, L"Running System File Checker (sfc /scannow)...");
    CapRun sfc = RunElevatedCaptured(id, L"sfc.exe", L"/scannow", cancel);
    if (sfc.cancelled || *cancel) return FixCancelled();
    if (sfc.timedOut) return FixFail(L"The repair timed out and was stopped.", L"sfc.exe did not finish within 30 minutes.");
    ws combined = ws(L"--- DISM RestoreHealth ---") + kNL + dism.output + kNL + kNL + L"--- SFC /scannow ---" + kNL + sfc.output;
    bool repaired = ContainsI(sfc.output, L"successfully repaired");
    bool clean = ContainsI(sfc.output, L"did not find any integrity violations");
    bool unrepairable = ContainsI(sfc.output, L"unable to fix");
    if (sfc.exitCode == 0 && (repaired || clean)) RememberIntegrity(true, combined);
    if (sfc.exitCode == 0 && (repaired || clean))
        return FixOk(Truncate4000(combined), repaired ? L"DISM repaired the component store and SFC successfully repaired corrupted files."
                                                       : L"DISM repaired the component store. SFC found no further integrity violations.");
    if (unrepairable)
        return FixFail(L"SFC found corrupted files it could not repair automatically. Check %windir%\\Logs\\CBS\\CBS.log for details.", Truncate4000(combined));
    return FixFail(L"SFC exited with an unexpected result (code " + SNum(sfc.exitCode) + L").", Truncate4000(combined));
}

// ---- WindowsUpdateService --------------------------------------------------------------
inline FixRes Fix_WindowsUpdate(const ws &id, volatile LONG *cancel) {
    FixProgress(id, L"Setting the Windows Update service to start automatically...");
    auto noop = []() {};
    ElevRun a = RunElevated(L"sc.exe", L"config wuauserv start= demand", 5ULL * 60 * 1000, cancel, noop);
    if (!a.started) return FixFail(L"The elevation request was cancelled or failed.", a.error);
    if (a.cancelled) return FixCancelled();
    if (a.timedOut) return FixFail(L"The elevation request was cancelled or failed.", L"sc.exe did not finish in time.");
    if (a.exitCode != 0) return FixFail(L"Could not change the Windows Update service's startup type.");
    FixProgress(id, L"Starting the Windows Update service...");
    ElevRun b = RunElevated(L"sc.exe", L"start wuauserv", 5ULL * 60 * 1000, cancel, noop);
    if (!b.started) return FixFail(L"The elevation request was cancelled or failed.", b.error);
    if (b.cancelled) return FixCancelled();
    if (b.timedOut) return FixFail(L"The elevation request was cancelled or failed.", L"sc.exe did not finish in time.");
    if (b.exitCode != 0 && b.exitCode != 1056) return FixFail(L"The service's startup type was fixed, but it could not be started.");
    return FixOk(L"The Windows Update service is enabled and running.");
}

typedef FixRes (*FixFn)(const ws &, volatile LONG *);
inline FixFn FindFixer(const ws &checkId) {
    if (checkId == L"windows-integrity") return Fix_Integrity;
    if (checkId == L"windows-update") return Fix_WindowsUpdate;
    if (checkId == L"drivers") return Fix_Drivers;
    if (checkId == L"temp-files") return Fix_TempFiles;
    if (checkId == L"dns-resolution") return Fix_Dns;
    return nullptr;
}

// ======================================================================================
// Engine state + summary store
// ======================================================================================
struct State {
    CRITICAL_SECTION cs;
    volatile LONG scanRunning = 0, fixRunning = 0, scanCancel = 0, fixCancel = 0;
    bool loaded = false, has = false, fromDisk = false;
    int total = 0, issues = 0; ws ranAt; long long ranMs = 0;
    State() { InitializeCriticalSection(&cs); }
};
inline State &St() { static State s; return s; }

inline ws SummaryFile() { return AppDataDir() + L"\\diagnostics-summary.json"; }
inline bool JsonNum(const ws &j, const wchar_t *key, long long &v) {
    size_t p = j.find(ws(L"\"") + key + L"\":"); if (p == ws::npos) return false;
    p += wcslen(key) + 3; while (p < j.size() && j[p] == L' ') p++;
    wchar_t *end = nullptr; v = _wcstoi64(j.c_str() + p, &end, 10); return end && end != j.c_str() + p;
}
inline bool JsonStr(const ws &j, const wchar_t *key, ws &v) {
    size_t p = j.find(ws(L"\"") + key + L"\":\""); if (p == ws::npos) return false;
    p += wcslen(key) + 4; size_t e = j.find(L'"', p); if (e == ws::npos) return false;
    v = j.substr(p, e - p); return true;
}
inline void LoadSummaryLocked(State &s) {
    if (s.loaded) return;
    s.loaded = true;
    ws j = ReadFileText(SummaryFile()); if (j.empty()) return;
    long long t = 0, i = 0, ms = 0; ws at;
    if (!JsonNum(j, L"totalChecks", t) || !JsonNum(j, L"issueCount", i)) return;
    JsonNum(j, L"ranAtUnixMs", ms); JsonStr(j, L"ranAt", at);
    s.has = true; s.fromDisk = true; s.total = (int)t; s.issues = (int)i; s.ranMs = ms; s.ranAt = at;
}
inline ws NowIsoLocal(long long &unixMs) {
    SYSTEMTIME st; GetLocalTime(&st);
    TIME_ZONE_INFORMATION tz; DWORD id = GetTimeZoneInformation(&tz);
    long bias = tz.Bias + (id == TIME_ZONE_ID_DAYLIGHT ? tz.DaylightBias : (id == TIME_ZONE_ID_STANDARD ? tz.StandardBias : 0));
    long off = -bias; wchar_t sign = off < 0 ? L'-' : L'+'; if (off < 0) off = -off;
    wchar_t b[64];
    swprintf(b, 64, L"%04d-%02d-%02dT%02d:%02d:%02d.%03d%lc%02ld:%02ld", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, sign, off / 60, off % 60);
    FILETIME ft; GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER u; u.LowPart = ft.dwLowDateTime; u.HighPart = ft.dwHighDateTime;
    unixMs = (long long)((u.QuadPart - 116444736000000000ULL) / 10000ULL);
    return b;
}
inline ws StatusMessageFor(int issues) {
    if (issues == 0) return L"Your system is running normally.";
    if (issues == 1) return L"1 issue needs your attention.";
    return SNum(issues) + L" issues need your attention.";
}
inline ws SummaryJsonLocked(State &s) {
    LoadSummaryLocked(s);
    JObj o; o.B(L"hasRun", s.has);
    if (!s.has) {
        o.Null(L"totalChecks").Null(L"issueCount").Null(L"ranAt").Null(L"ranAtUnixMs").B(L"fromPreviousSession", false)
         .Null(L"statusIsHealthy").S(L"statusMessage", L"Diagnostics haven't been run yet.");
    } else {
        o.N(L"totalChecks", s.total).N(L"issueCount", s.issues).S(L"ranAt", s.ranAt).N(L"ranAtUnixMs", s.ranMs)
         .B(L"fromPreviousSession", s.fromDisk).B(L"statusIsHealthy", s.issues == 0).S(L"statusMessage", StatusMessageFor(s.issues));
    }
    return o.Str();
}
inline ws UpdateSummary(int total, int issues) {
    State &s = St(); CsLock l(&s.cs);
    long long ms = 0; ws at = NowIsoLocal(ms);
    s.loaded = true; s.has = true; s.fromDisk = false; s.total = total; s.issues = issues; s.ranAt = at; s.ranMs = ms;
    JObj f; f.N(L"totalChecks", total).N(L"issueCount", issues).S(L"ranAt", at).N(L"ranAtUnixMs", ms);
    WriteFileText(SummaryFile(), f.Str());
    return SummaryJsonLocked(s);
}

// ======================================================================================
// Running a check with the 20 s timeout (engine.RunSingleCheckAsync)
// ======================================================================================
struct Job {
    CheckFn fn = nullptr; const Info *info = nullptr;
    volatile LONG abort = 0; HANDLE ev = nullptr;
    Res res; bool threw = false; ws err;
    Job() { ev = CreateEventW(nullptr, TRUE, FALSE, nullptr); }
    ~Job() { if (ev) CloseHandle(ev); }
};
inline DWORD WINAPI JobProc(LPVOID p) {
    std::shared_ptr<Job> *hp = (std::shared_ptr<Job> *)p; std::shared_ptr<Job> j = *hp; delete hp;
    try { j->res = j->fn(&j->abort); }
    catch (const std::exception &e) { j->threw = true; j->err = FromCp(e.what(), CP_ACP); }
    catch (...) { j->threw = true; }
    SetEvent(j->ev);
    return 0;
}
inline Res SyntheticInfo(const Info &in, const ws &summary, const ws &detail = L"") {
    Res r; r.id = in.id; r.name = in.name; r.category = in.cat; r.status = L"Informational"; r.summary = summary; r.detail = detail; return r;
}
// returns false when the whole scan was cancelled by the user
inline bool RunSingleCheck(size_t idx, volatile LONG *cancel, Res &out) {
    const Info &in = Infos()[idx];
    std::shared_ptr<Job> j = std::make_shared<Job>();
    j->fn = CheckFnAt(idx); j->info = &in;
    if (!j->ev) { out = SyntheticInfo(in, L"This check could not complete due to an unexpected error.", L"Could not create a synchronization object."); return true; }
    HANDLE th = CreateThread(nullptr, 0, JobProc, new std::shared_ptr<Job>(j), 0, nullptr);
    if (!th) { out = SyntheticInfo(in, L"This check could not complete due to an unexpected error.", L"Could not start a worker thread."); return true; }
    CloseHandle(th);
    const ULONGLONG kTimeout = 20000; ULONGLONG t0 = GetTickCount64();
    for (;;) {
        if (WaitForSingleObject(j->ev, 100) == WAIT_OBJECT_0) break;
        if (cancel && *cancel) { InterlockedExchange(&j->abort, 1); return false; }
        if (GetTickCount64() - t0 >= kTimeout) {
            InterlockedExchange(&j->abort, 1);
            out = SyntheticInfo(in, L"This check took too long to respond and was skipped."); return true;
        }
    }
    if (j->threw) { out = SyntheticInfo(in, L"This check could not complete due to an unexpected error.", j->err); return true; }
    out = j->res; return true;
}

inline const Info *FindInfo(const ws &id, size_t &idx) {
    for (size_t i = 0; i < kCheckCount; i++) if (id == Infos()[i].id) { idx = i; return &Infos()[i]; }
    return nullptr;
}

inline ws FixJson(const FixRes &f) {
    JObj o; o.B(L"success", f.ok).B(L"cancelled", f.cancelled)
     .R(L"value", OptStr(f.ok ? f.value : ws())).R(L"userMessage", OptStr(f.userMessage)).R(L"technicalDetail", OptStr(f.technical));
    ws msg = f.ok ? (f.value.empty() ? ws(L"Completed.") : f.value) : (f.userMessage.empty() ? ws(L"Unknown error.") : f.userMessage);
    if (f.cancelled) msg = L"Cancelled.";
    o.S(L"message", msg);
    o.R(L"note", f.ok ? OptStr(f.userMessage) : ws(L"null"));
    return o.Str();
}

} // namespace diag_detail

// ============================================================================================
// Commands
// ============================================================================================
inline bool Mod_Diag(const ws &cmd, const Args &a, ws &out) {
    using namespace diag_detail;
    if (!StartsWith(cmd, L"diag_")) return false;
    State &st = St();

    if (cmd == L"diag_list") {
        JArr checks;
        for (size_t i = 0; i < kCheckCount; i++) {
            const Info &in = Infos()[i];
            JObj o; o.S(L"id", in.id).S(L"name", in.name).S(L"category", in.cat).S(L"categoryLabel", CategoryLabel(in.cat))
             .B(L"hasFixer", FindFixer(in.id) != nullptr);
            checks.Obj(o);
        }
        out = JObj().R(L"checks", checks.Str()).B(L"englishUi", EnglishExpected()).S(L"uiCulture", UiCultureName()).Str();
        return true;
    }

    if (cmd == L"diag_status") {
        out = JObj().B(L"scanRunning", st.scanRunning != 0).B(L"fixRunning", st.fixRunning != 0).B(L"isAdmin", IsAdmin()).Str();
        return true;
    }

    if (cmd == L"diag_summary") {
        CsLock l(&st.cs); out = SummaryJsonLocked(st); return true;
    }

    if (cmd == L"diag_cancel" || cmd == L"diag_cancel_scan" || cmd == L"diag_cancel_fix") {
        ws target = cmd == L"diag_cancel_scan" ? ws(L"scan") : cmd == L"diag_cancel_fix" ? ws(L"fix") : Lower(Arg(a, L"target", L"all"));
        bool cs = false, cf = false;
        if ((target == L"scan" || target == L"all") && st.scanRunning) { InterlockedExchange(&st.scanCancel, 1); cs = true; }
        if ((target == L"fix" || target == L"all") && st.fixRunning) { InterlockedExchange(&st.fixCancel, 1); cf = true; }
        out = JObj().B(L"cancelledScan", cs).B(L"cancelledFix", cf).Str();
        return true;
    }

    if (cmd == L"diag_run_one") {
        ws id = Arg(a, L"checkId");   // never "id": that key carries the bridge request number
        size_t idx = 0; if (!FindInfo(id, idx)) { out = Err(L"Unknown check: " + id); return true; }
        if (InterlockedCompareExchange(&st.scanRunning, 1, 0) != 0) { out = Err(L"A diagnostics scan is already running."); return true; }
        if (st.fixRunning) { InterlockedExchange(&st.scanRunning, 0); out = Err(L"A fix is currently running."); return true; }
        InterlockedExchange(&st.scanCancel, 0);
        Res r; bool ok = RunSingleCheck(idx, &st.scanCancel, r);
        InterlockedExchange(&st.scanRunning, 0);
        if (!ok) out = JObj().B(L"cancelled", true).Null(L"result").Str();
        else out = JObj().B(L"cancelled", false).R(L"result", ResJson(r)).Str();
        return true;
    }

    if (cmd == L"diag_run_all") {
        if (InterlockedCompareExchange(&st.scanRunning, 1, 0) != 0) { out = Err(L"A diagnostics scan is already running."); return true; }
        if (st.fixRunning) { InterlockedExchange(&st.scanRunning, 0); out = Err(L"A fix is currently running."); return true; }
        InterlockedExchange(&st.scanCancel, 0);
        JArr results; bool cancelled = false; int issues = 0, done = 0;
        for (size_t i = 0; i < kCheckCount; i++) {
            if (st.scanCancel) { cancelled = true; break; }
            PushEvent(L"diag_check_started", JObj().S(L"checkId", Infos()[i].id).S(L"name", Infos()[i].name).N(L"index", (long long)i).N(L"total", (long long)kCheckCount).Str());
            Res r;
            if (!RunSingleCheck(i, &st.scanCancel, r)) { cancelled = true; break; }
            done++; if (IsIssue(r.status)) issues++;
            ws rj = ResJson(r); results.R(rj);
            PushEvent(L"diag_progress", JObj().N(L"index", (long long)i).N(L"total", (long long)kCheckCount).N(L"completed", done).R(L"result", rj).Str());
        }
        ws summary = L"null";
        if (!cancelled) {
            summary = UpdateSummary(done, issues);
            PushEvent(L"diag_summary_changed", summary);
        }
        InterlockedExchange(&st.scanRunning, 0);
        PushEvent(L"diag_finished", JObj().B(L"cancelled", cancelled).N(L"completed", done).Str());
        out = JObj().B(L"cancelled", cancelled).R(L"results", results.Str()).R(L"summary", summary).Str();
        return true;
    }

    if (cmd == L"diag_fix") {
        ws id = Arg(a, L"checkId");   // never "id": that key carries the bridge request number
        if (InterlockedCompareExchange(&st.fixRunning, 1, 0) != 0) { out = Err(L"A fix is already running."); return true; }
        if (st.scanRunning) { InterlockedExchange(&st.fixRunning, 0); out = Err(L"A diagnostics scan is currently running."); return true; }
        InterlockedExchange(&st.fixCancel, 0);
        FixRes fr;
        FixFn fn = FindFixer(id);
        if (!fn) fr = FixFail(L"No automatic fix is available for this check.");
        else {
            try { fr = fn(id, &st.fixCancel); }
            catch (const std::exception &e) { fr = FixFail(L"The fix failed unexpectedly.", FromCp(e.what(), CP_ACP)); }
            catch (...) { fr = FixFail(L"The fix failed unexpectedly."); }
        }
        InterlockedExchange(&st.fixRunning, 0);
        PushEvent(L"diag_fix_finished", JObj().S(L"checkId", id).B(L"success", fr.ok).B(L"cancelled", fr.cancelled).Str());
        out = FixJson(fr);
        return true;
    }

    return false;
}

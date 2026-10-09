// mod_system.h - port of WinKit.System (process / services / startup / installed apps / env vars /
// hosts file / system information). Header-only. See API_system.md for the command contract.
#pragma once
#include "common.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <objbase.h>
#include <oleauto.h>
#include <wbemidl.h>
#include <mutex>
#include <cmath>

#ifdef _MSC_VER
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "version.lib")
#endif

namespace wksys {

// ---------------------------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------------------------
inline ws WinErr(DWORD e) {
    wchar_t *b = nullptr;
    DWORD n = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                             nullptr, e, 0, (LPWSTR)&b, 0, nullptr);
    ws r;
    if (n && b) r = Trim(ws(b, n)); else r = L"Win32 error " + std::to_wstring((unsigned long)e);
    if (b) LocalFree(b);
    return r;
}

// OperationResult as JSON: {"success":bool,"message":string|null,"detail":string|null}
inline ws OpOk(const ws &message = L"") {
    JObj o; o.B(L"success", true);
    if (message.empty()) o.Null(L"message"); else o.S(L"message", message);
    o.Null(L"detail"); return o.Str();
}
inline ws OpFail(const ws &message, const ws &detail = L"") {
    JObj o; o.B(L"success", false).S(L"message", message);
    if (detail.empty()) o.Null(L"detail"); else o.S(L"detail", detail);
    return o.Str();
}
inline ws StrOrNull(bool has, const ws &v) { return has ? JStr(v) : ws(L"null"); }

// StringComparer.OrdinalIgnoreCase
inline int CmpOI(const ws &a, const ws &b) {
    int r = CompareStringOrdinal(a.c_str(), (int)a.size(), b.c_str(), (int)b.size(), TRUE);
    return r == 0 ? 0 : r - 2;
}
inline bool EqOI(const ws &a, const ws &b) { return CmpOI(a, b) == 0; }
inline bool EndsWithI(const ws &s, const ws &suf) { return s.size() >= suf.size() && EqOI(s.substr(s.size() - suf.size()), suf); }
inline bool IsBlank(const ws &s) { for (wchar_t c : s) if (!iswspace(c)) return false; return true; }

// RAII COM apartment for the duration of one command
struct ComScope {
    bool inited = false;
    ComScope() { HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED); inited = SUCCEEDED(hr); }
    ~ComScope() { if (inited) CoUninitialize(); }
};

// registry value readers on an open key; strings expand REG_EXPAND_SZ like RegistryKey.GetValue
inline bool KeyStr(HKEY k, const wchar_t *name, ws &out) {
    DWORD type = 0, sz = 0;
    if (RegQueryValueExW(k, name, nullptr, &type, nullptr, &sz) != ERROR_SUCCESS) return false;
    if (type != REG_SZ && type != REG_EXPAND_SZ) return false;
    std::vector<wchar_t> b(sz / 2 + 2, 0);
    if (RegQueryValueExW(k, name, nullptr, &type, (LPBYTE)b.data(), &sz) != ERROR_SUCCESS) return false;
    out = b.data();
    if (type == REG_EXPAND_SZ && !out.empty()) {
        DWORD n = ExpandEnvironmentStringsW(out.c_str(), nullptr, 0);
        if (n) { std::vector<wchar_t> e(n + 1, 0); DWORD m = ExpandEnvironmentStringsW(out.c_str(), e.data(), n + 1); if (m && m <= n + 1) out = e.data(); }
    }
    return true;
}
inline bool KeyDword(HKEY k, const wchar_t *name, DWORD &out) {
    DWORD type = 0, sz = sizeof(DWORD);
    return RegQueryValueExW(k, name, nullptr, &type, (LPBYTE)&out, &sz) == ERROR_SUCCESS && type == REG_DWORD;
}

// ---------------------------------------------------------------------------------------------
// process manager
// ---------------------------------------------------------------------------------------------
struct ProcSnap { DWORD pid; ws name; DWORD threads; };

inline std::vector<ProcSnap> SnapProcesses() {
    std::vector<ProcSnap> v;
    HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (h == INVALID_HANDLE_VALUE) return v;
    PROCESSENTRY32W pe; pe.dwSize = sizeof(pe);
    if (Process32FirstW(h, &pe)) {
        do {
            ProcSnap p; p.pid = pe.th32ProcessID; p.threads = pe.cntThreads;
            p.name = pe.szExeFile;
            if (p.pid == 0) p.name = L"Idle";
            else if (p.name.size() > 4 && EndsWithI(p.name, L".exe")) p.name.resize(p.name.size() - 4);   // Process.ProcessName
            v.push_back(p);
        } while (Process32NextW(h, &pe));
    }
    CloseHandle(h);
    return v;
}

inline ULONGLONG FtU64(const FILETIME &f) { return ((ULONGLONG)f.dwHighDateTime << 32) | f.dwLowDateTime; }

// total processor time in 100ns units (0 if not readable, like TryGetCpuTime)
inline ULONGLONG ProcCpu100ns(HANDLE h) {
    FILETIME c, e, k, u;
    if (h && GetProcessTimes(h, &c, &e, &k, &u)) return FtU64(k) + FtU64(u);
    return 0;
}

// ---- system-wide process table (NtQuerySystemInformation) ---------------------------------------------------
// Unlike OpenProcess + GetProcessMemoryInfo this also reports working set and CPU time for protected / system services
// when WinKit runs as a normal user (they would otherwise show 0 B and 0 %), and it needs no handle per process.
struct NtProc { DWORD pid = 0; ws name; DWORD threads = 0; unsigned long long workingSet = 0; ULONGLONG cpu100ns = 0; ULONGLONG created = 0; };
struct NtUnicode { USHORT Length; USHORT MaximumLength; PWSTR Buffer; };
struct NtSysProcInfo {            // SYSTEM_PROCESS_INFORMATION (layout stable since Windows 7)
    ULONG NextEntryOffset; ULONG NumberOfThreads; LARGE_INTEGER WorkingSetPrivateSize; ULONG HardFaultCount; ULONG NumberOfThreadsHighWatermark;
    ULONGLONG CycleTime; LARGE_INTEGER CreateTime, UserTime, KernelTime; NtUnicode ImageName; LONG BasePriority;
    HANDLE UniqueProcessId; HANDLE InheritedFromUniqueProcessId; ULONG HandleCount; ULONG SessionId; ULONG_PTR UniqueProcessKey;
    SIZE_T PeakVirtualSize; SIZE_T VirtualSize; ULONG PageFaultCount; SIZE_T PeakWorkingSetSize; SIZE_T WorkingSetSize;
};
inline bool NtSnapshot(std::vector<NtProc> &out) {
    typedef LONG (NTAPI *NtQsiFn)(ULONG, PVOID, ULONG, PULONG);
    static NtQsiFn fn = (NtQsiFn)(void *)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQuerySystemInformation");
    if (!fn) return false;
    std::vector<BYTE> buf(1 << 20);
    for (int attempt = 0; attempt < 8; attempt++) {
        ULONG need = 0;
        LONG st = fn(5 /* SystemProcessInformation */, buf.data(), (ULONG)buf.size(), &need);
        if (st == 0) {
            out.clear();
            for (size_t off = 0;;) {
                const NtSysProcInfo *p = (const NtSysProcInfo *)(buf.data() + off);
                NtProc r; r.pid = (DWORD)(ULONG_PTR)p->UniqueProcessId; r.threads = p->NumberOfThreads;
                r.workingSet = (unsigned long long)p->WorkingSetSize;
                r.cpu100ns = (ULONGLONG)p->UserTime.QuadPart + (ULONGLONG)p->KernelTime.QuadPart; r.created = (ULONGLONG)p->CreateTime.QuadPart;
                if (r.pid == 0) r.name = L"Idle";
                else if (p->ImageName.Buffer && p->ImageName.Length) {
                    r.name.assign(p->ImageName.Buffer, p->ImageName.Length / sizeof(wchar_t));
                    if (r.name.size() > 4 && EndsWithI(r.name, L".exe")) r.name.resize(r.name.size() - 4);          // Process.ProcessName
                } else r.name = L"System";
                out.push_back(r);
                if (!p->NextEntryOffset) break;
                off += p->NextEntryOffset;
            }
            return !out.empty();
        }
        if (st == (LONG)0xC0000004 /* STATUS_INFO_LENGTH_MISMATCH */) buf.resize((size_t)need + (256u << 10)); else return false;
    }
    return false;
}

inline ws ProcessListLegacy();

inline ws ProcessList() {
    std::vector<NtProc> first;
    if (!NtSnapshot(first)) return ProcessListLegacy();
    LARGE_INTEGER f, t0, t1; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&t0);
    Sleep(300);
    std::vector<NtProc> second;
    if (!NtSnapshot(second)) return ProcessListLegacy();
    QueryPerformanceCounter(&t1);
    double elapsedMs = f.QuadPart ? (double)(t1.QuadPart - t0.QuadPart) * 1000.0 / (double)f.QuadPart : 0;
    SYSTEM_INFO si; GetSystemInfo(&si);
    double cores = si.dwNumberOfProcessors ? (double)si.dwNumberOfProcessors : 1.0;
    DWORD self = GetCurrentProcessId();
    std::map<DWORD, ULONGLONG> before; for (auto &p : first) before[p.pid] = p.cpu100ns;

    struct Row { DWORD pid; ws name; bool hasPath; ws path; unsigned long long ws_; double cpu; DWORD threads; };
    std::vector<Row> rows; rows.reserve(second.size());
    // image paths never change for a running process, so resolve each (pid, creation time) once
    struct PathEntry { ULONGLONG created; bool has; ws path; };
    static std::mutex pathMutex; static std::map<DWORD, PathEntry> pathCache;
    std::lock_guard<std::mutex> pathLock(pathMutex);
    { std::map<DWORD, PathEntry> keep; for (auto &p : second) { auto c = pathCache.find(p.pid); if (c != pathCache.end() && c->second.created == p.created) keep[p.pid] = c->second; } pathCache.swap(keep); }
    for (auto &p : second) {
        Row r; r.pid = p.pid; r.name = p.name; r.hasPath = false; r.ws_ = p.workingSet; r.cpu = 0; r.threads = p.threads;
        auto it = before.find(p.pid);
        if (it != before.end() && p.pid != 0 && elapsedMs > 0 && p.cpu100ns >= it->second)
            r.cpu = std::min(100.0, std::max(0.0, (double)(p.cpu100ns - it->second) / 10000.0 / elapsedMs / cores * 100.0));
        if (p.pid > 4) {
            auto c = pathCache.find(p.pid);
            if (c == pathCache.end()) {
                PathEntry e{ p.created, false, ws() };
                HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, p.pid);
                if (h) {
                    wchar_t path[4096]; DWORD n = 4096;
                    if (QueryFullProcessImageNameW(h, 0, path, &n) && n) { e.has = true; e.path.assign(path, n); }
                    CloseHandle(h);
                }
                c = pathCache.emplace(p.pid, e).first;
            }
            r.hasPath = c->second.has; r.path = c->second.path;
        }
        rows.push_back(r);
    }
    std::stable_sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) {
        if (a.cpu != b.cpu) return a.cpu > b.cpu;
        return a.ws_ > b.ws_;
    });
    JArr arr;
    for (auto &r : rows) {
        JObj o; o.N(L"id", r.pid).S(L"name", r.name);
        o.R(L"filePath", StrOrNull(r.hasPath, r.path));
        o.U(L"workingSetBytes", r.ws_).D(L"cpuPercent", r.cpu, 2).N(L"threadCount", r.threads).B(L"canBeEnded", r.pid != self && r.pid > 4);
        arr.Obj(o);
    }
    return arr.Str();
}

inline ws ProcessListLegacy() {
    std::vector<ProcSnap> first = SnapProcesses();
    std::map<DWORD, ULONGLONG> samples;
    for (auto &p : first) {
        HANDLE h = p.pid ? OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, p.pid) : nullptr;
        samples[p.pid] = ProcCpu100ns(h);
        if (h) CloseHandle(h);
    }
    LARGE_INTEGER f, t0, t1; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&t0);
    Sleep(300);
    QueryPerformanceCounter(&t1);
    double elapsedMs = f.QuadPart ? (double)(t1.QuadPart - t0.QuadPart) * 1000.0 / (double)f.QuadPart : 0;
    SYSTEM_INFO si; GetSystemInfo(&si);
    double cores = si.dwNumberOfProcessors ? (double)si.dwNumberOfProcessors : 1.0;
    DWORD self = GetCurrentProcessId();

    struct Row { DWORD pid; ws name; bool hasPath; ws path; unsigned long long ws_; double cpu; DWORD threads; };
    std::vector<Row> rows;
    std::vector<ProcSnap> second = SnapProcesses();
    std::map<DWORD, const ProcSnap *> secondByPid;
    for (auto &p : second) secondByPid[p.pid] = &p;

    for (auto &p : first) {
        auto it = secondByPid.find(p.pid);
        if (it == secondByPid.end()) continue;               // exited while inspecting
        const ProcSnap &cur = *it->second;
        Row r; r.pid = p.pid; r.name = cur.name; r.hasPath = false; r.ws_ = 0; r.cpu = 0; r.threads = cur.threads;
        HANDLE h = p.pid ? OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, p.pid) : nullptr;
        if (h) {
            ULONGLONG now = ProcCpu100ns(h);
            ULONGLONG prev = samples[p.pid];
            double deltaMs = (double)((long long)now - (long long)prev) / 10000.0;
            r.cpu = elapsedMs <= 0 ? 0 : std::min(100.0, std::max(0.0, deltaMs / elapsedMs / cores * 100.0));
            PROCESS_MEMORY_COUNTERS pmc; pmc.cb = sizeof(pmc);
            if (GetProcessMemoryInfo(h, &pmc, sizeof(pmc))) r.ws_ = (unsigned long long)pmc.WorkingSetSize;
            wchar_t path[4096]; DWORD n = 4096;
            if (QueryFullProcessImageNameW(h, 0, path, &n) && n) { r.hasPath = true; r.path.assign(path, n); }
            CloseHandle(h);
        }
        rows.push_back(r);
    }
    std::stable_sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) {
        if (a.cpu != b.cpu) return a.cpu > b.cpu;
        return a.ws_ > b.ws_;
    });
    JArr arr;
    for (auto &r : rows) {
        JObj o; o.N(L"id", r.pid).S(L"name", r.name);
        o.R(L"filePath", StrOrNull(r.hasPath, r.path));
        o.U(L"workingSetBytes", r.ws_).D(L"cpuPercent", r.cpu, 2).N(L"threadCount", r.threads).B(L"canBeEnded", r.pid != self && r.pid > 4);   // 0 = Idle, 4 = System: never endable
        arr.Obj(o);
    }
    return arr.Str();
}

inline ws ProcessEnd(long long pidArg) {
    if (pidArg < 0 || pidArg > 0xFFFFFFFFLL) return OpFail(L"That process has already exited.");
    DWORD pid = (DWORD)pidArg;
    if (pid == GetCurrentProcessId()) return OpFail(L"Unable to end that process.", L"WinKit cannot end its own process.");
    if (pid == 0) return OpFail(L"Unable to end that process. It may require administrator access.", WinErr(ERROR_ACCESS_DENIED));
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!h) {
        DWORD e = GetLastError();
        if (e == ERROR_INVALID_PARAMETER) return OpFail(L"That process has already exited.");
        return OpFail(L"Unable to end that process. It may require administrator access.", WinErr(e));
    }
    BOOL ok = TerminateProcess(h, 1);
    DWORD e = ok ? 0 : GetLastError();
    CloseHandle(h);
    if (!ok) return OpFail(L"Unable to end that process. It may require administrator access.", WinErr(e));
    return OpOk();
}

// ---------------------------------------------------------------------------------------------
// services manager
// ---------------------------------------------------------------------------------------------
inline const wchar_t *SvcStateName(DWORD s) {
    switch (s) {
    case SERVICE_STOPPED: return L"Stopped";
    case SERVICE_START_PENDING: return L"StartPending";
    case SERVICE_STOP_PENDING: return L"StopPending";
    case SERVICE_RUNNING: return L"Running";
    case SERVICE_CONTINUE_PENDING: return L"ContinuePending";
    case SERVICE_PAUSE_PENDING: return L"PausePending";
    case SERVICE_PAUSED: return L"Paused";
    default: return L"Unknown";
    }
}
inline ws SvcStartType(SC_HANDLE scm, const wchar_t *name) {
    SC_HANDLE s = OpenServiceW(scm, name, SERVICE_QUERY_CONFIG);
    if (!s) return L"Unknown";
    ws r = L"Unknown";
    std::vector<BYTE> buf(8192);
    DWORD need = 0;
    bool have = QueryServiceConfigW(s, (LPQUERY_SERVICE_CONFIGW)buf.data(), (DWORD)buf.size(), &need) != FALSE;
    if (!have && GetLastError() == ERROR_INSUFFICIENT_BUFFER && need > 0) {
        buf.resize(need);
        have = QueryServiceConfigW(s, (LPQUERY_SERVICE_CONFIGW)buf.data(), (DWORD)buf.size(), &need) != FALSE;
    }
    if (have) {
        switch (((LPQUERY_SERVICE_CONFIGW)buf.data())->dwStartType) {
        case SERVICE_BOOT_START: r = L"Boot"; break;
        case SERVICE_SYSTEM_START: r = L"System"; break;
        case SERVICE_AUTO_START: r = L"Automatic"; break;
        case SERVICE_DEMAND_START: r = L"Manual"; break;
        case SERVICE_DISABLED: r = L"Disabled"; break;
        default: break;
        }
    }
    CloseServiceHandle(s);
    return r;
}

inline ws ServiceList() {
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE);
    if (!scm) return Err(L"Unable to open the Service Control Manager: " + WinErr(GetLastError()));
    struct Row { ws name, display, status, startType; bool canStart, canStop; };
    std::vector<Row> rows;
    std::vector<BYTE> buf(1 << 18);
    DWORD resume = 0;
    for (;;) {
        DWORD need = 0, count = 0;
        BOOL ok = EnumServicesStatusExW(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, buf.data(),
                                        (DWORD)buf.size(), &need, &count, &resume, nullptr);
        DWORD e = ok ? 0 : GetLastError();
        if (!ok && e != ERROR_MORE_DATA) { CloseServiceHandle(scm); return Err(L"Unable to list services: " + WinErr(e)); }
        ENUM_SERVICE_STATUS_PROCESSW *p = (ENUM_SERVICE_STATUS_PROCESSW *)buf.data();
        for (DWORD i = 0; i < count; i++) {
            Row r; r.name = p[i].lpServiceName ? p[i].lpServiceName : L""; r.display = p[i].lpDisplayName ? p[i].lpDisplayName : L"";
            DWORD st = p[i].ServiceStatusProcess.dwCurrentState;
            r.status = SvcStateName(st);
            r.startType = SvcStartType(scm, r.name.c_str());
            r.canStart = st == SERVICE_STOPPED;
            r.canStop = st == SERVICE_RUNNING && (p[i].ServiceStatusProcess.dwControlsAccepted & SERVICE_ACCEPT_STOP) != 0;
            rows.push_back(r);
        }
        if (ok) break;
        if (count == 0 && need > buf.size()) buf.resize(need);
    }
    CloseServiceHandle(scm);
    std::stable_sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) { return CmpOI(a.display, b.display) < 0; });
    JArr arr;
    for (auto &r : rows) {
        JObj o; o.S(L"name", r.name).S(L"displayName", r.display).S(L"status", r.status).S(L"startupType", r.startType)
            .B(L"canStart", r.canStart).B(L"canStop", r.canStop);
        arr.Obj(o);
    }
    return arr.Str();
}

inline ws ServiceChange(const ws &name, bool start) {
    const ws verb = start ? L"start" : L"stop";
    const ws failMsg = L"Unable to " + verb + L" \"" + name + L"\". This may require administrator access.";
    if (name.empty()) return OpFail(failMsg, L"Service name is empty.");
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) return OpFail(failMsg, L"Cannot open Service Control Manager on computer '.'. " + WinErr(GetLastError()));
    SC_HANDLE s = OpenServiceW(scm, name.c_str(), (start ? SERVICE_START : SERVICE_STOP) | SERVICE_QUERY_STATUS);
    if (!s) {
        ws d = L"Cannot open " + name + L" service on computer '.'. " + WinErr(GetLastError());
        CloseServiceHandle(scm); return OpFail(failMsg, d);
    }
    BOOL ok; DWORD e = 0;
    if (start) ok = StartServiceW(s, 0, nullptr);
    else { SERVICE_STATUS st; ok = ControlService(s, SERVICE_CONTROL_STOP, &st); }
    if (!ok) e = GetLastError();
    if (!ok) {
        ws d = L"Cannot " + verb + L" " + name + L" service on computer '.'. " + WinErr(e);
        CloseServiceHandle(s); CloseServiceHandle(scm); return OpFail(failMsg, d);
    }
    DWORD target = start ? SERVICE_RUNNING : SERVICE_STOPPED;
    ULONGLONG t0 = GetTickCount64();
    ws result;
    for (;;) {
        SERVICE_STATUS st;
        if (!QueryServiceStatus(s, &st)) { result = OpFail(failMsg, L"Cannot query status of " + name + L" service. " + WinErr(GetLastError())); break; }
        if (st.dwCurrentState == target) { result = OpOk(); break; }
        if (GetTickCount64() - t0 >= 10000) {
            result = OpFail(L"Timed out waiting for \"" + name + L"\" to " + verb + L".", L"Time out has expired and the operation has not been completed.");
            break;
        }
        Sleep(250);
    }
    CloseServiceHandle(s); CloseServiceHandle(scm);
    return result;
}

// ---------------------------------------------------------------------------------------------
// startup manager
// ---------------------------------------------------------------------------------------------
static const wchar_t *const kRunKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

struct DisabledReg { ws name, command; bool machine = false; };

inline ws BackupFilePath() { return AppDataDir() + L"\\disabled-startup.json"; }
inline std::mutex &BackupMutex() { static std::mutex m; return m; }

inline bool JsonReadString(const ws &s, size_t &i, ws &out) {   // i at opening quote; leaves i after closing quote
    out.clear();
    if (i >= s.size() || s[i] != L'"') return false;
    i++;
    while (i < s.size()) {
        wchar_t c = s[i++];
        if (c == L'"') return true;
        if (c != L'\\') { out += c; continue; }
        if (i >= s.size()) return false;
        wchar_t d = s[i++];
        switch (d) {
        case L'n': out += L'\n'; break;
        case L'r': out += L'\r'; break;
        case L't': out += L'\t'; break;
        case L'b': out += L'\b'; break;
        case L'f': out += L'\f'; break;
        case L'u': {
            if (i + 4 > s.size()) return false;
            out += (wchar_t)wcstoul(s.substr(i, 4).c_str(), nullptr, 16); i += 4; break;
        }
        default: out += d; break;     // \" \\ \/
        }
    }
    return false;
}

inline std::vector<DisabledReg> LoadBackup() {
    std::vector<DisabledReg> v;
    ws s = ReadFileText(BackupFilePath());
    size_t i = 0, n = s.size();
    bool inObj = false; DisabledReg cur;
    while (i < n) {
        wchar_t c = s[i];
        if (c == L'{') { inObj = true; cur = DisabledReg(); i++; }
        else if (c == L'}') { if (inObj) v.push_back(cur); inObj = false; i++; }
        else if (c == L'"') {
            ws key; if (!JsonReadString(s, i, key)) break;
            if (!inObj) continue;
            while (i < n && iswspace(s[i])) i++;
            if (i >= n || s[i] != L':') continue;
            i++;
            while (i < n && iswspace(s[i])) i++;
            if (i < n && s[i] == L'"') {
                ws val; if (!JsonReadString(s, i, val)) break;
                if (EqOI(key, L"Name")) cur.name = val; else if (EqOI(key, L"Command")) cur.command = val;
            } else {
                size_t j = i; while (j < n && s[j] != L',' && s[j] != L'}' && !iswspace(s[j])) j++;
                ws tok = s.substr(i, j - i); i = j;
                if (EqOI(key, L"IsMachineScope")) cur.machine = (tok == L"true");
            }
        } else i++;
    }
    return v;
}

inline bool SaveBackup(const std::vector<DisabledReg> &v) {
    ws t;
    if (v.empty()) t = L"[]";
    else {
        t = L"[\r\n";
        for (size_t i = 0; i < v.size(); i++) {
            t += L"  {\r\n    \"Name\": " + JStr(v[i].name) + L",\r\n    \"Command\": " + JStr(v[i].command) +
                 L",\r\n    \"IsMachineScope\": " + Bool(v[i].machine) + L"\r\n  }" + (i + 1 < v.size() ? L",\r\n" : L"\r\n");
        }
        t += L"]";
    }
    ws path = BackupFilePath(), tmp = path + L"." + std::to_wstring((unsigned long)GetCurrentProcessId()) + L"." + std::to_wstring((unsigned long long)GetTickCount64()) + L".tmp";
    if (!WriteFileText(tmp, t)) return false;
    if (!MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) { DeleteFileW(tmp.c_str()); return false; }
    return true;
}

struct StartupEntry { ws name, command; bool hasCommand = true; ws scope; bool enabled = true; };

inline void AddRegistryEntries(std::vector<StartupEntry> &out, HKEY hive, const wchar_t *scope) {
    HKEY k;
    if (RegOpenKeyExW(hive, kRunKey, 0, KEY_READ, &k) != ERROR_SUCCESS) return;
    for (DWORD i = 0;; i++) {
        wchar_t nm[16384]; DWORD nl = 16384;
        LONG r = RegEnumValueW(k, i, nm, &nl, nullptr, nullptr, nullptr, nullptr);
        if (r == ERROR_NO_MORE_ITEMS) break;
        if (r != ERROR_SUCCESS) continue;
        ws name(nm, nl), cmd;
        if (KeyStr(k, nm, cmd)) { StartupEntry e; e.name = name; e.command = cmd; e.scope = scope; e.enabled = true; out.push_back(e); }
    }
    RegCloseKey(k);
}

inline ws FolderPath(int csidl) {
    wchar_t p[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, csidl, nullptr, SHGFP_TYPE_CURRENT, p))) return p;
    return L"";
}
inline void AddFolderEntries(std::vector<StartupEntry> &out, int csidl, const wchar_t *scope) {
    ws dir = FolderPath(csidl);
    if (dir.empty() || !DirExists(dir)) return;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        // desktop.ini (and any other hidden/system file) is folder metadata, not a startup program - never list or rename it
        if ((fd.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) || EqOI(fd.cFileName, L"desktop.ini")) continue;
        ws file = dir + L"\\" + fd.cFileName;
        bool dis = EndsWithI(file, L".disabled");
        ws baseName = fd.cFileName;
        if (dis) baseName.resize(baseName.size() - 9);
        size_t dot = baseName.rfind(L'.');                                  // Path.GetFileNameWithoutExtension
        ws name = (dot == ws::npos) ? baseName : baseName.substr(0, dot);
        StartupEntry e; e.name = name; e.command = file; e.scope = scope; e.enabled = !dis;
        out.push_back(e);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

inline ws StartupList() {
    std::vector<StartupEntry> entries;
    std::vector<DisabledReg> disabled;
    { std::lock_guard<std::mutex> g(BackupMutex()); disabled = LoadBackup(); }
    AddRegistryEntries(entries, HKEY_CURRENT_USER, L"CurrentUserRegistry");
    AddRegistryEntries(entries, HKEY_LOCAL_MACHINE, L"AllUsersRegistry");
    for (auto &b : disabled) {
        StartupEntry e; e.name = b.name; e.command = b.command; e.scope = b.machine ? L"AllUsersRegistry" : L"CurrentUserRegistry"; e.enabled = false;
        entries.push_back(e);
    }
    AddFolderEntries(entries, CSIDL_STARTUP, L"CurrentUserFolder");
    AddFolderEntries(entries, CSIDL_COMMON_STARTUP, L"AllUsersFolder");
    std::stable_sort(entries.begin(), entries.end(), [](const StartupEntry &a, const StartupEntry &b) { return CmpOI(a.name, b.name) < 0; });
    JArr arr;
    for (auto &e : entries) {
        JObj o; o.S(L"name", e.name).S(L"command", e.command).S(L"scope", e.scope).B(L"isEnabled", e.enabled)
            .B(L"requiresElevationToChange", e.scope == L"AllUsersRegistry" || e.scope == L"AllUsersFolder");
        arr.Obj(o);
    }
    return arr.Str();
}

inline ws StartupAdminFail(DWORD e) {
    return OpFail(L"This startup item requires administrator access to change. Restart WinKit as Administrator.", WinErr(e));
}

inline ws StartupSet(const ws &scope, const ws &name, const ws &command, bool hasCommand, bool enabled) {
    if (scope == L"CurrentUserRegistry" || scope == L"AllUsersRegistry") {
        bool machine = scope == L"AllUsersRegistry";
        HKEY hive = machine ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
        std::lock_guard<std::mutex> g(BackupMutex());
        std::vector<DisabledReg> backups = LoadBackup();
        auto drop = [&]() { backups.erase(std::remove_if(backups.begin(), backups.end(), [&](const DisabledReg &b) { return b.name == name && b.machine == machine; }), backups.end()); };
        if (!enabled) {
            HKEY k = nullptr;
            LONG r = RegOpenKeyExW(hive, kRunKey, 0, KEY_READ | KEY_SET_VALUE, &k);
            if (r == ERROR_ACCESS_DENIED) return StartupAdminFail((DWORD)r);
            ws value = hasCommand ? command : ws();
            if (r == ERROR_SUCCESS) {
                ws cur;
                if (KeyStr(k, name.c_str(), cur)) value = cur;
                LONG d = RegDeleteValueW(k, name.c_str());
                RegCloseKey(k);
                if (d == ERROR_ACCESS_DENIED) return StartupAdminFail((DWORD)d);
            }
            drop();
            DisabledReg b; b.name = name; b.command = value; b.machine = machine; backups.push_back(b);
        } else {
            HKEY k = nullptr; DWORD disp = 0;
            LONG r = RegCreateKeyExW(hive, kRunKey, 0, nullptr, 0, KEY_READ | KEY_SET_VALUE, nullptr, &k, &disp);
            if (r == ERROR_ACCESS_DENIED) return StartupAdminFail((DWORD)r);
            if (r != ERROR_SUCCESS) return OpFail(L"Unable to change that startup item.", WinErr((DWORD)r));
            ws v = hasCommand ? command : ws();
            r = RegSetValueExW(k, name.c_str(), 0, REG_SZ, (const BYTE *)v.c_str(), (DWORD)((v.size() + 1) * sizeof(wchar_t)));
            RegCloseKey(k);
            if (r == ERROR_ACCESS_DENIED) return StartupAdminFail((DWORD)r);
            if (r != ERROR_SUCCESS) return OpFail(L"Unable to change that startup item.", WinErr((DWORD)r));
            drop();
        }
        if (!SaveBackup(backups)) return OpFail(L"Unable to save the startup backup file.", WinErr(GetLastError()));
        return OpOk();
    }
    if (scope == L"CurrentUserFolder" || scope == L"AllUsersFolder") {
        if (!hasCommand || command.empty() || !FileExists(command)) return OpFail(L"That shortcut could not be found.");
        ws target;
        if (enabled) {                               // string.Replace(".disabled", "", OrdinalIgnoreCase): every occurrence
            target = command; const ws pat = L".disabled"; ws low = Lower(target); size_t pos;
            while ((pos = low.find(pat)) != ws::npos) { target.erase(pos, pat.size()); low.erase(pos, pat.size()); }
        } else target = command + L".disabled";
        if (!MoveFileExW(command.c_str(), target.c_str(), 0)) {
            DWORD e = GetLastError();
            if (e == ERROR_ACCESS_DENIED) return StartupAdminFail(e);
            return OpFail(L"Unable to change that startup item.", WinErr(e));
        }
        return OpOk();
    }
    return OpFail(L"Unknown startup entry type.");
}

// ---------------------------------------------------------------------------------------------
// installed apps
// ---------------------------------------------------------------------------------------------
struct InstalledApp {
    ws name; bool hasPublisher = false, hasVersion = false, hasCmd = false, hasSize = false; ws publisher, version, cmd, installDate; bool hasDate = false; double sizeMb = 0;
};

inline bool ParseInstallDate(const ws &raw, ws &iso) {
    if (raw.size() != 8) return false;
    for (wchar_t c : raw) if (c < L'0' || c > L'9') return false;
    int y = _wtoi(raw.substr(0, 4).c_str()), m = _wtoi(raw.substr(4, 2).c_str()), d = _wtoi(raw.substr(6, 2).c_str());
    if (y < 1 || m < 1 || m > 12 || d < 1) return false;
    static const int dm[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int mx = dm[m - 1] + ((m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) ? 1 : 0);
    if (d > mx) return false;
    wchar_t b[16]; swprintf(b, 16, L"%04d-%02d-%02d", y, m, d); iso = b; return true;
}

inline std::vector<InstalledApp> InstalledApps() {
    struct Src { HKEY hive; const wchar_t *path; };
    const Src srcs[] = {
        {HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall"},
        {HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall"},
        {HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall"}};
    std::vector<InstalledApp> apps;
    for (auto &s : srcs) {
        for (const ws &sub : RegSubKeys(s.hive, s.path, KEY_WOW64_64KEY)) {
            HKEY k;
            if (RegOpenKeyExW(s.hive, (ws(s.path) + L"\\" + sub).c_str(), 0, KEY_READ | KEY_WOW64_64KEY, &k) != ERROR_SUCCESS) continue;
            InstalledApp a;
            ws name;
            if (!KeyStr(k, L"DisplayName", name) || IsBlank(name)) { RegCloseKey(k); continue; }
            DWORD sc = 0;
            if (KeyDword(k, L"SystemComponent", sc) && sc == 1) { RegCloseKey(k); continue; }
            a.name = name;
            a.hasPublisher = KeyStr(k, L"Publisher", a.publisher);
            a.hasVersion = KeyStr(k, L"DisplayVersion", a.version);
            ws dt; if (KeyStr(k, L"InstallDate", dt)) a.hasDate = ParseInstallDate(dt, a.installDate);
            DWORD es = 0; if (KeyDword(k, L"EstimatedSize", es)) { a.hasSize = true; a.sizeMb = (double)(int)es / 1024.0; }
            a.hasCmd = KeyStr(k, L"UninstallString", a.cmd);
            RegCloseKey(k);
            apps.push_back(a);
        }
    }
    std::vector<InstalledApp> uniq;
    for (auto &a : apps) {
        bool dup = false;
        for (auto &u : uniq) if (EqOI(u.name, a.name)) { dup = true; break; }
        if (!dup) uniq.push_back(a);
    }
    std::stable_sort(uniq.begin(), uniq.end(), [](const InstalledApp &a, const InstalledApp &b) { return CmpOI(a.name, b.name) < 0; });
    return uniq;
}

inline ws AppsList() {
    JArr arr;
    for (auto &a : InstalledApps()) {
        JObj o; o.S(L"name", a.name).R(L"publisher", StrOrNull(a.hasPublisher, a.publisher)).R(L"version", StrOrNull(a.hasVersion, a.version))
            .R(L"installDate", StrOrNull(a.hasDate, a.installDate));
        if (a.hasSize) o.D(L"estimatedSizeMb", a.sizeMb, 3); else o.Null(L"estimatedSizeMb");
        o.R(L"uninstallCommand", StrOrNull(a.hasCmd, a.cmd));
        arr.Obj(o);
    }
    return arr.Str();
}

inline void SplitCommand(ws command, ws &file, ws &args) {
    command = Trim(command);
    if (!command.empty() && command[0] == L'"') {
        size_t q = command.find(L'"', 1);
        if (q != ws::npos && q > 0) { file = command.substr(1, q - 1); args = Trim(command.substr(q + 1)); return; }
    }
    // Unquoted command with spaces in the path, e.g.  C:\Program Files\App\uninstall.exe /S
    // Look for the first "<ext> " boundary that names a file that really exists.
    {
        ws low = Lower(command);
        static const wchar_t *const exts[] = { L".exe", L".bat", L".cmd", L".com" };
        for (const wchar_t *ext : exts) {
            size_t len = wcslen(ext), pos = 0;
            while ((pos = low.find(ext, pos)) != ws::npos) {
                size_t end = pos + len;
                if (end == command.size() || command[end] == L' ') {
                    ws cand = command.substr(0, end);
                    if (FileExists(cand)) { file = cand; args = Trim(command.substr(end)); return; }
                }
                pos = end;
            }
        }
    }
    size_t sp = command.find(L' ');
    if (sp == ws::npos) { file = command; args.clear(); }
    else { file = command.substr(0, sp); args = Trim(command.substr(sp + 1)); }
}

inline ws AppsUninstall(const ws &name) {
    const InstalledApp *found = nullptr;
    std::vector<InstalledApp> apps = InstalledApps();
    for (auto &a : apps) if (EqOI(a.name, name)) { found = &a; break; }
    if (!found) return OpFail(L"That application is no longer installed.");
    if (!found->hasCmd || IsBlank(found->cmd)) return OpFail(L"This application does not provide an uninstaller.");
    ws file, args; SplitCommand(found->cmd, file, args);
    SHELLEXECUTEINFOW sei = {}; sei.cbSize = sizeof(sei); sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
    sei.lpFile = file.c_str(); sei.lpParameters = args.empty() ? nullptr : args.c_str(); sei.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&sei)) return OpFail(L"Unable to start the uninstaller for \"" + found->name + L"\".", WinErr(GetLastError()));
    if (sei.hProcess) CloseHandle(sei.hProcess);
    return OpOk();
}

// ---------------------------------------------------------------------------------------------
// environment variables
// ---------------------------------------------------------------------------------------------
static const wchar_t *const kMachineEnvKey = L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment";
static const wchar_t *const kUserEnvKey = L"Environment";

struct EnvEntry { ws name, value, scope; };

inline void AddEnv(std::vector<EnvEntry> &out, HKEY hive, const wchar_t *sub, const wchar_t *scope) {
    HKEY k;
    if (RegOpenKeyExW(hive, sub, 0, KEY_READ, &k) != ERROR_SUCCESS) return;
    for (DWORD i = 0;; i++) {
        wchar_t nm[16384]; DWORD nl = 16384;
        LONG r = RegEnumValueW(k, i, nm, &nl, nullptr, nullptr, nullptr, nullptr);
        if (r == ERROR_NO_MORE_ITEMS) break;
        if (r != ERROR_SUCCESS) continue;
        EnvEntry e; e.name.assign(nm, nl); e.scope = scope;
        ws v; DWORD dw = 0;
        if (KeyStr(k, nm, v)) e.value = v;
        else if (KeyDword(k, nm, dw)) e.value = std::to_wstring((unsigned long)dw);
        out.push_back(e);
    }
    RegCloseKey(k);
}

inline ws EnvList() {
    std::vector<EnvEntry> v;
    AddEnv(v, HKEY_CURRENT_USER, kUserEnvKey, L"User");
    AddEnv(v, HKEY_LOCAL_MACHINE, kMachineEnvKey, L"Machine");
    std::stable_sort(v.begin(), v.end(), [](const EnvEntry &a, const EnvEntry &b) {
        if (a.scope != b.scope) return a.scope == L"User";
        return CmpOI(a.name, b.name) < 0;
    });
    JArr arr;
    for (auto &e : v) { JObj o; o.S(L"name", e.name).S(L"value", e.value).S(L"scope", e.scope); arr.Obj(o); }
    return arr.Str();
}

inline ws EnvWrite(const ws &scope, const ws &name, const ws &value, bool remove) {
    bool machine = (scope == L"Machine");
    const ws adminMsg = L"Changing a system environment variable requires administrator access.";
    if (name.empty()) return OpFail(L"Variable name cannot be empty.");
    if (name[0] == 0) return OpFail(L"Unable to change the variable.", L"The first character of the environment variable name cannot be the null character.");
    if (name.find(L'=') != ws::npos) return OpFail(L"Unable to change the variable.", L"Environment variable name cannot contain equal character.");
    if (name.size() >= 255) return OpFail(L"Unable to change the variable.", L"The environment variable name is too long.");
    if (!remove && value.size() >= 32767) return OpFail(L"Unable to change the variable.", L"Environment variable name or value is too long.");
    bool del = remove || value.empty();             // Environment.SetEnvironmentVariable: null or "" deletes
    HKEY k = nullptr;
    LONG r = RegOpenKeyExW(machine ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER, machine ? kMachineEnvKey : kUserEnvKey, 0,
                           KEY_READ | KEY_WRITE, &k);
    if (r == ERROR_ACCESS_DENIED) return OpFail(adminMsg, WinErr((DWORD)r));
    if (r == ERROR_SUCCESS) {
        LONG w;
        if (del) { w = RegDeleteValueW(k, name.c_str()); if (w == ERROR_FILE_NOT_FOUND) w = ERROR_SUCCESS; }
        else {
            DWORD type = REG_SZ, qt = 0, qs = 0;                 // keep REG_EXPAND_SZ if the variable already used it
            if (RegQueryValueExW(k, name.c_str(), nullptr, &qt, nullptr, &qs) == ERROR_SUCCESS && qt == REG_EXPAND_SZ) type = REG_EXPAND_SZ;
            w = RegSetValueExW(k, name.c_str(), 0, type, (const BYTE *)value.c_str(), (DWORD)((value.size() + 1) * sizeof(wchar_t)));
        }
        RegCloseKey(k);
        if (w == ERROR_ACCESS_DENIED) return OpFail(adminMsg, WinErr((DWORD)w));
        if (w != ERROR_SUCCESS) return OpFail(L"Unable to change the variable.", WinErr((DWORD)w));
        DWORD_PTR res = 0;
        SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"Environment", SMTO_ABORTIFHUNG, 1000, &res);
    }
    return OpOk();
}

// ---------------------------------------------------------------------------------------------
// hosts file
// ---------------------------------------------------------------------------------------------
inline ws HostsPath() {
    ws root = EnvVar(L"SystemRoot"); if (root.empty()) root = L"C:\\Windows";
    return root + L"\\System32\\drivers\\etc\\hosts";
}

// reads like File.ReadAllText: BOM aware (UTF-8 / UTF-16 LE/BE), else UTF-8
inline ws HostsRead() {
    std::string b;
    if (!ReadFileBytes(HostsPath(), b)) return L"";
    if (b.size() >= 2 && (unsigned char)b[0] == 0xFF && (unsigned char)b[1] == 0xFE) {
        return ws((const wchar_t *)(b.data() + 2), (b.size() - 2) / 2);
    }
    if (b.size() >= 2 && (unsigned char)b[0] == 0xFE && (unsigned char)b[1] == 0xFF) {
        ws w; for (size_t i = 2; i + 1 < b.size(); i += 2) w += (wchar_t)(((unsigned char)b[i] << 8) | (unsigned char)b[i + 1]);
        return w;
    }
    if (b.size() >= 3 && (unsigned char)b[0] == 0xEF && (unsigned char)b[1] == 0xBB && (unsigned char)b[2] == 0xBF) b.erase(0, 3);
    return FromCp(b, CP_UTF8);
}

inline bool ValidIPv4Like(const ws &s) {          // IPAddress.TryParse accepts 1..4 dotted decimal parts
    std::vector<ws> parts; size_t i = 0;
    for (;;) { size_t j = s.find(L'.', i); parts.push_back(s.substr(i, j == ws::npos ? ws::npos : j - i)); if (j == ws::npos) break; i = j + 1; }
    if (parts.empty() || parts.size() > 4) return false;
    std::vector<unsigned long long> v;
    for (auto &p : parts) {
        if (p.empty() || p.size() > 10) return false;
        for (wchar_t c : p) if (c < L'0' || c > L'9') return false;
        v.push_back(wcstoull(p.c_str(), nullptr, 10));
    }
    size_t n = v.size();
    for (size_t k = 0; k + 1 < n; k++) if (v[k] > 255) return false;
    unsigned long long maxLast = (n == 1) ? 0xFFFFFFFFULL : (n == 2) ? 0xFFFFFFULL : (n == 3) ? 0xFFFFULL : 0xFFULL;
    return v[n - 1] <= maxLast;
}
inline bool ValidIPv6Like(ws s) {
    size_t pct = s.find(L'%'); if (pct != ws::npos) s.resize(pct);
    if (s.size() >= 2 && s.front() == L'[' && s.back() == L']') s = s.substr(1, s.size() - 2);
    if (s.find(L':') == ws::npos || s.empty()) return false;
    size_t dc = s.find(L"::");
    if (dc != ws::npos && s.find(L"::", dc + 1) != ws::npos) return false;
    int groups = 0; size_t i = 0; bool v4 = false;
    ws t = s;
    if (StartsWith(t, L"::")) t = t.substr(2); else if (!t.empty() && t[0] == L':') return false;
    while (!t.empty()) {
        size_t c = t.find(L':'); ws g = t.substr(0, c);
        if (g.empty()) { if (c == ws::npos) return false; t = t.substr(c + 1); continue; }   // part after "::" inside
        if (g.find(L'.') != ws::npos) { if (c != ws::npos || !ValidIPv4Like(g) || std::count(g.begin(), g.end(), L'.') != 3) return false; v4 = true; groups += 2; break; }
        if (g.size() > 4) return false;
        for (wchar_t ch : g) if (!iswxdigit(ch)) return false;
        groups++;
        if (c == ws::npos) break;
        t = t.substr(c + 1);
        if (t.empty()) { if (dc == ws::npos || !EndsWithI(s, L"::")) return false; }
    }
    (void)i; (void)v4;
    return dc != ws::npos ? groups < 8 : groups == 8;
}

inline ws HostsEntries() {
    JArr arr;
    for (const ws &rawLine : Lines(HostsRead())) {
        size_t a = rawLine.find_first_not_of(L"\r \t");
        if (a == ws::npos) continue;
        ws line = rawLine.substr(a, rawLine.find_last_not_of(L"\r \t") - a + 1);
        bool enabled = line[0] != L'#';
        ws content = line;
        if (!enabled) { size_t b = content.find_first_not_of(L"# "); content = b == ws::npos ? ws() : content.substr(b); }
        std::vector<ws> parts; size_t i = 0;
        while (i < content.size()) {
            while (i < content.size() && iswspace(content[i])) i++;
            size_t j = i; while (j < content.size() && !iswspace(content[j])) j++;
            if (j > i) parts.push_back(content.substr(i, j - i));
            i = j;
        }
        if (parts.size() < 2 || !(ValidIPv4Like(parts[0]) || ValidIPv6Like(parts[0]))) continue;
        JObj o; o.S(L"ipAddress", parts[0]).S(L"hostName", parts[1]).B(L"isEnabled", enabled);
        arr.Obj(o);
    }
    return arr.Str();
}

inline ws HostsSave(const ws &content) {
    ws path = HostsPath();
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        DWORD e = GetLastError();
        if (e == ERROR_ACCESS_DENIED) return OpFail(L"Editing the hosts file requires administrator access. Restart WinKit as Administrator.", WinErr(e));
        return OpFail(L"Unable to save the hosts file.", WinErr(e));
    }
    std::string u = ToUtf8(content);
    size_t off = 0; DWORD err = 0;
    while (off < u.size()) {
        DWORD n = 0, chunk = (DWORD)std::min<size_t>(u.size() - off, 1u << 20);
        if (!WriteFile(h, u.data() + off, chunk, &n, nullptr)) { err = GetLastError(); break; }
        off += n;
    }
    CloseHandle(h);
    if (err) return OpFail(L"Unable to save the hosts file.", WinErr(err));
    return OpOk();
}

// ---------------------------------------------------------------------------------------------
// system information
// ---------------------------------------------------------------------------------------------
inline ws UptimeText(unsigned long long ms) {
    unsigned long long s = ms / 1000, d = s / 86400, h = (s / 3600) % 24, m = (s / 60) % 60, sec = s % 60;
    if (d >= 1) return std::to_wstring(d) + L"d " + std::to_wstring(h) + L"h " + std::to_wstring(m) + L"m " + std::to_wstring(sec) + L"s";
    if (h > 0) return std::to_wstring(h) + L"h " + std::to_wstring(m) + L"m " + std::to_wstring(sec) + L"s";
    return std::to_wstring(m) + L"m " + std::to_wstring(sec) + L"s";
}

// Overall CPU load in percent, from deltas of GetSystemTimes between two calls (the first call samples over 250 ms).
inline double CpuUsagePercent() {
    static std::mutex m; static ULONGLONG pIdle = 0, pKernel = 0, pUser = 0; static bool have = false; static double last = 0;
    auto read = [](ULONGLONG &i, ULONGLONG &k, ULONGLONG &u) {
        FILETIME a, b, c; if (!GetSystemTimes(&a, &b, &c)) return false;
        i = FtU64(a); k = FtU64(b); u = FtU64(c); return true;
    };
    std::lock_guard<std::mutex> g(m);
    ULONGLONG i, k, u;
    if (!read(i, k, u)) return 0;
    if (!have) { pIdle = i; pKernel = k; pUser = u; have = true; Sleep(250); if (!read(i, k, u)) return 0; }
    ULONGLONG dIdle = i - pIdle, dTotal = (k - pKernel) + (u - pUser);      // kernel time already includes idle time
    if (dTotal < 500000) return last;                                       // sampled too soon after the previous call: keep the last value
    pIdle = i; pKernel = k; pUser = u;
    double busy = (double)(dTotal - std::min(dIdle, dTotal)) * 100.0 / (double)dTotal;
    return last = std::min(100.0, std::max(0.0, busy));
}

// First row of a WQL query; values as strings (uint32 CIM values arrive as VT_I4 and are read unsigned).
inline bool WmiFirst(const wchar_t *query, const std::vector<ws> &props, std::map<ws, ws> &out) {
    IWbemLocator *loc = nullptr; IWbemServices *svc = nullptr; IEnumWbemClassObject *en = nullptr; IWbemClassObject *obj = nullptr;
    bool ok = false;
    CoInitializeSecurity(nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE, nullptr);
    if (FAILED(CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (void **)&loc)) || !loc) return false;
    BSTR ns = SysAllocString(L"ROOT\\CIMV2");
    HRESULT hr = loc->ConnectServer(ns, nullptr, nullptr, nullptr, 0, nullptr, nullptr, &svc);
    SysFreeString(ns);
    if (SUCCEEDED(hr) && svc) {
        CoSetProxyBlanket(svc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
        BSTR wql = SysAllocString(L"WQL"), q = SysAllocString(query);
        hr = svc->ExecQuery(wql, q, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &en);
        SysFreeString(wql); SysFreeString(q);
        if (SUCCEEDED(hr) && en) {
            ULONG ret = 0;
            hr = en->Next(10000, 1, &obj, &ret);
            if (SUCCEEDED(hr) && ret == 1 && obj) {
                ok = true;
                for (auto &p : props) {
                    VARIANT v; VariantInit(&v);
                    if (SUCCEEDED(obj->Get(p.c_str(), 0, &v, nullptr, nullptr))) {
                        switch (v.vt) {
                        case VT_BSTR: if (v.bstrVal) out[p] = v.bstrVal; break;
                        case VT_I4: out[p] = std::to_wstring((unsigned long)(unsigned int)v.lVal); break;
                        case VT_UI4: out[p] = std::to_wstring((unsigned long)v.ulVal); break;
                        case VT_I2: out[p] = std::to_wstring((unsigned long)(unsigned short)v.iVal); break;
                        case VT_UI2: out[p] = std::to_wstring((unsigned long)v.uiVal); break;
                        default: break;
                        }
                    }
                    VariantClear(&v);
                }
                obj->Release();
            }
            en->Release();
        }
        svc->Release();
    }
    loc->Release();
    return ok;
}

inline ws SysInfo() {
    ComScope com;
    // --- CPU ---
    // The processor name never changes while we run, and CPU load comes from GetSystemTimes deltas: the dashboard polls this
    // every few seconds, so no WMI round trip per refresh.
    static const ws cpuName = []() -> ws {
        ws n;
        if (RegGetString(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", L"ProcessorNameString", n) && !Trim(n).empty()) return Trim(n);
        std::map<ws, ws> m;
        if (WmiFirst(L"SELECT Name FROM Win32_Processor", {L"Name"}, m) && m.count(L"Name") && !Trim(m[L"Name"]).empty()) return Trim(m[L"Name"]);
        return L"Unknown processor";
    }();
    const double cpuUsage = CpuUsagePercent();
    SYSTEM_INFO si; GetSystemInfo(&si);
    JObj cpu; cpu.S(L"name", cpuName).D(L"usagePercent", cpuUsage, 1).N(L"coreCount", si.dwNumberOfProcessors);
    // --- memory ---
    double totalGb = 0, usedGb = 0;
    { MEMORYSTATUSEX ms = {}; ms.dwLength = sizeof(ms);
      if (GlobalMemoryStatusEx(&ms)) { totalGb = (double)ms.ullTotalPhys / 1073741824.0; usedGb = (double)(ms.ullTotalPhys - ms.ullAvailPhys) / 1073741824.0; } }
    JObj mem; mem.D(L"totalGb", totalGb, 3).D(L"usedGb", usedGb, 3).D(L"usagePercent", totalGb <= 0 ? 0 : usedGb / totalGb * 100.0, 2);
    // --- GPU ---
    // cached after the first successful lookup (installed GPUs don't change while WinKit runs)
    static std::mutex gpuMutex; static bool gpuKnown = false; static ws gpuCache = L"Unknown"; static bool gpuCacheHasMem = false; static double gpuCacheMem = 0;
    {
        std::lock_guard<std::mutex> g(gpuMutex);
        if (!gpuKnown) {
            std::map<ws, ws> m;
            if (WmiFirst(L"SELECT Name, AdapterRAM FROM Win32_VideoController", {L"Name", L"AdapterRAM"}, m)) {
                gpuCache = m.count(L"Name") ? Trim(m[L"Name"]) : ws(L"Unknown GPU");
                if (m.count(L"AdapterRAM")) {
                    double bytes = (double)wcstoull(m[L"AdapterRAM"].c_str(), nullptr, 10);
                    if (bytes > 0) { gpuCacheHasMem = true; gpuCacheMem = std::floor(bytes / 1073741824.0 * 10.0 + 0.5) / 10.0; }
                }
                gpuKnown = true;
            }
        }
    }
    ws gpuName = gpuCache; bool gpuHasMem = gpuCacheHasMem; double gpuMem = gpuCacheMem;
    JObj gpu; gpu.S(L"name", gpuName); if (gpuHasMem) gpu.D(L"dedicatedMemoryGb", gpuMem, 1); else gpu.Null(L"dedicatedMemoryGb");
    // --- storage ---
    JArr stor;
    {
        UINT oldMode = SetErrorMode(SEM_FAILCRITICALERRORS);
        DWORD mask = GetLogicalDrives();
        for (int i = 0; i < 26; i++) {
            if (!(mask & (1u << i))) continue;
            wchar_t root[4] = {(wchar_t)(L'A' + i), L':', L'\\', 0};
            if (GetDriveTypeW(root) != DRIVE_FIXED) continue;
            ULARGE_INTEGER avail, total, freeB;
            if (!GetDiskFreeSpaceExW(root, &avail, &total, &freeB)) continue;      // !IsReady
            double t = (double)total.QuadPart / 1073741824.0, f = (double)avail.QuadPart / 1073741824.0, u = t - f;
            JObj o; o.S(L"name", ws(root, 2)).D(L"totalGb", t, 3).D(L"freeGb", f, 3).D(L"usedGb", u, 3).D(L"usagePercent", t <= 0 ? 0 : u / t * 100.0, 2);
            stor.Obj(o);
        }
        SetErrorMode(oldMode);
    }
    // --- network ---
    ws adapter = L"Not connected", ip; bool connected = false, hasIp = false;
    {
        ULONG sz = 32768; std::vector<BYTE> b; ULONG rc = ERROR_BUFFER_OVERFLOW;
        for (int t = 0; t < 4 && rc == ERROR_BUFFER_OVERFLOW; t++) {
            b.resize(sz);
            rc = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_INCLUDE_GATEWAYS, nullptr, (PIP_ADAPTER_ADDRESSES)b.data(), &sz);
        }
        if (rc == NO_ERROR) {
            // The adapter that actually carries traffic has a default gateway; virtual switches (Hyper-V, WSL, VPN taps)
            // often report a higher link speed but have none. Prefer a gateway, then the faster link.
            PIP_ADAPTER_ADDRESSES best = nullptr; bool bestGw = false;
            for (PIP_ADAPTER_ADDRESSES p = (PIP_ADAPTER_ADDRESSES)b.data(); p; p = p->Next) {
                if (p->OperStatus != IfOperStatusUp || p->IfType == IF_TYPE_SOFTWARE_LOOPBACK || p->IfType == IF_TYPE_TUNNEL) continue;
                bool gw = p->FirstGatewayAddress != nullptr;
                if (!best || (gw && !bestGw) || (gw == bestGw && p->TransmitLinkSpeed > best->TransmitLinkSpeed)) { best = p; bestGw = gw; }
            }
            if (best) {
                connected = true; adapter = best->FriendlyName ? best->FriendlyName : L"";
                for (PIP_ADAPTER_UNICAST_ADDRESS u = best->FirstUnicastAddress; u; u = u->Next) {
                    if (u->Address.lpSockaddr && u->Address.lpSockaddr->sa_family == AF_INET) {
                        const BYTE *a = (const BYTE *)&((sockaddr_in *)u->Address.lpSockaddr)->sin_addr;
                        wchar_t s[32]; swprintf(s, 32, L"%u.%u.%u.%u", a[0], a[1], a[2], a[3]); ip = s; hasIp = true; break;
                    }
                }
            }
        }
    }
    JObj net; net.S(L"adapterName", adapter).B(L"isConnected", connected).R(L"ipAddress", StrOrNull(hasIp, ip));
    // --- windows version (registry, same rules as C#) ---
    ws winVer;
    {
        ws prod = L"Windows", disp, build; bool hasDisp = false;
        HKEY k;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &k) == ERROR_SUCCESS) {
            ws t; if (KeyStr(k, L"ProductName", t)) prod = t;
            hasDisp = KeyStr(k, L"DisplayVersion", disp);
            KeyStr(k, L"CurrentBuildNumber", build);
            RegCloseKey(k);
        }
        wchar_t *endp = nullptr; long bn = build.empty() ? 0 : wcstol(build.c_str(), &endp, 10);
        if (!build.empty() && endp && *endp == 0 && bn >= 22000) {
            size_t p = prod.find(L"Windows 10"); while (p != ws::npos) { prod.replace(p, 10, L"Windows 11"); p = prod.find(L"Windows 10", p + 10); }
        }
        winVer = (!hasDisp || disp.empty()) ? prod + L" (Build " + build + L")" : prod + L" " + disp + L" (Build " + build + L")";
    }
    wchar_t mn[MAX_COMPUTERNAME_LENGTH + 2]; DWORD mnl = MAX_COMPUTERNAME_LENGTH + 1; ws machine;
    if (GetComputerNameW(mn, &mnl)) machine.assign(mn, mnl);
    SYSTEM_INFO nsi; GetNativeSystemInfo(&nsi);
    bool is64 = nsi.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 || nsi.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64 || nsi.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_IA64;
    unsigned long long up = GetTickCount64();

    JObj o;
    o.S(L"machineName", machine).S(L"architecture", is64 ? L"64-bit" : L"32-bit").S(L"windowsVersion", winVer)
        .U(L"uptimeMs", up).S(L"uptimeText", UptimeText(up)).B(L"isAdmin", IsAdmin())
        .R(L"cpu", cpu.Str()).R(L"memory", mem.Str()).R(L"gpu", gpu.Str()).R(L"storage", stor.Str()).R(L"network", net.Str());
    return o.Str();
}

} // namespace wksys

// =============================================================================================
inline bool Mod_System(const ws &cmd, const Args &a, ws &out) {
    using namespace wksys;
    if (cmd.size() < 4) return false;
    try {
        if (cmd == L"proc_list") { out = ProcessList(); return true; }
        if (cmd == L"proc_end") {
            if (a.find(L"pid") == a.end()) { out = Err(L"missing argument: pid"); return true; }
            out = ProcessEnd(ArgInt(a, L"pid", -1)); return true;
        }
        if (cmd == L"svc_list") { out = ServiceList(); return true; }
        if (cmd == L"svc_start" || cmd == L"svc_stop") {
            ws n = Arg(a, L"name");
            if (n.empty()) { out = Err(L"missing argument: name"); return true; }
            out = ServiceChange(n, cmd == L"svc_start"); return true;
        }
        if (cmd == L"startup_list") { out = StartupList(); return true; }
        if (cmd == L"startup_set") {
            if (a.find(L"scope") == a.end() || a.find(L"name") == a.end()) { out = Err(L"missing argument: scope/name"); return true; }
            out = StartupSet(Arg(a, L"scope"), Arg(a, L"name"), Arg(a, L"command"), a.find(L"command") != a.end(), ArgBool(a, L"enabled", false));
            return true;
        }
        if (cmd == L"apps_list") { out = AppsList(); return true; }
        if (cmd == L"apps_uninstall") {
            ws n = Arg(a, L"name");
            if (n.empty()) { out = Err(L"missing argument: name"); return true; }
            out = AppsUninstall(n); return true;
        }
        if (cmd == L"env_list") { out = EnvList(); return true; }
        if (cmd == L"env_set" || cmd == L"env_delete") {
            ws scope = Arg(a, L"scope", L"User");
            if (scope != L"User" && scope != L"Machine") { out = Err(L"scope must be User or Machine"); return true; }
            out = EnvWrite(scope, Arg(a, L"name"), Arg(a, L"value"), cmd == L"env_delete"); return true;
        }
        if (cmd == L"hosts_get") {
            JObj o; o.S(L"path", HostsPath()).S(L"content", HostsRead()).B(L"exists", FileExists(HostsPath()));
            out = o.Str(); return true;
        }
        if (cmd == L"hosts_entries") { out = HostsEntries(); return true; }
        if (cmd == L"hosts_save") {
            if (a.find(L"content") == a.end()) { out = Err(L"missing argument: content"); return true; }
            out = HostsSave(Arg(a, L"content")); return true;
        }
        if (cmd == L"sysinfo_get") { out = SysInfo(); return true; }
        if (cmd == L"sysinfo_uptime") {
            unsigned long long up = GetTickCount64();
            JObj o; o.U(L"uptimeMs", up).S(L"uptimeText", UptimeText(up)); out = o.Str(); return true;
        }
    } catch (const std::exception &e) {
        out = Err(FromCp(e.what(), CP_ACP)); return true;
    } catch (...) {
        out = Err(L"unexpected error"); return true;
    }
    return false;
}

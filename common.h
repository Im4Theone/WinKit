// common.h - shared helpers for every backend module. Header-only, no WebView2 dependency.
#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <cwctype>
#include <cstdio>
#include <cstdint>
#include <algorithm>

typedef std::wstring ws;
typedef std::map<ws, ws> Args;      // all arguments from JS arrive as strings (numbers/bools included: "1", "true")

// ---- module convention -------------------------------------------------------------------
//   inline bool Mod_Xxx(const ws &cmd, const Args &a, ws &out);
// Return false if `cmd` is not yours. Otherwise set `out` to a JSON VALUE (object, array, string,
// number, true/false/null) and return true. To report a failure set out = L"error:<message>".
// Every module runs on a worker thread, so blocking is fine. Never touch the UI from a module;
// use PushEvent() for progress.
// -------------------------------------------------------------------------------------------

// Defined in winkit.cpp. Sends {"event":name,"data":<jsonData>} to the page (thread-safe).
void PushEvent(const ws &name, const ws &jsonData);

inline ws Err(const ws &m) { return L"error:" + m; }
inline ws Num(unsigned long long v) { return std::to_wstring(v); }
inline ws SNum(long long v) { return std::to_wstring(v); }
inline ws Dbl(double v, int digits = 2) { wchar_t b[64]; swprintf(b, 64, L"%.*f", digits, v); return b; }
inline ws Bool(bool b) { return b ? L"true" : L"false"; }

inline ws JStr(const ws &s) {            // JSON string literal (escapes ", \, control chars)
    ws o = L"\"";
    for (wchar_t c : s) {
        switch (c) {
        case L'"': o += L"\\\""; break;
        case L'\\': o += L"\\\\"; break;
        case L'\n': o += L"\\n"; break;
        case L'\r': o += L"\\r"; break;
        case L'\t': o += L"\\t"; break;
        default:
            if (c < 0x20) { wchar_t b[8]; swprintf(b, 8, L"\\u%04x", (unsigned)c); o += b; }
            else o += c;
        }
    }
    return o + L"\"";
}

// Tiny JSON object/array builder:   JObj o; o.S(L"name", x).N(L"pid", 5).B(L"ok", true).R(L"list", arr.Str());
struct JObj {
    ws s = L"{"; bool first = true;
    JObj &K(const ws &k) { if (!first) s += L","; first = false; s += JStr(k) + L":"; return *this; }
    JObj &S(const ws &k, const ws &v) { K(k); s += JStr(v); return *this; }
    JObj &N(const ws &k, long long v) { K(k); s += SNum(v); return *this; }
    JObj &U(const ws &k, unsigned long long v) { K(k); s += Num(v); return *this; }
    JObj &D(const ws &k, double v, int digits = 2) { K(k); s += Dbl(v, digits); return *this; }
    JObj &B(const ws &k, bool v) { K(k); s += Bool(v); return *this; }
    JObj &Null(const ws &k) { K(k); s += L"null"; return *this; }
    JObj &R(const ws &k, const ws &rawJson) { K(k); s += rawJson; return *this; }   // raw JSON value
    ws Str() const { return s + L"}"; }
};
struct JArr {
    ws s = L"["; bool first = true;
    JArr &R(const ws &rawJson) { if (!first) s += L","; first = false; s += rawJson; return *this; }
    JArr &S(const ws &v) { return R(JStr(v)); }
    JArr &Obj(const JObj &o) { return R(o.Str()); }
    ws Str() const { return s + L"]"; }
};

// ---- args ---------------------------------------------------------------------------------
inline ws Arg(const Args &a, const wchar_t *k, const ws &def = L"") { auto i = a.find(k); return i == a.end() ? def : i->second; }
inline long long ArgInt(const Args &a, const wchar_t *k, long long def = 0) { auto i = a.find(k); return i == a.end() || i->second.empty() ? def : _wtoi64(i->second.c_str()); }
inline bool ArgBool(const Args &a, const wchar_t *k, bool def = false) { auto i = a.find(k); return i == a.end() ? def : (i->second == L"true" || i->second == L"1"); }

// ---- strings / encoding -------------------------------------------------------------------
inline ws FromCp(const std::string &s, UINT cp) {
    if (s.empty()) return ws();
    int n = MultiByteToWideChar(cp, 0, s.data(), (int)s.size(), nullptr, 0);
    ws w(n, 0); MultiByteToWideChar(cp, 0, s.data(), (int)s.size(), &w[0], n); return w;
}
inline std::string ToUtf8(const ws &w) {
    if (w.empty()) return std::string();
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0); WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], n, nullptr, nullptr); return s;
}
inline ws Lower(ws s) { for (auto &c : s) c = (wchar_t)towlower(c); return s; }
inline ws Trim(const ws &s) { size_t a = s.find_first_not_of(L" \t\r\n"); if (a == ws::npos) return L""; return s.substr(a, s.find_last_not_of(L" \t\r\n") - a + 1); }
inline std::vector<ws> Lines(const ws &s) {
    std::vector<ws> v; size_t i = 0;
    while (i <= s.size()) { size_t j = s.find(L'\n', i); if (j == ws::npos) j = s.size(); ws l = s.substr(i, j - i); if (!l.empty() && l.back() == L'\r') l.pop_back(); v.push_back(l); i = j + 1; }
    return v;
}
inline bool StartsWith(const ws &s, const ws &p) { return s.size() >= p.size() && s.compare(0, p.size(), p) == 0; }
inline bool ContainsI(const ws &s, const ws &p) { return Lower(s).find(Lower(p)) != ws::npos; }

// ---- environment / paths ------------------------------------------------------------------
inline ws EnvVar(const wchar_t *name) {
    wchar_t b[32768]; DWORD n = GetEnvironmentVariableW(name, b, 32768);
    return (n && n < 32768) ? ws(b) : ws();
}
inline ws ExeDir() { wchar_t p[MAX_PATH]; GetModuleFileNameW(nullptr, p, MAX_PATH); wchar_t *s = wcsrchr(p, L'\\'); if (s) *s = 0; return p; }
inline ws ExePath() { wchar_t p[MAX_PATH]; GetModuleFileNameW(nullptr, p, MAX_PATH); return p; }
inline ws WinDir() { wchar_t p[MAX_PATH]; GetWindowsDirectoryW(p, MAX_PATH); return p; }
inline ws SysDir() { wchar_t p[MAX_PATH]; GetSystemDirectoryW(p, MAX_PATH); return p; }
// %LOCALAPPDATA%\WinKit  (created on demand) - settings, logs, themes, downloads live here
inline ws AppDataDir() {
    ws l = EnvVar(L"LOCALAPPDATA"); ws d = l.empty() ? ExeDir() + L"\\data" : l + L"\\WinKit";
    CreateDirectoryW(d.c_str(), nullptr); return d;
}
inline bool FileExists(const ws &p) { DWORD a = GetFileAttributesW(p.c_str()); return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY); }
inline bool DirExists(const ws &p) { DWORD a = GetFileAttributesW(p.c_str()); return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY); }

inline bool IsAdmin() {
    BOOL r = FALSE; PSID sid = nullptr; SID_IDENTIFIER_AUTHORITY nt = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &sid)) { CheckTokenMembership(nullptr, sid, &r); FreeSid(sid); }
    return r != FALSE;
}

// ---- files --------------------------------------------------------------------------------
inline bool ReadFileBytes(const ws &path, std::string &out) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    out.clear(); char b[65536]; DWORD n;
    while (ReadFile(h, b, sizeof(b), &n, nullptr) && n) out.append(b, n);
    CloseHandle(h); return true;
}
inline ws ReadFileText(const ws &path) {      // UTF-8 (BOM tolerated) -> wide; "" if missing
    std::string s; if (!ReadFileBytes(path, s)) return L"";
    if (s.size() >= 3 && (unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF) s.erase(0, 3);
    return FromCp(s, CP_UTF8);
}
inline bool WriteFileText(const ws &path, const ws &text) {      // UTF-8, no BOM, replaces the file
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    std::string u = ToUtf8(text); DWORD n = 0; BOOL ok = u.empty() ? TRUE : WriteFile(h, u.data(), (DWORD)u.size(), &n, nullptr);
    CloseHandle(h); return ok && n == u.size();
}

// ---- processes ----------------------------------------------------------------------------
// Runs a program with no window, captures stdout+stderr (decoded with the OEM code page unless
// `utf8` is true). Returns the text; exit code via *exitCode (-1 if it could not start).
// `cmdline` is a full command line, e.g. L"ping.exe -n 4 1.1.1.1". Output is NOT passed through a shell.
inline ws RunCapture(const ws &cmdline, DWORD *exitCode = nullptr, DWORD timeoutMs = 60000, bool utf8 = false) {
    if (exitCode) *exitCode = (DWORD)-1;
    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
    HANDLE rd = nullptr, wr = nullptr;
    if (!CreatePipe(&rd, &wr, &sa, 0)) return L"";
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    // stdin = NUL (a GUI process has no usable stdin, and a child that reads it would otherwise hang)
    HANDLE nul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
    STARTUPINFOEXW six = {}; six.StartupInfo.cb = sizeof(six);
    STARTUPINFOW &si = six.StartupInfo;
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE; si.hStdOutput = wr; si.hStdError = wr; si.hStdInput = (nul != INVALID_HANDLE_VALUE) ? nul : nullptr;
    // Only hand the child the handles it needs. Without this, several RunCapture calls running at once on
    // different threads leak each other's pipe write ends into their children and EOF is delayed until those exit.
    HANDLE inherit[2] = { wr, nul }; DWORD inheritCount = (nul != INVALID_HANDLE_VALUE) ? 2 : 1;
    std::vector<char> attrBuf; LPPROC_THREAD_ATTRIBUTE_LIST attrs = nullptr; bool attrsOk = false;
    SIZE_T attrSize = 0; InitializeProcThreadAttributeList(nullptr, 1, 0, &attrSize);
    if (attrSize) {
        attrBuf.assign(attrSize, 0); attrs = (LPPROC_THREAD_ATTRIBUTE_LIST)attrBuf.data();
        if (InitializeProcThreadAttributeList(attrs, 1, 0, &attrSize)) {
            if (UpdateProcThreadAttribute(attrs, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherit, inheritCount * sizeof(HANDLE), nullptr, nullptr)) { six.lpAttributeList = attrs; attrsOk = true; }
            else DeleteProcThreadAttributeList(attrs);
        }
    }
    si.cb = attrsOk ? (DWORD)sizeof(STARTUPINFOEXW) : (DWORD)sizeof(STARTUPINFOW);
    PROCESS_INFORMATION pi = {};
    std::vector<wchar_t> buf(cmdline.begin(), cmdline.end()); buf.push_back(0);
    BOOL ok = CreateProcessW(nullptr, buf.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | (attrsOk ? EXTENDED_STARTUPINFO_PRESENT : 0), nullptr, nullptr, &si, &pi);
    if (attrsOk) DeleteProcThreadAttributeList(attrs);
    CloseHandle(wr);
    if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
    if (!ok) { CloseHandle(rd); return L""; }
    // watchdog: kill the child if it overruns, which also unblocks ReadFile
    struct Ctx { HANDLE p; DWORD ms; HANDLE done; } ctx = { pi.hProcess, timeoutMs, CreateEventW(nullptr, TRUE, FALSE, nullptr) };
    HANDLE wd = CreateThread(nullptr, 0, [](LPVOID v) -> DWORD { Ctx *c = (Ctx *)v; if (WaitForSingleObject(c->done, c->ms) == WAIT_TIMEOUT) TerminateProcess(c->p, 1); return 0; }, &ctx, 0, nullptr);
    std::string out; char tmp[8192]; DWORD got;
    while (ReadFile(rd, tmp, sizeof(tmp), &got, nullptr) && got) out.append(tmp, got);
    WaitForSingleObject(pi.hProcess, INFINITE);
    SetEvent(ctx.done); WaitForSingleObject(wd, INFINITE); CloseHandle(wd); CloseHandle(ctx.done);
    if (exitCode) GetExitCodeProcess(pi.hProcess, exitCode);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread); CloseHandle(rd);
    return FromCp(out, utf8 ? CP_UTF8 : GetOEMCP());
}
// PowerShell one-liner/script (no profile, hidden, output forced to UTF-8). Prefer native APIs
// where practical; use this for things only PowerShell/WMI exposes (Get-PhysicalDisk, Get-CimInstance ...).
inline ws RunPowerShell(const ws &script, DWORD *exitCode = nullptr, DWORD timeoutMs = 60000) {
    ws s = L"[Console]::OutputEncoding=[Text.Encoding]::UTF8;$ProgressPreference='SilentlyContinue';" + script;
    // -EncodedCommand avoids every quoting problem
    std::string u16((const char *)s.data(), s.size() * 2);
    static const char *T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string b64; size_t i = 0;
    for (; i + 2 < u16.size(); i += 3) { unsigned v = ((unsigned char)u16[i] << 16) | ((unsigned char)u16[i + 1] << 8) | (unsigned char)u16[i + 2]; b64 += T[v >> 18]; b64 += T[(v >> 12) & 63]; b64 += T[(v >> 6) & 63]; b64 += T[v & 63]; }
    if (i + 1 == u16.size()) { unsigned v = (unsigned char)u16[i] << 16; b64 += T[v >> 18]; b64 += T[(v >> 12) & 63]; b64 += "=="; }
    else if (i + 2 == u16.size()) { unsigned v = ((unsigned char)u16[i] << 16) | ((unsigned char)u16[i + 1] << 8); b64 += T[v >> 18]; b64 += T[(v >> 12) & 63]; b64 += T[(v >> 6) & 63]; b64 += "="; }
    return RunCapture(L"powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -EncodedCommand " + FromCp(b64, CP_UTF8), exitCode, timeoutMs, true);
}

// Host names / addresses only: letters, digits, . - : _  (we never use a shell, this is belt and braces)
inline bool SafeHost(const ws &h) {
    if (h.empty() || h.size() > 253) return false;
    for (wchar_t c : h) if (!(iswalnum(c) || c == L'.' || c == L'-' || c == L':' || c == L'_')) return false;
    return h[0] != L'-';
}

// ---- registry -----------------------------------------------------------------------------
inline bool RegGetString(HKEY root, const ws &sub, const ws &name, ws &out, REGSAM view = 0) {
    HKEY k; if (RegOpenKeyExW(root, sub.c_str(), 0, KEY_READ | view, &k) != ERROR_SUCCESS) return false;
    DWORD type = 0, sz = 0; bool ok = false;
    if (RegQueryValueExW(k, name.empty() ? nullptr : name.c_str(), nullptr, &type, nullptr, &sz) == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ)) {
        std::vector<wchar_t> b(sz / 2 + 2, 0);
        if (RegQueryValueExW(k, name.empty() ? nullptr : name.c_str(), nullptr, &type, (LPBYTE)b.data(), &sz) == ERROR_SUCCESS) {
            out = b.data();
            if (type == REG_EXPAND_SZ) { wchar_t e[4096]; DWORD n = ExpandEnvironmentStringsW(out.c_str(), e, 4096); if (n && n <= 4096) out = e; }
            ok = true;
        }
    }
    RegCloseKey(k); return ok;
}
inline bool RegGetDword(HKEY root, const ws &sub, const ws &name, DWORD &out, REGSAM view = 0) {
    HKEY k; if (RegOpenKeyExW(root, sub.c_str(), 0, KEY_READ | view, &k) != ERROR_SUCCESS) return false;
    DWORD type = 0, sz = sizeof(DWORD); bool ok = RegQueryValueExW(k, name.c_str(), nullptr, &type, (LPBYTE)&out, &sz) == ERROR_SUCCESS && type == REG_DWORD;
    RegCloseKey(k); return ok;
}
// enumerate sub-key names
inline std::vector<ws> RegSubKeys(HKEY root, const ws &sub, REGSAM view = 0) {
    std::vector<ws> v; HKEY k; if (RegOpenKeyExW(root, sub.c_str(), 0, KEY_READ | view, &k) != ERROR_SUCCESS) return v;
    for (DWORD i = 0;; i++) { wchar_t n[512]; DWORD l = 512; if (RegEnumKeyExW(k, i, n, &l, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break; v.push_back(n); }
    RegCloseKey(k); return v;
}

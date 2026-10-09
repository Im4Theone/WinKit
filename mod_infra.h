// mod_infra.h - native half of WinKit.Core / WinKit.Infrastructure / WinKit.Updater.
// Generic JSON file store, elevation, auto-start, update service, error reporting, shell helpers,
// window-material bridge and the portable-update helper (Updater_Main).
// See API_infra.md for the command reference. Pure logic (settings, profile, activity, themes,
// notifications, formatting) lives in js/core.js.
#pragma once
#include "common.h"
#include <shellapi.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <commdlg.h>

#ifdef _MSC_VER
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "advapi32.lib")
#endif

#ifndef WINKIT_VERSION
#define WINKIT_VERSION L"2.0.0"            // normally passed by build.bat from version.txt
#endif
#ifndef WINKIT_EXE_NAME
#define WINKIT_EXE_NAME L"WinKit.exe"      // file name the updater relaunches inside the install dir
#endif
#define WINKIT_UPDATER_SWITCH L"--apply-update"

// ---- implemented by the host (winkit.cpp). All may be called from a worker thread, so they must
// marshal to the UI thread themselves (PostMessage etc.).
void HostQuit();                                         // close the main window / end the process (thread-safe, asynchronous)
void HostSetMaterial(const ws &mode);                    // "dark" | "light" [",mica"|",acrylic"|",tabbed"|",none"]
void HostSetTitleBarColors(bool dark, unsigned caption, unsigned text, unsigned border);   // COLORREF 0x00BBGGRR, 0xFFFFFFFF = default

namespace wkinfra {

// ---- small utilities --------------------------------------------------------------------------
struct Lock {
    CRITICAL_SECTION cs;
    Lock() { InitializeCriticalSection(&cs); }
};
struct Guard {
    Lock &l;
    explicit Guard(Lock &x) : l(x) { EnterCriticalSection(&l.cs); }
    ~Guard() { LeaveCriticalSection(&l.cs); }
};
inline Lock &StoreLock() { static Lock *l = new Lock(); return *l; }

inline ws Pad(unsigned v, int w) { wchar_t b[16]; swprintf(b, 16, L"%0*u", w, v); return b; }

// ISO-8601 "O" style: 2026-10-09T15:44:00.1234567+00:00
inline ws IsoFromFileTime(const FILETIME &ft, bool utc) {
    ULARGE_INTEGER u; u.LowPart = ft.dwLowDateTime; u.HighPart = ft.dwHighDateTime;
    long long offMin = 0; FILETIME shown = ft;
    if (!utc) {
        FILETIME lft; if (FileTimeToLocalFileTime(&ft, &lft)) {
            ULARGE_INTEGER l; l.LowPart = lft.dwLowDateTime; l.HighPart = lft.dwHighDateTime;
            offMin = ((long long)l.QuadPart - (long long)u.QuadPart) / 600000000LL; shown = lft;
        }
    }
    ULARGE_INTEGER s; s.LowPart = shown.dwLowDateTime; s.HighPart = shown.dwHighDateTime;
    SYSTEMTIME st = {}; FileTimeToSystemTime(&shown, &st);
    unsigned frac = (unsigned)(s.QuadPart % 10000000ULL);
    ws o = Pad(st.wYear, 4) + L"-" + Pad(st.wMonth, 2) + L"-" + Pad(st.wDay, 2) + L"T" + Pad(st.wHour, 2) + L":" + Pad(st.wMinute, 2) + L":" + Pad(st.wSecond, 2) + L"." + Pad(frac, 7);
    long long am = offMin < 0 ? -offMin : offMin;
    o += (offMin < 0 ? L"-" : L"+") + Pad((unsigned)(am / 60), 2) + L":" + Pad((unsigned)(am % 60), 2);
    return o;
}
inline ws IsoNow(bool utc) { FILETIME ft; GetSystemTimeAsFileTime(&ft); return IsoFromFileTime(ft, utc); }
inline ws StampUtc() {   // yyyyMMddHHmmss
    SYSTEMTIME st; GetSystemTime(&st);
    return Pad(st.wYear, 4) + Pad(st.wMonth, 2) + Pad(st.wDay, 2) + Pad(st.wHour, 2) + Pad(st.wMinute, 2) + Pad(st.wSecond, 2);
}

inline bool RandBytes(unsigned char *p, ULONG n) { return BCryptGenRandom(nullptr, p, n, BCRYPT_USE_SYSTEM_PREFERRED_RNG) >= 0; }
inline ws RandHex(int bytes) {
    unsigned char b[32]; if (bytes > 32) bytes = 32;
    if (!RandBytes(b, (ULONG)bytes)) { for (int i = 0; i < bytes; i++) b[i] = (unsigned char)(GetTickCount() >> (i % 8)) ^ (unsigned char)i; }
    ws o; wchar_t t[4]; for (int i = 0; i < bytes; i++) { swprintf(t, 4, L"%02X", b[i]); o += t; } return o;
}

inline ws LogsDir() { ws d = AppDataDir() + L"\\Logs"; CreateDirectoryW(d.c_str(), nullptr); return d; }

inline bool AppendFileText(const ws &path, const ws &text) {
    HANDLE h = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    std::string u = ToUtf8(text); DWORD n = 0; BOOL ok = u.empty() ? TRUE : WriteFile(h, u.data(), (DWORD)u.size(), &n, nullptr);
    CloseHandle(h); return ok != FALSE;
}
// Same layout as the C# crash.log entries.
inline void CrashLog(const ws &message, const ws &tag = L"") {
    ws line = IsoNow(false) + L"\r\n" + (tag.empty() ? L"" : tag + L" ") + message + L"\r\n" + ws(40, L'-') + L"\r\n";
    AppendFileText(LogsDir() + L"\\crash.log", line);
}

inline bool CreateDirRecursive(const ws &path) {
    if (path.empty()) return false;
    if (DirExists(path)) return true;
    for (size_t i = 3; i < path.size(); i++)
        if (path[i] == L'\\' || path[i] == L'/') { ws p = path.substr(0, i); if (!DirExists(p)) CreateDirectoryW(p.c_str(), nullptr); }
    if (!DirExists(path)) CreateDirectoryW(path.c_str(), nullptr);
    return DirExists(path);
}
inline void DeleteDirRecursive(const ws &dir) {
    WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            ws n = fd.cFileName; if (n == L"." || n == L"..") continue;
            ws p = dir + L"\\" + n;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) RemoveDirectoryW(p.c_str()); else DeleteDirRecursive(p);
            } else { SetFileAttributesW(p.c_str(), FILE_ATTRIBUTE_NORMAL); DeleteFileW(p.c_str()); }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryW(dir.c_str());
}
inline ws TempDir() { wchar_t b[MAX_PATH + 2]; DWORD n = GetTempPathW(MAX_PATH + 1, b); ws t(b, n); while (!t.empty() && t.back() == L'\\') t.pop_back(); return t; }
inline ws LastErrText(DWORD code) {
    wchar_t buf[512] = {0};
    DWORD n = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code, 0, buf, 512, nullptr);
    return n ? Trim(buf) : L"error " + Num(code);
}

// ---- minimal JSON DOM (parse / validate / read) --------------------------------------------------
struct JV {
    enum T { Null, Bool, Num, Str, Arr, Obj } t = Null;
    bool b = false; double n = 0; ws s;
    std::vector<JV> a;                       // array items, or object values
    std::vector<ws> keys;                    // object keys (parallel to `a`)
    const JV *get(const ws &k, bool ci = true) const {
        if (t != Obj) return nullptr;
        for (size_t i = 0; i < keys.size(); i++) if (ci ? Lower(keys[i]) == Lower(k) : keys[i] == k) return &a[i];
        return nullptr;
    }
    bool isStr() const { return t == Str; }
};
struct JParser {
    const ws &s; size_t i = 0; int depth = 0;
    explicit JParser(const ws &x) : s(x) {}
    void ws_() { while (i < s.size() && (s[i] == L' ' || s[i] == L'\t' || s[i] == L'\r' || s[i] == L'\n')) i++; }
    bool lit(const wchar_t *w) { size_t n = wcslen(w); if (s.compare(i, n, w) == 0) { i += n; return true; } return false; }
    bool hex4(unsigned &v) {
        if (i + 4 > s.size()) return false;
        v = 0;
        for (int k = 0; k < 4; k++) { wchar_t c = s[i + k]; v <<= 4; if (c >= L'0' && c <= L'9') v |= c - L'0'; else if (c >= L'a' && c <= L'f') v |= c - L'a' + 10; else if (c >= L'A' && c <= L'F') v |= c - L'A' + 10; else return false; }
        i += 4; return true;
    }
    bool str(ws &o) {
        if (i >= s.size() || s[i] != L'"') return false;
        i++; o.clear();
        while (i < s.size()) {
            wchar_t c = s[i++];
            if (c == L'"') return true;
            if (c < 0x20) return false;
            if (c != L'\\') { o += c; continue; }
            if (i >= s.size()) return false;
            wchar_t e = s[i++];
            switch (e) {
            case L'"': o += L'"'; break; case L'\\': o += L'\\'; break; case L'/': o += L'/'; break;
            case L'b': o += L'\b'; break; case L'f': o += L'\f'; break; case L'n': o += L'\n'; break;
            case L'r': o += L'\r'; break; case L't': o += L'\t'; break;
            case L'u': { unsigned v; if (!hex4(v)) return false; o += (wchar_t)v; break; }
            default: return false;
            }
        }
        return false;
    }
    bool num(double &v) {
        size_t st = i;
        if (i < s.size() && s[i] == L'-') i++;
        if (i >= s.size()) return false;
        if (s[i] == L'0') i++;
        else if (s[i] >= L'1' && s[i] <= L'9') { while (i < s.size() && iswdigit(s[i])) i++; }
        else return false;
        if (i < s.size() && s[i] == L'.') { i++; size_t d = i; while (i < s.size() && iswdigit(s[i])) i++; if (d == i) return false; }
        if (i < s.size() && (s[i] == L'e' || s[i] == L'E')) {
            i++; if (i < s.size() && (s[i] == L'+' || s[i] == L'-')) i++;
            size_t d = i; while (i < s.size() && iswdigit(s[i])) i++; if (d == i) return false;
        }
        v = wcstod(s.substr(st, i - st).c_str(), nullptr); return true;
    }
    bool value(JV &o) {
        ws_(); if (i >= s.size()) return false;
        if (++depth > 128) return false;
        bool ok = false; wchar_t c = s[i];
        if (c == L'{') {
            i++; o.t = JV::Obj; ws_();
            if (i < s.size() && s[i] == L'}') { i++; ok = true; }
            else for (;;) {
                ws_(); ws k; if (!str(k)) break; ws_();
                if (i >= s.size() || s[i] != L':') break;
                i++;
                JV v; if (!value(v)) break;
                o.keys.push_back(k); o.a.push_back(std::move(v)); ws_();
                if (i < s.size() && s[i] == L',') { i++; continue; }
                if (i < s.size() && s[i] == L'}') { i++; ok = true; }
                break;
            }
        } else if (c == L'[') {
            i++; o.t = JV::Arr; ws_();
            if (i < s.size() && s[i] == L']') { i++; ok = true; }
            else for (;;) {
                JV v; if (!value(v)) break; o.a.push_back(std::move(v)); ws_();
                if (i < s.size() && s[i] == L',') { i++; continue; }
                if (i < s.size() && s[i] == L']') { i++; ok = true; }
                break;
            }
        } else if (c == L'"') { o.t = JV::Str; ok = str(o.s); }
        else if (c == L't') { o.t = JV::Bool; o.b = true; ok = lit(L"true"); }
        else if (c == L'f') { o.t = JV::Bool; o.b = false; ok = lit(L"false"); }
        else if (c == L'n') { o.t = JV::Null; ok = lit(L"null"); }
        else { o.t = JV::Num; ok = num(o.n); }
        depth--; return ok;
    }
};
inline bool JParse(const ws &text, JV &out) {
    JParser p(text); JV v; if (!p.value(v)) return false;
    p.ws_(); if (p.i != text.size()) return false;
    out = std::move(v); return true;
}
inline bool JIsValid(const ws &text) { JV v; return JParse(text, v); }
inline bool JGetStr(const JV &o, const wchar_t *k, ws &out) { const JV *v = o.get(k); if (v && v->t == JV::Str) { out = v->s; return true; } return false; }
inline bool JGetBool(const JV &o, const wchar_t *k, bool &out) { const JV *v = o.get(k); if (v && v->t == JV::Bool) { out = v->b; return true; } return false; }
inline ws JStrOrNull(const ws &s, bool has) { return has ? JStr(s) : ws(L"null"); }

// ---- JSON file store ---------------------------------------------------------------------------------
inline bool ReservedDeviceName(const ws &seg) {
    ws b = Lower(seg); size_t dot = b.find(L'.'); if (dot != ws::npos) b = b.substr(0, dot);
    while (!b.empty() && b.back() == L' ') b.pop_back();
    if (b == L"con" || b == L"prn" || b == L"aux" || b == L"nul") return true;
    if (b.size() == 4 && (StartsWith(b, L"com") || StartsWith(b, L"lpt")) && b[3] >= L'1' && b[3] <= L'9') return true;
    return false;
}
// "settings" -> <data>\settings.json ; "Themes/Midnight Copy" -> <data>\Themes\Midnight Copy.json
// `isDir`: the name designates a directory (may be empty = data root).
inline bool StoreResolve(const ws &name, bool isDir, ws &fullPath, ws &err) {
    std::vector<ws> segs; ws cur;
    for (wchar_t c : name) { if (c == L'/') { segs.push_back(cur); cur.clear(); } else cur += c; }
    segs.push_back(cur);
    if (isDir && name.empty()) segs.clear();
    if (segs.size() > 3) { err = L"invalid store name"; return false; }
    ws p = AppDataDir();
    for (size_t k = 0; k < segs.size(); k++) {
        ws g = segs[k];
        if (g.empty() || g == L"." || g == L".." || g.size() > 100) { err = L"invalid store name"; return false; }
        if (g.back() == L' ' || g.back() == L'.') { err = L"invalid store name"; return false; }
        for (wchar_t c : g) if (c < 0x20 || wcschr(L"<>:\"\\|?*", c)) { err = L"invalid store name"; return false; }
        if (ReservedDeviceName(g)) { err = L"invalid store name"; return false; }
        bool last = (k + 1 == segs.size());
        p += L"\\" + g + ((last && !isDir) ? L".json" : L"");
    }
    fullPath = p; return true;
}
inline void QuarantineCorrupt(const ws &path, const ws &cause) {
    ws q = path + L".corrupt-" + StampUtc() + L".json";
    if (MoveFileExW(path.c_str(), q.c_str(), MOVEFILE_REPLACE_EXISTING))
        CrashLog(L"Recovered from unreadable file: " + path + L"\r\nQuarantined to: " + q + L"\r\n" + cause);
}
// Returns the raw JSON text of the file, or "null" when missing / unreadable-as-JSON (corrupt files are moved aside).
inline bool StoreReadText(const ws &name, ws &json, ws &err) {
    ws path; if (!StoreResolve(name, false, path, err)) return false;
    Guard g(StoreLock());
    if (!FileExists(path)) { json = L"null"; return true; }
    std::string bytes;
    if (!ReadFileBytes(path, bytes)) { err = L"couldn't read " + name; return false; }
    if (bytes.size() >= 3 && (unsigned char)bytes[0] == 0xEF && (unsigned char)bytes[1] == 0xBB && (unsigned char)bytes[2] == 0xBF) bytes.erase(0, 3);
    ws text = FromCp(bytes, CP_UTF8);
    if (!JIsValid(text)) { QuarantineCorrupt(path, L"The file is not valid JSON."); json = L"null"; return true; }
    json = Trim(text); return true;
}
inline bool StoreWriteText(const ws &name, const ws &json, ws &err) {
    ws path; if (!StoreResolve(name, false, path, err)) return false;
    if (json.size() > 8u * 1024 * 1024) { err = L"document too large"; return false; }
    if (!JIsValid(json)) { err = L"not valid JSON"; return false; }
    Guard g(StoreLock());
    size_t cut = path.find_last_of(L'\\'); if (cut != ws::npos) CreateDirRecursive(path.substr(0, cut));
    ws tmp = path + L"." + RandHex(16) + L".tmp";
    HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) { err = L"couldn't write " + name + L" (" + LastErrText(GetLastError()) + L")"; return false; }
    std::string u = ToUtf8(json); DWORD n = 0;
    BOOL ok = u.empty() ? TRUE : WriteFile(h, u.data(), (DWORD)u.size(), &n, nullptr);
    if (ok) ok = FlushFileBuffers(h);
    CloseHandle(h);
    if (!ok || n != u.size()) { DeleteFileW(tmp.c_str()); err = L"couldn't write " + name; return false; }
    // atomic swap: destination is either the old or the new content, never partial
    bool done;
    if (FileExists(path)) done = ReplaceFileW(path.c_str(), tmp.c_str(), nullptr, 0, nullptr, nullptr) != FALSE
                              || MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING) != FALSE;
    else done = MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING) != FALSE;
    DWORD le = GetLastError();
    if (!done) { DeleteFileW(tmp.c_str()); err = L"couldn't write " + name + L" (" + LastErrText(le) + L")"; return false; }
    return true;
}
inline bool StoreDelete(const ws &name, ws &err) {
    ws path; if (!StoreResolve(name, false, path, err)) return false;
    Guard g(StoreLock());
    if (!FileExists(path)) return true;
    if (!DeleteFileW(path.c_str())) { err = L"couldn't delete " + name + L" (" + LastErrText(GetLastError()) + L")"; return false; }
    return true;
}
inline bool StoreList(const ws &dir, ws &outJson, ws &err) {
    ws path; if (!StoreResolve(dir, true, path, err)) return false;
    Guard g(StoreLock());
    std::vector<ws> names;
    WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW((path + L"\\*.json").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            ws n = fd.cFileName; if (n.size() <= 5 || Lower(n.substr(n.size() - 5)) != L".json") continue;
            if (Lower(n).find(L".corrupt-") != ws::npos) continue;
            names.push_back(n.substr(0, n.size() - 5));
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    std::sort(names.begin(), names.end(), [](const ws &x, const ws &y) { return Lower(x) < Lower(y); });
    JArr a; for (auto &n : names) a.S(n);
    outJson = a.Str(); return true;
}
// Used by the host at startup (the C# app reads only the setting; --start-minimized is written to the Run key but not read).
inline bool StartMinimizedSetting() {
    ws json, err; if (!StoreReadText(L"settings", json, err)) return false;
    JV v; if (!JParse(json, v)) return false;
    bool b = false; return JGetBool(v, L"StartMinimized", b) && b;
}

// ---- SHA-256 (BCrypt) ---------------------------------------------------------------------------------
inline bool Sha256File(const ws &path, ws &hexUpper) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    BCRYPT_ALG_HANDLE alg = nullptr; BCRYPT_HASH_HANDLE hash = nullptr; bool ok = false;
    DWORD objLen = 0, got = 0;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0
        && BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&objLen, sizeof(objLen), &got, 0) >= 0) {
        std::vector<unsigned char> obj(objLen);
        if (BCryptCreateHash(alg, &hash, obj.data(), objLen, nullptr, 0, 0) >= 0) {
            std::vector<unsigned char> buf(1 << 20); DWORD n = 0; ok = true;
            while (ReadFile(f, buf.data(), (DWORD)buf.size(), &n, nullptr) && n) {
                if (BCryptHashData(hash, buf.data(), n, 0) < 0) { ok = false; break; }
            }
            unsigned char d[32];
            if (ok && BCryptFinishHash(hash, d, 32, 0) >= 0) {
                hexUpper.clear(); wchar_t t[4];
                for (int i = 0; i < 32; i++) { swprintf(t, 4, L"%02X", d[i]); hexUpper += t; }
            } else ok = false;
            BCryptDestroyHash(hash);
        }
    }
    if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    CloseHandle(f); return ok;
}

// ---- WinHTTP -------------------------------------------------------------------------------------------------
struct HttpResult { bool ok = false; DWORD status = 0; ws reason; bool timedOut = false; ws error; std::string body; };
typedef std::function<void(unsigned long long, unsigned long long)> HttpProgress;
struct WinHttpH { HINTERNET h = nullptr; ~WinHttpH() { if (h) WinHttpCloseHandle(h); } };

inline ws WinHttpErrText(DWORD code) {
    wchar_t buf[512] = {0}; HMODULE m = GetModuleHandleW(L"winhttp.dll");
    DWORD n = FormatMessageW((m ? FORMAT_MESSAGE_FROM_HMODULE : 0) | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, m, code, 0, buf, 512, nullptr);
    return n ? Trim(buf) : L"error " + Num(code);
}
inline ws DefaultReason(DWORD c) {
    switch (c) {
    case 400: return L"Bad Request"; case 401: return L"Unauthorized"; case 403: return L"Forbidden"; case 404: return L"Not Found";
    case 408: return L"Request Timeout"; case 429: return L"Too Many Requests"; case 500: return L"Internal Server Error";
    case 502: return L"Bad Gateway"; case 503: return L"Service Unavailable"; case 504: return L"Gateway Timeout";
    default: return L"";
    }
}
// GET/POST. Body goes to `outFile` when given (downloads) else into r.body (capped at 16 MB).
inline void HttpDo(const ws &method, const ws &url, const ws &headers, const std::string &post, HttpResult &r,
                   HANDLE outFile = nullptr, const HttpProgress &prog = nullptr) {
    r = HttpResult();
    wchar_t host[256] = {0}, path[4096] = {0}, extra[4096] = {0};
    URL_COMPONENTSW uc = {}; uc.dwStructSize = sizeof(uc);
    uc.lpszHostName = host; uc.dwHostNameLength = 256; uc.lpszUrlPath = path; uc.dwUrlPathLength = 4096;
    uc.lpszExtraInfo = extra; uc.dwExtraInfoLength = 4096;
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc) || (uc.nScheme != INTERNET_SCHEME_HTTPS && uc.nScheme != INTERNET_SCHEME_HTTP)) { r.error = L"The address is not valid."; return; }
    bool secure = uc.nScheme == INTERNET_SCHEME_HTTPS;
    ws ua = ws(L"WinKit/") + WINKIT_VERSION;
    WinHttpH ses, con, req;
#ifdef WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY
    ses.h = WinHttpOpen(ua.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
#else
    ses.h = WinHttpOpen(ua.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
#endif
    if (!ses.h) { r.error = WinHttpErrText(GetLastError()); return; }
    WinHttpSetTimeouts(ses.h, 30000, 30000, 60000, 60000);
#ifdef WINHTTP_OPTION_DECOMPRESSION
    DWORD dec = WINHTTP_DECOMPRESSION_FLAG_ALL; WinHttpSetOption(ses.h, WINHTTP_OPTION_DECOMPRESSION, &dec, sizeof(dec));
#endif
    con.h = WinHttpConnect(ses.h, host, uc.nPort, 0);
    if (!con.h) { r.error = WinHttpErrText(GetLastError()); return; }
    ws obj = ws(path) + ws(extra); if (obj.empty()) obj = L"/";
    req.h = WinHttpOpenRequest(con.h, method.c_str(), obj.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, secure ? WINHTTP_FLAG_SECURE : 0);
    if (!req.h) { r.error = WinHttpErrText(GetLastError()); return; }
    DWORD fail = 0;
    #define WK_HTTP_FAIL() do { fail = GetLastError(); r.timedOut = (fail == ERROR_WINHTTP_TIMEOUT); r.error = WinHttpErrText(fail); return; } while (0)
    BOOL sent = WinHttpSendRequest(req.h, headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(), headers.empty() ? 0 : (DWORD)-1,
                                   post.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)post.data(), (DWORD)post.size(), (DWORD)post.size(), 0);
    if (!sent) WK_HTTP_FAIL();
    if (!WinHttpReceiveResponse(req.h, nullptr)) WK_HTTP_FAIL();
    DWORD code = 0, sz = sizeof(code);
    WinHttpQueryHeaders(req.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &code, &sz, WINHTTP_NO_HEADER_INDEX);
    r.status = code;
    wchar_t rb[256] = {0}; sz = sizeof(rb);
    if (WinHttpQueryHeaders(req.h, WINHTTP_QUERY_STATUS_TEXT, WINHTTP_HEADER_NAME_BY_INDEX, rb, &sz, WINHTTP_NO_HEADER_INDEX)) r.reason = rb;
    if (r.reason.empty()) r.reason = DefaultReason(code);
    unsigned long long total = 0; wchar_t cl[64] = {0}; sz = sizeof(cl);
    if (WinHttpQueryHeaders(req.h, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX, cl, &sz, WINHTTP_NO_HEADER_INDEX)) total = _wcstoui64(cl, nullptr, 10);
    bool success = code >= 200 && code < 300;
    // only stream the body into the file when the status is a success; error bodies are read into memory (small)
    bool toFile = outFile && success;
    std::vector<char> buf(65536); unsigned long long got = 0; ULONGLONG lastTick = 0;
    for (;;) {
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(req.h, &avail)) WK_HTTP_FAIL();
        if (!avail) break;
        if (avail > buf.size()) avail = (DWORD)buf.size();
        DWORD n = 0;
        if (!WinHttpReadData(req.h, buf.data(), avail, &n)) WK_HTTP_FAIL();
        if (!n) break;
        got += n;
        if (toFile) {
            DWORD w = 0; if (!WriteFile(outFile, buf.data(), n, &w, nullptr) || w != n) { r.error = L"Couldn't write the file (" + LastErrText(GetLastError()) + L")."; return; }
        } else if (r.body.size() < 16u * 1024 * 1024) r.body.append(buf.data(), n);
        if (prog && toFile) { ULONGLONG t = GetTickCount64(); if (t - lastTick >= 120) { lastTick = t; prog(got, total); } }
    }
    if (prog && toFile) prog(got, total ? total : got);
    r.ok = true;
    #undef WK_HTTP_FAIL
}
inline ws HttpStatusText(const HttpResult &r) { return Num(r.status) + (r.reason.empty() ? L"" : L" " + r.reason); }

// ---- version / app info -----------------------------------------------------------------------------------------
inline bool ParseVersion(const ws &t, int v[4], int &count) {
    count = 0; ws cur; v[0] = v[1] = v[2] = v[3] = -1;
    for (size_t i = 0; i <= t.size(); i++) {
        if (i == t.size() || t[i] == L'.') {
            if (cur.empty() || cur.size() > 9 || count >= 4) return false;
            v[count++] = _wtoi(cur.c_str()); cur.clear();
        } else if (t[i] >= L'0' && t[i] <= L'9') cur += t[i]; else return false;
    }
    return count >= 2;
}
inline int CompareVersion(const int a[4], const int b[4]) { for (int i = 0; i < 4; i++) { if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1; } return 0; }
inline void CurrentVersion(int v[4]) { int c; ParseVersion(WINKIT_VERSION, v, c); for (int i = 0; i < 4; i++) if (v[i] < 0) v[i] = 0; }

inline ws ArchName() {
#if defined(_M_ARM64) || defined(__aarch64__)
    return L"Arm64";
#elif defined(_M_ARM) || defined(__arm__)
    return L"Arm";
#elif defined(_M_X64) || defined(__x86_64__)
    return L"X64";
#else
    return L"X86";
#endif
}
inline ws WindowsVersionText() {
    const wchar_t *k = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
    ws product, disp, build;
    bool okP = RegGetString(HKEY_LOCAL_MACHINE, k, L"ProductName", product, KEY_WOW64_64KEY);
    bool okD = RegGetString(HKEY_LOCAL_MACHINE, k, L"DisplayVersion", disp, KEY_WOW64_64KEY);
    RegGetString(HKEY_LOCAL_MACHINE, k, L"CurrentBuildNumber", build, KEY_WOW64_64KEY);
    if (!okP) product = L"Windows";
    int b = build.empty() ? 0 : _wtoi(build.c_str());
    if (b >= 22000) { size_t p = product.find(L"Windows 10"); if (p != ws::npos) product.replace(p, 10, L"Windows 11"); }
    return (!okD || disp.empty()) ? product + L" (Build " + build + L")" : product + L" " + disp + L" (Build " + build + L")";
}
inline bool IsInstalledLocation() {
    ws base = Lower(ExeDir() + L"\\");
    ws pf = Lower(EnvVar(L"ProgramFiles")), pfx = Lower(EnvVar(L"ProgramFiles(x86)"));
    if (!pf.empty() && pf.back() != L'\\') pf += L'\\';           // "C:\Program Files\" must not match "C:\Program Files Portable\"
    if (!pfx.empty() && pfx.back() != L'\\') pfx += L'\\';
    return (!pf.empty() && StartsWith(base, pf)) || (!pfx.empty() && StartsWith(base, pfx));
}
inline bool SystemUsesLightTheme() {
    DWORD v = 0;
    return RegGetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"AppsUseLightTheme", v) && v == 1;
}

// ---- elevation ---------------------------------------------------------------------------------------------------------
// Returns true when the elevated process was started. false = UAC declined / failed.
inline bool RelaunchElevated(const ws &arguments) {
    ws exe = ExePath(); if (exe.empty()) return false;
    SHELLEXECUTEINFOW sei = {}; sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_NOASYNC;
    sei.lpVerb = L"runas"; sei.lpFile = exe.c_str();
    sei.lpParameters = arguments.empty() ? nullptr : arguments.c_str();
    sei.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&sei) != FALSE;
}

// ---- auto-start (HKCU Run key, value "WinKit") ---------------------------------------------------------------------------
static const wchar_t *const kRunKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const wchar_t *const kRunValue = L"WinKit";
inline bool AutoStartEnabled() { ws v; return RegGetString(HKEY_CURRENT_USER, kRunKey, kRunValue, v); }
inline bool AutoStartSet(bool enabled, ws &err) {
    HKEY k = nullptr;
    LONG rc = RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr);
    if (rc != ERROR_SUCCESS) { err = LastErrText((DWORD)rc); return false; }
    bool ok = true;
    if (enabled) {
        ws exe = ExePath();
        if (!exe.empty()) {
            ws val = L"\"" + exe + L"\" --start-minimized";
            rc = RegSetValueExW(k, kRunValue, 0, REG_SZ, (const BYTE *)val.c_str(), (DWORD)((val.size() + 1) * sizeof(wchar_t)));
            if (rc != ERROR_SUCCESS) { err = LastErrText((DWORD)rc); ok = false; }
        }
    } else {
        rc = RegDeleteValueW(k, kRunValue);
        if (rc != ERROR_SUCCESS && rc != ERROR_FILE_NOT_FOUND) { err = LastErrText((DWORD)rc); ok = false; }
    }
    RegCloseKey(k); return ok;
}

// ---- update service ---------------------------------------------------------------------------------------------------------
static const wchar_t *const kReleasesApi = L"https://api.github.com/repos/Im4Theone/WinKit/releases/latest";

struct UpdateInfo { bool valid = false; ws version, installerUrl, installerSha, portableUrl, portableSha; };
inline UpdateInfo &LastUpdate() { static UpdateInfo u; return u; }
inline Lock &UpdateLock() { static Lock *l = new Lock(); return *l; }

inline ws ParseDigest(const JV *v) {      // "sha256:<hex>" -> hex ; "" when absent/other
    if (!v || v->t != JV::Str) return L"";
    return StartsWith(Lower(v->s), L"sha256:") ? v->s.substr(7) : L"";
}
inline ws UpdateFailed(const ws &msg) { return JObj().S(L"status", L"Failed").S(L"errorMessage", msg).Str(); }

inline ws UpdateCheck() {
    HttpResult r;
    HttpDo(L"GET", kReleasesApi, L"Accept: application/vnd.github+json\r\n", std::string(), r);
    if (!r.ok) return UpdateFailed(r.timedOut ? ws(L"The update check timed out.") : L"Couldn't reach GitHub (" + r.error + L").");
    if (r.status < 200 || r.status >= 300) return UpdateFailed(L"GitHub responded with " + HttpStatusText(r) + L".");
    JV rel; if (!JParse(FromCp(r.body, CP_UTF8), rel) || rel.t != JV::Obj) return UpdateFailed(L"GitHub returned an unexpected response.");
    ws tag; if (!JGetStr(rel, L"tag_name", tag) || tag.empty()) return UpdateFailed(L"GitHub returned an unexpected response.");
    size_t st = 0; while (st < tag.size() && (tag[st] == L'v' || tag[st] == L'V')) st++;
    ws latestText = tag.substr(st);
    int lv[4], cnt; if (!ParseVersion(latestText, lv, cnt)) return UpdateFailed(L"Couldn't parse the latest release version.");
    for (int i = 0; i < 4; i++) if (lv[i] < 0) lv[i] = 0;      // "2.1" must compare equal to "2.1.0"
    int cv[4]; CurrentVersion(cv);
    if (CompareVersion(lv, cv) <= 0) { Guard g(UpdateLock()); LastUpdate() = UpdateInfo(); return JObj().S(L"status", L"UpToDate").Str(); }

    ws installerUrl, installerSha, portableUrl, portableSha, htmlUrl, body; bool hasHtml = JGetStr(rel, L"html_url", htmlUrl), hasBody = JGetStr(rel, L"body", body);
    bool foundI = false, foundP = false;
    const JV *assets = rel.get(L"assets");
    if (assets && assets->t == JV::Arr) for (const JV &a : assets->a) {
        ws name; if (a.t != JV::Obj || !JGetStr(a, L"name", name)) continue; ws ln = Lower(name);
        ws url; bool hasUrl = JGetStr(a, L"browser_download_url", url);
        if (!foundI && StartsWith(ln, L"winkitsetup-") && ln.size() >= 4 && ln.compare(ln.size() - 4, 4, L".exe") == 0) {
            foundI = true; if (hasUrl) installerUrl = url; installerSha = ParseDigest(a.get(L"digest"));
        } else if (!foundP && StartsWith(ln, L"winkit-") && ln.size() >= 13 && ln.compare(ln.size() - 13, 13, L"-portable.zip") == 0) {
            foundP = true; if (hasUrl) portableUrl = url; portableSha = ParseDigest(a.get(L"digest"));
        }
    }
    { Guard g(UpdateLock()); UpdateInfo &u = LastUpdate(); u.valid = true; u.version = latestText; u.installerUrl = installerUrl; u.installerSha = installerSha; u.portableUrl = portableUrl; u.portableSha = portableSha; }
    JObj o; o.S(L"status", L"UpdateAvailable").S(L"latestVersion", latestText);
    o.R(L"releaseUrl", JStrOrNull(htmlUrl, hasHtml)).R(L"releaseNotes", JStrOrNull(body, hasBody));
    o.R(L"installerAssetUrl", JStrOrNull(installerUrl, !installerUrl.empty())).R(L"installerSha256", JStrOrNull(installerSha, !installerSha.empty()));
    o.R(L"portableAssetUrl", JStrOrNull(portableUrl, !portableUrl.empty())).R(L"portableSha256", JStrOrNull(portableSha, !portableSha.empty()));
    return o.Str();
}

inline ws ApplyFailed(const ws &msg) { return JObj().S(L"status", L"Failed").S(L"errorMessage", msg).Str(); }
inline void UpdateProgress(const wchar_t *stage, unsigned long long got, unsigned long long total) {
    JObj o; o.S(L"stage", stage).U(L"received", got).U(L"total", total);
    o.D(L"percent", total ? (double)got * 100.0 / (double)total : 0.0, 1);
    PushEvent(L"update_progress", o.Str());
}
inline bool TrustedAssetUrl(const ws &url) {      // https + github.com / *.githubusercontent.com only
    if (!StartsWith(Lower(url), L"https://")) return false;
    ws h = url.substr(8); size_t e = h.find_first_of(L"/?#:"); if (e != ws::npos) h = h.substr(0, e); h = Lower(h);
    auto endsWith = [&](const wchar_t *s) { size_t n = wcslen(s); return h.size() >= n && h.compare(h.size() - n, n, s) == 0; };
    return h == L"github.com" || endsWith(L".github.com") || endsWith(L".githubusercontent.com");
}
inline ws SafeFilePart(const ws &s) {
    ws o; for (wchar_t c : s) o += (iswalnum(c) || c == L'.' || c == L'-' || c == L'_' || c == L'+') ? c : L'_'; return o;
}
inline bool DownloadToTemp(const ws &url, const ws &fileName, ws &outPath, ws &err) {
    ws dir = TempDir() + L"\\WinKitUpdate";
    if (!CreateDirRecursive(dir)) { err = L"Couldn't prepare the update (" + LastErrText(GetLastError()) + L")."; return false; }
    outPath = dir + L"\\" + fileName;
    HANDLE f = CreateFileW(outPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) { err = L"Couldn't prepare the update (" + LastErrText(GetLastError()) + L")."; return false; }
    HttpResult r; HttpDo(L"GET", url, L"", std::string(), r, f, [](unsigned long long g, unsigned long long t) { UpdateProgress(L"download", g, t); });
    CloseHandle(f);
    if (!r.ok) { DeleteFileW(outPath.c_str()); err = r.timedOut ? ws(L"The download timed out.") : L"Couldn't download the update (" + r.error + L")."; return false; }
    if (r.status < 200 || r.status >= 300) {
        DeleteFileW(outPath.c_str());
        err = L"Couldn't download the update (Response status code does not indicate success: " + Num(r.status) + L" (" + r.reason + L").)."; return false;
    }
    return true;
}
inline bool VerifyChecksum(const ws &path, const ws &expected) {
    UpdateProgress(L"verify", 0, 0);
    ws actual; if (!Sha256File(path, actual)) return false;
    return Lower(actual) == Lower(expected);
}
inline bool ExtractZip(const ws &zip, const ws &dest, ws &err) {
    if (!CreateDirRecursive(dest)) { err = LastErrText(GetLastError()); return false; }
    DWORD ec = (DWORD)-1;
    ws tar = SysDir() + L"\\tar.exe";
    if (FileExists(tar)) {
        RunCapture(L"\"" + tar + L"\" -xf \"" + zip + L"\" -C \"" + dest + L"\"", &ec, 600000);
        if (ec == 0) return true;
    }
    auto q = [](ws s) { ws o; for (wchar_t c : s) { if (c == L'\'') o += L"''"; else o += c; } return o; };
    ws out = RunPowerShell(L"Expand-Archive -LiteralPath '" + q(zip) + L"' -DestinationPath '" + q(dest) + L"' -Force", &ec, 600000);
    if (ec == 0) return true;
    err = Trim(out).empty() ? ws(L"extraction failed") : Trim(out).substr(0, 300); return false;
}
inline bool Launch(const ws &file, const ws &params, const ws &dir, ws &err) {
    SHELLEXECUTEINFOW sei = {}; sei.cbSize = sizeof(sei); sei.fMask = SEE_MASK_NOASYNC;
    sei.lpVerb = L"open"; sei.lpFile = file.c_str(); sei.lpParameters = params.empty() ? nullptr : params.c_str();
    sei.lpDirectory = dir.empty() ? nullptr : dir.c_str(); sei.nShow = SW_SHOWNORMAL;
    if (ShellExecuteExW(&sei)) return true;
    err = LastErrText(GetLastError()); return false;
}

inline ws UpdateDownloadApply(const ws &expectVersion) {
    UpdateInfo u; { Guard g(UpdateLock()); u = LastUpdate(); }
    if (!u.valid || (!expectVersion.empty() && Lower(expectVersion) != Lower(u.version))) return ApplyFailed(L"Check for updates first.");
    ws ver = SafeFilePart(u.version), err;
    if (IsInstalledLocation()) {
        if (u.installerUrl.empty() || u.installerSha.empty()) return ApplyFailed(L"This release doesn't include an installer download.");
        if (!TrustedAssetUrl(u.installerUrl)) return ApplyFailed(L"The installer download address is not trusted.");
        ws path; if (!DownloadToTemp(u.installerUrl, L"WinKitSetup-" + ver + L".exe", path, err)) return ApplyFailed(err);
        if (!VerifyChecksum(path, u.installerSha)) { DeleteFileW(path.c_str()); return ApplyFailed(L"The downloaded installer failed checksum verification."); }
        UpdateProgress(L"launch", 0, 0);
        // The installer elevates itself and replaces our files after we exit.
        if (!Launch(path, L"/VERYSILENT /NORESTART /launchafterinstall=1", L"", err)) return ApplyFailed(L"Couldn't start the installer (" + err + L").");
        return JObj().S(L"status", L"Started").Str();
    }
    if (u.portableUrl.empty() || u.portableSha.empty()) return ApplyFailed(L"This release doesn't include a portable download.");
    if (!TrustedAssetUrl(u.portableUrl)) return ApplyFailed(L"The portable download address is not trusted.");
    ws installDir = ExeDir();
    ws updaterPath = installDir + L"\\WinKit.Updater.exe", updaterParams;
    bool dedicated = FileExists(updaterPath);
    ws zip; if (!DownloadToTemp(u.portableUrl, L"WinKit-" + ver + L"-portable.zip", zip, err)) return ApplyFailed(err);
    if (!VerifyChecksum(zip, u.portableSha)) { DeleteFileW(zip.c_str()); return ApplyFailed(L"The downloaded update failed checksum verification."); }
    ws staging = TempDir() + L"\\WinKitUpdate\\staging-" + Lower(RandHex(16));
    UpdateProgress(L"extract", 0, 0);
    if (!ExtractZip(zip, staging, err)) { DeleteFileW(zip.c_str()); DeleteDirRecursive(staging); return ApplyFailed(L"Couldn't prepare the update (" + err + L")."); }
    DeleteFileW(zip.c_str());
    if (!dedicated) {
        // No separate updater exe shipped: run a private copy of this exe (the install dir copy would be locked / overwritten).
        updaterPath = TempDir() + L"\\WinKitUpdate\\updater-" + Lower(RandHex(8)) + L".exe";
        if (!CopyFileW(ExePath().c_str(), updaterPath.c_str(), FALSE)) { err = LastErrText(GetLastError()); DeleteDirRecursive(staging); return ApplyFailed(L"The updater helper is missing from this installation (" + err + L")."); }
        updaterParams = ws(WINKIT_UPDATER_SWITCH) + L" ";
    }
    updaterParams += L"\"" + staging + L"\" \"" + installDir + L"\" " + Num(GetCurrentProcessId());
    UpdateProgress(L"launch", 0, 0);
    if (!Launch(updaterPath, updaterParams, installDir, err)) { DeleteDirRecursive(staging); return ApplyFailed(L"Couldn't start the updater (" + err + L")."); }
    return JObj().S(L"status", L"Started").Str();
}

// ---- error reporting -----------------------------------------------------------------------------------------------------------
static const wchar_t *const kReportEndpoint = L"https://winkit-error-reports.kaloyankrastev2013.workers.dev/report";

inline ws Clip(const ws &s, size_t n = 100000) { return s.size() > n ? s.substr(0, n) : s; }
inline ws ReportText(const ws &id, const ws &comp, const ws &ts, const ws &ver, const ws &win, const ws &arch, const ws &type, const ws &msg, const ws &stack) {
    return L"Error ID: " + id + L"\r\nComponent: " + comp + L"\r\nTimestamp: " + ts + L"\r\n\r\n"
         + L"WinKit Version: " + ver + L"\r\nWindows Version: " + win + L"\r\nArchitecture: " + arch + L"\r\n\r\n"
         + L"Exception Type: " + type + L"\r\nException Message: " + msg + L"\r\n\r\nStack Trace:\r\n" + stack + L"\r\n";
}
inline ws ErrorReportCreate(const Args &a) {
    ws id = L"WK-" + RandHex(4), ts = IsoNow(true), win = WindowsVersionText(), arch = ArchName();
    ws comp = Clip(Arg(a, L"component", L"Unhandled Exception"), 500), type = Clip(Arg(a, L"exceptionType", L"Error"), 500);
    ws msg = Clip(Arg(a, L"exceptionMessage")), stack = Clip(Arg(a, L"stackTrace"));
    if (stack.empty()) stack = L"(no stack trace available)";
    JObj o; o.S(L"errorId", id).S(L"winKitVersion", WINKIT_VERSION).S(L"windowsVersion", win).S(L"architecture", arch).S(L"component", comp)
     .S(L"exceptionType", type).S(L"exceptionMessage", msg).S(L"stackTrace", stack).S(L"timestamp", ts)
     .S(L"text", ReportText(id, comp, ts, WINKIT_VERSION, win, arch, type, msg, stack));
    return o.Str();
}
// Only the nine DiagnosticReport fields are ever transmitted (same payload as ErrorReportingService.SendReportAsync).
inline ws ErrorReportSend(const Args &a) {
    JObj p;
    p.S(L"errorId", Clip(Arg(a, L"errorId"), 100)).S(L"winKitVersion", Clip(Arg(a, L"winKitVersion"), 100)).S(L"windowsVersion", Clip(Arg(a, L"windowsVersion"), 500))
     .S(L"architecture", Clip(Arg(a, L"architecture"), 100)).S(L"component", Clip(Arg(a, L"component"), 500)).S(L"exceptionType", Clip(Arg(a, L"exceptionType"), 500))
     .S(L"exceptionMessage", Clip(Arg(a, L"exceptionMessage"))).S(L"stackTrace", Clip(Arg(a, L"stackTrace"))).S(L"timestamp", Clip(Arg(a, L"timestamp"), 100));
    HttpResult r;
    HttpDo(L"POST", kReportEndpoint, L"Content-Type: application/json; charset=utf-8\r\n", ToUtf8(p.Str()), r);
    if (!r.ok) return JObj().B(L"ok", false).S(L"errorMessage", r.timedOut ? ws(L"The request timed out.") : L"Couldn't reach the reporting server (" + r.error + L").").Str();
    if (r.status < 200 || r.status >= 300) return JObj().B(L"ok", false).S(L"errorMessage", L"The server responded with " + HttpStatusText(r) + L".").Str();
    return JObj().B(L"ok", true).Null(L"errorMessage").Str();
}

// ---- shell helpers --------------------------------------------------------------------------------------------------------------
inline bool OpenUrl(const ws &url) {
    ws l = Lower(url);
    if (!(StartsWith(l, L"http://") || StartsWith(l, L"https://")) || url.size() > 4096) return false;
    return (INT_PTR)ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL) > 32;
}
inline ws ResolveFolderAlias(const ws &p) {
    if (p == L"@data") return AppDataDir();
    if (p == L"@logs") return LogsDir();
    if (p == L"@themes") { ws d = AppDataDir() + L"\\Themes"; CreateDirectoryW(d.c_str(), nullptr); return d; }
    if (p == L"@downloads") return TempDir() + L"\\WinKitUpdate";
    return p;
}
inline bool OpenFolder(const ws &path) {
    ws p = ResolveFolderAlias(path); if (!DirExists(p)) return false;
    return (INT_PTR)ShellExecuteW(nullptr, L"open", p.c_str(), nullptr, nullptr, SW_SHOWNORMAL) > 32;
}
inline bool RevealInExplorer(const ws &path) {
    ws p = ResolveFolderAlias(path);
    if (DirExists(p)) return OpenFolder(p);
    if (!FileExists(p)) return false;
    return (INT_PTR)ShellExecuteW(nullptr, L"open", L"explorer.exe", (L"/select,\"" + p + L"\"").c_str(), nullptr, SW_SHOWNORMAL) > 32;
}
inline bool ClipboardSet(const ws &text) {
    if (!OpenClipboard(nullptr)) return false;
    EmptyClipboard();
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(wchar_t));
    if (!h) { CloseClipboard(); return false; }
    wchar_t *d = (wchar_t *)GlobalLock(h); memcpy(d, text.c_str(), (text.size() + 1) * sizeof(wchar_t)); GlobalUnlock(h);
    if (!SetClipboardData(CF_UNICODETEXT, h)) { GlobalFree(h); CloseClipboard(); return false; }
    CloseClipboard(); return true;
}
// "Name (*.json)|*.json|All files|*.*"  ->  double-NUL separated buffer
inline std::vector<wchar_t> FilterBuf(const ws &f0) {
    ws f = f0.empty() ? ws(L"All files (*.*)|*.*") : f0;
    std::vector<wchar_t> b; for (wchar_t c : f) b.push_back(c == L'|' ? 0 : c); b.push_back(0); b.push_back(0); return b;
}
inline HWND DialogOwner() {
    HWND w = GetForegroundWindow(); DWORD pid = 0; if (w) GetWindowThreadProcessId(w, &pid);
    return (w && pid == GetCurrentProcessId()) ? w : nullptr;
}
// -> {"path":..,"text":..} or null (cancelled)
inline bool ImportFileDialog(const ws &title, const ws &filter, ws &out, ws &err) {
    wchar_t file[MAX_PATH * 2] = {0}; std::vector<wchar_t> fb = FilterBuf(filter);
    OPENFILENAMEW o = {}; o.lStructSize = sizeof(o); o.hwndOwner = DialogOwner(); o.lpstrFilter = fb.data(); o.nFilterIndex = 1;
    o.lpstrFile = file; o.nMaxFile = MAX_PATH * 2; o.lpstrTitle = title.empty() ? nullptr : title.c_str();
    o.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER | OFN_HIDEREADONLY;
    if (!GetOpenFileNameW(&o)) { out = L"null"; return true; }
    std::string bytes; if (!ReadFileBytes(file, bytes)) { err = L"Couldn't read the file."; return false; }
    if (bytes.size() > 4u * 1024 * 1024) { err = L"The file is too large."; return false; }
    if (bytes.size() >= 3 && (unsigned char)bytes[0] == 0xEF && (unsigned char)bytes[1] == 0xBB && (unsigned char)bytes[2] == 0xBF) bytes.erase(0, 3);
    out = JObj().S(L"path", file).S(L"text", FromCp(bytes, CP_UTF8)).Str(); return true;
}
// -> "path" or null (cancelled). Writes `text` (UTF-8, no BOM) to the chosen file.
inline bool ExportFileDialog(const ws &title, const ws &filter, const ws &defName, const ws &defExt, const ws &text, ws &out, ws &err) {
    wchar_t file[MAX_PATH * 2] = {0};
    wcsncpy_s(file, MAX_PATH * 2, defName.c_str(), MAX_PATH - 1);
    for (wchar_t *c = file; *c; ++c) if (wcschr(L"<>:\"/\\|?*", *c)) *c = L'_';
    std::vector<wchar_t> fb = FilterBuf(filter);
    OPENFILENAMEW o = {}; o.lStructSize = sizeof(o); o.hwndOwner = DialogOwner(); o.lpstrFilter = fb.data(); o.nFilterIndex = 1;
    o.lpstrFile = file; o.nMaxFile = MAX_PATH * 2; o.lpstrTitle = title.empty() ? nullptr : title.c_str();
    o.lpstrDefExt = defExt.empty() ? nullptr : defExt.c_str();
    o.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER | OFN_HIDEREADONLY;
    if (!GetSaveFileNameW(&o)) { out = L"null"; return true; }
    if (!WriteFileText(file, text)) { err = L"Couldn't write the file."; return false; }
    out = JStr(file); return true;
}

// ---- window material bridge ----------------------------------------------------------------------------------------------
inline unsigned ParseColorRef(const ws &hex) {      // "#RRGGBB" / "RRGGBB" -> 0x00BBGGRR ; anything else -> 0xFFFFFFFF
    ws h = hex; if (!h.empty() && h[0] == L'#') h.erase(0, 1);
    if (h.size() != 6) return 0xFFFFFFFFu;
    for (wchar_t c : h) if (!iswxdigit(c)) return 0xFFFFFFFFu;
    unsigned v = (unsigned)wcstoul(h.c_str(), nullptr, 16);
    return ((v & 0xFF) << 16) | (v & 0xFF00) | ((v >> 16) & 0xFF);
}

// ---- generic process helpers (port of ProcessRunner; for other native modules, not exposed to JS) ----------------------------
// Captures stdout+stderr (OEM code page); no shell involved. Returns false if the process could not be started.
inline bool Infra_RunProcess(const ws &file, const ws &arguments, ws &output, DWORD &exitCode, DWORD timeoutMs = 60000) {
    output = RunCapture(L"\"" + file + L"\" " + arguments, &exitCode, timeoutMs);
    return exitCode != (DWORD)-1;
}
// Elevated run ("runas"): output cannot be captured. Waits up to timeoutMs (or until `cancel` is signalled; the elevated child
// is not killed, like the C# version). Returns false if launching failed (UAC declined) or the wait timed out/cancelled.
inline bool Infra_RunElevated(const ws &file, const ws &arguments, DWORD &exitCode, DWORD timeoutMs = 600000, HANDLE cancel = nullptr) {
    SHELLEXECUTEINFOW sei = {}; sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI | SEE_MASK_NOASYNC;
    sei.lpVerb = L"runas"; sei.lpFile = file.c_str(); sei.lpParameters = arguments.empty() ? nullptr : arguments.c_str(); sei.nShow = SW_HIDE;
    if (!ShellExecuteExW(&sei) || !sei.hProcess) return false;
    HANDLE hs[2] = { sei.hProcess, cancel }; DWORD w = WaitForMultipleObjects(cancel ? 2 : 1, hs, FALSE, timeoutMs);
    bool ok = (w == WAIT_OBJECT_0);
    if (ok) GetExitCodeProcess(sei.hProcess, &exitCode);
    CloseHandle(sei.hProcess); return ok;
}

} // namespace wkinfra

inline bool Infra_StartMinimized() { return wkinfra::StartMinimizedSetting(); }

// ---- the module entry point ---------------------------------------------------------------------------------------------------
inline bool Mod_Infra(const ws &cmd, const Args &a, ws &out) {
    using namespace wkinfra;
    try {
        ws err;
        if (cmd == L"store_read") {
            if (!StoreReadText(Arg(a, L"name"), out, err)) out = Err(err);
            return true;
        }
        if (cmd == L"store_write") {
            out = StoreWriteText(Arg(a, L"name"), Arg(a, L"json"), err) ? ws(L"true") : Err(err);
            return true;
        }
        if (cmd == L"store_delete") {
            out = StoreDelete(Arg(a, L"name"), err) ? ws(L"true") : Err(err);
            return true;
        }
        if (cmd == L"store_list") {
            if (!StoreList(Arg(a, L"dir"), out, err)) out = Err(err);
            return true;
        }
        if (cmd == L"crash_log") { CrashLog(Clip(Arg(a, L"text"), 200000), Arg(a, L"tag")); out = L"true"; return true; }

        if (cmd == L"app_info") {
            ws build; RegGetString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"CurrentBuildNumber", build, KEY_WOW64_64KEY);
            JObj o; o.S(L"version", WINKIT_VERSION).S(L"exePath", ExePath()).S(L"exeDir", ExeDir()).S(L"dataDir", AppDataDir())
             .S(L"logsDir", LogsDir()).S(L"themesDir", AppDataDir() + L"\\Themes").B(L"isAdmin", IsAdmin()).B(L"isInstalledLocation", IsInstalledLocation())
             .S(L"userName", EnvVar(L"USERNAME")).S(L"architecture", ArchName()).S(L"windowsVersion", WindowsVersionText())
             .N(L"windowsBuild", build.empty() ? 0 : _wtoi(build.c_str())).B(L"systemUsesLightTheme", SystemUsesLightTheme());
            out = o.Str(); return true;
        }
        if (cmd == L"app_quit") { HostQuit(); out = L"true"; return true; }
        if (cmd == L"system_uses_light_theme") { out = Bool(SystemUsesLightTheme()); return true; }

        if (cmd == L"is_admin") { out = Bool(IsAdmin()); return true; }
        if (cmd == L"relaunch_elevated") {
            if (RelaunchElevated(Arg(a, L"arguments"))) { out = L"true"; HostQuit(); }
            else out = L"false";
            return true;
        }

        if (cmd == L"autostart_get") { out = Bool(AutoStartEnabled()); return true; }
        if (cmd == L"autostart_set") { out = AutoStartSet(ArgBool(a, L"enabled"), err) ? ws(L"true") : Err(err); return true; }

        if (cmd == L"update_check") { out = UpdateCheck(); return true; }
        if (cmd == L"update_download_apply") { out = UpdateDownloadApply(Arg(a, L"latestVersion")); return true; }

        if (cmd == L"error_report_create") { out = ErrorReportCreate(a); return true; }
        if (cmd == L"error_report_send") { out = ErrorReportSend(a); return true; }

        if (cmd == L"open_url") { out = OpenUrl(Arg(a, L"url")) ? ws(L"true") : Err(L"Couldn't open the link."); return true; }
        if (cmd == L"open_folder") { out = OpenFolder(Arg(a, L"path")) ? ws(L"true") : Err(L"That folder doesn't exist."); return true; }
        if (cmd == L"reveal_in_explorer") { out = RevealInExplorer(Arg(a, L"path")) ? ws(L"true") : Err(L"That file or folder doesn't exist."); return true; }
        if (cmd == L"clipboard_set") { out = ClipboardSet(Arg(a, L"text")) ? ws(L"true") : Err(L"The clipboard is busy."); return true; }
        if (cmd == L"import_file_dialog") {
            if (!ImportFileDialog(Arg(a, L"title"), Arg(a, L"filter"), out, err)) out = Err(err);
            return true;
        }
        if (cmd == L"export_file_dialog") {
            if (!ExportFileDialog(Arg(a, L"title"), Arg(a, L"filter"), Arg(a, L"defaultName"), Arg(a, L"defaultExt"), Arg(a, L"text"), out, err)) out = Err(err);
            return true;
        }

        if (cmd == L"set_window_material") { HostSetMaterial(Arg(a, L"mode", L"dark")); out = L"true"; return true; }
        if (cmd == L"set_title_bar_colors") {
            HostSetTitleBarColors(ArgBool(a, L"dark", true), ParseColorRef(Arg(a, L"caption")), ParseColorRef(Arg(a, L"text")), ParseColorRef(Arg(a, L"border")));
            out = L"true"; return true;
        }
    } catch (const std::exception &e) {
        out = Err(FromCp(e.what(), CP_ACP)); return true;
    } catch (...) {
        out = Err(L"unexpected error"); return true;
    }
    return false;
}

// ---- WinKit.Updater port ---------------------------------------------------------------------------------------------------------
// Entry point of the portable-update helper. Call it from wWinMain BEFORE creating any window / WebView when the
// process command line contains   --apply-update <stagingDir> <installDir> <pid>   (the switch may be absent when
// the helper is a standalone WinKit.Updater.exe that is started with exactly the three positional args, like the C# one).
// Waits for <pid> to exit (max 30 s), copies every file from stagingDir over installDir (parallel, with retries),
// deletes stagingDir, relaunches <installDir>\WinKit.exe. Returns 0 on success, 1 on failure (it still tries to relaunch).
namespace wkinfra {
struct CopyJobs {
    std::vector<std::pair<ws, ws>> jobs; volatile LONG next = 0; volatile LONG failed = 0; ws firstErr; Lock lock; ws selfPath;
};
inline void UpdaterLog(const ws &m) { CrashLog(m, L"[WinKit.Updater]"); }
inline bool SamePath(const ws &a, const ws &b) { return Lower(a) == Lower(b); }
inline bool CopyFileWithRetry(const ws &src, const ws &dst, const ws &self, ws &err) {
    const int maxAttempts = 5;
    for (int attempt = 1; attempt <= maxAttempts; attempt++) {
        if (CopyFileW(src.c_str(), dst.c_str(), FALSE)) return true;
        DWORD le = GetLastError();
        // A running exe (e.g. the helper itself) can be renamed but not overwritten: move it aside, then copy.
        if (SamePath(dst, self) || (le == ERROR_SHARING_VIOLATION && attempt == maxAttempts)) {
            ws aside = dst + L".old-" + Num(GetCurrentProcessId());
            DeleteFileW(aside.c_str());
            if (MoveFileExW(dst.c_str(), aside.c_str(), MOVEFILE_REPLACE_EXISTING) && CopyFileW(src.c_str(), dst.c_str(), FALSE)) return true;
            le = GetLastError();
        }
        if (attempt == maxAttempts) { err = dst + L": " + LastErrText(le); return false; }
        Sleep(300);
    }
    return false;
}
inline DWORD WINAPI CopyWorker(LPVOID p) {
    CopyJobs *c = (CopyJobs *)p;
    for (;;) {
        LONG i = InterlockedIncrement(&c->next) - 1;
        if (i < 0 || (size_t)i >= c->jobs.size()) break;
        ws err;
        if (!CopyFileWithRetry(c->jobs[(size_t)i].first, c->jobs[(size_t)i].second, c->selfPath, err)) {
            Guard g(c->lock); if (!c->failed) c->firstErr = err; InterlockedExchange(&c->failed, 1);
        }
    }
    return 0;
}
inline void CollectFiles(const ws &root, const ws &rel, std::vector<ws> &files, std::vector<ws> &dirs) {
    WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW((root + (rel.empty() ? L"" : L"\\" + rel) + L"\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        ws n = fd.cFileName; if (n == L"." || n == L"..") continue;
        ws r = rel.empty() ? n : rel + L"\\" + n;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) { dirs.push_back(r); CollectFiles(root, r, files, dirs); }
        else files.push_back(r);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}
inline bool CopyDirectoryWithRetry(const ws &srcDir, const ws &dstDir, ws &err) {
    std::vector<ws> files, dirs; CollectFiles(srcDir, L"", files, dirs);
    CreateDirRecursive(dstDir);
    for (auto &d : dirs) CreateDirRecursive(dstDir + L"\\" + d);
    CopyJobs c; c.selfPath = ExePath();
    for (auto &f : files) c.jobs.push_back({ srcDir + L"\\" + f, dstDir + L"\\" + f });
    SYSTEM_INFO si; GetSystemInfo(&si);
    int n = (int)si.dwNumberOfProcessors; if (n < 2) n = 2; if (n > 8) n = 8;
    if ((size_t)n > c.jobs.size()) n = (int)c.jobs.size();
    std::vector<HANDLE> th;
    for (int i = 0; i < n; i++) { HANDLE h = CreateThread(nullptr, 0, CopyWorker, &c, 0, nullptr); if (h) th.push_back(h); }
    if (th.empty()) CopyWorker(&c);
    for (HANDLE h : th) { WaitForSingleObject(h, INFINITE); CloseHandle(h); }
    if (c.failed) { err = c.firstErr; return false; }
    return true;
}
inline bool RelaunchWinKit(const ws &installDir, ws &err) {
    ws exe = installDir + L"\\" + WINKIT_EXE_NAME;
    SHELLEXECUTEINFOW sei = {}; sei.cbSize = sizeof(sei); sei.lpVerb = L"open"; sei.lpFile = exe.c_str(); sei.lpDirectory = installDir.c_str(); sei.nShow = SW_SHOWNORMAL;
    if (ShellExecuteExW(&sei)) return true;
    err = exe + L": " + LastErrText(GetLastError()); return false;
}
} // namespace wkinfra

inline int Updater_Main(const wchar_t *cmdline) {
    using namespace wkinfra;
    int argc = 0; wchar_t **argv = CommandLineToArgvW(cmdline ? cmdline : L"", &argc);
    std::vector<ws> args; if (argv) { for (int i = 0; i < argc; i++) args.push_back(argv[i]); LocalFree(argv); }
    // args[0] is the program name when `cmdline` is the full process command line (GetCommandLineW()); detect it by the switch / arg count.
    size_t first = 0;
    for (size_t i = 0; i < args.size(); i++) if (args[i] == WINKIT_UPDATER_SWITCH) { first = i + 1; break; }
    if (first == 0) first = (args.size() >= 4) ? 1 : 0;      // no switch: skip argv[0] only if it is present
    if (args.size() < first + 3) { UpdaterLog(L"Missing or invalid arguments; expected <stagingDir> <installDir> <pid>."); return 1; }
    ws stagingDir = args[first], installDir = args[first + 1], pidText = args[first + 2];
    while (installDir.size() > 3 && (installDir.back() == L'\\' || installDir.back() == L'/')) installDir.pop_back();
    bool digits = !pidText.empty() && pidText.size() < 10; for (wchar_t c : pidText) if (c < L'0' || c > L'9') digits = false;
    if (!digits) { UpdaterLog(L"Missing or invalid arguments; expected <stagingDir> <installDir> <pid>."); return 1; }
    DWORD pid = (DWORD)_wtoi(pidText.c_str());

    ws err;
    // wait for WinKit to exit (up to 30 s); already-gone is fine
    HANDLE p = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (p) { WaitForSingleObject(p, 30000); CloseHandle(p); }
    Sleep(500);      // let the OS release file handles
    if (!CopyDirectoryWithRetry(stagingDir, installDir, err)) {
        UpdaterLog(L"Update failed: " + err);
        ws rerr; if (!RelaunchWinKit(installDir, rerr)) UpdaterLog(L"Relaunch after failed update also failed: " + rerr);
        return 1;
    }
    DeleteDirRecursive(stagingDir);
    if (!RelaunchWinKit(installDir, err)) {
        UpdaterLog(L"Update failed: " + err);
        return 1;
    }
    return 0;
}

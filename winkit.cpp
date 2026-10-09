// winkit.cpp - WinKit host: a Win32 window with an embedded WebView2 browser.
// UI = index.html (embedded as a resource, see app.rc). Backends = mod_*.h, called from JS via WK.native().
// Messages from JS are JSON strings {"id":"7","cmd":"proc_list", ...flat string args}; replies are
// {"id":"7","ok":true,"data":<json>} / {"id":"7","ok":false,"error":"..."}; events are {"event":name,"data":<json>}.
//
// Build: build.bat (plain cmd).  Run `WinKit.exe --dev` to load ./index.html from disk (F12 = devtools).

#include "common.h"
#include "mod_system.h"
#include "mod_network.h"
#include "mod_cleanup.h"
#include "mod_diag.h"
#include "mod_infra.h"

#include <shellapi.h>
#include <dwmapi.h>
#include <thread>

#ifndef WK_SYNTAX_ONLY
#include <wrl.h>
#include <WebView2.h>
using namespace Microsoft::WRL;
#endif

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

static const wchar_t *kTitle = L"WinKit";
static const UINT WM_HOST_MSG   = WM_APP + 1;   // lParam = new ws*  (JSON to post to the page)
static const UINT WM_HOST_MAT   = WM_APP + 2;   // lParam = new ws*  (material mode)
static const UINT WM_HOST_COLOR = WM_APP + 3;   // lParam = new ColorReq*

struct ColorReq { bool dark; unsigned caption, text, border; };

static HWND g_hwnd = nullptr;
#ifndef WK_SYNTAX_ONLY
static ComPtr<ICoreWebView2Controller> g_controller;
static ComPtr<ICoreWebView2> g_webview;
#endif

static bool HasFlag(const wchar_t *f) { return wcsstr(GetCommandLineW(), f) != nullptr; }
static bool DevMode() { static bool d = HasFlag(L"--dev"); return d; }

// ------------------------------------------------------------------ host functions used by the modules
static void PostToPage(const ws &json) {
    if (g_hwnd) PostMessageW(g_hwnd, WM_HOST_MSG, 0, (LPARAM) new ws(json));
}
void PushEvent(const ws &name, const ws &jsonData) {
    PostToPage(L"{\"event\":" + JStr(name) + L",\"data\":" + (jsonData.empty() ? ws(L"null") : jsonData) + L"}");
}
void HostQuit() { if (g_hwnd) PostMessageW(g_hwnd, WM_CLOSE, 0, 0); }
void HostSetMaterial(const ws &mode) { if (g_hwnd) PostMessageW(g_hwnd, WM_HOST_MAT, 0, (LPARAM) new ws(mode)); }
void HostSetTitleBarColors(bool dark, unsigned caption, unsigned text, unsigned border) {
    if (g_hwnd) PostMessageW(g_hwnd, WM_HOST_COLOR, 0, (LPARAM) new ColorReq{ dark, caption, text, border });
}

static DWORD WinBuild() {
    typedef LONG (WINAPI *RtlGetVersionFn)(OSVERSIONINFOW *);
    OSVERSIONINFOW vi = {}; vi.dwOSVersionInfoSize = sizeof(vi);
    HMODULE nt = GetModuleHandleW(L"ntdll.dll");
    RtlGetVersionFn f = nt ? (RtlGetVersionFn)(void *)GetProcAddress(nt, "RtlGetVersion") : nullptr;
    if (f) f(&vi);
    return vi.dwBuildNumber;
}
static void ApplyMaterial(const ws &modeIn) {                 // runs on the UI thread
    ws mode = Lower(modeIn);
    BOOL dark = mode.find(L"light") == ws::npos;
    DwmSetWindowAttribute(g_hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    if (WinBuild() >= 22000) {
        int corner = 2;                                       // DWMWCP_ROUND
        DwmSetWindowAttribute(g_hwnd, 33, &corner, sizeof(corner));
        int backdrop = -1;
        if (mode.find(L"mica") != ws::npos) backdrop = 2;
        else if (mode.find(L"acrylic") != ws::npos) backdrop = 3;
        else if (mode.find(L"tabbed") != ws::npos) backdrop = 4;
        else if (mode.find(L"none") != ws::npos) backdrop = 1;
        if (backdrop > 0) DwmSetWindowAttribute(g_hwnd, 38, &backdrop, sizeof(backdrop));
    }
}
static void ApplyColors(const ColorReq &c) {                  // runs on the UI thread
    BOOL dark = c.dark ? TRUE : FALSE;
    DwmSetWindowAttribute(g_hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    if (WinBuild() >= 22000) {
        COLORREF cap = c.caption, txt = c.text, bor = c.border;
        DwmSetWindowAttribute(g_hwnd, 35, &cap, sizeof(cap));  // DWMWA_CAPTION_COLOR
        DwmSetWindowAttribute(g_hwnd, 36, &txt, sizeof(txt));  // DWMWA_TEXT_COLOR
        DwmSetWindowAttribute(g_hwnd, 34, &bor, sizeof(bor));  // DWMWA_BORDER_COLOR
    }
}

// ------------------------------------------------------------------ message parsing + dispatch
// Parses the flat JSON object the page sends: {"id":"7","cmd":"x","name":"value",...}. Values may be strings
// (with escapes) or bare scalars (numbers, true/false/null), which are stored as their text.
static Args ParseMsg(const ws &j) {
    Args m; size_t i = 0, n = j.size();
    auto readStr = [&](ws &out) -> bool {
        if (i >= n || j[i] != L'"') return false;
        i++; out.clear();
        while (i < n && j[i] != L'"') {
            if (j[i] == L'\\' && i + 1 < n) {
                i++;
                switch (j[i]) {
                case L'n': out += L'\n'; break; case L'r': out += L'\r'; break; case L't': out += L'\t'; break;
                case L'b': out += L'\b'; break; case L'f': out += L'\f'; break;
                case L'u': if (i + 4 < n) { out += (wchar_t)wcstoul(j.substr(i + 1, 4).c_str(), nullptr, 16); i += 4; } break;
                default: out += j[i];
                }
            } else out += j[i];
            i++;
        }
        i++; return true;
    };
    while (i < n) {
        while (i < n && j[i] != L'"') i++;
        ws k, v; if (!readStr(k)) break;
        while (i < n && (j[i] == L' ' || j[i] == L':')) i++;
        if (i < n && j[i] == L'"') { if (!readStr(v)) break; }
        else { size_t s = i; while (i < n && j[i] != L',' && j[i] != L'}') i++; v = Trim(j.substr(s, i - s)); if (v == L"null") v.clear(); }
        m[k] = v;
    }
    return m;
}

static ws Dispatch(const ws &cmd, const Args &a) {
    ws out;
    if (Mod_Infra(cmd, a, out)) return out;
    if (Mod_System(cmd, a, out)) return out;
    if (Mod_Network(cmd, a, out)) return out;
    if (Mod_Cleanup(cmd, a, out)) return out;
    if (Mod_Diag(cmd, a, out)) return out;
    return Err(L"Unknown command: " + cmd);
}

static void HandleMessage(ws json) {
    std::thread([json]() {
        Args a = ParseMsg(json);
        ws id = a[L"id"], cmd = a[L"cmd"], res;
        try { res = Dispatch(cmd, a); }
        catch (const std::exception &e) { res = Err(L"Internal error in " + cmd + L": " + FromCp(e.what(), CP_ACP)); }
        catch (...) { res = Err(L"Internal error in " + cmd); }
        if (res.empty()) res = L"null";
        ws reply = StartsWith(res, L"error:")
            ? L"{\"id\":" + JStr(id) + L",\"ok\":false,\"error\":" + JStr(res.substr(6)) + L"}"
            : L"{\"id\":" + JStr(id) + L",\"ok\":true,\"data\":" + res + L"}";
        PostToPage(reply);
    }).detach();
}

// ------------------------------------------------------------------ window + WebView2
#ifndef WK_SYNTAX_ONLY
static bool LoadEmbeddedHtml(ws &out) {
    HRSRC r = FindResourceW(nullptr, MAKEINTRESOURCEW(1), RT_RCDATA); if (!r) return false;
    HGLOBAL g = LoadResource(nullptr, r); if (!g) return false;
    const char *p = (const char *)LockResource(g); int sz = (int)SizeofResource(nullptr, r);
    if (!p || sz <= 0) return false;
    out = FromCp(std::string(p, sz), CP_UTF8);
    if (!out.empty() && out[0] == 0xFEFF) out.erase(0, 1);
    return true;
}
static ws FileUrl(const ws &path) {
    ws u = L"file:///";
    for (wchar_t c : path) { if (c == L'\\') u += L'/'; else if (c == L' ') u += L"%20"; else u += c; }
    return u;
}
static void LoadPage() {
    if (!g_webview) return;
    if (DevMode()) {
        ws f = ExeDir() + L"\\index.html";
        if (FileExists(f)) { g_webview->Navigate(FileUrl(f).c_str()); return; }
    }
    ws html;
    if (LoadEmbeddedHtml(html)) g_webview->NavigateToString(html.c_str());
    else g_webview->NavigateToString(L"<body style='background:#161618;color:#fff;font:14px sans-serif'><p>UI resource missing.</p></body>");
}
static void Resize(HWND h) { if (g_controller) { RECT rc; GetClientRect(h, &rc); g_controller->put_Bounds(rc); } }

static void ShowRuntimeError() {
    if (MessageBoxW(g_hwnd, L"WinKit needs the Microsoft Edge WebView2 Runtime, which wasn't found on this PC.\n\nOpen the download page now?",
                    kTitle, MB_YESNO | MB_ICONWARNING) == IDYES)
        ShellExecuteW(nullptr, L"open", L"https://go.microsoft.com/fwlink/p/?LinkId=2124703", nullptr, nullptr, SW_SHOWNORMAL);
    PostQuitMessage(1);
}
static bool AllowedUrl(const wchar_t *u) {
    return u && (wcsncmp(u, L"about:", 6) == 0 || wcsncmp(u, L"file:", 5) == 0 || wcsncmp(u, L"data:", 5) == 0);
}

static void InitWebView(HWND hwnd) {
    ws data = AppDataDir() + L"\\WebView2";
    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(nullptr, data.c_str(), nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
        [hwnd](HRESULT r, ICoreWebView2Environment *env) -> HRESULT {
            if (FAILED(r) || !env) { ShowRuntimeError(); return r; }
            env->CreateCoreWebView2Controller(hwnd,
                Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                [hwnd](HRESULT r2, ICoreWebView2Controller *c) -> HRESULT {
                    if (FAILED(r2) || !c) { ShowRuntimeError(); return r2; }
                    g_controller = c; g_controller->get_CoreWebView2(&g_webview);

                    ComPtr<ICoreWebView2Controller2> c2;
                    if (SUCCEEDED(g_controller.As(&c2))) { COREWEBVIEW2_COLOR bg = { 255, 22, 22, 24 }; c2->put_DefaultBackgroundColor(bg); }

                    ComPtr<ICoreWebView2Settings> st;
                    if (SUCCEEDED(g_webview->get_Settings(&st))) {
                        st->put_IsStatusBarEnabled(FALSE);
                        st->put_IsZoomControlEnabled(FALSE);
                        st->put_AreDevToolsEnabled(DevMode() ? TRUE : FALSE);
                        st->put_AreDefaultContextMenusEnabled(DevMode() ? TRUE : FALSE);
                        ComPtr<ICoreWebView2Settings3> st3;
                        if (SUCCEEDED(st.As(&st3))) st3->put_AreBrowserAcceleratorKeysEnabled(DevMode() ? TRUE : FALSE);
                        ComPtr<ICoreWebView2Settings4> st4;     // no autofill / password prompts on the app's own text fields
                        if (SUCCEEDED(st.As(&st4))) { st4->put_IsPasswordAutosaveEnabled(FALSE); st4->put_IsGeneralAutofillEnabled(FALSE); }
                    }
                    EventRegistrationToken tok;
                    // if the renderer crashes, reload the UI instead of leaving a blank window
                    g_webview->add_ProcessFailed(Callback<ICoreWebView2ProcessFailedEventHandler>(
                        [](ICoreWebView2 *, ICoreWebView2ProcessFailedEventArgs *args) -> HRESULT {
                            COREWEBVIEW2_PROCESS_FAILED_KIND kind = COREWEBVIEW2_PROCESS_FAILED_KIND_BROWSER_PROCESS_EXITED;
                            args->get_ProcessFailedKind(&kind);
                            if (kind == COREWEBVIEW2_PROCESS_FAILED_KIND_RENDER_PROCESS_EXITED) LoadPage();
                            return S_OK;
                        }).Get(), &tok);
                    g_webview->add_WebMessageReceived(Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                        [](ICoreWebView2 *, ICoreWebView2WebMessageReceivedEventArgs *args) -> HRESULT {
                            LPWSTR s = nullptr;
                            if (SUCCEEDED(args->TryGetWebMessageAsString(&s)) && s) { HandleMessage(s); CoTaskMemFree(s); }
                            return S_OK;
                        }).Get(), &tok);
                    // the window stays on our page; links open in the default browser
                    g_webview->add_NavigationStarting(Callback<ICoreWebView2NavigationStartingEventHandler>(
                        [](ICoreWebView2 *, ICoreWebView2NavigationStartingEventArgs *args) -> HRESULT {
                            LPWSTR u = nullptr; args->get_Uri(&u);
                            if (!AllowedUrl(u)) {
                                args->put_Cancel(TRUE);
                                if (u && (wcsncmp(u, L"http://", 7) == 0 || wcsncmp(u, L"https://", 8) == 0)) ShellExecuteW(nullptr, L"open", u, nullptr, nullptr, SW_SHOWNORMAL);
                            }
                            if (u) CoTaskMemFree(u);
                            return S_OK;
                        }).Get(), &tok);
                    g_webview->add_NewWindowRequested(Callback<ICoreWebView2NewWindowRequestedEventHandler>(
                        [](ICoreWebView2 *, ICoreWebView2NewWindowRequestedEventArgs *args) -> HRESULT {
                            LPWSTR u = nullptr; args->get_Uri(&u); args->put_Handled(TRUE);
                            if (u && (wcsncmp(u, L"http://", 7) == 0 || wcsncmp(u, L"https://", 8) == 0)) ShellExecuteW(nullptr, L"open", u, nullptr, nullptr, SW_SHOWNORMAL);
                            if (u) CoTaskMemFree(u);
                            return S_OK;
                        }).Get(), &tok);

                    Resize(hwnd); LoadPage();
                    // keyboard focus belongs to the page (Tab / Enter / typing work without a first click)
                    if (GetForegroundWindow() == hwnd) g_controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
                    return S_OK;
                }).Get());
            return S_OK;
        }).Get());
    if (FAILED(hr)) ShowRuntimeError();
}
#endif  // WK_SYNTAX_ONLY

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
#ifndef WK_SYNTAX_ONLY
    case WM_SIZE: Resize(hwnd); return 0;
    case WM_SETFOCUS:                                   // restored from the taskbar / alt-tab: hand focus back to the page
        if (g_controller) g_controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
        return 0;
#endif
    case WM_DPICHANGED: {                               // moved to a monitor with another scale: use the size Windows suggests
        const RECT *r = (const RECT *)lp;
        SetWindowPos(hwnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    case WM_GETMINMAXINFO: {
        MINMAXINFO *mm = (MINMAXINFO *)lp;
        UINT dpi = GetDpiForWindow(hwnd); if (!dpi) dpi = 96;
        mm->ptMinTrackSize.x = MulDiv(880, dpi, 96); mm->ptMinTrackSize.y = MulDiv(560, dpi, 96);
        return 0;
    }
    case WM_HOST_MSG: {
        ws *r = (ws *)lp;
#ifndef WK_SYNTAX_ONLY
        if (g_webview) g_webview->PostWebMessageAsString(r->c_str());
#endif
        delete r; return 0;
    }
    case WM_HOST_MAT:   { ws *m = (ws *)lp; ApplyMaterial(*m); delete m; return 0; }
    case WM_HOST_COLOR: { ColorReq *c = (ColorReq *)lp; ApplyColors(*c); delete c; return 0; }
    case WM_DESTROY:
#ifndef WK_SYNTAX_ONLY
        g_webview = nullptr; g_controller = nullptr;
#endif
        PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int show) {
    // portable-update helper mode: no window, no WebView
    if (HasFlag(L"--apply-update")) return Updater_Main(GetCommandLineW());

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc); wc.lpfnWndProc = WndProc; wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(101));
    if (!wc.hIcon) wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hbrBackground = CreateSolidBrush(RGB(22, 22, 24)); wc.lpszClassName = L"WinKitWindow";
    RegisterClassExW(&wc);

    int w = 1180, h = 760;
    UINT dpi = 96; { HDC dc = GetDC(nullptr); if (dc) { dpi = GetDeviceCaps(dc, LOGPIXELSX); ReleaseDC(nullptr, dc); } }
    w = MulDiv(w, dpi, 96); h = MulDiv(h, dpi, 96);
    int sx = GetSystemMetrics(SM_CXSCREEN), sy = GetSystemMetrics(SM_CYSCREEN);
    g_hwnd = CreateWindowExW(0, wc.lpszClassName, kTitle, WS_OVERLAPPEDWINDOW, (sx - w) / 2, (sy - h) / 2, w, h, nullptr, nullptr, inst, nullptr);
    if (!g_hwnd) return 1;
    ApplyMaterial(L"dark");

    bool minimized = false;
    try { minimized = Infra_StartMinimized(); } catch (...) {}
    ShowWindow(g_hwnd, minimized ? SW_SHOWMINNOACTIVE : show); UpdateWindow(g_hwnd);
#ifndef WK_SYNTAX_ONLY
    InitWebView(g_hwnd);
#endif

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) { TranslateMessage(&m); DispatchMessageW(&m); }
    CoUninitialize();
    return (int)m.wParam;
}

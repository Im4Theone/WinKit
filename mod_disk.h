// mod_disk.h - drives, "Disk usage" folder sizes and (later) hardware health.
// Commands: disk_drives disk_scan disk_cancel hw_health   (see API_disk.md)
#pragma once
#include "common.h"
#include "mod_cleanup.h"      // wkcl::OpScope / wkcl::CancelOp: the shared cancellation registry
#include <atomic>
#include <thread>
#include <chrono>
#include <setupapi.h>
#include <winioctl.h>
#include <batclass.h>

#ifdef _MSC_VER
#pragma comment(lib, "setupapi.lib")
#endif

namespace wkdisk {

// Win32 file APIs need the \\?\ prefix once a path nears MAX_PATH.
inline ws Lp(const ws &p) {
    if (p.size() < 240 || p.compare(0, 4, L"\\\\?\\") == 0) return p;
    if (p.compare(0, 2, L"\\\\") == 0) return L"\\\\?\\UNC\\" + p.substr(2);
    return L"\\\\?\\" + p;
}
inline bool IsDots(const wchar_t *n) { return n[0] == L'.' && (n[1] == 0 || (n[1] == L'.' && n[2] == 0)); }
// Cloud placeholders (OneDrive "online only" files) report a size but take no space on this disk.
inline bool NotOnDisk(DWORD a) { return (a & (FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS | FILE_ATTRIBUTE_RECALL_ON_OPEN | FILE_ATTRIBUTE_OFFLINE)) != 0; }
inline unsigned long long Size64(const WIN32_FIND_DATAW &fd) { return ((unsigned long long)fd.nFileSizeHigh << 32) | fd.nFileSizeLow; }

// ---- drives -------------------------------------------------------------------------------------
inline ws DrivesList() {
    DWORD oldMode = 0; SetThreadErrorMode(SEM_FAILCRITICALERRORS, &oldMode);          // no "insert a disk" dialogs
    JArr arr; DWORD mask = GetLogicalDrives();
    for (int i = 0; i < 26; i++) {
        if (!(mask & (1u << i))) continue;
        ws root = ws(1, (wchar_t)(L'A' + i)) + L":\\";
        UINT t = GetDriveTypeW(root.c_str());
        if (t != DRIVE_FIXED && t != DRIVE_REMOVABLE && t != DRIVE_REMOTE) continue;
        ULARGE_INTEGER freeToMe, total, freeAll;
        if (!GetDiskFreeSpaceExW(root.c_str(), &freeToMe, &total, &freeAll) || total.QuadPart == 0) continue;
        wchar_t label[MAX_PATH + 1] = L"", fs[MAX_PATH + 1] = L"";
        GetVolumeInformationW(root.c_str(), label, MAX_PATH, nullptr, nullptr, nullptr, fs, MAX_PATH);
        JObj o; o.S(L"letter", root.substr(0, 2)).S(L"path", root).S(L"label", label).S(L"fileSystem", fs)
            .S(L"type", t == DRIVE_FIXED ? L"Fixed" : t == DRIVE_REMOVABLE ? L"Removable" : L"Network")
            .U(L"totalBytes", total.QuadPart).U(L"freeBytes", freeToMe.QuadPart);
        arr.Obj(o);
    }
    SetThreadErrorMode(oldMode, nullptr);
    return arr.Str();
}


// ---- battery (laptops): charge, power source, time left and how much of the original capacity it still holds -------------
inline ws BatteryJson() {
    SYSTEM_POWER_STATUS ps;
    if (!GetSystemPowerStatus(&ps) || ps.BatteryFlag == 128 || ps.BatteryFlag == 255) return JObj().B(L"present", false).Str();     // 128 = no battery, 255 = unknown
    JObj o; o.B(L"present", true);
    if (ps.BatteryLifePercent <= 100) o.N(L"percent", ps.BatteryLifePercent); else o.Null(L"percent");
    o.B(L"onAc", ps.ACLineStatus == 1).B(L"charging", (ps.BatteryFlag & 8) != 0);
    if (ps.BatteryLifeTime != (DWORD)-1) o.N(L"remainingMinutes", (long long)(ps.BatteryLifeTime / 60)); else o.Null(L"remainingMinutes");
    // design and current full-charge capacity come from the battery device itself
    static const GUID kBattery = { 0x72631e54, 0x78a4, 0x11d0, { 0xbc, 0xf7, 0x00, 0xaa, 0x00, 0xb7, 0xb3, 0x2a } };
    unsigned long design = 0, full = 0, cycles = 0; ws chem;
    HDEVINFO di = SetupDiGetClassDevsW(&kBattery, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (di != INVALID_HANDLE_VALUE) {
        SP_DEVICE_INTERFACE_DATA did; did.cbSize = sizeof(did);
        for (DWORD i = 0; i < 4 && !design && SetupDiEnumDeviceInterfaces(di, nullptr, &kBattery, i, &did); i++) {
            DWORD need = 0; SetupDiGetDeviceInterfaceDetailW(di, &did, nullptr, 0, &need, nullptr); if (!need) continue;
            std::vector<char> buf(need, 0); auto *d = (SP_DEVICE_INTERFACE_DETAIL_DATA_W *)buf.data(); d->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
            if (!SetupDiGetDeviceInterfaceDetailW(di, &did, d, need, nullptr, nullptr)) continue;
            HANDLE h = CreateFileW(d->DevicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (h == INVALID_HANDLE_VALUE) continue;
            ULONG wait = 0, tag = 0, got = 0;
            if (DeviceIoControl(h, IOCTL_BATTERY_QUERY_TAG, &wait, sizeof(wait), &tag, sizeof(tag), &got, nullptr) && tag) {
                BATTERY_QUERY_INFORMATION q = {}; q.BatteryTag = tag; q.InformationLevel = BatteryInformation; BATTERY_INFORMATION bi = {};
                if (DeviceIoControl(h, IOCTL_BATTERY_QUERY_INFORMATION, &q, sizeof(q), &bi, sizeof(bi), &got, nullptr) && !(bi.Capabilities & BATTERY_CAPACITY_RELATIVE)) {
                    design = bi.DesignedCapacity; full = bi.FullChargedCapacity; cycles = bi.CycleCount;
                    for (int k = 0; k < 4 && bi.Chemistry[k]; k++) chem += (wchar_t)bi.Chemistry[k];
                }
            }
            CloseHandle(h);
        }
        SetupDiDestroyDeviceInfoList(di);
    }
    if (design > 0 && full > 0) {
        double wear = std::max(0.0, 100.0 - 100.0 * (double)full / (double)design);
        o.N(L"designMwh", design).N(L"fullMwh", full).D(L"wearPercent", wear, 1).S(L"health", wear < 20 ? L"Good" : wear < 40 ? L"Fair" : L"Worn");
    } else o.Null(L"wearPercent");
    if (cycles > 0) o.N(L"cycleCount", cycles);
    if (!chem.empty()) o.S(L"chemistry", chem);
    return o.Str();
}

// ---- folder sizes -------------------------------------------------------------------------------
struct Totals { std::atomic<unsigned long long> files{0}, bytes{0}; std::atomic<unsigned> unreadable{0}; };
struct Child { ws name, path; bool isDir = false, link = false; std::atomic<unsigned long long> bytes{0}, files{0}; };

inline void SizeTree(const ws &root, Child &c, Totals &t, const std::atomic<bool> &cancel) {
    std::vector<ws> stack; stack.push_back(root);
    while (!stack.empty() && !cancel) {
        ws dir = std::move(stack.back()); stack.pop_back();
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileExW(Lp(dir + L"\\*").c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
        if (h == INVALID_HANDLE_VALUE) { DWORD e = GetLastError(); if (e != ERROR_FILE_NOT_FOUND && e != ERROR_PATH_NOT_FOUND) t.unreadable++; continue; }
        unsigned long long b = 0, f = 0;
        do {
            if (IsDots(fd.cFileName)) continue;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) { if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) stack.push_back(dir + L"\\" + fd.cFileName); }   // never follow junctions or links
            else if (!NotOnDisk(fd.dwFileAttributes)) { b += Size64(fd); f++; }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
        c.bytes += b; c.files += f; t.bytes += b; t.files += f;
    }
}

inline ws ParentOf(const ws &p) {                                   // "" at a drive root
    if (p.size() <= 3) return ws();
    size_t i = p.find_last_of(L'\\'); if (i == ws::npos || i < 2) return ws();
    ws r = p.substr(0, i); if (r.size() == 2) r += L"\\"; return r;
}

inline ws DiskScan(const Args &a) {
    ws path = Trim(Arg(a, L"path")), opId = Arg(a, L"opId");
    if (path == L"@user") path = EnvVar(L"USERPROFILE");
    if (path.size() < 2 || path[1] != L':' || !((path[0] >= L'A' && path[0] <= L'Z') || (path[0] >= L'a' && path[0] <= L'z'))) return Err(L"Choose a folder on one of your drives.");
    while (path.size() > 3 && path.back() == L'\\') path.pop_back();
    if (path.size() == 2) path += L"\\";
    if (!DirExists(Lp(path))) return Err(L"That folder doesn't exist or can't be opened.");
    wkcl::OpScope scope(L"disk:" + opId); const std::atomic<bool> &cancel = scope.f->cancel;

    // list the folder itself: files are measured here, sub-folders are measured by the workers below
    std::vector<std::unique_ptr<Child>> kids; Totals tot; unsigned long long looseBytes = 0, looseFiles = 0;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileExW(Lp((path.back() == L'\\' ? path : path + L"\\") + L"*").c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
    if (h == INVALID_HANDLE_VALUE) return Err(L"Windows won't let WinKit read that folder. Try again as administrator.");
    ws base = path.back() == L'\\' ? path : path + L"\\";
    do {
        if (IsDots(fd.cFileName)) continue;
        auto c = std::make_unique<Child>(); c->name = fd.cFileName; c->path = base + fd.cFileName;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) { c->isDir = true; c->link = (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0; }
        else if (!NotOnDisk(fd.dwFileAttributes)) { c->bytes = Size64(fd); c->files = 1; looseBytes += c->bytes; looseFiles++; tot.bytes += c->bytes; tot.files++; }
        kids.push_back(std::move(c));
    } while (FindNextFileW(h, &fd));
    FindClose(h);

    std::vector<Child *> dirs; for (auto &k : kids) if (k->isDir && !k->link) dirs.push_back(k.get());
    std::atomic<size_t> next{0}, done{0}, running{0};
    unsigned hc = std::thread::hardware_concurrency(); unsigned nw = (unsigned)std::min<size_t>(dirs.size(), std::min(8u, hc < 2 ? 2u : hc));
    std::vector<std::thread> pool;
    for (unsigned w = 0; w < nw; w++) { running++; pool.emplace_back([&]() {
        for (;;) { size_t i = next++; if (i >= dirs.size() || cancel) break; SizeTree(dirs[i]->path, *dirs[i], tot, cancel); done++; }
        running--; }); }
    auto tick = [&]() { PushEvent(L"disk_progress", JObj().S(L"opId", opId).U(L"files", tot.files).U(L"bytes", tot.bytes).U(L"done", done).U(L"total", dirs.size()).Str()); };
    while (running > 0) { std::this_thread::sleep_for(std::chrono::milliseconds(250)); tick(); }
    for (auto &t : pool) t.join();
    if (cancel) return JObj().B(L"cancelled", true).Str();

    std::vector<Child *> order; for (auto &k : kids) order.push_back(k.get());
    std::sort(order.begin(), order.end(), [](Child *x, Child *y) { return x->bytes != y->bytes ? x->bytes > y->bytes : _wcsicmp(x->name.c_str(), y->name.c_str()) < 0; });
    const size_t MAXI = 300; JArr items;
    for (size_t i = 0; i < order.size() && i < MAXI; i++) {
        Child *c = order[i];
        items.Obj(JObj().S(L"name", c->name).B(L"isDir", c->isDir).B(L"link", c->link).U(L"bytes", c->bytes).U(L"files", c->files));
    }
    ws parent = ParentOf(path);
    JObj o; o.S(L"path", path).R(L"parent", parent.empty() ? ws(L"null") : JStr(parent)).U(L"totalBytes", tot.bytes).U(L"fileCount", tot.files)
        .U(L"unreadable", tot.unreadable).U(L"itemCount", order.size()).B(L"cancelled", false).R(L"items", items.Str());
    return o.Str();
}

} // namespace wkdisk

// =============================================================================================
inline bool Mod_Disk(const ws &cmd, const Args &a, ws &out) {
    using namespace wkdisk;
    if (cmd != L"hw_health" && (cmd.size() < 6 || cmd.compare(0, 5, L"disk_") != 0)) return false;
    try {
        if (cmd == L"disk_drives") { out = DrivesList(); return true; }
        if (cmd == L"disk_scan") { out = DiskScan(a); return true; }
        if (cmd == L"hw_health") { out = JObj().R(L"battery", BatteryJson()).Str(); return true; }
        if (cmd == L"disk_cancel") { out = Bool(wkcl::CancelOp(L"disk:" + Arg(a, L"opId"))); return true; }
    } catch (const std::exception &e) {
        out = Err(FromCp(e.what(), CP_ACP)); return true;
    } catch (...) {
        out = Err(L"unexpected error"); return true;
    }
    return false;
}

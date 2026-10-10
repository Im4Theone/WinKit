// mod_network.h - port of WinKit.Network (ping, traceroute, DNS lookup, IP configuration, maintenance).
// See API_network.md. Header-only; commands: ping_run ping_cancel trace_run trace_cancel dns_lookup
// ipcfg_get netmaint_flushdns netmaint_dhcprenew netmaint_dhcprelease netmaint_winsockreset netmaint_tcpipreset
#pragma once
#include "common.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <tlhelp32.h>
#include <windot11.h>
#include <wlanapi.h>      // types only; the functions are loaded at run time
#include <shellapi.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <cstring>

#ifdef _MSC_VER
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#endif

#ifndef GAA_FLAG_INCLUDE_GATEWAYS
#define GAA_FLAG_INCLUDE_GATEWAYS 0x0080
#endif
#ifndef IP_ADAPTER_DHCP_ENABLED
#define IP_ADAPTER_DHCP_ENABLED 0x00000004
#endif
#ifndef IP_ADAPTER_IPV4_ENABLED
#define IP_ADAPTER_IPV4_ENABLED 0x00000080
#endif
#ifndef SEE_MASK_NOASYNC
#define SEE_MASK_NOASYNC 0x00000100
#endif

namespace wknet {

// ---- cancellation registry (keyed by kind + opId) --------------------------------------------
struct OpFlag { std::atomic<bool> cancel{false}; };
inline std::mutex &OpMutex() { static std::mutex m; return m; }
inline std::map<ws, std::shared_ptr<OpFlag>> &OpMap() { static std::map<ws, std::shared_ptr<OpFlag>> m; return m; }
struct OpScope {
    ws key; std::shared_ptr<OpFlag> f;
    explicit OpScope(const ws &k) : key(k), f(std::make_shared<OpFlag>()) { std::lock_guard<std::mutex> l(OpMutex()); OpMap()[key] = f; }
    ~OpScope() { std::lock_guard<std::mutex> l(OpMutex()); auto it = OpMap().find(key); if (it != OpMap().end() && it->second == f) OpMap().erase(it); }
    OpScope(const OpScope &) = delete; OpScope &operator=(const OpScope &) = delete;
};
inline bool CancelOp(const ws &key) {
    std::lock_guard<std::mutex> l(OpMutex());
    auto it = OpMap().find(key); if (it == OpMap().end()) return false;
    it->second->cancel = true; return true;
}

// ---- helpers ---------------------------------------------------------------------------------
inline void EnsureWsa() { static bool once = []() { WSADATA d; WSAStartup(MAKEWORD(2, 2), &d); return true; }(); (void)once; }

struct ComInit {   // per-thread COM for ShellExecuteEx
    bool ok = false;
    ComInit() { HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED); ok = SUCCEEDED(hr); }
    ~ComInit() { if (ok) CoUninitialize(); }
};

inline ws ErrText(DWORD code) {
    wchar_t *buf = nullptr;
    DWORD n = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code, 0, (LPWSTR)&buf, 0, nullptr);
    ws r; if (n && buf) r = Trim(ws(buf, n)); if (buf) LocalFree(buf);
    if (r.empty()) r = L"Error " + Num(code);
    return r;
}

inline ws AddrStr(const sockaddr *sa) {
    wchar_t b[96] = {0};
    if (!sa) return L"";
    if (sa->sa_family == AF_INET) { InetNtopW(AF_INET, (void *)&((const sockaddr_in *)sa)->sin_addr, b, 96); return b; }
    if (sa->sa_family == AF_INET6) {
        const sockaddr_in6 *s = (const sockaddr_in6 *)sa;
        InetNtopW(AF_INET6, (void *)&s->sin6_addr, b, 96);
        ws r = b; if (s->sin6_scope_id) r += L"%" + Num(s->sin6_scope_id);
        return r;
    }
    return L"";
}

// First A/AAAA address for host (like Dns.GetHostEntry(...).AddressList.First()). Returns WSA error or 0.
inline int ResolveFirst(const ws &host, sockaddr_storage &ss) {
    EnsureWsa();
    ADDRINFOW hints; memset(&hints, 0, sizeof(hints)); hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM;
    ADDRINFOW *res = nullptr;
    int rc = GetAddrInfoW(host.c_str(), nullptr, &hints, &res);
    if (rc != 0) return rc;
    int ret = WSAHOST_NOT_FOUND;
    for (ADDRINFOW *p = res; p; p = p->ai_next) {
        if ((p->ai_family == AF_INET || p->ai_family == AF_INET6) && p->ai_addrlen <= sizeof(ss)) {
            memset(&ss, 0, sizeof(ss)); memcpy(&ss, p->ai_addr, p->ai_addrlen); ret = 0; break;
        }
    }
    if (res) FreeAddrInfoW(res);
    return ret;
}

inline const wchar_t *StatusName(long s) {   // System.Net.NetworkInformation.IPStatus names
    switch (s) {
    case 0: return L"Success";
    case 11002: return L"DestinationNetworkUnreachable";
    case 11003: return L"DestinationHostUnreachable";
    case 11004: return L"DestinationProtocolUnreachable";
    case 11005: return L"DestinationPortUnreachable";
    case 11006: return L"NoResources";
    case 11007: return L"BadOption";
    case 11008: return L"HardwareError";
    case 11009: return L"PacketTooBig";
    case 11010: return L"TimedOut";
    case 11012: return L"BadRoute";
    case 11013: return L"TtlExpired";
    case 11014: return L"TtlReassemblyTimeExceeded";
    case 11015: return L"ParameterProblem";
    case 11016: return L"SourceQuench";
    case 11018: return L"BadDestination";
    case 11040: return L"DestinationUnreachable";
    case 11041: return L"TimeExceeded";
    case 11042: return L"BadHeader";
    case 11043: return L"UnrecognizedNextHeader";
    case 11044: return L"IcmpError";
    case 11045: return L"DestinationScopeMismatch";
    default: return L"Unknown";
    }
}

// ---- ICMP echo (IcmpSendEcho2 / Icmp6SendEcho2, cancellable) ----------------------------------
struct EchoResult {
    long status = -1;            // IP_STATUS code (0 success, 11010 timeout, 11013 ttl expired ...), -1 = send failed (Unknown)
    unsigned long rtt = 0;
    bool hasFrom = false; sockaddr_storage from; bool cancelled = false;
    EchoResult() { memset(&from, 0, sizeof(from)); }
};

struct Pinger {
    HANDLE h4 = INVALID_HANDLE_VALUE, h6 = INVALID_HANDLE_VALUE;
    ~Pinger() { if (h4 != INVALID_HANDLE_VALUE) IcmpCloseHandle(h4); if (h6 != INVALID_HANDLE_VALUE) IcmpCloseHandle(h6); }
    struct Ctx { HANDLE ev; char buf[512]; };

    EchoResult Send(const sockaddr_storage &dst, int ttl, bool dontFragment, DWORD timeoutMs, const std::atomic<bool> &cancel) {
        EchoResult r;
        const bool v6 = dst.ss_family == AF_INET6;
        HANDLE &h = v6 ? h6 : h4;
        if (h == INVALID_HANDLE_VALUE) { h = v6 ? Icmp6CreateFile() : IcmpCreateFile(); if (h == INVALID_HANDLE_VALUE) return r; }
        char payload[32]; memset(payload, 'a', sizeof(payload));   // 32 x 'a' like the C# buffer
        IP_OPTION_INFORMATION opt; memset(&opt, 0, sizeof(opt));
        opt.Ttl = (UCHAR)ttl; opt.Flags = dontFragment ? IP_FLAG_DF : 0;
        Ctx *c = new Ctx(); memset(c->buf, 0, sizeof(c->buf));
        c->ev = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!c->ev) { delete c; return r; }
        DWORD n;
        if (v6) {
            sockaddr_in6 src; memset(&src, 0, sizeof(src)); src.sin6_family = AF_INET6;
            sockaddr_in6 d6; memcpy(&d6, &dst, sizeof(d6));
            n = Icmp6SendEcho2(h, c->ev, nullptr, nullptr, &src, &d6, payload, (WORD)sizeof(payload), &opt, c->buf, (DWORD)sizeof(c->buf), timeoutMs);
        } else {
            n = IcmpSendEcho2(h, c->ev, nullptr, nullptr, ((const sockaddr_in *)&dst)->sin_addr.s_addr, payload, (WORD)sizeof(payload), &opt, c->buf, (DWORD)sizeof(c->buf), timeoutMs);
        }
        DWORD err = n ? 0 : GetLastError();
        if (!n && err != ERROR_IO_PENDING) {
            CloseHandle(c->ev); delete c;
            if (err >= 11000) r.status = (long)err;
            return r;
        }
        bool done = false; ULONGLONG deadline = GetTickCount64() + timeoutMs + 3000;
        for (;;) {
            if (WaitForSingleObject(c->ev, 50) == WAIT_OBJECT_0) { done = true; break; }
            if (cancel.load() || GetTickCount64() > deadline) break;
        }
        if (!done) {
            // cancelled (or stack never answered): closing the ICMP handle completes the pending request.
            // If it still does not complete, leak the buffer rather than let the stack write into freed memory.
            IcmpCloseHandle(h); h = INVALID_HANDLE_VALUE;
            if (WaitForSingleObject(c->ev, 1000) == WAIT_OBJECT_0) { CloseHandle(c->ev); delete c; }
            if (cancel.load()) r.cancelled = true; else r.status = 11010;
            return r;
        }
        if (v6) {
            DWORD cnt = Icmp6ParseReplies(c->buf, (DWORD)sizeof(c->buf));
            if (cnt == 0) { DWORD e = GetLastError(); r.status = e >= 11000 ? (long)e : -1; }
            else {
                ICMPV6_ECHO_REPLY *e = (ICMPV6_ECHO_REPLY *)c->buf;
                r.status = (long)e->Status; r.rtt = e->RoundTripTime;
                sockaddr_in6 f; memset(&f, 0, sizeof(f)); f.sin6_family = AF_INET6;
                memcpy(&f.sin6_addr, e->Address.sin6_addr, 16); f.sin6_scope_id = e->Address.sin6_scope_id;
                memcpy(&r.from, &f, sizeof(f)); r.hasFrom = true;
            }
        } else {
            DWORD cnt = IcmpParseReplies(c->buf, (DWORD)sizeof(c->buf));
            if (cnt == 0) { DWORD e = GetLastError(); r.status = e >= 11000 ? (long)e : -1; }
            else {
                ICMP_ECHO_REPLY *e = (ICMP_ECHO_REPLY *)c->buf;
                r.status = (long)e->Status; r.rtt = e->RoundTripTime;
                sockaddr_in f; memset(&f, 0, sizeof(f)); f.sin_family = AF_INET; f.sin_addr.s_addr = e->Address;
                memcpy(&r.from, &f, sizeof(f)); r.hasFrom = true;
            }
        }
        CloseHandle(c->ev); delete c;
        return r;
    }
};

inline bool SleepCancellable(DWORD ms, const std::atomic<bool> &cancel) {   // false if cancelled
    for (DWORD t = 0; t < ms; t += 50) { if (cancel.load()) return false; Sleep(50); }
    return !cancel.load();
}

// ---- ping_run ---------------------------------------------------------------------------------
inline ws PingRun(const Args &a) {
    ws host = Trim(Arg(a, L"host", L"8.8.8.8"));
    if (host.empty()) return Err(L"host is required");
    long long count = ArgInt(a, L"count", 4); if (count < 1) count = 1; if (count > 1000) count = 1000;
    ws opId = Arg(a, L"opId");
    long long toMs = ArgInt(a, L"timeoutMs", 4000), gapMs = ArgInt(a, L"intervalMs", 1000);       // the Wi-Fi / connection test uses short ones
    toMs = std::max(200LL, std::min(4000LL, toMs)); gapMs = std::max(0LL, std::min(1000LL, gapMs));
    OpScope scope(L"ping:" + opId);
    const std::atomic<bool> &cancel = scope.f->cancel;
    EnsureWsa();
    Pinger pinger;
    JArr replies; bool cancelled = false;
    int sent = 0, received = 0; long long minMs = -1, maxMs = -1, sumMs = 0;
    ws firstAddr;
    for (long long i = 1; i <= count; i++) {
        if (cancel.load()) { cancelled = true; break; }
        long status = -1; unsigned long rtt = 0; ws addr; bool haveAddr = false;
        sockaddr_storage dst; memset(&dst, 0, sizeof(dst));
        if (ResolveFirst(host, dst) == 0) {
            EchoResult e = pinger.Send(dst, 64, false, (DWORD)toMs, cancel);   // Ttl=64, timeout 4000 ms unless asked for less
            if (e.cancelled) { cancelled = true; break; }
            status = e.status; rtt = e.rtt;
            if (e.hasFrom) { addr = AddrStr((const sockaddr *)&e.from); haveAddr = true; }
        }
        sent++;
        JObj r; r.N(L"sequence", i).S(L"status", StatusName(status)).N(L"statusCode", status == 0 || status >= 11000 ? status : -1)
            .N(L"rttMs", status == 0 ? (long long)rtt : 0);
        if (haveAddr) r.S(L"address", addr); else r.Null(L"address");
        ws rj = r.Str();
        replies.R(rj);
        PushEvent(L"ping_reply", JObj().S(L"opId", opId).R(L"reply", rj).Str());
        if (status == 0) {
            received++; sumMs += rtt; if (minMs < 0 || (long long)rtt < minMs) minMs = rtt; if ((long long)rtt > maxMs) maxMs = rtt;
            if (firstAddr.empty()) firstAddr = addr;
        }
        if (i < count && !SleepCancellable((DWORD)gapMs, cancel)) { cancelled = true; break; }   // 1 s between probes unless asked for less
    }
    JObj o; o.S(L"host", host).B(L"cancelled", cancelled).N(L"sent", sent).N(L"received", received).N(L"lost", sent - received);
    if (received) o.N(L"minMs", minMs).N(L"maxMs", maxMs).D(L"avgMs", (double)sumMs / received, 2);
    else o.Null(L"minMs").Null(L"maxMs").Null(L"avgMs");
    o.R(L"replies", replies.Str());
    return o.Str();
}

// ---- trace_run --------------------------------------------------------------------------------
inline JObj HopObj(int hop, const ws *addr, const ws *name, long long rtt, bool timedOut, bool reached) {
    JObj h; h.N(L"hop", hop);
    if (addr) h.S(L"address", *addr); else h.Null(L"address");
    if (name) h.S(L"hostName", *name); else h.Null(L"hostName");
    if (rtt >= 0) h.N(L"rttMs", rtt); else h.Null(L"rttMs");
    h.B(L"timedOut", timedOut).B(L"reachedDestination", reached);
    return h;
}

inline ws TraceRun(const Args &a) {
    ws host = Trim(Arg(a, L"host", L"8.8.8.8"));
    if (host.empty()) return Err(L"host is required");
    long long maxHops = ArgInt(a, L"maxHops", 30); if (maxHops < 1) maxHops = 1; if (maxHops > 255) maxHops = 255;
    ws opId = Arg(a, L"opId");
    OpScope scope(L"trace:" + opId);
    const std::atomic<bool> &cancel = scope.f->cancel;
    EnsureWsa();
    JArr hops; bool cancelled = false, reached = false, resolved = false; ws destStr;
    auto report = [&](const JObj &h) { ws j = h.Str(); hops.R(j); PushEvent(L"trace_hop", JObj().S(L"opId", opId).R(L"hop", j).Str()); };

    sockaddr_storage dst; memset(&dst, 0, sizeof(dst));
    if (ResolveFirst(host, dst) != 0) {
        report(HopObj(0, nullptr, nullptr, -1, true, false));   // C#: hop 0, timed out, then stop
    } else {
        resolved = true; destStr = AddrStr((const sockaddr *)&dst);
        Pinger pinger;
        for (int ttl = 1; ttl <= maxHops; ttl++) {
            if (cancel.load()) { cancelled = true; break; }
            EchoResult e = pinger.Send(dst, ttl, true, 4000, cancel);   // DontFragment, 4000 ms, 32-byte payload
            if (e.cancelled) { cancelled = true; break; }
            if ((e.status == 0 || e.status == 11013) && e.hasFrom) {
                ws addr = AddrStr((const sockaddr *)&e.from);
                ws name; bool haveName = false;
                wchar_t hb[NI_MAXHOST]; hb[0] = 0;
                if (GetNameInfoW((const sockaddr *)&e.from, e.from.ss_family == AF_INET6 ? (int)sizeof(sockaddr_in6) : (int)sizeof(sockaddr_in), hb, NI_MAXHOST, nullptr, 0, NI_NAMEREQD) == 0 && hb[0]) { name = hb; haveName = true; }
                bool done = e.status == 0;
                report(HopObj(ttl, &addr, haveName ? &name : nullptr, (long long)e.rtt, false, done));
                if (done) { reached = true; break; }
            } else {
                report(HopObj(ttl, nullptr, nullptr, -1, true, false));
            }
        }
    }
    JObj o; o.S(L"host", host).B(L"resolved", resolved);
    if (resolved) o.S(L"destination", destStr); else o.Null(L"destination");
    o.B(L"reached", reached).B(L"cancelled", cancelled).R(L"hops", hops.Str());
    return o.Str();
}

// ---- dns_lookup -------------------------------------------------------------------------------
inline ws OpFail(const ws &user, const ws &tech) {
    JObj o; o.B(L"success", false).S(L"userMessage", user);
    if (tech.empty()) o.Null(L"technicalDetail"); else o.S(L"technicalDetail", tech);
    return o.Str();
}
inline ws OpOk(const ws &msg = L"") {
    JObj o; o.B(L"success", true);
    if (msg.empty()) o.Null(L"userMessage"); else o.S(L"userMessage", msg);
    o.Null(L"technicalDetail"); return o.Str();
}

inline ws DnsLookup(const Args &a) {
    ws host = Trim(Arg(a, L"host", L"example.com"));
    if (host.empty()) return Err(L"host is required");
    EnsureWsa();
    ws quoted = L"Could not resolve \"" + host + L"\".";
    ws hostName; std::vector<ws> addrs;
    IN6_ADDR t6 = {}; IN_ADDR t4 = {};
    bool literal = InetPtonW(AF_INET, host.c_str(), &t4) == 1 || InetPtonW(AF_INET6, host.c_str(), &t6) == 1;
    if (literal) {
        // IP literal: reverse lookup for the name, then forward lookup of that name (like Dns.GetHostEntry(string))
        sockaddr_storage ss; memset(&ss, 0, sizeof(ss)); int len;
        if (InetPtonW(AF_INET, host.c_str(), &t4) == 1) { sockaddr_in *s = (sockaddr_in *)&ss; s->sin_family = AF_INET; s->sin_addr = t4; len = (int)sizeof(sockaddr_in); }
        else { sockaddr_in6 *s = (sockaddr_in6 *)&ss; s->sin6_family = AF_INET6; s->sin6_addr = t6; len = (int)sizeof(sockaddr_in6); }
        wchar_t hb[NI_MAXHOST]; hb[0] = 0;
        int rc = GetNameInfoW((const sockaddr *)&ss, len, hb, NI_MAXHOST, nullptr, 0, NI_NAMEREQD);
        if (rc != 0) return OpFail(quoted, ErrText((DWORD)(rc ? rc : WSAHOST_NOT_FOUND)));
        hostName = hb; addrs.push_back(host);
    } else {
        ADDRINFOW hints; memset(&hints, 0, sizeof(hints)); hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM; hints.ai_flags = AI_CANONNAME;
        ADDRINFOW *res = nullptr;
        int rc = GetAddrInfoW(host.c_str(), nullptr, &hints, &res);
        if (rc != 0) return OpFail(quoted, ErrText((DWORD)rc));
        for (ADDRINFOW *p = res; p; p = p->ai_next) {
            if (p == res && p->ai_canonname) hostName = p->ai_canonname;
            if (p->ai_family == AF_INET || p->ai_family == AF_INET6) {
                ws s = AddrStr(p->ai_addr);
                if (!s.empty()) addrs.push_back(s);
            }
        }
        if (res) FreeAddrInfoW(res);
        if (hostName.empty()) hostName = host;
    }
    if (addrs.empty()) return OpFail(L"No records found for \"" + host + L"\".", L"");
    JArr arr; for (auto &s : addrs) arr.S(s);
    return JObj().B(L"success", true).Null(L"userMessage").R(L"value", JObj().S(L"hostName", hostName).R(L"addresses", arr.Str()).Str()).Str();
}

// ---- ipcfg_get --------------------------------------------------------------------------------
inline ws IpCfgGet() {
    std::vector<char> buf; ULONG sz = 32768, rc = ERROR_BUFFER_OVERFLOW;
    for (int tries = 0; tries < 6 && rc == ERROR_BUFFER_OVERFLOW; tries++) {
        buf.assign(sz + 16, 0);
        rc = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_GATEWAYS, nullptr, (PIP_ADAPTER_ADDRESSES)buf.data(), &sz);
    }
    if (rc == ERROR_NO_DATA) return L"[]";
    if (rc != NO_ERROR) return Err(L"GetAdaptersAddresses failed: " + ErrText(rc));
    struct Item { bool up; ws json; };
    std::vector<Item> items;
    for (PIP_ADAPTER_ADDRESSES p = (PIP_ADAPTER_ADDRESSES)buf.data(); p; p = p->Next) {
        if (p->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;   // C# excludes NetworkInterfaceType.Loopback
        JArr v4, v6, gw, dns;
        for (auto u = p->FirstUnicastAddress; u; u = u->Next) {
            if (!u->Address.lpSockaddr) continue;
            if (u->Address.lpSockaddr->sa_family == AF_INET) v4.S(AddrStr(u->Address.lpSockaddr));
            else if (u->Address.lpSockaddr->sa_family == AF_INET6) v6.S(AddrStr(u->Address.lpSockaddr));
        }
        for (auto g = p->FirstGatewayAddress; g; g = g->Next) if (g->Address.lpSockaddr) gw.S(AddrStr(g->Address.lpSockaddr));
        for (auto d = p->FirstDnsServerAddress; d; d = d->Next) if (d->Address.lpSockaddr) dns.S(AddrStr(d->Address.lpSockaddr));
        JObj o;
        o.S(L"name", p->FriendlyName ? p->FriendlyName : L"").S(L"description", p->Description ? p->Description : L"");
        if (p->PhysicalAddressLength == 0) o.Null(L"macAddress");
        else {
            ws mac; for (ULONG i = 0; i < p->PhysicalAddressLength && i < MAX_ADAPTER_ADDRESS_LENGTH; i++) { wchar_t b[8]; swprintf(b, 8, L"%02X", (unsigned)p->PhysicalAddress[i]); if (i) mac += L"-"; mac += b; }
            o.S(L"macAddress", mac);
        }
        bool dhcp = (p->Flags & IP_ADAPTER_IPV4_ENABLED) && (p->Flags & IP_ADAPTER_DHCP_ENABLED);
        o.R(L"ipv4Addresses", v4.Str()).R(L"ipv6Addresses", v6.Str()).R(L"gateways", gw.Str()).R(L"dnsServers", dns.Str())
            .B(L"dhcpEnabled", dhcp).B(L"isUp", p->OperStatus == IfOperStatusUp);
        items.push_back({p->OperStatus == IfOperStatusUp, o.Str()});
    }
    std::stable_sort(items.begin(), items.end(), [](const Item &x, const Item &y) { return x.up && !y.up; });   // up adapters first
    JArr arr; for (auto &i : items) arr.R(i.json);
    return arr.Str();
}

// ---- netmaint_* -------------------------------------------------------------------------------
inline ws MaintNormal(const ws &exe, const ws &args, const ws &failMsg) {   // non-elevated, output captured
    DWORD code = (DWORD)-1;
    ws out = RunCapture(L"\"" + SysDir() + L"\\" + exe + L"\" " + args, &code, 60000);
    if (code == 0) return OpOk();
    if (code == (DWORD)-1 && out.empty()) return OpFail(failMsg, L"Could not start " + exe + L".");
    return OpFail(failMsg, Trim(out));
}
inline ws MaintElevated(const ws &exe, const ws &args, const ws &failMsg) {   // UAC "runas", waits for exit
    ComInit com;
    SHELLEXECUTEINFOW sei; memset(&sei, 0, sizeof(sei));
    ws file = SysDir() + L"\\" + exe;
    sei.cbSize = sizeof(sei); sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    sei.lpVerb = L"runas"; sei.lpFile = file.c_str(); sei.lpParameters = args.c_str(); sei.nShow = SW_HIDE;
    if (!ShellExecuteExW(&sei)) return OpFail(failMsg, ErrText(GetLastError()));   // e.g. UAC declined: "The operation was canceled by the user."
    if (!sei.hProcess) return OpOk();
    WaitForSingleObject(sei.hProcess, INFINITE);
    DWORD code = 1; GetExitCodeProcess(sei.hProcess, &code); CloseHandle(sei.hProcess);
    return code == 0 ? OpOk() : OpFail(failMsg, L"Exit code " + Num(code) + L".");
}

// ---- Ports: TCP connections and UDP sockets, each with the process that owns it ----------------------------------------
inline ws PortsList() {
    std::map<DWORD, ws> names;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe; pe.dwSize = sizeof(pe);
        for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe)) {
            ws n = pe.szExeFile; size_t d = n.size();
            if (d > 4 && _wcsicmp(n.c_str() + d - 4, L".exe") == 0) n.resize(d - 4);
            names[pe.th32ProcessID] = n;
        }
        CloseHandle(snap);
    }
    names[0].clear();                                                           // pid 0 owns connections that are closing
    static const wchar_t *const ST[] = { L"", L"Closed", L"Listening", L"SYN sent", L"SYN received", L"Established", L"FIN wait 1", L"FIN wait 2", L"Close wait", L"Closing", L"Last ACK", L"Time wait", L"Closing" };
    JArr arr; bool any = false; std::vector<char> buf;
    auto addr = [](int af, const void *a) { wchar_t b[64] = L""; InetNtopW(af, (void *)a, b, 64); return ws(b); };
    auto add = [&](const wchar_t *proto, const ws &l, DWORD lp, const ws &r, DWORD rp, const wchar_t *state, DWORD pid) {
        auto it = names.find(pid);
        JObj o; o.S(L"proto", proto).S(L"local", l).N(L"localPort", ntohs((u_short)lp)).S(L"remote", r).N(L"remotePort", r.empty() ? 0 : ntohs((u_short)rp))
            .S(L"state", state).N(L"pid", pid).S(L"process", it == names.end() ? ws() : it->second);
        arr.Obj(o);
    };
    auto fetch = [&](auto call) {
        DWORD sz = 0; DWORD r = call(nullptr, &sz);
        for (int i = 0; i < 5 && r == ERROR_INSUFFICIENT_BUFFER; i++) { buf.assign((size_t)sz + 8192, 0); sz = (DWORD)buf.size(); r = call(buf.data(), &sz); }
        if (r == NO_ERROR) any = true;
        return r == NO_ERROR;
    };
    if (fetch([](void *p, DWORD *sz) { return GetExtendedTcpTable(p, sz, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0); })) {
        auto *t = (MIB_TCPTABLE_OWNER_PID *)buf.data();
        for (DWORD i = 0; i < t->dwNumEntries; i++) {
            const auto &r = t->table[i]; DWORD s = r.dwState < 13 ? r.dwState : 0;
            add(L"TCP", addr(AF_INET, &r.dwLocalAddr), r.dwLocalPort, s == 2 ? ws() : addr(AF_INET, &r.dwRemoteAddr), r.dwRemotePort, ST[s], r.dwOwningPid);
        }
    }
    if (fetch([](void *p, DWORD *sz) { return GetExtendedTcpTable(p, sz, FALSE, AF_INET6, TCP_TABLE_OWNER_PID_ALL, 0); })) {
        auto *t = (MIB_TCP6TABLE_OWNER_PID *)buf.data();
        for (DWORD i = 0; i < t->dwNumEntries; i++) {
            const auto &r = t->table[i]; DWORD s = r.dwState < 13 ? r.dwState : 0;
            add(L"TCP", addr(AF_INET6, r.ucLocalAddr), r.dwLocalPort, s == 2 ? ws() : addr(AF_INET6, r.ucRemoteAddr), r.dwRemotePort, ST[s], r.dwOwningPid);
        }
    }
    if (fetch([](void *p, DWORD *sz) { return GetExtendedUdpTable(p, sz, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0); })) {
        auto *t = (MIB_UDPTABLE_OWNER_PID *)buf.data();
        for (DWORD i = 0; i < t->dwNumEntries; i++) { const auto &r = t->table[i]; add(L"UDP", addr(AF_INET, &r.dwLocalAddr), r.dwLocalPort, ws(), 0, L"Listening", r.dwOwningPid); }
    }
    if (fetch([](void *p, DWORD *sz) { return GetExtendedUdpTable(p, sz, FALSE, AF_INET6, UDP_TABLE_OWNER_PID, 0); })) {
        auto *t = (MIB_UDP6TABLE_OWNER_PID *)buf.data();
        for (DWORD i = 0; i < t->dwNumEntries; i++) { const auto &r = t->table[i]; add(L"UDP", addr(AF_INET6, r.ucLocalAddr), r.dwLocalPort, ws(), 0, L"Listening", r.dwOwningPid); }
    }
    if (!any) return Err(L"Couldn't read the list of open ports.");
    return arr.Str();
}

// ---- Wi-Fi: current connection, nearby networks and saved profiles (wlanapi, loaded at run time) --------------------
// wlanapi.dll is absent on some editions (Windows Server without the WLAN feature), so it is loaded on demand instead of linked.
struct WlanApi {
    HMODULE m = nullptr;
    DWORD (WINAPI *Open)(DWORD, void *, DWORD *, HANDLE *) = nullptr;
    DWORD (WINAPI *Close)(HANDLE, void *) = nullptr;
    DWORD (WINAPI *Enum)(HANDLE, void *, WLAN_INTERFACE_INFO_LIST **) = nullptr;
    DWORD (WINAPI *Query)(HANDLE, const GUID *, int, void *, DWORD *, void **, int *) = nullptr;
    DWORD (WINAPI *Bss)(HANDLE, const GUID *, void *, int, BOOL, void *, WLAN_BSS_LIST **) = nullptr;
    DWORD (WINAPI *Avail)(HANDLE, const GUID *, DWORD, void *, WLAN_AVAILABLE_NETWORK_LIST **) = nullptr;
    DWORD (WINAPI *Profiles)(HANDLE, const GUID *, void *, WLAN_PROFILE_INFO_LIST **) = nullptr;
    DWORD (WINAPI *Scan)(HANDLE, const GUID *, void *, void *, void *) = nullptr;
    void (WINAPI *Free)(void *) = nullptr;
    bool ok() const { return m && Open && Close && Enum && Query && Bss && Avail && Profiles && Scan && Free; }
};
inline const WlanApi &Wlan() {
    static WlanApi w = []() {
        WlanApi a; a.m = LoadLibraryExW(L"wlanapi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32); if (!a.m) return a;
        auto get = [&](const char *n) { return (void *)GetProcAddress(a.m, n); };
        a.Open = (decltype(a.Open))get("WlanOpenHandle"); a.Close = (decltype(a.Close))get("WlanCloseHandle"); a.Enum = (decltype(a.Enum))get("WlanEnumInterfaces");
        a.Query = (decltype(a.Query))get("WlanQueryInterface"); a.Bss = (decltype(a.Bss))get("WlanGetNetworkBssList"); a.Avail = (decltype(a.Avail))get("WlanGetAvailableNetworkList");
        a.Profiles = (decltype(a.Profiles))get("WlanGetProfileList"); a.Scan = (decltype(a.Scan))get("WlanScan"); a.Free = (decltype(a.Free))get("WlanFreeMemory");
        return a; }();
    return w;
}
inline ws SsidText(const DOT11_SSID &s) { return FromCp(std::string((const char *)s.ucSSID, std::min<ULONG>(s.uSSIDLength, 32)), CP_UTF8); }
inline ws MacText(const unsigned char *b) { wchar_t t[24]; swprintf_s(t, L"%02X:%02X:%02X:%02X:%02X:%02X", b[0], b[1], b[2], b[3], b[4], b[5]); return t; }
inline int ChannelOf(ULONG khz) {                                              // centre frequency (kHz) -> channel number
    long long f = khz / 1000;
    if (f == 2484) return 14; if (f >= 2412 && f <= 2472) return (int)((f - 2407) / 5);
    if (f >= 5160 && f <= 5885) return (int)((f - 5000) / 5);
    if (f >= 5955 && f <= 7115) return (int)((f - 5950) / 5);
    return 0;
}
inline const wchar_t *BandOf(ULONG khz) { long long f = khz / 1000; return f < 3000 ? L"2.4 GHz" : f < 5925 ? L"5 GHz" : L"6 GHz"; }
inline ws PhyText(int p) {
    switch (p) { case 4: return L"802.11a"; case 5: return L"802.11b"; case 6: return L"802.11g"; case 7: return L"802.11n (Wi-Fi 4)"; case 8: return L"802.11ac (Wi-Fi 5)";
        case 9: return L"802.11ad"; case 10: return L"802.11ax (Wi-Fi 6)"; case 11: return L"802.11be (Wi-Fi 7)"; default: return L""; }
}
inline ws AuthText(int a) {
    switch (a) { case 1: return L"Open (no password)"; case 2: return L"WEP"; case 3: return L"WPA-Enterprise"; case 4: return L"WPA-Personal"; case 5: return L"WPA-None";
        case 6: return L"WPA2-Enterprise"; case 7: return L"WPA2-Personal"; case 8: return L"WPA3-Enterprise 192-bit"; case 9: return L"WPA3-Personal"; case 10: return L"Enhanced Open"; case 11: return L"WPA3-Enterprise"; default: return L"Unknown"; }
}
inline ws CipherText(int c) {
    switch (c) { case 0: return L"none"; case 1: case 5: return L"WEP"; case 2: return L"TKIP"; case 4: return L"AES"; case 6: return L"GCMP"; default: return L""; }
}
inline const wchar_t *StateText(int s) {
    switch (s) { case 1: return L"Connected"; case 2: return L"Ad hoc network"; case 3: return L"Disconnecting"; case 4: return L"Not connected"; case 5: return L"Connecting"; case 6: return L"Looking for networks"; case 7: return L"Signing in"; default: return L"Not ready"; }
}

inline ws WifiInfo(bool rescan) {
    const WlanApi &w = Wlan();
    auto none = [](const wchar_t *why) { return JObj().B(L"available", false).S(L"reason", why).Str(); };
    if (!w.ok()) return none(L"Wi-Fi isn't available on this PC.");
    DWORD ver = 0; HANDLE h = nullptr;
    if (w.Open(2, nullptr, &ver, &h) != ERROR_SUCCESS || !h) return none(L"Wi-Fi isn't available on this PC. The WLAN AutoConfig service may be turned off.");
    WLAN_INTERFACE_INFO_LIST *il = nullptr;
    if (w.Enum(h, nullptr, &il) != ERROR_SUCCESS || !il || il->dwNumberOfItems == 0) { if (il) w.Free(il); w.Close(h, nullptr); return none(L"No Wi-Fi adapter was found."); }
    if (rescan) { for (DWORD i = 0; i < il->dwNumberOfItems; i++) w.Scan(h, &il->InterfaceInfo[i].InterfaceGuid, nullptr, nullptr, nullptr); Sleep(2500); }   // results arrive a moment after the request
    JArr adapters;
    for (DWORD i = 0; i < il->dwNumberOfItems; i++) {
        const WLAN_INTERFACE_INFO &ii = il->InterfaceInfo[i];
        JObj ad; ad.S(L"description", ii.strInterfaceDescription).S(L"state", StateText((int)ii.isState)).B(L"connected", ii.isState == wlan_interface_state_connected);
        // all access points in range (channel, signal), used for the connection details and the channel crowding figure
        WLAN_BSS_LIST *bl = nullptr; w.Bss(h, &ii.InterfaceGuid, nullptr, 3 /*dot11_BSS_type_any*/, FALSE, nullptr, &bl);
        // current connection
        WLAN_CONNECTION_ATTRIBUTES *ca = nullptr; DWORD sz = 0; int vt = 0; ws curSsid; unsigned char curBssid[6] = {}; ULONG curFreq = 0;
        if (ii.isState == wlan_interface_state_connected && w.Query(h, &ii.InterfaceGuid, 7 /*wlan_intf_opcode_current_connection*/, nullptr, &sz, (void **)&ca, &vt) == ERROR_SUCCESS && ca) {
            const WLAN_ASSOCIATION_ATTRIBUTES &as = ca->wlanAssociationAttributes; const WLAN_SECURITY_ATTRIBUTES &se = ca->wlanSecurityAttributes;
            curSsid = SsidText(as.dot11Ssid); memcpy(curBssid, as.dot11Bssid, 6);
            long rssi = 0; bool haveRssi = false; int sameChan = 0; ULONG freq = 0;
            if (bl) for (DWORD k = 0; k < bl->dwNumberOfItems; k++) if (memcmp(bl->wlanBssEntries[k].dot11Bssid, curBssid, 6) == 0) { rssi = bl->wlanBssEntries[k].lRssi; freq = bl->wlanBssEntries[k].ulChCenterFrequency; haveRssi = true; break; }
            curFreq = freq;
            if (bl && freq) for (DWORD k = 0; k < bl->dwNumberOfItems; k++) { const WLAN_BSS_ENTRY &e = bl->wlanBssEntries[k]; if (e.ulChCenterFrequency == freq && memcmp(e.dot11Bssid, curBssid, 6) != 0 && e.lRssi > -85) sameChan++; }
            ws sec = AuthText((int)se.dot11AuthAlgorithm); ws ci = CipherText((int)se.dot11CipherAlgorithm); if (!ci.empty() && ci != L"none") sec += L", " + ci;
            JObj c; c.S(L"ssid", curSsid).S(L"bssid", MacText(as.dot11Bssid)).N(L"signal", (long long)as.wlanSignalQuality).S(L"phy", PhyText((int)as.dot11PhyType))
                .N(L"rxMbps", (long long)(as.ulRxRate / 1000)).N(L"txMbps", (long long)(as.ulTxRate / 1000)).S(L"security", sec).S(L"profile", ca->strProfileName)
                .B(L"weakSecurity", se.dot11AuthAlgorithm == 1 || se.dot11AuthAlgorithm == 2 || se.dot11AuthAlgorithm == 4 || se.dot11CipherAlgorithm == 1 || se.dot11CipherAlgorithm == 2 || se.dot11CipherAlgorithm == 5);
            if (haveRssi) c.N(L"rssiDbm", rssi); else c.Null(L"rssiDbm");
            if (freq) c.N(L"channel", ChannelOf(freq)).S(L"band", BandOf(freq)).N(L"frequencyMhz", (long long)(freq / 1000)).N(L"othersOnChannel", sameChan);
            ad.R(L"connection", c.Str());
            w.Free(ca);
        } else ad.Null(L"connection");
        // nearby networks (one row per name, strongest access point wins)
        struct Net { ws ssid, sec; int signal = 0, channel = 0; const wchar_t *band = L""; bool secure = false, connected = false, saved = false; };
        std::vector<Net> nets;
        WLAN_AVAILABLE_NETWORK_LIST *al = nullptr;
        if (w.Avail(h, &ii.InterfaceGuid, 0, nullptr, &al) == ERROR_SUCCESS && al) {
            for (DWORD k = 0; k < al->dwNumberOfItems; k++) {
                const WLAN_AVAILABLE_NETWORK &n = al->Network[k]; ws s = SsidText(n.dot11Ssid); if (s.empty()) continue;
                Net *t = nullptr; for (auto &x : nets) if (x.ssid == s) { t = &x; break; }
                if (!t) { nets.emplace_back(); t = &nets.back(); t->ssid = s; }
                t->signal = std::max(t->signal, (int)n.wlanSignalQuality); t->secure = t->secure || n.bSecurityEnabled; t->connected = t->connected || (n.dwFlags & 1) != 0; t->saved = t->saved || (n.dwFlags & 2) != 0;
                if (t->sec.empty()) t->sec = n.bSecurityEnabled ? AuthText((int)n.dot11DefaultAuthAlgorithm) : ws(L"Open (no password)");
            }
            w.Free(al);
        }
        if (bl) for (auto &x : nets) { long best = -1000; for (DWORD k = 0; k < bl->dwNumberOfItems; k++) { const WLAN_BSS_ENTRY &e = bl->wlanBssEntries[k]; if (SsidText(e.dot11Ssid) == x.ssid && e.lRssi > best) { best = e.lRssi; x.channel = ChannelOf(e.ulChCenterFrequency); x.band = BandOf(e.ulChCenterFrequency); } } }
        std::sort(nets.begin(), nets.end(), [](const Net &a, const Net &b) { return a.connected != b.connected ? a.connected : a.signal > b.signal; });
        JArr na; for (size_t k = 0; k < nets.size() && k < 40; k++) { const Net &x = nets[k];
            JObj o; o.S(L"ssid", x.ssid).N(L"signal", x.signal).B(L"secure", x.secure).S(L"security", x.sec).B(L"connected", x.connected).B(L"saved", x.saved);
            if (x.channel) o.N(L"channel", x.channel).S(L"band", x.band); na.Obj(o); }
        ad.R(L"networks", na.Str());
        // saved networks
        JArr pa; WLAN_PROFILE_INFO_LIST *pl = nullptr;
        if (w.Profiles(h, &ii.InterfaceGuid, nullptr, &pl) == ERROR_SUCCESS && pl) { for (DWORD k = 0; k < pl->dwNumberOfItems && k < 100; k++) pa.S(pl->ProfileInfo[k].strProfileName); w.Free(pl); }
        ad.R(L"profiles", pa.Str());
        if (bl) w.Free(bl);
        adapters.Obj(ad);
    }
    w.Free(il); w.Close(h, nullptr);
    return JObj().B(L"available", true).R(L"adapters", adapters.Str()).Str();
}

} // namespace wknet

inline bool Mod_Network(const ws &cmd, const Args &a, ws &out) {
    using namespace wknet;
    try {
        if (cmd == L"ping_run") { out = PingRun(a); return true; }
        if (cmd == L"ping_cancel") { out = Bool(CancelOp(L"ping:" + Arg(a, L"opId"))); return true; }
        if (cmd == L"trace_run") { out = TraceRun(a); return true; }
        if (cmd == L"trace_cancel") { out = Bool(CancelOp(L"trace:" + Arg(a, L"opId"))); return true; }
        if (cmd == L"dns_lookup") { out = DnsLookup(a); return true; }
        if (cmd == L"ipcfg_get") { out = IpCfgGet(); return true; }
        if (cmd == L"ports_list") { out = PortsList(); return true; }
        if (cmd == L"wifi_info") { out = WifiInfo(ArgBool(a, L"rescan", false)); return true; }
        if (cmd == L"netmaint_flushdns") { out = MaintNormal(L"ipconfig.exe", L"/flushdns", L"Unable to flush the DNS cache."); return true; }
        if (cmd == L"netmaint_dhcprenew") { out = MaintElevated(L"ipconfig.exe", L"/renew", L"Unable to renew the DHCP lease."); return true; }
        if (cmd == L"netmaint_dhcprelease") { out = MaintElevated(L"ipconfig.exe", L"/release", L"Unable to release the DHCP lease."); return true; }
        if (cmd == L"netmaint_winsockreset") { out = MaintElevated(L"netsh.exe", L"winsock reset", L"Unable to reset Winsock."); return true; }
        if (cmd == L"netmaint_tcpipreset") { out = MaintElevated(L"netsh.exe", L"int ip reset", L"Unable to reset the TCP/IP stack."); return true; }
    } catch (...) {
        out = Err(L"internal error in network module"); return true;
    }
    return false;
}

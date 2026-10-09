// mod_network.h - port of WinKit.Network (ping, traceroute, DNS lookup, IP configuration, maintenance).
// See API_network.md. Header-only; commands: ping_run ping_cancel trace_run trace_cancel dns_lookup
// ipcfg_get netmaint_flushdns netmaint_dhcprenew netmaint_dhcprelease netmaint_winsockreset netmaint_tcpipreset
#pragma once
#include "common.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>
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
            EchoResult e = pinger.Send(dst, 64, false, 4000, cancel);   // Ttl=64, timeout 4000 ms
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
        if (i < count && !SleepCancellable(1000, cancel)) { cancelled = true; break; }   // 1 s between probes
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

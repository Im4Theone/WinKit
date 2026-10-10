// packui.cpp - build-time helper: compresses index.html into index.pack with the Windows Compression API (LZMS), which is built into
// Windows 8 and later. WinKit unpacks it at start-up with the same API, so the exe carries about a fifth of the page's size.
//   packui index.html index.pack
#include <windows.h>
#include <compressapi.h>
#include <cstdio>
#include <vector>
#pragma comment(lib, "cabinet.lib")

int wmain(int argc, wchar_t **argv) {
    if (argc < 3) { fwprintf(stderr, L"usage: packui <in> <out>\n"); return 2; }
    FILE *f = _wfopen(argv[1], L"rb"); if (!f) { fwprintf(stderr, L"cannot read %s\n", argv[1]); return 1; }
    std::vector<char> in; char buf[65536]; size_t n; while ((n = fread(buf, 1, sizeof(buf), f)) > 0) in.insert(in.end(), buf, buf + n); fclose(f);
    COMPRESSOR_HANDLE c = nullptr;
    if (!CreateCompressor(COMPRESS_ALGORITHM_LZMS, nullptr, &c)) { fwprintf(stderr, L"CreateCompressor failed (%lu)\n", GetLastError()); return 1; }
    SIZE_T need = 0; Compress(c, in.data(), in.size(), nullptr, 0, &need);
    if (!need) { fwprintf(stderr, L"compress sizing failed (%lu)\n", GetLastError()); return 1; }
    std::vector<char> out(need); SIZE_T got = 0;
    if (!Compress(c, in.data(), in.size(), out.data(), out.size(), &got)) { fwprintf(stderr, L"Compress failed (%lu)\n", GetLastError()); return 1; }
    CloseCompressor(c);
    f = _wfopen(argv[2], L"wb"); if (!f) { fwprintf(stderr, L"cannot write %s\n", argv[2]); return 1; }
    fwrite(out.data(), 1, got, f); fclose(f);
    wprintf(L"index.pack %u KB (page %u KB)\n", (unsigned)(got / 1024), (unsigned)(in.size() / 1024));
    return 0;
}

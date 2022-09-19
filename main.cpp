#include <stdio.h>
#include <io.h>

#include <string>
#include <iostream>
#include <exception>

#include "lsp.h"
#include "HttpClient.h"

#pragma comment(lib, "crypt32")
#pragma comment(lib, "libraries/libssl")
#pragma comment(lib, "libraries/libcrypto")

#ifndef F_OK 
#define F_OK 0
#endif

#ifndef LIBCOR32_API
#ifdef __cplusplus 
#define LIBCOR32_API extern "C" __declspec(dllexport)
#else
#define LIBCOR32_API extern
#endif
#endif

#pragma warning(disable: 4297)

typedef void(*libcor_http_read)(int status, void* buf, uint32_t length);

LIBCOR32_API int
libcor_http(bool post, const char* host, const char* path, const char* cacert_path, const char* data, size_t size, libcor_http_read http_read) noexcept {
    if (!host || !http_read) {
        return -1;
    }

    if (!data && size) {
        return -1;
    }

    size_t host_size = strlen(host);
    if (host_size < 7) {
        return -1;
    }

    if (_strnicmp(host, "http://", 7) != 0) {
        if (host_size < 8) {
            return -1;
        }

        if (_strnicmp(host, "https://", 8) != 0) {
            return -1;
        }
    }

    if (!path || (int)strlen(path) < 1) {
        path = "/";
    }

    if (*path != '/') {
        return -1;
    }

    int status = 0;
    if (!cacert_path) {
        cacert_path = "cacert.pem";
    }

    std::string content;
    if (post) {
        HttpClient http = HttpClient(host, cacert_path);
        content = http.Post(path, data, size, status);
    }
    else {
        HttpClient http = HttpClient(host, cacert_path);
        content = http.Get(path, status);
    }

    http_read(status, (void*)content.data(), content.size());
    return status;
}

LIBCOR32_API bool
libcor_is_install_paperairplane(bool b32) noexcept {
    return IsInstallProvider(b32);
}

LIBCOR32_API bool
libcor_install_paperairplane(wchar_t* path, bool b32) noexcept {
    return InstallProvider(path, b32);
}

LIBCOR32_API bool
libcor_uninstall_paperairplane(bool b32) noexcept {
    return UninstallProvider(b32);
}

BOOL APIENTRY
DllMain(
    HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved) noexcept {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        break;
    case DLL_PROCESS_DETACH:
        break;
    case DLL_THREAD_ATTACH:
        break;
    case DLL_THREAD_DETACH:
        break;
    };
    return TRUE;
}
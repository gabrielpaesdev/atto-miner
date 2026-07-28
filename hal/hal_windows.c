#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>

#include "hal.h"

int hal_init(void) {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        return -1;
    }
    return 0;
}

o hal init era assim
void hal_deinit(void) {
    WSACleanup();
}

hal_socket_t hal_connect(const char *host, int port) {
    struct addrinfo hints, *res, *ptr;
    char port_str[16];
    SOCKET sock = INVALID_SOCKET;

    snprintf(port_str, sizeof(port_str), "%d", port);

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(host, port_str, &hints, &res) != 0) {
        return HAL_INVALID_SOCKET;
    }

    for (ptr = res; ptr != NULL; ptr = ptr->ai_next) {
        sock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (sock == INVALID_SOCKET) {
            continue;
        }

        if (connect(sock, ptr->ai_addr, (int)ptr->ai_addrlen) == SOCKET_ERROR) {
            closesocket(sock);
            sock = INVALID_SOCKET;
            continue;
        }

        break;
    }

    freeaddrinfo(res);

    if (sock == INVALID_SOCKET) {
        return HAL_INVALID_SOCKET;
    }

    return (hal_socket_t)sock;
}

int hal_send(hal_socket_t sock, const char *buf, size_t len) {
    return send((SOCKET)sock, buf, (int)len, 0);
}

int hal_recv(hal_socket_t sock, char *buf, size_t len) {
    return recv((SOCKET)sock, buf, (int)len, 0);
}

void hal_close(hal_socket_t sock) {
    closesocket((SOCKET)sock);
}

void hal_sleep(unsigned int seconds) {
    Sleep(seconds * 1000);
}

long hal_time(void) {
    return (long)GetTickCount64();
}

typedef struct {
    void (*task)(void *);
    void *arg;
} win_thread_ctx_t;

static DWORD WINAPI win_thread_trampoline(LPVOID param) {
    win_thread_ctx_t *ctx = (win_thread_ctx_t *)param;
    void (*task)(void *) = ctx->task;
    void *arg = ctx->arg;

    free(ctx);

    task(arg);
    return 0;
}

void hal_thread_create(void (*task)(void *), void *arg) {
    win_thread_ctx_t *ctx = malloc(sizeof(win_thread_ctx_t));
    if (!ctx) {
        return;
    }
    ctx->task = task;
    ctx->arg = arg;

    HANDLE h = CreateThread(
        NULL,
        0,
        win_thread_trampoline,
        ctx,
        0,
        NULL
    );

    if (h == NULL) {
        free(ctx);
        return;
    }

    CloseHandle(h);
}

void hal_get_cpu_info(hal_cpu_info_t *info) {
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    info->logical_cores = (int)sysinfo.dwNumberOfProcessors;

    info->model_name[0] = '\0';

    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD dataSize = sizeof(info->model_name);
        RegQueryValueExA(hKey, "ProcessorNameString", NULL, NULL, (LPBYTE)info->model_name, &dataSize);
        RegCloseKey(hKey);
    }

    if (info->model_name[0] == '\0') {
        snprintf(info->model_name, sizeof(info->model_name), "Unknown Windows CPU");
    }
}

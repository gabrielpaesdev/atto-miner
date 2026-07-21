/* =========================================================
 * HAL BACKEND: Linux (x86 desktop/server; MIPS routers; Android ARM)
 * ========================================================= */
#include "hal.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>
#include <signal.h>

int hal_init(void) {

    signal(SIGPIPE, SIG_IGN);
    return 0;
}

void hal_deinit(void) {

}

hal_socket_t hal_connect(const char *host, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return HAL_INVALID_SOCKET;

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    if (getaddrinfo(host, port_str, &hints, &res) != 0) {
        close(sock);
        return HAL_INVALID_SOCKET;
    }

    struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));

    if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
        freeaddrinfo(res);
        close(sock);
        return HAL_INVALID_SOCKET;
    }
    freeaddrinfo(res);

    return sock;
}

int hal_send(hal_socket_t sock, const char *buf, size_t len) {
    return (int)send(sock, buf, len, 0);
}

int hal_recv(hal_socket_t sock, char *buf, size_t len) {
    return (int)recv(sock, buf, len, 0);
}

void hal_close(hal_socket_t sock) {
    close(sock);
}

void hal_sleep(unsigned int seconds) {
    sleep(seconds);
}

long hal_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
}


void hal_thread_create(void (*task)(void*), void *arg) {
    pthread_t t;
    if (pthread_create(&t, NULL, (void* (*)(void*))task, arg) == 0) {
        pthread_detach(t);
    }
}

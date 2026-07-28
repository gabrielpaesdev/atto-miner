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

void hal_get_cpu_info(hal_cpu_info_t *info) {
    long cores = sysconf(_SC_NPROCESSORS_ONLN);
    
    if (cores > 0) {
        info->logical_cores = (int)cores;
    } else {
        info->logical_cores = 1;
    }

    info->model_name[0] = '\0';

    FILE *f = fopen("/proc/cpuinfo", "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "model name", 10) == 0) {
                char *colon = strchr(line, ':');
                if (colon) {
                    colon++;
                    while (*colon == ' ') colon++;
                    
                    strncpy(info->model_name, colon, sizeof(info->model_name) - 1);
                    info->model_name[sizeof(info->model_name) - 1] = '\0';
                    
                    size_t len = strlen(info->model_name);
                    if (len > 0 && info->model_name[len - 1] == '\n') {
                        info->model_name[len - 1] = '\0';
                    }
                    break;
                }
            }
        }
        fclose(f);
    }

    if (info->model_name[0] == '\0') {
        strncpy(info->model_name, "Unknown CPU", sizeof(info->model_name) - 1);
        info->model_name[sizeof(info->model_name) - 1] = '\0';
    }
}

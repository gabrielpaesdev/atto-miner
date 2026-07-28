#ifndef HAL_H
#define HAL_H
/* =========================================================
 * HAL TEMPLATE — Hardware Abstraction Layer
 * =========================================================
 * This header is the ONLY thing main.c knows about the outside
 * world (network stack, clock, sleep, threads). Every platform
 * backend (hal_linux.c, hal_windows.c, ...) implements exactly this
 * contract, nothing more. Adding a new target = adding one new
 * .c file that implements these functions; main.c, sha1.c and
 * the mining loop never change.
 *
 * Design rules for backends:
 * - hal_init() must return only once the network is actually
 * usable (link up / Wi-Fi joined / DHCP lease obtained,
 * Winsock started, etc). On stacks that are ready by
 * construction (plain POSIX) it's a no-op.
 * - hal_connect() resolves host:port and returns an already
 * connected TCP stream socket, or HAL_INVALID_SOCKET.
 * - hal_send()/hal_recv() carry the exact same short-read/
 * short-write semantics as POSIX send()/recv(): return
 * bytes transferred, or a negative value on error.
 * - hal_sleep() blocks the calling context for N whole
 * seconds (used only for retry backoff, not timing-critical).
 * - hal_time() must return a monotonic counter in MILLISECONDS.
 * Due to the O(1) nonce injection speed, second-level precision
 * results in 0-second deltas, triggering anti-cheat mechanisms.
 * - hal_thread_create() must spawn an independent worker running
 * the provided task. Memory management of `arg` is handled by
 * the caller/worker, not the HAL.
 * ========================================================= */
#include <stddef.h>
/* Connected-socket handle. On every backend we target (POSIX
 * Linux and ESP-IDF/lwIP) this is a plain BSD socket fd, so a
 * bare int is sufficient — no backend currently needs an
 * opaque/boxed handle. */
typedef int hal_socket_t;
#define HAL_INVALID_SOCKET (-1)
/* One-time bring-up / tear-down of the underlying network stack. */
int hal_init(void);
void hal_deinit(void);
/* Resolve "host" and "port", connect TCP stream socket. */
hal_socket_t hal_connect(const char *host, int port);
/* Same contract as POSIX send()/recv(). */
int hal_send(hal_socket_t sock, const char *buf, size_t len);
int hal_recv(hal_socket_t sock, char *buf, size_t len);
void hal_close(hal_socket_t sock);
/* Blocking sleep, whole seconds. */
void hal_sleep(unsigned int seconds);
/* Monotonic counter in MILLISECONDS (Crucial for Hashrate validation) */
long hal_time(void);
/* =========================================================
 * MULTI-THREADING EXTENSION
 * ========================================================= */
typedef struct {
    int thread_id;
    char username[64];
    char mining_key[64];
    char requested_difficulty[16];
    char rig_id[64];
    int single_miner_id;
} miner_thread_cfg_t;
/* Spawns an abstracted thread to keep main.c OS-agnostic */
void hal_thread_create(void (*task)(void*), void *arg);

typedef struct {
    int logical_cores;
    char model_name[128];
} hal_cpu_info_t;

void hal_get_cpu_info(hal_cpu_info_t *info);
#endif /* HAL_H */

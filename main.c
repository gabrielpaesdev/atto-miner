#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sha1.h"
#include "hal.h"

#define CLOSE_SOCKET hal_close
#define SLEEP(x) hal_sleep(x)
#define INVALID_SOCKET HAL_INVALID_SOCKET

#define SHA_DIGEST_LENGTH 20
#define DEFAULT_POOL_IP "server.duinocoin.com"
#define DEFAULT_POOL_PORT 2813
#define MINER_VERSION "1.2.1"

#define MAX_THREADS 128
volatile unsigned int global_hashrates[MAX_THREADS] = {0};

#ifdef MINIMAL
    #if defined(ESP_PLATFORM)
        #define MINER_ID "atto-min-esp32"
    #else
        #define MINER_ID "atto-min-mips"
    #endif
    #define DUCO_DIFFICULTY "LOW"
    #define LOG(...) ((void)0)

    #ifndef DUCO_USERNAME
        #define DUCO_USERNAME "user"
    #endif
    #ifndef DUCO_MINING_KEY
        #define DUCO_MINING_KEY "password"
    #endif
    #ifndef DUCO_RIG_ID
        #define DUCO_RIG_ID "atto-minimal"
    #endif
#else
    #define MINER_ID "atto-miner"
    #define DUCO_DIFFICULTY "MEDIUM"
    #define LOG(...) printf(__VA_ARGS__)
#endif

static inline size_t append_nonce(char *base, size_t base_len, unsigned int nonce) {
    char *ptr = base + base_len;
    char *ptr1 = ptr;
    char tmp_char;
    unsigned int tmp_value;
    if (nonce == 0) {
        *ptr++ = '0';
        *ptr = '\0';
        return base_len + 1;
    }
    do {
        tmp_value = nonce;
        nonce /= 10;
        *ptr++ = "0123456789"[tmp_value - nonce * 10];
    } while (nonce);
    *ptr-- = '\0';
    size_t total_len = (ptr - base) + 1;
    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }
    return total_len;
}

static inline uint32_t parse_hex_word(const char *hex) {
    uint32_t val = 0;
    for (int i = 0; i < 8; i++) {
        char c = hex[i];
        uint32_t nibble = (c <= '9') ? (c - '0') : ((c | 0x20) - 'a' + 10);
        val = (val << 4) | nibble;
    }
    return val;
}

int json_get_scalar(const char *json, const char *key, char *out, size_t outsz) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    char *p = strstr(json, pattern);
    if (!p) return 0;
    p += strlen(pattern);
    while (*p == ' ' || *p == ':') p++;
    if (*p == '"') {
        p++;
        char *end = strchr(p, '"');
        if (!end) return 0;
        size_t len = (size_t)(end - p);
        if (len >= outsz) len = outsz - 1;
        memcpy(out, p, len);
        out[len] = '\0';
        return 1;
    } else {
        char *end = p;
        while (*end && *end != ',' && *end != '}' && *end != ' ' && *end != '\n' && *end != '\r') end++;
        size_t len = (size_t)(end - p);
        if (len >= outsz) len = outsz - 1;
        memcpy(out, p, len);
        out[len] = '\0';
        return 1;
    }
}

int fetch_pool_raw(char *ip_out, size_t ip_sz, int *port_out) {
    hal_socket_t sock = hal_connect("server.duinocoin.com", 80);
    if (sock == INVALID_SOCKET) return 0;
    const char *req = "GET /getPool HTTP/1.0\r\n"
                      "Host: server.duinocoin.com\r\n"
                      "User-Agent: atto-miner/1.2\r\n"
                      "Connection: close\r\n\r\n";
    hal_send(sock, req, strlen(req));
    char buf[4096];
    memset(buf, 0, sizeof(buf));
    int total = 0;
    while (total < sizeof(buf) - 1) {
        int n = hal_recv(sock, buf + total, sizeof(buf) - total - 1);
        if (n <= 0) break;
        total += n;
    }
    CLOSE_SOCKET(sock);
    if (strncmp(buf, "HTTP/1.", 7) != 0 || !strstr(buf, "200 OK")) return 0;
    char port_str[16] = "";
    if (json_get_scalar(buf, "ip", ip_out, ip_sz) && json_get_scalar(buf, "port", port_str, sizeof(port_str))) {
        *port_out = atoi(port_str);
        return (*port_out > 0);
    }
    return 0;
}

int recv_line(hal_socket_t sock, char *buf, size_t bufsz) {
    size_t total = 0;
    while (1) {
        char c;
        int n = hal_recv(sock, &c, 1);
        if (n <= 0) return -1;
        if (c == '\n') break;
        if (c != '\r' && total + 1 < bufsz) buf[total++] = c;
    }
    buf[total] = '\0';
    return (int)total;
}

static void miner_worker(void *arg) {
    miner_thread_cfg_t *cfg = (miner_thread_cfg_t *)arg;
    int t_id = cfg->thread_id;
    unsigned int rejected_shares = 0;
    unsigned int accepted_shares = 0;
    unsigned int hashrate = 0;
    long start_t, end_t;
    double diff_t;

    SLEEP(t_id);

    while (1) {
        char pool_ip[128] = DEFAULT_POOL_IP;
        int pool_port = DEFAULT_POOL_PORT;

        if (t_id == 0) LOG("[T%d] Fetching automated pool node...\n", t_id);
        fetch_pool_raw(pool_ip, sizeof(pool_ip), &pool_port);

        hal_socket_t socket_desc = hal_connect(pool_ip, pool_port);
        if (socket_desc == INVALID_SOCKET) {
            LOG("[T%d] Error: Couldn't connect, retrying...\n", t_id);
            SLEEP(5);
            continue;
        }

        char serverversion[64] = "";
        if (recv_line(socket_desc, serverversion, sizeof(serverversion)) < 0) {
            CLOSE_SOCKET(socket_desc);
            SLEEP(5);
            continue;
        }

        if (t_id == 0) LOG("[T%d] Server version: %s | Mining DUCO-S1\n", t_id, serverversion);

        int connection_alive = 1;
        while (connection_alive) {
            char job_message[256];
            snprintf(job_message, sizeof(job_message), "JOB,%s,%s,%s",
                     cfg->username, cfg->requested_difficulty, cfg->mining_key);

            if (hal_send(socket_desc, job_message, strlen(job_message)) < 0) {
                connection_alive = 0; break;
            }
            char serverreply[256];
            if (recv_line(socket_desc, serverreply, sizeof(serverreply)) < 0) {
                connection_alive = 0; break;
            }

            char reply_copy[256];
            size_t r_len = strlen(serverreply);
            if (r_len >= sizeof(reply_copy)) r_len = sizeof(reply_copy) - 1;
            memcpy(reply_copy, serverreply, r_len);
            reply_copy[r_len] = '\0';

            char *job = strtok(reply_copy, ",");
            char *work = strtok(NULL, ",");
            char *diff = strtok(NULL, ",");

            if (!job || !work || !diff) {
                connection_alive = 0; break;
            }

            start_t = hal_time();
            int share_found = 0;
            unsigned int diff_int = atoi(diff);
            unsigned int max_nonce = (100 * diff_int) + 1;

            uint32_t target_state[5];
            for (int i = 0; i < 5; i++) {
                target_state[i] = parse_hex_word(&work[i * 8]);
            }

            unsigned char block[64] = {0};
            size_t base_job_len = strlen(job);
            memcpy(block, job, base_job_len);

            for (unsigned int i = 0; i < max_nonce; i++) {
                size_t total_len = append_nonce((char*)block, base_job_len, i);
                block[total_len] = 0x80;

                uint32_t bits = (uint32_t)(total_len * 8);
                block[60] = (bits >> 24) & 0xFF;
                block[61] = (bits >> 16) & 0xFF;
                block[62] = (bits >> 8)  & 0xFF;
                block[63] = bits & 0xFF;

                uint32_t state[5] = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 };
                unsigned char clean_block[64];
                memcpy(clean_block, block, 64);
                SHA1Transform(state, clean_block);

                if (state[0] == target_state[0] && state[1] == target_state[1] &&
                    state[2] == target_state[2] && state[3] == target_state[3] &&
                    state[4] == target_state[4])
                {
                    end_t = hal_time();
                    diff_t = (double)(end_t - start_t);

                    double elapsed_sec = diff_t / 1000.0;
                    if (elapsed_sec <= 0.001) elapsed_sec = 0.001;

                    unsigned int reported_hashrate = (unsigned int)(i / elapsed_sec);
                    hashrate = reported_hashrate / 1000;

                    global_hashrates[t_id] = hashrate;

                    unsigned int total_hashrate = 0;
                    for (int j = 0; j < MAX_THREADS; j++) {
                        total_hashrate += global_hashrates[j];
                    }

                    char result_nonce_str[16];
                    append_nonce(result_nonce_str, 0, i);

                    char submit_message[128];
                    snprintf(submit_message, sizeof(submit_message), "%s,%u,%s v%s,%s,,%d",
                             result_nonce_str, reported_hashrate,
                             MINER_ID, MINER_VERSION, cfg->rig_id, cfg->single_miner_id);

                    if (hal_send(socket_desc, submit_message, strlen(submit_message)) < 0) {
                        connection_alive = 0; share_found = 1; break;
                    }

                    char feedback[64];
                    if (recv_line(socket_desc, feedback, sizeof(feedback)) < 0) {
                        connection_alive = 0; share_found = 1; break;
                    }

                    if (strncmp(feedback, "GOOD", 4) == 0 || strncmp(feedback, "BLOCK", 5) == 0) {
                        accepted_shares++;
                        LOG("[T%d] Accepted share #%u (%s) %u kH/s | Total: %u kH/s | Diff: %u\n",
                            t_id, accepted_shares, result_nonce_str, hashrate, total_hashrate, diff_int);
                    } else if (strncmp(feedback, "INVU", 4) == 0 || strncmp(feedback, "INVK", 4) == 0 || strstr(feedback, "key") != NULL) {
                        LOG("[T%d] Error: Incorrect username or key.\n", t_id);
                        connection_alive = 0; share_found = 1; break;
                    } else {
                        rejected_shares++;
                        LOG("[T%d] Rejected share #%u (%s) %u kH/s | Total: %u kH/s | Diff: %u [%s]\n",
                            t_id, rejected_shares, result_nonce_str, hashrate, total_hashrate, diff_int, feedback);
                    }
                    share_found = 1;
                    break;
                }
            }
            if (!share_found && connection_alive) continue;
        }
        CLOSE_SOCKET(socket_desc);
        SLEEP(5);
    }
    free(cfg);
}

#ifndef MINIMAL 
static void print_usage(const char *prog) {
    printf(
        "atto-miner v" MINER_VERSION " - One miner, any platform.\n"
        "Developed by Gabriel Paes, 2026. MIT License.\n"
        "\n"
        "USAGE:\n"
        "    %s [USERNAME] [MINING_KEY] [RIG_ID] [DIFFICULTY] [THREADS]\n"
        "\n"
        "DESCRIPTION:\n"
        "    Duino-Coin (DUCO) SHA1 CPU miner. Any argument left out is\n"
        "    requested interactively at startup.\n"
        "\n"
        "POSITIONAL ARGUMENTS:\n"
        "    USERNAME       Your Duino-Coin account username.\n"
        "    MINING_KEY     Your Duino-Coin mining key (password).\n"
        "    RIG_ID         Free-form name used to label this rig on the pool.\n"
        "    DIFFICULTY     LOW, MEDIUM, or NET.\n"
        "    THREADS        Number of mining threads to spawn.\n"
        "\n"
        "OPTIONS:\n"
        "    -h, --help     Show this help message and exit.\n"
        "\n"
        "EXAMPLES:\n"
        "    %s\n"
        "        Run fully interactively.\n"
        "\n"
        "    %s myUsername myMiningKey myRig LOW 4\n"
        "        Skip all prompts.\n"
        "\n"
        "PROJECT:\n"
        "    Originally based on the d-cpuminer project (Copyright (c) 2020).\n"
        "    https://github.com/gabrielpaesdev/atto-miner/\n",
        prog, prog, prog
    );
}
#endif

static int miner_main(int argc, char **argv) {
#ifndef MINIMAL
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }
#endif

    if (hal_init() != 0) return 1;

    hal_cpu_info_t cpu;
    hal_get_cpu_info(&cpu);

    char username[64] = "";
    char mining_key[64] = "";
    char rig_id[64] = "atto-rig";
    char difficulty[16];
    
    strcpy(difficulty, DUCO_DIFFICULTY);

    int num_threads = cpu.logical_cores;
    if (num_threads <= 0) num_threads = 1;
    if (num_threads > MAX_THREADS) num_threads = MAX_THREADS;

    srand((unsigned int)hal_time());
    int pool_group_id = rand() % 2812;

#ifndef MINIMAL
    LOG(
    "\033[1;36m============================================================\n"
    "\033[1;33matto-miner v" MINER_VERSION ". July 29, 2026.\n"
    "\033[1;35mOne miner, any platform.\n"
    "\033[1;32mYour device: %s (%d threads)\n"
    "\033[1;36m============================================================\n"
    "\033[0;37mDeveloped by Gabriel Paes, 2026. MIT License.\n"
    "Originally based on the d-cpuminer project (Copyright (c) 2020).\n"
    "\033[1;36m============================================================\033[0m\n",
    cpu.model_name, cpu.logical_cores
    );

    char buf_input[64];

    if (argc > 1) {
        size_t len = strlen(argv[1]);
        if (len >= sizeof(username)) len = sizeof(username) - 1;
        memcpy(username, argv[1], len);
        username[len] = '\0';
    } else {
        while (username[0] == '\0') {
            printf("Enter your DUCO username: ");
            if (fgets(buf_input, sizeof(buf_input), stdin)) {
                buf_input[strcspn(buf_input, "\r\n")] = '\0';
                if (buf_input[0] != '\0') {
                    size_t len = strlen(buf_input);
                    if (len >= sizeof(username)) len = sizeof(username) - 1;
                    memcpy(username, buf_input, len);
                    username[len] = '\0';
                }
            }
        }
    }

    if (argc > 2) {
        size_t len = strlen(argv[2]);
        if (len >= sizeof(mining_key)) len = sizeof(mining_key) - 1;
        memcpy(mining_key, argv[2], len);
        mining_key[len] = '\0';
    } else {
        while (mining_key[0] == '\0') {
            printf("Enter your mining key (password): ");
            if (fgets(buf_input, sizeof(buf_input), stdin)) {
                buf_input[strcspn(buf_input, "\r\n")] = '\0';
                if (buf_input[0] != '\0') {
                    size_t len = strlen(buf_input);
                    if (len >= sizeof(mining_key)) len = sizeof(mining_key) - 1;
                    memcpy(mining_key, buf_input, len);
                    mining_key[len] = '\0';
                }
            }
        }
    }

    if (argc > 3) {
        size_t len = strlen(argv[3]);
        if (len >= sizeof(rig_id)) len = sizeof(rig_id) - 1;
        memcpy(rig_id, argv[3], len);
        rig_id[len] = '\0';
    } else {
        printf("Enter Rig identifier (name) [default: atto-rig]: ");
        if (fgets(buf_input, sizeof(buf_input), stdin)) {
            buf_input[strcspn(buf_input, "\r\n")] = '\0';
            if (buf_input[0] != '\0') {
                size_t len = strlen(buf_input);
                if (len >= sizeof(rig_id)) len = sizeof(rig_id) - 1;
                memcpy(rig_id, buf_input, len);
                rig_id[len] = '\0';
            }
        }
    }

    if (argc > 4) {
        size_t len = strlen(argv[4]);
        if (len >= sizeof(difficulty)) len = sizeof(difficulty) - 1;
        memcpy(difficulty, argv[4], len);
        difficulty[len] = '\0';
    } else {
        printf("Difficulty (LOW/MEDIUM/NET) [default: %s]: ", DUCO_DIFFICULTY);
        if (fgets(buf_input, sizeof(buf_input), stdin)) {
            buf_input[strcspn(buf_input, "\r\n")] = '\0';
            if (buf_input[0] != '\0') {
                if (strcmp(buf_input, "LOW") == 0 ||
                    strcmp(buf_input, "MEDIUM") == 0 ||
                    strcmp(buf_input, "NET") == 0) {
                    size_t len = strlen(buf_input);
                    if (len >= sizeof(difficulty)) len = sizeof(difficulty) - 1;
                    memcpy(difficulty, buf_input, len);
                    difficulty[len] = '\0';
                }
            }
        }
    }

    if (argc > 5) {
        int typed_threads;
        if (sscanf(argv[5], "%d", &typed_threads) == 1) {
            num_threads = typed_threads;
        }
    } else {
        printf("Number of threads to spawn (detected %d, Enter to use default): ", num_threads);
        if (fgets(buf_input, sizeof(buf_input), stdin)) {
            buf_input[strcspn(buf_input, "\r\n")] = '\0';
            if (buf_input[0] != '\0') {
                int typed_threads;
                if (sscanf(buf_input, "%d", &typed_threads) == 1) {
                    num_threads = typed_threads;
                }
            }
        }
    }
    
    if (num_threads <= 0) num_threads = 1;
    if (num_threads > MAX_THREADS) num_threads = MAX_THREADS;

    LOG("Spawning %d threads for %s on rig %s (GroupID: %d)...\n\n", num_threads, username, rig_id, pool_group_id);
#else
    {
        size_t len = strlen(DUCO_USERNAME);
        if (len >= sizeof(username)) len = sizeof(username) - 1;
        memcpy(username, DUCO_USERNAME, len);
        username[len] = '\0';

        len = strlen(DUCO_MINING_KEY);
        if (len >= sizeof(mining_key)) len = sizeof(mining_key) - 1;
        memcpy(mining_key, DUCO_MINING_KEY, len);
        mining_key[len] = '\0';

        len = strlen(DUCO_RIG_ID);
        if (len >= sizeof(rig_id)) len = sizeof(rig_id) - 1;
        memcpy(rig_id, DUCO_RIG_ID, len);
        rig_id[len] = '\0';
    }

    #ifdef DUCO_THREADS
        num_threads = DUCO_THREADS;
        if (num_threads <= 0) num_threads = 1;
        if (num_threads > MAX_THREADS) num_threads = MAX_THREADS;
    #endif
#endif

    for (int i = 0; i < num_threads; i++) {
        miner_thread_cfg_t *cfg = malloc(sizeof(miner_thread_cfg_t));
        cfg->thread_id = i;
        
        size_t len = strlen(username);
        if (len >= sizeof(cfg->username)) len = sizeof(cfg->username) - 1;
        memcpy(cfg->username, username, len);
        cfg->username[len] = '\0';

        len = strlen(mining_key);
        if (len >= sizeof(cfg->mining_key)) len = sizeof(cfg->mining_key) - 1;
        memcpy(cfg->mining_key, mining_key, len);
        cfg->mining_key[len] = '\0';

        len = strlen(rig_id);
        if (len >= sizeof(cfg->rig_id)) len = sizeof(cfg->rig_id) - 1;
        memcpy(cfg->rig_id, rig_id, len);
        cfg->rig_id[len] = '\0';

        cfg->single_miner_id = pool_group_id;

#ifndef MINIMAL
        len = strlen(difficulty);
        if (len >= sizeof(cfg->requested_difficulty)) len = sizeof(cfg->requested_difficulty) - 1;
        memcpy(cfg->requested_difficulty, difficulty, len);
        cfg->requested_difficulty[len] = '\0';
#else
        len = strlen(DUCO_DIFFICULTY);
        if (len >= sizeof(cfg->requested_difficulty)) len = sizeof(cfg->requested_difficulty) - 1;
        memcpy(cfg->requested_difficulty, DUCO_DIFFICULTY, len);
        cfg->requested_difficulty[len] = '\0';
#endif

        hal_thread_create(miner_worker, cfg);
    }

    while (1) {
        SLEEP(60);
    }

    hal_deinit();
    return 0;
}

#if defined(ESP_PLATFORM)
void app_main(void) {
    miner_main(0, NULL);
}
#else
int main(int argc, char **argv) {
    return miner_main(argc, argv);
}
#endif

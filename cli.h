#ifndef CLI_H
#define CLI_H

#include "hal.h"

#define MINER_VERSION "1.2.2"

#ifdef MINIMAL
    #if defined(ESP_PLATFORM)
        #define MINER_ID "atto-min-esp32"
    #else
        #define MINER_ID "atto-min-mips"
    #endif
    #define DUCO_DIFFICULTY "LOW"
#else
    #define MINER_ID "atto-miner"
    #define DUCO_DIFFICULTY "MEDIUM"
#endif

#define MINER_USER_AGENT MINER_ID " v" MINER_VERSION

#define CLI_BUILD_DATE "August 1, 2026"

#ifndef MINIMAL


typedef struct {
    char username[64];
    char mining_key[64];
    char rig_id[64];
    char difficulty[16];
    int  num_threads;
} cli_config_t;

void cli_print_banner(const hal_cpu_info_t *cpu);

void cli_print_usage(const char *prog);


int cli_handle_help_flag(int argc, char **argv);

void cli_gather_config(int argc, char **argv, cli_config_t *cfg, int default_threads);

#endif /* MINIMAL */

#endif /* CLI_H */

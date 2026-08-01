#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "cli.h"

#ifndef MINIMAL

static void cli_read_line(char *buf, size_t bufsz) {
    if (fgets(buf, (int)bufsz, stdin)) {
        buf[strcspn(buf, "\r\n")] = '\0';
    } else {
        buf[0] = '\0';
    }
}

static void cli_copy(char *dst, size_t dstsz, const char *src) {
    size_t len = strlen(src);
    if (len >= dstsz) len = dstsz - 1;
    memcpy(dst, src, len);
    dst[len] = '\0';
}

void cli_print_banner(const hal_cpu_info_t *cpu) {
    printf(
    "\033[1;36m============================================================\n"
    "\033[1;33matto-miner v" MINER_VERSION ". " CLI_BUILD_DATE ".\n"
    "\033[1;35mOne miner, any platform.\n"
    "\033[1;32mYour device: %s (%d threads)\n"
    "\033[1;36m============================================================\n"
    "\033[0;37mDeveloped by Gabriel Paes, 2026. MIT License.\n"
    "Originally based on the d-cpuminer project (Copyright (c) 2020).\n"
    "\033[1;36m============================================================\033[0m\n",
    cpu->model_name, cpu->logical_cores
    );
}

void cli_print_usage(const char *prog) {
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

int cli_handle_help_flag(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            cli_print_usage(argv[0]);
            return 1;
        }
    }
    return 0;
}

void cli_gather_config(int argc, char **argv, cli_config_t *cfg, int default_threads) {
    char buf_input[64];

    memset(cfg, 0, sizeof(*cfg));
    cli_copy(cfg->rig_id, sizeof(cfg->rig_id), "atto-rig");
    cli_copy(cfg->difficulty, sizeof(cfg->difficulty), DUCO_DIFFICULTY);
    cfg->num_threads = default_threads;

    /* USERNAME */
    if (argc > 1) {
        cli_copy(cfg->username, sizeof(cfg->username), argv[1]);
    } else {
        while (cfg->username[0] == '\0') {
            printf("Enter your DUCO username: ");
            cli_read_line(buf_input, sizeof(buf_input));
            if (buf_input[0] != '\0') {
                cli_copy(cfg->username, sizeof(cfg->username), buf_input);
            }
        }
    }

    /* MINING KEY */
    if (argc > 2) {
        cli_copy(cfg->mining_key, sizeof(cfg->mining_key), argv[2]);
    } else {
        while (cfg->mining_key[0] == '\0') {
            printf("Enter your mining key (password): ");
            cli_read_line(buf_input, sizeof(buf_input));
            if (buf_input[0] != '\0') {
                cli_copy(cfg->mining_key, sizeof(cfg->mining_key), buf_input);
            }
        }
    }

    /* RIG ID */
    if (argc > 3) {
        cli_copy(cfg->rig_id, sizeof(cfg->rig_id), argv[3]);
    } else {
        printf("Enter Rig identifier (name) [default: atto-rig]: ");
        cli_read_line(buf_input, sizeof(buf_input));
        if (buf_input[0] != '\0') {
            cli_copy(cfg->rig_id, sizeof(cfg->rig_id), buf_input);
        }
    }

    /* DIFFICULTY */
    if (argc > 4) {
        cli_copy(cfg->difficulty, sizeof(cfg->difficulty), argv[4]);
    } else {
        printf("Difficulty (LOW/MEDIUM/NET) [default: %s]: ", DUCO_DIFFICULTY);
        cli_read_line(buf_input, sizeof(buf_input));
        if (buf_input[0] != '\0' &&
            (strcmp(buf_input, "LOW") == 0 ||
             strcmp(buf_input, "MEDIUM") == 0 ||
             strcmp(buf_input, "NET") == 0)) {
            cli_copy(cfg->difficulty, sizeof(cfg->difficulty), buf_input);
        }
    }

    /* THREADS */
    if (argc > 5) {
        int typed_threads;
        if (sscanf(argv[5], "%d", &typed_threads) == 1) {
            cfg->num_threads = typed_threads;
        }
    } else {
        printf("Number of threads to spawn (detected %d, Enter to use default): ", default_threads);
        cli_read_line(buf_input, sizeof(buf_input));
        if (buf_input[0] != '\0') {
            int typed_threads;
            if (sscanf(buf_input, "%d", &typed_threads) == 1) {
                cfg->num_threads = typed_threads;
            }
        }
    }
}

#endif /* MINIMAL */

/*
 *               _____  ____ __
 *   __ ____ _  /  _/ |/ / //_/
 *  / // /  ' \_/ //    / ,<
 *  \_,_/_/_/_/___/_/|_/_/|_|
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <umdaemon.h>
#include <signal.h>

// daemon name and description
const char *UMD_TYPE = "umsysagent";
const char *UMD_DESCRIPTION = "umINK System Agent";

// sysagent daemon instance descriptor type
typedef struct {
    void *pcfg;
    const char *plg_cfg_f;
} sysagentdd_t;

// help
static void print_help() {
    printf("%s - %s\n\nOptions:\n", UMD_TYPE, UMD_DESCRIPTION);
    printf(" %s\n %s\n %s\n %s\n\n",
           "-?    help",
           "-i    unique daemon id",
           "-p    plugin path",
           "-D    start in debug mode");
    printf("%s\n %s\n",
           "Plugins:",
           "--plugins-cfg    Plugins configuration file");
}

// process args
static void proc_args(umdaemon_t *umd, int argc, char **argv) {
    int opt;
    int option_index = 0;
    struct option long_options[] = {{"plugins-cfg", required_argument, 0, 0},
                                    {0, 0, 0, 0}};

    if (argc < 3) {
        print_help();
        exit(EXIT_FAILURE);
    }
    // descriptor
    sysagentdd_t *dd = umd->data;

    // get args
    while ((opt = getopt_long(argc, argv, "?i:p:D", long_options,
                              &option_index)) != -1) {
        switch (opt) {
            // long options
            case 0:
                if (long_options[option_index].flag != 0)
                    break;
                switch (option_index) {
                    // plugins-cfg
                    case 0:
                        dd->plg_cfg_f = optarg;
                        break;
                    default:
                        break;
                }

                break;

            // daemon id
            case 'i':
                if (umd_set_id(umd, optarg) != 0) {
                    printf("%s\n", "ERROR: Maximum size of daemon id string is "
                                   "15 characters!");
                    exit(EXIT_FAILURE);
                }
                break;

            // debug mode
            case 'D':
                umd_set_log_level(umd, UMD_LLT_DEBUG);
                break;

            default:
                break;
        }
    }
}

// main
int main(int argc, char **argv) {
    // create umink daemon
    umdaemon_t *umd = umd_create("", UMD_TYPE);
    // craeate deamon descriptor
    sysagentdd_t dd = {.pcfg = NULL, .plg_cfg_f = NULL};
    umd->data = &dd;
    // process command line arguments
    proc_args(umd, argc, argv);
    // signal handler
    signal(SIGTERM, &umd_signal_handler);
    // init daemon
    umd_start(umd);
    // loop until terminated
    umd_loop(umd);
    // cleanup
    umd_destroy(umd);
    // normal exit
    return 0;
}

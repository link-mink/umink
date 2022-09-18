/*
 *               _____  ____ __
 *   __ ____ _  /  _/ |/ / //_/
 *  / // /  ' \_/ //    / ,<
 *  \_,_/_/_/_/___/_/|_/_/|_|
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <umink_pkg_config.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <umdaemon.h>
#include <signal.h>
#include <utarray.h>
#include <umink_plugin.h>
#include <dirent.h>
#include <json_tokener.h>

// daemon name and description
const char *UMD_TYPE = "umsysagent";
const char *UMD_DESCRIPTION = "umINK System Agent";

// filter for scandir
typedef int (*fs_dir_filter_t)(const struct dirent *);

// sysagent daemon instance descriptor type
typedef struct {
    void *pcfg;
    const char *plg_cfg_f;
    const char *plg_pth;
    umplg_mngr_t *pm;
    struct json_object *cfg;
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

            // plugin path
            case 'p':
                dd->plg_pth = optarg;
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


static void plgn_cpy(void *dst, const void *src){
    umplgd_t *plgn_dst = (umplgd_t *)dst;
    umplgd_t *plgn_src = (umplgd_t *)src;

    plgn_dst->name = plgn_src->name ? strdup(plgn_src->name) : NULL;

}

static void plgn_dtor(void *elt){
    umplgd_t *plgn = (umplgd_t *)elt;
    if (plgn->name) free(plgn->name);
}

static void plgn_init(void *elt) {
}

static char **fs_readdir(const char *dir,
                         size_t *size,
                         fs_dir_filter_t filter) {
    // null check
    if (!(dir && size))
        return NULL;
    // vars
    char **res = NULL;
    struct dirent **fnames = NULL;
    // scan dir
    int n = scandir(dir, &fnames, filter, &alphasort);
    if (n >= 0) {
        *size = n;
        // alloc res
        res = (char **)malloc(sizeof(char *) * n);
        // loop results
        for (int i = 0; i < n; i++) {
            res[i] = strdup(fnames[i]->d_name);
            free(fnames[i]);
        }
    } else
        *size = 0;
    free(fnames);
    return res;
}

static void init_plugins(umplg_mngr_t *pm, const char *pdir) {
    int pdl;
    // plugin dir missing
    if (!pdir) {
        return;
    } else {
        pdl = strlen(pdir);
    }

    // read plugin dir
    size_t rl;
    char** lst = fs_readdir(pdir, &rl, NULL);
    if (lst == NULL) return;

    // copy dir path str
    char* plg_fname = strdup(pdir);
    int l;
    // loop results
    for (int i = 0; i < rl; i++) {
        // only .so files
        if (strstr(lst[i], ".so")) {
            // concat dir path and file path
            l = strlen(lst[i]);
            plg_fname = (char *)realloc(plg_fname, pdl + l + 2);
            strcpy(&plg_fname[pdl], "/");
            strcpy(&plg_fname[pdl + 1], lst[i]);
            // load plugin
            if (umplg_load(pm, plg_fname))
                printf("Loading plugin [%s]...\n", lst[i]);

            else
                printf("Cannot load plugin [%s], mandatory methods not found!\n",
                       lst[i]);
        }
        // free item
        free(lst[i]);
    }
    // free mem
    free(lst);
    free(plg_fname);
}

static void init(sysagentdd_t *dd) {
    // load plugins configuration
    FILE *f = fopen(dd->plg_cfg_f, "r");
    if (f == NULL) {
        return;
    }
    if(fseek(f, 0, SEEK_END) < 0){
        fclose(f);
        return;
    }
    int32_t fsz = ftell(f);
    if (fsz <= 0) {
        fclose(f);
        return;
    }
    rewind(f);
    char *b = calloc(fsz + 1, 1);
    fread(b, fsz, 1, f);
    fclose(f);

    // process plugins configurataion
    dd->cfg = json_tokener_parse(b);
    if (dd->cfg == NULL) {
        printf("%s\n",
               "ERROR: Invalid plugins configuration file");
        exit(EXIT_FAILURE);
    }
    // set plugin manager cfg pointer
    dd->pm->cfg = dd->cfg;
    free(b);

    // init plugins
    init_plugins(dd->pm, dd->plg_pth);
}

void std_items_init(void *_elt) {
    umplg_data_std_items_t *elt = (umplg_data_std_items_t *)_elt;
    elt->table = NULL;
    printf("...init %p\n", elt->table);
}

void std_items_dtor(void *_elt) {
    umplg_data_std_items_t *elt = (umplg_data_std_items_t *)_elt;

    umplg_data_std_item_t *s, *tmp;

    HASH_ITER(hh, elt->table, s, tmp) {
        HASH_DEL(elt->table, s);
        free(s->name);
        free(s->value);
        free(s);
    }
}

void std_items_copy(void *_dst, const void *_src) {
    umplg_data_std_items_t *dst = (umplg_data_std_items_t *)_dst;
    umplg_data_std_items_t *src = (umplg_data_std_items_t *)_src;

    umplg_data_std_item_t *s, *tmp, *n;
    dst->table = NULL;

    HASH_ITER(hh, src->table, s, tmp) {
        n = malloc(sizeof(umplg_data_std_item_t));
        n->name = strdup(s->name);
        n->value = strdup(s->value);
        HASH_ADD_KEYPTR(hh, dst->table, n->name, strlen(n->name), n);
        printf("[%p name %s: value %s\n", dst->table, s->name, s->value);
    }



}

// main
int main(int argc, char **argv) {
    // create umink daemon
    umdaemon_t *umd = umd_create("", UMD_TYPE);
    // craeate deamon descriptor
    sysagentdd_t dd = {
        .pcfg = NULL,
        .plg_cfg_f = NULL,
        .plg_pth = NULL,
        .pm = umplg_new_mngr()
    };
    umd->data = &dd;
    dd.pm->cfg = dd.cfg;
    // process command line arguments
    proc_args(umd, argc, argv);
    // signal handler
    signal(SIGTERM, &umd_signal_handler);
    // start daemon
    umd_start(umd);
    // init
    init(&dd);
    // loop until terminated
    umd_loop(umd);
    // cleanup
    umd_destroy(umd);
    umplg_free_mngr(dd.pm);
    // normal exit
    return 0;
}

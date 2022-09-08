/*
 *               _____  ____ __
 *   __ ____ _  /  _/ |/ / //_/
 *  / // /  ' \_/ //    / ,<
 *  \_,_/_/_/_/___/_/|_/_/|_|
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef UMINK_PLUGIN
#define UMINK_PLUGIN

#include <stdbool.h>
#include <utarray.h>
#include <uthash.h>

// types
typedef struct umplgd umplgd_t;
typedef struct umplg_mngrd umplg_mngrd_t;
typedef struct umplg_idata umplg_idata_t;
typedef struct umplg_mngr umplg_mngr_t;
typedef struct umplg_sh umplg_sh_t;
typedef struct umplg_data_std umplg_data_std_t;
typedef struct umplg_hkd umplg_hkd_t;

// consts
#define UMPLG_INIT_FN           "init"
#define UMPLG_TERM_FN           "terminate"
#define UMPLG_CMD_HNDLR         "run"
#define UMPLG_CMD_HNDLR_LOCAL   "run_local"
#define UMPLG_CMD_LST           "COMMANDS"

/**
 * Plugin init handler
 *
 * @param[in]   pm      Pointer to MINK plugin manager
 * @param[in]   pd      Pointer to plugin descriptor
 * @return      0 for success
 */
typedef int (*umplg_inith_t)(umplg_mngr_t *pm, umplgd_t *pd);

/**
 * Plugin terminate handler
 *
 * @param[in]   pm      Pointer to MINK plugin manager
 * @param[in]   pd      Pointer to plugin descriptor
 * @return      0 for success
 */
typedef int (*umplg_termh_t)(umplg_mngr_t *pm, umplgd_t *pd);

/**
 * Plugin cmd handler
 *
 * @param[in]       pm      Pointer to MINK plugin manager
 * @param[in]       pd      Pointer to plugin descriptor
 * @param[in]       hk      MINK daemon hook
 * @param[in,out]   data    Custom data
 * @return          0 for success
 */
typedef int (*umplg_cmdh_t)(umplg_mngr_t *pm,
                            umplgd_t *pd,
                            int cmd_id,
                            umplg_idata_t *data);

/**
 * Signal handler method
 *
 * @param[in]   d_in      Pointer to plugin standard input data
 * @param[in]   d_out     Pointer to plugin standard output data
 * @return      pointer to output data
 */
typedef umplg_data_std_t *(*umplg_shfn_t)(umplg_data_std_t *d_in,
                                          umplg_data_std_t *d_out);

// input data type for local interface
enum umplgd_t {
    // unknown data type (error)
    UMPLG_DT_UNKNOWN  = 0,
    // JSON-RPC (UNIX socket)
    UMPLG_DT_JSON_RPC = 1,
    // plugin-specific (custom plugin2plugin)
    UMPLG_DT_SPECIFIC = 2,
    // plugin-to-plugin standard in/out format
    // (standard plugin2plugin)
    UMPLG_DT_STANDARD = 4
};

// standard data descriptor
struct umplg_data_std {
    // TODO
};

// input data descriptor
struct umplg_idata {
    enum umplgd_t type;
    void *data;
};

// plugin descriptor
struct umplgd {
    /** Plugin handle */
    void *handle;
    /** Plugin name */
    char *name;
    /** Plugin type */
    int type;
    /** Plugin cmd handler method */
    umplg_cmdh_t cmdh;
    /** Plugin cmd handler method (local) */
    umplg_cmdh_t cmdh_l;
    /** Plugin terminate method */
    umplg_termh_t termh;
    /** Custom data filled by plugin */
    void *data;
    // hashable
    UT_hash_handle hh;
};

// hook descriptor
struct umplg_hkd {
    // id
    int id;
    // plugin pointer
    umplgd_t *plgp;
    // hashable
    UT_hash_handle hh;
};

// signal handler descriptor
struct umplg_sh {
    // singal invocation method
    umplg_shfn_t run;
};

// plugin manager descriptor
struct umplg_mngr {
    UT_array *plgs;
    umplg_hkd_t *hooks;
};

// create plugin manager
umplg_mngr_t *umplg_new_mngr();

// free plugin manager
void umplg_free_mngr(umplg_mngr_t *pm);

/**
 * Load and verify plugin
 *
 * @param[in]   pm          Pointer to plugin manager
 * @param[in]   fpath       Plugin file path
 * @return      new plugin descriptor or NULL on error
 */
umplgd_t *umplg_load(umplg_mngr_t *pm, const char *fpath);

/**
 * Unload plugin; call plugin's terminate method and
 * remove from list of active plugins
 *
 * @param[in]   pd          Pointer to plugin descriptor
 * @return      0 for success or error
 */
int umplg_unload(umplg_mngr_t *pm, umplgd_t *pd);

/**
 * Run plugin hook
 *
 * @param[in]   cmd_id  Command id
 * @param[in]   data    Custom data (input/output)
 * @param[in]   local   Local request flag
 *
 * @return      0 for success or error code
 */
int umplg_run(umplg_mngr_t *pm,
              int cmd_id,
              int idt,
              umplg_idata_t *data,
              bool is_local);

// register signal handler
int umplg_reg_signal(umplg_mngr_t *pm, const char *s, umplg_sh_t *sh);

// process signal
int umplg_proc_signal(umplg_mngr_t *pm,
                      const char *s,
                      umplg_data_std_t *d_in,
                      char *d_out);

#endif /* ifndef UMINK_PLUGIN */

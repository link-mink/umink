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
typedef struct umplg_data_std_items umplg_data_std_items_t;
typedef struct umplg_data_std_item umplg_data_std_item_t;
typedef struct umplg_data_std umplg_data_std_t;
typedef struct umplg_hkd umplg_hkd_t;

// consts
#define UMPLG_INIT_FN           "init"
#define UMPLG_TERM_FN           "terminate"
#define UMPLG_CMD_HNDLR         "run"
#define UMPLG_CMD_HNDLR_LOCAL   "run_local"
#define UMPLG_CMD_LST           "COMMANDS"

// plugin CMD ids
enum umplg_cmd_t {
    UNKNWON_COMMAND = 0,
    CMD_GET_SYSINFO = 1,
    CMD_GET_CPUSTATS = 2,
    CMD_GET_MEMINFO = 3,
    CMD_GET_UNAME = 4,
    CMD_GET_PROCESS_LST = 5,
    CMD_GET_FILE_STAT = 6,
    CMD_UBUS_CALL = 7,
    CMD_SHELL_EXEC = 8,
    CMD_SET_DATA = 9,
    CMD_RUN_RULES = 10,
    CMD_LOAD_RULES = 11,
    CMD_AUTH = 12,
    CMD_SOCKET_PROXY = 13,
    CMD_FIRMWARE_UPDATE = 14,
    CMD_SYSLOG_START = 15,
    CMD_SYSLOG_STOP = 16,
    CMD_REMOTE_EXEC_START = 17,
    CMD_REMOTE_EXEC_STOP = 18,
    CMD_GET_SYSMON_DATA = 19,
    CMD_NET_TCP_SEND = 20,
    CMD_CG2_GROUP_CREATE = 21,
    CMD_CG2_GROUP_DELETE = 22,
    CMD_CG2_GROUPS_LST = 23,
    CMD_CG2_CONTROLLER_GET = 24,
    CMD_CG2_CONTROLLER_SET = 25,
    CMD_CG2_CONTROLLERS_LST = 26,
    CMD_SYSD_FWLD_GET_ZONES = 27,
    CMD_SYSD_FWLD_GET_RICH_RULES = 28,
    CMD_SYSD_FWLD_ADD_RICH_RULE = 29,
    CMD_SYSD_FWLD_DEL_RICH_RULE = 30,
    CMD_SYSD_FWLD_RELOAD = 31,
    CMD_MODBUS_WRITE_BIT = 32,
    CMD_MODBUS_READ_BITS = 33,
    CMD_NDPI_GET_STATS = 34,
    CMD_MQTT_PUBLISH = 35,
    CMD_LUA_CALL = 36
};

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
 * Signal handler method (init)
 *
 * @param[in]   d_in      Pointer to signal handler descriptor
 * @param[in]   d_in      Pointer to plugin standard input data
 * @return      0 for success
 */
typedef int (*umplg_shfn_init_t)(umplg_sh_t *shd,
                                 umplg_data_std_t *d_in);

/**
 * Signal handler method (run)
 *
 * @param[in]       d_in      Pointer to signal handler descriptor
 * @param[in]       d_in      Pointer to plugin standard input data
 * @param[in,out]   d_out     Output data buffer pointer
 * @param[in,out]   out_sz    Output data size pointer
 * @return      0 for success
 */
typedef int (*umplg_shfn_run_t)(umplg_sh_t *shd,
                                umplg_data_std_t *d_in,
                                char **d_out,
                                size_t *out_sz);

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

// stamdard data item
struct umplg_data_std_item {
    // name = value
    char *name;
    char *value;
    // hashable
    UT_hash_handle hh;
};

// standard data items
struct umplg_data_std_items {
    umplg_data_std_item_t *table;
};

// standard data descriptor
struct umplg_data_std {
    UT_array *items;
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
    // signal id
    char *id;
    // singal invocation method
    umplg_shfn_init_t init;
    umplg_shfn_run_t run;
    // extra args
    UT_array *args;
    // hashable
    UT_hash_handle hh;
};

// plugin manager descriptor
struct umplg_mngr {
    // array of registered plugins
    UT_array *plgs;
    // hashmap of plugin <-> hook mappings
    umplg_hkd_t *hooks;
    // hasmap of registered signals
    umplg_sh_t *signals;
    // configuration data
    void *cfg;
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
int umplg_reg_signal(umplg_mngr_t *pm, umplg_sh_t *sh);

// process signal
int umplg_proc_signal(umplg_mngr_t *pm,
                      const char *s,
                      umplg_data_std_t *d_in,
                      char **d_out,
                      size_t *out_sz);

// standard data type
int umplg_stdd_items_add(umplg_data_std_t *data, umplg_data_std_items_t *items);
int umplg_stdd_item_add(umplg_data_std_items_t *items, umplg_data_std_item_t *item);
void umplg_stdd_init(umplg_data_std_t *data);
void umplg_stdd_free(umplg_data_std_t *data);


#endif /* ifndef UMINK_PLUGIN */

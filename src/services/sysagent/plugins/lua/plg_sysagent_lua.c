/*
 *               _____  ____ __
 *   __ ____ _  /  _/ |/ / //_/
 *  / // /  ' \_/ //    / ,<
 *  \_,_/_/_/_/___/_/|_/_/|_|
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <umink_plugin.h>

/*************/
/* Plugin ID */
/*************/
static const char *PLG_ID = "plg_sysagent_lua.so";

/**********************************************/
/* list of command implemented by this plugin */
/**********************************************/
int COMMANDS[] = {
    CMD_LUA_CALL,
    // end of list marker
    -1
};

/****************/
/* init handler */
/****************/
int init(umplg_mngr_t *pm, umplgd_t *pd) {
    // TODO
    return 0;
}

/*********************/
/* terminate handler */
/*********************/
int terminate(umplg_mngr_t *pm, umplgd_t *pd){
    // TODO
    return 0;
}

/*************************/
/* local command handler */
/*************************/
int run_local(umplg_mngr_t *pm, umplgd_t *pd, int cmd_id, umplg_idata_t *data) {
    // TODO
    return 0;
}

/*******************/
/* command handler */
/*******************/
int run(umplg_mngr_t *pm, umplgd_t *pd, int cmd_id, umplg_idata_t *data) {
    // TODO
    return 0;
}



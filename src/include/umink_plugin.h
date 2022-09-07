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

// types
typedef struct umplgd umplgd_t;
typedef struct umplg_mngrd umplg_mngrd_t;
typedef struct umplg_idata umplg_idata_t;

/**
 * Plugin init handler
 *
 * @param[in]   pm      Pointer to MINK plugin manager
 * @param[in]   pd      Pointer to plugin descriptor
 * @return      0 for success
 */
typedef int (*plg_inith_t)(umplg_mngrd_t *pm, umplgd_t *pd);

/**
 * Plugin terminatehandler
 *
 * @param[in]   pm      Pointer to MINK plugin manager
 * @param[in]   pd      Pointer to plugin descriptor
 * @return      0 for success
 */
typedef int (*plg_termh_t)(umplg_mngrd_t *pm, umplgd_t *pd);

/**
 * Plugin cmd handler
 *
 * @param[in]       pm      Pointer to MINK plugin manager
 * @param[in]       pd      Pointer to plugin descriptor
 * @param[in]       hk      MINK daemon hook
 * @param[in,out]   data    Custom data
 * @return          0 for success
 */
typedef int (*plg_cmdh_t)(umplg_mngrd_t *pm,
                          umplgd_t *pd,
                          int cmd_id,
                          umplg_idata_t *data);

// plugin descriptor
struct umplgd {
    /** Plugin handle */
    void *handle;
    /** Plugin name */
    char *name;
    /** Plugin type */
    int type;
    /** Plugin cmd handler method */
    plg_cmdh_t cmdh;
    /** Plugin cmd handler method (local) */
    plg_cmdh_t cmdh_l;
    /** Plugin terminate method */
    plg_termh_t termh;
    /** Custom data filled by plugin */
    void *data;
};

#endif /* ifndef UMINK_PLUGIN */

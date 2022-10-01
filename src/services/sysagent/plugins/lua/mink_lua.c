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
#include <umink_plugin.h>
#include <string.h>

/**********/
/* Signal */
/**********/
char *
mink_lua_signal(const char *s, const char *d, void *md)
{
    // plugin manager
    umplg_mngr_t *pm = md;
    // check signal
    if (!s) {
        return strdup("<SIGNAL UNDEFINED>");
    }
    // create std data
    umplg_data_std_t e_d = { .items = NULL };
    umplg_data_std_items_t items = { .table = NULL };
    umplg_data_std_item_t item = { .name = "", .value = (char *)(d ? d : "") };
    // init std data
    umplg_stdd_init(&e_d);
    umplg_stdd_item_add(&items, &item);
    umplg_stdd_items_add(&e_d, &items);
    // output buffer
    char *b = NULL;
    size_t sz = 0;
    // process signal
    if (umplg_proc_signal(pm, s, &e_d, &b, &sz) == 0) {
        return b;
    }
    // error
    return strdup("");
}

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

/*********/
/* Types */
/*********/
typedef struct {
    const char *key;
    const char *value;
} mink_cdata_column_t;

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

/*****************************/
/* Get plugin data row count */
/*****************************/
size_t
mink_lua_cmd_data_sz(void *p)
{
    if (p == NULL) {
        return 0;
    }
    // cast (unsafe) to standard plugin data type
    umplg_data_std_t *d = p;
    // row count
    return utarray_len(d->items);
}

/**************************************************/
/* Get plugin data columns count for specific row */
/**************************************************/
size_t
mink_lua_cmd_data_row_sz(const int r, void *p)
{
    // cast (unsafe) to standard plugin data type
    umplg_data_std_t *d = p;
    // verify row index
    if (r >= utarray_len(d->items)) {
        return 0;
    }
    // items elem at index
    umplg_data_std_items_t *items = utarray_eltptr(d->items, r);
    // column count for row at index
    return HASH_COUNT(items->table);
}

/********************************/
/* Get plugin data column value */
/********************************/
mink_cdata_column_t
mink_lua_cmd_data_get_column(const int r, const int c, void *p)
{
    // get row count
    size_t rc = mink_lua_cmd_data_sz(p);
    // sanity check (rows)
    if (rc <= r) {
        mink_cdata_column_t cdata = { 0 };
        return cdata;
    }
    // cast (unsafe) to standard plugin data type
    umplg_data_std_t *d = p;
    // get row at index
    umplg_data_std_items_t *row = utarray_eltptr(d->items, r);
    // sanity check (columns)
    if (HASH_COUNT(row->table) <= c) {
        mink_cdata_column_t cdata = { 0 };
        return cdata;
    }
    // get column at index
    umplg_data_std_item_t *column = NULL;
    int i = 0;
    for (column = row->table; column != NULL; column = column->hh.next) {
        if (i == c) {
            // return column value
            mink_cdata_column_t cdata = { column->name, column->value };
            return cdata;
        }
        ++i;
    }

    mink_cdata_column_t cdata = { 0 };
    return cdata;
}


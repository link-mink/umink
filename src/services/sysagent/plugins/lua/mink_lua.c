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
    // output buffer (allocated in signal handler)
    char *b = NULL;
    size_t sz = 0;
    // process signal
    if (umplg_proc_signal(pm, s, &e_d, &b, &sz) == 0) {
        // cleanup
        HASH_CLEAR(hh, items.table);
        umplg_stdd_free(&e_d);
        return b;
    }
    // cleanup
    HASH_CLEAR(hh, items.table);
    umplg_stdd_free(&e_d);
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

/*******************/
/* Free plugin res */
/*******************/
void
mink_lua_free_res(void *p)
{
    umplg_data_std_t *d = p;
    umplg_stdd_free(d);
    free(d);
}

/**************************/
/* Create new plugin data */
/**************************/
void *
mink_lua_new_cmd_data()
{
    umplg_data_std_t *d = malloc(sizeof(umplg_data_std_t));
    d->items = NULL;
    umplg_stdd_init(d);
    return d;
}

/************/
/* cmd_call */
/************/
int
mink_lua_cmd_call(void *md, int argc, const char **args, void *out)
{
    // plugin manager
    umplg_mngr_t *pm = md;
    // argument count check
    if (argc < 1) {
        return -1;
    }
    // output check
    if (!out) {
        return -2;
    }
    // cmd data
    umplg_data_std_t *d = out;
    // get command id
    int cmd_id = umplg_get_cmd_id(pm, args[0]);
    // cmd arguments
    for (int i = 1; i < argc; i++) {
        // column map
        umplg_data_std_items_t cmap = { .table = NULL };
        // insert columns
        umplg_data_std_item_t item = { .name = "", .value = (char *)args[i] };
        umplg_stdd_item_add(&cmap, &item);
        // add row
        umplg_stdd_items_add(d, &cmap);
        // cleanup
        HASH_CLEAR(hh, cmap.table);
    }
    // plugin input data
    umplg_idata_t idata = { UMPLG_DT_STANDARD, d };
    // run plugin method
    int res = umplg_run(pm, cmd_id, idata.type, &idata, true);
    // result
    return res;

}


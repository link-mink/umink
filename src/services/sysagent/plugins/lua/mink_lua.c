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
#include <luaconf.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

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

/************/
/* get_args */
/************/
int
mink_lua_get_args(lua_State *L)
{
    // get std data
    lua_pushstring(L, "mink_stdd");
    lua_gettable(L, LUA_REGISTRYINDEX);
    umplg_data_std_t *d = lua_touserdata(L, -1);
    size_t sz = mink_lua_cmd_data_sz(d);

    // lua table
    lua_newtable(L);

    // loop result data (rows)
    for (int i = 0; i < sz; i++) {
        // table key (array)
        lua_pushnumber(L, i + 1);

        // create table row
        lua_newtable(L);

        // get column count
        size_t sz_c = mink_lua_cmd_data_row_sz(i, d);
        // loop columns
        for (int j = 0; j < sz_c; j++) {
            // get column key/value
            mink_cdata_column_t c = mink_lua_cmd_data_get_column(i, j, d);
            // add column to lua table
            if (c.value != NULL) {
                int k = 1;
                // update key, if not null
                if (c.key != NULL && strlen(c.key) > 0) {
                    // k = ffi.string(c.key)
                    lua_pushstring(L, c.key);

                } else {
                    // k = 1
                    lua_pushnumber(L, k);
                }

                // add value and add table column
                lua_pushstring(L, c.value);
                lua_settable(L, -3);
            }
        }

        // add table row
        lua_settable(L, -3);
    }

    // return table
    return 1;
}

/********************/
/* cmd_call wrapper */
/********************/
int
mink_lua_do_cmd_call(lua_State *L)
{
    if (!lua_istable(L, -1)) {
        return 0;
    }
    // table size (length)
    size_t sz = lua_objlen(L, -1);
    // crate string array
    char *cmd_arg[sz];
    for (int i = 0; i<sz; i++){
        // get t[i] table value
        lua_pushnumber(L, i + 1);
        lua_gettable(L, -2);
        // check type
        if (lua_isstring(L, -1)) {
            cmd_arg[i] = strdup(lua_tostring(L, -1));

        } else if (lua_isnumber(L, -1)) {
            cmd_arg[i] = malloc(32);
            snprintf(cmd_arg[i], 32, "%d", (int)lua_tonumber(L, -1));

        } else if (lua_isboolean(L, -1)) {
            cmd_arg[i] = malloc(2);
            snprintf(cmd_arg[i], 2, "%d", lua_toboolean(L, -1));

        } else {
            cmd_arg[i] = strdup("");
        }
        lua_pop(L, 1);
    }

    // get pm
    lua_pushstring(L, "mink_pm");
    lua_gettable(L, LUA_REGISTRYINDEX);
    umplg_mngr_t *pm = lua_touserdata(L, -1);

    // output buffer
    umplg_data_std_t d = { .items = NULL };
    umplg_stdd_init(&d);
    // call method
    int res = mink_lua_cmd_call(pm, sz, (const char **)cmd_arg, &d);
    // free tmp string array
    for (int i = 0; i < sz; i++) {
        free(cmd_arg[i]);
    }
    // if successful, copy C data to lua table
    if (res == 0){
        // result
        lua_newtable(L);
        // cmd data size
        sz = mink_lua_cmd_data_sz(&d);
        // loop result data (rows)
        for (int i = 0; i < sz; i++) {
            // add row to table
            lua_pushnumber(L, i + 1);
            // crate table row
            lua_newtable(L);
            // get column count
            size_t sz_c = mink_lua_cmd_data_row_sz(i, &d);
            // loop columns
            for (int j = 0; j < sz_c; j++) {
                // get column key/value
                mink_cdata_column_t c = mink_lua_cmd_data_get_column(i, j, &d);
                // add column to lua table
                if (c.value != NULL) {
                    int k = 1;
                    // update key, if not null
                    if (c.key != NULL && strlen(c.key) > 0) {
                        // k = ffi.string(c.key)
                        lua_pushstring(L, c.key);

                    } else {
                        // k = 1
                        lua_pushnumber(L, k);
                    }

                    // add value and add table column
                    lua_pushstring(L, c.value);
                    lua_settable(L, -3);
                }
            }
            // add table row
            lua_settable(L, -3);
        }
        // cleanup
        umplg_stdd_free(&d);
        return 1;

    }
    umplg_stdd_free(&d);
    return 0;
}

/******************/
/* signal wrapper */
/******************/
int
mink_lua_do_signal(lua_State *L)
{

    // signal data/signal name
    const char *d = NULL;
    const char *s = NULL;

    // signal name and data
    if (lua_gettop(L) >= 2) {
        if (!lua_isstring(L, -1) || !lua_isstring(L, -2)) {
            return 0;
        }
        d = lua_tostring(L, -1);
        s = lua_tostring(L, -2);

    // signal name only
    } else {
        if (!lua_isstring(L, -1)) {
            return 0;
        }
        s = lua_tostring(L, -1);
    }

    // get pm
    lua_pushstring(L, "mink_pm");
    lua_gettable(L, LUA_REGISTRYINDEX);
    umplg_mngr_t *pm = lua_touserdata(L, -1);
    lua_pop(L, 1);

    // signal
    char *s_res = mink_lua_signal(s, d, pm);
    if (s_res != NULL) {
        lua_pushstring(L, s_res);
        free(s_res);

    } else {
        lua_pushstring(L, strdup(""));
    }

    return 1;
}


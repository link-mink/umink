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
#include <umatomic.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <umdaemon.h>
#include <linkhash.h>
#include <uthash.h>
#include <time.h>
#include <json_object.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <utarray.h>

/*************/
/* Plugin ID */
/*************/
static const char *PLG_ID = "plg_sysagent_lua.so";

/**********************************************/
/* list of command implemented by this plugin */
/**********************************************/
int COMMANDS[] = { CMD_LUA_CALL,
                   // end of list marker
                   -1 };

/****************/
/* LUA ENV data */
/****************/
struct lua_env_data {
    umplg_mngr_t *pm;
};

/**********************/
/* LUA ENV Descriptor */
/**********************/
struct lua_env_d {
    // label
    char *name;
    // in case of long running
    // envs, time between each
    // execution
    // 0 - one time execution
    uint64_t interval;
    // activity flag (used only
    // with long running envs)
    uint8_t active;
    // path to lua script
    char *path;
    // plugin manager pointer
    umplg_mngr_t *pm;
    // thread
    pthread_t th;
    // hashable
    UT_hash_handle hh;
};

/*******************/
/* LUA ENV Manager */
/*******************/
struct lua_env_mngr {
    // lua envs
    struct lua_env_d *envs;
    // lock
    pthread_mutex_t mtx;
};

// globals
struct lua_env_mngr *lenv_mngr = NULL;

struct lua_env_mngr *
lenvm_new()
{
    struct lua_env_mngr *lem = malloc(sizeof(struct lua_env_mngr));
    lem->envs = NULL;
    pthread_mutex_init(&lem->mtx, NULL);
    return lem;
}

void
lenvm_free(struct lua_env_mngr *m)
{
    free(m);
}

struct lua_env_d *
lenvm_get_envd(struct lua_env_mngr *lem, const char *n)
{
    // lock
    pthread_mutex_lock(&lem->mtx);
    // find env
    struct lua_env_d *env = NULL;
    HASH_FIND_STR(lem->envs, n, env);
    // unlock
    pthread_mutex_unlock(&lem->mtx);
    // return env
    return env;
}

struct lua_env_d *
lenvm_del_envd(struct lua_env_mngr *lem, const char *n, bool th_safe)
{
    // lock
    if (th_safe) {
        pthread_mutex_lock(&lem->mtx);
    }
    // find env
    struct lua_env_d *env = NULL;
    HASH_FIND_STR(lem->envs, n, env);
    if (env != NULL) {
        HASH_DEL(lem->envs, env);
    }
    // unlock
    if (th_safe) {
        pthread_mutex_unlock(&lem->mtx);
    }
    // return env
    return env;
}

struct lua_env_d *
lenvm_new_envd(struct lua_env_mngr *lem, struct lua_env_d *env)
{
    // lock
    pthread_mutex_lock(&lem->mtx);
    // find env
    struct lua_env_d *tmp_env = NULL;
    HASH_FIND_STR(lem->envs, env->name, tmp_env);
    if (tmp_env == NULL) {
        HASH_ADD_KEYPTR(hh, lem->envs, env->name, strlen(env->name), env);
        tmp_env = env;
    }
    // unlock
    pthread_mutex_unlock(&lem->mtx);
    // return new or exising env
    return tmp_env;
}

bool
lenvm_envd_exists(struct lua_env_mngr *lem, const char *n)
{
    // lock
    pthread_mutex_lock(&lem->mtx);
    // find env
    struct lua_env_d *tmp_env = NULL;
    HASH_FIND_STR(lem->envs, n, tmp_env);
    // unlock
    pthread_mutex_unlock(&lem->mtx);
    return (tmp_env != NULL);
}

void
lenvm_process_envs(struct lua_env_mngr *lem, void (*f)(struct lua_env_d *env))
{
    // lock
    pthread_mutex_lock(&lem->mtx);
    struct lua_env_d *c_env = NULL;
    struct lua_env_d *tmp_env = NULL;
    HASH_ITER(hh, lem->envs, c_env, tmp_env)
    {
        f(c_env);
    }
    // unlock
    pthread_mutex_unlock(&lem->mtx);
}

/*******************/
/* LUA Environment */
/*******************/
static void *
th_lua_env(void *arg)
{
    // lua envd
    struct lua_env_d *env = arg;
    // lua state
    lua_State *L = luaL_newstate();
    if (!L) {
        umd_log(UMD, UMD_LLT_ERROR, "plg_lua: [cannot create Lua environment]");
        return NULL;
    }
    // init lua
    luaL_openlibs(L);

    // load lua script
    FILE *f = fopen(env->path, "r");
    if (f == NULL) {
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) < 0) {
        fclose(f);
        return NULL;
    }
    int32_t fsz = ftell(f);
    if (fsz <= 0) {
        fclose(f);
        return NULL;
    }

    char lua_s[fsz + 1];
    memset(lua_s, 0, fsz + 1);
    rewind(f);
    fread(lua_s, fsz, 1, f);
    fclose(f);

    // load lua script
    if (luaL_loadstring(L, lua_s)) {
        umd_log(UMD, UMD_LLT_ERROR, "plg_lua: [cannot load Lua script (%s)]", env->path);
        lua_close(L);
        return NULL;
    }

    // sleep interval
    uint64_t sec = env->interval / 1000;
    uint64_t nsec = (uint64_t)env->interval % 1000 * 1000000;
    struct timespec st = { sec, nsec };

    // run
    while (!umd_is_terminating() && UM_ATOMIC_GET(&env->active)) {
        // copy precompiled lua chunk (pcall removes it)
        lua_pushvalue(L, -1);
        // push plugin manager pointer
        lua_pushlightuserdata(L, env->pm);
        // run lua script
        if (lua_pcall(L, 1, 1, 0)) {
            umd_log(UMD, UMD_LLT_ERROR, "plg_lua: [%s]", lua_tostring(L, -1));
        }
        // pop result or error message
        lua_pop(L, 1);

        // one-time only
        if (env->interval == 0) {
            break;

            // next iteration
        } else {
            nanosleep(&st, NULL);
        }
    }

    // remove lua state
    lua_close(L);

    return NULL;
}
// lua signal handler (init)
static int
lua_sig_hndlr_init(umplg_sh_t *shd, umplg_data_std_t *d_in)
{
    // get lua env
    struct lua_env_d **env = utarray_eltptr(shd->args, 1);

    // lua state
    lua_State *L = luaL_newstate();
    if (!L) {
        umd_log(UMD, UMD_LLT_ERROR, "plg_lua: [cannot create Lua environment]");
        return 1;
    }
    // init lua
    luaL_openlibs(L);

    // load lua script
    FILE *f = fopen((*env)->path, "r");
    if (f == NULL) {
        printf("fopen err: %s %s\n", (*env)->name, (*env)->path);
        return 2;
    }
    if (fseek(f, 0, SEEK_END) < 0) {
        fclose(f);
        return 3;
    }
    int32_t fsz = ftell(f);
    if (fsz <= 0) {
        fclose(f);
        return 4;
    }

    char lua_s[fsz + 1];
    memset(lua_s, 0, fsz + 1);
    rewind(f);
    fread(lua_s, fsz, 1, f);
    fclose(f);

    // load lua script
    if (luaL_loadstring(L, lua_s)) {
        umd_log(UMD,
                UMD_LLT_ERROR,
                "plg_lua: [cannot load Lua script (%s)]",
                (*env)->path);
        lua_close(L);
        return 5;
    }
    // save state
    utarray_push_back(shd->args, &L);

    // success
    return 0;
}

// lua signal handler (run)
static int
lua_sig_hndlr_run(umplg_sh_t *shd, umplg_data_std_t *d_in, char **d_out, size_t *out_sz)
{
    // get plugin manager and lua env
    umplg_mngr_t **pm = utarray_eltptr(shd->args, 0);
    // get lua state
    lua_State **L = utarray_eltptr(shd->args, 2);
    // copy precompiled lua chunk (pcall removes it)
    lua_pushvalue(*L, -1);
    // push plugin manager pointer
    lua_pushlightuserdata(*L, *pm);
    // push data
    lua_pushlightuserdata(*L, d_in);
    // run lua script
    if (lua_pcall(*L, 2, 1, 0)) {
        umd_log(UMD, UMD_LLT_ERROR, "plg_lua: [%s]", lua_tostring(*L, -1));
    }
    // check return (STRING)
    if (lua_isstring(*L, -1)) {
        // copy lua string to output buffer
        size_t sz = strlen(lua_tostring(*L, -1)) + 1;
        char *out = malloc(sz);
        int r = snprintf(out, sz, "%s", lua_tostring(*L, -1));
        // error (buffer too small)
        if (r <= 0 || r >= sz) {
            free(out);
            *out_sz = 0;
            // pop result or error message
            lua_pop(*L, 1);
            return 1;

            // success
        } else {
            *d_out = out;
            *out_sz = sz;
        }

        // NUMBER
    } else if (lua_isnumber(*L, -1)) {
        // copy lua number to output buffer
        size_t sz = 256;
        char *out = malloc(256);
        int r = snprintf(out, sz, "%f", lua_tonumber(*L, -1));
        // error (buffer too small)
        if (r <= 0 || r >= sz) {
            *out_sz = 0;
            // pop result or error message
            lua_pop(*L, 1);
            return 1;

            // success
        } else {
            *d_out = out;
            *out_sz = sz;
        }
    }
    // pop result or error message
    lua_pop(*L, 1);
    // success
    return 0;
}

// process plugin configuration
static int
process_cfg(umplg_mngr_t *pm, struct lua_env_mngr *lem)
{
    // get config
    if (pm->cfg == NULL) {
        return 1;
    }
    // cast (100% json)
    struct json_object *jcfg = pm->cfg;
    // find plugin id
    if (!json_object_is_type(jcfg, json_type_object)) {
        return 2;
    }
    // loop keys
    struct json_object *plg_cfg = NULL;
    json_object_object_foreach(jcfg, k, v)
    {
        if (strcmp(k, PLG_ID) == 0) {
            plg_cfg = v;
            break;
        }
    }
    // config found?
    if (plg_cfg == NULL) {
        return 3;
    }
    // get main handler (CMD_CALL)
    struct json_object *jobj = json_object_object_get(plg_cfg, "cmd_call");
    if (jobj != NULL) {
        // create ENV descriptor
        struct lua_env_d *env = malloc(sizeof(struct lua_env_d));
        env->name = strdup("CMD_CALL");
        env->interval = 0;
        UM_ATOMIC_COMP_SWAP(&env->active, 0, 1);
        env->path = strdup(json_object_get_string(jobj));
        // add to list
        lenvm_new_envd(lem, env);
    }
    // get envs
    jobj = json_object_object_get(plg_cfg, "envs");
    if (jobj != NULL && json_object_is_type(jobj, json_type_array)) {
        // loop an verify envs
        int env_l = json_object_array_length(jobj);
        for (int i = 0; i < env_l; ++i) {
            // get array object (v declared in json_object_object_foreach macro)
            v = json_object_array_get_idx(jobj, i);
            // check env object type
            if (!json_object_is_type(v, json_type_object)) {
                umd_log(UMD, UMD_LLT_ERROR, "plg_lua: [cannot start Lua environment]");
                return 4;
            }
            // *******************************
            // *** Get ENV values (verify) ***
            // *******************************
            // get values
            struct json_object *j_n = json_object_object_get(v, "name");
            struct json_object *j_as = json_object_object_get(v, "auto_start");
            struct json_object *j_int = json_object_object_get(v, "interval");
            struct json_object *j_p = json_object_object_get(v, "path");
            struct json_object *j_ev = json_object_object_get(v, "events");
            // all values are mandatory
            if (!(j_n && j_as && j_int && j_p && j_ev)) {
                umd_log(UMD,
                        UMD_LLT_ERROR,
                        "plg_lua: [malformed Lua environment (missing values)]");
                return 5;
            }
            // check types
            if (!(json_object_is_type(j_n, json_type_string) &&
                  json_object_is_type(j_as, json_type_boolean) &&
                  json_object_is_type(j_int, json_type_int) &&
                  json_object_is_type(j_p, json_type_string) &&
                  json_object_is_type(j_ev, json_type_array))) {

                umd_log(UMD,
                        UMD_LLT_ERROR,
                        "plg_lua: [malformed Lua environment (wrong type)]");
                return 6;
            }

            // check lua script size
            const char *s_p = json_object_get_string(j_p);
            FILE *f = fopen(s_p, "r");
            if (f == NULL) {
                umd_log(UMD, UMD_LLT_ERROR, "plg_lua: [malformed Lua script (%s)]", s_p);
                return 6;
            }
            if (fseek(f, 0, SEEK_END) < 0) {
                umd_log(UMD, UMD_LLT_ERROR, "plg_lua: [malformed Lua script (%s)]", s_p);
                fclose(f);
                return 7;
            }
            int32_t fsz = ftell(f);
            if (fsz <= 0) {
                umd_log(UMD, UMD_LLT_ERROR, "plg_lua: [malformed Lua script (%s)]", s_p);
                fclose(f);
                return 8;
            }

            // create ENV descriptor
            struct lua_env_d *env = malloc(sizeof(struct lua_env_d));
            env->name = strdup(json_object_get_string(j_n));
            env->interval = json_object_get_uint64(j_int);
            env->pm = pm;
            UM_ATOMIC_COMP_SWAP(&env->active, 0, json_object_get_boolean(j_as));
            env->path = strdup(json_object_get_string(j_p));

            // register events
            int ev_l = json_object_array_length(j_ev);
            for (int i = 0; i < ev_l; ++i) {
                // get array object (v declared in json_object_object_foreach
                // macro)
                v = json_object_array_get_idx(j_ev, i);
                // verify type
                if (!json_object_is_type(v, json_type_string)) {
                    continue;
                }
                // create signal
                umplg_sh_t *sh = malloc(sizeof(umplg_sh_t));
                sh->id = strdup(json_object_get_string(v));
                sh->run = &lua_sig_hndlr_run;
                sh->init = &lua_sig_hndlr_init;
                UT_icd icd = { sizeof(void *), NULL, NULL, NULL };
                utarray_new(sh->args, &icd);
                utarray_push_back(sh->args, &pm);
                utarray_push_back(sh->args, &env);

                // register signal
                umplg_reg_signal(pm, sh);
                umd_log(UMD,
                        UMD_LLT_ERROR,
                        "plg_lua: [attaching '%s' to '%s' event]",
                        env->path,
                        sh->id);
            }
            // add to list
            lenvm_new_envd(lem, env);
        }
    }

    return 0;
}

static void
process_lua_envs(struct lua_env_d *env)
{
    // check if ENV should auto-start
    if (env->interval >= 0 && strcmp(env->name, "CMD_CALL") != 0) {
        if (pthread_create(&env->th, NULL, th_lua_env, env)) {
            umd_log(UMD,
                    UMD_LLT_ERROR,
                    "plg_lua: [cannot start [%s] environment",
                    env->name);
        }
    }
}

static void
shutdown_lua_envs(struct lua_env_d *env)
{
    if (strcmp(env->name, "CMD_CALL") != 0) {
        pthread_join(env->th, NULL);
    }
    // cleanup
    lenvm_del_envd(lenv_mngr, env->name, false);
    free(env->name);
    free(env->path);
    free(env);
}

/****************/
/* init handler */
/****************/
int
init(umplg_mngr_t *pm, umplgd_t *pd)
{
    // lue env manager
    lenv_mngr = lenvm_new();
    if (process_cfg(pm, lenv_mngr)) {
        umd_log(UMD, UMD_LLT_ERROR, "plg_lua: [cannot process plugin configuration]");
    }
    // create environments
    lenvm_process_envs(lenv_mngr, &process_lua_envs);

    return 0;
}

/*********************/
/* terminate handler */
/*********************/
int
terminate(umplg_mngr_t *pm, umplgd_t *pd)
{
    // stop envs
    lenvm_process_envs(lenv_mngr, &shutdown_lua_envs);
    // free env manager
    lenvm_free(lenv_mngr);
    return 0;
}

/*************************/
/* local command handler */
/*************************/
int
run_local(umplg_mngr_t *pm, umplgd_t *pd, int cmd_id, umplg_idata_t *data)
{
    // unused
    return 0;
}

/*******************/
/* command handler */
/*******************/
int
run(umplg_mngr_t *pm, umplgd_t *pd, int cmd_id, umplg_idata_t *data)
{
    // unused
    return 0;
}

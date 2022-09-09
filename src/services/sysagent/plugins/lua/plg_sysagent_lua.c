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
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <umdaemon.h>
#include <linkhash.h>
#include <uthash.h>
#include <json_object.h>

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

struct lua_env_mngr *lenvm_new() {
    struct lua_env_mngr *lem = malloc(sizeof(struct lua_env_mngr));
    lem->envs = NULL;
    pthread_mutex_init(&lem->mtx, NULL);
    return lem;
}

struct lua_env_d *lenvm_get_envd(struct lua_env_mngr *lem, const char *n) {
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

struct lua_env_d *lenvm_new_envd(struct lua_env_mngr *lem,
                                 struct lua_env_d *env) {
    // lock
    pthread_mutex_lock(&lem->mtx);
    //find env
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

bool lenvm_envd_exists(struct lua_env_mngr *lem, const char *n) {
    // lock
    pthread_mutex_lock(&lem->mtx);
    //find env
    struct lua_env_d *tmp_env = NULL;
    HASH_FIND_STR(lem->envs, n, tmp_env);
    // unlock
    pthread_mutex_unlock(&lem->mtx);
    return (tmp_env != NULL);
}

void lenvm_process_envs(struct lua_env_mngr *lem,
                        void (*f)(struct lua_env_d *env)) {
    // lock
    pthread_mutex_lock(&lem->mtx);
    struct lua_env_d *tmp_env = NULL;
    for (tmp_env = lem->envs; tmp_env != NULL; tmp_env = tmp_env->hh.next) {
        f(tmp_env);
    }
    // unlock
    pthread_mutex_unlock(&lem->mtx);
}

// process plugin configuration
static int process_cfg(umplg_mngr_t *pm, struct lua_env_mngr *lem) {
    // get config
    if (pm->cfg == NULL) {
        return 1;
    }
    // cast (100% json)
    struct json_object *jcfg = pm->cfg;
    // find plugin id
    if(!json_object_is_type(jcfg, json_type_object)){
        return 2;
    }
    // loop keys
    struct json_object *plg_cfg = NULL;
    json_object_object_foreach(jcfg, k, v) {
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
        env->active = 1;
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
                umd_log(UMD, UMD_LLT_ERROR,
                        "plg_lua: [cannot start LUA environment]");
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
                umd_log(UMD, UMD_LLT_ERROR,
                        "plg_lua: [malformed Lua environment (missing values)]");
                return 5;
            }
            // check types
            if (!(json_object_is_type(j_n, json_type_string) &&
                  json_object_is_type(j_as, json_type_boolean) &&
                  json_object_is_type(j_int, json_type_int) &&
                  json_object_is_type(j_p, json_type_string) &&
                  json_object_is_type(j_ev, json_type_array))) {

                umd_log(UMD, UMD_LLT_ERROR,
                        "plg_lua: [malformed Lua environment (wrong type)]");
                return 6;
            }

            // check lua script size
            const char *s_p = json_object_get_string(j_p);
            FILE *f = fopen(s_p, "r");
            if (f == NULL) {
                umd_log(UMD, UMD_LLT_ERROR,
                        "plg_lua: [malformed Lua script (%s)]", s_p);
                return 6;

            }
            if (fseek(f, 0, SEEK_END) < 0) {
                umd_log(UMD, UMD_LLT_ERROR,
                        "plg_lua: [malformed Lua script (%s)]", s_p);
                return 7;
            }
            int32_t fsz = ftell(f);
            if (fsz <= 0) {
                umd_log(UMD, UMD_LLT_ERROR,
                        "plg_lua: [malformed Lua script (%s)]", s_p);
                fclose(f);
                return 8;
            }

            // create ENV descriptor
            struct lua_env_d *env = malloc(sizeof(struct lua_env_d));
            env->name = strdup(json_object_get_string(j_n));
            env->interval = json_object_get_uint64(j_int);
            env->active = 1;
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
                // register signal
                umplg_reg_signal(pm, sh);
                umd_log(UMD, UMD_LLT_ERROR,
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

/****************/
/* init handler */
/****************/
int init(umplg_mngr_t *pm, umplgd_t *pd) {
    // lue env manager
    struct lua_env_mngr *lem = lenvm_new();
    if (process_cfg(pm, lem))  {
        umd_log(UMD, UMD_LLT_ERROR,
                "plg_lua: [cannot process plugin configuration]");
    }
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



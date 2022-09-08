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
#include <dlfcn.h>
#include <dlfcn.h>


umplgd_t *umplg_load(umplg_mngr_t *pm, const char *fpath){
    // open and resolve symbols now
    void *h = dlopen(fpath, RTLD_NOW | RTLD_GLOBAL);
    if (!h)
        return NULL;

    // success, check if plg is a valid plugin
    // check for init, term and cmd handler
    const int *reg_hooks = dlsym(h, UMPLG_CMD_LST);
    umplg_inith_t init = dlsym(h, UMPLG_INIT_FN);
    umplg_termh_t term = dlsym(h, UMPLG_TERM_FN);
    umplg_cmdh_t cmdh = dlsym(h, UMPLG_CMD_HNDLR);
    umplg_cmdh_t cmdh_l = dlsym(h, UMPLG_CMD_HNDLR_LOCAL);

    // first 4 must exist
    if (!(reg_hooks && init && term && cmdh)) {
        dlclose(h);
        return NULL;
    }
    // check if all requested hooks are free
    const int *tmp_rh = reg_hooks;
    umplg_hkd_t *hook = NULL;
    while (*tmp_rh != -1) {
        HASH_FIND_INT(pm->hooks, tmp_rh, hook);
        if (hook != NULL) {
            dlclose(h);
            return NULL;
        }
    }

    // create descriptor
    umplgd_t *pd = malloc(sizeof(umplgd_t));
    // set data
    pd->handle = h;
    pd->name = strdup(fpath);
    pd->type = 0;
    pd->cmdh = cmdh;
    pd->cmdh_l = cmdh_l;
    pd->termh = term;
    pd->data = NULL;

    // attach hooks to plugin
    tmp_rh = reg_hooks;
    while (*tmp_rh != -1){
        // create new hook descriptor
        hook = malloc(sizeof(umplg_hkd_t));
        hook->id = *tmp_rh;
        hook->plgp = pd;
        // add to map
        HASH_ADD_INT(pm->hooks, id, hook);
        // next
        tmp_rh++;
    }

    // run ninit method
    if (!init(pm, pd)) {
        // add to list
        utarray_push_back(pm->plgs, pd);
    }

    // return descriptor
    return pd;
}


int umplg_unload(umplg_mngr_t *pm, umplgd_t *pd) {
    return 0;
}

int umplg_run(umplg_mngr_t *pm,
              int cmd_id,
              int idt,
              umplg_idata_t *data,
              bool is_local) {

    // find plugin from cmd_id (hook)
    umplg_hkd_t *hook = NULL;
    HASH_FIND_INT(pm->hooks, &cmd_id, hook);
    if (hook == NULL) {
        return 1;
    }
    // local
    if (is_local) {
        // handler implemented? (optional)
        if (hook->plgp->cmdh_l) {
            return hook->plgp->cmdh_l(pm, hook->plgp, cmd_id, data);

        // local handler not found
        } else {
            return -1;
        }
    }

    // remote
    return hook->plgp->cmdh(pm, hook->plgp, cmd_id, data);
}

int umplg_reg_signal(umplg_mngr_t *pm, const char *s, umplg_sh_t *sh) {
    // TODO
    return 0;
}

int umplg_proc_signal(umplg_mngr_t *pm,
                      const char *s,
                      umplg_data_std_t *d_in,
                      char *d_out) {
    // TODO
    return 0;
}

umplg_mngr_t *umplg_new_mngr() {
    umplg_mngr_t *pm = malloc(sizeof(umplg_mngr_t));
    pm->hooks = NULL;
    pm->plgs = NULL;
    return pm;
}

void umplg_free_mngr(umplg_mngr_t *pm) {
    umplgd_t *pd = NULL;
    // free plugins
    for (pd = (umplgd_t *)utarray_front(pm->plgs);
         pd != NULL;
         pd = (umplgd_t *)utarray_next(pm->plgs, pd)) {

        // terminate
        pd->termh(pm, pd);
        // free mem
        dlclose(pd->handle);
        free(pd->name);
        free(pd);
    }
    // release plugin manager memory
    utarray_free(pm->plgs);
    HASH_CLEAR(hh, pm->hooks);
    free(pm);
}

/*
 *               _____  ____ __
 *   __ ____ _  /  _/ |/ / //_/
 *  / // /  ' \_/ //    / ,<
 *  \_,_/_/_/_/___/_/|_/_/|_|
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <syslog.h>
#include <umdaemon.h>
#include <umatomic.h>

// running UMD
umdaemon_t *UMD = NULL;

umdaemon_t *
umd_create(const char *id, const char *type)
{
    if (id == 0 || type == 0) {
        return 0;
    }
    int l1 = strlen(id);
    int l2 = strlen(type);
    if (l1 < UMD_ID_MAX_SZ && l2 < UMD_ID_MAX_SZ) {
        // new descriptor
        umdaemon_t *res = (umdaemon_t *)calloc(sizeof(umdaemon_t), 1);
        // id/type
        strcpy(res->id, id);
        strcpy(res->type, type);
        // fqn
        res->fqn = (char *)malloc(l2 + l1 + 7);
        strcpy(res->fqn, "mink.");
        strcpy(&res->fqn[5], type);
        strcpy(&res->fqn[5 + l2], ".");
        strcpy(&res->fqn[6 + l2], id);
        res->fqn[l2 + l1 + 6] = 0;
        // default log level
        res->log_level = UMD_LLT_INFO;
        // set current running daemon
        UMD = res;
        return res;
    }
    return 0;
}

int
umd_set_id(umdaemon_t *umd, const char *id)
{
    if (!id || !umd) {
        return 1;
    }
    if (strlen(id) >= UMD_ID_MAX_SZ) {
        return 1;
    }
    strcpy(umd->id, id);
    int l1 = strlen(id);
    int l2 = strlen(umd->type);
    // change fqn
    umd->fqn = (char *)realloc(umd->fqn, l2 + l1 + 7);
    strcpy(umd->fqn, "mink.");
    strcpy(&umd->fqn[5], umd->type);
    strcpy(&umd->fqn[5 + l2], ".");
    strcpy(&umd->fqn[6 + l2], id);
    umd->fqn[l2 + l1 + 6] = 0;
    return 0;
}

void
umd_set_log_level(umdaemon_t *umd, enum umd_log_level_t ll)
{
    if (!umd) {
        return;
    }
    umd->log_level = ll;
}

void
umd_signal_handler(int signum)
{
    if (!UMD) {
        return;
    }
    switch (signum) {
    case SIGTERM:
        UM_ATOMIC_COMP_SWAP(&UMD->is_terminated, 0, 1);
        break;
    }
}

void
umd_init(umdaemon_t *umd)
{
    // n/a
}

void
umd_start(umdaemon_t *umd)
{
    if (!umd) {
        return;
    }
    // open log
    openlog(umd->fqn, LOG_PID | LOG_CONS, LOG_USER);
    // log
    syslog(LOG_INFO, "starting...");
    // call on init handler
    if (umd->on_init)
        umd->on_init(umd);
}

void
umd_terminate(umdaemon_t *umd)
{
    if (!umd) {
        return;
    }
    // open log
    openlog(umd->fqn, LOG_PID | LOG_CONS, LOG_USER);
    // log
    syslog(LOG_INFO, "terminating...");
    // call on terminate handler
    if (umd->on_terminate)
        umd->on_terminate(umd);
    closelog();
}

void
umd_destroy(umdaemon_t *umd)
{
    if (umd) {
        free(umd->fqn);
        free(umd);
    }
}

void
umd_loop(umdaemon_t *umd)
{
    while (!UM_ATOMIC_GET(&umd->is_terminated))
        sleep(1);
    // terminate
    umd_terminate(umd);
}

void
umd_log(umdaemon_t *umd, enum umd_log_level_t level, const char *msg, ...)
{
    va_list argp;
    va_start(argp, msg);
    // log level check
    if (level <= umd->log_level) {
        // open log
        openlog(umd->fqn, LOG_PID | LOG_CONS, LOG_USER);
        // log
        vsyslog(LOG_USER | level, msg, argp);
    }
    va_end(argp);
}

bool
umd_is_terminating()
{
    if (UMD != NULL){
        return UM_ATOMIC_GET(&UMD->is_terminated);
    }
    return false;
}

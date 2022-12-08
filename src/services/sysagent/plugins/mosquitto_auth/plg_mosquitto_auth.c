/*
 *               _____  ____ __
 *   __ ____ _  /  _/ |/ / //_/
 *  / // /  ' \_/ //    / ,<
 *  \_,_/_/_/_/___/_/|_/_/|_|
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <string.h>
#include <mosquitto.h>
#include <mosquitto_plugin.h>
#include <umdb.h>

// mink db
static char *db_name = NULL;
// db manager
static umdb_mngrd_t *dbm = NULL;

// plugin version 4
int
mosquitto_auth_plugin_version()
{
    return MOSQ_AUTH_PLUGIN_VERSION;
}

// Called after the plugin has been loaded and mosquitto_auth_plugin_version
// has been called.  This will only ever be called once and can be used to
// initialise the plugin.
int
mosquitto_auth_plugin_init(void **user_data, struct mosquitto_opt *opts, int opt_count)
{
    // process options
    for (int i = 0; i < opt_count; i++) {
        if (strncmp(opts[i].key, "db_name", 7) == 0) {
            db_name = opts[i].value;
        }
    }
    // missing db options
    if (db_name == NULL) {
        mosquitto_log_printf(MOSQ_LOG_ERR,
                             "plg_mosquitto_auth: db_name option is missing");
        return MOSQ_ERR_AUTH;
    }
    // create umdbm and connect
    dbm = umdb_mngr_new(db_name);
    if (dbm == NULL) {
        mosquitto_log_printf(MOSQ_LOG_ERR, "plg_mosquitto_auth: error while connecting");
        return MOSQ_ERR_AUTH;
    }

    // plugin initialised
    return MOSQ_ERR_SUCCESS;
}

// Called when the broker is shutting down.  This will only ever be called once
// per plugin.  Note that mosquitto_auth_security_cleanup will be called
// directly before this function.
int
mosquitto_auth_plugin_cleanup(void *userdata,
                              struct mosquitto_opt *options,
                              int option_count)
{
    return MOSQ_ERR_SUCCESS;
}

// Called:
// 1. When the broker starts up.
// 2. If the broker is requested to reload its configuration whilst running.
//    In this case, mosquitto_auth_security_cleanup will be called first, then
//    this function will be called.  In this situation, the reload parameter
//    will be true.
int
mosquitto_auth_security_init(void *user_data,
                             struct mosquitto_opt *opts,
                             int opt_count,
                             bool reload)
{
    return MOSQ_ERR_SUCCESS;
}

// Called:
// 1. When the broker is shutting down.
// 2. If the broker is requested to reload its configuration whilst running.
//    In this case, this function will be called, followed by
//    mosquitto_auth_security_init. In this situation, the reload parameter will be true.
int
mosquitto_auth_security_cleanup(void *user_data,
                                struct mosquitto_opt *opts,
                                int opt_count,
                                bool reload)
{
    return MOSQ_ERR_SUCCESS;
}

// This function is OPTIONAL.  Only include this function in your plugin if you
// are making basic username/password checks.  Called by the broker when a
// username/password must be checked.
int
mosquitto_auth_unpwd_check(void *user_data,
                           struct mosquitto *client,
                           const char *username,
                           const char *password)
{
    // anonymous not allowed
    if (username == NULL) {
        return MOSQ_ERR_AUTH;
    }
    // find user in db
    umdb_uauth_d_t uauth;
    if (!umdb_mngr_uauth(dbm, &uauth, username, password)) {
        // not found
        if (uauth.auth != 1) {
            return MOSQ_ERR_AUTH;
        }

    } else {
        mosquitto_log_printf(MOSQ_LOG_ERR, "plg_mosquitto_auth: cannot authenicate user");
        return MOSQ_ERR_AUTH;
    }

    // authenticated
    return MOSQ_ERR_SUCCESS;
}

// Called by the broker when topic access must be checked. access will be one
// of: MOSQ_ACL_SUBSCRIBE when a client is asking to subscribe to a topic
// string.  This differs from MOSQ_ACL_READ in that it allows you to deny
// access to topic strings rather than by pattern.  For example, you may use
// MOSQ_ACL_SUBSCRIBE to deny subscriptions to ‘#’, but allow all topics in
// MOSQ_ACL_READ.  This allows clients to subscribe to any topic they want, but
// not discover what topics are in use on the server.  MOSQ_ACL_READ when a
// message is about to be sent to a client (i.e. whether it can read that topic
// or not).  MOSQ_ACL_WRITE when a message has been received from a client
// (i.e. whether it can write to that topic or not).
int
mosquitto_auth_acl_check(void *user_data,
                         int access,
                         struct mosquitto *client,
                         const struct mosquitto_acl_msg *msg)
{
    // get username for client
    const char *username = mosquitto_client_username(client);
    // anonymous not allowed
    if (username == NULL) {
        return MOSQ_ERR_AUTH;
    }
    // find user in db
    umdb_uauth_d_t uauth;
    if (!umdb_mngr_uget(dbm, &uauth, username)) {
        // not found
        if (uauth.id < 1) {
            return MOSQ_ERR_AUTH;
        }
        // "admin" topic requested, check user flags
        if (strncmp(msg->topic, "mink/admin/", 11) == 0 && uauth.flags != 1) {
            return MOSQ_ERR_AUTH;
        }

    } else {
        mosquitto_log_printf(MOSQ_LOG_ERR, "plg_mosquitto_auth: cannot authenicate user");
        return MOSQ_ERR_AUTH;
    }

    // authenticated
    return MOSQ_ERR_SUCCESS;
}


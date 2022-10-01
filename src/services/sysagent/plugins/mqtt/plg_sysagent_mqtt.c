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
#include <utarray.h>
#include <MQTTAsync.h>

/*************/
/* Plugin ID */
/*************/
static const char *PLG_ID = "plg_sysagent_mqtt.so";

/**********************************************/
/* list of command implemented by this plugin */
/**********************************************/
int COMMANDS[] = { CMD_MQTT_PUBLISH,
                   // end of list marker
                   -1 };

static const char *SIG_MQTT_RX = "mqtt:RX";

/******************************/
/* MQTT connection descriptor */
/******************************/
struct mqtt_conn_d {
    // label
    char *name;
    // mqtt async client
    MQTTAsync client;
    // plugin manager pointer
    umplg_mngr_t *pm;
    // mqtt topics
    UT_array *topics;
    // hashable
    UT_hash_handle hh;
};

/***************************/
/* MQTT connection manager */
/***************************/
struct mqtt_conn_mngr {
    // mqtt connections
    struct mqtt_conn_d *conns;
};

// fwd declarations
static int mqtt_conn_sub(struct mqtt_conn_d *conn, const char *t);
static int mqtt_conn_connect(struct mqtt_conn_d *conn, struct json_object *j_conn);

struct mqtt_conn_d *
mqtt_conn_new(umplg_mngr_t *pm)
{
    struct mqtt_conn_d *c = malloc(sizeof(struct mqtt_conn_d));
    c->pm = pm;
    utarray_new(c->topics, &ut_str_icd);
    return c;
}

static int
mqtt_on_rx(void *ctx, char *t, int t_sz, MQTTAsync_message *msg)
{
    // context
    struct mqtt_conn_d *conn = ctx;
    // data
    umplg_data_std_t e_d = { .items = NULL };
    umplg_data_std_items_t items = { .table = NULL };
    umplg_data_std_item_t item_topic = { .name = "mqtt_topic", .value = t };
    umplg_data_std_item_t item_pld = { .name = "mqtt_payload", .value = t };
    // create signal input data
    umplg_stdd_init(&e_d);
    umplg_stdd_item_add(&items, &item_topic);
    umplg_stdd_item_add(&items, &item_pld);
    umplg_stdd_items_add(&e_d, &items);

    // output buffer
    char *b = NULL;
    size_t b_sz = 0;
    // process signal
    umplg_proc_signal(conn->pm, SIG_MQTT_RX, &e_d, &b, &b_sz);

    // cleanup
    MQTTAsync_freeMessage(&msg);
    MQTTAsync_free(t);
    return 1;
}

static void
mqtt_on_connect(void *context, char *cause)
{
    // context
    struct mqtt_conn_d *conn = context;
    // get topics
    char **t = NULL;
    while ((t = (char **)utarray_next(conn->topics, t))) {
        mqtt_conn_sub(conn, *t);
    }
}

static int
mqtt_conn_connect(struct mqtt_conn_d *conn, struct json_object *j_conn)
{
    // address and client id
    struct json_object *j_addr = json_object_object_get(j_conn, "address");
    struct json_object *j_clid = json_object_object_get(j_conn, "client_id");
    // username and password
    struct json_object *j_usr = json_object_object_get(j_conn, "username");
    struct json_object *j_pwd = json_object_object_get(j_conn, "password");

    // mqtt connec tion setup
    MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
    MQTTAsync_SSLOptions ssl_opts = MQTTAsync_SSLOptions_initializer;
    int rc = MQTTAsync_create(&conn->client,
                              json_object_get_string(j_addr),
                              json_object_get_string(j_clid),
                              MQTTCLIENT_PERSISTENCE_NONE,
                              NULL);
    if (rc != MQTTASYNC_SUCCESS) {
        return 1;
    }

    // set callbacks (RX)
    if (MQTTAsync_setCallbacks(conn->client, conn, NULL, mqtt_on_rx, NULL) !=
        MQTTASYNC_SUCCESS) {
        return 2;
    }
    // connection options
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
    conn_opts.onSuccess = NULL;
    conn_opts.onFailure = NULL;
    conn_opts.automaticReconnect = 1;
    conn_opts.context = conn;
    conn_opts.username = json_object_get_string(j_usr);
    conn_opts.password = json_object_get_string(j_pwd);
    conn_opts.ssl = &ssl_opts;

    // connect
    if (MQTTAsync_connect(conn->client, &conn_opts) != MQTTASYNC_SUCCESS) {
        return 3;
    }
    // set callbacks
    MQTTAsync_setConnected(conn->client, conn, mqtt_on_connect);

    // success
    return 0;
}

static int
mqtt_conn_pub(struct mqtt_conn_d *conn,
              const char *d,
              size_t d_sz,
              const char *t,
              bool retain)
{
    // setup mqtt payload
    MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
    MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
    opts.context = conn;
    pubmsg.payload = (void *)d;
    pubmsg.payloadlen = d_sz;
    pubmsg.qos = 1;
    pubmsg.retained = retain;
    // send
    if (MQTTAsync_sendMessage(conn->client, t, &pubmsg, &opts) != MQTTASYNC_SUCCESS) {
        // error
        return 1;
    }
    // ok
    return 0;
}

static int
mqtt_conn_sub(struct mqtt_conn_d *conn, const char *t)
{
    MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
    if (MQTTAsync_subscribe(conn->client, t, 1, &opts) != MQTTASYNC_SUCCESS) {
        // error
        return 1;
    }
    // ok
    return 0;
}

static void
mqtt_conn_add_topic(struct mqtt_conn_d *conn, const char *t)
{
    utarray_push_back(conn->topics, t);
}

static struct mqtt_conn_mngr *
mqtt_mngr_new()
{
    struct mqtt_conn_mngr *m = malloc(sizeof(struct mqtt_conn_mngr));
    m->conns = NULL;
    return m;
}

static struct mqtt_conn_d *
mqtt_mngr_add_conn(struct mqtt_conn_mngr *m, umplg_mngr_t *pm, struct json_object *j_conn)
{

    // address
    struct json_object *j_addr = json_object_object_get(j_conn, "address");
    // label
    struct json_object *j_name = json_object_object_get(j_conn, "name");
    // sanity check
    if (j_addr == NULL || j_name == NULL) {
        return NULL;
    }
    // label should not be present
    struct mqtt_conn_d *tmp_conn = NULL;
    HASH_FIND_STR(m->conns, json_object_get_string(j_name), tmp_conn);
    if (tmp_conn != NULL) {
        return NULL;
    }
    // new mqtt connectino
    struct mqtt_conn_d *c = mqtt_conn_new(pm);
    c->name = strdup(json_object_get_string(j_name));
    // add to conn list
    HASH_ADD_KEYPTR(hh, m->conns, c->name, strlen(c->name), c);
    // return new conn
    return c;
}

static void
mqtt_mngr_del_conn(struct mqtt_conn_mngr *m, const char *name)
{
    struct mqtt_conn_d *tmp_conn = NULL;
    HASH_FIND_STR(m->conns, name, tmp_conn);
    if (tmp_conn != NULL) {
        HASH_DEL(m->conns, tmp_conn);
        free(tmp_conn);
    }
}

static struct mqtt_conn_d *
mqtt_mngr_get_conn(struct mqtt_conn_mngr *m, const char *name)
{
    struct mqtt_conn_d *tmp_conn = NULL;
    HASH_FIND_STR(m->conns, name, tmp_conn);
    return tmp_conn;
}

static int
process_cfg(umplg_mngr_t *pm, struct mqtt_conn_mngr *mngr)
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
    // get connections
    struct json_object *jobj = json_object_object_get(plg_cfg, "connections");
    if (jobj != NULL && json_object_is_type(jobj, json_type_array)) {
        // loop an verify connections
        int conn_l = json_object_array_length(jobj);
        for (int i = 0; i < conn_l; ++i) {
            // get array object (v declared in json_object_object_foreach macro)
            struct json_object *j_conn = json_object_array_get_idx(jobj, i);
            // check env object type
            if (!json_object_is_type(v, json_type_object)) {
                umd_log(UMD, UMD_LLT_ERROR, "plg_mqtt: [invalid MQTT connection object]");
                return 4;
            }
            // get connection values
            struct json_object *j_n = json_object_object_get(j_conn, "name");
            struct json_object *j_addr = json_object_object_get(j_conn, "address");
            struct json_object *j_clid = json_object_object_get(j_conn, "client_id");
            struct json_object *j_usr = json_object_object_get(j_conn, "username");
            struct json_object *j_pwd = json_object_object_get(j_conn, "password");
            // all values are mandatory
            if (!(j_n && j_addr && j_clid && j_usr && j_pwd)) {
                umd_log(UMD,
                        UMD_LLT_ERROR,
                        "plg_mqtt: [malformed MQTT connection (missing values)]");
                return 5;
            }
            // check types
            if (!(json_object_is_type(j_n, json_type_string) &&
                  json_object_is_type(j_addr, json_type_string) &&
                  json_object_is_type(j_clid, json_type_string) &&
                  json_object_is_type(j_usr, json_type_string) &&
                  json_object_is_type(j_pwd, json_type_string))) {

                umd_log(UMD,
                        UMD_LLT_ERROR,
                        "plg_mqtt: [malformed MQTT connection (wrong type)]");
                return 6;
            }
            // create MQTT connection
            struct mqtt_conn_d *conn = mqtt_mngr_add_conn(mngr, pm, j_conn);
            if (!conn) {
                continue;
            }
            // subscribe to topics
            struct json_object *j_sub = json_object_object_get(j_conn, "subscriptions");
            if (j_sub != NULL && json_object_is_type(j_sub, json_type_array)) {
                int sub_l = json_object_array_length(j_sub);
                for (int i = 0; i < sub_l; ++i) {
                    // get array object (v declared in json_object_object_foreach macro)
                    v = json_object_array_get_idx(j_sub, i);
                    // verify type
                    if (!json_object_is_type(v, json_type_string)) {
                        continue;
                    }
                    mqtt_conn_add_topic(conn, json_object_get_string(v));
                }
            }
            umd_log(UMD, UMD_LLT_INFO, "plg_mqtt: [adding connection [%s]", conn->name);
            mqtt_conn_connect(conn, j_conn);
        }
    }

    return 0;
}

/****************/
/* init handler */
/****************/
int
init(umplg_mngr_t *pm, umplgd_t *pd)
{
    // mqtt conn manager
    struct mqtt_conn_mngr *mngr = mqtt_mngr_new();
    if (process_cfg(pm, mngr)) {
        umd_log(UMD, UMD_LLT_ERROR, "plg_mqtt: [cannot process plugin configuration]");
    }
    return 0;
}

/*********************/
/* terminate handler */
/*********************/
int
terminate(umplg_mngr_t *pm, umplgd_t *pd)
{
    // TODO
    return 0;
}

/*************************/
/* local command handler */
/*************************/
int
run_local(umplg_mngr_t *pm, umplgd_t *pd, int cmd_id, umplg_idata_t *data)
{
    // TODO
    return 0;
}

/*******************/
/* command handler */
/*******************/
int
run(umplg_mngr_t *pm, umplgd_t *pd, int cmd_id, umplg_idata_t *data)
{
    // not used
    return 0;
}

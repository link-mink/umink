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
    // lock
    pthread_mutex_t mtx;
};

// fwd declarations
static int mqtt_conn_sub(struct mqtt_conn_d *conn, const char *t);
static int mqtt_conn_connect(struct mqtt_conn_d *conn, struct json_object *j_conn);

// globals
struct mqtt_conn_mngr *mqtt_mngr = NULL;

struct mqtt_conn_d *
mqtt_conn_new(umplg_mngr_t *pm)
{
    struct mqtt_conn_d *c = malloc(sizeof(struct mqtt_conn_d));
    c->pm = pm;
    c->client = NULL;
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
    // sanity check
    if (conn->client == NULL) {
        return 1;
    }
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
    // sanity check
    if (conn->client == NULL) {
        return 1;
    }
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
    utarray_push_back(conn->topics, &t);
}

static struct mqtt_conn_mngr *
mqtt_mngr_new()
{
    struct mqtt_conn_mngr *m = malloc(sizeof(struct mqtt_conn_mngr));
    m->conns = NULL;
    pthread_mutex_init(&m->mtx, NULL);
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
    // lock
    pthread_mutex_lock(&m->mtx);
    // add to conn list
    HASH_ADD_KEYPTR(hh, m->conns, c->name, strlen(c->name), c);
    // unlock
    pthread_mutex_unlock(&m->mtx);
    // return new conn
    return c;
}

static void
mqtt_mngr_process_conns(struct mqtt_conn_mngr *m, void (*f)(struct mqtt_conn_d *conn))
{
    // lock
    pthread_mutex_lock(&m->mtx);
    struct mqtt_conn_d *c_conn = NULL;
    struct mqtt_conn_d *tmp_conn = NULL;
    HASH_ITER(hh, m->conns, c_conn, tmp_conn)
    {
        f(c_conn);
    }
    // unlock
    pthread_mutex_unlock(&m->mtx);
}

static void
mqtt_mngr_del_conn(struct mqtt_conn_mngr *m, const char *name, bool th_safe)
{
    struct mqtt_conn_d *tmp_conn = NULL;
    // lock
    if (th_safe) {
        pthread_mutex_lock(&m->mtx);
    }
    HASH_FIND_STR(m->conns, name, tmp_conn);
    if (tmp_conn != NULL) {
        HASH_DEL(m->conns, tmp_conn);
        MQTTAsync_destroy(&tmp_conn->client);
        free(tmp_conn->name);
        utarray_free(tmp_conn->topics);
        free(tmp_conn);
    }
    // unlock
    if (th_safe) {
        pthread_mutex_unlock(&m->mtx);
    }
}

static void
mqtt_term(struct mqtt_conn_d *conn)
{
    mqtt_mngr_del_conn(mqtt_mngr, conn->name, false);
}

static void
mqtt_mngr_free(struct mqtt_conn_mngr *m){
    mqtt_mngr_process_conns(mqtt_mngr, &mqtt_term);
    free(m);
}

static struct mqtt_conn_d *
mqtt_mngr_get_conn(struct mqtt_conn_mngr *m, const char *name)
{
    struct mqtt_conn_d *tmp_conn = NULL;
    // lock
    pthread_mutex_lock(&m->mtx);
    HASH_FIND_STR(m->conns, name, tmp_conn);
    // unlock
    pthread_mutex_unlock(&m->mtx);
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
    mqtt_mngr = mqtt_mngr_new();
    if (process_cfg(pm, mqtt_mngr)) {
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
    mqtt_mngr_free(mqtt_mngr);
    return 0;
}

/*************************************/
/* local CMD_MQTT_PUBLISH (standard) */
/*************************************/
static void
impl_mqtt_publish(umplg_data_std_t *data)
{
    // sanity check
    if (data == NULL || utarray_len(data->items) < 3) {
        umd_log(UMD, UMD_LLT_ERROR, "plg_mqtt: [CMD_MQTT_PUBLISH invalid data]");
        return;
    }
    // items elem at index
    umplg_data_std_items_t *row = utarray_eltptr(data->items, 0);
    // sanity check (columns)
    if (HASH_COUNT(row->table) < 1) {
        return;
    }
    // get first column
    umplg_data_std_item_t *column = row->table;

    // get connection
    struct mqtt_conn_d *c = mqtt_mngr_get_conn(mqtt_mngr, column->value);
    if (c == NULL) {
        return;
    }
    // mqtt data
    row = utarray_eltptr(data->items, 2);
    umplg_data_std_item_t *mqtt_data = row->table;
    // mqtt topic
    row = utarray_eltptr(data->items, 1);
    umplg_data_std_item_t *mqtt_topic = row->table;

    // do not retain by default
    bool retain = false;
    // check retain flag in input data
    if (utarray_len(data->items) >= 4) {
        row = utarray_eltptr(data->items, 3);
        retain = atoi(row->table->value);
    }

    // publish
    mqtt_conn_pub(c,
                  mqtt_data->value,
                  strlen(mqtt_data->value),
                  mqtt_topic->value,
                  retain);
}

/*************************/
/* local command handler */
/*************************/
int
run_local(umplg_mngr_t *pm, umplgd_t *pd, int cmd_id, umplg_idata_t *data)
{
    // null checks
    if (data == NULL) {
        return -1;
    }

    // plugin2plugin local interface (standard)
    if (data->type == UMPLG_DT_STANDARD) {
        // plugin input data
        umplg_data_std_t *plg_d = data->data;
        // check command id
        switch (cmd_id) {
        case CMD_MQTT_PUBLISH:
            impl_mqtt_publish(plg_d);
            break;

        default:
            break;
        }

        return 0;
    }

    // unsupported interface
    return -2;
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

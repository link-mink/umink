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
#include <string.h>
#include <umdb.h>

/******************/
/* sql statements */
/******************/
// authenticate user
static const char *SQL_USER_AUTH =
    "SELECT a.id, "
    "       a.flags, "
    "       a.username, "
    "       iif(b.username is NULL, 0, 1) as auth "
    "FROM user a "
    "LEFT JOIN "
    "   (SELECT flags, username  "
    "    FROM user a "
    "    WHERE username = ? AND "
    "    password = ?) b "
    "ON a.username = b.username "
    "WHERE a.username = ?";

// get user
static const char *SQL_USER_GET =
    "SELECT * FROM user "
    "WHERE username = ?";

umdb_mngrd_t *
umdb_mngr_new(const char *db)
{
    sqlite3 *db_p = NULL;
    if (!sqlite3_open_v2(db, &db_p, SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX, NULL)) {
        umdb_mngrd_t *m = malloc(sizeof(umdb_mngrd_t));
        m->db = db_p;
        return m;
    }

    return NULL;
}

void
umdb_mngr_free(umdb_mngrd_t *m)
{
    if (m == NULL) {
        return;
    }
    if (m->db) {
        sqlite3_close(m->db);
    }
    free(m);
}

int
umdb_mngr_uauth(umdb_mngrd_t *m, umdb_uauth_d_t *res, const char *u, const char *p)
{
    if (m == NULL || m->db == NULL || res == NULL || u == NULL || p == NULL) {
        return 1;
    }
    // prepare statement
    sqlite3_stmt *stmt = NULL;
    int r = sqlite3_prepare_v2(m->db, SQL_USER_AUTH, -1, &stmt, NULL);
    if (r != SQLITE_OK) {
        return 2;
    }

    // username
    if (sqlite3_bind_text(stmt, 1, u, strlen(u), SQLITE_STATIC)) {
        return 3;
    }
    // pwd
    if (sqlite3_bind_text(stmt, 2, p, strlen(p), SQLITE_STATIC)) {
        return 4;
    }
    // username
    if (sqlite3_bind_text(stmt, 3, u, strlen(u), SQLITE_STATIC)) {
        return 5;
    }

    // step
    int usr_flags = 0;
    int usr_id = -1;
    int auth = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        usr_id = sqlite3_column_int(stmt, 0);
        usr_flags = sqlite3_column_int(stmt, 1);
        auth = sqlite3_column_int(stmt, 3);
    }
    // cleanup
    if (sqlite3_clear_bindings(stmt)) {
        return 6;
    }
    if (sqlite3_reset(stmt)) {
        return 7;
    }
    if (sqlite3_finalize(stmt)) {
        return 8;
    }

    // user auth result
    res->auth = auth;
    res->id = usr_id;
    res->flags = usr_flags;

    // success
    return 0;
}

int
umdb_mngr_uget(umdb_mngrd_t *m, umdb_uauth_d_t *res, const char *u)
{
    if (m == NULL || m->db == NULL || res == NULL || u == NULL) {
        return 1;
    }
    // prepare statement
    sqlite3_stmt *stmt = NULL;
    int r = sqlite3_prepare_v2(m->db, SQL_USER_GET, -1, &stmt, NULL);
    if (r != SQLITE_OK) {
        return 2;
    }

    // username
    if (sqlite3_bind_text(stmt, 1, u, strlen(u), SQLITE_STATIC)) {
        return 3;
    }

    // step
    int usr_flags = 0;
    int usr_id = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        usr_id = sqlite3_column_int(stmt, 0);
        usr_flags = sqlite3_column_int(stmt, 3);
    }

    // cleanup
    if (sqlite3_clear_bindings(stmt)) {
        return 6;
    }
    if (sqlite3_reset(stmt)) {
        return 7;
    }
    if (sqlite3_finalize(stmt)) {
        return 8;
    }

    // user get result
    res->id = usr_id;
    res->flags = usr_flags;
    res->usr = u;

    // success
    return 0;
}

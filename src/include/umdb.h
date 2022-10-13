/*
 *               _____  ____ __
 *   __ ____ _  /  _/ |/ / //_/
 *  / // /  ' \_/ //    / ,<
 *  \_,_/_/_/_/___/_/|_/_/|_|
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef UMDB
#define UMDB

#include <sqlite3.h>

// types
typedef struct umdb_mngr_d umdb_mngrd_t;
typedef struct umdb_uauth_d umdb_uauth_d_t;

// sqlite query type
enum query_type
{
    USER_AUTH = 0,
    USER_ADD,
    USER_DEL,
    USER_CMD_DEL,
    USER_CMD_AUTH,
    USER_CMD_SPECIFIC_AUTH,
    USER_GET
};

// db descriptor
struct umdb_mngr_d {
    sqlite3 *db;
};

// user auth descriptor
struct umdb_uauth_d {
    int auth;
    int id;
    int flags;
    const char *usr;
};

umdb_mngrd_t *umdb_mngr_new(const char *db);
void umdb_mngr_free(umdb_mngrd_t *m);
int umdb_mngr_uauth(umdb_mngrd_t *m, umdb_uauth_d_t *res, const char *u, const char *p);
int umdb_mngr_uget(umdb_mngrd_t *m, umdb_uauth_d_t *res, const char *u);

#endif /* ifndef UMDB */

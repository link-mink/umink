pkglib_LTLIBRARIES += plg_sysagent_lua.la
plg_sysagent_lua_la_SOURCES = %reldir%/plg_sysagent_lua.c \
                              %reldir%/mink_lua.c
plg_sysagent_lua_la_CFLAGS = ${COMMON_INCLUDES} \
                             ${JSON_C_CFLAGS} \
                             -DLUA_COMPAT_ALL \
                             -DLUA_COMPAT_5_1 \
                             -DLUA_COMPAT_5_2 \
                             -DLUA_COMPAT_5_3 \
                             ${LUA_CFLAGS} \
                             -Isrc/services/sysagent
plg_sysagent_lua_la_LDFLAGS = -version-info 1:0:0 \
                              -shared \
                              -module \
                              -export-dynamic
plg_sysagent_lua_la_LIBADD = ${LUA_LIBS} \
                             ${JSON_C_LIBS}

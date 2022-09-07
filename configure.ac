#               _____  ____ __
#   __ ____ _  /  _/ |/ / //_/
#  / // /  ' \_/ //    / ,<
#  \_,_/_/_/_/___/_/|_/_/|_|
#
# SPDX-License-Identifier: MIT
#

AC_PREREQ([2.68])
AC_INIT([umink], [1.0], [info@link-mink.com], , [https://github.com/link-mink/umink])
config_flags="$*"
AC_DEFINE_UNQUOTED([CONFIG_FLAGS],["$config_flags"],[Flags passed to configure])
AC_SUBST(CONFIG_FLAGS, "$config_flags")
AM_INIT_AUTOMAKE([foreign subdir-objects nostdinc 1.14])
AC_CONFIG_SRCDIR([src])
AC_CONFIG_HEADERS([config.h])
AX_PREFIX_CONFIG_H(umink_pkg_config.h)
AC_CONFIG_MACRO_DIR([m4])
AC_LANG([C])
AC_USE_SYSTEM_EXTENSIONS
AC_PROG_CC
AC_PROG_CC_C99
AC_PROG_AWK
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_RANLIB
AC_PROG_SED
AC_CHECK_PROG(GPERF, gperf, gperf)
AS_IF([test -z "$GPERF"], [AC_MSG_ERROR([gperf not found!])])
PKG_PROG_PKG_CONFIG
LT_INIT

# save libs
COMMON_LIBS=$LIBS

# /*********************/
# /* enable DEBUG mode */
# /*********************/
AC_ARG_ENABLE(debug,
              [AS_HELP_STRING([--enable-debug], [enable DEBUG mode [default=no]])],,
              [enable_debug=no])
AM_CONDITIONAL(ENABLE_DEBUG, test "x$enable_debug" = "xyes")

# /***********************************/
# /* disable RT threads (SCHED_FIFO) */
# /***********************************/
AC_ARG_ENABLE(sched-fifo,
              [AS_HELP_STRING([--enable-sched-fifo], [Enable SCHED_FIFO scheduling [default=no]])],,
              [enable_sched_fifo=no])
AM_CONDITIONAL(ENABLE_SCHED_FIFO, test "x$enable_sched_fifo" = "xyes")
if test "x$enable_sched_fifo" != "xno"; then
    AC_DEFINE([ENABLE_SCHED_FIFO], [1], [Enable SCHED_FIFO scheduling])
fi

# /**********/
# /* LuaJIT */
# /**********/
AC_ARG_ENABLE(lua,
              [AS_HELP_STRING([--enable-lua], [Enable LuaJIT [default=no]])],,
              [enable_lua=no])
AM_CONDITIONAL(ENABLE_LUA, test "x$enable_lua" = "xyes")
if test "x$enable_lua" != "xno"; then
    PKG_CHECK_MODULES([LUA], [luajit], [], [AC_MSG_ERROR([LuaJIT not found!])])
    AC_DEFINE([ENABLE_LUA], [1], [Enable LuaJIT])
fi

# /********/
# /* MQTT */
# /********/
AC_ARG_ENABLE(mqtt,
              [AS_HELP_STRING([--enable-mqtt], [Enable MQTT [default=no]])],,
              [enable_mqtt=no])
AM_CONDITIONAL(ENABLE_MQTT, test "x$enable_mqtt" = "xyes")
if test "x$enable_mqtt" != "xno"; then
    AC_DEFINE([ENABLE_MQTT], [1], [Enable MQTT])
    AC_CHECK_LIB([paho-mqtt3as],
                 [MQTTAsync_create],
                 [AC_SUBST([MQTT_LIBS], ["-lpaho-mqtt3as"])],
                 [AC_MSG_ERROR([paho-mqtt3a library not found!])])

fi

# /*************************/
# /* Mosquitto Auth plugin */
# /*************************/
AC_ARG_ENABLE(mosquitto-auth,
              [AS_HELP_STRING([--enable-mosquitto-auth], [Enable Mosquitto Auth [default=no]])],,
              [enable_mosquitto_auth=no])
AM_CONDITIONAL(ENABLE_MOSQUITTO_AUTH, test "x$enable_mosquitto_auth" = "xyes")
if test "x$enable_mosquitto_auth" != "xno"; then
    AC_DEFINE([ENABLE_MOSQUITTO_AUTH], [1], [Enable Mosquitto Auth])
    AC_CHECK_HEADERS([mosquitto.h mosquitto_plugin.h], ,AC_MSG_ERROR([mosquitto headers not found!]))
fi

# /******/
# /* rt */
# /******/
AC_CHECK_LIB([rt], [clock_gettime], , AC_MSG_ERROR([clock_gettime not found!]))

# /******/
# /* lm */
# /******/
AC_CHECK_LIB([m], [pow], , AC_MSG_ERROR([libm not found!]))


# /***********/
# /* pthread */
# /***********/
AC_CHECK_LIB([pthread],
             [pthread_create], ,
             AC_MSG_ERROR([pthread library not found!]))

# /****************************/
# /* Checks for header files. */
# /****************************/
AC_CHECK_HEADERS([      arpa/inet.h \
                        fcntl.h \
                        inttypes.h \
                        locale.h \
                        netdb.h \
                        netinet/in.h \
                        stddef.h \
                        stdint.h \
                        stdlib.h \
                        string.h \
                        strings.h \
                        sys/ioctl.h \
                        sys/socket.h \
                        sys/time.h \
                        sys/timeb.h \
                        syslog.h \
                        pthread.h \
                        uthash.h \
                        unistd.h], , AC_MSG_ERROR([Header file missing!]))

# /******************************************************************/
# /* Checks for typedefs, structures, and compiler characteristics. */
# /******************************************************************/
AC_C_INLINE
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_INT8_T
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# /*********************************/
# /* Checks for library functions. */
# /*********************************/
AC_FUNC_ERROR_AT_LINE
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_MKTIME
AC_FUNC_STRNLEN
AC_FUNC_STRTOD
AC_CHECK_FUNCS([alarm \
                atexit \
                bzero \
                gethostname \
                gettimeofday \
                inet_ntoa \
                isascii \
                localtime_r \
                memmove \
                memset \
                mkdir \
                mkfifo \
                pow \
                select \
                poll \
                setenv \
                socket \
                strchr \
                strdup \
                strerror \
                strncasecmp \
                strstr \
                strtoul \
                strtoull \
                sysinfo], , AC_MSG_ERROR([System function missing!]))

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

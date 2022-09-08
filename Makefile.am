#               _____  ____ __
#   __ ____ _  /  _/ |/ / //_/
#  / // /  ' \_/ //    / ,<
#  \_,_/_/_/_/___/_/|_/_/|_|
#
# SPDX-License-Identifier: MIT
#

# aclocal m4 dir
ACLOCAL_AMFLAGS = -I m4

# check for debug mode [--enable-debug]
if ENABLE_DEBUG
CFLAGS += -O${OLEVEL} -g
else
CFLAGS += -O${OLEVEL}
endif

# compiler and linkerflags
COMMON_INCLUDES = -I. -Isrc/include

# umINK directories
confdir = $(sysconfdir)/umink

# convenience libraries
noinst_LTLIBRARIES = libumd.la \
                     libumplg.la

# umink daemon
libumd_la_SOURCES = src/umd/umdaemon.c
libumd_la_CFLAGS = ${COMMON_INCLUDES}

# umink plugins
libumplg_la_SOURCES = src/utils/umink_plugin.c
libumplg_la_CFLAGS = ${COMMON_INCLUDES}

# programs
bin_PROGRAMS = sysagentd

# sysagent
sysagentd_SOURCES = src/services/sysagent/sysagentd.c
sysagentd_CFLAGS = ${COMMON_INCLUDES} \
                   -Isrc/services/sysagent
sysagentd_LDFLAGS = -export-dynamic
sysagentd_LDADD = libumd.la


# cleanup rule
distclean-local: distclean-ax-prefix-umink-pkg-config-h
distclean-ax-prefix-umink-pkg-config-h:
	rm -f umink_pkg_config.h



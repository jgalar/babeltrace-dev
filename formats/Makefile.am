AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

SUBDIRS = . ctf ctf-text ctf-metadata bt-dummy

if !BABELTRACE_BUILD_WITH_MINGW
SUBDIRS += lttng-live
endif

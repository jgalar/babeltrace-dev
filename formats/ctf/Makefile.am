AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

SUBDIRS = types writer ir metadata .

lib_LTLIBRARIES = libbabeltrace-ctf.la

libbabeltrace_ctf_la_SOURCES = \
	ctf.c \
	events.c \
	iterator.c \
	callbacks.c \
	events-private.h

# Request that the linker keeps all static libraries objects.
libbabeltrace_ctf_la_LDFLAGS = \
	-Wl,--no-as-needed -version-info $(BABELTRACE_LIBRARY_VERSION)

libbabeltrace_ctf_la_LIBADD = \
	$(top_builddir)/lib/libbabeltrace.la \
	types/libctf-types.la \
	metadata/libctf-parser.la \
	metadata/libctf-ast.la \
	writer/libctf-writer.la \
	ir/libctf-ir.la

noinst_PROGRAMS = ctf-parser-test
ctf_parser_test_SOURCES = ctf-parser-test.c
ctf_parser_test_LDADD =	metadata/libctf-parser.la \
	metadata/libctf-ast.la \
	ir/libctf-ir.la \
	types/libctf-types.la \
	libbabeltrace-ctf.la

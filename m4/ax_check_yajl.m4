# ax_check_yajl.m4 -- check for YAJL 1.x and 2.x libraries
#
# Copyright (C) 2016 - Philippe Proulx <pproulx@efficios.com>
#
# This file is free software; the Free Software Foundation gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.

# This macro checks for YAJL 1.x and 2.x libraries. It sets the
# following shell variables:
#
#   * YAJL_1_EXISTS: "yes" or "no"
#   * YAJL_2_EXISTS: "yes" or "no"

AC_DEFUN([AX_CHECK_YAJL], [
    # Check common header
    AC_CHECK_HEADER([yajl/yajl_common.h],
                    [yajl_common_header_exists=yes],
                    [yajl_common_header_exists=no])

    if test x$yajl_common_header_exists = xno; then
        YAJL_1_EXISTS=no
        YAJL_2_EXISTS=no
    else
        # Check for YAJL 1.x
        AC_CHECK_LIB([yajl], [yajl_parse_complete],
                     [YAJL_1_EXISTS=yes], [YAJL_1_EXISTS=no])

        # Check for YAJL 2.x
        AC_CHECK_LIB([yajl], [yajl_complete_parse],
                     [YAJL_2_EXISTS=yes], [YAJL_2_EXISTS=no])
    fi
])

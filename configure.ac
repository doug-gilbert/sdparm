AC_INIT(sdparm, 0.95, dgilbert@interlog.com)

AM_INIT_AUTOMAKE
AM_CONFIG_HEADER(config.h)

AC_PROG_CC
AC_PROG_INSTALL

# check for headers
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h errno.h getopt.h limits.h ctype.h])
AC_CHECK_HEADERS([sys/ioctl.h sys/utsname.h scsi/scsi.h scsi/sg.h])

# AC_PROG_LIBTOOL
AC_OUTPUT(Makefile src/Makefile)

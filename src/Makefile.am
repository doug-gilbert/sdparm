bin_PROGRAMS = sdparm

# INCLUDES = -I$(top_srcdir)/include -I$(top_srcdir)/include/scsi

AM_CFLAGS = -Wall -W
# AM_CFLAGS = -Wall -W -pedantic -std=c99

sdparm_SOURCES =	sdparm.c	\
			sdparm.h	\
			sdparm_data.c	\
			sdparm_access.c	\
			sdparm_vpd.c	\
			sdparm_cmd.c	\
			sg_lib.c	\
			sg_lib.h	\
			sg_cmds_basic.c	\
			sg_cmds_basic.h	\
			sg_pt.h

sdparm_LDADD = @os_deps@ @GETOPT_O_FILES@ @os_libs@
sdparm_DEPENDENCIES = @os_deps@ @GETOPT_O_FILES@
EXTRA_sdparm_SOURCES =	sg_pt_linux.c	\
			sg_linux_inc.h	\
			sg_pt_freebsd.c	\
			sg_pt_osf1.c	\
			sg_pt_win32.c	\
			getopt_long.c	\
			port_getopt.h

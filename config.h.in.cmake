
#cmakedefine SG_LIB_LINUX 1
#cmakedefine SG_LIB_FREEBSD 1
#cmakedefine SG_LIB_SOLARIS 1
#cmakedefine SG_LIB_CYGWIN 1
#cmakedefine SG_LIB_MINGW 1
#cmakedefine SG_LIB_AIX 1
#cmakedefine SG_LIB_WINDOWS 1
#cmakedefine SG_LIB_ABROID 1
#cmakedefine SG_LIB_SOLARIS 1
#cmakedefine SG_LIB_AIX 1
#cmakedefine SG_LIB_MACOS 1

#cmakedefine HAVE_POSIX_MEMALIGN 1
#cmakedefine IGNORE_FAST_LEBE 1
#cmakedefine IGNORE_LINUX_BSG 1
#cmakedefine IGNORE_NVME 1
#cmakedefine SG_SCSI_STRINGS 1
#cmakedefine HAVE_BYTESWAP_H 1
#cmakedefine HAVE_LINUX_TYPES_H 1
#cmakedefine HAVE_LINUX_BSG_H 1
#cmakedefine HAVE_LINUX_KDEV_T_H 1

#cmakedefine XXXXXXX 1

#define BUILD_TIME "@BUILD_TIME@"

// #define SG_LIB_LINUX @OS_LINUX@
// #define SG_LIB_FREEBSD @OS_FREEBSD@
// #cmakedefine01 SG_LIB_AIX @SG_LIB_AIX@

// # This will generate a line in the output_file. Then in CMLists.txt:
// #      set(FEATURE_COMMENT "//")
// @FEATURE_COMMENT@#define OPTIONAL_SETTING 1


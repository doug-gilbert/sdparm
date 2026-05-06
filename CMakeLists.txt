cmake_minimum_required ( VERSION 3.10 )

project(sdparm VERSION 1.12.9)
# Use version number: x.y.9 to indicate pre-release of x.(y+1)
# Example: VERSION 1.12.9 is pre-release of 1.13

set ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -g" )

option ( BUILD_SHARED_LIBS "Build using shared libraries" ON)
option ( ENABLE_DEBUG "Turn on debugging (extra warnings)" OFF)
option ( DISABLE_NVME_SUPP "Remove all or most NVMe code" OFF)
option ( DISABLE_FAST_LEBE "Use generic little-endian/big-endian code instead" OFF)

include ( CheckIncludeFile )
CHECK_INCLUDE_FILE( "linux/nvme_ioctl.h" NVME_PRESENT )
CHECK_INCLUDE_FILE( "byteswap.h" HAVE_BYTESWAP_H )

if ( NVME_PRESENT )
  add_definitions ( -DHAVE_NVME )
  add_definitions ( -DHAVE_LINUX_NVME_IOCTL_H )
  MESSAGE( ">> Have NVME" )
endif ( NVME_PRESENT )

if ( HAVE_BYTESWAP_H )
  add_definitions ( -DHAVE_BYTESWAP_H )
endif ( HAVE_BYTESWAP_H )

if ( DISABLE_NVME_SUPP )
  add_definitions ( -DIGNORE_NVME )
endif ( DISABLE_NVME_SUPP )

if ( DISABLE_FAST_LEBE )
  add_definitions ( -DIGNORE_FAST_LEBE )
endif ( DISABLE_FAST_LEBE )


set ( sourcefiles_sdparm
        src/sdparm.c
        src/sdparm_access.c
        src/sdparm_cmd.c
        src/sdparm_data.c
        src/sdparm_data_vendor.c
        src/sdparm_vpd.c
)

set ( src_nolib_sdparm
        lib/sg_lib.c
        lib/sg_lib_data.c
        lib/sg_cmds_basic.c
        lib/sg_cmds_basic2.c
        lib/sg_cmds_mmc.c
        lib/sg_pr2serr.c
        lib/sg_pt_common.c
        lib/sg_pt_linux.c
        lib/sg_pt_linux_nvme.c
        lib/sg_cmds_extra.c
        lib/sg_snt.c
        lib/sg_json_builder.c
        lib/sg_json.c
        lib/sg_json_sg_lib.c
)

if ( BUILD_SHARED_LIBS )
    MESSAGE( ">> Build using shared libraries (default)" )
    find_library(LIBSGUTILS NAMES sgutils2 PATH_SUFFIXES lib)
    if ( LIBSGUTILS )
        MESSAGE( ">> Found shared library: ${LIBSGUTILS}" )
        add_executable ( sdparm ${sourcefiles_sdparm} )
        # target_include_directories(sdparm PUBLIC include)
        target_link_libraries(sdparm PRIVATE ${LIBSGUTILS})
    else ( LIBSGUTILS )
        MESSAGE( ">> Did not find shared library: ${LIBSGUTILS}" )
        add_definitions ( -DSG_LIB_LINUX )
        add_executable ( sdparm ${sourcefiles_sdparm} ${src_nolib_sdparm} )
        # target_include_directories(sdparm PUBLIC include)
        target_link_libraries(sdparm)
    endif ( LIBSGUTILS )
else ( BUILD_SHARED_LIBS )
    MESSAGE( ">> Trying to build a static executable" )
    add_definitions ( -DSG_LIB_LINUX )
    add_executable ( sdparm ${sourcefiles_sdparm} ${src_nolib_sdparm} )
    add_definitions ( -static )
    # target_include_directories(sdparm PUBLIC include)
    target_link_libraries(sdparm -static)
endif ( BUILD_SHARED_LIBS )

target_compile_definitions(sdparm PRIVATE _LARGEFILE64_SOURCE _FILE_OFFSET_BITS=64)
target_compile_options(sdparm PRIVATE -Wall -W)
target_include_directories(sdparm PRIVATE include ${CMAKE_SOURCE_DIR}/src)

if ( ENABLE_DEBUG )
  target_compile_definitions(sdparm PRIVATE DEBUG)
  target_compile_options(sdparm PRIVATE
    -Wextra -Wmisleading-indentation -Wduplicated-cond -Wlogical-op
    -Wnull-dereference -Wshadow -Wunused -Wsizeof-array-argument
    -Wduplicated-branches -Wjump-misses-init -Wparentheses)
endif ( ENABLE_DEBUG )

install(TARGETS sdparm RUNTIME DESTINATION bin)

include(GNUInstallDirs)
file(ARCHIVE_CREATE OUTPUT sdparm.8.gz PATHS doc/sdparm.8 FORMAT raw COMPRESSION GZip)
file(ARCHIVE_CREATE OUTPUT sdparm_json.8.gz PATHS doc/sdparm_json.8 FORMAT raw COMPRESSION GZip)
install(FILES sdparm.8.gz DESTINATION "${CMAKE_INSTALL_MANDIR}/man8")
install(FILES sdparm_json.8.gz DESTINATION "${CMAKE_INSTALL_MANDIR}/man8")
install(FILES ChangeLog DESTINATION "${CMAKE_INSTALL_DOCDIR}")
install(FILES README DESTINATION "${CMAKE_INSTALL_DOCDIR}")
install(FILES AUTHORS DESTINATION "${CMAKE_INSTALL_DOCDIR}")
install(FILES CREDITS DESTINATION "${CMAKE_INSTALL_DOCDIR}")

if ( ${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD" )
    MESSAGE( ">> Detected FreeBSD" )
    target_include_directories( sdparm PRIVATE /usr/local/include )
endif ( )


set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION "${PROJECT_VERSION}")
SET(CPACK_GENERATOR "DEB;RPM")
SET(CPACK_DEBIAN_PACKAGE_MAINTAINER "Douglas Gilbert") #required
SET(CPACK_DEBIAN_PACKAGE_SECTION "admin")
SET(CPACK_RPM_PACKAGE_DESCRIPTION "Permits reading and setting of SCSI device mode pages (cf hdparm for ATA disks)")
SET(CPACK_RPM_PACKAGE_LICENSE "BSD-2-clause")
SET(CPACK_RPM_PACKAGE_VENDOR "Douglas Gilbert")

include(CPack)
include(CTest)

cmake_minimum_required ( VERSION 3.10 )

project(sdparm VERSION 1.12.9)
# Use version number: x.y.9 to indicate pre-release of x.(y+1) 
# Example: VERSION 1.12.9 is pre-release of 1.13

option ( BUILD_SHARED_LIBS "Build using shared libraries" ON)

set (CMAKE_CXX_STANDARD 20 )
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_EXTENSIONS OFF)

include ( CheckIncludeFile )
include ( CheckIncludeFileCXX )

set ( sourcefiles_sdparm
	src/sdparm.c
	src/sdparm_access.c
	src/sdparm_cmd.c
	src/sdparm_data.c
	src/sdparm_data_vendor.c
	src/sdparm_vpd.c
)

set ( headerfiles
	src/sdparm.h
	lib/sg_json_builder.h
	include/sg_lib.h
	include/sg_json.h
	include/sg_json_sg_lib.h
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
        add_executable ( sdparm ${sourcefiles_sdparm} ${headerfiles} )
        target_include_directories(sdparm PUBLIC include)
	target_link_libraries(sdparm PRIVATE ${LIBSGUTILS})
    else ( LIBSGUTILS )
        MESSAGE( ">> Did not find shared library: ${LIBSGUTILS}" )
        add_executable ( sdparm ${sourcefiles_sdparm} ${src_nolib_sdparm} ${headerfiles} )
        target_include_directories(sdparm PUBLIC include)
        target_link_libraries(sdparm)
    endif ( LIBSGUTILS )
else ( BUILD_SHARED_LIBS )
    MESSAGE( ">> Trying to build a static executable" )
    add_executable ( sdparm ${sourcefiles_sdparm} ${src_nolib_sdparm} ${headerfiles} )
    add_definitions ( -static )
    target_include_directories(sdparm PUBLIC include)
    target_link_libraries(sdparm -static)
endif ( BUILD_SHARED_LIBS )

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

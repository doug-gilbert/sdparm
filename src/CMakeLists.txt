
set ( sourcefiles_sdparm
        sdparm.c
        sdparm_access.c
        sdparm_cmd.c
        sdparm_data.c
        sdparm_data_vendor.c
        sdparm_vpd.c
)

set ( src_nolib_sdparm
        ${CMAKE_SOURCE_DIR}/lib/sg_lib.c
        ${CMAKE_SOURCE_DIR}/lib/sg_lib_data.c
        ${CMAKE_SOURCE_DIR}/lib/sg_cmds_basic.c
        ${CMAKE_SOURCE_DIR}/lib/sg_cmds_basic2.c
        ${CMAKE_SOURCE_DIR}/lib/sg_cmds_mmc.c
        ${CMAKE_SOURCE_DIR}/lib/sg_pr2serr.c
        ${CMAKE_SOURCE_DIR}/lib/sg_pt_common.c
        ${CMAKE_SOURCE_DIR}/lib/sg_cmds_extra.c
        ${CMAKE_SOURCE_DIR}/lib/sg_snt.c
        ${CMAKE_SOURCE_DIR}/lib/sg_json_builder.c
        ${CMAKE_SOURCE_DIR}/lib/sg_json.c
        ${CMAKE_SOURCE_DIR}/lib/sg_json_sg_lib.c
)

if ( SG_LIB_LINUX )
    set ( src_plat_sdparm
        ${CMAKE_SOURCE_DIR}/lib/sg_pt_linux.c
        ${CMAKE_SOURCE_DIR}/lib/sg_pt_linux_nvme.c
    )
elseif ( SG_LIB_FREEBSD )
    set ( src_plat_sdparm
        ${CMAKE_SOURCE_DIR}/lib/sg_pt_freebsd.c
    )
else ()
    set ( src_plat_sdparm
        ${CMAKE_SOURCE_DIR}/lib/sg_pt_dummy.c
    )
endif ()

set ( SET_INC_DIR 0 )
if ( BUILD_SHARED_LIBS )
    set ( ADD_EXE_CALLED 0 )
    if ( DISABLE_LIBSGUTILS )
        MESSAGE( STATUS ">> Ignoring system sgutil2 library" )
    else ( DISABLE_LIBSGUTILS )
        MESSAGE( STATUS ">> Searching for sgutils2 shared library" )
        find_library(LIBSGUTILS NAMES sgutils2 PATH_SUFFIXES lib)
        if ( LIBSGUTILS )
            MESSAGE( STATUS ">> Found shared library: ${LIBSGUTILS}" )
            if ( PkgConfig_FOUND )
                pkg_check_modules(SGUTILS sgutils2)
            endif ()
            if ( NOT SGUTILS_FOUND )
                find_library(SGUTILS_LIB NAMES sgutils2 PATH_SUFFIXES lib)
                find_path(SGUTILS_INC sg_lib.h PATH_SUFFIXES scsi)
                if ( SGUTILS_LIB AND SGUTILS_INC )
                    set(SGUTILS_FOUND TRUE)
                    set(SGUTILS_LIBRARIES ${SGUTILS_LIB})
                    set(SGUTILS_INCLUDE_DIRS ${SGUTILS_INC})
                endif ()
            endif ()
            if ( SGUTILS_FOUND )
                MESSAGE( STATUS ">> Found system sgutils2 library" )
                add_executable ( sdparm ${sourcefiles_sdparm} )
                target_link_libraries(sdparm PRIVATE ${SGUTILS_LIBRARIES})
                target_include_directories(sdparm PRIVATE ${SGUTILS_INCLUDE_DIRS})
                set ( SET_INC_DIR 1 )
                if ( SG_LIB_FREEBSD )
                    target_include_directories( sdparm PRIVATE /usr/local/include )
                    target_link_libraries( sdparm PRIVATE cam )
                endif ( SG_LIB_FREEBSD )
                set ( ADD_EXE_CALLED 1 )
            else ()
                if ( NOT EXISTS ${CMAKE_SOURCE_DIR}/lib/sg_lib.c )
                    MESSAGE( FATAL_ERROR "System sgutils2 not found and bundled sg3_utils sources are not present.\n"
                        "Install the development package (e.g. libsgutils2-dev on Debian/Ubuntu,\n"
                        "sg3_utils-devel on Fedora/RHEL) or populate the lib/ and include/ directories." )
                endif ()
                MESSAGE( STATUS ">> System sgutils2 not found, using bundled sg3_utils sources" )
            endif ()
        else ( LIBSGUTILS )
            MESSAGE( STATUS ">> Did NOT find shared sgutils2 library" )
            if ( NOT EXISTS ${CMAKE_SOURCE_DIR}/lib/sg_lib.c )
                MESSAGE( FATAL_ERROR "Bundled sg3_utils sources are not present in lib/.\n"
                    "Static builds require the lib/ and include/ directories to be populated." )
            endif ()
        endif ( LIBSGUTILS )
    endif ( DISABLE_LIBSGUTILS )
    if ( NOT ADD_EXE_CALLED )
        add_executable ( sdparm ${sourcefiles_sdparm} ${src_nolib_sdparm} ${src_plat_sdparm} )
        target_link_libraries(sdparm)
        if ( SG_LIB_FREEBSD )
            target_include_directories( sdparm PRIVATE /usr/local/include )
            target_link_libraries( sdparm PRIVATE cam )
        endif ( SG_LIB_FREEBSD )
    endif ( NOT ADD_EXE_CALLED )
else ( BUILD_SHARED_LIBS )
    MESSAGE( STATUS ">> Trying to build a static executable" )
    # add_definitions ( -DSG_LIB_LINUX )
    add_executable ( sdparm ${sourcefiles_sdparm} ${src_nolib_sdparm} ${src_plat_sdparm} )
    add_definitions ( -static )
    if ( SG_LIB_FREEBSD )
        target_include_directories( sdparm PRIVATE /usr/local/include )
        target_link_libraries( sdparm PRIVATE cam )
    endif ( SG_LIB_FREEBSD )
    target_link_libraries(sdparm -static)
endif ( BUILD_SHARED_LIBS )

if ( NOT SET_INC_DIR )
    target_include_directories(sdparm PRIVATE ${CMAKE_SOURCE_DIR}/include)
endif ( NOT SET_INC_DIR )
target_include_directories(sdparm PRIVATE ${CMAKE_BINARY_DIR})

if ( ENABLE_DEBUG )
  target_compile_definitions(sdparm PRIVATE DEBUG)
  target_compile_options(sdparm PRIVATE
    -Wextra -Wmisleading-indentation -Wduplicated-cond -Wlogical-op
    -Wnull-dereference -Wshadow -Wunused -Wsizeof-array-argument
    -Wduplicated-branches -Wjump-misses-init -Wparentheses)
endif ( ENABLE_DEBUG )

install(TARGETS sdparm RUNTIME DESTINATION bin)


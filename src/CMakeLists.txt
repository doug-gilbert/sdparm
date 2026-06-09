
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
        ${CMAKE_SOURCE_DIR}/lib/sg_pt_linux.c
        ${CMAKE_SOURCE_DIR}/lib/sg_pt_linux_nvme.c
        ${CMAKE_SOURCE_DIR}/lib/sg_cmds_extra.c
        ${CMAKE_SOURCE_DIR}/lib/sg_snt.c
        ${CMAKE_SOURCE_DIR}/lib/sg_json_builder.c
        ${CMAKE_SOURCE_DIR}/lib/sg_json.c
        ${CMAKE_SOURCE_DIR}/lib/sg_json_sg_lib.c
)

set ( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -g" )

if ( BUILD_SHARED_LIBS )
    set ( GOT_EXE 0 )
    if ( DISABLE_LIBSGUTILS )
        MESSAGE( STATUS ">> Ignoring system sgutil2 library" )
    else ( DISABLE_LIBSGUTILS )
        MESSAGE( STATUS ">> Searching for sgutils2 shared library" )
        find_library(LIBSGUTILS NAMES sgutils2 PATH_SUFFIXES lib)
        if ( LIBSGUTILS )
            MESSAGE( STATUS ">> Found shared library: ${LIBSGUTILS}" )
            add_executable ( sdparm ${sourcefiles_sdparm} )
            target_link_libraries(sdparm PRIVATE ${LIBSGUTILS})
            if ( SG_LIB_FREEBSD )
                target_include_directories( sdparm PRIVATE /usr/local/include )
                target_link_libraries( sdparm PRIVATE cam )
            endif ( SG_LIB_FREEBSD )
            set ( GOT_EXE 1 )
        else ( LIBSGUTILS )
            MESSAGE( STATUS ">> Did NOT find shared sgutils2 library" )
        endif ( LIBSGUTILS )
    endif ( DISABLE_LIBSGUTILS )
    if ( NOT GOT_EXE )
        add_executable ( sdparm ${sourcefiles_sdparm} ${src_nolib_sdparm} )
        target_link_libraries(sdparm)
        if ( SG_LIB_FREEBSD )
            target_include_directories( sdparm PRIVATE /usr/local/include )
            target_link_libraries( sdparm PRIVATE cam )
        endif ( SG_LIB_FREEBSD )
    endif ( NOT GOT_EXE )
else ( BUILD_SHARED_LIBS )
    MESSAGE( STATUS ">> Trying to build a static executable" )
    # add_definitions ( -DSG_LIB_LINUX )
    add_executable ( sdparm ${sourcefiles_sdparm} ${src_nolib_sdparm} )
    add_definitions ( -static )
    if ( SG_LIB_FREEBSD )
        target_include_directories( sdparm PRIVATE /usr/local/include )
        target_link_libraries( sdparm PRIVATE cam )
    endif ( SG_LIB_FREEBSD )
    target_link_libraries(sdparm -static)
endif ( BUILD_SHARED_LIBS )

target_compile_definitions(sdparm PRIVATE _LARGEFILE64_SOURCE _FILE_OFFSET_BITS=64)
target_compile_options(sdparm PRIVATE -Wall -W)
target_include_directories(sdparm PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_include_directories(sdparm PRIVATE ${CMAKE_BINARY_DIR})

if ( ENABLE_DEBUG )
  target_compile_definitions(sdparm PRIVATE DEBUG)
  target_compile_options(sdparm PRIVATE
    -Wextra -Wmisleading-indentation -Wduplicated-cond -Wlogical-op
    -Wnull-dereference -Wshadow -Wunused -Wsizeof-array-argument
    -Wduplicated-branches -Wjump-misses-init -Wparentheses)
endif ( ENABLE_DEBUG )

install(TARGETS sdparm RUNTIME DESTINATION bin)


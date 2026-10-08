
MESSAGE( STATUS ">> Entering lib/CMakeLists.txt" )

# Select Hardware Abstraction Layer (HAL)
if ( PT_DUMMY )
    set ( sgutils2_lib_hal
	sg_pt_dummy.c
    )
    MESSAGE( STATUS "<<>> PT_DUMMY configured, no HAL")
elseif ( SG_LIB_LINUX )
    set ( sgutils2_lib_hal
	sg_io_linux.c
	sg_pt_linux.c
	sg_pt_linux_nvme.c
    )
elseif ( SG_LIB_FREEBSD )
    set ( sgutils2_lib_hal
	sg_pt_freebsd.c
    )
elseif ( SG_LIB_WIN32 )
    set ( sgutils2_lib_hal
	sg_pt_win32.c
    )
elseif ( SG_LIB_HAIKU )
    set ( sgutils2_lib_hal
	sg_pt_haiku.c
    )
elseif ( SG_LIB_NETBSD )
    set ( sgutils2_lib_hal
	sg_pt_netbsd.c
    )
elseif ( SG_LIB_SOLARIS )
    set ( sgutils2_lib_hal
	sg_pt_solaris.c
    )
else ( )
    MESSAGE( STATUS "<<>> PT_DUMMY set due to fall through, no HAL")
    set ( sgutils2_lib_hal
	sg_pt_dummy.c
    )
endif ( PT_DUMMY )

set ( sgutils2_lib_common
	sg_cmds_basic.c
	sg_cmds_basic2.c
	sg_cmds_extra.c
	sg_cmds_mmc.c
	sg_json.c
	sg_json_builder.c
	sg_json_sg_lib.c
	sg_lib.c
	sg_lib_data.c
	sg_lib_names.c
	sg_pr2serr.c
	sg_pt_common.c
	sg_snt.c
)

if ( BUILD_SHARED_LIBS )
    if ( DISABLE_LIBSGUTILS )
        MESSAGE( STATUS ">> Build sgutils2 library static, otherwise use shared library(s)" )
        add_library(sgutils2 STATIC ${sgutils2_lib_common} ${sgutils2_lib_hal} )
    else ( DISABLE_LIBSGUTILS )
        MESSAGE( STATUS ">> Build shared library(s) (default)" )
        add_library(sgutils2 SHARED ${sgutils2_lib_common} ${sgutils2_lib_hal} )
        set_target_properties(sgutils2 PROPERTIES
            WINDOWS_EXPORT_ALL_SYMBOLS ON
        )
    endif ( DISABLE_LIBSGUTILS )
else ( BUILD_SHARED_LIBS )
    MESSAGE( STATUS ">> Build a static library" )
    add_definitions ( -static )
    add_library(sgutils2 STATIC ${sgutils2_lib_common} ${sgutils2_lib_hal} )
endif ( BUILD_SHARED_LIBS )

# l_language_choice() is a function defined in the parent CMakeLists.txt
# and takes a list of files to be compiled. It optionally calls
#    set_source_files_properties(${it} PROPERTIES LANGUAGE CXX)
# if the intent is to compile this C code with a C++ compiler. That is
# primarily for more advanced code sanity checking in C++. The usual case
# is to compile with a C compiler in which case this function does nothing.
# First we need to build a list of files to be compiled:
## set ( compile_list ${sgutils2_lib_common} )
## list ( APPEND compile_list ${sgutils2_lib_hal} )
## 
## l_language_choice( "${compile_list}" )

l_set_compile_options ( sgutils2 )

target_include_directories(sgutils2
    PUBLIC
	$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_include_directories(sgutils2 PRIVATE ${CMAKE_BINARY_DIR})

# cmake in win32 doesn't like the following
# target_compile_features(sgutils2 PUBLIC c_std_99)

set_target_properties(sgutils2 PROPERTIES
	OUTPUT_NAME "sgutils2-${PROJECT_VERSION}"
	VERSION 2.0.0
	SOVERSION 2
)

#
# Install runtime components
#

install(TARGETS sgutils2
    EXPORT sgutils2Targets

    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT runtime
        NAMELINK_SKIP
)

# Match the autotools -release convention: the development symlink is the
# plain libsgutils2.so name (without the release version embedded).
# Create the directory first so this works when CPack stages components
# into separate trees.
# Use escaped CMAKE_INSTALL_PREFIX so CPack can override it at install
# time (CMAKE_INSTALL_FULL_LIBDIR bakes in the configure-time prefix).
install(CODE "file(MAKE_DIRECTORY \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}\")
file(CREATE_LINK
    \"libsgutils2-${PROJECT_VERSION}.so.2\"
    \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/libsgutils2.so\"
    SYMBOLIC)"
    COMPONENT development
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/sgutils2ConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)


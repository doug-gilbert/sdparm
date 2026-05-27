
MESSAGE( STATUS ">> Entering doc/CMakeLists.txt" )

set(SDPARM_MP
        sdparm
        sdparm_json
)

foreach(util ${SDPARM_MP})

    file(ARCHIVE_CREATE
         OUTPUT doc/${util}.8.gz
         PATHS doc/${util}.8
         FORMAT raw
         COMPRESSION GZip
    )
    install(FILES ${util}.8.gz
            DESTINATION "${CMAKE_INSTALL_MANDIR}/man8"
    )

endforeach()


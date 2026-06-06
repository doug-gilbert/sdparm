
MESSAGE( STATUS ">> Entering doc/CMakeLists.txt" )

set(SDPARM_MP
        sdparm
        sdparm_json
        sas_disk_blink
        scsi_ch_swp
)

foreach(util ${SDPARM_MP})

    file(ARCHIVE_CREATE
         OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${util}.8.gz
         PATHS ${CMAKE_CURRENT_SOURCE_DIR}/${util}.8
         FORMAT raw
         COMPRESSION GZip
    )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${util}.8.gz
            DESTINATION "${CMAKE_INSTALL_MANDIR}/man8"
    )

endforeach()


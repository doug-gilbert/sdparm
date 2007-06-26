/*
 * Copyright (c) 2005 Douglas Gilbert.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <stdlib.h>
#include "sdparm.h"


/* sdparm is a utility program for the Linux OS SCSI subsystem.
 *
 * This utility fetches various parameters associated with a given
 * SCSI disk (or a disk that uses, or translates the SCSI command
 * set). In some cases these parameters can be changed.
 *
 * This file contains data tables that may be useful for other
 * programs.
 */


/* Mode pages that aren't specific to any transport protocol */
struct sdparm_values_name_t sdparm_gen_mode_pg[] = {
    {IEC_MP, MSP_BACK_CTL, 0, 0, "bc", "Background control (SBC)"},
    {CACHING_MP, 0, 0, 0, "ca", "Caching (SBC)"},
    {MMCMS_MP, 0, 5, 1, "cms", "CD/DVD (MM) capabilities and mechanical "
        "status (MMC)"},        /* read only */
    {CONTROL_MP, 0, -1, 0, "co", "Control"},
    {DATA_COMPR_MP, 0, 1, 0, "dac", "Data compression (SSC)"},
    {DEV_CONF_MP, 0, 1, 0, "dc", "Device configuration (SSC)"},
    {ES_MAN_MP, 0, 0xd, 0, "esm", "Enclosure services management (SES)"},
    {FORMAT_MP, 0, 0, 0, "fo", "Format (SBC)"},
    {IEC_MP, 0, -1, 0, "ie", "Informational exceptions control"},
    {MRW_MP, 0, 5, 0, "mrw", "Mount rainier reWritable (MMC)"},
    {PROT_SPEC_LU_MP, 0, -1, 0, "pl", "Protocol specific logical unit"},
    {POWER_MP, 0, -1, 0, "po", "Power condition"},
    {PROT_SPEC_PORT_MP, 0, -1, 0, "pp", "Protocol specific port"},
    {RBC_DEV_PARAM_MP, 0, 0xe, 0, "rbc", "RBC device parameters (RBC)"},
    {RIGID_DISK_MP, 0, 0, 0, "rd", "Rigid disk (SBC)"},
    {TIMEOUT_PROT_MP, 0, 5, 0, "rp", "Timeout and protect (MMC)"},
    {RW_ERR_RECOVERY_MP, 0, -1, 0, "rw", "Read write error recovery"},
        /* since in SBC, SSC and MMC treat as if in SPC */
    {V_ERR_RECOVERY_MP, 0, 0, 0, "ve", "Verify error recovery (SBC)"},
    {WRITE_PARAM_MP, 0, 5, 0, "wp", "Write parameters (MMC)"},
    {XOR_MP, 0, 0, 0, "xo", "XOR control (SBC)"},
    {0, 0, 0, 0, NULL, NULL},
};

/* fixed length, indexed by transport protocol number, dummy on end. */
/* Those transports commented with "none" don't have transport specific */
/* mode pages. */
struct sdparm_values_name_t sdparm_transport_id[] = {
    {TP_FCP, 0, -1, 0, "fcp", "Fibre channel (FCP)"},
    {TP_SPI, 0, -1, 0, "spi", "SCSI parallel interface (SPI)"},
    {TP_SSA, 0, -1, 0, "ssa", "Serial storage architecture (SSA)"},
    {TP_1394, 0, -1, 0, "sbp", "Serial bus (SBP)"}, /* none */
    {TP_SRP, 0, -1, 0, "srp", "SCSI remote DMA (SRP)"},
    {TP_ISCSI, 0, -1, 0, "iscsi", "Internet SCSI (iSCSI)"}, /* none */
    {TP_SAS, 0, -1, 0, "sas", "Serial attached SCSI (SAS)"},
    {TP_ADT, 0, -1, 0, "adt", "Automation/Drive interface (ADT)"},
    {TP_ATA, 0, -1, 0, "ata", "AT attachment interface (ATA/ATAPI)"},
                                                         /* none */
    {0x9, 0, -1, 0, "u0x9", NULL},
    {0xa, 0, -1, 0, "u0xa", NULL},
    {0xb, 0, -1, 0, "u0xb", NULL},
    {0xc, 0, -1, 0, "u0xc", NULL},
    {0xd, 0, -1, 0, "u0xd", NULL},
    {0xe, 0, -1, 0, "u0xe", NULL},
    {TP_NONE, 0, -1, 1, "none", "No specific"},
    {0, 0, 0, 0, NULL, NULL},
};

static struct sdparm_values_name_t sdparm_fcp_mode_pg[] = {    /* FCP-3 */
    {DISCONNECT_MP, 0, -1, 0, "dr", "Disconnect-reconnect (FCP)"},
    {PROT_SPEC_LU_MP, 0, -1, 0, "luc", "lu: control (FCP)"},
    {PROT_SPEC_PORT_MP, 0, -1, 0, "pc", "port: control (FCP)"},
    {0, 0, 0, 0, NULL, NULL},
};

static struct sdparm_values_name_t sdparm_spi_mode_pg[] = {    /* SPI-4 */
    {DISCONNECT_MP, 0, -1, 0, "dr", "Disconnect-reconnect (SPI)"},
    {PROT_SPEC_LU_MP, 0, -1, 0, "luc", "lu: control (SPI)"},
    {PROT_SPEC_PORT_MP, MSP_SPI_MC, -1, 0, "mc",
        "port: margin control (SPI)"},
    {PROT_SPEC_PORT_MP, MSP_SPI_NS, -1, 0, "ns",
        "port: negotiated settings (SPI)"},
    {PROT_SPEC_PORT_MP, 0, -1, 0, "psf", "port: short format (SPI)"},
    {PROT_SPEC_PORT_MP, MSP_SPI_RTC, -1, 1, "rtc",
        "port: report transfer capabilities (SPI)"},
    {PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 0, "stc",
        "port: saved training config value (SPI)"},
    {0, 0, 0, 0, NULL, NULL},
};

static struct sdparm_values_name_t sdparm_srp_mode_pg[] = {    /* SRP */
    {DISCONNECT_MP, 0, -1, 0, "dr", "Disconnect-reconnect (SRP)"},
    {0, 0, 0, 0, NULL, NULL},
};

static struct sdparm_values_name_t sdparm_sas_mode_pg[] = {    /* SAS-1.1 */
    {DISCONNECT_MP, 0, -1, 0, "dr", "Disconnect-reconnect (SAS)"},
    {PROT_SPEC_LU_MP, 0, -1, 0, "lsf", "lu: SSP short format (SAS)"},
    {PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 0, "pcd",
        "port: phy control and discover (SAS)"},
    {PROT_SPEC_PORT_MP, 0, -1, 0, "psf", "port: SSP short format (SAS)"},
    {0, 0, 0, 0, NULL, NULL},
};

struct sdparm_values_name_t sdparm_vpd_pg[] = {
    {VPD_DEVICE_ID, 0, -1, 1, "di", "Device identification"},
    {VPD_EXT_INQ, 0, -1, 1, "ei", "Extended inquiry"},
    {VPD_UNIT_SERIAL_NUM, 0, -1, 1, "sn", "Unit serial number"},
    {VPD_SCSI_PORTS, 0, -1, 1, "sp", "SCSI ports"},
    {VPD_SUPPORTED_VPDS, 0, -1, 1, "sv", "Supported VPD pages"},
    {0, 0, 0, 0, NULL, NULL},
};

/* Generic (i.e. non-transport specific) mode page items follow, */
/* sorted by mode page number in ascending order */
struct sdparm_mode_page_item sdparm_mitem_arr[] = {

    /* Read write error recovery mode page [0x1] sbc2, mmc5, ssc3 */
    /* treat as spc since various command sets implement variants */
    {"AWRE", RW_ERR_RECOVERY_MP, 0, -1, 2, 7, 1, MF_COMMON,
        "Automatic write reallocation enabled"},
    {"ARRE", RW_ERR_RECOVERY_MP, 0, -1, 2, 6, 1, MF_COMMON,
        "Automatic read reallocation enabled"},
    {"TB", RW_ERR_RECOVERY_MP, 0, -1, 2, 5, 1, 0,
        "Transfer block"},
    {"RC", RW_ERR_RECOVERY_MP, 0, -1, 2, 4, 1, 0,
        "Read continuous"},
    {"EER", RW_ERR_RECOVERY_MP, 0, -1, 2, 3, 1, 0,
        "Enable early recover"},
    {"PER", RW_ERR_RECOVERY_MP, 0, -1, 2, 2, 1, MF_COMMON,
        "Post error"},
    {"DTE", RW_ERR_RECOVERY_MP, 0, -1, 2, 1, 1, 0,
        "Data terminate on error"},
    {"DCR", RW_ERR_RECOVERY_MP, 0, -1, 2, 0, 1, 0,
        "Disable correction"},
    {"RRC", RW_ERR_RECOVERY_MP, 0, -1, 3, 7, 8, 0,
        "Read retry count"},
    {"EMCDR", RW_ERR_RECOVERY_MP, 0, 5, 7, 1, 2, 0, /* MMC only */
        "Enhanced media certification and defect reporting"},
    {"WRC", RW_ERR_RECOVERY_MP, 0, -1, 8, 7, 8, 0,
        "Write retry count"},
    {"RTL", RW_ERR_RECOVERY_MP, 0, -1, 10, 7, 16, 0,
        "Recovery time limit (ms)"},

    /* Disconnect-reconnect mode page [0x2] see transport section */

    /* Format mode page [0x3] sbc2 (obsolete) */
    {"TPZ", FORMAT_MP, 0, 0, 2, 7, 16, 0,
        "Tracks per zone"},
    {"ASPZ", FORMAT_MP, 0, 0, 4, 7, 16, 0,
        "Alternate sectors per zone"},
    {"ATPZ", FORMAT_MP, 0, 0, 6, 7, 16, 0,
        "Alternate tracks per zone"},
    {"ATPLU", FORMAT_MP, 0, 0, 8, 7, 16, 0,
        "Alternate tracks per logical unit"},
    {"SPT", FORMAT_MP, 0, 0, 10, 7, 16, 0,
        "Sectors per track"},
    {"DBPPS", FORMAT_MP, 0, 0, 12, 7, 16, 0,
        "Data bytes per physical sector"},
    {"INTLV", FORMAT_MP, 0, 0, 14, 7, 16, 0,
        "Interleave"},
    {"TSF", FORMAT_MP, 0, 0, 16, 7, 16, 0,
        "Track skew factor"},
    {"CSF", FORMAT_MP, 0, 0, 18, 7, 16, 0,
        "Cylinder skew factor"},
    {"SSEC", FORMAT_MP, 0, 0, 20, 7, 1, 0,
        "Soft sector"},
    {"HSEC", FORMAT_MP, 0, 0, 20, 6, 1, 0,
        "Hard sector"},
    {"RMB", FORMAT_MP, 0, 0, 20, 5, 1, 0,
        "Removable"},
    {"SURF", FORMAT_MP, 0, 0, 20, 4, 1, 0,
        "Surface"},

    /* Mount Rainier reWitable mode page [0x3] mmc4  */
    {"LBAS", MRW_MP, 0, 5, 3, 0, 1, 0,
        "LBA space"},

    /* Rigid disk mode page [0x4] sbc2 (obsolete) */
    {"NOC", RIGID_DISK_MP, 0, 0, 2, 7, 24, 0,
        "Number of cylinders"},
    {"NOH", RIGID_DISK_MP, 0, 0, 5, 7, 8, 0,
        "Number of heads"},
    {"SCWP", RIGID_DISK_MP, 0, 0, 6, 7, 24, 0,
        "Starting cylinder for write precompensation"},
    {"SCRWC", RIGID_DISK_MP, 0, 0, 9, 7, 24, 0,
        "Starting cylinder for reduced write current"},
    {"DSR", RIGID_DISK_MP, 0, 0, 12, 7, 16, 0,
        "Device step rate"},
    {"LZC", RIGID_DISK_MP, 0, 0, 14, 7, 24, 0,
        "Landing zone cylinder"},
    {"RPL", RIGID_DISK_MP, 0, 0, 17, 1, 2, 0,
        "Rotational position locking"},
    {"ROTO", RIGID_DISK_MP, 0, 0, 18, 7, 8, 0,
        "Rotational offset"},
    {"MRR", RIGID_DISK_MP, 0, 0, 20, 7, 16, 0,
        "Medium rotation rate (rpm)"},

    /* Write parameters mode page [0x5] mmc5 */
    {"BUFE", WRITE_PARAM_MP, 0, 5, 2, 6, 1, MF_COMMON,
        "Buffer underrun free recording enable"},
    {"LS_V", WRITE_PARAM_MP, 0, 5, 2, 5, 1, 0,
        "Link size valid"},
    {"TST_W", WRITE_PARAM_MP, 0, 5, 2, 4, 1, 0,
        "Test write"},
    {"WR_T", WRITE_PARAM_MP, 0, 5, 2, 3, 4, MF_COMMON,
        "Write type"},
    {"MULTI_S", WRITE_PARAM_MP, 0, 5, 3, 7, 2, MF_COMMON,
        "Multi session"},
    {"FP", WRITE_PARAM_MP, 0, 5, 3, 5, 1, MF_COMMON,
        "Fixed packet type"},
    {"COPY", WRITE_PARAM_MP, 0, 5, 3, 4, 1, 0,
        "Serial copy management system (SCMS) enable"},
    {"TRACK_M", WRITE_PARAM_MP, 0, 5, 3, 3, 4, MF_COMMON,
        "Track mode"},
    {"DBT", WRITE_PARAM_MP, 0, 5, 4, 3, 4, 0,
        "Data block type"},
    {"LINK_S", WRITE_PARAM_MP, 0, 5, 5, 7, 8, 0,
        "Link size"},
    {"IAC", WRITE_PARAM_MP, 0, 5, 7, 5, 6, 0,
        "Initiator application code"},
    {"SESS_F", WRITE_PARAM_MP, 0, 5, 8, 7, 8, 0,
        "Session format"},
    {"PACK_S", WRITE_PARAM_MP, 0, 5, 10, 7, 32, 0,
        "Packet size"},
    {"APL", WRITE_PARAM_MP, 0, 5, 14, 7, 16, 0,
        "Audio pause length (blocks)"},

    /* Device parameters mode page [0x6] rbc */
    {"WCD", RBC_DEV_PARAM_MP, 0, 0xe, 2, 0, 1, MF_COMMON,
        "Write cache disable"},
    {"LBS", RBC_DEV_PARAM_MP, 0, 0xe, 3, 7, 16, MF_COMMON,
        "Logical block size"},
    {"NLBS", RBC_DEV_PARAM_MP, 0, 0xe, 5, 7, 40, (MF_COMMON | MF_HEX),
        "Number of logical blocks"},
    {"P_P", RBC_DEV_PARAM_MP, 0, 0xe, 10, 7, 8, 0,
        "Power/performance"},
    {"READD", RBC_DEV_PARAM_MP, 0, 0xe, 11, 3, 1, 0,
        "Read disable"},
    {"WRITED", RBC_DEV_PARAM_MP, 0, 0xe, 11, 2, 1, 0,
        "Write disable"},
    {"FORMATD", RBC_DEV_PARAM_MP, 0, 0xe, 11, 1, 1, 0,
        "Format disable"},
    {"LOCKD", RBC_DEV_PARAM_MP, 0, 0xe, 11, 0, 1, 0,
        "Lock disable"},

    /* Verify error recovery mode page [0x7] sbc2 */
    {"V_EER", V_ERR_RECOVERY_MP, 0, 0, 2, 3, 1, 0,
        "Enable early recover"},
    {"V_PER", V_ERR_RECOVERY_MP, 0, 0, 2, 2, 1, 0,
        "Post error"},
    {"V_DTE", V_ERR_RECOVERY_MP, 0, 0, 2, 1, 1, 0,
        "Data terminate on error"},
    {"V_DCR", V_ERR_RECOVERY_MP, 0, 0, 2, 0, 1, 0,
        "Disable correction"},
    {"V_RC", V_ERR_RECOVERY_MP, 0, 0, 3, 7, 8, 0,
        "Verify retry count"},
    {"V_RTL", V_ERR_RECOVERY_MP, 0, 0, 10, 7, 16, 0,
        "Verify recovery time limit (ms)"},

    /* Caching mode page [0x8] sbc2 */
    {"IC", CACHING_MP, 0, 0, 2, 7, 1, 0,
        "Initiator control"},
    {"ABPF", CACHING_MP, 0, 0, 2, 6, 1, 0,
        "Abort pre-fetch"},
    {"CAP", CACHING_MP, 0, 0, 2, 5, 1, 0,
        "Caching analysis permitted"},
    {"DISC", CACHING_MP, 0, 0, 2, 4, 1, 0,
        "Discontinuity"},
    {"SIZE", CACHING_MP, 0, 0, 2, 3, 1, 0,
        "Size (1->CSS valid, 0->NCS valid)"},
    {"WCE", CACHING_MP, 0, 0, 2, 2, 1, MF_COMMON,
        "Write cache enable"},
    {"MF", CACHING_MP, 0, 0, 2, 1, 1, 0,
        "Multiplication factor"},
    {"RCD", CACHING_MP, 0, 0, 2, 0, 1, MF_COMMON,
        "Read cache disable"},
    {"DRRP", CACHING_MP, 0, 0, 3, 7, 4, 0,
        "Demand read retension priority"},
    {"WRP", CACHING_MP, 0, 0, 3, 3, 4, 0,
        "Write retension priority"},
    {"DPTL", CACHING_MP, 0, 0, 4, 7, 16, 0,
        "Disable pre-fetch transfer length"},
    {"MIPF", CACHING_MP, 0, 0, 6, 7, 16, 0,
        "Minimum pre-fetch"},
    {"MAPF", CACHING_MP, 0, 0, 8, 7, 16, 0,
        "Maximum pre-fetch"},
    {"MAPFC", CACHING_MP, 0, 0, 10, 7, 16, 0,
        "Maximum pre-fetch ceiling"},
    {"FSW", CACHING_MP, 0, 0, 12, 7, 1, 0,
        "Force sequential write"},
    {"LBCSS", CACHING_MP, 0, 0, 12, 5, 1, 0,
        "Logical block cache segment size"},
    {"DRA", CACHING_MP, 0, 0, 12, 4, 1, 0,
        "Disable read ahead"},
    {"NV_DIS", CACHING_MP, 0, 0, 12, 0, 1, 0,
        "Non-volatile cache disable"},
    {"NCS", CACHING_MP, 0, 0, 13, 7, 8, 0,
        "Number of cache segments"},
    {"CSS", CACHING_MP, 0, 0, 14, 7, 16, 0,
        "Cache segment size"},

    /* Control mode page [0xa] spc3 */
    {"TST", CONTROL_MP, 0, -1, 2, 7, 3, 0,
        "Task set type"},
    {"TMF_ONLY", CONTROL_MP, 0, -1, 2, 4, 1, 0,
        "Task management functions only"},
    {"D_SENSE", CONTROL_MP, 0, -1, 2, 2, 1, 0,
        "Descriptor format sense data"},
    {"GLTSD", CONTROL_MP, 0, -1, 2, 1, 1, 0,
        "Global logging target save disable"},
    {"RLEC", CONTROL_MP, 0, -1, 2, 0, 1, 0,
        "Report log exception condition"},
    {"QAM", CONTROL_MP, 0, -1, 3, 7, 4, 0,
        "Queue algorithm modifier"},
    {"QERR", CONTROL_MP, 0, -1, 3, 2, 2, 0,
        "Queue error management"},
    {"RAC", CONTROL_MP, 0, -1, 4, 6, 1, 0,
        "Report a check"},
    {"UA_INTLCK", CONTROL_MP, 0, -1, 4, 5, 2, 0,
        "Unit attention interlocks controls"},
    {"SWP", CONTROL_MP, 0, -1, 4, 3, 1, MF_COMMON,
        "Software write protect"},
    {"ATO", CONTROL_MP, 0, -1, 5, 7, 1, 0,
        "Application tag owner"},
    {"TAS", CONTROL_MP, 0, -1, 5, 6, 1, 0,
        "Task aborted status"},
    {"AUTOLOAD", CONTROL_MP, 0, -1, 5, 2, 3, 0,
        "Autoload mode"},
    {"BTP", CONTROL_MP, 0, -1, 8, 7, 16, 0,
        "Busy timeout period (100us)"},
    {"ESTCT", CONTROL_MP, 0, -1, 10, 7, 16, 0,
        "Extended self test completion time (sec)"},

    /* Control extension mode subpage [0xa,0x1] spc3 */
    /* will this trip up older devices ?? */

    /* Data compression mode page [0xf] ssc3 */
    {"DCE", DATA_COMPR_MP, 0, 1, 2, 7, 1, MF_COMMON,
        "Data compression enable"},
    {"DCC", DATA_COMPR_MP, 0, 1, 2, 6, 1, MF_COMMON,
        "Data compression capable"},
    {"DDE", DATA_COMPR_MP, 0, 1, 3, 7, 1, MF_COMMON,
        "Data decompression enable"},
    {"RED", DATA_COMPR_MP, 0, 1, 3, 6, 2, 0,
        "Report exception on decompression"},
    {"COMPR_A", DATA_COMPR_MP, 0, 1, 4, 7, 32, 0,
        "Compression algorithm"},
    {"DCOMPR_A", DATA_COMPR_MP, 0, 1, 8, 7, 32, 0,
        "Decompression algorithm"},

    /* XOR control mode page [0x10] sbc2 */
    {"XORDIS", XOR_MP, 0, 0, 2, 1, 1, 0,
        "XOR disable"},
    {"MXWS", XOR_MP, 0, 0, 4, 7, 32, 0,
        "Maximum XOR write size (blocks)"},

    /* Device configuration mode page [0x10] ssc3 */
    {"CAF", DEV_CONF_MP, 0, 1, 2, 5, 1, 0,
        "Change active format"},
    {"ACT_F", DEV_CONF_MP, 0, 1, 2, 4, 5, 0,
        "Active format"},
    {"ACT_P", DEV_CONF_MP, 0, 1, 3, 7, 8, 0,
        "Active partition"},
    {"WOBFR", DEV_CONF_MP, 0, 1, 4, 7, 8, 0,
        "Write object buffer full ratio"},
    {"ROBER", DEV_CONF_MP, 0, 1, 5, 7, 8, 0,
        "Read object buffer empty ratio"},
    {"WDT", DEV_CONF_MP, 0, 1, 6, 7, 16, 0,
        "Write delay time (100 ms)"},
    {"OBR", DEV_CONF_MP, 0, 1, 8, 7, 1, 0,
        "Object buffer recovery"},
    {"LOIS", DEV_CONF_MP, 0, 1, 8, 6, 1, 0,
        "Logical object identifiers supported"},
    {"RSMK", DEV_CONF_MP, 0, 1, 8, 5, 1, MF_COMMON,
        "Report setmarks"},
    {"AVC", DEV_CONF_MP, 0, 1, 8, 4, 1, 0,
        "Automatic velocity control"},
    {"SOCF", DEV_CONF_MP, 0, 1, 8, 3, 2, 0,
        "Stop on consecutive filemarks"},
    {"ROBO", DEV_CONF_MP, 0, 1, 8, 1, 1, 0,
        "Recover object buffer order"},
    {"REW", DEV_CONF_MP, 0, 1, 8, 0, 1, 0,
        "Report early warning"},
    {"GAP_S", DEV_CONF_MP, 0, 1, 9, 7, 8, 0,
        "Gap size"},
    {"EOD_D", DEV_CONF_MP, 0, 1, 10, 7, 3, 0,
        "EOD (end-of-data) defined"},
    {"EEG", DEV_CONF_MP, 0, 1, 10, 4, 1, 0,
        "Enable EOD generation"},
    {"SEW", DEV_CONF_MP, 0, 1, 10, 3, 1, MF_COMMON,
        "Synchronize early warning"},
    {"SWP_T", DEV_CONF_MP, 0, 1, 10, 2, 1, 0,
        "Software write protect (tape)"},
    {"BAML", DEV_CONF_MP, 0, 1, 10, 1, 1, 0,
        "Block address mode lock"},
    {"BAM", DEV_CONF_MP, 0, 1, 10, 0, 1, 0,
        "Block address mode"},
    {"OBSAEW", DEV_CONF_MP, 0, 1, 11, 7, 24, 0,
        "Object buffer size at early warning"},
    {"SDCA", DEV_CONF_MP, 0, 1, 14, 7, 8, MF_COMMON,
        "Select data compression algorithm"},
    {"WRTE", DEV_CONF_MP, 0, 1, 15, 7, 2, 0,
        "WORM tamper read enable"},
    {"OIR", DEV_CONF_MP, 0, 1, 15, 5, 1, 0,
        "Only if reserved"},
    {"ROR", DEV_CONF_MP, 0, 1, 15, 4, 2, 0,
        "Rewind on reset"},
    {"ASOCWP", DEV_CONF_MP, 0, 1, 15, 2, 1, 0,
        "Associated write protection"},
    {"PERSWP", DEV_CONF_MP, 0, 1, 15, 1, 1, 0,
        "Persistent write protection"},
    {"PRMWP", DEV_CONF_MP, 0, 1, 15, 1, 0, 0,
        "Permanent write protection"},

    /* Enclosure services management mode page [0x14] ses2 */
    {"ENBLTC", ES_MAN_MP, 0, 0xd, 5, 0, 1, MF_COMMON,
        "Enable timed completion"},
    {"MTCT", ES_MAN_MP, 0, 0xd, 6, 7, 16, MF_COMMON,
        "Maximum task completion time (100ms)"},

    /* (Transport) protocol specific logical unit control mode page */
    /*   [0x18] spc3 */
    {"LUPID", PROT_SPEC_LU_MP, 0, -1, 2, 3, 4, 0,
        "Logical unit's (transport) protocol identifier"},

    /* (Transport) protocol specific port control mode page [0x19] spc3 */
    {"PPID", PROT_SPEC_PORT_MP, 0, -1, 2, 3, 4, 0,
        "Port's (transport) protocol identifier"},

    /* Power condition mode page [0x1a] spc3 */
    {"IDLE", POWER_MP, 0, -1, 3, 1, 1, 0,
        "Idle timer active"},
    {"STANDBY", POWER_MP, 0, -1, 3, 0, 1, 0,
        "Standby timer active"},
    {"ICT", POWER_MP, 0, -1, 4, 7, 32, 0,
        "Idle condition timer (100 ms)"},
    {"SCT", POWER_MP, 0, -1, 8, 7, 32, 0,
        "Standby condition timer (100 ms)"},

    /* Informational exception control mode page [0x1c] spc3 */
    {"PERF", IEC_MP, 0, -1, 2, 7, 1, 0,
        "Performance"},
    {"EBF", IEC_MP, 0, -1, 2, 5, 1, 0,
        "Enable background function"},
    {"EWASC", IEC_MP, 0, -1, 2, 4, 1, MF_COMMON,
        "Enable warning"},
    {"DEXCPT", IEC_MP, 0, -1, 2, 3, 1, MF_COMMON,
        "Disable exceptions"},
    {"TEST", IEC_MP, 0, -1, 2, 2, 1, 0,
        "Test (simulate device failure)"},
    {"LOGERR", IEC_MP, 0, -1, 2, 0, 1, 0,
        "Log informational exception errors"},
    {"MRIE", IEC_MP, 0, -1, 3, 3, 4, MF_COMMON,
        "Method of reporting informational exceptions"},
    {"INTT", IEC_MP, 0, -1, 4, 7, 32, 0,
        "Interval timer (100 ms)"},
    {"REPC", IEC_MP, 0, -1, 8, 7, 32, 0,
        "Report count"},

    /* Background control mode subpage [0x1c,0x1] sbc2 */
    {"EN_PS", IEC_MP, MSP_BACK_CTL, 0, 4, 0, 1, 0,
        "Enable pre-scan"},
    {"EN_BMS", IEC_MP, MSP_BACK_CTL, 0, 5, 0, 1, 0,
        "Enable background medium scan"},
    {"BMS_I", IEC_MP, MSP_BACK_CTL, 0, 6, 7, 16, 0,
        "Background medium scan interval time (hour)"},
    {"PS_T", IEC_MP, MSP_BACK_CTL, 0, 8, 7, 16, 0,
        "Pre-scan timeout (hour)"},

    /* Timeout and protect mode page [0x1d] mmc5 */
    {"G3E", TIMEOUT_PROT_MP, 0, 5, 4, 3, 1, 0,
        "Group 3 timeout capability enable"},
    {"TMOE", TIMEOUT_PROT_MP, 0, 5, 4, 2, 1, 0,
        "Timeout enable"},
    {"DISP", TIMEOUT_PROT_MP, 0, 5, 4, 1, 1, 0,
        "Disable (unavailable) until power cycle"},
    {"SWPP", TIMEOUT_PROT_MP, 0, 5, 4, 0, 1, 0,
        "Software write protect until power cycle"},
    {"G1MT", TIMEOUT_PROT_MP, 0, 5, 6, 7, 16, 0,
        "Group 1 minimum timeout (sec)"},
    {"G2MT", TIMEOUT_PROT_MP, 0, 5, 8, 7, 16, 0,
        "Group 2 minimum timeout (sec)"},

    /* CD/DVD (MM) capabilities and mechanical status mode page */
    /* [0x2a] obsolete in mmc4 and mmc5, last valid in mmc3 */
    {"D_RAM_R", MMCMS_MP, 0, 5, 2, 5, 1, 0,
        "DVD-RAM read"},
    {"D_R_R", MMCMS_MP, 0, 5, 2, 4, 1, 0,
        "DVD-R read"},
    {"D_ROM_R", MMCMS_MP, 0, 5, 2, 3, 1, 0,
        "DVD-ROM read"},
    {"METH2", MMCMS_MP, 0, 5, 2, 2, 1, 0,
        "Method 2"},
    {"CD_RW_R", MMCMS_MP, 0, 5, 2, 1, 1, 0,
        "CD-RW read"},
    {"CD_R_R", MMCMS_MP, 0, 5, 2, 0, 1, 0,
        "CD-R read"},
    {"D_RAM_W", MMCMS_MP, 0, 5, 3, 5, 1, 0,
        "DVD-RAM write"},
    {"D_R_R", MMCMS_MP, 0, 5, 3, 4, 1, 0,
        "DVD-R write"},
    {"TST_W", MMCMS_MP, 0, 5, 3, 2, 1, 0,
        "Test write"},
    {"CD_RW_W", MMCMS_MP, 0, 5, 3, 1, 1, 0,
        "CD-RW write"},
    {"CD_R_W", MMCMS_MP, 0, 5, 3, 0, 1, 0,
        "CD-R write"},
    {"BUF", MMCMS_MP, 0, 5, 4, 7, 1, 0,
        "Buffer underrun free recording"},
    {"MULTI_S", MMCMS_MP, 0, 5, 4, 6, 1, 0,
        "Multi session"},
    {"M2F2", MMCMS_MP, 0, 5, 4, 5, 1, 0,
        "Mode 2 form 2"},
    {"M2F1", MMCMS_MP, 0, 5, 4, 4, 1, 0,
        "Mode 2 form 1"},
    {"DP_2", MMCMS_MP, 0, 5, 4, 3, 1, 0,
        "Digital port 2"},
    {"DP_1", MMCMS_MP, 0, 5, 4, 2, 1, 0,
        "Digital port 1"},
    {"COMP", MMCMS_MP, 0, 5, 4, 1, 1, 0,
        "Composite"},
    {"AUDIO_P", MMCMS_MP, 0, 5, 4, 0, 1, 0,
        "Audio play"},
    {"RBC", MMCMS_MP, 0, 5, 5, 7, 1, 0,
        "Read bar code"},
    {"UPC", MMCMS_MP, 0, 5, 5, 6, 1, 0,
        "Uniform product code"},
    {"ISRC", MMCMS_MP, 0, 5, 5, 5, 1, 0,
        "International standard recording code"},
    {"C2PS", MMCMS_MP, 0, 5, 5, 4, 1, 0,
        "C 2 pointers supported"},
    {"RW_DC", MMCMS_MP, 0, 5, 5, 3, 1, 0,
        "R-W de-interleaved and corrected"},
    {"RW_S", MMCMS_MP, 0, 5, 5, 2, 1, 0,
        "R-W supported"},
    {"CDDA_SA", MMCMS_MP, 0, 5, 5, 1, 1, 0,
        "CD-DA stream accurate"},
    {"CDDA_CS", MMCMS_MP, 0, 5, 5, 0, 1, 0,
        "CD-DA commands supported"},
    {"LMT", MMCMS_MP, 0, 5, 6, 7, 3, 0,
        "Loading mechanism type"},
    {"EJECT", MMCMS_MP, 0, 5, 6, 3, 1, 0,
        "Eject (individual or magazine)"},
    {"PJ", MMCMS_MP, 0, 5, 6, 2, 1, 0,
        "Prevent jumper"},
    {"LS", MMCMS_MP, 0, 5, 6, 1, 1, 0,
        "Lock state"},
    {"LOCK", MMCMS_MP, 0, 5, 6, 0, 1, 0,
        "Lock (supported)"},
    {"RWILI", MMCMS_MP, 0, 5, 7, 5, 1, 0,
        "R-W in lead in"},
    {"SCC", MMCMS_MP, 0, 5, 7, 4, 1, 0,
        "Side change capable"},
    {"SSS", MMCMS_MP, 0, 5, 7, 3, 1, 0,
        "Software slot selection"},
    {"CSDP", MMCMS_MP, 0, 5, 7, 2, 1, 0,
        "Changer supports disc present"},
    {"SCM", MMCMS_MP, 0, 5, 7, 1, 1, 0,
        "Separate channel mute"},
    {"SVL", MMCMS_MP, 0, 5, 7, 0, 1, 0,
        "Separate volume levels"},
    {"NVLS", MMCMS_MP, 0, 5, 10, 7, 16, 0,
        "Number of volume levels supported"},
    {"BSS", MMCMS_MP, 0, 5, 12, 7, 16, 0,
        "Buffer size supported (1024 bytes)"},
    {"LENGTH", MMCMS_MP, 0, 5, 17, 5, 2, 0,
        "Length (bit length of IEC958 words)"},
    {"LSBF", MMCMS_MP, 0, 5, 17, 3, 1, 0,
        "LSB (least significant bit) first"},
    {"RCK", MMCMS_MP, 0, 5, 17, 2, 1, 0,
        "High on LRCK indicates left channel"},
    {"BCKF", MMCMS_MP, 0, 5, 17, 1, 1, 0,
        "BCK signal falling edge"},
    {"CMRS", MMCMS_MP, 0, 5, 22, 7, 16, 0,
        "Copy management revision supported"},
    {"RCS", MMCMS_MP, 0, 5, 27, 1, 2, 0,
        "Rotation control selected"},
    {"CWSS", MMCMS_MP, 0, 5, 28, 7, 16, 0,
        "Current write speed selected"},

    {NULL, 0, 0, 0, 0, 0, 0, 0, NULL},
};


/* << Transport protocol specific mode page items follow >> */

static struct sdparm_mode_page_item sdparm_mitem_fcp_arr[] = {
    /* disconnect-reconnect mode page [0x2] fcp3 */
    {"BFR", DISCONNECT_MP, 0, -1, 2, 7, 8, 0,
        "Buffer full ratio"},
    {"BER", DISCONNECT_MP, 0, -1, 3, 7, 8, 0,
        "Buffer empty ratio"},
    {"BIL", DISCONNECT_MP, 0, -1, 4, 7, 16, MF_COMMON,
        "Bus inactivity limit (transmission words)"},
    {"DTL", DISCONNECT_MP, 0, -1, 6, 7, 16, MF_COMMON,
        "Disconnect time limit (128 transmission words)"},
    {"CTL", DISCONNECT_MP, 0, -1, 8, 7, 16, MF_COMMON,
        "Connect time limit (128 transmission words)"},
    {"MBS", DISCONNECT_MP, 0, -1, 10, 7, 16, MF_COMMON,
        "Maximum burst size (512 bytes)"},
    {"EMDP", DISCONNECT_MP, 0, -1, 12, 7, 1, 0,
        "Enable modify data pointers"},
    {"FAA", DISCONNECT_MP, 0, -1, 12, 6, 1, 0,
        "Fairness access A [FCP_DATA]"},
    {"FAB", DISCONNECT_MP, 0, -1, 12, 5, 1, 0,
        "Fairness access B [FCP_XFER]"},
    {"FAC", DISCONNECT_MP, 0, -1, 12, 4, 1, 0,
        "Fairness access C [FCP_RSP]"},
    {"FBS", DISCONNECT_MP, 0, -1, 14, 7, 16, 0,
        "First burst size (512 bytes)"},

    /* protocol specific logical unit mode page [0x18] fcp3 */
    {"LUPID", PROT_SPEC_LU_MP, 0, -1, 2, 3, 4, MF_COMMON,
        "Logical unit's (transport) protocol identifier"},
    {"EPDC", PROT_SPEC_LU_MP, 0, -1, 3, 0, 1, MF_COMMON,
        "Enable precise delivery checking"},

    /* protocol specific port control page [0x19] fcp3 */
    {"PPID", PROT_SPEC_PORT_MP, 0, -1, 2, 3, 4, MF_COMMON,
        "Port's (transport) protocol identifier"},
    {"DTFD", PROT_SPEC_PORT_MP, 0, -1, 3, 7, 1, MF_COMMON,
        "Disable target fabric discovery"},
    {"PLPB", PROT_SPEC_PORT_MP, 0, -1, 3, 6, 1, MF_COMMON,
        "Prevent loop port bypass"},
    {"DDIS", PROT_SPEC_PORT_MP, 0, -1, 3, 5, 1, 0,
        "Disable discovery"},
    {"DLM", PROT_SPEC_PORT_MP, 0, -1, 3, 4, 1, 0,
        "Disable loop master"},
    {"RHA", PROT_SPEC_PORT_MP, 0, -1, 3, 3, 1, 0,
        "Require hard address"},
    {"ALWI", PROT_SPEC_PORT_MP, 0, -1, 3, 2, 1, 0,
        "Allow login without loop initialization"},
    {"DTIPE", PROT_SPEC_PORT_MP, 0, -1, 3, 1, 1, 0,
        "Disable target initialized port enable"},
    {"DTOLI", PROT_SPEC_PORT_MP, 0, -1, 3, 0, 1, 0,
        "Disable target originated loop initialization"},
    {"RRTVU", PROT_SPEC_PORT_MP, 0, -1, 6, 2, 3, 0,
        "Resource recovery timeout value unit"},
    {"SIRRTV", PROT_SPEC_PORT_MP, 0, -1, 7, 7, 8, 0,
        "Sequence initiative resource recovery timeout value"},

    {NULL, 0, 0, 0, 0, 0, 0, 0, NULL},
};

static struct sdparm_mode_page_item sdparm_mitem_spi_arr[] = {
    /* disconnect-reconnect mode page [0x2] spi4 */
    {"BFR", DISCONNECT_MP, 0, -1, 2, 7, 8, 0,
        "Buffer full ratio"},
    {"BER", DISCONNECT_MP, 0, -1, 3, 7, 8, 0,
        "Buffer empty ratio"},
    {"BIL", DISCONNECT_MP, 0, -1, 4, 7, 16, MF_COMMON,
        "Bus inactivity limit (100 us)"},
    {"PDTL", DISCONNECT_MP, 0, -1, 6, 7, 16, MF_COMMON,
        "Physical disconnect time limit (100 us)"},
    {"CTL", DISCONNECT_MP, 0, -1, 8, 7, 16, MF_COMMON,
        "Connect time limit (100 us)"},
    {"MBS", DISCONNECT_MP, 0, -1, 10, 7, 16, MF_COMMON,
        "Maximum burst size (512 bytes)"},
    {"EMDP", DISCONNECT_MP, 0, -1, 12, 7, 1, 0,
        "Enable modify data pointers"},
    {"FA", DISCONNECT_MP, 0, -1, 12, 6, 3, 0,
        "Fair arbitration"},
    {"DIMM", DISCONNECT_MP, 0, -1, 12, 3, 1, 0,
        "Disconnect immediate"},
    {"DTDC", DISCONNECT_MP, 0, -1, 12, 2, 3, 0,
        "Data transfer disconnect control"},

    /* protocol specific logical unit control mode page [0x18] spi4 */
    {"LUPID", PROT_SPEC_LU_MP, 0, -1, 2, 3, 4, MF_COMMON,
        "Logical unit's (transport) protocol identifier"},

    /* protocol specific port control page [0x19] spi4 */
    {"PPID", PROT_SPEC_PORT_MP, 0, -1, 2, 3, 4, MF_COMMON,
        "Port's (transport) protocol identifier"},
    {"STT", PROT_SPEC_PORT_MP, 0, -1, 4, 6, 16, MF_COMMON,
        "Synchronous transfer timeout (ms)"},

    /* margin control subpage  [0x19,0x1] spi4 */
    {"PPID_1", PROT_SPEC_PORT_MP, MSP_SPI_MC, -1, 5, 3, 4, 0,
        "Port's (transport) protocol identifier"},
    {"DS", PROT_SPEC_PORT_MP, MSP_SPI_MC, -1, 7, 7, 4, 0,
        "Driver strength"},
    {"DA", PROT_SPEC_PORT_MP, MSP_SPI_MC, -1, 8, 7, 4, 0,
        "Driver asymmetry"},
    {"DA", PROT_SPEC_PORT_MP, MSP_SPI_MC, -1, 8, 3, 4, 0,
        "Driver precompensation"},
    {"DSR", PROT_SPEC_PORT_MP, MSP_SPI_MC, -1, 9, 7, 4, 0,
        "Driver slew rate"},

    /* saved training configuration subpage [0x19,0x2] spi4 */
    {"PPID_2", PROT_SPEC_PORT_MP, MSP_SPI_NS, -1, 5, 3, 4, 0,
        "Port's (transport) protocol identifier"},
    {"DB0", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 10, 7, 32, MF_HEX,
        "DB(0) value"},
    {"DB1", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 14, 7, 32, MF_HEX,
        "DB(1) value"},
    {"DB2", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 18, 7, 32, MF_HEX,
        "DB(2) value"},
    {"DB3", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 22, 7, 32, MF_HEX,
        "DB(3) value"},
    {"DB4", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 26, 7, 32, MF_HEX,
        "DB(4) value"},
    {"DB5", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 30, 7, 32, MF_HEX,
        "DB(5) value"},
    {"DB6", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 34, 7, 32, MF_HEX,
        "DB(6) value"},
    {"DB7", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 38, 7, 32, MF_HEX,
        "DB(7) value"},
    {"DB8", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 42, 7, 32, MF_HEX,
        "DB(8) value"},
    {"DB9", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 46, 7, 32, MF_HEX,
        "DB(9) value"},
    {"DB10", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 50, 7, 32, MF_HEX,
        "DB(10) value"},
    {"DB11", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 54, 7, 32, MF_HEX,
        "DB(11) value"},
    {"DB12", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 58, 7, 32, MF_HEX,
        "DB(12) value"},
    {"DB13", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 62, 7, 32, MF_HEX,
        "DB(13) value"},
    {"DB14", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 66, 7, 32, MF_HEX,
        "DB(14) value"},
    {"DB15", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 70, 7, 32, MF_HEX,
        "DB(15) value"},
    {"P_CRCA", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 74, 7, 32, MF_HEX,
        "P_CRCA value"},
    {"P1", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 78, 7, 32, MF_HEX,
        "P1 value"},
    {"BSY", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 82, 7, 32, MF_HEX,
        "BSY value"},
    {"SEL", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 86, 7, 32, MF_HEX,
        "SEL value"},
    {"RST", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 90, 7, 32, MF_HEX,
        "RST value"},
    {"REQ", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 94, 7, 32, MF_HEX,
        "REQ value"},
    {"ACK", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 98, 7, 32, MF_HEX,
        "ACK value"},
    {"ATN", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 102, 7, 32, MF_HEX,
        "ATN value"},
    {"C_D", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 106, 7, 32, MF_HEX,
        "C/D value"},
    {"I_O", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 110, 7, 32, MF_HEX,
        "I/O value"},
    {"MSG", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 114, 7, 32, MF_HEX,
        "MSG value"},

    /* negotiated settings subpage [0x19,0x3] spi4 */
    {"PPID_3", PROT_SPEC_PORT_MP, MSP_SPI_NS, -1, 5, 3, 4, 0,
        "Port's (transport) protocol identifier"},
    {"TPF", PROT_SPEC_PORT_MP, MSP_SPI_NS, -1, 6, 7, 8, 0,
        "Transfer period factor"},
    {"RAO", PROT_SPEC_PORT_MP, MSP_SPI_NS, -1, 8, 7, 8, 0,
        "REQ/ACK offset"},
    {"TWE", PROT_SPEC_PORT_MP, MSP_SPI_NS, -1, 9, 7, 8, 0,
        "Transfer width exponent"},
    {"POB", PROT_SPEC_PORT_MP, MSP_SPI_NS, -1, 10, 6, 7, 0,
        "Protocol option bits"},
    {"TM", PROT_SPEC_PORT_MP, MSP_SPI_NS, -1, 11, 3, 2, 0,
        "Transceiver mode"},
    {"SPE", PROT_SPEC_PORT_MP, MSP_SPI_NS, -1, 11, 1, 1, 0,
        "Sent PCOMP_EN bit (for current I_T nexus)"},
    {"RPE", PROT_SPEC_PORT_MP, MSP_SPI_NS, -1, 11, 0, 1, 0,
        "Received PCOMP_EN bit (for current I_T nexus)"},

    /* report transfer capabilities subpage [0x19,0x4] spi4 */
    {"PPID_4", PROT_SPEC_PORT_MP, MSP_SPI_RTC, -1, 5, 3, 4, 0,
        "Port's (transport) protocol identifier"},
    {"MRAO", PROT_SPEC_PORT_MP, MSP_SPI_NS, -1, 8, 7, 8, 0,
        "Maximum REQ/ACK offset"},
    {"MTWE", PROT_SPEC_PORT_MP, MSP_SPI_NS, -1, 9, 7, 8, 0,
        "Maximum transfer width exponent"},
    {"POBS", PROT_SPEC_PORT_MP, MSP_SPI_NS, -1, 10, 7, 8, 0,
        "Protocol option bits supported"},

    {NULL, 0, 0, 0, 0, 0, 0, 0, NULL},
};

static struct sdparm_mode_page_item sdparm_mitem_srp_arr[] = {
    /* disconnect-reconnect mode page [0x2] srp */
    {"MBS", DISCONNECT_MP, 0, -1, 10, 7, 16, MF_COMMON,
        "Maximum burst size (512 bytes)"},
    {"EMDP", DISCONNECT_MP, 0, -1, 12, 7, 1, 0,
        "Enable modify data pointers"},

    {NULL, 0, 0, 0, 0, 0, 0, 0, NULL},
};

static struct sdparm_mode_page_item sdparm_mitem_sas_arr[] = {
    /* disconnect-reconnect mode page [0x2] sas1 */
    {"BITL", DISCONNECT_MP, 0, -1, 4, 7, 16, MF_COMMON,
        "Bus inactivity time limit (100us)"},
    {"MCTL", DISCONNECT_MP, 0, -1, 8, 7, 16, MF_COMMON,
        "Maximum connect time limit (100us)"},
    {"MBS", DISCONNECT_MP, 0, -1, 10, 7, 16, MF_COMMON,
        "Maximum burst size (512 bytes)"},
    {"FBS", DISCONNECT_MP, 0, -1, 14, 7, 16, 0,
        "First burst size (512 bytes)"},

    /* protocol specific logical unit control mode page [0x18] sas1 */
    {"LUPID", PROT_SPEC_LU_MP, 0, -1, 2, 3, 4, MF_COMMON,
        "Logical unit's (transport) protocol identifier"},
    {"TLR", PROT_SPEC_LU_MP, 0, -1, 2, 4, 1, 0,
        "Transport layer retries (supported)"},

    /* protocol specific port control page [0x19] sas1 */
    {"PPID", PROT_SPEC_PORT_MP, 0, -1, 2, 3, 4, MF_COMMON,
        "Port's (transport) protocol identifier"},
    {"RLM", PROT_SPEC_PORT_MP, 0, -1, 2, 4, 1, MF_COMMON, 
        "Ready LED meaning"},
    {"ITNLT", PROT_SPEC_PORT_MP, 0, -1, 4, 7, 16, MF_COMMON, 
        "I-T nexus loss time (ms)"},
    {"IRT", PROT_SPEC_PORT_MP, 0, -1, 6, 7, 16, MF_COMMON, 
        "Initiator response timeout (ms)"},

    /* phy control + discover subpage [0x19,0x1] sas1 */
    {"PPID_1", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 5, 3, 4, 0,
        "Port's (transport) protocol identifier"},
    {"NOP", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 7, 7, 8, MF_COMMON, 
        "Number of phys"},
    /* */
    {"PHID", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 9, 7, 8, 0, 
        "Phy identifier"},
    {"ADT", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 12, 6, 3, 0, 
        "Attached device type"},
    {"NPLR", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 13, 3, 4, 0, 
        "Negotiated physical link rate"},
    {"ASIP", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 14, 3, 1, 0, 
        "Attached SSP initiator port"},
    {"ATIP", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 14, 2, 1, 0, 
        "Attached STP initiator port"},
    {"AMIP", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 14, 1, 1, 0, 
        "Attached SMP initiator port"},
    {"ASTP", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 15, 3, 1, 0, 
        "Attached SSP target port"},
    {"ATTP", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 15, 2, 1, 0, 
        "Attached STP target port"},
    {"AMTP", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 15, 1, 1, 0, 
        "Attached SMP target port"},
    {"SASA", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 16, 7, 64, MF_HEX, 
        "SAS address"},
    {"ASASA", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 24, 7, 64, MF_HEX, 
        "Attached SAS address"},
    {"APHID", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 32, 7, 8, 0, 
        "Attached phy identifier"},
    {"PMILR", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 40, 7, 4, 0, 
        "Programmed minimum link rate"},
    {"HMILR", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 40, 3, 4, 0, 
        "Hardware minimum link rate"},
    {"PMALR", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 41, 7, 4, 0, 
        "Programmed maximum link rate"},
    {"HMALR", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 41, 3, 4, 0, 
        "Hardware maximum link rate"},
    /* */
    {"2_PHID", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 57, 7, 8, 0, 
        "Phy identifier"},
    {"2_ADT", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 60, 6, 3, 0, 
        "Attached device type"},
    {"2_NPLR", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 61, 3, 4, 0, 
        "Negotiated physical link rate"},
    {"2_ASIP", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 62, 3, 1, 0, 
        "Attached SSP initiator port"},
    {"2_ATIP", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 62, 2, 1, 0, 
        "Attached STP initiator port"},
    {"2_AMIP", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 62, 1, 1, 0, 
        "Attached SMP initiator port"},
    {"2_ASTP", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 63, 3, 1, 0, 
        "Attached SSP target port"},
    {"2_ATTP", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 63, 2, 1, 0, 
        "Attached STP target port"},
    {"2_AMTP", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 63, 1, 1, 0, 
        "Attached SMP target port"},
    {"2_SASA", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 64, 7, 64, MF_HEX, 
        "SAS address"},
    {"2_ASASA", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 72, 7, 64, MF_HEX, 
        "Attached SAS address"},
    {"2_APHID", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 80, 7, 8, 0, 
        "Attached phy identifier"},
    {"2_PMILR", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 88, 7, 4, 0, 
        "Programmed minimum link rate"},
    {"2_HMILR", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 88, 3, 4, 0, 
        "Hardware minimum link rate"},
    {"2_PMALR", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 89, 7, 4, 0, 
        "Programmed maximum link rate"},
    {"2_HMALR", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 89, 3, 4, 0, 
        "Hardware maximum link rate"},

    {NULL, 0, 0, 0, 0, 0, 0, 0, NULL},
};

/* fixed length, indexed by transport protocol number */
struct sdparm_transport_pair sdparm_transport_mp[] = {
    {sdparm_fcp_mode_pg, sdparm_mitem_fcp_arr},
    {sdparm_spi_mode_pg, sdparm_mitem_spi_arr},
    {NULL, NULL},
    {NULL, NULL},
    {sdparm_srp_mode_pg, sdparm_mitem_srp_arr},
    {NULL, NULL},
    {sdparm_sas_mode_pg, sdparm_mitem_sas_arr},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
};
const char * sdparm_scsi_ptype_strs[] = {
    /* 0 */ "disk",
    "tape",
    "printer",
    "processor",
    "write once optical disk",
    /* 5 */ "cd/dvd",
    "scanner",
    "optical memory device",
    "medium changer",
    "communications",
    /* 0xa */ "graphics",
    "graphics",
    "storage array controller",
    "enclosure services device",
    "simplified direct access device",
    "optical card reader/writer device",
    /* 0x10 */ "bridge controller commands",
    "object based storage",
    "automation/driver interface",
    "0x13", "0x14", "0x15", "0x16", "0x17", "0x18",
    "0x19", "0x1a", "0x1b", "0x1c", "0x1d",
    "well known logical unit",
    "no physical device on this lu",
};


const char * sdparm_transport_proto_arr[] =
{
    "Fibre Channel (FCP-2)",
    "Parallel SCSI (SPI-4)",
    "SSA (SSA-S3P)",
    "IEEE 1394 (SBP-3)",
    "Remote Direct Memory Access (RDMA)",
    "Internet SCSI (iSCSI)",
    "Serial Attached SCSI (SAS)",
    "Automation/Drive Interface (ADT)",
    "ATA Packet Interface (ATA/ATAPI-7)",
    "Ox9", "Oxa", "Oxb", "Oxc", "Oxd", "Oxe",
    "No specific protocol"
};

const char * sdparm_code_set_arr[] =
{
    "Reserved [0x0]",
    "Binary",
    "ASCII",
    "UTF-8",
    "Reserved [0x4]", "Reserved [0x5]", "Reserved [0x6]", "Reserved [0x7]",
    "Reserved [0x8]", "Reserved [0x9]", "Reserved [0xa]", "Reserved [0xb]",
    "Reserved [0xc]", "Reserved [0xd]", "Reserved [0xe]", "Reserved [0xf]",
};

const char * sdparm_assoc_arr[] =
{
    "Addressed logical unit",
    "Target port that received request",
    "Target device that contains addressed lu",
    "Reserved [0x3]",
};

const char * sdparm_id_type_arr[] =
{
    "vendor specific [0x0]",
    "T10 vendor identification",
    "EUI-64 based",
    "NAA",
    "Relative target port",
    "Target port group",
    "Logical unit group",
    "MD5 logical unit identifier",
    "SCSI name string",
    "Reserved [0x9]", "Reserved [0xa]", "Reserved [0xb]",
    "Reserved [0xc]", "Reserved [0xd]", "Reserved [0xe]", "Reserved [0xf]",
};

const char * sdparm_ansi_version_arr[] =
{
    "no conformance claimed",
    "SCSI-1",
    "SCSI-2",
    "SPC",
    "SPC-2",
    "SPC-3",
    "SPC-4",
    "ANSI version: 7",
};

struct sdparm_command sdparm_command_arr[] =
{
    {CMD_EJECT, "eject"},
    {CMD_LOAD, "load"},
    {CMD_READY, "ready"},
    {CMD_START, "start"},
    {CMD_STOP, "stop"},
    {CMD_UNLOCK, "unlock"},
    {-1, NULL},
};

/*
 * Copyright (c) 2005-2013 Douglas Gilbert.
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
#include "sg_lib.h"
#include "sdparm.h"


#define PROTO_IDENT_STR "0: fcp; 1: spi; 4: srp; 5: iscsi; 6: sas/spl; " \
        "7: adt;\t8: ata/acs; 9: uas; 10: sop"

/*
 * sdparm is a utility program to access and change SCSI device
 * (logical unit) mode page fields and do some other housekeeping.
 *
 * This file contains data tables that may be useful for other
 * programs. The data in these tables is derived from various (draft)
 * documents found at http://www.t10.org
 */


/* SSC's medium partition mode page has a variable number of
   partition size fields which are treated as descriptors here */
static struct sdparm_mode_descriptor_t ssc_mpa_desc = {
    3, 1, 1, 8, 2, -1, -1, "SSC medium partition"
};

/* SMC's transport geometry parameters mode page doesn't give the number
   of following descriptors but rather parameter length (in bytes).
   This is flagged by -1 in num_descs_inc (third) field */
static struct sdparm_mode_descriptor_t smc_tg_desc = {
    1, 1, -1, 2, 2, -1, -1, "SMC transport geometry"
};

/* SBC's logical block provisioning mode page doesn't give the number
   of following descriptors but rather parameter length (in bytes).
   This is flagged by -1 in num_descs_inc (third) field */
static struct sdparm_mode_descriptor_t sbc_lbp_desc = {
    2, 2, -1, 16, 8, -1, -1, "SBC logical block provisioning"
};
/* SBC's application tag mode page doesn't give the number of
   following descriptors but rather parameter length (in bytes).
   This is flagged by -1 in num_descs_inc (third) field */
static struct sdparm_mode_descriptor_t sbc_atag_desc = {
    2, 2, -1, 16, 24, -1, -1, "SBC application tag"
};

/* Mode pages that aren't specific to any transport protocol or vendor.
   Note that all standard periperal device types are include.
   The pages are listed in acronym alphabetical order. */
struct sdparm_mode_page_t sdparm_gen_mode_pg[] = {
    {ADC_MP, MSP_ADC_DT_DPP, PDT_ADC, 0, "addp",
        "DT device primary port (ADC)", NULL},
    {ADC_MP, MSP_ADC_LU, PDT_ADC, 0, "adlu", "logical unit (ADC)", NULL},
    {ADC_MP, MSP_ADC_TGT_DEV, PDT_ADC, 0, "adtd", "Targer device (ADC)",
        NULL},
    {ADC_MP, MSP_ADC_TD_SN, PDT_ADC, 0, "adts",
        "Targer device serial number (ADC)", NULL},
    {POWER_MP, MSP_SAT_POWER, -1, 0, "apo", "SAT ATA Power condition", NULL},
    {CONTROL_MP, MSP_SBC_APP_TAG, PDT_DISK, 0, "atag", "Application tag "
        "(SBC)", &sbc_atag_desc},
    {IEC_MP, MSP_BACK_CTL, PDT_DISK, 0, "bc", "Background control (SBC)",
        NULL},
    {CACHING_MP, 0, PDT_DISK, 0, "ca", "Caching (SBC)", NULL},
    {MMCMS_MP, 0, PDT_MMC, 1, "cms", "CD/DVD (MM) capabilities and "
        "mechanical status (MMC)", NULL},        /* read only */
    {CONTROL_MP, 0, -1, 0, "co", "Control", NULL},
    {CONTROL_MP, MSP_SPC_CE, -1, 0, "coe", "Control extension", NULL},
    {CONTROL_MP, MSP_SSC_CDP, -1, 0, "cdp", "Control data protection (SSC)",
        NULL},
    {DATA_COMPR_MP, 0, PDT_TAPE, 0, "dac", "Data compression (SSC)", NULL},
    {DEV_CONF_MP, 0, PDT_TAPE, 0, "dc", "Device configuration (SSC)", NULL},
    {DEV_CAP_MP, 0, PDT_MCHANGER, 0, "dca", "Device capabilities (SMC)",
        NULL},
    {DEV_CONF_MP, MSP_DEV_CONF_EXT, PDT_TAPE, 0, "dce",
        "Device configuration extension (SSC)", NULL},
    {DISCONNECT_MP, 0, -1, 0, "dr",
        "Disconnect-reconnect (SPC + transports)", NULL},
    {ELE_ADDR_ASS_MP, 0, PDT_MCHANGER, 0, "eaa",
        "Element address assignment (SMC)", NULL},
    {DEV_CAP_MP, MSP_EXT_DEV_CAP, PDT_MCHANGER, 0, "edc",
        "Extended device capabilities (SMC)", NULL},
    {ES_MAN_MP, 0, PDT_SES, 0, "esm", "Enclosure services management (SES)",
        NULL},
    {FORMAT_MP, 0, PDT_DISK, 0, "fo", "Format (SBC)", NULL},
    {IEC_MP, 0, -1, 0, "ie", "Informational exceptions control", NULL},
    {IEC_MP, MSP_SBC_LB_PROV, PDT_DISK, 0, "lbp", "Logical block "
        "provisioning (SBC)", &sbc_lbp_desc},
    {MED_CONF_MP, 0, PDT_TAPE, 0, "mco", "Medium configuration (SSC)", NULL},
    {MED_PART_MP, 0, PDT_TAPE, 0, "mpa", "Medium partition (SSC)",
        &ssc_mpa_desc},
    {MRW_MP, 0, PDT_MMC, 0, "mrw", "Mount rainier reWritable (MMC)", NULL},
    {CONTROL_MP, MSP_SAT_PATA, -1, 0, "pat", "SAT pATA control", NULL},
    {PROT_SPEC_LU_MP, 0, -1, 0, "pl", "Protocol specific logical unit", NULL},
    {POWER_MP, 0, -1, 0, "po", "Power condition", NULL},
    {POWER_MP, MSP_SPC_PS, -1, 0, "ps", "Power consumption", NULL},
    {POWER_OLD_MP, 0, PDT_DISK, 0, "poo", "Power condition - old version",
        NULL}, /* POWER_OLD_MP for disks as it clashes with old MMC specs */
    {PROT_SPEC_PORT_MP, 0, -1, 0, "pp", "Protocol specific port", NULL},
    {RBC_DEV_PARAM_MP, 0, PDT_RBC, 0, "rbc", "RBC device parameters (RBC)",
        NULL},
    {RIGID_DISK_MP, 0, PDT_DISK, 0, "rd", "Rigid disk (SBC)", NULL},
    {RW_ERR_RECOVERY_MP, 0, -1, 0, "rw", "Read write error recovery", NULL},
        /* since in SBC, SSC and MMC treat RW_ERR_RECOVERY_MP as if in SPC */
    {TRANS_GEO_PAR_MP, 0, PDT_MCHANGER, 0, "tgp", "Transport geometry "
        "parameters (SMC)", &smc_tg_desc},
    {TIMEOUT_PROT_MP, 0, PDT_MMC, 0, "tp", "Timeout and protect (MMC)", NULL},
    {V_ERR_RECOVERY_MP, 0, PDT_DISK, 0, "ve", "Verify error recovery (SBC)",
        NULL},
    {WRITE_PARAM_MP, 0, PDT_MMC, 0, "wp", "Write parameters (MMC)", NULL},
    {XOR_MP, 0, PDT_DISK, 0, "xo", "XOR control (SBC)", NULL},
        /* XOR control mode page made obsolete in sbc3r32 */
    {0, 0, 0, 0, NULL, NULL, NULL},
};

/* Array for transport id, acronym and name association. */
/* Those transports commented with "none" don't have transport specific */
/* mode pages. */
struct sdparm_transport_id_t sdparm_transport_id[] = {
    {TPROTO_FCP, "fcp", "Fibre channel (FCP)"},
    {TPROTO_SPI, "spi", "SCSI parallel interface (SPI)"},
    {TPROTO_SSA, "ssa", "Serial storage architecture (SSA)"},
    {TPROTO_1394, "sbp", "Serial bus (SBP)"}, /* none */
    {TPROTO_SRP, "srp", "SCSI remote DMA (SRP)"},
    {TPROTO_ISCSI, "iscsi", "Internet SCSI (iSCSI)"}, /* none */
    {TPROTO_SAS, "sas", "Serial attached SCSI (SAS, SPL) "},
    {TPROTO_ADT, "adt", "Automation/Drive interface (ADT)"},
    {TPROTO_ATA, "ata", "AT attachment interface (ATA, ACS)"}, /* none */
    {TPROTO_UAS, "uas", "USB attached SCSI (UAS)"}, /* none */
    {TPROTO_SOP, "sop", "SCSI over PCIe (SOP)"}, /* none */
    {0x9, "u0xb", NULL},      /* leading "u" so not number */
    {0xc, "u0xc", NULL},
    {0xd, "u0xd", NULL},
    {0xe, "u0xe", NULL},
    {TPROTO_NONE, "none", "No specific"},
    {0, NULL, NULL},
};

static struct sdparm_mode_page_t sdparm_fcp_mode_pg[] = {    /* FCP-3 */
    {DISCONNECT_MP, 0, -1, 0, "dr", "Disconnect-reconnect (FCP)", NULL},
    {PROT_SPEC_LU_MP, 0, -1, 0, "luc", "lu: control (FCP)", NULL},
    {PROT_SPEC_PORT_MP, 0, -1, 0, "pc", "port: control (FCP)", NULL},
    {PROT_SPEC_LU_MP, 0, -1, 0, "pl", "lu: control (generic name)", NULL},
    {PROT_SPEC_PORT_MP, 0, -1, 0, "pp", "port: control (generic name)", NULL},
    {0, 0, 0, 0, NULL, NULL, NULL},
};

static struct sdparm_mode_page_t sdparm_spi_mode_pg[] = {    /* SPI-4 */
    {DISCONNECT_MP, 0, -1, 0, "dr", "Disconnect-reconnect (SPI)", NULL},
    {PROT_SPEC_LU_MP, 0, -1, 0, "luc", "lu: control (SPI)", NULL},
    {PROT_SPEC_PORT_MP, MSP_SPI_MC, -1, 0, "mc",
        "port: margin control (SPI)", NULL},
    {PROT_SPEC_PORT_MP, MSP_SPI_NS, -1, 0, "ns",
        "port: negotiated settings (SPI)", NULL},
    {PROT_SPEC_PORT_MP, 0, -1, 0, "psf", "port: short format (SPI)", NULL},
    {PROT_SPEC_PORT_MP, MSP_SPI_RTC, -1, 1, "rtc",
        "port: report transfer capabilities (SPI)", NULL},
    {PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 0, "stc",
        "port: saved training config value (SPI)", NULL},
    /* second preference name so put out of alphabetical order */
    {PROT_SPEC_LU_MP, 0, -1, 0, "pl", "lu: control (generic name)", NULL},
    {PROT_SPEC_PORT_MP, 0, -1, 0, "pp", "port: short format (generic name)",
        NULL},
    {0, 0, 0, 0, NULL, NULL, NULL},
};

static struct sdparm_mode_page_t sdparm_srp_mode_pg[] = {    /* SRP */
    {DISCONNECT_MP, 0, -1, 0, "dr", "Disconnect-reconnect (SRP)", NULL},
    {0, 0, 0, 0, NULL, NULL, NULL},
};

static struct sdparm_mode_descriptor_t sas_pcd_desc = {   /* desc SAS/SPL */
    7, 1, 0, 8, 48, -1, -1, "SAS phy"
};

static struct sdparm_mode_descriptor_t sas_e_phy_desc = {  /* desc SAS/SPL */
    7, 1, 0, 8, -1, 2, 2, "Enhanced phy"
};

/* N.B. In SAS 2.1 the spec was split with the upper levels going into the
 * SAS Protocol Layer (SPL) document. So now the SPL drafts are the
 * relevant SAS references. */
static struct sdparm_mode_page_t sdparm_sas_mode_pg[] = {    /* SAS/SPL */
    {DISCONNECT_MP, 0, -1, 0, "dr", "Disconnect-reconnect (SAS)", NULL},
    {PROT_SPEC_LU_MP, 0, -1, 0, "pl", "Protocol specific logical unit (SAS)",
        NULL},
    {PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 0, "pcd",
        "Phy control and discover (SAS)", &sas_pcd_desc},
    {PROT_SPEC_PORT_MP, 0, -1, 0, "pp", "Protocol specific port (SAS)", NULL},
    {PROT_SPEC_PORT_MP, MSP_SAS_E_PHY, -1, 0, "sep",
        "Enhanced phy control (SAS)", &sas_e_phy_desc},
    {PROT_SPEC_PORT_MP, MSP_SAS_SPC, -1, 0, "spc",
        "Shared port control (SAS)", NULL},
    {0, 0, 0, 0, NULL, NULL, NULL},
};


/* These VPD pages are listed in alphabetical order based on their
 * 'acron' field. The standard inquiry response is added to this list. */
struct sdparm_vpd_page_t sdparm_vpd_pg[] = {
    {VPD_ATA_INFO, 0, -1, "ai", "ATA information (SAT)"},
    {VPD_ASCII_OP_DEF, 0, -1, "aod",
     "ASCII implemented operating definition (obs)"},
    {VPD_AUTOMATION_DEV_SN, 0, PDT_TAPE, "adsn", "Automation device serial "
     "number (SSC)"},
    {VPD_BLOCK_LIMITS, 0, PDT_DISK, "bl", "Block limits (SBC)"},
    {VPD_BLOCK_DEV_CHARS, 0, PDT_DISK, "bdc", "Block device characteristics "
     "(SBC)"},
    {VPD_CFA_PROFILE_INFO, 0, -1, "cfa", "CFA profile information"},
    {VPD_DEVICE_CONSTITUENTS, 0, -1, "dc", "Device constituents"},
    {VPD_DEVICE_ID, 0, -1, "di", "Device identification"},
    {VPD_DEVICE_ID, VPD_DI_SEL_AS_IS, -1, "di_asis", "Like 'di' "
     "but designators ordered as found"},
    {VPD_DEVICE_ID, VPD_DI_SEL_LU, -1, "di_lu", "Device identification, "
     "lu only"},
    {VPD_DEVICE_ID, VPD_DI_SEL_TPORT, -1, "di_port", "Device "
     "identification, target port only"},
    {VPD_DEVICE_ID, VPD_DI_SEL_TARGET, -1, "di_target", "Device "
     "identification, target device only"},
    {VPD_EXT_INQ, 0, -1, "ei", "Extended inquiry data"},
    {VPD_DTDE_ADDRESS, 0, 1, "dtde",
     "Data transfer device element address (SSC)"},
    {VPD_IMP_OP_DEF, 0, -1, "iod",
     "Implemented operating definition (obs)"},
    {VPD_LB_PROVISIONING, 0, PDT_DISK, "lbpv", "Logical block provisioning "
     "(SBC)"},
    {VPD_MAN_ASS_SN, 0, PDT_TAPE, "mas",
     "Manufacturer assigned serial number (SSC)"},
    {VPD_MAN_ASS_SN, 0, PDT_ADC, "masa",
     "Manufacturer assigned serial number (ADC)"},
    {VPD_MAN_NET_ADDR, 0, -1, "mna", "Management network addresses"},
    {VPD_MODE_PG_POLICY, 0, -1, "mpp", "Mode page policy"},
    {VPD_OSD_INFO, 0, PDT_OSD, "oi", "OSD information"},
    {VPD_POWER_CONDITION, 0, -1, "pc", "Power condition"},
    {VPD_POWER_CONSUMPTION, 0, -1, "psm", "Power consumption"},
    {VPD_PROTO_LU, 0, -1, "pslu", "Protocol-specific logical unit "
     "information"},
    {VPD_PROTO_PORT, 0, -1, "pspo", "Protocol-specific port information"},
    {VPD_REFERRALS, 0, PDT_DISK, "ref", "Referrals (SBC)"},
    {VPD_SA_DEV_CAP, 0, PDT_TAPE, "sad",
     "Sequential access device capabilities (SSC)"},
    {VPD_SOFTW_INF_ID, 0, -1, "sii", "Software interface identification"},
    {VPD_NOT_STD_INQ, 0, -1, "sinq", "Standard inquiry response"},
    {VPD_UNIT_SERIAL_NUM, 0, -1, "sn", "Unit serial number"},
    {VPD_SCSI_PORTS, 0, -1, "sp", "SCSI ports"},
    {VPD_SUPPORTED_VPDS, 0, -1, "sv", "Supported VPD pages"},
    {VPD_TA_SUPPORTED, 0, PDT_TAPE, "tas", "TapeAlert supported flags (SSC)"},
    {VPD_3PARTY_COPY, 0, -1, "tpc", "Third party copy"},
    {0, 0, 0, NULL, NULL},
};

/* Generic (i.e. non-transport specific) mode page items follow, */
/* sorted by mode page (then subpage) number in ascending order */
struct sdparm_mode_page_item sdparm_mitem_arr[] = {

    /* Read write error recovery mode page [0x1] sbc2, mmc5, ssc3 */
    /* treat as spc since various command sets implement variants */
    {"AWRE", RW_ERR_RECOVERY_MP, 0, -1, 2, 7, 1, MF_COMMON,
        "Automatic write reallocation enabled", NULL},
    {"ARRE", RW_ERR_RECOVERY_MP, 0, -1, 2, 6, 1, MF_COMMON,
        "Automatic read reallocation enabled", NULL},
    {"TB", RW_ERR_RECOVERY_MP, 0, -1, 2, 5, 1, 0,
        "Transfer block", NULL},
    {"RC", RW_ERR_RECOVERY_MP, 0, -1, 2, 4, 1, 0,
        "Read continuous", "0: error recovery may cause delays\t"
        "1: transfer data without waiting for error recovery"},
    {"EER", RW_ERR_RECOVERY_MP, 0, -1, 2, 3, 1, 0,
        "Enable early recovery",
        "1: increase chance of mis-detection or mis-correction of error"},
    {"PER", RW_ERR_RECOVERY_MP, 0, -1, 2, 2, 1, MF_COMMON,
        "Post error", "0: do not post recovered errors\t"
        "1: report recovered errors (via sense key: recovered error)"},
    {"DTE", RW_ERR_RECOVERY_MP, 0, -1, 2, 1, 1, 0,
        "Data terminate on error",
        "1: terminate data transfer when recovered error detected"},
    {"DCR", RW_ERR_RECOVERY_MP, 0, -1, 2, 0, 1, 0,
        "Disable correction", NULL},
    {"RRC", RW_ERR_RECOVERY_MP, 0, -1, 3, 7, 8, 0,
        "Read retry count", NULL},
    {"COR_S", RW_ERR_RECOVERY_MP, 0, -1, 4, 7, 8, 0,
        "Correction span (obsolete)", NULL},
    {"HOC", RW_ERR_RECOVERY_MP, 0, -1, 5, 7, 8, 0,
        "Head offset count (obsolete)", NULL},
    {"DSOC", RW_ERR_RECOVERY_MP, 0, -1, 6, 7, 8, 0,
        "Data strobe offset count (obsolete)", NULL},
    {"LBPERE", RW_ERR_RECOVERY_MP, 0, 0, 7, 7, 1, 0, /* SBC */
        "Logical block provisioning error reporting enabled", NULL},
    {"EMCDR", RW_ERR_RECOVERY_MP, 0, 5, 7, 1, 2, 0, /* MMC */
        "Enhanced media certification and defect reporting", NULL},
    {"WRC", RW_ERR_RECOVERY_MP, 0, -1, 8, 7, 8, 0,
        "Write retry count", NULL},
    {"ERWS", RW_ERR_RECOVERY_MP, 0, 5, 9, 7, 24, 0, /* MMC, was ERTL */
        "Error reporting window size (blocks)", NULL},
    {"RTL", RW_ERR_RECOVERY_MP, 0, 0, 10, 7, 16, 0, /* SBC */
        "Recovery time limit (ms)", "0: default, -1: 65.5 seconds"},

    /* Disconnect-reconnect mode page [0x2]: spc-4 + */
    /* See transport sections for more detailed information */
    {"BFR", DISCONNECT_MP, 0, -1, 2, 7, 8, 0,
        "Buffer full ratio",
        "fraction where this value is numerator, 256 is denominator"},
    {"BER", DISCONNECT_MP, 0, -1, 3, 7, 8, 0,
        "Buffer empty ratio",
        "fraction where this value is numerator, 256 is denominator"},
    {"BIL", DISCONNECT_MP, 0, -1, 4, 7, 16, 0,
        "Bus inactivity limit", "for unit see specific transport"},
    {"DTL", DISCONNECT_MP, 0, -1, 6, 7, 16, 0,
        "Disconnect time limit", "for unit see specific transport"},
    {"CTL", DISCONNECT_MP, 0, -1, 8, 7, 16, 0,
        "Connect time limit", "for unit see specific transport"},
    {"MBS", DISCONNECT_MP, 0, -1, 10, 7, 16, 0,
        "Maximum burst size (512 bytes)", NULL},
    {"EMDP", DISCONNECT_MP, 0, -1, 12, 7, 1, 0,
        "Enable modify data pointers",
        "1: target may send data out of order"},
    {"FA", DISCONNECT_MP, 0, -1, 12, 6, 3, 0,
        "Fair arbitration", NULL},
    {"DIMM", DISCONNECT_MP, 0, -1, 12, 3, 1, 0,
        "Disconnect immediate", NULL},
    {"DTDC", DISCONNECT_MP, 0, -1, 12, 2, 3, 0,
        "Data transfer disconnect control", NULL},
    {"FBS", DISCONNECT_MP, 0, -1, 14, 7, 16, 0,
        "First burst size (512 bytes)", NULL},

    /* Format mode page [0x3] sbc2 (obsolete) */
    {"TPZ", FORMAT_MP, 0, PDT_DISK, 2, 7, 16, 0,
        "Tracks per zone", NULL},
    {"ASPZ", FORMAT_MP, 0, PDT_DISK, 4, 7, 16, 0,
        "Alternate sectors per zone", NULL},
    {"ATPZ", FORMAT_MP, 0, PDT_DISK, 6, 7, 16, 0,
        "Alternate tracks per zone", NULL},
    {"ATPLU", FORMAT_MP, 0, PDT_DISK, 8, 7, 16, 0,
        "Alternate tracks per logical unit", NULL},
    {"SPT", FORMAT_MP, 0, PDT_DISK, 10, 7, 16, 0,
        "Sectors per track", NULL},
    {"DBPPS", FORMAT_MP, 0, PDT_DISK, 12, 7, 16, 0,
        "Data bytes per physical sector", NULL},
    {"INTLV", FORMAT_MP, 0, PDT_DISK, 14, 7, 16, 0,
        "Interleave", NULL},
    {"TSF", FORMAT_MP, 0, PDT_DISK, 16, 7, 16, 0,
        "Track skew factor", NULL},
    {"CSF", FORMAT_MP, 0, PDT_DISK, 18, 7, 16, 0,
        "Cylinder skew factor", NULL},
    {"SSEC", FORMAT_MP, 0, PDT_DISK, 20, 7, 1, 0,
        "Soft sector", NULL},
    {"HSEC", FORMAT_MP, 0, PDT_DISK, 20, 6, 1, 0,
        "Hard sector", NULL},
    {"RMB", FORMAT_MP, 0, PDT_DISK, 20, 5, 1, 0,
        "Removable", NULL},
    {"SURF", FORMAT_MP, 0, PDT_DISK, 20, 4, 1, 0,
        "Surface", NULL},

    /* Mount Rainier reWitable mode page [0x3] mmc4  */
    {"LBAS", MRW_MP, 0, PDT_MMC, 3, 0, 1, 0,
        "LBA space", NULL},

    /* Rigid disk mode page [0x4] sbc2 (obsolete) */
    {"NOC", RIGID_DISK_MP, 0, PDT_DISK, 2, 7, 24, 0,
        "Number of cylinders", NULL},
    {"NOH", RIGID_DISK_MP, 0, PDT_DISK, 5, 7, 8, 0,
        "Number of heads", NULL},
    {"SCWP", RIGID_DISK_MP, 0, PDT_DISK, 6, 7, 24, 0,
        "Starting cylinder for write precompensation", NULL},
    {"SCRWC", RIGID_DISK_MP, 0, PDT_DISK, 9, 7, 24, 0,
        "Starting cylinder for reduced write current", NULL},
    {"DSR", RIGID_DISK_MP, 0, PDT_DISK, 12, 7, 16, 0,
        "Device step rate", NULL},
    {"LZC", RIGID_DISK_MP, 0, PDT_DISK, 14, 7, 24, 0,
        "Landing zone cylinder", NULL},
    {"RPL", RIGID_DISK_MP, 0, PDT_DISK, 17, 1, 2, 0,
        "Rotational position locking", NULL},
    {"ROTO", RIGID_DISK_MP, 0, PDT_DISK, 18, 7, 8, 0,
        "Rotational offset", NULL},
    {"MRR", RIGID_DISK_MP, 0, PDT_DISK, 20, 7, 16, 0,
        "Medium rotation rate (rpm)", NULL},

    /* Write parameters mode page [0x5] mmc5 */
    {"BUFE", WRITE_PARAM_MP, 0, PDT_MMC, 2, 6, 1, MF_COMMON,
        "Buffer underrun free recording enable", NULL},
    {"LS_V", WRITE_PARAM_MP, 0, PDT_MMC, 2, 5, 1, 0,
        "Link size valid", NULL},
    {"TST_W", WRITE_PARAM_MP, 0, PDT_MMC, 2, 4, 1, 0,
        "Test write", NULL},
    {"WR_T", WRITE_PARAM_MP, 0, PDT_MMC, 2, 3, 4, MF_COMMON,
        "Write type",
        "0: packet/incremental; 1: track-at-once\t"
        "2: session-at-once; 3: raw; 4: layer jump recording"},
    {"MULTI_S", WRITE_PARAM_MP, 0, PDT_MMC, 3, 7, 2, MF_COMMON,
        "Multi session",
        "0: next session not allowed (no BO pointer)\t"
        "1: next session not allowed\t"
        "3: next seesion allowed (indicated by BO pointer)"},
    {"FP", WRITE_PARAM_MP, 0, PDT_MMC, 3, 5, 1, 0,
        "Fixed packet type", NULL},
    {"COPY", WRITE_PARAM_MP, 0, PDT_MMC, 3, 4, 1, 0,
        "Serial copy management system (SCMS) enable", NULL},
    {"TRACK_M", WRITE_PARAM_MP, 0, PDT_MMC, 3, 3, 4, 0,
        "Track mode", NULL},
    {"DBT", WRITE_PARAM_MP, 0, PDT_MMC, 4, 3, 4, 0,
        "Data block type", NULL},
    {"LINK_S", WRITE_PARAM_MP, 0, PDT_MMC, 5, 7, 8, 0,
        "Link size", NULL},
    {"IAC", WRITE_PARAM_MP, 0, PDT_MMC, 7, 5, 6, 0,
        "Initiator application code", NULL},
    {"SESS_F", WRITE_PARAM_MP, 0, PDT_MMC, 8, 7, 8, 0,
        "Session format", NULL},
    {"PACK_S", WRITE_PARAM_MP, 0, PDT_MMC, 10, 7, 32, 0,
        "Packet size", NULL},
    {"APL", WRITE_PARAM_MP, 0, PDT_MMC, 14, 7, 16, 0,
        "Audio pause length (blocks)", NULL},

    /* Device parameters mode page [0x6] rbc */
    {"WCD", RBC_DEV_PARAM_MP, 0, PDT_RBC, 2, 0, 1, MF_COMMON,
        "Write cache disable", NULL},
    {"LBS", RBC_DEV_PARAM_MP, 0, PDT_RBC, 3, 7, 16, MF_COMMON,
        "Logical block size", NULL},
    {"NLBS", RBC_DEV_PARAM_MP, 0, PDT_RBC, 5, 7, 40, (MF_COMMON | MF_HEX),
        "Number of logical blocks", NULL},
    {"P_P", RBC_DEV_PARAM_MP, 0, PDT_RBC, 10, 7, 8, 0,
        "Power/performance", NULL},
    {"READD", RBC_DEV_PARAM_MP, 0, PDT_RBC, 11, 3, 1, 0,
        "Read disable", NULL},
    {"WRITED", RBC_DEV_PARAM_MP, 0, PDT_RBC, 11, 2, 1, 0,
        "Write disable", NULL},
    {"FORMATD", RBC_DEV_PARAM_MP, 0, PDT_RBC, 11, 1, 1, 0,
        "Format disable", NULL},
    {"LOCKD", RBC_DEV_PARAM_MP, 0, PDT_RBC, 11, 0, 1, 0,
        "Lock disable", NULL},

    /* Verify error recovery mode page [0x7] sbc2 */
    {"V_EER", V_ERR_RECOVERY_MP, 0, PDT_DISK, 2, 3, 1, 0,
        "Enable early recovery", NULL},
    {"V_PER", V_ERR_RECOVERY_MP, 0, PDT_DISK, 2, 2, 1, 0,
        "Post error", NULL},
    {"V_DTE", V_ERR_RECOVERY_MP, 0, PDT_DISK, 2, 1, 1, 0,
        "Data terminate on error", NULL},
    {"V_DCR", V_ERR_RECOVERY_MP, 0, PDT_DISK, 2, 0, 1, 0,
        "Disable correction", NULL},
    {"V_RC", V_ERR_RECOVERY_MP, 0, PDT_DISK, 3, 7, 8, 0,
        "Verify retry count", NULL},
    {"V_COR_S", V_ERR_RECOVERY_MP, 0, PDT_DISK, 4, 7, 8, 0,
        "Verify correction span (obsolete)", NULL},
    {"V_RTL", V_ERR_RECOVERY_MP, 0, PDT_DISK, 10, 7, 16, 0,
        "Verify recovery time limit (ms)", NULL},

    /* Caching mode page [0x8] sbc2 */
    {"IC", CACHING_MP, 0, PDT_DISK, 2, 7, 1, 0,
        "Initiator control",
        "0: disk uses own adaptive caching algorithm\t"
        "1: disk caching algorithm controlled by NCS or CCS"},
    {"ABPF", CACHING_MP, 0, PDT_DISK, 2, 6, 1, 0,
        "Abort pre-fetch", NULL},
    {"CAP", CACHING_MP, 0, PDT_DISK, 2, 5, 1, 0,
        "Caching analysis permitted", NULL},
    {"DISC", CACHING_MP, 0, PDT_DISK, 2, 4, 1, 0,
        "Discontinuity",
        "0: pre-fetch truncated or wrapped at time discontinuity\t"
        "1: pre-fetch continues across time discontinuity"},
    {"SIZE", CACHING_MP, 0, PDT_DISK, 2, 3, 1, 0,
        "Size enable",
        "0: number of cache segments (NCS) controls cache segmentation\t"
        "1: the cache segment size (CCS) controls cache segmentation"},
    {"WCE", CACHING_MP, 0, PDT_DISK, 2, 2, 1, MF_COMMON,
        "Write cache enable", NULL},
    {"MF", CACHING_MP, 0, PDT_DISK, 2, 1, 1, 0,
        "Multiplication factor",
        "0: MIPF and MAPF specify blocks\t"
        "1: multiply MIPF and MAPF by blocks in read command"},
    {"RCD", CACHING_MP, 0, PDT_DISK, 2, 0, 1, MF_COMMON,
        "Read cache disable", NULL},
    {"DRRP", CACHING_MP, 0, PDT_DISK, 3, 7, 4, 0,
        "Demand read retention priority",
        "0: treat requested and other data equally\t"
        "1: replace requested data before other data\t"
        "15: replace other data before requested data"},
    {"WRP", CACHING_MP, 0, PDT_DISK, 3, 3, 4, 0,
        "Write retention priority",
        "0: treat requested and other data equally\t"
        "1: replace requested data before other data\t"
        "15: replace other data before requested data"},
    {"DPTL", CACHING_MP, 0, PDT_DISK, 4, 7, 16, 0,
        "Disable pre-fetch transfer length", NULL},
    {"MIPF", CACHING_MP, 0, PDT_DISK, 6, 7, 16, 0,
        "Minimum pre-fetch", NULL},
    {"MAPF", CACHING_MP, 0, PDT_DISK, 8, 7, 16, 0,
        "Maximum pre-fetch", NULL},
    {"MAPFC", CACHING_MP, 0, PDT_DISK, 10, 7, 16, 0,
        "Maximum pre-fetch ceiling", NULL},
    {"FSW", CACHING_MP, 0, PDT_DISK, 12, 7, 1, 0,
        "Force sequential write", NULL},
    {"LBCSS", CACHING_MP, 0, PDT_DISK, 12, 6, 1, 0,
        "Logical block cache segment size",
        "0: CSS unit is bytes; 1: CSS unit is blocks"},
    {"DRA", CACHING_MP, 0, PDT_DISK, 12, 5, 1, 0,
        "Disable read ahead", NULL},
    {"SYNC_PROG", CACHING_MP, 0, PDT_DISK, 12, 2, 2, 0, /* sbc3r33 */
        "Synchronous cache progress indication",
        "0: no pollable sense data during sync\t"
        "1: allow pollable sense data, allow all commands during sync\t"
        "2: allow pollable sense data, allow some commands during sync"},
    {"NV_DIS", CACHING_MP, 0, PDT_DISK, 12, 0, 1, 0,
        "Non-volatile cache disable", NULL},
    {"NCS", CACHING_MP, 0, PDT_DISK, 13, 7, 8, 0,
        "Number of cache segments", NULL},
    {"CSS", CACHING_MP, 0, PDT_DISK, 14, 7, 16, 0,
        "Cache segment size", NULL},

    /* Control mode page [0xa] spc3 */
    {"TST", CONTROL_MP, 0, -1, 2, 7, 3, 0,
        "Task set type",
        "0: lu maintains one task set for all I_T nexuses\t"
        "1: lu maintains separate task sets for each I_T nexus"},
    {"TMF_ONLY", CONTROL_MP, 0, -1, 2, 4, 1, 0,
        "Task management functions only", NULL},
    {"DPICZ", CONTROL_MP, 0, -1, 2, 3, 1, 0,
        "Disable protection information check if protect field zero", NULL},
    {"D_SENSE", CONTROL_MP, 0, -1, 2, 2, 1, 0,
        "Descriptor format sense data", NULL},
    {"GLTSD", CONTROL_MP, 0, -1, 2, 1, 1, 0,
        "Global logging target save disable", NULL},
    {"RLEC", CONTROL_MP, 0, -1, 2, 0, 1, 0,
        "Report log exception condition", NULL},
    {"QAM", CONTROL_MP, 0, -1, 3, 7, 4, 0,
        "Queue algorithm modifier",
        "0: restricted re-ordering; 1: unrestricted"},
    {"NUAR", CONTROL_MP, 0, -1, 3, 3, 1, 0,
        "No unit attention on release", NULL},
    {"QERR", CONTROL_MP, 0, -1, 3, 2, 2, 0,
        "Queue error management",
        "0: only affected task gets CC; 1: affected tasks aborted\t"
        "3: affected tasks aborted on same I_T nexus"},
    {"RAC", CONTROL_MP, 0, -1, 4, 6, 1, 0,
        "Report a check", NULL},
    {"UA_INTLCK", CONTROL_MP, 0, -1, 4, 5, 2, 0,
        "Unit attention interlocks control",
        "0: unit attention cleared with check condition status\t"
        "2: unit attention not cleared with check condition status\t"
        "3: as 2 plus ua on busy, task set full or reservation conflict"},
    {"SWP", CONTROL_MP, 0, -1, 4, 3, 1, MF_COMMON,
        "Software write protect", NULL},
    {"ATO", CONTROL_MP, 0, -1, 5, 7, 1, 0,
        "Application tag owner", NULL},
    {"TAS", CONTROL_MP, 0, -1, 5, 6, 1, 0,
        "Task aborted status",
        "0: tasks aborted without response to app client\t"
        "1: any other I_T nexuses receive task aborted"},
    {"ATMPE", CONTROL_MP, 0, -1, 5, 5, 1, 0,    /* spc4r27 */
        "Application tag mode page enabled", NULL},
    {"RWWP", CONTROL_MP, 0, -1, 5, 4, 1, 0,     /* spc4r27 */
        "Reject write without protection", NULL},
    {"AUTOLOAD", CONTROL_MP, 0, -1, 5, 2, 3, 0,
        "Autoload mode",
        "0: medium loaded for full access\t"
        "1: loaded for medium auxiliary access only\t"
        "2: medium shall not be loaded"},
    {"BTP", CONTROL_MP, 0, -1, 8, 7, 16, 0,
        "Busy timeout period (100us)",
        "0: undefined\t"
        "0ffffh (-1): unlimited"},
    {"ESTCT", CONTROL_MP, 0, -1, 10, 7, 16, 0,
        "Extended self test completion time (sec)",
        "0ffffh (-1) takes 65535 seconds or longer"},

    /* Control extension mode subpage [0xa,0x1] spc3 */
    {"TCMOS", CONTROL_MP, MSP_SPC_CE, -1, 4, 2, 1, 0,
        "Timestamp changeable by methods outside standard", NULL},
    {"SCSIP", CONTROL_MP, MSP_SPC_CE, -1, 4, 1, 1, 0,
        "SCSI timestamp commands take precedence over other methods", NULL},
    {"IALUAE", CONTROL_MP, MSP_SPC_CE, -1, 4, 0, 1, 0,
        "Implicit asymmetric logical unit access enabled", NULL},
    {"INIT_PR", CONTROL_MP, MSP_SPC_CE, -1, 5, 3, 4, 0,
        "Initial command priority", "0: none or vendor\t"
        "1: highest\t15: lowest"},
    {"MSDL", CONTROL_MP, MSP_SPC_CE, -1, 6, 7, 8, 0,  /* spc4r34 */
        "Maximum sense data length", "0: unlimited"},

    /* Application tag mode subpage [0xa,0xf0] sbc3 */
    /* descriptor starts here, <start_byte> is relative to start of mode
     * page (i.e. 16 more than shown in descriptor format table) */
    {"AT_LAST", CONTROL_MP, MSP_SBC_APP_TAG, PDT_DISK, 16, 7, 1, 0,
        "Last", NULL},
    {"AT_LBAT", CONTROL_MP, MSP_SBC_APP_TAG, PDT_DISK, 22, 7, 16, 0,
        "Logical block application tag", NULL},
    {"AT_LBA", CONTROL_MP, MSP_SBC_APP_TAG, PDT_DISK, 24, 7, 64, MF_HEX,
        "Logical block address", NULL},
    {"AT_COUNT", CONTROL_MP, MSP_SBC_APP_TAG, PDT_DISK, 32, 7, 64, MF_HEX,
        "Logical block count", NULL},

    /* Control data protection mode subpage [0xa,0xf0] ssc4 */
    {"LBPM", CONTROL_MP, MSP_SSC_CDP, PDT_TAPE, 4, 7, 8, 0,
        "Logical block protection method", "0: none\t"
        "1: Reed-Solomon CRC"},
    {"LBPIL", CONTROL_MP, MSP_SSC_CDP, PDT_TAPE, 5, 5, 6, 0,
        "Logical block protection information length", NULL},
    {"LBP_W", CONTROL_MP, MSP_SSC_CDP, PDT_TAPE, 6, 7, 1, 0,
        "Logical block protection during write", NULL},
    {"LBP_R", CONTROL_MP, MSP_SSC_CDP, PDT_TAPE, 6, 6, 1, 0,
        "Logical block protection during read", NULL},
    {"RBDP", CONTROL_MP, MSP_SSC_CDP, PDT_TAPE, 6, 5, 1, 0,
        "Recover buffered data protected", NULL},

    /* SAT: pATA control mode subpage [0xa,0xf1] sat-r09 */
    /* treat as spc since could be disk or ATAPI */
    {"MWD2", CONTROL_MP, MSP_SAT_PATA, -1, 4, 6, 1, 0,
        "Multi word DMA bit 2", NULL},
    {"MWD1", CONTROL_MP, MSP_SAT_PATA, -1, 4, 5, 1, 0,
        "Multi word DMA bit 1", NULL},
    {"MWD0", CONTROL_MP, MSP_SAT_PATA, -1, 4, 4, 1, 0,
        "Multi word DMA bit 0", NULL},
    {"PIO4", CONTROL_MP, MSP_SAT_PATA, -1, 4, 1, 1, 0,
        "Parallel IO bit 4", NULL},
    {"PIO3", CONTROL_MP, MSP_SAT_PATA, -1, 4, 0, 1, 0,
        "Parallel IO bit 3", NULL},
    {"UDMA6", CONTROL_MP, MSP_SAT_PATA, -1, 5, 6, 1, 0,
        "Ultra DMA bit 6", NULL},
    {"UDMA5", CONTROL_MP, MSP_SAT_PATA, -1, 5, 5, 1, 0,
        "Ultra DMA bit 5", NULL},
    {"UDMA4", CONTROL_MP, MSP_SAT_PATA, -1, 5, 4, 1, 0,
        "Ultra DMA bit 4", NULL},
    {"UDMA3", CONTROL_MP, MSP_SAT_PATA, -1, 5, 3, 1, 0,
        "Ultra DMA bit 3", NULL},
    {"UDMA2", CONTROL_MP, MSP_SAT_PATA, -1, 5, 2, 1, 0,
        "Ultra DMA bit 2", NULL},
    {"UDMA1", CONTROL_MP, MSP_SAT_PATA, -1, 5, 1, 1, 0,
        "Ultra DMA bit 1", NULL},
    {"UDMA0", CONTROL_MP, MSP_SAT_PATA, -1, 5, 0, 1, 0,
        "Ultra DMA bit 0", NULL},

    /* Power condition mode page - obsolete block-device-only version */
    /*   [0xd] sbc (replacement page now at 0x1a) */
    {"IDLE-OLD", POWER_OLD_MP, 0, PDT_DISK, 3, 1, 1, 0,
        "Idle timer active", NULL},
    {"STBY-OLD", POWER_OLD_MP, 0, PDT_DISK, 3, 0, 1, 0,
        "Standby timer active", NULL},
    {"ICT-OLD", POWER_OLD_MP, 0, PDT_DISK, 4, 7, 32, 0,
        "Idle condition timer (100 ms)", NULL},
    {"SCT-OLD", POWER_OLD_MP, 0, PDT_DISK, 8, 7, 32, 0,
        "Standby condition timer (100 ms)", NULL},

    /* Data compression mode page [0xf] ssc3 */
    {"DCE", DATA_COMPR_MP, 0, PDT_TAPE, 2, 7, 1, MF_COMMON,
        "Data compression enable", NULL},
    {"DCC", DATA_COMPR_MP, 0, PDT_TAPE, 2, 6, 1, MF_COMMON,
        "Data compression capable", NULL},
    {"DDE", DATA_COMPR_MP, 0, PDT_TAPE, 3, 7, 1, MF_COMMON,
        "Data decompression enable", NULL},
    {"RED", DATA_COMPR_MP, 0, PDT_TAPE, 3, 6, 2, 0,
        "Report exception on decompression", NULL},
    {"COMPR_A", DATA_COMPR_MP, 0, PDT_TAPE, 4, 7, 32, 0,
        "Compression algorithm",
        "0: none; 5: ALDC (2048 byte): 16: IDRC; 32: DCLZ"},
    {"DCOMPR_A", DATA_COMPR_MP, 0, PDT_TAPE, 8, 7, 32, 0,
        "Decompression algorithm",
        "0: none; 5: ALDC (2048 byte): 16: IDRC; 32: DCLZ"},

    /* XOR control mode page [0x10] sbc2 << obsolete in sbc3r32>> */
    {"XORDIS", XOR_MP, 0, PDT_DISK, 2, 1, 1, 0,
        "XOR disable", NULL},
    {"MXWS", XOR_MP, 0, PDT_DISK, 4, 7, 32, 0,
        "Maximum XOR write size (blocks)", NULL},

    /* Device configuration mode page [0x10] ssc3 */
    {"CAF", DEV_CONF_MP, 0, PDT_TAPE, 2, 5, 1, 0,
        "Change active format", NULL},
    {"ACT_F", DEV_CONF_MP, 0, PDT_TAPE, 2, 4, 5, 0,
        "Active format", NULL},
    {"ACT_P", DEV_CONF_MP, 0, PDT_TAPE, 3, 7, 8, 0,
        "Active partition", NULL},
    {"WOBFR", DEV_CONF_MP, 0, PDT_TAPE, 4, 7, 8, 0,
        "Write object buffer full ratio", NULL},
    {"ROBER", DEV_CONF_MP, 0, PDT_TAPE, 5, 7, 8, 0,
        "Read object buffer empty ratio", NULL},
    {"WDT", DEV_CONF_MP, 0, PDT_TAPE, 6, 7, 16, 0,
        "Write delay time (100 ms)", NULL},
    {"OBR", DEV_CONF_MP, 0, PDT_TAPE, 8, 7, 1, 0,
        "Object buffer recovery", NULL},
    {"LOIS", DEV_CONF_MP, 0, PDT_TAPE, 8, 6, 1, 0,
        "Logical object identifiers supported", NULL},
    {"RSMK", DEV_CONF_MP, 0, PDT_TAPE, 8, 5, 1, MF_COMMON,
        "Report setmarks (obsolete)", NULL},
    {"AVC", DEV_CONF_MP, 0, PDT_TAPE, 8, 4, 1, 0,
        "Automatic velocity control", NULL},
    {"SOCF", DEV_CONF_MP, 0, PDT_TAPE, 8, 3, 2, 0,
        "Stop on consecutive filemarks", NULL},
    {"ROBO", DEV_CONF_MP, 0, PDT_TAPE, 8, 1, 1, 0,
        "Recover object buffer order", NULL},
    {"REW", DEV_CONF_MP, 0, PDT_TAPE, 8, 0, 1, 0,
        "Report early warning", NULL},
    {"GAP_S", DEV_CONF_MP, 0, PDT_TAPE, 9, 7, 8, 0,
        "Gap size (obsolete)", NULL},
    {"EOD_D", DEV_CONF_MP, 0, PDT_TAPE, 10, 7, 3, 0,
        "EOD (end-of-data) defined",
        "0: default; 1: format defined; 2: SOCF; 3: not supported"},
    {"EEG", DEV_CONF_MP, 0, PDT_TAPE, 10, 4, 1, 0,
        "Enable EOD generation", NULL},
    {"SEW", DEV_CONF_MP, 0, PDT_TAPE, 10, 3, 1, MF_COMMON,
        "Synchronize early warning", NULL},
    {"SWP_T", DEV_CONF_MP, 0, PDT_TAPE, 10, 2, 1, 0,
        "Software write protect (tape)", NULL},
    {"BAML", DEV_CONF_MP, 0, PDT_TAPE, 10, 1, 1, 0,
        "Block address mode lock", NULL},
    {"BAM", DEV_CONF_MP, 0, PDT_TAPE, 10, 0, 1, 0,
        "Block address mode", NULL},
    {"OBSAEW", DEV_CONF_MP, 0, PDT_TAPE, 11, 7, 24, 0,
        "Object buffer size at early warning", NULL},
    {"SDCA", DEV_CONF_MP, 0, PDT_TAPE, 14, 7, 8, MF_COMMON,
        "Select data compression algorithm", NULL},
    {"WTRE", DEV_CONF_MP, 0, PDT_TAPE, 15, 7, 2, 0,
        "WORM tamper read enable", NULL},
    {"OIR", DEV_CONF_MP, 0, PDT_TAPE, 15, 5, 1, 0,
        "Only if reserved", NULL},
    {"ROR", DEV_CONF_MP, 0, PDT_TAPE, 15, 4, 2, 0,
        "Rewind on reset",
        "0: vendor specific; 1: to BOP 0 on lu reset\t"
        "2: hold position on lu reset"},
    {"ASOCWP", DEV_CONF_MP, 0, PDT_TAPE, 15, 2, 1, 0,
        "Associated write protection", NULL},
    {"PERSWP", DEV_CONF_MP, 0, PDT_TAPE, 15, 1, 1, 0,
        "Persistent write protection", NULL},
    {"PRMWP", DEV_CONF_MP, 0, PDT_TAPE, 15, 0, 1, 0,
        "Permanent write protection", NULL},

    /* Device configuration extension mode subpage [0x10,1 ] ssc3 */
    {"TARPF", DEV_CONF_MP, MSP_DEV_CONF_EXT, PDT_TAPE, 4, 3, 1, 0,
        "TapeAlert respect parameter fields", NULL},
    {"TASER", DEV_CONF_MP, MSP_DEV_CONF_EXT, PDT_TAPE, 4, 2, 1, 0,
        "TapeAlert select except reporting", NULL},
    {"TARPC", DEV_CONF_MP, MSP_DEV_CONF_EXT, PDT_TAPE, 4, 1, 1, 0,
        "TapeAlert respect page control", NULL},
    {"TAPLSD", DEV_CONF_MP, MSP_DEV_CONF_EXT, PDT_TAPE, 4, 0, 1, 0,
        "TapeAlert prevent log sense deactivation", NULL},
    {"SEM", DEV_CONF_MP, MSP_DEV_CONF_EXT, PDT_TAPE, 5, 3, 4, 0,
        "Short erase mode",
        "0: as per SSC-2; 1: erase has no effect; 2: record EOD indication"},

    /* Medium partition mode page [0x11] ssc3 */
    {"MAX_AP", MED_PART_MP, 0, PDT_TAPE, 2, 7, 8, 0,
        "Maximum additional partitions", NULL},
    {"APD", MED_PART_MP, 0, PDT_TAPE, 3, 7, 8, 0,
        "Additional partitions defined", NULL},
    {"FDP", MED_PART_MP, 0, PDT_TAPE, 4, 7, 1, 0,
        "Fixed data partitions", NULL},
    {"SDP", MED_PART_MP, 0, PDT_TAPE, 4, 6, 1, 0,
        "Select data partitions", NULL},
    {"IDP", MED_PART_MP, 0, PDT_TAPE, 4, 5, 1, 0,
        "Initiator defined partitions", NULL},
    {"PSUM", MED_PART_MP, 0, PDT_TAPE, 4, 4, 2, 0,
        "Partition size unit of measure",
        "0: bytes; 1: kilobytes; 2: megabytes; 3: 10**(partition_units)"},
    {"POFM", MED_PART_MP, 0, PDT_TAPE, 4, 2, 1, 0,
        "Partition on format", NULL},
    {"CLEAR", MED_PART_MP, 0, PDT_TAPE, 4, 1, 1, 0,
        "Erase partition(s) (in concert with ADDP)", NULL},
    {"ADDP", MED_PART_MP, 0, PDT_TAPE, 4, 0, 1, 0,
        "Additional partition bit (in concert with CLEAR)", NULL},
    {"MFR", MED_PART_MP, 0, PDT_TAPE, 5, 7, 8, 0,
        "Medium format recognition",
        "0: incapable; 1: format recognition; 2: partition recognition\t"
        "3: format and partition recognition"},
    {"PART_U", MED_PART_MP, 0, PDT_TAPE, 6, 3, 4, 0,
        "Partition units (exponent of 10, bytes)", NULL},
    /* "descriptor" starts here */
    {"P_SZ", MED_PART_MP, 0, PDT_TAPE, 8, 7, 16, 0,
        "Partition size", NULL},

    /* Enclosure services management mode page [0x14] ses2 */
    {"ENBLTC", ES_MAN_MP, 0, PDT_SES, 5, 0, 1, MF_COMMON,
        "Enable timed completion", NULL},
    {"MTCT", ES_MAN_MP, 0, PDT_SES, 6, 7, 16, MF_COMMON,
        "Maximum task completion time (100ms)", NULL},

    /* Protocol specific logical unit control mode page [0x18] spc3 */
    {"LUPID", PROT_SPEC_LU_MP, 0, -1, 2, 3, 4, 0,
        "Logical unit's (transport) protocol identifier",
        PROTO_IDENT_STR "\t"
        "[try adding '-t <transport>' to get more fields]"},

    /* Protocol specific port control mode page [0x19] spc3 */
    {"PPID", PROT_SPEC_PORT_MP, 0, -1, 2, 3, 4, 0,
        "Port's (transport) protocol identifier",
        PROTO_IDENT_STR "\t"
        "[try adding '-t <transport>' to get more fields]"},

    /* Power condition mode page [0x1a] spc3 (expanded in spc4r18) */
    {"PM_BG", POWER_MP, 0, -1, 2, 7, 2, 0,      /* added spc4r24 */
        "Power management, background functions, precedence",
        "0: vendor specific; 1: background function higher\t"
        "2: power management higher"},
    {"STANDBY_Y", POWER_MP, 0, -1, 2, 0, 1, 0,
        "Standby_y timer enable", NULL},
    {"IDLE_C", POWER_MP, 0, -1, 3, 3, 1, 0,
        "Idle_c timer enable", NULL},
    {"IDLE_B", POWER_MP, 0, -1, 3, 2, 1, 0,
        "Idle_b timer enable", NULL},
    {"IDLE", POWER_MP, 0, -1, 3, 1, 1, 0,   /* IDLE_A in future ? */
        "Idle_a timer enable",
        "named IDLE prior to spc4r18, thence IDLE_A"},
    {"STANDBY", POWER_MP, 0, -1, 3, 0, 1, 0, /* STANDBY_Z in future ? */
        "Standby_z timer enable",
        "named STANDBY prior to spc4r18, thence STANDBY_Z"},
    {"ICT", POWER_MP, 0, -1, 4, 7, 32, 0,    /* IACT in future ? */
        "Idle_a condition timer (100 ms)",
        "named IDLE prior to spc4r18, thence IDLE_A"},
    {"SCT", POWER_MP, 0, -1, 8, 7, 32, 0,    /* SZCT in future ? */
        "Standby_z condition timer (100 ms)",
        "named STANDBY prior to spc4r18, thence STANDBY_Z"},
    {"IBCT", POWER_MP, 0, -1, 12, 7, 32, 0,
        "Idle_b condition timer (100 ms)", NULL},
    {"ICCT", POWER_MP, 0, -1, 16, 7, 32, 0,
        "Idle_c condition timer (100 ms)", NULL},
    {"SYCT", POWER_MP, 0, -1, 20, 7, 32, 0,
        "Standby_y condition timer (100 ms)", NULL},
    {"CCF_IDLE", POWER_MP, 0, -1, 39, 7, 2, 0,     /* changed spc4r35 */
        "check condition on transition from idle", /* was FIDCPC (spc4r25) */
        "0: restricted (SAS-2); 1: disabled; 2: enabled\n"},
    {"CCF_STAND", POWER_MP, 0, -1, 39, 5, 2, 0,        /* changed spc4r35 */
        "check condition on transition from standby",  /* was FSBCPC */
        "0: restricted (SAS-2); 1: disabled; 2: enabled\n"},
    {"CCF_STOPP", POWER_MP, 0, -1, 39, 3, 2, 0,        /* changed spc4r35 */
        "check condition on transition from stopped",  /* was FSTCPC */
        "0: restricted (SAS-2); 1: disabled; 2: enabled\n"},

    /* Power consumption mode page [0x1a,1] added spc4r33 */
    {"ps_id", POWER_MP, MSP_SPC_PS, -1, 7, 7, 8, 0,      /* added spc4r24 */
        "Power consumption identifier",
        "references Power consumption VPD page"},

    /* SAT ATA Power condition mode page [0x1a,0xf1] sat2 */
    {"APMP", POWER_MP, MSP_SAT_POWER, -1, 5, 0, 1, 0,
        "Advanced Power Management (APM) enabled/change", NULL},
    {"APM", POWER_MP, MSP_SAT_POWER, -1, 6, 7, 8, 0,
        "Advanced Power Management (APM) value", NULL},

    /* Informational exception control mode page [0x1c] spc3 */
    {"PERF", IEC_MP, 0, -1, 2, 7, 1, 0,
        "Performance (impact of ie operations)",
        "0: normal (some delays); 1: abridge ie operations"},
    {"EBF", IEC_MP, 0, -1, 2, 5, 1, 0,
        "Enable background function", NULL},
    {"EWASC", IEC_MP, 0, -1, 2, 4, 1, MF_COMMON,
        "Enable warning", NULL},
    {"DEXCPT", IEC_MP, 0, -1, 2, 3, 1, MF_COMMON,
        "Disable exceptions", NULL},
    {"TEST", IEC_MP, 0, -1, 2, 2, 1, 0,
        "Test (simulate device failure)", NULL},
    {"EBACKERR", IEC_MP, 0, -1, 2, 1, 1, 0,
        "Enable background (scan + self test) error reporting", NULL},
    {"LOGERR", IEC_MP, 0, -1, 2, 0, 1, 0,
        "Log informational exception errors", NULL},
    {"MRIE", IEC_MP, 0, -1, 3, 3, 4, MF_COMMON,
        "Method of reporting informational exceptions",
        "0: no reporting; 1: async reporting (obs); 2: unit attention\t"
        "3: conditional recovered error; 4: recovered error\t"
        "5: check condition with no sense; 6: request sense only"},
    {"INTT", IEC_MP, 0, -1, 4, 7, 32, 0,
        "Interval timer (100 ms)", NULL},
    {"REPC", IEC_MP, 0, -1, 8, 7, 32, 0,
        "Report count (or Test flag number [SSC-3])", NULL},

    /* Background control mode subpage [0x1c,0x1] sbc3 */
    {"S_L_FULL", IEC_MP, MSP_BACK_CTL, PDT_DISK, 4, 2, 1, 0,
        "Suspend on log full", NULL},
    {"LOWIR", IEC_MP, MSP_BACK_CTL, PDT_DISK, 4, 1, 1, 0,
        "Log only when intervention required", NULL},
    {"EN_BMS", IEC_MP, MSP_BACK_CTL, PDT_DISK, 4, 0, 1, 0,
        "Enable background medium scan", NULL},
    {"EN_PS", IEC_MP, MSP_BACK_CTL, PDT_DISK, 5, 0, 1, 0,
        "Enable pre-scan", NULL},
    {"BMS_I", IEC_MP, MSP_BACK_CTL, PDT_DISK, 6, 7, 16, 0,
        "Background medium scan interval time (hour)", NULL},
    {"BPS_TL", IEC_MP, MSP_BACK_CTL, PDT_DISK, 8, 7, 16, 0,
        "Background pre-scan time limit (hour)", NULL},
    {"MIN_IDLE", IEC_MP, MSP_BACK_CTL, PDT_DISK, 10, 7, 16, 0,
        "Minumum idle time before background scan (ms)", NULL},
    {"MAX_SUSP", IEC_MP, MSP_BACK_CTL, PDT_DISK, 12, 7, 16, 0,
        "Maximum time to suspend background scan (ms)", NULL},

    /* Logical block provisioning mode subpage [0x1c,0x2] sbc3 */
    {"SITUA", IEC_MP, MSP_SBC_LB_PROV, PDT_DISK, 4, 0, 1, 0,
        "Single initiator threshold unit attention", NULL},
    /* descriptor starts here, the <start_byte> is relative to the start
     * of the mode page (i.e. 16 more than descriptor format table) */
    {"LBP_EN", IEC_MP, MSP_SBC_LB_PROV, PDT_DISK, 16, 7, 1, 0,
        "Threshold enabled", NULL},
    {"LBP_TYPE", IEC_MP, MSP_SBC_LB_PROV, PDT_DISK, 16, 5, 3, 0,
        "Threshold type", NULL},
    {"LBP_ARM", IEC_MP, MSP_SBC_LB_PROV, PDT_DISK, 16, 2, 3, 0,
        "Threshold arming", NULL},
    {"LBP_RES", IEC_MP, MSP_SBC_LB_PROV, PDT_DISK, 17, 7, 8, 0,
        "Threshold resource", NULL},
    {"LBP_COUNT", IEC_MP, MSP_SBC_LB_PROV, PDT_DISK, 20, 7, 32, 0,
        "Threshold count", NULL},

    /* Medium configuration mode page [0x1d] ssc3 */
    {"WORMM", MED_CONF_MP, 0, PDT_TAPE, 2, 0, 1, 0,
        "Worm mode", NULL},
    {"WMLR", MED_CONF_MP, 0, PDT_TAPE, 4, 7, 8, 0,
        "Worm mode label restrictions",
        "0: disallow overwrite; 1: disallow some format labels overwrite\t"
        "2: allow all format labels to be overwritten"},
    {"WMFR", MED_CONF_MP, 0, PDT_TAPE, 5, 7, 8, 0,
        "Worm mode filemark restrictions",
        "2: allow filemarks before EOD except closest to BOP\t"
        "3: allow any number of filemarks before EOD"},

    /* Timeout and protect mode page [0x1d] mmc5 */
    {"G3E", TIMEOUT_PROT_MP, 0, PDT_MMC, 4, 3, 1, 0,
        "Group 3 timeout capability enable", NULL},
    {"TMOE", TIMEOUT_PROT_MP, 0, PDT_MMC, 4, 2, 1, 0,
        "Timeout enable", NULL},
    {"DISP", TIMEOUT_PROT_MP, 0, PDT_MMC, 4, 1, 1, 0,
        "Disable (unavailable) until power cycle", NULL},
    {"SWPP", TIMEOUT_PROT_MP, 0, PDT_MMC, 4, 0, 1, 0,
        "Software write protect until power cycle", NULL},
    {"G1MT", TIMEOUT_PROT_MP, 0, PDT_MMC, 6, 7, 16, 0,
        "Group 1 minimum timeout (sec)", NULL},
    {"G2MT", TIMEOUT_PROT_MP, 0, PDT_MMC, 8, 7, 16, 0,
        "Group 2 minimum timeout (sec)", NULL},


    /* Element address assignment mode page [0x1d] smc2 */
    {"FMTEA", ELE_ADDR_ASS_MP, 0, PDT_MCHANGER, 2, 7, 16, 0,
        "First medium transport element address", NULL},
    {"NMTE", ELE_ADDR_ASS_MP, 0, PDT_MCHANGER, 4, 7, 16, 0,
        "Number of medium transport elements", NULL},
    {"FSEA", ELE_ADDR_ASS_MP, 0, PDT_MCHANGER, 6, 7, 16, 0,
        "First storage element address", NULL},
    {"NSE", ELE_ADDR_ASS_MP, 0, PDT_MCHANGER, 8, 7, 16, 0,
        "Number of storage elements", NULL},
    {"FIEEA", ELE_ADDR_ASS_MP, 0, PDT_MCHANGER, 10, 7, 16, 0,
        "First import/export element address", NULL},
    {"NIEE", ELE_ADDR_ASS_MP, 0, PDT_MCHANGER, 12, 7, 16, 0,
        "Number of import/export elements", NULL},
    {"FDTEA", ELE_ADDR_ASS_MP, 0, PDT_MCHANGER, 14, 7, 16, 0,
        "First data transfer element address", NULL},
    {"NDTE", ELE_ADDR_ASS_MP, 0, PDT_MCHANGER, 16, 7, 16, 0,
        "Number of data transfer elements", NULL},

    /* Transport geometry parameters mode page [0x1e] smc2 */
    /* transport geometry descriptor starts here, <start_byte> is relative
     * to start of mode page (i.e. 2 more than shown in descriptor table */
    {"ROTAT", TRANS_GEO_PAR_MP, 0, PDT_MCHANGER, 2, 0, 1, 0,
        "Rotation for double sided media handling", NULL},
    {"MNTES", TRANS_GEO_PAR_MP, 0, PDT_MCHANGER, 3, 7, 8, 0,
        "Member number in transport element set", NULL},

    /* Device capabilities mode page [0x1f] smc3 */
    {"STORDT", DEV_CAP_MP, 0, PDT_MCHANGER, 2, 3, 1, 0,
        "Storage for data transfer element", NULL},
    {"STORIE", DEV_CAP_MP, 0, PDT_MCHANGER, 2, 2, 1, 0,
        "Storage for import/export element", NULL},
    {"STORST", DEV_CAP_MP, 0, PDT_MCHANGER, 2, 1, 1, 0,
        "Storage for storage element", NULL},
    {"STORMT", DEV_CAP_MP, 0, PDT_MCHANGER, 2, 0, 1, 0,
        "Storage for medium transport element", NULL},
    {"ACE", DEV_CAP_MP, 0, PDT_MCHANGER, 3, 2, 1, 0,
        "Auto clean enabled", NULL},
    {"VTRP", DEV_CAP_MP, 0, PDT_MCHANGER, 3, 1, 1, 0,
        "Volume tag reader present", NULL},
    {"S2C", DEV_CAP_MP, 0, PDT_MCHANGER, 3, 0, 1, 0,
        "SMC-2 capabilities supported", NULL},
    {"MT_RA", DEV_CAP_MP, 0, PDT_MCHANGER, 4, 7, 2, 0,
        "Medium transport elements support Read Attribute", NULL},
    {"MT2DT", DEV_CAP_MP, 0, PDT_MCHANGER, 4, 3, 1, 0,
        "Medium transport -> data transfer; Move Medium", NULL},
    {"MT2IE", DEV_CAP_MP, 0, PDT_MCHANGER, 4, 2, 1, 0,
        "Medium transport -> import/export; Move Medium", NULL},
    {"MT2ST", DEV_CAP_MP, 0, PDT_MCHANGER, 4, 1, 1, 0,
        "Medium transport -> storage; Move Medium", NULL},
    {"MT2MT", DEV_CAP_MP, 0, PDT_MCHANGER, 4, 0, 1, 0,
        "Medium transport -> medium transport; Move Medium", NULL},
    {"ST_RA", DEV_CAP_MP, 0, PDT_MCHANGER, 5, 7, 2, 0,
        "Storage elements support Read Attribute", NULL},
    {"ST2DT", DEV_CAP_MP, 0, PDT_MCHANGER, 5, 3, 1, 0,
        "Storage -> data transfer; Move Medium", NULL},
    {"ST2IE", DEV_CAP_MP, 0, PDT_MCHANGER, 5, 2, 1, 0,
        "Storage -> import/export; Move Medium", NULL},
    {"ST2ST", DEV_CAP_MP, 0, PDT_MCHANGER, 5, 1, 1, 0,
        "Storage -> storage; Move Medium", NULL},
    {"ST2MT", DEV_CAP_MP, 0, PDT_MCHANGER, 5, 0, 1, 0,
        "Storage -> medium transport; Move Medium", NULL},
    {"IE_RA", DEV_CAP_MP, 0, PDT_MCHANGER, 6, 7, 2, 0,
        "Import/export elements support Read Attribute", NULL},
    {"IE2DT", DEV_CAP_MP, 0, PDT_MCHANGER, 6, 3, 1, 0,
        "Import/export -> data transfer; Move Medium", NULL},
    {"IE2IE", DEV_CAP_MP, 0, PDT_MCHANGER, 6, 2, 1, 0,
        "Import/export -> import/export; Move Medium", NULL},
    {"IE2ST", DEV_CAP_MP, 0, PDT_MCHANGER, 6, 1, 1, 0,
        "Import/export -> storage; Move Medium", NULL},
    {"IE2MT", DEV_CAP_MP, 0, PDT_MCHANGER, 6, 0, 1, 0,
        "Import/export -> medium transport; Move Medium", NULL},
    {"DT_RA", DEV_CAP_MP, 0, PDT_MCHANGER, 7, 7, 2, 0,
        "Data transfer elements support Read Attribute", NULL},
    {"DT2DT", DEV_CAP_MP, 0, PDT_MCHANGER, 7, 3, 1, 0,
        "Data transfer -> data transfer; Move Medium", NULL},
    {"DT2IE", DEV_CAP_MP, 0, PDT_MCHANGER, 7, 2, 1, 0,
        "Data transfer -> import/export; Move Medium", NULL},
    {"DT2ST", DEV_CAP_MP, 0, PDT_MCHANGER, 7, 1, 1, 0,
        "Data transfer -> storage; Move Medium", NULL},
    {"DT2MT", DEV_CAP_MP, 0, PDT_MCHANGER, 7, 0, 1, 0,
        "Data transfer -> medium transport; Move Medium", NULL},
    {"MT_WA", DEV_CAP_MP, 0, PDT_MCHANGER, 12, 7, 2, 0,
        "Medium transport elements support Write Attribute", NULL},
    {"MTEDT", DEV_CAP_MP, 0, PDT_MCHANGER, 12, 3, 1, 0,
        "Medium transport -> data transfer; Exchange Medium", NULL},
    {"MTEIE", DEV_CAP_MP, 0, PDT_MCHANGER, 12, 2, 1, 0,
        "Medium transport -> import/export; Exchange Medium", NULL},
    {"MTEST", DEV_CAP_MP, 0, PDT_MCHANGER, 12, 1, 1, 0,
        "Medium transport -> storage; Exchange Medium", NULL},
    {"MTEMT", DEV_CAP_MP, 0, PDT_MCHANGER, 12, 0, 1, 0,
        "Medium transport -> medium transport; Exchange Medium", NULL},
    {"ST_WA", DEV_CAP_MP, 0, PDT_MCHANGER, 13, 7, 2, 0,
        "Storage elements support Write Attribute", NULL},
    {"STEDT", DEV_CAP_MP, 0, PDT_MCHANGER, 13, 3, 1, 0,
        "Storage -> data transfer; Exchange Medium", NULL},
    {"STEIE", DEV_CAP_MP, 0, PDT_MCHANGER, 13, 2, 1, 0,
        "Storage -> import/export; Exchange Medium", NULL},
    {"STEST", DEV_CAP_MP, 0, PDT_MCHANGER, 13, 1, 1, 0,
        "Storage -> storage; Exchange Medium", NULL},
    {"STEMT", DEV_CAP_MP, 0, PDT_MCHANGER, 13, 0, 1, 0,
        "Storage -> medium transport; Exchange Medium", NULL},
    {"IE_WA", DEV_CAP_MP, 0, PDT_MCHANGER, 14, 7, 2, 0,
        "Import/export elements support Write Attribute", NULL},
    {"IEEDT", DEV_CAP_MP, 0, PDT_MCHANGER, 14, 3, 1, 0,
        "Import/export -> data transfer; Exchange Medium", NULL},
    {"IEEIE", DEV_CAP_MP, 0, PDT_MCHANGER, 14, 2, 1, 0,
        "Import/export -> import/export; Exchange Medium", NULL},
    {"IEEST", DEV_CAP_MP, 0, PDT_MCHANGER, 14, 1, 1, 0,
        "Import/export -> storage; Exchange Medium", NULL},
    {"IEEMT", DEV_CAP_MP, 0, PDT_MCHANGER, 14, 0, 1, 0,
        "Import/export -> medium transport; Exchange Medium", NULL},
    {"DT_WA", DEV_CAP_MP, 0, PDT_MCHANGER, 15, 7, 2, 0,
        "Data transfer elements support Write Attribute", NULL},
    {"DTEDT", DEV_CAP_MP, 0, PDT_MCHANGER, 15, 3, 1, 0,
        "Data transfer -> data transfer; Exchange Medium", NULL},
    {"DTEIE", DEV_CAP_MP, 0, PDT_MCHANGER, 15, 2, 1, 0,
        "Data transfer -> import/export; Exchange Medium", NULL},
    {"DTEST", DEV_CAP_MP, 0, PDT_MCHANGER, 15, 1, 1, 0,
        "Data transfer -> storage; Exchange Medium", NULL},
    {"DTEMT", DEV_CAP_MP, 0, PDT_MCHANGER, 15, 0, 1, 0,
        "Data transfer -> medium transport; Exchange Medium", NULL},

    /* Extended device capabilities mode page [0x1f,0x41] smc3 */
    {"MVPRV", DEV_CAP_MP, MSP_EXT_DEV_CAP, PDT_MCHANGER, 4, 5, 1, 0,
        "Move prevented to import/export element", NULL},
    {"MVCL", DEV_CAP_MP, MSP_EXT_DEV_CAP, PDT_MCHANGER, 4, 4, 1, 0,
        "Move closes import/export element", NULL},
    {"MVOP", DEV_CAP_MP, MSP_EXT_DEV_CAP, PDT_MCHANGER, 4, 3, 1, 0,
        "Move opens import/export element", NULL},
    {"USRCL", DEV_CAP_MP, MSP_EXT_DEV_CAP, PDT_MCHANGER, 4, 2, 1, 0,
        "User control import/export element close", NULL},
    {"USROP", DEV_CAP_MP, MSP_EXT_DEV_CAP, PDT_MCHANGER, 4, 1, 1, 0,
        "User control import/export element open", NULL},
    {"IEST", DEV_CAP_MP, MSP_EXT_DEV_CAP, PDT_MCHANGER, 4, 0, 1, 0,
        "Import/export element state", NULL},
    {"DTETA", DEV_CAP_MP, MSP_EXT_DEV_CAP, PDT_MCHANGER, 5, 4, 1, 0,
        "Data transfer element empty on door access", NULL},
    {"RSSEA", DEV_CAP_MP, MSP_EXT_DEV_CAP, PDT_MCHANGER, 5, 3, 1, 0,
        "Return to source storage element address", NULL},
    {"MVTRY", DEV_CAP_MP, MSP_EXT_DEV_CAP, PDT_MCHANGER, 5, 2, 1, 0,
        "Move tray", NULL},
    {"IEMGZ", DEV_CAP_MP, MSP_EXT_DEV_CAP, PDT_MCHANGER, 5, 1, 1, 0,
        "Import/export magazine", NULL},
    {"SMGZ", DEV_CAP_MP, MSP_EXT_DEV_CAP, PDT_MCHANGER, 5, 0, 1, 0,
        "Storage magazine", NULL},
    {"TREXC", DEV_CAP_MP, MSP_EXT_DEV_CAP, PDT_MCHANGER, 6, 2, 1, 0,
        "True exchange capable", NULL},
    {"LCKIE", DEV_CAP_MP, MSP_EXT_DEV_CAP, PDT_MCHANGER, 6, 1, 1, 0,
        "Lock import/export element", NULL},
    {"LCKD", DEV_CAP_MP, MSP_EXT_DEV_CAP, PDT_MCHANGER, 6, 0, 1, 0,
        "Lock door", NULL},
    {"SPMER", DEV_CAP_MP, MSP_EXT_DEV_CAP, PDT_MCHANGER, 7, 2, 1, 0,
        "Source pre-move eject required", NULL},
    {"DPMER", DEV_CAP_MP, MSP_EXT_DEV_CAP, PDT_MCHANGER, 7, 1, 1, 0,
        "Destination pre-move eject required", NULL},
    {"PEPOS", DEV_CAP_MP, MSP_EXT_DEV_CAP, PDT_MCHANGER, 7, 0, 1, 0,
        "Pre-eject position", NULL},
    {"UCST", DEV_CAP_MP, MSP_EXT_DEV_CAP, PDT_MCHANGER, 8, 0, 1, 0,
        "Unassigned cleaning storage", NULL},

    /* CD/DVD (MM) capabilities and mechanical status mode page */
    /* [0x2a] obsolete in mmc4 and mmc5, last valid in mmc3 */
    /* MRSS field was already obsolete in mmc3 */
    {"D_RAM_R", MMCMS_MP, 0, PDT_MMC, 2, 5, 1, 0,
        "DVD-RAM read", NULL},
    {"D_R_R", MMCMS_MP, 0, PDT_MMC, 2, 4, 1, 0,
        "DVD-R read", NULL},
    {"D_ROM_R", MMCMS_MP, 0, PDT_MMC, 2, 3, 1, 0,
        "DVD-ROM read", NULL},
    {"METH2", MMCMS_MP, 0, PDT_MMC, 2, 2, 1, 0,
        "Method 2", NULL},
    {"CD_RW_R", MMCMS_MP, 0, PDT_MMC, 2, 1, 1, 0,
        "CD-RW read", NULL},
    {"CD_R_R", MMCMS_MP, 0, PDT_MMC, 2, 0, 1, 0,
        "CD-R read", NULL},
    {"D_RAM_W", MMCMS_MP, 0, PDT_MMC, 3, 5, 1, 0,
        "DVD-RAM write", NULL},
    {"D_R_W", MMCMS_MP, 0, PDT_MMC, 3, 4, 1, 0,
        "DVD-R write", NULL},     /* was D_R_R, wrong, clashed with above */
    {"TST_WR", MMCMS_MP, 0, PDT_MMC, 3, 2, 1, 0,
        "Test write", NULL},      /* was TST_W but clashed with page 0x5 */
    {"CD_RW_W", MMCMS_MP, 0, PDT_MMC, 3, 1, 1, 0,
        "CD-RW write", NULL},
    {"CD_R_W", MMCMS_MP, 0, PDT_MMC, 3, 0, 1, 0,
        "CD-R write", NULL},
    {"BUF", MMCMS_MP, 0, PDT_MMC, 4, 7, 1, 0,
        "Buffer underrun free recording", NULL},
    {"MULT_S", MMCMS_MP, 0, PDT_MMC, 4, 6, 1, 0,
        "Multi session", NULL},   /* was MULTI_S but clashed with page 0x5 */
    {"M2F2", MMCMS_MP, 0, PDT_MMC, 4, 5, 1, 0,
        "Mode 2 form 2", NULL},
    {"M2F1", MMCMS_MP, 0, PDT_MMC, 4, 4, 1, 0,
        "Mode 2 form 1", NULL},
    {"DP_2", MMCMS_MP, 0, PDT_MMC, 4, 3, 1, 0,
        "Digital port 2", NULL},
    {"DP_1", MMCMS_MP, 0, PDT_MMC, 4, 2, 1, 0,
        "Digital port 1", NULL},
    {"COMP", MMCMS_MP, 0, PDT_MMC, 4, 1, 1, 0,
        "Composite", NULL},
    {"AUDIO_P", MMCMS_MP, 0, PDT_MMC, 4, 0, 1, 0,
        "Audio play", NULL},
    {"RBC", MMCMS_MP, 0, PDT_MMC, 5, 7, 1, 0,
        "Read bar code", NULL},
    {"UPC", MMCMS_MP, 0, PDT_MMC, 5, 6, 1, 0,
        "Uniform product code", NULL},
    {"ISRC", MMCMS_MP, 0, PDT_MMC, 5, 5, 1, 0,
        "International standard recording code", NULL},
    {"C2PS", MMCMS_MP, 0, PDT_MMC, 5, 4, 1, 0,
        "C 2 pointers supported", NULL},
    {"RW_DC", MMCMS_MP, 0, PDT_MMC, 5, 3, 1, 0,
        "R-W de-interleaved and corrected", NULL},
    {"RW_S", MMCMS_MP, 0, PDT_MMC, 5, 2, 1, 0,
        "R-W supported", NULL},
    {"CDDA_SA", MMCMS_MP, 0, PDT_MMC, 5, 1, 1, 0,
        "CD-DA stream accurate", NULL},
    {"CDDA_CS", MMCMS_MP, 0, PDT_MMC, 5, 0, 1, 0,
        "CD-DA commands supported", NULL},
    {"LMT", MMCMS_MP, 0, PDT_MMC, 6, 7, 3, 0,
        "Loading mechanism type", NULL},
    {"EJECT", MMCMS_MP, 0, PDT_MMC, 6, 3, 1, 0,
        "Eject (individual or magazine)", NULL},
    {"PJ", MMCMS_MP, 0, PDT_MMC, 6, 2, 1, 0,
        "Prevent jumper", NULL},
    {"LS", MMCMS_MP, 0, PDT_MMC, 6, 1, 1, 0,
        "Lock state", NULL},
    {"LOCK", MMCMS_MP, 0, PDT_MMC, 6, 0, 1, 0,
        "Lock (supported)", NULL},
    {"RWILI", MMCMS_MP, 0, PDT_MMC, 7, 5, 1, 0,
        "R-W in lead in", NULL},
    {"SCC", MMCMS_MP, 0, PDT_MMC, 7, 4, 1, 0,
        "Side change capable", NULL},
    {"SSS", MMCMS_MP, 0, PDT_MMC, 7, 3, 1, 0,
        "Software slot selection", NULL},
    {"CSDP", MMCMS_MP, 0, PDT_MMC, 7, 2, 1, 0,
        "Changer supports disc present", NULL},
    {"SCM", MMCMS_MP, 0, PDT_MMC, 7, 1, 1, 0,
        "Separate channel mute", NULL},
    {"SVL", MMCMS_MP, 0, PDT_MMC, 7, 0, 1, 0,
        "Separate volume levels", NULL},
    {"MRSS", MMCMS_MP, 0, PDT_MMC, 8, 7, 16, 0,
        "Maximum read speed supported (kBps) (obs)", NULL},
    {"NVLS", MMCMS_MP, 0, PDT_MMC, 10, 7, 16, 0,
        "Number of volume levels supported", NULL},
    {"BSS", MMCMS_MP, 0, PDT_MMC, 12, 7, 16, 0,
        "Buffer size supported (1024 bytes)", NULL},
    {"LENGTH", MMCMS_MP, 0, PDT_MMC, 17, 5, 2, 0,
        "Length (bit length of IEC958 words)", NULL},
    {"LSBF", MMCMS_MP, 0, PDT_MMC, 17, 3, 1, 0,
        "LSB (least significant bit) first", NULL},
    {"RCK", MMCMS_MP, 0, PDT_MMC, 17, 2, 1, 0,
        "High on LRCK indicates left channel", NULL},
    {"BCKF", MMCMS_MP, 0, PDT_MMC, 17, 1, 1, 0,
        "BCK signal falling edge", NULL},
    {"CMRS", MMCMS_MP, 0, PDT_MMC, 22, 7, 16, 0,
        "Copy management revision supported", NULL},
    {"RCS", MMCMS_MP, 0, PDT_MMC, 27, 1, 2, 0,
        "Rotation control selected", NULL},
    {"CWSS", MMCMS_MP, 0, PDT_MMC, 28, 7, 16, 0,
        "Current write speed selected", NULL},

    {NULL, 0, 0, 0, 0, 0, 0, 0, NULL, NULL},
};


/* << Transport protocol specific mode page items follow >> */

static struct sdparm_mode_page_item sdparm_mitem_fcp_arr[] = {
    /* disconnect-reconnect mode page [0x2] fcp3 */
    {"BFR", DISCONNECT_MP, 0, -1, 2, 7, 8, 0,
        "Buffer full ratio", NULL},
    {"BER", DISCONNECT_MP, 0, -1, 3, 7, 8, 0,
        "Buffer empty ratio", NULL},
    {"BIL", DISCONNECT_MP, 0, -1, 4, 7, 16, MF_COMMON,
        "Bus inactivity limit (transmission words)", NULL},
    {"DTL", DISCONNECT_MP, 0, -1, 6, 7, 16, MF_COMMON,
        "Disconnect time limit (128 transmission words)", NULL},
    {"CTL", DISCONNECT_MP, 0, -1, 8, 7, 16, MF_COMMON,
        "Connect time limit (128 transmission words)", NULL},
    {"MBS", DISCONNECT_MP, 0, -1, 10, 7, 16, MF_COMMON | MF_CLASH_OK,
        "Maximum burst size (512 bytes)", NULL},
    {"EMDP", DISCONNECT_MP, 0, -1, 12, 7, 1, MF_CLASH_OK,
        "Enable modify data pointers", NULL},
    {"FAA", DISCONNECT_MP, 0, -1, 12, 6, 1, 0,
        "Fairness access A [FCP_DATA]", NULL},
    {"FAB", DISCONNECT_MP, 0, -1, 12, 5, 1, 0,
        "Fairness access B [FCP_XFER]", NULL},
    {"FAC", DISCONNECT_MP, 0, -1, 12, 4, 1, 0,
        "Fairness access C [FCP_RSP]", NULL},
    {"FBS", DISCONNECT_MP, 0, -1, 14, 7, 16, MF_CLASH_OK,
        "First burst size (512 bytes)", NULL},

    /* protocol specific logical unit mode page [0x18] fcp3 */
    {"LUPID", PROT_SPEC_LU_MP, 0, -1, 2, 3, 4, MF_COMMON | MF_CLASH_OK,
        "Logical unit's (transport) protocol identifier",
        PROTO_IDENT_STR },
    {"EPDC", PROT_SPEC_LU_MP, 0, -1, 3, 0, 1, MF_COMMON,
        "Enable precise delivery checking", NULL},

    /* protocol specific port control page [0x19] fcp3 */
    {"PPID", PROT_SPEC_PORT_MP, 0, -1, 2, 3, 4, MF_COMMON | MF_CLASH_OK,
        "Port's (transport) protocol identifier",
        PROTO_IDENT_STR },
    {"DTFD", PROT_SPEC_PORT_MP, 0, -1, 3, 7, 1, MF_COMMON,
        "Disable target fabric discovery", NULL},
    {"PLPB", PROT_SPEC_PORT_MP, 0, -1, 3, 6, 1, MF_COMMON,
        "Prevent loop port bypass", NULL},
    {"DDIS", PROT_SPEC_PORT_MP, 0, -1, 3, 5, 1, 0,
        "Disable discovery", NULL},
    {"DLM", PROT_SPEC_PORT_MP, 0, -1, 3, 4, 1, 0,
        "Disable loop master", NULL},
    {"RHA", PROT_SPEC_PORT_MP, 0, -1, 3, 3, 1, 0,
        "Require hard address", NULL},
    {"ALWI", PROT_SPEC_PORT_MP, 0, -1, 3, 2, 1, 0,
        "Allow login without loop initialization", NULL},
    {"DTIPE", PROT_SPEC_PORT_MP, 0, -1, 3, 1, 1, 0,
        "Disable target initialized port enable", NULL},
    {"DTOLI", PROT_SPEC_PORT_MP, 0, -1, 3, 0, 1, 0,
        "Disable target originated loop initialization", NULL},
    {"RRTVU", PROT_SPEC_PORT_MP, 0, -1, 6, 2, 3, 0,
        "Resource recovery timeout value unit", NULL},
    {"SIRRTV", PROT_SPEC_PORT_MP, 0, -1, 7, 7, 8, 0,
        "Sequence initiative resource recovery timeout value", NULL},

    {NULL, 0, 0, 0, 0, 0, 0, 0, NULL, NULL},
};

static struct sdparm_mode_page_item sdparm_mitem_spi_arr[] = {
    /* disconnect-reconnect mode page [0x2] spi4 */
    {"BFR", DISCONNECT_MP, 0, -1, 2, 7, 8, 0,
        "Buffer full ratio", NULL},
    {"BER", DISCONNECT_MP, 0, -1, 3, 7, 8, 0,
        "Buffer empty ratio", NULL},
    {"BIL", DISCONNECT_MP, 0, -1, 4, 7, 16, MF_COMMON,
        "Bus inactivity limit (100 us)", NULL},
    {"PDTL", DISCONNECT_MP, 0, -1, 6, 7, 16, MF_COMMON,
        "Physical disconnect time limit (100 us)", NULL},
    {"CTL", DISCONNECT_MP, 0, -1, 8, 7, 16, MF_COMMON,
        "Connect time limit (100 us)", NULL},
    {"MBS", DISCONNECT_MP, 0, -1, 10, 7, 16, MF_COMMON | MF_CLASH_OK,
        "Maximum burst size (512 bytes)", NULL},
    {"EMDP", DISCONNECT_MP, 0, -1, 12, 7, 1, MF_CLASH_OK,
        "Enable modify data pointers", NULL},
    {"FA", DISCONNECT_MP, 0, -1, 12, 6, 3, 0,
        "Fair arbitration", NULL},
    {"DIMM", DISCONNECT_MP, 0, -1, 12, 3, 1, 0,
        "Disconnect immediate", NULL},
    {"DTDC", DISCONNECT_MP, 0, -1, 12, 2, 3, 0,
        "Data transfer disconnect control", NULL},

    /* protocol specific logical unit control mode page [0x18] spi4 */
    {"LUPID", PROT_SPEC_LU_MP, 0, -1, 2, 3, 4, MF_COMMON | MF_CLASH_OK,
        "Logical unit's (transport) protocol identifier",
        PROTO_IDENT_STR },

    /* protocol specific port control page [0x19] spi4 */
    {"PPID", PROT_SPEC_PORT_MP, 0, -1, 2, 3, 4, MF_COMMON | MF_CLASH_OK,
        "Port's (transport) protocol identifier",
        PROTO_IDENT_STR },
    {"STT", PROT_SPEC_PORT_MP, 0, -1, 4, 7, 16, MF_COMMON,
        "Synchronous transfer timeout (ms)", NULL},

    /* margin control subpage  [0x19,0x1] spi4 */
    {"PPID_1", PROT_SPEC_PORT_MP, MSP_SPI_MC, -1, 5, 3, 4, 0,
        "Port's (transport) protocol identifier",
        PROTO_IDENT_STR },
    {"DS", PROT_SPEC_PORT_MP, MSP_SPI_MC, -1, 7, 7, 4, 0,
        "Driver strength", NULL},
    {"DA", PROT_SPEC_PORT_MP, MSP_SPI_MC, -1, 8, 7, 4, 0,
        "Driver asymmetry", NULL},
    {"DP", PROT_SPEC_PORT_MP, MSP_SPI_MC, -1, 8, 3, 4, 0,
        "Driver precompensation", NULL},
    {"DSR", PROT_SPEC_PORT_MP, MSP_SPI_MC, -1, 9, 7, 4, 0,
        "Driver slew rate", NULL},

    /* saved training configuration subpage [0x19,0x2] spi4 */
    {"PPID_2", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 5, 3, 4, 0,
        "Port's (transport) protocol identifier",
        PROTO_IDENT_STR },
    {"DB0", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 10, 7, 32, MF_HEX,
        "DB(0) value", NULL},
    {"DB1", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 14, 7, 32, MF_HEX,
        "DB(1) value", NULL},
    {"DB2", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 18, 7, 32, MF_HEX,
        "DB(2) value", NULL},
    {"DB3", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 22, 7, 32, MF_HEX,
        "DB(3) value", NULL},
    {"DB4", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 26, 7, 32, MF_HEX,
        "DB(4) value", NULL},
    {"DB5", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 30, 7, 32, MF_HEX,
        "DB(5) value", NULL},
    {"DB6", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 34, 7, 32, MF_HEX,
        "DB(6) value", NULL},
    {"DB7", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 38, 7, 32, MF_HEX,
        "DB(7) value", NULL},
    {"DB8", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 42, 7, 32, MF_HEX,
        "DB(8) value", NULL},
    {"DB9", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 46, 7, 32, MF_HEX,
        "DB(9) value", NULL},
    {"DB10", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 50, 7, 32, MF_HEX,
        "DB(10) value", NULL},
    {"DB11", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 54, 7, 32, MF_HEX,
        "DB(11) value", NULL},
    {"DB12", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 58, 7, 32, MF_HEX,
        "DB(12) value", NULL},
    {"DB13", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 62, 7, 32, MF_HEX,
        "DB(13) value", NULL},
    {"DB14", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 66, 7, 32, MF_HEX,
        "DB(14) value", NULL},
    {"DB15", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 70, 7, 32, MF_HEX,
        "DB(15) value", NULL},
    {"P_CRCA", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 74, 7, 32, MF_HEX,
        "P_CRCA value", NULL},
    {"P1", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 78, 7, 32, MF_HEX,
        "P1 value", NULL},
    {"BSY", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 82, 7, 32, MF_HEX,
        "BSY value", NULL},
    {"SEL", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 86, 7, 32, MF_HEX,
        "SEL value", NULL},
    {"RST", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 90, 7, 32, MF_HEX,
        "RST value", NULL},
    {"REQ", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 94, 7, 32, MF_HEX,
        "REQ value", NULL},
    {"ACK", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 98, 7, 32, MF_HEX,
        "ACK value", NULL},
    {"ATN", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 102, 7, 32, MF_HEX,
        "ATN value", NULL},
    {"C_D", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 106, 7, 32, MF_HEX,
        "C/D value", NULL},
    {"I_O", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 110, 7, 32, MF_HEX,
        "I/O value", NULL},
    {"MSG", PROT_SPEC_PORT_MP, MSP_SPI_STC, -1, 114, 7, 32, MF_HEX,
        "MSG value", NULL},

    /* negotiated settings subpage [0x19,0x3] spi4 */
    {"PPID_3", PROT_SPEC_PORT_MP, MSP_SPI_NS, -1, 5, 3, 4, 0,
        "Port's (transport) protocol identifier",
        PROTO_IDENT_STR },
    {"TPF", PROT_SPEC_PORT_MP, MSP_SPI_NS, -1, 6, 7, 8, 0,
        "Transfer period factor", NULL},
    {"RAO", PROT_SPEC_PORT_MP, MSP_SPI_NS, -1, 8, 7, 8, 0,
        "REQ/ACK offset", NULL},
    {"TWE", PROT_SPEC_PORT_MP, MSP_SPI_NS, -1, 9, 7, 8, 0,
        "Transfer width exponent", NULL},
    {"POB", PROT_SPEC_PORT_MP, MSP_SPI_NS, -1, 10, 6, 7, 0,
        "Protocol option bits", NULL},
    {"TM", PROT_SPEC_PORT_MP, MSP_SPI_NS, -1, 11, 3, 2, 0,
        "Transceiver mode", NULL},
    {"SPE", PROT_SPEC_PORT_MP, MSP_SPI_NS, -1, 11, 1, 1, 0,
        "Sent PCOMP_EN bit (for current I_T nexus)", NULL},
    {"RPE", PROT_SPEC_PORT_MP, MSP_SPI_NS, -1, 11, 0, 1, 0,
        "Received PCOMP_EN bit (for current I_T nexus)", NULL},

    /* report transfer capabilities subpage [0x19,0x4] spi4 */
    {"PPID_4", PROT_SPEC_PORT_MP, MSP_SPI_RTC, -1, 5, 3, 4, 0,
        "Port's (transport) protocol identifier",
        PROTO_IDENT_STR },
    {"MTPF", PROT_SPEC_PORT_MP, MSP_SPI_RTC, -1, 6, 7, 8, 0,
        "Minimum transfer period factor", NULL},
    {"MRAO", PROT_SPEC_PORT_MP, MSP_SPI_RTC, -1, 8, 7, 8, 0,
        "Maximum REQ/ACK offset", NULL},
    {"MTWE", PROT_SPEC_PORT_MP, MSP_SPI_RTC, -1, 9, 7, 8, 0,
        "Maximum transfer width exponent", NULL},
    {"POBS", PROT_SPEC_PORT_MP, MSP_SPI_RTC, -1, 10, 7, 8, 0,
        "Protocol option bits supported", NULL},

    {NULL, 0, 0, 0, 0, 0, 0, 0, NULL, NULL},
};

static struct sdparm_mode_page_item sdparm_mitem_srp_arr[] = {
    /* disconnect-reconnect mode page [0x2] srp */
    {"MBS", DISCONNECT_MP, 0, -1, 10, 7, 16, MF_COMMON | MF_CLASH_OK,
        "Maximum burst size (512 bytes)", NULL},
    {"EMDP", DISCONNECT_MP, 0, -1, 12, 7, 1, MF_CLASH_OK,
        "Enable modify data pointers", NULL},
    {"FBS", DISCONNECT_MP, 0, -1, 14, 7, 16, MF_CLASH_OK,
        "First burst size (512 bytes)", NULL},  /* srp2r00 */

    {NULL, 0, 0, 0, 0, 0, 0, 0, NULL, NULL},
};

static struct sdparm_mode_page_item sdparm_mitem_sas_arr[] = {
    /* disconnect-reconnect mode page [0x2] sas/spl */
    {"BITL", DISCONNECT_MP, 0, -1, 4, 7, 16, MF_COMMON,
        "Bus inactivity time limit (100us)", NULL},
    {"MCTL", DISCONNECT_MP, 0, -1, 8, 7, 16, MF_COMMON,
        "Maximum connect time limit (100us)", NULL},
    {"MBS", DISCONNECT_MP, 0, -1, 10, 7, 16, MF_COMMON | MF_CLASH_OK,
        "Maximum burst size (512 bytes)", NULL},
        /* obsoleted spl3r2, re-instated spl3r3 */
    {"FBS", DISCONNECT_MP, 0, -1, 14, 7, 16, MF_CLASH_OK,
        "First burst size (512 bytes)", NULL},

    /* protocol specific logical unit mode page [0x18] sas/spl */
    {"LUPID", PROT_SPEC_LU_MP, 0, -1, 2, 3, 4, MF_COMMON | MF_CLASH_OK,
        "Logical unit's (transport) protocol identifier",
        PROTO_IDENT_STR },
    {"TLR", PROT_SPEC_LU_MP, 0, -1, 2, 4, 1, 0,
        "Transport layer retries (supported)", NULL},

    /* protocol specific port mode page [0x19] sas/spl */
    {"PPID", PROT_SPEC_PORT_MP, 0, -1, 2, 3, 4, MF_COMMON | MF_CLASH_OK,
        "Port's (transport) protocol identifier",
        PROTO_IDENT_STR },
    {"CAWT", PROT_SPEC_PORT_MP, 0, -1, 2, 6, 1, 0,
        "Continue AWT (arbitration wait time (timer))", NULL},
    {"BAE", PROT_SPEC_PORT_MP, 0, -1, 2, 5, 1, 0,
        "Broadcast asynchronous event", NULL},
    {"RLM", PROT_SPEC_PORT_MP, 0, -1, 2, 4, 1, MF_COMMON,
        "Ready LED meaning",
        "0: usually on, flash when command processing; off when stopped\t"
        "1: usually off, flash when command processing"},
    {"ITNLT", PROT_SPEC_PORT_MP, 0, -1, 4, 7, 16, MF_COMMON,
        "I_T nexus loss time (ms)",
        "0: vendor specific\t"
        "0ffffh (-1): never recognize IT nexus loss"},
    {"IRT", PROT_SPEC_PORT_MP, 0, -1, 6, 7, 16, MF_COMMON,
        "Initiator response timeout (ms)",
        "0: disable initiator response timeout timer"},
    {"RTOL", PROT_SPEC_PORT_MP, 0, -1, 8, 7, 16, MF_COMMON,
        "Reject to open limit (10 us)",         /* added in sas2r14 */
        "0: vendor specific"},

    /* phy control and discover mode page [0x19,0x1] sas/spl */
    {"PPID_1", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 5, 3, 4, 0,
        "Port's (transport) protocol identifier",
        PROTO_IDENT_STR },
    {"GENC", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 6, 7, 8, 0,
        "Generation code", "0: unknown, 1..255: valid"},
    {"NOP", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 7, 7, 8, MF_COMMON,
        "Number of phys", "one descriptor per phy"},
    /* Phy mode descriptor starts here, <start_byte> relative to start of
     * mode page (i.e. 8 more than phy mode descriptor table) */
    {"PHID", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 9, 7, 8, 0,
        "Phy identifier", NULL},
    {"ADT", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 12, 6, 3, 0,
        "Attached device type",
        "0: no device attached; 1: end device\t"
        "2: expander device; " /* in SAS-1.1 this was a "edge expander" */
        "3: expander device (fanout, SAS-1.1)"}, /* obsolete in SAS-2 */
    {"AREAS", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 12, 3, 4, 0,
        "Attached reason (other end did link reset)",
        "0: unknown; 1: power on; 2: hard reset; 3: SMP phy control\t"
        "4: loss of dword sync; 5: mux problem; ..."},
    {"REAS", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 13, 7, 4, 0,
        "Reason (for starting link reset)",
        "0: unknown; 1: power on; 2: hard reset; 3: SMP phy control\t"
        "4: loss of dword sync; 5: mux problem; ..."},
    {"NLLR", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 13, 3, 4, 0,
        "Negotiated logical link rate",         /* sas2r07 */
        "0: unknown; 1: disabled; 2: phy reset problem; 3: spinup hold\t"
        "4: port selector; 5: resetting; 6: attached unsupported\t"
        "8: 1.5 Gbps; 9: 3 Gbps; 10: 6 Gbps; 11: 12 Gbps"},
    {"ASIP", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 14, 3, 1, 0,
        "Attached SSP initiator port", NULL},
    {"ATIP", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 14, 2, 1, 0,
        "Attached STP initiator port", NULL},
    {"AMIP", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 14, 1, 1, 0,
        "Attached SMP initiator port", NULL},
    {"ASTP", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 15, 3, 1, 0,
        "Attached SSP target port", NULL},
    {"ATTP", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 15, 2, 1, 0,
        "Attached STP target port", NULL},
    {"AMTP", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 15, 1, 1, 0,
        "Attached SMP target port", NULL},
    {"SASA", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 16, 7, 64,
         MF_HEX | MF_COMMON, "SAS address", NULL},
    {"ASASA", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 24, 7, 64,
         MF_HEX | MF_COMMON, "Attached SAS address", NULL},
    {"APHID", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 32, 7, 8, 0,
        "Attached phy identifier", NULL},
    {"APOWCAP", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 33, 6, 2, 0,
        "Attached power capable", "0: not; 1: can consume; 2: can source"},
    {"ASLCAP", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 33, 4, 1, 0,
        "Attached slumber capable", NULL},
    {"APACAP", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 33, 3, 1, 0,
        "Attached partial capable", NULL},
    {"AIZPER", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 33, 2, 1, 0,
        "Attached inside ZPSDS persistent", NULL},
    {"AREQIZ", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 33, 1, 1, 0,
        "Attached request inside ZPSDS", NULL},
    {"ABRCAP", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 33, 0, 1, 0,
        "Attached break reply capable", NULL},
    {"PMILR", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 40, 7, 4, 0,
        "Programmed minimum link rate",
        "0: not programmed; 8: 1.5 Gbps; 9: 3 Gbps; 10: 6 Gbps; 11: 12 Gbps"},
    {"HMILR", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 40, 3, 4, 0,
        "Hardware minimum link rate",
        "8: 1.5 Gbps; 9: 3 Gbps; 10: 6 Gbps; 11: 12 Gbps"},
    {"PMALR", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 41, 7, 4, 0,
        "Programmed maximum link rate",
        "0: not programmed; 8: 1.5 Gbps; 9: 3 Gbps; 10: 6 Gbps; 11: 12 Gbps"},
    {"HMALR", PROT_SPEC_PORT_MP, MSP_SAS_PCD, -1, 41, 3, 4, 0,
        "Hardware maximum link rate",
        "8: 1.5 Gbps; 9: 3 Gbps; 10: 6 Gbps; 11: 12 Gbps"},

    /* shared port control mode page [0x19,0x2] sas/spl */
    {"PPID_2", PROT_SPEC_PORT_MP, MSP_SAS_SPC, -1, 5, 3, 4, 0,
        "Port's (transport) protocol identifier",
        PROTO_IDENT_STR },
    {"PLT", PROT_SPEC_PORT_MP, MSP_SAS_SPC, -1, 6, 7, 16, 0,
        "Power loss timeout(ms)", NULL},
    {"PGRATO", PROT_SPEC_PORT_MP, MSP_SAS_SPC, -1, 9, 7, 8, 0,
        "Power grant timeout(sec)", NULL},

    /* SAS-2 Enhanced phy mode page [0x19,0x3] sas/spl */
    {"PPID_3", PROT_SPEC_PORT_MP, MSP_SAS_E_PHY, -1, 5, 3, 4, 0,
        "Port's (transport) protocol identifier",
        PROTO_IDENT_STR },
    {"GENC_1", PROT_SPEC_PORT_MP, MSP_SAS_E_PHY, -1, 6, 7, 8, 0,
        "Generation code", "0: unknown, 1..255: valid"},
    {"NOP_1", PROT_SPEC_PORT_MP, MSP_SAS_E_PHY, -1, 7, 7, 8, 0,
        "Number of phys", "one descriptor per phy"},
    /* Phy mode descriptor starts here, <start_byte> relative to start of
     * mode page (i.e. 8 more than phy mode descriptor table) */
    {"PHID_1", PROT_SPEC_PORT_MP, MSP_SAS_E_PHY, -1, 9, 7, 8, 0,
        "Phy identifier", NULL},
    {"PPCAP", PROT_SPEC_PORT_MP, MSP_SAS_E_PHY, -1, 12, 7, 32, MF_HEX,
        "Programmed phy capabilities", NULL},
    {"CPCAP", PROT_SPEC_PORT_MP, MSP_SAS_E_PHY, -1, 16, 7, 32, MF_HEX,
        "Current phy capabilities", NULL},
    {"APCAP", PROT_SPEC_PORT_MP, MSP_SAS_E_PHY, -1, 20, 7, 32, MF_HEX,
        "Attached phy capabilities", NULL},
    {"OPT_M_EN", PROT_SPEC_PORT_MP, MSP_SAS_E_PHY, -1, 26, 5, 1, 0,
        "Optical mode enabled", NULL},
    {"N_SSC", PROT_SPEC_PORT_MP, MSP_SAS_E_PHY, -1, 26, 4, 1, 0,
        "Negotiated spread spectrum clocking", NULL},
    {"NPLR", PROT_SPEC_PORT_MP, MSP_SAS_E_PHY, -1, 26, 3, 4, 0,
        "Negotiated physical link rate",
        "0: unknown; 1: disabled; 2: phy reset problem; 3: spinup hold\t"
        "4: port selector; 5: resetting; 6: attached unsupported\t"
        "8: 1.5 Gbps; 9: 3 Gbps; 10: 6 Gbps; 11: 12 Gbps"},
    {"EN_SL", PROT_SPEC_PORT_MP, MSP_SAS_E_PHY, -1, 27, 2, 1, 0,
        "Enable slumber phy power condition", NULL},
    {"EN_PA", PROT_SPEC_PORT_MP, MSP_SAS_E_PHY, -1, 27, 1, 1, 0,
        "Enable partial phy power condition", NULL},
    {"HMS", PROT_SPEC_PORT_MP, MSP_SAS_E_PHY, -1, 27, 0, 1, 0,
        "Hardware muxing supported", NULL},

    {NULL, 0, 0, 0, 0, 0, 0, 0, NULL, NULL},
};

/* fixed length, indexed by transport protocol number */
struct sdparm_transport_pair sdparm_transport_mp[] = {
    {sdparm_fcp_mode_pg, sdparm_mitem_fcp_arr}, /* 0 */
    {sdparm_spi_mode_pg, sdparm_mitem_spi_arr},
    {NULL, NULL},
    {NULL, NULL},
    {sdparm_srp_mode_pg, sdparm_mitem_srp_arr},
    {NULL, NULL},
    {sdparm_sas_mode_pg, sdparm_mitem_sas_arr},
    {NULL, NULL},
    {NULL, NULL},       /* 8 */
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},
    {NULL, NULL},       /* 15 */
};


const char * sdparm_pdt_doc_strs[] = {
    /* 0 */ "SBC-3",    /* disk */
    "SSC-3",            /* tape */
    "SSC",              /* printer */
    "SPC-2",            /* processor */
    "SBC",              /* write once optical disk */
    /* 5 */ "MMC-5",    /* cd/dvd */
    "SCSI-2",           /* scanner */
    "SBC-2",            /* optical memory device */
    "SMC-2",            /* medium changer */
    "SCSI-2",           /* communications */
    /* 0xa */ "SCSI-2", /* graphics [0xa] */
    "SCSI-2",           /* graphics [0xb] */
    "SCC-2",            /* storage array controller */
    "SES-2",            /* enclosure services device */
    "RBC",              /* simplified direct access device */
    "OCRW",             /* optical card reader/writer device */
    /* 0x10 */ "BCC",   /* bridge controller commands */
    "OSD-2",            /* object based storage */
    "ADC",              /* automation/driver interface */
    "SPC-4",            /* security manager device */
    "unknown",          /* 0x14 */
    "unknown", "unknown", "unknown", "unknown",
    "unknown", "unknown", "unknown", "unknown",
    "unknown",          /* 0x1d */
    "SPC-4",            /* well known logical unit */
    "SPC-4",            /* no physical device on this lu */
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
    "Target port",      /* that received request; unless SCSI ports VPD */
    "Target device that contains addressed lu",
    "Reserved [0x3]",
};

const char * sdparm_desig_type_arr[] =
{
    "vendor specific [0x0]",
    "T10 vendor identification",
    "EUI-64 based",
    "NAA",
    "Relative target port",
    "Target port group",        /* spc4r09: _primary_ target port group */
    "Logical unit group",
    "MD5 logical unit identifier",
    "SCSI name string",
    "Protocol specific port identifier",        /* spc4r36 */
    "Reserved [0xa]", "Reserved [0xb]",
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

const char * sdparm_network_service_type_arr[] =
{
    "unspecified",
    "storage configuration service",
    "diagnostics",
    "status",
    "logging",
    "code download",
    "administrative configuration service",
    "reserved[0x7]", "reserved[0x8]", "reserved[0x9]",
    "reserved[0xa]", "reserved[0xb]", "reserved[0xc]", "reserved[0xd]",
    "reserved[0xe]", "reserved[0xf]", "reserved[0x10]", "reserved[0x11]",
    "reserved[0x12]", "reserved[0x13]", "reserved[0x14]", "reserved[0x15]",
    "reserved[0x16]", "reserved[0x17]", "reserved[0x18]", "reserved[0x19]",
    "reserved[0x1a]", "reserved[0x1b]", "reserved[0x1c]", "reserved[0x1d]",
    "reserved[0x1e]", "reserved[0x1f]",
};

const char * sdparm_mode_page_policy_arr[] =
{
    "shared",
    "per target port",
    "per initiator port (obsolete)",    /* made obsolete in SPC-4 */
    "per I_T nexus",
};

struct sdparm_command_t sdparm_command_arr[] =
{
    {CMD_CAPACITY, "capacity", "ca", NULL},
    {CMD_EJECT, "eject", "ej", NULL},
    {CMD_LOAD, "load", "lo", NULL},
    {CMD_PROFILE, "profile", "pr", NULL},
    {CMD_READY, "ready", "re", NULL},
    {CMD_SENSE, "sense", "se", NULL},
    {CMD_SPEED, "speed", "sp", "new_speed_kbps"},
    {CMD_START, "start", "sta", NULL},
    {CMD_STOP, "stop", "sto", NULL},
    {CMD_SYNC, "sync", "sy", NULL},
    {CMD_UNLOCK, "unlock", "un", NULL},
    {-1, NULL, NULL, NULL},
};

struct sdparm_val_desc_t sdparm_profile_arr[] = {
        {0x0, "No current profile"},
        {0x1, "Non-removable disk (obs)"},
        {0x2, "Removable disk"},
        {0x3, "Magneto optical erasable"},
        {0x4, "Optical write once"},
        {0x5, "AS-MO"},
        {0x8, "CD-ROM"},
        {0x9, "CD-R"},
        {0xa, "CD-RW"},
        {0x10, "DVD-ROM"},
        {0x11, "DVD-R sequential recording"},
        {0x12, "DVD-RAM"},
        {0x13, "DVD-RW restricted overwrite"},
        {0x14, "DVD-RW sequential recording"},
        {0x15, "DVD-R dual layer sequental recording"},
        {0x16, "DVD-R dual layer jump recording"},
        {0x17, "DVD-RW dual layer"},
        {0x18, "DVD-Download disc recording"},
        {0x1a, "DVD+RW"},
        {0x1b, "DVD+R"},
        {0x20, "DDCD-ROM"},
        {0x21, "DDCD-R"},
        {0x22, "DDCD-RW"},
        {0x2a, "DVD+RW dual layer"},
        {0x2b, "DVD+R dual layer"},
        {0x40, "BD-ROM"},
        {0x41, "BD-R SRM"},
        {0x42, "BD-R RRM"},
        {0x43, "BD-RE"},
        {0x50, "HD DVD-ROM"},
        {0x51, "HD DVD-R"},
        {0x52, "HD DVD-RAM"},
        {0x53, "HD DVD-RW"},
        {0x58, "HD DVD-R dual layer"},
        {0x5a, "HD DVD-RW dual layer"},
        {0xffff, "Non-conforming profile"},
        {-1, NULL},
};

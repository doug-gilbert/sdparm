/*
 * Copyright (c) 2005-2023, Douglas Gilbert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sg_lib.h"
#include "sdparm.h"


/*
 * sdparm is a utility program to access and change SCSI device
 * (logical unit) mode page fields and do some other housekeeping.
 *
 * This file contains vendor data tables that may be useful for other
 * programs.
 */


/* Vendor specific mode pages */
const struct sdparm_vendor_name_t sdparm_vendor_id[] = {
    {VENDOR_SEAGATE, "sea", "Seagate disk"},    /* 0 */
    {VENDOR_HITACHI, "hit", "Hitachi disk"},
    {VENDOR_HITACHI, "wdc", "Hitachi disk->HGST->WDC"},
    {VENDOR_MAXTOR, "max", "Maxtor disk"},
    {VENDOR_FUJITSU, "fuj", "Fujitsu disk"},
    {VENDOR_NONE, "none", "maps back to generic mode pages"},
    {VENDOR_LTO5, "lto5", "LTO-5 tape drive (IBM, HP)"},
    {VENDOR_LTO6, "lto6", "LTO-6 tape drive (IBM, HP)"},
    {VENDOR_NVME, "nvme", "NVMe, SNTL in library"},
    {VENDOR_SG, "sg", "sg3_utils package defined"},     /* 8 */
    {0, NULL, NULL},
};

static const struct sdparm_mp_name_t sdparm_v_seagate_mode_pg[] = {
    {UNIT_ATTENTION_MP, 0, 0, 0, "ua", "Unit attention (seagate)", NULL},
    {0, 0, 0, 0, NULL, NULL, NULL},
};

// MF_J_NPARAM_DESC
// MF_J_USE_DESC

/* Seagate make spinning magnetic disks, called "hard disks" abbreviated below
 * as 'hd' in the description. They also make Solid-State Drives (SSDs) which
 * are abbreviated below as 'ssd'. If a field is support by both their hard
 * disks and SSDs then neither 'hd' nor 'ssd' appears in the description. */
static const struct sdparm_mp_item_t sdparm_mitem_v_seagate_arr[] = {
    /* Unit attention page, ua [0x0] Seagate */
    {"PM", UNIT_ATTENTION_MP, 0, 0, 2, 7, 1, MF_COMMON,
        "Performance Mode (hd)", NULL,
        "0: adaptive cache ('server mode')\t"
        "1: number of cache segments as per caching page ('desktop mode')"},
    {"SSM", UNIT_ATTENTION_MP, 0, 0, 2, 6, 1, 0,
        "Synchronous select mode (SPI)", NULL,
        "0: drive will not initiate WDTR or SDTR\t"
        "1: drive may initiate WDTR or SDTR"},
    {"IL", UNIT_ATTENTION_MP, 0, 0, 2, 5, 1, MF_COMMON | MF_J_USE_DESC,
        "Inquiry length", NULL,
        "0: more than 36 bytes in response\t"
        "1: 36 byte response as per SCSI-2"},
    {"UA", UNIT_ATTENTION_MP, 0, 0, 2, 4, 1, MF_COMMON | MF_J_USE_DESC,
        "Unit attention", NULL,
        "0: unit attention condition for all initiators after reset\t"
        "1: no check condition with unit attention after reset"},
    {"DFUA", UNIT_ATTENTION_MP, 0, 0, 2, 3, 1, 0,
        "Disable force unit access (FUA) (obsolete)", NULL,
        "0: honour FUA bit setting on READ and WRITE commands\t"
        "1: ignore FUA bit setting"},
    {"ROUND", UNIT_ATTENTION_MP, 0, 0, 2, 2, 1, 0,
        "Reporting of log parameter rounding (wrap around)", NULL,
        "0: do not report (silently round)\t"
        "1: report rounding (as per SPC-4)"},
    {"STRICT", UNIT_ATTENTION_MP, 0, 0, 2, 1, 1, MF_COMMON,
        "Strict when trying to alter unchangeable mode page fields", NULL,
        "0: silently ignore\t"
        "1: report as error"},
    {"SCSI2", UNIT_ATTENTION_MP, 0, 0, 2, 0, 1, MF_COMMON,
        "SCSI-2 lengths for control and caching mode pages", "scsi_2",
        "0: as per recent standards\t"
        "1: SCSI-2 lengths: control, 6; caching, 10"},
    {"DAR", UNIT_ATTENTION_MP, 0, 0, 3, 7, 1, 0,
        "Deferred auto reallocation (hd)", NULL,
        "0: disabled\t"
        "1: enabled: unrecoverable read LBA remembered, re-assigned on next "
        "write"},
    {"SSEEK", UNIT_ATTENTION_MP, 0, 0, 3, 6, 1, MF_J_NPARAM_DESC,
        "Self seek (hd)", NULL,
        "0: off (normal operating mode)\t"
        "1: enter self seek mode (test power dissipation, acoustics, etc)"},
    {"SDTE", UNIT_ATTENTION_MP, 0, 0, 3, 1, 1, 0,
        "SMART depopulation trigger enable (hd)", NULL, NULL},
    {"VJIT_DIS", UNIT_ATTENTION_MP, 0, 0, 4, 7, 1, MF_J_NPARAM_DESC,
        "VJIT disabled (hd)", NULL,
        "0: follow settings of JIT0, JIT1, JIT2 and JIT3\t"
        "1: ignore settings of JIT0, JIT1, JIT2 and JIT3"},
    {"JIT3", UNIT_ATTENTION_MP, 0, 0, 4, 3, 1, 0,
        "Just in time 3, slowest seek type (hd)", NULL,
        "0: can not use this seek type in seek speed algorithm\t"
        "1: can use this seek type in seek speed algorithm"},
    {"JIT2", UNIT_ATTENTION_MP, 0, 0, 4, 2, 1, 0,
        "Just in time 2, second slowest seek type (hd)", NULL,
        "0: can not use this seek type in seek speed algorithm\t"
        "1: can use this seek type in seek speed algorithm"},
    {"JIT1", UNIT_ATTENTION_MP, 0, 0, 4, 1, 1, 0,
        "Just in time 1, second fastest seek type (hd)", NULL,
        "0: can not use this seek type in seek speed algorithm\t"
        "1: can use this seek type in seek speed algorithm"},
    {"JIT0", UNIT_ATTENTION_MP, 0, 0, 4, 0, 1, 0,
        "Just in time 0, fastest seek type (hd)", NULL,
        "0: can not use this seek type in seek speed algorithm\t"
        "1: can use this seek type in seek speed algorithm"},
    {"TTE", UNIT_ATTENTION_MP, 0, 0, 6, 0, 1, 0,
        "Thermal throttle enable (ssd)", NULL,
        "0: drive activity is not limited, based on temperature\t"
        "1: drive activity is limited, based on temperature"},

    {NULL, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL},
};

static const struct sdparm_mp_name_t sdparm_v_hitachi_mode_pg[] = {
    {UNIT_ATTENTION_MP, 0, 0, 0, "vup", "Vendor unique parameters (hitachi)",
        NULL},
    {0, 0, 0, 0, NULL, NULL, NULL},
};

/* Western Digital (WD) and Hitachi are synonymous */
static const struct sdparm_mp_item_t sdparm_mitem_v_hitachi_arr[] = {
    /* Vendor unique parameters page, vup [0x0] Hitachi/HGST/WDC */
    {"MRG", UNIT_ATTENTION_MP, 0, 0, 2, 3, 1, 0,
        "Merge Glist into Plist (during format)", NULL, NULL},
    {"VGMDE", UNIT_ATTENTION_MP, 0, 0, 3, 6, 1, MF_COMMON,
        "Veggie mode (do random seeks when idle)", NULL, NULL},
    {"RRNDE", UNIT_ATTENTION_MP, 0, 0, 3, 1, 1, 0,
        "Report recovered non data errors (when PER set)", NULL, NULL},
    {"DNS", UNIT_ATTENTION_MP, 0, 0, 4, 2, 1, 0,
        "Disable notify for standby (obsolete)", NULL, NULL},
    {"LRPMS", UNIT_ATTENTION_MP, 0, 0, 4, 1, 1, 0,
        "Low RPM standby (obsolete)", NULL, NULL},
    {"LCS", UNIT_ATTENTION_MP, 0, 0, 4, 0, 1, 0,
        "Limited current startup (obsolete)", NULL, NULL},
    {"FDD", UNIT_ATTENTION_MP, 0, 0, 5, 4, 1, 0,
        "Format degraded disable (reporting for Test Unit Ready)", NULL, NULL},
    {"CAEN", UNIT_ATTENTION_MP, 0, 0, 5, 1, 1, MF_COMMON,
        "Command aging enable", NULL, NULL},
    {"IGRA", UNIT_ATTENTION_MP, 0, 0, 6, 7, 1, MF_COMMON,
        "Ignore reassigned LBA (when RC also set)", NULL, NULL},
    {"AVERP", UNIT_ATTENTION_MP, 0, 0, 6, 6, 1, MF_COMMON,
        "AV ERP mode (maximum retry count for read errors)", NULL,
        "0: use default (ignore RRC)\t"
        "1: use RRC field"},
    {"OCT", UNIT_ATTENTION_MP, 0, 0, 6, 3, 12, MF_J_USE_DESC,
        "Overall command timer, 0 -> disabled (50 ms)", NULL, NULL},
    {"TT", UNIT_ATTENTION_MP, 0, 0, 9, 7, 8, MF_J_NPARAM_DESC,
        "Temperature threshold (celsius), 0 -> 85C", NULL, NULL},
    {"CAL", UNIT_ATTENTION_MP, 0, 0, 10, 7, 16, MF_J_NPARAM_DESC,
        "Command aging limit (50 ms)", NULL, NULL},
    {"RRT", UNIT_ATTENTION_MP, 0, 0, 12, 7, 8, 0,
        "Read reporting threshold for read recovered errors when PER set",
        "read_reporting_threshold", NULL},
    {"WRT", UNIT_ATTENTION_MP, 0, 0, 13, 7, 8, 0,
        "Write reporting threshold for write recovered errors when PER set",
        "write_reporting_threshold", NULL},
    {"DRRT", UNIT_ATTENTION_MP, 0, 0, 14, 7, 1, 0,
        "Disable restore reassign target", NULL,
        "0: REASSIGN attempts to recovery old data\t"
        "1: REASSIGN ignores old data"},
    {"FFMT", UNIT_ATTENTION_MP, 0, 0, 14, 3, 1, 0,
        "Fast format enable, format without writes to customer media", NULL,
        NULL},
    {"FCERT", UNIT_ATTENTION_MP, 0, 0, 15, 5, 1, 0,
        "Format certification (enable)", NULL, NULL},
    {"CERT_RDP", UNIT_ATTENTION_MP, 0, 0, 15, 3, 1, 0,
        "RDP certification (enable)", "certify_rdp_bit", NULL},

    {NULL, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL},
};

static const struct sdparm_mp_name_t sdparm_v_maxtor_mode_pg[] = {
    {UNIT_ATTENTION_MP, 0, 0, 0, "uac", "Unit attention condition (maxtor)",
        NULL},

    {0, 0, 0, 0, NULL, NULL, NULL},
};

static const struct sdparm_mp_item_t sdparm_mitem_v_maxtor_arr[] = {
    /* Unit attention page condition, uac [0x0] Maxtor */
    {"DUA", UNIT_ATTENTION_MP, 0, 0, 2, 4, 1, MF_COMMON,
        "Disable unit attention", NULL, NULL},

    {NULL, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL},
};

static const struct sdparm_mp_name_t sdparm_v_fujitsu_mode_pg[] = {
    {0x21, 0, 0, 0, "aerp", "Additional error recovery parameters (fujitsu)",
        NULL},

    {0, 0, 0, 0, NULL, NULL, NULL},
};

static const struct sdparm_mp_item_t sdparm_mitem_v_fujitsu_arr[] = {
    /* Additional error recovery parameters page, aerp [0x21] Fujitsu */
    {"RDSE", 0x21, 0, 0, 2, 3, 4, MF_COMMON,
        "Retries during a seek error", NULL, "0: no repositioning retries"},

    {NULL, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL},
};

static const struct sdparm_mp_name_t sdparm_v_lto5_mode_pg[] = {
    {0x24, 0, PDT_TAPE, 0, "l5vs", "Vendor specific (LTO-5)",
        NULL},
    {0x2f, 0, PDT_TAPE, 0, "l5bc", "Behaviour configuration (LTO-5)",
        NULL},
    {0x3b, 0, PDT_TAPE, 0, "l5sno", "Serial number override (LTO-5)",
        NULL},
    {0x3c, 0, PDT_TAPE, 0, "l5dt", "Device time (LTO-5)",
        NULL},
    {0x3d, 0, PDT_TAPE, 0, "l5er", "Extended reset (LTO-5)",
        NULL},
    {0x3e, 0, PDT_TAPE, 0, "l5cde", "cd-rom emulation / disaster recovery "
        "(LTO-5)", NULL},
    /* Device attribute settings [0x30] LTO-5 */

    {0, 0, 0, 0, NULL, NULL, NULL},
};

static const struct sdparm_mp_item_t sdparm_mitem_v_lto5_arr[] = {
    /* Vendor specific page [0x24] LTO-5 */
    {"ENCR_E", 0x24, 0, PDT_TAPE, 7, 3, 1, MF_COMMON,
        "Encryption enable", NULL, NULL},
    {"FIPS", 0x24, 0, PDT_TAPE, 7, 1, 1, MF_COMMON,
        "FIPS level of code", NULL, NULL},
    {"ENCR_C", 0x24, 0, PDT_TAPE, 7, 0, 1, MF_COMMON,
        "Encryption capable", NULL, NULL},
    /* Behaviour configuration [0x2f] LTO-5 */
    {"FE_BEH", 0x2f, 0, PDT_TAPE, 2, 7, 8, MF_COMMON | MF_J_USE_DESC,
        "Fence behavior", NULL, NULL},
    {"CL_BEH", 0x2f, 0, PDT_TAPE, 3, 7, 8, MF_COMMON | MF_J_USE_DESC,
        "Clean behavior", NULL, NULL},
    {"WO_BEH", 0x2f, 0, PDT_TAPE, 4, 7, 8, MF_COMMON | MF_J_USE_DESC,
        "Worm behavior", NULL, NULL},
    {"SD_BEH", 0x2f, 0, PDT_TAPE, 5, 7, 8, MF_COMMON | MF_J_USE_DESC,
        "Sense data behavior", NULL, NULL},
    {"CCDM", 0x2f, 0, PDT_TAPE, 6, 2, 1, MF_COMMON,
        "Check condition for dead media", NULL, NULL},
    {"DDEOR", 0x2f, 0, PDT_TAPE, 6, 1, 1, MF_COMMON,
        "Disable deferred error on rewind", NULL, NULL},
    {"CLNCHK", 0x2f, 0, PDT_TAPE, 6, 0, 1, MF_COMMON,
        "Clean check", NULL, NULL},
    {"DFMRDL", 0x2f, 0, PDT_TAPE, 7, 0, 1, MF_COMMON,
        "Disable field microcode replacement down level", NULL, NULL},
    {"UOE_C", 0x2f, 0, PDT_TAPE, 8, 5, 2, MF_COMMON,
        "Unload on error - cleaner", NULL, NULL},
    {"UOE_F", 0x2f, 0, PDT_TAPE, 8, 3, 2, MF_COMMON,
        "Unload on error - FMR", NULL, NULL},
    {"UOE_D", 0x2f, 0, PDT_TAPE, 8, 1, 2, MF_COMMON,
        "Unload on error - data", NULL, NULL},
    {"TA10", 0x2f, 0, PDT_TAPE, 9, 0, 1, MF_COMMON,
        "Tape alert 10h", NULL, NULL},
    /* Serial number override [0x3b] LTO-5, HP */
    {"MSN", 0x3b, 0, PDT_TAPE, 2, 1, 2, MF_COMMON | MF_CLASH_OK,
        "Non-auto", NULL, "0: not reported\t1: manufacturer's default SN\t2: "
        "not reported\t3: non-default Serial Number"},
    {"SN0_7", 0x3b, 0, PDT_TAPE, 6, 7, 8 * 8, MF_HEX | MF_CLASH_OK,
     "Serial Number, bytes 0 to 7", "serial_number_0_7",
     "ASCII hex in range 0x20 to 0x7f"},
    {"SN8_11", 0x3b, 0, PDT_TAPE, 14, 7, 4 * 8, MF_HEX | MF_CLASH_OK,
     "Serial Number, bytes 8 to 11", "serial_number_8_11",
     "ASCII hex in range 0x20 to 0x7f"},
    /* Device time [0x3c] LTO-5, HP */
    {"LT_VAL", 0x3c, 0, PDT_TAPE, 2, 2, 1, MF_COMMON | MF_CLASH_OK,
     "Library time valid", "lt", NULL},
    {"WT_VAL", 0x3c, 0, PDT_TAPE, 2, 1, 1, MF_COMMON | MF_CLASH_OK,
     "World time valid", "wt", NULL},
    {"PT_VAL", 0x3c, 0, PDT_TAPE, 2, 0, 1, MF_COMMON | MF_CLASH_OK,
     "Power-on time valid", "pt", NULL},
    {"CP_COUNT", 0x3c, 0, PDT_TAPE, 6, 7, 2 * 8,
     MF_COMMON | MF_CLASH_OK | MF_J_USE_DESC,
     "Current power-on count", NULL, NULL},
    {"UTC", 0x3c, 0, PDT_TAPE, 14, 1, 1, MF_COMMON | MF_CLASH_OK,
     "UTC", NULL, "0: local time zone\t1: UTC"},
    {"NTP", 0x3c, 0, PDT_TAPE, 14, 0, 1, MF_COMMON | MF_CLASH_OK,
     "NTP", NULL, "0: unsure if NTP synced\t1: NTP synced"},
    {"WOR_TIME", 0x3c, 0, PDT_TAPE, 16, 7, 4 * 8,
     MF_COMMON | MF_CLASH_OK | MF_J_USE_DESC,
     "World time", NULL, "seconds since 00:00:00, 1 January 1970 UTC"},
    {"LT_HR", 0x3c, 0, PDT_TAPE, 23, 7, 8, MF_COMMON | MF_CLASH_OK |
     MF_J_USE_DESC, "Library time (hrs)", NULL, NULL},
    {"LT_MIN", 0x3c, 0, PDT_TAPE, 24, 7, 8, MF_COMMON | MF_CLASH_OK |
     MF_J_USE_DESC, "Library time (mins)", NULL, NULL},
    {"LT_SEC", 0x3c, 0, PDT_TAPE, 25, 7, 8, MF_COMMON | MF_CLASH_OK |
     MF_J_USE_DESC, "Library time (secs)", NULL, NULL},
    {"CUM_PT", 0x3c, 0, PDT_TAPE, 32, 7, 4 * 8, MF_COMMON | MF_CLASH_OK |
     MF_J_NPARAM_DESC, "Cumulative power-on time (seconds)", NULL, NULL},
    /* Extended reset [0x3d] LTO-5, HP */
    {"RES_BEH", 0x3d, 0, PDT_TAPE, 2, 1, 2, MF_COMMON | MF_CLASH_OK |
        MF_J_USE_DESC, "Reset behavior", NULL,
        "0: normal\t1: flush, rewind\t2: no flush, maintain position"},
    /* CD-ROM emulator / disaster recovery [0x3e] LTO-5, HP */
    {"NON_AUTO", 0x3e, 0, PDT_TAPE, 2, 1, 1, MF_COMMON | MF_CLASH_OK |
        MF_J_USE_DESC, "Non-auto", NULL, "0: reverts to tape after 100 "
        "blocks read in cd-rom emulation mode\t1: inhibits return and "
        "stays in cd-rom emulation mode"},
    {"CD_MODE", 0x3e, 0, PDT_TAPE, 2, 0, 1, MF_COMMON | MF_CLASH_OK |
        MF_J_USE_DESC, "CDmode", NULL,
        "0: tape drive mode\t1: cd-rom emulation mode"},

    {NULL, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL},
};

static const struct sdparm_mp_name_t sdparm_v_lto6_mode_pg[] = {
    {0x3b, 0, PDT_TAPE, 0, "l6sno", "Serial number override (LTO-5)", NULL},
    {0x3c, 0, PDT_TAPE, 0, "l6dt", "Device time (LTO-5)", NULL},
    {0x3d, 0, PDT_TAPE, 0, "l6er", "Extended reset (LTO-5)", NULL},
    {0x3e, 0, PDT_TAPE, 0, "l6cde", "cd-rom emulation / disaster recovery "
        "(LTO-5)", NULL},

    {0, 0, 0, 0, NULL, NULL, NULL},
};

static const struct sdparm_mp_item_t sdparm_mitem_v_lto6_arr[] = {
    /* Serial number override [0x3b] LTO-5, HP */
    {"MSN", 0x3b, 0, PDT_TAPE, 2, 1, 2, MF_COMMON | MF_CLASH_OK,
        "Non-auto", NULL, "0: not reported\t1: manufacturer's default SN\t2: "
        "not reported\t3: non-default Serial Number"},
    {"SN0_7", 0x3b, 0, PDT_TAPE, 6, 7, 8 * 8, MF_HEX | MF_CLASH_OK,
     "Serial Number, bytes 0 to 7", "serial_number_0_7",
     "ASCII hex in range 0x20 to 0x7f"},
    {"SN8_11", 0x3b, 0, PDT_TAPE, 14, 7, 4 * 8, MF_HEX | MF_CLASH_OK,
     "Serial Number, bytes 8 to 11", "serial_number_8_11",
     "ASCII hex in range 0x20 to 0x7f"},
    /* Device time [0x3c] LTO-5, HP */
    {"LT_VAL", 0x3c, 0, PDT_TAPE, 2, 2, 1, MF_COMMON | MF_CLASH_OK,
     "Library time valid", "lt", NULL},
    {"WT_VAL", 0x3c, 0, PDT_TAPE, 2, 1, 1, MF_COMMON | MF_CLASH_OK,
     "World time valid", "wt", NULL},
    {"PT_VAL", 0x3c, 0, PDT_TAPE, 2, 0, 1, MF_COMMON | MF_CLASH_OK,
     "Power-on time valid", "pt", NULL},
    {"CP_COUNT", 0x3c, 0, PDT_TAPE, 6, 7, 2 * 8,
     MF_COMMON | MF_CLASH_OK | MF_J_USE_DESC,
     "Current power-on count", NULL, NULL},
    {"UTC", 0x3c, 0, PDT_TAPE, 14, 1, 1, MF_COMMON | MF_CLASH_OK,
     "UTC", NULL, "0: local time zone\t1: UTC"},
    {"NTP", 0x3c, 0, PDT_TAPE, 14, 0, 1, MF_COMMON | MF_CLASH_OK,
     "NTP", NULL, "0: unsure if NTP synced\t1: NTP synced"},
    {"WOR_TIME", 0x3c, 0, PDT_TAPE, 16, 7, 4 * 8,
     MF_COMMON | MF_CLASH_OK | MF_J_USE_DESC,
     "World time", NULL, "seconds since 00:00:00, 1 January 1970 UTC"},
    {"LT_HR", 0x3c, 0, PDT_TAPE, 23, 7, 8, MF_COMMON | MF_CLASH_OK |
     MF_J_USE_DESC, "Library time (hrs)", NULL, NULL},
    {"LT_MIN", 0x3c, 0, PDT_TAPE, 24, 7, 8, MF_COMMON | MF_CLASH_OK |
     MF_J_USE_DESC, "Library time (mins)", NULL, NULL},
    {"LT_SEC", 0x3c, 0, PDT_TAPE, 25, 7, 8, MF_COMMON | MF_CLASH_OK |
     MF_J_USE_DESC, "Library time (secs)", NULL, NULL},
    {"CUM_PT", 0x3c, 0, PDT_TAPE, 32, 7, 4 * 8, MF_COMMON | MF_CLASH_OK |
     MF_J_NPARAM_DESC, "Cumulative power-on time (seconds)", NULL, NULL},
    /* Extended reset [0x3d] LTO-5, HP */
    {"RES_BEH", 0x3d, 0, PDT_TAPE, 2, 1, 2, MF_COMMON | MF_CLASH_OK |
        MF_J_USE_DESC, "Reset behavior", NULL,
        "0: normal\t1: flush, rewind\t2: no flush, maintain position"},
    /* CD-ROM emulator / disaster recovery [0x3e] LTO-5, HP */
    {"NON_AUTO", 0x3e, 0, PDT_TAPE, 2, 1, 1, MF_COMMON | MF_CLASH_OK |
        MF_J_USE_DESC, "Non-auto", NULL, "0: reverts to tape after 100 "
        "blocks read in cd-rom emulation mode\t1: inhibits return and "
        "stays in cd-rom emulation mode"},
    {"CD_MODE", 0x3e, 0, PDT_TAPE, 2, 0, 1, MF_COMMON | MF_CLASH_OK |
        MF_J_USE_DESC, "CDmode", NULL,
        "0: tape drive mode\t1: cd-rom emulation mode"},

    {NULL, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL},
};

static const struct sdparm_mp_name_t sdparm_v_nvme_mode_pg[] = {
    {UNIT_ATTENTION_MP, 0, 0, 0, "nvme", "Unit attention (NVMe)", NULL},
    {0, 0, 0, 0, NULL, NULL, NULL},
};

/* Only used by library's SNTL to override settings implied by NVMSR (byte
 * 253 of Identify controller response) field, namely the NVMEE and NVMESD
 * bits within that field */
static const struct sdparm_mp_item_t sdparm_mitem_v_nvme_arr[] = {
    /* Unit attention page [0x0] NVMe */
    {"ENC_OV", UNIT_ATTENTION_MP, 0, 0, 2, 7, 8, MF_COMMON,
        "Enclosure override", NULL,
        "0: no override; 1: SES only; 2: SES+disk\t"
        "3: pdt=processor SAFTE; 255: disk only"},
    {"NVME2", UNIT_ATTENTION_MP, 0, 0, 3, 7, 8, 0,
        "Place holder, NVMe 2", NULL, NULL},
};

/* Indexed by VENDOR_* define */
const struct sdparm_vendor_pair sdparm_vendor_mp[] = {
    {sdparm_v_seagate_mode_pg, sdparm_mitem_v_seagate_arr},
    {sdparm_v_hitachi_mode_pg, sdparm_mitem_v_hitachi_arr},
    {sdparm_v_maxtor_mode_pg, sdparm_mitem_v_maxtor_arr},
    {sdparm_v_fujitsu_mode_pg, sdparm_mitem_v_fujitsu_arr},
    {sdparm_gen_mode_pg, sdparm_mitem_arr},     /* VENDOR_NONE --> generic */
    {sdparm_v_lto5_mode_pg, sdparm_mitem_v_lto5_arr},
    {sdparm_v_lto6_mode_pg, sdparm_mitem_v_lto6_arr},
    {sdparm_v_nvme_mode_pg, sdparm_mitem_v_nvme_arr},
    {NULL, NULL},       /* no VENDOR_SG defined mode pages */
};

const int sdparm_vendor_mp_len = SG_ARRAY_SIZE(sdparm_vendor_mp);

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

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <scsi/scsi.h>
#include <scsi/sg.h>
#include <scsi/sg_lib.h>
#include <scsi/sg_cmds.h>

/* A utility program for the Linux OS SCSI subsystem.
 *
 * This utility fetches various parameters associated with a given
 * SCSI disk (or a disk that uses, or translates the SCSI command
 * set). In some cases these parameters can be changed.
 */

static char * version_str = "0.92 20050520";

#define ME "sdparm: "

#define MAP_TO_SG_NODE

#define DEF_MODE_RESP_LEN 252
#define DEF_INQ_RESP_LEN 252
#define RW_ERR_RECOVERY_MP 1
#define DISCONNECT_MP 2
#define FORMAT_MP 3
#define RIGID_DISK_MP 4
#define WRITE_PARAM_MP 5
#define RBC_DEV_PARAM_MP 6
#define V_ERR_RECOVERY_MP 7
#define CACHING_MP 8
#define CONTROL_MP 0xa
#define DATA_COMPR_MP 0xf
#define DEV_CONF_MP 0x10
#define ES_MAN_MP 0x14
#define PROT_SPEC_LU_MP 0x18
#define PROT_SPEC_PORT_MP 0x19
#define POWER_MP 0x1a
#define IEC_MP 0x1c
#define TIMEOUT_PROT_MP 0x1d
#define XOR_MP 0x10

#define MODE_DATA_OVERHEAD 128
#define EBUFF_SZ 256
#define MAX_MP_IT_VAL 128
#define MAX_MODE_DATA_LEN 2048

#define VPD_SUPPORTED_VPDS 0x0
#define VPD_UNIT_SERIAL_NUM 0x80
#define VPD_DEVICE_ID 0x83
#define VPD_MAN_NET_ADDR 0x85
#define VPD_EXT_INQ 0x86
#define VPD_SCSI_PORTS 0x88
#define VPD_ASSOC_LU 0
#define VPD_ASSOC_TPORT 1
#define VPD_ASSOC_TDEVICE 2

static int find_corresponding_sg_fd(int sg_fd, const char * device_name,
                                    int flags, int verbose);

static struct option long_options[] = {
        {"six", 0, 0, '6'},
        {"all", 0, 0, 'a'},
        {"clear", 1, 0, 'c'},
        {"defaults", 1, 0, 'D'},
        {"dummy", 1, 0, 'd'},
        {"enumerate", 0, 0, 'e'},
        {"flexible", 0, 0, 'f'},
        {"get", 1, 0, 'g'},
        {"help", 0, 0, 'h'},
        {"hex", 0, 0, 'H'},
        {"inquiry", 0, 0, 'i'},
        {"long", 0, 0, 'l'},
        {"page", 1, 0, 'p'},
        {"set", 1, 0, 's'},
        {"save", 0, 0, 'S'},
        {"verbose", 0, 0, 'v'},
        {"version", 0, 0, 'V'},
        {0, 0, 0, 0},
};

static void usage()
{
    fprintf(stderr, "Usage: "
          "sdparm    [-all] [--clear=<str>] [--defaults] [--dummy] "
          "[--flexible] [--get=<str>]\n"
          "                 [--help] [--hex] [--inquiry] [--long] "
          "[--page=<pg[,spg]>]\n"
          "                 [--save] [--set=<str>] [--six] [--verbose] "
          "[--version]\n"
          "                 <scsi_disk>\n"
          "       sdparm    [-all] --enumerate [--inquiry] "
          "[--page=<pg[,spg]>]\n"
          "  where:\n"
          "      --all | -a            list all known parameters for given "
          "disk\n"
          "      --clear=<str> | -c <str>  clear (zero) parameter value(s)\n"
          "      --defaults | -D       set a mode page to its default "
          "values\n"
          "      --dummy | -d          don't write back modified mode page\n"
          "      --enumerate | -e      list known pages and parameters "
          "(ignore disk)\n"
          "      --get=<str> | -g <str>  get (fetch) parameter value(s)\n"
          "      --help | -h           print out usage message\n"
          "      --hex | -H            output in hex rather than name/value "
          "pairs\n"
          "      --inquiry | -i        output INQUIRY VPD page(s) (def mode "
          "page(s))\n"
          "      --long | -l           add description to parameter output\n"
          "      --page=<pg[,spg]> | -p <pg[,spg]>  page (and optionally "
          "subpage) number\n"
          "                            to output, change or enumerate\n"
          "      --save | -S           place mode changes in saved page as "
          "well\n"
          "      --set=<str> | -s <str>  set parameter value(s)\n"
          "      --six | -6            use 6 byte SCSI cdbs (def 10 byte)\n"
          "      --verbose | -v        increase verbosity\n"
          "      --version | -V        print version string and exit\n\n"
          "View or change parameters of a SCSI disk (or other device)\n"
          );
}

struct values_name_t {
    int value;
    int subvalue;
    int pdt;  /* -1 for SPC-3 mode pages, else primary device type; */
              /* -1 for VPD pages */
    const char * acron;
    const char * name;
};

static struct values_name_t mode_nums_name[] = {
    {CACHING_MP, 0, 0, "ca", "Caching (SBC)"},
    {CONTROL_MP, 0, -1, "co", "Control"},
    {DATA_COMPR_MP, 0, 1, "dac", "Data compression (SSC)"},
    {DEV_CONF_MP, 0, 1, "dc", "Device configuration (SSC)"},
    {ES_MAN_MP, 0, 0xd, "esm", "Enclosure services management (SES)"},
    {DISCONNECT_MP, 0, -1, "dr", "Disconnect-reconnect"},
    {FORMAT_MP, 0, 0, "fo", "Format (SBC)"},
    {IEC_MP, 0, -1, "ie", "Informational exception control"},
    {PROT_SPEC_LU_MP, 0, -1, "pl", "Protocol specific logical unit"},
    {POWER_MP, 0, -1, "po", "Power condition"},
    {PROT_SPEC_PORT_MP, 0, -1, "pp", "Protocol specific port"},
    {RBC_DEV_PARAM_MP, 0, 0xe, "rbc", "RBC device parameters (RBC)"},
    {RIGID_DISK_MP, 0, 0, "rd", "Rigid disk (SBC)"},
    {TIMEOUT_PROT_MP, 0, 5, "rp", "Timeout and protect (MMC)"},
    {RW_ERR_RECOVERY_MP, 0, -1, "rw", "Read write error recovery"},
        /* since in SBC, SSC and MMC treat as if in SPC */
    {V_ERR_RECOVERY_MP, 0, 0, "ve", "Verify error recovery (SBC)"},
    {WRITE_PARAM_MP, 0, 5, "wp", "Write parameters (MMC)"},
    {XOR_MP, 0, 0, "xo", "XOR control (SBC)"},
};

static int mode_nums_name_len =
        (sizeof(mode_nums_name) / sizeof(mode_nums_name[0]));

static void list_mps()
{
    int k;
    const struct values_name_t * vnp;

    for (k = 0, vnp = mode_nums_name; k < mode_nums_name_len; ++k, ++vnp) {
        if (vnp->subvalue)
            printf("  %-4s 0x%02x,0x%02x %s\n", vnp->acron, vnp->value,
                   vnp->subvalue, vnp->name);
        else
            printf("  %-4s 0x%02x      %s\n", vnp->acron, vnp->value,
                   vnp->name);
    }
}

static const struct values_name_t * get_mode_detail(int page_num,
                                                    int subpage_num, int pdt)
{
    int k;
    const struct values_name_t * vnp;

    for (k = 0, vnp = mode_nums_name; k < mode_nums_name_len; ++k, ++vnp) {
        if ((page_num == vnp->value) && (subpage_num == vnp->subvalue)) {
            if ((pdt < 0) || (vnp->pdt < 0) || (vnp->pdt == pdt))
                return vnp;
        }
    }
    return NULL;
}

static void get_mode_page_name(int page_num, int subpage_num, int pdt,
                               int hex, char * bp, int max_b_len)
{
    int len = max_b_len - 1;
    const struct values_name_t * vnp;

    if (len < 0)
        return;
    bp[len] = '\0';
    vnp = get_mode_detail(page_num, subpage_num, pdt);
    if (NULL == vnp)
        vnp = get_mode_detail(page_num, subpage_num, -1);
    if (vnp && vnp->name) {
        if (hex) {
            if (0 == subpage_num)
                snprintf(bp, len, "%s [0x%x]", vnp->name, page_num);
            else
                snprintf(bp, len, "%s [0x%x,0x%x]", vnp->name, page_num,
                         subpage_num);
        } else
            snprintf(bp, len, "%s", vnp->name);
    } else {
        if (0 == subpage_num)
            snprintf(bp, len, "[0x%x]", page_num);
        else
            snprintf(bp, len, "[0x%x,0x%x]", page_num, subpage_num);
    }
}

static const struct values_name_t * find_mp_by_acron(const char * ap)
{
    int k;
    const struct values_name_t * vnp;

    for (k = 0, vnp = mode_nums_name; k < mode_nums_name_len; ++k, ++vnp) {
        if (0 == strncmp(vnp->acron, ap, 3))
            return vnp;
    }
    return NULL;
}

static struct values_name_t vpd_nums_name[] = {
    {VPD_DEVICE_ID, 0, -1, "di", "Device identification"},
    {VPD_SCSI_PORTS, 0, -1, "sp", "SCSI ports"},
    {VPD_SUPPORTED_VPDS, 0, -1, "sv", "Supported VPD pages"},
    {VPD_UNIT_SERIAL_NUM, 0, -1, "sn", "Unit serial number"},
};

static int vpd_nums_name_len =
        (sizeof(vpd_nums_name) / sizeof(vpd_nums_name[0]));

static void list_vpds()
{
    int k;
    const struct values_name_t * vnp;

    for (k = 0, vnp = vpd_nums_name; k < vpd_nums_name_len; ++k, ++vnp)
        printf("  %-4s 0x%02x      %s\n", vnp->acron, vnp->value, vnp->name);
}

static const char * get_vpd_name(int page_num)
{
    int k;
    const struct values_name_t * vnp;

    for (k = 0, vnp = vpd_nums_name; k < vpd_nums_name_len; ++k, ++vnp) {
        if (page_num == vnp->value)
            return vnp->name;
    }
    return NULL;
}

static const struct values_name_t * find_vpd_by_acron(const char * ap)
{
    int k;
    const struct values_name_t * vnp;

    for (k = 0, vnp = vpd_nums_name; k < vpd_nums_name_len; ++k, ++vnp) {
        if (0 == strncmp(vnp->acron, ap, 2))
            return vnp;
    }
    return NULL;
}

struct opt_coll {
    int all;
    int mode_6;
    int defaults;
    int dummy;
    int enumerate;
    int hex;
    int inquiry;
    int long_out;
    int saved;
    int flexible;
};

struct mode_page_item {
    const char * acron;
    int page_num;
    int subpage_num;
    int pdt;            /* -1 if in SPC or multiple sets */
    int start_byte;
    int start_bit;
    int num_bits;
    int common;         /* set to list out in summary */
    const char * description;
};

struct mode_page_it_val {
    struct mode_page_item mpi;
    int val;
};

struct mode_page_settings {
    int page_num;
    int subpage_num;
    struct mode_page_it_val it_vals[MAX_MP_IT_VAL];
    int num_it_vals;
};


static struct mode_page_item mitem_arr[] = {
    /* treat as spc since various command sets implement variants */
    {"AWRE", RW_ERR_RECOVERY_MP, 0, -1, 2, 7, 1, 1,   /* [0x1] sbc2 */
        "Automatic write reallocation enabled"},
    {"ARRE", RW_ERR_RECOVERY_MP, 0, -1, 2, 6, 1, 1,
        "Automatic read reallocation enabled"},
    {"TB", RW_ERR_RECOVERY_MP, 0, -1, 2, 5, 1, 0,
        "Transfer block"},
    {"RC", RW_ERR_RECOVERY_MP, 0, -1, 2, 4, 1, 0,
        "Read continuous"},
    {"EER", RW_ERR_RECOVERY_MP, 0, -1, 2, 3, 1, 0,
        "Enable early recover"},
    {"PER", RW_ERR_RECOVERY_MP, 0, -1, 2, 2, 1, 1,
        "Post error"},
    {"DTE", RW_ERR_RECOVERY_MP, 0, -1, 2, 1, 1, 0,
        "Data terminate on error"},
    {"DCR", RW_ERR_RECOVERY_MP, 0, -1, 2, 0, 1, 0,
        "Disable correction"},
    {"RRC", RW_ERR_RECOVERY_MP, 0, -1, 3, 7, 8, 0,
        "Read retry count"},
    {"EMCDR", RW_ERR_RECOVERY_MP, 0, -1, 7, 1, 2, 0,
        "Enhanced media certification and defect reporting (mmc only)"},
    {"WRC", RW_ERR_RECOVERY_MP, 0, -1, 8, 7, 8, 0,
        "Write retry count"},
    {"RTL", RW_ERR_RECOVERY_MP, 0, -1, 10, 7, 16, 0,
        "Recovery time limit (ms)"},

    {"BITL", DISCONNECT_MP, 0, -1, 4, 7, 16, 0,  /* [0x2] spc3,sas1 */
        "Bus inactivity time limit (sas: 100us)"},
    {"MCTL", DISCONNECT_MP, 0, -1, 8, 7, 16, 0,
        "Maximum connect time limit (sas: 100us)"},
    {"MBS", DISCONNECT_MP, 0, -1, 10, 7, 16, 0,
        "Maximum burst size"},
    {"FBS", DISCONNECT_MP, 0, -1, 14, 7, 16, 0,
        "First burst size"},

    {"TPZ", FORMAT_MP, 0, 0, 2, 7, 16, 0,        /* [0x3] sbc2 (obsolete) */
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

    {"NOC", RIGID_DISK_MP, 0, 0, 2, 7, 24, 0,   /* [0x4] sbc2 (obsolete) */
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

    {"BUFE", WRITE_PARAM_MP, 0, 5, 2, 6, 1, 1,      /* [0x5] mmc5 */
        "Buffer underrun free recording enable"},
    {"LS_V", WRITE_PARAM_MP, 0, 5, 2, 5, 1, 0,
        "Link size valid"},
    {"TST_W", WRITE_PARAM_MP, 0, 5, 2, 4, 1, 0,
        "Test write"},
    {"WR_T", WRITE_PARAM_MP, 0, 5, 2, 3, 4, 1,
        "Write type"},
    {"MULTI_S", WRITE_PARAM_MP, 0, 5, 3, 7, 2, 1,
        "Multi session"},
    {"FP", WRITE_PARAM_MP, 0, 5, 3, 5, 1, 1,
        "Fixed packet type"},
    {"COPY", WRITE_PARAM_MP, 0, 5, 3, 4, 1, 0,
        "Serial copy management system (SCMS) enable"},
    {"TRACK_M", WRITE_PARAM_MP, 0, 5, 3, 3, 4, 1,
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

    {"WCD", RBC_DEV_PARAM_MP, 0, 0xe, 2, 0, 1, 1, /* [0x6] rbc */
        "Write cache disable"},
    {"LBS", RBC_DEV_PARAM_MP, 0, 0xe, 3, 7, 16, 1,
        "Logical block size"},
    {"NLBS", RBC_DEV_PARAM_MP, 0, 0xe, 6, 7, 32, 1,
        "Number of logical blocks (ignore MSB)"},
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

    {"V_EER", V_ERR_RECOVERY_MP, 0, 0, 2, 3, 1, 0,   /* [0x8] sbc2 */
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

    {"IC", CACHING_MP, 0, 0, 2, 7, 1, 0,    /* [0x8] sbc2 */
        "Initiator control"},
    {"ABPF", CACHING_MP, 0, 0, 2, 6, 1, 0,
        "Abort pre-fetch"},
    {"CAP", CACHING_MP, 0, 0, 2, 5, 1, 0,
        "Caching analysis permitted"},
    {"DISC", CACHING_MP, 0, 0, 2, 4, 1, 0,
        "Discontinuity"},
    {"SIZE", CACHING_MP, 0, 0, 2, 3, 1, 0,
        "Size"},
    {"WCE", CACHING_MP, 0, 0, 2, 2, 1, 1,
        "Write cache enable"},
    {"MF", CACHING_MP, 0, 0, 2, 1, 1, 0,
        "Multiplication factor"},
    {"RCD", CACHING_MP, 0, 0, 2, 0, 1, 1,
        "Read cache disable"},
    {"DRRP", CACHING_MP, 0, 0, 3, 7, 4, 0,
        "Demand read retension prioriry"},
    {"WRP", CACHING_MP, 0, 0, 3, 3, 4, 0,
        "Write retension prioriry"},
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
        "disable read ahead"},
    {"NV_DIS", CACHING_MP, 0, 0, 12, 0, 1, 0,
        "Non-volatile cache disbale"},
    {"NCS", CACHING_MP, 0, 0, 13, 7, 8, 0,
        "Number of cache segments"},
    {"CSS", CACHING_MP, 0, 0, 14, 7, 16, 0,
        "Cache segment size"},

    {"TST", CONTROL_MP, 0, -1, 2, 7, 3, 0,    /* [0xa] spc3 */
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
    {"SWP", CONTROL_MP, 0, -1, 4, 3, 1, 1,
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

    {"DCE", DATA_COMPR_MP, 0, 1, 2, 7, 1, 1,    /* [0xf] ssc3 */
        "Data compression enable"},
    {"DCC", DATA_COMPR_MP, 0, 1, 2, 6, 1, 1,
        "Data compression capable"},
    {"DDE", DATA_COMPR_MP, 0, 1, 3, 7, 1, 1,
        "Data decompression enable"},
    {"RED", DATA_COMPR_MP, 0, 1, 3, 6, 2, 0,
        "Report exception on decompression"},
    {"COMPR_A", DATA_COMPR_MP, 0, 1, 4, 7, 32, 0,
        "Compression algorithm"},
    {"DCOMPR_A", DATA_COMPR_MP, 0, 1, 8, 7, 32, 0,
        "Decompression algorithm"},

    {"XORDIS", XOR_MP, 0, 0, 2, 1, 1, 0,    /* [0x10] sbc2 */
        "XOR disable"},
    {"MXWS", XOR_MP, 0, 0, 4, 7, 32, 0,
        "Maximum XOR write size (blocks)"},

    {"CAF", DEV_CONF_MP, 0, 1, 2, 5, 1, 0,    /* [0x10] ssc3 */
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
    {"RSMK", DEV_CONF_MP, 0, 1, 8, 5, 1, 1,
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
    {"SEW", DEV_CONF_MP, 0, 1, 10, 3, 1, 1,
        "Synchronize early warning"},
    {"SWP_T", DEV_CONF_MP, 0, 1, 10, 2, 1, 0,
        "Software write protect (tape)"},
    {"BAML", DEV_CONF_MP, 0, 1, 10, 1, 1, 0,
        "Block address mode lock"},
    {"BAM", DEV_CONF_MP, 0, 1, 10, 0, 1, 0,
        "Block address mode"},
    {"OBSAEW", DEV_CONF_MP, 0, 1, 11, 7, 24, 0,
        "Object buffer size at early warning"},
    {"SDCA", DEV_CONF_MP, 0, 1, 14, 7, 8, 1,
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

    {"ENBLTC", ES_MAN_MP, 0, 0xd, 5, 0, 1, 1,    /* [0x14] ses2 */
        "Enable timed completion"},
    {"MTCT", ES_MAN_MP, 0, 0xd, 6, 7, 16, 1,
        "Maximum task completion time (100ms)"},

    {"PID", PROT_SPEC_PORT_MP, 0, -1, 2, 3, 4, 0,    /* [0x19] spc3 */
        "Protocol identifier"},

    {"LUPID", PROT_SPEC_LU_MP, 0, -1, 2, 3, 4, 0,    /* [0x18] spc3 */
        "Protocol identifier"},

    {"IDLE", POWER_MP, 0, -1, 3, 1, 1, 0,    /* [0x1a] spc3 */
        "Idle timer active"},
    {"STANDBY", POWER_MP, 0, -1, 3, 0, 1, 0,
        "Standby timer active"},
    {"ICT", POWER_MP, 0, -1, 4, 7, 32, 0,
        "Idle condition timer (100 ms)"},
    {"SCT", POWER_MP, 0, -1, 8, 7, 32, 0,
        "Standby condition timer (100 ms)"},

    {"PERF", IEC_MP, 0, -1, 2, 7, 1, 0,    /* [0x1c] spc3 */
        "Performance"},
    {"EBF", IEC_MP, 0, -1, 2, 5, 1, 0,
        "Enable background function"},
    {"EWASC", IEC_MP, 0, -1, 2, 4, 1, 1,
        "Enable warning"},
    {"DEXCPT", IEC_MP, 0, -1, 2, 3, 1, 1,
        "Disable exceptions"},
    {"TEST", IEC_MP, 0, -1, 2, 2, 1, 0,
        "Test (simulate device failure)"},
    {"LOGERR", IEC_MP, 0, -1, 2, 0, 1, 0,
        "Log errors"},
    {"MRIE", IEC_MP, 0, -1, 3, 3, 4, 1,
        "Method of reporting infomational exceptions"},
    {"INTT", IEC_MP, 0, -1, 4, 7, 32, 0,
        "Interval timer (100 ms)"},
    {"REPC", IEC_MP, 0, -1, 8, 7, 32, 0,
        "Report count"},

    {"G3E", TIMEOUT_PROT_MP, 0, 5, 4, 3, 1, 0,    /* [0x1d] mmc5 */
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
};

static int mitem_arr_len = (sizeof(mitem_arr) / sizeof(mitem_arr[0]));

static void list_mitems(int pn, int spn, int pdt)
{
    int k, t_pn, t_spn, t_pdt;
    const struct mode_page_item * mpi;
    char buff[128];
    int found = 0;

    t_pn = -1;
    t_spn = -1;
    t_pdt = -2;
    for (k = 0, mpi = mitem_arr; k < mitem_arr_len; ++k, ++mpi) {
        if ((pdt >= 0) && (mpi->pdt >= 0) && (pdt != mpi->pdt))
            continue;
        if ((t_pn != mpi->page_num) || (t_spn != mpi->subpage_num) ||
            (t_pdt != mpi->pdt)) {
            t_pn = mpi->page_num;
            t_spn = mpi->subpage_num;
            t_pdt = mpi->pdt;
            if ((pn >= 0) && ((pn != t_pn) || (spn != t_spn)))
                continue;
            if ((pdt >= 0) && (pdt != t_pdt))
                continue;
            get_mode_page_name(t_pn, t_spn, t_pdt, 1, buff, sizeof(buff));
            printf("%s mode page:\n", buff); 
        } else {
            if ((pn >= 0) && ((pn != t_pn) || (spn != t_spn)))
                continue;
        }
        printf("  %-10s [0x%02x:%d:%-2d]  %s\n", mpi->acron, mpi->start_byte,
               mpi->start_bit, mpi->num_bits, mpi->description);
        found = 1;
    }
    if ((! found) && (pn >= 0)) {
        get_mode_page_name(pn, spn, pdt, 1, buff, sizeof(buff));
        fprintf(stderr, "%s mode page: no items found\n", buff);
    }
}

static const struct mode_page_item * find_mitem_by_acron(const char * ap,
                                                         int * from)
{
    int k = 0;
    const struct mode_page_item * mpi;

    if (from) {
        k = *from;
        if (k < 0)
            k = 0;
    }
    for (mpi = mitem_arr + k; k < mitem_arr_len; ++k, ++mpi) {
        if (0 == strcmp(mpi->acron, ap))
            break;
    }
    if (k >= mitem_arr_len) {
        k = mitem_arr_len;
        mpi = NULL;
    }
    if (from)
        *from = k + 1;
    return mpi;
}

static void list_mp_settings(struct mode_page_settings * mps, int get)
{
    struct mode_page_item * mpip;
    int k;

    printf("mp_settings: page,subpage=0x%x,0x%x  num=%d\n",
           mps->page_num, mps->subpage_num, mps->num_it_vals);
    for (k = 0; k < mps->num_it_vals; ++k) {
        mpip = &mps->it_vals[k].mpi;
        if (get)
            printf("  [0x%x,0x%x]  pdt=0x%x, byte_off=0x%x, bit_off=%d, "
                   "num_bits=%d  val=%d  acronym: %s\n", mpip->page_num,
                   mpip->subpage_num, mpip->start_byte, mpip->pdt,
                   mpip->start_bit, mpip->num_bits, mps->it_vals[k].val,
                   (mpip->acron ? mpip->acron : ""));
        else
            printf("  pdt=0x%x, byte_off=0x%x, bit_off=%d, num_bits=%d "
                   " val=%d  acronym: %s\n", mpip->pdt, mpip->start_byte,
                   mpip->start_bit, mpip->num_bits, mps->it_vals[k].val,
                   (mpip->acron ? mpip->acron : ""));
    }
}

static const char * scsi_ptype_strs[] = {
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

static unsigned int get_big_endian(const unsigned char * from, int start_bit,
                                   int num_bits)
{
    unsigned int res;
    int sbit_o1 = start_bit + 1;

    res = (*from++ & ((1 << sbit_o1) - 1));
    num_bits -= sbit_o1;
    while (num_bits > 0) {
        res <<= 8;
        res |= *from++;
        num_bits -= 8;
    }
    if (num_bits < 0)
        res >>= (-num_bits);
    return res;
}

static void set_big_endian(unsigned int val, unsigned char * to,
                           int start_bit, int num_bits)
{
    int sbit_o1 = start_bit + 1;
    int mask, num, k, x;

    mask = (8 != sbit_o1) ? ((1 << sbit_o1) - 1) : 0xff;
    k = start_bit - ((num_bits - 1) % 8);
    if (0 != k)
        val <<= ((k > 0) ? k : (8 + k));
    num = (num_bits + 15 - sbit_o1) / 8;
    for (k = 0; k < num; ++k) {
        if ((sbit_o1 - num_bits) > 0)
            mask &= ~((1 << (sbit_o1 - num_bits)) - 1);
        if (k < (num - 1))
            x = (val >> ((num - k - 1) * 8)) & 0xff;
        else
            x = val & 0xff;
        to[k] = (to[k] & ~mask) | (x & mask); 
        mask = 0xff;
        num_bits -= sbit_o1;
        sbit_o1 = 8;
    }
}

static unsigned int mp_get_value(const struct mode_page_item *mpi,
                                 const unsigned char * mp)
{
    return get_big_endian(mp + mpi->start_byte, mpi->start_bit,
                          mpi->num_bits);
}

static unsigned int mp_get_value_check(const struct mode_page_item *mpi,
                                       const unsigned char * mp,
                                       int * all_set)
{
    unsigned int res;

    res = get_big_endian(mp + mpi->start_byte, mpi->start_bit,
                         mpi->num_bits);
    if (all_set) {
        if ((16 == mpi->num_bits) && (0xffff == res))
            *all_set = 1;
        else if ((32 == mpi->num_bits) && (0xffffffff == res))
            *all_set = 1;
        else
            *all_set = 0;
    }
    return res;
}

static void mp_set_value(unsigned int val, struct mode_page_item *mpi,
                         unsigned char * mp)
{
    set_big_endian(val, mp + mpi->start_byte, mpi->start_bit, mpi->num_bits);
}
 
static void print_mp_entry(const char * pre, int smask,
                           const struct mode_page_item *mpi,
                           const unsigned char * cur_mp,
                           const unsigned char * cha_mp,
                           const unsigned char * def_mp,
                           const unsigned char * sav_mp,
                           int long_out)
{
    int sep = 0;
    int all_set;
    unsigned int u;
    const char * acron;

    all_set = 0;
    acron = (mpi->acron ? mpi->acron : "");
    u = mp_get_value_check(mpi, cur_mp, &all_set);
    if (all_set)
        printf("%s%-10s -1", pre, acron);
    else
        printf("%s%-10s%3u", pre, acron, u);
    if (smask & 0xe) {
        printf("  [");
        if (smask & 2) {
            printf("Changeable: %s",
                   (mp_get_value(mpi, cha_mp) ? "y" : "n"));
            sep = 1;
        }
        if (smask & 4) {
            all_set = 0;
            u = mp_get_value_check(mpi, def_mp, &all_set);
            if (all_set)
                printf("%sdef: -1", (sep ? ", " : " "));
            else
                printf("%sdef:%3u", (sep ? ", " : " "), u);
            sep = 1;
        }
        if (smask & 8) {
            all_set = 0;
            u = mp_get_value_check(mpi, sav_mp, &all_set);
            if (all_set)
                printf("%ssaved: -1", (sep ? ", " : " "));
            else
                printf("%ssaved:%3u", (sep ? ", " : " "), u);
        }
        printf("]");
    }
    if (long_out && mpi->description)
        printf("  %s", mpi->description);
    printf("\n");
}

static void print_mode_info(int sg_fd, int pn, int spn, int pdt,
                            const struct opt_coll * opts, int verbose)
{
    int k, res, len, verb, smask, single_pg, fetch_pg, rep_len, orig_pn;
    const struct mode_page_item * mpi;
    unsigned char cur_mp[DEF_MODE_RESP_LEN];
    unsigned char cha_mp[DEF_MODE_RESP_LEN];
    unsigned char def_mp[DEF_MODE_RESP_LEN];
    unsigned char sav_mp[DEF_MODE_RESP_LEN];
    const char * name;
    void * pc_arr[4];
    char buff[128];

    verb = (verbose > 0) ? verbose - 1 : 0;
    orig_pn = pn;
    if (pn >= 0) {
        single_pg = 1;
        fetch_pg = 1;
        for (k = 0, mpi = mitem_arr; k < mitem_arr_len; ++k, ++mpi) {
            if ((pn == mpi->page_num) && (spn == mpi->subpage_num)) {
                if ((pdt < 0) || (mpi->pdt < 0) || (pdt == mpi->pdt) ||
                    opts->flexible)
                    break;
            }
        }
        if (k >= mitem_arr_len) {
            if (opts->hex) {
                k = 0;
                mpi = mitem_arr;    /* trick to enter main loop once */
            } else {
                get_mode_page_name(pn, spn, pdt, opts->hex, buff,
                                   sizeof(buff));
                fprintf(stderr, "%s mode page, attributes not found\n", buff);
                if ((0 == opts->flexible) && verbose)
                    fprintf(stderr, "    perhaps try '--flexible'\n");
            }
        }
    } else {
        single_pg = 0;
        fetch_pg = 0;
        mpi = mitem_arr;
        k = 0;
    }
    name = "";
    smask = 0;
    for (; k < mitem_arr_len; ++k, ++mpi, fetch_pg = 0) {
        if (0 == fetch_pg) {
            if ((pdt >=0) && (mpi->pdt >= 0) && (pdt != mpi->pdt) &&
                (0 == opts->flexible))
                continue;
            if (! (((orig_pn >= 0) ? 1 : opts->all) || mpi->common))
                continue;
            if ((pn != mpi->page_num) || (spn != mpi->subpage_num)) {
                if (single_pg)
                    break;
                fetch_pg = 1;
                pn = mpi->page_num;
                spn = mpi->subpage_num;
            }
        }
        if (fetch_pg) {
            smask = 0;
            pc_arr[0] = cur_mp;
            pc_arr[1] = cha_mp;
            pc_arr[2] = def_mp;
            pc_arr[3] = sav_mp;
            res = sg_get_mode_page_controls(sg_fd, opts->mode_6, pn, spn,
                                            opts->flexible, DEF_MODE_RESP_LEN,
                                            &smask, pc_arr, &rep_len, verb);
            if (SG_LIB_CAT_INVALID_OP == res) {
                if (opts->mode_6)
                    fprintf(stderr, "6 byte MODE SENSE cdb not supported, "
                            "try again without '-6' option\n");
                else
                    fprintf(stderr, "10 byte MODE SENSE cdb not supported, "
                            "try again with '-6' option\n");
                return;
            }
            if (smask & 1) {
                get_mode_page_name(pn, spn, pdt, opts->hex, buff,
                                   sizeof(buff));
                if (opts->long_out)
                    printf("%s [PS=%d] mode page:\n", buff,
                           !!(cur_mp[0] & 0x80));
                else
                    printf("%s mode page:\n", buff);
                if (pn != (cur_mp[0] & 0x3f)) {
                    if (opts->flexible)
                        fprintf(stderr, ">>> warning: mode page seems "
                                "malformed\n");
                    else
                        fprintf(stderr, ">>> warning: mode page seems "
                                "malformed, try '--flexible'\n");
                } else if (verbose && (rep_len > 0xa00)) {
                    if (opts->flexible)
                        fprintf(stderr, ">>> warning: mode page length=%d "
                                "too long,\n", rep_len);
                    else
                        fprintf(stderr, ">>> warning: mode page length=%d "
                                "too long, perhaps try '--flexible'\n",
                                rep_len);
                }
                if (opts->hex) {
                    if (cur_mp[0] & 0x40)
                        len = (cur_mp[2] << 8) + cur_mp[3] + 4;
                    else
                        len = cur_mp[1] + 2;
                    if (len > (int)sizeof(cur_mp)) {
                        fprintf(stderr, ">> decoded page length too "
                                        "large=%d, trim\n", len);
                        len = sizeof(cur_mp);
                    }
                    printf("    Current:\n");
                    dStrHex((const char *)cur_mp, len, 1);
                    if (smask & 2) {
                        printf("    Changeable:\n");
                        dStrHex((const char *)cha_mp, len, 1);
                    }
                    if (smask & 4) {
                        printf("    Default:\n");
                        dStrHex((const char *)def_mp, len, 1);
                    }
                    if (smask & 8) {
                        printf("    Saved:\n");
                        dStrHex((const char *)sav_mp, len, 1);
                    }
                }
            } else {
                if (verbose || single_pg) {
                    get_mode_page_name(pn, spn, pdt, opts->hex, buff,
                                       sizeof(buff));
                    fprintf(stderr, ">> %s mode page not supported\n", buff);
                }
            }
        }
        if (smask && (! opts->hex))
            print_mp_entry("  ", smask, mpi, cur_mp, cha_mp,
                     def_mp, sav_mp, opts->long_out);
    }
}

static void get_mode_info(int sg_fd, struct mode_page_settings * mps,
                          int pdt, const struct opt_coll * opts, int verbose)
{
    int k, res, verb, smask, pn, spn, warned, rep_len;
    unsigned int u, val;
    const struct mode_page_item * mpi;
    unsigned char cur_mp[DEF_MODE_RESP_LEN];
    unsigned char cha_mp[DEF_MODE_RESP_LEN];
    unsigned char def_mp[DEF_MODE_RESP_LEN];
    unsigned char sav_mp[DEF_MODE_RESP_LEN];
    const struct mode_page_it_val * ivp;
    char buff[128];
    void * pc_arr[4];

    warned = 0;
    verb = (verbose > 0) ? verbose - 1 : 0;
    for (k = 0, pn = 0, spn = 0; k < mps->num_it_vals; ++k) {
        ivp = &mps->it_vals[k];
        val = ivp->val;
        mpi = &ivp->mpi;
        if ((0 == k) || (pn != mpi->page_num) || (spn != mpi->subpage_num)) {
            pn = mpi->page_num;
            spn = mpi->subpage_num;
            smask = 0;
            switch (val) {
            case 0:
                pc_arr[0] = cur_mp;
                pc_arr[1] = cha_mp;
                pc_arr[2] = def_mp;
                pc_arr[3] = sav_mp;
                res = sg_get_mode_page_controls(sg_fd, opts->mode_6, pn, spn,
                                 opts->flexible, DEF_MODE_RESP_LEN, &smask,
                                 pc_arr, &rep_len, verb);
                break;
            case 1:
                pc_arr[0] = cur_mp;
                pc_arr[1] = NULL;
                pc_arr[2] = NULL;
                pc_arr[3] = NULL;
                res = sg_get_mode_page_controls(sg_fd, opts->mode_6, pn, spn,
                                 opts->flexible, DEF_MODE_RESP_LEN, &smask,
                                 pc_arr, &rep_len, verb);
                break;
            default:
                if (mpi->acron)
                    fprintf(stderr, "bad value given to %s\n",
                            mpi->acron);
                else
                    fprintf(stderr, "bad value given to 0x%x:%d:%d\n",
                            mpi->start_byte, mpi->start_bit, mpi->num_bits);
                return;
            }
            if (SG_LIB_CAT_INVALID_OP == res) {
                if (opts->mode_6)
                    fprintf(stderr, "6 byte MODE SENSE cdb not supported, "
                            "try again without '-6' option\n");
                else
                    fprintf(stderr, "10 byte MODE SENSE cdb not supported, "
                            "try again with '-6' option\n");
                return;
            }
            if ((0 == smask) && res) {
                if (mpi->acron)
                    fprintf(stderr, "%s ", mpi->acron);
                else
                    fprintf(stderr, "0x%x:%d:%d ",
                            mpi->start_byte, mpi->start_bit, mpi->num_bits);
                if (SG_LIB_CAT_ILLEGAL_REQ == res)
                    fprintf(stderr, "not found in ");
                else
                    fprintf(stderr, "error (res=%d) in ", res);
                get_mode_page_name(pn, spn, mpi->pdt, opts->hex, buff,
                                   sizeof(buff));
                fprintf(stderr, "%s mode page\n", buff);
                return;
            }
            if (smask & 1) {
                if (pn != (cur_mp[0] & 0x3f)) {
                    if (opts->flexible)
                        fprintf(stderr, ">>> warning: mode page seems "
                                "malformed\n");
                    else
                        fprintf(stderr, ">>> warning: mode page seems "
                                "malformed, try '--flexible'\n");
                } else if (verbose && (rep_len > 0xa00)) {
                    if (opts->flexible)
                        fprintf(stderr, ">>> warning: mode page length=%d "
                                "too long,\n", rep_len);
                    else
                        fprintf(stderr, ">>> warning: mode page length=%d "
                                "too long, perhaps try '--flexible'\n",
                                rep_len);
                }
            }
        }
        if ((pdt >= 0) && (0 == warned) && mpi->acron &&
            (mpi->pdt >= 0) && (pdt != mpi->pdt)) {
            warned = 1;
            fprintf(stderr, ">> warning: peripheral device type (pdt) is "
                    "0x%x but acronym %s\n   is associated with pdt 0x%x.\n",
                    pdt, ivp->mpi.acron, ivp->mpi.pdt);
        }
        if (0 == val) {
            if (opts->hex) {
                if (smask & 1) {
                    u = mp_get_value(mpi, cur_mp);
                    printf("0x%02x ", u);
                } else
                    printf("-    ");
                if (smask & 2) {
                    u = mp_get_value(mpi, cha_mp);
                    printf("0x%02x ", u);
                } else
                    printf("-    ");
                if (smask & 4) {
                    u = mp_get_value(mpi, def_mp);
                    printf("0x%02x ", u);
                } else
                    printf("-    ");
                if (smask & 8) {
                    u = mp_get_value(mpi, sav_mp);
                    printf("0x%02x ", u);
                } else
                    printf("-    ");
                printf("\n");
            } else
                print_mp_entry("", smask, mpi, cur_mp, cha_mp,
                               def_mp, sav_mp, opts->long_out);
        } else if (1 == val) {
            if (opts->hex) {
                if (smask & 1) {
                    u = mp_get_value(mpi, cur_mp);
                    printf("0x%02x ", u);
                } else
                    printf("-    ");
                printf("\n");
            } else
                print_mp_entry("", smask, mpi, cur_mp, NULL,
                               NULL, NULL, opts->long_out);
        }
    }
}

/* Return of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> invalid opcode, SG_LIB_CAT_ILLEGAL_REQ ->
 * bad field in cdb, -1 -> other failure */
static int change_mode_page(int sg_fd, int pdt,
                            struct mode_page_settings * mps,
                            const struct opt_coll * opts, int verbose)
{
    int k, len, off, md_len, res;
    char ebuff[EBUFF_SZ];
    unsigned char mdpg[MAX_MODE_DATA_LEN];
    struct mode_page_it_val * ivp;
    char buff[128];

    if (pdt >= 0) {
        /* sanity check: check acronym's pdt matches device's pdt */
        for (k = 0; k < mps->num_it_vals; ++k) {
            ivp = &mps->it_vals[k];
            if (ivp->mpi.acron && (ivp->mpi.pdt >= 0) &&
                (pdt != ivp->mpi.pdt)) {
                fprintf(stderr, "change_mode_page: peripheral device type "
                        "(pdt) is 0x%x but acronym %s\n  is associated with "
                        "pdt 0x%x. To bypass use numeric addressing mode.\n",
                        pdt, ivp->mpi.acron, ivp->mpi.pdt);
                return -1;
            }
        }
    }
    len = MAX_MODE_DATA_LEN;
    memset(mdpg, 0, len);
    if (opts->mode_6)
        res = sg_ll_mode_sense6(sg_fd, 0 /* dbd */, 0 /*current */,
                                mps->page_num, mps->subpage_num, mdpg,
                                ((len > 252) ? 252 : len), 1, verbose);
    else
        res = sg_ll_mode_sense10(sg_fd, 0 /* llbaa */, 0 /* dbd */,
                                 0 /* current */, mps->page_num,
                                 mps->subpage_num, mdpg, len, 1, verbose);
    if (0 != res) {
        get_mode_page_name(mps->page_num, mps->subpage_num, pdt, 0, buff,
                           sizeof(buff));
        fprintf(stderr, "change_mode_page: failed fetching page: %s\n",
                buff);
        return -1;
    }
    off = sg_mode_page_offset(mdpg, len, opts->mode_6, ebuff, EBUFF_SZ);
    if (off < 0) {
        fprintf(stderr, "change_mode_page: page offset failed: %s\n", ebuff);
        return -1;
    }
    if (opts->mode_6)
        md_len = mdpg[0] + 1;
    else
        md_len = (mdpg[0] << 8) + mdpg[1] + 2;
    mdpg[0] = 0;        /* mode data length reserved for mode select */
    if (! opts->mode_6)
        mdpg[1] = 0;    /* mode data length reserved for mode select */
    if (md_len > len) {
        fprintf(stderr, "change_mode_page: mode data length=%d exceeds "
                "allocation length=%d\n", md_len, len);
        return -1;
    }

    for (k = 0; k < mps->num_it_vals; ++k) {
        ivp = &mps->it_vals[k];
        mp_set_value(ivp->val, &ivp->mpi, mdpg + off);
    }

    if ((! (mdpg[off] & 0x80)) && opts->saved) {
        fprintf(stderr, "change_mode_page: mode page indicates it is not "
                "savable but\n    '--save' option given (try without "
                "it)\n");
        return -1;
    }
    mdpg[off] &= 0x7f;   /* mask out PS bit, reserved in mode select */
    if (opts->dummy) {
        fprintf(stderr, "Mode data that would have been written:\n");
        dStrHex((const char *)mdpg, md_len, 1);
        return 0;
    }
    if (opts->mode_6)
        res = sg_ll_mode_select6(sg_fd, 1, opts->saved, mdpg, md_len, 1,
                                 verbose);
    else
        res = sg_ll_mode_select10(sg_fd, 1, opts->saved, mdpg, md_len, 1,
                                  verbose);
    if (0 != res) {
        get_mode_page_name(mps->page_num, mps->subpage_num, pdt, 0, buff,
                           sizeof(buff));
        fprintf(stderr, "change_mode_page: failed setting page: %s\n", buff);
        return -1;
    }
    return 0;
}

/* Return of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> invalid opcode, SG_LIB_CAT_ILLEGAL_REQ ->
 * bad field in cdb, -1 -> other failure */
static int set_def_mode_page(int sg_fd, int pn, int spn, int save,
                             int mode_6, unsigned char * mode_pg,
                             int mode_pg_len, int dummy, int verbose)
{
    int len, off, md_len;
    unsigned char * mdp;
    char ebuff[EBUFF_SZ];
    int ret = -1;
    char buff[128];

    len = mode_pg_len + MODE_DATA_OVERHEAD;
    mdp = malloc(len);
    if (NULL ==mdp) {
        fprintf(stderr, "set_def_mode_page: malloc failed, out of memory\n");
        return -1;
    }
    memset(mdp, 0, len);
    if (mode_6)
        ret = sg_ll_mode_sense6(sg_fd, 0 /* dbd */, 0 /*current */, pn,
                                spn, mdp, ((len > 252) ? 252 : len), 1,
                                verbose);
    else
        ret = sg_ll_mode_sense10(sg_fd, 0 /* llbaa */, 0 /* dbd */,
                                 0 /* current */, pn, spn, mdp, len, 1,
                                 verbose);
    if (0 != ret) {
        get_mode_page_name(pn, spn, -1, 0, buff, sizeof(buff));
        fprintf(stderr, "set_def_mode_page: failed fetching page: %s\n",
                buff);
        goto err_out;
    }
    off = sg_mode_page_offset(mdp, len, mode_6, ebuff, EBUFF_SZ);
    if (off < 0) {
        fprintf(stderr, "set_def_mode_page: page offset failed: %s\n",
                ebuff);
        ret = -1;
        goto err_out;
    }
    if (mode_6)
        md_len = mdp[0] + 1;
    else
        md_len = (mdp[0] << 8) + mdp[1] + 2;
    mdp[0] = 0;        /* mode data length reserved for mode select */
    if (! mode_6)
        mdp[1] = 0;    /* mode data length reserved for mode select */
    if (md_len > len) {
        fprintf(stderr, "set_def_mode_page: mode data length=%d exceeds "
                "allocation length=%d\n", md_len, len);
        ret = -1;
        goto err_out;
    }
    if ((md_len - off) > mode_pg_len) {
        fprintf(stderr, "set_def_mode_page: mode length length=%d exceeds "
                "new contents length=%d\n", md_len - off, mode_pg_len);
        ret = -1;
        goto err_out;
    }
    memcpy(mdp + off, mode_pg, md_len - off);
    mdp[off] &= 0x7f;   /* mask out PS bit, reserved in mode select */
    if (dummy) {
        fprintf(stderr, "Mode data that would have been written:\n");
        dStrHex((const char *)mdp, md_len, 1);
        ret = 0;
        goto err_out;
    }
    if (mode_6)
        ret = sg_ll_mode_select6(sg_fd, 1, save, mdp, md_len, 1,
                                 verbose);
    else
        ret = sg_ll_mode_select10(sg_fd, 1, save, mdp, md_len, 1,
                                  verbose);
    if (0 != ret) {
        get_mode_page_name(pn, spn, -1, 0, buff, sizeof(buff));
        fprintf(stderr, "set_def_mode_page: failed setting page: %s\n",
                buff);
        goto err_out;
    }

err_out:
    free(mdp);
    return ret;
}

static int set_mp_defaults(int sg_fd, int pn, int spn, int pdt, int saved,
                           int mode_6, int dummy, int flexible, int verbose)
{
    int smask, res, len, rep_len;
    unsigned char cur_mp[DEF_MODE_RESP_LEN];
    unsigned char def_mp[DEF_MODE_RESP_LEN];
    char buff[128];
    void * pc_arr[4];

    smask = 0;
    pc_arr[0] = cur_mp;
    pc_arr[1] = NULL;
    pc_arr[2] = def_mp;
    pc_arr[3] = NULL;
    res = sg_get_mode_page_controls(sg_fd, mode_6, pn, spn,
                 flexible, DEF_MODE_RESP_LEN, &smask, pc_arr,
                 &rep_len, verbose);
    if (SG_LIB_CAT_INVALID_OP == res) {
        if (mode_6)
            fprintf(stderr, "6 byte MODE SENSE cdb not supported, "
                    "try again without '-6' option\n");
        else
            fprintf(stderr, "10 byte MODE SENSE cdb not supported, "
                    "try again with '-6' option\n");
        return -1;
    }
    if (verbose && (0 == flexible) && (rep_len > 0xa00)) {
        get_mode_page_name(pn, spn, pdt, 0, buff, sizeof(buff));
        fprintf(stderr, "%s mode page length=%d too long, perhaps "
                "try '--flexible'\n", buff, rep_len);
    }
    if ((smask & 1)) {
        if ((smask & 4)) {
            if (cur_mp[0] & 0x40)
                len = (cur_mp[2] << 8) + cur_mp[3] + 4; /* spf set */
            else
                len = cur_mp[1] + 2; /* spf clear (not subpage) */
            return set_def_mode_page(sg_fd, pn, spn, saved, mode_6, def_mp,
                                     len, dummy, verbose);
        }
        else {
            get_mode_page_name(pn, spn, pdt, 0, buff, sizeof(buff));
            fprintf(stderr, ">> %s mode page (default) not supported\n",
                    buff);
            return -1;
        }
    } else {
        get_mode_page_name(pn, spn, pdt, 0, buff, sizeof(buff));
        fprintf(stderr, ">> %s mode page not supported\n", buff);
        return -1;
    }
}

/* Trying to decode multipliers as sg_get_num() [in sg_libs does] would
 * only confuse things here, so use this local trimmed version */
static int get_num(const char * buf)
{
    int res;
    int num;
    unsigned int unum;

    if ((NULL == buf) || ('\0' == buf[0]))
        return -1;
    if (('0' == buf[0]) && (('x' == buf[1]) || ('X' == buf[1]))) {
        res = sscanf(buf + 2, "%x", &unum);
        num = unum;
    } else
        res = sscanf(buf, "%d", &num);
    if (1 == res)
        return num;
    else
        return -1;
}

static int build_mp_settings(const char * arg,
                             struct mode_page_settings * mps, int clear,
                             int get)
{
    int len, b_sz, num, from, cont;
    unsigned int u;
    char buff[64];
    char acron[64];
    char vb[64];
    const char * cp;
    const char * ncp;
    const char * ecp;
    struct mode_page_it_val * ivp;
    const struct mode_page_item * mpi;
    const struct mode_page_item * prev_mpi;

    b_sz = sizeof(buff) - 1;
    cp = arg;
    while (mps->num_it_vals < MAX_MP_IT_VAL) {
        memset(buff, 0, sizeof(buff));
        ivp = &mps->it_vals[mps->num_it_vals];
        if ('\0' == *cp)
            break;
        ncp = strchr(cp, ',');
        if (ncp) {
            len = ncp - cp;
            if (len <= 0) {
                ++cp;
                continue;
            }
            strncpy(buff, cp, (len < b_sz ? len : b_sz));
        } else
            strncpy(buff, cp, b_sz);
        if (isalpha(buff[0])) {
            ecp = strchr(buff, '=');
            if (ecp) {
                strncpy(acron, buff, ecp - buff);
                acron[ecp - buff] = '\0';
                strcpy(vb, ecp + 1);
                if (0 == strcmp("-1", vb))
                    ivp->val = -1;
                else {
                    ivp->val = get_num(vb);
                    if (-1 == ivp->val) {
                        fprintf(stderr, "build_mp_settings: unable to "
                                "decode: %s value\n", buff);
                        fprintf(stderr, "    expected: <acronym>[=<val>]\n");
                        return -1;
                    }
                }
            } else {
                strcpy(acron, buff);
                ivp->val = ((clear || get) ? 0 : -1);
            }
            from = 0;
            cont = 0;
            prev_mpi = NULL;
            if (get) {
                do {
                    mpi = find_mitem_by_acron(acron, &from);
                    if (NULL == mpi) {
                        if (cont) {
                            mpi = prev_mpi;
                            break;
                        } else
                            fprintf(stderr, "build_mp_settings: couldn't "
                                    "find acronym: %s\n", acron);
                        return -1;
                    }
                    if (mps->page_num < 0) {
                        mps->page_num = mpi->page_num;
                        mps->subpage_num = mpi->subpage_num;
                        break;
                    }
                    cont = 1;
                    prev_mpi = mpi;
                } while ((mps->page_num != mpi->page_num) ||
                         (mps->subpage_num != mpi->subpage_num));
            } else {
                do {
                    mpi = find_mitem_by_acron(acron, &from);
                    if (NULL == mpi) {
                        if (cont) {
                            fprintf(stderr, "build_mp_settings: mode page "
                                    "of acronym: %s [0x%x,0x%x] doesn't "
                                    "match prior\n", acron,
                                    prev_mpi->page_num,
                                    prev_mpi->subpage_num);
                            fprintf(stderr, "    mode page: 0x%x,0x%x\n",
                                    mps->page_num, mps->subpage_num);
                        } else
                            fprintf(stderr, "build_mp_settings: couldn't "
                                    "find acronym: %s\n", acron);
                        return -1;
                    }
                    if (mps->page_num < 0) {
                        mps->page_num = mpi->page_num;
                        mps->subpage_num = mpi->subpage_num;
                        break;
                    }
                    cont = 1;
                    prev_mpi = mpi;
                } while ((mps->page_num != mpi->page_num) ||
                         (mps->subpage_num != mpi->subpage_num));
            }
            if (mpi->num_bits < 32)
                ivp->val &= (1 << mpi->num_bits) - 1;
            ivp->mpi = *mpi;    /* struct assignment */
        } else {    /* expect "byte_off:bit_off:num_bits[=<val>]" */
            if ((0 == strncmp("0x", buff, 2)) ||
                (0 == strncmp("0X", buff, 2))) {
                num = sscanf(buff + 2, "%x:%d:%d=%s", &u,
                             &ivp->mpi.start_bit, &ivp->mpi.num_bits, vb);
                ivp->mpi.start_byte = u;
            } else
                num = sscanf(buff, "%d:%d:%d=%s", &ivp->mpi.start_byte,
                             &ivp->mpi.start_bit, &ivp->mpi.num_bits, vb);
            if (num < 3) {
                fprintf(stderr, "build_mp_settings: unable to decode: %s\n",
                        buff);
                fprintf(stderr, "    expected: byte_off:bit_off:num_bits[="
                        "<val>]\n");
                return -1;
            }
            if (3 == num)
                ivp->val = ((clear || get) ? 0 : -1);
            else {
                if (0 == strcmp("-1", vb))
                    ivp->val = -1;
                else {
                    ivp->val = get_num(vb);
                    if (-1 == ivp->val) {
                        fprintf(stderr, "build_mp_settings: unable to "
                                "decode byte_off:bit_off:num_bits value\n");
                        return -1;
                    }
                }
            }
            ivp->mpi.pdt = -1;  /* don't known pdt now, so don't restrict */
            if (ivp->mpi.start_byte < 0) {
                fprintf(stderr, "build_mp_settings: need positive start "
                        "byte offset\n");
                return -1;
            }
            if ((ivp->mpi.start_bit < 0) || (ivp->mpi.start_bit > 7)) {
                fprintf(stderr, "build_mp_settings: need start bit in "
                        "0..7 range (inclusive)\n");
                return -1;
            }
            if ((ivp->mpi.num_bits < 1) || (ivp->mpi.num_bits > 32)) {
                fprintf(stderr, "build_mp_settings: need number of bits in "
                        "1..32 range (inclusive)\n");
                return -1;
            }
            if (mps->page_num < 0) {
                fprintf(stderr, "build_mp_settings: need '--page=' option "
                        "for mode page number\n");
                return -1;
            } else if (get) {
                ivp->mpi.page_num = mps->page_num;
                ivp->mpi.subpage_num = mps->subpage_num;
            }
            if (ivp->mpi.num_bits < 32)
                ivp->val &= (1 << ivp->mpi.num_bits) - 1;
        }
        ++mps->num_it_vals;
        if (ncp)
            cp = ncp + 1;
        else
            break;
    }
    return 0;
}

static const char * transport_proto_arr[] =
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

static const char * code_set_arr[] =
{
    "Reserved [0x0]",
    "Binary",
    "ASCII",
    "UTF-8",
    "Reserved [0x4]", "Reserved [0x5]", "Reserved [0x6]", "Reserved [0x7]",
    "Reserved [0x8]", "Reserved [0x9]", "Reserved [0xa]", "Reserved [0xb]",
    "Reserved [0xc]", "Reserved [0xd]", "Reserved [0xe]", "Reserved [0xf]",
};

static const char * assoc_arr[] =
{
    "Addressed logical unit",
    "Target port that received request",
    "Target device that contains addressed lu",
    "Reserved [0x3]",
};

static const char * id_type_arr[] =
{
    "vendor specific [0x0]",
    "T10 vendor identication",
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

/* These are target port, device server (i.e. target) and lu identifiers */
static int decode_dev_ids(const char * print_if_found, unsigned char * buff,
                          int len, int match_assoc, int long_out, int do_hex)
{
    int k, j, m, id_len, p_id, c_set, piv, assoc, id_type, i_len;
    int ci_off, c_id, d_id, naa, vsi, printed;
    unsigned long long vsei;
    unsigned long long id_ext;
    const unsigned char * ucp;
    const unsigned char * ip;

    ucp = buff;
    printed = 0;
    for (k = 0, j = 1; k < len; k += id_len, ucp += id_len, ++j) {
        i_len = ucp[3];
        id_len = i_len + 4;
        if (match_assoc < 0)
            printf("  Identification descriptor number %d, "
                   "descriptor length: %d\n", j, id_len);
        if ((k + id_len) > len) {
            fprintf(stderr, "    VPD page error: descriptor length longer "
                    "than\n     remaining response length=%d\n", (len - k));
            return -1;
        }
        ip = ucp + 4;
        p_id = ((ucp[0] >> 4) & 0xf);
        c_set = (ucp[0] & 0xf);
        piv = ((ucp[1] & 0x80) ? 1 : 0);
        assoc = ((ucp[1] >> 4) & 0x3);
        id_type = (ucp[1] & 0xf);
        if ((match_assoc >= 0) && (match_assoc != assoc))
            continue;
        else if (print_if_found && (0 == printed)) {
            printed = 1;
            printf("  %s:\n", print_if_found);
        }
        if (piv && ((1 == assoc) || (2 == assoc)))
            printf("    transport: %s\n", transport_proto_arr[p_id]);
        printf("    id_type: %s,  code_set: %s\n", id_type_arr[id_type],
               code_set_arr[c_set]);
        /* printf("    associated with the %s\n", assoc_arr[assoc]); */
        if (do_hex) {
            printf("    descriptor header(hex): %.2x %.2x %.2x %.2x\n",
                   ucp[0], ucp[1], ucp[2], ucp[3]);
            printf("    identifier:\n");
            dStrHex((const char *)ip, i_len, 0);
            continue;
        }
        switch (id_type) {
        case 0: /* vendor specific */
            dStrHex((const char *)ip, i_len, 0);
            break;
        case 1: /* T10 vendor identication */
            printf("      vendor id: %.8s\n", ip);
            if (i_len > 8)
                printf("      vendor specific: %.*s\n", i_len - 8, ip + 8);
            break;
        case 2: /* EUI-64 based */
            if (! long_out) {
                printf("      [0x");
                if ((8 != i_len) && (12 != i_len) && (16 != i_len)) {
                    printf("      << expect 8, 12 and 16 byte ids, got "
                           "%d>>\n", i_len);
                    dStrHex((const char *)ip, i_len, 0);
                    break;
                }
                for (m = 0; m < i_len; ++m)
                    printf("%02x", (unsigned int)ip[m]);
                printf("]\n");
                break;
            }
            printf("      EUI-64 based %d byte identifier\n", i_len);
            if (1 != c_set) {
                printf("      << expected binary code_set (1)>>\n");
                dStrHex((const char *)ip, i_len, 0);
                break;
            }
            ci_off = 0;
            if (16 == i_len) {
                ci_off = 8;
                id_ext = 0;
                for (m = 0; m < 8; ++m) {
                    if (m > 0)
                        id_ext <<= 8;
                    id_ext |= ip[m];
                }
                printf("      Identifier extension: 0x%llx\n", id_ext);
            } else if ((8 != i_len) && (12 != i_len)) {
                printf("      << can only decode 8, 12 and 16 byte ids>>\n");
                dStrHex((const char *)ip, i_len, 0);
                break;
            }
            c_id = ((ip[ci_off] << 16) | (ip[ci_off + 1] << 8) |
                    ip[ci_off + 2]);
            printf("      IEEE Company_id: 0x%x\n", c_id);
            vsei = 0;
            for (m = 0; m < 5; ++m) {
                if (m > 0)
                    vsei <<= 8;
                vsei |= ip[ci_off + 3 + m];
            }
            printf("      Vendor Specific Extension Identifier: 0x%llx\n",
                   vsei);
            if (12 == i_len) {
                d_id = ((ip[8] << 24) | (ip[9] << 16) | (ip[10] << 8) |
                        ip[11]);
                printf("      Directory ID: 0x%x\n", d_id);
            }
            break;
        case 3: /* NAA */
            if (1 != c_set) {
                printf("      << expected binary code_set (1)>>\n");
                dStrHex((const char *)ip, i_len, 0);
                break;
            }
            naa = (ip[0] >> 4) & 0xff;
            if (! ((2 == naa) || (5 == naa) || (6 == naa))) {
                printf("      << expected naa [0x%x]>>\n", naa);
                dStrHex((const char *)ip, i_len, 0);
                break;
            }
            if (2 == naa) {
                if (8 != i_len) {
                    fprintf(stderr, "      << expected NAA 2 identifier "
                            "length: 0x%x>>\n", i_len);
                    dStrHex((const char *)ip, i_len, 0);
                    break;
                }
                d_id = (((ip[0] & 0xf) << 8) | ip[1]);
                c_id = ((ip[2] << 16) | (ip[3] << 8) | ip[4]);
                vsi = ((ip[5] << 16) | (ip[6] << 8) | ip[7]);
                if (long_out) {
                    printf("      NAA 2, vendor specific identifier A: "
                           "0x%x\n", d_id);
                    printf("      IEEE Company_id: 0x%x\n", c_id);
                    printf("      vendor specific identifier B: 0x%x\n", vsi);
                }
                printf("      [0x");
                for (m = 0; m < 8; ++m)
                    printf("%02x", (unsigned int)ip[m]);
                printf("]\n");
            } else if (5 == naa) {
                if (8 != i_len) {
                    fprintf(stderr, "      << expected NAA 5 identifier "
                            "length: 0x%x>>\n", i_len);
                    dStrHex((const char *)ip, i_len, 0);
                    break;
                }
                c_id = (((ip[0] & 0xf) << 20) | (ip[1] << 12) | 
                        (ip[2] << 4) | ((ip[3] & 0xf0) >> 4));
                vsei = ip[3] & 0xf;
                for (m = 1; m < 5; ++m) {
                    vsei <<= 8;
                    vsei |= ip[3 + m];
                }
                if (long_out) {
                    printf("      NAA 5, IEEE Company_id: 0x%x\n", c_id);
                    printf("      Vendor Specific Identifier: 0x%llx\n",
                           vsei);
                }
                printf("      [0x");
                for (m = 0; m < 8; ++m)
                    printf("%02x", (unsigned int)ip[m]);
                printf("]\n");
            } else if (6 == naa) {
                if (16 != i_len) {
                    fprintf(stderr, "      << expected NAA 6 identifier "
                            "length: 0x%x>>\n", i_len);
                    dStrHex((const char *)ip, i_len, 0);
                    break;
                }
                c_id = (((ip[0] & 0xf) << 20) | (ip[1] << 12) | 
                        (ip[2] << 4) | ((ip[3] & 0xf0) >> 4));
                vsei = ip[3] & 0xf;
                for (m = 1; m < 5; ++m) {
                    vsei <<= 8;
                    vsei |= ip[3 + m];
                }
                if (long_out) {
                    printf("      NAA 6, IEEE Company_id: 0x%x\n", c_id);
                    printf("      Vendor Specific Identifier: 0x%llx\n",
                           vsei);
                    vsei = 0;
                    for (m = 0; m < 8; ++m) {
                        if (m > 0)
                            vsei <<= 8;
                        vsei |= ip[8 + m];
                    }
                    printf("      Vendor Specific Identifier Extension: "
                           "0x%llx\n", vsei);
                }
                printf("      [0x");
                for (m = 0; m < 16; ++m)
                    printf("%02x", (unsigned int)ip[m]);
                printf("]\n");
            }
            break;
        case 4: /* Relative target port */
            if ((1 != c_set) || (1 != assoc) || (4 != i_len)) {
                fprintf(stderr, "      << expected binary code_set, target "
                        "port association, length 4>>\n");
                dStrHex((const char *)ip, i_len, 0);
                break;
            }
            d_id = ((ip[2] << 8) | ip[3]);
            printf("      Relative target port: 0x%x\n", d_id);
            break;
        case 5: /* Target port group */
            if ((1 != c_set) || (1 != assoc) || (4 != i_len)) {
                fprintf(stderr, "      << expected binary code_set, target "
                        "port association, length 4>>\n");
                dStrHex((const char *)ip, i_len, 0);
                break;
            }
            d_id = ((ip[2] << 8) | ip[3]);
            printf("      Target port group: 0x%x\n", d_id);
            break;
        case 6: /* Logical unit group */
            if ((1 != c_set) || (0 != assoc) || (4 != i_len)) {
                fprintf(stderr, "      << expected binary code_set, logical "
                        "unit association, length 4>>\n");
                dStrHex((const char *)ip, i_len, 0);
                break;
            }
            d_id = ((ip[2] << 8) | ip[3]);
            printf("      Logical unit group: 0x%x\n", d_id);
            break;
        case 7: /* MD5 logical unit identifier */
            if ((1 != c_set) || (0 != assoc)) {
                printf("      << expected binary code_set, logical "
                       "unit association>>\n");
                dStrHex((const char *)ip, i_len, 0);
                break;
            }
            printf("      MD5 logical unit identifier:\n");
            dStrHex((const char *)ip, i_len, 0);
            break;
        case 8: /* SCSI name string */
            if (3 != c_set) {
                printf("      << expected UTF-8 code_set>>\n");
                dStrHex((const char *)ip, i_len, 0);
                break;
            }
            printf("      MD5 logical unit identifier:\n");
            /* does %s print out UTF-8 ok??
             * Seems to depend on the locale. Looks ok here with my
             * locale setting: en_AU.UTF-8
             */
            printf("      %s\n", (const char *)ip);
            break;
        default: /* reserved */
            dStrHex((const char *)ip, i_len, 0);
            break;
        }
    }
    return 0;
}

static int process_vpd_page(int sg_fd, int pn, const struct opt_coll * opts,
                            int verbose)
{
    int res, len, k;
    unsigned char b[DEF_INQ_RESP_LEN];
    int sz;
    const char * cp;

    sz = sizeof(b);
    memset(b, 0, sz);
    if (pn < 0) {
        if (opts->all)
            pn = VPD_SUPPORTED_VPDS;  /* if '--all' list supported vpds */
        else
            pn = VPD_DEVICE_ID;  /* default to device identification page */
    }
    res = sg_ll_inquiry(sg_fd, 0, 1, pn, b, sz, 0, verbose);
    if (res) {
        fprintf(stderr, "INQUIRY fetching VPD page=0x%x failed\n", pn);
        return res;
    }
    switch (pn) {
    case VPD_SUPPORTED_VPDS:
        if (b[1] != pn)
            goto dumb_inq;
        len = b[3];
        printf("Supported VPD pages VPD page:\n");
        if (opts->hex) {
            dStrHex((const char *)b, len + 4, 0);
            return 0;
        }
        if (len > 0) {
            for (k = 0; k < len; ++k) {
                cp = get_vpd_name(b[4 + k]);
                if (cp) {
                    if (opts->long_out)
                        printf("  [0x%02x] %s\n", b[4 + k], cp);
                    else
                        printf("  %s\n", cp);
                } else
                    printf("  0x%x\n", b[4 + k]);
            }
        } else
            printf("  <empty>\n");
        break;
    case VPD_DEVICE_ID:
        if (b[1] != pn)
            goto dumb_inq;
        len = (b[2] << 8) + b[3];
        if (len > sz) {
            fprintf(stderr, "Response to device identification VPD page "
                    "truncated\n");
            len = sz;
        }
        if (opts->long_out)
            printf("Device identification [0x83] VPD page:\n");
        else
            printf("Device identification VPD page:\n");
        if (opts->hex) {
            dStrHex((const char *)b, len + 4, 0);
            return 0;
        }
        res = decode_dev_ids(assoc_arr[VPD_ASSOC_LU], b + 4, len,
                             VPD_ASSOC_LU, opts->long_out, opts->hex);
        if (res)
            return res;
        res = decode_dev_ids(assoc_arr[VPD_ASSOC_TPORT], b + 4, len,
                             VPD_ASSOC_TPORT, opts->long_out, opts->hex);
        if (res)
            return res;
        res = decode_dev_ids(assoc_arr[VPD_ASSOC_TDEVICE], b + 4, len,
                             VPD_ASSOC_TDEVICE, opts->long_out, opts->hex);
        if (res)
            return res;
        break;
    case VPD_UNIT_SERIAL_NUM:
        if (b[1] != pn)
            goto dumb_inq;
        len = b[3];
        printf("Unit serial number VPD page:\n");
        if (opts->hex) {
            dStrHex((const char *)b, len + 4, 0);
            return 0;
        }
        if (len > 0)
            printf("  %s\n", b + 4);
        else
            printf("  <empty>\n");
        break;
    default:
        if (b[1] != pn)
            goto dumb_inq;
        len = (b[2] << 8) + b[3] + 4;
        cp = get_vpd_name(pn);
        if (cp)
            fprintf(stderr, "%s VPD page in hex:\n", cp);
        else
            fprintf(stderr, "VPD page 0x%x in hex:\n", pn);
        if (len > (int)sizeof(b)) {
            if (verbose)
                fprintf(stderr, "page length=%d too long, trim\n", len);
            len = sizeof(b);
        }
        dStrHex((const char *)b, len, 0);
        break;
    }
    return 0;

dumb_inq:
    fprintf(stderr, "malformed VPD response, VPD pages probably not "
            "supported\n");
    return -1;
}

static const char * ansi_version_arr[] =
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

static const char * get_ansi_version_str(int version, char * buff,
                                         int buff_len)
{
    version &= 0x7;
    buff[buff_len - 1] = '\0';
    strncpy(buff, ansi_version_arr[version], buff_len - 1);
    return buff;
}

static int open_and_simple_inquiry(const char * device_name, int flags,
                                   int * pdt, const struct opt_coll * opts,
                                   int verbose)
{
    int res, verb, sg_fd, sg_sg_fd, l_pdt;
    struct sg_simple_inquiry_resp sir;

    verb = (verbose > 0) ? verbose - 1 : 0;
    sg_fd = open(device_name, flags);
    if (sg_fd < 0) {
        fprintf(stderr, "open error: %s, flags=0x%x: ", device_name,
                flags);
        perror("");
        return -1;
    } 
    res = sg_simple_inquiry(sg_fd, &sir, 0, verb);
    if (res) {
        if (res < 1) {
            /* could be lk 2.4 and not using a sg device */
            struct utsname a_uts;
            int two, four;

            if (uname(&a_uts) < 0) {
                fprintf(stderr, "uname system call failed, couldn't send "
                        "SG_IO ioctl to %s\n", device_name);
                goto err_out;
            }
            res = sscanf(a_uts.release, "%d.%d", &two, &four);
            if (2 != res) {
                fprintf(stderr, "unable to read uname release\n");
                goto err_out;
            }
            if (! ((2 == two) && (4 == four))) {
                fprintf(stderr, "unable to open %s (not lk 2.4)\n",
                        device_name);
                goto err_out;
            }
            sg_sg_fd = find_corresponding_sg_fd(sg_fd, device_name, flags,
                                                verbose);
            if (sg_sg_fd < 0)
                goto err_out;
            close(sg_fd);
            sg_fd = sg_sg_fd;
            res = sg_simple_inquiry(sg_fd, &sir, 0, verb);
        }
        if (res) {
            fprintf(stderr, "SCSI INQUIRY command failed on %s\n",
                    device_name);
            goto err_out;
        }
    }
    l_pdt = sir.peripheral_type;
    if ((4 == l_pdt) || (7 == l_pdt))
        *pdt = 0;       /* map disk like pdt's to zero */
    else
        *pdt = l_pdt;
    if (0 == opts->hex) {
        printf("    %s: %.8s  %.16s  %.4s",
               device_name, sir.vendor, sir.product, sir.revision);
        if (0 != l_pdt)
            printf("  [pdt=0x%x]", l_pdt);
        printf("\n");
        if (opts->long_out > 1) {
            char buff[32];

            printf("  PQual=%d  Device_type=0x%x  RMB=%d  version=0x%02x ",
                   sir.peripheral_qualifier, l_pdt, sir.rmb, sir.version);
            printf(" [%s]\n", get_ansi_version_str(sir.version, buff,
                                                   sizeof(buff)));
            printf("  [AERC=%d]  [TrmTsk=%d]  NormACA=%d  HiSUP=%d "
                   " Resp_data_format=%d\n  SCCS=%d  ",
                   !!(sir.byte_3 & 0x80), !!(sir.byte_3 & 0x40),
                   !!(sir.byte_3 & 0x20), !!(sir.byte_3 & 0x10),
                   sir.byte_3 & 0x0f, !!(sir.byte_5 & 0x80));
            printf("ACC=%d  TGPS=%d  3PC=%d  Protect=%d ",
                   !!(sir.byte_5 & 0x40), ((sir.byte_5 & 0x30) >> 4),
                   !!(sir.byte_5 & 0x08), !!(sir.byte_5 & 0x01));
            printf(" BQue=%d\n  EncServ=%d  ", !!(sir.byte_6 & 0x80),
                   !!(sir.byte_6 & 0x40));
            if (sir.byte_6 & 0x10)
                printf("MultiP=1 (VS=%d)  ", !!(sir.byte_6 & 0x20));
            else
                printf("MultiP=0  ");
            printf("MChngr=%d  [ACKREQQ=%d]  Addr16=%d\n  [RelAdr=%d]  ",
                   !!(sir.byte_6 & 0x08), !!(sir.byte_6 & 0x04),
                   !!(sir.byte_6 & 0x01), !!(sir.byte_7 & 0x80));
            printf("WBus16=%d  Sync=%d  Linked=%d  [TranDis=%d]  ",
                   !!(sir.byte_7 & 0x20), !!(sir.byte_7 & 0x10),
                   !!(sir.byte_7 & 0x08), !!(sir.byte_7 & 0x04));
            printf("CmdQue=%d\n", !!(sir.byte_7 & 0x02));
        }
        if (opts->long_out || verbose) {
            if (0 != *pdt)
                fprintf(stderr, "     note: given %s rather than disk "
                        "type\n", scsi_ptype_strs[l_pdt]);
        }
    }
    return sg_fd;

err_out:
    close(sg_fd);
    return -1;
}

static int process_mode_page(int sg_fd, struct mode_page_settings * mps,
                             int pn, int spn, int rw, int get,
                             const struct opt_coll * opts, int pdt,
                             int verbose)
{
    int res;
    const struct values_name_t * vnp;

    if ((pn > 0x3e) || (spn > 0xfe)) {
        fprintf(stderr, "Allowable mode page numbers are 0 to 62\n");
        fprintf(stderr, "  Allowable mode subpage numbers are 0 to 254\n");
        return -1;
    }
    if ((pn > 0) && (pdt >= 0)) {
        vnp = get_mode_detail(pn, spn, pdt);
        if (NULL == vnp)
            vnp = get_mode_detail(pn, spn, -1);
        if (vnp && vnp->name && (vnp->pdt >= 0) && (pdt != vnp->pdt)) {
            fprintf(stderr, ">> Warning: %s mode page associated with "
                    "peripheral\n", vnp->name);
            fprintf(stderr, "   device type 0x%x but device pdt is "
                    "0x%x\n", vnp->pdt, pdt);
        }
    }
    if (opts->defaults) {
        res = set_mp_defaults(sg_fd, pn, spn, pdt, opts->saved,
                              opts->mode_6, opts->dummy,
                              opts->flexible, verbose);
        if (0 != res)
            return -1;
    } else if (rw) {
        if (mps->num_it_vals < 1) {
            fprintf(stderr, "no parameters found to set or clear\n");
            return -1;
        }
        res = change_mode_page(sg_fd, pdt, mps, opts, verbose);
        if (0 != res)
            return -1;
    } else if (get) {
        if (mps->num_it_vals < 1) {
            fprintf(stderr, "no parameters found to get\n");
            return -1;
        }
        get_mode_info(sg_fd, mps, pdt, opts, verbose);
    } else
        print_mode_info(sg_fd, pn, spn, pdt, opts, verbose);
    return 0;
}


#ifdef MAP_TO_SG_NODE
typedef struct my_scsi_idlun
{
    int mux4;
    int host_unique_id;

} My_scsi_idlun;

#define DEVNAME_SZ 256
#define MAX_SG_DEVS 256
#define MAX_NUM_NODEVS 4

/* Given a file descriptor 'oth_fd' that refers to a linux SCSI device node
 * this function returns the open file descriptor of the corresponding sg
 * device node. Returns a value >= 0 on success, else -1 or -2. device_name
 * should correspond with oth_fd. If a corresponding sg device node is found
 * then it is opened with flags. The oth_fd is left as is (i.e. it is not
 * closed). sg device node scanning is done "O_RDONLY | O_NONBLOCK".
 * Assumes (and is currently only invoked for) lk 2.4.
 */
static int find_corresponding_sg_fd(int oth_fd, const char * device_name,
                                    int flags, int verbose)
{
    int fd, err, bus, bbus, k, v;
    My_scsi_idlun m_idlun, mm_idlun;
    char name[DEVNAME_SZ];
    int num_nodevs = 0;

    err = ioctl(oth_fd, SCSI_IOCTL_GET_BUS_NUMBER, &bus);
    if (err < 0) {
        fprintf(stderr, "%s does not understand SCSI commands; or "
                "bypasses the linux SCSI\n",
                device_name);
        fprintf(stderr, " subsystem, need sd, scd, st, osst or sg "
                "based device name\n For example: /dev/hdd is not "
                "suitable.\n");
        return -2;
    }
    err = ioctl(oth_fd, SCSI_IOCTL_GET_IDLUN, &m_idlun);
    if (err < 0) {
        if (verbose)
            fprintf(stderr, "%s does not understand SCSI commands(2)\n",
                    device_name);
        return -2;
    }

    fd = -2;
    for (k = 0; (k < MAX_SG_DEVS) && (num_nodevs < MAX_NUM_NODEVS); k++) {
        snprintf(name, sizeof(name), "/dev/sg%d", k);
        fd = open(name, O_RDONLY | O_NONBLOCK);
        if (fd < 0) {
            if ((ENODEV == errno) || (ENOENT == errno) ||
                (ENXIO == errno)) {
                ++num_nodevs;
                continue;       /* step over MAX_NUM_NODEVS holes */
            }
            if (EBUSY == errno)
                continue;   /* step over if O_EXCL already on it */
            else
                break;
        }
        err = ioctl(fd, SCSI_IOCTL_GET_BUS_NUMBER, &bbus);
        if (err < 0) {
            if (verbose)
                perror("SCSI_IOCTL_GET_BUS_NUMBER failed");
            return -2;
        }
        err = ioctl(fd, SCSI_IOCTL_GET_IDLUN, &mm_idlun);
        if (err < 0) {
            if (verbose)
                perror("SCSI_IOCTL_GET_IDLUN failed");
            return -2;
        }
        if ((bus == bbus) && 
            ((m_idlun.mux4 & 0xff) == (mm_idlun.mux4 & 0xff)) &&
            (((m_idlun.mux4 >> 8) & 0xff) == 
                                    ((mm_idlun.mux4 >> 8) & 0xff)) &&
            (((m_idlun.mux4 >> 16) & 0xff) == 
                                    ((mm_idlun.mux4 >> 16) & 0xff)))
            break;
        else {
            close(fd);
            fd = -2;
        }
    }
    if (fd >= 0) {
        if ((ioctl(fd, SG_GET_VERSION_NUM, &v) < 0) || (v < 30000)) {
            fprintf(stderr, "requires lk 2.4 (sg driver) or lk 2.6\n");
            close(fd);
            return -2;
        }
        close(fd);
        if (verbose)
            fprintf(stderr, ">> mapping %s to %s (in lk 2.4 series)\n",
                    device_name, name);
        /* re-opening corresponding sg device with given flags */
        return open(name, flags);
    }
    else
        return fd;
}
#else

static int find_corresponding_sg_fd(int sg_fd, const char * device_name,
                                    int flags, int verbose)
{
    fprintf(stderr, "Mapping %s to sg device name not supported\n",
           device_name);
    return -2;
}
#endif


int main(int argc, char * argv[])
{
    int sg_fd, res, c, pdt, flags;
    struct opt_coll opts;
    const char * clear_str = NULL;
    const char * get_str = NULL;
    const char * set_str = NULL;
    int verbose = 0;
    char device_name[256];
    int pn = -1;
    int spn = -1;
    int rw = 0;
    const struct values_name_t * vnp;
    struct mode_page_settings mp_settings; 
    char * cp;
    int ret = 1;

    memset(&opts, 0, sizeof(opts));
    memset(device_name, 0, sizeof(device_name));
    memset(&mp_settings, 0, sizeof(mp_settings));
    pdt = -1;
    while (1) {
        int option_index = 0;

        c = getopt_long(argc, argv, "6ac:dDefg:hHilp:s:SvV", long_options,
                        &option_index);
        if (c == -1)
            break;

        switch (c) {
        case '6':
            opts.mode_6 = 1;
            break;
        case 'a':
            opts.all = 1;
            break;
        case 'c':
            clear_str = optarg;
            rw = 1;
            break;
        case 'd':
            opts.dummy = 1;
            break;
        case 'D':
            opts.defaults = 1;
            rw = 1;
            break;
        case 'e':
            opts.enumerate = 1;
            break;
        case 'f':
            opts.flexible = 1;
            break;
        case 'g':
            get_str = optarg;
            break;
        case 'h':
        case '?':
            usage();
            return 0;
        case 'H':
            opts.hex = 1;
            break;
        case 'i':
            opts.inquiry = 1;
            break;
        case 'l':
            ++opts.long_out;
            break;
        case 'p':
            if (isalpha(optarg[0])) {
                vnp = find_mp_by_acron(optarg);
                if (NULL == vnp) {
                    vnp = find_vpd_by_acron(optarg);
                    if (NULL == vnp) {
                        fprintf(stderr, "acronym does not match a mode nor "
                                "a VPD page\n");
                        return 1;
                    } else {
                        pn = vnp->value;
                        opts.inquiry = 1;
                    }
                } else {
                    pn = vnp->value;
                    spn = vnp->subvalue;
                    pdt = vnp->pdt;
                }
            } else {
                cp = strchr(optarg, ',');
                pn = get_num(optarg);
                if ((pn < 0) || (pn > 255)) {
                    fprintf(stderr, "Bad page code value after '-p' "
                            "switch\n");
                    return 1;
                }
                if (cp) {
                    spn = get_num(cp + 1);
                    if ((spn < 0) || (spn > 255)) {
                        fprintf(stderr, "Bad page code value after "
                                "'-p' switch\n");
                        return 1;
                    }
                } else
                    spn = 0;
            }
            break;
        case 's':
            set_str = optarg;
            rw = 1;
            break;
        case 'S':
            opts.saved = 1;
            rw = 1;
            break;
        case 'v':
            ++verbose;
            break;
        case 'V':
            fprintf(stderr, ME "version: %s\n", version_str);
            return 0;
        default:
            fprintf(stderr, "unrecognised switch code 0x%x ??\n", c);
            usage();
            return 1;
        }
    }
    if (optind < argc) {
        if ('\0' == device_name[0]) {
            strncpy(device_name, argv[optind], sizeof(device_name) - 1);
            device_name[sizeof(device_name) - 1] = '\0';
            ++optind;
        }
        if (optind < argc) {
            for (; optind < argc; ++optind)
                fprintf(stderr, "Unexpected extra argument: %s\n",
                        argv[optind]);
            usage();
            return 1;
        }
    }

    if (opts.inquiry) {
        if (set_str || clear_str || get_str || opts.defaults || opts.saved) {
            fprintf(stderr, "'--inquiry' option lists VPD pages so other "
                    "options that are\nconcerned with mode pages are "
                    "inappropriate\n");
            return 1;
        }
        if ((pn > 255) || (spn > 0)) {
            fprintf(stderr, "VPD page numbers are from 0 to 255 with no "
                    "subpages\n");
            return 1;
        }
        if (opts.enumerate) {
            printf("VPD pages:\n");
            list_vpds();
            return 0;
        }
    } else {
        /* assume mode pages */
        if (pn < 0) {
            mp_settings.page_num = -1;
            mp_settings.subpage_num = -1;
        } else {
            mp_settings.page_num = pn;
            mp_settings.subpage_num = spn;
        }
        if (get_str) {
            if (set_str || clear_str) {
                fprintf(stderr, "'--get=' can't be used with '--set=' or "
                        "'--clear='\n");
                return 1;
            }
            if (build_mp_settings(get_str, &mp_settings, 0, 1))
                return 1;
        }
        if (opts.enumerate) {
            if (device_name[0] || set_str || clear_str || get_str ||
                opts.saved)
                /* think about --get= with --enumerate */
                printf("Most option including <scsi_disk> are ignored when "
                       "'--enumerate' is given\n");
            if (pn < 0) {
                printf("Mode pages:\n");
                list_mps();
            }
            if (opts.all || (pn >= 0))
                list_mitems(pn, spn, pdt);
            return 0;
        }

        if (opts.defaults && (set_str || clear_str || get_str)) {
            fprintf(stderr, "'--get=', '--set=' or '--clear=' can't be used "
                    "with '--defaults'\n");
            return 1;
        }

        if (set_str) {
            if (build_mp_settings(set_str, &mp_settings, 0, 0))
                return 1;
        }
        if (clear_str) {
            if (build_mp_settings(clear_str, &mp_settings, 1, 0))
                return 1;
        }
 
        if (verbose && (mp_settings.num_it_vals > 0))
            list_mp_settings(&mp_settings, (NULL != get_str));

        if (opts.defaults && (pn < 0)) {
            fprintf(stderr, "to set defaults, the '--page=' option must "
                    "be used\n");
            return 1;
        }
    }

    if (0 == device_name[0]) {
        fprintf(stderr, "missing device name!\n");
        usage();
        return 1;
    }

    pdt = -1;
    flags = (O_NONBLOCK | (rw ? O_RDWR : O_RDONLY));
    sg_fd = open_and_simple_inquiry(device_name, flags, &pdt, &opts, verbose);
    if (sg_fd < 0) 
        return 1;

    if (opts.inquiry) {
        res = process_vpd_page(sg_fd, pn, &opts, verbose);
        if (res)
            goto err_out;
    } else {    /* mode page */
        res = process_mode_page(sg_fd, &mp_settings, pn, spn, rw,
                                (NULL != get_str), &opts, pdt, verbose);
        if (res)
            goto err_out;
    }
    ret = 0;

err_out:
    res = close(sg_fd);
    if (res < 0) {
        perror("close error");
        return 1;
    }
    return ret;
}

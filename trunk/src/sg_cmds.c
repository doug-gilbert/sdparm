/*
 * Copyright (c) 1999-2005 Douglas Gilbert.
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

/*
 * CONTENTS
 *    Some SCSI commands are executed in many contexts and hence began
 *    to appear in several sg3_utils utilities. This files centralizes
 *    some of the low level command execution code. In most cases the
 *    interpretation of the command response is left to the each
 *    utility.
 *    One example is the SCSI INQUIRY command which is often required
 *    to identify and categorize (e.g. disk, tape or enclosure device)
 *    a SCSI target device.
 * CHANGELOG
 *      v1.00 (20041018)
 *        fetch low level command execution code from other utilities
 *      v1.01 (20041026)
 *        fix "ll" read capacity calls, add sg_ll_report_luns
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <scsi/scsi.h>
#include <scsi/sg.h>
#include <scsi/sg_lib.h>
#include <scsi/sg_cmds.h>

static char * version_str = "1.12 20050519";


#define SENSE_BUFF_LEN 32       /* Arbitrary, could be larger */
#define DEF_TIMEOUT 60000       /* 60,000 millisecs == 60 seconds */
#define LONG_TIMEOUT 7200000    /* 7,200,000 millisecs == 120 minutes */
#define EBUFF_SZ 256

#define INQUIRY_CMD     0x12
#define INQUIRY_CMDLEN  6
#define SYNCHRONIZE_CACHE_CMD     0x35
#define SYNCHRONIZE_CACHE_CMDLEN  10
#define SERVICE_ACTION_IN_16_CMD 0x9e
#define SERVICE_ACTION_IN_16_CMDLEN 16
#define READ_CAPACITY_16_SA 0x10
#define READ_CAPACITY_10_CMD 0x25
#define READ_CAPACITY_10_CMDLEN 10
#define MODE_SENSE6_CMD      0x1a
#define MODE_SENSE6_CMDLEN   6
#define MODE_SENSE10_CMD     0x5a
#define MODE_SENSE10_CMDLEN  10
#define MODE_SELECT6_CMD   0x15
#define MODE_SELECT6_CMDLEN   6
#define MODE_SELECT10_CMD   0x55
#define MODE_SELECT10_CMDLEN  10
#define REQUEST_SENSE_CMD 0x3
#define REQUEST_SENSE_CMDLEN 6
#define REPORT_LUNS_CMD 0xa0
#define REPORT_LUNS_CMDLEN 12
#define MAINTENANCE_IN_CMD 0xa3
#define MAINTENANCE_IN_CMDLEN 12
#define REPORT_TGT_PRT_GRP_SA 0x0a
#define LOG_SENSE_CMD     0x4d
#define LOG_SENSE_CMDLEN  10
#define LOG_SELECT_CMD     0x4c
#define LOG_SELECT_CMDLEN  10
#define TUR_CMD  0x0
#define TUR_CMDLEN  6
#define SEND_DIAGNOSTIC_CMD   0x1d
#define SEND_DIAGNOSTIC_CMDLEN  6
#define RECEIVE_DIAGNOSTICS_CMD   0x1c
#define RECEIVE_DIAGNOSTICS_CMDLEN  6
#define READ_DEFECT10_CMD     0x37
#define READ_DEFECT10_CMDLEN    10
#define SERVICE_ACTION_IN_12_CMD 0xab
#define SERVICE_ACTION_IN_12_CMDLEN 12
#define READ_MEDIA_SERIAL_NUM_SA 0x1

#define MODE6_RESP_HDR_LEN 4
#define MODE10_RESP_HDR_LEN 8
#define MODE_RESP_ARB_LEN 1024


const char * sg_cmds_version()
{
    return version_str;
}

/* Invokes a SCSI INQUIRY command and yields the response */
/* Returns 0 when successful, -1 -> SG_IO ioctl failed, -2 -> bad response */
int sg_ll_inquiry(int sg_fd, int cmddt, int evpd, int pg_op, 
                  void * resp, int mx_resp_len, int noisy, int verbose)
{
    int res, k;
    unsigned char inqCmdBlk[INQUIRY_CMDLEN] = {INQUIRY_CMD, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_io_hdr io_hdr;

    if (cmddt)
        inqCmdBlk[1] |= 2;
    if (evpd)
        inqCmdBlk[1] |= 1;
    inqCmdBlk[2] = (unsigned char)pg_op;
    /* 16 bit allocation length (was 8) is a recent SPC-3 addition */
    inqCmdBlk[3] = (unsigned char)((mx_resp_len >> 8) & 0xff);
    inqCmdBlk[4] = (unsigned char)(mx_resp_len & 0xff);
    if (NULL == sg_warnings_str)
        sg_warnings_str = stderr;
    if (verbose) {
        fprintf(sg_warnings_str, "    inquiry cdb: ");
        for (k = 0; k < INQUIRY_CMDLEN; ++k)
            fprintf(sg_warnings_str, "%02x ", inqCmdBlk[k]);
        fprintf(sg_warnings_str, "\n");
    }
    memset(&io_hdr, 0, sizeof(struct sg_io_hdr));
    memset(sense_b, 0, sizeof(sense_b));
    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = sizeof(inqCmdBlk);
    io_hdr.mx_sb_len = sizeof(sense_b);
    io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
    io_hdr.dxfer_len = mx_resp_len;
    io_hdr.dxferp = resp;
    io_hdr.cmdp = inqCmdBlk;
    io_hdr.sbp = sense_b;
    io_hdr.timeout = DEF_TIMEOUT;

    if (ioctl(sg_fd, SG_IO, &io_hdr) < 0) {
        if (noisy || verbose)
            fprintf(sg_warnings_str, "SG_IO (inquiry) error: %s\n",
                    safe_strerror(errno));
        return -1;
    }
    if (verbose > 2)
        fprintf(sg_warnings_str, "      duration=%u ms\n", io_hdr.duration);
    res = sg_err_category3(&io_hdr);
    switch (res) {
    case SG_LIB_CAT_RECOVERED:
        if (noisy || verbose)
            sg_chk_n_print3("Inquiry", &io_hdr);
        /* fall through */
    case SG_LIB_CAT_CLEAN:
        if (verbose && io_hdr.resid)
            fprintf(sg_warnings_str, "    inquiry: resid=%d\n", io_hdr.resid);
        return 0;
    default:
        if (noisy || verbose) {
            char ebuff[EBUFF_SZ];

            if (evpd)
                snprintf(ebuff, EBUFF_SZ, "Inquiry error, VPD page=0x%x",
                         pg_op);
            else if (cmddt)
                snprintf(ebuff, EBUFF_SZ, "Inquiry error, CmdDt opcode=0x%x",
                         pg_op);
            else
                snprintf(ebuff, EBUFF_SZ, "Inquiry error, [standard]");
            sg_chk_n_print3(ebuff, &io_hdr);
        }
        return -2;
    }
}

/* Yields most of first 36 bytes of a standard INQUIRY (evpd==0) response. */
/* Returns 0 when successful, -1 -> SG_IO ioctl failed, -2 -> bad response */
int sg_simple_inquiry(int sg_fd, struct sg_simple_inquiry_resp * inq_data,
                      int noisy, int verbose)
{
    int res, k;
    unsigned char inqCmdBlk[INQUIRY_CMDLEN] = {INQUIRY_CMD, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_io_hdr io_hdr;
    unsigned char inq_resp[36];

    if (inq_data) {
        memset(inq_data, 0, sizeof(* inq_data));
        inq_data->peripheral_qualifier = 0x3;
        inq_data->peripheral_type = 0x1f;
    }
    inqCmdBlk[4] = (unsigned char)sizeof(inq_resp);
    if (NULL == sg_warnings_str)
        sg_warnings_str = stderr;
    if (verbose) {
        fprintf(sg_warnings_str, "    inquiry cdb: ");
        for (k = 0; k < INQUIRY_CMDLEN; ++k)
            fprintf(sg_warnings_str, "%02x ", inqCmdBlk[k]);
        fprintf(sg_warnings_str, "\n");
    }
    memset(&io_hdr, 0, sizeof(struct sg_io_hdr));
    memset(sense_b, 0, sizeof(sense_b));
    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = sizeof(inqCmdBlk);
    io_hdr.mx_sb_len = sizeof(sense_b);
    io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
    io_hdr.dxfer_len = sizeof(inq_resp);
    io_hdr.dxferp = inq_resp;
    io_hdr.cmdp = inqCmdBlk;
    io_hdr.sbp = sense_b;
    io_hdr.timeout = DEF_TIMEOUT;

    if (ioctl(sg_fd, SG_IO, &io_hdr) < 0) {
        if (noisy || verbose)
            fprintf(sg_warnings_str, "SG_IO (inquiry) error: %s\n",
                    safe_strerror(errno));
        return -1;
    }
    if (verbose > 2)
        fprintf(sg_warnings_str, "      duration=%u ms\n", io_hdr.duration);
    res = sg_err_category3(&io_hdr);
    switch (res) {
    case SG_LIB_CAT_RECOVERED:
        if (noisy || verbose)
            sg_chk_n_print3("Inquiry", &io_hdr);
        /* fall through */
    case SG_LIB_CAT_CLEAN:
        if (inq_data) {
            inq_data->peripheral_qualifier = (inq_resp[0] >> 5) & 0x7;
            inq_data->peripheral_type = inq_resp[0] & 0x1f;
            inq_data->rmb = (inq_resp[1] & 0x80) ? 1 : 0;
            inq_data->version = inq_resp[2];
            inq_data->byte_3 = inq_resp[3];
            inq_data->byte_5 = inq_resp[5];
            inq_data->byte_6 = inq_resp[6];
            inq_data->byte_7 = inq_resp[7];
            memcpy(inq_data->vendor, inq_resp + 8, 8);
            memcpy(inq_data->product, inq_resp + 16, 16);
            memcpy(inq_data->revision, inq_resp + 32, 4);
        }
        if (verbose && io_hdr.resid)
            fprintf(sg_warnings_str, "    inquiry: resid=%d\n", io_hdr.resid);
        return 0;
    default:
        if (noisy) {
            char ebuff[EBUFF_SZ];

            snprintf(ebuff, EBUFF_SZ, "Inquiry error ");
            sg_chk_n_print3(ebuff, &io_hdr);
        }
        return -2;
    }
}

/* Invokes a SCSI TEST UNIT READY command.
 * 'pack_id' is just for diagnostics, safe to set to 0.
 * Return of 0 -> success, -1 -> failure */
int sg_ll_test_unit_ready(int sg_fd, int pack_id, int noisy, int verbose)
{
    int res, k;
    unsigned char turCmbBlk[TUR_CMDLEN] = {TUR_CMD, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_io_hdr io_hdr;

    if (NULL == sg_warnings_str)
        sg_warnings_str = stderr;
    if (verbose) {
        fprintf(sg_warnings_str, "    test unit ready cdb: ");
        for (k = 0; k < TUR_CMDLEN; ++k)
            fprintf(sg_warnings_str, "%02x ", turCmbBlk[k]);
        fprintf(sg_warnings_str, "\n");
    }
    memset(&io_hdr, 0, sizeof(struct sg_io_hdr));
    memset(sense_b, 0, sizeof(sense_b));
    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = sizeof(turCmbBlk);
    io_hdr.mx_sb_len = sizeof(sense_b);
    io_hdr.dxfer_direction = SG_DXFER_NONE;
    io_hdr.dxfer_len = 0;
    io_hdr.dxferp = NULL;
    io_hdr.cmdp = turCmbBlk;
    io_hdr.sbp = sense_b;
    io_hdr.timeout = DEF_TIMEOUT;
    io_hdr.pack_id = pack_id;   /* diagnostic: safe to set to 0 */

    if (ioctl(sg_fd, SG_IO, &io_hdr) < 0) {
        if (noisy || verbose)
            fprintf(sg_warnings_str, "test_unit_ready (SG_IO) error: %s\n",
                    safe_strerror(errno));
        return -1;
    }
    if (verbose > 2)
        fprintf(sg_warnings_str, "      duration=%u ms\n", io_hdr.duration);
    res = sg_err_category3(&io_hdr);
    switch (res) {
    case SG_LIB_CAT_CLEAN:
        return 0;
    default:
        if (noisy || verbose)
            sg_chk_n_print3("test unit ready", &io_hdr);
        return -1;
    }
}

/* Invokes a SCSI SYNCHRONIZE CACHE (10) command. Return of 0 -> success,
 * -1 -> failure, SG_LIB_CAT_MEDIA_CHANGED -> repeat, SG_LIB_CAT_INVALID_OP
 * -> cdb not supported, SG_LIB_CAT_IlLEGAL_REQ -> bad field in cdb */
int sg_ll_sync_cache_10(int sg_fd, int sync_nv, int immed, int group,
                        unsigned int lba, unsigned int count, int noisy,
                        int verbose)
{
    int res, k;
    unsigned char scCmdBlk[SYNCHRONIZE_CACHE_CMDLEN] =
                {SYNCHRONIZE_CACHE_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_io_hdr io_hdr;

    if (sync_nv)
        scCmdBlk[1] |= 4;
    if (immed)
        scCmdBlk[1] |= 2;
    scCmdBlk[2] = (lba >> 24) & 0xff;
    scCmdBlk[3] = (lba >> 16) & 0xff;
    scCmdBlk[4] = (lba >> 8) & 0xff;
    scCmdBlk[5] = lba & 0xff;
    scCmdBlk[6] = group & 0x1f;
    if (NULL == sg_warnings_str)
        sg_warnings_str = stderr;
    if (count > 0xffff) {
        fprintf(sg_warnings_str, "count too big\n");
        return -1;
    }
    scCmdBlk[7] = (count >> 8) & 0xff;
    scCmdBlk[8] = count & 0xff;

    if (verbose) {
        fprintf(sg_warnings_str, "    synchronize cache(10) cdb: ");
        for (k = 0; k < SYNCHRONIZE_CACHE_CMDLEN; ++k)
            fprintf(sg_warnings_str, "%02x ", scCmdBlk[k]);
        fprintf(sg_warnings_str, "\n");
    }
    memset(&io_hdr, 0, sizeof(struct sg_io_hdr));
    memset(sense_b, 0, sizeof(sense_b));
    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = sizeof(scCmdBlk);
    io_hdr.mx_sb_len = sizeof(sense_b);
    io_hdr.dxfer_direction = SG_DXFER_NONE;
    io_hdr.dxfer_len = 0;
    io_hdr.dxferp = NULL;
    io_hdr.cmdp = scCmdBlk;
    io_hdr.sbp = sense_b;
    io_hdr.timeout = DEF_TIMEOUT;

    if (ioctl(sg_fd, SG_IO, &io_hdr) < 0) {
        if (noisy || verbose)
            fprintf(sg_warnings_str, "synchronize_cache (SG_IO) error: %s\n",
                    safe_strerror(errno));
        return -1;
    }
    if (verbose > 2)
        fprintf(sg_warnings_str, "      duration=%u ms\n", io_hdr.duration);
    res = sg_err_category3(&io_hdr);
    switch (res) {
    case SG_LIB_CAT_CLEAN:
        return 0;
    case SG_LIB_CAT_MEDIA_CHANGED:
    case SG_LIB_CAT_INVALID_OP:
    case SG_LIB_CAT_ILLEGAL_REQ:
        if (verbose > 1)
            sg_chk_n_print3("synchronize cache", &io_hdr);
        return res;
    default:
        if (noisy || verbose)
            sg_chk_n_print3("synchronize cache", &io_hdr);
        return -1;
    }
    return 0;
}


/* Invokes a SCSI READ CAPACITY (16) command. Returns 0 -> success,
 * -1 -> failure, SG_LIB_CAT_MEDIA_CHANGED -> repeat, SG_LIB_CAT_INVALID_OP
 *  -> cdb not supported, SG_LIB_CAT_IlLEGAL_REQ -> bad field in cdb */
int sg_ll_readcap_16(int sg_fd, int pmi, unsigned long long llba, 
                     void * resp, int mx_resp_len, int verbose)
{
    int k, res;
    unsigned char rcCmdBlk[SERVICE_ACTION_IN_16_CMDLEN] = 
                        {SERVICE_ACTION_IN_16_CMD, READ_CAPACITY_16_SA, 
                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_io_hdr io_hdr;

    if (pmi) { /* lbs only valid when pmi set */
        rcCmdBlk[14] |= 1;
        rcCmdBlk[2] = (llba >> 56) & 0xff;
        rcCmdBlk[3] = (llba >> 48) & 0xff;
        rcCmdBlk[4] = (llba >> 40) & 0xff;
        rcCmdBlk[5] = (llba >> 32) & 0xff;
        rcCmdBlk[6] = (llba >> 24) & 0xff;
        rcCmdBlk[7] = (llba >> 16) & 0xff;
        rcCmdBlk[8] = (llba >> 8) & 0xff;
        rcCmdBlk[9] = llba & 0xff;
    }
    /* Allocation length, no guidance in SBC-2 rev 15b */
    rcCmdBlk[10] = (mx_resp_len >> 24) & 0xff;
    rcCmdBlk[11] = (mx_resp_len >> 16) & 0xff;
    rcCmdBlk[12] = (mx_resp_len >> 8) & 0xff;
    rcCmdBlk[13] = mx_resp_len & 0xff;
    if (NULL == sg_warnings_str)
        sg_warnings_str = stderr;
    if (verbose) {
        fprintf(sg_warnings_str, "    read capacity (16) cdb: ");
        for (k = 0; k < SERVICE_ACTION_IN_16_CMDLEN; ++k)
            fprintf(sg_warnings_str, "%02x ", rcCmdBlk[k]);
        fprintf(sg_warnings_str, "\n");
    }
    memset(&io_hdr, 0, sizeof(struct sg_io_hdr));
    memset(sense_b, 0, sizeof(sense_b));
    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = sizeof(rcCmdBlk);
    io_hdr.mx_sb_len = sizeof(sense_b);
    io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
    io_hdr.dxfer_len = mx_resp_len;
    io_hdr.dxferp = resp;
    io_hdr.cmdp = rcCmdBlk;
    io_hdr.sbp = sense_b;
    io_hdr.timeout = DEF_TIMEOUT;

    if (ioctl(sg_fd, SG_IO, &io_hdr) < 0) {
        if (verbose)
            fprintf(sg_warnings_str, "read_capacity16 (SG_IO) error: %s\n",
                    safe_strerror(errno));
        return -1;
    }
    if (verbose > 2)
        fprintf(sg_warnings_str, "      duration=%u ms\n", io_hdr.duration);
    res = sg_err_category3(&io_hdr);
    switch (res) {
    case SG_LIB_CAT_RECOVERED:
        if (verbose)
            sg_chk_n_print3("Read capacity (16)", &io_hdr);
        /* fall through */
    case SG_LIB_CAT_CLEAN:
        if (verbose && io_hdr.resid)
            fprintf(sg_warnings_str, "    read_capacity16: resid=%d\n",
                    io_hdr.resid);
        return 0;
    case SG_LIB_CAT_INVALID_OP:
    case SG_LIB_CAT_ILLEGAL_REQ:
    case SG_LIB_CAT_MEDIA_CHANGED:
        if (verbose > 1)
            sg_chk_n_print3("READ CAPACITY 16 command error", &io_hdr);
        return res;
    default:
        sg_chk_n_print3("READ CAPACITY 16 command error", &io_hdr);
        return -1;
    }
}

/* Invokes a SCSI READ CAPACITY (10) command. Return of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> invalid opcode, SG_LIB_CAT_MEDIA_CHANGED
 * -> media changed, SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb,
 * -1 -> other failure */
int sg_ll_readcap_10(int sg_fd, int pmi, unsigned int lba, 
                     void * resp, int mx_resp_len, int verbose)
{
    int k, res;
    unsigned char rcCmdBlk[READ_CAPACITY_10_CMDLEN] =
                         {READ_CAPACITY_10_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_io_hdr io_hdr;

    if (pmi) { /* lbs only valid when pmi set */
        rcCmdBlk[8] |= 1;
        rcCmdBlk[2] = (lba >> 24) & 0xff;
        rcCmdBlk[3] = (lba >> 16) & 0xff;
        rcCmdBlk[4] = (lba >> 8) & 0xff;
        rcCmdBlk[5] = lba & 0xff;
    }
    if (NULL == sg_warnings_str)
        sg_warnings_str = stderr;
    if (verbose) {
        fprintf(sg_warnings_str, "    read capacity (10) cdb: ");
        for (k = 0; k < READ_CAPACITY_10_CMDLEN; ++k)
            fprintf(sg_warnings_str, "%02x ", rcCmdBlk[k]);
        fprintf(sg_warnings_str, "\n");
    }
    memset(&io_hdr, 0, sizeof(struct sg_io_hdr));
    memset(sense_b, 0, sizeof(sense_b));
    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = sizeof(rcCmdBlk);
    io_hdr.mx_sb_len = sizeof(sense_b);
    io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
    io_hdr.dxfer_len = mx_resp_len;     /* should be 8 */
    io_hdr.dxferp = resp;
    io_hdr.cmdp = rcCmdBlk;
    io_hdr.sbp = sense_b;
    io_hdr.timeout = DEF_TIMEOUT;

    if (ioctl(sg_fd, SG_IO, &io_hdr) < 0) {
        if (verbose)
            fprintf(sg_warnings_str, "read_capacity (10) (SG_IO) error: %s\n",
                    safe_strerror(errno));
        return -1;
    }
    if (verbose > 2)
        fprintf(sg_warnings_str, "      duration=%u ms\n", io_hdr.duration);
    res = sg_err_category3(&io_hdr);
    switch (res) {
    case SG_LIB_CAT_RECOVERED:
        if (verbose)
            sg_chk_n_print3("Read capacity (10)", &io_hdr);
        /* fall through */
    case SG_LIB_CAT_CLEAN:
        if (verbose && io_hdr.resid)
            fprintf(sg_warnings_str, "    read_capacity10: resid=%d\n",
                    io_hdr.resid);
        return 0;
    case SG_LIB_CAT_INVALID_OP:
    case SG_LIB_CAT_ILLEGAL_REQ:
    case SG_LIB_CAT_MEDIA_CHANGED:
        if (verbose > 1)
            sg_chk_n_print3("READ CAPACITY 10 command error", &io_hdr);
        return res;
    default:
        sg_chk_n_print3("READ CAPACITY 10 command error", &io_hdr);
        return -1;
    }
}

/* Invokes a SCSI MODE SENSE (6) command. Return of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> invalid opcode, SG_LIB_CAT_ILLEGAL_REQ ->
 * bad field in cdb, -1 -> other failure */
int sg_ll_mode_sense6(int sg_fd, int dbd, int pc, int pg_code, int sub_pg_code,
                      void * resp, int mx_resp_len, int noisy, int verbose)
{
    int res, k;
    unsigned char modesCmdBlk[MODE_SENSE6_CMDLEN] = 
        {MODE_SENSE6_CMD, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_io_hdr io_hdr;

    modesCmdBlk[1] = (unsigned char)(dbd ? 0x8 : 0);
    modesCmdBlk[2] = (unsigned char)(((pc << 6) & 0xc0) | (pg_code & 0x3f));
    modesCmdBlk[3] = (unsigned char)(sub_pg_code & 0xff);
    modesCmdBlk[4] = (unsigned char)(mx_resp_len & 0xff);
    if (NULL == sg_warnings_str)
        sg_warnings_str = stderr;
    if (mx_resp_len > 0xff) {
        fprintf(sg_warnings_str, "mx_resp_len too big\n");
        return -1;
    }
    if (verbose) {
        fprintf(sg_warnings_str, "    mode sense (6) cdb: ");
        for (k = 0; k < MODE_SENSE6_CMDLEN; ++k)
            fprintf(sg_warnings_str, "%02x ", modesCmdBlk[k]);
        fprintf(sg_warnings_str, "\n");
    }

    memset(&io_hdr, 0, sizeof(struct sg_io_hdr));
    memset(sense_b, 0, sizeof(sense_b));
    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = MODE_SENSE6_CMDLEN;
    io_hdr.mx_sb_len = sizeof(sense_b);
    io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
    io_hdr.dxfer_len = mx_resp_len;
    io_hdr.dxferp = resp;
    io_hdr.cmdp = modesCmdBlk;
    io_hdr.sbp = sense_b;
    io_hdr.timeout = DEF_TIMEOUT;

    if (ioctl(sg_fd, SG_IO, &io_hdr) < 0) {
        if (noisy || verbose)
            fprintf(sg_warnings_str, "mode sense (6) SG_IO error: %s\n",
                    safe_strerror(errno));
        return -1;
    }
    if (verbose > 2)
        fprintf(sg_warnings_str, "      duration=%u ms\n", io_hdr.duration);
    res = sg_err_category3(&io_hdr);
    switch (res) {
    case SG_LIB_CAT_RECOVERED:
        if (noisy || verbose)
            sg_chk_n_print3("Mode sense (6)", &io_hdr);
        /* fall through */
    case SG_LIB_CAT_CLEAN:
        if (verbose && io_hdr.resid)
            fprintf(sg_warnings_str, "    mode sense (6): resid=%d\n",
                    io_hdr.resid);
        if (verbose > 2) {
            k = mx_resp_len - io_hdr.resid;
            if (k > 0) {
                fprintf(sg_warnings_str, "    mode sense (6): response%s\n",
                        (k > 256 ? ", first 256 bytes" : ""));
                dStrHex(resp, (k > 256 ? 256 : k), -1);
            }
        }
        return 0;
    case SG_LIB_CAT_INVALID_OP:
    case SG_LIB_CAT_ILLEGAL_REQ:
        if (verbose > 1)
            sg_chk_n_print3("Mode sense (6) error", &io_hdr);
        return res;
    default:
        if (noisy || verbose) {
            char ebuff[EBUFF_SZ];

            snprintf(ebuff, EBUFF_SZ, "Mode sense (6) error, dbd=%d "
                    "pc=%d page_code=%x sub_page_code=%x\n     ", dbd,
                    pc, pg_code, sub_pg_code);
            sg_chk_n_print3(ebuff, &io_hdr);
        }
        return -1;
    }
}

/* Invokes a SCSI MODE SENSE (10) command. Return of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> invalid opcode, SG_LIB_CAT_ILLEGAL_REQ ->
 * bad field in cdb, -1 -> other failure */
int sg_ll_mode_sense10(int sg_fd, int llbaa, int dbd, int pc, int pg_code,
                       int sub_pg_code, void * resp, int mx_resp_len,
                       int noisy, int verbose)
{
    int res, k;
    unsigned char modesCmdBlk[MODE_SENSE10_CMDLEN] = 
        {MODE_SENSE10_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_io_hdr io_hdr;

    modesCmdBlk[1] = (unsigned char)((dbd ? 0x8 : 0) | (llbaa ? 0x10 : 0));
    modesCmdBlk[2] = (unsigned char)(((pc << 6) & 0xc0) | (pg_code & 0x3f));
    modesCmdBlk[3] = (unsigned char)(sub_pg_code & 0xff);
    modesCmdBlk[7] = (unsigned char)((mx_resp_len >> 8) & 0xff);
    modesCmdBlk[8] = (unsigned char)(mx_resp_len & 0xff);
    if (NULL == sg_warnings_str)
        sg_warnings_str = stderr;
    if (mx_resp_len > 0xffff) {
        fprintf(sg_warnings_str, "mx_resp_len too big\n");
        return -1;
    }
    if (verbose) {
        fprintf(sg_warnings_str, "    mode sense (10) cdb: ");
        for (k = 0; k < MODE_SENSE10_CMDLEN; ++k)
            fprintf(sg_warnings_str, "%02x ", modesCmdBlk[k]);
        fprintf(sg_warnings_str, "\n");
    }

    memset(&io_hdr, 0, sizeof(struct sg_io_hdr));
    memset(sense_b, 0, sizeof(sense_b));
    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = MODE_SENSE10_CMDLEN;
    io_hdr.mx_sb_len = sizeof(sense_b);
    io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
    io_hdr.dxfer_len = mx_resp_len;
    io_hdr.dxferp = resp;
    io_hdr.cmdp = modesCmdBlk;
    io_hdr.sbp = sense_b;
    io_hdr.timeout = DEF_TIMEOUT;

    if (ioctl(sg_fd, SG_IO, &io_hdr) < 0) {
        if (noisy || verbose)
            fprintf(sg_warnings_str, "mode sense (10) SG_IO error: %s\n",
                    safe_strerror(errno));
        return -1;
    }
    if (verbose > 2)
        fprintf(sg_warnings_str, "      duration=%u ms\n", io_hdr.duration);
    res = sg_err_category3(&io_hdr);
    switch (res) {
    case SG_LIB_CAT_RECOVERED:
        if (noisy || verbose)
            sg_chk_n_print3("Mode sense (10)", &io_hdr);
        /* fall through */
    case SG_LIB_CAT_CLEAN:
        if (verbose && io_hdr.resid)
            fprintf(sg_warnings_str, "    mode sense (10): resid=%d\n",
                    io_hdr.resid);
        if (verbose > 2) {
            k = mx_resp_len - io_hdr.resid;
            if (k > 0) {
                fprintf(sg_warnings_str, "    mode sense (10): response%s\n",
                        (k > 256 ? ", first 256 bytes" : ""));
                dStrHex(resp, (k > 256 ? 256 : k), -1);
            }
        }
        return 0;
    case SG_LIB_CAT_INVALID_OP:
    case SG_LIB_CAT_ILLEGAL_REQ:
        if (verbose > 1)
            sg_chk_n_print3("Mode sense (10) error", &io_hdr);
        return res;
    default:
        if (noisy || verbose) {
            char ebuff[EBUFF_SZ];

            snprintf(ebuff, EBUFF_SZ, "Mode sense (10) error, dbd=%d "
                    "pc=%d page_code=%x sub_page_code=%x\n     ", dbd,
                    pc, pg_code, sub_pg_code);
            sg_chk_n_print3(ebuff, &io_hdr);
        }
        return -1;
    }
}

/* Invokes a SCSI MODE SELECT (6) command.  Return of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> invalid opcode, SG_LIB_CAT_ILLEGAL_REQ ->
 * bad field in cdb, -1 -> other failure */
int sg_ll_mode_select6(int sg_fd, int pf, int sp, void * paramp,
                       int param_len, int noisy, int verbose)
{
    int res, k;
    unsigned char modesCmdBlk[MODE_SELECT6_CMDLEN] = 
        {MODE_SELECT6_CMD, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_io_hdr io_hdr;

    modesCmdBlk[1] = (unsigned char)(((pf << 4) & 0x10) | (sp & 0x1));
    modesCmdBlk[4] = (unsigned char)(param_len & 0xff);
    if (NULL == sg_warnings_str)
        sg_warnings_str = stderr;
    if (param_len > 0xff) {
        fprintf(sg_warnings_str, "mode select (6): param_len too big\n");
        return -1;
    }
    if (verbose) {
        fprintf(sg_warnings_str, "    mode select (6) cdb: ");
        for (k = 0; k < MODE_SELECT6_CMDLEN; ++k)
            fprintf(sg_warnings_str, "%02x ", modesCmdBlk[k]);
        fprintf(sg_warnings_str, "\n");
    }
    if (verbose > 1) {
        fprintf(sg_warnings_str, "    mode select (6) parameter block\n");
        dStrHex((const char *)paramp, param_len, -1);
    }

    memset(&io_hdr, 0, sizeof(struct sg_io_hdr));
    memset(sense_b, 0, sizeof(sense_b));
    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = MODE_SELECT6_CMDLEN;
    io_hdr.mx_sb_len = sizeof(sense_b);
    io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
    io_hdr.dxfer_len = param_len;
    io_hdr.dxferp = paramp;
    io_hdr.cmdp = modesCmdBlk;
    io_hdr.sbp = sense_b;
    io_hdr.timeout = DEF_TIMEOUT;

    if (ioctl(sg_fd, SG_IO, &io_hdr) < 0) {
        if (noisy || verbose)
            fprintf(sg_warnings_str, "mode select (6) SG_IO error: %s\n",
                    safe_strerror(errno));
        return -1;
    }
    if (verbose > 2)
        fprintf(sg_warnings_str, "      duration=%u ms\n", io_hdr.duration);
    res = sg_err_category3(&io_hdr);
    switch (res) {
    case SG_LIB_CAT_RECOVERED:
        if (noisy || verbose)
            sg_chk_n_print3("Mode select (6)", &io_hdr);
        /* fall through */
    case SG_LIB_CAT_CLEAN:
        return 0;
    case SG_LIB_CAT_INVALID_OP:
    case SG_LIB_CAT_ILLEGAL_REQ:
        if (verbose > 1)
            sg_chk_n_print3("Mode select (6) error", &io_hdr);
        return res;
    default:
        if (noisy || verbose) {
            char ebuff[EBUFF_SZ];

            snprintf(ebuff, EBUFF_SZ, "Mode select (6) error, pf=%d "
                    "sp=%d\n     ", pf, sp);
            sg_chk_n_print3(ebuff, &io_hdr);
        }
        return -1;
    }
}

/* Invokes a SCSI MODE SELECT (10) command.  Return of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> invalid opcode, SG_LIB_CAT_ILLEGAL_REQ ->
 * bad field in cdb, -1 -> other failure */
int sg_ll_mode_select10(int sg_fd, int pf, int sp, void * paramp,
                       int param_len, int noisy, int verbose)
{
    int res, k;
    unsigned char modesCmdBlk[MODE_SELECT10_CMDLEN] = 
        {MODE_SELECT10_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_io_hdr io_hdr;

    modesCmdBlk[1] = (unsigned char)(((pf << 4) & 0x10) | (sp & 0x1));
    modesCmdBlk[7] = (unsigned char)((param_len >> 8) & 0xff);
    modesCmdBlk[8] = (unsigned char)(param_len & 0xff);
    if (NULL == sg_warnings_str)
        sg_warnings_str = stderr;
    if (param_len > 0xffff) {
        fprintf(sg_warnings_str, "mode select (10): param_len too big\n");
        return -1;
    }
    if (verbose) {
        fprintf(sg_warnings_str, "    mode select (10) cdb: ");
        for (k = 0; k < MODE_SELECT10_CMDLEN; ++k)
            fprintf(sg_warnings_str, "%02x ", modesCmdBlk[k]);
        fprintf(sg_warnings_str, "\n");
    }
    if (verbose > 1) {
        fprintf(sg_warnings_str, "    mode select (10) parameter block\n");
        dStrHex((const char *)paramp, param_len, -1);
    }

    memset(&io_hdr, 0, sizeof(struct sg_io_hdr));
    memset(sense_b, 0, sizeof(sense_b));
    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = MODE_SELECT10_CMDLEN;
    io_hdr.mx_sb_len = sizeof(sense_b);
    io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
    io_hdr.dxfer_len = param_len;
    io_hdr.dxferp = paramp;
    io_hdr.cmdp = modesCmdBlk;
    io_hdr.sbp = sense_b;
    io_hdr.timeout = DEF_TIMEOUT;

    if (ioctl(sg_fd, SG_IO, &io_hdr) < 0) {
        if (noisy || verbose)
            fprintf(sg_warnings_str, "mode select (10) SG_IO error: %s\n",
                    safe_strerror(errno));
        return -1;
    }
    if (verbose > 2)
        fprintf(sg_warnings_str, "      duration=%u ms\n", io_hdr.duration);
    res = sg_err_category3(&io_hdr);
    switch (res) {
    case SG_LIB_CAT_RECOVERED:
        if (noisy || verbose)
            sg_chk_n_print3("Mode select (10)", &io_hdr);
        /* fall through */
    case SG_LIB_CAT_CLEAN:
        return 0;
    case SG_LIB_CAT_INVALID_OP:
    case SG_LIB_CAT_ILLEGAL_REQ:
        if (verbose > 1)
            sg_chk_n_print3("Mode select (10) error", &io_hdr);
        return res;
    default:
        if (noisy || verbose) {
            char ebuff[EBUFF_SZ];

            snprintf(ebuff, EBUFF_SZ, "Mode select (10) error, pf=%d "
                    "sp=%d\n     ", pf, sp);
            sg_chk_n_print3(ebuff, &io_hdr);
        }
        return -1;
    }
}

/* MODE SENSE commands yield a response that has block descriptors followed
 * by mode pages. In most cases users are interested in the first mode page.
 * This function returns the (byte) offset of the start of the first mode
 * page. Set mode_sense_6 to 1 for MODE SENSE (6) and 0 for MODE SENSE (10).
 * Returns >= 0 is successful or -1 if failure. If there is a failure
 * a message is written to err_buff. */
int sg_mode_page_offset(const unsigned char * resp, int resp_len,
                        int mode_sense_6, char * err_buff, int err_buff_len)
{
    int bd_len;
    int calc_len;
    int offset;

    if ((NULL == resp) || (resp_len < 4) ||
        ((! mode_sense_6) && (resp_len < 8))) {
        snprintf(err_buff, err_buff_len, "given response length too short: "
                 "%d\n", resp_len);
        return -1;
    }
    if (mode_sense_6) {
        calc_len = resp[0] + 1;
        bd_len = resp[3];
        offset = bd_len + MODE6_RESP_HDR_LEN;
    } else {
        calc_len = (resp[0] << 8) + resp[1] + 2;
        bd_len = (resp[6] << 8) + resp[7];
        /* LongLBA doesn't change this calculation */
        offset = bd_len + MODE10_RESP_HDR_LEN;
    }
    if ((offset + 2) > resp_len) {
        snprintf(err_buff, err_buff_len, "given response length "
                 "too small, offset=%d given_len=%d bd_len=%d\n",
                 offset, resp_len, bd_len);
         offset = -1;
    } else if ((offset + 2) > calc_len) {
        snprintf(err_buff, err_buff_len, "calculated response "
                 "length too small, offset=%d calc_len=%d bd_len=%d\n",
                 offset, calc_len, bd_len);
        offset = -1;
    }
    return offset;
}

/* Fetches current, changeable, default and/or saveable modes pages as
 * indicated by pcontrol_arr for given pg_code and sub_pg_code. If
 * mode6==0 then use MODE SENSE (10) else use MODE SENSE (6). If
 * flexible set and mode data length seems wrong then try and 
 * fix (compensating hack for bad device or driver). pcontrol_arr
 * should have 4 elements for output of current, changeable, default
 * and saved values respectively. Each element should be NULL or
 * at least mx_mpage_len bytes long.
 * Return of 0 -> overall success, SG_LIB_CAT_INVALID_OP -> invalid opcode,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb, -1 -> other failure.
 * If success_mask pointer is not NULL then zeroes it then sets bit 0, 1,
 * 2 and/or 3 if the current, changeable, default and saved values
 * respectively have been fetched. If error on current page
 * then stops and returns that error; otherwise continues if an error is
 * detected but returns the first error encountered.  */
int sg_get_mode_page_controls(int sg_fd, int mode6, int pg_code,
                              int sub_pg_code, int flexible, int mx_mpage_len,
                              int * success_mask, void * pcontrol_arr[],
                              int * reported_len, int verbose)
{
    int k, n, res, offset, calc_len, xfer_len, resp_mode6;
    unsigned char buff[MODE_RESP_ARB_LEN];
    char ebuff[EBUFF_SZ];
    int first_err = 0;

    if (success_mask)
        *success_mask = 0;
    if (reported_len)
        *reported_len = 0;
    if (mx_mpage_len < 4)
        return 0;
    if (NULL == sg_warnings_str)
        sg_warnings_str = stderr;
    memset(ebuff, 0, sizeof(ebuff));
    /* first try to find length of current page response */
    memset(buff, 0, MODE10_RESP_HDR_LEN);
    if (mode6)  /* want first 8 bytes just in case */
        res = sg_ll_mode_sense6(sg_fd, 0 /* dbd */, 0 /* pc */, pg_code,
                                sub_pg_code, buff, MODE10_RESP_HDR_LEN, 0,
                                verbose);
    else
        res = sg_ll_mode_sense10(sg_fd, 0 /* llbaa */, 0 /* dbd */,
                                 0 /* pc */, pg_code, sub_pg_code, buff,
                                 MODE10_RESP_HDR_LEN, 0, verbose);
    if (0 != res)
        return res;
    n = buff[0];
    if (reported_len)
        *reported_len = mode6 ? (n + 1) : ((n << 8) + buff[1] + 2);
    resp_mode6 = mode6;
    if (flexible) {
        if (mode6 && (n < 3)) {
            resp_mode6 = 0;
            if (verbose)
                fprintf(sg_warnings_str, ">>> msense(6) but resp[0]=%d so "
                        "try msense(10) response processing\n", n);
        }
        if ((0 == mode6) && (n > 5)) {
            if ((n > 11) && (0 == (n % 2)) && (0 == buff[4]) &&
                (0 == buff[5]) && (0 == buff[6])) {
                buff[1] = n;
                buff[0] = 0;
                if (verbose)
                    fprintf(sg_warnings_str, ">>> msense(10) but resp[0]=%d "
                            "and not msense(6) response so fix length\n", n);
            } else
                resp_mode6 = 1;
        }
    }
    if (verbose && (resp_mode6 != mode6))
        fprintf(sg_warnings_str, ">>> msense(%d) but resp[0]=%d "
                "so switch response processing\n", (mode6 ? 6 : 10),
                buff[0]);
    calc_len = resp_mode6 ? (buff[0] + 1) : ((buff[0] << 8) + buff[1] + 2);
    if (calc_len > MODE_RESP_ARB_LEN)
        calc_len = MODE_RESP_ARB_LEN;
    offset = sg_mode_page_offset(buff, calc_len, resp_mode6,
                                 ebuff, EBUFF_SZ);
    if (offset < 0) {
        if (('\0' != ebuff[0]) && (verbose > 0))
            fprintf(sg_warnings_str, "sg_get_mode_page_types: "
                    "current values: %s\n", ebuff);
        return offset;
    }
    xfer_len = calc_len - offset;
    if (xfer_len > mx_mpage_len)
        xfer_len = mx_mpage_len;

    for (k = 0; k < 4; ++k) {
        if (NULL == pcontrol_arr[k])
            continue;
        memset(pcontrol_arr[k], 0, mx_mpage_len);
        if (mode6)
            res = sg_ll_mode_sense6(sg_fd, 0 /* dbd */, k /* pc */,
                                    pg_code, sub_pg_code, buff,
                                    calc_len, 0, verbose);
        else
            res = sg_ll_mode_sense10(sg_fd, 0 /* llbaa */, 0 /* dbd */,
                                     k /* pc */, pg_code, sub_pg_code,
                                     buff, calc_len, 0, verbose);
        if (0 != res) {
            if (0 == first_err)
                first_err = res;
            if (0 == k)
                break;  /* if problem on current page, it won't improve */
            else
                continue;
        }
        if (xfer_len > 0)
            memcpy(pcontrol_arr[k], buff + offset, xfer_len);
        if (success_mask)
            *success_mask |= (1 << k);
    }
    return first_err;
}

/* Invokes a SCSI REQUEST SENSE command. Return of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> Request Sense not * supported??,
 * SG_LIB_CAT_ILEGAL_REQ -> bad field in cdb, -1 -> other failure */
int sg_ll_request_sense(int sg_fd, int desc, void * resp, int mx_resp_len,
                        int verbose)
{
    int k, res;
    unsigned char rsCmdBlk[REQUEST_SENSE_CMDLEN] = 
        {REQUEST_SENSE_CMD, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_io_hdr io_hdr;

    if (desc)
        rsCmdBlk[1] |= 0x1;
    if (NULL == sg_warnings_str)
        sg_warnings_str = stderr;
    if (mx_resp_len > 0xfc) {
        fprintf(sg_warnings_str, "SPC-3 says request sense allocation "
                "length should be <= 252\n");
        return -1;
    }
    rsCmdBlk[4] = mx_resp_len & 0xff;
    if (verbose) {
        fprintf(sg_warnings_str, "    Request Sense cmd: ");
        for (k = 0; k < REQUEST_SENSE_CMDLEN; ++k)
            fprintf(sg_warnings_str, "%02x ", rsCmdBlk[k]);
        fprintf(sg_warnings_str, "\n");
    }

    memset(&io_hdr, 0, sizeof(struct sg_io_hdr));
    memset(sense_b, 0, sizeof(sense_b));
    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = REQUEST_SENSE_CMDLEN;
    io_hdr.mx_sb_len = sizeof(sense_b);
    io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
    io_hdr.dxfer_len = mx_resp_len;
    io_hdr.dxferp = resp;
    io_hdr.cmdp = rsCmdBlk;
    io_hdr.sbp = sense_b;
    io_hdr.timeout = DEF_TIMEOUT;

    if (ioctl(sg_fd, SG_IO, &io_hdr) < 0) {
        if (verbose)
            fprintf(sg_warnings_str, "request sense SG_IO error: %s\n",
                    safe_strerror(errno));
        return -1;
    }

    if (verbose > 2)
        fprintf(sg_warnings_str, "      duration=%u ms\n", io_hdr.duration);
    /* shouldn't get errors on Request Sense but it is best to be safe */
    res =  sg_err_category3(&io_hdr);
    switch (res) {
    case SG_LIB_CAT_RECOVERED:
        if (verbose)
            sg_chk_n_print3("Request sense", &io_hdr);
        /* fall through */
    case SG_LIB_CAT_CLEAN:
        if ((mx_resp_len >= 8) && (io_hdr.resid > (mx_resp_len - 8))) {
            fprintf(sg_warnings_str, "    request sense: resid=%d "
                    "indicates response too short\n", io_hdr.resid);
            return -1;
        } else if (verbose && io_hdr.resid)
            fprintf(sg_warnings_str, "    request sense: resid=%d\n",
                    io_hdr.resid);
        return 0;
    case SG_LIB_CAT_INVALID_OP:
    case SG_LIB_CAT_ILLEGAL_REQ:
        if (verbose > 1)
            sg_chk_n_print3("REQUEST SENSE command problem", &io_hdr);
        return res;
    default:
        sg_chk_n_print3("REQUEST SENSE command problem", &io_hdr);
        return -1;
    }
}

/* Invokes a SCSI REPORT LUNS command. Return of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> Report Luns not supported,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb, -1 -> other failure */
int sg_ll_report_luns(int sg_fd, int select_report, void * resp,
                      int mx_resp_len, int noisy, int verbose)
{
    int k, res;
    unsigned char rlCmdBlk[REPORT_LUNS_CMDLEN] =
                         {REPORT_LUNS_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_io_hdr io_hdr;

    rlCmdBlk[2] = select_report & 0xff;
    rlCmdBlk[6] = (mx_resp_len >> 24) & 0xff;
    rlCmdBlk[7] = (mx_resp_len >> 16) & 0xff;
    rlCmdBlk[8] = (mx_resp_len >> 8) & 0xff;
    rlCmdBlk[9] = mx_resp_len & 0xff;
    if (NULL == sg_warnings_str)
        sg_warnings_str = stderr;
    if (verbose) {
        fprintf(sg_warnings_str, "    report luns cdb: ");
        for (k = 0; k < REPORT_LUNS_CMDLEN; ++k)
            fprintf(sg_warnings_str, "%02x ", rlCmdBlk[k]);
        fprintf(sg_warnings_str, "\n");
    }
    memset(&io_hdr, 0, sizeof(struct sg_io_hdr));
    memset(sense_b, 0, sizeof(sense_b));
    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = sizeof(rlCmdBlk);
    io_hdr.mx_sb_len = sizeof(sense_b);
    io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
    io_hdr.dxfer_len = mx_resp_len;
    io_hdr.dxferp = resp;
    io_hdr.cmdp = rlCmdBlk;
    io_hdr.sbp = sense_b;
    io_hdr.timeout = DEF_TIMEOUT;

    if (ioctl(sg_fd, SG_IO, &io_hdr) < 0) {
        if (noisy || verbose)
            fprintf(sg_warnings_str, "report_luns (SG_IO) error: %s\n",
                    safe_strerror(errno));
        return -1;
    }
    if (verbose > 2)
        fprintf(sg_warnings_str, "      duration=%u ms\n", io_hdr.duration);
    res = sg_err_category3(&io_hdr);
    switch (res) {
    case SG_LIB_CAT_RECOVERED:
        if (noisy || verbose)
            sg_chk_n_print3("Report luns", &io_hdr);
        /* fall through */
    case SG_LIB_CAT_CLEAN:
        if (verbose && io_hdr.resid)
            fprintf(sg_warnings_str, "    report_luns: resid=%d\n",
                    io_hdr.resid);
        return 0;
    case SG_LIB_CAT_INVALID_OP:
    case SG_LIB_CAT_ILLEGAL_REQ:
        if (verbose > 1)
            sg_chk_n_print3("REPORTS LUNS command error", &io_hdr);
        return res;
    case SG_LIB_CAT_MEDIA_CHANGED:
        return 2;
    default:
        if (noisy || verbose)
            sg_chk_n_print3("REPORT LUNS command error", &io_hdr);
        return -1;
    }
}

/* Invokes a SCSI LOG SENSE command. Return of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> Log Sense not supported,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb, -1 -> other failure */
int sg_ll_log_sense(int sg_fd, int ppc, int sp, int pc, int pg_code, 
                    int paramp, unsigned char * resp, int mx_resp_len, 
                    int noisy, int verbose)
{
    int res, k;
    unsigned char logsCmdBlk[LOG_SENSE_CMDLEN] = 
        {LOG_SENSE_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_io_hdr io_hdr;

    if (NULL == sg_warnings_str)
        sg_warnings_str = stderr;
    if (mx_resp_len > 0xffff) {
        fprintf(sg_warnings_str, "mx_resp_len too big\n");
        return -1;
    }
    logsCmdBlk[1] = (unsigned char)((ppc ? 2 : 0) | (sp ? 1 : 0));
    logsCmdBlk[2] = (unsigned char)(((pc << 6) & 0xc0) | (pg_code & 0x3f));
    logsCmdBlk[5] = (unsigned char)((paramp >> 8) & 0xff);
    logsCmdBlk[6] = (unsigned char)(paramp & 0xff);
    logsCmdBlk[7] = (unsigned char)((mx_resp_len >> 8) & 0xff);
    logsCmdBlk[8] = (unsigned char)(mx_resp_len & 0xff);
    if (verbose) {
        fprintf(sg_warnings_str, "    log sense cdb: ");
        for (k = 0; k < LOG_SENSE_CMDLEN; ++k)
            fprintf(sg_warnings_str, "%02x ", logsCmdBlk[k]);
        fprintf(sg_warnings_str, "\n");
    }

    memset(&io_hdr, 0, sizeof(struct sg_io_hdr));
    memset(sense_b, 0, sizeof(sense_b));
    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = sizeof(logsCmdBlk);
    io_hdr.mx_sb_len = sizeof(sense_b);
    io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
    io_hdr.dxfer_len = mx_resp_len;
    io_hdr.dxferp = resp;
    io_hdr.cmdp = logsCmdBlk;
    io_hdr.sbp = sense_b;
    io_hdr.timeout = DEF_TIMEOUT;

    if (ioctl(sg_fd, SG_IO, &io_hdr) < 0) {
        if (noisy || verbose)
            fprintf(sg_warnings_str, "log sense (SG_IO) error: %s\n",
                    safe_strerror(errno));
        return -1;
    }
    if (verbose > 2)
        fprintf(sg_warnings_str, "      duration=%u ms\n", io_hdr.duration);
    res = sg_err_category3(&io_hdr);
    switch (res) {
    case SG_LIB_CAT_RECOVERED:
        if (noisy || verbose)
            sg_chk_n_print3("Log sense", &io_hdr);
        /* fall through */
    case SG_LIB_CAT_CLEAN:
        if (verbose && io_hdr.resid)
            fprintf(sg_warnings_str, "    log_sense: resid=%d\n",
                    io_hdr.resid);
        return 0;
    case SG_LIB_CAT_INVALID_OP:
    case SG_LIB_CAT_ILLEGAL_REQ:
        if (verbose > 1)
            sg_chk_n_print3("log_sense error", &io_hdr);
        return res;
    default:
        if (noisy || verbose) {
            char ebuff[EBUFF_SZ];
            snprintf(ebuff, EBUFF_SZ, "log_sense: ppc=%d, sp=%d, "
                     "pc=%d, page_code=%x, paramp=%x\n    ", ppc, sp, pc, 
                     pg_code, paramp);
            sg_chk_n_print3(ebuff, &io_hdr);
        }
        return -1;
    }
}


/* Invokes a SCSI LOG SELECT command. Return of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> Log Select not supported,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb, -1 -> other failure */
int sg_ll_log_select(int sg_fd, int pcr, int sp, int pc,
                     unsigned char * paramp, int param_len, 
                     int noisy, int verbose)
{
    int res, k;
    unsigned char logsCmdBlk[LOG_SELECT_CMDLEN] = 
        {LOG_SELECT_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_io_hdr io_hdr;

    if (NULL == sg_warnings_str)
        sg_warnings_str = stderr;
    if (param_len > 0xffff) {
        fprintf(sg_warnings_str, "log select: param_len too big\n");
        return -1;
    }
    logsCmdBlk[1] = (unsigned char)((pcr ? 2 : 0) | (sp ? 1 : 0));
    logsCmdBlk[2] = (unsigned char)((pc << 6) & 0xc0);
    logsCmdBlk[7] = (unsigned char)((param_len >> 8) & 0xff);
    logsCmdBlk[8] = (unsigned char)(param_len & 0xff);
    if (verbose) {
        fprintf(sg_warnings_str, "    log select cdb: ");
        for (k = 0; k < LOG_SELECT_CMDLEN; ++k)
            fprintf(sg_warnings_str, "%02x ", logsCmdBlk[k]);
        fprintf(sg_warnings_str, "\n");
    }
    if ((verbose > 1) && (param_len > 0)) {
        fprintf(sg_warnings_str, "    log select parameter block\n");
        dStrHex((const char *)paramp, param_len, -1);
    }

    memset(&io_hdr, 0, sizeof(struct sg_io_hdr));
    memset(sense_b, 0, sizeof(sense_b));
    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = sizeof(logsCmdBlk);
    io_hdr.mx_sb_len = sizeof(sense_b);
    io_hdr.dxfer_direction = param_len ? SG_DXFER_TO_DEV : SG_DXFER_NONE;
    io_hdr.dxfer_len = param_len;
    io_hdr.dxferp = paramp;
    io_hdr.cmdp = logsCmdBlk;
    io_hdr.sbp = sense_b;
    io_hdr.timeout = DEF_TIMEOUT;

    if (ioctl(sg_fd, SG_IO, &io_hdr) < 0) {
        if (noisy || verbose)
            fprintf(sg_warnings_str, "log select (SG_IO) error: %s\n",
                    safe_strerror(errno));
        return -1;
    }
    if (verbose > 2)
        fprintf(sg_warnings_str, "      duration=%u ms\n", io_hdr.duration);
    res = sg_err_category3(&io_hdr);
    switch (res) {
    case SG_LIB_CAT_RECOVERED:
        if (noisy || verbose)
            sg_chk_n_print3("Log select", &io_hdr);
        /* fall through */
    case SG_LIB_CAT_CLEAN:
        if (verbose && io_hdr.resid)
            fprintf(sg_warnings_str, "    log_select: resid=%d\n",
                    io_hdr.resid);
        return 0;
    case SG_LIB_CAT_INVALID_OP:
    case SG_LIB_CAT_ILLEGAL_REQ:
        if (verbose > 1)
            sg_chk_n_print3("log_select error", &io_hdr);
        return res;
    default:
        if (noisy || verbose) {
            char ebuff[EBUFF_SZ];
            snprintf(ebuff, EBUFF_SZ, "log_select: pcr=%d, sp=%d, "
                     "pc=%d\n    ", pcr, sp, pc);
            sg_chk_n_print3(ebuff, &io_hdr);
        }
        return -1;
    }
}

/* Invokes a SCSI REPORT TARGET PORT GROUPS command. Return of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> Report Target Port Groups not supported,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb, -1 -> other failure */
int sg_ll_report_tgt_prt_grp(int sg_fd, void * resp,
                             int mx_resp_len, int noisy, int verbose)
{
    int k, res;
    unsigned char rtpgCmdBlk[MAINTENANCE_IN_CMDLEN] =
                         {MAINTENANCE_IN_CMD, REPORT_TGT_PRT_GRP_SA,
                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_io_hdr io_hdr;

    rtpgCmdBlk[6] = (mx_resp_len >> 24) & 0xff;
    rtpgCmdBlk[7] = (mx_resp_len >> 16) & 0xff;
    rtpgCmdBlk[8] = (mx_resp_len >> 8) & 0xff;
    rtpgCmdBlk[9] = mx_resp_len & 0xff;
    if (NULL == sg_warnings_str)
        sg_warnings_str = stderr;
    if (verbose) {
        fprintf(sg_warnings_str, "    report target port groups cdb: ");
        for (k = 0; k < MAINTENANCE_IN_CMDLEN; ++k)
            fprintf(sg_warnings_str, "%02x ", rtpgCmdBlk[k]);
        fprintf(sg_warnings_str, "\n");
    }
    memset(&io_hdr, 0, sizeof(struct sg_io_hdr));
    memset(sense_b, 0, sizeof(sense_b));
    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = sizeof(rtpgCmdBlk);
    io_hdr.mx_sb_len = sizeof(sense_b);
    io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
    io_hdr.dxfer_len = mx_resp_len;
    io_hdr.dxferp = resp;
    io_hdr.cmdp = rtpgCmdBlk;
    io_hdr.sbp = sense_b;
    io_hdr.timeout = DEF_TIMEOUT;

    if (ioctl(sg_fd, SG_IO, &io_hdr) < 0) {
        if (noisy || verbose)
            fprintf(sg_warnings_str, "report_tgt_prt_grp (SG_IO) error: "
                    "%s\n", safe_strerror(errno));
        return -1;
    }
    if (verbose > 2)
        fprintf(sg_warnings_str, "      duration=%u ms\n", io_hdr.duration);
    res = sg_err_category3(&io_hdr);
    switch (res) {
    case SG_LIB_CAT_RECOVERED:
        if (noisy || verbose)
            sg_chk_n_print3("Report target port groups", &io_hdr);
        /* fall through */
    case SG_LIB_CAT_CLEAN:
        if (verbose && io_hdr.resid)
            fprintf(sg_warnings_str, "    report_tgt_prt_grp: resid=%d\n",
                    io_hdr.resid);
        return 0;
    case SG_LIB_CAT_INVALID_OP:
    case SG_LIB_CAT_ILLEGAL_REQ:
        if (verbose > 1)
            sg_chk_n_print3("REPORT TARGET PORT GROUPS", &io_hdr);
        return res;
    default:
        if (noisy || verbose)
            sg_chk_n_print3("REPORT TARGET PORT GROUPS command error", &io_hdr);
        return -1;
    }
}

/* Invokes a SCSI SEND DIAGNOSTIC command. Foreground, extended self tests can
 * take a long time, if so set long_duration flag. Return of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> Send diagnostic not supported,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb, -1 -> other failure */
int sg_ll_send_diag(int sg_fd, int sf_code, int pf_bit, int sf_bit,
                    int devofl_bit, int unitofl_bit, int long_duration,
                    void * paramp, int param_len, int noisy,
                    int verbose)
{
    int k, res;
    unsigned char senddiagCmdBlk[SEND_DIAGNOSTIC_CMDLEN] = 
        {SEND_DIAGNOSTIC_CMD, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_io_hdr io_hdr;

    senddiagCmdBlk[1] = (unsigned char)((sf_code << 5) | (pf_bit << 4) |
                        (sf_bit << 2) | (devofl_bit << 1) | unitofl_bit);
    senddiagCmdBlk[3] = (unsigned char)((param_len >> 8) & 0xff);
    senddiagCmdBlk[4] = (unsigned char)(param_len & 0xff);

    if (NULL == sg_warnings_str)
        sg_warnings_str = stderr;
    if (verbose) {
        fprintf(sg_warnings_str, "    Send diagnostic cmd: ");
        for (k = 0; k < SEND_DIAGNOSTIC_CMDLEN; ++k)
            fprintf(sg_warnings_str, "%02x ", senddiagCmdBlk[k]);
        fprintf(sg_warnings_str, "\n");
        if ((verbose > 1) && paramp && param_len) {
            fprintf(sg_warnings_str, "    Send diagnostic parameter "
                    "block:\n");
            dStrHex(paramp, param_len, -1);
        }
    }

    memset(&io_hdr, 0, sizeof(struct sg_io_hdr));
    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = SEND_DIAGNOSTIC_CMDLEN;
    io_hdr.mx_sb_len = sizeof(sense_b);
    io_hdr.dxfer_direction = param_len ? SG_DXFER_TO_DEV : SG_DXFER_NONE;
    io_hdr.dxfer_len = param_len;
    io_hdr.dxferp = paramp;
    io_hdr.cmdp = senddiagCmdBlk;
    io_hdr.sbp = sense_b;
    io_hdr.timeout = long_duration ? LONG_TIMEOUT : DEF_TIMEOUT;

    if (ioctl(sg_fd, SG_IO, &io_hdr) < 0) {
        if (noisy || verbose)
            fprintf(sg_warnings_str, "send diagnostic (SG_IO) error: %s\n",
                    safe_strerror(errno));
        return -1;
    }
    if (verbose > 2)
        fprintf(sg_warnings_str, "      duration=%u ms\n", io_hdr.duration);
    res = sg_err_category3(&io_hdr);
    switch (res) {
    case SG_LIB_CAT_RECOVERED:
        if (noisy || verbose)
            sg_chk_n_print3("Send diagnostic, continuing", &io_hdr);
        /* fall through */
    case SG_LIB_CAT_CLEAN:
        return 0;
    case SG_LIB_CAT_INVALID_OP:
    case SG_LIB_CAT_ILLEGAL_REQ:
        if (verbose > 1)
            sg_chk_n_print3("SEND DIAGNOSTIC", &io_hdr);
        return res;
    default:
        if (noisy) {
            char ebuff[EBUFF_SZ];
            snprintf(ebuff, EBUFF_SZ, "Send diagnostic error, sf_code=0x%x, "
                     "pf_bit=%d, sf_bit=%d ", sf_code, pf_bit, sf_bit);
            sg_chk_n_print3(ebuff, &io_hdr);
        }
        return -1;
    }
}

/* Invokes a SCSI RECEIVE DIAGNOSTICS RESULTS command. Return of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> Receive diagnostics results not supported,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb, -1 -> other failure */
int sg_ll_receive_diag(int sg_fd, int pcv, int pg_code, void * resp, 
                       int mx_resp_len, int noisy, int verbose)
{
    int k, res;
    unsigned char rcvdiagCmdBlk[RECEIVE_DIAGNOSTICS_CMDLEN] = 
        {RECEIVE_DIAGNOSTICS_CMD, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_io_hdr io_hdr;

    rcvdiagCmdBlk[1] = (unsigned char)(pcv ? 0x1 : 0);
    rcvdiagCmdBlk[2] = (unsigned char)(pg_code);
    rcvdiagCmdBlk[3] = (unsigned char)((mx_resp_len >> 8) & 0xff);
    rcvdiagCmdBlk[4] = (unsigned char)(mx_resp_len & 0xff);

    if (NULL == sg_warnings_str)
        sg_warnings_str = stderr;
    if (verbose) {
        fprintf(sg_warnings_str, "    Receive diagnostics results cmd: ");
        for (k = 0; k < RECEIVE_DIAGNOSTICS_CMDLEN; ++k)
            fprintf(sg_warnings_str, "%02x ", rcvdiagCmdBlk[k]);
        fprintf(sg_warnings_str, "\n");
    }

    memset(&io_hdr, 0, sizeof(struct sg_io_hdr));
    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = RECEIVE_DIAGNOSTICS_CMDLEN;
    io_hdr.mx_sb_len = sizeof(sense_b);
    io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
    io_hdr.dxfer_len = mx_resp_len;
    io_hdr.dxferp = resp;
    io_hdr.cmdp = rcvdiagCmdBlk;
    io_hdr.sbp = sense_b;
    io_hdr.timeout = DEF_TIMEOUT;

    if (ioctl(sg_fd, SG_IO, &io_hdr) < 0) {
        if (noisy || verbose)
            fprintf(sg_warnings_str, "receive diagnostics results (SG_IO) "
                    "error: %s\n", safe_strerror(errno));
        return -1;
    }
    if (verbose > 2)
        fprintf(sg_warnings_str, "      duration=%u ms\n", io_hdr.duration);
    res = sg_err_category3(&io_hdr);
    switch (res) {
    case SG_LIB_CAT_RECOVERED:
        if (noisy || verbose)
            sg_chk_n_print3("Receive diagnostics results, continuing",
                            &io_hdr);
        /* fall through */
    case SG_LIB_CAT_CLEAN:
        return 0;
    case SG_LIB_CAT_INVALID_OP:
    case SG_LIB_CAT_ILLEGAL_REQ:
        if (verbose > 1)
            sg_chk_n_print3("RECEIVE DIAGNOSTICS RESULTS", &io_hdr);
        return res;
    default:
        if (noisy) {
            char ebuff[EBUFF_SZ];
            snprintf(ebuff, EBUFF_SZ, "Receive diagnostics results error, "
                     "pcv=%d, page_code=%x ", pcv, pg_code);
            sg_chk_n_print3(ebuff, &io_hdr);
        }
        return -1;
    }
}

/* Invokes a SCSI READ DEFECT DATA (10) command (SBC). Return of 0 ->
 * success, SG_LIB_CAT_INVALID_OP -> invalid opcode,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb, -1 -> other failure */
int sg_ll_read_defect10(int sg_fd, int req_plist, int req_glist,
                        int dl_format, void * resp, int mx_resp_len,
                        int noisy, int verbose)
{
    int res, k;
    unsigned char rdefCmdBlk[READ_DEFECT10_CMDLEN] = 
        {READ_DEFECT10_CMD, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_io_hdr io_hdr;

    rdefCmdBlk[2] = (unsigned char)(((req_plist << 4) & 0x10) |
                         ((req_glist << 3) & 0x8) | (dl_format & 0x7));
    rdefCmdBlk[7] = (unsigned char)((mx_resp_len >> 8) & 0xff);
    rdefCmdBlk[8] = (unsigned char)(mx_resp_len & 0xff);
    if (NULL == sg_warnings_str)
        sg_warnings_str = stderr;
    if (mx_resp_len > 0xffff) {
        fprintf(sg_warnings_str, "mx_resp_len too big\n");
        return -1;
    }
    if (verbose) {
        fprintf(sg_warnings_str, "    read defect (10) cdb: ");
        for (k = 0; k < READ_DEFECT10_CMDLEN; ++k)
            fprintf(sg_warnings_str, "%02x ", rdefCmdBlk[k]);
        fprintf(sg_warnings_str, "\n");
    }

    memset(&io_hdr, 0, sizeof(struct sg_io_hdr));
    memset(sense_b, 0, sizeof(sense_b));
    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = READ_DEFECT10_CMDLEN;
    io_hdr.mx_sb_len = sizeof(sense_b);
    io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
    io_hdr.dxfer_len = mx_resp_len;
    io_hdr.dxferp = resp;
    io_hdr.cmdp = rdefCmdBlk;
    io_hdr.sbp = sense_b;
    io_hdr.timeout = DEF_TIMEOUT;

    if (ioctl(sg_fd, SG_IO, &io_hdr) < 0) {
        if (noisy || verbose)
            fprintf(sg_warnings_str, "read defect (10) SG_IO error: %s\n",
                    safe_strerror(errno));
        return -1;
    }
    if (verbose > 2)
        fprintf(sg_warnings_str, "      duration=%u ms\n", io_hdr.duration);
    res = sg_err_category3(&io_hdr);
    switch (res) {
    case SG_LIB_CAT_RECOVERED:
        if (noisy || verbose)
            sg_chk_n_print3("Read defect (10)", &io_hdr);
        /* fall through */
    case SG_LIB_CAT_CLEAN:
        if (verbose && io_hdr.resid)
            fprintf(sg_warnings_str, "    read defect (10): resid=%d\n",
                    io_hdr.resid);
        if (verbose > 2) {
            k = mx_resp_len - io_hdr.resid;
            if (k > 0) {
                fprintf(sg_warnings_str, "    read defect (10): response%s\n",
                        (k > 256 ? ", first 256 bytes" : ""));
                dStrHex(resp, (k > 256 ? 256 : k), -1);
            }
        }
        return 0;
    case SG_LIB_CAT_INVALID_OP:
    case SG_LIB_CAT_ILLEGAL_REQ:
        if (verbose > 1)
            sg_chk_n_print3("Read defect (10) error", &io_hdr);
        return res;
    default:
        if (noisy || verbose) {
            char ebuff[EBUFF_SZ];

            snprintf(ebuff, EBUFF_SZ, "Read defect (10) error, req_plist=%d "
                    "req_glist=%d dl_format=%x\n     ", req_plist, req_glist,
                    dl_format);
            sg_chk_n_print3(ebuff, &io_hdr);
        }
        return -1;
    }
}

/* Invokes a SCSI READ MEDIA SERIAL NUMBER command. Return of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> Read media serial number not supported,
 * SG_LIB_CAT_ILLEGAL_REQ -> bad field in cdb, -1 -> other failure */
int sg_ll_read_media_serial_num(int sg_fd, void * resp,
                                int mx_resp_len, int noisy, int verbose)
{
    int k, res;
    unsigned char rmsnCmdBlk[SERVICE_ACTION_IN_12_CMDLEN] =
                         {SERVICE_ACTION_IN_12_CMD, READ_MEDIA_SERIAL_NUM_SA,
                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    unsigned char sense_b[SENSE_BUFF_LEN];
    struct sg_io_hdr io_hdr;

    rmsnCmdBlk[6] = (mx_resp_len >> 24) & 0xff;
    rmsnCmdBlk[7] = (mx_resp_len >> 16) & 0xff;
    rmsnCmdBlk[8] = (mx_resp_len >> 8) & 0xff;
    rmsnCmdBlk[9] = mx_resp_len & 0xff;
    if (NULL == sg_warnings_str)
        sg_warnings_str = stderr;
    if (verbose) {
        fprintf(sg_warnings_str, "    read media serial number cdb: ");
        for (k = 0; k < SERVICE_ACTION_IN_12_CMDLEN; ++k)
            fprintf(sg_warnings_str, "%02x ", rmsnCmdBlk[k]);
        fprintf(sg_warnings_str, "\n");
    }
    memset(&io_hdr, 0, sizeof(struct sg_io_hdr));
    memset(sense_b, 0, sizeof(sense_b));
    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = sizeof(rmsnCmdBlk);
    io_hdr.mx_sb_len = sizeof(sense_b);
    io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
    io_hdr.dxfer_len = mx_resp_len;
    io_hdr.dxferp = resp;
    io_hdr.cmdp = rmsnCmdBlk;
    io_hdr.sbp = sense_b;
    io_hdr.timeout = DEF_TIMEOUT;

    if (ioctl(sg_fd, SG_IO, &io_hdr) < 0) {
        if (noisy || verbose)
            fprintf(sg_warnings_str, "read_media_serial_num (SG_IO) error:"
                    " %s\n", safe_strerror(errno));
        return -1;
    }
    if (verbose > 2)
        fprintf(sg_warnings_str, "      duration=%u ms\n", io_hdr.duration);
    res = sg_err_category3(&io_hdr);
    switch (res) {
    case SG_LIB_CAT_RECOVERED:
        if (noisy || verbose)
            sg_chk_n_print3("Read media serial number", &io_hdr);
        /* fall through */
    case SG_LIB_CAT_CLEAN:
        if (verbose && io_hdr.resid)
            fprintf(sg_warnings_str, "    read_media_serial_num: resid=%d\n",
                    io_hdr.resid);
        return 0;
    case SG_LIB_CAT_INVALID_OP:
    case SG_LIB_CAT_ILLEGAL_REQ:
        if (verbose > 1)
            sg_chk_n_print3("READ MEDIA SERIAL NUMBER", &io_hdr);
        return res;
    default:
        if (noisy || verbose)
            sg_chk_n_print3("READ MEDIA SERIAL NUMBER command error",
                            &io_hdr);
        return -1;
    }
}

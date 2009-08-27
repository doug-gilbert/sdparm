/*
 * Copyright (c) 2005-2008 Douglas Gilbert.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>

#include "sdparm.h"
#include "sg_lib.h"
#include "sg_cmds_basic.h"
#include "sg_cmds_mmc.h"

/* sdparm_cmd.c : contains code to implement commands
 * (i.e "--command=<cmd>") in sdparm.
 */


#define RCAP_REPLY_LEN 8
#define RCAP16_REPLY_LEN 32

static int
do_cmd_read_capacity(int sg_fd, int verbose)
{
    int res, k, do16;
    unsigned int last_blk_addr, block_size;
    unsigned char resp_buff[RCAP16_REPLY_LEN];
    uint64_t llast_blk_addr;
    double sz_mib;

    do16 = 0;
    res = sg_ll_readcap_10(sg_fd, 0 /* pmi */, 0 /* lba */, resp_buff,
                           RCAP_REPLY_LEN, 1, verbose);
    if (0 == res) {
        last_blk_addr = (((unsigned int)resp_buff[0] << 24) |
                         (resp_buff[1] << 16) | (resp_buff[2] << 8) |
                         resp_buff[3]);
        if (0xffffffff != last_blk_addr) {
            block_size = (((unsigned int)resp_buff[4] << 24) |
                          (resp_buff[5] << 16) | (resp_buff[6] << 8) |
                          resp_buff[7]);
            printf("blocks: %u\n", last_blk_addr + 1);
            printf("block_length: %u\n", block_size);
            sz_mib = ((double)(last_blk_addr + 1) * block_size) /
                      (double)(1048576);
#ifdef SG3_UTILS_MINGW
            printf("capacity_mib: %g\n", sz_mib);
#else
            printf("capacity_mib: %.1f\n", sz_mib);
#endif
        } else
            do16 = 1;
    } else
        return res;
    if (do16) {
        /* within SERVICE ACTION IN. May need RW or root permissions. */
        res = sg_ll_readcap_16(sg_fd, 0 /* pmi */, 0 /* llba */, resp_buff,
                               RCAP16_REPLY_LEN, 1, verbose);
        if (0 == res) {
            for (k = 0, llast_blk_addr = 0; k < 8; ++k) {
                llast_blk_addr <<= 8;
                llast_blk_addr |= resp_buff[k];
            }
            block_size = (((unsigned int)resp_buff[8] << 24) |
                          (resp_buff[9] << 16) | (resp_buff[10] << 8) |
                          resp_buff[11]);
            printf("blocks: %" PRIu64 "\n", llast_blk_addr + 1);
            printf("block_length: %u\n", block_size);
            sz_mib = ((double)(llast_blk_addr + 1) * block_size) /
                      (double)(1048576);
#ifdef SG3_UTILS_MINGW
            printf("capacity_mib: %g\n", sz_mib);
#else
            printf("capacity_mib: %.1f\n", sz_mib);
#endif
        } else
            return res;
    }
    return 0;
}

static int
do_cmd_sense(int sg_fd, int hex, int quiet, int verbose)
{
    int res, resp_len, sk, asc, ascq, progress, something;
    unsigned char buff[32];
    char b[128];

    memset(buff, 0, sizeof(buff));
    res = sg_ll_request_sense(sg_fd, 0 /* fixed format */,
                              buff, sizeof(buff), 1, verbose);
    if (0 == res) {
        resp_len = buff[7] + 8;
        if (resp_len > (int)sizeof(buff))
            resp_len = sizeof(buff);
        sk = (0xf & buff[2]);
        if (hex) {
            dStrHex((const char *)buff, resp_len, 1);
            return 0;
        }
        something = 0;
        if (verbose) {
            fprintf(stderr, "Decode response as sense data:\n");
            sg_print_sense(NULL, buff, resp_len, 0);
            if (verbose > 1) {
                fprintf(stderr, "\nOutput response in hex\n");
                dStrHex((const char *)buff, resp_len, 1);
            }
            something = 1;
        }
        asc = (resp_len > 12) ? buff[12] : 0;
        ascq = (resp_len > 13) ? buff[13] : 0;
        if (sg_get_sense_progress_fld(buff, resp_len, &progress)) {
            printf("Operation in progress, %d%% done\n",
                   progress * 100 / 65536);
            something = 1;
        }
        if (0 == sk) {  /* NO SENSE */
            /* check for hardware threshold exceeded or warning */
            if ((0xb == asc) || (0x5d == asc))
                printf("%s\n", sg_get_asc_ascq_str(asc, ascq,
                                                   (int)sizeof(b), b));
            /* check for low power conditions */
            if (0x5e == asc)
                printf("%s\n", sg_get_asc_ascq_str(asc, ascq,
                                                   (int)sizeof(b), b));
            return 0;
        } else {
            if (! (something || verbose || quiet)) {
                fprintf(stderr, "Decode response as sense data:\n");
                sg_print_sense(NULL, buff, resp_len, 0);
            }
            return 0;
        }
    } else if (SG_LIB_CAT_INVALID_OP == res)
        fprintf(stderr, "Request Sense command not supported\n");
    else if (SG_LIB_CAT_ILLEGAL_REQ == res)
        fprintf(stderr, "bad field in Request Sense cdb\n");
    else if (SG_LIB_CAT_NOT_READY == res)
        fprintf(stderr, "Request Sense failed, device not ready\n");
    else if (SG_LIB_CAT_ABORTED_COMMAND == res)
        fprintf(stderr, "Request Sense failed, aborted command\n");
    else {
        fprintf(stderr, "Request Sense command failed\n");
        if (0 == verbose)
            fprintf(stderr, "    try the '-v' option for "
                    "more information\n");
    }
    return res;
}

/* cmd_arg is kBytes/sec if given (i.e. 1000 bytes per second */
static int
do_cmd_speed(int sg_fd, int cmd_arg, const struct sdparm_opt_coll * opts)
{
    int res;
    unsigned int u;
    unsigned int last_lba = 0xfffffffe;
    unsigned int rw_time = 1000;

    if (cmd_arg >= 0) {
        unsigned char perf_desc[28];
#if 0
        int kbps;

        if (0 == cmd_arg)
            kbps = 0xffff;
        else
            kbps = cmd_arg;
        res = sg_ll_set_cd_speed(sg_fd, 0 /* rot_control */, kbps,
                                 0 /* drv_write_speed */, 1, opts->verbose);
#else
        memset(perf_desc, 0, sizeof(perf_desc));
        if (0 == cmd_arg)
            perf_desc[0] |= 0x4;  /* set RDD bit: restore drive defaults */
        else {
            perf_desc[8] = (last_lba >> 24) & 0xff;
            perf_desc[9] = (last_lba >> 16) & 0xff;
            perf_desc[10] = (last_lba >> 8) & 0xff;
            perf_desc[11] = last_lba & 0xff;
            perf_desc[12] = (cmd_arg >> 24) & 0xff;
            perf_desc[13] = (cmd_arg >> 16) & 0xff;
            perf_desc[14] = (cmd_arg >> 8) & 0xff;
            perf_desc[15] = cmd_arg & 0xff;
            perf_desc[16] = (rw_time >> 24) & 0xff;
            perf_desc[17] = (rw_time >> 16) & 0xff;
            perf_desc[18] = (rw_time >> 8) & 0xff;
            perf_desc[19] = rw_time & 0xff;
            perf_desc[20] = (cmd_arg >> 24) & 0xff;
            perf_desc[21] = (cmd_arg >> 16) & 0xff;
            perf_desc[22] = (cmd_arg >> 8) & 0xff;
            perf_desc[23] = cmd_arg & 0xff;
            perf_desc[24] = (rw_time >> 24) & 0xff;
            perf_desc[25] = (rw_time >> 16) & 0xff;
            perf_desc[26] = (rw_time >> 8) & 0xff;
            perf_desc[27] = rw_time & 0xff;
        }
        /* performance (type=0), tolerance 10% nominal, read speed */
        res = sg_ll_set_streaming(sg_fd, 0x0 /* type */, perf_desc,
                                  sizeof(perf_desc), 1, opts->verbose);
        if (res) {
            if (SG_LIB_CAT_NOT_READY == res)
                fprintf(stderr, "Set Streaming failed, device not ready\n");
            else
                fprintf(stderr, "Set Streaming failed, add '-v' for more "
                        "information\n");
        }
    } else {
        const int max_num_desc = 16;
        unsigned char buff[8 + (16 * 16)];
        unsigned int lba;

        /* performance (type=0), tolerance 10% nominal, read speed */
        res = sg_ll_get_performance(sg_fd, 0x10 /* data_type */,
                                    0 /* starting_lba */,
                                    max_num_desc,
                                    0 /* type */, buff, sizeof(buff),
                                    1, opts->verbose);
        if (0 == res) {
            if (opts->verbose) {
                lba = (((unsigned int)buff[8] << 24) + (buff[9] << 16) +
                       (buff[10] << 8) + buff[11]);
                printf("starting LBA: %u\n", lba);
            }
            u = (((unsigned int)buff[12] << 24) + (buff[13] << 16) +
                 (buff[14] << 8) + buff[15]);
            if (opts->quiet)
                printf("%u\n", u);
            else
                printf("Nominal speed at starting LBA: %u kiloBytes/sec\n",
                       u);
            if (opts->verbose) {
                lba = (((unsigned int)buff[16] << 24) + (buff[17] << 16) +
                       (buff[18] << 8) + buff[19]);
                printf("ending LBA: %u\n", lba);
            }
            u = (((unsigned int)buff[20] << 24) + (buff[21] << 16) +
                 (buff[22] << 8) + buff[23]);
            if (1 == opts->quiet)
                printf("%u\n", u);
            else if (0 == opts->quiet)
                printf("Nominal speed at ending LBA: %u kiloBytes/sec\n",
                       u);
        }
    }
    return res;
#endif
}

static const char *
get_profile_str(int profile_num, char * buff)
{
    const struct sdparm_val_desc_t * pdp;

    for (pdp = sdparm_profile_arr; pdp->desc; ++pdp) {
        if (pdp->val == profile_num) {
            strcpy(buff, pdp->desc);
            return buff;
        }
    }
    snprintf(buff, 64, "0x%x", profile_num);
    return buff;
}

static void
decode_get_config_feature(int feature, unsigned char * ucp, int len)
{
    int k, profile;
    char buff[128];
    const char * cp;

    cp = "";
    switch (feature) {
    case 0:     /* Profile list */
        printf("Available profiles, profile of current media marked "
               "with * \n");
        for (k = 4; k < len; k += 4) {
            profile = (ucp[k] << 8) + ucp[k + 1];
            printf("    %s   %s\n", get_profile_str(profile, buff),
                   ((ucp[k + 2] & 1) ? "*" : ""));
        }
        break;
    default:
        /* ignore other features */
        break;
    }
}


static void
decode_get_config(unsigned char * resp, int max_resp_len, int len)
{
    int k, extra, feature;
    unsigned char * ucp;

    if (max_resp_len < len) {
        printf("get_config: response to long for buffer, resp_len=%d>>>\n",
               len);
            len = max_resp_len;
    }
    if (len < 8) {
        printf("get_config: response length too short: %d\n", len);
        return;
    }
    ucp = resp + 8;
    len -= 8;
    for (k = 0; k < len; k += extra, ucp += extra) {
        extra = 4 + ucp[3];
        feature = (ucp[0] << 8) + ucp[1];
        if (0 != (extra % 4))
            printf("    get_config: additional length [%d] not a multiple "
                   "of 4, ignore\n", extra - 4);
        else
            decode_get_config_feature(feature, ucp, extra);
    }
}

#define MAX_CONFIG_RESPLEN 2048

static int
do_cmd_profile(int sg_fd, const struct sdparm_opt_coll * opts)
{
    int res, len;
    unsigned char resp[MAX_CONFIG_RESPLEN];

    /* performance (type=0), tolerance 10% nominal, read speed */
    res = sg_ll_get_config(sg_fd, 0x0 /* rt */, 0 /* starting_lba */,
                           resp, sizeof(resp), 1, opts->verbose);
    if (0 == res) {
        len = ((unsigned int)resp[0] << 24) + (resp[1] << 16) +
               (resp[2] << 8) + resp[3] + 4;
        decode_get_config(resp, sizeof(resp), len);
    }
    return res;
}

const struct sdparm_command_t *
sdp_build_cmd(const char * cmd_str, int * rwp, int * argp)
{
    const struct sdparm_command_t * scmdp;
    const char * eq_cp;
    const char * cp;
    char buff[16];
    int len;
    int arg = -1;

    eq_cp = strchr(cmd_str, '=');
    if (eq_cp) {
        len = eq_cp - cmd_str;
        if (len >= (int)sizeof(buff))
            return NULL;
        strncpy(buff, cmd_str, len);
        buff[len] = '\0';
        if (1 != sscanf(eq_cp + 1, "%d", &arg))
            return NULL;
        cp = buff;
    } else
        cp = cmd_str;
    if (argp)
        *argp = arg;

    for (scmdp = sdparm_command_arr; scmdp->name; ++scmdp) {
        if (0 == strcmp(scmdp->name, cp))
            break;
    }
    if ((NULL == scmdp->name) && (strlen(cp) >= 2)) {
        for (scmdp = sdparm_command_arr; scmdp->name; ++scmdp) {
            len = strlen(scmdp->min_abbrev);
            if (0 == memcmp(scmdp->min_abbrev, cp, 2))
                break;
        }
    }
    if (scmdp->name) {
        if (rwp) {
            if ((CMD_READY  == scmdp->cmd_num) ||
                (CMD_SENSE  == scmdp->cmd_num) ||
                (CMD_CAPACITY  == scmdp->cmd_num))
                *rwp = 0;
            else
                *rwp = 1;
        }
        return scmdp;
    } else
        return NULL;
}

void
sdp_enumerate_commands()
{
    const struct sdparm_command_t * scmdp;

    for (scmdp = sdparm_command_arr; scmdp->name; ++scmdp) {
        if (scmdp->extra_arg)
            printf("  %s[=%s]\n", scmdp->name, scmdp->extra_arg);
        else
            printf("  %s\n", scmdp->name);
    }
}

/* Returns 0 if successful */
int
sdp_process_cmd(int sg_fd, const struct sdparm_command_t * scmdp, int cmd_arg,
                int pdt, const struct sdparm_opt_coll * opts)
{
    int res, progress;

    if (! (opts->flexible ||
          (CMD_READY == scmdp->cmd_num) ||
          (CMD_SENSE == scmdp->cmd_num) ||
          (0 == pdt) || (5 == pdt)) ) {
        fprintf(stderr, "this command only valid on a disk or cd/dvd; "
                "use '--flexible' to override\n");
        return SG_LIB_SYNTAX_ERROR;
    }
    switch (scmdp->cmd_num)
    {
    case CMD_CAPACITY:
        res = do_cmd_read_capacity(sg_fd, opts->verbose);
        break;
    case CMD_EJECT:
        res = sg_ll_start_stop_unit(sg_fd, 0 /* immed */, 0 /* fl_num */,
                                    0 /* power cond. */, 0 /* fl */,
                                    1 /*loej */, 0 /* start */, 1 /* noisy */,
                                    opts->verbose);
        break;
    case CMD_LOAD:
        res = sg_ll_start_stop_unit(sg_fd, 0, 0, 0, 0, 1, 1, 1, opts->verbose);
        break;
    case CMD_PROFILE:
        res = do_cmd_profile(sg_fd, opts);
        break;
    case CMD_READY:
        progress = -1;
        res = sg_ll_test_unit_ready_progress(sg_fd, 0, &progress, 0,
                                             opts->verbose);
        if (0 == res)
            printf("Ready\n");
        else {
            if (progress >= 0)
                printf("Not ready, progress indication: %d%% done\n",
                       (progress * 100) / 65536);
            else
                printf("Not ready\n");
        }
        break;
    case CMD_SENSE:
        res = do_cmd_sense(sg_fd, opts->hex, opts->quiet, opts->verbose);
        break;
    case CMD_SPEED:
        res = do_cmd_speed(sg_fd, cmd_arg, opts);
        break;
    case CMD_START:
        res = sg_ll_start_stop_unit(sg_fd, 0, 0, 0, 0, 0, 1, 1, opts->verbose);
        break;
    case CMD_STOP:
        res = sg_ll_start_stop_unit(sg_fd, 0, 0, 0, 0, 0, 0, 1, opts->verbose);
        break;
    case CMD_SYNC:
        res = sg_ll_sync_cache_10(sg_fd, 0, 0, 0, 0, 0, 1, opts->verbose);
        break;
    case CMD_UNLOCK:
        res = sg_ll_prevent_allow(sg_fd, 0, 1, opts->verbose);
        break;
    default:
        fprintf(stderr, "unknown cmd number [%d]\n", scmdp->cmd_num);
        return SG_LIB_SYNTAX_ERROR;
    }
    return res;
}

/*
 * Copyright (c) 2005-2006 Douglas Gilbert.
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
 * sdparm is a utility program for getting and setting parameters on devices
 * that use one of the SCSI command sets. In some cases commands can be sent
 * to the device (e.g. eject removable media).
 *
 * Note that some devices, such as CD/DVD drives, use a SCSI command set
 * (i.e. MMC-4 and SPC-3) but are not normally categorized as "SCSI" since
 * most use the packet interface over the ATA transport (ATAPI).
 */

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_GETOPT_LONG
#include <getopt.h>
#else
#include "port_getopt.h"
#endif

#ifdef SDPARM_LINUX
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/utsname.h>

#include <scsi/scsi.h>
#include <scsi/sg.h>

static int map_if_lk24(int sg_fd, const char * device_name, int rw,
                       int verbose);
#endif  /* SDPARM_LINUX */

#include "sdparm.h"
#include "sg_lib.h"
#include "sg_cmds.h"

static char * version_str = "0.97 20060125";


static struct option long_options[] = {
    {"six", 0, 0, '6'},
    {"all", 0, 0, 'a'},
    {"dbd", 0, 0, 'B'},
    {"clear", 1, 0, 'c'},
    {"command", 1, 0, 'C'},
    {"defaults", 0, 0, 'D'},
    {"dummy", 0, 0, 'd'},
    {"enumerate", 0, 0, 'e'},
    {"flexible", 0, 0, 'f'},
    {"get", 1, 0, 'g'},
    {"help", 0, 0, 'h'},
    {"hex", 0, 0, 'H'},
    {"inquiry", 0, 0, 'i'},
    {"long", 0, 0, 'l'},
    {"page", 1, 0, 'p'},
    {"quiet", 1, 0, 'q'},
    {"set", 1, 0, 's'},
    {"save", 0, 0, 'S'},
    {"transport", 1, 0, 't'},
    {"verbose", 0, 0, 'v'},
    {"version", 0, 0, 'V'},
    {0, 0, 0, 0},
};

static void usage()
{
    fprintf(stderr, "Usage: "
          "sdparm    [--all] [--clear=<str>] [--command=<cmd>] [--dbd]\n"
          "                 [--defaults] [--dummy] [--flexible] "
          "[--get=<str>] [--help]\n"
          "                 [--hex] [--inquiry] [--long] "
          "[--page=<pg[,spg]>] [--quiet]\n"
          "                 [--save] [--set=<str>] [--six] "
          "[--transport=<tn>]\n"
          "                 [--verbose] [--version] <scsi_device>\n\n"
          "       sdparm    --enumerate [--all] [--inquiry] [--long] "
          "[--page=<pg[,spg]>]\n"
          "                 [--transport=<tn>]\n"
          "  where:\n"
          "      --all | -a            list all known attributes for given "
          "device\n"
          "      --clear=<str> | -c <str>  clear (zero) attribute value(s)\n"
          "      --command=<cmd> | -C <cmd>  perform <cmd> (e.g. 'eject')\n"
          "      --dbd | -B            set DBD bit in mode sense cdb\n"
          "      --defaults | -D       set a mode page to its default "
          "values\n"
          "      --dummy | -d          don't write back modified mode page\n"
          "      --enumerate | -e      list known pages and attributes "
          "(ignore device)\n"
          "      --flexible | -f       compensate for common errors, "
          "relax some checks\n"
          "      --get=<str> | -g <str>  get (fetch) attribute value(s)\n"
          "      --help | -h           print out usage message\n"
          "      --hex | -H            output in hex rather than name/value "
          "pairs\n"
          "      --inquiry | -i        output INQUIRY VPD page(s) (def: mode "
          "page(s))\n"
          "      --long | -l           add description to attribute output\n"
          "      --page=<pg[,spg]> | -p <pg[,spg]>  page (and optionally "
          "subpage) number\n"
          "                            [or abbrev] to output, change or "
          "enumerate\n"
          "      --quiet | -q          suppress device vendor/product/"
          "revision string line\n"
          "      --save | -S           place mode changes in saved page as "
          "well\n"
          "      --set=<str> | -s <str>  set attribute value(s)\n"
          "      --six | -6            use 6 byte SCSI mode cdbs (def: 10 "
          "byte)\n"
          "      --transport=<tn> | -t <tn>     transport protocol number "
          "[or abbrev]\n"
          "      --verbose | -v        increase verbosity\n"
          "      --version | -V        print version string and exit\n\n"
          "View or change attributes of a SCSI device (e.g. disk or CD/DVD "
          "drive)\n"
          );
}

static void enumerate_mps(int transp_proto)
{
    const struct sdparm_values_name_t * vnp;

    if ((transp_proto < 0) || (transp_proto > 15)) {
        for (vnp = sdparm_gen_mode_pg; vnp->acron; ++vnp) {
            if (vnp->name) {
                if (vnp->subvalue)
                    printf("  %-4s 0x%02x,0x%02x  %s\n", vnp->acron,
                           vnp->value, vnp->subvalue, vnp->name);
                else
                    printf("  %-4s 0x%02x       %s\n", vnp->acron,
                           vnp->value, vnp->name);
            }
        }
        return;
    }
    for (vnp = sdparm_transport_mp[transp_proto].mpage; vnp && vnp->acron;
         ++vnp) {
        if (vnp->name) {
            if (vnp->subvalue)
                printf("  %-4s 0x%02x,0x%02x  %s\n", vnp->acron, vnp->value,
                       vnp->subvalue, vnp->name);
            else
                printf("  %-4s 0x%02x       %s\n", vnp->acron, vnp->value,
                       vnp->name);
        }
    }
}

static void enumerate_vpds()
{
    const struct sdparm_values_name_t * vnp;

    for (vnp = sdparm_vpd_pg; vnp->acron; ++vnp) {
        if (vnp->name)
            printf("  %-10s 0x%02x      %s\n", vnp->acron, vnp->value,
                   vnp->name);
    }
}

static void enumerate_transports()
{
    const struct sdparm_values_name_t * vnp;

    for (vnp = sdparm_transport_id; vnp->acron; ++vnp) {
        if (vnp->name)
            printf("  %-6s 0x%02x     %s\n", vnp->acron, vnp->value,
                   vnp->name);
    }
}

static void print_mp_extra(const char * extra)
{
    int n;
    char b[128];
    char * cp;
    char * p;

    for (p = (char *)extra; (cp = strchr(p, '\t')); p = cp + 1) {
        n = cp - p;
        if (n > (int)(sizeof(b) - 1))
            n = (sizeof(b) - 1);
        strncpy(b, p, n);
        b[n] = '\0';
        printf("\t%s\n", b);
    }
    printf("\t%s\n", p);
}

static void enumerate_mitems(int pn, int spn, int pdt, int transp_proto,
                             int long_out)
{
    int t_pn, t_spn, t_pdt;
    const struct sdparm_mode_page_item * mpi;
    char buff[128];
    char b[128];
    int found = 0;

    t_pn = -1;
    t_spn = -1;
    t_pdt = -2;
    if ((transp_proto < 0) || (transp_proto > 15))
        mpi = sdparm_mitem_arr;
    else {
        mpi = sdparm_transport_mp[transp_proto].mitem;
        if (NULL == mpi)
            return;
    }
    for ( ; mpi->acron; ++mpi) {
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
            sdp_get_mpage_name(t_pn, t_spn, t_pdt, transp_proto, 1, buff,
                               sizeof(buff));
            if (long_out)
                printf("%s [%s] mode page:\n", buff,
                       sdp_get_pdt_doc_str(t_pdt, sizeof(b), b)); 
            else
                printf("%s mode page:\n", buff); 
        } else {
            if ((pn >= 0) && ((pn != t_pn) || (spn != t_spn)))
                continue;
        }
        printf("  %-10s [0x%02x:%d:%-2d]  %s\n", mpi->acron, mpi->start_byte,
               mpi->start_bit, mpi->num_bits, mpi->description);
        if ((long_out > 1) && mpi->extra)
            print_mp_extra(mpi->extra);
        found = 1;
    }
    if ((! found) && (pn >= 0)) {
        sdp_get_mpage_name(pn, spn, pdt, transp_proto, 1, buff,
                           sizeof(buff));
        fprintf(stderr, "%s mode page: no items found\n", buff);
    }
}

static void list_mp_settings(struct sdparm_mode_page_settings * mps, int get)
{
    struct sdparm_mode_page_item * mpip;
    int k;

    printf("mp_settings: page,subpage=0x%x,0x%x  num=%d\n",
           mps->page_num, mps->subpage_num, mps->num_it_vals);
    for (k = 0; k < mps->num_it_vals; ++k) {
        mpip = &mps->it_vals[k].mpi;
        if (get)
            printf("  [0x%x,0x%x]", mpip->page_num, mpip->subpage_num);

        printf("  pdt=%d start_byte=0x%x start_bit=%d num_bits=%d  val=%lld",
               mpip->pdt, mpip->start_byte, mpip->start_bit, mpip->num_bits,
               mps->it_vals[k].val);
        if (mpip->acron)
            printf("  acronym: %s\n", mpip->acron);
        else
            printf("\n");
    }
}

static void print_mp_entry(const char * pre, int smask,
                           const struct sdparm_mode_page_item *mpi,
                           const unsigned char * cur_mp,
                           const unsigned char * cha_mp,
                           const unsigned char * def_mp,
                           const unsigned char * sav_mp,
                           int long_out, int force_decimal)
{
    int sep = 0;
    int all_set;
    unsigned long long u;
    const char * acron;

    all_set = 0;
    acron = (mpi->acron ? mpi->acron : "");
    u = sdp_mp_get_value_check(mpi, cur_mp, &all_set);
    printf("%s%-10s", pre, acron);
    if (force_decimal)
        printf("%lld", (long long)u);
    else if (mpi->flags & MF_HEX)
        printf("0x%llx", u);
    else if (all_set)
        printf(" -1");
    else
        printf("%3llu", u);
    if (smask & 0xe) {
        printf("  [");
        if (cha_mp && (smask & 2)) {
            printf("cha: %s",
                   (sdp_mp_get_value(mpi, cha_mp) ? "y" : "n"));
            sep = 1;
        }
        if (def_mp && (smask & 4)) {
            all_set = 0;
            u = sdp_mp_get_value_check(mpi, def_mp, &all_set);
            printf("%sdef:", (sep ? ", " : " "));
            if (force_decimal)
                printf("%lld", (long long)u);
            else if (mpi->flags & MF_HEX)
                printf("0x%llx", u);
            else if (all_set)
                printf(" -1");
            else
                printf("%3llu", u);
            sep = 1;
        }
        if (sav_mp && (smask & 8)) {
            all_set = 0;
            u = sdp_mp_get_value_check(mpi, sav_mp, &all_set);
            printf("%ssav:", (sep ? ", " : " "));
            if (force_decimal)
                printf("%lld", (long long)u);
            else if (mpi->flags & MF_HEX)
                printf("0x%llx", u);
            else if (all_set)
                printf(" -1");
            else
                printf("%3llu", u);
        }
        printf("]");
    }
    if (long_out && mpi->description)
        printf("  %s", mpi->description);
    printf("\n");
    if ((long_out > 1) && mpi->extra)
        print_mp_extra(mpi->extra);
}

static int ll_mode_sense(int fd, const struct sdparm_opt_coll * opts, int pn,
                         int spn, unsigned char * resp, int mx_resp_len,
                         int noisy, int verb)
{
    if (opts->mode_6)
        return sg_ll_mode_sense6(fd, opts->dbd, 0 /*current */, pn, spn,
                                 resp, mx_resp_len, noisy, verb);
    else
        return sg_ll_mode_sense10(fd, 0 /* llbaa */, opts->dbd, 0, pn,
                                  spn, resp, mx_resp_len, noisy, verb);
}

static void print_mode_info(int sg_fd, int pn, int spn, int pdt,
                            const struct sdparm_opt_coll * opts, int verbose)
{
    int res, len, verb, smask, single_pg, fetch_pg, rep_len, orig_pn, warned;
    const struct sdparm_mode_page_item * mpi;
    const struct sdparm_mode_page_item * init_mpi;
    unsigned char cur_mp[DEF_MODE_RESP_LEN];
    unsigned char cha_mp[DEF_MODE_RESP_LEN];
    unsigned char def_mp[DEF_MODE_RESP_LEN];
    unsigned char sav_mp[DEF_MODE_RESP_LEN];
    const char * name;
    void * pc_arr[4];
    char buff[128];

    verb = (verbose > 0) ? verbose - 1 : 0;
    if ((0 == pdt) && (opts->long_out > 0) && (0 == opts->quiet)) {
        memset(cur_mp, 0, sizeof(cur_mp));
        res = ll_mode_sense(sg_fd, opts, ALL_MPAGES, 0, cur_mp, 8, 0, verb);
        if (0 == res) {
            smask = cur_mp[opts->mode_6 ? 2 : 3];
            printf("    Direct access device specific parameters: WP=%d  "
                   "DPOFUA=%d\n", !!(smask & 0x80), !!(smask & 0x10));
        }
    }
    orig_pn = pn;
    if ((opts->transport < 0) || (opts->transport > 15))
        mpi = sdparm_mitem_arr;
    else {
        mpi = sdparm_transport_mp[opts->transport].mitem;
        if (NULL == mpi)
            return;
    }
    init_mpi = mpi;
    if (pn >= 0) {
        single_pg = 1;
        fetch_pg = 1;
        for ( ; mpi->acron; ++mpi) {
            if ((pn == mpi->page_num) && (spn == mpi->subpage_num)) {
                if ((pdt < 0) || (mpi->pdt < 0) ||
                    (pdt == mpi->pdt) || opts->flexible)
                    break;
            }
        }
        if (NULL == mpi->acron) {
            if (opts->hex)
                mpi = init_mpi;    /* trick to enter main loop once */
            else {
                sdp_get_mpage_name(pn, spn, pdt, opts->transport, opts->hex,
                                   buff, sizeof(buff));
                fprintf(stderr, "%s mode page, attributes not found\n", buff);
                if ((0 == opts->flexible) && verbose)
                    fprintf(stderr, "    perhaps try '--flexible'\n");
            }
        }
    } else {
        single_pg = 0;
        fetch_pg = 0;
        mpi = init_mpi;
    }
    name = "";
    for (smask = 0, len = 0, warned = 0 ; mpi->acron; ++mpi, fetch_pg = 0) {
        if (0 == fetch_pg) {
            if ((pdt >=0) && (mpi->pdt >= 0) &&
                (pdt != mpi->pdt) && (0 == opts->flexible))
                continue;
            if (! (((orig_pn >= 0) ? 1 : opts->all) ||
                   (MF_COMMON & mpi->flags)))
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
            warned = 0;
            pc_arr[0] = cur_mp;
            pc_arr[1] = cha_mp;
            pc_arr[2] = def_mp;
            pc_arr[3] = sav_mp;
            res = sg_get_mode_page_controls(sg_fd, opts->mode_6, pn, spn,
                                            opts->dbd, opts->flexible,
                                            DEF_MODE_RESP_LEN, &smask,
                                            pc_arr, &rep_len, verb);
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
                len = sdp_get_mp_len(cur_mp);
                sdp_get_mpage_name(pn, spn, pdt, opts->transport, opts->hex,
                                   buff, sizeof(buff));
                printf("%s ", buff);
                if (verbose) {
                    if (spn)
                        printf("[0x%x,0x%x] ", pn, spn);
                    else
                        printf("[0x%x] ", pn);
                }
                printf("mode page");
                if ((opts->long_out > 1) || verbose)
                    printf(" [PS=%d]:\n", !!(cur_mp[0] & 0x80));
                else
                    printf(":\n");
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
                    sdp_get_mpage_name(pn, spn, pdt, opts->transport,
                                       opts->hex, buff, sizeof(buff));
                    fprintf(stderr, ">> %s mode %spage ", buff,
                            (spn ? "sub" : ""));
                    if (verbose) {
                        if (spn)
                            fprintf(stderr, "[0x%x,0x%x] ", pn, spn);
                        else
                            fprintf(stderr, "[0x%x] ", pn);
                    }
                    fprintf(stderr, "not supported\n");
                }
            }
        }
        if (smask && (! opts->hex)) {
            if (mpi->start_byte >= len) {
                if ((0 == opts->flexible) && (0 == verbose))
                    continue;   // step over
                if (0 == warned) {
                    warned = 1;
                    if (opts->flexible)
                        fprintf(stderr, " >> hereafter field position "
                                "exceeds mode page length=%d\n", len);
                    else {
                        fprintf(stderr, " >> skipping rest as field position "
                                "exceeds mode page length=%d\n", len);
                        continue;
                    }
                }
                if (0 == opts->flexible)
                    continue;
            }
            print_mp_entry("  ", smask, mpi, cur_mp, cha_mp,
                     def_mp, sav_mp, opts->long_out, 0);
        }
    }
}

static void get_mode_info(int sg_fd, struct sdparm_mode_page_settings * mps,
                          int pdt, const struct sdparm_opt_coll * opts,
                          int verbose)
{
    int k, res, verb, smask, pn, spn, warned, rep_len, len;
    unsigned long long u;
    long long val;
    const struct sdparm_mode_page_item * mpi;
    unsigned char cur_mp[DEF_MODE_RESP_LEN];
    unsigned char cha_mp[DEF_MODE_RESP_LEN];
    unsigned char def_mp[DEF_MODE_RESP_LEN];
    unsigned char sav_mp[DEF_MODE_RESP_LEN];
    const struct sdparm_mode_page_it_val * ivp;
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
                                 opts->dbd, opts->flexible, DEF_MODE_RESP_LEN,
                                 &smask, pc_arr, &rep_len, verb);
                break;
            case 1:
            case 2:
                pc_arr[0] = cur_mp;
                pc_arr[1] = NULL;
                pc_arr[2] = NULL;
                pc_arr[3] = NULL;
                res = sg_get_mode_page_controls(sg_fd, opts->mode_6, pn, spn,
                                 opts->dbd, opts->flexible, DEF_MODE_RESP_LEN,
                                 &smask, pc_arr, &rep_len, verb);
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
                    fprintf(stderr, "error %sin ",
                            (verb ? "" : "(try adding '-vv') "));
                sdp_get_mpage_name(pn, spn, mpi->pdt, opts->transport,
                                   opts->hex, buff, sizeof(buff));
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
        len = (smask & 1) ? sdp_get_mp_len(cur_mp) : 0;
        if (mpi->start_byte >= len) {
            fprintf(stderr, ">> warning: ");
            if (mpi->acron)
                fprintf(stderr, "%s ", mpi->acron);
            else
                fprintf(stderr, "0x%x:%d:%d ",
                        mpi->start_byte, mpi->start_bit, mpi->num_bits);
            fprintf(stderr, "field position exceeds mode page length=%d\n",
                    len);
            if (! opts->flexible)
                continue;
        }
        if (0 == val) {
            if (opts->hex) {
                if (smask & 1) {
                    u = sdp_mp_get_value(mpi, cur_mp);
                    printf("0x%02llx ", u);
                } else
                    printf("-    ");
                if (smask & 2) {
                    u = sdp_mp_get_value(mpi, cha_mp);
                    printf("0x%02llx ", u);
                } else
                    printf("-    ");
                if (smask & 4) {
                    u = sdp_mp_get_value(mpi, def_mp);
                    printf("0x%02llx ", u);
                } else
                    printf("-    ");
                if (smask & 8) {
                    u = sdp_mp_get_value(mpi, sav_mp);
                    printf("0x%02llx ", u);
                } else
                    printf("-    ");
                printf("\n");
            } else
                print_mp_entry("", smask, mpi, cur_mp, cha_mp,
                               def_mp, sav_mp, opts->long_out, 0);
        } else if (1 == val) {
            if (opts->hex) {
                if (smask & 1) {
                    u = sdp_mp_get_value(mpi, cur_mp);
                    printf("0x%02llx ", u);
                } else
                    printf("-    ");
                printf("\n");
            } else
                print_mp_entry("", smask & 1, mpi, cur_mp, NULL,
                               NULL, NULL, opts->long_out, 0);
        } else if (2 == val) {
            if (opts->hex) {
                if (smask & 1) {
                    u = sdp_mp_get_value(mpi, cur_mp);
                    printf("%02lld ", (long long)u);
                } else
                    printf("-    ");
                printf("\n");
            } else
                print_mp_entry("", smask & 1, mpi, cur_mp, NULL,
                               NULL, NULL, opts->long_out, 1);
        }
    }
}

/* Return of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> invalid opcode, SG_LIB_CAT_ILLEGAL_REQ ->
 * bad field in cdb, -1 -> other failure */
static int change_mode_page(int sg_fd, int pdt,
                            struct sdparm_mode_page_settings * mps,
                            const struct sdparm_opt_coll * opts, int verbose)
{
    int k, off, md_len, len, res;
    char ebuff[EBUFF_SZ];
    unsigned char mdpg[MAX_MODE_DATA_LEN];
    struct sdparm_mode_page_it_val * ivp;
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
    memset(mdpg, 0, sizeof(mdpg));
    res = ll_mode_sense(sg_fd, opts, mps->page_num, mps->subpage_num, mdpg,
                        4, 1, verbose);
    if (0 != res) {
        sdp_get_mpage_name(mps->page_num, mps->subpage_num, pdt,
                           opts->transport, 0, buff, sizeof(buff));
        fprintf(stderr, "change_mode_page: failed fetching page: %s\n",
                buff);
        return -1;
    }
    md_len = opts->mode_6 ? (mdpg[0] + 1) : ((mdpg[0] << 8) + mdpg[1] + 2);
    if (md_len > (int)sizeof(mdpg)) {
        fprintf(stderr, "change_mode_page: mode data length=%d exceeds "
                "allocation length=%d\n", md_len, (int)sizeof(mdpg));
        return -1;
    }
    res = ll_mode_sense(sg_fd, opts, mps->page_num, mps->subpage_num, mdpg,
                        md_len, 1, verbose);
    if (0 != res) {
        sdp_get_mpage_name(mps->page_num, mps->subpage_num, pdt,
                           opts->transport, 0, buff, sizeof(buff));
        fprintf(stderr, "change_mode_page: failed fetching page: %s\n",
                buff);
        return -1;
    }
    off = sg_mode_page_offset(mdpg, md_len, opts->mode_6, ebuff, EBUFF_SZ);
    if (off < 0) {
        fprintf(stderr, "change_mode_page: page offset failed: %s\n", ebuff);
        return -1;
    }
    len = sdp_get_mp_len(mdpg + off);
    mdpg[0] = 0;        /* mode data length reserved for mode select */
    if (! opts->mode_6)
        mdpg[1] = 0;    /* mode data length reserved for mode select */
    if (0 == pdt)       /* DPOFUA in disk specific parameters is ... */
        mdpg[opts->mode_6 ? 2 : 3] &= 0xef;     /* reserved for mode select */

    for (k = 0; k < mps->num_it_vals; ++k) {
        ivp = &mps->it_vals[k];
        if (ivp->mpi.start_byte >= len) {
            /* mpi.start_byte is too large for actual mpage length */
            fprintf(stderr, "The start_byte of ");
            if (ivp->mpi.acron)
                fprintf(stderr, "%s ", ivp->mpi.acron);
            else
                fprintf(stderr, "0x%x:%d:%d ",
                        ivp->mpi.start_byte, ivp->mpi.start_bit,
                        ivp->mpi.num_bits);
            fprintf(stderr, "exceeds length of this mode page: %d [0x%x]\n",
                    len, len);
            if (opts->flexible)
                fprintf(stderr, "    applying anyway\n");
            else {
                fprintf(stderr, "    nothing modified, use '--flexible' to "
                        "override\n");
                return -1;
            }
        }
        sdp_mp_set_value(ivp->val, &ivp->mpi, mdpg + off);
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
        sdp_get_mpage_name(mps->page_num, mps->subpage_num, pdt,
                           opts->transport, 0, buff, sizeof(buff));
        fprintf(stderr, "change_mode_page: failed setting page: %s\n", buff);
        return -1;
    }
    return 0;
}

/* Return of 0 -> success,
 * SG_LIB_CAT_INVALID_OP -> invalid opcode, SG_LIB_CAT_ILLEGAL_REQ ->
 * bad field in cdb, -1 -> other failure */
static int set_def_mode_page(int sg_fd, int pn, int spn, 
                             unsigned char * mode_pg, int mode_pg_len,
                             const struct sdparm_opt_coll * opts, int verbose)
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
    ret = ll_mode_sense(sg_fd, opts, pn, spn, mdp, 4, 1, verbose);
    if (0 != ret) {
        sdp_get_mpage_name(pn, spn, -1, opts->transport, 0, buff,
                           sizeof(buff));
        fprintf(stderr, "set_def_mode_page: failed fetching page: %s\n",
                buff);
        goto err_out;
    }
    md_len = opts->mode_6 ? (mdp[0] + 1) : ((mdp[0] << 8) + mdp[1] + 2);
    if (md_len > len) {
        fprintf(stderr, "set_def_mode_page: mode data length=%d exceeds "
                "allocation length=%d\n", md_len, len);
        ret = -1;
        goto err_out;
    }
    ret = ll_mode_sense(sg_fd, opts, pn, spn, mdp, md_len, 1, verbose);
    if (0 != ret) {
        sdp_get_mpage_name(pn, spn, -1, opts->transport, 0, buff,
                           sizeof(buff));
        fprintf(stderr, "set_def_mode_page: failed fetching page: %s\n",
                buff);
        goto err_out;
    }
    off = sg_mode_page_offset(mdp, len, opts->mode_6, ebuff, EBUFF_SZ);
    if (off < 0) {
        fprintf(stderr, "set_def_mode_page: page offset failed: %s\n",
                ebuff);
        ret = -1;
        goto err_out;
    }
    mdp[0] = 0;        /* mode data length reserved for mode select */
    if (! opts->mode_6)
        mdp[1] = 0;    /* mode data length reserved for mode select */
    if ((md_len - off) > mode_pg_len) {
        fprintf(stderr, "set_def_mode_page: mode length length=%d exceeds "
                "new contents length=%d\n", md_len - off, mode_pg_len);
        ret = -1;
        goto err_out;
    }
    memcpy(mdp + off, mode_pg, md_len - off);
    mdp[off] &= 0x7f;   /* mask out PS bit, reserved in mode select */
    if (opts->dummy) {
        fprintf(stderr, "Mode data that would have been written:\n");
        dStrHex((const char *)mdp, md_len, 1);
        ret = 0;
        goto err_out;
    }
    if (opts->mode_6)
        ret = sg_ll_mode_select6(sg_fd, 1, opts->saved, mdp, md_len, 1,
                                 verbose);
    else
        ret = sg_ll_mode_select10(sg_fd, 1, opts->saved, mdp, md_len, 1,
                                  verbose);
    if (0 != ret) {
        sdp_get_mpage_name(pn, spn, -1, opts->transport, 0, buff,
                           sizeof(buff));
        fprintf(stderr, "set_def_mode_page: failed setting page: %s\n",
                buff);
        goto err_out;
    }

err_out:
    free(mdp);
    return ret;
}

static int set_mp_defaults(int sg_fd, int pn, int spn, int pdt,
                           const struct sdparm_opt_coll * opts,
                           int verbose)
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
    res = sg_get_mode_page_controls(sg_fd, opts->mode_6, pn, spn, opts->dbd,
                 opts->flexible, DEF_MODE_RESP_LEN, &smask, pc_arr,
                 &rep_len, verbose);
    if (SG_LIB_CAT_INVALID_OP == res) {
        if (opts->mode_6)
            fprintf(stderr, "6 byte MODE SENSE cdb not supported, "
                    "try again without '-6' option\n");
        else
            fprintf(stderr, "10 byte MODE SENSE cdb not supported, "
                    "try again with '-6' option\n");
        return -1;
    }
    if (verbose && (0 == opts->flexible) && (rep_len > 0xa00)) {
        sdp_get_mpage_name(pn, spn, pdt, opts->transport, 0, buff,
                           sizeof(buff));
        fprintf(stderr, "%s mode page length=%d too long, perhaps "
                "try '--flexible'\n", buff, rep_len);
    }
    if ((smask & 1)) {
        len = sdp_get_mp_len(cur_mp);
        if ((smask & 4)) {
            return set_def_mode_page(sg_fd, pn, spn, def_mp, len, opts,
                                     verbose);
        }
        else {
            sdp_get_mpage_name(pn, spn, pdt, opts->transport, 0, buff,
                               sizeof(buff));
            fprintf(stderr, ">> %s mode page (default) not supported\n",
                    buff);
            return -1;
        }
    } else {
        sdp_get_mpage_name(pn, spn, pdt, opts->transport, 0, buff,
                           sizeof(buff));
        fprintf(stderr, ">> %s mode page not supported\n", buff);
        return -1;
    }
}

/* Trying to decode multipliers as sg_get_num() [as sg_libs does] would
 * only confuse things here, so use this local trimmed version */
static int get_num(const char * buf)
{
    int res, len, num;
    unsigned int unum;

    if ((NULL == buf) || ('\0' == buf[0]))
        return -1;
    len = strlen(buf);
    if (('0' == buf[0]) && (('x' == buf[1]) || ('X' == buf[1]))) {
        res = sscanf(buf + 2, "%x", &unum);
        num = unum;
    } else if ('H' == toupper(buf[len - 1])) {
        res = sscanf(buf, "%x", &unum);
        num = unum;
    } else
        res = sscanf(buf, "%d", &num);
    if (1 == res)
        return num;
    else
        return -1;
}

static long long get_llnum(const char * buf)
{
    int res, len;
    long long num;
    unsigned long long unum;

    if ((NULL == buf) || ('\0' == buf[0]))
        return -1;
    len = strlen(buf);
    if (('0' == buf[0]) && (('x' == buf[1]) || ('X' == buf[1]))) {
        res = sscanf(buf + 2, "%llx", &unum);
        num = unum;
    } else if ('H' == toupper(buf[len - 1])) {
        res = sscanf(buf, "%llx", &unum);
        num = unum;
    } else
        res = sscanf(buf, "%lld", &num);
    return (1 == res) ? num : -1;
}

static int build_mp_settings(const char * arg,
                             struct sdparm_mode_page_settings * mps,
                             int transp_proto, int clear, int get)
{
    int len, b_sz, num, from, cont, colon;
    unsigned int u;
    long long ll;
    char buff[64];
    char acron[64];
    char vb[64];
    const char * cp;
    const char * ncp;
    const char * ecp;
    struct sdparm_mode_page_it_val * ivp;
    const struct sdparm_mode_page_item * mpi;
    const struct sdparm_mode_page_item * prev_mpi;

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
        colon = strchr(buff, ':') ? 1 : 0;
        if ((isalpha(buff[0]) && (! colon)) ||
            (isdigit(buff[0]) && ('_' == buff[1]))) {
            ecp = strchr(buff, '=');
            if (ecp) {
                strncpy(acron, buff, ecp - buff);
                acron[ecp - buff] = '\0';
                strcpy(vb, ecp + 1);
                if (0 == strcmp("-1", vb))
                    ivp->val = -1;
                else {
                    ivp->val = get_llnum(vb);
                    if (-1 == ivp->val) {
                        fprintf(stderr, "unable to decode: %s value\n",
                                buff);
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
                    mpi = sdp_find_mitem_by_acron(acron, &from, transp_proto);
                    if (NULL == mpi) {
                        if (cont) {
                            mpi = prev_mpi;
                            break;
                        } else {
                            fprintf(stderr, "couldn't find acronym: %s\n",
                                    acron);
                            fprintf(stderr, "    [perhaps a '--transport="
                                    "<tn>' option is needed]\n");
                        }
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
                    mpi = sdp_find_mitem_by_acron(acron, &from, transp_proto);
                    if (NULL == mpi) {
                        if (cont) {
                            fprintf(stderr, "mode page of acronym: %s "
                                    "[0x%x,0x%x] doesn't match prior\n",
                                    acron, prev_mpi->page_num,
                                    prev_mpi->subpage_num);
                            fprintf(stderr, "    mode page: 0x%x,0x%x\n",
                                    mps->page_num, mps->subpage_num);
                            fprintf(stderr, "For '--set' and '--clear' all "
                                    "attributes must be in the same mode "
                                    "page\n");
                        } else {
                            fprintf(stderr, "couldn't find acronym: %s\n",
                                    acron);
                            fprintf(stderr, "    [perhaps a '--transport="
                                    "<tn>' option is needed]\n");
                        }
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
            if (mpi->num_bits < 64) {
                ll = 1;
                ivp->val &= (ll << mpi->num_bits) - 1;
            }
            ivp->mpi = *mpi;    /* struct assignment */
        } else {    /* expect "start_byte:start_bit:num_bits[=<val>]" */
            /* start_byte may be in hex ('0x' prefix or 'h' suffix) */
            if ((0 == strncmp("0x", buff, 2)) ||
                (0 == strncmp("0X", buff, 2))) {
                num = sscanf(buff + 2, "%x:%d:%d=%s", &u,
                             &ivp->mpi.start_bit, &ivp->mpi.num_bits, vb);
                ivp->mpi.start_byte = u;
            } else {
                if (strstr(buff, "h:"))
                    num = sscanf(buff, "%xh:%d:%d=%s", &ivp->mpi.start_byte,
                                 &ivp->mpi.start_bit, &ivp->mpi.num_bits, vb);
                else if (strstr(buff, "H:"))
                    num = sscanf(buff, "%xH:%d:%d=%s", &ivp->mpi.start_byte,
                                 &ivp->mpi.start_bit, &ivp->mpi.num_bits, vb);
                else
                    num = sscanf(buff, "%d:%d:%d=%s", &ivp->mpi.start_byte,
                                 &ivp->mpi.start_bit, &ivp->mpi.num_bits, vb);
            }
            if (num < 3) {
                fprintf(stderr, "unable to decode: %s\n", buff);
                fprintf(stderr, "    expected: start_byte:start_bit:num_bits"
                        "[=<val>]\n");
                return -1;
            }
            if (3 == num)
                ivp->val = ((clear || get) ? 0 : -1);
            else {
                if (0 == strcmp("-1", vb))
                    ivp->val = -1;
                else {
                    ivp->val = get_llnum(vb);
                    if (-1 == ivp->val) {
                        fprintf(stderr, "unable to decode "
                                "start_byte:start_bit:num_bits value\n");
                        return -1;
                    }
                }
            }
            ivp->mpi.pdt = -1;  /* don't known pdt now, so don't restrict */
            if (ivp->mpi.start_byte < 0) {
                fprintf(stderr, "need positive start byte offset\n");
                return -1;
            }
            if ((ivp->mpi.start_bit < 0) || (ivp->mpi.start_bit > 7)) {
                fprintf(stderr, "need start bit in 0..7 range "
                        "(inclusive)\n");
                return -1;
            }
            if ((ivp->mpi.num_bits < 1) || (ivp->mpi.num_bits > 64)) {
                fprintf(stderr, "need number of bits in 1..64 range "
                        "(inclusive)\n");
                return -1;
            }
            if (mps->page_num < 0) {
                fprintf(stderr, "need '--page=' option for mode page "
                        "name or number\n");
                return -1;
            } else if (get) {
                ivp->mpi.page_num = mps->page_num;
                ivp->mpi.subpage_num = mps->subpage_num;
            }
            if (ivp->mpi.num_bits < 64) {
                long long ll = 1;

                ivp->val &= (ll << ivp->mpi.num_bits) - 1;
            }
        }
        ++mps->num_it_vals;
        if (ncp)
            cp = ncp + 1;
        else
            break;
    }
    return 0;
}

static int open_and_simple_inquiry(const char * device_name, int rw,
                                   int * pdt,
                                   const struct sdparm_opt_coll * opts,
                                   int verbose)
{
    int res, verb, sg_fd, l_pdt;
    struct sg_simple_inquiry_resp sir;
    char b[32];

    verb = (verbose > 0) ? verbose - 1 : 0;
    sg_fd = sg_cmds_open_device(device_name, ! rw, 0);
    if (sg_fd < 0) {
        fprintf(stderr, "open error: %s, rw=%d: %s\n", device_name,
                rw, safe_strerror(-sg_fd));
        return -1;
    } 
    res = sg_simple_inquiry(sg_fd, &sir, 0, verb);
    if (res) {
#ifdef SDPARM_LINUX
        if (-1 == res) {
            int sg_sg_fd;

            sg_sg_fd = map_if_lk24(sg_fd, device_name, rw, verbose);
            if (sg_sg_fd < 0)
                goto err_out;
            sg_cmds_close_device(sg_fd);
            sg_fd = sg_sg_fd;
            res = sg_simple_inquiry(sg_fd, &sir, 0, verb);
            if (sg_sg_fd < 0)
                goto err_out;
        }
#endif  /* SDPARM_LINUX */
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
    if ((0 == opts->hex) && (0 == opts->quiet)) {
        printf("    %s: %.8s  %.16s  %.4s",
               device_name, sir.vendor, sir.product, sir.revision);
        if (0 != l_pdt)
            printf("  [%s]", sg_get_pdt_str(l_pdt, sizeof(b), b));
        printf("\n");
        if (verbose && opts->inquiry) {
            char buff[32];

            printf("  PQual=%d  Device_type=0x%x  RMB=%d  version=0x%02x ",
                   sir.peripheral_qualifier, l_pdt, sir.rmb, sir.version);
            printf(" [%s]\n", sdp_get_ansi_version_str(sir.version,
                                                       sizeof(buff), buff));
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
    }
    return sg_fd;

err_out:
    sg_cmds_close_device(sg_fd);
    return -1;
}

static int process_mode_page(int sg_fd, struct sdparm_mode_page_settings * mps,
                             int pn, int spn, int rw, int get,
                             const struct sdparm_opt_coll * opts, int pdt,
                             int verbose)
{
    int res;
    const struct sdparm_values_name_t * vnp;

    if ((pn > 0x3e) || (spn > 0xfe)) {
        fprintf(stderr, "Allowable mode page numbers are 0 to 62\n");
        fprintf(stderr, "  Allowable mode subpage numbers are 0 to 254\n");
        return -1;
    }
    if ((pn > 0) && (pdt >= 0)) {
        vnp = sdp_get_mode_detail(pn, spn, pdt, opts->transport);
        if (NULL == vnp)
            vnp = sdp_get_mode_detail(pn, spn, -1, opts->transport);
        if (vnp && vnp->name && (vnp->pdt >= 0) && (pdt != vnp->pdt)) {
            fprintf(stderr, ">> Warning: %s mode page associated with\n",
                    vnp->name);
            fprintf(stderr, "   peripheral device type 0x%x but device "
                    "pdt is 0x%x\n", vnp->pdt, pdt);
        }
    }
    if (opts->defaults) {
        res = set_mp_defaults(sg_fd, pn, spn, pdt, opts, verbose);
        if (0 != res)
            return -1;
    } else if (rw) {
        if (mps->num_it_vals < 1) {
            fprintf(stderr, "no attributes found to set or clear\n");
            return -1;
        }
        res = change_mode_page(sg_fd, pdt, mps, opts, verbose);
        if (0 != res)
            return -1;
    } else if (get) {
        if (mps->num_it_vals < 1) {
            fprintf(stderr, "no attributes found to get\n");
            return -1;
        }
        get_mode_info(sg_fd, mps, pdt, opts, verbose);
    } else
        print_mode_info(sg_fd, pn, spn, pdt, opts, verbose);
    return 0;
}


int main(int argc, char * argv[])
{
    int sg_fd, res, c, pdt, k;
    struct sdparm_opt_coll opts;
    const char * clear_str = NULL;
    const char * cmd_str = NULL;
    const char * get_str = NULL;
    const char * set_str = NULL;
    const char * page_str = NULL;
    int verbose = 0;
    char device_name[256];
    int pn = -1;
    int spn = -1;
    int rw = 0;
    const struct sdparm_values_name_t * vnp;
    struct sdparm_mode_page_settings mp_settings; 
    char * cp;
    const char * ccp;
    const struct sdparm_command * scmdp = NULL;
    int ret = 1;

    memset(&opts, 0, sizeof(opts));
    opts.transport = -1;
    memset(device_name, 0, sizeof(device_name));
    memset(&mp_settings, 0, sizeof(mp_settings));
    pdt = -1;
    while (1) {
        int option_index = 0;

        c = getopt_long(argc, argv, "6aBc:C:dDefg:hHilp:qs:St:vV",
                        long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
        case '6':
            opts.mode_6 = 1;
            break;
        case 'a':
            opts.all = 1;
            break;
        case 'B':
            opts.dbd = 1;
            break;
        case 'c':
            clear_str = optarg;
            rw = 1;
            break;
        case 'C':
            cmd_str = optarg;
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
            ++opts.hex;
            break;
        case 'i':
            opts.inquiry = 1;
            break;
        case 'l':
            ++opts.long_out;
            break;
        case 'q':
            opts.quiet = 1;
            break;
        case 'p':
            if (page_str) {
                fprintf(stderr, "only one '--page=' option permitted\n");
                usage();
                return 1;
            } else
                page_str = optarg;
            break;
        case 's':
            set_str = optarg;
            rw = 1;
            break;
        case 'S':
            opts.saved = 1;
            rw = 1;
            break;
        case 't':
            if (isalpha(optarg[0])) {
                vnp = sdp_find_transport_by_acron(optarg);
                if (NULL == vnp) {
                    fprintf(stderr, "abbreviation does not match a "
                            "transport protocol\n");
                    printf("Available transport protocols:\n");
                    enumerate_transports();
                    return 1;
                } else
                    opts.transport = vnp->value;
            } else {
                res = get_num(optarg);
                if ((res < 0) || (res > 15)) {
                    fprintf(stderr, "Bad transport value after '-t' "
                            "option\n");
                    printf("Available transport protocols:\n");
                    enumerate_transports();
                    return 1;
                }
                opts.transport = res;
            }
            break;
        case 'v':
            ++verbose;
            break;
        case 'V':
            fprintf(stderr, "version: %s\n", version_str);
            return 0;
        default:
            fprintf(stderr, "unrecognised option code 0x%x ??\n", c);
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

    if (page_str) {
        if (isalpha(page_str[0])) {
            vnp = sdp_find_mp_by_acron(page_str, opts.transport);
            if (NULL == vnp) {
                vnp = sdp_find_vpd_by_acron(page_str);
                if (NULL == vnp) {
                    fprintf(stderr, "abbreviation matches neither a mode "
                            "page nor a VPD page\n");
                    if (opts.transport < 0)
                        fprintf(stderr, "    perhaps a '--transport=<tn>' "
                                "option is needed\n");
                    if (opts.inquiry) {
                        printf("available VPD pages:\n");
                        enumerate_vpds();
                        return 1;
                    } else {
                        printf("available mode pages");
                        if (opts.transport < 0)
                            printf(":\n");
                        else
                            printf(" (for given transport):\n");
                        enumerate_mps(opts.transport);
                        return 1;
                    }
                    return 1;
                } else {
                    pn = vnp->value;
                    spn = vnp->subvalue;
                    opts.inquiry = 1;
                    pdt = vnp->pdt;
                }
            } else {
                if (opts.inquiry) {
                    fprintf(stderr, "matched mode page acronym but given "
                            "'-i' so expecting a VPD page\n");
                    return 1;
                }
                pn = vnp->value;
                spn = vnp->subvalue;
                pdt = vnp->pdt;
            }
        } else {
            cp = strchr(page_str, ',');
            pn = get_num(page_str);
            if ((pn < 0) || (pn > 255)) {
                fprintf(stderr, "Bad page code value after '-p' "
                        "option\n");
                if (opts.inquiry) {
                    printf("available VPD pages:\n");
                    enumerate_vpds();
                    return 1;
                } else {
                    printf("available mode pages");
                    if (opts.transport < 0)
                        printf(":\n");
                    else
                        printf(" (for given transport):\n");
                    enumerate_mps(opts.transport);
                    return 1;
                }
            }
            if (cp) {
                spn = get_num(cp + 1);
                if ((spn < 0) || (spn > 255)) {
                    fprintf(stderr, "Bad page code value after "
                            "'-p' option\n");
                    return 1;
                }
            } else
                spn = 0;
        }
    }

    if (opts.inquiry) {
        if (set_str || clear_str || get_str || cmd_str || opts.defaults ||
            opts.saved) {
            fprintf(stderr, "'--inquiry' option lists VPD pages so other "
                    "options that are\nconcerned with mode pages are "
                    "inappropriate\n");
            return 1;
        }
        if (pn > 255) {
            fprintf(stderr, "VPD page numbers are from 0 to 255\n");
            return 1;
        }
        if (opts.enumerate) {
            printf("VPD pages:\n");
            enumerate_vpds();
            return 0;
        }
    } else if (cmd_str) {
        if (set_str || clear_str || get_str || opts.defaults || opts.saved) {
            fprintf(stderr, "'--command=' option is not valid with other "
                    "options that are\nconcerned with mode pages\n");
            return 1;
        }
        if (opts.enumerate) {
            printf("Available commands:\n");
            sdp_enumerate_commands();
            return 0;
        }
        scmdp = sdp_build_cmd(cmd_str, &rw);
        if (NULL == scmdp) {
            fprintf(stderr, "'--command=%s' not found\n", cmd_str);
            printf("available commands\n");
            sdp_enumerate_commands();
            return 1;
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
                fprintf(stderr, "'--get=' can't be used with '--set=' "
                        "or " "'--clear='\n");
                return 1;
            }
            if (build_mp_settings(get_str, &mp_settings, opts.transport,
                                  0, 1))
                return 1;
        }
        if (opts.enumerate) {
            if (device_name[0] || set_str || clear_str || get_str ||
                opts.saved)
                /* think about --get= with --enumerate */
                printf("<scsi_device> as well as most options are ignored "
                       "when '--enumerate' is given\n");
            if (pn < 0) {
                if (opts.transport < 0) {
                    if (opts.long_out) {
                        printf("Mode pages (not related to any transport "
                               "protocol):\n");
                        enumerate_mps(-1);
                        printf("\n");
                        printf("Transport protocols:\n");
                        enumerate_transports();
                        if (opts.all) {
                            printf("\n");
                            enumerate_mitems(pn, spn, pdt, opts.transport,
                                             opts.long_out);
                            for (k = 0; k < 16; ++k) {
                                ccp = sdp_get_transport_name(k);
                                if (NULL == ccp)
                                    continue;
                                printf("\n");
                                printf("Mode pages for %s transport "
                                       "protocol:\n", ccp);
                                enumerate_mitems(pn, spn, pdt, k,
                                                 opts.long_out);
                            }
                        } else {
                            for (k = 0; k < 16; ++k) {
                                ccp = sdp_get_transport_name(k);
                                if (NULL == ccp)
                                    continue;
                                printf("\n");
                                printf("Mode pages for %s transport "
                                       "protocol:\n", ccp);
                                enumerate_mps(k);
                            }
                        }
                        printf("\n");
                        printf("Commands:\n");
                        sdp_enumerate_commands();
                    } else {
                        printf("Mode pages:\n");
                        enumerate_mps(-1);
                        if (opts.all)
                            enumerate_mitems(pn, spn, pdt, opts.transport,
                                             opts.long_out);
                    }
                } else {        /* given transport protocol */
                    ccp = sdp_get_transport_name(opts.transport);
                    if (ccp)
                        printf("Mode pages for %s transport protocol:\n",
                               ccp);
                    else
                        printf("Mode pages for transport protocol 0x%x:\n",
                               opts.transport);
                    if (opts.all)
                        enumerate_mitems(pn, spn, pdt, opts.transport,
                                         opts.long_out);
                    else {
                        enumerate_mps(opts.transport);
                    }
                }
            } else      /* given mode page number */ 
                enumerate_mitems(pn, spn, pdt, opts.transport, opts.long_out);
            return 0;
        }

        if (opts.defaults && (set_str || clear_str || get_str)) {
            fprintf(stderr, "'--get=', '--set=' or '--clear=' "
                    "can't be used with '--defaults'\n");
            return 1;
        }

        if (set_str) {
            if (build_mp_settings(set_str, &mp_settings, opts.transport,
                                  0, 0))
                return 1;
        }
        if (clear_str) {
            if (build_mp_settings(clear_str, &mp_settings, opts.transport,
                                  1, 0))
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
    sg_fd = open_and_simple_inquiry(device_name, rw, &pdt, &opts, verbose);
    if (sg_fd < 0) 
        return 1;

    if (opts.inquiry) {
        res = sdp_process_vpd_page(sg_fd, pn, ((spn < 0) ? 0: spn), &opts,
                                   verbose);
        if (res)
            goto err_out;
    } else if (cmd_str && scmdp) {
        res = sdp_process_cmd(sg_fd, scmdp, pdt, &opts, verbose);
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
    res = sg_cmds_close_device(sg_fd);
    if (res < 0) {
        fprintf(stderr, "close error: %s\n", safe_strerror(-sg_fd));
        return 1;
    }
    return ret;
}

#ifdef SDPARM_LINUX
/*     ============ */

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
 * then it is opened with rw setting. The oth_fd is left as is (i.e. it is
 * not closed). sg device node scanning is done "O_RDONLY | O_NONBLOCK".
 * Assumes (and is currently only invoked for) lk 2.4.
 */
static int find_corresponding_sg_fd(int oth_fd, const char * device_name,
                                    int rw, int verbose)
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
        /* re-opening corresponding sg device with given rw setting */
        return open(name, O_NONBLOCK | (rw ? O_RDWR : O_RDONLY));
    }
    else
        return fd;
}

static int map_if_lk24(int sg_fd, const char * device_name, int rw,
                       int verbose)
{
    /* could be lk 2.4 and not using a sg device */
    struct utsname a_uts;
    int two, four;
    int res;

    if (uname(&a_uts) < 0) {
        fprintf(stderr, "uname system call failed, couldn't send "
                "SG_IO ioctl to %s\n", device_name);
        return -1;
    }
    res = sscanf(a_uts.release, "%d.%d", &two, &four);
    if (2 != res) {
        fprintf(stderr, "unable to read uname release\n");
        return -1;
    }
    if (! ((2 == two) && (4 == four))) {
        fprintf(stderr, "unable to access %s, ATA disk?\n",
                device_name);
        return -1;
    }
    return find_corresponding_sg_fd(sg_fd, device_name, rw, verbose);
}

#endif  /* SDPARM_LINUX */


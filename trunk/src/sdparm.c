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

#include "sdparm.h"

/* sdparm is a utility program for the Linux OS SCSI subsystem.
 *
 * This utility fetches various parameters associated with a given
 * SCSI disk (or a disk that uses, or translates the SCSI command
 * set). In some cases these parameters can be changed.
 */

static char * version_str = "0.95 20050920";

#define MAP_TO_SG_NODE

static int find_corresponding_sg_fd(int sg_fd, const char * device_name,
                                    int flags, int verbose);

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
          "[--page=<pg[,spg]>] [--save]\n"
          "                 [--set=<str>] [--six] [--transport=<tn>] "
          "[--verbose]\n"
          "                 [--version] <scsi_device>\n\n"
          "       sdparm    --enumerate [--all] [--inquiry] [--long] "
          "[--page=<pg[,spg]>]\n"
          "                 [--transport=<tn>]\n"
          "  where:\n"
          "      --all | -a            list all known parameters for given "
          "device\n"
          "      --clear=<str> | -c <str>  clear (zero) parameter value(s)\n"
          "      --command=<cmd> | -C <cmd>  perform <cmd> (e.g. 'eject')\n"
          "      --dbd | -B            set DBD bit in mode sense cdb\n"
          "      --defaults | -D       set a mode page to its default "
          "values\n"
          "      --dummy | -d          don't write back modified mode page\n"
          "      --enumerate | -e      list known pages and parameters "
          "(ignore device)\n"
          "      --get=<str> | -g <str>  get (fetch) parameter value(s)\n"
          "      --help | -h           print out usage message\n"
          "      --hex | -H            output in hex rather than name/value "
          "pairs\n"
          "      --inquiry | -i        output INQUIRY VPD page(s) (def: mode "
          "page(s))\n"
          "      --long | -l           add description to parameter output\n"
          "      --page=<pg[,spg]> | -p <pg[,spg]>  page (and optionally "
          "subpage) number\n"
          "                            [or abbrev] to output, change or "
          "enumerate\n"
          "      --save | -S           place mode changes in saved page as "
          "well\n"
          "      --set=<str> | -s <str>  set parameter value(s)\n"
          "      --six | -6            use 6 byte SCSI cdbs (def: 10 byte)\n"
          "      --transport=<tn> | -t <tn>     transport protocol number "
          "[or abbrev]\n"
          "      --verbose | -v        increase verbosity\n"
          "      --version | -V        print version string and exit\n\n"
          "View or change parameters of a SCSI disk (or other device)\n"
          );
}

static int get_mp_len(unsigned char * mp)
{
    return (mp[0] & 0x40) ? ((mp[2] << 8) + mp[3] + 4) : (mp[1] + 2);
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

static const struct sdparm_values_name_t *
        get_mode_detail(int page_num, int subpage_num, int pdt,
                        int transp_proto)
{
    const struct sdparm_values_name_t * vnp;

    if ((transp_proto < 0) || (transp_proto > 15)) {
        for (vnp = sdparm_gen_mode_pg; vnp->acron; ++vnp) {
            if ((page_num == vnp->value) && (subpage_num == vnp->subvalue)) {
                if ((pdt < 0) || (vnp->pdt < 0) || (vnp->pdt == pdt))
                    return vnp;
            }
        }
        return NULL;
    }
    for (vnp = sdparm_transport_mp[transp_proto].mpage; vnp && vnp->acron;
         ++vnp) {
        if ((page_num == vnp->value) && (subpage_num == vnp->subvalue)) {
            if ((pdt < 0) || (vnp->pdt < 0) || (vnp->pdt == pdt))
                return vnp;
        }
    }
    return NULL;
}

static void get_mode_page_name(int page_num, int subpage_num, int pdt,
                               int transp_proto, int hex, char * bp,
                               int max_b_len)
{
    int len = max_b_len - 1;
    const struct sdparm_values_name_t * vnp;

    if (len < 0)
        return;
    bp[len] = '\0';
    vnp = get_mode_detail(page_num, subpage_num, pdt, transp_proto);
    if (NULL == vnp)
        vnp = get_mode_detail(page_num, subpage_num, -1, transp_proto);
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

static const struct sdparm_values_name_t * find_mp_by_acron(const char * ap,
                                                            int transp_proto)
{
    const struct sdparm_values_name_t * vnp;

    if ((transp_proto < 0) || (transp_proto > 15)) {
        for (vnp = sdparm_gen_mode_pg; vnp->acron; ++vnp) {
            if (0 == strncmp(vnp->acron, ap, 4))
                return vnp;
        }
        return NULL;
    }
    for (vnp = sdparm_transport_mp[transp_proto].mpage; vnp && vnp->acron;
         ++vnp) {
        if (0 == strncmp(vnp->acron, ap, 4))
            return vnp;
    }
    return NULL;
}

static void enumerate_vpds()
{
    const struct sdparm_values_name_t * vnp;

    for (vnp = sdparm_vpd_pg; vnp->acron; ++vnp) {
        if (vnp->name)
            printf("  %-4s 0x%02x      %s\n", vnp->acron, vnp->value,
                   vnp->name);
    }
}

static const char * get_vpd_name(int page_num)
{
    const struct sdparm_values_name_t * vnp;

    for (vnp = sdparm_vpd_pg; vnp->acron; ++vnp) {
        if (page_num == vnp->value)
            return vnp->name;
    }
    return NULL;
}

static const struct sdparm_values_name_t * find_vpd_by_acron(const char * ap)
{
    const struct sdparm_values_name_t * vnp;

    for (vnp = sdparm_vpd_pg; vnp->acron; ++vnp) {
        if (0 == strncmp(vnp->acron, ap, 3))
            return vnp;
    }
    return NULL;
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

static const char * get_transport_name(int proto_num)
{
    const struct sdparm_values_name_t * vnp;

    for (vnp = sdparm_transport_id; vnp->acron; ++vnp) {
        if (proto_num == vnp->value)
            return vnp->name;
    }
    return NULL;
}

static const struct sdparm_values_name_t *
                 find_transport_by_acron(const char * ap)
{
    const struct sdparm_values_name_t * vnp;

    for (vnp = sdparm_transport_id; vnp->acron; ++vnp) {
        if (0 == strncmp(vnp->acron, ap, 3))
            return vnp;
    }
    return NULL;
}


static void enumerate_mitems(int pn, int spn, int pdt, int transp_proto)
{
    int t_pn, t_spn, t_pdt;
    const struct sdparm_mode_page_item * mpi;
    char buff[128];
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
            get_mode_page_name(t_pn, t_spn, t_pdt, transp_proto, 1, buff,
                               sizeof(buff));
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
        get_mode_page_name(pn, spn, pdt, transp_proto, 1, buff,
                           sizeof(buff));
        fprintf(stderr, "%s mode page: no items found\n", buff);
    }
}

static const struct sdparm_mode_page_item *
                 find_mitem_by_acron(const char * ap, int * from,
                                     int transp_proto)
{
    int k = 0;
    const struct sdparm_mode_page_item * mpi;

    if (from) {
        k = *from;
        if (k < 0)
            k = 0;
    }
    if ((transp_proto < 0) || (transp_proto > 15))
        mpi = sdparm_mitem_arr;
    else {
        mpi = sdparm_transport_mp[transp_proto].mitem;
        if (NULL == mpi)
            return NULL;
    }
    for (mpi += k; mpi->acron; ++k, ++mpi) {
        if (0 == strcmp(mpi->acron, ap))
            break;
    }
    if (NULL == mpi->acron)
        mpi = NULL;
    if (from)
        *from = (mpi ? (k + 1) : k);
    return mpi;
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

        printf("  pdt=%d byte_off=0x%x bit_off=%d num_bits=%d  val=%lld",
               mpip->pdt, mpip->start_byte, mpip->start_bit, mpip->num_bits,
               mps->it_vals[k].val);
        if (mpip->acron)
            printf("  acronym: %s\n", mpip->acron);
        else
            printf("\n");
    }
}

static unsigned long long 
        get_big_endian(const unsigned char * from, int start_bit,
                       int num_bits)
{
    unsigned long long res;
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

static void set_big_endian(unsigned long long val, unsigned char * to,
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

static unsigned long long
        mp_get_value(const struct sdparm_mode_page_item *mpi,
                     const unsigned char * mp)
{
    return get_big_endian(mp + mpi->start_byte, mpi->start_bit,
                          mpi->num_bits);
}

static unsigned long long
        mp_get_value_check(const struct sdparm_mode_page_item *mpi,
                           const unsigned char * mp, int * all_set)
{
    unsigned long long res;

    res = get_big_endian(mp + mpi->start_byte, mpi->start_bit,
                         mpi->num_bits);
    if (all_set) {
        if ((16 == mpi->num_bits) && (0xffff == res))
            *all_set = 1;
        else if ((32 == mpi->num_bits) && (0xffffffff == res))
            *all_set = 1;
        else if ((64 == mpi->num_bits) && (0xffffffffffffffffULL == res))
            *all_set = 1;
        else
            *all_set = 0;
    }
    return res;
}

static void mp_set_value(unsigned long long val,
                         struct sdparm_mode_page_item *mpi,
                         unsigned char * mp)
{
    set_big_endian(val, mp + mpi->start_byte, mpi->start_bit, mpi->num_bits);
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
    u = mp_get_value_check(mpi, cur_mp, &all_set);
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
                   (mp_get_value(mpi, cha_mp) ? "y" : "n"));
            sep = 1;
        }
        if (def_mp && (smask & 4)) {
            all_set = 0;
            u = mp_get_value_check(mpi, def_mp, &all_set);
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
            u = mp_get_value_check(mpi, sav_mp, &all_set);
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
                get_mode_page_name(pn, spn, pdt, opts->transport, opts->hex,
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
                len = get_mp_len(cur_mp);
                get_mode_page_name(pn, spn, pdt, opts->transport, opts->hex,
                                   buff, sizeof(buff));
                printf("%s ", buff);
                if (verbose) {
                    if (spn)
                        printf("[0x%x,0x%x] ", pn, spn);
                    else
                        printf("[0x%x] ", pn);
                }
                if (opts->long_out)
                    printf("[PS=%d] ", !!(cur_mp[0] & 0x80));
                printf("mode page:\n");
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
                    get_mode_page_name(pn, spn, pdt, opts->transport,
                                       opts->hex, buff, sizeof(buff));
                    fprintf(stderr, ">> %s mode %spage ", buff,
                            (spn ? "sub" : ""));
                    if (verbose > 1) {
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
                    fprintf(stderr, "error (res=%d) in ", res);
                get_mode_page_name(pn, spn, mpi->pdt, opts->transport,
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
        len = (smask & 1) ? get_mp_len(cur_mp) : 0;
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
                    u = mp_get_value(mpi, cur_mp);
                    printf("0x%02llx ", u);
                } else
                    printf("-    ");
                if (smask & 2) {
                    u = mp_get_value(mpi, cha_mp);
                    printf("0x%02llx ", u);
                } else
                    printf("-    ");
                if (smask & 4) {
                    u = mp_get_value(mpi, def_mp);
                    printf("0x%02llx ", u);
                } else
                    printf("-    ");
                if (smask & 8) {
                    u = mp_get_value(mpi, sav_mp);
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
                    u = mp_get_value(mpi, cur_mp);
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
                    u = mp_get_value(mpi, cur_mp);
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
    if (opts->mode_6)
        res = sg_ll_mode_sense6(sg_fd, opts->dbd, 0 /*current */,
                                mps->page_num, mps->subpage_num, mdpg,
                                4, 1, verbose);
    else
        res = sg_ll_mode_sense10(sg_fd, 0 /* llbaa */, opts->dbd,
                                 0 /* current */, mps->page_num,
                                 mps->subpage_num, mdpg, 4, 1, verbose);
    if (0 != res) {
        get_mode_page_name(mps->page_num, mps->subpage_num, pdt,
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
    if (opts->mode_6)
        res = sg_ll_mode_sense6(sg_fd, opts->dbd, 0 /*current */,
                                mps->page_num, mps->subpage_num, mdpg,
                                md_len, 1, verbose);
    else
        res = sg_ll_mode_sense10(sg_fd, 0 /* llbaa */, opts->dbd,
                                 0 /* current */, mps->page_num,
                                 mps->subpage_num, mdpg, md_len, 1, verbose);
    if (0 != res) {
        get_mode_page_name(mps->page_num, mps->subpage_num, pdt,
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
    len = get_mp_len(mdpg + off);
    mdpg[0] = 0;        /* mode data length reserved for mode select */
    if (! opts->mode_6)
        mdpg[1] = 0;    /* mode data length reserved for mode select */

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
        get_mode_page_name(mps->page_num, mps->subpage_num, pdt,
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
    if (opts->mode_6)
        ret = sg_ll_mode_sense6(sg_fd, opts->dbd, 0 /*current */, pn,
                                spn, mdp, 4, 1, verbose);
    else
        ret = sg_ll_mode_sense10(sg_fd, 0 /* llbaa */, opts->dbd,
                                 0 /* current */, pn, spn, mdp, 4, 1,
                                 verbose);
    if (0 != ret) {
        get_mode_page_name(pn, spn, -1, opts->transport, 0, buff,
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
    if (opts->mode_6)
        ret = sg_ll_mode_sense6(sg_fd, opts->dbd, 0 /*current */, pn,
                                spn, mdp, md_len, 1, verbose);
    else
        ret = sg_ll_mode_sense10(sg_fd, 0 /* llbaa */, opts->dbd,
                                 0 /* current */, pn, spn, mdp, md_len, 1,
                                 verbose);
    if (0 != ret) {
        get_mode_page_name(pn, spn, -1, opts->transport, 0, buff,
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
        get_mode_page_name(pn, spn, -1, opts->transport, 0, buff,
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
        get_mode_page_name(pn, spn, pdt, opts->transport, 0, buff,
                           sizeof(buff));
        fprintf(stderr, "%s mode page length=%d too long, perhaps "
                "try '--flexible'\n", buff, rep_len);
    }
    if ((smask & 1)) {
        len = get_mp_len(cur_mp);
        if ((smask & 4)) {
            return set_def_mode_page(sg_fd, pn, spn, def_mp, len, opts,
                                     verbose);
        }
        else {
            get_mode_page_name(pn, spn, pdt, opts->transport, 0, buff,
                               sizeof(buff));
            fprintf(stderr, ">> %s mode page (default) not supported\n",
                    buff);
            return -1;
        }
    } else {
        get_mode_page_name(pn, spn, pdt, opts->transport, 0, buff,
                           sizeof(buff));
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

static long long get_llnum(const char * buf)
{
    int res;
    long long num;
    unsigned long long unum;

    if ((NULL == buf) || ('\0' == buf[0]))
        return -1;
    if (('0' == buf[0]) && (('x' == buf[1]) || ('X' == buf[1]))) {
        res = sscanf(buf + 2, "%llx", &unum);
        num = unum;
    } else
        res = sscanf(buf, "%lld", &num);
    return (1 == res) ? num : -1;
}

static int build_mp_settings(const char * arg,
                             struct sdparm_mode_page_settings * mps,
                             int transp_proto, int clear, int get)
{
    int len, b_sz, num, from, cont;
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
        if (isalpha(buff[0]) || (isdigit(buff[0]) && ('_' == buff[1]))) {
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
                    mpi = find_mitem_by_acron(acron, &from, transp_proto);
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
                    mpi = find_mitem_by_acron(acron, &from, transp_proto);
                    if (NULL == mpi) {
                        if (cont) {
                            fprintf(stderr, "mode page of acronym: %s "
                                    "[0x%x,0x%x] doesn't match prior\n",
                                    acron, prev_mpi->page_num,
                                    prev_mpi->subpage_num);
                            fprintf(stderr, "    mode page: 0x%x,0x%x\n",
                                    mps->page_num, mps->subpage_num);
                            fprintf(stderr, "For '--set' and '--clear' all "
                                    "parameters must be in the same mode "
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
                fprintf(stderr, "unable to decode: %s\n", buff);
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
                    ivp->val = get_llnum(vb);
                    if (-1 == ivp->val) {
                        fprintf(stderr, "unable to decode "
                                "byte_off:bit_off:num_bits value\n");
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

/* These are target port, device server (i.e. target) and lu identifiers */
static int decode_dev_ids(const char * print_if_found, unsigned char * buff,
                          int len, int match_assoc, int long_out)
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
            printf("  Descriptor number %d, "
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
            printf("    transport: %s\n", sdparm_transport_proto_arr[p_id]);
        printf("    id_type: %s,  code_set: %s\n", sdparm_id_type_arr[id_type],
               sdparm_code_set_arr[c_set]);
        /* printf("    associated with the %s\n", sdparm_assoc_arr[assoc]); */
        switch (id_type) {
        case 0: /* vendor specific */
            dStrHex((const char *)ip, i_len, 0);
            break;
        case 1: /* T10 vendor identification */
            printf("      vendor id: %.8s\n", ip);
            if (i_len > 8)
                printf("      vendor specific: %.*s\n", i_len - 8, ip + 8);
            break;
        case 2: /* EUI-64 based */
            if (! long_out) {
                if ((8 != i_len) && (12 != i_len) && (16 != i_len)) {
                    printf("      << expect 8, 12 and 16 byte ids, got "
                           "%d>>\n", i_len);
                }
                printf("      [0x");
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

static int decode_mode_policy_vpd(unsigned char * buff, int len)
{
    int k, bump;
    unsigned char * ucp;

    if (len < 4) {
        fprintf(stderr, "Mode page policy VPD page length too short=%d\n",
                len);
        return -1;
    }
    len -= 4;
    ucp = buff + 4;
    for (k = 0; k < len; k += bump, ucp += bump) {
        bump = 4;
        if ((k + bump) > len) {
            fprintf(stderr, "Mode page policy VPD page, short "
                    "descriptor length=%d, left=%d\n", bump, (len - k));
            return -1;
        }
        printf("  Policy page code: 0x%x", (ucp[0] & 0x3f));
        if (ucp[1])
            printf(",  subpage code: 0x%x\n", ucp[1]);
        else
            printf("\n");
        printf("    MLUS=%d,  Policy: %s\n", !!(ucp[2] & 0x80),
               sdparm_mode_page_policy_arr[ucp[2] & 0x3]);
    }
    return 0;
}

static int decode_man_net_vpd(unsigned char * buff, int len)
{
    int k, bump, na_len;
    unsigned char * ucp;

    if (len < 4) {
        fprintf(stderr, "Management network addresses VPD page length too "
                "short=%d\n", len);
        return -1;
    }
    len -= 4;
    ucp = buff + 4;
    for (k = 0; k < len; k += bump, ucp += bump) {
        printf("  %s, Service type: %s\n",
               sdparm_assoc_arr[(ucp[0] >> 5) & 0x3],
               sdparm_network_service_type_arr[ucp[0] & 0x1f]);
        na_len = (ucp[2] << 8) + ucp[3];
        bump = 4 + na_len;
        if ((k + bump) > len) {
            fprintf(stderr, "Management network addresses VPD page, short "
                    "descriptor length=%d, left=%d\n", bump, (len - k));
            return -1;
        }
        if (na_len > 0) {
            printf("    %s\n", ucp + 4);
        }
    }
    return 0;
}

static int decode_scsi_ports_vpd(unsigned char * buff, int len,
                                 int long_out)
{
    int k, bump, rel_port, ip_tid_len, tpd_len, res;
    unsigned char * ucp;

    if (len < 4) {
        fprintf(stderr, "SCSI Ports VPD page length too short=%d\n", len);
        return -1;
    }
    len -= 4;
    ucp = buff + 4;
    for (k = 0; k < len; k += bump, ucp += bump) {
        rel_port = (ucp[2] << 8) + ucp[3];
        printf("Relative port=%d\n", rel_port);
        ip_tid_len = (ucp[6] << 8) + ucp[7];
        bump = 8 + ip_tid_len;
        if ((k + bump) > len) {
            fprintf(stderr, "SCSI Ports VPD page, short descriptor "
                    "length=%d, left=%d\n", bump, (len - k));
            return -1;
        }
        if (ip_tid_len > 0) {
            /* 
             * SCSI devices that are both target and initiator are rare.
             * Only target devices can receive this command, so if they
             * are also initiators then print out the "Initiator port
             * transport id" in hex. sg_inq in sg3_utils decodes it.
             */
            printf(" Initiator port transport id:\n");
            dStrHex((const char *)(ucp + 8), ip_tid_len, 1);
        }
        tpd_len = (ucp[bump + 2] << 8) + ucp[bump + 3];
        if ((k + bump + tpd_len + 4) > len) {
            fprintf(stderr, "SCSI Ports VPD page, short descriptor(tgt) "
                    "length=%d, left=%d\n", bump, (len - k));
            return -1;
        }
        if (tpd_len > 0) {
            printf(" Target port descriptor(s):\n");
            res = decode_dev_ids(NULL, ucp + bump + 4, tpd_len,
                                 VPD_ASSOC_TPORT, long_out);
            if (res)
                return res;
        }
        bump += tpd_len + 4;
    }
    return 0;
}

static int decode_ext_inq_vpd(unsigned char * buff, int len)
{
    if (len < 7) {
        fprintf(stderr, "Extended INQUIRY data VPD page length too "
                "short=%d\n", len);
        return -1;
    }
    printf("  RTO: %d  GRD_CHK: %d  APP_CHK: %d  REF_CHK: %d\n",
           !!(buff[4] & 0x8), !!(buff[4] & 0x4), !!(buff[4] & 0x2),
           !!(buff[4] & 0x1));
    printf("  GRP_SUP: %d  PRIOR_SUP: %d  HEADSUP: %d  ORDSUP: %d  "
           "SIMPSUP: %d\n", !!(buff[5] & 0x10), !!(buff[5] & 0x8),
           !!(buff[5] & 0x4), !!(buff[5] & 0x2), !!(buff[5] & 0x1));
    printf("  NV_SUP: %d  V_SUP: %d\n", !!(buff[6] & 0x2), !!(buff[6] & 0x1));
    return 0;
}

static int decode_ata_info_vpd(unsigned char * buff, int len, int do_hex)
{
    char b[32];

    if (len < 36) {
        fprintf(stderr, "ATA information VPD page length too "
                "short=%d\n", len);
        return -1;
    }
    memcpy(b, buff + 8, 8);
    b[8] = '\0';
    printf("  SAT Vendor identification: %s\n", b);
    memcpy(b, buff + 16, 16);
    b[16] = '\0';
    printf("  SAT Product identification: %s\n", b);
    memcpy(b, buff + 32, 4);
    b[4] = '\0';
    printf("  SAT Product revision level: %s\n", b);
    if (len < 56)
        return -1;;
    printf("  Signature (20 bytes):\n");
    dStrHex((const char *)buff + 36, 20, 0);
    if (len < 60)
        return -1;;
    if (0xec == buff[56])
        printf("  ATA command IDENTIFY DEVICE got following response:\n");
    else if (0xa1 == buff[56])
        printf("  ATA command IDENTIFY PACKET DEVICE got following "
               "response:\n");
    else
        printf("  ATA command 0x%x got following response:\n",
               (unsigned int)buff[56]);
    if (len < 572)
        return -1;;
    if (do_hex)	/* here for '-HH' option */
        dStrHex((const char *)(buff + 60), 512, 0);
    else
        dWordHex((const unsigned short *)(buff + 60), 256, 0,
                 sg_is_big_endian());
    return 0;
}

static int decode_block_limits_vpd(unsigned char * buff, int len)
{
    unsigned int u;

    if (len < 16) {
        fprintf(stderr, "Block limits VPD page length too "
                "short=%d\n", len);
        return -1;
    }
    u = (buff[6] << 8) | buff[7];
    printf("  Optimal transfer length granularity: %u blocks\n", u);
    u = (buff[8] << 24) | (buff[9] << 16) | (buff[10] << 8) |
        buff[11];
    printf("  Maximum transfer length: %u blocks\n", u);
    u = (buff[12] << 24) | (buff[13] << 16) | (buff[14] << 8) |
        buff[15];
    printf("  Optimal transfer length: %u blocks\n", u);
    return 0;
}

static int process_vpd_page(int sg_fd, int pn,
                            const struct sdparm_opt_coll * opts,
                            int verbose)
{
    int res, len, k, verb;
    unsigned char b[VPD_ATA_INFO_RESP_LEN];
    int sz;
    const char * cp;

    verb = (verbose > 0) ? verbose - 1 : 0;
    sz = sizeof(b);
    memset(b, 0, sz);
    if (pn < 0) {
        if (opts->all)
            pn = VPD_SUPPORTED_VPDS;  /* if '--all' list supported vpds */
        else
            pn = VPD_DEVICE_ID;  /* default to device identification page */
    }
    sz = (VPD_ATA_INFO == pn) ? VPD_ATA_INFO_RESP_LEN : DEF_INQ_RESP_LEN;
    res = sg_ll_inquiry(sg_fd, 0, 1, pn, b, sz, 0, verb);
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
    case VPD_ATA_INFO:
        if (b[1] != pn)
            goto dumb_inq;
        len = (b[2] << 8) + b[3];
        if (len > sz) {
            fprintf(stderr, "Response to ATA information VPD page "
                    "truncated\n");
            len = sz;
        }
        if (opts->long_out)
            printf("ATA information [0x89] VPD page:\n");
        else
            printf("ATA information VPD page:\n");
        if (opts->hex && (2 != opts->hex)) {
            dStrHex((const char *)b, len + 4, 0);
            return 0;
        }
        res = decode_ata_info_vpd(b, len + 4, opts->hex);
        if (res)
            return res;
        break;
    case VPD_BLOCK_LIMITS:
        if (b[1] != pn)
            goto dumb_inq;
        len = (b[2] << 8) + b[3];
        if (len > sz) {
            fprintf(stderr, "Response to Blocks limits VPD page "
                    "truncated\n");
            len = sz;
        }
        if (opts->long_out)
            printf("Block limits [0xb0] VPD page:\n");
        else
            printf("Block limits VPD page:\n");
        if (opts->hex) {
            dStrHex((const char *)b, len + 4, 0);
            return 0;
        }
        res = decode_block_limits_vpd(b, len + 4);
        if (res)
            return res;
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
        res = decode_dev_ids(sdparm_assoc_arr[VPD_ASSOC_LU], b + 4, len,
                             VPD_ASSOC_LU, opts->long_out);
        if (res)
            return res;
        res = decode_dev_ids(sdparm_assoc_arr[VPD_ASSOC_TPORT], b + 4, len,
                             VPD_ASSOC_TPORT, opts->long_out);
        if (res)
            return res;
        res = decode_dev_ids(sdparm_assoc_arr[VPD_ASSOC_TDEVICE], b + 4, len,
                             VPD_ASSOC_TDEVICE, opts->long_out);
        if (res)
            return res;
        break;
    case VPD_EXT_INQ:
        if (b[1] != pn)
            goto dumb_inq;
        len = (b[2] << 8) + b[3];
        if (len > sz) {
            fprintf(stderr, "Response to Extended inquiry data VPD page "
                    "truncated\n");
            len = sz;
        }
        if (opts->long_out)
            printf("Extended inquiry data [0x86] VPD page:\n");
        else
            printf("Extended inquiry data VPD page:\n");
        if (opts->hex) {
            dStrHex((const char *)b, len + 4, 0);
            return 0;
        }
        res = decode_ext_inq_vpd(b, len + 4);
        if (res)
            return res;
        break;
    case VPD_MAN_NET_ADDR:
        if (b[1] != pn)
            goto dumb_inq;
        len = (b[2] << 8) + b[3];
        if (len > sz) {
            fprintf(stderr, "Response to Management network addresses VPD "
                    "page truncated\n");
            len = sz;
        }
        if (opts->long_out)
            printf("Management network addresses [0x85] VPD page:\n");
        else
            printf("Management network addresses VPD page:\n");
        if (opts->hex) {
            dStrHex((const char *)b, len + 4, 0);
            return 0;
        }
        res = decode_man_net_vpd(b, len + 4);
        if (res)
            return res;
        break;
    case VPD_MODE_PG_POLICY:
        if (b[1] != pn)
            goto dumb_inq;
        len = (b[2] << 8) + b[3];
        if (len > sz) {
            fprintf(stderr, "Response to Mode page policy VPD page "
                    "truncated\n");
            len = sz;
        }
        if (opts->long_out)
            printf("Mode page policy [0x87] VPD page:\n");
        else
            printf("mode page policy VPD page:\n");
        if (opts->hex) {
            dStrHex((const char *)b, len + 4, 0);
            return 0;
        }
        res = decode_mode_policy_vpd(b, len + 4);
        if (res)
            return res;
        break;
    case VPD_SCSI_PORTS:
        if (b[1] != pn)
            goto dumb_inq;
        len = (b[2] << 8) + b[3];
        if (len > sz) {
            fprintf(stderr, "Response to SCSI Ports VPD page "
                    "truncated\n");
            len = sz;
        }
        if (opts->long_out)
            printf("SCSI Ports [0x88] VPD page:\n");
        else
            printf("SCSI Ports VPD page:\n");
        if (opts->hex) {
            dStrHex((const char *)b, len + 4, 0);
            return 0;
        }
        res = decode_scsi_ports_vpd(b, len + 4, opts->long_out);
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

static const char * get_ansi_version_str(int version, char * buff,
                                         int buff_len)
{
    version &= 0x7;
    buff[buff_len - 1] = '\0';
    strncpy(buff, sdparm_ansi_version_arr[version], buff_len - 1);
    return buff;
}

static void enumerate_commands()
{
    const struct sdparm_command * scmdp;

    for (scmdp = sdparm_command_arr; scmdp->name; ++scmdp)
        printf("  %s\n", scmdp->name);
}

static const struct sdparm_command *
        build_cmd(const char * cmd_str, int * rwp)
{
    const struct sdparm_command * scmdp;

    for (scmdp = sdparm_command_arr; scmdp->name; ++scmdp) {
        if (0 == strcmp(scmdp->name, cmd_str))
            break;
    }
    if (scmdp->name) {
        if (rwp) {
            if (CMD_READY  == scmdp->cmd_num)
                *rwp = 0;
             else
                *rwp = 1;
        }
        return scmdp;
    } else
        return NULL;
}

static int process_cmd(int sg_fd, const struct sdparm_command * scmdp,
                       int pdt, const struct sdparm_opt_coll * opts,
                       int verbose)
{
    int res;

    if (! (opts->flexible ||
          (CMD_READY == scmdp->cmd_num) ||
          (0 == pdt) || (5 == pdt)) ) {
        fprintf(stderr, "this command only valid on a disk or cd/dvd; "
                "use '--flexible' to override\n");
        return 1;

    }

    switch (scmdp->cmd_num)
    {
    case CMD_READY:
        res = sg_ll_test_unit_ready(sg_fd, 0, 0, verbose);
        if (0 == res)
            printf("Ready\n");
        else {
            printf("Not ready\n");
            res = 1;
        }
        break;
    case CMD_START:
        res = sg_ll_start_stop_unit(sg_fd, 0, 0, 0, 1, 1, verbose);
        break;
    case CMD_STOP:
        res = sg_ll_start_stop_unit(sg_fd, 0, 0, 0, 0, 1, verbose);
        break;
    case CMD_LOAD:
        res = sg_ll_start_stop_unit(sg_fd, 0, 0, 1, 1, 1, verbose);
        break;
    case CMD_EJECT:
        res = sg_ll_start_stop_unit(sg_fd, 0, 0, 1, 0, 1, verbose);
        break;
    case CMD_UNLOCK:
        res = sg_ll_prevent_allow(sg_fd, 0, 1, verbose);
        break;
    default:
        fprintf(stderr, "unknown cmd number [%d]\n", scmdp->cmd_num);
        return 1;
    }
    return res;
}

static int open_and_simple_inquiry(const char * device_name, int flags,
                                   int * pdt,
                                   const struct sdparm_opt_coll * opts,
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
            if ((0 != *pdt) && (5 != *pdt))
                fprintf(stderr, "     note: given %s rather than disk "
                        "or cd/dvd type\n", sdparm_scsi_ptype_strs[l_pdt]);
        }
    }
    return sg_fd;

err_out:
    close(sg_fd);
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
        vnp = get_mode_detail(pn, spn, pdt, opts->transport);
        if (NULL == vnp)
            vnp = get_mode_detail(pn, spn, -1, opts->transport);
        if (vnp && vnp->name && (vnp->pdt >= 0) && (pdt != vnp->pdt)) {
            fprintf(stderr, ">> Warning: %s mode page associated with "
                    "peripheral\n", vnp->name);
            fprintf(stderr, "   device type 0x%x but device pdt is "
                    "0x%x\n", vnp->pdt, pdt);
        }
    }
    if (opts->defaults) {
        res = set_mp_defaults(sg_fd, pn, spn, pdt, opts, verbose);
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
    int sg_fd, res, c, pdt, flags, k;
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

        c = getopt_long(argc, argv, "6aBc:C:dDefg:hHilp:s:St:vV",
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
                vnp = find_transport_by_acron(optarg);
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
            vnp = find_mp_by_acron(page_str, opts.transport);
            if (NULL == vnp) {
                vnp = find_vpd_by_acron(page_str);
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
        if ((pn > 255) || (spn > 0)) {
            fprintf(stderr, "VPD page numbers are from 0 to 255 with no "
                    "subpages\n");
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
        scmdp = build_cmd(cmd_str, &rw);
        if (NULL == scmdp) {
            fprintf(stderr, "'--command=%s' not found\n", cmd_str);
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
                            enumerate_mitems(pn, spn, pdt, opts.transport);
                            for (k = 0; k < 16; ++k) {
                                ccp = get_transport_name(k);
                                if (NULL == ccp)
                                    continue;
                                printf("\n");
                                printf("Mode pages for %s transport "
                                       "protocol:\n", ccp);
                                enumerate_mitems(pn, spn, pdt, k);
                            }
                        } else {
                            for (k = 0; k < 16; ++k) {
                                ccp = get_transport_name(k);
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
                        enumerate_commands();
                    } else {
                        printf("Mode pages:\n");
                        enumerate_mps(-1);
                        if (opts.all)
                            enumerate_mitems(pn, spn, pdt, opts.transport);
                    }
                } else {        /* given transport protocol */
                    ccp = get_transport_name(opts.transport);
                    if (ccp)
                        printf("Mode pages for %s transport protocol:\n",
                               ccp);
                    else
                        printf("Mode pages for transport protocol 0x%x:\n",
                               opts.transport);
                    if (opts.all)
                        enumerate_mitems(pn, spn, pdt, opts.transport);
                    else {
                        enumerate_mps(opts.transport);
                    }
                }
            } else      /* given mode page number */ 
                enumerate_mitems(pn, spn, pdt, opts.transport);
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
    flags = (O_NONBLOCK | (rw ? O_RDWR : O_RDONLY));
    sg_fd = open_and_simple_inquiry(device_name, flags, &pdt, &opts, verbose);
    if (sg_fd < 0) 
        return 1;

    if (opts.inquiry) {
        res = process_vpd_page(sg_fd, pn, &opts, verbose);
        if (res)
            goto err_out;
    } else if (cmd_str && scmdp) {
        res = process_cmd(sg_fd, scmdp, pdt, &opts, verbose);
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

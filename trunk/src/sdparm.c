/*
 * Copyright (c) 2005-2016 Douglas Gilbert.
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
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_GETOPT_LONG
#include <getopt.h>
#else
#include "port_getopt.h"
#endif

#ifdef SG_LIB_LINUX
#include <sys/ioctl.h>
#include <sys/utsname.h>

/* Following needed for lk 2.4 series; may be dropped in future */
#include <scsi/scsi.h>
#include <scsi/sg.h>

static int map_if_lk24(int sg_fd, const char * device_name, int rw,
                       int verbose);

#endif  /* SG_LIB_LINUX */

#include "sg_lib.h"
#include "sg_cmds_basic.h"
#include "sg_unaligned.h"
#include "sg_pr2serr.h"
#include "sdparm.h"

static const char * version_str = "1.10 20160202 [svn: r276]";


#define MAX_DEV_NAMES 256
#define INHEX_BUFF_SZ 4096

static unsigned char inhex_buff[INHEX_BUFF_SZ];


static struct option long_options[] = {
    {"six", no_argument, 0, '6'},
    {"all", no_argument, 0, 'a'},
    {"dbd", no_argument, 0, 'B'},
    {"clear", required_argument, 0, 'c'},
    {"command", required_argument, 0, 'C'},
    {"defaults", no_argument, 0, 'D'},
    {"dummy", no_argument, 0, 'd'},
    {"enumerate", no_argument, 0, 'e'},
    {"flexible", no_argument, 0, 'f'},
    {"get", required_argument, 0, 'g'},
    {"help", no_argument, 0, 'h'},
    {"hex", no_argument, 0, 'H'},
    {"inquiry", no_argument, 0, 'i'},
    {"inhex", required_argument, 0, 'I'},
    {"long", no_argument, 0, 'l'},
    {"num-desc", no_argument, 0, 'n'},
    {"page", required_argument, 0, 'p'},
    {"pdt", required_argument, 0, 'P'},
    {"quiet", no_argument, 0, 'q'},
    {"readonly", no_argument, 0, 'r'},
    {"set", required_argument, 0, 's'},
    {"save", no_argument, 0, 'S'},
    {"transport", required_argument, 0, 't'},
    {"vendor", required_argument, 0, 'M'},
    {"verbose", no_argument, 0, 'v'},
    {"version", no_argument, 0, 'V'},
#ifdef SG_LIB_WIN32
    {"wscan", no_argument, 0, 'w'},
#endif
    {0, 0, 0, 0},
};


static void
usage(int do_help)
{
    if (1 != do_help)
        goto secondary_help;
    pr2serr("Usage: sdparm [--all] [--dbd] [--flexible] [--get=STR] [--hex] "
            "[--long]\n"
            "              [--num-desc] [--page=PG[,SPG]] [--quiet] "
            "[--readonly]\n"
            "              [--six] [--transport=TN] [--vendor=VN] "
            "[--verbose]\n"
            "              DEVICE [DEVICE...]\n"
            "       sdparm [--clear=STR] [--defaults] [--dummy] "
            "[--flexible]\n"
            "              [--page=PG[,SPG]] [--quiet] [--readonly] "
            "[--save] [--set=STR]\n"
            "              [--six] [--transport=TN] [--vendor=VN] "
            "[--verbose]\n"
            "              DEVICE [DEVICE...]\n\n"
            "  where mode page read (1st usage) and change (2nd usage) "
            "options are:\n"
            "    --all | -a            list all known fields for given "
            "DEVICE\n"
            "    --clear=STR | -c STR    clear (zero) field value(s)\n"
            "    --dbd | -B            set DBD bit in mode sense cdb\n"
            "    --defaults | -D       set a mode page to its default "
            "values\n"
            "    --dummy | -d          don't write back modified mode page\n"
            "    --flexible | -f       compensate for common errors, "
            "relax some checks\n"
            "    --get=STR | -g STR    get (fetch) field value(s)\n"
            "    --hex | -H            output in hex rather than name/value "
            "pairs\n"
            "    --long | -l           add description to field output\n"
            "    --num-desc | -n       report number of mode page "
            "descriptors\n"
            "    --page=PG[,SPG] | -p PG[,SPG]    page (and optionally "
            "subpage) number\n"
            "                          [or abbrev] to output, change or "
            "enumerate\n"
            "    --quiet | -q          suppress DEVICE vendor/product/"
            "revision string line\n"
            "    --readonly | -r       force read-only open of DEVICE (def: "
            "depends\n"
            "                          on operation). Mainly for ATA disks\n"
            "    --save | -S           place mode changes in saved page as "
            "well\n"
            "    --set=STR | -s STR    set field value(s)\n"
            "    --six | -6            use 6 byte SCSI mode cdbs (def: 10 "
            "byte)\n"
            "    --transport=TN | -t TN    transport protocol number "
            "[or abbrev]\n"
            "    --vendor=VN | -M VN    vendor (manufacturer) number "
            "[or abbrev]\n"
            "    --verbose | -v        increase verbosity\n"
            "\nView or change SCSI mode page fields (e.g. of a disk or "
            "CD/DVD drive).\nSTR can be <acronym>[=val] or "
            "<start_byte>:<start_bit>:<num_bits>[=val].\nUse '-h' or "
            "'--help' twice for help on other usages including "
            "executing\nsome simple commands, reading and decoding VPD "
            "pages, enumerating internal\ntables of mode and VPD pages, and "
            "decoding response data supplied in a\nfile or stdin (rather "
            "than from a DEVICE).\n"
           );
    return;
secondary_help:
    pr2serr("Further usages of the sdparm utility:\n"
            "       sdparm --command=CMD [-hex] [--readonly] [--verbose]\n"
            "              DEVICE [DEVICE...]\n"
            "       sdparm --inquiry [--all] [--flexible] [--hex]\n"
            "              [--page=PG[,SPG]] [--quiet] [--readonly] "
            "[--transport=TN]\n"
            "              [--vendor=VN] [--verbose] DEVICE [DEVICE...]\n");
    pr2serr("       sdparm --enumerate [--all] [--inquiry] [--long] "
            "[--page=PG[,SPG]]\n"
            "              [--transport=TN] [--vendor=VN]\n"
            "       sdparm --inhex=FN [--all] [--flexible] [--hex] "
            "[--inquiry]\n"
            "              [--long] [--pdt=PDT] [--six] [--transport=TN] "
            "[--vendor=VN]\n"
            "       sdparm --wscan [--verbose]\n"
            "       sdparm [--help] [--version]\n\n"
            "  where the additional options are:\n"
            "    --command=CMD | -C CMD    perform CMD (e.g. 'eject')\n"
            "    --enumerate | -e      list known pages and fields "
            "(ignore DEVICE)\n"
            "    --help | -h           print out usage message\n"
            "    --inhex=FN|-I FN      read ASCII hex from file FN instead "
            "of DEVICE;\n"
            "                          if used with -HH then read binary "
            "from FN\n"
            "    --inquiry | -i        output INQUIRY VPD page(s) (def: mode "
            "page(s))\n"
            "                          use --page=PG for VPD number (-1 "
            "for std inq)\n"
            "    --pdt=DT|-P DT        peripheral Device Type (e.g. "
            "0->disk)\n"
            "    --version | -V        print version string and exit\n"
            "    --wscan | -w          windows scan for device names\n"
            "\nThe available commands will be listed when a invalid CMD is "
            "given\n(e.g. '--command=xxx'). VPD page(s) are read and decoded "
            "in the\n'--inquiry DEVICE' form. The '--enumerate' form outputs "
            "internal data\nabout mode or VPD pages (and ignores DEVICE if "
            "given). The '--inhex'\nform reads data from the the file FN "
            "(or stdin) and decodes it as a\nmode or VPD page response. The "
            "'--wscan' form is for listing Windows\ndevices and is only "
            "available on Windows machines.\n"
           );
}

/* Read ASCII hex bytes or binary from fname (a file named '-' taken as
 * stdin). If reading ASCII hex then there should be either one entry per
 * line or a comma, space or tab separated list of bytes. If no_space is
 * set then a string of ACSII hex digits is expected, 2 per byte. Everything
 * from and including a '#' on a line is ignored. Returns 0 if ok, or 1 if
 * error. */
static int
f2hex_arr(const char * fname, int as_binary, int no_space,
          unsigned char * mp_arr, int * mp_arr_len, int max_arr_len)
{
    int fn_len, in_len, k, j, m, split_line, fd, has_stdin;
    unsigned int h;
    const char * lcp;
    FILE * fp;
    char line[512];
    char carry_over[4];
    int off = 0;
    struct stat a_stat;

    if ((NULL == fname) || (NULL == mp_arr) || (NULL == mp_arr_len))
        return 1;
    fn_len = strlen(fname);
    if (0 == fn_len)
        return 1;
    has_stdin = ((1 == fn_len) && ('-' == fname[0]));   /* read from stdin */
    if (as_binary) {
        if (has_stdin)
            fd = STDIN_FILENO;
        else {
            fd = open(fname, O_RDONLY);
            if (fd < 0) {
                pr2serr("unable to open binary file %s: %s\n", fname,
                         safe_strerror(errno));
                return 1;
            }
        }
        k = read(fd, mp_arr, max_arr_len);
        if (k <= 0) {
            if (0 == k)
                pr2serr("read 0 bytes from binary file %s\n", fname);
            else
                pr2serr("read from binary file %s: %s\n", fname,
                        safe_strerror(errno));
            if (! has_stdin)
                close(fd);
            return 1;
        }
        if ((0 == fstat(fd, &a_stat)) && S_ISFIFO(a_stat.st_mode)) {
            /* pipe; keep reading till error or 0 read */
            while (k < max_arr_len) {
                m = read(fd, mp_arr + k, max_arr_len - k);
                if (0 == m)
                   break;
                if (m < 0) {
                    pr2serr("read from binary pipe %s: %s\n", fname,
                            safe_strerror(errno));
                    if (! has_stdin)
                        close(fd);
                    return 1;
                }
                k += m;
            }
        }
        *mp_arr_len = k;
        if (! has_stdin)
            close(fd);
        return 0;
    } else {    /* So read the file as ASCII hex */
        if (has_stdin)
            fp = stdin;
        else {
            fp = fopen(fname, "r");
            if (NULL == fp) {
                pr2serr("Unable to open %s for reading\n", fname);
                return 1;
            }
        }
     }

    carry_over[0] = 0;
    for (j = 0; j < 512; ++j) {
        if (NULL == fgets(line, sizeof(line), fp))
            break;
        in_len = strlen(line);
        if (in_len > 0) {
            if ('\n' == line[in_len - 1]) {
                --in_len;
                line[in_len] = '\0';
                split_line = 0;
            } else
                split_line = 1;
        }
        if (in_len < 1) {
            carry_over[0] = 0;
            continue;
        }
        if (carry_over[0]) {
            if (isxdigit(line[0])) {
                carry_over[1] = line[0];
                carry_over[2] = '\0';
                if (1 == sscanf(carry_over, "%x", &h))
                    mp_arr[off - 1] = h;       /* back up and overwrite */
                else {
                    pr2serr("%s: carry_over error ['%s'] around line %d\n",
                            __func__, carry_over, j + 1);
                    goto bad;
                }
                lcp = line + 1;
                --in_len;
            } else
                lcp = line;
            carry_over[0] = 0;
        } else
            lcp = line;

        m = strspn(lcp, " \t");
        if (m == in_len)
            continue;
        lcp += m;
        in_len -= m;
        if ('#' == *lcp)
            continue;
        k = strspn(lcp, "0123456789aAbBcCdDeEfF ,\t");
        if ((k < in_len) && ('#' != lcp[k]) && ('\r' != lcp[k])) {
            pr2serr("%s: syntax error at line %d, pos %d\n", __func__,
                    j + 1, m + k + 1);
            goto bad;
        }
        if (no_space) {
            for (k = 0; isxdigit(*lcp) && isxdigit(*(lcp + 1));
                 ++k, lcp += 2) {
                if (1 != sscanf(lcp, "%2x", &h)) {
                    pr2serr("%s: bad hex number in line %d, pos %d\n",
                            __func__, j + 1, (int)(lcp - line + 1));
                    goto bad;
                }
                if ((off + k) >= max_arr_len) {
                    pr2serr("%s: array length exceeded\n", __func__);
                    goto bad;
                }
                mp_arr[off + k] = h;
            }
            if (isxdigit(*lcp) && (! isxdigit(*(lcp + 1))))
                carry_over[0] = *lcp;
            off += k;
        } else {
            for (k = 0; k < 1024; ++k) {
                if (1 == sscanf(lcp, "%x", &h)) {
                    if (h > 0xff) {
                        pr2serr("%s: hex number larger than 0xff in line "
                                "%d, pos %d\n", __func__, j + 1,
                                (int)(lcp - line + 1));
                        goto bad;
                    }
                    if (split_line && (1 == strlen(lcp))) {
                        /* single trailing hex digit might be a split pair */
                        carry_over[0] = *lcp;
                    }
                    if ((off + k) >= max_arr_len) {
                        pr2serr("%s: array length exceeded\n", __func__);
                        goto bad;
                    }
                    mp_arr[off + k] = h;
                    lcp = strpbrk(lcp, " ,\t");
                    if (NULL == lcp)
                        break;
                    lcp += strspn(lcp, " ,\t");
                    if ('\0' == *lcp)
                        break;
                } else {
                    if (('#' == *lcp) || ('\r' == *lcp)) {
                        --k;
                        break;
                    }
                    pr2serr("%s: error in line %d, at pos %d\n", __func__,
                            j + 1, (int)(lcp - line + 1));
                    goto bad;
                }
            }
            off += (k + 1);
        }
    }
    *mp_arr_len = off;
    if (stdin != fp)
        fclose(fp);
    return 0;
bad:
    if (stdin != fp)
        fclose(fp);
    return 1;
}

static void
enumerate_mpages(int transport, int vendor)
{
    const struct sdparm_mode_page_t * mpp;

    if (vendor >= 0) {
        const struct sdparm_vendor_pair * vpp;

        vpp = sdp_get_vendor_pair(vendor);
        if (NULL == vpp) {
            pr2serr("Bad vendor number\n");
            return;
        }
        mpp = vpp->mpage;
    } else if ((transport >= 0) && (transport < 16))
        mpp = sdparm_transport_mp[transport].mpage;
    else
        mpp = sdparm_gen_mode_pg;

    for ( ; mpp && mpp->acron; ++mpp) {
        if (mpp->name) {
            if (mpp->subpage)
                printf("  %-4s 0x%02x,0x%02x  %s\n", mpp->acron,
                       mpp->page, mpp->subpage, mpp->name);
            else
                printf("  %-4s 0x%02x       %s\n", mpp->acron,
                       mpp->page, mpp->name);
        }
    }
}

static void
enumerate_vpds()
{
    const struct sdparm_vpd_page_t * vpp;

    for (vpp = sdparm_vpd_pg; vpp->acron; ++vpp) {
        if (vpp->name) {
            if (vpp->vpd_num < 0)
                printf("  %-10s -1        %s\n", vpp->acron, vpp->name);
            else
                printf("  %-10s 0x%02x      %s\n", vpp->acron, vpp->vpd_num,
                       vpp->name);
        }
    }
}

static void
enumerate_transports()
{
    const struct sdparm_transport_id_t * tip;

    for (tip = sdparm_transport_id; tip->acron; ++tip) {
        if (tip->name)
            printf("  %-6s 0x%02x     %s\n", tip->acron, tip->proto_num,
                   tip->name);
    }
}

static void
enumerate_vendors()
{
    const struct sdparm_vendor_name_t * vnp;

    for (vnp = sdparm_vendor_id; vnp->acron; ++vnp) {
        if (vnp->name)
            printf("  %-6s 0x%02x     %s\n", vnp->acron, vnp->vendor_num,
                   vnp->name);
    }
}

static void
print_mp_extra(const char * extra)
{
    int n;
    char b[128];
    char * cp;
    char * p;

    for (p = (char *)extra; (cp = (char *)strchr(p, '\t')); p = cp + 1) {
        n = cp - p;
        if (n > (int)(sizeof(b) - 1))
            n = (sizeof(b) - 1);
        strncpy(b, p, n);
        b[n] = '\0';
        printf("\t%s\n", b);
    }
    printf("\t%s\n", p);
}

/* Enumerate mode page items. Reading from internal dataset. */
static void
enumerate_mitems(int pn, int spn, int pdt,
                 const struct sdparm_opt_coll * op)
{
    int t_pn, t_spn, t_pdt, vendor, transp, long_o;
    const struct sdparm_mode_page_t * mpp = NULL;
    const struct sdparm_mode_page_item * mpi;
    char buff[128];
    char b[128];
    int found = 0;

    t_pn = -1;
    t_spn = -1;
    t_pdt = -2;
    vendor = op->vendor;
    transp = op->transport;
    long_o = op->do_long;
    if (vendor >= 0) {
        const struct sdparm_vendor_pair * vpp;

        vpp = sdp_get_vendor_pair(vendor);
        mpi = (vpp ? vpp->mitem : NULL);
    } else if ((transp >= 0) && (transp < 16))
        mpi = sdparm_transport_mp[transp].mitem;
    else
        mpi = sdparm_mitem_arr;
    if (NULL == mpi)
        return;

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
            mpp = sdp_get_mpage_name(t_pn, t_spn, t_pdt, transp, vendor,
                                     long_o, 1, buff, sizeof(buff));
            if (long_o && (transp < 0) && (vendor < 0))
                printf("%s [%s] mode page:\n", buff,
                       sg_get_pdt_str(t_pdt, sizeof(b), b));
            else
                printf("%s mode page:\n", buff);
        } else {
            if ((pn >= 0) && ((pn != t_pn) || (spn != t_spn)))
                continue;
        }
        printf("  %-10s [0x%02x:%d:%-2d]  %s\n", mpi->acron, mpi->start_byte,
               mpi->start_bit, mpi->num_bits, mpi->description);
        if ((long_o > 1) && mpi->extra)
            print_mp_extra(mpi->extra);
        found = 1;
    }
    if ((! found) && (pn >= 0)) {
        sdp_get_mpage_name(pn, spn, pdt, transp, vendor, long_o, 1, buff,
                           sizeof(buff));
        pr2serr("%s mode page: no items found\n", buff);
    }
    if (found && mpp && mpp->mp_desc && long_o) {
        const struct sdparm_mode_descriptor_t * mdp = mpp->mp_desc;

        if (mdp->name)
            printf("  <<%s mode descriptor>>\n", mdp->name);
        else
            printf("  <<mode descriptor>>\n");
        printf("    num_descs_off=%d, num_descs_bytes=%d, "
               "num_descs_inc=%d, first_desc_off=%d\n",
               mdp->num_descs_off, mdp->num_descs_bytes,
               mdp->num_descs_inc,  mdp->first_desc_off);
        if (mdp->desc_len > 0)
            printf("    descriptor_len=%d\n", mdp->desc_len);
        else
            printf("    desc_len_off=%d, desc_len_bytes=%d\n",
                   mdp->desc_len_off, mdp->desc_len_bytes);
    }
}

static void
list_mp_settings(const struct sdparm_mode_page_settings * mps, int get)
{
    const struct sdparm_mode_page_item * mpip;
    int k;

    printf("mp_settings: page,subpage=0x%x,0x%x  num=%d\n",
           mps->page_num, mps->subpage_num, mps->num_it_vals);
    for (k = 0; k < mps->num_it_vals; ++k) {
        mpip = &mps->it_vals[k].mpi;
        if (get)
            printf("  [0x%x,0x%x]", mpip->page_num, mpip->subpage_num);

        printf("  pdt=%d start_byte=0x%x start_bit=%d num_bits=%d  val=%"
               PRId64 "", mpip->pdt, mpip->start_byte, mpip->start_bit,
               mpip->num_bits, mps->it_vals[k].val);
        if (mpip->acron) {
            printf("  acronym: %s", mpip->acron);
            if (mps->it_vals[k].descriptor_num > 0)
                printf("  descriptor_num=%d\n",
                       mps->it_vals[k].descriptor_num);
            else
                printf("\n");
        } else
            printf("\n");
    }
}

/* Print prefix and acronymn plus current, changeable [smask & 2], default
 * [smask & 4] and saved [smask & 8] values for an item */
static void
print_mp_entry(const char * pre, int smask,
               const struct sdparm_mode_page_item *mpi,
               const unsigned char * cur_mp,
               const unsigned char * cha_mp,
               const unsigned char * def_mp,
               const unsigned char * sav_mp,
               int force_decimal,
               const struct sdparm_opt_coll * op)
{
    int sep = 0;
    int all_set;
    uint64_t u;
    const char * acron;

    all_set = 0;
    acron = (mpi->acron ? mpi->acron : "");
    u = sdp_mp_get_value_check(mpi, cur_mp, &all_set);
    printf("%s%-14s", pre, acron);
    if (force_decimal)
        printf("%" PRId64 "", (int64_t)u);
    else if (mpi->flags & MF_HEX)
        printf("0x%" PRIx64 "", u);
    else if (all_set)
        printf("-1");
    else
        printf("%" PRIu64 "", u);
    if ((smask & 0xe) && (op->do_quiet < 2)) {
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
                printf(" %" PRId64 "", (int64_t)u);
            else if (mpi->flags & MF_HEX)
                printf(" 0x%" PRIx64 "", u);
            else if (all_set)
                printf(" -1");
            else
                printf("%3" PRIu64 "", u);
            sep = 1;
        }
        if (sav_mp && (smask & 8)) {
            all_set = 0;
            u = sdp_mp_get_value_check(mpi, sav_mp, &all_set);
            printf("%ssav:", (sep ? ", " : " "));
            if (force_decimal)
                printf(" %" PRId64 "", (int64_t)u);
            else if (mpi->flags & MF_HEX)
                printf(" 0x%" PRIx64 "", u);
            else if (all_set)
                printf(" -1");
            else
                printf("%3" PRIu64 "", u);
        }
        printf("]");
    }
    if (op->do_long && mpi->description)
        printf("  %s", mpi->description);
    printf("\n");
    if ((op->do_long > 1) && mpi->extra)
        print_mp_extra(mpi->extra);
}

static void
print_mp_arr_entry(const char * pre, int smask,
                   const struct sdparm_mode_page_item *mpi,
                   void ** pc_arr, int force_decimal,
                   const struct sdparm_opt_coll * op)
{
    const unsigned char * cur_mp = (const unsigned char *)pc_arr[0];
    const unsigned char * cha_mp = (const unsigned char *)pc_arr[1];
    const unsigned char * def_mp = (const unsigned char *)pc_arr[2];
    const unsigned char * sav_mp = (const unsigned char *)pc_arr[3];

    print_mp_entry(pre, smask, mpi, cur_mp, cha_mp, def_mp, sav_mp,
                   force_decimal, op);
}

static int
ll_mode_sense(int fd, const struct sdparm_opt_coll * op, int pn, int spn,
              unsigned char * resp, int mx_resp_len, int verb)
{
    if (op->mode_6)
        return sg_ll_mode_sense6(fd, op->dbd, 0 /*current */, pn, spn,
                                 resp, mx_resp_len, 1 /* noisy */, verb);
    else
        return sg_ll_mode_sense10(fd, 0 /* llbaa */, op->dbd, 0, pn,
                                  spn, resp, mx_resp_len, 1 /* noisy */,
                                  verb);
}

/* Make some noise if the mode page response seems badly formed */
static void
check_mode_page(unsigned char * cur_mp, int pn, int rep_len,
                const struct sdparm_opt_coll * op)
{
    int const pn_in_page = cur_mp[0] & 0x3f;

    if (pn != pn_in_page) {
        if (pn == POWER_MP && pn_in_page == POWER_OLD_MP) {
            /* Some devices, e.g. IBM DCHS disk, ca. 1997, in violation of
             * SCSI, report the old block device power control page when
             * asked for the new generic power control page.  The format
             * is identical except for the page number, so we can ignore
             * the mismatch. */
        } else {
            pr2serr(">>> warning: mode page seems malformed\n   The page "
                    "number field should be 0x%02x, but is 0x%02x",
                    pn, pn_in_page);
            if (! op->flexible)
                pr2serr("; try '--flexible'");
            pr2serr("\n");
        }
    } else if (op->verbose && (rep_len > 0xa00)) {
        pr2serr(">>> warning: mode page length=%d too long", rep_len);
        if (! op->flexible)
            pr2serr(", perhaps try '--flexible'");
        pr2serr("\n");
    }
}

/* When mode page has descriptor, print_mode_pages() will print the first */
/* descriptor. This function is called to print subsequent descriptors */
static void
print_mpage_extra_desc(void ** pc_arr, int rep_len,
                       const struct sdparm_mode_page_t * mpp,
                       const struct sdparm_mode_page_item * fdesc_mpi,
                       const struct sdparm_mode_page_item * last_mpi,
                       const struct sdparm_opt_coll * op,
                       int smask)
{
    const struct sdparm_mode_descriptor_t * mdp = mpp->mp_desc;
    const struct sdparm_mode_page_item * mpi;
    const unsigned char * cur_mp = (const unsigned char *)pc_arr[0];
    const unsigned char * cha_mp = (const unsigned char *)pc_arr[1];
    const unsigned char * def_mp = (const unsigned char *)pc_arr[2];
    const unsigned char * sav_mp = (const unsigned char *)pc_arr[3];
    const unsigned char * ucp;
    struct sdparm_mode_page_item ampi;
    uint64_t u;
    int k, num, len, d_off, n, bad;
    char b[32];

    if ((NULL == mdp) || (NULL == cur_mp) || (rep_len < 4) ||
        (mdp->num_descs_off >= rep_len))
        return;
    if ((mdp->num_descs_inc < 0) && (mdp->desc_len > 0)) {
        k = mdp->first_desc_off - (mdp->num_descs_off + mdp->num_descs_bytes);
        u = (sdp_get_big_endian(cur_mp + mdp->num_descs_off, 7,
                                mdp->num_descs_bytes * 8) - k) /
            mdp->desc_len;
    } else
        u = sdp_get_big_endian(cur_mp + mdp->num_descs_off, 7,
                       mdp->num_descs_bytes * 8) + mdp->num_descs_inc;
    num = (int)u;
    if (op->verbose > 1)
        pr2serr("    >>> mode page says it has %d descriptors\n", num);
    if ((u < 2) || (u > 256))
        return;
    if (mdp->desc_len <= 0) {
        ucp = cur_mp + mdp->first_desc_off + mdp->desc_len_off;
        u = sdp_get_big_endian(ucp, 7, mdp->desc_len_bytes * 8);
        len = mdp->desc_len_off + mdp->desc_len_bytes + u;
    } else
        len = mdp->desc_len;
    d_off = mdp->first_desc_off + len;
    for (k = 1; k < num; ++k, d_off += len) {
        bad = 0;
        for (mpi = fdesc_mpi; mpi <= last_mpi; ++mpi) {
            strncpy(b, mpi->acron, sizeof(b));
            ampi = *mpi;
            b[sizeof(b) - 8] = '\0';
            n = strlen(b);
            snprintf(b + n, sizeof(b) - n, ".%d", k);
            ampi.acron = b;
            ampi.start_byte += (d_off - mdp->first_desc_off);
            if (ampi.start_byte >= rep_len) {
                pr2serr("descriptor overflows reply len (%d) for %s\n",
                        rep_len, ampi.acron);
                bad = 1;
                break;
            }
            print_mp_entry("  ", smask, &ampi, cur_mp, cha_mp, def_mp,
                           sav_mp, 0, op);
        }
        if (bad)
            break;
        if (mdp->desc_len <= 0) {
            ucp = cur_mp + d_off + mdp->desc_len_off;
            u = sdp_get_big_endian(ucp, 7, mdp->desc_len_bytes * 8);
            len = mdp->desc_len_off + mdp->desc_len_bytes + u;
        }
    }
}

/* Print WP and DPOFUA bits for direct access devices (disks).
 * Return 0 for success, else error value. */
static int
print_direct_access_info(int sg_fd, const struct sdparm_opt_coll * op,
                         int verb)
{
    int res, v;
    unsigned char cur_mp[DEF_MODE_RESP_LEN];

    memset(cur_mp, 0, sizeof(cur_mp));
    res = ll_mode_sense(sg_fd, op, ALL_MPAGES, 0, cur_mp, 8, verb);
    if (0 == res) {
        v = cur_mp[op->mode_6 ? 2 : 3];
        printf("    Direct access device specific parameters: WP=%d  "
               "DPOFUA=%d\n", !!(v & 0x80), !!(v & 0x10));
    } else if (SG_LIB_CAT_NOT_READY == res) {
        pr2serr("mode sense command failed, device not ready\n");
        return res;
    } else if (SG_LIB_CAT_INVALID_OP == res) {
        if (op->mode_6)
            pr2serr("6 byte MODE SENSE cdb not supported, try again without "
                    "'-6' option\n");
        else
            pr2serr("10 byte MODE SENSE cdb not supported, try again with "
                    "'-6' option\n");
        return res;
    } else if (SG_LIB_CAT_UNIT_ATTENTION == res) {
        pr2serr("mode sense command failed, unit attention\n");
        return res;
    } else if (SG_LIB_CAT_ABORTED_COMMAND == res) {
        pr2serr("mode sense command failed, aborted command\n");
        return res;
    }
    return 0;
}

/* Print one or more mode pages. Returns 0 if ok else IO error number.  */
static int
print_mode_pages(int sg_fd, int pn, int spn, int pdt,
                 const struct sdparm_opt_coll * op)
{
    int res, pg_len, verb, smask, single_pg, fetch_pg, rep_len, orig_pn;
    int warned, first_desc_off;
    const struct sdparm_mode_page_item * mpi;
    const struct sdparm_mode_page_item * hmpi;
    const struct sdparm_mode_page_item * fdesc_mpi = NULL;
    const struct sdparm_mode_page_t * mpp = NULL;
    const struct sdparm_mode_descriptor_t * mdp;
    unsigned char cur_mp[DEF_MODE_RESP_LEN];
    unsigned char cha_mp[DEF_MODE_RESP_LEN];
    unsigned char def_mp[DEF_MODE_RESP_LEN];
    unsigned char sav_mp[DEF_MODE_RESP_LEN];
    void * pc_arr[4];
    char buff[128];

    buff[0] = '\0';
    verb = (op->verbose > 0) ? op->verbose - 1 : 0;
    if ((0 == pdt) && (op->do_long > 0) && (0 == op->do_quiet)) {
        if (0 != (res = print_direct_access_info(sg_fd, op, verb)))
            return res;
    }
    orig_pn = pn;
    /* choose a mode page item namespace (vendor, transport or generic) */
    if (op->vendor >= 0) {
        const struct sdparm_vendor_pair * vpp;

        vpp = sdp_get_vendor_pair(op->vendor);
        mpi = (vpp ? vpp->mitem : NULL);
    } else if ((op->transport >= 0) && (op->transport < 16))
        mpi = sdparm_transport_mp[op->transport].mitem;
    else
        mpi = sdparm_mitem_arr;
    if (NULL == mpi)
        return SG_LIB_CAT_OTHER;

    hmpi = mpi;
    if (pn >= 0) {      /* step to first item of given page */
        single_pg = 1;
        fetch_pg = 1;
        for ( ; mpi->acron; ++mpi) {
            if ((pn == mpi->page_num) && (spn == mpi->subpage_num)) {
                if ((pdt < 0) || (mpi->pdt < 0) ||
                    (pdt == mpi->pdt) || op->flexible)
                    break;
            }
        }
        if (NULL == mpi->acron) {       /* page has no known fields */
            if (op->do_hex)
                mpi = hmpi;    /* trick to enter main loop once */
            else {
                sdp_get_mpage_name(pn, spn, pdt, op->transport,
                                   op->vendor, 0, 0, buff, sizeof(buff));
                if ((op->vendor < 0) && ((0 == pn) || (pn >= 0x20)))
                    pr2serr("%s mode page seems to be vendor specific, try "
                            "'--vendor=VN'.\nOtherwise add '-H' to see page "
                            "in hex.\n", buff);
                else
                    pr2serr("%s mode page, no fields found, add '-H' to see "
                            "page in hex.\n", buff);
            }
        }
    } else {    /* want all so check all items in given namespace */
        single_pg = 0;
        fetch_pg = 0;
        mpi = hmpi;
    }
    mdp = NULL;
    pg_len = 0;

    /* starting at first match, loop over each mode page item in given
     * namespace */
    for (smask = 0, warned = 0 ; mpi->acron; ++mpi, fetch_pg = 0) {
        if (0 == fetch_pg) {
            if ((pn != mpi->page_num) || (spn != mpi->subpage_num)) {
                if (fdesc_mpi) {
                    hmpi = mpi - 1;
                    if ((pn == hmpi->page_num) && (spn == hmpi->subpage_num))
                        print_mpage_extra_desc(pc_arr, pg_len, mpp,
                                               fdesc_mpi, hmpi, op, smask);
                }
                if (single_pg)
                    break;
                fetch_pg = 1;
                pn = mpi->page_num;
                spn = mpi->subpage_num;
            } else {
                if ((pdt >=0) && (mpi->pdt >= 0) &&
                    (pdt != mpi->pdt) && (0 == op->flexible))
                    continue;
                if (! (((orig_pn >= 0) ? 1 : op->do_all) ||
                       (MF_COMMON & mpi->flags)))
                    continue;
            }
        }
        if (fetch_pg) {
            pg_len = 0;
            /* Only fetch mode page when needed (e.g. item page changed) */
            mpp = sdp_get_mpage_name(pn, spn, pdt, op->transport,
                                     op->vendor, op->do_long, op->do_hex,
                                     buff, sizeof(buff));
            smask = 0;
            warned = 0;
            fdesc_mpi = NULL;
            pc_arr[0] = cur_mp;
            pc_arr[1] = cha_mp;
            pc_arr[2] = def_mp;
            pc_arr[3] = sav_mp;
            res = sg_get_mode_page_controls(sg_fd, op->mode_6, pn, spn,
                                            op->dbd, op->flexible,
                                            DEF_MODE_RESP_LEN, &smask,
                                            pc_arr, &rep_len, verb);
            if (SG_LIB_CAT_INVALID_OP == res) {
                if (op->mode_6)
                    pr2serr("6 byte MODE SENSE cdb not supported, try again "
                            "without '-6' option\n");
                else
                    pr2serr("10 byte MODE SENSE cdb not supported, try again "
                            "with '-6' option\n");
                return res;
            } else if (SG_LIB_CAT_NOT_READY == res) {
                pr2serr("MODE SENSE failed, device not ready\n");
                return res;
            } else if (SG_LIB_CAT_UNIT_ATTENTION == res) {
                pr2serr("mode sense command failed, unit attention\n");
                return res;
            } else if (SG_LIB_CAT_ABORTED_COMMAND == res) {
                pr2serr("mode sense command failed, aborted command\n");
                return res;
            }
            /* check for mode page descriptors */
            mdp = (mpp && (! op->do_hex)) ? mpp->mp_desc : NULL;
            first_desc_off = mdp ? mdp->first_desc_off : 0;
            if (first_desc_off > 1) {
                for (res = 0, fdesc_mpi = mpi;
                     fdesc_mpi && (pn == fdesc_mpi->page_num) &&
                     (spn == fdesc_mpi->subpage_num); ++fdesc_mpi) {
                    if (fdesc_mpi->start_byte >= first_desc_off) {
                        res = 1;
                        break;
                    }
                }
                if (0 == res)
                    fdesc_mpi = NULL;
            }
            if (op->num_desc) {
                int num = 0;
                uint64_t u;

                if (fdesc_mpi && (smask & 1)) {
                    if ((mdp->num_descs_inc < 0) && (mdp->desc_len > 0))
                        u = sdp_get_big_endian(cur_mp + mdp->num_descs_off, 7,
                                               mdp->num_descs_bytes * 8) /
                            mdp->desc_len;
                    else
                        u = sdp_get_big_endian(cur_mp + mdp->num_descs_off, 7,
                                               mdp->num_descs_bytes * 8) +
                            mdp->num_descs_inc;
                    num = (int)u;
                }
                if (op->do_long)
                    printf("number of descriptors=%d\n", num);
                else
                    printf("%d\n", num);
                return 0;
            }
            if (smask & 1) {
                pg_len = sdp_get_mp_len(cur_mp);
                printf("%s ", buff);
                if (op->verbose) {
                    if (spn)
                        printf("[0x%x,0x%x] ", pn, spn);
                    else
                        printf("[0x%x] ", pn);
                }
                printf("mode page");
                if ((op->do_long > 1) || op->verbose)
                    printf(" [PS=%d]:\n", !!(cur_mp[0] & 0x80));
                else
                    printf(":\n");
                check_mode_page(cur_mp, pn, pg_len, op);
                if (op->do_hex) {
                    if (pg_len > (int)sizeof(cur_mp)) {
                        pr2serr(">> decoded page length too large=%d, trim\n",
                                pg_len);
                        pg_len = sizeof(cur_mp);
                    }
                    printf("    Current:\n");
                    dStrHex((const char *)cur_mp, pg_len, 1);
                    if (smask & 2) {
                        printf("    Changeable:\n");
                        dStrHex((const char *)cha_mp, pg_len, 1);
                    }
                    if (smask & 4) {
                        printf("    Default:\n");
                        dStrHex((const char *)def_mp, pg_len, 1);
                    }
                    if (smask & 8) {
                        printf("    Saved:\n");
                        dStrHex((const char *)sav_mp, pg_len, 1);
                    }
                }
            } else {
                if (op->verbose || single_pg) {
                    pr2serr(">> %s mode %spage ", buff, (spn ? "sub" : ""));
                    if (op->verbose) {
                        if (spn)
                            pr2serr("[0x%x,0x%x] ", pn, spn);
                        else
                            pr2serr("[0x%x] ", pn);
                    }
                    if (SG_LIB_CAT_ILLEGAL_REQ == res)
                        pr2serr("not found\n");
                    else if (0 == res)
                        pr2serr("some problem\n");
                    else
                        pr2serr("failed\n");
                }
            }
        }       /* end of fetch_pg */
        if (smask && (! op->do_hex)) {
            if (mpi->start_byte >= pg_len) {
                if ((0 == op->flexible) && (0 == op->verbose))
                    continue;   // step over
                if (0 == warned) {
                    warned = 1;
                    if (op->flexible)
                        pr2serr(" >> hereafter field position exceeds mode "
                                "page length=%d\n", pg_len);
                    else {
                        pr2serr(" >> skipping rest as field position exceeds "
                                "mode page length=%d\n", pg_len);
                        continue;
                    }
                }
                if (0 == op->flexible)
                    continue;
            }
            print_mp_arr_entry("  ", smask, mpi, pc_arr, 0, op);
        }
    }   /* end of mode page item loop */
    if (mpi && (NULL == mpi->acron) && fdesc_mpi) {
        --mpi;
        if ((pn == mpi->page_num) && (spn == mpi->subpage_num))
            print_mpage_extra_desc(pc_arr, pg_len, mpp, fdesc_mpi, mpi,
                                   op, smask);
    }
    return 0;
}

/* Print a mode page provided as argument. Returns 0 if ok else error
 * number.  */
static int
print_mode_page_given(uint8_t * cur_mp, int cur_mp_len,
                      const struct sdparm_opt_coll * op)
{
    int res, smask, first_time, rep_len, orig_pn, warned,  v, off;
    int first_desc_off, pn, spn, pg_len, transport, vendor;
    const struct sdparm_mode_page_item * mpi;
    const struct sdparm_mode_page_item * hmpi;
    const struct sdparm_mode_page_item * fdesc_mpi = NULL;
    const struct sdparm_mode_page_t * mpp = NULL;
    const struct sdparm_mode_descriptor_t * mdp;
    uint8_t * pg_p;
    void * pc_arr[4];
    char buff[128];
    char ebuff[128];

    rep_len = op->mode_6 ? (cur_mp[0] + 1) :
                           (sg_get_unaligned_be16(cur_mp) + 2);
    if (cur_mp_len < rep_len) {
        pr2serr("given mode page is too short, given %d bytes, expected "
                "%d\n", cur_mp_len, rep_len);
        if (op->inhex_fn)
            pr2serr("perhaps %s is a VPD page, if so add '-i'\n",
                    op->inhex_fn);
        return SG_LIB_CAT_OTHER;
    }
    off = sg_mode_page_offset(cur_mp, cur_mp_len, op->mode_6, ebuff,
                              sizeof(ebuff));
    if (off < 0) {
        pr2serr("%s\n", ebuff);
        return SG_LIB_CAT_OTHER;
    }
    if ((0 == op->pdt) && (op->do_long > 0) && (0 == op->do_quiet)) {
        v = cur_mp[op->mode_6 ? 2 : 3];
        printf("    Direct access device specific parameters: WP=%d  "
               "DPOFUA=%d\n", !!(v & 0x80), !!(v & 0x10));
    }

and_again:
    pg_p = cur_mp + off;
    pn = pg_p[0] & 0x3f;
    pg_len = sdp_get_mp_len(pg_p);
    spn = (pg_p[0] & 0x40) ? pg_p[1] : 0;
    pc_arr[0] = pg_p;
    pc_arr[1] = NULL;
    pc_arr[2] = NULL;
    pc_arr[3] = NULL;
    smask = 0x1;        /* treat as single "current" page */

    buff[0] = '\0';
    orig_pn = pn;
    /* choose a mode page item namespace (vendor, transport or generic) */
    transport = op->transport;
    vendor = op->vendor;
    if (vendor >= 0) {
        const struct sdparm_vendor_pair * vpp;

        vpp = sdp_get_vendor_pair(vendor);
        mpi = (vpp ? vpp->mitem : NULL);
    } else if ((transport >= 0) && (transport < 16))
        mpi = sdparm_transport_mp[transport].mitem;
    else
        mpi = sdparm_mitem_arr;
    if (NULL == mpi)
        return SG_LIB_CAT_OTHER;

now_try_generic:
    hmpi = mpi;
    for ( ; mpi->acron; ++mpi) {
        if ((pn == mpi->page_num) && (spn == mpi->subpage_num)) {
            if ((op->pdt < 0) || (mpi->pdt < 0) ||
                (op->pdt == mpi->pdt) || op->flexible)
                break;
        }
    }
    if (NULL == mpi->acron) {       /* page has no known fields */
        if (op->do_all && ((transport >= 0) || (vendor >= 0))) {
            if (transport >= 0)
                transport = -1;
            if (vendor >= 0)
                vendor = -1;
            mpi = sdparm_mitem_arr;
            goto now_try_generic;
        }
        sdp_get_mpage_name(pn, spn, op->pdt, transport, vendor, 0, 0,
                           buff, sizeof(buff));
        if ((vendor < 0) && ((0 == pn) || (pn >= 0x20)))
            pr2serr("%s mode page seems to be vendor specific, try "
                    "'--vendor=VN'.\nOtherwise add '-H' to see page "
                    "in hex.\n", buff);
        else
            pr2serr("%s mode page, no fields found, add '-H' to see "
                    "page in hex.\n", buff);
    }
    mdp = NULL;

    /* starting at first match, loop over each mode page item in given
     * namespace */
    for (warned = 0, first_time = 1 ; mpi->acron; ++mpi) {
        if (first_time) {
            first_time = 0;
            mpp = sdp_get_mpage_name(pn, spn, op->pdt, transport, vendor,
                                     op->do_long, op->do_hex, buff,
                                     sizeof(buff));
            warned = 0;
            fdesc_mpi = NULL;
            /* check for mode page descriptors */
            mdp = (mpp && (! op->do_hex)) ? mpp->mp_desc : NULL;
            first_desc_off = mdp ? mdp->first_desc_off : 0;
            if (first_desc_off > 1) {
                for (res = 0, fdesc_mpi = mpi;
                     fdesc_mpi && (pn == fdesc_mpi->page_num) &&
                     (spn == fdesc_mpi->subpage_num); ++fdesc_mpi) {
                    if (fdesc_mpi->start_byte >= first_desc_off) {
                        res = 1;
                        break;
                    }
                }
                if (0 == res)
                    fdesc_mpi = NULL;
            }
            if (op->num_desc) {
                int num = 0;
                uint64_t u;

                if (fdesc_mpi && (smask & 1)) {
                    if ((mdp->num_descs_inc < 0) && (mdp->desc_len > 0))
                        u = sdp_get_big_endian(pg_p + mdp->num_descs_off, 7,
                                               mdp->num_descs_bytes * 8) /
                            mdp->desc_len;
                    else
                        u = sdp_get_big_endian(pg_p + mdp->num_descs_off, 7,
                                               mdp->num_descs_bytes * 8) +
                            mdp->num_descs_inc;
                    num = (int)u;
                }
                if (op->do_long)
                    printf("number of descriptors=%d\n", num);
                else
                    printf("%d\n", num);
                return 0;
            }
            printf("%s ", buff);
            if (op->verbose) {
                if (spn)
                    printf("[0x%x,0x%x] ", pn, spn);
                else
                    printf("[0x%x] ", pn);
            }
            printf("mode page");
            if ((op->do_long > 1) || op->verbose)
                printf(" [PS=%d]:\n", !!(pg_p[0] & 0x80));
            else
                printf(":\n");
            check_mode_page(pg_p, pn, pg_len, op);
        } else {        /* if not first_time */
            if ((pn != mpi->page_num) || (spn != mpi->subpage_num)) {
                if (fdesc_mpi) {
                    hmpi = mpi - 1;
                    if ((pn == hmpi->page_num) && (spn == hmpi->subpage_num))
                        print_mpage_extra_desc(pc_arr, pg_len, mpp,
                                               fdesc_mpi, hmpi, op, smask);
                }
                break;
            } else {
                if ((op->pdt >=0) && (mpi->pdt >= 0) &&
                    (op->pdt != mpi->pdt) && (0 == op->flexible))
                    continue;
                if (! (((orig_pn >= 0) ? 1 : op->do_all) ||
                       (MF_COMMON & mpi->flags)))
                    continue;
            }
        }
        if (smask && (! op->do_hex)) {
            if (mpi->start_byte >= pg_len) {
                if ((0 == op->flexible) && (0 == op->verbose))
                    continue;   // step over
                if (0 == warned) {
                    warned = 1;
                    if (op->flexible)
                        pr2serr(" >> hereafter field position exceeds mode "
                                "page length=%d\n", pg_len);
                    else {
                        pr2serr(" >> skipping rest as field position exceeds "
                                "mode page length=%d\n", pg_len);
                        continue;
                    }
                }
                if (0 == op->flexible)
                    continue;
            }
            print_mp_arr_entry("  ", smask, mpi, pc_arr, 0, op);
        }
    }   /* end of mode page item loop */
    if (mpi && (NULL == mpi->acron) && fdesc_mpi) {
        --mpi;
        if ((pn == mpi->page_num) && (spn == mpi->subpage_num))
            print_mpage_extra_desc(pc_arr, pg_len, mpp, fdesc_mpi, mpi,
                                   op, smask);
    }
    if (op->do_all) {
        off += pg_len;
        if ((off + 4) < rep_len)
            goto and_again;
    }
    return 0;
}

/* returns 1 when ok, else 0 */
static int
check_desc_convert_mpi(int desc_num, const struct sdparm_mode_page_t * mpp,
                       const struct sdparm_mode_page_item * ref_mpi,
                       struct sdparm_mode_page_item * out_mpi,
                       char * b, int b_len)
{
    int n;

    if (mpp && mpp->mp_desc && ref_mpi->acron) {
        *out_mpi = *ref_mpi;
        strncpy(b, ref_mpi->acron, b_len);
        b[(b_len > 10) ? (b_len - 8) : 4] = '\0';
        n = strlen(b);
        snprintf(b + n, b_len - n, ".%d", desc_num);
        out_mpi->acron = b;
        return 1;
    } else
        return 0;
}

/* returns 1 when ok, else 0 */
static int
desc_adjust_start_byte(int desc_num, const struct sdparm_mode_page_t * mpp,
                       unsigned char * cur_mp, int rep_len,
                       struct sdparm_mode_page_item * mpi,
                       const struct sdparm_opt_coll * op)
{
    const struct sdparm_mode_descriptor_t * mdp;
    uint64_t u;
    const unsigned char * ucp;
    int d_off, sb_off, j;

    mdp = mpp->mp_desc;
    if ((mdp->num_descs_off < rep_len) && (mdp->num_descs_off < 64)) {
        if ((mdp->num_descs_inc < 0) && (mdp->desc_len > 0))
            u = sdp_get_big_endian(cur_mp + mdp->num_descs_off, 7,
                    mdp->num_descs_bytes * 8) / mdp->desc_len;
        else
            u = sdp_get_big_endian(cur_mp + mdp->num_descs_off, 7,
                    mdp->num_descs_bytes * 8) + mdp->num_descs_inc;
        if ((uint64_t)desc_num < u) {
            if (mdp->desc_len > 0) {
                mpi->start_byte += (mdp->desc_len * desc_num);
                if (mpi->start_byte < rep_len)
                    return 1;
            } else if (mdp->desc_len_off > 0) {
                /* need to walk through variable length descriptors */

                sb_off = mpi->start_byte - mdp->first_desc_off;
                d_off = mdp->first_desc_off;
                for (j = 0; ; ++j) {
                    if (j > desc_num) {
                        pr2serr(">> descriptor number sanity ...\n");
                        break;  /* sanity */
                    }
                    if (j == desc_num) {
                        mpi->start_byte = d_off + sb_off;
                        if (mpi->start_byte < rep_len)
                            return 1;
                        else
                            pr2serr(">> new start_byte exceeds current page "
                                    "...\n");
                        break;
                    }
                    ucp = cur_mp + d_off + mdp->desc_len_off;
                    u = sdp_get_big_endian(ucp, 7,
                                           mdp->desc_len_bytes * 8);
                    d_off += mdp->desc_len_off +
                             mdp->desc_len_bytes + u;
                    if (d_off >= rep_len) {
                        pr2serr(">> descriptor number too large for current "
                                "page ...\n");
                        break;
                    }
                }
            }
        } else if (op->verbose)
            pr2serr("    >> mode page says it has only %d descriptors\n",
                    (int)u);
    }
    return 0;
}

/* Print one or more mode page items (from '--get='). Returns 0 if ok. */
static int
print_mode_items(int sg_fd, const struct sdparm_mode_page_settings * mps,
                 int pdt, const struct sdparm_opt_coll * op)
{
    int k, res, verb, smask, pn, spn, warned, rep_len, len, desc_num, adapt;
    uint64_t u;
    int64_t val;
    const struct sdparm_mode_page_item * mpi;
    struct sdparm_mode_page_item ampi;
    const struct sdparm_mode_page_t * mpp = NULL;
    unsigned char cur_mp[DEF_MODE_RESP_LEN];
    unsigned char cha_mp[DEF_MODE_RESP_LEN];
    unsigned char def_mp[DEF_MODE_RESP_LEN];
    unsigned char sav_mp[DEF_MODE_RESP_LEN];
    const struct sdparm_mode_page_it_val * ivp;
    char buff[128];
    char b_tmp[32];
    void * pc_arr[4];

    warned = 0;
    verb = (op->verbose > 0) ? op->verbose - 1 : 0;
    for (k = 0, pn = 0, spn = 0; k < mps->num_it_vals; ++k) {
        ivp = &mps->it_vals[k];
        val = ivp->val;
        desc_num = ivp->descriptor_num;
        mpi = &ivp->mpi;
        mpp = sdp_get_mpage_name(mpi->page_num, mpi->subpage_num, mpi->pdt,
                                 op->transport, op->vendor, op->do_long,
                                 op->do_hex, buff, sizeof(buff));
        if (desc_num > 0) {
            if (check_desc_convert_mpi(desc_num, mpp, mpi, &ampi, b_tmp,
                                       sizeof(b_tmp))) {
                adapt = 1;
                mpi = &ampi;
            } else {
               pr2serr("can't decode descriptors for %s in %s mode page\n",
                       (mpi->acron ? mpi->acron : ""), buff);
               return SG_LIB_SYNTAX_ERROR;
            }
        } else
            adapt = 0;
        if ((0 == k) || (pn != mpi->page_num) || (spn != mpi->subpage_num)) {
            pn = mpi->page_num;
            spn = mpi->subpage_num;
            smask = 0;
            res = 0;
            switch (val) {
            case 0:
                pc_arr[0] = cur_mp;
                pc_arr[1] = cha_mp;
                pc_arr[2] = def_mp;
                pc_arr[3] = sav_mp;
                res = sg_get_mode_page_controls(sg_fd, op->mode_6, pn, spn,
                                 op->dbd, op->flexible, DEF_MODE_RESP_LEN,
                                 &smask, pc_arr, &rep_len, verb);
                break;
            case 1:
            case 2:
                pc_arr[0] = cur_mp;
                pc_arr[1] = NULL;
                pc_arr[2] = NULL;
                pc_arr[3] = NULL;
                res = sg_get_mode_page_controls(sg_fd, op->mode_6, pn, spn,
                                 op->dbd, op->flexible, DEF_MODE_RESP_LEN,
                                 &smask, pc_arr, &rep_len, verb);
                break;
            default:
                if (mpi->acron)
                    pr2serr("bad value given to %s\n", mpi->acron);
                else
                    pr2serr("bad value given to 0x%x:%d:%d\n",
                            mpi->start_byte, mpi->start_bit, mpi->num_bits);
                return SG_LIB_SYNTAX_ERROR;
            }
            if (SG_LIB_CAT_INVALID_OP == res) {
                if (op->mode_6)
                    pr2serr("6 byte MODE SENSE cdb not supported, try again "
                            "without '-6' option\n");
                else
                    pr2serr("10 byte MODE SENSE cdb not supported, try again "
                            "with '-6' option\n");
                return res;
            } else if (SG_LIB_CAT_NOT_READY == res) {
                pr2serr("MODE SENSE failed, device not ready\n");
                return res;
            } else if (SG_LIB_CAT_UNIT_ATTENTION == res) {
                pr2serr("MODE SENSE failed, unit attention\n");
                return res;
            } else if (SG_LIB_CAT_ABORTED_COMMAND == res) {
                pr2serr("MODE SENSE failed, aborted command\n");
                return res;
            }
            if ((0 == smask) && res) {
                if (mpi->acron)
                    pr2serr("%s ", mpi->acron);
                else
                    pr2serr("0x%x:%d:%d ", mpi->start_byte, mpi->start_bit,
                            mpi->num_bits);
                if (SG_LIB_CAT_ILLEGAL_REQ == res)
                    pr2serr("not found in ");
                else
                    pr2serr("error %sin ",
                            (verb ? "" : "(try adding '-vv') "));
                pr2serr("%s mode page\n", buff);
                return res;
            }
            if (smask & 1)
                check_mode_page(cur_mp, pn, rep_len, op);
        }
        if (adapt) {
            if (! desc_adjust_start_byte(desc_num, mpp, cur_mp, rep_len,
                                         &ampi, op)) {
                pr2serr(">> failed to find field acronym: %s in current "
                        "page\n", mpi->acron);
                return SG_LIB_CAT_OTHER;
            }
        }
        if ((pdt >= 0) && (0 == warned) && mpi->acron &&
            (mpi->pdt >= 0) && (pdt != mpi->pdt)) {
            warned = 1;
            pr2serr(">> warning: peripheral device type (pdt) is 0x%x but "
                    "acronym %s\n   is associated with pdt 0x%x.\n", pdt,
                    ivp->mpi.acron, ivp->mpi.pdt);
        }
        len = (smask & 1) ? sdp_get_mp_len(cur_mp) : 0;
        if (mpi->start_byte >= len) {
            pr2serr(">> warning: ");
            if (mpi->acron)
                pr2serr("%s ", mpi->acron);
            else
                pr2serr("0x%x:%d:%d ", mpi->start_byte, mpi->start_bit,
                        mpi->num_bits);
            pr2serr("field position exceeds mode page length=%d\n", len);
            if (! op->flexible)
                continue;
        }
        if (0 == val) {
            if (op->do_hex) {
                if (smask & 1) {
                    u = sdp_mp_get_value(mpi, cur_mp);
                    printf("0x%02" PRIx64 " ", u);
                } else
                    printf("-    ");
                if (smask & 2) {
                    u = sdp_mp_get_value(mpi, cha_mp);
                    printf("0x%02" PRIx64 " ", u);
                } else
                    printf("-    ");
                if (smask & 4) {
                    u = sdp_mp_get_value(mpi, def_mp);
                    printf("0x%02" PRIx64 " ", u);
                } else
                    printf("-    ");
                if (smask & 8) {
                    u = sdp_mp_get_value(mpi, sav_mp);
                    printf("0x%02" PRIx64 " ", u);
                } else
                    printf("-    ");
                printf("\n");
            } else
                print_mp_arr_entry("", smask, mpi, pc_arr, 0, op);
        } else if (1 == val) {
            if (op->do_hex) {
                if (smask & 1) {
                    u = sdp_mp_get_value(mpi, cur_mp);
                    printf("0x%02" PRIx64 " ", u);
                } else
                    printf("-    ");
                printf("\n");
            } else
                print_mp_arr_entry("", smask & 1, mpi, pc_arr, 0, op);
        } else if (2 == val) {
            if (op->do_hex) {
                if (smask & 1) {
                    u = sdp_mp_get_value(mpi, cur_mp);
                    printf("%02" PRId64 " ", (int64_t)u);
                } else
                    printf("-    ");
                printf("\n");
            } else
                print_mp_arr_entry("", smask & 1, mpi, pc_arr, 1, op);
        }
    }
    return 0;
}

/* Return of 0 -> success, various SG_LIB_CAT_* positive values,
 * -1 -> other failure */
static int
change_mode_page(int sg_fd, int pdt,
                 const struct sdparm_mode_page_settings * mps,
                 const struct sdparm_opt_coll * op)
{
    int k, off, md_len, len, res, desc_num, pn, spn;
    char ebuff[EBUFF_SZ];
    const struct sdparm_mode_page_t * mpp = NULL;
    unsigned char mdpg[MAX_MODE_DATA_LEN];
    const struct sdparm_mode_page_it_val * ivp;
    const struct sdparm_mode_page_item * mpi;
    struct sdparm_mode_page_item ampi;
    char b[128];
    char b_tmp[32];

    if (pdt >= 0) {
        /* sanity check: check acronym's pdt matches device's pdt */
        for (k = 0; k < mps->num_it_vals; ++k) {
            ivp = &mps->it_vals[k];
            if (ivp->mpi.acron && (ivp->mpi.pdt >= 0) &&
                (pdt != ivp->mpi.pdt)) {
                pr2serr("%s: peripheral device type (pdt) is 0x%x but "
                        "acronym %s\n  is associated with pdt 0x%x. To "
                        "bypass use numeric addressing mode.\n", __func__,
                         pdt, ivp->mpi.acron, ivp->mpi.pdt);
                return SG_LIB_SYNTAX_ERROR;
            }
        }
    }
    pn = mps->page_num;
    spn = mps->subpage_num;
    mpp = sdp_get_mpage_name(pn, spn, pdt, op->transport, op->vendor,
                             0, 0, b, sizeof(b));
    memset(mdpg, 0, sizeof(mdpg));
    res = ll_mode_sense(sg_fd, op, pn, spn, mdpg, 4, op->verbose);
    if (0 != res) {
        if (SG_LIB_CAT_INVALID_OP == res) {
            if (op->mode_6)
                pr2serr("6 byte MODE SENSE cdb not supported, try again "
                        "without '-6' option\n");
            else
                pr2serr("10 byte MODE SENSE cdb not supported, try again "
                        "with '-6' option\n");
        } else {
            char b[80];

            sg_get_category_sense_str(res, sizeof(b), b, op->verbose);
            pr2serr("mode sense command: %s\n", b);
        }
        pr2serr("%s: failed fetching page: %s\n", __func__, b);
        return res;
    }
    md_len = op->mode_6 ? (mdpg[0] + 1) : (sg_get_unaligned_be16(mdpg) + 2);
    if (md_len > (int)sizeof(mdpg)) {
        pr2serr("%s: mode data length=%d exceeds allocation length=%d\n",
                __func__, md_len, (int)sizeof(mdpg));
        return SG_LIB_CAT_MALFORMED;
    }
    res = ll_mode_sense(sg_fd, op, pn, spn, mdpg, md_len, op->verbose);
    if (0 != res) {
        pr2serr("%s: failed fetching page: %s\n", __func__, b);
        return res;
    }
    off = sg_mode_page_offset(mdpg, md_len, op->mode_6, ebuff, EBUFF_SZ);
    if (off < 0) {
        pr2serr("%s: page offset failed: %s\n", __func__, ebuff);
        return SG_LIB_CAT_MALFORMED;
    }
    len = sdp_get_mp_len(mdpg + off);
    mdpg[0] = 0;        /* mode data length reserved for mode select */
    if (! op->mode_6)
        mdpg[1] = 0;    /* mode data length reserved for mode select */
    if (PDT_DISK == pdt)       /* entire disk specific parameters is ... */
        mdpg[op->mode_6 ? 2 : 3] = 0x00;     /* reserved for mode select */

    for (k = 0; k < mps->num_it_vals; ++k) {
        ivp = &mps->it_vals[k];
        mpi = &ivp->mpi;
        desc_num = ivp->descriptor_num;
        if (desc_num > 0) {
            if (check_desc_convert_mpi(desc_num, mpp, mpi, &ampi, b_tmp,
                                       sizeof(b_tmp))) {
                mpi = &ampi;
                if (! desc_adjust_start_byte(desc_num, mpp, mdpg + off, len,
                                             &ampi, op)) {
                    pr2serr(">> failed to find field acronym: %s in "
                            "current page\n", mpi->acron);
                    return SG_LIB_CAT_OTHER;
                }
            } else {
               pr2serr("can't decode descriptors for %s in %s mode "
                       "page\n", (mpi->acron ? mpi->acron : ""), b);
               return SG_LIB_SYNTAX_ERROR;
            }
        }
        if (mpi->start_byte >= len) {
            /* mpi->start_byte is too large for actual mpage length */
            pr2serr("The start_byte of ");
            if (mpi->acron)
                pr2serr("%s ", mpi->acron);
            else
                pr2serr("0x%x:%d:%d ", mpi->start_byte, mpi->start_bit,
                        mpi->num_bits);
            pr2serr("exceeds length of this mode page: %d [0x%x]\n", len,
                    len);
            if (op->flexible)
                pr2serr("    applying anyway\n");
            else {
                pr2serr("    nothing modified, use '--flexible' to "
                        "override\n");
                return SG_LIB_CAT_MALFORMED;
            }
        }
        if ((ivp->val >= 0) && (ivp->orig_val > 0) &&
            (ivp->orig_val > ivp->val) && (0 == op->do_quiet)) {
            pr2serr("warning: given value (%" PRId64 ") truncated "
                    "to %" PRId64 " by field size [%d bits]\n", ivp->orig_val,
                    ivp->val, mpi->num_bits);
            pr2serr("    applying anyway\n");
        }
        sdp_mp_set_value(ivp->val, mpi, mdpg + off);
    }

    if ((! (mdpg[off] & 0x80)) && op->save) {
        pr2serr("%s: mode page indicates it is not savable but\n"
                "    '--save' option given (try without it)\n", __func__);
        return SG_LIB_CAT_MALFORMED;
    }
    mdpg[off] &= 0x7f;   /* mask out PS bit, reserved in mode select */
    if (op->dummy) {
        pr2serr("Mode data that would have been written:\n");
        dStrHexErr((const char *)mdpg, md_len, 1);
        return 0;
    }
    if (op->mode_6)
        res = sg_ll_mode_select6(sg_fd, 1 /* PF */, op->save, mdpg, md_len,
                                 1, op->verbose);
    else
        res = sg_ll_mode_select10(sg_fd, 1, op->save, mdpg, md_len, 1,
                                  op->verbose);
    if (0 != res) {
        pr2serr("%s: failed setting page: %s\n", __func__, b);
        return res;
    }
    return 0;
}

/* Return of 0 -> success, various SG_LIB_CAT_* positive values,
 * -1 -> other failure */
static int
set_def_mode_page(int sg_fd, int pn, int spn, unsigned char * mode_pg,
                  int mode_pg_len, const struct sdparm_opt_coll * op)
{
    int len, off, md_len;
    unsigned char * mdp;
    char ebuff[EBUFF_SZ];
    int ret = -1;
    char buff[128];

    len = mode_pg_len + MODE_DATA_OVERHEAD;
    mdp = (unsigned char *)malloc(len);
    if (NULL ==mdp) {
        pr2serr("%s: malloc failed, out of memory\n", __func__);
        return SG_LIB_FILE_ERROR;
    }
    memset(mdp, 0, len);
    ret = ll_mode_sense(sg_fd, op, pn, spn, mdp, 4, op->verbose);
    if (0 != ret) {
        sdp_get_mpage_name(pn, spn, -1, op->transport, op->vendor, 0, 0,
                           buff, sizeof(buff));
        pr2serr("%s: failed fetching page: %s\n", __func__, buff);
        goto err_out;
    }
    md_len = op->mode_6 ? (mdp[0] + 1) : (sg_get_unaligned_be16(mdp) + 2);
    if (md_len > len) {
        pr2serr("%s: mode data length=%d exceeds allocation length=%d\n",
                __func__, md_len, len);
        ret = SG_LIB_CAT_MALFORMED;
        goto err_out;
    }
    ret = ll_mode_sense(sg_fd, op, pn, spn, mdp, md_len, op->verbose);
    if (0 != ret) {
        sdp_get_mpage_name(pn, spn, -1, op->transport, op->vendor,
                           0, 0, buff, sizeof(buff));
        pr2serr("%s: failed fetching page: %s\n", __func__, buff);
        goto err_out;
    }
    off = sg_mode_page_offset(mdp, len, op->mode_6, ebuff, EBUFF_SZ);
    if (off < 0) {
        pr2serr("%s: page offset failed: %s\n", __func__, ebuff);
        ret = SG_LIB_CAT_MALFORMED;
        goto err_out;
    }
    mdp[0] = 0;        /* mode data length reserved for mode select */
    if (! op->mode_6)
        mdp[1] = 0;    /* mode data length reserved for mode select */
    if ((md_len - off) > mode_pg_len) {
        pr2serr("%s: mode length length=%d exceeds new contents length=%d\n",
                __func__, md_len - off, mode_pg_len);
        ret = SG_LIB_CAT_MALFORMED;
        goto err_out;
    }
    memcpy(mdp + off, mode_pg, md_len - off);
    mdp[off] &= 0x7f;   /* mask out PS bit, reserved in mode select */
    if (op->dummy) {
        pr2serr("Mode data that would have been written:\n");
        dStrHexErr((const char *)mdp, md_len, 1);
        ret = 0;
        goto err_out;
    }
    if (op->mode_6)
        ret = sg_ll_mode_select6(sg_fd, 1 /* PF */, op->save, mdp, md_len, 1,
                                 op->verbose);
    else
        ret = sg_ll_mode_select10(sg_fd, 1, op->save, mdp, md_len, 1,
                                  op->verbose);
    if (0 != ret) {
        sdp_get_mpage_name(pn, spn, -1, op->transport, op->vendor,
                           0, 0, buff, sizeof(buff));
        pr2serr("%s: failed setting page: %s\n", __func__, buff);
        goto err_out;
    }

err_out:
    free(mdp);
    return ret;
}

static int
set_mp_defaults(int sg_fd, int pn, int spn, int pdt,
                const struct sdparm_opt_coll * op)
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
    res = sg_get_mode_page_controls(sg_fd, op->mode_6, pn, spn, op->dbd,
                 op->flexible, DEF_MODE_RESP_LEN, &smask, pc_arr,
                 &rep_len, op->verbose);
    if (SG_LIB_CAT_INVALID_OP == res) {
        if (op->mode_6)
            pr2serr("6 byte MODE SENSE cdb not supported, try again without "
                    "'-6' option\n");
        else
            pr2serr("10 byte MODE SENSE cdb not supported, try again with "
                    "'-6' option\n");
        return res;
    }
    else if (SG_LIB_CAT_NOT_READY == res) {
        pr2serr("MODE SENSE failed, device not ready\n");
        return res;
    } else if (SG_LIB_CAT_ABORTED_COMMAND == res) {
        pr2serr("MODE SENSE failed, aborted command\n");
        return res;
    }
    if (op->verbose && (0 == op->flexible) && (rep_len > 0xa00)) {
        sdp_get_mpage_name(pn, spn, pdt, op->transport, op->vendor,
                           0, 0, buff, sizeof(buff));
        pr2serr("%s mode page length=%d too long, perhaps try '--flexible'\n",
                buff, rep_len);
    }
    if ((smask & 1)) {
        len = sdp_get_mp_len(cur_mp);
        if ((smask & 4)) {
            return set_def_mode_page(sg_fd, pn, spn, def_mp, len, op);
        }
        else {
            sdp_get_mpage_name(pn, spn, pdt, op->transport, op->vendor,
                               0, 0, buff, sizeof(buff));
            pr2serr(">> %s mode page (default) not supported\n", buff);
            return SG_LIB_CAT_ILLEGAL_REQ;
        }
    } else {
        sdp_get_mpage_name(pn, spn, pdt, op->transport, op->vendor,
                           0, 0, buff, sizeof(buff));
        pr2serr(">> %s mode page not supported\n", buff);
        return SG_LIB_CAT_ILLEGAL_REQ;
    }
}

/* Returns 64 bit signed integer given in either decimal or in hex. The
 * hex number is either preceded by "0x" or followed by "h". Returns -1
 * on error (so check for "-1" string before using this function). */
static int64_t
get_llnum(const char * buf)
{
    int res, len;
    int64_t num;
    uint64_t unum;

    if ((NULL == buf) || ('\0' == buf[0]))
        return -1;
    len = strlen(buf);
    if (('0' == buf[0]) && (('x' == buf[1]) || ('X' == buf[1]))) {
        res = sscanf(buf + 2, "%" SCNx64 "", &unum);
        num = unum;
    } else if ('H' == toupper(buf[len - 1])) {
        res = sscanf(buf, "%" SCNx64 "", &unum);
        num = unum;
    } else
        res = sscanf(buf, "%" SCNd64 "", &num);
    return (1 == res) ? num : -1;
}

static int
build_mp_settings(const char * arg, struct sdparm_mode_page_settings * mps,
                  struct sdparm_opt_coll * op, int clear, int get)
{
    int len, b_sz, num, from, cont, colon;
    unsigned int u;
    int64_t ll;
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
            (isdigit(buff[0]) && ('_' == buff[1]))) { /* expect acronym */
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
                        pr2serr("unable to decode: %s value\n", buff);
                        pr2serr("    expected: <acronym>[=<val>]\n");
                        return -1;
                    }
                }
            } else {
                strcpy(acron, buff);
                ivp->val = ((clear || get) ? 0 : -1);
            }
            if ((ecp = strchr(acron, '.'))) {
                strcpy(vb, acron);
                strncpy(acron, vb, ecp - acron);
                acron[ecp - acron] = '\0';
                strcpy(vb, ecp + 1);
                ivp->descriptor_num = get_llnum(vb);
                if (ivp->descriptor_num < 0) {
                    pr2serr("unable to decode: %s descriptor number\n", buff);
                    pr2serr("    expected: <acronym_name>"
                            "[.<desc_num>][=<val>]\n");
                    return -1;
                }
            }
            ivp->orig_val = ivp->val;
            from = 0;
            cont = 0;
            prev_mpi = NULL;
            if (get) {
                do {
                    mpi = sdp_find_mitem_by_acron(acron, &from,
                                         op->transport,
                                         op->vendor);
                    if (NULL == mpi) {
                        if (cont) {
                            mpi = prev_mpi;
                            break;
                        }
                        if ((op->vendor < 0) && (op->transport < 0)) {
                            from = 0;
                            mpi = sdp_find_mitem_by_acron(acron, &from,
                                          DEF_TRANSPORT_PROTOCOL, -1);
                            if (NULL == mpi) {
                                pr2serr("couldn't find field acronym: %s\n",
                                        acron);
                                pr2serr("    [perhaps a '--transport=<tn>' "
                                        "or '--vendor=<vn>' option is "
                                        "needed]\n");
                                return -1;
                            } else /* keep going in this case */
                                op->transport = DEF_TRANSPORT_PROTOCOL;
                        } else {
                            pr2serr("couldn't find field acronym: %s\n",
                                    acron);
                            return -1;
                        }
                    }
                    if (mps->page_num < 0) {
                        mps->page_num = mpi->page_num;
                        mps->subpage_num = mpi->subpage_num;
                        break;
                    }
                    cont = 1;
                    prev_mpi = mpi;
                    /* got acronym match but if not at specified pn,spn */
                    /* then keep searching */
                } while ((mps->page_num != mpi->page_num) ||
                         (mps->subpage_num != mpi->subpage_num));
            } else {    /* --set or --clear */
                do {
                    mpi = sdp_find_mitem_by_acron(acron, &from,
                                 op->transport, op->vendor);
                    if (NULL == mpi) {
                        if (cont) {
                            pr2serr("mode page of acronym: %s [0x%x,0x%x] "
                                    "doesn't match prior\n", acron,
                                    prev_mpi->page_num,
                                    prev_mpi->subpage_num);
                            pr2serr("    mode page: 0x%x,0x%x\n",
                                    mps->page_num, mps->subpage_num);
                            pr2serr("For '--set' and '--clear' all fields "
                                    "must be in the same mode page\n");
                            return -1;
                        }
                        if ((op->vendor < 0) && (op->transport < 0)) {
                            from = 0;
                            mpi = sdp_find_mitem_by_acron(acron, &from,
                                          DEF_TRANSPORT_PROTOCOL, -1);
                            if (NULL == mpi) {
                                pr2serr("couldn't find field acronym: %s\n",
                                        acron);
                                pr2serr("    [perhaps a '--transport=<tn>' "
                                        "or '--vendor=<vn>' option is "
                                        "needed]\n");
                                return -1;
                            } else /* keep going in this case */
                                op->transport = DEF_TRANSPORT_PROTOCOL;
                        } else {
                            pr2serr("couldn't find field acronym: %s\n",
                                    acron);
                            return -1;
                        }
                    }
                    if (mps->page_num < 0) {
                        mps->page_num = mpi->page_num;
                        mps->subpage_num = mpi->subpage_num;
                        break;
                    }
                    cont = 1;
                    prev_mpi = mpi;
                    /* got acronym match but if not at specified pn,spn */
                    /* then keep searching */
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
                num = sscanf(buff + 2, "%x:%d:%d=%62s", &u,
                             &ivp->mpi.start_bit, &ivp->mpi.num_bits, vb);
                ivp->mpi.start_byte = u;
            } else {
                if (strstr(buff, "h:")) {
                    num = sscanf(buff, "%xh:%d:%d=%62s", &u,
                                 &ivp->mpi.start_bit, &ivp->mpi.num_bits, vb);
                    ivp->mpi.start_byte = u;
                } else if (strstr(buff, "H:")) {
                    num = sscanf(buff, "%xH:%d:%d=%62s", &u,
                                 &ivp->mpi.start_bit, &ivp->mpi.num_bits, vb);
                    ivp->mpi.start_byte = u;
                } else
                    num = sscanf(buff, "%d:%d:%d=%62s", &ivp->mpi.start_byte,
                                 &ivp->mpi.start_bit, &ivp->mpi.num_bits, vb);
            }
            if (num < 3) {
                pr2serr("unable to decode: %s\n", buff);
                pr2serr("    expected: start_byte:start_bit:num_bits[=<val>]"
                        "\n");
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
                        pr2serr("unable to decode start_byte:start_bit:"
                                "num_bits value\n");
                        return -1;
                    }
                }
            }
            ivp->mpi.pdt = -1;  /* don't known pdt now, so don't restrict */
            if (ivp->mpi.start_byte < 0) {
                pr2serr("need positive start byte offset\n");
                return -1;
            }
            if ((ivp->mpi.start_bit < 0) || (ivp->mpi.start_bit > 7)) {
                pr2serr("need start bit in 0..7 range (inclusive)\n");
                return -1;
            }
            if ((ivp->mpi.num_bits < 1) || (ivp->mpi.num_bits > 64)) {
                pr2serr("need number of bits in 1..64 range (inclusive)\n");
                return -1;
            }
            if (mps->page_num < 0) {
                pr2serr("need '--page=' option for mode page name or "
                        "number\n");
                return -1;
            } else if (get) {
                ivp->mpi.page_num = mps->page_num;
                ivp->mpi.subpage_num = mps->subpage_num;
            }
            ivp->orig_val = ivp->val;
            if (ivp->mpi.num_bits < 64) {
                int64_t ll = 1;

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

static int
open_and_simple_inquiry(const char * device_name, int rw, int * pdt,
                        int * protect, const struct sdparm_opt_coll * op)
{
    int res, verb, sg_fd, l_pdt;
    struct sg_simple_inquiry_resp sir;
    char b[32];

    verb = (op->verbose > 0) ? op->verbose - 1 : 0;
    sg_fd = sg_cmds_open_device(device_name, ! rw /* read_only */ , verb);
    if (sg_fd < 0) {
        pr2serr("open error: %s [%s]: %s\n", device_name,
                (rw ? "read/write" : "read only"), safe_strerror(-sg_fd));
        return -1;
    }
    res = sg_simple_inquiry(sg_fd, &sir, 0, verb);
    if (res) {
#ifdef SG_LIB_LINUX
        if (-1 == res) {
            int sg_sg_fd;

            sg_sg_fd = map_if_lk24(sg_fd, device_name, rw, op->verbose);
            if (sg_sg_fd < 0)
                goto err_out;
            sg_cmds_close_device(sg_fd);
            sg_fd = sg_sg_fd;
            res = sg_simple_inquiry(sg_fd, &sir, 0, verb);
            if (sg_sg_fd < 0)
                goto err_out;
        }
#endif  /* SG_LIB_LINUX */
        if (res) {
            pr2serr("SCSI INQUIRY command failed on %s\n", device_name);
            goto err_out;
        }
    }
    l_pdt = sir.peripheral_type;
    if ((PDT_WO == l_pdt) || (PDT_OPTICAL == l_pdt))
        *pdt = PDT_DISK;       /* map disk-like pdt's to PDT_DOSK */
    else
        *pdt = l_pdt;
    if (protect)
        *protect = sir.byte_5 & 0x1;     /* PROTECT bit SPC-3 and later */
    if ((0 == op->do_hex) && (0 == op->do_quiet)) {
        printf("    %s: %.8s  %.16s  %.4s",
               device_name, sir.vendor, sir.product, sir.revision);
        if (0 != l_pdt)
            printf("  [%s]", sg_get_pdt_str(l_pdt, sizeof(b), b));
        printf("\n");
    }
    return sg_fd;

err_out:
    sg_cmds_close_device(sg_fd);
    return -1;
}

/* Process mode page(s) operation. Returns 0 if successful */
static int
process_mode(int sg_fd, const struct sdparm_mode_page_settings * mps, int pn,
             int spn, int set_clear, int get,
             const struct sdparm_opt_coll * op, int pdt)
{
    int res;
    const struct sdparm_mode_page_t * mpp;

    if ((pn > 0x3e) || (spn > 0xfe)) {
        if ((0x3f == pn) || (0xff == spn))
            pr2serr("Does not support requesting all mode pages or subpages "
                    "this way.\n  Try '--all' option.\n");
        else
            pr2serr("Accepts mode page numbers from 0 to 62 .\n"
                    "  Accepts mode subpage numbers from 0 to 254 .\n"
                    "  For VPD pages add a '--inquiry' option.\n");
        return SG_LIB_SYNTAX_ERROR;
    }
    if ((pn > 0) && (pdt >= 0)) {
        mpp = sdp_get_mode_detail(pn, spn, pdt, op->transport, op->vendor);
        if (NULL == mpp)
            mpp = sdp_get_mode_detail(pn, spn, -1, op->transport, op->vendor);
        if (mpp && mpp->name && (mpp->pdt >= 0) && (pdt != mpp->pdt)) {
            pr2serr(">> Warning: %s mode page associated with\n", mpp->name);
            pr2serr("   peripheral device type 0x%x but device pdt is 0x%x\n",
                    mpp->pdt, pdt);
            if (! op->flexible)
                pr2serr("   may need '--flexible' option to override\n");
        }
    }
    if (op->defaults)
        res = set_mp_defaults(sg_fd, pn, spn, pdt, op);
    else if (set_clear) {
        if (mps->num_it_vals < 1) {
            pr2serr("no fields found to set or clear\n");
            return SG_LIB_CAT_OTHER;
        }
        res = change_mode_page(sg_fd, pdt, mps, op);
        if (res)
            return res;
    } else if (get) {
        if (mps->num_it_vals < 1) {
            pr2serr("no fields found to get\n");
            return SG_LIB_CAT_OTHER;
        }
        res = print_mode_items(sg_fd, mps, pdt, op);
    } else
        res = print_mode_pages(sg_fd, pn, spn, pdt, op);
    return res;
}


int
main(int argc, char * argv[])
{
    int sg_fd, res, c, pdt, req_pdt, k, orig_transport, r;
    struct sdparm_opt_coll opts;
    struct sdparm_opt_coll * op;
    const char * clear_str = NULL;
    const char * cmd_str = NULL;
    const char * get_str = NULL;
    const char * set_str = NULL;
    const char * page_str = NULL;
    const char * device_name_arr[MAX_DEV_NAMES];
    int num_devices = 0;
    int pn = -1;
    int spn = -1;
    int rw = 0;         /* 1: requires RDWR, 0: perhaps RDONLY ok */
    int set_clear = 0;
    int protect = 0;
    int do_help = 0;
    int cmd_arg = -1;
    const struct sdparm_mode_page_t * mpp = NULL;
    const struct sdparm_transport_id_t * tip;
    const struct sdparm_vpd_page_t * vpp = NULL;
    const struct sdparm_vendor_name_t * vnp;
    struct sdparm_mode_page_settings mp_settings;
    char * cp;
    const char * ccp;
    const struct sdparm_command_t * scmdp = NULL;
    int ret = 0;
#ifdef SG_LIB_WIN32
    int do_wscan = 0;
#endif

    op = &opts;
    memset(op, 0, sizeof(opts));
    op->transport = -1;
    op->vendor = -1;
    op->pdt = -1;
    memset(device_name_arr, 0, sizeof(device_name_arr));
    memset(&mp_settings, 0, sizeof(mp_settings));
    pdt = -1;
    while (1) {
        int option_index = 0;

#ifdef SG_LIB_WIN32
        c = getopt_long(argc, argv, "6aBc:C:dDefg:hHiI:lM:np:P:qrs:St:vVw",
                        long_options, &option_index);
#else
        c = getopt_long(argc, argv, "6aBc:C:dDefg:hHiI:lM:np:P:qrs:St:vV",
                        long_options, &option_index);
#endif
        if (c == -1)
            break;

        switch (c) {
        case '6':
            ++op->mode_6;
            break;
        case 'a':
            ++op->do_all;
            break;
        case 'B':
            ++op->dbd;
            break;
        case 'c':
            clear_str = optarg;
            set_clear = 1;
            rw = 1;
            break;
        case 'C':
            cmd_str = optarg;
        case 'd':
            ++op->dummy;
            break;
        case 'D':
            ++op->defaults;
            rw = 1;
            break;
        case 'e':
            ++op->do_enum;
            break;
        case 'f':
            ++op->flexible;
            break;
        case 'g':
            get_str = optarg;
            break;
        case 'h':
        case '?':
            ++do_help;
            break;
        case 'H':
            ++op->do_hex;
            break;
        case 'i':
            ++op->inquiry;
            break;
        case 'I':
            op->inhex_fn = optarg;
            break;
        case 'l':
            ++op->do_long;
            break;
        case 'M':
            if (isalpha(optarg[0])) {
                vnp = sdp_find_vendor_by_acron(optarg);
                if (NULL == vnp) {
                    pr2serr("abbreviation does not match a vendor\n");
                    printf("Available vendors:\n");
                    enumerate_vendors();
                    return SG_LIB_SYNTAX_ERROR;
                } else
                    op->vendor = vnp->vendor_num;
            } else {
                const struct sdparm_vendor_pair * vpp;

                res = sg_get_num_nomult(optarg);
                vpp = sdp_get_vendor_pair(res);
                if (NULL == vpp) {
                    pr2serr("Bad vendor value after '-M' (or '--vendor=') "
                            "option\n");
                    printf("Available vendors:\n");
                    enumerate_vendors();
                    return SG_LIB_SYNTAX_ERROR;
                }
                op->vendor = res;
            }
            break;
        case 'n':
            ++op->num_desc;
            break;
        case 'q':
            ++op->do_quiet;
            break;
        case 'p':
            if (page_str) {
                pr2serr("only one '--page=' option permitted\n");
                usage(1);
                return SG_LIB_SYNTAX_ERROR;
            } else
                page_str = optarg;
            break;
        case 'P':
            if ('-' == optarg[0])
                op->pdt = -1;           /* those in SPC */
            else {
                op->pdt = sg_get_num_nomult(optarg);
                if ((op->pdt < 0) || (op->pdt > 0x1f)) {
                    pr2serr("--pdt argument should be -1 to 31\n");
                    return SG_LIB_SYNTAX_ERROR;
                }
            }
            break;
        case 'r':
            ++op->read_only;
            break;
        case 's':
            set_str = optarg;
            rw = 1;
            set_clear = 1;
            break;
        case 'S':
            ++op->save;
            break;
        case 't':
            if (isalpha(optarg[0])) {
                tip = sdp_find_transport_by_acron(optarg);
                if (NULL == tip) {
                    pr2serr("abbreviation does not match a transport "
                            "protocol\n");
                    printf("Available transport protocols:\n");
                    enumerate_transports();
                    return SG_LIB_SYNTAX_ERROR;
                } else
                    op->transport = tip->proto_num;
            } else {
                res = sg_get_num_nomult(optarg);
                if ((res < 0) || (res > 15)) {
                    pr2serr("Bad transport value after '-t' option\n");
                    printf("Available transport protocols:\n");
                    enumerate_transports();
                    return SG_LIB_SYNTAX_ERROR;
                }
                op->transport = res;
            }
            break;
        case 'v':
            ++op->verbose;
            break;
        case 'V':
            pr2serr("version: %s\n", version_str);
            return 0;
#ifdef SG_LIB_WIN32
        case 'w':
            ++do_wscan;
            break;
#endif
        default:
            pr2serr("unrecognised option code 0x%x ??\n", c);
            usage(1);
            return SG_LIB_SYNTAX_ERROR;
        }
    }
    while (optind < argc) {
        if (num_devices < MAX_DEV_NAMES) {
            device_name_arr[num_devices++] = argv[optind];
            ++optind;
        } else {
            for (; optind < argc; ++optind)
                pr2serr("Unexpected extra argument: %s\n", argv[optind]);
            usage(1);
            return SG_LIB_SYNTAX_ERROR;
        }
    }

    if (do_help) {
        usage(do_help);
        return 0;
    }

    if (op->read_only)
        rw = 0;         // override any read-write settings

    if ((!!get_str + !!set_str + !!clear_str) > 1) {
        pr2serr("Can only give one of '--get=', '--set=' and '--clear='\n");
        return SG_LIB_SYNTAX_ERROR;
    }
#ifdef SG_LIB_WIN32
    if (do_wscan)
        return sg_do_wscan('\0', do_wscan, op->verbose);
#endif

    if (page_str) {
        if ((0 == strcmp("-1", page_str)) || (0 == strcmp("-2", page_str))) {
            op->inquiry = 1;
            pn = VPD_NOT_STD_INQ;
        } else if (isalpha(page_str[0])) {
            while ( 1 ) {      /* dummy loop, just using break */
                mpp = sdp_find_mp_by_acron(page_str, op->transport,
                                           op->vendor);
                if (mpp)
                    break;
                vpp = sdp_find_vpd_by_acron(page_str);
                if (vpp)
                    break;
                orig_transport = op->transport;
                if ((op->vendor < 0) && (op->transport < 0)) {
                    op->transport = DEF_TRANSPORT_PROTOCOL;
                    mpp = sdp_find_mp_by_acron(page_str, op->transport,
                                               op->vendor);
                    if (mpp)
                        break;
                }
                if ((op->vendor < 0) && (orig_transport < 0))
                    pr2serr("abbreviation matches neither a mode page nor "
                            "a VPD page\n"
                            "    [perhaps a '--transport=<tn>' or "
                            "'--vendor=<vn>' option is needed]\n");
                else
                    pr2serr("abbreviation matches neither a mode page nor "
                            "a VPD page\n");
                if (op->inquiry) {
                    printf("available VPD pages:\n");
                    enumerate_vpds();
                    return SG_LIB_SYNTAX_ERROR;
                } else {
                    printf("available mode pages");
                    if (op->vendor >= 0)
                        printf(" (for given vendor):\n");
                    else if (orig_transport >= 0)
                        printf(" (for given transport):\n");
                    else
                        printf(":\n");
                    enumerate_mpages(orig_transport, op->vendor);
                    return SG_LIB_SYNTAX_ERROR;
                }
            }
            if (vpp) {
                pn = vpp->vpd_num;
                spn = vpp->subvalue;
                op->inquiry = 1;
                pdt = vpp->pdt;
            } else {
                if (op->inquiry) {
                    pr2serr("matched mode page acronym but given '-i' so "
                            "expecting a VPD page\n");
                    return SG_LIB_SYNTAX_ERROR;
                }
                pn = mpp->page;
                spn = mpp->subpage;
                pdt = mpp->pdt;
            }
        } else {        /* got page_str and first char probably numeric */
            cp = (char *)strchr(page_str, ',');
            pn = sg_get_num_nomult(page_str);
            if ((pn < 0) || (pn > 255)) {
                pr2serr("Bad page code value after '-p' option\n");
                if (op->inquiry) {
                    printf("available VPD pages:\n");
                    enumerate_vpds();
                    return SG_LIB_SYNTAX_ERROR;
                } else {
                    printf("available mode pages");
                    if (op->vendor >= 0)
                        printf(" (for given vendor):\n");
                    else if (op->transport >= 0)
                        printf(" (for given transport):\n");
                    else
                        printf(":\n");
                    enumerate_mpages(op->transport, op->vendor);
                    return SG_LIB_SYNTAX_ERROR;
                }
            }
            if (cp) {
                spn = sg_get_num_nomult(cp + 1);
                if ((spn < 0) || (spn > 255)) {
                    pr2serr("Bad second value after '-p' option\n");
                    return SG_LIB_SYNTAX_ERROR;
                }
            } else
                spn = 0;
        }
    }

    if (op->inquiry) {
        if (set_clear || get_str || cmd_str || op->defaults || op->save) {
            pr2serr("'--inquiry' option lists VPD pages so other options "
                    "that are\nconcerned with mode pages are "
                    "inappropriate\n");
            return SG_LIB_SYNTAX_ERROR;
        }
        if (pn > 255) {
            pr2serr("VPD page numbers are from 0 to 255\n");
            return SG_LIB_SYNTAX_ERROR;
        }
        if (op->do_enum) {
            printf("VPD pages:\n");
            enumerate_vpds();
            return 0;
        }
    } else if (cmd_str) {
        if (set_clear || get_str || op->defaults || op->save) {
            pr2serr("'--command=' option is not valid with other "
                    "options that are\nconcerned with mode pages\n");
            return SG_LIB_SYNTAX_ERROR;
        }
        if (op->do_enum) {
            printf("Available commands:\n");
            sdp_enumerate_commands();
            return 0;
        }
        scmdp = sdp_build_cmd(cmd_str, &rw, &cmd_arg);
        if (NULL == scmdp) {
            pr2serr("'--command=%s' not found\n", cmd_str);
            printf("available commands\n");
            sdp_enumerate_commands();
            return SG_LIB_SYNTAX_ERROR;
        }
        if (op->read_only)
            rw = 0;         // override any read-write settings
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
            if (set_clear) {
                pr2serr("'--get=' can't be used with '--set=' or "
                        "'--clear='\n");
                return SG_LIB_SYNTAX_ERROR;
            }
            if (build_mp_settings(get_str, &mp_settings, op, 0, 1))
                return SG_LIB_SYNTAX_ERROR;
        }
        if (op->do_enum) {
            if ((num_devices > 0) || set_clear || get_str || op->save)
                /* think about --get= with --enumerate */
                printf("<scsi_device> as well as most options are ignored "
                       "when '--enumerate' is given\n");
            if (pn < 0) {
                if (op->vendor >= 0) {
                    ccp = sdp_get_vendor_name(op->vendor);
                    if (ccp)
                        printf("Mode pages for %s vendor:\n", ccp);
                    else
                        printf("Mode pages for vendor 0x%x:\n", op->vendor);
                    if (op->do_all)
                        enumerate_mitems(pn, spn, pdt, op);
                    else {
                        enumerate_mpages(op->transport, op->vendor);
                    }
                } else if (op->transport >= 0) {
                    ccp = sdp_get_transport_name(op->transport);
                    if (ccp)
                        printf("Mode pages for %s transport protocol:\n",
                               ccp);
                    else
                        printf("Mode pages for transport protocol 0x%x:\n",
                               op->transport);
                    if (op->do_all)
                        enumerate_mitems(pn, spn, pdt, op);
                    else {
                        enumerate_mpages(op->transport, op->vendor);
                    }
                } else {        /* neither vendor nor transport given */
                    if (op->do_long || (op->do_enum > 1)) {
                        printf("Mode pages (not related to any transport "
                               "protocol or vendor):\n");
                        enumerate_mpages(-1, -1);
                        printf("\n");
                        printf("Transport protocols:\n");
                        enumerate_transports();
                        printf("\n");
                        printf("Vendors:\n");
                        enumerate_vendors();
                        if (op->do_all) {
                            struct sdparm_opt_coll t_opts;

                            printf("\n");
                            t_opts = opts;
                            enumerate_mitems(pn, spn, pdt, op);
                            for (k = 0; k < 16; ++k) {
                                ccp = sdp_get_transport_name(k);
                                if (NULL == ccp)
                                    continue;
                                printf("\n");
                                printf("Mode pages for %s transport "
                                       "protocol:\n", ccp);
                                t_opts.transport = k;
                                t_opts.vendor = -1;
                                enumerate_mitems(pn, spn, pdt, &t_opts);
                            }
                            for (k = 0; k < sdparm_vendor_mp_len; ++k) {
                                ccp = sdp_get_vendor_name(k);
                                if (NULL == ccp)
                                    continue;
                                printf("\n");
                                printf("Mode pages for %s vendor:\n", ccp);
                                t_opts.transport = -1;
                                t_opts.vendor = k;
                                enumerate_mitems(pn, spn, pdt, &t_opts);
                            }
                        } else {
                            for (k = 0; k < 16; ++k) {
                                ccp = sdp_get_transport_name(k);
                                if (NULL == ccp)
                                    continue;
                                printf("\n");
                                printf("Mode pages for %s transport "
                                       "protocol:\n", ccp);
                                enumerate_mpages(k, -1);
                            }
                            for (k = 0; k < sdparm_vendor_mp_len; ++k) {
                                ccp = sdp_get_vendor_name(k);
                                if (NULL == ccp)
                                    continue;
                                printf("\n");
                                printf("Mode pages for %s vendor:\n", ccp);
                                enumerate_mpages(-1, k);
                            }
                        }
                        printf("\n");
                        printf("Commands:\n");
                        sdp_enumerate_commands();
                    } else {
                        printf("Mode pages:\n");
                        enumerate_mpages(-1, -1);
                        if (op->do_all)
                            enumerate_mitems(pn, spn, pdt, op);
                    }
                }
            } else      /* given mode page number */
                enumerate_mitems(pn, spn, pdt, op);
            return 0;
        }

        if ((op->num_desc > 0) && (pn < 0)) {
            pr2serr("when '--num-desc' is given an explicit mode page is "
                    "required\n");
            return SG_LIB_SYNTAX_ERROR;
        }

        if (op->defaults && (set_clear || get_str)) {
            pr2serr("'--get=', '--set=' or '--clear=' "
                    "can't be used with '--defaults'\n");
            return SG_LIB_SYNTAX_ERROR;
        }

        if (set_str) {
            if (build_mp_settings(set_str, &mp_settings, op, 0, 0))
                return SG_LIB_SYNTAX_ERROR;
        }
        if (clear_str) {
            if (build_mp_settings(clear_str, &mp_settings, op, 1, 0))
                return SG_LIB_SYNTAX_ERROR;
        }

        if (op->verbose && (mp_settings.num_it_vals > 0))
            list_mp_settings(&mp_settings, (NULL != get_str));

        if (op->defaults && (pn < 0)) {
            pr2serr("to set defaults, the '--page=' option must be used\n");
            return SG_LIB_SYNTAX_ERROR;
        }
    }

    if (op->inhex_fn) {
        int inhex_len;

        if (num_devices > 0) {
            pr2serr("Cannot have both a DEVICE and --inhex= option\n");
            return SG_LIB_SYNTAX_ERROR;
        }
        if (f2hex_arr(op->inhex_fn, (op->do_hex > 1), 0, inhex_buff,
                      &inhex_len, sizeof(inhex_buff)))
            return SG_LIB_FILE_ERROR;
        if (op->verbose > 2)
            pr2serr("Read %d bytes from user input\n", inhex_len);
        if (op->verbose > 3)
            dStrHexErr((const char *)inhex_buff, inhex_len, 0);
        op->do_hex = 0;
        if (op->inquiry)
            return sdp_process_vpd_page(0, pn, ((spn < 0) ? 0: spn), op,
                                        pdt, protect, inhex_buff, inhex_len);
        else
            return print_mode_page_given(inhex_buff, inhex_len, op);
    }

    if (0 == num_devices) {
        pr2serr("one or more device names required\n");
        usage(1);
        return SG_LIB_SYNTAX_ERROR;
    }

    req_pdt = pdt;
    ret = 0;
    for (k = 0; k < num_devices; ++k) {
        r = 0;
        pdt = -1;
        if (op->verbose > 1)
            pr2serr(">>> about to open device name: %s\n",
                    device_name_arr[k]);
        sg_fd = open_and_simple_inquiry(device_name_arr[k], rw, &pdt,
                                        &protect, op);
        if (sg_fd < 0) {
            r = SG_LIB_FILE_ERROR;
            if (0 == ret)
                ret = r;
            continue;
        }

        if (op->inquiry)
            r = sdp_process_vpd_page(sg_fd, pn, ((spn < 0) ? 0: spn), op,
                                     req_pdt, protect, NULL, 0);
        else {
            if (cmd_str && scmdp)   /* process command */
                r = sdp_process_cmd(sg_fd, scmdp, cmd_arg, pdt, op);
            else                    /* mode page */
                r = process_mode(sg_fd, &mp_settings, pn, spn, set_clear,
                                 (NULL != get_str), op, pdt);
        }

        res = sg_cmds_close_device(sg_fd);
        if (res < 0) {
            pr2serr("close error: %s\n", safe_strerror(-sg_fd));
            if (0 == r)
                r = SG_LIB_FILE_ERROR;
        }
        if (r  && ((0 == ret) || (SG_LIB_FILE_ERROR == ret)))
            ret = r;
    }
    return (ret >= 0) ? ret : SG_LIB_CAT_OTHER;
}

#ifdef SG_LIB_LINUX
/* Following needed for lk 2.4 series; may be dropped in future.
 * In the lk 2.4 series (and earlier) no pass-through was available on the
 * often used /dev/sd* device nodes. So the code below attempts to map a
 * given /dev/sd<n> device node to the corresponding /dev/sg<m> device
 * node.
 */

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
static int
find_corresponding_sg_fd(int oth_fd, const char * device_name, int rw,
                         int verbose)
{
    int fd, err, bus, bbus, k, v;
    My_scsi_idlun m_idlun, mm_idlun;
    char name[DEVNAME_SZ];
    int num_nodevs = 0;
    int num_sgdevs = 0;

    err = ioctl(oth_fd, SCSI_IOCTL_GET_BUS_NUMBER, &bus);
    if (err < 0) {
        pr2serr("%s does not present a standard Linux SCSI device "
                "interface (a\nSCSI_IOCTL_GET_BUS_NUMBER ioctl to it "
                "failed). Examples of typical\n", device_name);
        pr2serr("names of devices that do are /dev/sda, /dev/scd0, "
                "dev/st0, /dev/osst0,\nand /dev/sg0. An example of a "
                "typical non-SCSI device name is /dev/hdd.\n");
        return -2;
    }
    err = ioctl(oth_fd, SCSI_IOCTL_GET_IDLUN, &m_idlun);
    if (err < 0) {
        if (verbose)
            pr2serr("%s does not understand SCSI commands(2)\n", device_name);
        return -2;
    }

    fd = -2;
    for (k = 0; (k < MAX_SG_DEVS) && (num_nodevs < MAX_NUM_NODEVS); k++) {
        snprintf(name, sizeof(name), "/dev/sg%d", k);
        fd = open(name, O_RDONLY | O_NONBLOCK);
        if (fd < 0) {
            if (ENODEV != errno)
                ++num_sgdevs;
            if ((ENODEV == errno) || (ENOENT == errno) ||
                (ENXIO == errno)) {
                ++num_nodevs;
                continue;       /* step over MAX_NUM_NODEVS holes */
            }
            if (EBUSY == errno)
                continue;   /* step over if O_EXCL already on it */
            else
                break;
        } else
            ++num_sgdevs;
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
    if (0 == num_sgdevs) {
        pr2serr("No /dev/sg* devices found; is the sg driver loaded?\n");
        return -2;
    }
    if (fd >= 0) {
        if ((ioctl(fd, SG_GET_VERSION_NUM, &v) < 0) || (v < 30000)) {
            pr2serr("requires lk 2.4 (sg driver) or lk 2.6\n");
            close(fd);
            return -2;
        }
        close(fd);
        if (verbose)
            pr2serr(">> mapping %s to %s (in lk 2.4 series)\n", device_name,
                    name);
        /* re-opening corresponding sg device with given rw setting */
        return open(name, O_NONBLOCK | (rw ? O_RDWR : O_RDONLY));
    }
    else
        return fd;
}

static int
map_if_lk24(int sg_fd, const char * device_name, int rw, int verbose)
{
    /* could be lk 2.4 and not using a sg device */
    struct utsname a_uts;
    int two, four;
    int res;
    struct stat a_stat;

    if (stat(device_name, &a_stat) < 0) {
        pr2serr("unable to 'stat' %s, errno=%d\n", device_name, errno);
        perror("stat failed");
        return -1;
    }
    if ((! S_ISBLK(a_stat.st_mode)) && (! S_ISCHR(a_stat.st_mode))) {
        pr2serr("expected %s to be a block or char device\n", device_name);
        return -1;
    }
    if (uname(&a_uts) < 0) {
        pr2serr("uname system call failed, couldn't send SG_IO ioctl to %s\n",
                device_name);
        return -1;
    }
    res = sscanf(a_uts.release, "%d.%d", &two, &four);
    if (2 != res) {
        pr2serr("unable to read uname release\n");
        return -1;
    }
    if (! ((2 == two) && (4 == four))) {
        pr2serr("unable to access %s, ATA disk?\n", device_name);
        return -1;
    }
    return find_corresponding_sg_fd(sg_fd, device_name, rw, verbose);
}

#endif  /* SG_LIB_LINUX */

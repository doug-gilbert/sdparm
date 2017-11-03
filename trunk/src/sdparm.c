/*
 * Copyright (c) 2005-2017 Douglas Gilbert.
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
#include <stdarg.h>
#include <stdbool.h>
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

static int map_if_lk24(int sg_fd, const char * device_name, bool rw,
                       int verbose);

#endif  /* SG_LIB_LINUX */

#include "sg_lib.h"
#include "sg_cmds_basic.h"
#include "sg_unaligned.h"
#include "sg_pr2serr.h"
#include "sdparm.h"

static const char * version_str = "1.11 20171103 [svn: r298]";


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
    {"num_desc", no_argument, 0, 'n'},
    {"page", required_argument, 0, 'p'},
    {"pdt", required_argument, 0, 'P'},
    {"quiet", no_argument, 0, 'q'},
    {"raw", no_argument, 0, 'R'},
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

static const char * ms_s = "MODE SENSE";


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
            "       sdparm --command=CMD [-hex] [--long] [--readonly] "
            "[--verbose]\n"
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
            "              [--long] [--pdt=PDT] [--raw] [--six] "
            "[--transport=TN]\n"
            "              [--vendor=VN]\n"
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
            "    --raw | -R            FN (in '-I FN') assumed to be "
            "binary\n"
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
 * from and including a '#' on a line is ignored. Returns true if ok, or
 * false if error. */
static bool
f2hex_arr(const char * fname, bool as_binary, bool no_space,
          unsigned char * mp_arr, int * mp_arr_len, int max_arr_len)
{
    bool split_line, has_stdin;
    int fn_len, in_len, k, j, m, fd;
    unsigned int h;
    const char * lcp;
    FILE * fp;
    char line[512];
    char carry_over[4];
    int off = 0;
    struct stat a_stat;

    if ((NULL == fname) || (NULL == mp_arr) || (NULL == mp_arr_len))
        return false;
    fn_len = strlen(fname);
    if (0 == fn_len)
        return false;
    has_stdin = ((1 == fn_len) && ('-' == fname[0]));   /* read from stdin */
    if (as_binary) {
        if (has_stdin)
            fd = STDIN_FILENO;
        else {
            fd = open(fname, O_RDONLY);
            if (fd < 0) {
                pr2serr("unable to open binary file %s: %s\n", fname,
                         safe_strerror(errno));
                return false;
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
            return false;
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
                    return false;
                }
                k += m;
            }
        }
        *mp_arr_len = k;
        if (! has_stdin)
            close(fd);
        return true;
    } else {    /* So read the file as ASCII hex */
        if (has_stdin)
            fp = stdin;
        else {
            fp = fopen(fname, "r");
            if (NULL == fp) {
                pr2serr("Unable to open %s for reading\n", fname);
                return false;
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
                split_line = false;
            } else
                split_line = true;
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
    return true;
bad:
    if (stdin != fp)
        fclose(fp);
    return false;
}

static void
enumerate_mpages(int transport, int vendor_id)
{
    const struct sdparm_mode_page_t * mpp;

    if (vendor_id >= 0) {
        const struct sdparm_vendor_pair * vpp;

        vpp = sdp_get_vendor_pair(vendor_id);
        if (NULL == vpp) {
            pr2serr("Bad vendor identifier (number)\n");
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
enumerate_transports(bool multiple_acrons, const struct sdparm_opt_coll * op)
{
    int len;
    const struct sdparm_val_desc_t * addp;
    const struct sdparm_val_desc_t * t_vdp;
    char b[64];
    char d[128];

    for (t_vdp = sdparm_transport_id; t_vdp->desc; ++t_vdp) {
        if (op->do_long || multiple_acrons) {
            snprintf(d, sizeof(d), "%s", t_vdp->desc);
            len = strlen(d);
            for (addp = sdparm_add_transport_acron; addp->desc; ++addp) {
                if ((addp->val == t_vdp->val) &&
                    ((int)sizeof(d) >= (len - 1))) {
                    snprintf(d + len, sizeof(d) - len, ",%s", addp->desc);
                    len = strlen(d);
                }
            }
            printf("  %-24s 0x%02x     %s\n", d, t_vdp->val,
                   sg_get_trans_proto_str(t_vdp->val, sizeof(b), b));
        } else
            printf("  %-6s 0x%02x     %s\n", t_vdp->desc, t_vdp->val,
                   sg_get_trans_proto_str(t_vdp->val, sizeof(b), b));
    }
}

static void
enumerate_vendors()
{
    const struct sdparm_vendor_name_t * vnp;

    for (vnp = sdparm_vendor_id; vnp->acron; ++vnp) {
        if (vnp->name && (VENDOR_NONE != vnp->vendor_id))
            printf("  %-6s 0x%02x     %s\n", vnp->acron, vnp->vendor_id,
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
    bool found = false;
    bool have_desc = false;
    bool have_desc_id = false;
    int t_pn, t_spn, t_pdt, vendor_id, transp, long_o, e_num;
    const struct sdparm_mode_page_t * mpp = NULL;
    const struct sdparm_mode_descriptor_t * mdp = NULL;
    const struct sdparm_mode_page_item * mpi;
    char d[128];
    char b[128];

    t_pn = -1;
    t_spn = -1;
    t_pdt = -2;
    vendor_id = op->vendor_id;
    transp = op->transport;
    long_o = op->do_long;
    e_num = op->do_enum;
    if (vendor_id >= 0) {
        const struct sdparm_vendor_pair * vpp;

        vpp = sdp_get_vendor_pair(vendor_id);
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
        if ((t_pn == mpi->pg_num) && (t_spn == mpi->subpg_num) &&
            (t_pdt == mpi->pdt)) {
            if ((pn >= 0) && ((pn != t_pn) || (spn != t_spn)))
                continue;
        } else {
            t_pn = mpi->pg_num;
            t_spn = mpi->subpg_num;
            t_pdt = mpi->pdt;
            if ((pn >= 0) && ((pn != t_pn) || (spn != t_spn)))
                continue;
            if ((pdt >= 0) && (pdt != t_pdt))
                continue;
            mpp = sdp_get_mp_with_str(t_pn, t_spn, t_pdt, transp, vendor_id,
                                      long_o, true, sizeof(d), d);
            if (long_o && (transp < 0) && (vendor_id < 0))
                printf("%s [%s] mode page:\n", d,
                       sg_get_pdt_str(t_pdt, sizeof(b), b));
            else
                printf("%s mode page:\n", d);
            if (mpp && ((mdp = mpp->mp_desc))) {
                have_desc = true;
                have_desc_id = mdp->have_desc_id;
            } else {
                have_desc = false;
                have_desc_id = false;
            }
        }
        if (have_desc_id && (MF_CLASH_OK & mpi->flags))
            printf("  %-10s [0x%02x:%d:%-2d  %d]  %s\n", mpi->acron,
                   mpi->start_byte, mpi->start_bit, mpi->num_bits,
                   sdp_get_desc_id(mpi->flags), mpi->description);
        else
            printf("  %-10s [0x%02x:%d:%-2d]  %s\n", mpi->acron,
                   mpi->start_byte, mpi->start_bit, mpi->num_bits,
                   mpi->description);
        if (mpi->extra) {
            if ((long_o > 1) || (e_num > 1))
                print_mp_extra(mpi->extra);
        }
        found = true;
    }
    if ((! found) && (pn >= 0)) {
        sdp_get_mp_with_str(pn, spn, pdt, transp, vendor_id,
                            long_o, true, sizeof(d), d);
        pr2serr("%s mode page: no items found\n", d);
    }
    if (found && have_desc && (long_o || (e_num > 1))) {
        if (mdp->name)
            printf("  <<%s mode descriptor>>\n", mdp->name);
        else
            printf("  <<mode descriptor>>\n");
        printf("    num_descs_off=%d, num_descs_bytes=%d, "
               "num_descs_inc=%d, first_desc_off=%d\n",
               mdp->num_descs_off, mdp->num_descs_bytes,
               mdp->num_descs_inc,  mdp->first_desc_off);
        if (mdp->desc_len > 0)
            printf("    descriptor_len=%d, ", mdp->desc_len);
        else
            printf("    desc_len_off=%d, desc_len_bytes=%d, ",
                   mdp->desc_len_off, mdp->desc_len_bytes);
        printf("desc_id=%s\n", (have_desc_id ? "true" : "false"));
    }
}

static void
list_mp_settings(const struct sdparm_mode_page_settings * mps, bool get)
{
    const struct sdparm_mode_page_item * mpip;
    int k;

    printf("mp_settings: page,subpage=0x%x,0x%x  num=%d\n",
           mps->pg_num, mps->subpg_num, mps->num_it_vals);
    for (k = 0; k < mps->num_it_vals; ++k) {
        mpip = &mps->it_vals[k].mpi;
        if (get)
            printf("  [0x%x,0x%x]", mpip->pg_num, mpip->subpg_num);

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
print_mitem(const char * pre, int smask,
            const struct sdparm_mode_page_item *mpi,
            const unsigned char * cur_mp,
            const unsigned char * cha_mp,
            const unsigned char * def_mp,
            const unsigned char * sav_mp,
            bool force_decimal,
            const struct sdparm_opt_coll * op)
{
    bool all_set = false;
    bool sep = false;
    uint64_t u;
    const char * acron;

    acron = (mpi->acron ? mpi->acron : "");
    u = sdp_mitem_get_value_check(mpi, cur_mp, &all_set);
    printf("%s%-14s", pre, acron);
    if (force_decimal || (mpi->flags & MF_TWOS_COMP))
        sdp_print_signed_decimal(u, mpi->num_bits, false);
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
                   (sdp_mitem_get_value(mpi, cha_mp) ? "y" : "n"));
            sep = true;
        }
        if (def_mp && (smask & 4)) {
            all_set = false;
            u = sdp_mitem_get_value_check(mpi, def_mp, &all_set);
            printf("%sdef:", (sep ? ", " : " "));
            if (force_decimal || (mpi->flags & MF_TWOS_COMP))
                sdp_print_signed_decimal(u, mpi->num_bits, false);
            else if (mpi->flags & MF_HEX)
                printf(" 0x%" PRIx64 "", u);
            else if (all_set)
                printf(" -1");
            else
                printf("%3" PRIu64 "", u);
            sep = true;
        }
        if (sav_mp && (smask & 8)) {
            all_set = false;
            u = sdp_mitem_get_value_check(mpi, sav_mp, &all_set);
            printf("%ssav:", (sep ? ", " : " "));
            if (force_decimal || (mpi->flags & MF_TWOS_COMP))
                sdp_print_signed_decimal(u, mpi->num_bits, false);
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
print_mitem_pc_arr(const char * pre, int smask,
                   const struct sdparm_mode_page_item *mpi,
                   void ** pc_arr, bool force_decimal,
                   const struct sdparm_opt_coll * op)
{
    const unsigned char * cur_mp = (const unsigned char *)pc_arr[0];
    const unsigned char * cha_mp = (const unsigned char *)pc_arr[1];
    const unsigned char * def_mp = (const unsigned char *)pc_arr[2];
    const unsigned char * sav_mp = (const unsigned char *)pc_arr[3];

    print_mitem(pre, smask, mpi, cur_mp, cha_mp, def_mp, sav_mp,
                force_decimal, op);
}

static int
ll_mode_sense(int fd, const struct sdparm_opt_coll * op, int pn, int spn,
              unsigned char * resp, int mx_resp_len, int * residp, int verb)
{
    if (op->mode_6) {
        if (residp)
            *residp = 0;
        return sg_ll_mode_sense6(fd, op->dbd, 0 /*current */, pn, spn,
                                 resp, mx_resp_len, true /* noisy */, verb);
    } else
        return sg_ll_mode_sense10_v2(fd, false /* llbaa */, op->dbd, 0, pn,
                                  spn, resp, mx_resp_len, true /* noisy */,
                                  0, residp, verb);
}

static int
report_error(int res, bool mode6)
{
    const char * cdbLenS = mode6 ? "6" : "10";

    switch (res) {
    case SG_LIB_CAT_INVALID_OP:
        pr2serr("%s(%s) cdb not supported, try again with%s '-6' "
                "option\n", ms_s, cdbLenS, mode6 ? "out" : "");
        break;
    case SG_LIB_CAT_NOT_READY:
        pr2serr("%s(%s) failed, device not ready\n", ms_s,
                cdbLenS);
        break;
    case SG_LIB_CAT_UNIT_ATTENTION:
        pr2serr("%s(%s) failed, unit attention\n", ms_s, cdbLenS);
        break;
    case SG_LIB_CAT_ABORTED_COMMAND:
        pr2serr("%s(%s), aborted command\n", ms_s, cdbLenS);
        break;
    default:
        pr2serr("%s(%s), res=%d\n", ms_s, cdbLenS, res);
        break;
    }
    return res;
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

/* When mode page has descriptor, print_mode_pages() and
 * print_mode_page_inhex() will print the items associated with the first
 * descriptor. This function is called to print items associated with
 * subsequent descriptors */
static void
print_mitem_desc_after1(void ** pc_arr, int rep_len,
                        const struct sdparm_mode_page_t * mpp,
                        const struct sdparm_mode_page_item * fdesc_mpi,
                        const struct sdparm_mode_page_item * last_mpi,
                        const struct sdparm_opt_coll * op,
                        int smask, int desc_len1)
{
    bool broke = false;
    bool have_desc_id = false;
    int k, len, d_off, n;
    int desc_id = -1;
    int num = 0;
    const struct sdparm_mode_descriptor_t * mdp = mpp->mp_desc;
    const struct sdparm_mode_page_item * mpi;
    const unsigned char * cur_mp = (const unsigned char *)pc_arr[0];
    const unsigned char * cha_mp = (const unsigned char *)pc_arr[1];
    const unsigned char * def_mp = (const unsigned char *)pc_arr[2];
    const unsigned char * sav_mp = (const unsigned char *)pc_arr[3];
    const unsigned char * bp;
    struct sdparm_mode_page_item ampi;
    uint64_t u;
    char b[32];

    if ((NULL == mdp) || (NULL == cur_mp) || (rep_len < 4) ||
        (mdp->num_descs_off >= rep_len))
        return;
    if ((mdp->num_descs_inc < 0) && (mdp->desc_len > 0)) {
        k = mdp->first_desc_off - (mdp->num_descs_off + mdp->num_descs_bytes);
        u = (sdp_get_big_endian(cur_mp + mdp->num_descs_off, 7,
                                mdp->num_descs_bytes * 8) - k) /
            mdp->desc_len;
        num = (int)u;
    } else if (mdp->num_descs_bytes > 0) {
        u = sdp_get_big_endian(cur_mp + mdp->num_descs_off, 7,
                       mdp->num_descs_bytes * 8) + mdp->num_descs_inc;
        num = (int)u;
    } else {
        int pg_len = sdp_mpage_len(cur_mp);
        int off = mdp->first_desc_off;
        int d_len = mdp->desc_len_off + mdp->desc_len_bytes;

        have_desc_id = true;
        bp = cur_mp + mdp->desc_len_off;
        while (off + d_len < pg_len) {
            ++num;
            u = sdp_get_big_endian(bp + off, 7,
                                   mdp->desc_len_bytes * 8);
            if (u > 1024) {
                pr2serr(">> mpage descriptor too large=%"
                        PRIu64 ", stop counting "
                        "descriptors\n", u);
                break;
            }
            off += (d_len + (int)u);
        }
    }
    if (op->verbose > 1)
        pr2serr("    >>> mode page says it has %d descriptors\n", num);

    if (((num >= 0) && (num < 2)) || (num > 512))
        return;
    if (have_desc_id) {
        if (desc_len1 <= 0) {
            pr2serr("%s: desc_id logic fails for acron: %s\n", __func__,
                    mpi->acron ? mpi->acron : "??");
            return;
        }
        len = desc_len1;
    } else if ((mdp->num_descs_inc < 0) && (mdp->desc_len > 0))
        len = mdp->desc_len;
    else /* if (mdp->num_descs_bytes > 0) */ {
        bp = cur_mp + mdp->first_desc_off + mdp->desc_len_off;
        u = sdp_get_big_endian(bp, 7, mdp->desc_len_bytes * 8);
        len = mdp->desc_len_off + mdp->desc_len_bytes + u;
    }
    d_off = mdp->first_desc_off + len;
    for (k = 1; k < num; ++k, d_off += len) {
        if (op->verbose > 3)
            pr2serr("%s: desc_len1=%d, d_off=%d, len=%d\n", __func__,
                    desc_len1, d_off, len);
        broke = false;
        for (mpi = fdesc_mpi; mpi <= last_mpi; ++mpi) {
            if (have_desc_id) {
                if (mpi->flags & MF_CLASH_OK) {
                    if (desc_id != sdp_get_desc_id(mpi->flags))
                        continue; /* skip this item due to desc_id mismatch */
                } else {
                    desc_id = 0xf & *(cur_mp + d_off);
                    len = sg_get_unaligned_be16(cur_mp + d_off + 2) + 4;
                }
            }
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
                broke = true;
                break;
            }
            print_mitem("  ", smask, &ampi, cur_mp, cha_mp, def_mp, sav_mp,
                        false, op);
        }
        if (broke)
            break;
        if ((mdp->num_descs_bytes > 0) && (! have_desc_id)) {
            bp = cur_mp + d_off + mdp->desc_len_off;
            u = sdp_get_big_endian(bp, 7, mdp->desc_len_bytes * 8);
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
    int res, v, resid;
    unsigned char cur_mp[DEF_MODE_RESP_LEN];

    memset(cur_mp, 0, sizeof(cur_mp));
    res = ll_mode_sense(sg_fd, op, ALL_MPAGES, 0, cur_mp, 8, &resid, verb);
    if (0 == res) {
        if (resid < 5) {
            v = cur_mp[op->mode_6 ? 2 : 3];
            printf("    Direct access device specific parameters: WP=%d  "
                   "DPOFUA=%d\n", !!(v & 0x80), !!(v & 0x10));
        } else if (verb > 0)
            pr2serr("%s: resid=%d too large, implies truncated response\n",
                    __func__, resid);
    } else if (SG_LIB_CAT_ILLEGAL_REQ != res)
       return report_error(res, op->mode_6);
    return 0;
}

static void
report_number_of_descriptors(unsigned char * cur_mp,
                             const struct sdparm_mode_descriptor_t * mdp,
                             const struct sdparm_opt_coll * op)
{
    int num = 0;
    uint64_t u;

    if ((mdp->num_descs_inc < 0) && (mdp->desc_len > 0)) {
        u = sdp_get_big_endian(cur_mp + mdp->num_descs_off, 7,
                               mdp->num_descs_bytes * 8) /
            mdp->desc_len;
        num = (int)u;
    } else if (mdp->num_descs_bytes > 0) {
        u = sdp_get_big_endian(cur_mp + mdp->num_descs_off, 7,
                               mdp->num_descs_bytes * 8) +
            mdp->num_descs_inc;
        num = (int)u;
    } else {    /* no descriptor count, so walk them */
        int pg_len = sdp_mpage_len(cur_mp);
        int d_len = mdp->desc_len_off + mdp->desc_len_bytes;
        int off = mdp->first_desc_off;
        const unsigned char * bp = cur_mp + mdp->desc_len_off;

        if (pg_len < 4)
            return;
        while (off + d_len < pg_len) {
            ++num;
            u = sdp_get_big_endian(bp + off, 7,
                                   mdp->desc_len_bytes * 8);
            if (u > 1024) {
                pr2serr(">> mpage descriptor too large=%"
                        PRIu64 ", stop counting "
                        "descriptors\n", u);
                break;
            }
            off += (d_len + (int)u);
        }
    }
    if (op->do_long)
        printf("number of descriptors=%d\n", num);
    else
        printf("%d\n", num);
}

/* Print one or more mode pages. Returns 0 if ok else IO error number.  */
static int
print_mode_pages(int sg_fd, int pn, int spn, int pdt,
                 const struct sdparm_opt_coll * op)
{
    bool single_pg, fetch_pg, desc_part, warned, have_desc_id;
    bool mode6 = op->mode_6;
    int res, pg_len, verb, smask, rep_len, orig_pn, desc0_off;
    int desc_id, desc_len;
    const struct sdparm_mode_page_item * mpi;
    const struct sdparm_mode_page_item * last_mpi;
    const struct sdparm_mode_page_item * fdesc_mpi = NULL;
    const struct sdparm_mode_page_t * mpp = NULL;
    const struct sdparm_mode_descriptor_t * mdp;
    void * pc_arr[4];
    unsigned char cur_mp[DEF_MODE_RESP_LEN];
    unsigned char cha_mp[DEF_MODE_RESP_LEN];
    unsigned char def_mp[DEF_MODE_RESP_LEN];
    unsigned char sav_mp[DEF_MODE_RESP_LEN];
    char b[128];

    b[0] = '\0';
    verb = (op->verbose > 0) ? op->verbose - 1 : 0;
    if ((0 == pdt) && (op->do_long > 0) && (0 == op->do_quiet)) {
        if (0 != (res = print_direct_access_info(sg_fd, op, verb)))
            return res;
    }
    orig_pn = pn;
    /* choose a mode page item namespace (vendor, transport or generic) */
    if (op->vendor_id >= 0) {
        const struct sdparm_vendor_pair * vpp;

        vpp = sdp_get_vendor_pair(op->vendor_id);
        mpi = (vpp ? vpp->mitem : NULL);
    } else if ((op->transport >= 0) && (op->transport < 16))
        mpi = sdparm_transport_mp[op->transport].mitem;
    else        /* generic */
        mpi = sdparm_mitem_arr;
    if (NULL == mpi)
        return SG_LIB_CAT_OTHER;

    last_mpi = mpi;
    if (pn >= 0) {      /* step to first item of given page */
        single_pg = true;
        fetch_pg = true;
        for ( ; mpi->acron; ++mpi) {
            if ((pn == mpi->pg_num) && (spn == mpi->subpg_num)) {
                if ((pdt < 0) || (mpi->pdt < 0) ||
                    (pdt == mpi->pdt) || op->flexible)
                    break;
            }
        }
        if (NULL == mpi->acron) {       /* page has no known fields */
            if (op->do_hex)
                mpi = last_mpi;    /* trick to enter main loop once */
            else {
                sdp_get_mp_with_str(pn, spn, pdt, op->transport, op->vendor_id,
                                    0, false, sizeof(b), b);
                if ((op->vendor_id < 0) && ((0 == pn) || (0x18 == pn) ||
                                            (0x19 == pn) || (pn >= 0x20)))
                    pr2serr("%s mode page may be transport or vendor "
                            "specific,\ntry '--transport=TN' or '--vendor=VN'"
                            ". Otherwise add '-H' to\nsee page in hex.\n", b);
                else
                    pr2serr("%s mode page, no fields found, add '-H' to see "
                            "page in hex.\n", b);
            }
        }
    } else {    /* want all, so check all items in given namespace */
        single_pg = false;
        fetch_pg = false;
        mpi = last_mpi;
    }
    mdp = NULL;
    pg_len = 0;
    have_desc_id = false;
    desc_id = -1;
    desc_len = -1;

    /* starting at first match, loop over each mode page item in given
     * namespace */
    for (smask = 0, warned = false ; mpi->acron; ++mpi, fetch_pg = false) {
        if (! fetch_pg) {
            if ((pn == mpi->pg_num) && (spn == mpi->subpg_num)) {
                if ((pdt >=0) && (mpi->pdt >= 0) &&
                    (pdt != mpi->pdt) && (! op->flexible))
                    continue;
                if (! (((orig_pn >= 0) ? 1 : op->do_all) ||
                       (MF_COMMON & mpi->flags)))
                    continue;
                /* here if matches everything at the mpage level */
            } else {    /* page changed? some clean up to do */
                if (fdesc_mpi) {
                    last_mpi = mpi - 1;
                    if ((pn == last_mpi->pg_num) &&
                        (spn == last_mpi->subpg_num))
                        print_mitem_desc_after1(pc_arr, pg_len, mpp,
                                                fdesc_mpi, last_mpi, op,
                                                smask, desc_len);
                }
                if (single_pg)
                    break;
                fetch_pg = true;
                pn = mpi->pg_num;
                spn = mpi->subpg_num;
            }
        }
        if (fetch_pg) {
            pg_len = 0;
            /* Only fetch mode page when needed (e.g. item page changed) */
            mpp = sdp_get_mp_with_str(pn, spn, pdt, op->transport,
                                      op->vendor_id, op->do_long,
                                      (op->do_hex > 0), sizeof(b), b);
            smask = 0;
            warned = false;
            fdesc_mpi = NULL;
            pc_arr[0] = cur_mp;
            pc_arr[1] = cha_mp;
            pc_arr[2] = def_mp;
            pc_arr[3] = sav_mp;
            res = sg_get_mode_page_controls(sg_fd, mode6, pn, spn, op->dbd,
                                            op->flexible, DEF_MODE_RESP_LEN,
                                            &smask, pc_arr, &rep_len, verb);
            if (res && (SG_LIB_CAT_ILLEGAL_REQ != res))
                return report_error(res, mode6);
            /* check for mode page descriptors */
            mdp = (mpp && (! op->do_hex)) ? mpp->mp_desc : NULL;
            have_desc_id = mdp ? mdp->have_desc_id : false;
            desc0_off = mdp ? mdp->first_desc_off : 0;
            if (desc0_off > 1) {
                for (desc_part = false, fdesc_mpi = mpi;
                     fdesc_mpi && (pn == fdesc_mpi->pg_num) &&
                     (spn == fdesc_mpi->subpg_num); ++fdesc_mpi) {
                    if (fdesc_mpi->start_byte >= desc0_off) {
                        desc_part = true;
                        break;
                    }
                }
                if (! desc_part)
                    fdesc_mpi = NULL;
            }
            if (op->num_desc) {  /* report number of descriptors */
                if (mdp && fdesc_mpi && (1 & smask))
                    report_number_of_descriptors(cur_mp, mdp, op);
                return 0;
            }
            if (smask & 1) {
                pg_len = sdp_mpage_len(cur_mp);
                printf("%s ", b);
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
            } else {    /* no (smask & 1), problem */
                if (op->verbose || single_pg) {
                    pr2serr(">> %s mode %spage ", b, (spn ? "sub" : ""));
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
                if ((! op->flexible) && (0 == op->verbose))
                    continue;   // step over
                if (! warned) {
                    warned = true;
                    if (op->flexible)
                        pr2serr(" >> hereafter field position exceeds mode "
                                "page length=%d\n", pg_len);
                    else {
                        pr2serr(" >> skipping rest as field position exceeds "
                                "mode page length=%d\n", pg_len);
                        continue;
                    }
                }
                if (! op->flexible)
                    continue;
            }
            if (have_desc_id && (mpi->start_byte == desc0_off) &&
                (! (mpi->flags & MF_CLASH_OK))) {
                desc_id = 0xf & *(cur_mp + mpi->start_byte);
                desc_len = sg_get_unaligned_be16(cur_mp + desc0_off + 2) + 4;
            }
            if (have_desc_id && (desc_id >= 0) &&
                (mpi->flags & MF_CLASH_OK)) {
                if (desc_id != sdp_get_desc_id(mpi->flags))
                    continue;
            }
            print_mitem_pc_arr("  ", smask, mpi, pc_arr, false, op);
        }
    }   /* end of mode page item loop */
    if (mpi && (NULL == mpi->acron) && fdesc_mpi) {
        --mpi;
        if ((pn == mpi->pg_num) && (spn == mpi->subpg_num))
            print_mitem_desc_after1(pc_arr, pg_len, mpp, fdesc_mpi, mpi,
                                    op, smask, desc_len);
    }
    return 0;
}

/* Print a mode page provided as argument. Returns 0 if ok else error
 * number.  */
static int
print_mode_page_inhex(uint8_t * msense_resp, int msense_resp_len,
                      const struct sdparm_opt_coll * op)
{
    bool first_time, desc_part, warned, spf, have_desc_id;
    bool mode6 = op->mode_6;
    int smask, resp_len, orig_pn, v, off, desc0_off, pn, spn;
    int pg_len, transport, vendor_id, desc_id, desc_len;
    const struct sdparm_mode_page_item * mpi;
    const struct sdparm_mode_page_item * last_mpi;
    const struct sdparm_mode_page_item * fdesc_mpi = NULL;
    const struct sdparm_mode_page_t * mpp = NULL;
    const struct sdparm_mode_descriptor_t * mdp;
    uint8_t * pg_p;
    void * pc_arr[4];
    char b[128];

    resp_len = sg_msense_calc_length(msense_resp, msense_resp_len, mode6,
                                     NULL);
    off = sg_mode_page_offset(msense_resp, msense_resp_len, mode6, NULL, 0);
    if ((resp_len < 2) || (off < 2)) {
        pr2serr("Couldn't decode %s as a %s(%d) command reponse\n",
                (op->inhex_fn ? op->inhex_fn : "??"), ms_s, (mode6 ? 6 : 10));
        pr2serr("perhaps it is a VPD page, if so add '-i'\n");
        return SG_LIB_CAT_OTHER;
    }
    if (msense_resp_len < resp_len) {
        pr2serr("given %s response is too short, given %d bytes, expected "
                "%d\n", ms_s, msense_resp_len, resp_len);
        return SG_LIB_CAT_OTHER;
    }
    if ((0 == op->pdt) && (op->do_long > 0) && (0 == op->do_quiet)) {
        v = msense_resp[mode6 ? 2 : 3];
        printf("    Direct access device specific parameters: WP=%d  "
               "DPOFUA=%d\n", !!(v & 0x80), !!(v & 0x10));
    }

and_again:
    pg_p = msense_resp + off;
    pn = pg_p[0] & 0x3f;
    pg_len = sdp_mpage_len(pg_p);
    spf = !! (pg_p[0] & 0x40);
    spn = spf ? pg_p[1] : 0;
    pc_arr[0] = pg_p;
    pc_arr[1] = NULL;
    pc_arr[2] = NULL;
    pc_arr[3] = NULL;
    smask = 0x1;        /* treat as single "current" page */
    have_desc_id = false;
    desc_id = -1;
    desc_len = -1;

    b[0] = '\0';
    orig_pn = pn;
    /* choose a mode page item namespace (vendor, transport or generic) */
    transport = op->transport;
    vendor_id = op->vendor_id;
    if (vendor_id >= 0) {
        const struct sdparm_vendor_pair * vpp;

        vpp = sdp_get_vendor_pair(vendor_id);
        mpi = (vpp ? vpp->mitem : NULL);
    } else if ((transport >= 0) && (transport < 16))
        mpi = sdparm_transport_mp[transport].mitem;
    else
        mpi = sdparm_mitem_arr;
    if (NULL == mpi)
        return SG_LIB_CAT_OTHER;

now_try_generic:
    last_mpi = mpi;
    for ( ; mpi->acron; ++mpi) {
        if ((pn == mpi->pg_num) && (spn == mpi->subpg_num)) {
            if ((op->pdt < 0) || (mpi->pdt < 0) ||
                (op->pdt == mpi->pdt) || op->flexible)
                break;
        }
    }
    if (NULL == mpi->acron) {       /* page has no known fields */
        if (op->do_all && ((transport >= 0) || (vendor_id >= 0))) {
            if (transport >= 0)
                transport = -1;
            if (vendor_id >= 0)
                vendor_id = -1;
            mpi = sdparm_mitem_arr;
            goto now_try_generic;
        }
        sdp_get_mp_with_str(pn, spn, op->pdt, transport, vendor_id, 0, false,
                            sizeof(b), b);
        if ((vendor_id < 0) && ((0 == pn) || (0x18 == pn) || (0x19 == pn) ||
                                (pn >= 0x20)))
            pr2serr("%s mode page may be transport or vendor specific,\n"
                    "try '--transport=TN' or '--vendor=VN'.\n", b);
        else
            pr2serr("%s mode page, no fields found. Add '-v' or '--v' "
                    "to\nsee more debug.\n", b);
    }
    mdp = NULL;

    /* starting at first match, loop over each mode page item in given
     * namespace */
    for (warned = false, first_time = true ; mpi->acron; ++mpi) {
        if (first_time) {
            first_time = false;
            mpp = sdp_get_mp_with_str(pn, spn, op->pdt, transport, vendor_id,
                                      op->do_long, (op->do_hex > 0),
                                      sizeof(b), b);
            warned = false;
            fdesc_mpi = NULL;
            /* check for mode page descriptors */
            mdp = (mpp && (! op->do_hex)) ? mpp->mp_desc : NULL;
            have_desc_id = mdp ? mdp->have_desc_id : false;
            desc0_off = mdp ? mdp->first_desc_off : 0;
            if (desc0_off > 1) {
                for (desc_part = false, fdesc_mpi = mpi;
                     fdesc_mpi && (pn == fdesc_mpi->pg_num) &&
                     (spn == fdesc_mpi->subpg_num); ++fdesc_mpi) {
                    if (fdesc_mpi->start_byte >= desc0_off) {
                        desc_part = true;
                        break;
                    }
                }
                if (! desc_part)
                    fdesc_mpi = NULL;
            }
            if (op->num_desc) {  /* report number of descriptors */
                if (mdp && fdesc_mpi && (1 & smask))
                    report_number_of_descriptors(pg_p, mdp, op);
                return 0;
            }
            printf("%s ", b);
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
            if ((pn == mpi->pg_num) && (spn == mpi->subpg_num)) {
                if ((op->pdt >=0) && (mpi->pdt >= 0) &&
                    (op->pdt != mpi->pdt) && (! op->flexible))
                    continue;
                if (! (((orig_pn >= 0) ? true : op->do_all) ||
                       (MF_COMMON & mpi->flags)))
                    continue;
            } else {    /* mode page or subpage changed */
                if (fdesc_mpi) {
                    last_mpi = mpi - 1; /* last_mpi that matched ... */
                    if ((pn == last_mpi->pg_num) &&
                        (spn == last_mpi->subpg_num))
                        print_mitem_desc_after1(pc_arr, pg_len, mpp,
                                                fdesc_mpi, last_mpi, op,
                                                smask, desc_len);
                }
                break;
            }
        }
        if (smask && (! op->do_hex)) {
            if (mpi->start_byte >= pg_len) {
                if ((! op->flexible) && (0 == op->verbose))
                    continue;   // step over
                if (! warned) {
                    warned = true;
                    if (op->flexible)
                        pr2serr(" >> hereafter field position exceeds mode "
                                "page length=%d\n", pg_len);
                    else {
                        pr2serr(" >> skipping rest as field position exceeds "
                                "mode page length=%d\n", pg_len);
                        continue;
                    }
                }
                if (! op->flexible)
                    continue;
            }
            if (have_desc_id && (mpi->start_byte == desc0_off) &&
                (! (mpi->flags & MF_CLASH_OK))) {
                desc_id = 0xf & *(pg_p + mpi->start_byte);
                desc_len = sg_get_unaligned_be16(pg_p + desc0_off + 2) + 4;
            }
            if (have_desc_id && (desc_id >= 0) &&
                (mpi->flags & MF_CLASH_OK)) {
                if (desc_id != sdp_get_desc_id(mpi->flags))
                    continue;
            }
            print_mitem_pc_arr("  ", smask, mpi, pc_arr, false, op);
        }
    }   /* end of mode page item loop */
    if (mpi && (NULL == mpi->acron) && fdesc_mpi) {
        --mpi;
        if ((pn == mpi->pg_num) && (spn == mpi->subpg_num))
            print_mitem_desc_after1(pc_arr, pg_len, mpp, fdesc_mpi, mpi,
                                    op, smask, desc_len);
    }
    if (op->do_all) {
        off += pg_len;
        if ((off + 4) < resp_len)
            goto and_again;
    }
    return 0;
}

/* returns true when ok, else false */
static bool
check_desc_convert_mpi(int desc_num, const struct sdparm_mode_page_t * mpp,
                       const struct sdparm_mode_page_item * ref_mpi,
                       struct sdparm_mode_page_item * out_mpi,
                       int b_len, char * b)
{
    int n;

    if (mpp && mpp->mp_desc && ref_mpi->acron && b) {
        *out_mpi = *ref_mpi;
        strncpy(b, ref_mpi->acron, b_len);
        b[(b_len > 10) ? (b_len - 8) : 4] = '\0';
        n = strlen(b);
        snprintf(b + n, b_len - n, ".%d", desc_num);
        out_mpi->acron = b;
        return true;
    } else
        return false;
}

/* returns true when ok, else false */
static bool
desc_adjust_start_byte(int desc_num, const struct sdparm_mode_page_t * mpp,
                       const unsigned char * cur_mp, int rep_len,
                       struct sdparm_mode_page_item * mpi,
                       const struct sdparm_opt_coll * op)
{
    const struct sdparm_mode_descriptor_t * mdp;
    uint64_t u;
    const unsigned char * bp;
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
                    return true;
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
                            return true;
                        else
                            pr2serr(">> new start_byte exceeds current page "
                                    "...\n");
                        break;
                    }
                    bp = cur_mp + d_off + mdp->desc_len_off;
                    u = sdp_get_big_endian(bp, 7, mdp->desc_len_bytes * 8);
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
    return false;
}

/* Print one or more mode page items (from '--get='). Returns 0 if ok. */
static int
print_mitems(int sg_fd, const struct sdparm_mode_page_settings * mps,
             int pdt, const struct sdparm_opt_coll * op)
{
    bool adapt, warned;
    bool mode6 = op->mode_6;
    int k, res, verb, smask, pn, spn, rep_len, len, desc_num;
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
    char b[128];
    char b_tmp[32];
    void * pc_arr[4];

    warned = false;
    verb = (op->verbose > 0) ? op->verbose - 1 : 0;
    for (k = 0, pn = 0, spn = 0; k < mps->num_it_vals; ++k) {
        ivp = &mps->it_vals[k];
        val = ivp->val;
        desc_num = ivp->descriptor_num;
        mpi = &ivp->mpi;
        mpp = sdp_get_mp_with_str(mpi->pg_num, mpi->subpg_num, mpi->pdt,
                                  op->transport, op->vendor_id, op->do_long,
                                  (op->do_hex > 0), sizeof(b), b);
        if (desc_num > 0) {
            if (check_desc_convert_mpi(desc_num, mpp, mpi, &ampi,
                                       sizeof(b_tmp), b_tmp)) {
                adapt = true;
                mpi = &ampi;
            } else {
               pr2serr("can't decode descriptors for %s in %s mode page\n",
                       (mpi->acron ? mpi->acron : ""), b);
               return SG_LIB_SYNTAX_ERROR;
            }
        } else
            adapt = false;
        if ((0 == k) || (pn != mpi->pg_num) || (spn != mpi->subpg_num)) {
            pn = mpi->pg_num;
            spn = mpi->subpg_num;
            smask = 0;
            res = 0;
            switch (val) {      /* for --get=<acron>=0|1|2 */
            case 0:
                pc_arr[0] = cur_mp;
                pc_arr[1] = cha_mp;
                pc_arr[2] = def_mp;
                pc_arr[3] = sav_mp;
                break;
            case 1:
            case 2:     /* signed integer */
                pc_arr[0] = cur_mp;
                pc_arr[1] = NULL;
                pc_arr[2] = NULL;
                pc_arr[3] = NULL;
                break;
            default:
                if (mpi->acron)
                    pr2serr("bad value given to %s\n", mpi->acron);
                else
                    pr2serr("bad value given to 0x%x:%d:%d\n",
                            mpi->start_byte, mpi->start_bit, mpi->num_bits);
                return SG_LIB_SYNTAX_ERROR;
            }
            res = sg_get_mode_page_controls(sg_fd, mode6, pn, spn, op->dbd,
                                            op->flexible, DEF_MODE_RESP_LEN,
                                            &smask, pc_arr, &rep_len, verb);
            if (res && (SG_LIB_CAT_ILLEGAL_REQ != res))
                return report_error(res, mode6);
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
                pr2serr("%s mode page\n", b);
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
        if ((pdt >= 0) && (! warned) && mpi->acron &&
            (mpi->pdt >= 0) && (pdt != mpi->pdt)) {
            warned = true;
            pr2serr(">> warning: peripheral device type (pdt) is 0x%x but "
                    "acronym %s\n   is associated with pdt 0x%x.\n", pdt,
                    ivp->mpi.acron, ivp->mpi.pdt);
        }
        len = (smask & 1) ? sdp_mpage_len(cur_mp) : 0;
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
        if (0 == val) { /* current, changed, default and saved; in hex */
            if (op->do_hex) {
                if (smask & 1) {
                    u = sdp_mitem_get_value(mpi, cur_mp);
                    printf("0x%02" PRIx64 " ", u);
                } else
                    printf("-    ");
                if (smask & 2) {
                    u = sdp_mitem_get_value(mpi, cha_mp);
                    printf("0x%02" PRIx64 " ", u);
                } else
                    printf("-    ");
                if (smask & 4) {
                    u = sdp_mitem_get_value(mpi, def_mp);
                    printf("0x%02" PRIx64 " ", u);
                } else
                    printf("-    ");
                if (smask & 8) {
                    u = sdp_mitem_get_value(mpi, sav_mp);
                    printf("0x%02" PRIx64 " ", u);
                } else
                    printf("-    ");
                printf("\n");
            } else
                print_mitem_pc_arr("", smask, mpi, pc_arr, false, op);
        } else if (1 == val) {  /* just current in hex */
            if (op->do_hex) {
                if (smask & 1) {
                    u = sdp_mitem_get_value(mpi, cur_mp);
                    printf("0x%02" PRIx64 " ", u);
                } else
                    printf("-    ");
                printf("\n");
            } else
                print_mitem_pc_arr("", smask & 1, mpi, pc_arr, false, op);
        } else if (2 == val) {  /* just current in signed decimal */
            if (op->do_hex) {
                if (smask & 1) {
                    u = sdp_mitem_get_value(mpi, cur_mp);
                    sdp_print_signed_decimal(u, mpi->num_bits, true);
                } else
                    printf("-    ");
                printf("\n");
            } else
                print_mitem_pc_arr("", smask & 1, mpi, pc_arr, true, op);
        }
    }
    return 0;
}

/* Return of 0 -> success, most errors indicated by various SG_LIB_CAT_*
 * positive values, -1 -> other failures */
static int
change_mode_page(int sg_fd, int pdt,
                 const struct sdparm_mode_page_settings * mps,
                 const struct sdparm_opt_coll * op)
{
    bool mode6 = op->mode_6;
    int k, off, md_len, len, res, desc_num, pn, spn;
    int resid = 0;
    const struct sdparm_mode_page_t * mpp = NULL;
    const struct sdparm_mode_page_it_val * ivp;
    const struct sdparm_mode_page_item * mpi;
    const char * cdbLenS = mode6 ? "6": "10";
    char b[128];
    char b_tmp[32];
    char ebuff[EBUFF_SZ];
    unsigned char mdpg[MAX_MODE_DATA_LEN];
    struct sdparm_mode_page_item ampi;

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
    pn = mps->pg_num;
    spn = mps->subpg_num;
    mpp = sdp_get_mp_with_str(pn, spn, pdt, op->transport, op->vendor_id,
                              0, false, sizeof(b), b);
    memset(mdpg, 0, sizeof(mdpg));
    res = ll_mode_sense(sg_fd, op, pn, spn, mdpg, 4, &resid, op->verbose);
    if (0 != res) {
        if (SG_LIB_CAT_INVALID_OP == res) {
            pr2serr("%s byte %s cdb not supported, try again with%s '-6' "
                    "option\n", cdbLenS, ms_s, mode6 ? "out" : "");
        } else {
            char b[80];

            sg_get_category_sense_str(res, sizeof(b), b, op->verbose);
            pr2serr("%s command: %s\n", ms_s, b);
        }
        pr2serr("%s: failed fetching page: %s\n", __func__, b);
        return res;
    }
    if (resid > 0) {
        pr2serr("%s: resid=%d implies even short mpage truncated\n",
                __func__, resid);
        return SG_LIB_CAT_MALFORMED;
    }
    md_len = mode6 ? (mdpg[0] + 1) : (sg_get_unaligned_be16(mdpg) + 2);
    if (md_len > (int)sizeof(mdpg)) {
        pr2serr("%s: mode data length=%d exceeds allocation length=%d\n",
                __func__, md_len, (int)sizeof(mdpg));
        return SG_LIB_CAT_MALFORMED;
    }
    res = ll_mode_sense(sg_fd, op, pn, spn, mdpg, md_len, &resid, op->verbose);
    if (0 != res) {
        pr2serr("%s: failed fetching page: %s\n", __func__, b);
        return res;
    }
    if (resid > 0) {
        md_len -= resid;
        if (md_len < 4) {
            pr2serr("%s: resid=%d implies mpage truncated\n", __func__,
                    resid);
            return SG_LIB_CAT_MALFORMED;
        }
    }
    off = sg_mode_page_offset(mdpg, md_len, mode6, ebuff, EBUFF_SZ);
    if (off < 0) {
        pr2serr("%s: page offset failed: %s\n", __func__, ebuff);
        return SG_LIB_CAT_MALFORMED;
    }
    len = sdp_mpage_len(mdpg + off);
    mdpg[0] = 0;        /* mode data length reserved for mode select */
    if (! mode6)
        mdpg[1] = 0;    /* mode data length reserved for mode select */
    if (PDT_DISK == pdt)       /* entire disk specific parameters is ... */
        mdpg[mode6 ? 2 : 3] = 0x00;     /* reserved for mode select */

    for (k = 0; k < mps->num_it_vals; ++k) {
        ivp = &mps->it_vals[k];
        mpi = &ivp->mpi;
        desc_num = ivp->descriptor_num;
        if (desc_num > 0) {
            if (check_desc_convert_mpi(desc_num, mpp, mpi, &ampi,
                                       sizeof(b_tmp), b_tmp)) {
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
        sdp_mitem_set_value(ivp->val, mpi, mdpg + off);
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
    if (mode6)
        res = sg_ll_mode_select6(sg_fd, true /* PF */, op->save, mdpg, md_len,
                                 true, op->verbose);
    else
        res = sg_ll_mode_select10(sg_fd, true, op->save, mdpg, md_len, true,
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
set_def_mode_page(int sg_fd, int pn, int spn,
                  const unsigned char * mode_pg, int mode_pg_len,
                  const struct sdparm_opt_coll * op)
{
    bool mode6 = op->mode_6;
    int len, off, md_len;
    int resid = 0;
    int ret = -1;
    unsigned char * mdp;
    char ebuff[EBUFF_SZ];
    char buff[128];

    len = mode_pg_len + MODE_DATA_OVERHEAD;
    mdp = (unsigned char *)malloc(len);
    if (NULL ==mdp) {
        pr2serr("%s: malloc failed, out of memory\n", __func__);
        return SG_LIB_FILE_ERROR;
    }
    memset(mdp, 0, len);
    ret = ll_mode_sense(sg_fd, op, pn, spn, mdp, 4, &resid, op->verbose);
    if (0 != ret) {
        sdp_get_mp_with_str(pn, spn, -1, op->transport, op->vendor_id, 0,
                            false, sizeof(buff), buff);
        pr2serr("%s: failed fetching page: %s\n", __func__, buff);
        goto err_out;
    }
    if (resid > 0) {
        pr2serr("%s: resid=%d implies even short mpage truncated\n",
                __func__, resid);
        ret = SG_LIB_CAT_MALFORMED;
        goto err_out;
    }
    md_len = mode6 ? (mdp[0] + 1) : (sg_get_unaligned_be16(mdp) + 2);
    if (md_len > len) {
        pr2serr("%s: mode data length=%d exceeds allocation length=%d\n",
                __func__, md_len, len);
        ret = SG_LIB_CAT_MALFORMED;
        goto err_out;
    }
    ret = ll_mode_sense(sg_fd, op, pn, spn, mdp, md_len, &resid, op->verbose);
    if (0 != ret) {
        sdp_get_mp_with_str(pn, spn, -1, op->transport, op->vendor_id,
                            0, false, sizeof(buff), buff);
        pr2serr("%s: failed fetching page: %s\n", __func__, buff);
        goto err_out;
    }
    if (resid > 0) {
        md_len -= resid;
        if (md_len < 4) {
            pr2serr("%s: resid=%d implies mpage truncated\n", __func__,
                    resid);
            ret = SG_LIB_CAT_MALFORMED;
            goto err_out;
        }
    }
    off = sg_mode_page_offset(mdp, len, mode6, ebuff, EBUFF_SZ);
    if (off < 0) {
        pr2serr("%s: page offset failed: %s\n", __func__, ebuff);
        ret = SG_LIB_CAT_MALFORMED;
        goto err_out;
    }
    mdp[0] = 0;        /* mode data length reserved for mode select */
    if (! mode6)
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
    if (mode6)
        ret = sg_ll_mode_select6(sg_fd, true /* PF */, op->save, mdp, md_len,
                                 true, op->verbose);
    else
        ret = sg_ll_mode_select10(sg_fd, true, op->save, mdp, md_len, true,
                                  op->verbose);
    if (0 != ret) {
        sdp_get_mp_with_str(pn, spn, -1, op->transport, op->vendor_id,
                            0, false, sizeof(buff), buff);
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
    bool mode6 = op->mode_6;
    int smask, res, len, rep_len;
    const char * cdbLenS = mode6 ? "6" : "10";
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
        pr2serr("%s byte %s cdb not supported, try again with%s '-6' "
                "option\n", cdbLenS, ms_s, mode6 ? "out" : "");
        return res;
    } else if (SG_LIB_CAT_NOT_READY == res) {
        pr2serr("%s(%s) failed, device not ready\n", ms_s, cdbLenS);
        return res;
    } else if (SG_LIB_CAT_ABORTED_COMMAND == res) {
        pr2serr("%s(%s) failed, aborted command\n", ms_s, cdbLenS);
        return res;
    }
    if (op->verbose && (! op->flexible) && (rep_len > 0xa00)) {
        sdp_get_mp_with_str(pn, spn, pdt, op->transport, op->vendor_id,
                            0, false, sizeof(buff), buff);
        pr2serr("%s mode page length=%d too long, perhaps try '--flexible'\n",
                buff, rep_len);
    }
    if ((smask & 1)) {
        len = sdp_mpage_len(cur_mp);
        if ((smask & 4)) {
            return set_def_mode_page(sg_fd, pn, spn, def_mp, len, op);
        }
        else {
            sdp_get_mp_with_str(pn, spn, pdt, op->transport, op->vendor_id,
                                0, false, sizeof(buff), buff);
            pr2serr(">> %s mode page (default) not supported\n", buff);
            return SG_LIB_CAT_ILLEGAL_REQ;
        }
    } else {
        sdp_get_mp_with_str(pn, spn, pdt, op->transport, op->vendor_id,
                            0, false, sizeof(buff), buff);
        pr2serr(">> %s mode page not supported\n", buff);
        return SG_LIB_CAT_ILLEGAL_REQ;
    }
}

/* Parse 'arg' given to command line --get=, --clear= or set= into 'mps'
 * which contains an array ('mps->it_vals[]') used for output. 'arg' may be
 * a comma separated list of item value=pairs. This function places
 * 'mps->num_it_vals' elements in that array. If 'clear','get' are
 * false,false then assume "--set="; true,true is invalid. Returns true if
 * successful, else false. */
static bool
build_mp_settings(const char * arg, struct sdparm_mode_page_settings * mps,
                  struct sdparm_opt_coll * op, bool clear, bool get)
{
    bool cont;
    int len, b_sz, num, from, colon;
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
                    ivp->val = sg_get_llnum_nomult(vb);
                    if (-1 == ivp->val) {
                        pr2serr("unable to decode: %s value\n", buff);
                        pr2serr("    expected: <acronym>[=<val>]\n");
                        return false;
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
                ivp->descriptor_num = sg_get_llnum_nomult(vb);
                if (ivp->descriptor_num < 0) {
                    pr2serr("unable to decode: %s descriptor number\n", buff);
                    pr2serr("    expected: <acronym_name>"
                            "[.<desc_num>][=<val>]\n");
                    return false;
                }
            }
            ivp->orig_val = ivp->val;
            from = 0;
            cont = false;
            prev_mpi = NULL;
            if (get) {
                do {
                    mpi = sdp_find_mitem_by_acron(acron, &from, op->transport,
                                         op->vendor_id);
                    if (NULL == mpi) {
                        if (cont) {
                            mpi = prev_mpi;
                            break;
                        }
                        if ((op->vendor_id < 0) && (op->transport < 0)) {
                            from = 0;
                            mpi = sdp_find_mitem_by_acron(acron, &from,
                                          DEF_TRANSPORT_PROTOCOL, -1);
                            if (NULL == mpi) {
                                pr2serr("couldn't find field acronym: %s\n",
                                        acron);
                                pr2serr("    [perhaps a '--transport=<tn>' "
                                        "or '--vendor=<vn>' option is "
                                        "needed]\n");
                                return false;
                            } else /* keep going in this case */
                                op->transport = DEF_TRANSPORT_PROTOCOL;
                        } else {
                            pr2serr("couldn't find field acronym: %s\n",
                                    acron);
                            return false;
                        }
                    }
                    if (mps->pg_num < 0) {
                        mps->pg_num = mpi->pg_num;
                        mps->subpg_num = mpi->subpg_num;
                        break;
                    }
                    cont = true;
                    prev_mpi = mpi;
                    /* got acronym match but if not at specified pn,spn */
                    /* then keep searching */
                } while ((mps->pg_num != mpi->pg_num) ||
                         (mps->subpg_num != mpi->subpg_num));
            } else {    /* --set or --clear */
                do {
                    mpi = sdp_find_mitem_by_acron(acron, &from,
                                 op->transport, op->vendor_id);
                    if (NULL == mpi) {
                        if (cont) {
                            pr2serr("mode page of acronym: %s [0x%x,0x%x] "
                                    "doesn't match prior\n", acron,
                                    prev_mpi->pg_num,
                                    prev_mpi->subpg_num);
                            pr2serr("    mode page: 0x%x,0x%x\n",
                                    mps->pg_num, mps->subpg_num);
                            pr2serr("For '--set' and '--clear' all fields "
                                    "must be in the same mode page\n");
                            return false;
                        }
                        if ((op->vendor_id < 0) && (op->transport < 0)) {
                            from = 0;
                            mpi = sdp_find_mitem_by_acron(acron, &from,
                                          DEF_TRANSPORT_PROTOCOL, -1);
                            if (NULL == mpi) {
                                pr2serr("couldn't find field acronym: %s\n",
                                        acron);
                                pr2serr("    [perhaps a '--transport=<tn>' "
                                        "or '--vendor=<vn>' option is "
                                        "needed]\n");
                                return false;
                            } else /* keep going in this case */
                                op->transport = DEF_TRANSPORT_PROTOCOL;
                        } else {
                            pr2serr("couldn't find field acronym: %s\n",
                                    acron);
                            return false;
                        }
                    }
                    if (mps->pg_num < 0) {
                        mps->pg_num = mpi->pg_num;
                        mps->subpg_num = mpi->subpg_num;
                        break;
                    }
                    cont = true;
                    prev_mpi = mpi;
                    /* got acronym match but if not at specified pn,spn */
                    /* then keep searching */
                } while ((mps->pg_num != mpi->pg_num) ||
                         (mps->subpg_num != mpi->subpg_num));
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
                return false;
            }
            if (3 == num)
                ivp->val = ((clear || get) ? 0 : -1);
            else {
                if (0 == strcmp("-1", vb))
                    ivp->val = -1;
                else {
                    ivp->val = sg_get_llnum_nomult(vb);
                    if (-1 == ivp->val) {
                        pr2serr("unable to decode start_byte:start_bit:"
                                "num_bits value\n");
                        return false;
                    }
                }
            }
            ivp->mpi.pdt = -1;  /* don't known pdt now, so don't restrict */
            if (ivp->mpi.start_byte < 0) {
                pr2serr("need positive start byte offset\n");
                return false;
            }
            if ((ivp->mpi.start_bit < 0) || (ivp->mpi.start_bit > 7)) {
                pr2serr("need start bit in 0..7 range (inclusive)\n");
                return false;
            }
            if ((ivp->mpi.num_bits < 1) || (ivp->mpi.num_bits > 64)) {
                pr2serr("need number of bits in 1..64 range (inclusive)\n");
                return false;
            }
            if (mps->pg_num < 0) {
                pr2serr("need '--page=' option for mode page name or "
                        "number\n");
                return false;
            } else if (get) {
                ivp->mpi.pg_num = mps->pg_num;
                ivp->mpi.subpg_num = mps->subpg_num;
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
    if (mps->num_it_vals >= MAX_MP_IT_VAL)
        pr2serr("%s: maximum number (%d) of command line itm/value pairs "
                "reached,\nignore rest\n", __func__, MAX_MP_IT_VAL);
    return true;
}

static int
open_and_simple_inquiry(const char * device_name, bool rw, int * pdt,
                        bool * protect, const struct sdparm_opt_coll * op)
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
        *protect = !! (sir.byte_5 & 0x1);  /* PROTECT bit SPC-3 and later */
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
             int spn, bool set_clear, bool get,
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
        mpp = sdp_get_mpage(pn, spn, pdt, op->transport, op->vendor_id);
        if (NULL == mpp)
            mpp = sdp_get_mpage(pn, spn, -1, op->transport, op->vendor_id);
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
        res = print_mitems(sg_fd, mps, pdt, op);
    } else
        res = print_mode_pages(sg_fd, pn, spn, pdt, op);
    return res;
}


int
main(int argc, char * argv[])
{
#ifdef SG_LIB_WIN32
    bool do_wscan = false;
#endif
    bool protect = false;
    bool rw = false;      /* true: requires RDWR, false: perhaps RDONLY ok */
    bool set_clear = false;
    int sg_fd, res, c, pdt, req_pdt, k, orig_transport, r;
    int cmd_arg = -1;
    int do_help = 0;
    int num_devices = 0;
    int pn = -1;
    int spn = -1;
    int ret = 0;
    struct sdparm_opt_coll * op;
    const char * clear_str = NULL;
    const char * cmd_str = NULL;
    const char * get_str = NULL;
    const char * set_str = NULL;
    const char * page_str = NULL;
    const char * device_name_arr[MAX_DEV_NAMES];
    const struct sdparm_mode_page_t * mpp = NULL;
    int t_proto;
    const struct sdparm_vpd_page_t * vpp = NULL;
    const struct sdparm_vendor_name_t * vnp;
    char * cp;
    const char * ccp;
    const struct sdparm_command_t * scmdp = NULL;
    struct sdparm_opt_coll opts;
    struct sdparm_mode_page_settings mp_settings;
    char b[256];

    op = &opts;
    memset(op, 0, sizeof(opts));
    op->transport = -1;
    op->vendor_id = -1;
    op->pdt = -1;
    memset(device_name_arr, 0, sizeof(device_name_arr));
    memset(&mp_settings, 0, sizeof(mp_settings));
    pdt = -1;
    while (1) {
        int option_index = 0;

#ifdef SG_LIB_WIN32
        c = getopt_long(argc, argv, "6aBc:C:dDefg:hHiI:lM:np:P:qrRs:St:vVw",
                        long_options, &option_index);
#else
        c = getopt_long(argc, argv, "6aBc:C:dDefg:hHiI:lM:np:P:qrRs:St:vV",
                        long_options, &option_index);
#endif
        if (c == -1)
            break;

        switch (c) {
        case '6':
            op->mode_6 = true;
            break;
        case 'a':
            op->do_all = true;
            break;
        case 'B':
            op->dbd = true;
            break;
        case 'c':
            clear_str = optarg;
            set_clear = true;
            rw = true;
            break;
        case 'C':
            cmd_str = optarg;
            break;
        case 'd':
            op->dummy = true;
            break;
        case 'D':
            op->defaults = true;
            rw = true;
            break;
        case 'e':
            ++op->do_enum;
            break;
        case 'f':
            op->flexible = true;
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
            op->inquiry = true;
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
                    op->vendor_id = vnp->vendor_id;
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
                op->vendor_id = res;
            }
            break;
        case 'n':
            op->num_desc = true;
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
            op->read_only = true;
            break;
        case 'R':
            op->do_raw = true;
            break;
        case 's':
            set_str = optarg;
            rw = true;
            set_clear = true;
            break;
        case 'S':
            op->save = true;
            break;
        case 't':
            if (isalpha(optarg[0])) {
                t_proto = sdp_find_transport_id_by_acron(optarg);
                if (t_proto < 0) {
                    pr2serr("abbreviation does not match a transport "
                            "protocol\n");
                    printf("Available transport protocols:\n");
                    enumerate_transports(true, op);
                    return SG_LIB_SYNTAX_ERROR;
                } else
                    op->transport = t_proto;
            } else {
                res = sg_get_num_nomult(optarg);
                if ((res < 0) || (res > 15)) {
                    pr2serr("Bad transport value after '-t' option\n");
                    printf("Available transport protocols:\n");
                    enumerate_transports(false, op);
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
            do_wscan = true;
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
        rw = false;         // override any read-write settings

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
            op->inquiry = true;
            pn = VPD_NOT_STD_INQ;
        } else if (isalpha(page_str[0])) {
            while ( 1 ) {      /* dummy loop, just using break */
                mpp = sdp_find_mp_by_acron(page_str, op->transport,
                                           op->vendor_id);
                if (mpp)
                    break;
                vpp = sdp_find_vpd_by_acron(page_str);
                if (vpp)
                    break;
                orig_transport = op->transport;
                if ((op->vendor_id < 0) && (op->transport < 0)) {
                    op->transport = DEF_TRANSPORT_PROTOCOL;
                    mpp = sdp_find_mp_by_acron(page_str, op->transport,
                                               op->vendor_id);
                    if (mpp)
                        break;
                }
                if ((op->vendor_id < 0) && (orig_transport < 0))
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
                    if (op->vendor_id >= 0)
                        printf(" (for given vendor):\n");
                    else if (orig_transport >= 0)
                        printf(" (for given transport):\n");
                    else
                        printf(":\n");
                    enumerate_mpages(orig_transport, op->vendor_id);
                    return SG_LIB_SYNTAX_ERROR;
                }
            }
            if (vpp) {
                pn = vpp->vpd_num;
                spn = vpp->subvalue;
                op->inquiry = true;
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
                    if (op->vendor_id >= 0)
                        printf(" (for given vendor):\n");
                    else if (op->transport >= 0)
                        printf(" (for given transport):\n");
                    else
                        printf(":\n");
                    enumerate_mpages(op->transport, op->vendor_id);
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

    if (op->inquiry) {  /* VPD pages or standard INQUIRY response */
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
            rw = false;         // override any read-write settings
    } else {            /* assume mode page access */
        if (pn < 0) {
            mp_settings.pg_num = -1;
            mp_settings.subpg_num = -1;
        } else {
            mp_settings.pg_num = pn;
            mp_settings.subpg_num = spn;
        }
        if (get_str) {
            if (set_clear) {
                pr2serr("'--get=' can't be used with '--set=' or "
                        "'--clear='\n");
                return SG_LIB_SYNTAX_ERROR;
            }
            if (! build_mp_settings(get_str, &mp_settings, op, false, true))
                return SG_LIB_SYNTAX_ERROR;
        }
        if (op->do_enum) {
            if ((num_devices > 0) || set_clear || get_str || op->save)
                /* think about --get= with --enumerate */
                printf("<scsi_device> as well as most options are ignored "
                       "when '--enumerate' is given\n");
            if (pn < 0) {
                if (op->vendor_id >= 0) {
                    ccp = sdp_get_vendor_name(op->vendor_id);
                    if (ccp)
                        printf("Mode pages for %s vendor:\n", ccp);
                    else
                        printf("Mode pages for vendor 0x%x:\n", op->vendor_id);
                    if (op->do_all)
                        enumerate_mitems(pn, spn, pdt, op);
                    else {
                        enumerate_mpages(op->transport, op->vendor_id);
                    }
                } else if (op->transport >= 0) {
                    ccp = sdp_get_transport_name(op->transport, sizeof(b), b);
                    if (strlen(ccp) > 0)
                        printf("Mode pages for %s transport protocol:\n",
                               ccp);
                    else
                        printf("Mode pages for transport protocol 0x%x:\n",
                               op->transport);
                    if (op->do_all)
                        enumerate_mitems(pn, spn, pdt, op);
                    else {
                        enumerate_mpages(op->transport, op->vendor_id);
                    }
                } else {        /* neither vendor nor transport given */
                    if (op->do_long || (op->do_enum > 1)) {
                        printf("Mode pages (not related to any transport "
                               "protocol or vendor):\n");
                        enumerate_mpages(-1, -1);
                        printf("\n");
                        printf("Transport protocols:\n");
                        enumerate_transports(false, op);
                        printf("\n");
                        printf("Vendors:\n");
                        enumerate_vendors();
                        if (op->do_all) {
                            struct sdparm_opt_coll t_opts;

                            printf("\n");
                            t_opts = opts;
                            enumerate_mitems(pn, spn, pdt, op);
                            for (k = 0; k < 15; ++k) {
                                /* skip "no specific protocol" (0xf) */
                                ccp = sdp_get_transport_name(k, sizeof(b), b);
                                if (0 == strlen(ccp))
                                    continue;
                                printf("\n");
                                printf("Mode pages for %s transport "
                                       "protocol:\n", ccp);
                                t_opts.transport = k;
                                t_opts.vendor_id = -1;
                                enumerate_mitems(pn, spn, pdt, &t_opts);
                            }
                            for (k = 0; k < sdparm_vendor_mp_len; ++k) {
                                if (VENDOR_NONE == k)
                                    continue;
                                ccp = sdp_get_vendor_name(k);
                                if (NULL == ccp)
                                    continue;
                                printf("\n");
                                printf("Mode pages for %s vendor:\n", ccp);
                                t_opts.transport = -1;
                                t_opts.vendor_id = k;
                                enumerate_mitems(pn, spn, pdt, &t_opts);
                            }
                        } else {
                            for (k = 0; k < 15; ++k) {
                                /* skip "no specific protocol" (0xf) */
                                ccp = sdp_get_transport_name(k, sizeof(b), b);
                                if (0 == strlen(ccp))
                                    continue;
                                printf("\n");
                                printf("Mode pages for %s transport "
                                       "protocol:\n", ccp);
                                enumerate_mpages(k, -1);
                            }
                            for (k = 0; k < sdparm_vendor_mp_len; ++k) {
                                if (VENDOR_NONE == k)
                                    continue;
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
                        printf("Generic mode pages:\n");
                        enumerate_mpages(-1, -1);
                        if (op->do_all)
                            enumerate_mitems(pn, spn, pdt, op);
                    }
                }
            } else      /* given mode page number */
                enumerate_mitems(pn, spn, pdt, op);
            return 0;
        }

        if (op->num_desc && (pn < 0)) {
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
            if (! build_mp_settings(set_str, &mp_settings, op, false, false))
                return SG_LIB_SYNTAX_ERROR;
        }
        if (clear_str) {
            if (! build_mp_settings(clear_str, &mp_settings, op, true, false))
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
        if (! f2hex_arr(op->inhex_fn, op->do_raw, false, inhex_buff,
                        &inhex_len, sizeof(inhex_buff)))
            return SG_LIB_FILE_ERROR;
        if (op->verbose > 2)
            pr2serr("Read %d bytes from user input\n", inhex_len);
        if (op->verbose > 3)
            dStrHexErr((const char *)inhex_buff, inhex_len, 0);
        op->do_raw = false;
        if (op->inquiry)
            return sdp_process_vpd_page(0, pn, ((spn < 0) ? 0: spn), op,
                                        pdt, protect, inhex_buff, inhex_len);
        else
            return print_mode_page_inhex(inhex_buff, inhex_len, op);
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
            pr2serr("close error: %s\n", safe_strerror(-res));
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
find_corresponding_sg_fd(int oth_fd, const char * device_name, bool rw,
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
map_if_lk24(int sg_fd, const char * device_name, bool rw, int verbose)
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

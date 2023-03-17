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

/*
 * sdparm is a utility program for getting and setting parameters on devices
 * that use one of the SCSI command sets. In some cases commands can be sent
 * to the device (e.g. eject removable media). The parameters are usually
 * arranged into pages. The pages are know as 'mode pages' (for parameters
 * that may be altered) and 'Vital Product Data (VPD) pages (for parameters
 * that seldom, if ever, change). Each page contains parameters that are
 * logically similar (e.g. the Caching mode page contains both the Write
 * Cache Enable (WCE) and Read Cache Disable (RCD) parameters).
 *
 *
 */

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
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

static const char * version_str = "1.17 20230316 [svn: r372]";


#define MAX_DEV_NAMES 256

static uint8_t * inhex_buffp;
static uint8_t * cur_aligned_mp;
static uint8_t * cha_aligned_mp;
static uint8_t * def_aligned_mp;
static uint8_t * sav_aligned_mp;
static uint8_t * oth_aligned_mp;
static uint8_t * free_inhex_buffp;
static uint8_t * free_cur_aligned_mp;
static uint8_t * free_cha_aligned_mp;
static uint8_t * free_def_aligned_mp;
static uint8_t * free_sav_aligned_mp;
static uint8_t * free_oth_aligned_mp;


static struct option long_options[] = {
    {"six", no_argument, 0, '6'},
    {"all", no_argument, 0, 'a'},
    {"dbd", no_argument, 0, 'B'},
    {"clear", required_argument, 0, 'c'},
    {"command", required_argument, 0, 'C'},
    {"defaults", no_argument, 0, 'D'},
    {"dummy", no_argument, 0, 'd'},
    {"enumerate", no_argument, 0, 'e'},
    {"examine", no_argument, 0, 'E'},
    {"flexible", no_argument, 0, 'f'},
    {"get", required_argument, 0, 'g'},
    {"help", no_argument, 0, 'h'},
    {"hex", no_argument, 0, 'H'},
    {"inquiry", no_argument, 0, 'i'},
    {"inhex", required_argument, 0, 'I'},
    {"inner-hex", no_argument, 0, 'x'},
    {"inner_hex", no_argument, 0, 'x'},
    {"json", optional_argument, 0, 'j'},
    {"js-file", required_argument, 0, 'J'},
    {"js_file", required_argument, 0, 'J'},
    {"long", no_argument, 0, 'l'},
    {"num-desc", no_argument, 0, 'n'},
    {"num_desc", no_argument, 0, 'n'},
    {"numdesc", no_argument, 0, 'n'},
    {"out-mask", required_argument, 0, 'o'},
    {"out_mask", required_argument, 0, 'o'},
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

/* incremented when subpage requested but non-subpage returned */
static int non_spg_warning;

static const char * ms_s = "Mode sense";
static const char * ump_s = "Mode page";
static const char * mp_s = "mode page";
static const char * mpgf_s = "Mode pages for";
static const char * sdp_sn = "sdparm";
static const char * sdp_rsp_sn = "sdparm_response";
static const char * sdp_enum_sn = "sdparm_enumeration";
static const char * cur_s = "current";
static const char * cha_s = "changeable";
static const char * def_s = "default";
static const char * sav_s = "saveable";
static const char * cffa_s = "couldn't find field acronym";
static const char * tn_s = "'--transport=<tn>'";
static const char * vn_s = "'--vendor=<vn>'";

static int print_mode_pages(int sg_fd, int pn, int spn, int pdt,
                            struct sdparm_opt_coll * op, sgj_opaque_p jop);


static void mp_rd_usage(bool long_opt)
{
    if (long_opt)
        pr2serr(
            "    sdparm [--all] [--dbd] [--examine] [--flexible] [--get=STR] "
            "[--hex]\n"
            "           [--inner-hex] [--json[=JO]] [--js-file=JFN] "
            "[--long]\n"
            "           [--num-desc] [--out-mask=OM] [--page=PG[,SPG]] "
            "[--quiet]\n"
            "           [--readonly] [--six] [--transport=TN] "
            "[--vendor=VN]\n"
            "           [--verbose] DEVICE [DEVICE...]\n"
              );
    else
        pr2serr(
            "    sdparm [-a] [-B] [-E] [-f] [-g STR] [-H] [-x] [-j[JO]] "
            "[-J JFN] [-l]\n"
            "           [-n] [-o OM] [-p PG[,SPG]] [-q] [-r] [-6] [-t TN] "
            "[-M VN] [-v]\n"
            "           DEVICE [DEVICE...]\n"
              );
}

static void mp_wr_usage(bool long_opt)
{
    if (long_opt)
        pr2serr(
            "    sdparm [--clear=STR] [--defaults] [--dummy] [--flexible]\n"
            "           [--page=PG[,SPG]] [--quiet] [--save] [--set=STR] "
            "[--six]\n"
            "           [--transport=TN] [--vendor=VN] [--verbose]\n"
            "           DEVICE [DEVICE...]\n"
              );
    else
        pr2serr(
            "    sdparm [-c STR] [-D] [-d] [-f] [-p PG[,SPG]] [-q] [-S] "
            "[-s STR] [-6]\n"
            "           [-t TN] [-M VN] [-v] DEVICE [DEVICE...]\n"
              );
}

static void inq_usage(bool long_opt)
{
    if (long_opt)
        pr2serr(
            "    sdparm --inquiry [--all] [--examine] [--flexible] "
            "[--hex]\n"
            "           [--json[=JO]] [--js-file=JFN] [--num-desc] "
            "[--page=PG[,SPG]]\n"
            "           [--quiet] [--read‐only] [--transport=TN] "
            "[--vendor=VN]\n"
            "           [--verbose] DEVICE [DEVICE...]\n"
              );
    else
        pr2serr(
            "    sdparm -i [-a] [-E] [-f] [-H] [-j[JO]] [-J JFN] [-n] "
            "[-p PG[,SPG]]\n"
            "           [-q] [-r] [-t TN]  [-M VN] [-v] DEVICE [DEVICE...]\n"
              );
}

static void cmd_usage(bool long_opt)
{
    if (long_opt)
        pr2serr(
            "    sdparm --command=CMD [--hex] [--long] [--readonly] "
            "[--verbose]\n"
            "           DEVICE [DEVICE...]\n"
              );
    else
        pr2serr(
            "    sdparm -C CMD [-H] [-l] [-r] [-v] DEVICE [DEVICE...]\n"
              );
}

static void enum_usage(bool long_opt)
{
    if (long_opt)
        pr2serr(
            "    sdparm --enumerate [--all] [--inquiry] [--long] "
            "[--page=PG[,SPG]]\n"
            "           [--transport=TN] [--vendor=VN]\n"
              );
    else
        pr2serr(
            "    sdparm -e [-a] [-i] [-l] [-p PG[,SPG]] [-t TN] [-M VN]\n"
              );
}

static void inhex_usage(bool long_opt)
{
    if (long_opt)
        pr2serr(
            "    sdparm --inhex=FN [--all] [--flexible] [--hex] "
            "[--inner-hex]\n"
            "           [--inquiry] [--json[=JO]] [--js-file=JFN] "
            "[--long]\n"
            "           [--out=mask=,IM] [--pdt=DT] [--raw] [--six]\n"
            "           [--transport=TN] [--vendor=VN] [--verbose]\n"
              );
    else
        pr2serr(
            "    sdparm -I FN [-a] [-f] [-H] [-x] [-i] [-j[JO]] [-J JFN] "
            "[-l]\n"
            "           [-o ,IM] [-P PDT] [-R] [-6] [-t TN] [-M VN] [-v]\n"
              );
}

static void
usage(const struct sdparm_opt_coll * op)
{
    if (0 == op->do_help) {
        pr2serr("Usage with long form options for %s access:\n", mp_s);
        mp_rd_usage(true);
        pr2serr(" Usage with corresponding short form options:\n");
        mp_rd_usage(false);
        pr2serr("\n");
        pr2serr("Usage with long form options for %s changes:\n", mp_s);
        mp_wr_usage(true);
        pr2serr(" Usage with corresponding short form options:\n");
        mp_wr_usage(false);
        pr2serr("\n");
        pr2serr("Usage with long form options for VPD page access:\n");
        inq_usage(true);
        pr2serr(" Usage with corresponding short form options:\n");
        inq_usage(false);
        pr2serr("\n");
        pr2serr("Usage with long form options for SCSI commands:\n");
        cmd_usage(true);
        pr2serr(" Usage with corresponding short form options:\n");
        cmd_usage(false);
        pr2serr("\n");
        pr2serr("Usage with long form options for enumerating internal "
                "tables:\n");
        enum_usage(true);
        pr2serr(" Usage with corresponding short form options:\n");
        enum_usage(false);
        pr2serr("\n");
        pr2serr("Usage with long form options with inhex:\n");
        inhex_usage(true);
        pr2serr(" Usage with corresponding short form options:\n");
        inhex_usage(false);
        return;
    }
    if (1 == op->do_help) {
        pr2serr("Usage for mode pages:\n");
        mp_rd_usage(true);
        mp_wr_usage(true);
        pr2serr("\n");
        pr2serr(
            // "       sdparm   << for more usages, see 'sdparm -hh' >>\n\n"

            "  where mode page access (1st usage) and change (2nd usage) "
            "options are:\n"
            "    --all | -a            list all known pages and fields for "
            "given DEVICE\n"
            "    --clear=STR | -c STR    clear (zero) field value(s), or "
            "set to 'val'\n"
            "    --dbd | -B            set DBD bit in mode sense cdb\n"
            "    --defaults | -D       set a mode page to its default "
            "values\n"
            "                          when use twice set all pages to "
            "their defaults\n"
            "    --dummy | -d          don't write back modified mode page\n"
            "    --flexible | -f       compensate for common errors, "
            "relax some checks\n"
            "    --get=STR | -g STR    get (fetch) field value(s), by "
            "acronym or pos\n"
            "    --hex | -H            output in hex rather than name/value "
            "pairs\n"
            "    --inner-hex | -x      print innermost fields in hex\n"
            "    --json[=JO] | -jJO    output in JSON instead of plain "
            "text\n"
            "                          test. Use --json=? for JSON help\n"
            "    --long | -l           add description to field output\n"
            "    --num-desc | -n       report number of mode page "
            "descriptors\n"
            "    --out-mask=OM | -o OM    select whether current(1), "
            " changeable(2),\n"
            "                             default(4) and/or saveable "
            "values(8)\n"
            "                             are output, (def: all(0xf))\n"
            "    --page=PG[,SPG] | -p PG[,SPG]    page (and optionally "
            "subpage) number\n"
            "                          [or abbrev] to output, change or "
            "enumerate\n"
            "    --quiet | -q          suppress DEVICE vendor/product/"
            "revision strings\n"
            "    --readonly | -r       force read-only open of DEVICE (def: "
            "depends\n"
            "                          on operation). Mainly for ATA disks\n"
            "    --save | -S           place mode changes in saved page as "
            "well\n"
            "    --set=STR | -s STR    set field value(s) to 1, or to "
            "'val'\n"
            "    --six | -6            use 6 byte SCSI mode cdbs (def: 10 "
            "byte)\n"
            "    --transport=TN | -t TN    transport protocol number "
            "[or abbrev]\n"
            "    --vendor=VN | -M VN    vendor (manufacturer) number "
            "[or abbrev]\n"
            "    --verbose | -v        increase verbosity\n"
            "\nAccess or change SCSI mode page fields (e.g. of a disk or "
            "CD/DVD drive).\nSTR can be <acronym>[=val] or "
            "<start_byte>:<start_bit>:<num_bits>[=val].\nUse '-h' or "
            "'--help' twice or more for help on other usages.\n"
#if 0
            "including "
            "executing\nsome simple commands, reading and decoding VPD "
            "pages, enumerating internal\ntables of mode and VPD pages, and "
            "decoding response data supplied in a\nfile or stdin (rather "
            "than from a DEVICE).\n"
#endif
           );

            return;
    }
    if (2 == op->do_help) {
        pr2serr("Usage for VPD pages and inhex:\n");
        inq_usage(true);
        inhex_usage(true);
        pr2serr("\n");
        pr2serr(
            "  where some additional options are:\n"
            "    --examine | -E        cycle through mode or vpd page "
            "numbers (default\n"
            "                          with '-a': only check pages with "
            "known fields)\n"
            "    --help | -h           print out usage message\n"
            "    --inhex=FN|-I FN      read ASCII hex from file FN instead "
            "of DEVICE;\n"
            "                          if used with -HH then read binary "
            "from FN\n"
            "    --inquiry | -i        output INQUIRY VPD page(s) (def: mode "
            "page(s))\n"
            "                          use --page=PG for VPD number (-1 "
            "for std inq)\n"
            "    --js-file=JFN | -J JFN    JFN is a filename to which JSON "
            "output is\n"
            "                              written (def: stdout); truncates "
            "then writes\n"
            "    --out-mask=,IM | -o ,IM    mask like '-o OM' but applies "
            "to inhex\n"
            "    --pdt=DT|-P DT        peripheral Device Type (e.g. "
            "0->disk)\n"
            "    --raw | -R            FN (in '-I FN') assumed to be "
            "binary\n"
            "    --version | -V        print version string and exit\n"
            "\nThe available commands will be listed when a invalid CMD is "
            "given\n(e.g. '--command=xxx'). VPD page(s) are read and decoded "
            "in the\n'--inquiry DEVICE' form. The '--enumerate' form outputs "
            "internal data\nabout mode or VPD pages (and ignores DEVICE if "
            "given). The '--inhex'\nform reads data from the the file FN "
            "(or stdin) and decodes it as a\nmode or VPD page response. The "
            "'--wscan' form is for listing Windows\ndevices and is only "
            "available on Windows machines.\n"
           );
        return;
    }
    if (3 == op->do_help) {
        pr2serr("Usage for commands, enumerate and others:\n");
        cmd_usage(true);
        enum_usage(true);
        pr2serr("\n");
        pr2serr(
            "  where some additional options are:\n"
            "    --command=CMD | -C CMD    perform CMD (e.g. 'eject')\n"
            "    --enumerate | -e      list known pages and fields "
            "(ignore DEVICE)\n"
            "    --wscan | -w          windows scan for device names\n"
        );
        return;
    }
    if (op->do_help < 2) {
        pr2serr(
            "       sdparm [--all] [--dbd] [--examine] [--flexible] "
            "[--get=STR] [--hex]\n"
            "              [--inner-hex] [--json[=JO]] [--js-file=JFN] "
            "[--long]\n"
            "              [--num-desc] [--out-mask=OM] [--page=PG[,SPG]] "
            "[--quiet]\n"
            "              [--readonly] [--six] [--transport=TN] "
            "[--vendor=VN]\n"
            "              [--verbose] DEVICE [DEVICE...]\n\n"
            "       sdparm [--clear=STR] [--defaults] [--dummy] "
            "[--flexible]\n"
            "              [--page=PG[,SPG]] [--quiet] [--readonly] "
            "[--save] [--set=STR]\n"
            "              [--six] [--transport=TN] [--vendor=VN] "
            "[--verbose]\n"
            "              DEVICE [DEVICE...]\n\n"
              );
        if (op->do_help < 1) {
            pr2serr(
                "       sdparm --command=CMD [--hex] [--long] [--readonly] "
                "[--verbose]\n"
                "              DEVICE [DEVICE...]\n\n"
                "       sdparm --inquiry [--all] [--examine] [--flexible] "
                "[--hex]\n"
                "              [--json[=JO]] [--js-file=JFN] [--num-desc] "
                "[--page=PG[,SPG]]\n"
                "              [--quiet] [--read‐only] [--transport=TN] "
                "[--vendor=VN]\n"
                "              [--verbose] DEVICE [DEVICE...]\n\n"
                "       sdparm --enumerate [--all] [--inquiry] [--long] "
                "[--page=PG[,SPG]]\n"
                "              [--transport=TN] [--vendor=VN]\n\n"
                "       sdparm --inhex=FN [--all] [--flexible] [--hex] "
                "[--inner-hex]\n"
                "              [--inquiry] [--json[=JO]] [--js-file=JFN] "
                "[--long]\n"
                "              [--out=mask=,IM] [--pdt=DT] [--raw] [--six]\n"
                "              [--transport=TN] [--vendor=VN] [--verbose]\n\n"
                "Or the corresponding short option usage: \n"
                "  sdparm [-a] [-B] [-E] [-f] [-g STR] [-H] [-x] [-j[JO]] "
                "[-J JFN] [-l]\n"
                "         [-n] [-o OM] [-p PG[,SPG]] [-q] [-r] [-6] [-t TN] "
                "[-M VN] [-v]\n"
                "         DEVICE [DEVICE...]\n"
                "\n"
                "  sdparm [-c STR] [-D] [-d] [-f] [-p PG[,SPG]] [-q] [-S] "
                "[-s STR] [-6]\n"
                "         [-t TN] [-M VN] [-v] DEVICE [DEVICE...]\n"
                "\n"
                "  sdparm -C CMD [-H] [-l] [-r] [-v] DEVICE [DEVICE...]\n"
                "\n"
                "  sdparm -i [-a] [-E] [-f] [-H] [-j[JO]] [-J JFN] [-n] "
                "[-p PG[,SPG]]\n"
                "         [-q] [-r] [-t TN]  [-M VN] [-v] DEVICE "
                "[DEVICE...]\n"
                "\n"
                "  sdparm -e [-a] [-i] [-l] [-p PG[,SPG]] [-t TN] [-M VN]\n"
                "\n"
                "  sdparm -I FN [-a] [-f] [-H] [-x] [-j[JO]] [-J JFN] [-i] "
                "[-l]\n"
                "         [-o ,IM] [-P PDT] [-R] [-6] [-t TN] [-M VN] [-v]\n"
                   );
            pr2serr("\nFor help use '-h' one or more times\n");
            return;
        }
        pr2serr(
            "       sdparm   << for more usages, see 'sdparm -hh' >>\n\n"

            "  where mode page read (1st usage) and change (2nd usage) "
            "options are:\n"
            "    --all | -a            list all known pages and fields for "
            "given DEVICE\n"
            "    --clear=STR | -c STR    clear (zero) field value(s), or "
            "set to 'val'\n"
            "    --dbd | -B            set DBD bit in mode sense cdb\n"
            "    --defaults | -D       set a mode page to its default "
            "values\n"
            "                          when use twice set all pages to "
            "their defaults\n"
            "    --dummy | -d          don't write back modified mode page\n"
            "    --examine | -E        cycle through mode or vpd page "
            "numbers (default\n"
            "                          with '-a': only check pages with "
            "known fields)\n"
            "    --flexible | -f       compensate for common errors, "
            "relax some checks\n"
            "    --get=STR | -g STR    get (fetch) field value(s), by "
            "acronym or pos\n"
            "    --hex | -H            output in hex rather than name/value "
            "pairs\n"
            "    --json[=JO]|-jJO      output in JSON instead of plain "
            "text\n"
            "                          test. Use --json=? for JSON help\n"
            "    --js-file=JFN|-J JFN    JFN is a filename to which JSON "
            "output is\n"
            "                            written (def: stdout); truncates "
            "then writes\n"
            "    --long | -l           add description to field output\n"
            "    --num-desc | -n       report number of mode page "
            "descriptors\n"
            "    --out-mask=OM | -o OM    select whether current(1), "
            " changeable(2),\n"
            "                             default(4) and/or saveable "
            "values(8)\n"
            "                             are output, (def: all(0xf))\n"
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
            "    --set=STR | -s STR    set field value(s) to 1, or to "
            "'val'\n"
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
    } else {
        pr2serr("Further usages of the sdparm utility:\n"
            "       sdparm --command=CMD [-hex] [--long] [--readonly] "
            "[--verbose]\n"
            "              DEVICE [DEVICE...]\n\n"
            "       sdparm --inquiry [--all] [--flexible] [--hex]\n"
            "              [--page=PG[,SPG]] [--quiet] [--readonly] "
            "[--transport=TN]\n"
            "              [--vendor=VN] [--verbose] DEVICE [DEVICE...]\n\n");
        pr2serr("       sdparm --enumerate [--all] [--inquiry] [--long] "
            "[--page=PG[,SPG]]\n"
            "              [--transport=TN] [--vendor=VN]\n\n"
            "       sdparm --inhex=FN [--all] [--flexible] [--hex] "
            "[--inquiry]\n"
            "              [--long] [--pdt=PDT] [--raw] [--six] "
            "[--transport=TN]\n"
            "              [--vendor=VN]\n\n"
            "       sdparm --wscan [--verbose]\n\n"
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
            "    --out-mask=,IM | -o ,IM    mask like '-o OM' but applies "
            "to inhex\n"
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
}

static void
enumerate_mpage_names(int transport, int vendor_id,
                      struct sdparm_opt_coll * op)
{
    sgj_state * jsp = &op->json_st;
    const struct sdparm_mp_name_t * mnp;
    char b[144];
    char c[80];
    static const int blen = sizeof(b);
    static const int clen = sizeof(c);

    if (vendor_id >= 0) {
        const struct sdparm_vendor_pair * vpp;

        vpp = sdp_get_vendor_pair(vendor_id);
        if (NULL == vpp) {
            pr2serr("Bad vendor identifier (number)\n");
            return;
        }
        mnp = vpp->mpage;
    } else if ((transport >= 0) && (transport < 16))
        mnp = sdparm_transport_mp[transport].mpage;
    else
        mnp = sdparm_gen_mode_pg;

    for ( ; mnp && mnp->mp_acron; ++mnp) {
        if (mnp->mp_name) {
            if (mnp->subpage)
                sg_scnpr(b, blen, "  %-4s 0x%02x,0x%02x  %s", mnp->mp_acron,
                         mnp->page, mnp->subpage, mnp->mp_name);
            else
                sg_scnpr(b, blen, "  %-4s 0x%02x       %s", mnp->mp_acron,
                         mnp->page, mnp->mp_name);
            if (op->do_long > 2) {
                if (mnp->js_name)
                    sgj_pr_hr(jsp, "%s\n\t\t[json_name: %s]\n", b,
                              mnp->js_name);
                else {
                    sdp_mp_convert2snake(mnp->mp_name, c, clen);
                    sgj_pr_hr(jsp, "%s\n\t\t[json_name: %s]\n", b, c);
                }
            } else
                sgj_pr_hr(jsp, "%s\n", b);
        }
    }
}

static void
enumerate_vpd_names(struct sdparm_opt_coll * op)
{
    const struct sdparm_vpd_page_t * vpp;
    sgj_state * jsp = &op->json_st;

    for (vpp = sdparm_vpd_pg; vpp->vpd_acron; ++vpp) {
        if (vpp->name) {
            if (vpp->vpd_num < 0)
                sgj_pr_hr(jsp, "  %-10s -1        %s\n", vpp->vpd_acron,
                          vpp->name);
            else
                sgj_pr_hr(jsp, "  %-10s 0x%02x      %s\n", vpp->vpd_acron,
                          vpp->vpd_num, vpp->name);
        }
    }
}

static void
enumerate_transport_names(bool multiple_acrons, struct sdparm_opt_coll * op)
{
    int ln;
    const struct sdparm_val_desc_t * addp;
    const struct sdparm_val_desc_t * t_vdp;
    sgj_state * jsp = &op->json_st;
    char b[64];
    char d[128];
    static const int blen = sizeof(b);
    static const int dlen = sizeof(d);

    for (t_vdp = sdparm_transport_id; t_vdp->desc; ++t_vdp) {
        if (op->do_long || multiple_acrons) {
            snprintf(d, dlen, "%s", t_vdp->desc);
            ln = strlen(d);
            for (addp = sdparm_add_transport_acron; addp->desc; ++addp) {
                if ((addp->val == t_vdp->val) &&
                    (dlen >= (ln - 1))) {
                    snprintf(d + ln, dlen - ln, ",%s", addp->desc);
                    ln = strlen(d);
                }
            }
            sgj_pr_hr(jsp, "  %-24s 0x%02x     %s\n", d, t_vdp->val,
                      sg_get_trans_proto_str(t_vdp->val, blen, b));
        } else
            sgj_pr_hr(jsp, "  %-6s 0x%02x     %s\n", t_vdp->desc, t_vdp->val,
                      sg_get_trans_proto_str(t_vdp->val, blen, b));
    }
}

static void
enumerate_vendor_names(struct sdparm_opt_coll * op)
{
    const struct sdparm_vendor_name_t * vnp;
    sgj_state * jsp = &op->json_st;

    for (vnp = sdparm_vendor_id; vnp->acron; ++vnp) {
        if (vnp->name && (VENDOR_NONE != vnp->vendor_id))
            sgj_pr_hr(jsp, "  %-6s 0x%02x     %s\n", vnp->acron,
                      vnp->vendor_id, vnp->name);
    }
}

static void
print_mp_extra(const char * extra, struct sdparm_opt_coll * op,
               sgj_opaque_p jop)
{
    int n;
    int m = 0;
    int o = 0;
    char * cp;
    char * p;
    sgj_state * jsp = &op->json_st;
    char b[128];
    char d[256];
    char e[256];
    static const int blen = sizeof(b);
    static const int dlen = sizeof(d);
    static const int elen = sizeof(e);

    d[0] = '\0';
    for (p = (char *)extra; (cp = (char *)strchr(p, '\t')); p = cp + 1) {
        n = cp - p;
        if (n > (blen - 1))
            n = blen - 1;
        strncpy(b, p, n);
        b[n] = '\0';
        m += sg_scnpr(d + m, dlen - m, "\t%s\n", b);
        o += sg_scnpr(e + o, elen - o, "%s; ", b);
    }
    sgj_pr_hr(jsp, "%s\t%s\n", d, p);
    if (op->do_json) {
        snprintf(d, dlen, "%.200s%s", e, p);
        sgj_js_nv_s(jsp, jop, "extra", d);
    }
}

static sgj_opaque_p
jsonify_mpip(const struct sdparm_mp_item_t *mpip, struct sdparm_opt_coll * op,
             sgj_opaque_p jop)
{
    int m;
    sgj_state * jsp = &op->json_st;
    sgj_opaque_p jo2p;
    const char * cp;
    const char * js_nm;
    const char * js_acron = NULL;
    const char * js_desc = NULL;
    char d[96];
    char e[80];
    static const int elen = sizeof(e);
    static const char * no_name = "no_name";

    if (mpip->js_name) {
        js_nm = mpip->js_name;
        js_acron = mpip->acron;
        js_desc = mpip->description;
    } else if (MF_J_USE_DESC & mpip->flags) {
        js_nm = mpip->description;
        js_acron = mpip->acron;
        js_desc = mpip->description;
    } else if (MF_J_NPARAM_DESC & mpip->flags) {
        /* truncate mpip->description at first '(' */
        cp = strchr(mpip->description, '(');
        if (cp && (cp > mpip->description)) {
            m = cp - mpip->description - 1;
            memcpy(d, mpip->description, m);
            d[m] = '\0';
            js_nm = d;  /* trailing spaces removed by sgj_convert2snake */
        } else {
            pr2serr("%s: bad JSON name candidate: %s\n", __func__,
                    mpip->description);
            js_nm = no_name;
        }
        js_acron = mpip->acron;
        js_desc = mpip->description;
    } else if (mpip->acron) {
        js_nm = mpip->acron;
        js_acron = mpip->acron;
        js_desc = mpip->description;
    } else
        js_nm = no_name;
    jo2p = sgj_named_subobject_r(jsp, jop,
                                 sgj_convert2snake(js_nm, e, elen));
    if (js_acron)
        sgj_js_nv_s(jsp, jo2p, "acronym", js_acron);
    if (js_desc)
        sgj_js_nv_s(jsp, jo2p, "description", js_desc);
    return jo2p;
}

/* Enumerate mode page items. Reading from internal dataset. */
static void
enumerate_mitems(int pn, int spn, int pdt, struct sdparm_opt_coll * op,
                 sgj_opaque_p jop)
{
    bool found = false;
    bool have_desc = false;
    bool have_desc_id = false;
    int t_pn, t_spn, t_com_pdt, vendor_id, transp, long_o, e_num, decay_pdt;
    int t;
    sgj_state * jsp = &op->json_st;
    sgj_opaque_p jo2p = NULL;
    sgj_opaque_p jo3p = NULL;
    const struct sdparm_mp_name_t * mnp = NULL;
    const struct sdparm_mode_descriptor_t * mdp = NULL;
    const struct sdparm_mp_item_t * mpip;
    char b[256];
    char d[128];
    char e[128];
    static const int blen = sizeof(b);
    static const int dlen = sizeof(d);
    static const int elen = sizeof(e);

    t_pn = -1;
    t_spn = -1;
    t_com_pdt = -2;
    vendor_id = op->vendor_id;
    transp = op->transport;
    long_o = op->do_long;
    e_num = op->do_enum;
    decay_pdt = sg_lib_pdt_decay(pdt);
    if (decay_pdt != pdt) {
        if (op->verbose > 1)
            pr2serr("%s: decaying pdt=0x%x to 0x%x\n", __func__, pdt,
                    decay_pdt);
        pdt = decay_pdt;
    }
    if (vendor_id >= 0) {
        const struct sdparm_vendor_pair * vpp;

        vpp = sdp_get_vendor_pair(vendor_id);
        mpip = (vpp ? vpp->mitem : NULL);
    } else if ((transp >= 0) && (transp < 16))
        mpip = sdparm_transport_mp[transp].mitem;
    else
        mpip = sdparm_mitem_arr;
    if (NULL == mpip)
        return;

    for ( ; mpip->acron; ++mpip) {
        if (! sg_pdt_s_eq(pdt, mpip->com_pdt))
            continue;
        if ((t_pn == mpip->pg_num) && (t_spn == mpip->subpg_num) &&
            sg_pdt_s_eq(t_com_pdt, mpip->com_pdt)) {
            if ((pn >= 0) && ((pn != t_pn) || (spn != t_spn)))
                continue;
        } else {
            t_pn = mpip->pg_num;
            t_spn = mpip->subpg_num;
            t_com_pdt = mpip->com_pdt;
            if ((pn >= 0) && ((pn != t_pn) || (spn != t_spn)))
                continue;
            if (! sg_pdt_s_eq(pdt, t_com_pdt))
                continue;
            mnp = sdp_get_mp_nm_with_str(t_pn, t_spn, t_com_pdt, transp,
                                         vendor_id, long_o, true, dlen, d);
            if (long_o && (transp < 0) && (vendor_id < 0))
                sgj_pr_hr(jsp, "%s [%s] %s:\n", d,
                          sg_get_pdt_str(t_com_pdt, elen, e), mp_s);
            else
                sgj_pr_hr(jsp, "%s %s:\n", d, mp_s);
            if (mnp && ((mdp = mnp->mp_desc))) {
                have_desc = true;
                have_desc_id = mdp->have_desc_id;
            } else {
                have_desc = false;
                have_desc_id = false;
            }
// yyyyyy
            if (op->do_json) {
                jo2p = sgj_named_subobject_r(jsp, jop,
                                        sdp_mp_convert2snake(d, b, blen));
                sgj_js_nv_i(jsp, jo2p, "page_code", t_pn);
                sgj_js_nv_i(jsp, jo2p, "spf", !! (t_spn > 0));
                if (t_spn > 0)
                    sgj_js_nv_i(jsp, jo2p, "subpage_code", t_spn);
                t = (0xff & t_com_pdt);
                if (0xff == t)
                    t = -1;
                sgj_js_nv_i(jsp, jo2p, "lower_pdt", t);
                t = (0xff & (t_com_pdt >> 8));
                if ((0xff != t) && (t > 0))
                    sgj_js_nv_i(jsp, jo2p, "upper_pdt", t);
                sgj_js_nv_i(jsp, jo2p, "descriptor_format", (int)have_desc);
            }
        }
        if (have_desc_id && (MF_CLASH_OK & mpip->flags))
            snprintf(b, blen, "[0x%02x:%d:%-2d  %d]", mpip->start_byte,
                     mpip->start_bit, mpip->num_bits,
                     sdp_get_desc_id(mpip->flags));
        else
            snprintf(b, blen, "[0x%02x:%d:%-2d]", mpip->start_byte,
                     mpip->start_bit, mpip->num_bits);

        sgj_pr_hr(jsp, "  %-10s %s  %s\n", mpip->acron, b, mpip->description);
        if (op->do_json) {
            jo3p = jsonify_mpip(mpip, op, jo2p);
            sgj_js_nv_s(jsp, jo3p, "position_length", b);
        }
        if (mpip->extra) {
            if ((long_o > 1) || (e_num > 1))
                print_mp_extra(mpip->extra, op, jo3p);
        }
        found = true;
    }
    if ((! found) && (pn >= 0)) {
        sdp_get_mp_nm_with_str(pn, spn, pdt, transp, vendor_id,
                               long_o, true, dlen, d);
        pr2serr("%s %s: no items found\n", d, mp_s);
    }
    if (found && have_desc && (long_o || (e_num > 1))) {
        if ((-1 == mdp->num_descs_off) && (-1 == mdp->num_descs_bytes))
            return;     /* Nothing to warn about in this case */
        if (mdp->name)
            sgj_pr_hr(jsp, "  <<%s mode descriptor>>\n", mdp->name);
        else
            sgj_pr_hr(jsp, "  <<mode descriptor>>\n");
        sgj_pr_hr(jsp, "    num_descs_off=%d, num_descs_bytes=%d, "
                  "num_descs_inc=%d, first_desc_off=%d\n",
                  mdp->num_descs_off, mdp->num_descs_bytes,
                  mdp->num_descs_inc,  mdp->first_desc_off);
        if (mdp->desc_len > 0)
            sgj_pr_hr(jsp, "    descriptor_len=%d, ", mdp->desc_len);
        else
            sgj_pr_hr(jsp, "    desc_len_off=%d, desc_len_bytes=%d, ",
                      mdp->desc_len_off, mdp->desc_len_bytes);
        sgj_pr_hr(jsp, "desc_id=%s\n", (have_desc_id ? "true" : "false"));
    }
}

#define SDP_HIGHEST_MPAGE_NUM 0x3e
#define SDP_HIGHEST_MSUBPAGE_NUM 0xfe

static int
examine_mode_pages(int sg_fd, int pn, int req_pdt,
                   struct sdparm_opt_coll * op, sgj_opaque_p jop)
{
    bool first = true;
    bool not_subpages = (pn < 0);
    int k, n, epn, espn, res;
    sgj_state * jsp = &op->json_st;

    if (pn > SDP_HIGHEST_MPAGE_NUM) {
        pr2serr("No %s numbers higher than 0x%x are allowed\n", mp_s,
                SDP_HIGHEST_MPAGE_NUM);
        return SG_LIB_SYNTAX_ERROR;
    }
    n = not_subpages ? SDP_HIGHEST_MPAGE_NUM : SDP_HIGHEST_MSUBPAGE_NUM;
    for (k = 0, res = 0; k <= n; ++k) {
        epn = not_subpages ? k : pn;
        espn = not_subpages ? 0 : k;
        if (first)
            first = false;
        else if (0 == res)
            sgj_pr_hr(jsp, "\n");
        if (op->verbose)
            pr2serr("%s: checking %s 0x%x,0x%x\n", __func__, mp_s, epn, espn);
        res = print_mode_pages(sg_fd, epn, espn, req_pdt, op, jop);
    }
    return 0;
}

#define SDP_MAX_T10_VPD_NUM 0xbf

static int
examine_vpd_page(int sg_fd, int pn, int spn, int req_pdt, bool protect,
                 struct sdparm_opt_coll * op, sgj_opaque_p jop)
{
    bool first = true;
    int k, n, res;
    sgj_state * jsp = &op->json_st;

    n = (spn < 0) ? SDP_MAX_T10_VPD_NUM : spn;
    for (k = (pn < 0) ? 0 : pn, res = 0; k <= n; ++k) {
        if (first)
            first = false;
        else if (0 == res)
            sgj_pr_hr(jsp, "\n");
        res = sdp_process_vpd_page(sg_fd, k, 0, req_pdt, protect, NULL, NULL,
                                   0, op, jop);
    }
    return 0;
}

static void
list_mp_settings(const struct sdparm_mp_settings_t * mps, bool getter,
                 struct sdparm_opt_coll * op)
{
    int k, n;
    const struct sdparm_mp_item_t * mpip;
    sgj_state * jsp = &op->json_st;
    char b[168];
    static const int blen = sizeof(b);

    sgj_pr_hr(jsp, "mp_settings: page,subpage=0x%x,0x%x  num=%d\n",
              mps->pg_num, mps->subpg_num, mps->num_it_vals);
    for (k = 0; k < mps->num_it_vals; ++k) {
        mpip = &mps->it_vals[k].mp_it;
        n = 0;
        if (getter)
            n += sg_scnpr(b + n, blen - n, "  [0x%x,0x%x]", mpip->pg_num,
                          mpip->subpg_num);

        n += sg_scnpr(b + n, blen - n, "  pdt=%d start_byte=0x%x "
                      "start_bit=%d num_bits=%d  val=%" PRId64 "",
                      mpip->com_pdt, mpip->start_byte, mpip->start_bit,
                      mpip->num_bits, mps->it_vals[k].val);
        if (mpip->acron) {
            n += sg_scnpr(b + n, blen - n, "  acronym: %s", mpip->acron);
            if (mps->it_vals[k].descriptor_num > 0)
                sg_scnpr(b + n, blen - n, "  descriptor_num=%d\n",
                         mps->it_vals[k].descriptor_num);
            else
                sgj_pr_hr(jsp, "%s\n", b);
        } else
            sgj_pr_hr(jsp, "%s\n", b);
    }
}

/* Print prefix and acronymn plus current, changeable [smask & MP_OM_CHA],
 * default [smask & MP_OM_DEF] and saved [smask & MP_OM_SAV] values for an
 * item. Return true if MF_STOP_IF_SET is given and current value is
 * non-zero. */
static bool
print_a_mitem(const char * pre, int smask,
              const struct sdparm_mp_item_t *mpip,
              const uint8_t * cur_mp,
              const uint8_t * cha_mp,
              const uint8_t * def_mp,
              const uint8_t * sav_mp,
              bool force_decimal,
              struct sdparm_opt_coll * op,
              sgj_opaque_p jop)
{
    bool all_set = false;
    bool prt_pre = false;
    bool sep = false;
    bool stop_if_set = false;
    int n = 0;
    uint64_t u;
    const char * acron;
    sgj_state * jsp = &op->json_st;
    sgj_opaque_p jo2p = NULL;
    char b[144];
    char e[80];
    static const int blen = sizeof(b);
    static const int elen = sizeof(e);

    acron = (mpip->acron ? mpip->acron : "");
    if (op->do_json)
        jo2p = jsonify_mpip(mpip, op, jop);

    if (MP_OM_CUR & smask) {
        u = sdp_mitem_get_value_check(mpip, cur_mp, &all_set);
        if (op->do_json)
            sgj_js_nv_i(jsp, jo2p, cur_s, u);
        n = sg_scnpr(b, blen, "%s%-14s", pre, acron);
        prt_pre = true;
        if (force_decimal || (mpip->flags & MF_TWOS_COMP)) {
            sdp_signed_decimal_str(u, mpip->num_bits, false, e, elen);
            n += sg_scnpr(b + n, blen - n, "%s", e);
        } else if (mpip->flags & MF_HEX) {
            if ((mpip->flags & MF_ALL_1S) && all_set)
                n += sg_scnpr(b + n, blen - n, "-1");
            else
                n += sg_scnpr(b + n, blen - n, "0x%" PRIx64 "", u);
        } else if (all_set)
            n += sg_scnpr(b + n, blen - n, "-1");
        else
            n += sg_scnpr(b + n, blen - n, "%" PRIu64 "", u);
        if ((mpip->flags & MF_STOP_IF_SET) && (u != 0))
            stop_if_set = true;
    }
    if ((smask & (MP_OM_CHA | MP_OM_DEF | MP_OM_SAV)) && (op->do_quiet < 2)) {
        if (prt_pre)
            n += sg_scnpr(b + n, blen - n, "  [");
        if (cha_mp && (smask & MP_OM_CHA)) {
            u = sdp_mitem_get_value(mpip, cha_mp);
            if (op->do_json)
                sgj_js_nv_i(jsp, jo2p, cha_s, u);
            if (! prt_pre) {
                n += sg_scnpr(b + n, blen - n, "%s%-14s  [", pre, acron);
                prt_pre = true;
            }
            n += sg_scnpr(b + n, blen - n, "cha: %s", (u ? "y" : "n"));
            sep = true;
        }
        if (def_mp && (smask & MP_OM_DEF)) {
            if (! prt_pre) {
                n += sg_scnpr(b + n, blen - n, "%s%-14s  [", pre, acron);
                prt_pre = true;
            }
            all_set = false;
            u = sdp_mitem_get_value_check(mpip, def_mp, &all_set);
            if (op->do_json)
                sgj_js_nv_i(jsp, jo2p, def_s, u);
            n += sg_scnpr(b + n, blen - n, "%sdef:", (sep ? ", " : " "));
            if (force_decimal || (mpip->flags & MF_TWOS_COMP)) {
                sdp_signed_decimal_str(u, mpip->num_bits, false, b, blen);
                n += sg_scnpr(b + n, blen - n, "%s", b);
            } else if (mpip->flags & MF_HEX) {
                if ((mpip->flags & MF_ALL_1S) && all_set)
                    n += sg_scnpr(b + n, blen - n, "-1");
                else
                    n += sg_scnpr(b + n, blen - n, "0x%" PRIx64 "", u);
            } else if (all_set)
                n += sg_scnpr(b + n, blen - n, " -1");
            else
                n += sg_scnpr(b + n, blen - n, "%3" PRIu64 "", u);
            sep = true;
        }
        if (sav_mp && (smask & MP_OM_SAV)) {
            if (! prt_pre) {
                n += sg_scnpr(b + n, blen - n, "%s%-14s  [", pre, acron);
                // prt_pre = true;
            }
            all_set = false;
            u = sdp_mitem_get_value_check(mpip, sav_mp, &all_set);
            if (op->do_json)
                sgj_js_nv_i(jsp, jo2p, sav_s, u);
            n += sg_scnpr(b + n, blen - n, "%ssav:", (sep ? ", " : " "));
            if (force_decimal || (mpip->flags & MF_TWOS_COMP)) {
                sdp_signed_decimal_str(u, mpip->num_bits, false, b, blen);
                n += sg_scnpr(b + n, blen - n, "%s", b);
            } else if (mpip->flags & MF_HEX) {
                if ((mpip->flags & MF_ALL_1S) && all_set)
                    n += sg_scnpr(b + n, blen - n, "-1");
                else
                    n += sg_scnpr(b + n, blen - n, "0x%" PRIx64 "", u);
            } else if (all_set)
                n += sg_scnpr(b + n, blen - n, " -1");
            else
                n += sg_scnpr(b + n, blen - n, "%3" PRIu64 "", u);
        }
        n += sg_scnpr(b + n, blen - n, "]");
    }
    if (op->do_long && mpip->description)
        n += sg_scnpr(b + n, blen - n, "  %s", mpip->description);
    sgj_pr_hr(jsp, "%s\n", b);
    if ((op->do_long > 1) && mpip->extra)
        print_mp_extra(mpip->extra, op, jo2p);
    return stop_if_set;
}

static bool
print_mitem_pc_arr(const char * pre, int smask,
                   const struct sdparm_mp_item_t *mpip,
                   void ** pc_arr, bool force_decimal,
                   struct sdparm_opt_coll * op, sgj_opaque_p jop)
{
    const uint8_t * cur_mp = (const uint8_t *)pc_arr[0];
    const uint8_t * cha_mp = (const uint8_t *)pc_arr[1];
    const uint8_t * def_mp = (const uint8_t *)pc_arr[2];
    const uint8_t * sav_mp = (const uint8_t *)pc_arr[3];

    return print_a_mitem(pre, smask, mpip, cur_mp, cha_mp, def_mp, sav_mp,
                         force_decimal, op, jop);
}

/* Gets current (page control) response from MODE SENSE (10(def) or 6) for
 * the given mode page ('pn') and subpage ('spn'). The start of the response
 * (if there is no error) is the mode parameter header followed by zero or
 * more block descriptors, followed by zero or more mode pages.  */
static int
ll_mode_sense(int fd, int pn, int spn, bool llbaa, uint8_t * resp,
              int mx_resp_len, int * residp, int verb,
              const struct sdparm_opt_coll * op)
{
    int res;
    const int vb = (verb >= 0) ? verb : op->verbose;

    if (op->mode_6) {
        if (residp)
            *residp = 0;
        res = sg_ll_mode_sense6(fd, op->dbd, 0 /*current */, pn, spn,
                                resp, mx_resp_len, true /* noisy */, vb);
    } else
        res = sg_ll_mode_sense10_v2(fd, llbaa, op->dbd, 0, pn,
                                    spn, resp, mx_resp_len, 0, residp,
                                    true /* noisy */, vb);
    if ((0 == res) && (vb > 2)) {
        int resid = residp ? *residp : 0;
        int num_ret = mx_resp_len - resid;

        pr2serr("[0x%x,0x%x]: MSns(%d), mx_resp_len=%d, resid=%d thus %d "
                "returned\n", pn, spn, op->mode_6 ? 6 : 10, mx_resp_len,
                resid, num_ret);
        if (vb > 3)
            hex2stderr(resp, num_ret, 1);
    }
    return res;
}

static int
report_error(int res, bool mode6)
{
    const char * cdbLenS = mode6 ? "6" : "10";
    char b[96];

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
    case 0:
        break;
    default:
        if (sg_exit2str(res, false, sizeof(b), b))
            pr2serr("%s(%s): %s\n", ms_s, cdbLenS, b);
        break;
    }
    return res;
}

/* Make some noise if the mode page response seems badly formed */
static void
check_mode_page(uint8_t * cur_mp, int pn, int rep_len,
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
            pr2serr(">>> warning: %s seems malformed\n   The page number "
                    "field should be 0x%02x, but is 0x%02x", mp_s, pn,
                    pn_in_page);
            if (! op->flexible)
                pr2serr("; try '--flexible'");
            pr2serr("\n");
        }
    } else if (op->verbose && (rep_len > 0xa00)) {
        pr2serr(">>> warning: %s length=%d too long", mp_s, rep_len);
        if (! op->flexible)
            pr2serr(", perhaps try '--flexible'");
        pr2serr("\n");
    }
}

/* When mode page has descriptor, print_mode_pages() and
 * print_inhex_mode_pages() will print the items associated with the first
 * descriptor. This function is called to print items associated with
 * subsequent descriptors */
static void
print_mitem_desc_after1(void ** pc_arr, int rep_len,
                        const struct sdparm_mp_name_t * mnp,
                        const struct sdparm_mp_item_t * fdesc_mpip,
                        const struct sdparm_mp_item_t * last_mpip,
                        int smask, int desc_len1, bool stop_if_set,
                        struct sdparm_opt_coll * op, sgj_opaque_p jop)
{
    bool sis;
    bool broke = false;
    bool have_desc_id = false;
    int k, len, d_off, n;
    int desc_id = -1;
    int num = 0;
    const struct sdparm_mode_descriptor_t * mdp = mnp->mp_desc;
    const struct sdparm_mp_item_t * mpip;
    const uint8_t * cur_mp = (const uint8_t *)pc_arr[0];
    const uint8_t * cha_mp = (const uint8_t *)pc_arr[1];
    const uint8_t * def_mp = (const uint8_t *)pc_arr[2];
    const uint8_t * sav_mp = (const uint8_t *)pc_arr[3];
    const uint8_t * bp;
    struct sdparm_mp_item_t a_mp_it;
    uint64_t u;
    char b[32];

    if ((NULL == mdp) || (NULL == cur_mp) || (rep_len < 4) ||
        (mdp->num_descs_off >= rep_len))
        return;
    if ((mdp->num_descs_inc < 0) && (mdp->desc_len > 0)) {
        k = mdp->first_desc_off - (mdp->num_descs_off + mdp->num_descs_bytes);
        u = (sg_get_big_endian(cur_mp + mdp->num_descs_off, 7,
                               mdp->num_descs_bytes * 8) - k) /
            mdp->desc_len;
        num = (int)u;
    } else if (mdp->num_descs_bytes > 0) {
        u = sg_get_big_endian(cur_mp + mdp->num_descs_off, 7,
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
            u = sg_get_big_endian(bp + off, 7, mdp->desc_len_bytes * 8);
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
        pr2serr("    >>> %d descriptors in %s\n", num, mp_s);

    if (((num >= 0) && (num < 2)) || (num > 512))
        return;
    if (have_desc_id) {
        if (desc_len1 <= 0) {
            pr2serr("%s: desc_id logic fails for acron: %s\n", __func__,
                    fdesc_mpip->acron ? fdesc_mpip->acron : "??");
            return;
        }
        len = desc_len1;
    } else if (mdp->desc_len <= 0) {
        bp = cur_mp + mdp->first_desc_off + mdp->desc_len_off;
        u = sg_get_big_endian(bp, 7, mdp->desc_len_bytes * 8);
        len = mdp->desc_len_off + mdp->desc_len_bytes + u;
    } else
        len = mdp->desc_len;
    d_off = mdp->first_desc_off + len;
    for (k = 1; k < num; ++k, d_off += len) {
        if (op->verbose > 3)
            pr2serr("%s: desc_len1=%d, d_off=%d, len=%d\n", __func__,
                    desc_len1, d_off, len);
        if ((! op->flexible) && stop_if_set)
            break;
        broke = false;
        for (mpip = fdesc_mpip; mpip <= last_mpip; ++mpip) {
            if (have_desc_id) {
                if (mpip->flags & MF_CLASH_OK) {
                    if (desc_id != sdp_get_desc_id(mpip->flags))
                        continue; /* skip this item due to desc_id mismatch */
                } else {
                    desc_id = 0xf & *(cur_mp + d_off);
                    len = sg_get_unaligned_be16(cur_mp + d_off + 2) + 4;
                }
            }
            sg_scnpr(b, sizeof(b), "%s", mpip->acron);
            a_mp_it = *mpip;
            b[sizeof(b) - 8] = '\0';
            n = strlen(b);
            snprintf(b + n, sizeof(b) - n, ".%d", k);
            a_mp_it.acron = b;
            a_mp_it.start_byte += (d_off - mdp->first_desc_off);
            if (a_mp_it.start_byte >= rep_len) {
                pr2serr("descriptor overflows reply len (%d) for %s\n",
                        rep_len, a_mp_it.acron);
                broke = true;
                break;
            }
            sis = print_a_mitem("  ", smask, &a_mp_it, cur_mp, cha_mp, def_mp,
                                sav_mp, false, op, jop);
            if (sis)
                stop_if_set = true;
        }
        if (broke)
            break;
        if ((mdp->desc_len <= 0) && (mdp->num_descs_bytes > 0) &&
            (! have_desc_id)) {
            bp = cur_mp + d_off + mdp->desc_len_off;
            u = sg_get_big_endian(bp, 7, mdp->desc_len_bytes * 8);
            len = mdp->desc_len_off + mdp->desc_len_bytes + u;
        }
    }
}

static const char * pc_nm_arr[] =
                        {"current", "changeable", "default", "saved"};

/* Now we really want to get _all_ pages and possibly subpages for all
 * page controls unless pn>=0 . When pn>=0 then the [pn,spn] mode page
 * is fetched (with leading parameter header and block descriptors.
 * In the "single page" case op->mode_6 is followed. In the "all pages"
 * case MODE SENSE (10) is used irrespective of op->mode_6, first it
 * attempts get all pages and subpages. If that fails (and no page
 * controls are fetched) then it tries again, the second time requesting
 * all pages, but no subpages. */
static int
print_mps_in_hex(int sg_fd, int pn, int spn, struct sdparm_opt_coll * op)
{
    bool l_mode_6 = op->mode_6;
    bool spf, break_after;
    bool llbaa = true;
    uint16_t bd_len, md_len;
    int k, j1, res, req_len, resp_len, off, l_pn, l_spn, pg_len, rmask;
    int  msk, l_msk, l_j1, resid;
    int dhex = op->do_hex;
    int trans = op->transport;
    int verb = (op->verbose > 0) ? op->verbose - 1 : 0;
    uint8_t * pg_p;
    uint8_t * pg1_p;
    const struct sdparm_mp_name_t * mnp;
    void * pc_arr[4];
    uint8_t * oth_mp = oth_aligned_mp;

    /* First get mode parameter header and any following block descriptors */
    memset(oth_mp, 0, op->mode_6 ? DEF_MODE_6_RESP_LEN : DEF_MODE_RESP_LEN);
    req_len = 8 + (15 * 16);    /* allow for 15 'long' block descriptors */
    l_pn = pn;
    l_spn = spn;
    if (pn < 0) {
        l_pn = ALL_MPAGES;
        /* If version >= SPC then ask for all subpages as well */
        l_spn = ((0xf & op->sinq_version) >= 0x3) ? ALL_MSPAGES : 0;
        l_mode_6 = false;       /* want MODE SENSE(10) if fetching all */
    }
try_again:
    res = ll_mode_sense(sg_fd, l_pn, l_spn, llbaa, oth_mp, req_len, &resid,
                        verb, op);
    if (0 == res) {
        resp_len = req_len - resid;
        if (resp_len < 4) {
            if (verb > 0)
                pr2serr("%s: resid=%d too large, implies truncated "
                        "response\n", __func__, resid);
        }
    } else {
        if (SG_LIB_CAT_ILLEGAL_REQ == res) {
            if (llbaa) {
                llbaa = false;
                goto try_again;
            } else if (ALL_MSPAGES == l_spn) {
                l_spn = 0;
                goto try_again;
            }
        }
        return verb ? report_error(res, l_mode_6) : res;
    }
    md_len = l_mode_6 ? (oth_mp[0] + 1) : (sg_get_unaligned_be16(oth_mp) + 2);
    bd_len = l_mode_6 ? oth_mp[3] : sg_get_unaligned_be16(oth_mp + 6);
    if (verb > 1)
        pr2serr("%s: md_len=%u, bd_len=%u\n", __func__, md_len, bd_len);

    if ((2 == dhex) || (dhex > 3))
        printf("# Mode parameter header(%d)%s, llbaa=%d:\n",
               (l_mode_6 ? 6 : 10),
               (bd_len ? " and block descriptor(s)" : ""), (int)llbaa);
    off = bd_len + (l_mode_6 ? 4 : 8);
    hex2stdout(oth_mp, off, no_ascii_4hex(op));

    /* Now get the mode pages, for each available page control */
    req_len = l_mode_6 ? DEF_MODE_6_RESP_LEN : MAX_MP_BUFF_SZ;

    pc_arr[0] = cur_aligned_mp;
    pc_arr[1] = cha_aligned_mp;
    pc_arr[2] = def_aligned_mp;
    pc_arr[3] = sav_aligned_mp;

one_more:
    res = sg_get_mode_page_controls(sg_fd, l_mode_6, l_pn, l_spn, op->dbd,
                                    op->flexible, req_len, &rmask, pc_arr,
                                    &resp_len, verb);
    if (res) {
        if (0 == rmask) {
            if ((pn < 0) && (ALL_MSPAGES == l_spn)) {
                l_spn = 0;      /* may not support subpages, try without */
                goto one_more;
            }
            return verb ? report_error(res, l_mode_6) : res;
        } else if (SG_LIB_CAT_ILLEGAL_REQ != res)
            return verb ? report_error(res, l_mode_6) : res;
    } else if (verb > 2)
        pr2serr("%s: get_mp_controls: [0x%x,0x%x] req_len=%d, resp_len=%d\n",
                __func__, l_pn, l_spn, req_len, resp_len);
    if (resp_len < 4) {
        if (verb > 0)
            pr2serr("%s: response length %d too small\n", __func__, resp_len);
        return SG_LIB_FILE_ERROR;
    }
    for (j1 = 0, msk = 1; j1 < 4; ++j1, msk <<= 1) {
        if (rmask & msk)
            break;
    }
    if (j1 >= 4)
        return SG_LIB_LOGIC_ERROR;
    pg1_p = (uint8_t *)pc_arr[j1];

    for (k = 0, break_after = false; k < resp_len; k += pg_len) {
        l_msk = msk;
        l_j1 = j1;
        pg_p = pg1_p + k;
        l_pn = pg_p[0] & 0x3f;
        pg_len = sdp_mpage_len(pg_p);
        spf = !! (pg_p[0] & 0x40);
        l_spn = spf ? pg_p[1] : 0;
        if (pn >= 0) {
            if (pn != l_pn)
                continue;
            if (spn != l_spn)
                continue;
            break_after = true;
        }
        /* put a newline before each new mode page */
        printf("\n");
        mnp = sdp_get_mp_nm(l_pn, l_spn, op->cl_pdt, -1, -1 /* vendor_id */);
        if (NULL == mnp) {      /* try hard to get name */
            mnp = sdp_get_mp_nm(l_pn, l_spn, op->cl_pdt,
                                (trans >= 0) ? trans : TPROTO_SAS, -1);
            if ((NULL == mnp) && (op->vendor_id >= 0))
                mnp = sdp_get_mp_nm(l_pn, l_spn, op->cl_pdt, -1,
                                    op->vendor_id);
        }
        if ((2 == dhex) || (dhex > 3)) {
            if (NULL == mnp) {
                if (spf)
                    printf("# %s [0x%x,0x%x]:\n", ump_s, l_pn, l_spn);
                else
                    printf("# %s [0x%x]:\n", ump_s, l_pn);
            } else {
                if (dhex > 2) {
                    if (spf)
                        printf("# %s %s [0x%x,0x%x]:\n", mnp->mp_name, mp_s,
                               l_pn, l_spn);
                    else
                        printf("# %s %s [0x%x]:\n", mnp->mp_name, mp_s, l_pn);
                } else
                    printf("# %s %s:\n", mnp->mp_name, mp_s);
            }
        }
        for ( ; l_j1 < 4; ++l_j1, l_msk <<= 1) {
            if ((l_msk & rmask) && (l_msk & op->out_mask)) {
                if ((2 == dhex) || (dhex > 3))
                    printf("#    %s:\n", pc_nm_arr[l_j1]);
                hex2stdout(pc_arr[l_j1] + k, pg_len, no_ascii_4hex(op));
            }
        }
        if (break_after)
            break;
    }
    return 0;
}

/* Print WP, CAPPID and DPOFUA bits for direct access devices (disks).
 * Return 0 for success, else error value. */
static int
print_mode_param_hdr(int sg_fd, int verb, const struct sdparm_opt_coll * op)
{
    int res, v, req_len, resp_len, resid;
    uint8_t * cur_mp = oth_aligned_mp;

    memset(cur_mp, 0, op->mode_6 ? DEF_MODE_6_RESP_LEN : DEF_MODE_RESP_LEN);
    req_len = 8 + (15 * 16);    /* allow for 15 'long' block descriptors */
    res = ll_mode_sense(sg_fd, ALL_MPAGES, 0, true, cur_mp, req_len, &resid,
                        verb, op);
    if (0 == res) {
        bool mode6 = op->mode_6;

        resp_len = req_len - resid;
        if (resp_len < 4) {
            if (verb > 0)
                pr2serr("%s: resid=%d too large, implies truncated "
                        "response\n", __func__, resid);
        } else {
            v = cur_mp[mode6 ? 2 : 3];
            printf("    Direct access device specific parameters: WP=%d  "
                   "CAPPID=%d  DPOFUA=%d\n", !!(v & 0x80), !!(v & 0x20),
                   !!(v & 0x10));
        }
    } else if (SG_LIB_CAT_ILLEGAL_REQ != res)
        return verb ? report_error(res, op->mode_6) : res;
    return 0;
}

static void
report_number_of_descriptors(uint8_t * cur_mp,
                             const struct sdparm_mode_descriptor_t * mdp,
                             const struct sdparm_opt_coll * op)
{
    int num = 0;
    uint64_t u;

    if ((mdp->num_descs_inc < 0) && (mdp->desc_len > 0)) {
        u = sg_get_big_endian(cur_mp + mdp->num_descs_off, 7,
                              mdp->num_descs_bytes * 8) /
            mdp->desc_len;
        num = (int)u;
    } else if (mdp->num_descs_bytes > 0) {
        u = sg_get_big_endian(cur_mp + mdp->num_descs_off, 7,
                              mdp->num_descs_bytes * 8) +
            mdp->num_descs_inc;
        num = (int)u;
    } else {    /* no descriptor count, so walk them */
        int pg_len = sdp_mpage_len(cur_mp);
        int d_len = mdp->desc_len_off + mdp->desc_len_bytes;
        int off = mdp->first_desc_off;
        const uint8_t * bp = cur_mp + mdp->desc_len_off;

        if (pg_len < 4)
            return;
        while (off + d_len < pg_len) {
            ++num;
            u = sg_get_big_endian(bp + off, 7, mdp->desc_len_bytes * 8);
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

static int
print_mode_page_hex(int sg_fd, int pn, int spn,
                    const struct sdparm_opt_coll * op)
{
    int res, len, resid, vb;
    uint8_t * mdpg = oth_aligned_mp;

    vb = (op->verbose > 2) ? op->verbose - 2 : 0;
    len = (op->mode_6) ? DEF_MODE_6_RESP_LEN : MAX_MODE_DATA_LEN;
    res = ll_mode_sense(sg_fd, pn, spn, true, mdpg, len, &resid, vb, op);
    if (res)
        return res;
    printf("%s [0x%x,0x%x] current values in hex:\n", ump_s, pn, spn);
    len -= resid;
    if (len > 0)
        hex2stdout(mdpg, len, no_ascii_4hex(op));
    return res;
}

/* Print one or more mode pages. Returns 0 if ok else IO error number.  */
static int
print_mode_pages(int sg_fd, int o_pn, int o_spn, int pdt,
                 struct sdparm_opt_coll * op, sgj_opaque_p jop)
{
    bool single_pg, fetch_pg, desc_part, warned, have_desc_id, sis;
    bool mode6 = op->mode_6;
    bool stop_if_set = false;
    int res, pg_len, verb, smask, rep_len, req_len, decay_pdt;
    int n, desc_id, desc_len, l_pn, l_spn;
    int desc0_off = 0;
    int skip_spn = INT_MIN;
    const struct sdparm_mp_item_t * mpip;
    const struct sdparm_mp_item_t * last_mpip;
    const struct sdparm_mp_item_t * fdesc_mpip = NULL;
    const struct sdparm_mp_name_t * mnp = NULL;
    const struct sdparm_mode_descriptor_t * mdp;
    void * pc_arr[4];
    uint8_t * cur_mp;
    uint8_t * cha_mp;
    uint8_t * def_mp;
    uint8_t * sav_mp;
    uint8_t * first_mp;
    sgj_state * jsp = &op->json_st;
    sgj_opaque_p jo2p = NULL;
    char b[144];
    char e[80];
    static const int blen = sizeof(b);
    static const int elen = sizeof(e);

    if (op->do_hex > 0) {
        return print_mps_in_hex(sg_fd, o_pn, o_spn, op);
    }
    cur_mp = (MP_OM_CUR & op->out_mask) ? cur_aligned_mp : NULL;
    cha_mp = (MP_OM_CHA & op->out_mask) ? cha_aligned_mp : NULL;
    def_mp = (MP_OM_DEF & op->out_mask) ? def_aligned_mp : NULL;
    sav_mp = (MP_OM_SAV & op->out_mask) ? sav_aligned_mp : NULL;
    if (cur_mp)
        first_mp = cur_mp;
    else if (cha_mp)
        first_mp = cha_mp;
    else if (def_mp)
        first_mp = def_mp;
    else if (sav_mp)
        first_mp = sav_mp;
    else
        first_mp = NULL;
    b[0] = '\0';
    res = 0;
    verb = (op->verbose > 0) ? op->verbose - 1 : 0;
    decay_pdt = sg_lib_pdt_decay(pdt);
    if (decay_pdt != pdt) {
        if ((pdt >= 0) && (op->verbose > 2))
            pr2serr("%s: decaying pdt=0x%x to 0x%x\n", __func__, pdt,
                    decay_pdt);
        pdt = decay_pdt;
    }
    if ((0 == pdt) && (op->do_long > 0) && (0 == op->do_quiet) &&
        (! op->examine)) {
        res = print_mode_param_hdr(sg_fd, verb, op);
        if (res)
            return res;
    }
    if (NULL == first_mp) {
        printf("No page control selected by --out_mask=OM\n");
        return 0;
    }
    l_pn = o_pn;
    l_spn = o_spn;
    /* choose a mode page item namespace (vendor, transport or generic) */
    if (op->vendor_id >= 0) {
        const struct sdparm_vendor_pair * svpp;

        svpp = sdp_get_vendor_pair(op->vendor_id);
        mpip = (svpp ? svpp->mitem : NULL);
    } else if ((op->transport >= 0) && (op->transport < 16))
        mpip = sdparm_transport_mp[op->transport].mitem;
    else        /* generic */
        mpip = sdparm_mitem_arr;
    if (NULL == mpip)
        return SG_LIB_CAT_OTHER;

    last_mpip = mpip;
    if (l_pn >= 0) {      /* step to first item of given page */
        single_pg = true;
        fetch_pg = true;
        for ( ; mpip->acron; ++mpip) {
            if ((l_pn == mpip->pg_num) && (l_spn == mpip->subpg_num)) {
                if (sg_pdt_s_eq(pdt, mpip->com_pdt) || op->flexible)
                    break;
            }
        }
        if (NULL == mpip->acron) {       /* page has no known fields */
            if (op->do_hex)
                mpip = last_mpip;    /* trick to enter main loop once */
            else if (op->examine || op->do_hex)
                return print_mode_page_hex(sg_fd, l_pn, l_spn, op);
            else {
                sdp_get_mp_nm_with_str(l_pn, l_spn, pdt, op->transport,
                                       op->vendor_id, 0, false, blen, b);
                if ((op->vendor_id < 0) && ((0 == l_pn) || (0x18 == l_pn) ||
                                            (0x19 == l_pn) || (l_pn >= 0x20)))
                    pr2serr("%s %s may be transport or vendor specific,\ntry "
                            "%s or %s. Otherwise add '-H' to\nsee page in "
                            "hex.\n", b, mp_s, tn_s, vn_s);
                else
                    pr2serr("%s %s, no fields found, add '-H' to see page "
                            "in hex.\n", b, mp_s);
            }
        }
    } else {    /* want all, so check all items in given namespace */
        single_pg = false;
        fetch_pg = false;
        mpip = last_mpip;
    }
    mdp = NULL;
    pg_len = 0;
    have_desc_id = false;
    desc_id = -1;
    desc_len = -1;
    req_len = mode6 ? DEF_MODE_6_RESP_LEN : MAX_MP_BUFF_SZ;

    /* starting at first match, loop over each mode page item in given
     * namespace */
    stop_if_set = false;
    for (smask = 0, warned = false ; mpip->acron; ++mpip, fetch_pg = false) {
        if (skip_spn == mpip->subpg_num)
            continue;
        else if (skip_spn >= 0)
            skip_spn = INT_MIN;
        if (! sg_pdt_s_eq(pdt, mpip->com_pdt) && ! op->flexible)
            continue;
        if (! (((o_pn >= 0) ? true : op->do_all) ||
               (MF_COMMON & mpip->flags)))
            continue;
        if (! fetch_pg) {
            if (! ((l_pn == mpip->pg_num) && (l_spn == mpip->subpg_num))) {
                /* page changed? some clean up to do */
                if (fdesc_mpip) {
                    last_mpip = mpip - 1;
                    if ((l_pn == last_mpip->pg_num) &&
                        (l_spn == last_mpip->subpg_num) &&
                        (single_pg || op->do_all))
                        print_mitem_desc_after1(pc_arr, pg_len, mnp,
                                        fdesc_mpip, last_mpip, smask,
                                        desc_len, stop_if_set, op, jo2p);
                }
                if (single_pg)
                    break;
                if (stop_if_set)
                    stop_if_set = false;
                if (! sg_pdt_s_eq(pdt, mpip->com_pdt))
                    continue;   /* ... but com_pdt didn't match */
                fetch_pg = true;
                l_pn = mpip->pg_num;
                l_spn = mpip->subpg_num;
            }
        }
        if (fetch_pg) {
            pg_len = 0;
            /* Only fetch mode page when needed (e.g. item mpage changed) */
            mnp = sdp_get_mp_nm_with_str(l_pn, l_spn, pdt, op->transport,
                                         op->vendor_id, op->do_long,
                                         (op->do_hex > 0), elen, e);
            smask = 0;
            warned = false;
            fdesc_mpip = NULL;
            pc_arr[0] = cur_mp;
            pc_arr[1] = cha_mp;
            pc_arr[2] = def_mp;
            pc_arr[3] = sav_mp;
            res = sg_get_mode_page_controls(
                        sg_fd, mode6, l_pn, l_spn, op->dbd, op->flexible,
                        req_len, &smask, pc_arr, &rep_len, verb);
            if (res && (SG_LIB_CAT_ILLEGAL_REQ != res))
                return verb ? report_error(res, mode6) : res;
            else if (verb > 2)
                pr2serr("%s: get_mp_controls: [0x%x,0x%x] res=%d, "
                        "req_len=%d, resp_len=%d\n", __func__, l_pn, l_spn,
                        res, req_len, rep_len);
            if ((l_spn > 0) && (l_spn < ALL_MSPAGES) && (1 & smask)) {
                /* asked for subpage and got back 'current' control. It
                 * should have the SPF bit set in the first mpage */
                if (! (0x40 & cur_mp[0])) {
                    if (verb > 0)
                        pr2serr("%s: asked to subpage [0x%x,0x%x] but SPF "
                                "bit not set in response, ignore\n",
                                __func__, l_pn, l_spn);
                    /* say nothing was reported */
                    ++non_spg_warning;
                    skip_spn = l_spn;
                    continue;
                }
            }

            /* check for mode page descriptors */
            mdp = (mnp && (! op->do_hex)) ? mnp->mp_desc : NULL;
            have_desc_id = mdp ? mdp->have_desc_id : false;
            desc0_off = mdp ? mdp->first_desc_off : 0;
            if (desc0_off > 1) {
                for (desc_part = false, fdesc_mpip = mpip;
                     fdesc_mpip && (l_pn == fdesc_mpip->pg_num) &&
                     (l_spn == fdesc_mpip->subpg_num); ++fdesc_mpip) {
                    if (fdesc_mpip->start_byte >= desc0_off) {
                        desc_part = true;
                        break;
                    }
                }
                if (! desc_part)
                    fdesc_mpip = NULL;
            }
            if (op->num_desc) {  /* report number of descriptors */
                if (mdp && fdesc_mpip && (op->out_mask & smask))
                    report_number_of_descriptors(first_mp, mdp, op);
                return 0;
            }
            if ((smask & MP_OM_CUR) || (! (MP_OM_CUR & op->out_mask))) {
                n = sg_scnpr(b, blen, "%s ", e);
                if (op->verbose) {
                    if (l_spn)
                        n += sg_scnpr(b + n, blen - n, "[0x%x,0x%x] ", l_pn,
                                      l_spn);
                    else
                        n += sg_scnpr(b + n, blen - n, "[0x%x] ", l_pn);
                }
                n += sg_scnpr(b + n, blen - n, "%s", mp_s);
                if ((op->do_long > 1) || op->verbose)
                    n += sg_scnpr(b + n, blen - n, " [PS=%d]",
                                  !!(first_mp[0] & 0x80));
                sgj_pr_hr(jsp, "%s:\n", b);
                if (op->do_json)
                    jo2p = sgj_named_subobject_r(jsp, jop,
                                         sdp_mp_convert2snake(e, b, blen));

                pg_len = sdp_mpage_len(first_mp);
                check_mode_page(first_mp, l_pn, pg_len, op);
            } else {    /* asked for current values and didn't get them */
                if (op->verbose || (single_pg && (! op->examine))) {
                    pr2serr(">> %s mode %spage ", e, (l_spn ? "sub" : ""));
                    if (op->verbose) {
                        if (l_spn)
                            pr2serr("[0x%x,0x%x] ", l_pn, l_spn);
                        else
                            pr2serr("[0x%x] ", l_pn);
                    }
                    if (SG_LIB_CAT_ILLEGAL_REQ == res)
                        pr2serr("not found\n");
                    else if (0 == res)
                        pr2serr("some problem\n");
                    else
                        pr2serr("failed\n");
                }
            }
        }       /* end of if(fetch_pg) block */
        if (smask) {
            if (mpip->start_byte >= pg_len) {
                if ((! op->flexible) && (0 == op->verbose))
                    continue;   // step over
                if (! warned) {
                    warned = true;
                    if (op->flexible)
                        pr2serr(" >> hereafter field position exceeds mode "
                                "page length=%d\n", pg_len);
                    else {
                        pr2serr(" >> skipping rest as field position exceeds "
                                "%s length=%d\n", mp_s, pg_len);
                        continue;
                    }
                }
                if (! op->flexible)
                    continue;
            }
            if (have_desc_id && (mpip->start_byte == desc0_off) &&
                (! (mpip->flags & MF_CLASH_OK))) {
                desc_id = 0xf & *(first_mp + mpip->start_byte);
                desc_len = sg_get_unaligned_be16(first_mp + desc0_off + 2) + 4;
            }
            if (have_desc_id && (desc_id >= 0) &&
                (mpip->flags & MF_CLASH_OK)) {
                if (desc_id != sdp_get_desc_id(mpip->flags))
                    continue;
            }
            sis = print_mitem_pc_arr("  ", smask, mpip, pc_arr, false,
                                     op, jo2p);
            if (sis)
                stop_if_set = true;
        }
    }   /* end of mode page item loop */
    if ((0 == res) && mpip && (NULL == mpip->acron) && fdesc_mpip) {
        --mpip;
        if ((l_pn == mpip->pg_num) && (l_spn == mpip->subpg_num))
            print_mitem_desc_after1(pc_arr, pg_len, mnp, fdesc_mpip, mpip,
                                    smask, desc_len, stop_if_set, op, jo2p);
    }
    return res;
}

/* Print one or more mode pages found in FN specified by the --inhex=FN
 * command line option. Returns 0 if ok else error number.
 * Note that MF_COMMON flag is ignored with --inhex=FN , in the absence
 * of other options, the first mode page in FN is decoded.  */
static int
print_inhex_mode_pages(uint8_t * msense_resp, struct sdparm_opt_coll * op,
                       sgj_opaque_p jop)
{
    bool first_time, desc_part, warned, nxt_spf, have_desc_id, sis;
    bool deja_vu;
    bool mode6 = op->mode_6;
    bool stop_if_set = false;
    int smask, resp_len, resp_mp_len, v, off, desc0_off, l_pn, l_spn;
    int pdt, decay_pdt, k, pg_len, transport, vendor_id, desc_id, desc_len;
    int n, inhx_mp_len;
    int inhx_len = op->inhex_len;
    int vb = op->verbose;
    const struct sdparm_mp_item_t * mpip;
    const struct sdparm_mp_item_t * last_mpip;
    const struct sdparm_mp_item_t * fdesc_mpip = NULL;
    const struct sdparm_mp_name_t * mnp = NULL;
    const struct sdparm_mode_descriptor_t * mdp;
    uint8_t * l_pg_p;
    uint8_t * nxt_pg_p;
    sgj_state * jsp = &op->json_st;
    sgj_opaque_p jo2p = NULL;
    void * pc_arr[MP_NUM_PG_CTL];
    char b[192];
    char c[96];
    static const int blen = sizeof(b);
    static const int clen = sizeof(c);

    pdt = op->cl_pdt;
    decay_pdt = sg_lib_pdt_decay(pdt);
    if (decay_pdt != pdt) {
        if (vb > 1)
            pr2serr("%s: decaying pdt=0x%x to 0x%x\n", __func__, pdt,
                    decay_pdt);
        pdt = decay_pdt;
    }
    resp_len = sg_msense_calc_length(msense_resp, inhx_len, mode6, NULL);
    off = sg_mode_page_offset(msense_resp, inhx_len, mode6, NULL, 0);
    if ((resp_len < 2) || (off < 2) || (off >= resp_len)) {
        pr2serr("Couldn't decode %s as a %s(%d) command response\n",
                (op->inhex_fn ? op->inhex_fn : "??"), ms_s, (mode6 ? 6 : 10));
        pr2serr("perhaps it is a VPD page, if so add '-i'\n");
        return SG_LIB_CAT_OTHER;
    }
    if (inhx_len < resp_len) {
        pr2serr("given %s(%d) response is too short, given %d bytes, expected "
                "%d\n", ms_s, (mode6 ? 6 : 10), inhx_len, resp_len);
        return SG_LIB_CAT_OTHER;
    }
    inhx_mp_len = inhx_len - off;
    resp_mp_len = resp_len - off;
    if (inhx_mp_len > resp_mp_len) {
        if (0 == (inhx_mp_len % resp_mp_len)) {
            n = inhx_mp_len / resp_mp_len;
            if (vb > 2)
                pr2serr("%s: have %d times as many mpages, assume page "
                        "controls\n", __func__, n);
            resp_len = inhx_len;
        } else {
            if (vb > 0)
                pr2serr("%s: strange, inhx_mp_len=%d, resp_mp_len=%d\n",
                        __func__, inhx_mp_len, resp_mp_len);
        }
    }
    if ((0 == pdt) && (op->do_long > 0) && (0 == op->do_quiet)) {
        v = msense_resp[mode6 ? 2 : 3];
        sgj_pr_hr(jsp, "    Direct access device specific parameters: "
                  "WP=%d  CAPPID=%d  DPOFUA=%d\n", !!(v & 0x80),
                  !!(v & 0x20), !!(v & 0x10));
    }
    if (0 == op->in_mask) {
        sgj_pr_hr(jsp, "in_mask is 0, so don't decode any pages\n");
        return 0;
    }

and_again:
    l_pg_p = msense_resp + off;
    l_pn = l_pg_p[0] & 0x3f;
    pg_len = sdp_mpage_len(l_pg_p);
    l_spn = (l_pg_p[0] & 0x40) ? l_pg_p[1] : 0;
    pc_arr[0] = l_pg_p;
    k = 0;
    pc_arr[1] = NULL;
    pc_arr[2] = NULL;
    pc_arr[3] = NULL;
    smask = 0x1;        /* treat as single "current" page */
    have_desc_id = false;
    desc_id = -1;
    desc_len = -1;
    while ((off + pg_len + 4) < resp_len) {
        nxt_pg_p = msense_resp + off + pg_len;
        nxt_spf = !! (nxt_pg_p[0] & 0x40);
        if ((l_pn != (nxt_pg_p[0] & 0x3f)) ||
            (l_spn != (nxt_spf ? nxt_pg_p[1] : 0)))
            break;
        pg_len = sdp_mpage_len(nxt_pg_p);
        off += pg_len;
        pc_arr[++k] = nxt_pg_p;
        smask |= (1 << k);
        l_pg_p = nxt_pg_p;
        if (k > 2)
            break;
    }
    if (op->page_str) {         /* looking for specific mpage */
        if ((l_pn != op->cl_pn) || (l_spn != op->cl_spn))
            goto fini;
    }
    if (op->in_mask < MP_IM_ALL) {
        int j, m, msk;
        void * t_pc_arr[MP_NUM_PG_CTL];

        smask = 0;
        for (j = 0; j < MP_NUM_PG_CTL; ++j) {
            t_pc_arr[j] = pc_arr[j];
            pc_arr[j] = 0;
        }
        for (j = 0, m = 0, msk = 1; j < MP_NUM_PG_CTL; ++j, msk <<= 1) {
            if (NULL == t_pc_arr[m])
                m++;
            else if (op->in_mask & msk) {
                pc_arr[j] = t_pc_arr[m++];
                smask |= msk;
            }
        }
        if (vb > 1)
            pr2serr("%s: local in_mask is 0x%x for pn,spg=0x%x,0x%x\n",
                    __func__, smask, l_pn, l_spn);
    }
    b[0] = '\0';
    c[0] = '\0';
#if 0
    orig_pn = l_pn;
#endif
    /* choose a mode page item namespace (vendor, transport or generic) */
    transport = op->transport;
    vendor_id = op->vendor_id;
    if (vendor_id >= 0) {
        const struct sdparm_vendor_pair * vpp;

        vpp = sdp_get_vendor_pair(vendor_id);
        mpip = (vpp ? vpp->mitem : NULL);
    } else if ((transport >= 0) && (transport < 16))
        mpip = sdparm_transport_mp[transport].mitem;
    else
        mpip = sdparm_mitem_arr;
    if (NULL == mpip)
        return SG_LIB_CAT_OTHER;
    deja_vu = false;

now_try_generic:
    // last_mpip = mpip;
    for ( ; mpip->acron; ++mpip) {
        if ((l_pn == mpip->pg_num) && (l_spn == mpip->subpg_num)) {
            if (sg_pdt_s_eq(pdt, mpip->com_pdt) || op->flexible)
                break;
        }
    }
    if (NULL == mpip->acron) {       /* page has no known fields */
        if (op->do_all && ((transport >= 0) || (vendor_id >= 0))) {
            deja_vu =  true;
            if (transport >= 0)
                transport = -1;
            if (vendor_id >= 0)
                vendor_id = -1;
            mpip = sdparm_mitem_arr;
            goto now_try_generic;
        }
        sdp_get_mp_nm_with_str(l_pn, l_spn, pdt, transport, vendor_id, 0,
                               false, clen, c);
        if (vendor_id < 0) {
            if ((0 == l_pn) || (l_pn >= 0x20))
                pr2serr("%s %s may be vendor specific, try '--vendor=VN'\n",
                        c, mp_s);
            else if (((0x18 == l_pn) || (0x19 == l_pn)) &&
                     (transport < 0) && (! deja_vu)) {
                transport = TPROTO_SAS;
                mpip = sdparm_mitem_sas_arr;
                goto now_try_generic;   /* ... try SAS */
            } else
                pr2serr("%s %s may be transport specific, try %s\n", c, mp_s,
                        tn_s);
        } else
            pr2serr("%s %s, no fields found. Add '-v' or '--v' to\nsee "
                    "more debug.\n", c, mp_s);
    }
    mdp = NULL;

    /* starting at first match, loop over each mode page item in given
     * namespace */
    for (warned = false, first_time = true ; mpip->acron; ++mpip) {
        if (first_time) {
            first_time = false;
            mnp = sdp_get_mp_nm_with_str(l_pn, l_spn, pdt, transport,
                                         vendor_id, op->do_long,
                                         (op->do_hex > 0), clen, c);
            warned = false;
            fdesc_mpip = NULL;
            /* check for mode page descriptors */
            mdp = (mnp && (! op->do_hex)) ? mnp->mp_desc : NULL;
            have_desc_id = mdp ? mdp->have_desc_id : false;
            desc0_off = mdp ? mdp->first_desc_off : 0;
            if (desc0_off > 1) {
                for (desc_part = false, fdesc_mpip = mpip;
                     fdesc_mpip && (l_pn == fdesc_mpip->pg_num) &&
                     (l_spn == fdesc_mpip->subpg_num); ++fdesc_mpip) {
                    if (fdesc_mpip->start_byte >= desc0_off) {
                        desc_part = true;
                        break;
                    }
                }
                if (! desc_part)
                    fdesc_mpip = NULL;
            }
            if (op->num_desc) {  /* report number of descriptors */
                if (mdp && fdesc_mpip && (1 & smask))
                    report_number_of_descriptors(l_pg_p, mdp, op);
                return 0;
            }
            n = sg_scnpr(b, blen, "%s ", c);
            if (vb) {
                if (l_spn)
                    n += sg_scnpr(b + n, blen - n, "[0x%x,0x%x] ", l_pn,
                                  l_spn);
                else
                    n += sg_scnpr(b + n, blen - n, "[0x%x] ", l_pn);
            }
            sg_scnpr(b + n, blen - n, "%s", mp_s);
            if ((op->do_long > 1) || vb)
                sgj_pr_hr(jsp, "%s [PS=%d]:\n", b, !!(l_pg_p[0] & 0x80));
            else
                sgj_pr_hr(jsp, "%s:\n", b);
            if (op->do_json)
                jo2p = sgj_named_subobject_r(jsp, jop,
                                        sdp_mp_convert2snake(c, b, blen));
            check_mode_page(l_pg_p, l_pn, pg_len, op);
        } else {        /* if not first_time */
            if ((l_pn == mpip->pg_num) && (l_spn == mpip->subpg_num)) {
                if (! sg_pdt_s_eq(pdt, mpip->com_pdt) && ! op->flexible)
                    continue;
#if 0
                if (! (((orig_pn >= 0) ? true : op->do_all) ||
                       (MF_COMMON & mpip->flags)))
                    continue;
#endif
            } else {    /* mode page or subpage changed */
                if (fdesc_mpip) {
                    last_mpip = mpip - 1; /* last_mpip that matched ... */
                    if ((l_pn == last_mpip->pg_num) &&
                        (l_spn == last_mpip->subpg_num))
                        print_mitem_desc_after1(pc_arr, pg_len, mnp,
                                        fdesc_mpip, last_mpip, smask,
                                        desc_len, stop_if_set, op, jo2p);
                }
                break;
            }
        }
        if (smask) {
            if (mpip->start_byte >= pg_len) {
                if ((! op->flexible) && (0 == vb))
                    continue;   // step over
                if (! warned) {
                    warned = true;
                    if (op->flexible)
                        pr2serr(" >> hereafter field position exceeds mode "
                                "page length=%d\n", pg_len);
                    else {
                        pr2serr(" >> skipping rest as field position exceeds "
                                "%s length=%d\n", mp_s, pg_len);
                        continue;
                    }
                }
                if (! op->flexible)
                    continue;
            }
            if (have_desc_id && (mpip->start_byte == desc0_off) &&
                (! (mpip->flags & MF_CLASH_OK))) {
                desc_id = 0xf & *(l_pg_p + mpip->start_byte);
                desc_len = sg_get_unaligned_be16(l_pg_p + desc0_off + 2) + 4;
            }
            if (have_desc_id && (desc_id >= 0) &&
                (mpip->flags & MF_CLASH_OK)) {
                if (desc_id != sdp_get_desc_id(mpip->flags))
                    continue;
            }
            sis = print_mitem_pc_arr("  ", smask, mpip, pc_arr, false, op,
                                     jo2p);
            if (sis)
                stop_if_set = true;
        }
    }   /* end of mode page item loop */
    if (mpip && (NULL == mpip->acron) && fdesc_mpip) {
        --mpip;
        if ((l_pn == mpip->pg_num) && (l_spn == mpip->subpg_num))
            print_mitem_desc_after1(pc_arr, pg_len, mnp, fdesc_mpip, mpip,
                                    smask, desc_len, stop_if_set, op, jo2p);
    }
fini:
    if (op->do_all || op->page_str) {
        off += pg_len;
        if ((off + 4) < resp_len) {
            if (op->page_str) {
                if ((l_pn == op->cl_pn) && (l_spn == op->cl_spn))
                    return 0;
            }
            goto and_again;
        }
    }
    return 0;
}

/* returns true when ok, else false */
static bool
check_desc_convert_mpip(int desc_num, const struct sdparm_mp_name_t * mnp,
                       const struct sdparm_mp_item_t * ref_mpip,
                       struct sdparm_mp_item_t * out_mpip,
                       int b_len, char * b)
{
    int n;

    if (mnp && mnp->mp_desc && ref_mpip->acron && b) {
        *out_mpip = *ref_mpip;
        sg_scnpr(b, b_len, "%s", ref_mpip->acron);
        b[(b_len > 10) ? (b_len - 8) : 4] = '\0';
        n = strlen(b);
        snprintf(b + n, b_len - n, ".%d", desc_num);
        out_mpip->acron = b;
        return true;
    } else
        return false;
}

/* returns true when ok, else false */
static bool
desc_adjust_start_byte(int desc_num, const struct sdparm_mp_name_t * mnp,
                       const uint8_t * cur_mp, int rep_len,
                       struct sdparm_mp_item_t * mpip,
                       const struct sdparm_opt_coll * op)
{
    const struct sdparm_mode_descriptor_t * mdp;
    uint64_t u;
    const uint8_t * bp;
    int d_off, sb_off, j;

    mdp = mnp->mp_desc;
    if ((mdp->num_descs_off < rep_len) && (mdp->num_descs_off < 64)) {
        if ((mdp->num_descs_inc < 0) && (mdp->desc_len > 0))
            u = sg_get_big_endian(cur_mp + mdp->num_descs_off, 7,
                    mdp->num_descs_bytes * 8) / mdp->desc_len;
        else
            u = sg_get_big_endian(cur_mp + mdp->num_descs_off, 7,
                    mdp->num_descs_bytes * 8) + mdp->num_descs_inc;
        if ((uint64_t)desc_num < u) {
            if (mdp->desc_len > 0) {
                mpip->start_byte += (mdp->desc_len * desc_num);
                if (mpip->start_byte < rep_len)
                    return true;
            } else if (mdp->desc_len_off > 0) {
                /* need to walk through variable length descriptors */

                sb_off = mpip->start_byte - mdp->first_desc_off;
                d_off = mdp->first_desc_off;
                for (j = 0; ; ++j) {
                    if (j > desc_num) {
                        pr2serr(">> descriptor number sanity ...\n");
                        break;  /* sanity */
                    }
                    if (j == desc_num) {
                        mpip->start_byte = d_off + sb_off;
                        if (mpip->start_byte < rep_len)
                            return true;
                        else
                            pr2serr(">> new start_byte exceeds current page "
                                    "...\n");
                        break;
                    }
                    bp = cur_mp + d_off + mdp->desc_len_off;
                    u = sg_get_big_endian(bp, 7, mdp->desc_len_bytes * 8);
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
            pr2serr("    >> %s says it has only %d descriptors\n", mp_s,
                    (int)u);
    }
    return false;
}

/* Print one or more mode page items (from '--get='). Returns 0 if ok. */
static int
print_mitems(int sg_fd, const struct sdparm_mp_settings_t * mps,
             int pdt, struct sdparm_opt_coll * op, sgj_opaque_p jop)
{
    bool adapt, warned;
    bool mode6 = op->mode_6;
    int k, n, verb, smask, pn, spn, rep_len, req_len, len, desc_num;
    int res = 0;
    uint64_t u;
    int64_t val;
    const struct sdparm_mp_item_t * mpip;
    struct sdparm_mp_item_t a_mp_it;
    const struct sdparm_mp_name_t * mnp = NULL;
    uint8_t * cur_mp;
    uint8_t * cha_mp;
    uint8_t * def_mp;
    uint8_t * sav_mp;
    uint8_t * free_cur_mp = NULL;
    uint8_t * free_cha_mp = NULL;
    uint8_t * free_def_mp = NULL;
    uint8_t * free_sav_mp = NULL;
    const struct sdparm_mp_it_val_t * ivp;
    sgj_state * jsp = &op->json_st;
    void * pc_arr[4];
    char b[144];
    char e[32];
    static const int blen = sizeof(b);
    static const int elen = sizeof(e);

    req_len = mode6 ? DEF_MODE_6_RESP_LEN : DEF_MODE_RESP_LEN;
    cur_mp = sg_memalign(req_len, 0, &free_cur_mp, false);
    cha_mp = sg_memalign(req_len, 0, &free_cha_mp, false);
    def_mp = sg_memalign(req_len, 0, &free_def_mp, false);
    sav_mp = sg_memalign(req_len, 0, &free_sav_mp, false);
    if ((NULL == cur_mp) || (NULL == cha_mp) || (NULL == def_mp) ||
        (NULL == sav_mp)) {
        pr2serr("%s: unable to allocate heap\n", __func__);
        res = sg_convert_errno(ENOMEM);
        goto out;
    }
    warned = false;
    verb = (op->verbose > 0) ? op->verbose - 1 : 0;
    for (k = 0, pn = 0, spn = 0; k < mps->num_it_vals; ++k) {
        ivp = &mps->it_vals[k];
        val = ivp->val;
        desc_num = ivp->descriptor_num;
        mpip = &ivp->mp_it;
        mnp = sdp_get_mp_nm_with_str(mpip->pg_num, mpip->subpg_num,
                                     mpip->com_pdt, op->transport,
                                     op->vendor_id, op->do_long,
                                     (op->do_hex > 0), blen, b);
        if (desc_num > 0) {
            if (check_desc_convert_mpip(desc_num, mnp, mpip, &a_mp_it,
                                        elen, e)) {
                adapt = true;
                mpip = &a_mp_it;
            } else {
                pr2serr("can't decode descriptors for %s in %s %s\n",
                        (mpip->acron ? mpip->acron : ""), b, mp_s);
                res = SG_LIB_SYNTAX_ERROR;
                goto out;
            }
        } else
            adapt = false;
        if ((0 == k) || (pn != mpip->pg_num) || (spn != mpip->subpg_num)) {
            pn = mpip->pg_num;
            spn = mpip->subpg_num;
            smask = 0;
            // res = 0;
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
                if (mpip->acron)
                    pr2serr("bad value given to %s\n", mpip->acron);
                else
                    pr2serr("bad value given to 0x%x:%d:%d\n",
                            mpip->start_byte, mpip->start_bit, mpip->num_bits);
                res = SG_LIB_SYNTAX_ERROR;
                goto out;
            }
            res = sg_get_mode_page_controls(sg_fd, mode6, pn, spn, op->dbd,
                                            op->flexible, req_len, &smask,
                                            pc_arr, &rep_len, verb);
            if (res && (SG_LIB_CAT_ILLEGAL_REQ != res)) {
                if (verb)
                    report_error(res, mode6);
                goto out;
            } else if (verb > 2)
                pr2serr("%s: get_mp_controls: [0x%x,0x%x] res=%d, "
                        "req_len=%d, resp_len=%d\n", __func__, pn, spn, res,
                        req_len, rep_len);
            if ((0 == smask) && res) {
                if (mpip->acron)
                    pr2serr("%s ", mpip->acron);
                else
                    pr2serr("0x%x:%d:%d ", mpip->start_byte, mpip->start_bit,
                            mpip->num_bits);
                if (SG_LIB_CAT_ILLEGAL_REQ == res)
                    pr2serr("not found in ");
                else
                    pr2serr("error %sin ",
                            (verb ? "" : "(try adding '-vv') "));
                pr2serr("%s %s\n", b, mp_s);
                goto out;
            }
            if (smask & 1)
                check_mode_page(cur_mp, pn, rep_len, op);
        }
        if (adapt) {
            if (! desc_adjust_start_byte(desc_num, mnp, cur_mp, rep_len,
                                         &a_mp_it, op)) {
                pr2serr(">> %s: %s in current page\n", cffa_s, mpip->acron);
                res = SG_LIB_CAT_OTHER;
                goto out;
            }
        }
        if (! warned && mpip->acron && ! sg_pdt_s_eq(pdt, mpip->com_pdt)) {
            warned = true;
            pr2serr(">> warning: peripheral device type (pdt) is 0x%x but "
                    "acronym %s\n   is associated with pdt 0x%x.\n", pdt,
                    ivp->mp_it.acron, ivp->mp_it.com_pdt);
        }
        len = (smask & 1) ? sdp_mpage_len(cur_mp) : 0;
        if (mpip->start_byte >= len) {
            pr2serr(">> warning: ");
            if (mpip->acron)
                pr2serr("%s ", mpip->acron);
            else
                pr2serr("0x%x:%d:%d ", mpip->start_byte, mpip->start_bit,
                        mpip->num_bits);
            pr2serr("field position exceeds %s length=%d\n", mp_s, len);
            if (! op->flexible)
                continue;
        }
        if (MP_GET_VAL_ALL == val) { /* current, changed, default and saved */
            /* this case: --get=<acron>[=0] */
            if (op->inner_hex) {
                if (smask & MP_OM_CUR) {
                    u = sdp_mitem_get_value(mpip, cur_mp);
                    n = sg_scnpr(b, blen, "0x%02" PRIx64 " ", u);
                } else
                    n = sg_scnpr(b, blen, "-    ");
                if (smask & MP_OM_CHA) {
                    u = sdp_mitem_get_value(mpip, cha_mp);
                    n += sg_scnpr(b + n, blen - n, "0x%02" PRIx64 " ", u);
                } else
                    n += sg_scnpr(b + n, blen - n, "-    ");
                if (smask & MP_OM_DEF) {
                    u = sdp_mitem_get_value(mpip, def_mp);
                    n += sg_scnpr(b + n, blen - n, "0x%02" PRIx64 " ", u);
                } else
                    n += sg_scnpr(b + n, blen - n, "-    ");
                if (smask & MP_OM_SAV) {
                    u = sdp_mitem_get_value(mpip, sav_mp);
                    n += sg_scnpr(b + n, blen - n, "0x%02" PRIx64 " ", u);
                } else
                    n += sg_scnpr(b + n, blen - n, "-    ");
                sgj_pr_hr(jsp, "%s\n", b);
            } else      /* all page ctls with unsigned decimal */
                print_mitem_pc_arr("", smask, mpip, pc_arr, false, op, jop);
        } else if (MP_GET_VAL_CURR == val) {  /* just current */
            /* this case: --get=<acron>=1 */
            if (op->inner_hex) {
                if (smask & MP_OM_CUR) {
                    u = sdp_mitem_get_value(mpip, cur_mp);
                    sg_scnpr(b, blen, "0x%02" PRIx64 " ", u);
                } else
                    sg_scnpr(b, blen, "-    ");
                sgj_pr_hr(jsp, "%s\n", b);
            } else      /* just current with unsigned value */
                print_mitem_pc_arr("", smask & 1, mpip, pc_arr, false, op,
                                   jop);
        } else if (MP_GET_VAL_SIGNED == val) {  /* just current */
            /* this case: --get=<acron>=2 */
            if (op->inner_hex) {
                if (smask & MP_OM_CUR) {
                    u = sdp_mitem_get_value(mpip, cur_mp);
                    sg_scnpr(b, blen, "0x%02" PRIx64 " ", u);
                } else
                    sg_scnpr(b, blen, "-    ");
                sgj_pr_hr(jsp, "%s\n", b);
            } else      /* just current with signed value */
                print_mitem_pc_arr("", smask & 1, mpip, pc_arr, true, op, jop);
        }
    }
out:
    if (free_cur_mp)
        free(free_cur_mp);
    if (free_cha_mp)
        free(free_cha_mp);
    if (free_def_mp)
        free(free_def_mp);
    if (free_sav_mp)
        free(free_sav_mp);
    return res;
}

/* Return of 0 -> success, most errors indicated by various SG_LIB_CAT_*
 * positive values, -1 -> other failures */
static int
change_mode_page(int sg_fd, int pdt,
                 const struct sdparm_mp_settings_t * mps,
                 const struct sdparm_opt_coll * op)
{
    bool mode6 = op->mode_6;
    int k, off, md_len, len, res, desc_num, pn, spn;
    int resid = 0;
    int vb = op->verbose;
    const struct sdparm_mp_name_t * mnp = NULL;
    const struct sdparm_mp_it_val_t * ivp;
    const struct sdparm_mp_item_t * mpip;
    const char * cdbLenS = mode6 ? "6": "10";
    char b[128];
    char b_tmp[32];
    char ebuff[EBUFF_SZ];
    uint8_t * mdpg = oth_aligned_mp;
    struct sdparm_mp_item_t a_mp_it;

    if (pdt >= 0) {
        /* sanity check: check acronym's pdt matches device's pdt */
        for (k = 0; k < mps->num_it_vals; ++k) {
            ivp = &mps->it_vals[k];
            if (ivp->mp_it.acron && ! sg_pdt_s_eq(pdt, ivp->mp_it.com_pdt)) {
                pr2serr("%s: peripheral device type (pdt) is 0x%x but "
                        "acronym %s\n  is associated with pdt 0x%x. To "
                        "bypass use numeric addressing mode.\n", __func__,
                         pdt, ivp->mp_it.acron, ivp->mp_it.com_pdt);
                return SG_LIB_SYNTAX_ERROR;
            }
        }
    }
    pn = mps->pg_num;
    spn = mps->subpg_num;
    mnp = sdp_get_mp_nm_with_str(pn, spn, pdt, op->transport, op->vendor_id,
                                 0, false, sizeof(b), b);
    memset(mdpg, 0, MAX_MODE_DATA_LEN);
    res = ll_mode_sense(sg_fd, pn, spn, false, mdpg, 4, &resid, vb, op);
    if (0 != res) {
        if (SG_LIB_CAT_INVALID_OP == res) {
            pr2serr("%s byte %s cdb not supported, try again with%s '-6' "
                    "option\n", cdbLenS, ms_s, mode6 ? "out" : "");
        } else {
            char bb[80];

            sg_get_category_sense_str(res, sizeof(bb), bb, vb);
            pr2serr("%s command: %s\n", ms_s, bb);
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
    if (md_len > MAX_MODE_DATA_LEN) {
        pr2serr("%s: mode data length=%d exceeds allocation length=%d\n",
                __func__, md_len, MAX_MODE_DATA_LEN);
        return SG_LIB_CAT_MALFORMED;
    }
    res = ll_mode_sense(sg_fd, pn, spn, false, mdpg, md_len, &resid, vb, op);
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
        mpip = &ivp->mp_it;
        desc_num = ivp->descriptor_num;
        if (desc_num > 0) {
            if (check_desc_convert_mpip(desc_num, mnp, mpip, &a_mp_it,
                                       sizeof(b_tmp), b_tmp)) {
                mpip = &a_mp_it;
                if (! desc_adjust_start_byte(desc_num, mnp, mdpg + off, len,
                                             &a_mp_it, op)) {
                    pr2serr(">> %s: %s in current page\n", cffa_s,
                            mpip->acron);
                    return SG_LIB_CAT_OTHER;
                }
            } else {
               pr2serr("can't decode descriptors for %s in %s mode "
                       "page\n", (mpip->acron ? mpip->acron : ""), b);
               return SG_LIB_SYNTAX_ERROR;
            }
        }
        if (mpip->start_byte >= len) {
            /* mpip->start_byte is too large for actual mpage length */
            pr2serr("The start_byte of ");
            if (mpip->acron)
                pr2serr("%s ", mpip->acron);
            else
                pr2serr("0x%x:%d:%d ", mpip->start_byte, mpip->start_bit,
                        mpip->num_bits);
            pr2serr("exceeds length of this %s: %d [0x%x]\n", mp_s, len, len);
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
                    ivp->val, mpip->num_bits);
            pr2serr("    applying anyway\n");
        }
        sdp_mitem_set_value(ivp->val, mpip, mdpg + off);
    }

    if ((! (mdpg[off] & 0x80)) && op->save) {
        pr2serr("%s: %s indicates it is not saveable but\n    '--save' "
                "option given (try without it)\n", __func__, mp_s);
        return SG_LIB_CAT_MALFORMED;
    }
    mdpg[off] &= 0x7f;   /* mask out PS bit, reserved in mode select */
    if (op->dummy) {
        pr2serr("Mode data that would have been written:\n");
        hex2stderr(mdpg, md_len, 1);
        return 0;
    }
    if (mode6)
        res = sg_ll_mode_select6(sg_fd, true /* PF */, op->save, mdpg, md_len,
                                 true, vb);
    else
        res = sg_ll_mode_select10_v2(sg_fd, true /* PF */, false /* RTD */,
                                     op->save, mdpg, md_len, true, vb);
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
                  const uint8_t * mode_pg, int mode_pg_len,
                  const struct sdparm_opt_coll * op)
{
    bool mode6 = op->mode_6;
    int len, off, md_len;
    int resid = 0;
    int ret = -1;
    uint8_t * mdp;
    char ebuff[EBUFF_SZ];
    char b[128];

    len = mode_pg_len + MODE_DATA_OVERHEAD;
    mdp = (uint8_t *)malloc(len);
    if (NULL ==mdp) {
        pr2serr("%s: malloc failed, out of memory\n", __func__);
        return sg_convert_errno(ENOMEM);
    }
    memset(mdp, 0, len);
    ret = ll_mode_sense(sg_fd, pn, spn, true, mdp, 4, &resid, op->verbose, op);
    if (0 != ret) {
        sdp_get_mp_nm_with_str(pn, spn, -1, op->transport, op->vendor_id, 0,
                               false, sizeof(b), b);
        pr2serr("%s: failed fetching page: %s\n", __func__, b);
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
    ret = ll_mode_sense(sg_fd, pn, spn, true, mdp, md_len, &resid,
                        op->verbose, op);
    if (0 != ret) {
        sdp_get_mp_nm_with_str(pn, spn, -1, op->transport, op->vendor_id,
                               0, false, sizeof(b), b);
        pr2serr("%s: failed fetching page: %s\n", __func__, b);
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
        hex2stderr(mdp, md_len, 1);
        ret = 0;
        goto err_out;
    }
    if (mode6)
        ret = sg_ll_mode_select6(sg_fd, true /* PF */, op->save, mdp, md_len,
                                 true, op->verbose);
    else
        ret = sg_ll_mode_select10_v2(sg_fd, true, false /* RTD */, op->save,
                                     mdp, md_len, true, op->verbose);
    if (0 != ret) {
        sdp_get_mp_nm_with_str(pn, spn, -1, op->transport, op->vendor_id,
                               0, false, sizeof(b), b);
        pr2serr("%s: failed setting page: %s\n", __func__, b);
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
    int smask, req_len, len, rep_len;
    int res = 0;
    const char * cdbLenS = mode6 ? "6" : "10";
    uint8_t * cur_mp;
    uint8_t * def_mp;
    uint8_t * free_cur_mp = NULL;
    uint8_t * free_def_mp = NULL;
    char b[128];
    void * pc_arr[4];

    req_len = mode6 ? DEF_MODE_6_RESP_LEN : DEF_MODE_RESP_LEN;
    cur_mp = sg_memalign(req_len, 0, &free_cur_mp, false);
    def_mp = sg_memalign(req_len, 0, &free_def_mp, false);
    if ((NULL == cur_mp) || (NULL == def_mp)) {
        pr2serr("%s: unable to allocate heap\n", __func__);
        res = sg_convert_errno(ENOMEM);
        goto out;
    }
    smask = 0;
    pc_arr[0] = cur_mp;
    pc_arr[1] = NULL;
    pc_arr[2] = def_mp;
    pc_arr[3] = NULL;
    res = sg_get_mode_page_controls(sg_fd, op->mode_6, pn, spn, op->dbd,
                                    op->flexible, req_len, &smask, pc_arr,
                                    &rep_len, op->verbose);
    if (res) {
        if (SG_LIB_CAT_INVALID_OP == res)
            pr2serr("%s byte %s cdb not supported, try again with%s '-6' "
                    "option\n", cdbLenS, ms_s, mode6 ? "out" : "");
        else if (SG_LIB_CAT_NOT_READY == res)
            pr2serr("%s(%s) failed, device not ready\n", ms_s, cdbLenS);
        else if (SG_LIB_CAT_ABORTED_COMMAND == res)
            pr2serr("%s(%s) failed, aborted command\n", ms_s, cdbLenS);
        goto out;
    } else if (op->verbose > 2)
        pr2serr("%s: get_mp_controls: [0x%x,0x%x] req_len=%d, resp_len=%d\n",
                __func__, pn, spn, req_len, rep_len);
    if (op->verbose && (! op->flexible) && (rep_len > 0xa00)) {
        sdp_get_mp_nm_with_str(pn, spn, pdt, op->transport, op->vendor_id,
                               0, false, sizeof(b), b);
        pr2serr("%s %s length=%d too long, perhaps try '--flexible'\n",
                b, mp_s, rep_len);
    }
    if ((smask & MP_OM_CUR)) {
        len = sdp_mpage_len(cur_mp);
        if ((smask & MP_OM_DEF)) {
            res = set_def_mode_page(sg_fd, pn, spn, def_mp, len, op);
            goto out;
        } else {
            sdp_get_mp_nm_with_str(pn, spn, pdt, op->transport, op->vendor_id,
                                   0, false, sizeof(b), b);
            pr2serr(">> %s %s (default) not supported\n", b, mp_s);
            res = SG_LIB_CAT_ILLEGAL_REQ;
            goto out;
        }
    } else {
        sdp_get_mp_nm_with_str(pn, spn, pdt, op->transport, op->vendor_id,
                               0, false, sizeof(b), b);
        pr2serr(">> %s %s not supported\n", b, mp_s);
        res = SG_LIB_CAT_ILLEGAL_REQ;
        goto out;
    }
out:
    if (free_cur_mp)
        free(free_cur_mp);
    if (free_def_mp)
        free(free_def_mp);
    return res;
}

static int
set_all_page_defaults(int sg_fd, const struct sdparm_opt_coll * op)
{
    int ret;

    if (op->mode_6)
        ret = sg_ll_mode_select6_v2(sg_fd, false /* PF */, true /* RTD */,
                                    op->save, NULL, 0, true, op->verbose);
    else
        ret = sg_ll_mode_select10_v2(sg_fd, false /* PF */, true /* RTD */,
                                     op->save, NULL, 0, true, op->verbose);
    if (0 != ret)
        pr2serr("%s: setting RTD (reset to defaults) bit failed\n",
                __func__);
    return ret;
}

/* Parse 'arg' given to command line --get=, --clear= or set= into 'mps'
 * which contains an array ('mps->it_vals[]') used for output. 'arg' may be
 * a comma separated list of item value=pairs. This function places
 * 'mps->num_it_vals' elements in that array. If 'clear','get' are
 * false,false then assume "--set="; true,true is invalid. Returns true if
 * successful, else false. */
static bool
build_mp_settings(const char * arg, struct sdparm_mp_settings_t * mps,
                  struct sdparm_opt_coll * op, bool clear, bool get)
{
    bool cont;
    int len, num, from, colon;
    unsigned int u;
    int64_t ll;
    char b[64];
    char acron[64];
    char vb[64];
    const char * cp;
    const char * ncp;
    const char * ecp;
    struct sdparm_mp_it_val_t * ivp;
    const struct sdparm_mp_item_t * mpip;
    const struct sdparm_mp_item_t * prev_mpip;
    static const int blen = sizeof(b);

    cp = arg;
    while (mps->num_it_vals < MAX_MP_IT_VAL) {
        memset(b, 0, sizeof(b));
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
            strncpy(b, cp, (len < (blen - 1) ? len : (blen - 1)));
        } else
            strncpy(b, cp, (blen - 1));
        colon = strchr(b, ':') ? 1 : 0;
        if ((isalpha(b[0]) && (! colon)) ||
            (isdigit(b[0]) && ('_' == b[1]))) { /* expect acronym */
            ecp = strchr(b, '=');
            if (ecp) {
                strncpy(acron, b, ecp - b);
                acron[ecp - b] = '\0';
                strcpy(vb, ecp + 1);
                if (0 == strcmp("-1", vb))
                    ivp->val = -1;
                else {
                    ivp->val = sg_get_llnum_nomult(vb);
                    if (-1 == ivp->val) {
                        pr2serr("unable to decode: %s value\n", b);
                        pr2serr("    expected: <acronym>[=<val>]\n");
                        goto err;
                    }
                }
            } else {
                strcpy(acron, b);
                ivp->val = ((clear || get) ? 0 : -1);
            }
            if ((ecp = strchr(acron, '.'))) {
                strcpy(vb, acron);
                strncpy(acron, vb, ecp - acron);
                acron[ecp - acron] = '\0';
                strcpy(vb, ecp + 1);
                ivp->descriptor_num = sg_get_llnum_nomult(vb);
                if (ivp->descriptor_num < 0) {
                    pr2serr("unable to decode: %s descriptor number\n", b);
                    pr2serr("    expected: <acronym_name>"
                            "[.<desc_num>][=<val>]\n");
                    goto err;
                }
            }
            ivp->orig_val = ivp->val;
            from = 0;
            cont = false;
            prev_mpip = NULL;
            if (get) {
                do {
                    mpip = sdp_find_mitem_by_acron(acron, &from, op->transport,
                                         op->vendor_id);
                    if (NULL == mpip) {
                        if (cont) {
                            mpip = prev_mpip;
                            break;
                        }
                        if ((op->vendor_id < 0) && (op->transport < 0)) {
                            from = 0;
                            mpip = sdp_find_mitem_by_acron(acron, &from,
                                          DEF_TRANSPORT_PROTOCOL, -1);
                            if (NULL == mpip) {
                                pr2serr("%s: %s\n", cffa_s, acron);
                                pr2serr("    [perhaps a %s or %s option is "
                                        "needed]\n", tn_s, vn_s);
                                goto err;
                            } else /* keep going in this case */
                                op->transport = DEF_TRANSPORT_PROTOCOL;
                        } else {
                            pr2serr("%s: %s\n", cffa_s, acron);
                            goto err;
                        }
                    }
                    if (mps->pg_num < 0) {
                        mps->pg_num = mpip->pg_num;
                        mps->subpg_num = mpip->subpg_num;
                        break;
                    }
                    cont = true;
                    prev_mpip = mpip;
                    /* got acronym match but if not at specified pn,spn */
                    /* then keep searching */
                } while ((mps->pg_num != mpip->pg_num) ||
                         (mps->subpg_num != mpip->subpg_num));
            } else {    /* --set or --clear */
                do {
                    mpip = sdp_find_mitem_by_acron(acron, &from,
                                 op->transport, op->vendor_id);
                    if (NULL == mpip) {
                        if (cont) {
                            pr2serr("%s of acronym: %s [0x%x,0x%x] doesn't "
                                    "match prior\n", mp_s, acron,
                                    prev_mpip->pg_num,
                                    prev_mpip->subpg_num);
                            pr2serr("    %s: 0x%x,0x%x\n", mp_s,
                                    mps->pg_num, mps->subpg_num);
                            pr2serr("For '--set' and '--clear' all fields "
                                    "must be in the same %s\n", mp_s);
                            goto err;
                        }
                        if ((op->vendor_id < 0) && (op->transport < 0)) {
                            from = 0;
                            mpip = sdp_find_mitem_by_acron(acron, &from,
                                          DEF_TRANSPORT_PROTOCOL, -1);
                            if (NULL == mpip) {
                                pr2serr("%s: %s\n", cffa_s, acron);
                                pr2serr("    [perhaps a %s or %s option is "
                                        "needed]\n", tn_s, vn_s);
                                goto err;
                            } else /* keep going in this case */
                                op->transport = DEF_TRANSPORT_PROTOCOL;
                        } else {
                            pr2serr("%s: %s\n", cffa_s, acron);
                            goto err;
                        }
                    }
                    if (mps->pg_num < 0) {
                        mps->pg_num = mpip->pg_num;
                        mps->subpg_num = mpip->subpg_num;
                        break;
                    }
                    cont = true;
                    prev_mpip = mpip;
                    /* got acronym match but if not at specified pn,spn */
                    /* then keep searching */
                } while ((mps->pg_num != mpip->pg_num) ||
                         (mps->subpg_num != mpip->subpg_num));
            }
            if (mpip->num_bits < 64) {
                ll = 1;
                ivp->val &= (ll << mpip->num_bits) - 1;
            }
            ivp->mp_it = *mpip;    /* struct assignment */
        } else {    /* expect "start_byte:start_bit:num_bits[=<val>]" */
            /* start_byte may be in hex ('0x' prefix or 'h' suffix) */
            if ((0 == strncmp("0x", b, 2)) ||
                (0 == strncmp("0X", b, 2))) {
                num = sscanf(b + 2, "%x:%d:%d=%62s", &u,
                             &ivp->mp_it.start_bit, &ivp->mp_it.num_bits, vb);
                ivp->mp_it.start_byte = u;
            } else {
                if (strstr(b, "h:")) {
                    num = sscanf(b, "%xh:%d:%d=%62s", &u,
                                 &ivp->mp_it.start_bit, &ivp->mp_it.num_bits,
                                 vb);
                    ivp->mp_it.start_byte = u;
                } else if (strstr(b, "H:")) {
                    num = sscanf(b, "%xH:%d:%d=%62s", &u,
                                 &ivp->mp_it.start_bit, &ivp->mp_it.num_bits,
                                 vb);
                    ivp->mp_it.start_byte = u;
                } else
                    num = sscanf(b, "%d:%d:%d=%62s", &ivp->mp_it.start_byte,
                                 &ivp->mp_it.start_bit, &ivp->mp_it.num_bits,
                                 vb);
            }
            if (num < 3) {
                pr2serr("unable to decode: %s\n", b);
                pr2serr("    expected: start_byte:start_bit:num_bits[=<val>]"
                        "\n");
                goto err;
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
                        goto err;
                    }
                }
            }
            ivp->mp_it.com_pdt = -1;    /* pdt unknown, so don't restrict */
            if (ivp->mp_it.start_byte < 0) {
                pr2serr("need positive start byte offset\n");
                goto err;
            }
            if ((ivp->mp_it.start_bit < 0) || (ivp->mp_it.start_bit > 7)) {
                pr2serr("need start bit in 0..7 range (inclusive)\n");
                goto err;
            }
            if ((ivp->mp_it.num_bits < 1) || (ivp->mp_it.num_bits > 64)) {
                pr2serr("need number of bits in 1..64 range (inclusive)\n");
                goto err;
            }
            if (mps->pg_num < 0) {
                pr2serr("need '--page=' option for %s name or number\n",
                        mp_s);
                goto err;
            } else if (get) {
                ivp->mp_it.pg_num = mps->pg_num;
                ivp->mp_it.subpg_num = mps->subpg_num;
            }
            ivp->orig_val = ivp->val;
            if (ivp->mp_it.num_bits < 64) {
                int64_t ll1 = 1;

                ivp->val &= (ll1 << ivp->mp_it.num_bits) - 1;
            }
        }
        ++mps->num_it_vals;
        if (ncp)
            cp = ncp + 1;
        else
            break;
    }           /* end of big while() loop */
    if (mps->num_it_vals >= MAX_MP_IT_VAL)
        pr2serr("%s: maximum number (%d) of command line itm/value pairs "
                "reached,\nignore rest\n", __func__, MAX_MP_IT_VAL);
    return true;
err:
    mps->num_it_vals = 0;
    return false;
}

/* Returns open file descriptor ( >= 0) or negated sg3_utils error code. */
static int
open_and_simple_inquiry(const char * device_name, bool rw, int * pdt,
                        bool * protect, struct sdparm_opt_coll * op)
{
    int n, res, verb, sg_fd, l_pdt;
    sgj_state * jsp = &op->json_st;
    struct sg_simple_inquiry_resp sir;
    char b[168];
    char c[32];
    static const int blen = sizeof(b);
    static const int clen = sizeof(c);

    verb = (op->verbose > 0) ? op->verbose - 1 : 0;
    sg_fd = sg_cmds_open_device(device_name, ! rw /* read_only */ , verb);
    if (sg_fd < 0) {
        pr2serr("open error: %s [%s]: %s\n", device_name,
                (rw ? "read/write" : "read only"), safe_strerror(-sg_fd));
        return -sg_convert_errno(-sg_fd);
    }
    res = sg_simple_inquiry(sg_fd, &sir, 0, verb);
    if (res) {
#ifdef SG_LIB_LINUX
        if (-1 == res) {
            int sg_sg_fd;

            sg_sg_fd = map_if_lk24(sg_fd, device_name, rw, op->verbose);
            if (sg_sg_fd < 0) {
                res = -SG_LIB_SYNTAX_ERROR;
                goto err_out;
            }
            sg_cmds_close_device(sg_fd);
            sg_fd = sg_sg_fd;
            res = sg_simple_inquiry(sg_fd, &sir, 0, verb);
            if (res < 0)
                goto err_out;
        }
#endif  /* SG_LIB_LINUX */
        if (res) {
            pr2serr("SCSI INQUIRY command failed on %s\n", device_name);
            if (res > 0)
                res = -res;
            goto err_out;
        }
    }
    l_pdt = sir.peripheral_type;
    op->sinq_version = sir.version;
    if ((PDT_WO == l_pdt) || (PDT_OPTICAL == l_pdt))
        *pdt = PDT_DISK;       /* map disk-like pdt's to PDT_DISK */
    else
        *pdt = l_pdt;
    if (protect)
        *protect = !! (sir.byte_5 & 0x1);  /* PROTECT bit SPC-3 and later */
    if ((0 == op->do_hex) && (0 == op->do_quiet)) {
        n = sg_scnpr(b, blen, "    %s: %.8s  %.16s  %.4s",
               device_name, sir.vendor, sir.product, sir.revision);
        if (0 != l_pdt)
            sg_scnpr(b + n, blen - n, "  [%s]",
                     sg_get_pdt_str(l_pdt, clen, c));
        sgj_pr_hr(jsp, "%s\n", b);
    }
    return sg_fd;

err_out:
    sg_cmds_close_device(sg_fd);
    return res;
}

/* Process mode page(s) operation. Returns 0 if successful */
static int
process_mode_page(int sg_fd, const struct sdparm_mp_settings_t * mps,
                  int pn, int spn, int pdt, struct sdparm_opt_coll * op,
                  sgj_opaque_p jop)
{
    bool set_clear = op->set_clear;
    bool get = !! op->get_str;
    int res;
    const struct sdparm_mp_name_t * mnp;

    if ((pn > 0x3e) || (spn > 0xfe)) {
        if ((0x3f == pn) || (0xff == spn))
            pr2serr("Does not support requesting all %ss or subpages this "
                    "way.\n  Try '--all' option.\n", mp_s);
        else
            pr2serr("Accepts %s numbers from 0 to 62 .\n"
                    "  Accepts mode subpage numbers from 0 to 254 .\n"
                    "  For VPD pages add a '--inquiry' option.\n", mp_s);
        return SG_LIB_SYNTAX_ERROR;
    }
    if ((pn > 0) && (pdt >= 0)) {
        mnp = sdp_get_mp_nm(pn, spn, pdt, op->transport, op->vendor_id);
        if (NULL == mnp)
            mnp = sdp_get_mp_nm(pn, spn, -1, op->transport, op->vendor_id);
        if (mnp && mnp->mp_name && ! sg_pdt_s_eq(pdt, mnp->com_pdt) &&
            ! sg_pdt_s_eq(sg_lib_pdt_decay(pdt), mnp->com_pdt)) {
            pr2serr(">> Warning: %s %s associated with\n", mnp->mp_name,
                    mp_s);
            pr2serr("   peripheral device type 0x%x but device pdt is 0x%x\n",
                    mnp->com_pdt, pdt);
            if (! op->flexible)
                pr2serr("   may need '--flexible' option to override\n");
        }
    }
    if (1 == op->defaults)
        res = set_mp_defaults(sg_fd, pn, spn, pdt, op);
    else if (op->defaults > 1)
        res = set_all_page_defaults(sg_fd, op);
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
        res = print_mitems(sg_fd, mps, pdt, op, jop);
    } else
        res = print_mode_pages(sg_fd, pn, spn, pdt, op, jop);
    return res;
}

static int
enumerate_mp_contents(int pn, int spn, int com_pdt,
                      struct sdparm_opt_coll * op, sgj_opaque_p jop)
{
    int k;
    const char * ccp;
    sgj_state * jsp = &op->json_st;
    char b[256];
    static const int blen = sizeof(b);
    static const char * tp_s = "Transport protocol";
    static const char * weig_s = "when '--enumerate' is given";

    if (op->num_devices > 0)
        /* think about --get= with --enumerate */
        pr2serr("<scsi_device> as well as most options are ignored %s\n",
                weig_s);
    if (op->set_clear || op->save) {
        pr2serr("'--clear=', '--save', and '--set=' cannot be used %s\n",
                weig_s);
        return SG_LIB_CONTRADICT;
    }
    if (op->get_str)
        pr2serr("'--get=' is currently ignored %s\n", weig_s);
    if (pn < 0) {
        if (op->vendor_id >= 0) {
            ccp = sdp_get_vendor_name(op->vendor_id);
            if (ccp)
                sgj_pr_hr(jsp, "%s %s vendor:\n", mpgf_s, ccp);
            else
                sgj_pr_hr(jsp, "%s vendor 0x%x:\n", mpgf_s, op->vendor_id);
            if (op->do_all)
                enumerate_mitems(pn, spn, com_pdt, op, jop);
            else {
                enumerate_mpage_names(op->transport, op->vendor_id, op);
            }
        } else if (op->transport >= 0) {
            ccp = sdp_get_transport_name(op->transport, blen, b);
            if (strlen(ccp) > 0)
                sgj_pr_hr(jsp, "%s %s %s:\n", mpgf_s, ccp, tp_s);
            else
                sgj_pr_hr(jsp, "%s %s 0x%x:\n", mpgf_s, tp_s, op->transport);
            if (op->do_all)
                enumerate_mitems(pn, spn, com_pdt, op, jop);
            else
                enumerate_mpage_names(op->transport, op->vendor_id, op);
        } else {        /* neither vendor nor transport given */
            if (op->do_long || (op->do_enum > 1)) {
                sgj_pr_hr(jsp, "%ss (not related to any %s or vendor):\n",
                          ump_s, tp_s);
                enumerate_mpage_names(-1, -1, op);
                sgj_pr_hr(jsp, "\n%ss:\n", tp_s);
                enumerate_transport_names(false, op);
                sgj_pr_hr(jsp, "\nVendors:\n");
                enumerate_vendor_names(op);
                if (op->do_all) {
                    struct sdparm_opt_coll t_opts;

                    sgj_pr_hr(jsp, "\n");
                    t_opts = *op;
                    enumerate_mitems(pn, spn, com_pdt, op, jop);
                    for (k = 0; k < 15; ++k) {
                        /* skip "no specific protocol" (0xf) */
                        ccp = sdp_get_transport_name(k, blen, b);
                        if (0 == strlen(ccp))
                            continue;
                        if (NULL == sdparm_transport_mp[k].mitem)
                            continue;
                        sgj_pr_hr(jsp, "\n");
                        sgj_pr_hr(jsp, "%s %s %s:\n", mpgf_s, ccp, tp_s);
                        t_opts.transport = k;
                        t_opts.vendor_id = -1;
                        enumerate_mitems(pn, spn, com_pdt, &t_opts, jop);
                    }
                    for (k = 0; k < sdparm_vendor_mp_len; ++k) {
                        if (VENDOR_NONE == k)
                            continue;
                        ccp = sdp_get_vendor_name(k);
                        if (NULL == ccp)
                            continue;
                        sgj_pr_hr(jsp, "\n");
                        sgj_pr_hr(jsp, "%s %s vendor:\n", mpgf_s, ccp);
                        t_opts.transport = -1;
                        t_opts.vendor_id = k;
                        enumerate_mitems(pn, spn, com_pdt, &t_opts, jop);
                    }
                } else {
                    for (k = 0; k < 15; ++k) {
                        /* skip "no specific protocol" (0xf) */
                        ccp = sdp_get_transport_name(k, blen, b);
                        if (0 == strlen(ccp))
                            continue;
                        sgj_pr_hr(jsp, "\n");
                        sgj_pr_hr(jsp, "%s %s %s:\n", mpgf_s, ccp, tp_s);
                        enumerate_mpage_names(k, -1, op);
                    }
                    for (k = 0; k < sdparm_vendor_mp_len; ++k) {
                        if (VENDOR_NONE == k)
                            continue;
                        ccp = sdp_get_vendor_name(k);
                        if (NULL == ccp)
                            continue;
                        sgj_pr_hr(jsp, "\n");
                        sgj_pr_hr(jsp, "%s %s vendor:\n", mpgf_s, ccp);
                        enumerate_mpage_names(-1, k, op);
                    }
                }
                sgj_pr_hr(jsp, "\n");
                sgj_pr_hr(jsp, "Commands:\n");
                sdp_enumerate_commands(op);
            } else {
                sgj_pr_hr(jsp, "Generic %ss:\n", mp_s);
                enumerate_mpage_names(-1, -1, op);
                if (op->do_all)
                    enumerate_mitems(pn, spn, com_pdt, op, jop);
                else
                    sgj_pr_hr(jsp, "\nTo see other %ss use the %s or %s "
                              "option\n", mp_s, tn_s, vn_s);
            }
        }
    } else      /* given mode page number */
        enumerate_mitems(pn, spn, com_pdt, op, jop);
    return 0;
}

static int
parse_cmd_line(struct sdparm_opt_coll * op, int argc, char * argv[],
               const char * device_name_arr[])
{
    int c, res, t_proto;
    const char * ccp;
    const struct sdparm_vendor_name_t * vnp;

    while (1) {
        int option_index = 0;

#ifdef SG_LIB_WIN32
        c = getopt_long(argc, argv,
                        "6aBc:C:dDeEfg:hHiI:j::J:lM:no:p:P:qrRs:St:vVwx",
                        long_options, &option_index);
#else
        c = getopt_long(argc, argv,
                        "6aBc:C:dDeEfg:hHiI:j::J:lM:no:p:P:qrRs:St:vVx",
                        long_options, &option_index);
#endif
        if (c == -1)
            break;

        switch (c) {
        case '6':
            op->mode_6 = true;
            break;
        case 'a':
            ++op->do_all;
            break;
        case 'B':
            op->dbd = true;
            break;
        case 'c':
            op->clear_str = optarg;
            op->set_clear = true;
            op->do_rw = true;
            break;
        case 'C':
            op->cmd_str = optarg;
            break;
        case 'd':
            op->dummy = true;
            break;
        case 'D':
            ++op->defaults;
            op->do_rw = true;
            break;
        case 'e':
            ++op->do_enum;
            break;
        case 'E':
            op->examine = true;
            break;
        case 'f':
            op->flexible = true;
            break;
        case 'g':
            op->get_str = optarg;
            break;
        case 'h':
            ++op->do_help;
            break;
        case '?':
            pr2serr("\n");
            usage(op);
            return SG_LIB_SYNTAX_ERROR;
        case 'H':
            ++op->do_hex;
            break;
        case 'i':
            op->inquiry = true;
            break;
        case 'I':
            op->inhex_fn = optarg;
            break;
        case 'j':
            op->do_json = true;
            op->json_arg = optarg;
            break;
        case 'J':
            op->do_json = true;
            op->js_file = optarg;
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
                    enumerate_vendor_names(op);
                    return SG_LIB_SYNTAX_ERROR;
                } else
                    op->vendor_id = vnp->vendor_id;
            } else {
                const struct sdparm_vendor_pair * svpp;

                res = sg_get_num_nomult(optarg);
                svpp = sdp_get_vendor_pair(res);
                if (NULL == svpp) {
                    pr2serr("Bad vendor value after '-M' (or '--vendor=') "
                            "option\n");
                    printf("Available vendors:\n");
                    enumerate_vendor_names(op);
                    return SG_LIB_SYNTAX_ERROR;
                }
                op->vendor_id = res;
            }
            break;
        case 'n':
            op->num_desc = true;
            break;
        case 'o':
            ccp = strchr(optarg, ',');
            if (ccp) {
                res = sg_get_num_nomult(ccp + 1);
                if ((res < 0) || (res > 15)) {
                    pr2serr("Bad out-mask value after comma, expect 0 to "
                             "15 (or 0xf)\n");
                    return SG_LIB_SYNTAX_ERROR;
                }
                op->in_mask = res;
            }
            if (',' != optarg[0]) {
                res = sg_get_num_nomult(optarg);
                if ((res < 0) || (res > 15)) {
                    pr2serr("Bad out-mask value, expect 0 to 15 (or 0xf)\n");
                    return SG_LIB_SYNTAX_ERROR;
                }
                op->out_mask = res;
            }
            break;
        case 'q':
            ++op->do_quiet;
            break;
        case 'p':
            if (op->page_str) {
                pr2serr("only one '--page=' option permitted\n");
                usage(op);
                return SG_LIB_CONTRADICT;
            } else
                op->page_str = optarg;
            break;
        case 'P':
            if ('-' == optarg[0])
                op->cl_pdt = -1;           /* those in SPC */
            else if isdigit(optarg[0]) {
                op->cl_pdt = sg_get_num_nomult(optarg);
                if ((op->cl_pdt < 0) || (op->cl_pdt > 0x1f)) {
                    pr2serr("--pdt= argument should be -1 to 31 or "
                            "acronym\n");
                    return SG_LIB_SYNTAX_ERROR;
                }
            } else {
                op->cl_pdt = sg_get_pdt_from_acronym(optarg);
                if (op->cl_pdt < -1) {
                    if (-3 == op->cl_pdt)
                        return SG_LIB_OK_FALSE;
                    pr2serr("could not decode acronym in --pdt= argument, "
                            "try '--pdt=xxx' to see what is available\n");
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
            op->set_str = optarg;
            op->do_rw = true;
            op->set_clear = true;
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
                    enumerate_transport_names(true, op);
                    return SG_LIB_SYNTAX_ERROR;
                } else
                    op->transport = t_proto;
            } else {
                res = sg_get_num_nomult(optarg);
                if ((res < 0) || (res > 15)) {
                    pr2serr("Bad transport value after '-t' option\n");
                    printf("Available transport protocols:\n");
                    enumerate_transport_names(false, op);
                    return SG_LIB_SYNTAX_ERROR;
                }
                op->transport = res;
            }
            break;
        case 'v':
            op->verbose_given = true;
            ++op->verbose;
            break;
        case 'V':
            op->version_given = true;
            break;
#ifdef SG_LIB_WIN32
        case 'w':
            ++op->do_wscan;
            break;
#endif
        case 'x':
            ++op->inner_hex;
            break;
        default:
            pr2serr("unrecognised option code 0x%x ??\n", c);
            usage(op);
            return SG_LIB_SYNTAX_ERROR;
        }
    }
    while (optind < argc) {
        if (op->num_devices < MAX_DEV_NAMES) {
            device_name_arr[op->num_devices++] = argv[optind];
            ++optind;
        } else {
            for (; optind < argc; ++optind)
                pr2serr("Unexpected extra argument: %s\n", argv[optind]);
            usage(op);
            return SG_LIB_SYNTAX_ERROR;
        }
    }
    return 0;
}

static int
parse_page_str(int * t_com_pdtp, struct sdparm_opt_coll * op)
{
    int orig_transport;
    int res = 0;
    int pn = -1;
    int spn = -1;
    int trans = op->transport;
    const struct sdparm_mp_name_t * mnp = NULL;
    const struct sdparm_vpd_page_t * vpp = NULL;
    const char * pg_str = op->page_str;
    char * cp;
    sgj_state * jsp = &op->json_st;
    char b[80];
    static const int blen = sizeof(b);

    if ((0 == strcmp("-1", pg_str)) || (0 == strcmp("-2", pg_str))) {
        op->inquiry = true;
        pn = VPD_NOT_STD_INQ;
    } else if (isalpha(pg_str[0])) {
        while ( true ) {      /* dummy loop, exit using break */
            mnp = sdp_find_mp_nm_by_acron(pg_str, trans, op->vendor_id);
            if (mnp)
                break;
            vpp = sdp_find_vpd_by_acron(pg_str);
            if (vpp)
                break;
            orig_transport = trans;
            if ((op->vendor_id < 0) && (trans < 0)) {
                op->transport = DEF_TRANSPORT_PROTOCOL;
                trans = DEF_TRANSPORT_PROTOCOL;
                mnp = sdp_find_mp_nm_by_acron(pg_str, trans, op->vendor_id);
                if (mnp)
                    break;
            }
            if ((op->vendor_id < 0) && (orig_transport < 0))
                pr2serr("abbreviation matches neither a %s nor a VPD page\n"
                        "    [perhaps a %s or %s option is needed]\n", mp_s,
                        tn_s, vn_s);
            else
                pr2serr("abbreviation matches neither a %s nor a VPD page\n",
                        mp_s);
            if (op->inquiry) {
                sgj_pr_hr(jsp, "available VPD pages:\n");
                enumerate_vpd_names(op);
                return SG_LIB_SYNTAX_ERROR;
            } else {
                sg_scnpr(b, blen, "available %ss", mp_s);
                if (op->vendor_id >= 0)
                    sgj_pr_hr(jsp, "%s (for given vendor):\n", b);
                else if (orig_transport >= 0)
                    sgj_pr_hr(jsp, "%s (for given transport):\n", b);
                else
                    sgj_pr_hr(jsp, "%s:\n", b);
                enumerate_mpage_names(orig_transport, op->vendor_id, op);
                return SG_LIB_SYNTAX_ERROR;
            }
        }           /* end of dummy while (true) */
        if (vpp) {
            pn = vpp->vpd_num;
            spn = vpp->subvalue;
            op->inquiry = true;
            *t_com_pdtp = vpp->com_pdt;
        } else {
            if (op->inquiry) {
                pr2serr("matched %s acronym but given '-i' so expecting "
                        "a VPD page\n", mp_s);
                return SG_LIB_CONTRADICT;
            }
            pn = mnp->page;
            spn = mnp->subpage;
            *t_com_pdtp = mnp->com_pdt;
        }
    } else {        /* got page_str and first char probably numeric */
        cp = (char *)strchr(pg_str, ',');
        pn = sg_get_num_nomult(pg_str);
        if ((pn < 0) || (pn > 255)) {
            pr2serr("Bad page code value after '-p' option\n");
            if (op->inquiry) {
                sgj_pr_hr(jsp, "available VPD pages:\n");
                enumerate_vpd_names(op);
                return SG_LIB_SYNTAX_ERROR;
            } else {
                sg_scnpr(b, blen, "available %ss", mp_s);
                if (op->vendor_id >= 0)
                    sgj_pr_hr(jsp, "%s (for given vendor):\n", b);
                else if (trans >= 0)
                    sgj_pr_hr(jsp, "%s (for given transport):\n", b);
                else
                    sgj_pr_hr(jsp, "%s:\n", b);
                enumerate_mpage_names(trans, op->vendor_id, op);
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
    op->cl_pn = pn;
    op->cl_spn = spn;
    return res;
}


int
main(int argc, char * argv[])
{
    bool protect = false;
    bool as_json = false;
    int t_com_pdt, req_pdt, k, r;
    int res = 0;
    int sg_fd = -1;
    int cmd_arg = -1;
    int pn = -1;
    int spn = -1;
    int ret = 0;
    struct sdparm_opt_coll * op;
    const char * device_name_arr[MAX_DEV_NAMES];
    sgj_state * jsp;
    sgj_opaque_p jop = NULL;
    sgj_opaque_p jo2p = NULL;
    const struct sdparm_command_t * scmdp = NULL;
    struct sdparm_opt_coll opts;
    struct sdparm_mp_settings_t mp_settings;

    op = &opts;
    memset(op, 0, sizeof(opts));
    op->out_mask = MP_OM_ALL;   /* 0xf */
    op->in_mask = MP_IM_ALL;    /* 0xf */
    op->cl_pdt = -1;
    op->transport = -1;
    op->vendor_id = -1;
    memset(device_name_arr, 0, sizeof(device_name_arr));
    memset(&mp_settings, 0, sizeof(mp_settings));
    t_com_pdt = -1;

    res = parse_cmd_line(op, argc, argv, device_name_arr);
    if (res) {
        if (SG_LIB_OK_FALSE == res)
            return 0;
        else
            return res;
    }

#ifdef DEBUG
    pr2serr("In DEBUG mode, ");
    if (op->verbose_given && op->version_given) {
        pr2serr("but override: '-vV' given, zero verbose and continue\n");
        op->verbose_given = false;
        op->version_given = false;
        op->verbose = 0;
    } else if (! op->verbose_given) {
        pr2serr("set '-vv'\n");
        op->verbose = 2;
    } else
        pr2serr("keep verbose=%d\n", op->verbose);
#else
    if (op->verbose_given && op->version_given)
        pr2serr("Not in DEBUG mode, so '-vV' has no special action\n");
#endif
    if (op->version_given) {
        pr2serr("version: %s\n", version_str);
        return 0;
    }

    if (op->do_help > 0) {
        usage(op);
        return 0;
    }

    if (op->read_only)
        op->do_rw = false;         // override any read-write settings

    if ((!!op->get_str + !!op->set_str + !!op->clear_str) > 1) {
        pr2serr("Can only give one of '--get=', '--set=' and '--clear='\n");
        return SG_LIB_CONTRADICT;
    }
#ifdef SG_LIB_WIN32
    if (op->do_wscan)
        return sg_do_wscan('\0', op->do_wscan, op->verbose);
#endif
    jsp = &op->json_st;
    if (op->do_json) {
        if (! sgj_init_state(jsp, op->json_arg)) {
            int bad_char = jsp->first_bad_char;
            char e[1500];

            if (bad_char) {
                pr2serr("bad argument to --json= option, unrecognized "
                        "character '%c'\n\n", bad_char);
            }
            sg_json_usage(0, e, sizeof(e));
            pr2serr("%s", e);
            ret = SG_LIB_SYNTAX_ERROR;
            goto fini;
        }
        jop = sgj_start_r(sdp_sn, version_str, argc, argv, jsp);
        jo2p = sgj_named_subobject_r(jsp, jop,
                                 op->do_enum ? sdp_enum_sn : sdp_rsp_sn);
    }
    as_json = jsp->pr_as_json;

    t_com_pdt = op->cl_pdt;
    if (op->page_str) {
        ret = parse_page_str(&t_com_pdt, op);
        if (ret)
            goto fini;
        pn = op->cl_pn;
        spn = op->cl_spn;
    }

    if (op->inquiry) {  /* VPD pages or standard INQUIRY response */
        if (op->set_clear || op->get_str || op->cmd_str || op->defaults ||
            op->save) {
            pr2serr("'--inquiry' option lists VPD pages so other options "
                    "that are\nconcerned with %ss are inappropriate\n", mp_s);
            ret = SG_LIB_CONTRADICT;
            goto fini;
        }
        if (pn > 255) {
            pr2serr("VPD page numbers are from 0 to 255\n");
            ret = SG_LIB_SYNTAX_ERROR;
            goto fini;
        }
        if (op->do_enum) {
            sgj_pr_hr(jsp, "VPD pages:\n");
            enumerate_vpd_names(op);
            ret = 0;
            goto fini;
        }
    } else if (op->cmd_str) {
        if (op->set_clear || op->get_str || op->defaults || op->save ||
            op->examine) {
            pr2serr("'--command=' option is not valid with other "
                    "options that are\nconcerned with %ss\n", mp_s);
            ret = SG_LIB_CONTRADICT;
            goto fini;
        }
        if (op->do_enum) {
            sgj_pr_hr(jsp, "Available commands:\n");
            sdp_enumerate_commands(op);
            ret = 0;
            goto fini;
        }
        scmdp = sdp_build_cmd(op->cmd_str, &op->do_rw, &cmd_arg);
        if (NULL == scmdp) {
            pr2serr("'--command=%s' not found\n", op->cmd_str);
            sgj_pr_hr(jsp, "available commands\n");
            sdp_enumerate_commands(op);
            ret = SG_LIB_SYNTAX_ERROR;
            goto fini;
        }
        if (op->read_only)
            op->do_rw = false;         // override any read-write settings
    } else {            /* assume mode page access */
        if (pn < 0) {
            mp_settings.pg_num = -1;
            mp_settings.subpg_num = -1;
        } else {
            mp_settings.pg_num = pn;
            mp_settings.subpg_num = spn;
        }
        if (op->do_enum) {
            ret = enumerate_mp_contents(pn, spn, t_com_pdt, op, jo2p);
            goto fini;
        }
        if (op->get_str) {
            if (! build_mp_settings(op->get_str, &mp_settings, op, false,
                                    true)) {
                ret = SG_LIB_SYNTAX_ERROR;
                goto fini;
            }
        }

        if (op->num_desc && (pn < 0)) {
            pr2serr("when '--num-desc' is given an explicit %s is "
                    "required\n", mp_s);
            ret = SG_LIB_CONTRADICT;
            goto fini;
        }

        if (op->defaults && (op->set_clear || op->get_str)) {
            pr2serr("'--get=', '--set=' or '--clear=' "
                    "can't be used with '--defaults'\n");
            ret = SG_LIB_CONTRADICT;
            goto fini;
        }

        if (op->set_str) {
            if (! build_mp_settings(op->set_str, &mp_settings, op, false,
                                    false)) {
                ret = SG_LIB_SYNTAX_ERROR;
                goto fini;
            }
        }
        if (op->clear_str) {
            if (! build_mp_settings(op->clear_str, &mp_settings, op, true,
                                    false)) {
                ret = SG_LIB_SYNTAX_ERROR;
                goto fini;
            }
        }

        if (op->verbose && (mp_settings.num_it_vals > 0))
            list_mp_settings(&mp_settings, (NULL != op->get_str), op);

        if ((1 == op->defaults) && (pn < 0)) {
            pr2serr("to set a page's defaults, the '--page=' option must be "
                    "used\n");
            ret = SG_LIB_CONTRADICT;
            goto fini;
        }
    }

    if (op->inhex_fn) {
        if (op->num_devices > 0) {
            pr2serr("Cannot have both a DEVICE and --inhex= option\n");
            ret = SG_LIB_CONTRADICT;
            goto fini;
        }
        inhex_buffp = sg_memalign(MAX_MP_BUFF_SZ, 0, &free_inhex_buffp, false);
        if (NULL == inhex_buffp) {
            ret = sg_convert_errno(ENOMEM);
            goto fini;
        }
        ret = sg_f2hex_arr(op->inhex_fn, op->do_raw, false, inhex_buffp,
                           &op->inhex_len, MAX_MP_BUFF_SZ);
        if (ret)
            goto fini;
        if (op->verbose > 2)
            pr2serr("Read %d bytes from user input\n", op->inhex_len);
        if (op->verbose > 3)
            hex2stderr(inhex_buffp, op->inhex_len, 0);
        op->do_raw = false;
        if (op->inquiry)
            ret = sdp_process_vpd_page(-1, pn, ((spn < 0) ? 0: spn),
                                       t_com_pdt, protect, inhex_buffp,
                                       NULL, 0, op, jo2p);
        else
            ret = print_inhex_mode_pages(inhex_buffp, op, jo2p);
        goto fini;
    }

    if (0 == op->num_devices) {
        pr2serr("one or more device names required\n");
        if (! op->do_quiet) {
            pr2serr("\n");
            usage(op);
        }
        ret = SG_LIB_SYNTAX_ERROR;
        goto fini;
    }

    /* MAX_MP_BUFF_SZ byte sized heap allocations aligned to a page */
    cur_aligned_mp = sg_memalign(MAX_MP_BUFF_SZ, 0, &free_cur_aligned_mp,
                                 false);
    cha_aligned_mp = sg_memalign(MAX_MP_BUFF_SZ, 0, &free_cha_aligned_mp,
                                 false);
    def_aligned_mp = sg_memalign(MAX_MP_BUFF_SZ, 0, &free_def_aligned_mp,
                                 false);
    sav_aligned_mp = sg_memalign(MAX_MP_BUFF_SZ, 0, &free_sav_aligned_mp,
                                 false);
    oth_aligned_mp = sg_memalign(MAX_MP_BUFF_SZ, 0, &free_oth_aligned_mp,
                                 false);
    if ((NULL == cur_aligned_mp) || (NULL == cha_aligned_mp) ||
        (NULL == def_aligned_mp) || (NULL == sav_aligned_mp) ||
        (NULL == oth_aligned_mp)) {
        ret = sg_convert_errno(ENOMEM);
        goto fini;
    }

    req_pdt = t_com_pdt;
    ret = 0;
    for (k = 0; k < op->num_devices; ++k) {
        int pdt = -1;

        if (op->verbose > 1)
            pr2serr(">>> about to open device name: %s\n",
                    device_name_arr[k]);
        sg_fd = open_and_simple_inquiry(device_name_arr[k], op->do_rw, &pdt,
                                        &protect, op);
        if (sg_fd < 0) {
            if (0 == ret)
                ret = -sg_fd;
            continue;
        }

        if (op->inquiry) {
            if (op->examine)
                r = examine_vpd_page(sg_fd, pn, spn, req_pdt, protect, op,
                                     jo2p);
            else
                r = sdp_process_vpd_page(sg_fd, pn, ((spn < 0) ? 0: spn),
                                         req_pdt, protect, NULL, NULL, 0,
                                         op, jo2p);
        } else {
            if (op->cmd_str && scmdp)   /* process command */
                r = sdp_process_cmd(sg_fd, scmdp, cmd_arg, pdt, op);
            else {                  /* mode page */
                if (op->examine)
                    r = examine_mode_pages(sg_fd, pn, req_pdt, op, jo2p);

                else
                    r = process_mode_page(sg_fd, &mp_settings, pn, spn,
                                          pdt, op, jo2p);
            }
        }

        res = sg_cmds_close_device(sg_fd);
        if (res < 0) {
            pr2serr("close error: %s\n", safe_strerror(-res));
            if (0 == r)
                r = -res;
        }
        if (r  && ((0 == ret) || (SG_LIB_FILE_ERROR == ret)))
            ret = r;
    }   /* end of DEVICEs for loop */

fini:           /* error expected in ret, ret==0 means no error */
    if (free_inhex_buffp)
        free(free_inhex_buffp);
    if (free_cur_aligned_mp)
        free(free_cur_aligned_mp);
    if (free_cha_aligned_mp)
        free(free_cha_aligned_mp);
    if (free_def_aligned_mp)
        free(free_def_aligned_mp);
    if (free_sav_aligned_mp)
        free(free_sav_aligned_mp);
    if (free_oth_aligned_mp)
        free(free_oth_aligned_mp);
    ret = (ret >= 0) ? ret : SG_LIB_CAT_OTHER;
    if (as_json) {
        FILE * fp = stdout;

        if (op->js_file) {
            if ((1 != strlen(op->js_file)) || ('-' != op->js_file[0])) {
                fp = fopen(op->js_file, "w");   /* truncate if exists */
                if (NULL == fp) {
                    pr2serr("unable to open file: %s\n", op->js_file);
                    res = SG_LIB_FILE_ERROR;
                }
            }
            /* '--js-file=-' will send JSON output to stdout */
        }
        if (fp)
            sgj_js2file(jsp, NULL, res, fp);
        if (op->js_file && fp && (stdout != fp))
            fclose(fp);
        sgj_finish(jsp);
        if ((0 == ret) && (res > 0))
            ret = res;
    }
    if ((0 == op->do_quiet) && (non_spg_warning > 0))
        pr2serr("%d instances of mode subpage requested but non-subpage "
                "returned\n", non_spg_warning);
    return ret;
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

    if ((NULL == device_name) || (stat(device_name, &a_stat) < 0)) {
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

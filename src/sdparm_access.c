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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
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

#include "sdparm.h"
#include "sg_lib.h"
#include "sg_unaligned.h"
#include "sg_pr2serr.h"

/* sdparm_access.c : helpers for sdparm to access tables in
 * sdparm_data.c and command line help.
 */

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
    {"flags", no_argument, 0, 'F'},
    {"flexible", no_argument, 0, 'f'},
    {"get", required_argument, 0, 'g'},
    {"help", no_argument, 0, 'h'},
    {"hex", no_argument, 0, 'H'},
    {"inquiry", no_argument, 0, 'i'},
    {"inhex", required_argument, 0, 'I'},
    {"inner-hex", no_argument, 0, 'x'},
    {"inner_hex", no_argument, 0, 'x'},
    {"json", optional_argument, 0, '^'},    /* short option is '-j' */
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
            "    sdparm [-a] [-B] [-E] [-f] [-g STR] [-H] [-x] [-j[=JO]] "
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
            "    sdparm -i [-a] [-E] [-f] [-H] [-j[=JO]] [-J JFN] [-n] "
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
            "    sdparm --enumerate [--all] [--flags] [--get=STR] "
            "[--inquiry]\n"
            "           [--json[=JO]] [--js-file=JFN] [--long] "
            "[--page=PG[,SPG]]\n"
            "           [--transport=TN] [--vendor=VN]\n"
              );
    else
        pr2serr(
            "    sdparm -e [-a] [-F] [-g STR] [-i] [-j[=JO]] [-J JFN] [-l]\n"
            "           [-p PG[,SPG]] [-t TN] [-M VN]\n"
              );
}

static void inhex_usage(bool long_opt)
{
    if (long_opt)
        pr2serr(
            "    sdparm --inhex=FN [--all] [--flexible] [--get=STR] [--hex] "
            "[--inner-hex]\n"
            "           [--inquiry] [--json[=JO]] [--js-file=JFN] "
            "[--long]\n"
            "           [--out=mask=,IM] [--page=PG[,SPG]] [--pdt=DT] "
            "[--raw] [--six]\n"
            "           [--transport=TN] [--vendor=VN] [--verbose]\n"
              );
    else
        pr2serr(
            "    sdparm -I FN [-a] [-f] [-g STR] [-H] [-x] [-i] [-j[=JO]] "
            "[-J JFN]\n"
            "           [-l] [-o ,IM] [-p PG[,SPG]] [-P PDT] [-R] [-6] "
            "[-t TN]\n"
            "           [-M VN] [-v]\n"
              );
}

void
sdp_usage(const struct sdparm_opt_coll * op)
{
    static const char * mp_s = "mode page";

    if (0 == op->do_help) {
        pr2serr("%s access usage with long form options:\n", mp_s);
        mp_rd_usage(true);
        pr2serr(" Usage with corresponding short form options:\n");
        mp_rd_usage(false);
        pr2serr("\n");
        pr2serr("%s changes usage with long form options:\n", mp_s);
        mp_wr_usage(true);
        pr2serr(" Usage with corresponding short form options:\n");
        mp_wr_usage(false);
        pr2serr("\n");
        pr2serr("VPD page access usage with long form options:\n");
        inq_usage(true);
        pr2serr(" Usage with corresponding short form options:\n");
        inq_usage(false);
        pr2serr("\n");
        pr2serr("SCSI commands usage with long form options:\n");
        cmd_usage(true);
        pr2serr(" Usage with corresponding short form options:\n");
        cmd_usage(false);
        pr2serr("\n");
        pr2serr("Enumeration of internal tables usage with long form "
                "options:\n");
        enum_usage(true);
        pr2serr(" Usage with corresponding short form options:\n");
        enum_usage(false);
        pr2serr("\n");
        pr2serr("inhex specific usage with long form options:\n");
        inhex_usage(true);
        pr2serr(" Usage with corresponding short form options:\n");
        inhex_usage(false);
        return;
    }
    if (1 == op->do_help) {
        pr2serr("Usage for mode pages for DEVICEs:\n");
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
            "    --dbd | -B            set DBD bit in mode sense cdb "
            "(disable\n"
            "                          block descriptors)\n"
            "    --defaults | -D       set a mode page to its default "
            "values\n"
            "                          when use twice set all pages to "
            "their defaults\n"
            "    --dummy | -d          don't write back modified mode page\n"
            "    --flags | -F          show enumeration item flags\n"
            "    --flexible | -f       compensate for common errors, "
            "relax some checks\n"
            "    --get=STR | -g STR    get (fetch) field value(s), by "
            "acronym or pos\n"
            "    --hex | -H            output in hex rather than name/value "
            "pairs\n"
            "    --inner-hex | -x      print innermost fields in hex\n"
            "    --json[=JO] | -j[=JO]    output in JSON instead of plain "
            "text\n"
            "                             Use --json=? for JSON help\n"
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
                "  sdparm [-a] [-B] [-E] [-f] [-g STR] [-H] [-x] [-j[=JO]] "
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
                "  sdparm -i [-a] [-E] [-f] [-H] [-j[=JO]] [-J JFN] [-n] "
                "[-p PG[,SPG]]\n"
                "         [-q] [-r] [-t TN]  [-M VN] [-v] DEVICE "
                "[DEVICE...]\n"
                "\n"
                "  sdparm -e [-a] [-i] [-l] [-p PG[,SPG]] [-t TN] [-M VN]\n"
                "\n"
                "  sdparm -I FN [-a] [-f] [-H] [-x] [-j[=JO]] [-J JFN] [-i] "
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
            "    --json[=JO]|-j[=JO]    output in JSON instead of plain "
            "text\n"
            "                           Use --json=? for JSON help\n"
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

/* Handles short options after '-j' including a sequence of short options
 * that include one 'j' (for JSON). Want optional argument to '-j' to be
 * prefixed by '='. Return 0 for good, SG_LIB_SYNTAX_ERROR for syntax error
 * and SG_LIB_OK_FALSE for exit with no error. */
static int
chk_short_opts(const char sopt_ch, struct sdparm_opt_coll * op)
{
    /* only need to process short, non-argument options */
    switch (sopt_ch) {
    case '6':
        op->mode_6 = true;
        break;
    case 'a':
        ++op->do_all;
        break;
    case 'B':
        op->dbd = true;
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
    case 'F':
        ++op->do_flags;
        break;
    case 'h':
        ++op->do_help;
        break;
    case '?':
        pr2serr("\n");
        sdp_usage(op);
        return SG_LIB_OK_FALSE;
    case 'H':
        ++op->do_hex;
        break;
    case 'i':
        op->inquiry = true;
        break;
    case 'j':
        break;  /* simply ignore second 'j' (e.g. '-jxj') */
    case 'l':
        ++op->do_long;
        break;
    case 'n':
        op->num_desc = true;
        break;
    case 'q':
        ++op->do_quiet;
        break;
    case 'r':
        op->read_only = true;
        break;
    case 'R':
        op->do_raw = true;
        break;
    case 'S':
        op->save = true;
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
        pr2serr("unrecognised option code %c [0x%x] ??\n", sopt_ch, sopt_ch);
        return SG_LIB_SYNTAX_ERROR;
    }
    return 0;
}

int
sdp_parse_cmdline(struct sdparm_opt_coll * op, int argc, char * argv[],
                  const char * device_name_arr[])
{
    int c, res, t_proto;
    const char * ccp;
    const struct sdparm_vendor_name_t * vnp;

    while (1) {
        int option_index = 0;

#ifdef SG_LIB_WIN32
        c = getopt_long(argc, argv,
                        "^6aBc:C:dDeEfFg:hHiI:j::J:lM:no:p:P:qrRs:St:vVwx",
                        long_options, &option_index);
#else
        c = getopt_long(argc, argv,
                        "^6aBc:C:dDeEfFg:hHiI:j::J:lM:no:p:P:qrRs:St:vVx",
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
        case 'F':
            ++op->do_flags;
            break;
        case 'g':
            if (op->get_str) {
                pr2serr("Can have only one --get= option. Instead the "
                        "arguments to\n--get= can be concaternated using a "
                        "comma as a separator.\n");
                return SG_LIB_SYNTAX_ERROR;
            }
            op->get_str = optarg;
            break;
        case 'h':
            ++op->do_help;
            break;
        case '?':
            pr2serr("\n");
            sdp_usage(op);
            return SG_LIB_OK_FALSE;
        case 'H':
            ++op->do_hex;
            break;
        case 'i':
            op->inquiry = true;
            break;
        case 'I':
            op->inhex_fn = optarg;
            break;
        case 'j':       /* for: -j[=JO] */
        case '^':       /* for: --json[=JO] */
            op->do_json = true;
            /* Now want '=' to precede all JSON optional arguments */
            if (optarg) {
                int k, n, q;

                if ('^' == c) {
                    op->json_arg = optarg;
                    break;
                } else if ('=' == *optarg) {
                    op->json_arg = optarg + 1;
                    break;
                }
                n = strlen(optarg);
                for (k = 0; k < n; ++k) {
                    q = chk_short_opts(*(optarg + k), op);
                    if (SG_LIB_SYNTAX_ERROR == q)
                        return SG_LIB_SYNTAX_ERROR;
                    if (SG_LIB_OK_FALSE == q)
                        return 0;
                }
            } else
                op->json_arg = NULL;
            break;
        case 'J':
            op->do_json = true;
            op->js_file = optarg;
            break;
        case 'l':
            ++op->do_long;
            break;
        case 'M':
            if (isalpha((uint8_t)optarg[0])) {
                vnp = sdp_find_vendor_by_acron(optarg);
                if (NULL == vnp) {
                    pr2serr("abbreviation does not match a vendor\n");
                    printf("Available vendors:\n");
                    sdp_enumerate_vendor_names(op);
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
                    sdp_enumerate_vendor_names(op);
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
                sdp_usage(op);
                return SG_LIB_CONTRADICT;
            } else
                op->page_str = optarg;
            break;
        case 'P':
            if ('-' == optarg[0])
                op->cl_pdt = -1;           /* those in SPC */
            else if (isdigit((uint8_t)optarg[0])) {
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
            if (op->set_str) {
                pr2serr("Can have only one --set= option. Instead the "
                        "arguments to\n--set= can be concaternated using a "
                        "comma as a separator.\n");
                return SG_LIB_SYNTAX_ERROR;
            }
            op->set_str = optarg;
            op->do_rw = true;
            op->set_clear = true;
            break;
        case 'S':
            op->save = true;
            break;
        case 't':
            if (isalpha((uint8_t)optarg[0])) {
                t_proto = sdp_find_transport_id_by_acron(optarg);
                if (t_proto < 0) {
                    pr2serr("abbreviation does not match a transport "
                            "protocol\n");
                    printf("Available transport protocols:\n");
                    sdp_enumerate_transport_names(true, op);
                    return SG_LIB_SYNTAX_ERROR;
                } else
                    op->transport = t_proto;
            } else {
                res = sg_get_num_nomult(optarg);
                if ((res < 0) || (res > 15)) {
                    pr2serr("Bad transport value after '-t' option\n");
                    printf("Available transport protocols:\n");
                    sdp_enumerate_transport_names(false, op);
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
            sdp_usage(op);
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
            sdp_usage(op);
            return SG_LIB_SYNTAX_ERROR;
        }
    }
    return 0;
}

/* Returns 1 if strings equal (same length, characters same or only differ
 * by case), else returns 0. Assumes 7 bit ASCII (English alphabet). */
int
sdp_strcase_eq(const char * s1p, const char * s2p)
{
    int c1, c2;

    do {
        c1 = *s1p++;
        c2 = *s2p++;
        if (c1 != c2) {
            if (c2 >= 'a')
                c2 = toupper(c2);
            else if (c1 >= 'a')
                c1 = toupper(c1);
            else
                return 0;
            if (c1 != c2)
                return 0;
        }
    } while (c1);
    return 1;
}

/* Returns 1 if strings equal up to the nth character (characters same or only
 * differ by case), else returns 0. Assumes 7 bit ASCII (English alphabet). */
int
sdp_strcase_eq_upto(const char * s1p, const char * s2p, int n)
{
    int k, c1, c2;

    for (k = 0; k < n; ++k) {
        c1 = *s1p++;
        c2 = *s2p++;
        if (c1 != c2) {
            if (c2 >= 'a')
                c2 = toupper(c2);
            else if (c1 >= 'a')
                c1 = toupper(c1);
            else
                return 0;
            if (c1 != c2)
                return 0;
        }
        if (0 == c1)
            break;
    }
    return 1;
}

/* Returns length of mode page. Assumes mp pointing at start of a mode
 * page (not the start of a MODE SENSE response). */
int
sdp_mpage_len(const uint8_t * mp)
{
    /* if SPF (byte 0, bit 6) is set then 4 byte header, else 2 byte header */
    return (mp[0] & 0x40) ? (sg_get_unaligned_be16(mp + 2) + 4) : (mp[1] + 2);
}

enum mode_page_class {MPC_VENDOR, MPC_TRANSPORT, MPC_GENERAL};

const struct sdparm_mp_name_t *
sdp_get_mp_nm(int page_num, int subpage_num, int pdt, int transp_proto,
              int vendor_id)
{
    enum mode_page_class mpc;
    int decayed_pdt;
    const struct sdparm_mp_name_t * mnp;

    if (vendor_id >= 0) {
        const struct sdparm_vendor_pair * vpp;

        vpp = sdp_get_vendor_pair(vendor_id);
        mnp = (vpp ? vpp->mpage : NULL);
        mpc = MPC_VENDOR;
    } else if ((transp_proto >= 0) && (transp_proto < 16)) {
        mnp = sdparm_transport_mp[transp_proto].mpage;
        mpc = MPC_TRANSPORT;
    } else {
        mnp = sdparm_gen_mode_pg;
        mpc = MPC_GENERAL;
    }
    if (NULL == mnp)
        return NULL;

try_again:
    for ( ; mnp->mp_acron; ++mnp) {
        if ((page_num == mnp->page) && (subpage_num == mnp->subpage)) {
            if ((pdt < 0) || (mnp->com_pdt < 0) ||
                sg_pdt_s_eq(mnp->com_pdt, pdt))
                return mnp;
        }
    }
    if (MPC_GENERAL == mpc) {
        decayed_pdt = sg_lib_pdt_decay(pdt);
        if (decayed_pdt != pdt) {
            pdt = decayed_pdt;
            goto try_again;
        }
    }
    return NULL;
}

/* Returns pointer to a sdparm_mp_name_t object matching the first 5
 * arguments. If no match returns NULL. If match and bp is non-NULL
 * then outputs a the name, acronym (if plus_acron) and hex values
 * (if 'hex' == true) to bp . */
const struct sdparm_mp_name_t *
sdp_get_mp_nm_with_str(int page_num, int subpage_num, int pdt,
                       int transp_proto, int vendor_id, bool plus_acron,
                       bool hex, int b_len, char * bp)
{
    int len = b_len - 1;
    const struct sdparm_mp_name_t * mnp = NULL;
    const char * cp;

    /* first try to match given pdt */
    mnp = sdp_get_mp_nm(page_num, subpage_num, pdt, transp_proto, vendor_id);
    if (NULL == mnp) /* didn't match specific pdt so try -1 (ie. SPC) */
        mnp = sdp_get_mp_nm(page_num, subpage_num, -1, transp_proto,
                            vendor_id);
    if ((len < 0) || (NULL == bp))
        return mnp;
    bp[len] = '\0';
    if (mnp && mnp->mp_name) {
        cp = mnp->mp_acron;
        if (NULL == cp)
            cp = "";
        if (hex) {
            if (0 == subpage_num) {
                if (plus_acron)
                    snprintf(bp, len, "%s [%s: 0x%x]", mnp->mp_name, cp,
                             page_num);
                else
                    snprintf(bp, len, "%s [0x%x]", mnp->mp_name, page_num);
            } else {
                if (plus_acron)
                    snprintf(bp, len, "%s [%s: 0x%x,0x%x]", mnp->mp_name, cp,
                             page_num, subpage_num);
                else
                    snprintf(bp, len, "%s [0x%x,0x%x]", mnp->mp_name,
                             page_num, subpage_num);
            }
        } else {
            if (plus_acron)
                snprintf(bp, len, "%s [%s]", mnp->mp_name, cp);
            else
                snprintf(bp, len, "%s", mnp->mp_name);
        }
    } else {
        if (0 == subpage_num)
            snprintf(bp, len, "[0x%x]", page_num);
        else
            snprintf(bp, len, "[0x%x,0x%x]", page_num, subpage_num);
    }
    return mnp;
}

const struct sdparm_mp_name_t *
sdp_find_mp_nm_by_acron(const char * ap, int transp_proto, int vendor_id)
{
    const struct sdparm_mp_name_t * mnp;

    if (vendor_id >= 0) {
        const struct sdparm_vendor_pair * vpp;

        vpp = sdp_get_vendor_pair(vendor_id);
        mnp = (vpp ? vpp->mpage : NULL);
    } else if ((transp_proto >= 0) && (transp_proto < 16))
        mnp = sdparm_transport_mp[transp_proto].mpage;
    else
        mnp = sdparm_gen_mode_pg;
    if (NULL == mnp)
        return NULL;

    for ( ; mnp->mp_acron; ++mnp) {
        if (sdp_strcase_eq(mnp->mp_acron, ap))
            return mnp;
    }
    return NULL;
}

const struct sdparm_vpd_page_t *
sdp_get_vpd_detail(int page_num, int subvalue, int pdt)
{
    const struct sdparm_vpd_page_t * vpp;
    int sv, ty;

    sv = (subvalue < 0) ? 1 : 0;
    ty = (pdt < 0) ? 1 : 0;
    for (vpp = sdparm_vpd_pg; vpp->vpd_acron; ++vpp) {
        if ((page_num == vpp->vpd_num) &&
            (sv || (subvalue == vpp->subvalue)) &&
            (ty || (pdt == vpp->com_pdt)))
            return vpp;
    }
    if (! ty)
        return sdp_get_vpd_detail(page_num, subvalue, -1);
    if (! sv)
        return sdp_get_vpd_detail(page_num, -1, -1);
    return NULL;
}

const struct sdparm_vpd_page_t *
sdp_find_vpd_by_acron(const char * ap)
{
    const struct sdparm_vpd_page_t * vpp;

    for (vpp = sdparm_vpd_pg; vpp->vpd_acron; ++vpp) {
        if (sdp_strcase_eq(vpp->vpd_acron, ap))
            return vpp;
    }
    return NULL;
}

char *
sdp_get_transport_name(int proto_num, int b_len, char * b)
{
    char d[128];

    if (NULL == b)
        ;
    else if (b_len < 2) {
        if (1 == b_len)
            b[0] = '\0';
    } else
        snprintf(b, b_len, "%s", sg_get_trans_proto_str(proto_num, sizeof(d),
                 d));
    return b;
}

int
sdp_find_transport_id_by_acron(const char * ap)
{
    const struct sdparm_val_desc_t * t_vdp;
    const struct sdparm_val_desc_t * t_addp;

    for (t_vdp = sdparm_transport_id; t_vdp->desc; ++t_vdp) {
        if (sdp_strcase_eq(t_vdp->desc, ap))
            return t_vdp->val;
    }
    /* No match ... try additional transport acronyms */
    for (t_addp = sdparm_add_transport_acron; t_addp->desc; ++t_addp) {
        if (sdp_strcase_eq(t_addp->desc, ap))
            return t_addp->val;
    }
    return -1;
}

const char *
sdp_get_vendor_name(int vendor_id)
{
    const struct sdparm_vendor_name_t * vnp;

    for (vnp = sdparm_vendor_id; vnp->acron; ++vnp) {
        if (vendor_id == vnp->vendor_id)
            return vnp->name;
    }
    return NULL;
}

const struct sdparm_vendor_name_t *
sdp_find_vendor_by_acron(const char * ap)
{
    const struct sdparm_vendor_name_t * vnp;

    for (vnp = sdparm_vendor_id; vnp->acron; ++vnp) {
        if (sdp_strcase_eq_upto(vnp->acron, ap, strlen(vnp->acron)))
            return vnp;
    }
    return NULL;
}

const struct sdparm_vendor_pair *
sdp_get_vendor_pair(int vendor_id)
{
     return ((vendor_id >= 0) && (vendor_id < sdparm_vendor_mp_len))
            ? (sdparm_vendor_mp + vendor_id) : NULL;
}

/* Searches mpage items table from (and including) the current position
 * looking for the first match on 'ap' (pointer to acromym). Checks
 * against the inbuilt table (in sdparm_data.c) of generic (when both
 * transp_proto and vendor_id are -1), transport (when transp_proto is
 * >= 0) or vendor (when vendor_id is >= 0) mode page items (fields).
 * If found a pointer to that mitem is returned and *from_p is set to
 * the offset after the match. If not found then NULL is returned and
 * *from_p is set to the offset of the sentinel at the end of the
 * selected mitem array. Start iteration by setting from_p to NULL or
 * point it at -1. */
const struct sdparm_mp_item_t *
sdp_find_mitem_by_acron(const char * ap, int * from_p, int transp_proto,
                        int vendor_id)
{
    int k = 0;
    const struct sdparm_mp_item_t * mpi;

    if (from_p) {
        k = *from_p;
        if (k < 0)
            k = 0;
    }
    if (vendor_id >= 0) {
        const struct sdparm_vendor_pair * vpp;

        vpp = sdp_get_vendor_pair(vendor_id);
        mpi = (vpp ? vpp->mitem : NULL);
    } else if ((transp_proto >= 0) && (transp_proto < 16))
        mpi = sdparm_transport_mp[transp_proto].mitem;
    else
        mpi = sdparm_mitem_arr;
    if (NULL == mpi)
        return NULL;

    for (mpi += k; mpi->acron; ++k, ++mpi) {
        if (sdp_strcase_eq(mpi->acron, ap))
            break;
    }
    if (NULL == mpi->acron)
        mpi = NULL;
    if (from_p)
        *from_p = (mpi ? (k + 1) : k);
    return mpi;
}

uint64_t
sdp_mitem_get_value(const struct sdparm_mp_item_t *mpi,
                    const uint8_t * mp)
{
    return sg_get_big_endian(mp + mpi->start_byte, mpi->start_bit,
                              mpi->num_bits);
}

/* Gets a mode page item's value given a pointer to the mode page response
 * (mp). If all_setp is non-NULL then checks 8, 16, 24, 32, 48 and 64 bit
 * quantities for all bits set (e.g. for 8 bits that would be 0xff) and if
 * so sets the bool addressed by all_setp to true. Otherwise if all_setp
 * is non-NULL then it sets the bool addressed by all_setp to false.
 * Returns the value in an unsigned 64 bit integer. To print a value as a
 * signed quantity use sdp_print_signed_decimal(). */
uint64_t
sdp_mitem_get_value_check(const struct sdparm_mp_item_t *mpi,
                          const uint8_t * mp, bool * all_setp)
{
    uint64_t res;

    res = sg_get_big_endian(mp + mpi->start_byte, mpi->start_bit,
                             mpi->num_bits);
    if (all_setp) {
        switch (mpi->num_bits) {
        case 8:
            if (0xff == res)
                *all_setp = true;
            break;
        case 16:
            if (0xffff == res)
                *all_setp = true;
            break;
        case 24:
            if (0xffffff == res)
                *all_setp = true;
            break;
        case 32:
            if (0xffffffff == res)
                *all_setp = true;
            break;
        case 48:
            if (0xffffffffffffULL == res)
                *all_setp = true;
            break;
        case 64:
            if (0xffffffffffffffffULL == res)
                *all_setp = true;
            break;
        default:
            *all_setp = false;
            break;
        }
    }
    return res;
}

char *
sdp_signed_decimal_str(uint64_t u, int num_bits, bool leading_zeros,
                       char * b, int blen)
{
    unsigned int ui;
    uint8_t uc;

    switch (num_bits) {
    /* could support other num_bits, as required */
    case 4:     /* -8 to 7 */
        uc = u & 0xf;
        if (0x8 & uc)
            uc |= 0xf0;         /* sign extend */
        if (leading_zeros)
            sg_scnpr(b, blen, "%02hhd", (signed char)uc);
        else
            sg_scnpr(b, blen, "%hhd", (signed char)uc);
        break;
    case 8:     /* -128 to 127 */
        if (leading_zeros)
            sg_scnpr(b, blen, "%02hhd", (signed char)u);
        else
            sg_scnpr(b, blen, "%hhd", (signed char)u);
        break;
    case 16:    /* -32768 to 32767 */
        if (leading_zeros)
            sg_scnpr(b, blen, "%02hd", (short int)u);
        else
            sg_scnpr(b, blen, "%hd", (short int)u);
        break;
    case 24:
        ui = 0xffffff & u;
        if (0x800000 & ui)
            ui |= 0xff000000;
        if (leading_zeros)
            sg_scnpr(b, blen, "%02d", (int)ui);
        else
            sg_scnpr(b, blen, "%d", (int)ui);
        break;
    case 32:
        if (leading_zeros)
            sg_scnpr(b, blen, "%02d", (int)u);
        else
            sg_scnpr(b, blen, "%d", (int)u);
        break;
    case 64:
    default:
        if (leading_zeros)
            sg_scnpr(b, blen, "%02" PRId64 , (int64_t)u);
        else
            sg_scnpr(b, blen, "%" PRId64 , (int64_t)u);
        break;
    }
    return b;
}

/* Place 'val' at an offset to 'mp' as indicated by mpi. */
void
sdp_mitem_set_value(uint64_t val, const struct sdparm_mp_item_t * mpi,
                    uint8_t * mp)
{
    sg_set_big_endian(val, mp + mpi->start_byte, mpi->start_bit,
                      mpi->num_bits);
}

int
sdp_get_desc_id(int flags)
{
    return (MF_DESC_ID_MASK & flags) >> MF_DESC_ID_SHIFT;
}

/* Remove up to two parenthesised and two square bracketed expressions.
 * Append 'mode page' to what is left, then convert to snake case. */
char *
sdp_mp_convert2snake(const char * in_name, char * sn_name,
                     int max_sn_name_len)
{
    int inlen, o_inlen, k, m;
    const char * lpar;
    const char * rpar;
    const char * lbra;
    const char * rbra;
    char b[168];
    static const int blen = sizeof(b);
    static const char * dummy_in_s = "null mode page";
    static const char * ump_s = "_mode page";

    inlen = strlen(in_name ? in_name : dummy_in_s);
    o_inlen = inlen;
    if (inlen >= (blen - 1)) {
        static const char * bts_s = "buffer too short";

        pr2serr("%s: %s\n", __func__, bts_s);
        return sgj_convert2snake(bts_s, sn_name, max_sn_name_len);
    }
    memcpy(b, in_name ? in_name : dummy_in_s,
           inlen + 1);  /* want trailing null */
    for (m = 0; m < 2; ++m) {
        if (m > 0) {
            if (inlen == o_inlen)
                break;
        }
        lpar = strchr(b, '(');
        if (lpar) {
            rpar = strchr(b, ')');
            if (rpar) {         /* remove parentheses */
                memmove((char *)lpar, rpar + 1, rpar + 1 - lpar);
                inlen = strlen(b);
            }
        }
        lbra = strchr(b, '[');
        if (lbra) {
            rbra = strchr(b, ']');
            if (rbra) {         /* remove square brackets */
                memmove((char *)lbra, rbra + 1, rbra + 1 - lbra);
                inlen = strlen(b);
            }
        }
    }
    /* append 'mode page' */
    for (k = inlen; k < blen; ++k) {
        b[k] = ump_s[k - inlen];
        if ('\0' == b[k])
            break;
    }
    if (k == blen)
        b[k - 1] = '\0';
    return sgj_convert2snake(b, sn_name, max_sn_name_len);
}

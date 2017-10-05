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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>

#include "sg_lib.h"
#include "sg_cmds_basic.h"
#include "sg_unaligned.h"
#include "sdparm.h"
#include "sg_pr2serr.h"

/* sdparm_vpd.c : does mainly VPD page processing associated with the
 * INQUIRY SCSI command.
 */

/* VPD_DEVICE_ID  0x83 */
/* Prints outs an abridged set of device identification designators
   selected by association, designator type and/or code set. */
static int
decode_dev_ids_quiet(unsigned char * buff, int len, int m_assoc,
                     int m_desig_type, int m_code_set)
{
    int m, p_id, c_set, assoc, desig_type, i_len, naa, off, u, rtp;
    bool piv, is_sas;
    const unsigned char * bp;
    const unsigned char * ip;
    unsigned char sas_tport_addr[8];

    rtp = 0;
    memset(sas_tport_addr, 0, sizeof(sas_tport_addr));
    off = -1;
    while ((u = sg_vpd_dev_id_iter(buff, len, &off, m_assoc, m_desig_type,
                                   m_code_set)) == 0) {
        bp = buff + off;
        i_len = bp[3];
        if ((off + i_len + 4) > len) {
            pr2serr("    VPD page error: designator length longer than\n"
                    "     remaining response length=%d\n", (len - off));
            return SG_LIB_CAT_MALFORMED;
        }
        ip = bp + 4;
        p_id = ((bp[0] >> 4) & 0xf);
        c_set = (bp[0] & 0xf);
        piv = !!(bp[1] & 0x80);
        is_sas = (piv && (6 == p_id));
        assoc = ((bp[1] >> 4) & 0x3);
        desig_type = (bp[1] & 0xf);
        switch (desig_type) {
        case 0: /* vendor specific */
            break;
        case 1: /* T10 vendor identification */
            break;
        case 2: /* EUI-64 based */
            if ((8 != i_len) && (12 != i_len) && (16 != i_len))
                pr2serr("      << expect 8, 12 and 16 byte EUI, got %d >>\n",
                        i_len);
            printf("0x");
            for (m = 0; m < i_len; ++m)
                printf("%02x", (unsigned int)ip[m]);
            printf("\n");
            break;
        case 3: /* NAA <n> */
            naa = (ip[0] >> 4) & 0xff;
            if (1 != c_set) {
                pr2serr("      << unexpected code set %d for NAA=%d >>\n",
                        c_set, naa);
                dStrHexErr((const char *)ip, i_len, 0);
                break;
            }
            switch (naa) {
            case 2:     /* NAA 2: IEEE Extended */
                if (8 != i_len) {
                    pr2serr("      << unexpected NAA 2 identifier length: "
                            "0x%x >>\n", i_len);
                    dStrHexErr((const char *)ip, i_len, 0);
                    break;
                }
                printf("0x");
                for (m = 0; m < 8; ++m)
                    printf("%02x", (unsigned int)ip[m]);
                printf("\n");
                break;
            case 3:     /* NAA 3: Locally assigned */
                if (8 != i_len) {
                    pr2serr("      << unexpected NAA 3 identifier "
                            "length: 0x%x >>\n", i_len);
                    dStrHexErr((const char *)ip, i_len, 0);
                    break;
                }
                printf("0x");
                for (m = 0; m < 8; ++m)
                    printf("%02x", (unsigned int)ip[m]);
                printf("\n");
                break;
            case 5:     /* NAA 5: IEEE Registered */
                if (8 != i_len) {
                    pr2serr("      << unexpected NAA 5 identifier "
                            "length: 0x%x >>\n", i_len);
                    dStrHexErr((const char *)ip, i_len, 0);
                    break;
                }
                if ((! is_sas) || (1 != assoc)) {
                    printf("0x");
                    for (m = 0; m < 8; ++m)
                        printf("%02x", (unsigned int)ip[m]);
                    printf("\n");
                } else if (rtp) {
                    printf("0x");
                    for (m = 0; m < 8; ++m)
                        printf("%02x", (unsigned int)ip[m]);
                    printf(",0x%x\n", rtp);
                    rtp = 0;
                } else {
                    if (sas_tport_addr[0]) {
                        printf("0x");
                        for (m = 0; m < 8; ++m)
                            printf("%02x", (unsigned int)sas_tport_addr[m]);
                        printf("\n");
                    }
                    memcpy(sas_tport_addr, ip, sizeof(sas_tport_addr));
                }
                break;
            case 6:     /* NAA 5: IEEE Registered Extended */
                if (16 != i_len) {
                    pr2serr("      << unexpected NAA 6 identifier length: "
                            "0x%x >>\n", i_len);
                    dStrHexErr((const char *)ip, i_len, 0);
                    break;
                }
                printf("0x");
                for (m = 0; m < 16; ++m)
                    printf("%02x", (unsigned int)ip[m]);
                printf("\n");
                break;
            default:
                pr2serr("      << expected NAA nibble of 2, 3, 5 or 6, got "
                        "%d >>\n", naa);
                dStrHexErr((const char *)ip, i_len, 0);
                break;
            }
            break;
        case 4: /* Relative target port */
            if ((! is_sas) || (1 != c_set) || (1 != assoc) || (4 != i_len))
                break;
            rtp = sg_get_unaligned_be16(ip + 2);
            if (sas_tport_addr[0]) {
                printf("0x");
                for (m = 0; m < 8; ++m)
                    printf("%02x", (unsigned int)sas_tport_addr[m]);
                printf(",0x%x\n", rtp);
                memset(sas_tport_addr, 0, sizeof(sas_tport_addr));
                rtp = 0;
            }
            break;
        case 5: /* (primary) Target port group */
            break;
        case 6: /* Logical unit group */
            break;
        case 7: /* MD5 logical unit identifier */
            break;
        case 8: /* SCSI name string */
            if (c_set < 2) {    /* accept ASCII as subset of UTF-8 */
                pr2serr("      << expected UTF-8 code_set >>\n");
                dStrHexErr((const char *)ip, i_len, 0);
                break;
            }
            /* does %s print out UTF-8 ok??
             * Seems to depend on the locale. Looks ok here with my
             * locale setting: en_AU.UTF-8
             */
            printf("%.*s\n", i_len, (const char *)ip);
            break;
        case 9: /* Protocol specific port identifier */
            break;
        case 0xa: /* UUID identifier */
            if ((1 != c_set) || (18 != i_len) || (1 != ((ip[0] >> 4) & 0xf)))
                break;
            for (m = 0; m < 16; ++m) {
                if ((4 == m) || (6 == m) || (8 == m) || (10 == m))
                    printf("-");
                printf("%02x", (unsigned int)ip[2 + m]);
            }
            printf("\n");
            break;
        default: /* reserved */
            break;
        }
    }
    if (sas_tport_addr[0]) {
        printf("0x");
        for (m = 0; m < 8; ++m)
            printf("%02x", (unsigned int)sas_tport_addr[m]);
        printf("\n");
    }
    if (-2 == u) {
        pr2serr("VPD page error: short designator near offset %d\n", off);
        return SG_LIB_CAT_MALFORMED;
    }
    return 0;
}

static void
decode_designation_descriptor(const unsigned char * bp, int i_len,
                              int print_assoc,
                              const struct sdparm_opt_coll * op)
{
    char b[2048];

    sg_get_designation_descriptor_str(NULL, bp, i_len + 4, print_assoc,
                                      op->do_long, sizeof(b), b);
    printf("%s", b);
}

/* VPD_DEVICE_ID  0x83 */
/* Prints outs device identification designators selected by association,
   designator type and/or code set. */
static int
decode_dev_ids(const char * print_if_found, unsigned char * buff, int len,
               int m_assoc, int m_desig_type, int m_code_set,
               const struct sdparm_opt_coll * op)
{
    int i_len, assoc, printed, off, u;
    const unsigned char * bp;

    if (op->do_quiet)
        return decode_dev_ids_quiet(buff, len, m_assoc, m_desig_type,
                                    m_code_set);
    off = -1;
    printed = 0;
    while ((u = sg_vpd_dev_id_iter(buff, len, &off, m_assoc, m_desig_type,
                                   m_code_set)) == 0) {
        bp = buff + off;
        i_len = bp[3];
        if ((off + i_len + 4) > len) {
            pr2serr("    VPD page error: designator length longer than\n"
                    "     remaining response length=%d\n", (len - off));
            return SG_LIB_CAT_MALFORMED;
        }
        assoc = ((bp[1] >> 4) & 0x3);
        if (print_if_found && (0 == printed)) {
            printed = 1;
            printf("  %s:\n", print_if_found);
        }
        if (NULL == print_if_found)
            printf("  %s:\n", sg_get_desig_assoc_str(assoc));
        decode_designation_descriptor(bp, i_len, 0, op);
    }
    if (-2 == u) {
        pr2serr("VPD page error: short designator around offset %d\n", off);
        return SG_LIB_CAT_MALFORMED;
    }
    return 0;
}

/* VPD_MODE_PG_POLICY   0x87 */
static int
decode_mode_policy_vpd(unsigned char * buff, int len)
{
    int k, bump;
    unsigned char * bp;

    if (len < 4) {
        pr2serr("Mode page policy VPD page length too short=%d\n", len);
        return SG_LIB_CAT_MALFORMED;
    }
    len -= 4;
    bp = buff + 4;
    for (k = 0; k < len; k += bump, bp += bump) {
        bump = 4;
        if ((k + bump) > len) {
            pr2serr("Mode page policy VPD page, short descriptor length=%d, "
                    "left=%d\n", bump, (len - k));
            return SG_LIB_CAT_MALFORMED;
        }
        printf("  Policy page code: 0x%x", (bp[0] & 0x3f));
        if (bp[1])
            printf(",  subpage code: 0x%x\n", bp[1]);
        else
            printf("\n");
        printf("    MLUS=%d,  Policy: %s\n", !!(bp[2] & 0x80),
               sdparm_mode_page_policy_arr[bp[2] & 0x3]);
    }
    return 0;
}

/* VPD_DEVICE_CONSTITUENTS  0x8b */
static int
decode_dev_const_vpd(unsigned char * buff, int len)
{
    int k, j, bump, cd_len;
    unsigned char * bp;

    if (len < 4) {
        pr2serr("Device constituents VPD page length too short=%d\n",
                len);
        return SG_LIB_CAT_MALFORMED;
    }
    len -= 4;
    bp = buff + 4;
    for (k = 0, j = 0; k < len; k += bump, bp += bump, ++j) {
        printf("  Constituent descriptor %d:\n", j + 1);
        if ((k + 36) > len) {
            pr2serr("Device constituents VPD page, short descriptor "
                    "length=36, left=%d\n", (len - k));
            return SG_LIB_CAT_MALFORMED;
        }
        printf("    Constituent type: 0x%x\n",
               sg_get_unaligned_be16(bp + 0));
        printf("    Constituent device type: 0x%x\n", bp[2]);
        printf("    Vendor_identification: %.8s\n", bp + 4);
        printf("    Product_identification: %.16s\n", bp + 12);
        printf("    Product_revision_level: %.4s\n", bp + 28);
        cd_len = sg_get_unaligned_be16(bp + 34);
        bump = 36 + cd_len;
        if ((k + bump) > len) {
            pr2serr("Device constituents VPD page, short descriptor "
                    "length=%d, left=%d\n", bump, (len - k));
            return SG_LIB_CAT_MALFORMED;
        }
        if (cd_len > 0) {
            printf("   Constituent specific descriptor list (in hex):\n");
            dStrHex((const char *)(bp + 36), cd_len, 1);
        }
    }
    return 0;
}

/* VPD_MAN_NET_ADDR  0x85 */
static int
decode_man_net_vpd(unsigned char * buff, int len)
{
    int k, bump, na_len;
    unsigned char * bp;

    if (len < 4) {
        pr2serr("Management network addresses VPD page length too short=%d\n",
                len);
        return SG_LIB_CAT_MALFORMED;
    }
    len -= 4;
    bp = buff + 4;
    for (k = 0; k < len; k += bump, bp += bump) {
        printf("  %s, Service type: %s\n",
               sg_get_desig_assoc_str((bp[0] >> 5) & 0x3),
               sdparm_network_service_type_arr[bp[0] & 0x1f]);
        na_len = sg_get_unaligned_be16(bp + 2);
        bump = 4 + na_len;
        if ((k + bump) > len) {
            pr2serr("Management network addresses VPD page, short descriptor "
                    "length=%d, left=%d\n", bump, (len - k));
            return SG_LIB_CAT_MALFORMED;
        }
        if (na_len > 0) {
            printf("    %s\n", bp + 4);
        }
    }
    return 0;
}

/* This is xcopy(LID4) related: "ROD" == Representation Of Data
 * Used by VPD_3PARTY_COPY */
static void
decode_rod_descriptor(const unsigned char * buff, int len)
{
    const unsigned char * bp = buff;
    int k, bump;

    for (k = 0; k < len; k += bump, bp += bump) {
        bump = sg_get_unaligned_be16(bp + 2) + 4;
        switch (bp[0]) {
            case 0:
                /* Block ROD device type specific descriptor */
                printf("  Optimal block ROD length granularity: %d\n",
                       sg_get_unaligned_be16(bp + 6));
                printf("  Maximum Bytes in block ROD: %" PRIu64 "\n",
                       sg_get_unaligned_be64(bp + 8));
                printf("  Optimal Bytes in block ROD transfer: %" PRIu64 "\n",
                       sg_get_unaligned_be64(bp + 16));
                printf("  Optimal Bytes to token per segment: %" PRIu64 "\n",
                       sg_get_unaligned_be64(bp + 24));
                printf("  Optimal Bytes from token per segment:"
                       " %" PRIu64 "\n", sg_get_unaligned_be64(bp + 32));
                break;
            case 1:
                /* Stream ROD device type specific descriptor */
                printf("  Maximum Bytes in stream ROD: %" PRIu64 "\n",
                       sg_get_unaligned_be64(bp + 8));
                printf("  Optimal Bytes in stream ROD transfer:"
                       " %" PRIu64 "\n", sg_get_unaligned_be64(bp + 16));
                break;
            case 3:
                /* Copy manager ROD device type specific descriptor */
                printf("  Maximum Bytes in processor ROD: %" PRIu64 "\n",
                       sg_get_unaligned_be64(bp + 8));
                printf("  Optimal Bytes in processor ROD transfer:"
                       " %" PRIu64 "\n", sg_get_unaligned_be64(bp + 16));
                break;
            default:
                printf("  Unhandled descriptor (format %d, device type %d)\n",
                       bp[0] >> 5, bp[0] & 0x1F);
                break;
        }
    }
}

struct tpc_desc_type {
    unsigned char code;
    const char * name;
};

struct tpc_desc_type tpc_desc_arr[] = {
    {0x0, "block -> stream"},
    {0x1, "stream -> block"},
    {0x2, "block -> block"},
    {0x3, "stream -> stream"},
    {0x4, "inline -> stream"},
    {0x5, "embedded -> stream"},
    {0x6, "stream -> discard"},
    {0x7, "verify CSCD"},
    {0x8, "block<o> -> stream"},
    {0x9, "stream -> block<o>"},
    {0xa, "block<o> -> block<o>"},
    {0xb, "block -> stream & application_client"},
    {0xc, "stream -> block & application_client"},
    {0xd, "block -> block & application_client"},
    {0xe, "stream -> stream&application_client"},
    {0xf, "stream -> discard&application_client"},
    {0x10, "filemark -> tape"},
    {0x11, "space -> tape"},            /* obsolete: spc5r02 */
    {0x12, "locate -> tape"},           /* obsolete: spc5r02 */
    {0x13, "<i>tape -> <i>tape"},
    {0x14, "register persistent reservation key"},
    {0x15, "third party persistent reservation source I_T nexus"},
    {0x16, "<i>block -> <i>block"},
    {0x17, "positioning -> tape"},      /* this and next added spc5r02 */
    {0x18, "<loi>tape -> <loi>tape"},   /* loi: logical object identifier */
    {0xbe, "ROD <- block range(n)"},
    {0xbf, "ROD <- block range(1)"},
    {0xe0, "CSCD: FC N_Port_Name"},
    {0xe1, "CSCD: FC N_Port_ID"},
    {0xe2, "CSCD: FC N_Port_ID with N_Port_Name, checking"},
    {0xe3, "CSCD: Parallel interface: I_T"},
    {0xe4, "CSCD: Identification Descriptor"},
    {0xe5, "CSCD: IPv4"},
    {0xe6, "CSCD: Alias"},
    {0xe7, "CSCD: RDMA"},
    {0xe8, "CSCD: IEEE 1394 EUI-64"},
    {0xe9, "CSCD: SAS SSP"},
    {0xea, "CSCD: IPv6"},
    {0xeb, "CSCD: IP copy service"},
    {0xfe, "CSCD: ROD"},
    {0xff, "CSCD: extension"},
    {0x0, NULL},
};

static const char *
get_tpc_desc_name(unsigned char code)
{
    const struct tpc_desc_type * dtp;

    for (dtp = tpc_desc_arr; dtp->name; ++dtp) {
        if (code == dtp->code)
            return dtp->name;
    }
    return "";
}

struct tpc_rod_type {
    uint32_t type;
    const char * name;
};

static struct tpc_rod_type tpc_rod_arr[] = {
    {0x0, "copy manager internal"},
    {0x10000, "access upon reference"},
    {0x800000, "point in time copy - default"},
    {0x800001, "point in time copy - change vulnerable"},
    {0x800002, "point in time copy - persistent"},
    {0x80ffff, "point in time copy - any"},
    {0xffff0001, "block device zero"},
    {0x0, NULL},
};

static const char *
get_tpc_rod_name(uint32_t rod_type)
{
    const struct tpc_rod_type * rtp;

    for (rtp = tpc_rod_arr; rtp->name; ++rtp) {
        if (rod_type == rtp->type)
            return rtp->name;
    }
    return "";
}

struct cscd_desc_id_t {
    uint16_t id;
    const char * name;
};

static struct cscd_desc_id_t cscd_desc_id_arr[] = {
    /* only values higher than 0x7ff are listed */
    {0xc000, "copy src or dst null LU, pdt=0"},
    {0xc001, "copy src or dst null LU, pdt=1"},
    {0xf800, "copy src or dst in ROD token"},
    {0xffff, "copy src or dst is copy manager LU"},
    {0x0, NULL},
};

static const char *
get_cscd_desc_id_name(uint16_t cscd_desc_id)
{
    const struct cscd_desc_id_t * cdip;

    for (cdip = cscd_desc_id_arr; cdip->name; ++cdip) {
        if (cscd_desc_id == cdip->id)
            return cdip->name;
    }
    return "";
}


/* VPD_3PARTY_COPY (3PC, THIRD PARTY COPY, SPC-4, SBC-3)  0x8f */
static void
decode_3party_copy_vpd(unsigned char * buff, int len, int do_hex, int pdt,
                       int verbose)
{
    int j, k, m, bump, desc_type, desc_len, sa_len;
    unsigned int u;
    const unsigned char * bp;
    const char * cp;
    uint64_t ull;
    char b[80];

    if (len < 4) {
        pr2serr("Third-party Copy VPD page length too short=%d\n", len);
        return;
    }
    len -= 4;
    bp = buff + 4;
    for (k = 0; k < len; k += bump, bp += bump) {
        desc_type = sg_get_unaligned_be16(bp);
        desc_len = sg_get_unaligned_be16(bp + 2);
        if (verbose)
            printf("Descriptor type=%d, len %d\n", desc_type, desc_len);
        bump = 4 + desc_len;
        if ((k + bump) > len) {
            pr2serr("Third-party Copy VPD page, short descriptor length=%d, "
                    "left=%d\n", bump, (len - k));
            return;
        }
        if (0 == desc_len)
            continue;
        if (2 == do_hex)
            dStrHex((const char *)bp + 4, desc_len, 1);
        else if (do_hex > 2)
            dStrHex((const char *)bp, bump, 1);
        else {
            int csll;

            switch (desc_type) {
            case 0x0000:    /* Required if POPULATE TOKEN (or friend) used */
                printf(" Block Device ROD Token Limits:\n");
                u = sg_get_unaligned_be16(bp + 10);
                printf("  Maximum range descriptors: ");
                if (0 == u)
                    printf("0 [not reported]\n");
                else
                    printf("%u\n", u);
                u = sg_get_unaligned_be32(bp + 12);
                printf("  Maximum inactivity timeout: ");
                if (0 == u)
                    printf("0 [not reported]\n");
                else if (SG_LIB_UNBOUNDED_32BIT == u)
                    printf("-1 [no maximum given]\n");
                else
                    printf("%u seconds\n", u);
                u = sg_get_unaligned_be32(bp + 16);
                printf("  Default inactivity timeout: ");
                if (0 == u)
                    printf("0 [not reported]\n");
                else
                    printf("%u seconds\n", u);
                ull = sg_get_unaligned_be64(bp + 20);
                printf("  Maximum token transfer size: ");
                if (0 == ull)
                    printf("0 [not reported]\n");
                else
                    printf("%" PRIu64 "\n", ull);
                ull = sg_get_unaligned_be64(bp + 28);
                printf("  Optimal transfer count: ");
                if (0 == ull)
                    printf("0 [not reported]\n");
                else
                    printf("%" PRIu64 "\n", ull);
                break;
            case 0x0001:    /* Mandatory (SPC-4) */
                printf(" Supported Commands:\n");
                j = 0;
                csll = bp[4];
                if (csll >= desc_len) {
                    pr2serr("Command supported list length (%d) >= "
                            "descriptor length (%d), wrong so trim\n",
                            csll, desc_len);
                    csll = desc_len - 1;
                }
                while (j < csll) {
                    sa_len = bp[6 + j];
                    for (m = 0; (m < sa_len) && ((j + m) < csll); ++m) {
                        sg_get_opcode_sa_name(bp[5 + j], bp[7 + j + m],
                                              pdt, sizeof(b), b);
                        printf("  %s\n", b);
                    }
                    if (0 == sa_len) {
                        sg_get_opcode_name(bp[5 + j], pdt, sizeof(b), b);
                        printf("  %s\n",  b);
                    } else if (m < sa_len)
                        pr2serr("Supported service actions list length (%d) "
                                "is too large\n", sa_len);
                    j += m + 2;
                }
                break;
            case 0x0004:
                printf(" Parameter data:\n");
                printf("  Maximum CSCD descriptor count: %d\n",
                       sg_get_unaligned_be16(bp + 8));
                printf("  Maximum segment descriptor count: %d\n",
                       sg_get_unaligned_be16(bp + 10));;
                u = sg_get_unaligned_be32(bp + 12);
                printf("  Maximum descriptor list length: %u\n", u);
                u = sg_get_unaligned_be32(bp + 16);
                printf("  Maximum inline data length: %u\n", u);
                break;
            case 0x0008:
                printf(" Supported descriptors:\n");
                for (j = 0; j < bp[4]; j++) {
                    cp = get_tpc_desc_name(bp[5 + j]);
                    if (strlen(cp) > 0)
                        printf("  %s [0x%x]\n", cp, bp[5 + j]);
                    else
                        printf("  0x%x\n", bp[5 + j]);
                }
                break;
            case 0x000C:
                printf(" Supported CSCD IDs (above 0x7ff):\n");
                for (j = 0; j < sg_get_unaligned_be16(bp + 4); j += 2) {
                    u = sg_get_unaligned_be16(bp + 6 + j);
                    cp = get_cscd_desc_id_name(u);
                    if (strlen(cp) > 0)
                        printf("  %s [0x%04x]\n", cp, u);
                    else
                        printf("  0x%04x\n", u);
                }
                break;
            case 0x0106:
                printf(" ROD token features:\n");
                printf("  Remote tokens: %d\n", bp[4] & 0x0f);
                u = sg_get_unaligned_be32(bp + 16);
                printf("  Minimum token lifetime: %u seconds\n", u);
                u = sg_get_unaligned_be32(bp + 20);
                printf("  Maximum token lifetime: %u seconds\n", u);
                u = sg_get_unaligned_be32(bp + 24);
                printf("  Maximum token inactivity timeout: %u\n", u);
                decode_rod_descriptor(bp + 48,
                                      sg_get_unaligned_be16(bp + 46));
                break;
            case 0x0108:
                printf(" Supported ROD token and ROD types:\n");
                for (j = 0; j < sg_get_unaligned_be16(bp + 6); j+= 64) {
                    u = sg_get_unaligned_be32(bp + 8 + j);
                    cp = get_tpc_rod_name(u);
                    if (strlen(cp) > 0)
                        printf("  ROD type: %s [0x%x]\n", cp, u);
                    else
                        printf("  ROD type: 0x%x\n", u);
                    printf("    Internal: %s\n",
                           (bp[8 + j + 4] & 0x80) ? "yes" : "no");
                    printf("    Token in: %s\n",
                           (bp[8 + j + 4] & 0x02) ? "yes" : "no");
                    printf("    Token out: %s\n",
                           (bp[8 + j + 4] & 0x01) ? "yes" : "no");
                    printf("    preference: %d\n",
                           sg_get_unaligned_be16(bp + 8 + j + 6));
                }
                break;
            case 0x8001:    /* Mandatory (SPC-4) */
                printf(" General copy operations:\n");
                u = sg_get_unaligned_be32(bp + 4);
                printf("  Total concurrent copies: %u\n", u);
                u = sg_get_unaligned_be32(bp + 8);
                printf("  Maximum identified concurrent copies: %u\n", u);
                u = sg_get_unaligned_be32(bp + 12);
                printf("  Maximum segment length: %u\n", u);
                ull = (1 << bp[16]);   /* field is power of 2 */
                printf("  Data segment granularity: %" PRIu64 "\n", ull);
                ull = (1 << bp[17]);
                printf("  Inline data granularity: %" PRIu64 "\n", ull);
                break;
            case 0x9101:
                printf(" Stream copy operations:\n");
                u = sg_get_unaligned_be32(bp + 4);
                printf("  Maximum stream device transfer size: %u\n", u);
                break;
            case 0xC001:
                printf(" Held data:\n");
                u = sg_get_unaligned_be32(bp + 4);
                printf("  Held data limit: %u\n", u);
                ull = (1 << bp[8]);
                printf("  Held data granularity: %" PRIu64 "\n", ull);
                break;
            default:
                pr2serr("Unexpected type=%d\n", desc_type);
                dStrHexErr((const char *)bp, bump, 1);
                break;
            }
        }
    }
}


/* VPD_PROTO_LU  0x90 */
static int
decode_proto_lu_vpd(unsigned char * buff, int len)
{
    int k, bump, rel_port, desc_len, proto;
    unsigned char * bp;

    if (len < 4) {
        pr2serr("Protocol-specific logical unit information VPD page length "
                "too short=%d\n", len);
        return SG_LIB_CAT_MALFORMED;
    }
    len -= 4;
    bp = buff + 4;
    for (k = 0; k < len; k += bump, bp += bump) {
        rel_port = sg_get_unaligned_be16(bp);
        printf("Relative port=%d\n", rel_port);
        proto = bp[2] & 0xf;
        desc_len = sg_get_unaligned_be16(bp + 6);
        bump = 8 + desc_len;
        if ((k + bump) > len) {
            pr2serr("Protocol-specific logical unit information VPD page, "
                    "short descriptor length=%d, left=%d\n", bump, (len - k));
            return SG_LIB_CAT_MALFORMED;
        }
        if (desc_len > 0) {
            switch (proto) {
            case TPROTO_SAS:
                printf(" Protocol identifier: SAS\n");
                printf(" TLR control supported: %d\n", !!(bp[8] & 0x1));
                break;
            default:
                pr2serr("Unexpected proto=%d\n", proto);
                dStrHexErr((const char *)bp, bump, 1);
                break;
            }
        }
    }
    return 0;
}

/* VPD_PROTO_PORT  0x91 */
static int
decode_proto_port_vpd(unsigned char * buff, int len)
{
    int k, j, bump, rel_port, desc_len, proto;
    unsigned char * bp;
    unsigned char * pidp;

    if (len < 4) {
        pr2serr("Protocol-specific port information VPD page length too "
                "short=%d\n", len);
        return SG_LIB_CAT_MALFORMED;
    }
    len -= 4;
    bp = buff + 4;
    for (k = 0; k < len; k += bump, bp += bump) {
        rel_port = sg_get_unaligned_be16(bp);
        printf("Relative port=%d\n", rel_port);
        proto = bp[2] & 0xf;
        desc_len = sg_get_unaligned_be16(bp + 6);
        bump = 8 + desc_len;
        if ((k + bump) > len) {
            pr2serr("Protocol-specific port information VPD page, short "
                    "descriptor length=%d, left=%d\n", bump, (len - k));
            return SG_LIB_CAT_MALFORMED;
        }
        if (desc_len > 0) {
            switch (proto) {
            case TPROTO_SAS:    /* page added in spl3r02 */
                printf("    power disable supported (pwr_d_s)=%d\n",
                       !!(bp[3] & 0x1));       /* added spl3r03 */
                pidp = bp + 8;
                for (j = 0; j < desc_len; j += 4, pidp += 4)
                    printf("      phy id=%d, SSP persistent capable=%d\n",
                           pidp[1], (0x1 & pidp[2]));
                break;
            default:
                pr2serr("Unexpected proto=%d\n", proto);
                dStrHexErr((const char *)bp, bump, 1);
                break;
            }
        }
    }
    return 0;
}

/* VPD_SCSI_FEATURE_SETS [0x92] (sfs) */
static int
decode_feature_sets_vpd(unsigned char * buff, int len,
                        const struct sdparm_opt_coll * op)
{
    int k, bump;
    uint16_t sf_code;
    bool found;
    unsigned char * bp;
    char b[64];

    if (len < 4) {
        pr2serr("SCSI Feature sets VPD page length too short=%d\n", len);
        return SG_LIB_CAT_MALFORMED;
    }
    len -= 8;
    bp = buff + 8;
    for (k = 0; k < len; k += bump, bp += bump) {
        sf_code = sg_get_unaligned_be16(bp);
        bump = 2;
        if ((k + bump) > len) {
            pr2serr("SCSI Feature sets, short descriptor length=%d, "
                    "left=%d\n", bump, (len - k));
            return SG_LIB_CAT_MALFORMED;
        }
        printf("    %s", sg_get_sfs_str(sf_code, -2, sizeof(b), b,
               &found, op->verbose));
        if (op->verbose == 1)
            printf(" [0x%x]\n", (unsigned int)sf_code);
        else if (op->verbose > 1)
            printf(" [0x%x] found=%s\n", (unsigned int)sf_code,
                   found ? "true" : "false");
        else
            printf("\n");
    }
    return 0;
}

/* VPD_SCSI_PORTS  0x88 */
static int
decode_scsi_ports_vpd(unsigned char * buff, int len,
                      const struct sdparm_opt_coll * op)
{
    int k, bump, rel_port, ip_tid_len, tpd_len, res;
    unsigned char * bp;

    if (len < 4) {
        pr2serr("SCSI Ports VPD page length too short=%d\n", len);
        return SG_LIB_CAT_MALFORMED;
    }
    len -= 4;
    bp = buff + 4;
    for (k = 0; k < len; k += bump, bp += bump) {
        rel_port = sg_get_unaligned_be16(bp + 2);
        printf("Relative port=%d\n", rel_port);
        ip_tid_len = sg_get_unaligned_be16(bp + 6);
        bump = 8 + ip_tid_len;
        if ((k + bump) > len) {
            pr2serr("SCSI Ports VPD page, short descriptor length=%d, "
                    "left=%d\n", bump, (len - k));
            return SG_LIB_CAT_MALFORMED;
        }
        if (ip_tid_len > 0) {
            char b[1024];

            printf("%s", sg_decode_transportid_str("  ", bp + 8,
                                 ip_tid_len, true, sizeof(b), b));
        }
        tpd_len = sg_get_unaligned_be16(bp + bump + 2);
        if ((k + bump + tpd_len + 4) > len) {
            pr2serr("SCSI Ports VPD page, short descriptor(tgt) length=%d, "
                    "left=%d\n", bump, (len - k));
            return SG_LIB_CAT_MALFORMED;
        }
        if (tpd_len > 0) {
            res = decode_dev_ids(" Target port descriptor(s)",
                                 bp + bump + 4, tpd_len, VPD_ASSOC_TPORT,
                                 -1, -1, op);
            if (res)
                return res;
        }
        bump += tpd_len + 4;
    }
    return 0;
}

/* VPD_EXT_INQ  Extended Inquiry page  0x86 */
static int
decode_ext_inq_vpd(unsigned char * b, int len, int do_long, int protect)
{
    int n;

    if (len < 7) {
        pr2serr("Extended INQUIRY data VPD page length too short=%d\n", len);
        return SG_LIB_CAT_MALFORMED;
    }
    if (do_long) {
        n = (b[4] >> 6) & 0x3;
        printf("  ACTIVATE_MICROCODE=%d", n);
        if (1 == n)
            printf(" [before final WRITE BUFFER]\n");
        else if (2 == n)
            printf(" [after power on or hard reset]\n");
        else
            printf("\n");
        n = (b[4] >> 3) & 0x7;
        printf("  SPT=%d", n);
        if (protect) {
            switch (n)
            {
            case 0:
                printf(" [protection type 1 supported]\n");
                break;
            case 1:
                printf(" [protection types 1 and 2 supported]\n");
                break;
            case 2:
                printf(" [protection type 2 supported]\n");
                break;
            case 3:
                printf(" [protection types 1 and 3 supported]\n");
                break;
            case 4:
                printf(" [protection type 3 supported]\n");
                break;
            case 5:
                printf(" [protection types 2 and 3 supported]\n");
                break;
            case 6:
                printf(" [see Supported block lengths and protection types "
                       "VPD page]\n");
                break;
            case 7:
                printf(" [protection types 1, 2 and 3 supported]\n");
                break;
            default:
                printf("\n");
                break;
            }
        } else
            printf("\n");
        printf("  GRD_CHK=%d\n", !!(b[4] & 0x4));
        printf("  APP_CHK=%d\n", !!(b[4] & 0x2));
        printf("  REF_CHK=%d\n", !!(b[4] & 0x1));
        printf("  UASK_SUP=%d\n", !!(b[5] & 0x20));
        printf("  GROUP_SUP=%d\n", !!(b[5] & 0x10));
        printf("  PRIOR_SUP=%d\n", !!(b[5] & 0x8));
        printf("  HEADSUP=%d\n", !!(b[5] & 0x4));
        printf("  ORDSUP=%d\n", !!(b[5] & 0x2));
        printf("  SIMPSUP=%d\n", !!(b[5] & 0x1));
        printf("  WU_SUP=%d\n", !!(b[6] & 0x8));
        printf("  CRD_SUP=%d\n", !!(b[6] & 0x4));
        printf("  NV_SUP=%d\n", !!(b[6] & 0x2));
        printf("  V_SUP=%d\n", !!(b[6] & 0x1));
        printf("  NO_PI_CHK=%d\n", !!(b[7] & 0x20));    /* spc5r02 */
        printf("  P_I_I_SUP=%d\n", !!(b[7] & 0x10));
        printf("  LUICLR=%d\n", !!(b[7] & 0x1));
        printf("  LU_COLL_TYPE=%d\n", (b[8] >> 5) & 0x7); /* spc5r09 */
        printf("  R_SUP=%d\n", !!(b[8] & 0x10));
        printf("  HSSRELEF=%d\n", !!(b[8] & 0x2));      /* spc5r02 */
        printf("  CBCS=%d\n", !!(b[8] & 0x1));  /* obsolete in spc5r01 */
        printf("  Multi I_T nexus microcode download=%d\n", b[9] & 0xf);
        printf("  Extended self-test completion minutes=%d\n",
               sg_get_unaligned_be16(b + 10));
        printf("  POA_SUP=%d\n", !!(b[12] & 0x80));     /* spc4r32 */
        printf("  HRA_SUP=%d\n", !!(b[12] & 0x40));     /* spc4r32 */
        printf("  VSA_SUP=%d\n", !!(b[12] & 0x20));     /* spc4r32 */
        printf("  Maximum supported sense data length=%d\n",
               b[13]); /* spc4r34 */
        printf("  IBS=%d\n", !!(b[14] & 0x80));     /* spc5r09 */
        printf("  IAS=%d\n", !!(b[14] & 0x40));     /* spc5r09 */
        printf("  SAC=%d\n", !!(b[14] & 0x4));      /* spc5r09 */
        printf("  NRD1=%d\n", !!(b[14] & 0x2));     /* spc5r09 */
        printf("  NRD0=%d\n", !!(b[14] & 0x1));     /* spc5r09 */
        printf("  Maximum inquiry change logs=%u\n",
               sg_get_unaligned_be16(b + 15));      /* spc5r17 */
        printf("  Maximum mode page change logs=%u\n",
               sg_get_unaligned_be16(b + 17));      /* spc5r17 */
    } else {
        printf("  ACTIVATE_MICROCODE=%d SPT=%d GRD_CHK=%d APP_CHK=%d "
               "REF_CHK=%d\n", ((b[4] >> 6) & 0x3), ((b[4] >> 3) & 0x7),
               !!(b[4] & 0x4), !!(b[4] & 0x2), !!(b[4] & 0x1));
        printf("  UASK_SUP=%d GROUP_SUP=%d PRIOR_SUP=%d HEADSUP=%d ORDSUP=%d "
               "SIMPSUP=%d\n", !!(b[5] & 0x20), !!(b[5] & 0x10),
               !!(b[5] & 0x8), !!(b[5] & 0x4), !!(b[5] & 0x2),
               !!(b[5] & 0x1));
        /* CRD_SUP made obsolete in spc5r04 */
        printf("  WU_SUP=%d [CRD_SUP=%d] NV_SUP=%d V_SUP=%d\n",
               !!(b[6] & 0x8), !!(b[6] & 0x4),
               !!(b[6] & 0x2), !!(b[6] & 0x1));
        printf("  NO_PI_CHK=%d P_I_I_SUP=%d LUICLR=%d\n", !!(b[7] & 0x20),
               !!(b[7] & 0x10), !!(b[7] & 0x1));
        /* LU_COLL_TYPE in spc5r09, CBCS obsolete in spc5r01 */
        printf("  LU_COLL_TYPE=%d R_SUP=%d HSSRELEF=%d [CBCS=%d]\n",
               (b[8] >> 5) & 0x7, !!(b[8] & 0x10), !!(b[8] & 0x2),
               !!(b[8] & 0x1));
        printf("  Multi I_T nexus microcode download=%d\n", b[9] & 0xf);
        printf("  Extended self-test completion minutes=%d\n",
               sg_get_unaligned_be16(b + 10));
        printf("  POA_SUP=%d HRA_SUP=%d VSA_SUP=%d\n",      /* spc4r32 */
               !!(b[12] & 0x80), !!(b[12] & 0x40), !!(b[12] & 0x20));
        printf("  Maximum supported sense data length=%d\n",
               b[13]); /* spc4r34 */
        printf("  IBS=%d IAS=%d SAC=%d NRD1=%d NRD0=%d\n", !!(b[14] & 0x80),
               !!(b[14] & 0x40), !!(b[14] & 0x4), !!(b[14] & 0x2),
               !!(b[14] & 0x1));  /* added in spc5r09 */
        printf("  Maximum inquiry change logs=%u\n",
               sg_get_unaligned_be16(b + 15));     /* spc5r17 */
        printf("  Maximum mode page change logs=%u\n",
               sg_get_unaligned_be16(b + 17));     /* spc5r17 */
    }
    return 0;
}

/* VPD_ATA_INFO  0x89 */
static int
decode_ata_info_vpd(unsigned char * buff, int len, int do_long, int do_hex)
{
    char b[80];
    int num, is_be, cc;
    const char * cp;
    const char * ata_transp;

    if (len < 36) {
        pr2serr("ATA information VPD page length too short=%d\n", len);
        return SG_LIB_CAT_MALFORMED;
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
        return SG_LIB_CAT_MALFORMED;
    ata_transp = (0x34 == buff[36]) ? "SATA" : "PATA";
    if (do_long) {
        printf("  Device signature [%s] (in hex):\n", ata_transp);
        dStrHex((const char *)buff + 36, 20, 1);
    } else
        printf("  Device signature indicates %s transport\n", ata_transp);
    cc = buff[56];      /* 0xec for IDENTIFY DEVICE and 0xa1 for IDENTIFY
                         * PACKET DEVICE (obsolete) */
    printf("  Command code: 0x%x\n", cc);
    if (len < 60)
        return SG_LIB_CAT_MALFORMED;
    if (0xec == cc)
        cp = "";
    else if (0xa1 == cc)
        cp = "PACKET ";
    else
        cp = NULL;
    is_be = sg_is_big_endian();
    if (cp) {
        printf("  ATA command IDENTIFY %sDEVICE response summary:\n", cp);
        num = sg_ata_get_chars((const unsigned short *)(buff + 60), 27, 20,
                               is_be, b);
        b[num] = '\0';
        printf("    model: %s\n", b);
        num = sg_ata_get_chars((const unsigned short *)(buff + 60), 10, 10,
                               is_be, b);
        b[num] = '\0';
        printf("    serial number: %s\n", b);
        num = sg_ata_get_chars((const unsigned short *)(buff + 60), 23, 4,
                               is_be, b);
        b[num] = '\0';
        printf("    firmware revision: %s\n", b);
        if (do_long)
            printf("  ATA command IDENTIFY %sDEVICE response in hex:\n", cp);
    } else if (do_long)
        printf("  ATA command 0x%x got following response:\n",
               (unsigned int)buff[56]);
    if (len < 572)
        return SG_LIB_CAT_MALFORMED;
    if (do_hex)
        dStrHex((const char *)(buff + 60), 512, 0);
    else if (do_long)
        dWordHex((const unsigned short *)(buff + 60), 256, 0, is_be);
    return 0;
}

/* VPD_POWER_CONDITION  0x8a */
static int
decode_power_condition(unsigned char * buff, int len)
{
    if (len < 18) {
        pr2serr("Power condition VPD page length too short=%d\n", len);
        return SG_LIB_CAT_MALFORMED;
    }
    printf("  Standby_y=%d Standby_z=%d Idle_c=%d Idle_b=%d Idle_a=%d\n",
           !!(buff[4] & 0x2), !!(buff[4] & 0x1),
           !!(buff[5] & 0x4), !!(buff[5] & 0x2), !!(buff[5] & 0x1));
    printf("  Stopped condition recovery time (ms) %d\n",
           sg_get_unaligned_be16(buff + 6));
    printf("  Standby_z condition recovery time (ms) %d\n",
           sg_get_unaligned_be16(buff + 8));
    printf("  Standby_y condition recovery time (ms) %d\n",
           sg_get_unaligned_be16(buff + 10));
    printf("  Idle_a condition recovery time (ms) %d\n",
           sg_get_unaligned_be16(buff + 12));
    printf("  Idle_b condition recovery time (ms) %d\n",
           sg_get_unaligned_be16(buff + 14));
    printf("  Idle_c condition recovery time (ms) %d\n",
           sg_get_unaligned_be16(buff + 16));
    return 0;
}

static const char * power_unit_arr[] =
{
    "Gigawatts",
    "Megawatts",
    "Kilowatts",
    "Watts",
    "Milliwatts",
    "Microwatts",
    "Unit reserved",
    "Unit reserved",
};

/* VPD_POWER_CONSUMPTION   0x8d */
static int
decode_power_consumption_vpd(unsigned char * buff, int len)
{
    int k, bump;
    unsigned char * bp;
    unsigned int value;

    if (len < 4) {
        pr2serr("Power consumption VPD page length too short=%d\n", len);
        return SG_LIB_CAT_MALFORMED;
    }
    len -= 4;
    bp = buff + 4;
    for (k = 0; k < len; k += bump, bp += bump) {
        bump = 4;
        if ((k + bump) > len) {
            pr2serr("Power consumption VPD page, short descriptor length=%d, "
                    "left=%d\n", bump, (len - k));
            return SG_LIB_CAT_MALFORMED;
        }
        value = sg_get_unaligned_be16(bp + 2);
        printf("  Power consumption identifier: 0x%x", bp[0]);
        if (value >= 1000 && (bp[1] & 0x7) > 0)
            printf("    Maximum power consumption: %d.%03d %s\n",
                   value / 1000, value % 1000,
                   power_unit_arr[(bp[1] & 0x7) - 1]);
        else
            printf("    Maximum power consumption: %d %s\n",
                   value, power_unit_arr[bp[1] & 0x7]);
    }
    return 0;
}

/* VPD_BLOCK_LIMITS  0xb0 */
static int
decode_block_limits_vpd(unsigned char * buff, int len)
{
    unsigned int u;
    uint64_t ull;
    bool ugavalid;

    if (len < 16) {
        pr2serr("Block limits VPD page length too short=%d\n", len);
        return SG_LIB_CAT_MALFORMED;
    }
    printf("  Write same non-zero (WSNZ): %d\n", !!(buff[4] & 0x1));
    u = buff[5];
    printf("  Maximum compare and write length: ");
    if (0 == u)
        printf("0 blocks [Command not implemented]\n");
    else
        printf("%u blocks\n", buff[5]);
    u = sg_get_unaligned_be16(buff + 6);
    printf("  Optimal transfer length granularity: ");
    if (0 == u)
        printf("0 blocks [not reported]\n");
    else
        printf("%u blocks\n", u);
    u = sg_get_unaligned_be32(buff + 8);
    printf("  Maximum transfer length: ");
    if (0 == u)
        printf("0 blocks [not reported]\n");
    else
        printf("%u blocks\n", u);
    u = sg_get_unaligned_be32(buff + 12);
    printf("  Optimal transfer length: ");
    if (0 == u)
        printf("0 blocks [not reported]\n");
    else
        printf("%u blocks\n", u);
    if (len > 19) {     /* added in sbc3r09 */
        u = sg_get_unaligned_be32(buff + 16);
        printf("  Maximum prefetch transfer length: ");
        if (0 == u)
            printf("0 blocks [ignored]\n");
        else
            printf("%u blocks\n", u);
    }
    if (len > 27) {     /* added in sbc3r18 */
        u = sg_get_unaligned_be32(buff + 20);
        printf("  Maximum unmap LBA count: ");
        if (0 == u)
            printf("0 [Unmap command not implemented]\n");
        else if (SG_LIB_UNBOUNDED_32BIT == u)
            printf("-1 [unbounded]\n");
        else
            printf("%u\n", u);
        u = sg_get_unaligned_be32(buff + 24);
        printf("  Maximum unmap block descriptor count: ");
        if (0 == u)
            printf("0 [Unmap command not implemented]\n");
        else if (SG_LIB_UNBOUNDED_32BIT == u)
            printf("-1 [unbounded]\n");
        else
            printf("%u\n", u);
    }
    if (len > 35) {     /* added in sbc3r19 */
        u = sg_get_unaligned_be32(buff + 28);
        printf("  Optimal unmap granularity: ");
        if (0 == u)
            printf("0 blocks [not reported]\n");
        else
            printf("%u blocks\n", u);

        ugavalid = !!(buff[32] & 0x80);
        printf("  Unmap granularity alignment valid: %s\n",
               ugavalid ? "true" : "false");
        u = 0x7fffffff & sg_get_unaligned_be32(buff + 32);
        printf("  Unmap granularity alignment: %u%s\n", u,
               ugavalid ? "" : " [invalid]");
    }
    if (len > 43) {     /* added in sbc3r26 */
        ull = sg_get_unaligned_be64(buff + 36);
        printf("  Maximum write same length: \n");
        if (0 == ull)
            printf("0 blocks [not reported]\n");
        else
            printf("0x%" PRIx64 " blocks\n", ull);
    }
    if (len > 44) {     /* added in sbc4r02 */
        u = sg_get_unaligned_be32(buff + 44);
        printf("  Maximum atomic transfer length: ");
        if (0 == u)
            printf("0 blocks [not reported]\n");
        else
            printf("%u blocks\n", u);
        u = sg_get_unaligned_be32(buff + 48);
        printf("  Atomic alignment: ");
        if (0 == u)
            printf("0 [unaligned atomic writes permitted]\n");
        else
            printf("%u\n", u);
        u = sg_get_unaligned_be32(buff + 52);
        printf("  Atomic transfer length granularity: ");
        if (0 == u)
            printf("0 [no granularity requirement\n");
        else
            printf("%u\n", u);
    }
    if (len > 56) {
        u = sg_get_unaligned_be32(buff + 56);
        printf("  Maximum atomic transfer length with atomic "
               "boundary: ");
        if (0 == u)
            printf("0 blocks [not reported]\n");
        else
            printf("%u blocks\n", u);
        u = sg_get_unaligned_be32(buff + 60);
        printf("  Maximum atomic boundary size: ");
        if (0 == u)
            printf("0 blocks [can only write atomic 1 block]\n");
        else
            printf("%u blocks\n", u);
    }
    return 0;
}

/* VPD_BLOCK_LIMITS_EXT  0xb7 */
static int
decode_block_limits_ext_vpd(unsigned char * buff, int len)
{
    unsigned int u;

    if (len < 12) {
        pr2serr("Block limits extension VPD page length too short=%d\n", len);
        return SG_LIB_CAT_MALFORMED;
    }
    u = sg_get_unaligned_be16(buff + 6);
    printf("  Maximum number of streams: ");
    if (0 == u)
        printf("0 [Stream control not supported]\n");
    else
        printf("%u\n", u);
    u = sg_get_unaligned_be16(buff + 8);
    printf("  Optimal stream write size: %u blocks\n", u);
    u = sg_get_unaligned_be32(buff + 10);
    printf("  Stream granularity size: %u\n", u);
    if (len > 27) {
        u = sg_get_unaligned_be32(buff + 16);
        printf("  Maximum scattered LBA range transfer length: ");
        if (0 == u)
            printf("0 blocks [not reported]\n");
        else
            printf("%u blocks\n", u);
        u = sg_get_unaligned_be16(buff + 22);
        printf("  Maximum scattered LBA range descriptor count: ");
        if (0 == u)
            printf("0 [not reported]\n");
        else
            printf("%u\n", u);
        u = sg_get_unaligned_be32(buff + 24);
        printf("  Maximum scattered transfer length: ");
        if (0 == u)
            printf("0 blocks [not reported]\n");
        else
            printf("%u blocks\n", u);
    }
    return 0;
}

static const char * product_type_arr[] =
{
    "Not specified",
    "CFast",
    "CompactFlash",
    "MemoryStick",
    "MultiMediaCard",
    "Secure Digital Card (SD)",
    "XQD",
    "Universal Flash Storage Card (UFS)",
};

/* VPD_BLOCK_DEV_CHARS  0xb1 */
static int
decode_block_dev_chars_vpd(unsigned char * buff, int len)
{
    unsigned int u, k;

    if (len < 64) {
        pr2serr("Block device capabilities VPD page length too short=%d\n",
                len);
        return SG_LIB_CAT_MALFORMED;
    }
    u = sg_get_unaligned_be16(buff + 4);
    if (0 == u)
        printf("  Medium rotation rate is not reported\n");
    else if (1 == u)
        printf("  Non-rotating medium (e.g. solid state)\n");
    else if ((u < 0x401) || (0xffff == u))
        printf("  Reserved [0x%x]\n", u);
    else
        printf("  Nominal rotation rate: %d rpm\n", u);
    printf("  Product type=%d\n", buff[6]);
    k = sizeof(product_type_arr) / sizeof(product_type_arr[0]);
    if (u < k)
        printf("  Product type: %s\n", product_type_arr[u]);
    else if (u < 0xf0)
        printf("  Product type: Reserved [0x%x]\n", u);
    else
        printf("  Product type: Vendor specific [0x%x]\n", u);
    u = buff[7] & 0xf;
    printf("  WABEREQ=%d\n", (buff[7] >> 6) & 0x3);
    printf("  WACEREQ=%d\n", (buff[7] >> 4) & 0x3);
    printf("  Nominal form factor");
    switch (u) {
    case 0:
        printf(" not reported\n");
        break;
    case 1:
        printf(": 5.25 inch\n");
        break;
    case 2:
        printf(": 3.5 inch\n");
        break;
    case 3:
        printf(": 2.5 inch\n");
        break;
    case 4:
        printf(": 1.8 inch\n");
        break;
    case 5:
        printf(": less then 1.8 inch\n");
        break;
    default:
        printf(": reserved\n");
        break;
    }
    printf("  ZONED=%d\n", (buff[8] >> 4) & 0x3);       /* sbc4r04 */
    printf("  BOCS=%d\n", !!(buff[8] & 0x4));
    printf("  FUAB=%d\n", !!(buff[8] & 0x2));
    printf("  VBULS=%d\n", !!(buff[8] & 0x1));
    printf("  DEPOPULATION_TIME=%u (seconds)\n",
           sg_get_unaligned_be32(buff + 12));           /* added sbc4r14 */
    return 0;
}

/* VPD_SA_DEV_CAP  0xb0 */
static int
decode_tape_dev_caps_vpd(unsigned char * buff, int len)
{
    if (len < 6) {
        pr2serr("Sequential access device capabilities VPD page length too "
                "short=%d\n", len);
        return SG_LIB_CAT_MALFORMED;
    }
    printf("  Worm: %d\n", !!(buff[4] & 0x1));
    return 0;
}

/* VPD_MAN_ASS_SN  0xb1 */
static int
decode_tape_man_ass_sn_vpd(unsigned char * buff, int len)
{
    if (len < 4) {
        pr2serr("Manufacturer-assigned serial number VPD page length too "
                "short=%d\n", len);
        return SG_LIB_CAT_MALFORMED;
    }
    printf("  Manufacturer-assigned serial number: %.*s\n",
                   len - 4, buff + 4);
    return 0;
}

static const char * prov_type_arr[8] = {
    "not known or fully provisioned",
    "resource provisioned",
    "thin provisioned",
    "reserved [0x3]",
    "reserved [0x4]",
    "reserved [0x5]",
    "reserved [0x6]",
    "reserved [0x7]",
};

/* VPD_LB_PROVISIONING  0xb2 */
static int
decode_block_lb_prov_vpd(unsigned char * b, int len,
                         const struct sdparm_opt_coll * op)
{
    int dp, pt;
    unsigned int u;

    if (len < 4) {
        pr2serr("Logical block provisioning page too short=%d\n", len);
        return SG_LIB_CAT_MALFORMED;
    }
    pt = b[6] & 0x7;
    printf("  Unmap command supported (LBPU): %d\n", !!(0x80 & b[5]));
    printf("  Write same (16) with unmap bit supported (LBPWS): %d\n",
           !!(0x40 & b[5]));
    printf("  Write same (10) with unmap bit supported (LBPWS10): %d\n",
           !!(0x20 & b[5]));
    printf("  Logical block provisioning read zeros (LBPRZ): %d\n",
           (0x7 & (b[5] >> 2)));  /* expanded from 1 to 3 bits in sbc4r07 */
    printf("  Anchored LBAs supported (ANC_SUP): %d\n", !!(0x2 & b[5]));
    u = b[4];
    printf("  Threshold exponent: ");
    if (0 == u)
        printf("0 [threshold sets not supported]\n");
    else
        printf("%u\n", u);
    dp = !!(b[5] & 0x1);
    printf("  Descriptor present: %d\n", dp);
    printf("  Minimum percentage: ");
    u = 0x1f & (b[6] >> 3);
    if (0 == u)
        printf("0 [not reported]\n");
    else
        printf("%d\n", u);
    printf("  Provisioning type: %d (%s)\n", pt, prov_type_arr[pt]);
    printf("  Threshold percentage: ");
    if (0 == b[7])
        printf("0 [percentages not supported]\n");
    else
        printf("%u\n", b[7]);
    if (dp) {
        const unsigned char * bp;
        int i_len;

        bp = b + 8;
        i_len = bp[3];
        if (0 == i_len) {
            pr2serr("Logical block provisioning page provisioning group "
                    "descriptor too short=%d\n", i_len);
            return 0;
        }
        printf("  Provisioning group descriptor\n");
        decode_designation_descriptor(bp, i_len, 1, op);
    }
    return 0;
}

/* VPD_TA_SUPPORTED  0xb2 */
static int
decode_tapealert_supported_vpd(unsigned char * b, int len)
{
    int k, mod, div;

    if (len < 12) {
        pr2serr("TapeAlert supported flags length too short=%d\n", len);
        return SG_LIB_CAT_MALFORMED;
    }
    for (k = 1; k < 0x41; ++k) {
        mod = ((k - 1) % 8);
        div = (k - 1) / 8;
        if (0 == mod) {
            if (div > 0)
                printf("\n");
            printf("  Flag%02Xh: %d", k, !! (b[4 + div] & 0x80));
        } else
            printf("  %02Xh: %d", k, !! (b[4 + div] & (1 << (7 - mod))));
    }
    printf("\n");
    return 0;
}

/* VPD_REFERRALS  0xb3 */
static int
decode_referrals_vpd(unsigned char * b, int len)
{
    unsigned int u;

    if (len < 16) {
        pr2serr("Referrals VPD page length too short=%d\n", len);
        return SG_LIB_CAT_MALFORMED;
    }
    u = sg_get_unaligned_be32(b + 8);
    printf("  User data segment size: ");
    if (0 == u)
        printf("0 [per sense descriptor]\n");
    else
        printf("%u\n", u);
    u = sg_get_unaligned_be32(b + 12);
    printf("  User data segment multiplier: %u\n", u);
    return 0;
}

/* VPD_SUP_BLOCK_LENS  0xb4  [added sbc4r01] */
static int
decode_sup_block_lens_vpd(unsigned char * buff, int len)
{
    int k;
    unsigned int u;
    unsigned char * bp;

    if (len < 4) {
        pr2serr("Supported block lengths and protection types VPD page "
                "length too short=%d\n", len);
        return SG_LIB_CAT_MALFORMED;
    }
    len -= 4;
    bp = buff + 4;
    for (k = 0; k < len; k += 8, bp += 8) {
        u = sg_get_unaligned_be32(bp + 0);
        printf("  Logical block length: %u\n", u);
        printf("    P_I_I_SUP: %d\n", !!(bp[4] & 0x40));
        printf("    NO_PI_CHK: %d\n", !!(bp[4] & 0x8));  /* sbc4r05 */
        printf("    GRD_CHK: %d\n", !!(bp[4] & 0x4));
        printf("    APP_CHK: %d\n", !!(bp[4] & 0x2));
        printf("    REF_CHK: %d\n", !!(bp[4] & 0x1));
        printf("    T3PS: %d\n", !!(bp[5] & 0x8));
        printf("    T2PS: %d\n", !!(bp[5] & 0x4));
        printf("    T1PS: %d\n", !!(bp[5] & 0x2));
        printf("    T0PS: %d\n", !!(bp[5] & 0x1));
    }
    return 0;
}

/* VPD_BLOCK_DEV_C_EXTENS  0xb5  [added sbc4r02] */
static int
decode_block_dev_char_ext_vpd(unsigned char * b, int len)
{
    if (len < 16) {
        pr2serr("Block device characteristics extension VPD page "
                "length too short=%d\n", len);
        return SG_LIB_CAT_MALFORMED;
    }
    printf("  Utilization type: ");
    switch (b[5]) {
    case 1:
        printf("Combined writes and reads");
        break;
    case 2:
        printf("Writes only");
        break;
    case 3:
        printf("Separate writes and reads");
        break;
    default:
        printf("Reserved");
        break;
    }
    printf(" [0x%x]\n", b[5]);
    printf("  Utilization units: ");
    switch (b[6]) {
    case 2:
        printf("megabytes");
        break;
    case 3:
        printf("gigabytes");
        break;
    case 4:
        printf("terabytes");
        break;
    case 5:
        printf("petabytes");
        break;
    case 6:
        printf("exabytes");
        break;
    default:
        printf("Reserved");
        break;
    }
    printf(" [0x%x]\n", b[6]);
    printf("  Utilization interval: ");
    switch (b[7]) {
    case 0xa:
        printf("per day");
        break;
    case 0xe:
        printf("per year");
        break;
    default:
        printf("Reserved");
        break;
    }
    printf(" [0x%x]\n", b[7]);
    printf("  Utilization B: %" PRIu32 "\n", sg_get_unaligned_be32(b + 8));
    printf("  Utilization A: %" PRIu32 "\n", sg_get_unaligned_be32(b + 12));
    return 0;
}

/* VPD_LB_PROTECTION_ADDR  0xb5  [added ssc5r02a] */
static int
decode_lb_protection_vpd(unsigned char * buff, int len)
{
    int k, bump;
    unsigned char * bp;

    if (len < 8) {
        pr2serr("Logical block protection VPD page length too short=%d\n",
                len);
        return SG_LIB_CAT_MALFORMED;
    }
    len -= 8;
    bp = buff + 8;
    for (k = 0; k < len; k += bump, bp += bump) {
        bump = 1 + bp[0];
        printf("  method: %d, info_len: %d, LBP_W_C=%d, LBP_R_C=%d, "
               "RBDP_C=%d\n", bp[1], 0x3f & bp[2], !!(0x80 & bp[3]),
               !!(0x40 & bp[3]), !!(0x20 & bp[3]));
        if ((k + bump) > len) {
            pr2serr("Logical block protection VPD page, short descriptor "
                    "length=%d, left=%d\n", bump, (len - k));
            return SG_LIB_CAT_MALFORMED;
        }
    }
    return 0;
}

/* VPD_ZBC_DEV_CHARS  sbc or zbc */
static int
decode_zbdc_vpd(unsigned char * b, int len)
{
    uint32_t u;

    if (len < 64) {
        pr2serr("Zoned block device characteristics VPD page length too "
                "short=%d\n", len);
        return SG_LIB_CAT_MALFORMED;
    }
    printf("  URSWRZ type: %d\n", !!(b[4] & 0x1));
    u = sg_get_unaligned_be32(b + 8);
    printf("  Optimal number of open sequential write preferred zones: ");
    if (SG_LIB_UNBOUNDED_32BIT == u)
        printf("not reported\n");
    else
        printf("%" PRIu32 "\n", u);
    u = sg_get_unaligned_be32(b + 12);
    printf("  Optimal number of non-sequentially written sequential write "
           "preferred zones: ");
    if (SG_LIB_UNBOUNDED_32BIT == u)
        printf("not reported\n");
    else
        printf("%" PRIu32 "\n", u);
    u = sg_get_unaligned_be32(b + 16);
    printf("  Maximum number of open sequential write required zones: ");
    if (SG_LIB_UNBOUNDED_32BIT == u)
        printf("no limit\n");
    else
        printf("%" PRIu32 "\n", u);
    return 0;
}

/* Assume index is less than 16 */
const char * sg_ansi_version_arr[] =
{
    "no conformance claimed",
    "SCSI-1",           /* obsolete, ANSI X3.131-1986 */
    "SCSI-2",           /* obsolete, ANSI X3.131-1994 */
    "SPC",              /* withdrawn, ANSI INCITS 301-1997 */
    "SPC-2",            /* ANSI INCITS 351-2001, ISO/IEC 14776-452 */
    "SPC-3",            /* ANSI INCITS 408-2005, ISO/IEC 14776-453 */
    "SPC-4",            /* ANSI INCITS 513-2015 */
    "SPC-5",
    "ecma=1, [8h]",
    "ecma=1, [9h]",
    "ecma=1, [Ah]",
    "ecma=1, [Bh]",
    "reserved [Ch]",
    "reserved [Dh]",
    "reserved [Eh]",
    "reserved [Fh]",
};

/* Hack: use vpd page=-1 to indicate want standard INQUIRY response */
static int
decode_std_inq(int sg_fd, const struct sdparm_opt_coll * op)
{
    int res, verb, sz, len, pqual;
    unsigned char b[DEF_INQ_RESP_LEN];

    verb = (op->verbose > 0) ? op->verbose - 1 : 0;
    sz = sizeof(b);
    memset(b, 0, sizeof(b));
    sz = op->do_long ? sizeof(b) : 36;
    res = sg_ll_inquiry(sg_fd, 0, 0, 0, b, sz, 0, verb);
    if (res) {
        pr2serr("INQUIRY fetching standar response failed\n");
        return res;
    }
    pqual = (b[0] & 0xe0) >> 5;
    if (0 == pqual)
        printf("standard INQUIRY:\n");
    else if (1 == pqual)
        printf("standard INQUIRY: [qualifier indicates no connected "
               "LU]\n");
    else if (3 == pqual)
        printf("standard INQUIRY: [qualifier indicates not capable "
               "of supporting LU]\n");
    else
        printf("standard INQUIRY: [reserved or vendor specific "
                       "qualifier [%d]]\n", pqual);
    len = b[4] + 5;
    len = (len <= sz) ? len : sz;
    if (op->do_hex) {
        dStrHex((const char *)b, len, 0);
        return 0;
    }
    printf("  PQual=%d  Device_type=%d  RMB=%d  LU_CONG=%d  version=0x%02x ",
           pqual, b[0] & 0x1f, !!(b[1] & 0x80), !!(b[1] & 0x40),
           (unsigned int)b[2]);
    printf(" [%s]\n", sg_ansi_version_arr[b[2] & 0xf]);
    printf("  [AERC=%d]  [TrmTsk=%d]  NormACA=%d  HiSUP=%d "
           " Resp_data_format=%d\n  SCCS=%d  ",
           !!(b[3] & 0x80), !!(b[3] & 0x40), !!(b[3] & 0x20),
           !!(b[3] & 0x10), b[3] & 0x0f, !!(b[5] & 0x80));
    printf("ACC=%d  TPGS=%d  3PC=%d  Protect=%d ",
           !!(b[5] & 0x40), ((b[5] & 0x30) >> 4), !!(b[5] & 0x08),
           !!(b[5] & 0x01));
    printf(" [BQue=%d]\n  EncServ=%d  ", !!(b[6] & 0x80), !!(b[6] & 0x40));
    if (b[6] & 0x10)
        printf("MultiP=1 (VS=%d)  ", !!(b[6] & 0x20));
    else
        printf("MultiP=0  ");
    printf("[MChngr=%d]  [ACKREQQ=%d]  Addr16=%d\n  [RelAdr=%d]  ",
           !!(b[6] & 0x08), !!(b[6] & 0x04), !!(b[6] & 0x01),
           !!(b[7] & 0x80));
    printf("WBus16=%d  Sync=%d  [Linked=%d]  [TranDis=%d]  ",
           !!(b[7] & 0x20), !!(b[7] & 0x10), !!(b[7] & 0x08),
           !!(b[7] & 0x04));
    printf("CmdQue=%d\n", !!(b[7] & 0x02));
    printf("  Vendor_identification: %.8s\n", b + 8);
    printf("  Product_identification: %.16s\n", b + 16);
    printf("  Product_revision_level: %.4s\n", b + 32);
    return 0;
}

/* Returns 0 if successful, else error */
int
sdp_process_vpd_page(int sg_fd, int pn, int spn,
                     const struct sdparm_opt_coll * op, int req_pdt,
                     int protect, const unsigned char * ihbp, int ihb_len)
{
    int res, len, k, verb, dev_pdt, pdt, sz, hex_format;
    bool adc = false;
    bool sbc = false;
    bool ssc = false;
    unsigned char * up;
    const struct sdparm_vpd_page_t * vpp;
    const char * vpd_name;
    unsigned char b[VPD_ATA_INFO_RESP_LEN];
    char c[80];

    verb = (op->verbose > 0) ? op->verbose - 1 : 0;
    hex_format = (op->do_hex > 2) ? -1 : 0;
    sz = sizeof(b);
    memset(b, 0, sz);
    if (ihbp) {         /* response data supplied by user as hex or binary */
        if (ihb_len < sz)
            sz = ihb_len;
        memcpy(b, ihbp, sz);
        pn = b[1];
    } else {            /* need to read from given device */
        if (pn < 0) {
            if (VPD_NOT_STD_INQ == pn)
                return decode_std_inq(sg_fd, op);
            else if (op->do_all)
                pn = VPD_SUPPORTED_VPDS;  /* if '--all' list supported vpds */
            else
                pn = VPD_DEVICE_ID;  /* default to device id page */
        }
        sz = (VPD_ATA_INFO == pn) ? VPD_ATA_INFO_RESP_LEN : DEF_INQ_RESP_LEN;
        res = sg_ll_inquiry(sg_fd, 0, 1, pn, b, sz, 0, verb);
        if (res) {
            pr2serr("INQUIRY fetching VPD page=0x%x failed\n", pn);
            return res;
        }
    }
    dev_pdt = b[0] & 0x1f;
    if ((req_pdt >= 0) && (req_pdt != (dev_pdt))) {
        pr2serr("given peripheral device type [%d] differs from reported "
                "[%d]\n", req_pdt, dev_pdt);
        pr2serr("  start with given pdt\n");
        pdt = req_pdt;
    } else
        pdt = dev_pdt;

    switch (pn) {
    case VPD_SUPPORTED_VPDS:    /* 0x0 */
        if (b[1] != pn)
            goto dumb_inq;
        len = sg_get_unaligned_be16(b + 2);       /* spc4r25 */
        printf("Supported VPD pages VPD page:\n");
        if (op->do_hex) {
            dStrHex((const char *)b, len + 4, hex_format);
            return 0;
        }
        if (len > 0) {
            for (k = 0; k < len; ++k) {
                vpp = sdp_get_vpd_detail(b[4 + k], -1, pdt);
                if (vpp) {
                    if (op->do_long)
                        printf("  [0x%02x] %s [%s]\n", b[4 + k],
                               vpp->name, vpp->acron);
                    else
                        printf("  %s [%s]\n", vpp->name, vpp->acron);
                } else
                    printf("  0x%x\n", b[4 + k]);
            }
        } else
            printf("  <empty>\n");
        break;
    case VPD_ATA_INFO:          /* 0x89 */
        if (b[1] != pn)
            goto dumb_inq;
        len = sg_get_unaligned_be16(b + 2);       /* spc4r25 */
        if (len > sz) {
            pr2serr("Response to ATA information VPD page truncated\n");
            len = sz;
        }
        if (3 == op->do_hex) {   /* output suitable for "hdparm --Istdin" */
            dWordHex((const unsigned short *)(b + 60), 256, -2,
                     sg_is_big_endian());
            return 0;
        }
        if (op->do_long)
            printf("ATA information [0x89] VPD page:\n");
        else
            printf("ATA information VPD page:\n");
        if (op->do_hex && (2 != op->do_hex)) {
            dStrHex((const char *)b, len + 4, 0);
            return 0;
        }
        res = decode_ata_info_vpd(b, len + 4, op->do_long, op->do_hex);
        if (res)
            return res;
        break;
    case VPD_DEVICE_ID:         /* 0x83 */
        if (b[1] != pn)
            goto dumb_inq;
        len = sg_get_unaligned_be16(b + 2);       /* spc4r25 */
        if (len > sz) {
            pr2serr("Response to device identification VPD page truncated\n");
            len = sz;
        }
        if (op->do_long)
            printf("Device identification [0x83] VPD page:\n");
        else if (! op->do_quiet)
            printf("Device identification VPD page:\n");
        if (op->do_hex) {
            dStrHex((const char *)b, len + 4, hex_format);
            return 0;
        }
        res = 0;
        if ((0 == spn) || (VPD_DI_SEL_LU & spn))
            res = decode_dev_ids(sg_get_desig_assoc_str(VPD_ASSOC_LU), b + 4,
                                 len, VPD_ASSOC_LU, -1, -1, op);
        if ((0 == spn) || (VPD_DI_SEL_TPORT & spn))
            res = decode_dev_ids(sg_get_desig_assoc_str(VPD_ASSOC_TPORT),
                                 b + 4, len, VPD_ASSOC_TPORT, -1, -1, op);
        if ((0 == spn) || (VPD_DI_SEL_TARGET & spn))
            res = decode_dev_ids(sg_get_desig_assoc_str(VPD_ASSOC_TDEVICE),
                                 b + 4, len, VPD_ASSOC_TDEVICE, -1, -1, op);
        if (VPD_DI_SEL_AS_IS & spn)
            res = decode_dev_ids(NULL, b + 4, len, -1, -1, -1, op);
        if (res)
            return res;
        break;
    case VPD_EXT_INQ:           /* 0x86 */
        if (b[1] != pn)
            goto dumb_inq;
        len = sg_get_unaligned_be16(b + 2);       /* spc4r25 */
        if (len > sz) {
            pr2serr("Response to Extended inquiry data VPD page truncated\n");
            len = sz;
        }
        if (op->do_long)
            printf("Extended inquiry data [0x86] VPD page:\n");
        else if (! op->do_quiet)
            printf("Extended inquiry data VPD page:\n");
        if (op->do_hex) {
            dStrHex((const char *)b, len + 4, hex_format);
            return 0;
        }
        res = decode_ext_inq_vpd(b, len + 4, op->do_long, protect);
        if (res)
            return res;
        break;
    case VPD_MAN_NET_ADDR:              /* 0x85 */
        if (b[1] != pn)
            goto dumb_inq;
        len = sg_get_unaligned_be16(b + 2);       /* spc4r25 */
        if (len > sz) {
            pr2serr("Response to Management network addresses VPD page "
                    "truncated\n");
            len = sz;
        }
        if (op->do_long)
            printf("Management network addresses [0x85] VPD page:\n");
        else
            printf("Management network addresses VPD page:\n");
        if (op->do_hex) {
            dStrHex((const char *)b, len + 4, hex_format);
            return 0;
        }
        res = decode_man_net_vpd(b, len + 4);
        if (res)
            return res;
        break;
    case VPD_MODE_PG_POLICY:            /* 0x87 */
        if (b[1] != pn)
            goto dumb_inq;
        len = sg_get_unaligned_be16(b + 2);       /* spc4r25 */
        if (len > sz) {
            pr2serr("Response to Mode page policy VPD page truncated\n");
            len = sz;
        }
        if (op->do_long)
            printf("Mode page policy [0x87] VPD page:\n");
        else
            printf("mode page policy VPD page:\n");
        if (op->do_hex) {
            dStrHex((const char *)b, len + 4, hex_format);
            return 0;
        }
        res = decode_mode_policy_vpd(b, len + 4);
        if (res)
            return res;
        break;
    case VPD_POWER_CONDITION:           /* 0x8a */
        if (b[1] != pn)
            goto dumb_inq;
        len = sg_get_unaligned_be16(b + 2);       /* spc4r25 */
        if (len > sz) {
            pr2serr("Response to Power condition VPD page truncated\n");
            len = sz;
        }
        if (op->do_long)
            printf("Power condition [0x8a] VPD page:\n");
        else if (! op->do_quiet)
            printf("Power condition VPD page:\n");
        if (op->do_hex) {
            dStrHex((const char *)b, len + 4, hex_format);
            return 0;
        }
        res = decode_power_condition(b, len + 4);
        if (res)
            return res;
        break;
    case VPD_DEVICE_CONSTITUENTS:       /* 0x8b */
        if (b[1] != pn)
            goto dumb_inq;
        len = sg_get_unaligned_be16(b + 2);
        if (len > sz) {
            pr2serr("Response to Device constituents VPD page truncated\n");
            len = sz;
        }
        if (op->do_long)
            printf("Device constituents [0x8b] VPD page:\n");
        else if (! op->do_quiet)
            printf("Device constituents VPD page:\n");
        if (op->do_hex) {
            dStrHex((const char *)b, len + 4, hex_format);
            return 0;
        }
        res = decode_dev_const_vpd(b, len + 4);
        if (res)
            return res;
        break;
    case VPD_POWER_CONSUMPTION:         /* 0x8d */
        if (b[1] != pn)
            goto dumb_inq;
        len = sg_get_unaligned_be16(b + 2);       /* spc4r25 */
        if (len > sz) {
            pr2serr( "Response to Power consumption VPD page truncated\n");
            len = sz;
        }
        if (op->do_long)
            printf("Power consumption [0x8d] VPD page:\n");
        else
            printf("Power consumption VPD page:\n");
        if (op->do_hex) {
            dStrHex((const char *)b, len + 4, hex_format);
            return 0;
        }
        res = decode_power_consumption_vpd(b, len + 4);
        if (res)
            return res;
        break;
    case VPD_PROTO_LU:                  /* 0x90 */
        if (b[1] != pn)
            goto dumb_inq;
        len = sg_get_unaligned_be16(b + 2);       /* spc4r25 */
        if (len > sz) {
            pr2serr("Response to Protocol-specific logical unit information "
                    "VPD page truncated\n");
            len = sz;
        }
        if (op->do_long)
            printf("Protocol-specific logical unit information [0x90] VPD "
                   "page:\n");
        else
            printf("Protocol-specific logical unit information VPD page:\n");
        if (op->do_hex) {
            dStrHex((const char *)b, len + 4, hex_format);
            return 0;
        }
        res = decode_proto_lu_vpd(b, len + 4);
        if (res)
            return res;
        break;
    case VPD_PROTO_PORT:                /* 0x91 */
        if (b[1] != pn)
            goto dumb_inq;
        len = sg_get_unaligned_be16(b + 2);       /* spc4r25 */
        if (len > sz) {
            pr2serr("Response to Protocol-specific port information VPD page "
                    "truncated\n");
            len = sz;
        }
        if (op->do_long)
            printf("Protocol-specific port information [0x91] VPD "
                   "page:\n");
        else
            printf("Protocol-specific port information VPD page:\n");
        if (op->do_hex) {
            dStrHex((const char *)b, len + 4, hex_format);
            return 0;
        }
        res = decode_proto_port_vpd(b, len + 4);
        if (res)
            return res;
        break;
    case VPD_SCSI_FEATURE_SETS:         /* 0x92 */
        if (b[1] != pn)
            goto dumb_inq;
        len = sg_get_unaligned_be16(b + 2);
        if (len > sz) {
            pr2serr("Response to SCSI Feature sets VPD page truncated\n");
            len = sz;
        }
        if (op->do_long)
            printf("SCSI Feature sets [0x92] VPD page:\n");
        else
            printf("SCSI Feature sets:\n");
        if (op->do_hex) {
            dStrHex((const char *)b, len + 4, hex_format);
            return 0;
        }
        res = decode_feature_sets_vpd(b, len + 4, op);
        if (res)
            return res;
        break;
    case VPD_SCSI_PORTS:                /* 0x88 */
        if (b[1] != pn)
            goto dumb_inq;
        len = sg_get_unaligned_be16(b + 2);       /* spc4r25 */
        if (len > sz) {
            pr2serr("Response to SCSI Ports VPD page truncated\n");
            len = sz;
        }
        if (op->do_long)
            printf("SCSI Ports [0x88] VPD page:\n");
        else
            printf("SCSI Ports VPD page:\n");
        if (op->do_hex) {
            dStrHex((const char *)b, len + 4, hex_format);
            return 0;
        }
        res = decode_scsi_ports_vpd(b, len + 4, op);
        if (res)
            return res;
        break;
    case VPD_SOFTW_INF_ID:              /* 0x84 */
        if (b[1] != pn)
            goto dumb_inq;
        len = sg_get_unaligned_be16(b + 2);       /* spc4r25 */
        if (len > sz) {
            pr2serr("Response to Software interface identification VPD page "
                    "truncated\n");
            len = sz;
        }
        if (op->do_long)
            printf("Software interface identification [0x84] VPD page:\n");
        else
            printf("Software interface identification VPD page:\n");
        if (op->do_hex) {
            dStrHex((const char *)b, len + 4, hex_format);
            return 0;
        }
        up = b + 4;
        for ( ; len > 5; len -= 6, up += 6)
            printf("    IEEE Company_id: 0x%06x, vendor specific extension "
                   "id: 0x%06x\n", sg_get_unaligned_be24(up + 0),
                   sg_get_unaligned_be24(up + 3));
        break;
    case VPD_UNIT_SERIAL_NUM:           /* 0x80 */
        if (b[1] != pn)
            goto dumb_inq;
        if ((0x2 == b[2]) && (0x2 == b[3])) {
            /* could be a Unit Serial number VPD page with a very long
             * length of 4+514 bytes; more likely standard response for
             * SCSI-2, RMB=1 and a response_data_format of 0x2. */
            pr2serr("very unlikely to be a Unit Serial Number VPD "
                    "response, so ...\n");
            goto dumb_inq;
        }
        len = sg_get_unaligned_be16(b + 2);       /* spc4r25 */
        if (op->do_long)
            printf("Unit serial number [0x80] VPD page:\n");
        else
            printf("Unit serial number VPD page:\n");
        if (op->do_hex)
            dStrHex((const char *)b, len + 4, hex_format);
        else if (len > 0) {
            if (len + 4 < (int)sizeof(b))
                b[len + 4] = '\0';
            else
                b[sizeof(b) - 1] = '\0';
            printf("  %s\n", b + 4);
        } else
            printf("  <empty>\n");
        break;
    case VPD_3PARTY_COPY:               /* 0x8f */
        if (b[1] != pn)
            goto dumb_inq;
        len = sg_get_unaligned_be16(b + 2);       /* spc4r25 */
        if ((NULL == ihbp) && (len > sz)) {
            /* Increase response size to maximum we can handle here */
            sz = VPD_ATA_INFO_RESP_LEN;
            res = sg_ll_inquiry(sg_fd, 0, 1, pn, b, sz, 0, verb);
            if (res) {
                pr2serr("INQUIRY fetching VPD page=0x%x failed\n", pn);
                return res;
            }
            len = sg_get_unaligned_be16(b + 2);
            if (len > sz) {
                pr2serr("Response to Third party copy VPD page truncated\n");
                len = sz;
            }
        }
        if (op->do_long)
            printf("Third party copy [0x8f] VPD page:\n");
        else
            printf("Third party copy VPD page:\n");
        if (1 == op->do_hex) {
            dStrHex((const char *)b, len + 4, 0);
            return 0;
        }
        decode_3party_copy_vpd(b, len + 4, op->do_hex, pdt, op->verbose);
        break;
    case 0xb0:          /* VPD page depends on pdt */
        if (b[1] != pn)
            goto dumb_inq;
        switch (pdt) {
        case PDT_DISK: case PDT_WO: case PDT_OPTICAL: case PDT_ZBC:
            vpd_name = "Block limits (SBC)";
            sbc = true;
            break;
        case PDT_TAPE: case PDT_MCHANGER:
            vpd_name = "Sequential-access device capabilities (SSC)";
            ssc = true;
            break;
        case PDT_OSD:
            vpd_name = "OSD information (OSD)" ;
            /* osd = 1; */
            break;
        default:
            vpd_name = "unexpected pdt for B0h";
            break;
        }
        len = sg_get_unaligned_be16(b + 2);       /* spc4r25 */
        if (len > sz) {
            pr2serr("Response to %s VPD page truncated\n", vpd_name);
            len = sz;
        }
        if (op->do_long)
            printf("%s [0xb0] VPD page:\n", vpd_name);
        else
            printf("%s VPD page:\n", vpd_name);
        if (op->do_hex) {
            dStrHex((const char *)b, len + 4, hex_format);
            return 0;
        }
        res = 0;
        if (ssc)
            res = decode_tape_dev_caps_vpd(b, len + 4);
        else if (sbc)
            res = decode_block_limits_vpd(b, len + 4);
        else
            dStrHex((const char *)b, len + 4, 0);
        if (res)
            return res;
        break;
    case 0xb1:          /* VPD page depends on pdt */
        if (b[1] != pn)
            goto dumb_inq;
        switch (pdt) {
        case PDT_DISK: case PDT_WO: case PDT_OPTICAL: case PDT_ZBC:
            vpd_name = "Block device characteristics (SBC)";
            sbc = true;
            break;
        case PDT_TAPE: case PDT_MCHANGER:
            vpd_name = "Manufactured-assigned serial number (SSC)";
            ssc = true;
            break;
        case PDT_OSD:
            vpd_name = "Security token (OSD)";
            /* osd = 1; */
            break;
        case PDT_ADC:
            vpd_name = "Manufactured assigned serial number (ADC)";
            adc = true;
            break;
        default:
            vpd_name = "unexpected pdt for B1h";
            break;
        }
        len = sg_get_unaligned_be16(b + 2);       /* spc4r25 */
        if (len > sz) {
            pr2serr("Response to %s VPD page truncated\n", vpd_name);
            len = sz;
        }
        if (op->do_long)
            printf("%s [0xb1] VPD page:\n", vpd_name);
        else
            printf("%s VPD page:\n", vpd_name);
        if (op->do_hex) {
            dStrHex((const char *)b, len + 4, hex_format);
            return 0;
        }
        res = 0;
        if (ssc || adc)
            res = decode_tape_man_ass_sn_vpd(b, len + 4);
        else if (sbc)
            res = decode_block_dev_chars_vpd(b, len + 4);
        else
            dStrHex((const char *)b, len + 4, 0);
        if (res)
            return res;
        break;
    case 0xb2:          /* VPD page depends on pdt */
        if (b[1] != pn)
            goto dumb_inq;

        len = sg_get_unaligned_be16(b + 2);       /* spc4r25 */
        switch (pdt) {
        case PDT_DISK: case PDT_WO: case PDT_OPTICAL: case PDT_ZBC:
            vpd_name = "Logical block provisioning (SBC)";
            sbc = true;
            break;
        case PDT_TAPE: case PDT_MCHANGER:
            vpd_name = "TapeAlert supported flags (SSC)";
            ssc = true;
            break;
        default:
            vpd_name = "unexpected pdt for B2h";
            break;
        }
        if (op->do_long)
            printf("%s [0xb2] VPD page:\n", vpd_name);
        else
            printf("%s VPD page:\n", vpd_name);
        if (op->do_hex) {
            dStrHex((const char *)b, len + 4, hex_format);
            return 0;
        }
        res = 0;
        if (ssc)
            res = decode_tapealert_supported_vpd(b, len + 4);
        else if (sbc)       /* added in sbc3r20 */
            res = decode_block_lb_prov_vpd(b, len + 4, op);
        else
            dStrHex((const char *)b, len + 4, 0);
        if (res)
            return res;
        break;
    case 0xb3:          /* VPD page depends on pdt */
        if (b[1] != pn)
            goto dumb_inq;
        len = sg_get_unaligned_be16(b + 2);       /* spc4r25 */
        switch (pdt) {
        case PDT_DISK: case PDT_WO: case PDT_OPTICAL: case PDT_ZBC:
            vpd_name = "Referrals (SBC)";
            sbc = true;
            break;
        case PDT_TAPE: case PDT_MCHANGER:
            vpd_name = "Automation device serial number (SSC)";
            ssc = true;
            break;
        default:
            vpd_name = "unexpected pdt for B3h";
            break;
        }
        if (op->do_long)
            printf("%s [0xb3] VPD page:\n", vpd_name);
        else
            printf("%s VPD page:\n", vpd_name);
        if (op->do_hex) {
            dStrHex((const char *)b, len + 4, hex_format);
            return 0;
        }
        res = 0;
        if (ssc) {
            /* VPD_AUTOMATION_DEV_SN ssc */
            if (len > 0) {
                if (len + 4 < (int)sizeof(b))
                    b[len + 4] = '\0';
                else
                    b[sizeof(b) - 1] = '\0';
                printf("  serial number: %s\n", b + 4);
            } else
                printf("  serial number: <empty>\n");
        } else if (sbc)       /* added in sbc3r22 */
            res = decode_referrals_vpd(b, len + 4);
        else
            dStrHex((const char *)b, len + 4, 0);
        if (res)
            return res;
        break;
    case 0xb4:          /* VPD page depends on pdt */
        if (b[1] != pn)
            goto dumb_inq;
        len = sg_get_unaligned_be16(b + 2);       /* spc4r25 */
        switch (pdt) {
        case PDT_DISK: case PDT_WO: case PDT_OPTICAL: case PDT_ZBC:
            vpd_name = "Supported block lengths and protection types (SBC)";
            sbc = true;
            break;
        case PDT_TAPE: case PDT_MCHANGER:
            vpd_name = "Data transfer device element address (SSC)";
            ssc = true;
            break;
        default:
            vpd_name = "unexpected pdt for B4h";
            break;
        }
        if (op->do_long)
            printf("%s [0xb4] VPD page:\n", vpd_name);
        else
            printf("%s VPD page:\n", vpd_name);
        if (op->do_hex) {
            dStrHex((const char *)b, len + 4, hex_format);
            return 0;
        }
        res = 0;
        if (ssc) {
            if (len > 0) {
                printf("  0x");
                for (k = 0; k < len; ++k)
                    printf("%02x", (unsigned int)b[4 + k]);
                printf("\n");
            } else
                printf("  <empty>\n");
        } else if (sbc)       /* added in sbc4r01 */
            res = decode_sup_block_lens_vpd(b, len + 4);
        else
            dStrHex((const char *)b, len + 4, 0);
        if (res)
            return res;
        break;
    case 0xb5:          /* VPD page depends on pdt */
        if (b[1] != pn)
            goto dumb_inq;
        len = sg_get_unaligned_be16(b + 2);
        switch (pdt) {
        case PDT_DISK: case PDT_WO: case PDT_OPTICAL: case PDT_ZBC:
            vpd_name = "Block device characteristics extension (SBC)";
            sbc = true;
            break;
        case PDT_TAPE: case PDT_MCHANGER:
            vpd_name = "Logical block protection (SSC)";
            ssc = true;
            break;
        default:
            vpd_name = "unexpected pdt for B5h";
            break;
        }
        if (op->do_long)
            printf("%s [0xb5] VPD page:\n", vpd_name);
        else
            printf("%s VPD page:\n", vpd_name);
        if (op->do_hex) {
            dStrHex((const char *)b, len + 4, hex_format);
            return 0;
        }
        res = 0;
        if (sbc)       /* added in sbc4r02 */
            res = decode_block_dev_char_ext_vpd(b, len + 4);
        else if (ssc)       /* added in ssc5r02a */
            res = decode_lb_protection_vpd(b, len + 4);
        else
            dStrHex((const char *)b, len + 4, 0);
        if (res)
            return res;
        break;
    case VPD_ZBC_DEV_CHARS:       /* 0xb6 for both pdt=0 and pdt=0x14 */
        if (b[1] != pn)
            goto dumb_inq;
        len = sg_get_unaligned_be16(b + 2);
        switch (pdt) {
        case PDT_DISK: case PDT_WO: case PDT_OPTICAL: case PDT_ZBC:
            vpd_name = "Zoned block device characteristics";
            sbc = true;
            break;
        default:
            vpd_name = "unexpected pdt for B6h";
            break;
        }
        if (op->do_long)
            printf("%s [0xb6] VPD page:\n", vpd_name);
        else
            printf("%s VPD page:\n", vpd_name);
        if (op->do_hex) {
            dStrHex((const char *)b, len + 4, hex_format);
            return 0;
        }
        res = 0;
        if (sbc)       /* added in zbc-r01c */
            res = decode_zbdc_vpd(b, len + 4);
        else
            dStrHex((const char *)b, len + 4, 0);
        if (res)
            return res;
        break;
    case 0xb7:          /* VPD page depends on pdt */
        if (b[1] != pn)
            goto dumb_inq;
        len = sg_get_unaligned_be16(b + 2);
        switch (pdt) {
        case PDT_DISK: case PDT_WO: case PDT_OPTICAL: case PDT_ZBC:
            vpd_name = "Block limits extension";
            sbc = true;
            break;
        default:
            vpd_name = "unexpected pdt for B7h";
            break;
        }
        if (op->do_long)
            printf("%s [0xb7] VPD page:\n", vpd_name);
        else
            printf("%s VPD page:\n", vpd_name);
        if (op->do_hex) {
            dStrHex((const char *)b, len + 4, hex_format);
            return 0;
        }
        res = 0;
        if (sbc)       /* added in sbc4r07 */
            res = decode_block_limits_ext_vpd(b, len + 4);
        else
            dStrHex((const char *)b, len + 4, 0);
        if (res)
            return res;
        break;
    default:
        if (b[1] != pn)
            goto dumb_inq;
        len = sg_get_unaligned_be16(b + 2) + 4;
        vpp = sdp_get_vpd_detail(pn, -1, pdt);
        if (vpp)
            snprintf(c, sizeof(c), "%s VPD page", vpp->name);
        else
            snprintf(c, sizeof(c), "VPD page 0x%x", pn);
        if (op->do_hex)
            printf("%s in hex:\n", c);
        else
            pr2serr("%s in hex:\n", c);
        if (len > (int)sizeof(b)) {
            if (op->verbose)
                pr2serr("page length=%d too long, trim\n", len);
            len = sizeof(b);
        }
        if (op->do_hex)
            dStrHex((const char *)b, len, hex_format);
        else
            dStrHexErr((const char *)b, len, 0);
        break;
    }
    return 0;

dumb_inq:
    pr2serr("malformed VPD response, VPD pages probably not supported\n");
    return SG_LIB_CAT_MALFORMED;
}

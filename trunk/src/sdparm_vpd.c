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

#include <stdio.h>
#include <stdlib.h>
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
    int m, p_id, c_set, piv, assoc, desig_type, i_len, is_sas;
    int naa, off, u, rtp;
    const unsigned char * ucp;
    const unsigned char * ip;
    unsigned char sas_tport_addr[8];

    rtp = 0;
    memset(sas_tport_addr, 0, sizeof(sas_tport_addr));
    off = -1;
    while ((u = sg_vpd_dev_id_iter(buff, len, &off, m_assoc, m_desig_type,
                                   m_code_set)) == 0) {
        ucp = buff + off;
        i_len = ucp[3];
        if ((off + i_len + 4) > len) {
            pr2serr("    VPD page error: designator length longer than\n"
                    "     remaining response length=%d\n", (len - off));
            return SG_LIB_CAT_MALFORMED;
        }
        ip = ucp + 4;
        p_id = ((ucp[0] >> 4) & 0xf);
        c_set = (ucp[0] & 0xf);
        piv = ((ucp[1] & 0x80) ? 1 : 0);
        is_sas = (piv && (6 == p_id)) ? 1 : 0;
        assoc = ((ucp[1] >> 4) & 0x3);
        desig_type = (ucp[1] & 0xf);
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
                if ((0 == is_sas) || (1 != assoc)) {
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
            if ((0 == is_sas) || (1 != c_set) || (1 != assoc) || (4 != i_len))
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
            if (3 != c_set) {
                pr2serr("      << expected UTF-8 code_set >>\n");
                dStrHexErr((const char *)ip, i_len, 0);
                break;
            }
            /* does %s print out UTF-8 ok??
             * Seems to depend on the locale. Looks ok here with my
             * locale setting: en_AU.UTF-8
             */
            printf("%s\n", (const char *)ip);
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
decode_designation_descriptor(const unsigned char * ucp, int i_len,
                              int print_assoc,
                              const struct sdparm_opt_coll * op)
{
    char b[2048];

    sg_get_designation_descriptor_str(NULL, ucp, i_len + 4, print_assoc,
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
    const unsigned char * ucp;

    if (op->do_quiet)
        return decode_dev_ids_quiet(buff, len, m_assoc, m_desig_type,
                                    m_code_set);
    off = -1;
    printed = 0;
    while ((u = sg_vpd_dev_id_iter(buff, len, &off, m_assoc, m_desig_type,
                                   m_code_set)) == 0) {
        ucp = buff + off;
        i_len = ucp[3];
        if ((off + i_len + 4) > len) {
            pr2serr("    VPD page error: designator length longer than\n"
                    "     remaining response length=%d\n", (len - off));
            return SG_LIB_CAT_MALFORMED;
        }
        assoc = ((ucp[1] >> 4) & 0x3);
        if (print_if_found && (0 == printed)) {
            printed = 1;
            printf("  %s:\n", print_if_found);
        }
        if (NULL == print_if_found)
            printf("  %s:\n", sg_get_desig_assoc_str(assoc));
        decode_designation_descriptor(ucp, i_len, 0, op);
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
    unsigned char * ucp;

    if (len < 4) {
        pr2serr("Mode page policy VPD page length too short=%d\n", len);
        return SG_LIB_CAT_MALFORMED;
    }
    len -= 4;
    ucp = buff + 4;
    for (k = 0; k < len; k += bump, ucp += bump) {
        bump = 4;
        if ((k + bump) > len) {
            pr2serr("Mode page policy VPD page, short descriptor length=%d, "
                    "left=%d\n", bump, (len - k));
            return SG_LIB_CAT_MALFORMED;
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

/* VPD_DEVICE_CONSTITUENTS  0x8b */
static int
decode_dev_const_vpd(unsigned char * buff, int len)
{
    int k, j, bump, cd_len;
    unsigned char * ucp;

    if (len < 4) {
        pr2serr("Device constituents VPD page length too short=%d\n",
                len);
        return SG_LIB_CAT_MALFORMED;
    }
    len -= 4;
    ucp = buff + 4;
    for (k = 0, j = 0; k < len; k += bump, ucp += bump, ++j) {
        printf("  Constituent descriptor %d:\n", j + 1);
        if ((k + 36) > len) {
            pr2serr("Device constituents VPD page, short descriptor "
                    "length=36, left=%d\n", (len - k));
            return SG_LIB_CAT_MALFORMED;
        }
        printf("    Constituent type: 0x%x\n",
               sg_get_unaligned_be16(ucp + 0));
        printf("    Constituent device type: 0x%x\n", ucp[2]);
        printf("    Vendor_identification: %.8s\n", ucp + 4);
        printf("    Product_identification: %.16s\n", ucp + 12);
        printf("    Product_revision_level: %.4s\n", ucp + 28);
        cd_len = sg_get_unaligned_be16(ucp + 34);
        bump = 36 + cd_len;
        if ((k + bump) > len) {
            pr2serr("Device constituents VPD page, short descriptor "
                    "length=%d, left=%d\n", bump, (len - k));
            return SG_LIB_CAT_MALFORMED;
        }
        if (cd_len > 0) {
            printf("   Constituent specific descriptor list (in hex):\n");
            dStrHex((const char *)(ucp + 36), cd_len, 1);
        }
    }
    return 0;
}

/* VPD_MAN_NET_ADDR  0x85 */
static int
decode_man_net_vpd(unsigned char * buff, int len)
{
    int k, bump, na_len;
    unsigned char * ucp;

    if (len < 4) {
        pr2serr("Management network addresses VPD page length too short=%d\n",
                len);
        return SG_LIB_CAT_MALFORMED;
    }
    len -= 4;
    ucp = buff + 4;
    for (k = 0; k < len; k += bump, ucp += bump) {
        printf("  %s, Service type: %s\n",
               sg_get_desig_assoc_str((ucp[0] >> 5) & 0x3),
               sdparm_network_service_type_arr[ucp[0] & 0x1f]);
        na_len = sg_get_unaligned_be16(ucp + 2);
        bump = 4 + na_len;
        if ((k + bump) > len) {
            pr2serr("Management network addresses VPD page, short descriptor "
                    "length=%d, left=%d\n", bump, (len - k));
            return SG_LIB_CAT_MALFORMED;
        }
        if (na_len > 0) {
            printf("    %s\n", ucp + 4);
        }
    }
    return 0;
}

/* This is xcopy(LID4) related: "ROD" == Representation Of Data
 * Used by VPD_3PARTY_COPY */
static void
decode_rod_descriptor(const unsigned char * buff, int len)
{
    const unsigned char * ucp = buff;
    int k, bump;

    for (k = 0; k < len; k += bump, ucp += bump) {
        bump = sg_get_unaligned_be16(ucp + 2) + 4;
        switch (ucp[0]) {
            case 0:
                /* Block ROD device type specific descriptor */
                printf("  Optimal block ROD length granularity: %d\n",
                       sg_get_unaligned_be16(ucp + 6));
                printf("  Maximum Bytes in block ROD: %" PRIu64 "\n",
                       sg_get_unaligned_be64(ucp + 8));
                printf("  Optimal Bytes in block ROD transfer: %" PRIu64 "\n",
                       sg_get_unaligned_be64(ucp + 16));
                printf("  Optimal Bytes to token per segment: %" PRIu64 "\n",
                       sg_get_unaligned_be64(ucp + 24));
                printf("  Optimal Bytes from token per segment:"
                       " %" PRIu64 "\n", sg_get_unaligned_be64(ucp + 32));
                break;
            case 1:
                /* Stream ROD device type specific descriptor */
                printf("  Maximum Bytes in stream ROD: %" PRIu64 "\n",
                       sg_get_unaligned_be64(ucp + 8));
                printf("  Optimal Bytes in stream ROD transfer:"
                       " %" PRIu64 "\n", sg_get_unaligned_be64(ucp + 16));
                break;
            case 3:
                /* Copy manager ROD device type specific descriptor */
                printf("  Maximum Bytes in processor ROD: %" PRIu64 "\n",
                       sg_get_unaligned_be64(ucp + 8));
                printf("  Optimal Bytes in processor ROD transfer:"
                       " %" PRIu64 "\n", sg_get_unaligned_be64(ucp + 16));
                break;
            default:
                printf("  Unhandled descriptor (format %d, device type %d)\n",
                       ucp[0] >> 5, ucp[0] & 0x1F);
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
decode_3party_copy_vpd(unsigned char * buff, int len, int do_hex, int verbose)
{
    int j, k, m, bump, desc_type, desc_len, sa_len;
    unsigned int u;
    const unsigned char * ucp;
    const char * cp;
    uint64_t ull;
    char b[80];

    if (len < 4) {
        pr2serr("Third-party Copy VPD page length too short=%d\n", len);
        return;
    }
    len -= 4;
    ucp = buff + 4;
    for (k = 0; k < len; k += bump, ucp += bump) {
        desc_type = sg_get_unaligned_be16(ucp);
        desc_len = sg_get_unaligned_be16(ucp + 2);
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
            dStrHex((const char *)ucp + 4, desc_len, 1);
        else if (do_hex > 2)
            dStrHex((const char *)ucp, bump, 1);
        else {
            switch (desc_type) {
            case 0x0000:    /* Required if POPULATE TOKEN (or friend) used */
                printf(" Block Device ROD Token Limits:\n");
                printf("  Maximum Range Descriptors: %d\n",
                       sg_get_unaligned_be16(ucp + 10));
                u = sg_get_unaligned_be32(ucp + 12);
                printf("  Maximum Inactivity Timeout: %u seconds\n", u);
                u = sg_get_unaligned_be32(ucp + 16);
                printf("  Default Inactivity Timeout: %u seconds\n", u);
                ull = sg_get_unaligned_be64(ucp + 20);
                printf("  Maximum Token Transfer Size: %" PRIu64 "\n", ull);
                ull = sg_get_unaligned_be64(ucp + 28);
                printf("  Optimal Transfer Count: %" PRIu64 "\n", ull);
                break;
            case 0x0001:    /* Mandatory (SPC-4) */
                printf(" Supported Commands:\n");
                j = 0;
                while (j < ucp[4]) {
                    sa_len = ucp[6 + j];
                    for (m = 0; m < sa_len; ++m) {
                        sg_get_opcode_sa_name(ucp[5 + j], ucp[7 + j + m],
                                              0, sizeof(b), b);
                        printf("  %s\n", b);
                    }
                    j += sa_len + 2;
                }
                break;
            case 0x0004:
                printf(" Parameter Data:\n");
                printf("  Maximum CSCD Descriptor Count: %d\n",
                       sg_get_unaligned_be16(ucp + 8));
                printf("  Maximum Segment Descriptor Count: %d\n",
                       sg_get_unaligned_be16(ucp + 10));;
                u = sg_get_unaligned_be32(ucp + 12);
                printf("  Maximum Descriptor List Length: %u\n", u);
                u = sg_get_unaligned_be32(ucp + 16);
                printf("  Maximum Inline Data Length: %u\n", u);
                break;
            case 0x0008:
                printf(" Supported Descriptors:\n");
                for (j = 0; j < ucp[4]; j++) {
                    cp = get_tpc_desc_name(ucp[5 + j]);
                    if (strlen(cp) > 0)
                        printf("  %s [0x%x]\n", cp, ucp[5 + j]);
                    else
                        printf("  0x%x\n", ucp[5 + j]);
                }
                break;
            case 0x000C:
                printf(" Supported CSCD IDs (above 0x7ff):\n");
                for (j = 0; j < sg_get_unaligned_be16(ucp + 4); j += 2) {
                    u = sg_get_unaligned_be16(ucp + 6 + j);
                    cp = get_cscd_desc_id_name(u);
                    if (strlen(cp) > 0)
                        printf("  %s [0x%04x]\n", cp, u);
                    else
                        printf("  0x%04x\n", u);
                }
                break;
            case 0x0106:
                printf(" ROD Token Features:\n");
                printf("  Remote Tokens: %d\n", ucp[4] & 0x0f);
                u = sg_get_unaligned_be32(ucp + 16);
                printf("  Minimum Token Lifetime: %u seconds\n", u);
                u = sg_get_unaligned_be32(ucp + 20);
                printf("  Maximum Token Lifetime: %u seconds\n", u);
                u = sg_get_unaligned_be32(ucp + 24);
                printf("  Maximum Token inactivity timeout: %d\n", u);
                decode_rod_descriptor(ucp + 48,
                                      sg_get_unaligned_be16(ucp + 46));
                break;
            case 0x0108:
                printf(" Supported ROD Token and ROD Types:\n");
                for (j = 0; j < sg_get_unaligned_be16(ucp + 6); j+= 64) {
                    u = sg_get_unaligned_be32(ucp + 8 + j);
                    cp = get_tpc_rod_name(u);
                    if (strlen(cp) > 0)
                        printf("  ROD Type: %s [0x%x]\n", cp, u);
                    else
                        printf("  ROD Type: 0x%x\n", u);
                    printf("    Internal: %s\n",
                           (ucp[8 + j + 4] & 0x80) ? "yes" : "no");
                    printf("    Token In: %s\n",
                           (ucp[8 + j + 4] & 0x02) ? "yes" : "no");
                    printf("    Token Out: %s\n",
                           (ucp[8 + j + 4] & 0x01) ? "yes" : "no");
                    printf("    Preference: %d\n",
                           sg_get_unaligned_be16(ucp + 8 + j + 6));
                }
                break;
            case 0x8001:    /* Mandatory (SPC-4) */
                printf(" General Copy Operations:\n");
                u = sg_get_unaligned_be32(ucp + 4);
                printf("  Total Concurrent Copies: %u\n", u);
                u = sg_get_unaligned_be32(ucp + 8);
                printf("  Maximum Identified Concurrent Copies: %u\n", u);
                u = sg_get_unaligned_be32(ucp + 12);
                printf("  Maximum Segment Length: %u\n", u);
                ull = (1 << ucp[16]);   /* field is power of 2 */
                printf("  Data Segment Granularity: %" PRIu64 "\n", ull);
                ull = (1 << ucp[17]);
                printf("  Inline Data Granularity: %" PRIu64 "\n", ull);
                break;
            case 0x9101:
                printf(" Stream Copy Operations:\n");
                u = sg_get_unaligned_be32(ucp + 4);
                printf("  Maximum Stream Device Transfer Size: %u\n", u);
                break;
            case 0xC001:
                printf(" Held Data:\n");
                u = sg_get_unaligned_be32(ucp + 4);
                printf("  Held Data Limit: %u\n", u);
                ull = (1 << ucp[8]);
                printf("  Held Data Granularity: %" PRIu64 "\n", ull);
                break;
            default:
                pr2serr("Unexpected type=%d\n", desc_type);
                dStrHexErr((const char *)ucp, bump, 1);
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
    unsigned char * ucp;

    if (len < 4) {
        pr2serr("Protocol-specific logical unit information VPD page length "
                "too short=%d\n", len);
        return SG_LIB_CAT_MALFORMED;
    }
    len -= 4;
    ucp = buff + 4;
    for (k = 0; k < len; k += bump, ucp += bump) {
        rel_port = sg_get_unaligned_be16(ucp);
        printf("Relative port=%d\n", rel_port);
        proto = ucp[2] & 0xf;
        desc_len = sg_get_unaligned_be16(ucp + 6);
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
                printf(" TLR control supported: %d\n", !!(ucp[8] & 0x1));
                break;
            default:
                pr2serr("Unexpected proto=%d\n", proto);
                dStrHexErr((const char *)ucp, bump, 1);
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
    unsigned char * ucp;
    unsigned char * pidp;

    if (len < 4) {
        pr2serr("Protocol-specific port information VPD page length too "
                "short=%d\n", len);
        return SG_LIB_CAT_MALFORMED;
    }
    len -= 4;
    ucp = buff + 4;
    for (k = 0; k < len; k += bump, ucp += bump) {
        rel_port = sg_get_unaligned_be16(ucp);
        printf("Relative port=%d\n", rel_port);
        proto = ucp[2] & 0xf;
        desc_len = sg_get_unaligned_be16(ucp + 6);
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
                       !!(ucp[3] & 0x1));       /* added spl3r03 */
                pidp = ucp + 8;
                for (j = 0; j < desc_len; j += 4, pidp += 4)
                    printf("      phy id=%d, SSP persistent capable=%d\n",
                           pidp[1], (0x1 & pidp[2]));
                break;
            default:
                pr2serr("Unexpected proto=%d\n", proto);
                dStrHexErr((const char *)ucp, bump, 1);
                break;
            }
        }
    }
    return 0;
}

/* VPD_SCSI_PORTS  0x88 */
static int
decode_scsi_ports_vpd(unsigned char * buff, int len,
                      const struct sdparm_opt_coll * op)
{
    int k, bump, rel_port, ip_tid_len, tpd_len, res;
    unsigned char * ucp;

    if (len < 4) {
        pr2serr("SCSI Ports VPD page length too short=%d\n", len);
        return SG_LIB_CAT_MALFORMED;
    }
    len -= 4;
    ucp = buff + 4;
    for (k = 0; k < len; k += bump, ucp += bump) {
        rel_port = sg_get_unaligned_be16(ucp + 2);
        printf("Relative port=%d\n", rel_port);
        ip_tid_len = sg_get_unaligned_be16(ucp + 6);
        bump = 8 + ip_tid_len;
        if ((k + bump) > len) {
            pr2serr("SCSI Ports VPD page, short descriptor length=%d, "
                    "left=%d\n", bump, (len - k));
            return SG_LIB_CAT_MALFORMED;
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
        tpd_len = sg_get_unaligned_be16(ucp + bump + 2);
        if ((k + bump + tpd_len + 4) > len) {
            pr2serr("SCSI Ports VPD page, short descriptor(tgt) length=%d, "
                    "left=%d\n", bump, (len - k));
            return SG_LIB_CAT_MALFORMED;
        }
        if (tpd_len > 0) {
            res = decode_dev_ids(" Target port descriptor(s)",
                                 ucp + bump + 4, tpd_len, VPD_ASSOC_TPORT,
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
        printf("  NO_PI_CHK=%d\n", !!(b[7] & 0x10));    /* spc5r02 */
        printf("  P_I_I_SUP=%d\n", !!(b[7] & 0x10));
        printf("  LUICLR=%d\n", !!(b[7] & 0x1));
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
        /* CBCS, capability-based command security, obsolete in spc5r01 */
        printf("  P_I_I_SUP=%d LUICLR=%d R_SUP=%d CBCS=%d\n",
               !!(b[7] & 0x10), !!(b[7] & 0x1),
               !!(b[8] & 0x10), !!(b[8] & 0x1));
        printf("  Multi I_T nexus microcode download=%d\n", b[9] & 0xf);
        printf("  Extended self-test completion minutes=%d\n",
               sg_get_unaligned_be16(b + 10));
        printf("  POA_SUP=%d HRA_SUP=%d VSA_SUP=%d\n",      /* spc4r32 */
               !!(b[12] & 0x80), !!(b[12] & 0x40), !!(b[12] & 0x20));
        printf("  Maximum supported sense data length=%d\n",
               b[13]); /* spc4r34 */
    }
    return 0;
}

/* VPD_ATA_INFO  0x89 */
static int
decode_ata_info_vpd(unsigned char * buff, int len, int do_long, int do_hex)
{
    char b[80];
    int num, is_be;
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
    if (len < 60)
        return SG_LIB_CAT_MALFORMED;
    is_be = sg_is_big_endian();
    if ((0xec == buff[56]) || (0xa1 == buff[56])) {
        cp = (0xa1 == buff[56]) ? "PACKET " : "";
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
    unsigned char * ucp;
    unsigned int value;

    if (len < 4) {
        pr2serr("Power consumption VPD page length too short=%d\n", len);
        return SG_LIB_CAT_MALFORMED;
    }
    len -= 4;
    ucp = buff + 4;
    for (k = 0; k < len; k += bump, ucp += bump) {
        bump = 4;
        if ((k + bump) > len) {
            pr2serr("Power consumption VPD page, short descriptor length=%d, "
                    "left=%d\n", bump, (len - k));
            return SG_LIB_CAT_MALFORMED;
        }
        value = sg_get_unaligned_be16(ucp + 2);
        printf("  Power consumption identifier: 0x%x", ucp[0]);
        if (value >= 1000 && (ucp[1] & 0x7) > 0)
            printf("    Maximum power consumption: %d.%03d %s\n",
                   value / 1000, value % 1000,
                   power_unit_arr[(ucp[1] & 0x7) - 1]);
        else
            printf("    Maximum power consumption: %d %s\n",
                   value, power_unit_arr[ucp[1] & 0x7]);
    }
    return 0;
}

/* VPD_BLOCK_LIMITS  0xb0 */
static int
decode_block_limits_vpd(unsigned char * buff, int len)
{
    unsigned int u;
    unsigned char b[4];

    if (len < 16) {
        pr2serr("Block limits VPD page length too short=%d\n", len);
        return SG_LIB_CAT_MALFORMED;
    }
    printf("  Write same non-zero (WSNZ): %d\n", !!(buff[4] & 0x1));
    printf("  Maximum compare and write length: %u blocks\n", buff[5]);
    u = sg_get_unaligned_be16(buff + 6);
    printf("  Optimal transfer length granularity: %u blocks\n", u);
    u = sg_get_unaligned_be32(buff + 8);
    printf("  Maximum transfer length: %u blocks\n", u);
    u = sg_get_unaligned_be32(buff + 12);
    printf("  Optimal transfer length: %u blocks\n", u);
    if (len > 19) {     /* added in sbc3r09 */
        u = sg_get_unaligned_be32(buff + 16);
        printf("  Maximum prefetch length: %u blocks\n", u);
        /* was 'Maximum prefetch transfer length' prior to sbc3r33 */
    }
    if (len > 27) {     /* added in sbc3r18 */
        u = sg_get_unaligned_be32(buff + 29);
        printf("  Maximum unmap LBA count: %u\n", u);
        u = sg_get_unaligned_be32(buff + 24);
        printf("  Maximum unmap block descriptor count: %u\n", u);
    }
    if (len > 35) {     /* added in sbc3r19 */
        u = sg_get_unaligned_be32(buff + 28);
        printf("  Optimal unmap granularity: %u\n", u);
        printf("  Unmap granularity alignment valid: %u\n",
               !!(buff[32] & 0x80));
        memcpy(b, buff + 32, 4);
        b[0] &= 0x7f;       /* mask off top bit */
        u = sg_get_unaligned_be32(b);
        printf("  Unmap granularity alignment: %u\n", u);
    }
    if (len > 43)      /* added in sbc3r26 */
        printf("  Maximum write same length: 0x%" PRIx64 " blocks\n",
               sg_get_unaligned_be64(buff + 36));
    if (len > 44) {     /* added in sbc4r02 */
        u = sg_get_unaligned_be32(buff + 44);
        printf("  Maximum atomic transfer length: %u\n", u);
        u = sg_get_unaligned_be32(buff + 48);
        printf("  Atomic alignment: %u\n", u);
        u = sg_get_unaligned_be32(buff + 52);
        printf("  Atomic transfer length granularity: %u\n", u);
    }
    if (len > 56) {     /* added in sbc4r04 */
        u = sg_get_unaligned_be32(buff + 56);
        printf("  Maximum atomic transfer length with atomic boundary: %u\n",
               u);
        u = sg_get_unaligned_be32(buff + 60);
        printf("  Maximum atomic boundary size: %u\n", u);
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
    printf("  Maximum number of streams: %u\n", u);
    u = sg_get_unaligned_be16(buff + 8);
    printf("  Optimal stream write size: %u logical blocks\n", u);
    u = sg_get_unaligned_be32(buff + 10);
    printf("  Stream granularity size: %u\n", u);
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

/* VPD_LB_PROVISIONING  0xb2 */
static int
decode_block_lb_prov_vpd(unsigned char * b, int len,
                         const struct sdparm_opt_coll * op)
{
    int dp;

    if (len < 4) {
        pr2serr("Logical block provisioning page too short=%d\n", len);
        return SG_LIB_CAT_MALFORMED;
    }
    printf("  Unmap command supported (LBPU): %d\n", !!(0x80 & b[5]));
    printf("  Write same (16) with unmap bit supported (LBWS): %d\n",
           !!(0x40 & b[5]));
    printf("  Write same (10) with unmap bit supported (LBWS10): %d\n",
           !!(0x20 & b[5]));
    printf("  Logical block provisioning read zeros (LBPRZ): %d\n",
           (0x7 & (b[5] >> 2)));
    printf("  Anchored LBAs supported (ANC_SUP): %d\n", !!(0x2 & b[5]));
    printf("  Threshold exponent: %d\n", b[4]);
    dp = !!(b[5] & 0x1);
    printf("  Descriptor present: %d\n", dp);
    printf("  Minimum percentage: %d\n", 0x1f & (b[6] >> 3));
    printf("  Provisioning type: %d\n", b[6] & 0x7);
    printf("  Threshold percentage: %d\n", b[7]);
    if (dp) {
        const unsigned char * ucp;
        int i_len;

        ucp = b + 8;
        i_len = ucp[3];
        if (0 == i_len) {
            pr2serr("Logical block provisioning page provisioning group "
                    "descriptor too short=%d\n", i_len);
            return 0;
        }
        printf("  Provisioning group descriptor\n");
        decode_designation_descriptor(ucp, i_len, 1, op);
    }
    return 0;
}

/* VPD_TA_SUPPORTED  0xb2 */
static int
decode_tapealert_supported_vpd(unsigned char * b, int len)
{
    if (len < 12) {
        pr2serr("TapeAlert supported flags length too short=%d\n", len);
        return SG_LIB_CAT_MALFORMED;
    }
    printf("  Flag01h: %d  02h: %d  03h: %d  04h: %d  05h: %d  06h: %d  "
           "07h: %d  08h: %d\n", !!(b[4] & 0x80), !!(b[4] & 0x40),
           !!(b[4] & 0x20), !!(b[4] & 0x10), !!(b[4] & 0x8), !!(b[4] & 0x4),
           !!(b[4] & 0x2), !!(b[4] & 0x1));
    printf("  Flag09h: %d  0ah: %d  0bh: %d  0ch: %d  0dh: %d  0eh: %d  "
           "0fh: %d  10h: %d\n", !!(b[5] & 0x80), !!(b[5] & 0x40),
           !!(b[5] & 0x20), !!(b[5] & 0x10), !!(b[5] & 0x8), !!(b[5] & 0x4),
           !!(b[5] & 0x2), !!(b[5] & 0x1));
    printf("  Flag11h: %d  12h: %d  13h: %d  14h: %d  15h: %d  16h: %d  "
           "17h: %d  18h: %d\n", !!(b[6] & 0x80), !!(b[6] & 0x40),
           !!(b[6] & 0x20), !!(b[6] & 0x10), !!(b[6] & 0x8), !!(b[6] & 0x4),
           !!(b[6] & 0x2), !!(b[6] & 0x1));
    printf("  Flag19h: %d  1ah: %d  1bh: %d  1ch: %d  1dh: %d  1eh: %d  "
           "1fh: %d  20h: %d\n", !!(b[7] & 0x80), !!(b[7] & 0x40),
           !!(b[7] & 0x20), !!(b[7] & 0x10), !!(b[7] & 0x8), !!(b[7] & 0x4),
           !!(b[7] & 0x2), !!(b[7] & 0x1));
    printf("  Flag21h: %d  22h: %d  23h: %d  24h: %d  25h: %d  26h: %d  "
           "27h: %d  28h: %d\n", !!(b[8] & 0x80), !!(b[8] & 0x40),
           !!(b[8] & 0x20), !!(b[8] & 0x10), !!(b[8] & 0x8), !!(b[8] & 0x4),
           !!(b[8] & 0x2), !!(b[8] & 0x1));
    printf("  Flag29h: %d  2ah: %d  2bh: %d  2ch: %d  2dh: %d  2eh: %d  "
           "2fh: %d  30h: %d\n", !!(b[9] & 0x80), !!(b[9] & 0x40),
           !!(b[9] & 0x20), !!(b[9] & 0x10), !!(b[9] & 0x8), !!(b[9] & 0x4),
           !!(b[9] & 0x2), !!(b[9] & 0x1));
    printf("  Flag31h: %d  32h: %d  33h: %d  34h: %d  35h: %d  36h: %d  "
           "37h: %d  38h: %d\n", !!(b[10] & 0x80), !!(b[10] & 0x40),
           !!(b[10] & 0x20), !!(b[10] & 0x10), !!(b[10] & 0x8),
           !!(b[10] & 0x4), !!(b[10] & 0x2), !!(b[10] & 0x1));
    printf("  Flag39h: %d  3ah: %d  3bh: %d  3ch: %d  3dh: %d  3eh: %d  "
           "3fh: %d  40h: %d\n", !!(b[11] & 0x80), !!(b[11] & 0x40),
           !!(b[11] & 0x20), !!(b[11] & 0x10), !!(b[11] & 0x8),
           !!(b[11] & 0x4), !!(b[11] & 0x2), !!(b[11] & 0x1));
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
    printf("  User data segment size: %u\n", u);
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
    unsigned char * ucp;

    if (len < 4) {
        pr2serr("Supported block lengths and protection types VPD page "
                "length too short=%d\n", len);
        return SG_LIB_CAT_MALFORMED;
    }
    len -= 4;
    ucp = buff + 4;
    for (k = 0; k < len; k += 8, ucp += 8) {
        u = sg_get_unaligned_be32(ucp + 0);
        printf("  Logical block length: %u\n", u);
        printf("    P_I_I_SUP: %d\n", !!(ucp[4] & 0x40));
        printf("    NO_PI_CHK: %d\n", !!(ucp[4] & 0x8));  /* sbc4r05 */
        printf("    GRD_CHK: %d\n", !!(ucp[4] & 0x4));
        printf("    APP_CHK: %d\n", !!(ucp[4] & 0x2));
        printf("    REF_CHK: %d\n", !!(ucp[4] & 0x1));
        printf("    T3PS_SUP: %d\n", !!(ucp[5] & 0x8));
        printf("    T2PS_SUP: %d\n", !!(ucp[5] & 0x4));
        printf("    T1PS_SUP: %d\n", !!(ucp[5] & 0x2));
        printf("    T0PS_SUP: %d\n", !!(ucp[5] & 0x1));
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
    if (0xffffffff == u)
        printf("not reported\n");
    else
        printf("%" PRIu32 "\n", u);
    u = sg_get_unaligned_be32(b + 12);
    printf("  Optimal number of non-sequentially written sequential write "
           "preferred zones: ");
    if (0xffffffff == u)
        printf("not reported\n");
    else
        printf("%" PRIu32 "\n", u);
    u = sg_get_unaligned_be32(b + 16);
    printf("  Maximum number of open sequential write required zones: ");
    if (0xffffffff == u)
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
    "SPC",              /* withdrawn */
    "SPC-2",
    "SPC-3",
    "SPC-4",
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
    int res, len, k, verb, dev_pdt, pdt;
    unsigned char b[VPD_ATA_INFO_RESP_LEN];
    int sz;
    unsigned char * up;
    const struct sdparm_vpd_page_t * vpp;
    const char * vpd_name;
    int adc = 0;
    int sbc = 0;
    int ssc = 0;

    verb = (op->verbose > 0) ? op->verbose - 1 : 0;
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
            dStrHex((const char *)b, len + 4, 0);
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
            dStrHex((const char *)b, len + 4, 0);
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
            dStrHex((const char *)b, len + 4, 0);
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
            dStrHex((const char *)b, len + 4, 0);
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
            dStrHex((const char *)b, len + 4, 0);
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
            dStrHex((const char *)b, len + 4, 0);
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
            dStrHex((const char *)b, len + 4, 0);
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
            dStrHex((const char *)b, len + 4, 0);
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
            dStrHex((const char *)b, len + 4, 0);
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
            dStrHex((const char *)b, len + 4, 0);
            return 0;
        }
        res = decode_proto_port_vpd(b, len + 4);
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
            dStrHex((const char *)b, len + 4, 0);
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
            dStrHex((const char *)b, len + 4, 0);
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
            dStrHex((const char *)b, len + 4, 0);
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
        decode_3party_copy_vpd(b, len + 4, op->do_hex, op->verbose);
        break;
    case 0xb0:          /* VPD page depends on pdt */
        if (b[1] != pn)
            goto dumb_inq;
        switch (pdt) {
        case PDT_DISK: case PDT_WO: case PDT_OPTICAL: case PDT_ZBC:
            vpd_name = "Block limits";
            sbc = 1;
            break;
        case PDT_TAPE: case PDT_MCHANGER:
            vpd_name = "Sequential access device capabilities";
            ssc = 1;
            break;
        case PDT_OSD:
            vpd_name = "OSD information";
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
            dStrHex((const char *)b, len + 4, 0);
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
            vpd_name = "Block device characteristics";
            sbc = 1;
            break;
        case PDT_TAPE: case PDT_MCHANGER:
            vpd_name = "Manufactured assigned serial number";
            ssc = 1;
            break;
        case PDT_OSD:
            vpd_name = "Security token";
            /* osd = 1; */
            break;
        case PDT_ADC:
            vpd_name = "Manufactured assigned serial number";
            adc = 1;
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
            dStrHex((const char *)b, len + 4, 0);
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
            vpd_name = "Logical block provisioning";
            sbc = 1;
            break;
        case PDT_TAPE: case PDT_MCHANGER:
            vpd_name = "TapeAlert supported flags";
            ssc = 1;
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
            dStrHex((const char *)b, len + 4, 0);
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
            vpd_name = "Referrals";
            sbc = 1;
            break;
        case PDT_TAPE: case PDT_MCHANGER:
            vpd_name = "Automation device serial number";
            ssc = 1;
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
            dStrHex((const char *)b, len + 4, 0);
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
            vpd_name = "Supported block lengths and protection types";
            sbc = 1;
            break;
        case PDT_TAPE: case PDT_MCHANGER:
            vpd_name = "Data transfer device element address";
            ssc = 1;
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
            dStrHex((const char *)b, len + 4, 0);
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
            vpd_name = "Block device characteristics extension";
            sbc = 1;
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
            dStrHex((const char *)b, len + 4, 0);
            return 0;
        }
        res = 0;
        if (sbc)       /* added in sbc4r02 */
            res = decode_block_dev_char_ext_vpd(b, len + 4);
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
            sbc = 1;
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
            dStrHex((const char *)b, len + 4, 0);
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
            sbc = 1;
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
            dStrHex((const char *)b, len + 4, 0);
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
            pr2serr("%s VPD page in hex:\n", vpp->name);
        else
            pr2serr("VPD page 0x%x in hex:\n", pn);
        if (len > (int)sizeof(b)) {
            if (op->verbose)
                pr2serr("page length=%d too long, trim\n", len);
            len = sizeof(b);
        }
        dStrHexErr((const char *)b, len, 0);
        break;
    }
    return 0;

dumb_inq:
    pr2serr("malformed VPD response, VPD pages probably not supported\n");
    return SG_LIB_CAT_MALFORMED;
}

/*
 * Copyright (c) 2005-2014 Douglas Gilbert.
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
#include "sdparm.h"

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
                pr2serr("      << expect 8, 12 and 16 byte EUI, got %d>>\n",
                        i_len);
            printf("0x");
            for (m = 0; m < i_len; ++m)
                printf("%02x", (unsigned int)ip[m]);
            printf("\n");
            break;
        case 3: /* NAA <n> */
            naa = (ip[0] >> 4) & 0xff;
            if (1 != c_set) {
                pr2serr("      << unexpected code set %d for NAA=%d>>\n",
                        c_set, naa);
                dStrHexErr((const char *)ip, i_len, 0);
                break;
            }
            switch (naa) {
            case 2:     /* NAA 2: IEEE Extended */
                if (8 != i_len) {
                    pr2serr("      << unexpected NAA 2 identifier length: "
                            "0x%x>>\n", i_len);
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
                            "length: 0x%x>>\n", i_len);
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
                            "length: 0x%x>>\n", i_len);
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
                            "0x%x>>\n", i_len);
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
                        "%d>>\n", naa);
                dStrHexErr((const char *)ip, i_len, 0);
                break;
            }
            break;
        case 4: /* Relative target port */
            if ((0 == is_sas) || (1 != c_set) || (1 != assoc) || (4 != i_len))
                break;
            rtp = ((ip[2] << 8) | ip[3]);
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
                pr2serr("      << expected UTF-8 code_set>>\n");
                dStrHexErr((const char *)ip, i_len, 0);
                break;
            }
            /* does %s print out UTF-8 ok??
             * Seems to depend on the locale. Looks ok here with my
             * locale setting: en_AU.UTF-8
             */
            printf("%s\n", (const char *)ip);
            break;
        case 9: /* PCIe routing ID */
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
                              int long_out, int print_assoc)
{
    int m, p_id, piv, c_set, assoc, desig_type, ci_off, c_id, d_id, naa;
    int vsi, k;
    const unsigned char * ip;
    uint64_t vsei;
    uint64_t id_ext;
    char b[64];

    ip = ucp + 4;
    p_id = ((ucp[0] >> 4) & 0xf);
    c_set = (ucp[0] & 0xf);
    piv = ((ucp[1] & 0x80) ? 1 : 0);
    assoc = ((ucp[1] >> 4) & 0x3);
    desig_type = (ucp[1] & 0xf);
    if (print_assoc)
        printf("  %s:\n", sdparm_assoc_arr[assoc]);
    printf("    designator type: %s,  code set: %s\n",
           sdparm_desig_type_arr[desig_type], sdparm_code_set_arr[c_set]);
    if (piv && ((1 == assoc) || (2 == assoc)))
        printf("     transport: %s\n",
               sg_get_trans_proto_str(p_id, sizeof(b), b));
    /* printf("    associated with the %s\n", sdparm_assoc_arr[assoc]); */
    switch (desig_type) {
    case 0: /* vendor specific */
        k = 0;
        if ((1 == c_set) || (2 == c_set)) { /* ASCII or UTF-8 */
            for (k = 0; (k < i_len) && isprint(ip[k]); ++k)
                ;
            if (k >= i_len)
                k = 1;
        }
        if (k)
            printf("      vendor specific: %.*s\n", i_len, ip);
        else {
            pr2serr("      vendor specific:\n");
            dStrHexErr((const char *)ip, i_len, 0);
        }
        break;
    case 1: /* T10 vendor identification */
        printf("      vendor id: %.8s\n", ip);
        if (i_len > 8) {
            if ((2 == c_set) || (3 == c_set)) { /* ASCII or UTF-8 */
                printf("      vendor specific: %.*s\n", i_len - 8, ip + 8);
            } else {
                printf("      vendor specific: 0x");
                for (m = 8; m < i_len; ++m)
                    printf("%02x", (unsigned int)ip[m]);
                printf("\n");
            }
        }
        break;
    case 2: /* EUI-64 based */
        if (! long_out) {
            if ((8 != i_len) && (12 != i_len) && (16 != i_len)) {
                pr2serr("      << expect 8, 12 and 16 byte EUI, got %d>>\n",
                        i_len);
                dStrHexErr((const char *)ip, i_len, 0);
                break;
            }
            printf("      0x");
            for (m = 0; m < i_len; ++m)
                printf("%02x", (unsigned int)ip[m]);
            printf("\n");
            break;
        }
        printf("      EUI-64 based %d byte identifier\n", i_len);
        if (1 != c_set) {
            pr2serr("      << expected binary code_set (1)>>\n");
            dStrHexErr((const char *)ip, i_len, 0);
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
            printf("      Identifier extension: 0x%" PRIx64 "\n", id_ext);
        } else if ((8 != i_len) && (12 != i_len)) {
            pr2serr("      << can only decode 8, 12 and 16 byte ids>>\n");
            dStrHexErr((const char *)ip, i_len, 0);
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
        printf("      Vendor Specific Extension Identifier: 0x%" PRIx64
               "\n", vsei);
        if (12 == i_len) {
            d_id = (((unsigned int)ip[8] << 24) | (ip[9] << 16) |
                    (ip[10] << 8) | ip[11]);
            printf("      Directory ID: 0x%x\n", d_id);
        }
        break;
    case 3: /* NAA <n> */
        if (1 != c_set) {
            pr2serr("      << unexpected code set %d for NAA>>\n", c_set);
            dStrHexErr((const char *)ip, i_len, 0);
            break;
        }
        naa = (ip[0] >> 4) & 0xff;
        switch (naa) {
        case 2:         /* NAA 2: IEEE Extended */
            if (8 != i_len) {
                pr2serr("      << unexpected NAA 2 identifier length: "
                        "0x%x>>\n", i_len);
                dStrHexErr((const char *)ip, i_len, 0);
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
                printf("      [0x");
                for (m = 0; m < 8; ++m)
                    printf("%02x", (unsigned int)ip[m]);
                printf("]\n");
            }
            printf("      0x");
            for (m = 0; m < 8; ++m)
                printf("%02x", (unsigned int)ip[m]);
            printf("\n");
            break;
        case 3:         /* NAA 3: Locally assigned */
            if (8 != i_len) {
                pr2serr("      << unexpected NAA 3 identifier length: "
                        "0x%x>>\n", i_len);
                dStrHexErr((const char *)ip, i_len, 0);
                break;
            }
            if (long_out)
                printf("      NAA 3, Locally assigned:\n");
            printf("      0x");
            for (m = 0; m < 8; ++m)
                printf("%02x", (unsigned int)ip[m]);
            printf("\n");
            break;
        case 5:         /* NAA 5: IEEE Registered */
            if (8 != i_len) {
                pr2serr("      << unexpected NAA 5 identifier length: "
                        "0x%x>>\n", i_len);
                dStrHexErr((const char *)ip, i_len, 0);
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
                printf("      Vendor Specific Identifier: 0x%" PRIx64
                       "\n", vsei);
                printf("      [0x");
                for (m = 0; m < 8; ++m)
                    printf("%02x", (unsigned int)ip[m]);
                printf("]\n");
            } else {
                printf("      0x");
                for (m = 0; m < 8; ++m)
                    printf("%02x", (unsigned int)ip[m]);
                printf("\n");
            }
            break;
        case 6:         /* NAA 6: IEEE Registered extended */
            if (16 != i_len) {
                pr2serr("      << unexpected NAA 6 identifier length: "
                        "0x%x>>\n", i_len);
                dStrHexErr((const char *)ip, i_len, 0);
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
                printf("      Vendor Specific Identifier: 0x%" PRIx64
                       "\n", vsei);
                vsei = 0;
                for (m = 0; m < 8; ++m) {
                    if (m > 0)
                        vsei <<= 8;
                    vsei |= ip[8 + m];
                }
                printf("      Vendor Specific Identifier Extension: "
                       "0x%" PRIx64 "\n", vsei);
                printf("      [0x");
                for (m = 0; m < 16; ++m)
                    printf("%02x", (unsigned int)ip[m]);
                printf("]\n");
            } else {
                printf("      0x");
                for (m = 0; m < 16; ++m)
                    printf("%02x", (unsigned int)ip[m]);
                printf("\n");
            }
            break;
        default:
            pr2serr("      << unexpected NAA [0x%x]>>\n", naa);
            dStrHexErr((const char *)ip, i_len, 0);
            break;
        }
        break;
    case 4: /* Relative target port */
        if ((1 != c_set) || (1 != assoc) || (4 != i_len)) {
            pr2serr("      << expected binary code_set, target port "
                    "association, length 4>>\n");
            dStrHexErr((const char *)ip, i_len, 0);
            break;
        }
        d_id = ((ip[2] << 8) | ip[3]);
        printf("      Relative target port: 0x%x\n", d_id);
        break;
    case 5: /* (primary) Target port group */
        if ((1 != c_set) || (1 != assoc) || (4 != i_len)) {
            pr2serr("      << expected binary code_set, target port "
                    "association, length 4>>\n");
            dStrHexErr((const char *)ip, i_len, 0);
            break;
        }
        d_id = ((ip[2] << 8) | ip[3]);
        printf("      Target port group: 0x%x\n", d_id);
        break;
    case 6: /* Logical unit group */
        if ((1 != c_set) || (0 != assoc) || (4 != i_len)) {
            pr2serr("      << expected binary code_set, logical unit "
                    "association, length 4>>\n");
            dStrHexErr((const char *)ip, i_len, 0);
            break;
        }
        d_id = ((ip[2] << 8) | ip[3]);
        printf("      Logical unit group: 0x%x\n", d_id);
        break;
    case 7: /* MD5 logical unit identifier */
        if ((1 != c_set) || (0 != assoc)) {
            pr2serr("      << expected binary code_set, logical unit "
                    "association>>\n");
            dStrHexErr((const char *)ip, i_len, 0);
            break;
        }
        printf("      MD5 logical unit identifier:\n");
        dStrHex((const char *)ip, i_len, 0);
        break;
    case 8: /* SCSI name string */
        if (3 != c_set) {
            pr2serr("      << expected UTF-8 code_set>>\n");
            dStrHexErr((const char *)ip, i_len, 0);
            break;
        }
        printf("      SCSI name string:\n");
        /* does %s print out UTF-8 ok??
         * Seems to depend on the locale. Looks ok here with my
         * locale setting: en_AU.UTF-8
         */
        printf("      %s\n", (const char *)ip);
        break;
    case 9: /* Protocol specific port identifier */
        /* added in spc4r36, PIV must be set, proto_id indicates */
        /* whether UAS (USB) or SOP (PCIe) or ... */
        if (! piv)
            printf("      >>>> Protocol specific port identifier "
                   "expects protocol\n"
                   "           identifier to be valid and it is not\n");
        if (TPROTO_UAS == p_id) {
            printf("      USB device address: 0x%x\n", 0x7f & ip[0]);
            printf("      USB interface number: 0x%x\n", ip[2]);
        } else if (TPROTO_SOP == p_id) {
            printf("      PCIe routing ID, bus number: 0x%x\n", ip[0]);
            printf("          function number: 0x%x\n", ip[1]);
            printf("          [or device number: 0x%x, function number: "
                   "0x%x]\n", (0x1f & (ip[1] >> 3)), 0x7 & ip[1]);
        } else
            printf("      >>>> unexpected protocol indentifier: %s\n"
                   "           with Protocol specific port "
                   "identifier\n",
                   sg_get_trans_proto_str(p_id, sizeof(b), b));
        break;
    default: /* reserved */
        pr2serr("      reserved designator=0x%x\n", desig_type);
        dStrHexErr((const char *)ip, i_len, 0);
        break;
    }
}

/* VPD_DEVICE_ID  0x83 */
/* Prints outs device identification designators selected by association,
   designator type and/or code set. */
static int
decode_dev_ids(const char * print_if_found, unsigned char * buff, int len,
               int m_assoc, int m_desig_type, int m_code_set, int long_out,
               int quiet)
{
    int i_len, assoc, printed, off, u;
    const unsigned char * ucp;

    if (quiet)
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
            printf("  %s:\n", sdparm_assoc_arr[assoc]);
        decode_designation_descriptor(ucp, i_len, long_out, 0);
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
               sdparm_assoc_arr[(ucp[0] >> 5) & 0x3],
               sdparm_network_service_type_arr[ucp[0] & 0x1f]);
        na_len = (ucp[2] << 8) + ucp[3];
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
    int k, bump, j;
    uint64_t ull;

    for (k = 0; k < len; k += bump, ucp += bump) {
        bump = (ucp[2] << 8) + ucp[3];
        switch (ucp[0]) {
            case 0:
                /* Block ROD device type specific descriptor */
                printf("   Optimal block ROD length granularity: %d\n",
                       (ucp[6] << 8) + ucp[7]);
                ull = 0;
                for (j = 0; j < 8; j++) {
                    if (j > 0)
                        ull <<= 8;
                    ull |= ucp[8 + j];
                }
                printf("  Maximum Bytes in block ROD: %" PRIu64 "\n", ull);
                ull = 0;
                for (j = 0; j < 8; j++) {
                    if (j > 0)
                        ull <<= 8;
                    ull |= ucp[16 + j];
                }
                printf("  Optimal Bytes in block ROD transfer: %" PRIu64 "\n",
                       ull);
                ull = 0;
                for (j = 0; j < 8; j++) {
                    if (j > 0)
                        ull <<= 8;
                    ull |= ucp[24 + j];
                }
                printf("  Optimal Bytes to token per segment: %" PRIu64 "\n",
                       ull);
                ull = 0;
                for (j = 0; j < 8; j++) {
                    if (j > 0)
                        ull <<= 8;
                    ull |= ucp[32 + j];
                }
                printf("  Optimal Bytes from token per segment:"
                       " %" PRIu64 "\n", ull);
                break;
            case 1:
                /* Stream ROD device type specific descriptor */
                ull = 0;
                for (j = 0; j < 8; j++) {
                    if (j > 0)
                        ull <<= 8;
                    ull |= ucp[8 + j];
                }
                printf("  Maximum Bytes in stream ROD: %" PRIu64 "\n", ull);
                ull = 0;
                for (j = 0; j < 8; j++) {
                    if (j > 0)
                        ull <<= 8;
                    ull |= ucp[16 + j];
                }
                printf("  Optimal Bytes in stream ROD transfer:"
                       " %" PRIu64 "\n", ull);
                break;
            case 3:
                /* Copy manager ROD device type specific descriptor */
                ull = 0;
                for (j = 0; j < 8; j++) {
                    if (j > 0)
                        ull <<= 8;
                    ull |= ucp[8 + j];
                }
                printf("  Maximum Bytes in processor ROD:"
                       " %" PRIu64 "\n", ull);
                ull = 0;
                for (j = 0; j < 8; j++) {
                    if (j > 0)
                        ull <<= 8;
                    ull |= ucp[16 + j];
                }
                printf("  Optimal Bytes in processor ROD transfer:"
                       " %" PRIu64 "\n", ull);
                break;
            default:
                printf("  Unhandled descriptor (format %d, device type %d)\n",
                       ucp[0] >> 5, ucp[0] & 0x1F);
                break;
        }
    }
}

/* VPD_3PARTY_COPY (3PC, THIRD PARTY COPY, SPC-4, SBC-3)  0x8f */
static void
decode_3party_copy_vpd(unsigned char * buff, int len, int do_hex, int verbose)
{
    int j, k, bump, desc_type, desc_len, sa_len;
    unsigned int u;
    const unsigned char * ucp;
    uint64_t ull;
    char b[80];

    if (len < 4) {
        fprintf(stderr, "Third-party Copy VPD "
                "page length too short=%d\n", len);
        return;
    }
    len -= 4;
    ucp = buff + 4;
    for (k = 0; k < len; k += bump, ucp += bump) {
        desc_type = (ucp[0] << 8) + ucp[1];
        desc_len = (ucp[2] << 8) + ucp[3];
        if (verbose)
            printf("Descriptor type=%d, len %d\n", desc_type, desc_len);
        bump = 4 + desc_len;
        if ((k + bump) > len) {
            fprintf(stderr, "Third-party Copy VPD "
                    "page, short descriptor length=%d, left=%d\n", bump,
                    (len - k));
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
                       (ucp[10] << 8) + ucp[11]);
                u = (ucp[12] << 24) | (ucp[13] << 16) | (ucp[14] << 8) |
                    ucp[15];
                printf("  Maximum Inactivity Timeout: %u seconds\n", u);
                u = (ucp[16] << 24) | (ucp[17] << 16) | (ucp[18] << 8) |
                    ucp[19];
                printf("  Default Inactivity Timeout: %u seconds\n", u);
                ull = 0;
                for (j = 0; j < 8; j++) {
                    if (j > 0)
                        ull <<= 8;
                    ull |= ucp[20 + j];
                }
                printf("  Maximum Token Transfer Size: %" PRIu64 "\n", ull);
                ull = 0;
                for (j = 0; j < 8; j++) {
                    if (j > 0)
                        ull <<= 8;
                    ull |= ucp[28 + j];
                }
                printf("  Optimal Transfer Count: %" PRIu64 "\n", ull);
                break;
            case 0x0001:    /* Mandatory (SPC-4) */
                printf(" Supported Commands:\n");
                j = 0;
                while (j < ucp[4]) {
                    sa_len = ucp[6 + j];
                    for (k = 0; k < sa_len; k++) {
                        sg_get_opcode_sa_name(ucp[5 + j], ucp[7 + j + k],
                                              0, sizeof(b), b);
                        printf("   %s\n", b);
                    }
                    j += sa_len;
                }
                break;
            case 0x0004:
                printf(" Parameter Data:\n");
                printf("  Maximum CSCD Descriptor Count: %d\n",
                       (ucp[8] << 8) + ucp[9]);
                printf("  Maximum Segment Descriptor Count: %d\n",
                       (ucp[10] << 8) + ucp[11]);
                u = (ucp[12] << 24) | (ucp[13] << 16) | (ucp[14] << 8) |
                    ucp[15];
                printf("  Maximum Descriptor List Length: %u\n", u);
                u = (ucp[16] << 24) | (ucp[17] << 16) | (ucp[18] << 8) |
                    ucp[19];
                printf("  Maximum Inline Data Length: %u\n", u);
                break;
            case 0x0008:
                printf(" Supported Descriptors:\n");
                for (j = 0; j < ucp[4]; j++) {
                    printf("  0x%x\n", ucp[5 + j]);
                }
                break;
            case 0x000C:
                printf(" Supported CSCD IDs:\n");
                for (j = 0; j < (ucp[4] << 8) + ucp[5]; j += 2) {
                    u = (ucp[6 + j] << 8) | ucp[7 + j];
                    printf("  0x%04x\n", u);
                }
                break;
            case 0x0106:
                printf(" ROD Token Features:\n");
                printf("  Remote Tokens: %d\n", ucp[4] & 0x0f);
                u = (ucp[16] << 24) | (ucp[17] << 16) | (ucp[18] << 8) |
                    ucp[19];
                printf("  Minimum Token Lifetime: %u seconds\n", u);
                u = (ucp[20] << 24) | (ucp[21] << 16) | (ucp[22] << 8) |
                    ucp[23];
                printf("  Maximum Token Lifetime: %u seconds\n", u);
                u = (ucp[24] << 24) | (ucp[25] << 16) | (ucp[26] << 8) |
                    ucp[27];
                printf("  Maximum Token inactivity timeout: %d\n", u);
                decode_rod_descriptor(&ucp[48], (ucp[46] << 8) + ucp[47]);
                break;
            case 0x0108:
                printf(" Supported ROD Token and ROD Types:\n");
                for (j = 0; j < (ucp[6] << 8) + ucp[7]; j+= 64) {
                    u = (ucp[8 + j] << 24) | (ucp[8 + j + 1] << 16) |
                        (ucp[8 + j + 2] << 8) | ucp[8 + j + 3];
                    printf("  ROD Type %u:\n", u);
                    printf("    Internal: %s\n",
                           (ucp[8 + j + 4] & 0x80) ? "yes" : "no");
                    printf("    Token In: %s\n",
                           (ucp[8 + j + 4] & 0x02) ? "yes" : "no");
                    printf("    Token Out: %s\n",
                           (ucp[8 + j + 4] & 0x01) ? "yes" : "no");
                    printf("    Preference: %d\n",
                           (ucp[8 + j + 6] << 8) + ucp[8 + j + 7]);
                }
                break;
            case 0x8001:    /* Mandatory (SPC-4) */
                printf(" General Copy Operations:\n");
                u = (ucp[4] << 24) | (ucp[5] << 16) | (ucp[6] << 8) |
                    ucp[7];
                printf("  Total Concurrent Copies: %u\n", u);
                u = (ucp[8] << 24) | (ucp[9] << 16) | (ucp[10] << 8) |
                    ucp[11];
                printf("  Maximum Identified Concurrent Copies: %u\n", u);
                u = (ucp[12] << 24) | (ucp[13] << 16) | (ucp[14] << 8) |
                    ucp[15];
                printf("  Maximum Segment Length: %u\n", u);
                ull = (1 << ucp[16]);
                printf("  Data Segment Granularity: %" PRIu64 "\n", ull);
                ull = (1 << ucp[17]);
                printf("  Inline Data Granularity: %" PRIu64 "\n", ull);
                break;
            case 0x9101:
                printf(" Stream Copy Operations:\n");
                u = (ucp[4] << 24) | (ucp[5] << 16) | (ucp[6] << 8) |
                    ucp[7];
                printf("  Maximum Stream Device Transfer Size: %u\n", u);
                break;
            case 0xC001:
                printf(" Held Data:\n");
                u = (ucp[4] << 24) | (ucp[5] << 16) | (ucp[6] << 8) |
                    ucp[7];
                printf("  Held Data Limit: %u\n", u);
                ull = (1 << ucp[8]);
                printf("  Held Data Granularity: %" PRIu64 "\n", ull);
                break;
            default:
                fprintf(stderr, "Unexpected type=%d\n", desc_type);
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
        rel_port = (ucp[0] << 8) + ucp[1];
        printf("Relative port=%d\n", rel_port);
        proto = ucp[2] & 0xf;
        desc_len = (ucp[6] << 8) + ucp[7];
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
        rel_port = (ucp[0] << 8) + ucp[1];
        printf("Relative port=%d\n", rel_port);
        proto = ucp[2] & 0xf;
        desc_len = (ucp[6] << 8) + ucp[7];
        bump = 8 + desc_len;
        if ((k + bump) > len) {
            pr2serr("Protocol-specific port information VPD page, short "
                    "descriptor length=%d, left=%d\n", bump, (len - k));
            return SG_LIB_CAT_MALFORMED;
        }
        if (desc_len > 0) {
            switch (proto) {
            case TPROTO_SAS:    /* for SSP, added spl3r2 */
                printf(" pwr_d_s=%d\n", !!(ucp[3] & 0x1));
                pidp = ucp + 8;
                for (j = 0; j < desc_len; j += 4, pidp += 4)
                    printf("  phy id=%d, SSP persistent capable=%d\n",
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
decode_scsi_ports_vpd(unsigned char * buff, int len, int long_out, int quiet)
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
        rel_port = (ucp[2] << 8) + ucp[3];
        printf("Relative port=%d\n", rel_port);
        ip_tid_len = (ucp[6] << 8) + ucp[7];
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
        tpd_len = (ucp[bump + 2] << 8) + ucp[bump + 3];
        if ((k + bump + tpd_len + 4) > len) {
            pr2serr("SCSI Ports VPD page, short descriptor(tgt) length=%d, "
                    "left=%d\n", bump, (len - k));
            return SG_LIB_CAT_MALFORMED;
        }
        if (tpd_len > 0) {
            res = decode_dev_ids(" Target port descriptor(s)",
                                 ucp + bump + 4, tpd_len, VPD_ASSOC_TPORT,
                                 -1, -1, long_out, quiet);
            if (res)
                return res;
        }
        bump += tpd_len + 4;
    }
    return 0;
}

/* VPD_EXT_INQ  Extended Inquiry page  0x86 */
static int
decode_ext_inq_vpd(unsigned char * b, int len, int long_out, int protect)
{
    int n;

    if (len < 7) {
        pr2serr("Extended INQUIRY data VPD page length too short=%d\n", len);
        return SG_LIB_CAT_MALFORMED;
    }
    if (long_out) {
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
        printf("  P_I_I_SUP=%d\n", !!(b[7] & 0x10));
        printf("  LUICLR=%d\n", !!(b[7] & 0x1));
        printf("  R_SUP=%d\n", !!(b[8] & 0x10));
        printf("  CBCS=%d\n", !!(b[8] & 0x1));
        printf("  Multi I_T nexus microcode download=%d\n", b[9] & 0xf);
        printf("  Extended self-test completion minutes=%d\n",
               (b[10] << 8) + b[11]);
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
        printf("  WU_SUP=%d CRD_SUP=%d NV_SUP=%d V_SUP=%d\n",
               !!(b[6] & 0x8), !!(b[6] & 0x4),
               !!(b[6] & 0x2), !!(b[6] & 0x1));
        printf("  P_I_I_SUP=%d LUICLR=%d R_SUP=%d CBCS=%d\n",
               !!(b[7] & 0x10), !!(b[7] & 0x1),
               !!(b[8] & 0x10), !!(b[8] & 0x1));
        printf("  Multi I_T nexus microcode download=%d\n", b[9] & 0xf);
        printf("  Extended self-test completion minutes=%d\n",
               (b[10] << 8) + b[11]);
        printf("  POA_SUP=%d HRA_SUP=%d VSA_SUP=%d\n",      /* spc4r32 */
               !!(b[12] & 0x80), !!(b[12] & 0x40), !!(b[12] & 0x20));
        printf("  Maximum supported sense data length=%d\n",
               b[13]); /* spc4r34 */
    }
    return 0;
}

/* VPD_ATA_INFO  0x89 */
static int
decode_ata_info_vpd(unsigned char * buff, int len, int long_out, int do_hex)
{
    char b[80];
    int num, is_be;
    const char * cp;

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
    if (long_out) {
        printf("  Signature (Device to host FIS):\n");
        dStrHex((const char *)buff + 36, 20, 1);
    }
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
        if (long_out)
            printf("  ATA command IDENTIFY %sDEVICE response in hex:\n", cp);
    } else if (long_out)
        printf("  ATA command 0x%x got following response:\n",
               (unsigned int)buff[56]);
    if (len < 572)
        return SG_LIB_CAT_MALFORMED;
    if (do_hex)
        dStrHex((const char *)(buff + 60), 512, 0);
    else if (long_out)
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
            (buff[6] << 8) + buff[7]);
    printf("  Standby_z condition recovery time (ms) %d\n",
            (buff[8] << 8) + buff[9]);
    printf("  Standby_y condition recovery time (ms) %d\n",
            (buff[10] << 8) + buff[11]);
    printf("  Idle_a condition recovery time (ms) %d\n",
            (buff[12] << 8) + buff[13]);
    printf("  Idle_b condition recovery time (ms) %d\n",
            (buff[14] << 8) + buff[15]);
    printf("  Idle_c condition recovery time (ms) %d\n",
            (buff[16] << 8) + buff[17]);
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
        value = (ucp[2] << 8) + ucp[3];
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
    int m;
    uint64_t mwsl;

    if (len < 16) {
        pr2serr("Block limits VPD page length too short=%d\n", len);
        return SG_LIB_CAT_MALFORMED;
    }
    printf("  Write same no zero (WSNZ): %d\n", !!(buff[4] & 0x1));
    printf("  Maximum compare and write length: %u blocks\n", buff[5]);
    u = (buff[6] << 8) | buff[7];
    printf("  Optimal transfer length granularity: %u blocks\n", u);
    u = ((unsigned int)buff[8] << 24) | (buff[9] << 16) |
        (buff[10] << 8) | buff[11];
    printf("  Maximum transfer length: %u blocks\n", u);
    u = ((unsigned int)buff[12] << 24) | (buff[13] << 16) |
        (buff[14] << 8) | buff[15];
    printf("  Optimal transfer length: %u blocks\n", u);
    if (len > 19) {     /* added in sbc3r09 */
        u = ((unsigned int)buff[16] << 24) | (buff[17] << 16) |
            (buff[18] << 8) | buff[19];
        printf("  Maximum prefetch length: %u blocks\n", u);
        /* was 'Maximum prefetch transfer length' prior to sbc3r33 */
    }
    if (len > 27) {     /* added in sbc3r18 */
        u = ((unsigned int)buff[20] << 24) | (buff[21] << 16) |
            (buff[22] << 8) | buff[23];
        printf("  Maximum unmap LBA count: %u\n", u);
        u = ((unsigned int)buff[24] << 24) | (buff[25] << 16) |
            (buff[26] << 8) | buff[27];
        printf("  Maximum unmap block descriptor count: %u\n", u);
    }
    if (len > 35) {     /* added in sbc3r19 */
        u = ((unsigned int)buff[28] << 24) | (buff[29] << 16) |
            (buff[30] << 8) | buff[31];
        printf("  Optimal unmap granularity: %u\n", u);
        printf("  Unmap granularity alignment valid: %u\n",
               !!(buff[32] & 0x80));
        u = ((unsigned int)(buff[32] & 0x7f) << 24) | (buff[33] << 16) |
            (buff[34] << 8) | buff[35];
        printf("  Unmap granularity alignment: %u\n", u);
    }
    if (len > 43) {     /* added in sbc3r26 */
        mwsl = 0;
        for (m = 0; m < 8; ++m) {
            if (m > 0)
                mwsl <<= 8;
            mwsl |= buff[36 + m];
        }
        printf("  Maximum write same length: 0x%" PRIx64 " blocks\n", mwsl);
    }
    if (len > 44) {     /* added in sbc4r02 */
        u = ((unsigned int)buff[44] << 24) | (buff[45] << 16) |
            (buff[46] << 8) | buff[47];
        printf("  Maximum atomic transfer length: %u\n", u);
        u = ((unsigned int)buff[48] << 24) | (buff[49] << 16) |
            (buff[50] << 8) | buff[51];
        printf("  Atomic alignment: %u\n", u);
        u = ((unsigned int)buff[52] << 24) | (buff[53] << 16) |
            (buff[54] << 8) | buff[55];
        printf("  Atomic transfer length granularity: %u\n", u);
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
    u = (buff[4] << 8) | buff[5];
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
    printf("  HAW_ZBC=%d\n", !!(buff[8] & 0x10));       /* sbc4r01 */
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
decode_block_lb_prov_vpd(unsigned char * b, int len)
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
           !!(0x4 & b[5]));
    printf("  Anchored LBAs supported (ANC_SUP): %d\n", !!(0x2 & b[5]));
    printf("  Threshold exponent: %d\n", b[4]);
    dp = !!(b[5] & 0x1);
    printf("  Descriptor present: %d\n", dp);
    printf("  Provisioning type: %d\n", b[6] & 0x7);
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
        decode_designation_descriptor(ucp, i_len, 0, 1);
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
    u = ((unsigned int)b[8] << 24) | (b[9] << 16) | (b[10] << 8) | b[11];
    printf("  User data segment size: %u\n", u);
    u = ((unsigned int)b[12] << 24) | (b[13] << 16) |
        (b[14] << 8) | b[15];
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
        u = ((unsigned int)ucp[0] << 24) | (ucp[1] << 16) | (ucp[2] << 8) |
            ucp[3];
        printf("  Logical block length: %u\n", u);
        printf("    P_I_I_SUP: %d\n", !!(ucp[4] & 0x40));
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
    unsigned int u;

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
    u = ((unsigned int)b[8] << 24) | (b[9] << 16) | (b[10] << 8) | b[11];
    printf("  Utilization B: %u\n", u);
    u = ((unsigned int)b[12] << 24) | (b[13] << 16) | (b[14] << 8) | b[15];
    printf("  Utilization A: %u\n", u);
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
    sz = op->long_out ? sizeof(b) : 36;
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
    if (op->hex) {
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
                     int protect)
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
    if (pn < 0) {
        if (VPD_NOT_STD_INQ == pn)
            return decode_std_inq(sg_fd, op);
        else if (op->all)
            pn = VPD_SUPPORTED_VPDS;  /* if '--all' list supported vpds */
        else
            pn = VPD_DEVICE_ID;  /* default to device identification page */
    }
    sz = (VPD_ATA_INFO == pn) ? VPD_ATA_INFO_RESP_LEN : DEF_INQ_RESP_LEN;
    res = sg_ll_inquiry(sg_fd, 0, 1, pn, b, sz, 0, verb);
    if (res) {
        pr2serr("INQUIRY fetching VPD page=0x%x failed\n", pn);
        return res;
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
        len = (b[2] << 8) + b[3];       /* spc4r25 */
        printf("Supported VPD pages VPD page:\n");
        if (op->hex) {
            dStrHex((const char *)b, len + 4, 0);
            return 0;
        }
        if (len > 0) {
            for (k = 0; k < len; ++k) {
                vpp = sdp_get_vpd_detail(b[4 + k], -1, pdt);
                if (vpp) {
                    if (op->long_out)
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
        len = (b[2] << 8) + b[3];
        if (len > sz) {
            pr2serr("Response to ATA information VPD page truncated\n");
            len = sz;
        }
        if (3 == op->hex) {   /* output suitable for "hdparm --Istdin" */
            dWordHex((const unsigned short *)(b + 60), 256, -2,
                     sg_is_big_endian());
            return 0;
        }
        if (op->long_out)
            printf("ATA information [0x89] VPD page:\n");
        else
            printf("ATA information VPD page:\n");
        if (op->hex && (2 != op->hex)) {
            dStrHex((const char *)b, len + 4, 0);
            return 0;
        }
        res = decode_ata_info_vpd(b, len + 4, op->long_out, op->hex);
        if (res)
            return res;
        break;
    case VPD_DEVICE_ID:         /* 0x83 */
        if (b[1] != pn)
            goto dumb_inq;
        len = (b[2] << 8) + b[3];
        if (len > sz) {
            pr2serr("Response to device identification VPD page truncated\n");
            len = sz;
        }
        if (op->long_out)
            printf("Device identification [0x83] VPD page:\n");
        else if (! op->quiet)
            printf("Device identification VPD page:\n");
        if (op->hex) {
            dStrHex((const char *)b, len + 4, 0);
            return 0;
        }
        res = 0;
        if ((0 == spn) || (VPD_DI_SEL_LU & spn))
            res = decode_dev_ids(sdparm_assoc_arr[VPD_ASSOC_LU], b + 4, len,
                                 VPD_ASSOC_LU, -1, -1, op->long_out,
                                 op->quiet);
        if ((0 == spn) || (VPD_DI_SEL_TPORT & spn))
            res = decode_dev_ids(sdparm_assoc_arr[VPD_ASSOC_TPORT], b + 4,
                                 len, VPD_ASSOC_TPORT, -1, -1,
                                 op->long_out, op->quiet);
        if ((0 == spn) || (VPD_DI_SEL_TARGET & spn))
            res = decode_dev_ids(sdparm_assoc_arr[VPD_ASSOC_TDEVICE], b + 4,
                                 len, VPD_ASSOC_TDEVICE, -1, -1,
                                 op->long_out, op->quiet);
        if (VPD_DI_SEL_AS_IS & spn)
            res = decode_dev_ids(NULL, b + 4, len, -1, -1, -1,
                                 op->long_out, op->quiet);
        if (res)
            return res;
        break;
    case VPD_EXT_INQ:           /* 0x86 */
        if (b[1] != pn)
            goto dumb_inq;
        len = (b[2] << 8) + b[3];   /* spc4r25 */
        if (len > sz) {
            pr2serr("Response to Extended inquiry data VPD page truncated\n");
            len = sz;
        }
        if (op->long_out)
            printf("Extended inquiry data [0x86] VPD page:\n");
        else if (! op->quiet)
            printf("Extended inquiry data VPD page:\n");
        if (op->hex) {
            dStrHex((const char *)b, len + 4, 0);
            return 0;
        }
        res = decode_ext_inq_vpd(b, len + 4, op->long_out, protect);
        if (res)
            return res;
        break;
    case VPD_MAN_NET_ADDR:              /* 0x85 */
        if (b[1] != pn)
            goto dumb_inq;
        len = (b[2] << 8) + b[3];
        if (len > sz) {
            pr2serr("Response to Management network addresses VPD page "
                    "truncated\n");
            len = sz;
        }
        if (op->long_out)
            printf("Management network addresses [0x85] VPD page:\n");
        else
            printf("Management network addresses VPD page:\n");
        if (op->hex) {
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
        len = (b[2] << 8) + b[3];
        if (len > sz) {
            pr2serr("Response to Mode page policy VPD page truncated\n");
            len = sz;
        }
        if (op->long_out)
            printf("Mode page policy [0x87] VPD page:\n");
        else
            printf("mode page policy VPD page:\n");
        if (op->hex) {
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
        len = (b[2] << 8) + b[3];
        if (len > sz) {
            pr2serr("Response to Power condition VPD page truncated\n");
            len = sz;
        }
        if (op->long_out)
            printf("Power condition [0x8a] VPD page:\n");
        else if (! op->quiet)
            printf("Power condition VPD page:\n");
        if (op->hex) {
            dStrHex((const char *)b, len + 4, 0);
            return 0;
        }
        res = decode_power_condition(b, len + 4);
        if (res)
            return res;
        break;
    case VPD_POWER_CONSUMPTION:         /* 0x8d */
        if (b[1] != pn)
            goto dumb_inq;
        len = (b[2] << 8) + b[3];
        if (len > sz) {
            pr2serr( "Response to Power consumption VPD page truncated\n");
            len = sz;
        }
        if (op->long_out)
            printf("Power consumption [0x8d] VPD page:\n");
        else
            printf("Power consumption VPD page:\n");
        if (op->hex) {
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
        len = (b[2] << 8) + b[3];
        if (len > sz) {
            pr2serr("Response to Protocol-specific logical unit information "
                    "VPD page truncated\n");
            len = sz;
        }
        if (op->long_out)
            printf("Protocol-specific logical unit information [0x90] VPD "
                   "page:\n");
        else
            printf("Protocol-specific logical unit information VPD page:\n");
        if (op->hex) {
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
        len = (b[2] << 8) + b[3];
        if (len > sz) {
            pr2serr("Response to Protocol-specific port information VPD page "
                    "truncated\n");
            len = sz;
        }
        if (op->long_out)
            printf("Protocol-specific port information [0x91] VPD "
                   "page:\n");
        else
            printf("Protocol-specific port information VPD page:\n");
        if (op->hex) {
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
        len = (b[2] << 8) + b[3];
        if (len > sz) {
            pr2serr("Response to SCSI Ports VPD page truncated\n");
            len = sz;
        }
        if (op->long_out)
            printf("SCSI Ports [0x88] VPD page:\n");
        else
            printf("SCSI Ports VPD page:\n");
        if (op->hex) {
            dStrHex((const char *)b, len + 4, 0);
            return 0;
        }
        res = decode_scsi_ports_vpd(b, len + 4, op->long_out, op->quiet);
        if (res)
            return res;
        break;
    case VPD_SOFTW_INF_ID:              /* 0x84 */
        if (b[1] != pn)
            goto dumb_inq;
        len = (b[2] << 8) + b[3];       /* spc4r25 */
        if (len > sz) {
            pr2serr("Response to Software interface identification VPD page "
                    "truncated\n");
            len = sz;
        }
        if (op->long_out)
            printf("Software interface identification [0x84] VPD page:\n");
        else
            printf("Software interface identification VPD page:\n");
        if (op->hex) {
            dStrHex((const char *)b, len + 4, 0);
            return 0;
        }
        up = b + 4;
        for ( ; len > 5; len -= 6, up += 6)
            printf("    IEEE Company_id: 0x%06x, vendor specific extension "
                   "id: 0x%06x\n", (up[0] << 16) | (up[1] << 8) | up[2],
                   (up[3] << 16) | (up[4] << 8) | up[5]);
        break;
    case VPD_UNIT_SERIAL_NUM:           /* 0x80 */
        if (b[1] != pn)
            goto dumb_inq;
        len = (b[2] << 8) + b[3];       /* spc4r25 */
        if (op->long_out)
            printf("Unit serial number [0x80] VPD page:\n");
        else
            printf("Unit serial number VPD page:\n");
        if (op->hex)
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
        len = (b[2] << 8) + b[3];       /* spc4r25 */
        if (len > sz) {
            pr2serr("Response to Third party copy VPD page truncated\n");
            len = sz;
        }
        if (op->long_out)
            printf("Third party copy [0x8f] VPD page:\n");
        else
            printf("Third party copy VPD page:\n");
        if (1 == op->hex) {
            dStrHex((const char *)b, len + 4, 0);
            return 0;
        }
        decode_3party_copy_vpd(b, len + 4, op->hex, op->verbose);
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
        len = (b[2] << 8) + b[3];
        if (len > sz) {
            pr2serr("Response to %s VPD page truncated\n", vpd_name);
            len = sz;
        }
        if (op->long_out)
            printf("%s [0xb0] VPD page:\n", vpd_name);
        else
            printf("%s VPD page:\n", vpd_name);
        if (op->hex) {
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
        len = (b[2] << 8) + b[3];
        if (len > sz) {
            pr2serr("Response to %s VPD page truncated\n", vpd_name);
            len = sz;
        }
        if (op->long_out)
            printf("%s [0xb1] VPD page:\n", vpd_name);
        else
            printf("%s VPD page:\n", vpd_name);
        if (op->hex) {
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

        len = (b[2] << 8) + b[3];
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
        if (op->long_out)
            printf("%s [0xb2] VPD page:\n", vpd_name);
        else
            printf("%s VPD page:\n", vpd_name);
        if (op->hex) {
            dStrHex((const char *)b, len + 4, 0);
            return 0;
        }
        res = 0;
        if (ssc)
            res = decode_tapealert_supported_vpd(b, len + 4);
        else if (sbc)       /* added in sbc3r20 */
            res = decode_block_lb_prov_vpd(b, len + 4);
        else
            dStrHex((const char *)b, len + 4, 0);
        if (res)
            return res;
        break;
    case 0xb3:          /* VPD page depends on pdt */
        if (b[1] != pn)
            goto dumb_inq;
        len = (b[2] << 8) + b[3];
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
        if (op->long_out)
            printf("%s [0xb3] VPD page:\n", vpd_name);
        else
            printf("%s VPD page:\n", vpd_name);
        if (op->hex) {
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
        len = (b[2] << 8) + b[3];
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
        if (op->long_out)
            printf("%s [0xb4] VPD page:\n", vpd_name);
        else
            printf("%s VPD page:\n", vpd_name);
        if (op->hex) {
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
        len = (b[2] << 8) + b[3];
        switch (pdt) {
        case PDT_DISK: case PDT_WO: case PDT_OPTICAL: case PDT_ZBC:
            vpd_name = "Block device characteristics extension";
            sbc = 1;
            break;
        default:
            vpd_name = "unexpected pdt for B5h";
            break;
        }
        if (op->long_out)
            printf("%s [0xb5] VPD page:\n", vpd_name);
        else
            printf("%s VPD page:\n", vpd_name);
        if (op->hex) {
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
    default:
        if (b[1] != pn)
            goto dumb_inq;
        len = (b[2] << 8) + b[3] + 4;
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

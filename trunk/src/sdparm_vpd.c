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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <scsi/sg_lib.h>
#include <scsi/sg_cmds.h>

#include "sdparm.h"

/* sdparm_vpd.c : does mainly VPD page processing associated with the
 * INQUIRY SCSI command.
 */


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
    printf("  SPT: %d  GRD_CHK: %d  APP_CHK: %d  REF_CHK: %d\n",
           ((buff[4] >> 3) & 0x7), !!(buff[4] & 0x4), !!(buff[4] & 0x2),
           !!(buff[4] & 0x1));
    printf("  GRP_SUP: %d  PRIOR_SUP: %d  HEADSUP: %d  ORDSUP: %d  "
           "SIMPSUP: %d\n", !!(buff[5] & 0x10), !!(buff[5] & 0x8),
           !!(buff[5] & 0x4), !!(buff[5] & 0x2), !!(buff[5] & 0x1));
    printf("  CORR_D_SUP=%d NV_SUP=%d V_SUP=%d\n", !!(buff[6] & 0x80),
           !!(buff[6] & 0x2), !!(buff[6] & 0x1));
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
    if (do_hex) /* here for '-HH' option */
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

/* Returns 0 if successful, else error */
int sdp_process_vpd_page(int sg_fd, int pn, int spn,
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
                cp = sdp_get_vpd_name(b[4 + k]);
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
        if (3 == opts->hex) {   /* output suitable for "hdparm --Istdin" */
            dWordHex((const unsigned short *)(b + 60), 256, -2,
                     sg_is_big_endian());
            return 0;
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
        if ((0 == spn) || (VPD_DI_SEL_LU & spn)) {
            res = decode_dev_ids(sdparm_assoc_arr[VPD_ASSOC_LU], b + 4, len,
                                 VPD_ASSOC_LU, opts->long_out);
            if (res)
                return res;
        }
        if ((0 == spn) || (VPD_DI_SEL_TPORT & spn)) {
            res = decode_dev_ids(sdparm_assoc_arr[VPD_ASSOC_TPORT], b + 4,
                                 len, VPD_ASSOC_TPORT, opts->long_out);
            if (res)
                return res;
        }
        if ((0 == spn) || (VPD_DI_SEL_TARGET & spn)) {
            res = decode_dev_ids(sdparm_assoc_arr[VPD_ASSOC_TDEVICE], b + 4,
                                 len, VPD_ASSOC_TDEVICE, opts->long_out);
            if (res)
                return res;
        }
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
        cp = sdp_get_vpd_name(pn);
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

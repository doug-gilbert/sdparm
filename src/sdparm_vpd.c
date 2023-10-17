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
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sg_lib.h"
#include "sg_lib_data.h"
#include "sg_cmds_basic.h"
#include "sg_unaligned.h"
#include "sdparm.h"
#include "sg_pr2serr.h"

/* sdparm_vpd.c : does mainly VPD page processing associated with the
 * INQUIRY SCSI command.
 */

static const char * ns_s = "not supported";
static const char * null_s = "";
static const char * lts_s = "length too short";
static const char * vpd_pg_s = "VPD page";
static const char * nlr_s = "no limit reported";
static char rsv_s[] = "Reserved";
static char nr_s[] = "not reported";
static char updt_s[] = "unexpected pdt for";
static const char * const y_s = "yes";
static const char * const n_s = "no";
static const char * nl_s = "no limit";
static const char * mn_s = "meaning";
static const char * vs_s = "Vendor specific";
static const char * t10_vendor_id_hr = "T10_vendor_identification";
static const char * t10_vendor_id_sn = "t10_vendor_identification";
static const char * product_id_hr = "Product_identification";
static const char * product_id_sn = "product_identification";
static const char * product_rev_lev_hr = "Product_revision_level";
static const char * product_rev_lev_sn = "product_revision_level";

static const char * di_vpdp = "Device identification VPD page";
static const char * sp_vpdp = "SCSI ports VPD page";
static const char * svp_vpdp = "Supported VPD pages VPD page";
static const char * usn_vpdp = "Unit serial number VPD page";
static const char * ai_vpdp = "ATA information VPD page";
static const char * eid_vpdp = "Extended inquiry data VPD page";
static const char * const sii_vpdp =
                        "Software interface identification VPD page";
static const char * mna_vpdp = "Management network addresses VPD page";
static const char * mpp_vpdp = "Mode page policy VPD page";
static const char * pc_vpdp = "Power condition VPD page";
static const char * sfs_vpdp = "SCSI feature sets VPD page";
static const char * dc_vpdp = "Device constituents VPD page";
static const char * psm_vpdp = "Power consumption VPD page";
static const char * cpi_vpdp = "CFA profile information VPD page";
static const char * pslu_vpdp = "Protocol-specific logical unit information "
                        "VPD page";
static const char * pspo_vpdp = "Protocol-specific port information VPD page";
static const char * tpc_vpdp = "Third party copy VPD page";
static const char * bl_vpdp = "Block limits VPD page";
static const char * sad_vpdp =
                        "Sequential-access device capabilities VPD page";
static const char * osdi_vpdp = "OSD information VPD page";
static const char * bdc_vpdp = "Block device characteristics VPD page";
static const char * masn_vpdp =
                        "Manufactured-assigned serial number VPD page";
static const char * st_vpdp = "Security token VPD page";
static const char * lbpv_vpdp = "Logical block provisioning VPD page";
static const char * tas_vpdp = "TapeAlert supported flags VPD page";
static const char * ref_vpdp = "Referrals VPD page";
static const char * adsn_vpdp = "Automation device serial number VPD page";
static const char * sbl_vpdp = "Supported block lengths and protection types "
                        "VPD page";
static const char * dtde_vpdp =
                         "Data transfer device element address VPD page";
static const char * bdce_vpdp =
                        "Block device characteristics extension VPD page";
static const char * lbpro_vpdp = "Logical block protection VPD page";
static const char * zbdc_vpdp = "Zoned block device characteristics VPD page";
static const char * ble_vpdp = "Block limits extension VPD page";
static const char * fp_vpdp = "Format presets VPD page";
static const char * cpr_vpdp = "Concurrent positioning ranges VPD page";
static const char * cap_vpdp =
                "Capacity/Product identification mapping VPD page";

static const char * pqual_str(int pqual);


sgj_opaque_p
sg_vpd_js_hdr(sgj_state * jsp, sgj_opaque_p jop, const char * name,
              const uint8_t * vpd_hdrp)
{
    int pdt = vpd_hdrp[0] & PDT_MASK;
    int pqual = (vpd_hdrp[0] & 0xe0) >> 5;
    int pn = vpd_hdrp[1];
    const char * pdt_str;
    sgj_opaque_p jo2p = sgj_snake_named_subobject_r(jsp, jop, name);
    char d[64];

    pdt_str = sg_get_pdt_str(pdt, sizeof(d), d);
    sgj_js_nv_ihexstr(jsp, jo2p, "peripheral_qualifier",
                      pqual, NULL, pqual_str(pqual));
    sgj_js_nv_ihexstr(jsp, jo2p, "peripheral_device_type",
                      pdt, NULL, pdt_str);
    sgj_js_nv_ihex(jsp, jo2p, "page_code", pn);
    return jo2p;
}

static void
sgjv_js_hex_long(sgj_state * jsp, sgj_opaque_p jop, const uint8_t * bp,
                 int len)
{
    bool gt256 = (len > 256);
    int k, rem;
    sgj_opaque_p jo2p = NULL;
    sgj_opaque_p jap = NULL;

    if (gt256)
        jap = sgj_named_subarray_r(jsp, jop, "in_hex_list");
    for (k = 0; k < len; bp += 256, k += 256) {
        rem = len - k;
        if (gt256)
            jo2p = sgj_new_unattached_object_r(jsp);
        else
            jo2p = jop;
        sgj_js_nv_hex_bytes(jsp, jo2p, "in_hex", bp,
                            (rem > 256) ? 256 : rem);
        if (gt256)
            sgj_js_nv_o(jsp, jap, NULL /* name */, jo2p);
    }
}

static void
named_hhh_output(const char * pname, const uint8_t * b, int blen,
                 const struct sdparm_opt_coll * op)
{
    if (op->do_hex > 4) {
        if (pname)
            printf("\n# %s\n", pname);
        else
            printf("\n# VPD page 0x%x\n", b[1]);
    }
    hex2stdout(b, blen, -1);
}

static void             /* VPD_SUPPORTED_VPDS  ["sv"] */
decode_supported_vpd(uint8_t * buff, int len, struct sdparm_opt_coll * op,
                     sgj_opaque_p jap)
{
    uint8_t pn;
    int k, rlen, pdt;
    sgj_state * jsp = &op->json_st;
    sgj_opaque_p jo2p;
    const struct sdparm_vpd_page_t * vpp;

    uint8_t * bp;
    char b[144];
    static const int blen = sizeof(b);

    pdt = PDT_MASK & buff[0];
    rlen = buff[3] + 4;
    if (rlen > len)
        pr2serr("%s truncated, indicates %d, got %d\n", svp_vpdp, rlen,
                len);
    else
        len = rlen;
    if (len < 4) {
        pr2serr("%s %s=%d\n", svp_vpdp,  lts_s, len);
        return;
    }
    len -= 4;
    bp = buff + 4;

    for (k = 0; k < len; ++k) {
        pn = bp[k];
        snprintf(b, blen, "0x%02x", pn);
        vpp = sdp_get_vpd_detail(pn, -1, pdt);
        if (vpp) {
            if (op->do_long)
                sgj_pr_hr(jsp, "  %s  %s [%s]\n", b, vpp->name,
                          vpp->vpd_acron);
            else
                sgj_pr_hr(jsp, "  %s [%s]\n", vpp->name, vpp->vpd_acron);
        } else
            sgj_pr_hr(jsp, "  %s\n", b);
        if (jsp->pr_as_json) {
            jo2p = sgj_new_unattached_object_r(jsp);
            sgj_js_nv_i(jsp, jo2p, "i", pn);
            sgj_js_nv_s(jsp, jo2p, "hex", b + 2);
            if (vpp) {
                sgj_js_nv_s(jsp, jo2p, "name", vpp->name);
                sgj_js_nv_s(jsp, jo2p, "acronym", vpp->vpd_acron);
            } else {
                sgj_js_nv_s(jsp, jo2p, "name", "unknown");
                sgj_js_nv_s(jsp, jo2p, "acronym", "unknown");
            }
            sgj_js_nv_o(jsp, jap, NULL /* name */, jo2p);
        }
    }
}

/* VPD_DEVICE_ID  0x83 */
/* Prints outs an abridged set of device identification designators
   selected by association, designator type and/or code set. */
static int
decode_dev_ids_quiet(uint8_t * buff, int len, int m_assoc,
                     int m_desig_type, int m_code_set)
{
    int m, p_id, c_set, assoc, desig_type, i_len, naa, off, u, rtp;
    bool piv, is_sas;
    const uint8_t * bp;
    const uint8_t * ip;
    uint8_t sas_tport_addr[8];
    static const int sas_tport_addr_sz = sizeof(sas_tport_addr);

    rtp = 0;
    memset(sas_tport_addr, 0, sas_tport_addr_sz);
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
                hex2stderr(ip, i_len, 0);
                break;
            }
            switch (naa) {
            case 2:     /* NAA 2: IEEE Extended */
                if (8 != i_len) {
                    pr2serr("      << unexpected NAA 2 identifier length: "
                            "0x%x >>\n", i_len);
                    hex2stderr(ip, i_len, 0);
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
                    hex2stderr(ip, i_len, 0);
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
                    hex2stderr(ip, i_len, 0);
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
                    memcpy(sas_tport_addr, ip, sas_tport_addr_sz);
                }
                break;
            case 6:     /* NAA 5: IEEE Registered Extended */
                if (16 != i_len) {
                    pr2serr("      << unexpected NAA 6 identifier length: "
                            "0x%x >>\n", i_len);
                    hex2stderr(ip, i_len, 0);
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
                hex2stderr(ip, i_len, 0);
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
                memset(sas_tport_addr, 0, sas_tport_addr_sz);
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
                hex2stderr(ip, i_len, 0);
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
        case 0xa: /* UUID identifier [spc5r08] RFC 4122 */
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

static int
decode_json_dev_ids(uint8_t * buff, int len, int m_assoc,
                    struct sdparm_opt_coll * op, sgj_opaque_p jap)
{
    int u, off, i_len;
    sgj_opaque_p jo2p;
    const uint8_t * bp;
    sgj_state * jsp = &op->json_st;

    off = -1;
    while ((u = sg_vpd_dev_id_iter(buff, len, &off, m_assoc, -1, -1)) == 0) {
        bp = buff + off;
        i_len = bp[3];
        if ((off + i_len + 4) > len) {
            pr2serr("    %s error: designator length longer than remaining\n"
                    "     response length=%d\n", vpd_pg_s, (len - off));
            return SG_LIB_CAT_MALFORMED;
        }
        jo2p = sgj_new_unattached_object_r(jsp);
        sgj_js_designation_descriptor(jsp, jo2p, bp, i_len + 4);
        sgj_js_nv_o(jsp, jap, NULL /* name */, jo2p);
    }
    if (-2 == u) {
        pr2serr("%s error: short designator around offset %d\n", vpd_pg_s,
                off);
        return SG_LIB_CAT_MALFORMED;
    }
    return 0;
}

/* VPD_DEVICE_ID  0x83 */
/* Prints outs designation descriptors (dd_s) selected by association,
   designator type and/or code set. VPD_DEVICE_ID and VPD_SCSI_PORTS */
static int
decode_dev_ids(const char * print_if_found, int num_leading, uint8_t * buff,
               int len, int m_assoc, int m_desig_type, int m_code_set,
               struct sdparm_opt_coll * op, sgj_opaque_p jap)
{
    bool printed, sgj_out_hr;
    int assoc, off, u, i_len;
    const uint8_t * bp;
    sgj_state * jsp = &op->json_st;
    char b[1024];
    char sp[82];
    static const int blen = sizeof(b);

    if (op->do_quiet && (! jsp->pr_as_json))
        return decode_dev_ids_quiet(buff, len, m_assoc, m_desig_type,
                                    m_code_set);
    sgj_out_hr = false;
    if (jsp->pr_as_json) {
        int ret = decode_json_dev_ids(buff, len, m_assoc, op, jap);

        if (ret || (! jsp->pr_out_hr))
            return ret;
        sgj_out_hr = true;
    }
    if (num_leading > (int)(sizeof(sp) - 2))
        num_leading = sizeof(sp) - 2;
    if (num_leading > 0)
        snprintf(sp, sizeof(sp), "%*c", num_leading, ' ');
    else
        sp[0] = '\0';
    if (buff[2] != 0) { /* all valid dd_s should have 0 in this byte */
        if (op->verbose)
            pr2serr("%s: designation descriptors byte 2 should be 0\n"
                    "perhaps this is a standard inquiry response, ignore\n",
                    __func__);
        return 0;
    }
    off = -1;
    printed = false;
    while ((u = sg_vpd_dev_id_iter(buff, len, &off, m_assoc, m_desig_type,
                                   m_code_set)) == 0) {
        bp = buff + off;
        i_len = bp[3];
        if ((off + i_len + 4) > len) {
            pr2serr("    %s error: designator length longer than\n"
                    "     remaining response length=%d\n", vpd_pg_s,
                    (len - off));
            return SG_LIB_CAT_MALFORMED;
        }
        assoc = ((bp[1] >> 4) & 0x3);
        if (print_if_found && (! printed)) {
            printed = true;
            if (strlen(print_if_found) > 0) {
                snprintf(b, blen, "  %s:", print_if_found);
                if (sgj_out_hr)
                    sgj_hr_str_out(jsp, b, strlen(b));
                else
                    printf("%s\n", b);
            }
        }
        if (NULL == print_if_found) {
            snprintf(b, blen, "  %s%s:", sp, sg_get_desig_assoc_str(assoc));
            if (sgj_out_hr)
                sgj_hr_str_out(jsp, b, strlen(b));
            else
                printf("%s\n", b);
        }
        sg_get_designation_descriptor_str(sp, bp, i_len + 4, false,
                                          op->do_long, blen, b);
        if (sgj_out_hr)
            sgj_hr_str_out(jsp, b, strlen(b));
        else
            printf("%s", b);
    }
    if (-2 == u) {
        pr2serr("%s error: short designator around offset %d\n", vpd_pg_s,
                 off);
        return SG_LIB_CAT_MALFORMED;
    }
    return 0;
}

static const char * mode_page_policy_arr[] =
{
    "shared",
    "per target port",
    "per initiator port",
    "per I_T nexus",
};

/* VPD_MODE_PG_POLICY  0x87 ["mpp"] */
static void
decode_mode_policy_vpd(const uint8_t * buff, int len,
                       struct sdparm_opt_coll * op, sgj_opaque_p jap)
{
    int k, n, bump, ppc, pspc;
    sgj_state * jsp = &op->json_st;
    sgj_opaque_p jo2p;
    const uint8_t * bp;
    char b[128];
    static const int blen = sizeof(b);

    if (len < 4) {
        pr2serr("%s length too short=%d\n", mpp_vpdp, len);
        return;
    }
    len -= 4;
    bp = buff + 4;
    for (k = 0; k < len; k += bump, bp += bump) {
        bump = 4;
        if ((k + bump) > len) {
            pr2serr("%s, short descriptor length=%d, left=%d\n", mpp_vpdp,
                    bump, (len - k));
            return;
        }
        if (op->do_hex > 1)
            hex2stdout(bp, 4, 1);
        else {
            ppc =  (bp[0] & 0x3f);
            pspc = bp[1];
            n = sg_scnpr(b, blen, "  Policy page code: 0x%x", ppc);
            if (pspc)
                sg_scn3pr(b, blen, n, ",  subpage code: 0x%x", pspc);
            sgj_pr_hr(jsp, "%s\n", b);
            if ((0 == k) && (0x3f == (0x3f & bp[0])) && (0xff == bp[1]))
                sgj_pr_hr(jsp, "  therefore the policy applies to all modes "
                          "pages and subpages\n");
            sgj_pr_hr(jsp, "    MLUS=%d,  Policy: %s\n", !!(bp[2] & 0x80),
                      mode_page_policy_arr[bp[2] & 0x3]);
            if (jsp->pr_as_json) {
                jo2p = sgj_new_unattached_object_r(jsp);
                sgj_js_nv_ihex(jsp, jo2p, "policy_page_code", ppc);
                sgj_js_nv_ihex(jsp, jo2p, "policy_subpage_code", pspc);
                sgj_js_nv_ihex_nex(jsp, jo2p, "mlus", !!(bp[2] & 0x80), false,
                                   "Multiple logical units share");
                sgj_js_nv_ihexstr(jsp, jo2p, "mode_page_policy", bp[2] & 0x3,
                                  NULL, mode_page_policy_arr[bp[2] & 0x3]);
                sgj_js_nv_o(jsp, jap, NULL /* name */, jo2p);
            }
        }
    }
}

static const char * constituent_type_arr[] = {
    "Reserved",
    "Virtual tape library",
    "Virtual tape drive",
    "Direct access block device",
};

/* VPD_DEVICE_CONSTITUENTS 0x8b, can recurse at least one level */
static int
decode_dev_constit_vpd(uint8_t * buff, int len, int req_pdt, bool protect,
                       struct sdparm_opt_coll * op, sgj_opaque_p jap)
{
    int k, j, res, bump, csd_len;
    uint16_t constit_type;
    const uint8_t * bp;
    sgj_state * jsp = &op->json_st;
    sgj_opaque_p jo2p, jo3p, ja2p;
    char b[256];
    char d[64];
    static const int blen = sizeof(b);
    static const int dlen = sizeof(d);

    if (len < 4) {
        pr2serr("%s length too short=%d\n", dc_vpdp, len);
        return SG_LIB_CAT_MALFORMED;
    }
    len -= 4;
    bp = buff + 4;
    for (k = 0, j = 0; k < len; k += bump, bp += bump, ++j) {
        jo2p = sgj_new_unattached_object_r(jsp);
        if (j > 0)
            sgj_pr_hr(jsp, "\n");
        sgj_pr_hr(jsp, "  Constituent descriptor %d:\n", j + 1);
        if ((k + 36) > len) {
            pr2serr("%s, short descriptor length=36, left=%d\n", dc_vpdp,
                    (len - k));
            sgj_js_nv_o(jsp, jap, NULL, jo2p);
            return SG_LIB_CAT_MALFORMED;
        }
        constit_type = sg_get_unaligned_be16(bp + 0);
        if (constit_type >= SG_ARRAY_SIZE(constituent_type_arr))
            sgj_pr_hr(jsp, "    Constituent type: unknown [0x%x]\n",
                      constit_type);
        else
            sgj_pr_hr(jsp, "    Constituent type: %s [0x%x]\n",
                      constituent_type_arr[constit_type], constit_type);
        sg_scnpr(b, blen, "    Constituent device type: ");
        if (0xff == bp[2])
            sgj_pr_hr(jsp, "%sUnknown [0xff]\n", b);
        else if (bp[2] >= 0x20)
            sgj_pr_hr(jsp, "%sReserved [0x%x]\n", b, bp[2]);
        else
            sgj_pr_hr(jsp, "%s%s [0x%x]\n", b,
                      sg_get_pdt_str(0x1f & bp[2], dlen, d), bp[2]);
        strcpy(d, "T10_vendor_identification");
        snprintf(b, blen, "%.8s", bp + 4);
        sgj_pr_hr(jsp, "    %s: %s\n", d, b);
        d[0] = tolower((uint8_t)d[0]);
        sgj_js_nv_s(jsp, jo2p, d, b);
        strcpy(d, "Product_identification");
        snprintf(b, blen, "%.16s", bp + 12);
        sgj_pr_hr(jsp, "    %s: %s\n", d, b);
        d[0] = tolower((uint8_t)d[0]);
        sgj_js_nv_s(jsp, jo2p, d, b);
        strcpy(d, "Product_revision_level");
        snprintf(b, blen, "%.4s", bp + 28);
        sgj_pr_hr(jsp, "    %s: %s\n", d, b);
        d[0] = tolower((uint8_t)d[0]);
        sgj_js_nv_s(jsp, jo2p, d, b);
        csd_len = sg_get_unaligned_be16(bp + 34);
        bump = 36 + csd_len;
        if ((k + bump) > len) {
            pr2serr("%s, short descriptor length=%d, left=%d\n", dc_vpdp,
                    bump, (len - k));
            sgj_js_nv_o(jsp, jap, NULL, jo2p);
            return SG_LIB_CAT_MALFORMED;
        }
        if (csd_len > 0) {
            int m, q, cs_bump;
            uint8_t cs_type;
            uint8_t cs_len;
            const uint8_t * cs_bp;

            sgj_pr_hr(jsp, "    Constituent specific descriptors:\n");
            ja2p = sgj_named_subarray_r(jsp, jo2p,
                                "constituent_specific_descriptor_list");
            for (m = 0, q = 0, cs_bp = bp + 36; m < csd_len;
                 m += cs_bump, ++q, cs_bp += cs_bump) {
                jo3p = sgj_new_unattached_object_r(jsp);
                cs_type = cs_bp[0];
                cs_len = sg_get_unaligned_be16(cs_bp + 2);
                cs_bump = cs_len + 4;
                sgj_js_nv_ihex(jsp, jo3p, "constituent_specific_type",
                               cs_type);
                if (1 == cs_type) {     /* VPD page */
                    int off = cs_bp + 4 - buff;
                    int pn = buff[off + 1];

                    sgj_pr_hr(jsp, "      Constituent specific %s %d:\n",
                              vpd_pg_s, q + 1);
                    /* SPC-5 says these shall _not_ themselves be Device
                     * Constituent VPD pages. So no infinite recursion. */
                    res = sdp_process_vpd_page(-1, pn, 0, req_pdt, protect,
                                               NULL, buff, off, op, jo3p);
                    if (res)
                        return res;
                } else {
                    if (0xff == cs_type)
                        sgj_pr_hr(jsp, "      Vendor specific data (in "
                                  "hex):\n");
                    else
                        sgj_pr_hr(jsp, "      %s [0x%x] specific data (in "
                                  "hex):\n", rsv_s, cs_type);
                    if (jsp->pr_as_json)
                        sgj_js_nv_hex_bytes(jsp, jo3p,
                                            "constituent_specific_data_hex",
                                            cs_bp + 4, cs_len);
                    else
                        hex2stdout(cs_bp + 4, cs_len,
                                   (op->do_hex > 2) ? -1 : no_ascii_4hex(op));
                }
                sgj_js_nv_o(jsp, ja2p, NULL, jo3p);
            }   /* end of Constituent specific descriptor loop */
        }
        sgj_js_nv_o(jsp, jap, NULL, jo2p);
    }   /* end Constituent descriptor loop */
    return 0;
}

/* VPD_CFA_PROFILE_INFO  0x8c ["cfa"] */
static void
decode_cga_profile_vpd(const uint8_t * buff, int len,
                       struct sdparm_opt_coll * op, sgj_opaque_p jap)
{
    int k;
    uint32_t u;
    sgj_state * jsp = &op->json_st;
    const uint8_t * bp;
    sgj_opaque_p jo2p;

    if (len < 4) {
        pr2serr("%s length too short=%d\n", cpi_vpdp, len);
        return;
    }
    len -= 4;
    bp = buff + 4;
    for (k = 0; k < len; k += 4, bp += 4) {
        jo2p = sgj_new_unattached_object_r(jsp);
        sgj_haj_vi(jsp, jo2p, 0, "CGA profile supported",
                   SGJ_SEP_COLON_1_SPACE, bp[0], true);
        u = sg_get_unaligned_be16(bp + 2);
        sgj_haj_vi_nex(jsp, jo2p, 2, "Sequential write data size",
                       SGJ_SEP_COLON_1_SPACE, u, true, "unit: LB");
        sgj_js_nv_o(jsp, jap, NULL /* name */, jo2p);
    }
}

static const char * network_service_type_arr[] =
{
    "unspecified",
    "storage configuration service",
    "diagnostics",
    "status",
    "logging",
    "code download",
    "copy service",
    "administrative configuration service",
    "reserved[0x8]", "reserved[0x9]",
    "reserved[0xa]", "reserved[0xb]", "reserved[0xc]", "reserved[0xd]",
    "reserved[0xe]", "reserved[0xf]", "reserved[0x10]", "reserved[0x11]",
    "reserved[0x12]", "reserved[0x13]", "reserved[0x14]", "reserved[0x15]",
    "reserved[0x16]", "reserved[0x17]", "reserved[0x18]", "reserved[0x19]",
    "reserved[0x1a]", "reserved[0x1b]", "reserved[0x1c]", "reserved[0x1d]",
    "reserved[0x1e]", "reserved[0x1f]",
};

/* VPD_MAN_NET_ADDR     0x85 ["mna"] */
static void
decode_man_net_vpd(const uint8_t * buff, int len, struct sdparm_opt_coll * op,
                   sgj_opaque_p jap)
{
    int k, bump, na_len, assoc, nst;
    sgj_state * jsp = &op->json_st;
    sgj_opaque_p jo2p;
    const uint8_t * bp;
    const char * assoc_str;
    const char * nst_str;

    if (len < 4) {
        pr2serr("%s length too short=%d\n", mna_vpdp, len);
        return;
    }
    len -= 4;
    bp = buff + 4;
    for (k = 0; k < len; k += bump, bp += bump) {
        assoc = (bp[0] >> 5) & 0x3;
        assoc_str = sg_get_desig_assoc_str(assoc);
        nst = bp[0] & 0x1f;
        nst_str = network_service_type_arr[nst];
        sgj_pr_hr(jsp, "  %s, Service type: %s\n", assoc_str, nst_str);
        na_len = sg_get_unaligned_be16(bp + 2);
        if (jsp->pr_as_json) {
            jo2p = sgj_new_unattached_object_r(jsp);
            sgj_js_nv_ihexstr(jsp, jo2p, "association", assoc, NULL,
                              assoc_str);
            sgj_js_nv_ihexstr(jsp, jo2p, "service_type", nst, NULL,
                              nst_str);
            sgj_js_nv_s_len(jsp, jo2p, "network_address",
                            (const char *)(bp + 4), na_len);
            sgj_js_nv_o(jsp, jap, NULL /* name */, jo2p);
        }
        if (na_len > 0) {
            if (op->do_hex > 1) {
                sgj_pr_hr(jsp, "    Network address:\n");
                hex2stdout((bp + 4), na_len, 0);
            } else
                sgj_pr_hr(jsp, "    %s\n", bp + 4);
        }
        bump = 4 + na_len;
        if ((k + bump) > len) {
            pr2serr("%s, short descriptor length=%d, left=%d\n", mna_vpdp,
                    bump, (len - k));
            return;
        }
    }
}

/* This is xcopy(LID4) related: "ROD" == Representation Of Data
 * Used by VPD_3PARTY_COPY   0x8f ["tpc"] */
static void
decode_rod_descriptor(const uint8_t * buff, int len,
                      struct sdparm_opt_coll * op, sgj_opaque_p jap)
{
    uint8_t pdt;
    uint32_t u;
    int k, bump;
    uint64_t ull;
    const uint8_t * bp = buff;
    sgj_state * jsp = &op->json_st;
    sgj_opaque_p jo2p;
    char b[80];
    static const int blen = sizeof(b);
    static const char * ab_pdt = "abnormal use of 'pdt'";

    for (k = 0; k < len; k += bump, bp += bump) {
        jo2p = sgj_new_unattached_object_r(jsp);
        bump = sg_get_unaligned_be16(bp + 2) + 4;
        pdt = 0x1f & bp[0];
        u = (bp[0] >> 5) & 0x7;
        sgj_js_nv_i(jsp, jo2p, "descriptor_format", u);
        if (0 != u) {
            sgj_pr_hr(jsp, "  Unhandled descriptor (format %u, device type "
                      "%u)\n", u, pdt);
            sgj_js_nv_o(jsp, jap, NULL /* name */, jo2p);
            break;
        }
        switch (pdt) {
        case 0:
            /* Block ROD device type specific descriptor */
            sgj_js_nv_ihexstr_nex(jsp, jo2p, "peripheral_device_type",
                                  pdt, false, NULL, "Block ROD device "
                                  "type specific descriptor", ab_pdt);
            sgj_haj_vi_nex(jsp, jo2p, 4, "Optimal block ROD length "
                           "granularity", SGJ_SEP_COLON_1_SPACE,
                           sg_get_unaligned_be16(bp + 6), true, "unit: LB");
            ull = sg_get_unaligned_be64(bp + 8);
            sgj_haj_vi(jsp, jo2p, 4, "Maximum bytes in block ROD",
                       SGJ_SEP_COLON_1_SPACE, ull, true);
            ull = sg_get_unaligned_be64(bp + 16);
            sgj_haj_vistr(jsp, jo2p, 4, "Optimal Bytes in block ROD "
                          "transfer", SGJ_SEP_COLON_1_SPACE, ull, true,
                          (SG_LIB_UNBOUNDED_64BIT == ull) ? nl_s : NULL);
            ull = sg_get_unaligned_be64(bp + 24);
            sgj_haj_vistr(jsp, jo2p, 4, "Optimal Bytes to token per "
                          "segment", SGJ_SEP_COLON_1_SPACE, ull, true,
                          (SG_LIB_UNBOUNDED_64BIT == ull) ? nl_s : NULL);
            ull = sg_get_unaligned_be64(bp + 32);
            sgj_haj_vistr(jsp, jo2p, 4, "Optimal Bytes from token per "
                          "segment", SGJ_SEP_COLON_1_SPACE, ull, true,
                          (SG_LIB_UNBOUNDED_64BIT == ull) ? nl_s : NULL);
            break;
        case 1:
            /* Stream ROD device type specific descriptor */
            sgj_js_nv_ihexstr_nex(jsp, jo2p, "peripheral_device_type",
                                  pdt, false, NULL, "Stream ROD device "
                                  "type specific descriptor", ab_pdt);
            ull = sg_get_unaligned_be64(bp + 8);
            sgj_haj_vi(jsp, jo2p, 4, "Maximum bytes in stream ROD",
                       SGJ_SEP_COLON_1_SPACE, ull, true);
            ull = sg_get_unaligned_be64(bp + 16);
            snprintf(b, blen, "  Optimal Bytes in stream ROD transfer: ");
            if (SG_LIB_UNBOUNDED_64BIT == ull)
                sgj_pr_hr(jsp, "%s-1 [no limit]\n", b);
            else
                sgj_pr_hr(jsp, "%s%" PRIu64 "\n", b, ull);
            break;
        case 3:
            /* Copy manager ROD device type specific descriptor */
            sgj_js_nv_ihexstr_nex(jsp, jo2p, "peripheral_device_type",
                                  pdt, false, NULL, "Copy manager ROD "
                                  "device type specific descriptor",
                                  ab_pdt);
            sgj_pr_hr(jsp, "  Maximum Bytes in processor ROD: %" PRIu64 "\n",
                      sg_get_unaligned_be64(bp + 8));
            ull = sg_get_unaligned_be64(bp + 16);
            snprintf(b, blen, "  Optimal Bytes in processor ROD transfer: ");
            if (SG_LIB_UNBOUNDED_64BIT == ull)
                sgj_pr_hr(jsp, "%s-1 [no limit]\n", b);
            else
                sgj_pr_hr(jsp, "%s%" PRIu64 "\n", b, ull);
            break;
        default:
            sgj_js_nv_ihexstr(jsp, jo2p, "peripheral_device_type",
                              pdt, NULL, "unknown");
            break;
        }
        sgj_js_nv_o(jsp, jap, NULL /* name */, jo2p);
    }
}

struct tpc_desc_type {
    uint8_t code;
    const char * name;
};

static struct tpc_desc_type tpc_desc_arr[] = {
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
get_tpc_desc_name(uint8_t code)
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

static const char *
get_tpc_desc_type_s(uint32_t desc_type)
{
    switch(desc_type) {
    case 0:
        return "Block Device ROD Limits";
    case 1:
        return "Supported Commands";
    case 4:
        return "Parameter Data";
    case 8:
        return "Supported Descriptors";
    case 0xc:
        return "Supported CSCD Descriptor IDs";
    case 0xd:
        return "Copy Group Identifier";
    case 0x106:
        return "ROD Token Features";
    case 0x108:
        return "Supported ROD Token and ROD Types";
    case 0x8001:
        return "General Copy Operations";
    case 0x9101:
        return "Stream Copy Operations";
    case 0xC001:
        return "Held Data";
    default:
        if ((desc_type >= 0xE000) && (desc_type <= 0xEFFF))
            return "Restricted";
        else
            return "Reserved";
    }
}

/* VPD_3PARTY_COPY   3PC, third party copy  0x8f ["tpc"] */
static void
decode_3party_copy_vpd(const uint8_t * buff, int len,
                       struct sdparm_opt_coll * op, sgj_opaque_p jap)
{
    int j, k, m, bump, desc_type, desc_len, sa_len, pdt /* , dhex */ ;
    uint32_t u, v;
    uint64_t ull;
    const uint8_t * bp;
    const char * cp;
    const char * dtp;
    sgj_state * jsp = &op->json_st;
    sgj_opaque_p jo2p = NULL;
    sgj_opaque_p ja2p = NULL;
    sgj_opaque_p jo3p = NULL;
    char b[144];
    static const int blen = sizeof(b);

#if 0
    dhex = op->do_hex;
    if (dhex > 0) {
        if (dhex > 2)
            named_hhh_output(tpc_vpdp, buff, len, op);
        else
            hex2stdout(buff, len, no_ascii_4hex(op));
        return;
    } else if (dhex < 0)
        dhex = -dhex;
#endif

    if (len < 4) {
        pr2serr("%s length too short=%d\n", vpd_pg_s, len);
        return;
    }
    pdt = buff[0] & PDT_MASK;
    len -= 4;
    bp = buff + 4;
    for (k = 0; k < len; k += bump, bp += bump) {
        jo2p = sgj_new_unattached_object_r(jsp);
        desc_type = sg_get_unaligned_be16(bp);
        desc_len = sg_get_unaligned_be16(bp + 2);
        if (op->verbose)
            sgj_pr_hr(jsp, "Descriptor type=%d [0x%x] , len %d\n", desc_type,
                      desc_type, desc_len);
        bump = 4 + desc_len;
        if ((k + bump) > len) {
            pr2serr("%s, short descriptor length=%d, left=%d\n", vpd_pg_s,
                    bump, (len - k));
            break;
        }
        if (0 == desc_len)
            goto skip;          /* continue plus attach jo2p */
#if 0
        if (2 == dhex)
            hex2stdout(bp + 4, desc_len, 1);
        else if (dhex > 2)
            hex2stdout(bp, bump, 1);
        else {
#else
        if (true) {
#endif
            int csll;

            dtp = get_tpc_desc_type_s(desc_type);
            sgj_js_nv_ihexstr(jsp, jo2p, "third_party_copy_descriptor_type",
                              desc_type, NULL, dtp);
            sgj_js_nv_ihex(jsp, jo2p, "third_party_copy_descriptor_length",
                           desc_len);

            switch (desc_type) {
            case 0x0000:    /* Required if POPULATE TOKEN (or friend) used */
                sgj_pr_hr(jsp, " %s:\n", dtp);
                u = sg_get_unaligned_be16(bp + 10);
                sgj_haj_vistr(jsp, jo2p, 2, "Maximum range descriptors",
                              SGJ_SEP_COLON_1_SPACE, u, true,
                              (0 == u) ? nr_s : NULL);
                u = sg_get_unaligned_be32(bp + 12);
                if (0 == u)
                    cp = nr_s;
                else if (SG_LIB_UNBOUNDED_32BIT == u)
                    cp = "No maximum given";
                else
                    cp = NULL;
                sgj_haj_vistr_nex(jsp, jo2p, 2, "Maximum inactivity timeout",
                                  SGJ_SEP_COLON_1_SPACE, u, true, cp,
                                  "unit: second");
                u = sg_get_unaligned_be32(bp + 16);
                sgj_haj_vistr_nex(jsp, jo2p, 2, "Default inactivity timeout",
                                  SGJ_SEP_COLON_1_SPACE, u, true,
                                  (0 == u) ? nr_s : NULL, "unit: second");
                ull = sg_get_unaligned_be64(bp + 20);
                sgj_haj_vistr_nex(jsp, jo2p, 2, "Maximum token transfer size",
                                  SGJ_SEP_COLON_1_SPACE, ull, true,
                                  (0 == ull) ? nr_s : NULL, "unit: LB");
                ull = sg_get_unaligned_be64(bp + 28);
                sgj_haj_vistr_nex(jsp, jo2p, 2, "Optimal transfer count",
                                  SGJ_SEP_COLON_1_SPACE, ull, true,
                                  (0 == ull) ? nr_s : NULL, "unit: LB");
                break;
            case 0x0001:    /* Mandatory (SPC-4) */
                sgj_pr_hr(jsp, " %s:\n", "Commands supported list");
                ja2p = sgj_named_subarray_r(jsp, jo2p,
                                            "commands_supported_list");
                j = 0;
                csll = bp[4];
                if (csll >= desc_len) {
                    pr2serr("Command supported list length (%d) >= "
                            "descriptor length (%d), wrong so trim\n",
                            csll, desc_len);
                    csll = desc_len - 1;
                }
                while (j < csll) {
                    uint8_t opc, sa;
                    static const char * soc = "supported_operation_code";
                    static const char * ssa = "supported_service_action";

                    jo3p = NULL;
                    opc = bp[5 + j];
                    sa_len = bp[6 + j];
                    for (m = 0; (m < sa_len) && ((j + m) < csll); ++m) {
                        jo3p = sgj_new_unattached_object_r(jsp);
                        sa = bp[7 + j + m];
                        sg_get_opcode_sa_name(opc, sa, pdt, blen, b);
                        sgj_pr_hr(jsp, "  %s\n", b);
                        sgj_js_nv_s(jsp, jo3p, "name", b);
                        sgj_js_nv_ihex(jsp, jo3p, soc, opc);
                        sgj_js_nv_ihex(jsp, jo3p, ssa, sa);
                        sgj_js_nv_o(jsp, ja2p, NULL /* name */, jo3p);
                    }
                    if (0 == sa_len) {
                        jo3p = sgj_new_unattached_object_r(jsp);
                        sg_get_opcode_name(opc, pdt, blen, b);
                        sgj_pr_hr(jsp, "  %s\n", b);
                        sgj_js_nv_s(jsp, jo3p, "name", b);
                        sgj_js_nv_ihex(jsp, jo3p, soc, opc);
                        sgj_js_nv_o(jsp, ja2p, NULL /* name */, jo3p);
                    } else if (m < sa_len)
                        pr2serr("Supported service actions list length (%d) "
                                "is too large\n", sa_len);
                    j += m + 2;
                }
                break;
            case 0x0004:
                sgj_pr_hr(jsp, " %s:\n", dtp);
                sgj_haj_vi(jsp, jo2p, 2, "Maximum CSCD descriptor count",
                           SGJ_SEP_COLON_1_SPACE,
                           sg_get_unaligned_be16(bp + 8), true);
                sgj_haj_vi(jsp, jo2p, 2, "Maximum segment descriptor count",
                           SGJ_SEP_COLON_1_SPACE,
                           sg_get_unaligned_be16(bp + 10), true);
                sgj_haj_vi(jsp, jo2p, 2, "Maximum descriptor list length",
                           SGJ_SEP_COLON_1_SPACE,
                           sg_get_unaligned_be32(bp + 12), true);
                sgj_haj_vi(jsp, jo2p, 2, "Maximum inline data length",
                           SGJ_SEP_COLON_1_SPACE,
                           sg_get_unaligned_be32(bp + 17), true);
                break;
            case 0x0008:
                sgj_pr_hr(jsp, " Supported descriptors:\n");
                ja2p = sgj_named_subarray_r(jsp, jo2p,
                                            "supported_descriptor_list");
                for (j = 0; j < bp[4]; j++) {
                    bool found_name;

                    jo3p = sgj_new_unattached_object_r(jsp);
                    u = bp[5 + j];
                    cp = get_tpc_desc_name(u);
                    found_name = (strlen(cp) > 0);
                    if (found_name)
                        sgj_pr_hr(jsp, "  %s [0x%x]\n", cp, u);
                    else
                        sgj_pr_hr(jsp, "  0x%x\n", u);
                    sgj_js_nv_s(jsp, jo3p, "name", found_name ? cp : nr_s);
                    sgj_js_nv_ihex(jsp, jo3p, "code", u);
                    sgj_js_nv_o(jsp, ja2p, NULL /* name */, jo3p);
                }
                break;
            case 0x000C:
                sgj_pr_hr(jsp, " Supported CSCD IDs (above 0x7ff):\n");
                ja2p = sgj_named_subarray_r(jsp, jo2p, "supported_cscd_"
                                            "descriptor_id_list");
                v = sg_get_unaligned_be16(bp + 4);
                for (j = 0; j < (int)v; j += 2) {
                    bool found_name;

                    jo3p = sgj_new_unattached_object_r(jsp);
                    u = sg_get_unaligned_be16(bp + 6 + j);
                    cp = get_cscd_desc_id_name(u);
                    found_name = (strlen(cp) > 0);
                    if (found_name)
                        sgj_pr_hr(jsp, "  %s [0x%04x]\n", cp, u);
                    else
                        sgj_pr_hr(jsp, "  0x%04x\n", u);
                    sgj_js_nv_s(jsp, jo3p, "name", found_name ? cp : nr_s);
                    sgj_js_nv_ihex(jsp, jo3p, "id", u);
                    sgj_js_nv_o(jsp, ja2p, NULL /* name */, jo3p);
                }
                break;
            case 0x000D:
                sgj_pr_hr(jsp, " Copy group identifier:\n");
                u = bp[4];
                sg_t10_uuid_desig2str(bp + 5, u, 1 /* c_set */, false,
                                      true, NULL, blen, b);
                sgj_pr_hr(jsp, "  Locally assigned UUID: %s", b);
                sgj_js_nv_s(jsp, jo2p, "locally_assigned_uuid", b);
                break;
            case 0x0106:
                sgj_pr_hr(jsp, " ROD token features:\n");
                sgj_haj_vi(jsp, jo2p, 2, "Remote tokens",
                           SGJ_SEP_COLON_1_SPACE, bp[4] & 0x0f, true);
                u = sg_get_unaligned_be32(bp + 16);
                sgj_pr_hr(jsp, "  Minimum token lifetime: %u seconds\n", u);
                sgj_js_nv_ihex_nex(jsp, jo2p, "minimum_token_lifetime", u,
                                   true, "unit: second");
                u = sg_get_unaligned_be32(bp + 20);
                sgj_pr_hr(jsp, "  Maximum token lifetime: %u seconds\n", u);
                sgj_js_nv_ihex_nex(jsp, jo2p, "maximum_token_lifetime", u,
                                   true, "unit: second");
                u = sg_get_unaligned_be32(bp + 24);
                sgj_haj_vi_nex(jsp, jo2p, 2, "Maximum token inactivity "
                               "timeout", SGJ_SEP_COLON_1_SPACE, u,
                               true, "unit: second");
                u = sg_get_unaligned_be16(bp + 46);
                ja2p = sgj_named_subarray_r(jsp, jo2p,
                    "rod_device_type_specific_features_descriptor_list");
                decode_rod_descriptor(bp + 48, u, op, ja2p);
                break;
            case 0x0108:
                sgj_pr_hr(jsp, " Supported ROD token and ROD types:\n");
                ja2p = sgj_named_subarray_r(jsp, jo2p, "rod_type_"
                                            "descriptor_list");
                for (j = 0; j < sg_get_unaligned_be16(bp + 6); j+= 64) {
                    bool found_name;

                    jo3p = sgj_new_unattached_object_r(jsp);
                    u = sg_get_unaligned_be32(bp + 8 + j);
                    cp = get_tpc_rod_name(u);
                    found_name = (strlen(cp) > 0);
                    if (found_name > 0)
                        sgj_pr_hr(jsp, "  ROD type: %s [0x%x]\n", cp, u);
                    else
                        sgj_pr_hr(jsp, "  ROD type: 0x%x\n", u);
                    sgj_js_nv_ihexstr(jsp, jo3p, "rod_type", u, NULL,
                                      found_name ? cp : NULL);
                    u = bp[8 + j + 4];
                    sgj_pr_hr(jsp, "    ECPY_INT: %s\n",
                              (u & 0x80) ? y_s : n_s);
                    sgj_js_nv_ihex_nex(jsp, jo3p, "ecpy_int", !!(0x80 & u),
                                       false, "Extended CoPY INTernal rods");
                    sgj_pr_hr(jsp, "    Token in: %s\n",
                              (u & 0x2) ? y_s : n_s);
                    sgj_js_nv_i(jsp, jo3p, "token_in", !!(0x2 & u));
                    sgj_pr_hr(jsp, "    Token out: %s\n",
                              (u & 0x1) ? y_s : n_s);
                    sgj_js_nv_i(jsp, jo3p, "token_out", !!(0x2 & u));
                    u = sg_get_unaligned_be16(bp + 8 + j + 6);
                    sgj_haj_vi(jsp, jo3p, 4, "Preference indicator",
                               SGJ_SEP_COLON_1_SPACE, u, true);
                    sgj_js_nv_o(jsp, ja2p, NULL /* name */, jo3p);
                }
                break;
            case 0x8001:    /* Mandatory (SPC-4) */
                sgj_pr_hr(jsp, " General copy operations:\n");
                u = sg_get_unaligned_be32(bp + 4);
                sgj_haj_vi(jsp, jo2p, 2, "Total concurrent copies",
                           SGJ_SEP_COLON_1_SPACE, u, true);
                u = sg_get_unaligned_be32(bp + 8);
                sgj_haj_vi(jsp, jo2p, 2, "Maximum identified concurrent "
                           "copies", SGJ_SEP_COLON_1_SPACE, u, true);
                u = sg_get_unaligned_be32(bp + 12);
                sgj_haj_vi_nex(jsp, jo2p, 2, "Maximum segment length",
                               SGJ_SEP_COLON_1_SPACE, u, true, "unit: byte");
                u = bp[16];     /* field is power of 2 */
                sgj_haj_vi_nex(jsp, jo2p, 2, "Data segment granularity",
                               SGJ_SEP_COLON_1_SPACE, u, true,
                               "unit: 2^val LB");
                u = bp[17];     /* field is power of 2 */
                sgj_haj_vi_nex(jsp, jo2p, 2, "Inline data granularity",
                               SGJ_SEP_COLON_1_SPACE, u, true,
                               "unit: 2^val LB");
                break;
            case 0x9101:
                sgj_pr_hr(jsp, " Stream copy operations:\n");
                u = sg_get_unaligned_be32(bp + 4);
                sgj_haj_vi_nex(jsp, jo2p, 2, "Maximum stream device transfer "
                               "size", SGJ_SEP_COLON_1_SPACE, u, true,
                               "unit: byte");
                break;
            case 0xC001:
                sgj_pr_hr(jsp, " Held data:\n");
                u = sg_get_unaligned_be32(bp + 4);
                sgj_haj_vi_nex(jsp, jo2p, 2, "Held data limit",
                               SGJ_SEP_COLON_1_SPACE, u, true,
                               "unit: byte; (lower limit: minimum)");
                sgj_haj_vi_nex(jsp, jo2p, 2, "Held data granularity",
                               SGJ_SEP_COLON_1_SPACE, bp[8], true,
                               "unit: 2^val byte");
                break;
            default:
                pr2serr("Unexpected type=%d\n", desc_type);
                hex2stderr(bp, bump, 1);
                break;
            }
        }
skip:
        sgj_js_nv_o(jsp, jap, NULL /* name */, jo2p);
        jo2p = NULL;
    }
    if (jo2p)
        sgj_js_nv_o(jsp, jap, NULL /* name */, jo2p);
}

/* VPD_PROTO_LU  0x90 ["pslu"] */
static void
decode_proto_lu_vpd(const uint8_t * buff, int len,
                    struct sdparm_opt_coll * op, sgj_opaque_p jap)
{
    int k, bump, rel_port, desc_len, proto;
    const uint8_t * bp;
    sgj_state * jsp = &op->json_st;
    sgj_opaque_p jo2p = NULL;
    char b[128];
    static const int blen = sizeof(b);

    if (len < 4) {
        pr2serr("%s length too short=%d\n", pslu_vpdp, len);
        return;
    }
    len -= 4;
    bp = buff + 4;
    for (k = 0; k < len; k += bump, bp += bump) {
        jo2p = sgj_new_unattached_object_r(jsp);
        rel_port = sg_get_unaligned_be16(bp);
        sgj_haj_vi(jsp, jo2p, 2, "Relative port",
                   SGJ_SEP_COLON_1_SPACE, rel_port, true);
        proto = bp[2] & 0xf;
        sg_get_trans_proto_str(proto, blen, b);
        sgj_haj_vistr(jsp, jo2p, 4, "Protocol identifier",
                      SGJ_SEP_COLON_1_SPACE, proto, false, b);
        desc_len = sg_get_unaligned_be16(bp + 6);
        bump = 8 + desc_len;
        if ((k + bump) > len) {
            pr2serr("%s, short descriptor length=%d, left=%d\n", pslu_vpdp,
                    bump, (len - k));
            sgj_js_nv_o(jsp, jap, NULL /* name */, jo2p);
            return;
        }
        if (0 == desc_len)
            goto again;
        switch (proto) {
        case TPROTO_SAS:
            sgj_haj_vi(jsp, jo2p, 2, "TLR control supported",
                       SGJ_SEP_COLON_1_SPACE, !!(bp[8] & 0x1), false);
            break;
        default:
            pr2serr("Unexpected proto=%d\n", proto);
            hex2stderr(bp, bump, 1);
            break;
        }
again:
        sgj_js_nv_o(jsp, jap, NULL /* name */, jo2p);
    }
}

/* VPD_PROTO_PORT  0x91 ["pspo"] */
static void
decode_proto_port_vpd(const uint8_t * buff, int len,
                      struct sdparm_opt_coll * op, sgj_opaque_p jap)
{
    bool pds, ssp_pers;
    int k, j, bump, rel_port, desc_len, proto, phy;
    const uint8_t * bp;
    const uint8_t * pidp;
    sgj_state * jsp = &op->json_st;
    sgj_opaque_p jo2p = NULL;
    sgj_opaque_p ja2p = NULL;
    sgj_opaque_p jo3p = NULL;
    char b[128];
    static const int blen = sizeof(b);

    if (len < 4) {
        pr2serr("%s length too short=%d\n", pspo_vpdp, len);
        return;
    }
    len -= 4;
    bp = buff + 4;
    for (k = 0; k < len; k += bump, bp += bump) {
        jo2p = sgj_new_unattached_object_r(jsp);
        rel_port = sg_get_unaligned_be16(bp);
        sgj_haj_vi(jsp, jo2p, 2, "Relative port",
                   SGJ_SEP_COLON_1_SPACE, rel_port, true);
        proto = bp[2] & 0xf;
        sg_get_trans_proto_str(proto, blen, b);
        sgj_haj_vistr(jsp, jo2p, 4, "Protocol identifier",
                      SGJ_SEP_COLON_1_SPACE, proto, false, b);
        desc_len = sg_get_unaligned_be16(bp + 6);
        bump = 8 + desc_len;
        if ((k + bump) > len) {
            pr2serr("%s, short descriptor length=%d, left=%d\n", vpd_pg_s,
                    bump, (len - k));
            sgj_js_nv_o(jsp, jap, NULL /* name */, jo2p);
            return;
        }
        if (0 == desc_len)
            goto again;
        switch (proto) {
        case TPROTO_SAS:    /* page added in spl3r02 */
            pds = !!(bp[3] & 0x1);
            sgj_pr_hr(jsp, "    power disable supported (pwr_d_s)=%d\n", pds);
            sgj_js_nv_ihex_nex(jsp, jo2p, "pwr_d_s", pds, false,
                       "PoWeR Disable Supported");
            ja2p = sgj_named_subarray_r(jsp, jo2p,
                                    "sas_phy_information_descriptor_list");
            pidp = bp + 8;
            for (j = 0; j < desc_len; j += 4, pidp += 4) {
                jo3p = sgj_new_unattached_object_r(jsp);
                phy = pidp[1];
                ssp_pers = !!(0x1 & pidp[2]);
                sgj_pr_hr(jsp, "      phy id=%d, SSP persistent capable=%d\n",
                          phy, ssp_pers);
                sgj_js_nv_ihex(jsp, jo3p, "phy_identifier", phy);
                sgj_js_nv_i(jsp, jo3p, "ssp_persistent_capable", ssp_pers);
                sgj_js_nv_o(jsp, ja2p, NULL /* name */, jo3p);
            }
            break;
        default:
            pr2serr("Unexpected proto=%d\n", proto);
            hex2stderr(bp, bump, 1);
            break;
        }
again:
        sgj_js_nv_o(jsp, jap, NULL /* name */, jo2p);
    }
}

/* VPD_SCSI_FEATURE_SETS  0x92  ["sfs"] */
static void
decode_feature_sets_vpd(const uint8_t * buff, int len,
                        struct sdparm_opt_coll * op, sgj_opaque_p jap)
{
    int k, bump;
    uint16_t sf_code;
    bool found;
    const uint8_t * bp;
    sgj_opaque_p jo2p;
    sgj_state * jsp = &op->json_st;
    char b[256];
    char d[80];

    if (len < 4) {
        pr2serr("%s length too short=%d\n", sfs_vpdp, len);
        return;
    }
    len -= 8;
    bp = buff + 8;
    for (k = 0; k < len; k += bump, bp += bump) {
        jo2p = sgj_new_unattached_object_r(jsp);
        sf_code = sg_get_unaligned_be16(bp);
        bump = 2;
        if ((k + bump) > len) {
            pr2serr("%s, short descriptor length=%d, left=%d\n", sfs_vpdp,
                    bump, (len - k));
            return;
        }
        if (2 == op->do_hex)
            hex2stdout(bp + 8, 2, 1);
        else if (op->do_hex > 2)
            hex2stdout(bp, 2, 1);
        else {
             sg_scnpr(b, sizeof(b), "    %s",
                      sg_get_sfs_str(sf_code, -2, sizeof(d), d, &found,
                                     op->verbose));
            if (op->verbose == 1)
                sgj_pr_hr(jsp, "%s [0x%x]\n", b, (unsigned int)sf_code);
            else if (op->verbose > 1)
                sgj_pr_hr(jsp, "%s [0x%x] found=%s\n", b,
                          (unsigned int)sf_code, found ? "true" : "false");
            else
                sgj_pr_hr(jsp, "%s\n", b);
            sgj_js_nv_ihexstr(jsp, jo2p, "feature_set_code", sf_code, NULL,
                              d);
            if (jsp->verbose)
                sgj_js_nv_b(jsp, jo2p, "meaning_is_match", found);
        }
        sgj_js_nv_o(jsp, jap, NULL, jo2p);
    }
}

/* VPD_SCSI_PORTS     0x88  ["sp"] */
static int
decode_scsi_ports_vpd(uint8_t * buff, int len, struct sdparm_opt_coll * op,
                      sgj_opaque_p jap)
{
    int k, bump, rel_port, ip_tid_len, tpd_len, dhex;
    sgj_state * jsp = &op->json_st;
    sgj_opaque_p jo2p = NULL;
    sgj_opaque_p ja2p = NULL;
    uint8_t * bp;

    dhex = op->do_hex;
    if (dhex < 0)
        dhex = -dhex;

    if (len < 4) {
        pr2serr("%s %s=%d\n", sp_vpdp,  lts_s, len);
        return SG_LIB_CAT_MALFORMED;
    }
    len -= 4;
    bp = buff + 4;
    for (k = 0; k < len; k += bump, bp += bump) {
        rel_port = sg_get_unaligned_be16(bp + 2);
        sgj_pr_hr(jsp, "  Relative port=%d\n", rel_port);
        jo2p = sgj_new_unattached_object_r(jsp);
        sgj_js_nv_i(jsp, jo2p, "relative_port", rel_port);
        ip_tid_len = sg_get_unaligned_be16(bp + 6);
        bump = 8 + ip_tid_len;
        if ((k + bump) > len) {
            pr2serr("%s, short descriptor length=%d, left=%d\n", sp_vpdp,
                    bump, (len - k));
            return SG_LIB_CAT_MALFORMED;
        }
        if (ip_tid_len > 0) {
            if (dhex > 1) {
                sgj_pr_hr(jsp, "    Initiator port transport id:\n");
                hex2stdout((bp + 8), ip_tid_len, 1);
            } else {
                char b[1024];

                sg_decode_transportid_str("    ", bp + 8, ip_tid_len,
                                          true, sizeof(b), b);
                if (jsp->pr_as_json)
                    sgj_js_nv_s(jsp, jo2p, "initiator_port_transport_id", b);
                sgj_pr_hr(jsp, "%s",
                          sg_decode_transportid_str("    ", bp + 8,
                                            ip_tid_len, true, sizeof(b), b));
            }
        }
        tpd_len = sg_get_unaligned_be16(bp + bump + 2);
        if ((k + bump + tpd_len + 4) > len) {
            pr2serr("%s, short descriptor(tgt) length=%d, left=%d\n", sp_vpdp,
                    bump, (len - k));
            return SG_LIB_CAT_MALFORMED;
        }
        if (tpd_len > 0) {
            if (dhex > 1) {
                sgj_pr_hr(jsp, "    Target port descriptor(s):\n");
                hex2stdout(bp + bump + 4, tpd_len, 1);
            } else {
                if ((0 == op->do_quiet) || (ip_tid_len > 0))
                    sgj_pr_hr(jsp, "    Target port descriptor(s):\n");
                if (jsp->pr_as_json) {
                    sgj_opaque_p jo3p = sgj_named_subobject_r(jsp, jo2p,
                                                              "target_port");

                    ja2p = sgj_named_subarray_r(jsp, jo3p,
                                        "designation_descriptor_list");
                }
                decode_dev_ids("", 2 /* leading spaces */, bp + bump + 4,
                               tpd_len, VPD_ASSOC_TPORT, -1, -1, op, ja2p);
            }
        }
        bump += tpd_len + 4;
        sgj_js_nv_o(jsp, jap, NULL, jo2p);
    }
    return 0;
}

/* VPD_EXT_INQ    Extended Inquiry data VPD ["ei"] */
static void
decode_ext_inq_vpd(const uint8_t * b, int len, bool protect,
                   struct sdparm_opt_coll * op, sgj_opaque_p jop)
{
    bool do_long_nq = op->do_long && (! op->do_quiet);
    int n;
    sgj_state * jsp = &op->json_st;
    sgj_opaque_p jo2p;
    const char * cp;
    const char * np;
    const char * nex_p;
    char d[128];
    static const int dlen = sizeof(d);

    if (len < 7) {
        pr2serr("%s length too short=%d\n", eid_vpdp, len);
        return;
    }
    if (do_long_nq || jsp->pr_as_json) {
        n = (b[4] >> 6) & 0x3;
        if (1 == n)
            cp = "before final WRITE BUFFER";
        else if (2 == n)
            cp = "after power on or hard reset";
        else {
            cp = "none";
            d[0] = '\0';
        }
        if (cp[0])
            snprintf(d, dlen, " [%s]", cp);
        sgj_pr_hr(jsp, "  ACTIVATE_MICROCODE=%d%s\n", n, d);
        sgj_js_nv_ihexstr(jsp, jop, "activate_microcode", n, NULL, cp);
        n = (b[4] >> 3) & 0x7;
        if (protect) {
            switch (n)
            {
            case 0:
                cp = "protection type 1 supported";
                break;
            case 1:
                cp = "protection types 1 and 2 supported";
                break;
            case 2:
                cp = "protection type 2 supported";
                break;
            case 3:
                cp = "protection types 1 and 3 supported";
                break;
            case 4:
                cp = "protection type 3 supported";
                break;
            case 5:
                cp = "protection types 2 and 3 supported";
                break;
            case 6:
                cp = "see Supported block lengths and protection types "
                     "VPD page";
                break;
            case 7:
                cp = "protection types 1, 2 and 3 supported";
                break;
            }
        } else {
            cp = "none";
            d[0] = '\0';
        }
        if (cp[0])
            snprintf(d, dlen, " [%s]", cp);
        sgj_pr_hr(jsp, "  SPT=%d%s\n", n, d);
        sgj_js_nv_ihexstr_nex(jsp, jop, "spt", n, false, NULL,
                              cp, "Supported Protection Type");
        sgj_haj_vi_nex(jsp, jop, 2, "GRD_CHK", SGJ_SEP_EQUAL_NO_SPACE,
                       !!(b[4] & 0x4), false, "guard check");
        sgj_haj_vi_nex(jsp, jop, 2, "APP_CHK", SGJ_SEP_EQUAL_NO_SPACE,
                       !!(b[4] & 0x2), false, "application tag check");
        sgj_haj_vi_nex(jsp, jop, 2, "REF_CHK", SGJ_SEP_EQUAL_NO_SPACE,
                       !!(b[4] & 0x1), false, "reference tag check");
        sgj_haj_vi_nex(jsp, jop, 2, "UASK_SUP", SGJ_SEP_EQUAL_NO_SPACE,
                       !!(b[5] & 0x20), false, "Unit Attention "
                       "condition Sense Key specific data Supported");
        sgj_haj_vi_nex(jsp, jop, 2, "GROUP_SUP", SGJ_SEP_EQUAL_NO_SPACE,
                       !!(b[5] & 0x10), false, "grouping function supported");
        sgj_haj_vi_nex(jsp, jop, 2, "PRIOR_SUP", SGJ_SEP_EQUAL_NO_SPACE,
                       !!(b[5] & 0x8), false, "priority supported");
        sgj_haj_vi_nex(jsp, jop, 2, "HEADSUP", SGJ_SEP_EQUAL_NO_SPACE,
                       !!(b[5] & 0x4), false, "head of queue supported");
        sgj_haj_vi_nex(jsp, jop, 2, "ORDSUP", SGJ_SEP_EQUAL_NO_SPACE,
                       !!(b[5] & 0x2), false, "ordered (task attribute) "
                       "supported");
        sgj_haj_vi_nex(jsp, jop, 2, "SIMPSUP", SGJ_SEP_EQUAL_NO_SPACE,
                       !!(b[5] & 0x1), false, "simple (task attribute) "
                       "supported");
        sgj_haj_vi_nex(jsp, jop, 2, "WU_SUP", SGJ_SEP_EQUAL_NO_SPACE,
                       !!(b[6] & 0x8), false, "Write uncorrectable "
                       "supported");
        sgj_haj_vi_nex(jsp, jop, 2, "CRD_SUP", SGJ_SEP_EQUAL_NO_SPACE,
                       !!(b[6] & 0x4), false, "Correction disable "
                       "supported (obsolete SPC-5)");
        sgj_haj_vi_nex(jsp, jop, 2, "NV_SUP", SGJ_SEP_EQUAL_NO_SPACE,
                       !!(b[6] & 0x2), false, "Nonvolatile cache "
                       "supported");
        sgj_haj_vi_nex(jsp, jop, 2, "V_SUP", SGJ_SEP_EQUAL_NO_SPACE,
                       !!(b[6] & 0x1), false, "Volatile cache supported");
        sgj_haj_vi_nex(jsp, jop, 2, "NO_PI_CHK", SGJ_SEP_EQUAL_NO_SPACE,
                       !!(b[7] & 0x20), false, "No protection "
                       "information checking");        /* spc5r02 */
        sgj_haj_vi_nex(jsp, jop, 2, "P_I_I_SUP", SGJ_SEP_EQUAL_NO_SPACE,
                       !!(b[7] & 0x10), false, "Protection information "
                       "interval supported");
        sgj_haj_vi_nex(jsp, jop, 2, "LUICLR", SGJ_SEP_EQUAL_NO_SPACE,
                       !!(b[7] & 0x1), false, "Logical unit I_T nexus clear");
        np = "LU_COLL_TYPE";
        n = (b[8] >> 5) & 0x7;
        nex_p = "Logical unit collection type";
        if (jsp && (jsp->pr_string)) {
            switch (n) {
            case 0:
                cp = "not reported";
                break;
            case 1:
                cp = "Conglomerate";
                break;
            case 2:
                cp = "Logical unit group";
                break;
            default:
                cp = rsv_s;
                break;
            }
            jo2p = sgj_haj_subo_r(jsp, jop, 2, np, SGJ_SEP_EQUAL_NO_SPACE,
                                  n, false);
            sgj_js_nv_s(jsp, jo2p, mn_s, cp);
            if (jsp->pr_name_ex)
                sgj_js_nv_s(jsp, jo2p, "abbreviated_name_expansion", nex_p);
        } else
            sgj_haj_vi_nex(jsp, jop, 2, np, SGJ_SEP_EQUAL_NO_SPACE, n,
                           true, nex_p);

        sgj_haj_vi_nex(jsp, jop, 2, "R_SUP", SGJ_SEP_EQUAL_NO_SPACE,
                       !!(b[8] & 0x10), false, "Referrals supported");
        sgj_haj_vi_nex(jsp, jop, 2, "RTD_SUP", SGJ_SEP_EQUAL_NO_SPACE,
                       !!(b[8] & 0x8), false,
                       "Revert to defaults supported");
        sgj_haj_vi_nex(jsp, jop, 2, "HSSRELEF", SGJ_SEP_EQUAL_NO_SPACE,
                       !!(b[8] & 0x2), false,
                       "History snapshots release effects");
        sgj_haj_vi_nex(jsp, jop, 2, "CBCS", SGJ_SEP_EQUAL_NO_SPACE,
                       !!(b[8] & 0x1), false, "Capability-based command "
                       "security (obsolete SPC-5)");
        sgj_haj_vi(jsp, jop, 2, "Multi I_T nexus microcode download",
                   SGJ_SEP_EQUAL_NO_SPACE, b[9] & 0xf, true);
        sgj_haj_vi(jsp, jop, 2, "Extended self-test completion minutes",
                   SGJ_SEP_EQUAL_NO_SPACE,
                   sg_get_unaligned_be16(b + 10), true);
        sgj_haj_vi_nex(jsp, jop, 2, "POA_SUP", SGJ_SEP_EQUAL_NO_SPACE,
                       !!(b[12] & 0x80), false,
                       "Power on activation supported");
        sgj_haj_vi_nex(jsp, jop, 2, "HRA_SUP", SGJ_SEP_EQUAL_NO_SPACE,
                       !!(b[12] & 0x40), false,
                       "Hard reset activation supported");
        sgj_haj_vi_nex(jsp, jop, 2, "VSA_SUP", SGJ_SEP_EQUAL_NO_SPACE,
                       !!(b[12] & 0x20), false,
                       "Vendor specific activation supported");
        sgj_haj_vi_nex(jsp, jop, 2, "DMS_VALID", SGJ_SEP_EQUAL_NO_SPACE,
                       !!(b[12] & 0x10), false,
                       "Download microcode support byte valid");
        sgj_haj_vi(jsp, jop, 2, "Maximum supported sense data length",
                   SGJ_SEP_EQUAL_NO_SPACE, b[13], true);
        sgj_haj_vi_nex(jsp, jop, 2, "IBS", SGJ_SEP_EQUAL_NO_SPACE,
                       !!(b[14] & 0x80), false, "Implicit bind supported");
        sgj_haj_vi_nex(jsp, jop, 2, "IAS", SGJ_SEP_EQUAL_NO_SPACE,
                       !!(b[14] & 0x40), false,
                       "Implicit affiliation supported");
        sgj_haj_vi_nex(jsp, jop, 2, "SAC", SGJ_SEP_EQUAL_NO_SPACE,
                       !!(b[14] & 0x4), false,
                       "Set affiliation command supported");
        sgj_haj_vi_nex(jsp, jop, 2, "NRD1", SGJ_SEP_EQUAL_NO_SPACE,
                       !!(b[14] & 0x2), false,
                       "No redirect one supported (BIND)");
        sgj_haj_vi_nex(jsp, jop, 2, "NRD0", SGJ_SEP_EQUAL_NO_SPACE,
                       !!(b[14] & 0x1), false,
                       "No redirect zero supported (BIND)");
        sgj_haj_vi(jsp, jop, 2, "Maximum inquiry change logs",
                   SGJ_SEP_EQUAL_NO_SPACE,
                   sg_get_unaligned_be16(b + 15), true);
        sgj_haj_vi(jsp, jop, 2, "Maximum mode page change logs",
                   SGJ_SEP_EQUAL_NO_SPACE,
                   sg_get_unaligned_be16(b + 17), true);
        sgj_haj_vi_nex(jsp, jop, 2, "DM_MD_4", SGJ_SEP_EQUAL_NO_SPACE,
                       !!(b[19] & 0x80), false,
                       "Download microcode mode 4 supported");
        sgj_haj_vi_nex(jsp, jop, 2, "DM_MD_5", SGJ_SEP_EQUAL_NO_SPACE,
                       !!(b[19] & 0x40), false,
                       "Download microcode mode 5 supported");
        sgj_haj_vi_nex(jsp, jop, 2, "DM_MD_6", SGJ_SEP_EQUAL_NO_SPACE,
                       !!(b[19] & 0x20), false,
                       "Download microcode mode 6 supported");
        sgj_haj_vi_nex(jsp, jop, 2, "DM_MD_7", SGJ_SEP_EQUAL_NO_SPACE,
                       !!(b[19] & 0x10), false,
                       "Download microcode mode 7 supported");
        sgj_haj_vi_nex(jsp, jop, 2, "DM_MD_D", SGJ_SEP_EQUAL_NO_SPACE,
                       !!(b[19] & 0x8), false,
                       "Download microcode mode 0xd supported");
        sgj_haj_vi_nex(jsp, jop, 2, "DM_MD_E", SGJ_SEP_EQUAL_NO_SPACE,
                       !!(b[19] & 0x4), false,
                       "Download microcode mode 0xe supported");
        sgj_haj_vi_nex(jsp, jop, 2, "DM_MD_F", SGJ_SEP_EQUAL_NO_SPACE,
                       !!(b[19] & 0x2), false,
                       "Download microcode mode 0xf supported");
        if (do_long_nq || (! jsp->pr_out_hr))
            return;
    }
    sgj_pr_hr(jsp, "  ACTIVATE_MICROCODE=%d SPT=%d GRD_CHK=%d APP_CHK=%d "
              "REF_CHK=%d\n", ((b[4] >> 6) & 0x3), ((b[4] >> 3) & 0x7),
              !!(b[4] & 0x4), !!(b[4] & 0x2), !!(b[4] & 0x1));
    sgj_pr_hr(jsp, "  UASK_SUP=%d GROUP_SUP=%d PRIOR_SUP=%d HEADSUP=%d "
              "ORDSUP=%d SIMPSUP=%d\n", !!(b[5] & 0x20), !!(b[5] & 0x10),
              !!(b[5] & 0x8), !!(b[5] & 0x4), !!(b[5] & 0x2), !!(b[5] & 0x1));
    sgj_pr_hr(jsp, "  WU_SUP=%d [CRD_SUP=%d] NV_SUP=%d V_SUP=%d\n",
              !!(b[6] & 0x8), !!(b[6] & 0x4), !!(b[6] & 0x2), !!(b[6] & 0x1));
    sgj_pr_hr(jsp, "  NO_PI_CHK=%d P_I_I_SUP=%d LUICLR=%d\n", !!(b[7] & 0x20),
              !!(b[7] & 0x10), !!(b[7] & 0x1));
    /* RTD_SUP added in spc5r11, LU_COLL_TYPE added in spc5r09,
     * HSSRELEF added in spc5r02; CBCS obsolete in spc5r01 */
    sgj_pr_hr(jsp, "  LU_COLL_TYPE=%d R_SUP=%d RTD_SUP=%d HSSRELEF=%d "
              "[CBCS=%d]\n", (b[8] >> 5) & 0x7, !!(b[8] & 0x10),
              !!(b[8] & 0x8), !!(b[8] & 0x2), !!(b[8] & 0x1));
    sgj_pr_hr(jsp, "  Multi I_T nexus microcode download=%d\n", b[9] & 0xf);
    sgj_pr_hr(jsp, "  Extended self-test completion minutes=%d\n",
              sg_get_unaligned_be16(b + 10));    /* spc4r27 */
    sgj_pr_hr(jsp, "  POA_SUP=%d HRA_SUP=%d VSA_SUP=%d DMS_VALID=%d\n",
              !!(b[12] & 0x80), !!(b[12] & 0x40), !!(b[12] & 0x20),
              !!(b[12] & 0x10));                   /* spc5r20 */
    sgj_pr_hr(jsp, "  Maximum supported sense data length=%d\n",
              b[13]); /* spc4r34 */
    sgj_pr_hr(jsp, "  IBS=%d IAS=%d SAC=%d NRD1=%d NRD0=%d\n",
              !!(b[14] & 0x80), !!(b[14] & 0x40), !!(b[14] & 0x4),
              !!(b[14] & 0x2), !!(b[14] & 0x1));  /* added in spc5r09 */
    sgj_pr_hr(jsp, "  Maximum inquiry change logs=%u\n",
              sg_get_unaligned_be16(b + 15));        /* spc5r17 */
    sgj_pr_hr(jsp, "  Maximum mode page change logs=%u\n",
              sg_get_unaligned_be16(b + 17));        /* spc5r17 */
    sgj_pr_hr(jsp, "  DM_MD_4=%d DM_MD_5=%d DM_MD_6=%d DM_MD_7=%d\n",
              !!(b[19] & 0x80), !!(b[19] & 0x40), !!(b[19] & 0x20),
              !!(b[19] & 0x10));                     /* spc5r20 */
    sgj_pr_hr(jsp, "  DM_MD_D=%d DM_MD_E=%d DM_MD_F=%d\n",
              !!(b[19] & 0x8), !!(b[19] & 0x4), !!(b[19] & 0x2));
}

/* VPD_SOFTW_INF_ID   0x84 */
static void
decode_softw_inf_id(const uint8_t * buff, int len,
                    struct sdparm_opt_coll * op, sgj_opaque_p jap)
{
    sgj_state * jsp = &op->json_st;
    sgj_opaque_p jop;
    uint64_t ieee_id;

    len -= 4;
    buff += 4;
    for ( ; len > 5; len -= 6, buff += 6) {
        ieee_id = sg_get_unaligned_be48(buff + 0);
        sgj_pr_hr(jsp, "    IEEE identifier: 0x%" PRIx64 "\n", ieee_id);
        if (jsp->pr_as_json) {
            jop = sgj_new_unattached_object_r(jsp);
            sgj_js_nv_ihex(jsp, jop, "ieee_identifier", ieee_id);
            sgj_js_nv_o(jsp, jap, NULL /* name */, jop);
        }
   }
}

/* VPD_ATA_INFO    0x89 ["ai"] */
static void
decode_ata_info_vpd(const uint8_t * buff, int len, struct sdparm_opt_coll * op,
                    sgj_opaque_p jop)
{
    bool do_long_nq = op->do_long && (! op->do_quiet);
    int num, is_be, cc, n;
    sgj_state * jsp = &op->json_st;
    const char * cp;
    const char * ata_transp;
    char b[512];
    char d[80];
    static const int blen = sizeof(b);
    static const int dlen = sizeof(d);
    static const char * sat_vip = "SAT Vendor identification";
    static const char * sat_pip = "SAT Product identification";
    static const char * sat_prlp = "SAT Product revision level";

    if (len < 36) {
        pr2serr("%s length too short=%d\n", ai_vpdp, len);
        return;
    }
    if (op->do_hex > 0) {
        if (op->do_hex > 2)
            named_hhh_output(ai_vpdp, buff, len, op);
        else
            hex2stdout(buff, len, no_ascii_4hex(op));
        return;
    }
    memcpy(b, buff + 8, 8);
    b[8] = '\0';
    sgj_pr_hr(jsp, "  %s: %s\n", sat_vip, b);
    memcpy(b, buff + 16, 16);
    b[16] = '\0';
    sgj_pr_hr(jsp, "  %s: %s\n", sat_pip, b);
    memcpy(b, buff + 32, 4);
    b[4] = '\0';
    sgj_pr_hr(jsp, "  %s: %s\n", sat_prlp, b);
    if (len < 56)
        return;
    ata_transp = (0x34 == buff[36]) ? "SATA" : "PATA";
    if (do_long_nq) {
        sgj_pr_hr(jsp, "  Device signature [%s] (in hex):\n", ata_transp);
        hex2stdout(buff + 36, 20, 0);
    } else
        sgj_pr_hr(jsp, "  Device signature indicates %s transport\n",
                  ata_transp);
    cc = buff[56];      /* 0xec for IDENTIFY DEVICE and 0xa1 for IDENTIFY
                         * PACKET DEVICE (obsolete) */
    n = sg_scnpr(b, blen, "  Command code: 0x%x\n", cc);
    if (len < 60)
        return;
    if (0xec == cc)
        cp = null_s;
    else if (0xa1 == cc)
        cp = "PACKET ";
    else
        cp = NULL;
    is_be = sg_is_big_endian();
    if (cp) {
        n += sg_scn3pr(b, blen, n, "  ATA command IDENTIFY %sDEVICE "
                      "response summary:\n", cp);
        num = sg_ata_get_chars((const unsigned short *)(buff + 60), 27, 20,
                               is_be, d);
        d[num] = '\0';
        n += sg_scn3pr(b, blen, n, "    model: %s\n", d);
        num = sg_ata_get_chars((const unsigned short *)(buff + 60), 10, 10,
                               is_be, d);
        d[num] = '\0';
        n += sg_scn3pr(b, blen, n, "    serial number: %s\n", d);
        num = sg_ata_get_chars((const unsigned short *)(buff + 60), 23, 4,
                               is_be, d);
        d[num] = '\0';
        sg_scn3pr(b, blen, n, "    firmware revision: %s\n", d);
        sgj_pr_hr(jsp, "%s", b);
        if (do_long_nq)
            sgj_pr_hr(jsp, "  ATA command IDENTIFY %sDEVICE response in "
                      "hex:\n", cp);
    } else if (do_long_nq)
        sgj_pr_hr(jsp, "  ATA command 0x%x got following response:\n",
                  (unsigned int)cc);
    if (jsp->pr_as_json) {
        sgj_convert2snake(sat_vip, d, dlen);
        sgj_js_nv_s_len(jsp, jop, d, (const char *)(buff + 8), 8);
        sgj_convert2snake(sat_pip, d, dlen);
        sgj_js_nv_s_len(jsp, jop, d, (const char *)(buff + 16), 16);
        sgj_convert2snake(sat_prlp, d, dlen);
        sgj_js_nv_s_len(jsp, jop, d, (const char *)(buff + 32), 4);
        sgj_js_nv_hex_bytes(jsp, jop, "ata_device_signature", buff + 36, 20);
        sgj_js_nv_ihex(jsp, jop, "command_code", buff[56]);
        sgj_js_nv_s(jsp, jop, "ata_identify_device_data_example",
                    "sg_vpd -p ai -HHH /dev/sdc | hdparm --Istdin");
    }
    if (len < 572)
        return;
    if (2 == op->do_hex)
        hex2stdout((buff + 60), 512, 0);
    else if (do_long_nq)
        dWordHex((const unsigned short *)(buff + 60), 256, 0, is_be);
}

/* VPD_POWER_CONDITION 0x8a ["pc"] */
static void
decode_power_condition(const uint8_t * buff, int len,
                       struct sdparm_opt_coll * op, sgj_opaque_p jop)
{
    sgj_state * jsp = &op->json_st;

    if (len < 18) {
        pr2serr("%s length too short=%d\n", pc_vpdp, len);
        return;
    }
    sgj_pr_hr(jsp, "  Standby_y=%d Standby_z=%d Idle_c=%d Idle_b=%d "
              "Idle_a=%d\n", !!(buff[4] & 0x2), !!(buff[4] & 0x1),
              !!(buff[5] & 0x4), !!(buff[5] & 0x2), !!(buff[5] & 0x1));
    if (jsp->pr_as_json) {
        sgj_js_nv_ihex(jsp, jop, "standby_y", !!(buff[4] & 0x2));
        sgj_js_nv_ihex(jsp, jop, "standby_z", !!(buff[4] & 0x1));
        sgj_js_nv_ihex(jsp, jop, "idle_c", !!(buff[5] & 0x4));
        sgj_js_nv_ihex(jsp, jop, "idle_b", !!(buff[5] & 0x2));
        sgj_js_nv_ihex(jsp, jop, "idle_a", !!(buff[5] & 0x1));
    }
    sgj_haj_vi_nex(jsp, jop, 2, "Stopped condition recovery time",
                   SGJ_SEP_SPACE_1, sg_get_unaligned_be16(buff + 6),
                   true, "unit: millisecond");
    sgj_haj_vi_nex(jsp, jop, 2, "Standby_z condition recovery time",
                   SGJ_SEP_SPACE_1, sg_get_unaligned_be16(buff + 8),
                   true, "unit: millisecond");
    sgj_haj_vi_nex(jsp, jop, 2, "Standby_y condition recovery time",
                   SGJ_SEP_SPACE_1, sg_get_unaligned_be16(buff + 10),
                   true, "unit: millisecond");
    sgj_haj_vi_nex(jsp, jop, 2, "Idle_a condition recovery time",
                   SGJ_SEP_SPACE_1, sg_get_unaligned_be16(buff + 12),
                   true, "unit: millisecond");
    sgj_haj_vi_nex(jsp, jop, 2, "Idle_b condition recovery time",
                   SGJ_SEP_SPACE_1, sg_get_unaligned_be16(buff + 14),
                   true, "unit: millisecond");
    sgj_haj_vi_nex(jsp, jop, 2, "Idle_c condition recovery time",
                   SGJ_SEP_SPACE_1, sg_get_unaligned_be16(buff + 16),
                   true, "unit: millisecond");
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

/* VPD_POWER_CONSUMPTION  0x8d  ["psm"] */
static void
decode_power_consumption_vpd(const uint8_t * buff, int len,
                             struct sdparm_opt_coll * op, sgj_opaque_p jap)
{
    int k, bump, pcmp_id, pcmp_unit;
    unsigned int pcmp_val;
    sgj_state * jsp = &op->json_st;
    sgj_opaque_p jo2p;
    const uint8_t * bp;
    char b[128];
    static const int blen = sizeof(b);
    static const char * pcmp = "power_consumption";
    static const char * pci = "Power consumption identifier";
    static const char * mpc = "Maximum power consumption";

    if (len < 4) {
        pr2serr("%s length too short=%d\n", psm_vpdp, len);
        return;
    }
    len -= 4;
    bp = buff + 4;
    for (k = 0; k < len; k += bump, bp += bump) {
        bump = 4;
        if ((k + bump) > len) {
            pr2serr("short descriptor length=%d, left=%d\n", bump,
                    (len - k));
            return;
        }
        if (op->do_hex > 1)
            hex2stdout(bp, 4, 1);
        else {
            jo2p = sgj_new_unattached_object_r(jsp);
            pcmp_id = bp[0];
            pcmp_unit = 0x7 & bp[1];
            pcmp_val = sg_get_unaligned_be16(bp + 2);
            if (jsp->pr_as_json) {
                sgj_convert2snake(pci, b, blen);
                sgj_js_nv_ihex(jsp, jo2p, b, pcmp_id);
                snprintf(b, blen, "%s_units", pcmp);
                sgj_js_nv_ihexstr(jsp, jo2p, b, pcmp_unit, NULL,
                                  power_unit_arr[pcmp_unit]);
                snprintf(b, blen, "%s_value", pcmp);
                sgj_js_nv_ihex(jsp, jo2p, b, pcmp_val);
            }
            snprintf(b, blen, "  %s: 0x%x", pci, pcmp_id);
            if (pcmp_val >= 1000 && pcmp_unit > 0)
                sgj_pr_hr(jsp, "%s    %s: %d.%03d %s\n", b, mpc,
                          pcmp_val / 1000, pcmp_val % 1000,
                          power_unit_arr[pcmp_unit - 1]); /* up one unit */
            else
                sgj_pr_hr(jsp, "%s    %s: %u %s\n", b, mpc, pcmp_val,
                          power_unit_arr[pcmp_unit]);
            sgj_js_nv_o(jsp, jap, NULL /* name */, jo2p);
        }
    }
}

/* VPD_BLOCK_LIMITS    0xb0 ["bl"] */
void
decode_block_limits_vpd(const uint8_t * buff, int len,
                        struct sdparm_opt_coll * op, sgj_opaque_p jop)
{
    int wsnz, ugavalid;
    uint32_t u;
    uint64_t ull;
    sgj_state * jsp = &op->json_st;
    char b[144];
    static const int blen = sizeof(b);
    static const char * mcawl = "Maximum compare and write length";
    static const char * otlg = "Optimal transfer length granularity";
    static const char * cni = "command not implemented";
    static const char * ul = "unlimited";
    static const char * mtl = "Maximum transfer length";
    static const char * otl = "Optimal transfer length";
    static const char * mpl = "Maximum prefetch length";
    static const char * mulc = "Maximum unmap LBA count";
    static const char * mubdc = "Maximum unmap block descriptor count";
    static const char * oug = "Optimal unmap granularity";
    static const char * ugav = "Unmap granularity alignment valid";
    static const char * uga = "Unmap granularity alignment";
    static const char * mwsl = "Maximum write same length";
    static const char * matl = "Maximum atomic transfer length";
    static const char * aa = "Atomic alignment";
    static const char * atlg = "Atomic transfer length granularity";
    static const char * matlwab = "Maximum atomic transfer length with "
                                  "atomic boundary";
    static const char * mabs = "Maximum atomic boundary size";

#if 0
    if (op->do_hex > 0) {
        if (op->do_hex > 2)
            named_hhh_output(bl_vpdp, buff, len, op);
        else
            hex2stdout(buff, len, no_ascii_4hex(op));
        return;
    }
#endif
    if (len < 16) {
        pr2serr("%s length too short=%d\n", bl_vpdp, len);
        return;
    }
    wsnz = !!(buff[4] & 0x1);
    sgj_pr_hr(jsp, "  Write same non-zero (WSNZ): %d\n", wsnz);
    sgj_js_nv_ihex_nex(jsp, jop, "wsnz", wsnz, false,
                "Write Same Non-Zero (number of LBs must be > 0)");
    u = buff[5];
    if (0 == u) {
        sgj_pr_hr(jsp, "  %s: 0 blocks [%s]\n", mcawl, cni);
        sgj_convert2snake(mcawl, b, blen);
        sgj_js_nv_ihexstr(jsp, jop, b, u, NULL, cni);
    } else
        sgj_haj_vi_nex(jsp, jop, 2, mcawl, SGJ_SEP_COLON_1_SPACE, u,
                       true, "unit: LB");

    u = sg_get_unaligned_be16(buff + 6);
    if (0 == u) {
        sgj_pr_hr(jsp, "  %s: 0 blocks [%s]\n", otlg, nr_s);
        sgj_convert2snake(otlg, b, blen);
        sgj_js_nv_ihexstr(jsp, jop, b, u, NULL, nr_s);
    } else
        sgj_haj_vi_nex(jsp, jop, 2, otlg, SGJ_SEP_COLON_1_SPACE, u,
                       true, "unit: LB");

    u = sg_get_unaligned_be32(buff + 8);
    if (0 == u) {
        sgj_pr_hr(jsp, "  %s: 0 blocks [%s]\n", mtl, nr_s);
        sgj_convert2snake(mtl, b, blen);
        sgj_js_nv_ihexstr(jsp, jop, b, u, NULL, nr_s);
    } else
        sgj_haj_vi_nex(jsp, jop, 2, mtl, SGJ_SEP_COLON_1_SPACE, u,
                       true, "unit: LB");

    u = sg_get_unaligned_be32(buff + 12);
    if (0 == u) {
        sgj_pr_hr(jsp, "  %s: 0 blocks [%s]\n", otl, nr_s);
        sgj_convert2snake(otl, b, blen);
        sgj_js_nv_ihexstr(jsp, jop, b, u, NULL, nr_s);
    } else
        sgj_haj_vi_nex(jsp, jop, 2, otl, SGJ_SEP_COLON_1_SPACE, u,
                       true, "unit: LB");
    if (len > 19) {     /* added in sbc3r09 */
        u = sg_get_unaligned_be32(buff + 16);
        if (0 == u) {
            sgj_pr_hr(jsp, "  %s: 0 blocks [%s]\n", mpl, nr_s);
            sgj_convert2snake(mpl, b, blen);
            sgj_js_nv_ihexstr(jsp, jop, b, u, NULL, nr_s);
        } else
            sgj_haj_vi_nex(jsp, jop, 2, mpl, SGJ_SEP_COLON_1_SPACE, u,
                           true, "unit: LB");
    }
    if (len > 27) {     /* added in sbc3r18 */
        u = sg_get_unaligned_be32(buff + 20);
        sgj_convert2snake(mulc, b, blen);
        if (0 == u) {
            sgj_pr_hr(jsp, "  %s: 0 blocks [%s]\n", mulc, cni);
            sgj_js_nv_ihexstr(jsp, jop, b, u, NULL, cni);
        } else if (0xffffffff == u) {
            sgj_pr_hr(jsp, "  %s: %s blocks\n", ul, mulc);
            sgj_js_nv_ihexstr(jsp, jop, b, u, NULL, ul);
        } else
            sgj_haj_vi_nex(jsp, jop, 2, mulc, SGJ_SEP_COLON_1_SPACE, u,
                           true, "unit: LB");

        u = sg_get_unaligned_be32(buff + 24);
        sgj_convert2snake(mulc, b, blen);
        if (0 == u) {
            sgj_pr_hr(jsp, "  %s: 0 block descriptors [%s]\n", mubdc, cni);
            sgj_js_nv_ihexstr(jsp, jop, b, u, NULL, cni);
        } else if (0xffffffff == u) {
            sgj_pr_hr(jsp, "  %s: %s block descriptors\n", ul, mubdc);
            sgj_js_nv_ihexstr(jsp, jop, b, u, NULL, ul);
        } else
            sgj_haj_vi(jsp, jop, 2, mubdc, SGJ_SEP_COLON_1_SPACE,
                       u, true);
    }
    if (len > 35) {     /* added in sbc3r19 */
        u = sg_get_unaligned_be32(buff + 28);
        if (0 == u) {
            sgj_pr_hr(jsp, "  %s: 0 blocks [%s]\n", oug, nr_s);
            sgj_convert2snake(oug, b, blen);
            sgj_js_nv_ihexstr(jsp, jop, b, u, NULL, nr_s);
        } else
            sgj_haj_vi_nex(jsp, jop, 2, oug, SGJ_SEP_COLON_1_SPACE, u,
                           true, "unit: LB");

        ugavalid = !!(buff[32] & 0x80);
        sgj_pr_hr(jsp, "  %s: %s\n", ugav, ugavalid ? "true" : "false");
        sgj_js_nv_i(jsp, jop, ugav, ugavalid);
        if (ugavalid) {
            u = 0x7fffffff & sg_get_unaligned_be32(buff + 32);
            sgj_haj_vi_nex(jsp, jop, 2, uga, SGJ_SEP_COLON_1_SPACE, u,
                           true, "unit: LB");
        }
    }
    if (len > 43) {     /* added in sbc3r26 */
        ull = sg_get_unaligned_be64(buff + 36);
        if (0 == ull) {
            sgj_pr_hr(jsp, "  %s: 0 blocks [%s]\n", mwsl, nr_s);
            sgj_convert2snake(mwsl, b, blen);
            sgj_js_nv_ihexstr(jsp, jop, b, ull, NULL, nr_s);
        } else
            sgj_haj_vi_nex(jsp, jop, 2, mwsl, SGJ_SEP_COLON_1_SPACE,
                           ull, true, "unit: LB");
    }
    if (len > 47) {     /* added in sbc4r02 */
        u = sg_get_unaligned_be32(buff + 44);
        if (0 == u) {
            sgj_pr_hr(jsp, "  %s: 0 blocks [%s]\n", matl, nr_s);
            sgj_convert2snake(matl, b, blen);
            sgj_js_nv_ihexstr(jsp, jop, b, u, NULL, nr_s);
        } else
            sgj_haj_vi_nex(jsp, jop, 2, matl, SGJ_SEP_COLON_1_SPACE,
                           u, true, "unit: LB");

        u = sg_get_unaligned_be32(buff + 48);
        if (0 == u) {
            static const char * uawp = "unaligned atomic writes permitted";

            sgj_pr_hr(jsp, "  %s: 0 blocks [%s]\n", aa, uawp);
            sgj_convert2snake(aa, b, blen);
            sgj_js_nv_ihexstr(jsp, jop, b, u, NULL, uawp);
        } else
            sgj_haj_vi_nex(jsp, jop, 2, aa, SGJ_SEP_COLON_1_SPACE,
                           u, true, "unit: LB");

        u = sg_get_unaligned_be32(buff + 52);
        if (0 == u) {
            static const char * ngr = "no granularity requirement";

            sgj_pr_hr(jsp, "  %s: 0 blocks [%s]\n", atlg, ngr);
            sgj_convert2snake(atlg, b, blen);
            sgj_js_nv_ihexstr(jsp, jop, b, u, NULL, ngr);
        } else
            sgj_haj_vi_nex(jsp, jop, 2, aa, SGJ_SEP_COLON_1_SPACE,
                           u, true, "unit: LB");
    }
    if (len > 56) {
        u = sg_get_unaligned_be32(buff + 56);
        if (0 == u) {
            sgj_pr_hr(jsp, "  %s: 0 blocks [%s]\n", matlwab, nr_s);
            sgj_convert2snake(matlwab, b, blen);
            sgj_js_nv_ihexstr(jsp, jop, b, u, NULL, nr_s);
        } else
            sgj_haj_vi_nex(jsp, jop, 2, matlwab, SGJ_SEP_COLON_1_SPACE,
                           u, true, "unit: LB");

        u = sg_get_unaligned_be32(buff + 60);
        if (0 == u) {
            static const char * cowa1b = "can only write atomic 1 block";

            sgj_pr_hr(jsp, "  %s: 0 blocks [%s]\n", mabs, cowa1b);
            sgj_convert2snake(mabs, b, blen);
            sgj_js_nv_ihexstr(jsp, jop, b, u, NULL, cowa1b);
        } else
            sgj_haj_vi_nex(jsp, jop, 2, mabs, SGJ_SEP_COLON_1_SPACE,
                           u, true, "unit: LB");
    }
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

/* ZONED field here replaced by ZONED BLOCK DEVICE EXTENSION field in the
 * Zoned Block Device Characteristics VPD page. The new field includes
 * Zone Domains and Realms (see ZBC-2) */
static const char * bdc_zoned_strs[] = {
    nr_s,
    "host-aware",
    "host-managed",
    rsv_s,
};

/* VPD_BLOCK_DEV_CHARS    0xb1 ["bdc"] */
static void
decode_block_dev_ch_vpd(const uint8_t * buff, int len,
                        struct sdparm_opt_coll * op, sgj_opaque_p jop)
{
    int zoned;
    unsigned int u, k;
    sgj_state * jsp = &op->json_st;
    const char * cp;
    char b[144];
    static const char * mrr_j = "medium_rotation_rate";
    static const char * mrr_h = "Medium rotation rate";
    static const char * nrm = "Non-rotating medium (e.g. solid state)";
    static const char * pt_j = "product_type";

    if (len < 64) {
        pr2serr("%s length too short=%d\n", bdc_vpdp, len);
        return;
    }
    u = sg_get_unaligned_be16(buff + 4);
    if (0 == u) {
        sgj_pr_hr(jsp, "  %s is %s\n", mrr_h, nr_s);
        sgj_js_nv_ihexstr(jsp, jop, mrr_j, 0, NULL, nr_s);
    } else if (1 == u) {
        sgj_pr_hr(jsp, "  %s\n", nrm);
        sgj_js_nv_ihexstr(jsp, jop, mrr_j, 1, NULL, nrm);
    } else if ((u < 0x401) || (0xffff == u)) {
        sgj_pr_hr(jsp, "  %s [0x%x]\n", rsv_s, u);
        sgj_js_nv_ihexstr(jsp, jop, mrr_j, u, NULL, rsv_s);
    } else {
        sgj_js_nv_ihex_nex(jsp, jop, mrr_j, u, true,
                           "unit: rpm; nominal rotation rate");
    }
    u = buff[6];
    k = SG_ARRAY_SIZE(product_type_arr);
    if (u < k) {
        sgj_pr_hr(jsp, "  %s: %s\n", "Product type", product_type_arr[u]);
        sgj_js_nv_ihexstr(jsp, jop, pt_j, u, NULL, product_type_arr[u]);
    } else {
        sgj_pr_hr(jsp, "  %s: %s [0x%x]\n", "Product type",
                  (u < 0xf0) ? rsv_s : vs_s, u);
        sgj_js_nv_ihexstr(jsp, jop, pt_j, u, NULL, (u < 0xf0) ? rsv_s : vs_s);
    }
    sgj_haj_vi_nex(jsp, jop, 2, "WABEREQ", SGJ_SEP_EQUAL_NO_SPACE,
                   (buff[7] >> 6) & 0x3, false,
                   "Write After Block Erase REQuired");
    sgj_haj_vi_nex(jsp, jop, 2, "WACEREQ", SGJ_SEP_EQUAL_NO_SPACE,
                   (buff[7] >> 4) & 0x3, false,
                   "Write After Cryptographic Erase REQuired");
    u = buff[7] & 0xf;
    switch (u) {
    case 0:
        strcpy(b, nr_s);
        break;
    case 1:
        strcpy(b, "5.25 inch");
        break;
    case 2:
        strcpy(b, "3.5 inch");
        break;
    case 3:
        strcpy(b, "2.5 inch");
        break;
    case 4:
        strcpy(b, "1.8 inch");
        break;
    case 5:
        strcpy(b, "less then 1.8 inch");
        break;
    default:
        strcpy(b, rsv_s);
        break;
    }
    sgj_pr_hr(jsp, "  Nominal form factor: %s\n", b);
    sgj_js_nv_ihexstr(jsp, jop, "nominal_form_factor", u, NULL, b);
    sgj_haj_vi_nex(jsp, jop, 2, "MACT", SGJ_SEP_EQUAL_NO_SPACE,
                   !!(buff[8] & 0x40), false, "Multiple ACTuator");
    zoned = (buff[8] >> 4) & 0x3;   /* added sbc4r04, obsolete sbc5r01 */
    cp = bdc_zoned_strs[zoned];
    sgj_pr_hr(jsp, "  ZONED=%d [%s]\n", zoned, cp);
    sgj_js_nv_ihexstr_nex(jsp, jop, "zoned", zoned, false, NULL,
                          cp, "Added in SBC-4, obsolete in SBC-5");
    sgj_haj_vi_nex(jsp, jop, 2, "RBWZ", SGJ_SEP_EQUAL_NO_SPACE,
                   !!(buff[8] & 0x4), false,
                   "Background Operation Control Supported");
    sgj_haj_vi_nex(jsp, jop, 2, "FUAB", SGJ_SEP_EQUAL_NO_SPACE,
                   !!(buff[8] & 0x2), false,
                   "Force Unit Access Behaviour");
    sgj_haj_vi_nex(jsp, jop, 2, "VBULS", SGJ_SEP_EQUAL_NO_SPACE,
                   !!(buff[8] & 0x1), false,
                   "Verify Byte check Unmapped Lba Supported");
    u = sg_get_unaligned_be32(buff + 12);
    sgj_haj_vi_nex(jsp, jop, 2, "DEPOPULATION TIME", SGJ_SEP_COLON_1_SPACE,
                   u, true, "unit: second");
}

/* VPD_SA_DEV_CAP  0xb0 */
static void
decode_tape_dev_caps_vpd(uint8_t * buff, int len,
                         struct sdparm_opt_coll * op, sgj_opaque_p jop)
{
    sgj_state * jsp = &op->json_st;

    if (len < 6) {
        pr2serr("%s length too short=%d\n", sad_vpdp, len);
        return;
    }
    sgj_haj_vi_nex(jsp, jop, 2, "TSMC", SGJ_SEP_EQUAL_NO_SPACE,
                   !!(buff[4] & 0x2), false, "Tape Stream Mirror Capable");
    sgj_haj_vi_nex(jsp, jop, 2, "WORM", SGJ_SEP_EQUAL_NO_SPACE,
                   !!(buff[4] & 0x1), false,
                   "Write Once Read Multiple supported");
    return;
}

/* VPD_MAN_ASS_SN  0xb1 */
static void
decode_tape_man_ass_sn_vpd(uint8_t * buff, int len,
                          struct sdparm_opt_coll * op, sgj_opaque_p jop)
{
    sgj_state * jsp = &op->json_st;

    if (len < 4) {
        pr2serr("%s length too short=%d\n", masn_vpdp, len);
        return;
    }
    sgj_pr_hr(jsp, "  Manufacturer-assigned serial number: %.*s\n",
              len - 4, buff + 4);
    sgj_js_nv_s_len(jsp, jop, "manufacturer_assigned_serial_number",
                    (const char *)buff + 4, len - 4);
}

static const char * prov_type_arr[8] = {
    "not known or fully provisioned",
    "resource provisioned",
    "thin provisioned",
    rsv_s,
    rsv_s,
    rsv_s,
    rsv_s,
    rsv_s,
};

/* VPD_LB_PROVISIONING   0xb2 ["lbpv"] */
static void
decode_block_lb_prov_vpd(const uint8_t * buff, int len,
                         struct sdparm_opt_coll * op, sgj_opaque_p jop)
{
    unsigned int u, dp, pt, t_exp;
    sgj_state * jsp = &op->json_st;
    const char * cp;
    char b[1024];
    static const int blen = sizeof(b);
    static const char * mp = "Minimum percentage";
    static const char * tp = "Threshold percentage";
    static const char * pgd = "Provisioning group descriptor";

#if 0
    if (op->do_hex > 0) {
        if (op->do_hex > 2)
            named_hhh_output(lbpv_vpdp, buff, len, op);
        else
            hex2stdout(buff, len, no_ascii_4hex(op));
        return 0;
    }
#endif
    if (len < 4) {
        pr2serr("%s too short=%d\n", lbpv_vpdp, len);
        return;
    }
    t_exp = buff[4];
    sgj_js_nv_ihexstr(jsp, jop, "threshold_exponent", t_exp, NULL,
                      (0 == t_exp) ? ns_s : NULL);
    sgj_haj_vi_nex(jsp, jop, 2, "LBPU", SGJ_SEP_EQUAL_NO_SPACE,
                   !!(buff[5] & 0x80), false,
                   "Logical Block Provisioning Unmap command supported");
    sgj_haj_vi_nex(jsp, jop, 2, "LBPWS", SGJ_SEP_EQUAL_NO_SPACE,
                   !!(buff[5] & 0x40), false, "Logical Block Provisioning "
                   "Write Same (16) command supported");
    sgj_haj_vi_nex(jsp, jop, 2, "LBPWS10", SGJ_SEP_EQUAL_NO_SPACE,
                   !!(buff[5] & 0x20), false, "Logical Block Provisioning "
                   "Write Same (10) command supported");
    sgj_haj_vi_nex(jsp, jop, 2, "LBPRZ", SGJ_SEP_EQUAL_NO_SPACE,
                   (0x7 & (buff[5] >> 2)), true,
                   "Logical Block Provisioning Read Zero");
    sgj_haj_vi_nex(jsp, jop, 2, "ANC_SUP", SGJ_SEP_EQUAL_NO_SPACE,
                   !!(buff[5] & 0x2), false,
                   "ANChor SUPported");
    dp = !!(buff[5] & 0x1);
    sgj_haj_vi_nex(jsp, jop, 2, "DP", SGJ_SEP_EQUAL_NO_SPACE,
                   dp, false, "Descriptor Present");
    u = 0x1f & (buff[6] >> 3);  /* minimum percentage */
    if (0 == u)
        sgj_pr_hr(jsp, "  %s: 0 [%s]\n", mp, nr_s);
    else
        sgj_pr_hr(jsp, "  %s: %u\n", mp, u);
    sgj_convert2snake(mp, b, blen);
    sgj_js_nv_ihexstr(jsp, jop, b, u, NULL, (0 == u) ? nr_s : NULL);
    pt = buff[6] & 0x7;
    cp = prov_type_arr[pt];
    if (pt > 2)
        snprintf(b, blen, " [%u]", u);
    else
        b[0] = '\0';
    sgj_pr_hr(jsp, "  Provisioning type: %s%s\n", cp, b);
    sgj_js_nv_ihexstr(jsp, jop, "provisioning_type", pt, NULL, cp);
    u = buff[7];        /* threshold percentage */
    strcpy(b, tp);
    if (0 == u)
        sgj_pr_hr(jsp, "  %s: 0 [percentages %s]\n", b, ns_s);
    else
        sgj_pr_hr(jsp, "  %s: %u", b, u);
    sgj_convert2snake(tp, b, blen);
    sgj_js_nv_ihexstr(jsp, jop, b, u, NULL, (0 == u) ? ns_s : NULL);
    if (dp && (len > 11)) {
        int i_len;
        const uint8_t * bp;
        sgj_opaque_p jo2p;

        bp = buff + 8;
        i_len = bp[3];
        if (0 == i_len) {
            pr2serr("%s too short=%d\n", pgd, i_len);
            return;
        }
        if (jsp->pr_as_json) {
            jo2p = sgj_snake_named_subobject_r(jsp, jop, pgd);
            sgj_js_designation_descriptor(jsp, jo2p, bp, i_len + 4);
        }
        sgj_pr_hr(jsp, "  %s:\n", pgd);
        sg_get_designation_descriptor_str("    ", bp, i_len + 4, true,
                                          op->do_long, blen, b);
        if (jsp->pr_as_json && jsp->pr_out_hr)
            sgj_hr_str_out(jsp, b, strlen(b));
        else
            sgj_pr_hr(jsp, "%s", b);
    }
}

/* VPD_TA_SUPPORTED  0xb2 ["tas"] */
static void
decode_tapealert_supported_vpd(const uint8_t * buff, int len,
                               struct sdparm_opt_coll * op, sgj_opaque_p jop)
{
    bool have_ta_strs = !! sg_lib_tapealert_strs[0];
    int k, mod, div, n;
    unsigned int supp;
    sgj_state * jsp = &op->json_st;
    char b[144];
    char d[64];
    static const int blen = sizeof(b);

#if 0
    if (op->do_hex > 0) {
        if (op->do_hex > 2)
            named_hhh_output(tas_vpdp, buff, len, op);
        else
            hex2stdout(buff, len, no_ascii_4hex(op));
        return;
    }
#endif
    if (len < 12) {
        pr2serr("%s length too short=%d\n", tas_vpdp, len);
        return;
    }
    b[0] ='\0';
    for (k = 1, n = 0; k < 0x41; ++k) {
        mod = ((k - 1) % 8);
        div = (k - 1) / 8;
        supp = !! (buff[4 + div] & (1 << (7 - mod)));
        if (jsp->pr_as_json) {
            snprintf(d, sizeof(d), "flag%02xh", k);
            if (have_ta_strs)
                sgj_js_nv_ihex_nex(jsp, jop, d, supp, false,
                                   sg_lib_tapealert_strs[k]);
            else
                sgj_js_nv_i(jsp, jop, d, supp);
        }
        if (0 == mod) {
            if (div > 0) {
                sgj_pr_hr(jsp, "%s\n", b);
                n = 0;
            }
            n += sg_scn3pr(b, blen, n, "  Flag%02Xh: %d", k, supp);
        } else
            n += sg_scn3pr(b, blen, n, "  %02Xh: %d", k, supp);
    }
    sgj_pr_hr(jsp, "%s\n", b);
}

/* VPD_REFERRALS   0xb3 ["ref"] */
static void
decode_referrals_vpd(const uint8_t * buff, int len,
                     struct sdparm_opt_coll * op, sgj_opaque_p jop)
{
    uint32_t u;
    sgj_state * jsp = &op->json_st;
    char b[64];

#if 0
    if (op->do_hex > 0) {
        if (op->do_hex > 2)
            named_hhh_output(ref_vpdp, buff, len, op);
        else
            hex2stdout(buff, len, no_ascii_4hex(op));
        return;
    }
#endif
    if (len < 16) {
        pr2serr("%s length too short=%d\n", ref_vpdp, len);
        return;
    }
    u = sg_get_unaligned_be32(buff + 8);
    strcpy(b, "  User data segment size: ");
    if (0 == u)
        sgj_pr_hr(jsp, "%s0 [per sense descriptor]\n", b);
    else
        sgj_pr_hr(jsp, "%s%u\n", b, u);
    sgj_js_nv_ihex(jsp, jop, "user_data_segment_size", u);
    u = sg_get_unaligned_be32(buff + 12);
    sgj_haj_vi(jsp, jop, 2, "User data segment multiplier",
               SGJ_SEP_COLON_1_SPACE, u, true);
}

/* VPD_SUP_BLOCK_LENS  0xb4 ["sbl"] (added sbc4r01) */
static void
decode_sup_block_lens_vpd(const uint8_t * buff, int len,
                          struct sdparm_opt_coll * op, sgj_opaque_p jap)
{
    int k;
    unsigned int u;
    const uint8_t * bp;
    sgj_state * jsp = &op->json_st;
    sgj_opaque_p jo2p = NULL;

#if 0
    if (op->do_hex > 0) {
        if (op->do_hex > 2)
            named_hhh_output(sbl_vpdp, buff, len, op);
        else
            hex2stdout(buff, len, no_ascii_4hex(op));
        return;
    }
#endif
    if (len < 4) {
        pr2serr("%s length too short=%d\n", sbl_vpdp, len);
        return;
    }
    len -= 4;
    bp = buff + 4;
    for (k = 0; k < len; k += 8, bp += 8) {
        if (jsp->pr_as_json)
            jo2p = sgj_new_unattached_object_r(jsp);
        u = sg_get_unaligned_be32(bp);
        sgj_haj_vi(jsp, jo2p, 2, "Logical block length",
                   SGJ_SEP_COLON_1_SPACE, u, true);
        sgj_haj_vi_nex(jsp, jo2p, 4, "P_I_I_SUP",
                       SGJ_SEP_COLON_1_SPACE, !!(bp[4] & 0x40), false,
                       "Protection Information Interval SUPported");
        sgj_haj_vi_nex(jsp, jo2p, 4, "NO_PI_CHK",
                       SGJ_SEP_COLON_1_SPACE, !!(bp[4] & 0x8), false,
                       "NO Protection Information CHecKing");
        sgj_haj_vi_nex(jsp, jo2p, 4, "GRD_CHK", SGJ_SEP_COLON_1_SPACE,
                       !!(bp[4] & 0x4), false, "GuaRD CHecK");
        sgj_haj_vi_nex(jsp, jo2p, 4, "APP_CHK", SGJ_SEP_COLON_1_SPACE,
                       !!(bp[4] & 0x2), false, "APPlication tag CHecK");
        sgj_haj_vi_nex(jsp, jo2p, 4, "REF_CHK", SGJ_SEP_COLON_1_SPACE,
                       !!(bp[4] & 0x1), false, "REFerence tag CHecK");
        sgj_haj_vi_nex(jsp, jo2p, 4, "T3PS", SGJ_SEP_COLON_1_SPACE,
                       !!(bp[5] & 0x8), false, "Type 3 Protection Supported");
        sgj_haj_vi_nex(jsp, jo2p, 4, "T2PS", SGJ_SEP_COLON_1_SPACE,
                       !!(bp[5] & 0x4), false, "Type 2 Protection Supported");
        sgj_haj_vi_nex(jsp, jo2p, 4, "T1PS", SGJ_SEP_COLON_1_SPACE,
                       !!(bp[5] & 0x2), false, "Type 1 Protection Supported");
        sgj_haj_vi_nex(jsp, jo2p, 4, "T0PS", SGJ_SEP_COLON_1_SPACE,
                       !!(bp[5] & 0x1), false, "Type 0 Protection Supported");
        sgj_js_nv_o(jsp, jap, NULL /* name */, jo2p);
    }
}

/* VPD_BLOCK_DEV_C_EXTENS  0xb5 ["bdce"] (added sbc4r02) */
static void
decode_block_dev_char_ext_vpd(const uint8_t * buff, int len,
                              struct sdparm_opt_coll * op, sgj_opaque_p jop)
{
    bool b_active = false;
    bool combined = false;
    int n;
    uint32_t u;
    sgj_state * jsp = &op->json_st;
    const char * utp;
    const char * uup;
    const char * uip;
    char b[128];
    static const int blen = sizeof(b);

#if 0
    if (op->do_hex > 0) {
        if (op->do_hex > 2)
            named_hhh_output(bdce_vpdp, buff, len, op);
        else
            hex2stdout(buff, len, no_ascii_4hex(op));
        return;
    }
#endif
    if (len < 16) {
        pr2serr("%s length too short=%d\n", bdce_vpdp, len);
        return;
    }
    switch (buff[5]) {
    case 1:
        utp = "Combined writes and reads";
        combined = true;
        break;
    case 2:
        utp = "Writes only";
        break;
    case 3:
        utp = "Separate writes and reads";
        b_active = true;
        break;
    default:
        utp = rsv_s;
        break;
    }
    sgj_haj_vistr(jsp, jop, 2, "Utilization type", SGJ_SEP_COLON_1_SPACE,
                  buff[5], true, utp);
    switch (buff[6]) {
    case 2:
        uup = "megabytes";
        break;
    case 3:
        uup = "gigabytes";
        break;
    case 4:
        uup = "terabytes";
        break;
    case 5:
        uup = "petabytes";
        break;
    case 6:
        uup = "exabytes";
        break;
    default:
        uup = rsv_s;
        break;
    }
    sgj_haj_vistr(jsp, jop, 2, "Utilization units", SGJ_SEP_COLON_1_SPACE,
                  buff[6], true, uup);
    switch (buff[7]) {
    case 0xa:
        uip = "per day";
        break;
    case 0xe:
        uip = "per year";
        break;
    default:
        uip = rsv_s;
        break;
    }
    sgj_haj_vistr(jsp, jop, 2, "Utilization interval", SGJ_SEP_COLON_1_SPACE,
                  buff[7], true, uip);
    u = sg_get_unaligned_be32(buff + 8);
    sgj_haj_vistr(jsp, jop, 2, "Utilization B", SGJ_SEP_COLON_1_SPACE,
                  u, true, (b_active ? NULL : rsv_s));
    n = sg_scnpr(b, blen, "%s: ", "Designed utilization");
    if (b_active)
        n += sg_scn3pr(b, blen, n, "%u %s for reads and ", u, uup);
    u = sg_get_unaligned_be32(buff + 12);
    sgj_haj_vi(jsp, jop, 2, "Utilization A", SGJ_SEP_COLON_1_SPACE, u, true);
    sg_scn3pr(b, blen, n, "%u %s for %swrites, %s", u, uup,
             combined ? "reads and " : null_s, uip);
    sgj_pr_hr(jsp, "  %s\n", b);
    if (jsp->pr_string)
        sgj_js_nv_s(jsp, jop, "summary", b);
}

/* VPD_LB_PROTECTION 0xb5 (SSC)  [added in ssc5r02a] */
static void
decode_lb_protection_vpd(const uint8_t * buff, int len,
                         struct sdparm_opt_coll * op, sgj_opaque_p jap)
{
    int k, bump;
    const uint8_t * bp;
    sgj_state * jsp = &op->json_st;
    sgj_opaque_p jo2p = NULL;

#if 0
    if (op->do_hex > 0) {
        if (op->do_hex > 2)
            named_hhh_output(lbpro_vpdp, buff, len, op);
        else
            hex2stdout(buff, len, no_ascii_4hex(op));
        return;
    }
#endif
    if (len < 8) {
        pr2serr("%s length too short=%d\n", lbpro_vpdp, len);
        return;
    }
    len -= 8;
    bp = buff + 8;
    for (k = 0; k < len; k += bump, bp += bump) {
        jo2p = sgj_new_unattached_object_r(jsp);
        bump = 1 + bp[0];
        sgj_pr_hr(jsp, "  method: %d, info_len: %d, LBP_W_C=%d, LBP_R_C=%d, "
                  "RBDP_C=%d\n", bp[1], 0x3f & bp[2], !!(0x80 & bp[3]),
                  !!(0x40 & bp[3]), !!(0x20 & bp[3]));
        sgj_js_nv_ihex(jsp, jo2p, "logical_block_protection_method", bp[1]);
        sgj_js_nv_ihex_nex(jsp, jo2p,
                           "logical_block_protection_information_length",
                           0x3f & bp[2], true, "unit: byte");
        sgj_js_nv_ihex_nex(jsp, jo2p, "lbp_w_c", !!(0x80 & bp[3]), false,
                           "Logical Blocks Protected during Write supported");
        sgj_js_nv_ihex_nex(jsp, jo2p, "lbp_r_c", !!(0x40 & bp[3]), false,
                           "Logical Blocks Protected during Read supported");
        sgj_js_nv_ihex_nex(jsp, jo2p, "rbdp_c", !!(0x20 & bp[3]), false,
                           "Recover Buffered Data Protected supported");
        if ((k + bump) > len) {
            pr2serr("Logical block protection %s, short descriptor "
                    "length=%d, left=%d\n", vpd_pg_s, bump, (len - k));
            sgj_js_nv_o(jsp, jap, NULL /* name */, jo2p);
            return;
        }
        sgj_js_nv_o(jsp, jap, NULL /* name */, jo2p);
    }
}

#if 0
static char *
fp_schema_t_s(uint8_t sch_type, int blen, char * b)
{
    if ((NULL == b) || (blen < 1))
        return NULL;
    switch (sch_type) {
    case 1:
        snprintf(b, blen, "no zones defined [1]");
        break;
    case 2:
        snprintf(b, blen, "host-aware zoned device model [2]");
        break;
    case 3:
        snprintf(b, blen, "host-managed zoned device model [3]");
        break;
    case 4:
        snprintf(b, blen, "zone domains and realms zoned device model [4]");
        break;
    default:
        snprintf(b, blen, "unknown [%u]", sch_type);
        break;
    }
    return b;
}
#endif

static const char * sch_type_arr[8] = {
    rsv_s,
    "non-zoned",
    "host aware zoned",
    "host managed zoned",
    "zone domain and realms zoned",
    rsv_s,
    rsv_s,
    rsv_s,
};

static char *
get_zone_align_method(uint8_t val, char * b, int blen)
{
   if (blen < 32)
        return b;
   switch (val) {
    case 0:
        strcpy(b, nr_s);
        break;
    case 1:
        strcpy(b, "using constant zone lengths");
        break;
    case 8:
        strcpy(b, "taking gap zones into account");
        break;
    default:
        strcpy(b, rsv_s);
        break;
    }
    return b;
}

/* VPD_FORMAT_PRESETS  0xb8 ["fp"] (added sbc4r18) */
static void
decode_format_presets_vpd(const uint8_t * buff, int len,
                          struct sdparm_opt_coll * op, sgj_opaque_p jap)
{
    uint8_t sch_type;
    int k;
    uint32_t u;
    uint64_t ul;
    sgj_state * jsp = &op->json_st;
    const uint8_t * bp;
    sgj_opaque_p jo2p, jo3p;
    const char * cp;
    char b[128];
    char d[64];
    static const int blen = sizeof(b);
    static const int dlen = sizeof(d);
    static const char * llczp = "Low LBA conventional zones percentage";
    static const char * hlczp = "High LBA conventional zones percentage";
    static const char * ztzd = "Zone type for zone domain";

#if 0
    if (op->do_hex > 0) {
        if (op->do_hex > 2)
            named_hhh_output(fp_vpdp, buff, len, op);
        else
            hex2stdout(buff, len, no_ascii_4hex(op));
        return;
    }
#endif
    if (len < 4) {
        pr2serr("%s length too short=%d\n", fp_vpdp, len);
        return;
    }
    len -= 4;
    bp = buff + 4;
    for (k = 0; k < len; k += 64, bp += 64) {
        jo2p = sgj_new_unattached_object_r(jsp);
        sgj_haj_vi(jsp, jo2p, 2, "Preset identifier", SGJ_SEP_COLON_1_SPACE,
                   sg_get_unaligned_be64(bp + 0), true);
        sch_type = bp[4];
        if (sch_type < 8) {
            cp = sch_type_arr[sch_type];
            if (rsv_s != cp)
                snprintf(b, blen, "%s block device", cp);
            else
                snprintf(b, blen, "%s", cp);
        } else
            strcpy(b, rsv_s);
        sgj_haj_vistr(jsp, jo2p, 4, "Schema type", SGJ_SEP_COLON_1_SPACE,
                      sch_type, true, b);
        sgj_haj_vi(jsp, jo2p, 4, "Logical blocks per physical block "
                   "exponent", SGJ_SEP_COLON_1_SPACE,
                   0xf & bp[7], true);
        sgj_haj_vi_nex(jsp, jo2p, 4, "Logical block length",
                       SGJ_SEP_COLON_1_SPACE, sg_get_unaligned_be32(bp + 8),
                       true, "unit: byte");
        sgj_haj_vi(jsp, jo2p, 4, "Designed last Logical Block Address",
                   SGJ_SEP_COLON_1_SPACE,
                   sg_get_unaligned_be64(bp + 16), true);
        sgj_haj_vi_nex(jsp, jo2p, 4, "FMTPINFO", SGJ_SEP_COLON_1_SPACE,
                       (bp[38] >> 6) & 0x3, false,
                       "ForMaT Protection INFOrmation (see Format Unit)");
        sgj_haj_vi(jsp, jo2p, 4, "Protection field usage",
                   SGJ_SEP_COLON_1_SPACE, bp[38] & 0x7, false);
        sgj_haj_vi(jsp, jo2p, 4, "Protection interval exponent",
                   SGJ_SEP_COLON_1_SPACE, bp[39] & 0xf, true);
        jo3p = sgj_named_subobject_r(jsp, jo2p,
                                     "schema_type_specific_information");
        switch (sch_type) {
        case 2:
            sgj_pr_hr(jsp, "    Defines zones for host aware device:\n");
            u = bp[40 + 0];
            sgj_pr_hr(jsp, "      %s: %u.%u %%\n", llczp, u / 10, u % 10);
            sgj_convert2snake(llczp, b, blen);
            sgj_js_nv_ihex_nex(jsp, jo3p, b, u, true, "unit: 1/10 of a "
                               "percent");
            u = bp[40 + 1];
            sgj_pr_hr(jsp, "      %s: %u.%u %%\n", hlczp, u / 10, u % 10);
            sgj_convert2snake(hlczp, b, blen);
            sgj_js_nv_ihex_nex(jsp, jo3p, b, u, true, "unit: 1/10 of a "
                               "percent");
            u = sg_get_unaligned_be32(bp + 40 + 12);
            sgj_haj_vistr(jsp, jo3p, 6, "Logical blocks per zone",
                          SGJ_SEP_COLON_1_SPACE, u, true,
                          (0 == u ? rsv_s : NULL));
            break;
        case 3:
            sgj_pr_hr(jsp, "    Defines zones for host managed device:\n");
            u = bp[40 + 0];
            sgj_pr_hr(jsp, "      %s: %u.%u %%\n", llczp, u / 10, u % 10);
            sgj_convert2snake(llczp, b, blen);
            sgj_js_nv_ihex_nex(jsp, jo3p, b, u, true, "unit: 1/10 of a "
                               "percent");
            u = bp[40 + 1];
            sgj_pr_hr(jsp, "      %s: %u.%u %%\n", hlczp, u / 10, u % 10);
            sgj_convert2snake(hlczp, b, blen);
            sgj_js_nv_ihex_nex(jsp, jo3p, b, u, true, "unit: 1/10 of a "
                               "percent");
            u = bp[40 + 3] & 0x7;
            sgj_haj_vistr(jsp, jo3p, 6, "Designed zone alignment method",
                           SGJ_SEP_COLON_1_SPACE, u, true,
                           get_zone_align_method(u, d, dlen));
            ul = sg_get_unaligned_be64(bp + 40 + 4);
            sgj_haj_vi_nex(jsp, jo3p, 6, "Designed zone starting LBA "
                           "granularity", SGJ_SEP_COLON_1_SPACE, ul, true,
                           "unit: LB");
            u = sg_get_unaligned_be32(bp + 40 + 12);
            sgj_haj_vistr(jsp, jo3p, 6, "Logical blocks per zone",
                          SGJ_SEP_COLON_1_SPACE, u, true,
                          (0 == u ? rsv_s : NULL));
            break;
        case 4:
            sgj_pr_hr(jsp, "    Defines zones for zone domains and realms "
                      "device:\n");
            snprintf(b, blen, "%s 0", ztzd);
            u = bp[40 + 0];
            sg_get_zone_type_str((u >> 4) & 0xf, dlen, d);
            sgj_haj_vistr(jsp, jo3p, 6, b, SGJ_SEP_COLON_1_SPACE, u, true, d);
            snprintf(b, blen, "%s 1", ztzd);
            sg_get_zone_type_str(u & 0xf, dlen, d);
            sgj_haj_vistr(jsp, jo3p, 6, b, SGJ_SEP_COLON_1_SPACE, u, true, d);

            snprintf(b, blen, "%s 2", ztzd);
            u = bp[40 + 1];
            sg_get_zone_type_str((u >> 4) & 0xf, dlen, d);
            sgj_haj_vistr(jsp, jo3p, 6, b, SGJ_SEP_COLON_1_SPACE, u, true, d);
            snprintf(b, blen, "%s 3", ztzd);
            sg_get_zone_type_str(u & 0xf, dlen, d);
            sgj_haj_vistr(jsp, jo3p, 6, b, SGJ_SEP_COLON_1_SPACE, u, true, d);
            u = bp[40 + 3] & 0x7;
            sgj_haj_vistr(jsp, jo3p, 6, "Designed zone alignment method",
                          SGJ_SEP_COLON_1_SPACE, u, true,
                          get_zone_align_method(u, d, dlen));
            ul = sg_get_unaligned_be64(bp + 40 + 4);
            sgj_haj_vi_nex(jsp, jo3p, 6, "Designed zone starting LBA "
                           "granularity", SGJ_SEP_COLON_1_SPACE, ul, true,
                           "unit: LB");
            u = sg_get_unaligned_be32(bp + 40 + 12);
            sgj_haj_vistr(jsp, jo3p, 6, "Logical blocks per zone",
                          SGJ_SEP_COLON_1_SPACE, u, true,
                          (0 == u ? rsv_s : NULL));
            ul = sg_get_unaligned_be64(bp + 40 + 16);
            sgj_haj_vi_nex(jsp, jo3p, 6, "Designed zone maximum address",
                           SGJ_SEP_COLON_1_SPACE, ul, true, "unit: LBA");
            break;
        default:
            sgj_pr_hr(jsp, "    No schema type specific information\n");
            break;
        }
        sgj_js_nv_o(jsp, jap, NULL /* name */, jo2p);
    }
}

/* VPD_CON_POS_RANGE  0xb9 (added sbc5r01) */
static void
decode_con_pos_range_vpd(const uint8_t * buff, int len,
                         struct sdparm_opt_coll * op, sgj_opaque_p jap)
{
    int k;
    uint32_t u;
    sgj_state * jsp = &op->json_st;
    const uint8_t * bp;
    sgj_opaque_p jo2p;

#if 0
    if (op->do_hex > 0) {
        if (op->do_hex > 2)
            named_hhh_output(cpr_vpdp, buff, len, op);
        else
            hex2stdout(buff, len, no_ascii_4hex(op));
        return;
    }
#endif
    if (len < 64) {
        pr2serr("%s length too short=%d\n", cpr_vpdp, len);
        return;
    }
    len -= 64;
    bp = buff + 64;
    for (k = 0; k < len; k += 32, bp += 32) {
        jo2p = sgj_new_unattached_object_r(jsp);
        sgj_haj_vi(jsp, jo2p, 2, "LBA range number",
                   SGJ_SEP_COLON_1_SPACE, bp[0], true);
        u = bp[1];
        sgj_haj_vistr(jsp, jo2p, 4, "Number of storage elements",
                      SGJ_SEP_COLON_1_SPACE, u, true, (0 == u ? nr_s : NULL));
        sgj_haj_vi(jsp, jo2p, 4, "Starting LBA", SGJ_SEP_COLON_1_SPACE,
                   sg_get_unaligned_be64(bp + 8), true);
        sgj_haj_vi(jsp, jo2p, 4, "Number of LBAs", SGJ_SEP_COLON_1_SPACE,
                   sg_get_unaligned_be64(bp + 16), true);
        sgj_js_nv_o(jsp, jap, NULL /* name */, jo2p);
    }
}

/* VPD_ZBC_DEV_CHARS 0xb6  ["zdbch"]  sbc or zbc [zbc2r04] */
static void
decode_zbdch_vpd(const uint8_t * buff, int len,
                 struct sdparm_opt_coll * op, sgj_opaque_p jop)
{
    uint32_t u, pdt;
    sgj_state * jsp = &op->json_st;
    char b[128];
    static const int blen = sizeof(b);

#if 0
    if (op->do_hex > 0) {
        if (op->do_hex > 2)
            named_hhh_output(zbdc_vpdp, buff, len, op);
        else
            hex2stdout(buff, len, no_ascii_4hex(op));
        return;
    }
#endif
    if (len < 64) {
        pr2serr("%s length too short=%d\n", zbdc_vpdp, len);
        return;
    }
    pdt = PDT_MASK & buff[0];
    sgj_pr_hr(jsp, "  Peripheral device type: %s\n",
              sg_get_pdt_str(pdt, blen, b));

    sgj_pr_hr(jsp, "  Zoned block device extension: ");
    u = (buff[4] >> 4) & 0xf;
    switch (u) {
    case 0:
        if (PDT_ZBC == (PDT_MASK & buff[0]))
            strcpy(b, "host managed zoned block device");
        else
            strcpy(b, nr_s);
        break;
    case 1:
        strcpy(b, "host aware zoned block device model");
        break;
    case 2:
        strcpy(b, "Domains and realms zoned block device model");
        break;
    default:
        strcpy(b, rsv_s);
        break;
    }
    sgj_haj_vistr(jsp, jop, 2, "Zoned block device extension",
                  SGJ_SEP_COLON_1_SPACE, u, true, b);
    sgj_haj_vi_nex(jsp, jop, 2, "AAORB", SGJ_SEP_COLON_1_SPACE,
                   !!(buff[4] & 0x2), false,
                   "Activation Aligned On Realm Boundaries");
    sgj_haj_vi_nex(jsp, jop, 2, "URSWRZ", SGJ_SEP_COLON_1_SPACE,
                   !!(buff[4] & 0x1), false,
                   "Unrestricted Read in Sequential Write Required Zone");
    u = sg_get_unaligned_be32(buff + 8);
    sgj_haj_vistr(jsp, jop, 2, "Optimal number of open sequential write "
                  "preferred zones", SGJ_SEP_COLON_1_SPACE, u, true,
                  (SG_LIB_UNBOUNDED_32BIT == u) ? nr_s : NULL);
    u = sg_get_unaligned_be32(buff + 12);
    sgj_haj_vistr(jsp, jop, 2, "Optimal number of non-sequentially "
                  "written sequential write preferred zones",
                  SGJ_SEP_COLON_1_SPACE, u, true,
                  (SG_LIB_UNBOUNDED_32BIT == u) ? nr_s : NULL);
    u = sg_get_unaligned_be32(buff + 16);
    sgj_haj_vistr(jsp, jop, 2, "Maximum number of open sequential write "
                  "required zones", SGJ_SEP_COLON_1_SPACE, u, true,
                  (SG_LIB_UNBOUNDED_32BIT == u) ? nl_s : NULL);
    u = buff[23] & 0xf;
    switch (u) {
    case 0:
        strcpy(b, nr_s);
        break;
    case 1:
        strcpy(b, "Zoned starting LBAs aligned using constant zone lengths");
        break;
    case 0x8:
        strcpy(b, "Zoned starting LBAs potentially non-constant (as "
                 "reported by REPORT ZONES)");
        break;
    default:
        strcpy(b, rsv_s);
        break;
    }
    sgj_haj_vistr(jsp, jop, 2, "Zoned alignment method",
                  SGJ_SEP_COLON_1_SPACE, u, true, b);
    sgj_haj_vi(jsp, jop, 2, "Zone starting LBA granularity",
               SGJ_SEP_COLON_1_SPACE, sg_get_unaligned_be64(buff + 24), true);
}

/* VPD_BLOCK_LIMITS_EXT  0xb7 ["ble"] SBC */
static void
decode_block_limits_ext_vpd(const uint8_t * buff, int len,
                            struct sdparm_opt_coll * op, sgj_opaque_p jop)
{
    uint32_t u;
    sgj_state * jsp = &op->json_st;

#if 0
    if (op->do_hex > 0) {
        if (op->do_hex > 2)
            named_hhh_output(ble_vpdp, buff, len, op);
        else
            hex2stdout(buff, len, no_ascii_4hex(op));
        return;
    }
#endif
    if (len < 12) {
        pr2serr("%s length too short=%d\n", ble_vpdp, len);
        return;
    }
    sgj_haj_vi_nex(jsp, jop, 2, "RSCS", SGJ_SEP_COLON_1_SPACE,
                   !!(buff[5] & 0x1), false,
                   "Reduced Stream Control Supported");
    u = sg_get_unaligned_be16(buff + 6);
    sgj_haj_vistr(jsp, jop, 2, "Maximum number of streams",
                  SGJ_SEP_COLON_1_SPACE, u, true,
                  (0 == u) ? "Stream control not supported" : NULL);
    u = sg_get_unaligned_be16(buff + 8);
    sgj_haj_vi_nex(jsp, jop, 2, "Optimal stream write size",
                   SGJ_SEP_COLON_1_SPACE, u, true, "unit: LB");
    u = sg_get_unaligned_be32(buff + 10);
    sgj_haj_vi_nex(jsp, jop, 2, "Stream granularity size",
                   SGJ_SEP_COLON_1_SPACE, u, true,
                   "unit: number of optimal stream write size blocks");
    if (len < 28)
        return;
    u = sg_get_unaligned_be32(buff + 16);
    sgj_haj_vistr_nex(jsp, jop, 2, "Maximum scattered LBA range transfer "
                      "length", SGJ_SEP_COLON_1_SPACE, u, true,
                      (0 == u ? nlr_s : NULL),
                      "unit: LB (in a single LBA range descriptor)");
    u = sg_get_unaligned_be16(buff + 22);
    sgj_haj_vistr(jsp, jop, 2, "Maximum scattered LBA range descriptor "
                  "count", SGJ_SEP_COLON_1_SPACE, u, true,
                  (0 == u ? nlr_s : NULL));
    u = sg_get_unaligned_be32(buff + 24);
    sgj_haj_vistr_nex(jsp, jop, 2, "Maximum scattered transfer length",
                      SGJ_SEP_COLON_1_SPACE, u, true,
                      (0 == u ? nlr_s : NULL),
                      "unit: LB (per single Write Scattered command)");
}

/* VPD_CAP_PROD_ID  0xba ["cap"] (added sbc5r04) */
static void
decode_cap_prod_id_vpd(const uint8_t * buff, int len,
                       struct sdparm_opt_coll * op, sgj_opaque_p jap)
{
    int k, n;
    uint64_t ull;
    const uint8_t * bp;
    sgj_state * jsp = &op->json_st;
    sgj_opaque_p jo2p = NULL;
    char b[64];
    static const int blen = sizeof(b);

    if (len < 4) {
        pr2serr("%s length too short=%d\n", cap_vpdp, len);
        return;
    }
    len -= 4;
    bp = buff + 4;
    for (k = 0; k < len; k += 48, bp += 48) {
        if (jsp->pr_as_json)
            jo2p = sgj_new_unattached_object_r(jsp);
        ull = sg_get_unaligned_be64(bp);
        sgj_haj_vi(jsp, jo2p, 2, "Allowed number of logical blocks",
                   SGJ_SEP_COLON_1_SPACE, ull, true);
        /* should be left justified ASCII with spaces to the right */
        n = sg_first_non_printable(bp + 8, 16);
        if (n > 0)
            snprintf(b, blen, "%.*s", n, (const char *)bp + 8);
        else
            strcpy(b, "<empty>");
        sgj_haj_vs(jsp, jo2p, 2, "Product identification",
                   SGJ_SEP_COLON_1_SPACE, b);
        sgj_js_nv_o(jsp, jap, NULL /* name */, jo2p);
    }
}

#define DECODE_ALL_VPDS_BUFLEN 256

/* Called from end of 'case VPD_SUPPORTED_VPDS:' in sdp_process_vpd_page().
 * Must take care not to call that function to decode the VPD_SUPPORTED_VPDS
 * page again. That will cause an infinite loop!
 * Called when '--inquiry --all --all' or '-iaa' used on command line. */
static int
decode_all_vpds(uint8_t * b, int len, int sg_fd, int req_pdt, bool protect,
                uint8_t * alt_buf, int off, struct sdparm_opt_coll * op,
                sgj_opaque_p jop)
{
    uint16_t u;
    int k, ret, moff;
    uint8_t bb[DECODE_ALL_VPDS_BUFLEN];

    len -= 4;
    if (len > DECODE_ALL_VPDS_BUFLEN)
        len = DECODE_ALL_VPDS_BUFLEN;
    memcpy(bb, b + 4, len);

    for (k = 0, moff = off; k < len; ++k) {
        if (VPD_SUPPORTED_VPDS == bb[k])
            continue;   /* this avoids infinite loop */
        if (alt_buf) {
            u = sg_get_unaligned_be16(alt_buf + moff + 2);
            if (u > 16 * 1024)
                return SG_LIB_LOGIC_ERROR;
            moff += u + 4;
        }
        sgj_pr_hr(&op->json_st, "\n");
        ret = sdp_process_vpd_page(sg_fd, bb[k], 0, req_pdt, protect,
                                   NULL, alt_buf, moff, op, jop);
        if (ret)
            return ret;
    }
    return 0;
}

static const char *
pqual_str(int pqual)
{
    switch (pqual) {
    case 0:
        return "LU accessible";
    case 1:
        return "LU temporarily unavailable";
    case 3:
        return "LU not accessible via this port";
    default:
        return "value reserved by T10";
    }
}

static const char *
hot_pluggable_str(int hp)
{
    switch (hp) {
    case 0:
        return "No information";
    case 1:
        return "target device designed to be removed from SCSI domain";
    case 2:
        return "target device not designed to be removed from SCSI domain";
    default:
        return "value reserved by T10";
    }
}

static const char *
tpgs_str(int tpgs)
{
    switch (tpgs) {
    case 1:
        return "only implicit asymmetric logical unit access";
    case 2:
        return "only explicit asymmetric logical unit access";
    case 3:
        return "both explicit and implicit asymmetric logical unit access";
    case 0:
    default:
        return ns_s;
    }
}

static sgj_opaque_p
std_inq_decode_js(const uint8_t * b, int len, struct sdparm_opt_coll * op,
                  sgj_opaque_p jop)
{
    int tpgs;
    int pqual = (b[0] & 0xe0) >> 5;
    int pdt = b[0] & PDT_MASK;
    int hp = (b[1] >> 4) & 0x3;
    int ver = b[2];
    sgj_state * jsp = &op->json_st;
    sgj_opaque_p jo2p = NULL;
    char c[256];
    char d[64];
    static const int clen = sizeof(c);
    static const int dlen = sizeof(d);

    jo2p = sgj_named_subobject_r(jsp, jop, "standard_inquiry_data_format");
    sgj_js_nv_ihexstr(jsp, jo2p, "peripheral_qualifier", pqual, NULL,
                      pqual_str(pqual));
    sgj_js_nv_ihexstr(jsp, jo2p, "peripheral_device_type", pdt, NULL,
                      sg_get_pdt_str(pdt, clen, c));
    sgj_js_nv_ihex_nex(jsp, jo2p, "rmb", !!(b[1] & 0x80), false,
                       "Removable Medium Bit");
    sgj_js_nv_ihex_nex(jsp, jo2p, "lu_cong", !!(b[1] & 0x40), false,
                       "Logical Unit Conglomerate");
    sgj_js_nv_ihexstr(jsp, jo2p, "hot_pluggable", hp, NULL,
                      hot_pluggable_str(hp));
    snprintf(c, clen, "%s", (ver > 0xf) ? "old or reserved version code" :
                  sg_get_scsi_ansi_version_str(ver, dlen, d));
    sgj_js_nv_ihexstr(jsp, jo2p, "version", ver, NULL, c);
    sgj_js_nv_ihex_nex(jsp, jo2p, "aerc", !!(b[3] & 0x80), false,
                       "Asynchronous Event Reporting Capability (obsolete "
                       "SPC-3)");
    sgj_js_nv_ihex_nex(jsp, jo2p, "trmtsk", !!(b[3] & 0x40), false,
                       "Terminate Task (obsolete SPC-2)");
    sgj_js_nv_ihex_nex(jsp, jo2p, "normaca", !!(b[3] & 0x20), false,
                       "Normal ACA (Auto Contingent Allegiance)");
    sgj_js_nv_ihex_nex(jsp, jo2p, "hisup", !!(b[3] & 0x10), false,
                       "Hierarchial Support");
    sgj_js_nv_ihex(jsp, jo2p, "response_data_format", b[3] & 0xf);
    sgj_js_nv_ihex_nex(jsp, jo2p, "sccs", !!(b[5] & 0x80), false,
                       "SCC (SCSI Storage Commands) Supported");
    sgj_js_nv_ihex_nex(jsp, jo2p, "acc", !!(b[5] & 0x40), false,
                       "Access Commands Coordinator (obsolete SPC-5)");
    tpgs = (b[5] >> 4) & 0x3;
    sgj_js_nv_ihexstr_nex(jsp, jo2p, "tpgs", tpgs, false, NULL,
                          tpgs_str(tpgs), "Target Port Group Support");
    sgj_js_nv_ihex_nex(jsp, jo2p, "3pc", !!(b[5] & 0x8), false,
                       "Third Party Copy");
    sgj_js_nv_ihex(jsp, jo2p, "protect", !!(b[5] & 0x1));
    /* Skip SPI specific flags which have been obsolete for a while) */
    sgj_js_nv_ihex_nex(jsp, jo2p, "bque", !!(b[6] & 0x80), false,
                       "Basic task management model (obsolete SPC-4)");
    sgj_js_nv_ihex_nex(jsp, jo2p, "encserv", !!(b[6] & 0x40), false,
                       "Enclousure Services supported");
    sgj_js_nv_ihex_nex(jsp, jo2p, "multip", !!(b[6] & 0x10), false,
                       "Multiple SCSI port");
    sgj_js_nv_ihex_nex(jsp, jo2p, "mchngr", !!(b[6] & 0x8), false,
                       "Medium changer (obsolete SPC-4)");
    sgj_js_nv_ihex_nex(jsp, jo2p, "reladr", !!(b[7] & 0x80), false,
                       "Relative Addressing (obsolete in SPC-4)");
    sgj_js_nv_ihex_nex(jsp, jo2p, "linked", !!(b[7] & 0x8), false,
                       "Linked Commands (obsolete in SPC-4)");
    sgj_js_nv_ihex_nex(jsp, jo2p, "cmdque", !!(b[7] & 0x2), false,
                       "Command Management Model (command queuing)");
    if (len < 16)
        return jo2p;
    snprintf(c, clen, "%.8s", b + 8);
    sgj_js_nv_s(jsp, jo2p, t10_vendor_id_sn, c);
    if (len < 32)
        return jo2p;
    snprintf(c, clen, "%.16s", b + 16);
    sgj_js_nv_s(jsp, jo2p, product_id_sn, c);
    if (len < 36)
        return jo2p;
    snprintf(c, clen, "%.4s", b + 32);
    sgj_js_nv_s(jsp, jo2p, product_rev_lev_sn, c);
    return jo2p;
}

static void
decode_std_inq(int blen, uint8_t * b, struct sdparm_opt_coll * op,
               sgj_opaque_p jop)
{
    uint8_t ver;
    int pqual, pdt, hp, j, n;
    sgj_state * jsp = &op->json_st;
    const char * cp;
    char c[256];
    static const int clen = sizeof(c);
    static const char * np = "Standard INQUIRY data format:";

    if (blen < 4) {
        pr2serr("%s: len [%d] too short\n", __func__, blen);
        return;
    }
    pqual = (b[0] & 0xe0) >> 5;
    pdt = b[0] & PDT_MASK;
    hp = (b[1] >> 4) & 0x3;
    ver = b[2];
    sgj_pr_hr(jsp, "%s", np);
    if (0 == pqual)
        sgj_pr_hr(jsp, "\n");
    else {
        cp = pqual_str(pqual);

        if (pqual < 3)
            sgj_pr_hr(jsp, " [PQ indicates %s]\n", cp);
        else
            sgj_pr_hr(jsp, " [PQ indicates %s [0x%x] ]\n", cp, pqual);
    }
    sgj_pr_hr(jsp, "  PQual=%d  PDT=%d  RMB=%d  LU_CONG=%d  hot_pluggable="
              "%d  version=0x%02x  [%s]\n", pqual, pdt, !!(b[1] & 0x80),
              !!(b[1] & 0x40), hp, ver,
              sg_get_scsi_ansi_version_str(ver, clen, c));
    sgj_pr_hr(jsp, "  [AERC=%d]  [TrmTsk=%d]  NormACA=%d  HiSUP=%d "
           " Resp_data_format=%d\n",
           !!(b[3] & 0x80), !!(b[3] & 0x40), !!(b[3] & 0x20),
           !!(b[3] & 0x10), b[3] & 0x0f);
    if (blen < 5)
        goto skip1;
    j = b[4] + 5;
    if (op->verbose > 2)
        pr2serr(">> requested %d bytes, %d bytes available\n", blen, j);
    sgj_pr_hr(jsp, "  SCCS=%d  ACC=%d  TPGS=%d  3PC=%d  Protect=%d  "
              "[BQue=%d]\n", !!(b[5] & 0x80), !!(b[5] & 0x40),
              ((b[5] & 0x30) >> 4), !!(b[5] & 0x08), !!(b[5] & 0x01),
              !!(b[6] & 0x80));
    n = sg_scnpr(c, clen, "EncServ=%d  ", !!(b[6] & 0x40));
    if (b[6] & 0x10)
        n += sg_scn3pr(c, clen, n, "MultiP=1 (VS=%d)  ", !!(b[6] & 0x20));
    else
        n += sg_scn3pr(c, clen, n, "MultiP=0  ");
    sg_scn3pr(c, clen, n, "[MChngr=%d]  [ACKREQQ=%d]  Addr16=%d",
              !!(b[6] & 0x08), !!(b[6] & 0x04), !!(b[6] & 0x01));
    sgj_pr_hr(jsp, "  %s\n", c);
    sgj_pr_hr(jsp, "  [RelAdr=%d]  WBus16=%d  Sync=%d  [Linked=%d]  "
              "[TranDis=%d]  CmdQue=%d\n", !!(b[7] & 0x80), !!(b[7] & 0x20),
              !!(b[7] & 0x10), !!(b[7] & 0x08), !!(b[7] & 0x04),
              !!(b[7] & 0x02));
    if (blen < 36)
        goto skip1;
    sgj_pr_hr(jsp, "  %s: %.8s\n", t10_vendor_id_hr, b + 8);
    sgj_pr_hr(jsp, "  %s: %.16s\n", product_id_hr, b + 16);
    sgj_pr_hr(jsp, "  %s: %.4s\n", product_rev_lev_hr, b + 32);
skip1:
    if (! jsp->pr_as_json || (blen < 8))
        return;
    std_inq_decode_js(b, blen, op, jop);
}

/* Hack: use vpd page=-1 to indicate want standard INQUIRY response */
static int
fetch_decode_std_inq(int sg_fd, struct sdparm_opt_coll * op, sgj_opaque_p jop)
{
    int res, verb, sz;
    int resid = 0;
    int b_sz = DEF_INQ_RESP_LEN;
    uint8_t * b = NULL;
    uint8_t * free_b = NULL;

    verb = (op->verbose > 0) ? op->verbose - 1 : 0;
    b = sg_memalign(b_sz, 0, &free_b, false);
    if (NULL == b) {
        pr2serr("%s: unalign to allocate ram\n", __func__);
        return sg_convert_errno(ENOMEM);
    }
    sz = op->do_long ? b_sz : 36;
    res = sg_ll_inquiry_v2(sg_fd, false, 0, b, sz, 0, &resid, false, verb);
    if (res) {
        pr2serr("INQUIRY fetching standard response failed\n");
        goto fini;
    }
    if (resid > 0) {
        sz -= resid;
        if (sz < 5) {
            pr2serr("%s: after resid (%d) response size is too short (%d)\n",
                    __func__, resid, sz);
            res = SG_LIB_WILD_RESID;
            goto fini;
        }
    }
    decode_std_inq(sz, b, op, jop);
    res = 0;
fini:
    if (free_b)
        free(free_b);
    return res;
}

/* If ihbp is NULL then need to send SCSI INQUIRY command to device referred
 * to by sg_fd; then process the response received. If ihbp is non-NULL then
 * sg_fd is ignored and the buffer pointed to by ihbp (with length no greater
 * than op->inhex_len) is assumed to be the response to a SCSI INQUIRY
 * command. Alternatively when alt_buf is non-NULL, is carries 1 or more VPD
 * page response, the one to decode starts at offset 'off'. If both ihbp and
 * alt_buf are NULL then expect sg_fd >= 0, an open file descriptor to which
 * the a SCSI INQUIRY command will be sent to fetch one or more VPD pages.
 * Returns 0 if successful, else error. spn changes what is output, currently
 * it only changes the output of the Device Identification VPD page. */
int
sdp_process_vpd_page(int sg_fd, int pn, int spn, int req_pdt, bool protect,
                     const uint8_t * ihbp, uint8_t * alt_buf, int off,
                     struct sdparm_opt_coll * op, sgj_opaque_p jop)
{
    bool sbc = false;
    bool ssc = false;
    bool as_json, cap;
    int len, verb, dev_pdt, pdt, sz, hex_format;
    int dhex = op->do_hex;
    int resid = 0;
    int ret = 0;
    const char * nm;
    sgj_state * jsp = &op->json_st;
    sgj_opaque_p jo2p = NULL;
    sgj_opaque_p jap = NULL;
    const struct sdparm_vpd_page_t * vpp;
    uint8_t * b = NULL;
    uint8_t * free_b = NULL;
    const int b_sz = 2 * sg_get_page_size();
    char c[128];
    static const int clen = sizeof(c);

    verb = (op->verbose > 0) ? op->verbose - 1 : 0;
    as_json = jsp->pr_as_json;
    if (verb > 3)
        pr2serr("%s: sg_fd=%d, pn=0x%x, spn=%d, ihbp is %sgiven, alt_buff is "
                "%sgiven, ihb_len=%d, off=%d\n", __func__, sg_fd,
                (unsigned int)pn, spn, ((!! ihbp) ? "" : "not "),
                ((!! alt_buf) ? "" : "not "), op->inhex_len, off);
    hex_format = (dhex > 2) ? -1 : no_ascii_4hex(op);
    sz = b_sz;
    if (NULL == alt_buf) {
        b = sg_memalign(b_sz, 0 /* page align */, &free_b, false);
        if (NULL == b) {
            pr2serr("Unable to allocate %d bytes on the heap\n", b_sz);
            ret = sg_convert_errno(ENOMEM);
            goto fini;
        }
    }
    if (sg_fd < 0) {
        if ((!! alt_buf) == (!! ihbp)) {
            pr2serr("%s: logic error, if no sg_fd need either ihbp or "
                    "alt_buf, not both\n", __func__);
            ret = sg_convert_errno(EINVAL);
            goto fini;
        } else if (alt_buf) {
            b = alt_buf + off;
            sz -= off;
            if (pn < 0)
                pn = b[1];
        } else {        /* so ihbp must be non-NULL */
            if (op->inhex_len < sz)
                sz = op->inhex_len;
            memcpy(b, ihbp, sz);
            if (pn < 0)
                pn = b[1];
        }
        if (pn < 0) {
            if (VPD_NOT_STD_INQ == pn) {
                decode_std_inq(sz, b, op, jop);
                goto fini;
            } else if ((VPD_SUPPORTED_VPDS == b[1]) || op->do_all)
                pn = VPD_SUPPORTED_VPDS;
            else if (VPD_DEVICE_ID == b[1])
                pn = VPD_DEVICE_ID;
            else
                pr2serr("please give --page=<vpd_page> option\n");
        } else if (op->do_all > 0) {
            if (verb > 2)
                pr2serr("%s: do_all=%d, skip pn!=b[1] loop\n", __func__,
                        op->do_all);
        } else if (pn != b[1]) {
            /* told to look for specific page from --inhex input but we are
             * not there at the moment. So scan whole --inhex buffer */
            int k, l_pn;
            int bump = 4;
            int prev_l_pn = -1;
            uint8_t * rp;

            for (k = 0; k < sz; k += bump) {
                rp = b + k;
                l_pn = rp[1];
                bump = sg_get_unaligned_be16(rp + 2) + 4;
                if ((k + bump) > sz) {
                    pr2serr("%s: page 0x%x size (%d) exceeds buffer\n",
                            __func__, l_pn, bump);
                    bump = sz - k;
                }
                if (l_pn <= prev_l_pn) {
                    pr2serr("%s: prev_pn=0x%x, this pn=0x%x, not ascending so "
                            "exit\n", __func__, prev_l_pn, l_pn);
                    ret = SG_LIB_CAT_MALFORMED;
                    goto fini;
                }
                if (pn != l_pn) {
                    prev_l_pn = l_pn;
                    continue;
                }
#if 0
                if (pn > max_pn) {
                    if (op->verbose > 2)
                        pr2serr("%s: skipping as this pn=0x%x exceeds "
                                "max_pn=0x%x\n", __func__, pn, max_pn);
                    continue;
                }
#endif
                b = rp;
                break;
            }
            if (k >= sz) {
                pr2serr("VPD page 0x%x not found in %s\n", pn,
                        op->inhex_fn ? op->inhex_fn : "");
                ret = SG_LIB_CAT_OTHER;
                goto fini;
            } else
                sz = bump;
        }
    } else {             /* so (sg_fd >= 0) , need to read from device */
        if (pn < 0) {
            if (VPD_NOT_STD_INQ == pn) {
                ret = fetch_decode_std_inq(sg_fd, op, jop);
                goto fini;
            } else if (op->do_all)
                pn = VPD_SUPPORTED_VPDS;  /* if '--all' list supported vpds */
            else
                pn = VPD_DEVICE_ID;  /* default to device id page */
        }
        sz = (VPD_ATA_INFO == pn) ? VPD_ATA_INFO_RESP_LEN : DEF_INQ_RESP_LEN;
        if (NULL == b) {
            pr2serr("Logic error, b should not be NULL\n");
            ret = SG_LIB_CAT_MALFORMED;
            goto fini;
        }
try_larger:
        ret = sg_ll_inquiry_v2(sg_fd, true, pn, b, sz, 0, &resid, false,
                               verb);
        if (ret) {
            if (! op->examine)
                pr2serr("INQUIRY fetching VPD page=0x%x failed\n", pn);
            goto fini;
        }
        len = ((sz - resid) >= 4) ? (sg_get_unaligned_be16(b + 2) + 4) : 0;
        if (len > sz) {
            if (sz < VPD_LARGE_RESP_LEN) {
                sz = VPD_LARGE_RESP_LEN;
                    goto try_larger;
            }
            pr2serr("%s: resid=%d implies response too short (%d)\n",
                    __func__, resid, len);
            ret = SG_LIB_WILD_RESID;
            goto fini;
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
        len = sg_get_unaligned_be16(b + 2) + 4;
        if (dhex < 3)
            sgj_pr_hr(jsp, "%s:\n", svp_vpdp);
        if (dhex > 0) {
            if (dhex > 2)
                named_hhh_output(svp_vpdp, b, len, op);
            else
                hex2stdout(b, len, hex_format);
            ret = 0;
            if (op->do_all > 1)
                ret = decode_all_vpds(b, len, sg_fd, req_pdt, protect,
                                      (uint8_t *)ihbp, 0, op, jop);
            goto fini;
        }
        if (len > 0) {
            if (as_json) {
                jo2p = sg_vpd_js_hdr(jsp, jop, di_vpdp, b);
                jap = sgj_named_subarray_r(jsp, jo2p,
                                           "supported_vpd_page_list");
            }
            decode_supported_vpd(b, len, op, jap);
#if 0
            for (k = 0; k < len; ++k) {
                vpp = sdp_get_vpd_detail(b[4 + k], -1, pdt);
                if (vpp) {
                    if (op->do_long)
                        sgj_pr_hr(jsp, "  [0x%02x] %s [%s]\n", b[4 + k],
                                  vpp->name, vpp->acron);
                    else
                        sgj_pr_hr(jsp, "  %s [%s]\n", vpp->name, vpp->acron);
                } else
                    sgj_pr_hr(jsp, "  0x%x\n", b[4 + k]);
            }
#endif
            /* Take care not to decode this VPD page [0x0] again! */
            if (op->do_all > 1) {
                ret = decode_all_vpds(b, len, sg_fd, req_pdt, protect,
                                      (uint8_t *)ihbp, 0, op, jop);
                if (ret)
                    goto fini;
            }
        } else
            sgj_pr_hr(jsp, "  <empty>\n");
        break;
    case VPD_ATA_INFO:          /* 0x89 */
        if (b[1] != pn)
            goto dumb_inq;
        len = sg_get_unaligned_be16(b + 2) + 4;       /* spc4r25 */
        if (len > sz) {
            pr2serr("Response to %s truncated\n", ai_vpdp);
            len = sz;
        }
        if (dhex < 3) {
            if (op->do_long)
                sgj_pr_hr(jsp, "%s [0x89]:\n", ai_vpdp);
            else
                sgj_pr_hr(jsp, "%s:\n", ai_vpdp);
        }
        if (3 == dhex) {   /* output suitable for "hdparm --Istdin" */
            dWordHex((const unsigned short *)(b + 60), 256, -2,
                     sg_is_big_endian());
            goto fini;
        }
        if ((dhex > 0) && (2 != dhex)) {
            if (1 == dhex)
                hex2stdout(b, len, 0);
            else if (4 == dhex)
                hex2stdout(b, len, -1);
            else if (dhex > 4)
                named_hhh_output(svp_vpdp, b, len, op);
            goto fini;
        }
        if (as_json)
            jo2p = sg_vpd_js_hdr(jsp, jop, ai_vpdp, b);
        decode_ata_info_vpd(b, len, op, jo2p);
        break;
    case VPD_DEVICE_ID:         /* 0x83 */
        if (b[1] != pn)
            goto dumb_inq;
        len = sg_get_unaligned_be16(b + 2) + 4;
        if (len > sz) {
            pr2serr("Response to %s truncated\n", di_vpdp);
            len = sz;
        }
        if (dhex < 3) {
            if (op->do_long)
                sgj_pr_hr(jsp, "%s [0x83]:\n", di_vpdp);
            else if (! op->do_quiet)
                sgj_pr_hr(jsp, "%s:\n", di_vpdp);
        }
        if (dhex > 0) {
            if (dhex > 2)
                named_hhh_output(di_vpdp, b, len, op);
            else
                hex2stdout(b, len, hex_format);
            goto fini;
        }
        if (as_json) {
            jo2p = sg_vpd_js_hdr(jsp, jop, di_vpdp, b);
            jap = sgj_named_subarray_r(jsp, jo2p,
                                       "designation_descriptor_list");
        }
        ret = 0;
        if ((0 == spn) || (VPD_DI_SEL_LU & spn))
            ret = decode_dev_ids(sg_get_desig_assoc_str(VPD_ASSOC_LU), 0,
                                 b + 4, len - 4, VPD_ASSOC_LU, -1, -1, op,
                                 jap);
        if ((0 == spn) || (VPD_DI_SEL_TPORT & spn))
            ret = decode_dev_ids(sg_get_desig_assoc_str(VPD_ASSOC_TPORT), 0,
                                 b + 4, len - 4, VPD_ASSOC_TPORT, -1, -1, op,
                                 jap);
        if ((0 == spn) || (VPD_DI_SEL_TARGET & spn))
            ret = decode_dev_ids(sg_get_desig_assoc_str(VPD_ASSOC_TDEVICE), 0,
                                 b + 4, len - 4, VPD_ASSOC_TDEVICE, -1, -1,
                                 op, jap);
        if (VPD_DI_SEL_AS_IS & spn)
            ret = decode_dev_ids(NULL, 0, b + 4, len - 4, -1, -1, -1, op,
                                 jap);
        if (ret)
            goto fini;
        break;
    case VPD_EXT_INQ:           /* 0x86 */
        if (b[1] != pn)
            goto dumb_inq;
        len = sg_get_unaligned_be16(b + 2) + 4;
        if (len > sz) {
            pr2serr("Response to %s truncated\n", eid_vpdp);
            len = sz;
        }
        if (dhex < 3) {
            if (op->do_long)
                sgj_pr_hr(jsp, "%s [0x86]:\n", eid_vpdp);
            else if (! op->do_quiet)
                sgj_pr_hr(jsp, "%s:\n", eid_vpdp);
        }
        if (dhex > 0) {
            if (dhex > 2)
                named_hhh_output(eid_vpdp, b, len, op);
            else
                hex2stdout(b, len, hex_format);
            goto fini;
        }
        if (as_json)
            jo2p = sg_vpd_js_hdr(jsp, jop, eid_vpdp, b);
        decode_ext_inq_vpd(b, len, protect, op, jo2p);
        break;
    case VPD_MAN_NET_ADDR:              /* 0x85 */
        if (b[1] != pn)
            goto dumb_inq;
        len = sg_get_unaligned_be16(b + 2) + 4;
        if (len > sz) {
            pr2serr("Response to %s truncated\n", mna_vpdp);
            len = sz;
        }
        if (dhex < 3) {
            if (op->do_long)
                sgj_pr_hr(jsp, "%s [0x85]:\n", mna_vpdp);
            else
                sgj_pr_hr(jsp, "%s:\n", mna_vpdp);
        }
        if (dhex > 0) {
            if (dhex > 2)
                named_hhh_output(mna_vpdp, b, len, op);
            else
                hex2stdout(b, len, hex_format);
            goto fini;
        }
        if (as_json) {
            jo2p = sg_vpd_js_hdr(jsp, jop, eid_vpdp, b);
            jap = sgj_named_subarray_r(jsp, jo2p,
                                       "network_services_descriptor_list");
        }
        decode_man_net_vpd(b, len, op, jap);
        break;
    case VPD_MODE_PG_POLICY:            /* 0x87 */
        if (b[1] != pn)
            goto dumb_inq;
        len = sg_get_unaligned_be16(b + 2) + 4;
        if (len > sz) {
            pr2serr("Response to %s truncated\n", mpp_vpdp);
            len = sz;
        }
        if (dhex < 3) {
            if (op->do_long)
                 sgj_pr_hr(jsp, "%s [0x87]:\n", mpp_vpdp);
            else
                 sgj_pr_hr(jsp, "%s:\n", mpp_vpdp);
        }
        if (dhex > 0) {
            if (dhex > 2)
                named_hhh_output(mpp_vpdp, b, len, op);
            else
                hex2stdout(b, len, hex_format);
            goto fini;
        }
        if (as_json) {
            jo2p = sg_vpd_js_hdr(jsp, jop, mpp_vpdp, b);
            jap = sgj_named_subarray_r(jsp, jo2p,
                                      "mode_page_policy_descriptor_list");
        }
        decode_mode_policy_vpd(b, len, op, jap);
        break;
    case VPD_POWER_CONDITION:           /* 0x8a */
        if (b[1] != pn)
            goto dumb_inq;
        len = sg_get_unaligned_be16(b + 2) + 4;
        if (len > sz) {
            pr2serr("Response to %s truncated\n", pc_vpdp);
            len = sz;
        }
        if (dhex < 3) {
            if (op->do_long)
                sgj_pr_hr(jsp, "%s [0x8a]:\n", pc_vpdp);
            else if (! op->do_quiet)
                sgj_pr_hr(jsp, "%s:\n", pc_vpdp);
        }
        if (dhex > 0) {
            if (dhex > 2)
                named_hhh_output(pc_vpdp, b, len, op);
            else
                hex2stdout(b, len, hex_format);
            goto fini;
        }
        if (as_json)
            jo2p = sg_vpd_js_hdr(jsp, jop, pc_vpdp, b);
        decode_power_condition(b, len, op, jo2p);
        break;
    case VPD_DEVICE_CONSTITUENTS:       /* 0x8b */
        if (b[1] != pn)
            goto dumb_inq;
        len = sg_get_unaligned_be16(b + 2) + 4;
        if (len > sz) {
            pr2serr("Response to %s truncated\n", dc_vpdp);
            len = sz;
        }
        if (dhex < 3) {
            if (op->do_long)
                sgj_pr_hr(jsp, "%s [0x8b]:\n", dc_vpdp);
            else if (! op->do_quiet)
                sgj_pr_hr(jsp, "%s:\n", dc_vpdp);
        }
        if (dhex > 0) {
            if (dhex > 2)
                named_hhh_output(dc_vpdp, b, len, op);
            else
                hex2stdout(b, len, hex_format);
            goto fini;
        }
        if (as_json) {
            jo2p = sg_vpd_js_hdr(jsp, jop, dc_vpdp, b);
            jap = sgj_named_subarray_r(jsp, jo2p,
                                       "constituent_descriptor_list");
        }
        ret = decode_dev_constit_vpd(b, len, req_pdt, protect, op, jap);
        if (ret)
            goto fini;
        break;
    case VPD_CFA_PROFILE_INFO:           /* 0x8c */
        if (b[1] != pn)
            goto dumb_inq;
        len = sg_get_unaligned_be16(b + 2) + 4;
        if (len > sz) {
            pr2serr("Response to %s truncated\n", cpi_vpdp);
            len = sz;
        }
        if (dhex < 3) {
            if (op->do_long)
                sgj_pr_hr(jsp, "%s [0x86]:\n", cpi_vpdp);
            else if (! op->do_quiet)
                sgj_pr_hr(jsp, "%s:\n", cpi_vpdp);
        }
        if (dhex > 0) {
            if (dhex > 2)
                named_hhh_output(cpi_vpdp, b, len, op);
            else
                hex2stdout(b, len, hex_format);
            goto fini;
        }
        if (as_json) {
            jo2p = sg_vpd_js_hdr(jsp, jop, cpi_vpdp, b);
            jap = sgj_named_subarray_r(jsp, jo2p,
                                       "cfa_profile_descriptor_list");
        }
        decode_cga_profile_vpd(b, len, op, jap);
        break;
    case VPD_POWER_CONSUMPTION:         /* 0x8d */
        if (b[1] != pn)
            goto dumb_inq;
        len = sg_get_unaligned_be16(b + 2) + 4;
        if (len > sz) {
            pr2serr( "Response to %s truncated\n", psm_vpdp);
            len = sz;
        }
        if (dhex < 3) {
            if (op->do_long)
                sgj_pr_hr(jsp, "%s [0x8d]:\n", psm_vpdp);
            else
                sgj_pr_hr(jsp, "%s:\n", psm_vpdp);
        }
        if (dhex > 0) {
            if (dhex > 2)
                named_hhh_output(psm_vpdp, b, len, op);
            else
                hex2stdout(b, len, hex_format);
            goto fini;
        }
        if (as_json) {
            jo2p = sg_vpd_js_hdr(jsp, jop, psm_vpdp, b);
            jap = sgj_named_subarray_r(jsp, jo2p,
                                       "power_consumption_descriptor_list");
        }
        decode_power_consumption_vpd(b, len, op, jap);
        break;
    case VPD_PROTO_LU:                  /* 0x90 */
        if (b[1] != pn)
            goto dumb_inq;
        len = sg_get_unaligned_be16(b + 2) + 4;
        if (len > sz) {
            pr2serr("Response to %s truncated\n", pslu_vpdp);
            len = sz;
        }
        if (dhex < 3) {
            if (op->do_long)
                sgj_pr_hr(jsp, "%s [0x90]:\n", pslu_vpdp);
            else
                sgj_pr_hr(jsp, "%s:\n", pslu_vpdp);
        }
        if (dhex > 0) {
            if (dhex > 2)
                named_hhh_output(pslu_vpdp, b, len, op);
            else
                hex2stdout(b, len, hex_format);
            goto fini;
        }
        if (as_json) {
            jo2p = sg_vpd_js_hdr(jsp, jop, pslu_vpdp, b);
            jap = sgj_named_subarray_r(jsp, jo2p,
                                  "logical_unit_information_descriptor_list");
        }
        decode_proto_lu_vpd(b, len, op, jap);
        break;
    case VPD_PROTO_PORT:                /* 0x91 */
        if (b[1] != pn)
            goto dumb_inq;
        len = sg_get_unaligned_be16(b + 2) + 4;
        if (len > sz) {
            pr2serr("Response to %s truncated\n", pspo_vpdp);
            len = sz;
        }
        if (dhex < 3) {
            if (op->do_long)
                sgj_pr_hr(jsp, "%s [0x91]:\n", pspo_vpdp);
            else
                sgj_pr_hr(jsp, "%s:\n", pspo_vpdp);
        }
        if (dhex > 0) {
            if (dhex > 2)
                named_hhh_output(pspo_vpdp, b, len, op);
            else
                hex2stdout(b, len, hex_format);
            goto fini;
        }
        if (as_json) {
            jo2p = sg_vpd_js_hdr(jsp, jop, pslu_vpdp, b);
            jap = sgj_named_subarray_r(jsp, jo2p,
                                  "port_information_descriptor_list");
        }
        decode_proto_port_vpd(b, len, op, jap);
        break;
    case VPD_SCSI_FEATURE_SETS:         /* 0x92 */
        if (b[1] != pn)
            goto dumb_inq;
        len = sg_get_unaligned_be16(b + 2) + 4;
        if (len > sz) {
            pr2serr("Response to %s truncated\n", sfs_vpdp);
            len = sz;
        }
        if (dhex < 3) {
            if (op->do_long)
                sgj_pr_hr(jsp, "%s [0x92]:\n", sfs_vpdp);
            else
                sgj_pr_hr(jsp, "%s:\n", sfs_vpdp);
        }
        if (dhex > 0) {
            if (dhex > 2)
                named_hhh_output(sfs_vpdp, b, len, op);
            else
                hex2stdout(b, len, hex_format);
            goto fini;
        }
        if (as_json) {
            jo2p = sg_vpd_js_hdr(jsp, jop, sfs_vpdp, b);
            jap = sgj_named_subarray_r(jsp, jo2p, "feature_set_code_list");
        }
        decode_feature_sets_vpd(b, len, op, jap);
        break;
    case VPD_SCSI_PORTS:                /* 0x88 */
        if (b[1] != pn)
            goto dumb_inq;
        len = sg_get_unaligned_be16(b + 2) + 4;
        if (len > sz) {
            pr2serr("Response to %s truncated\n", sp_vpdp);
            len = sz;
        }
        if (dhex < 3) {
            if (op->do_long)
                sgj_pr_hr(jsp, "%s [0x88]:\n", sp_vpdp);
            else
                sgj_pr_hr(jsp, "%s:\n", sp_vpdp);
        }
        if (dhex > 0) {
            if (dhex > 2)
                named_hhh_output(sp_vpdp, b, len, op);
            else
                hex2stdout(b, len, hex_format);
            goto fini;
        }
        if (as_json) {
            jo2p = sg_vpd_js_hdr(jsp, jop, sp_vpdp, b);
            jap = sgj_named_subarray_r(jsp, jo2p,
                                       "scsi_ports_descriptor_list");
        }
        ret = decode_scsi_ports_vpd(b, len, op, jap);
        if (ret)
            goto fini;
        break;
    case VPD_SOFTW_INF_ID:              /* 0x84 */
        if (b[1] != pn)
            goto dumb_inq;
        len = sg_get_unaligned_be16(b + 2) + 4;
        if (len > sz) {
            pr2serr("Response to %s truncated\n", sii_vpdp);
            len = sz;
        }
        if (dhex < 3) {
            if (op->do_long)
                sgj_pr_hr(jsp, "%s [0x84]:\n", sii_vpdp);
            else
                sgj_pr_hr(jsp, "%s:\n", sii_vpdp);
        }
        if (dhex > 0) {
            if (dhex > 2)
                named_hhh_output(sii_vpdp, b, len, op);
            else
                hex2stdout(b, len, hex_format);
            goto fini;
        }
        if (as_json) {
            jo2p = sg_vpd_js_hdr(jsp, jop, sii_vpdp, b);
            jap = sgj_named_subarray_r(jsp, jo2p,
                                      "software_interface_identifier_list");
        }
        decode_softw_inf_id(b, len, op, jap);
        break;
    case VPD_UNIT_SERIAL_NUM:           /* 0x80 */
        if (b[1] != pn)
            goto dumb_inq;
        if ((0x2 == b[2]) && (0x2 == b[3])) {
            /* could be a Unit Serial number VPD page with a very long
             * length of 4+514 bytes; more likely standard response for
             * SCSI-2, RMB=1 and a response_data_format of 0x2. */
            pr2serr("very unlikely to be a %s response, so ...\n", usn_vpdp);
            goto dumb_inq;
        }
        len = sg_get_unaligned_be16(b + 2) + 4;
        if (dhex < 3) {
            if (op->do_long)
                sgj_pr_hr(jsp, "%s [0x80]:\n", usn_vpdp);
            else
                sgj_pr_hr(jsp, "%s:\n", usn_vpdp);
        }
        if (dhex > 0) {
            if (dhex > 2)
                named_hhh_output(usn_vpdp, b, len, op);
            else
                hex2stdout(b, len, hex_format);
            break;
        }
        if (len >= 0) {
            if (len >= clen)
                len = clen - 1;
            memcpy(c, b + 4, len - 4);
            c[len] = '\0';
        } else
            strcpy(c, "<empty>");
        if (as_json)
            jo2p = sg_vpd_js_hdr(jsp, jop, usn_vpdp, b);
        sgj_haj_vs(jsp, jo2p, 2, "Product serial number",
                           SGJ_SEP_COLON_1_SPACE, c);
        break;
    case VPD_3PARTY_COPY:               /* 0x8f */
        if (b[1] != pn)
            goto dumb_inq;
        len = sg_get_unaligned_be16(b + 2) + 4;
        if ((NULL == ihbp) && (len > sz) && (sg_fd >= 0)) {
            /* Increase response size to maximum we can handle here */
            sz = VPD_XCOPY_RESP_LEN;
            ret = sg_ll_inquiry_v2(sg_fd, true, pn, b, sz, 0, &resid, false,
                                   verb);
            if (ret) {
                pr2serr("INQUIRY fetching VPD page=0x%x failed\n", pn);
                goto fini;
            }
            if (resid) {
                sz += resid;
                if (resid < 4) {
                    pr2serr("%s: resid=%d implies response too short (%d)\n",
                            __func__, resid, sz);
                    ret = SG_LIB_WILD_RESID;
                    goto fini;
                }
            }
            len = sg_get_unaligned_be16(b + 2) + 4;
            if (len > sz) {
                pr2serr("Response to Third party copy VPD page truncated\n");
                len = sz;
            }
        }
        if (dhex < 3) {
            if (op->do_long)
                sgj_pr_hr(jsp, "%s [0x8f]:\n", tpc_vpdp);
            else
                sgj_pr_hr(jsp, "%s:\n", tpc_vpdp);
        }
        if (dhex > 0) {
            if (dhex > 2)
                named_hhh_output(tpc_vpdp, b, len, op);
            else
                hex2stdout(b, len, hex_format);
            goto fini;
        }
        if (as_json) {
            jo2p = sg_vpd_js_hdr(jsp, jop, tpc_vpdp, b);
            jap = sgj_named_subarray_r(jsp, jo2p,
                              "third_party_copy_descriptors");
        }
        decode_3party_copy_vpd(b, len, op, jap);
        break;
    case 0xb0:          /* VPD page depends on pdt */
        if (b[1] != pn)
            goto dumb_inq;
        switch (pdt) {
        case PDT_DISK: case PDT_WO: case PDT_OPTICAL: case PDT_ZBC:
            nm = bl_vpdp;
            sbc = true;
            break;
        case PDT_TAPE: case PDT_MCHANGER:
            nm = sad_vpdp;
            ssc = true;
            break;
        case PDT_OSD:
            nm = osdi_vpdp;
            /* osd = true; */
            break;
        default:
            snprintf(c, clen, "%s %s 0x%x", updt_s, vpd_pg_s, pn);
            nm = c;
            break;
        }
        len = sg_get_unaligned_be16(b + 2) + 4;
        if (len > sz) {
            pr2serr("Response to %s truncated\n", nm);
            len = sz;
        }
        if (dhex < 3) {
            if (op->do_long)
                sgj_pr_hr(jsp, "%s [0xb0]:\n", nm);
            else
                sgj_pr_hr(jsp, "%s:\n", nm);
        }
        if (dhex > 0) {
            if (dhex > 2)
                named_hhh_output(nm, b, len, op);
            else
                hex2stdout(b, len, hex_format);
            goto fini;
        }
        if (ssc) {
            jo2p = sg_vpd_js_hdr(jsp, jop, nm, b);
            decode_tape_dev_caps_vpd(b, len, op, jo2p);
        } else if (sbc) {
            jo2p = sg_vpd_js_hdr(jsp, jop, nm, b);
            decode_block_limits_vpd(b, len, op, jo2p);
        } else
            hex2stdout(b, len, hex_format);
        break;
    case 0xb1:          /* VPD page depends on pdt */
        if (b[1] != pn)
            goto dumb_inq;
        switch (pdt) {
        case PDT_DISK: case PDT_WO: case PDT_OPTICAL: case PDT_ZBC:
            nm = bdc_vpdp;
            sbc = true;
            break;
        case PDT_TAPE: case PDT_MCHANGER: case PDT_ADC:
            nm = masn_vpdp;
            ssc = true;
            break;
        case PDT_OSD:
            nm = st_vpdp;
            /* osd = true; */
            break;
        default:
            snprintf(c, clen, "%s %s 0x%x", updt_s, vpd_pg_s, pn);
            nm = c;
            break;
        }
        len = sg_get_unaligned_be16(b + 2) + 4;
        if (len > sz) {
            pr2serr("Response to %s VPD page truncated\n", nm);
            len = sz;
        }
        if (dhex < 3) {
            if (op->do_long)
                sgj_pr_hr(jsp, "%s [0xb1]:\n", nm);
            else
                sgj_pr_hr(jsp, "%s\n", nm);
        }
        if (dhex > 0) {
            if (dhex > 2)
                named_hhh_output(nm, b, len, op);
            else
                hex2stdout(b, len, hex_format);
            goto fini;
        }
        if (ssc) {
            jo2p = sg_vpd_js_hdr(jsp, jop, nm, b);
            decode_tape_man_ass_sn_vpd(b, len, op, jo2p);
        } else if (sbc) {
            jo2p = sg_vpd_js_hdr(jsp, jop, nm, b);
            decode_block_dev_ch_vpd(b, len, op, jo2p);
        } else
            hex2stdout(b, len, 0);
        break;
    case 0xb2:          /* VPD page depends on pdt */
        if (b[1] != pn)
            goto dumb_inq;

        len = sg_get_unaligned_be16(b + 2) + 4;
        switch (pdt) {
        case PDT_DISK: case PDT_WO: case PDT_OPTICAL: case PDT_ZBC:
            nm = lbpv_vpdp;
            sbc = true;
            break;
        case PDT_TAPE: case PDT_MCHANGER:
            nm = tas_vpdp;
            ssc = true;
            break;
        default:
            snprintf(c, clen, "%s %s 0x%x", updt_s, vpd_pg_s, pn);
            nm = c;
            break;
        }
        if (dhex < 3) {
            if (op->do_long)
                sgj_pr_hr(jsp, "%s [0xb2]:\n", nm);
            else
                sgj_pr_hr(jsp, "%s:\n", nm);
        }
        if (dhex > 0) {
            if (dhex > 2)
                named_hhh_output(nm, b, len, op);
            else
                hex2stdout(b, len, hex_format);
            goto fini;
        }
        if (ssc) {
            jo2p = sg_vpd_js_hdr(jsp, jop, nm, b);
            decode_tapealert_supported_vpd(b, len, op, jo2p);
        } else if (sbc) {       /* added in sbc3r20 */
            jo2p = sg_vpd_js_hdr(jsp, jop, nm, b);
            decode_block_lb_prov_vpd(b, len, op, jo2p);
        } else
            hex2stdout(b, len, hex_format);
        break;
    case 0xb3:          /* VPD page depends on pdt */
        if (b[1] != pn)
            goto dumb_inq;
        len = sg_get_unaligned_be16(b + 2) + 4;
        switch (pdt) {
        case PDT_DISK: case PDT_WO: case PDT_OPTICAL: case PDT_ZBC:
            nm = ref_vpdp;
            sbc = true;
            break;
        case PDT_TAPE: case PDT_MCHANGER:
            nm = adsn_vpdp;
            ssc = true;
            break;
        default:
            snprintf(c, clen, "%s %s 0x%x", updt_s, vpd_pg_s, pn);
            nm = c;
            break;
        }
        if (dhex < 3) {
            if (op->do_long)
                sgj_pr_hr(jsp, "%s [0xb3]:\n", nm);
            else
                sgj_pr_hr(jsp, "%s:\n", nm);
        }
        if (dhex > 0) {
            if (dhex > 2)
                named_hhh_output(nm, b, len, op);
            else
                hex2stdout(b, len, hex_format);
            goto fini;
        }
        if (ssc) {
            /* VPD_AUTOMATION_DEV_SN ssc */
            jo2p = sg_vpd_js_hdr(jsp, jop, nm, b);
            sgj_pr_hr(jsp, "  Manufacturer-assigned serial number: %.*s\n",
                      len - 4, b + 4);
            sgj_js_nv_s_len(jsp, jo2p, "manufacturer_assigned_serial_number",
                            (const char *)b + 4, len - 4);
        } else if (sbc) {     /* added in sbc3r22 */
            jo2p = sg_vpd_js_hdr(jsp, jop, nm, b);
            decode_referrals_vpd(b, len, op, jo2p);
        } else
            hex2stdout(b, len, 0);
        break;
    case 0xb4:          /* VPD page depends on pdt */
        if (b[1] != pn)
            goto dumb_inq;
        len = sg_get_unaligned_be16(b + 2) + 4;
        switch (pdt) {
        case PDT_DISK: case PDT_WO: case PDT_OPTICAL: case PDT_ZBC:
            nm = sbl_vpdp;
            sbc = true;
            break;
        case PDT_TAPE: case PDT_MCHANGER:
            nm = dtde_vpdp;
            ssc = true;
            break;
        default:
            snprintf(c, clen, "%s %s 0x%x", updt_s, vpd_pg_s, pn);
            nm = c;
            break;
        }
        if (dhex < 3) {
            if (op->do_long)
                sgj_pr_hr(jsp, "%s [0xb4]:\n", nm);
            else
                sgj_pr_hr(jsp, "%s:\n", nm);
        }
        if (dhex > 0) {
            if (dhex > 2)
                named_hhh_output(nm, b, len, op);
            else
                hex2stdout(b, len, hex_format);
            goto fini;
        }
        if (ssc) {
            sgj_pr_hr(jsp, "  Device transfer data element:\n");
            if (! jsp->pr_as_json)
                hex2stdout(b + 4, len - 4, 1);
            jo2p = sg_vpd_js_hdr(jsp, jop, nm, b);
            sgj_js_nv_hex_bytes(jsp, jo2p, "device_transfer_data_element",
                                b + 4, len - 4);
        } else if (sbc) {       /* added in sbc4r01 */
            jo2p = sg_vpd_js_hdr(jsp, jop, nm, b);
            jap = sgj_named_subarray_r(jsp, jo2p, "logical_block_length_"
                                "and_protection_types_descriptor_list");
            decode_sup_block_lens_vpd(b, len, op, jap);
        } else
            hex2stdout(b, len, 0);
        break;
    case 0xb5:          /* VPD page depends on pdt */
        if (b[1] != pn)
            goto dumb_inq;
        len = sg_get_unaligned_be16(b + 2) + 4;
        switch (pdt) {
        case PDT_DISK: case PDT_WO: case PDT_OPTICAL: case PDT_ZBC:
            nm = bdce_vpdp;
            sbc = true;
            break;
        case PDT_TAPE: case PDT_MCHANGER:
            nm = lbpro_vpdp;
            ssc = true;
            break;
        default:
            snprintf(c, clen, "%s %s 0x%x", updt_s, vpd_pg_s, pn);
            nm = c;
            break;
        }
        if (dhex < 3) {
            if (op->do_long)
                sgj_pr_hr(jsp, "%s [0xb5]:\n", nm);
            else
                sgj_pr_hr(jsp, "%s:\n", nm);
        }
        if (dhex > 0) {
            if (dhex > 2)
                named_hhh_output(nm, b, len, op);
            else
                hex2stdout(b, len, hex_format);
            goto fini;
        }
        if (sbc) {              /* added in sbc4r02 */
            jo2p = sg_vpd_js_hdr(jsp, jop, nm, b);
            decode_block_dev_char_ext_vpd(b, len, op, jo2p);
        } else if (ssc) {       /* added in ssc5r02a */
            jo2p = sg_vpd_js_hdr(jsp, jop, nm, b);
            jap = sgj_named_subarray_r(jsp, jo2p,
                         "logical_block_protection_method_descriptor_list");
            decode_lb_protection_vpd(b, len, op, jap);
        } else
            hex2stdout(b, len, 0);
        break;
    case VPD_ZBC_DEV_CHARS:       /* 0xb6 for both pdt=0 and pdt=0x14 */
        if (b[1] != pn)
            goto dumb_inq;
        len = sg_get_unaligned_be16(b + 2) + 4;
        switch (pdt) {
        case PDT_DISK: case PDT_WO: case PDT_OPTICAL: case PDT_ZBC:
            nm = zbdc_vpdp;
            sbc = true;
            break;
        default:
            snprintf(c, clen, "%s %s 0x%x", updt_s, vpd_pg_s, pn);
            nm = c;
            break;
        }
        if (dhex < 3) {
            if (op->do_long)
                sgj_pr_hr(jsp, "%s [0xb6]:\n", nm);
            else
                sgj_pr_hr(jsp, "%s:\n", nm);
        }
        if (dhex > 0) {
            if (dhex > 2)
                named_hhh_output(nm, b, len, op);
            else
                hex2stdout(b, len, hex_format);
            goto fini;
        }
        if (sbc) {       /* added in zbc-r01c */
            jo2p = sg_vpd_js_hdr(jsp, jop, nm, b);
            decode_zbdch_vpd(b, len, op, jo2p);
        } else
            hex2stdout(b, len, 0);
        break;
    case 0xb7:          /* VPD page depends on pdt */
        if (b[1] != pn)
            goto dumb_inq;
        len = sg_get_unaligned_be16(b + 2) + 4;
        switch (pdt) {
        case PDT_DISK: case PDT_WO: case PDT_OPTICAL: case PDT_ZBC:
            nm = ble_vpdp;
            sbc = true;
            break;
        default:
            snprintf(c, clen, "%s %s 0x%x", updt_s, vpd_pg_s, pn);
            nm = c;
            break;
        }
        if (dhex < 3) {
            if (op->do_long)
                sgj_pr_hr(jsp, "%s [0xb7]:\n", nm);
            else
                sgj_pr_hr(jsp, "%s:\n", nm);
        }
        if (dhex > 0) {
            if (dhex > 2)
                named_hhh_output(nm, b, len, op);
            else
                hex2stdout(b, len, hex_format);
            goto fini;
        }
        if (sbc) {       /* added in sbc4r07 */
            jo2p = sg_vpd_js_hdr(jsp, jop, nm, b);
            decode_block_limits_ext_vpd(b, len, op, jo2p);
        } else
            hex2stdout(b, len, 0);
        break;
    case 0xb8:          /* VPD page depends on pdt */
        if (b[1] != pn)
            goto dumb_inq;
        len = sg_get_unaligned_be16(b + 2) + 4;
        switch (pdt) {
        case PDT_DISK: case PDT_WO: case PDT_OPTICAL: case PDT_ZBC:
            nm = fp_vpdp;
            sbc = true;
            break;
        default:
            snprintf(c, clen, "%s %s 0x%x", updt_s, vpd_pg_s, pn);
            nm = c;
            break;
        }
        if (dhex < 3) {
            if (op->do_long)
                sgj_pr_hr(jsp, "%s [0xb8]:\n", nm);
            else
                sgj_pr_hr(jsp, "%s:\n", nm);
        }
        if (dhex > 0) {
            if (dhex > 2)
                named_hhh_output(nm, b, len, op);
            else
                hex2stdout(b, len, hex_format);
            goto fini;
        }
        if (sbc) {      /* added in sbc4r18 */
            jo2p = sg_vpd_js_hdr(jsp, jop, nm, b);
            jap = sgj_named_subarray_r(jsp, jo2p,
                                       "format_preset_descriptor_list");
            decode_format_presets_vpd(b, len, op, jap);
        } else
            hex2stdout(b, len, 0);
        if (ret)
            goto fini;
        break;
    case 0xb9:          /* VPD page depends on pdt */
        if (b[1] != pn)
            goto dumb_inq;
        len = sg_get_unaligned_be16(b + 2) + 4;
        switch (pdt) {
        case PDT_DISK: case PDT_WO: case PDT_OPTICAL: case PDT_ZBC:
            nm = cpr_vpdp;
            sbc = true;
            break;
        default:
            snprintf(c, clen, "%s %s 0x%x", updt_s, vpd_pg_s, pn);
            nm = c;
            break;
        }
        if (dhex < 3) {
            if (op->do_long)
                sgj_pr_hr(jsp, "%s [0xb9]:\n", nm);
            else
                sgj_pr_hr(jsp, "%s:\n", nm);
        }
        if (dhex > 0) {
            if (dhex > 2)
                named_hhh_output(nm, b, len, op);
            else
                hex2stdout(b, len, hex_format);
            goto fini;
        }
        if (sbc) {
            jo2p = sg_vpd_js_hdr(jsp, jop, nm, b);
            jap = sgj_named_subarray_r(jsp, jo2p,
                                       "lba_range_descriptor_list");
            decode_con_pos_range_vpd(b, len, op, jap);
        } else
            hex2stdout(b, len, 0);
        break;
    case 0xba:
        if (b[1] != pn)
            goto dumb_inq;
        len = sg_get_unaligned_be16(b + 2) + 4;
        switch (pdt) {
        case PDT_DISK: case PDT_WO: case PDT_OPTICAL: case PDT_ZBC:
            cap = true;
            break;
        default:
            cap = false;
            break;
        }
        if (dhex < 3) {
            if (cap)
                sgj_pr_hr(jsp, "%s:\n", cap_vpdp);
            else
                sgj_pr_hr(jsp, "%s=0x%x, pdt=0x%x:\n", vpd_pg_s, pn, pdt);
            if (dhex > 0)
                hex2stdout(b, len, hex_format);
        }
        if (dhex > 0) {
            if (dhex > 2)
                named_hhh_output(cap_vpdp, b, len, op);
            else
                hex2stdout(b, len, hex_format);
            goto fini;
        }
        if (cap) {
            if (dhex > 2) {
                named_hhh_output(cap_vpdp, b, len, op);
                goto fini;
            }
            if (as_json) {
                jo2p = sg_vpd_js_hdr(jsp, jop, cap_vpdp, b);
                jap = sgj_named_subarray_r(jsp, jo2p,
                         "capacity_product_identification_descriptors_list");
            }
            decode_cap_prod_id_vpd(b, len, op, jap);
        } else if (dhex > 2) {
            snprintf(c, clen, "%s=0x%x, pdt=0x%x:\n", vpd_pg_s, pn, pdt);
            named_hhh_output(c, b, len, op);
            goto fini;
        }
        break;
    default:
        if (b[1] != pn)
            goto dumb_inq;
        len = sg_get_unaligned_be16(b + 2) + 4;
        vpp = sdp_get_vpd_detail(pn, -1, pdt);
        if (vpp)
            snprintf(c, clen, "%s VPD page", vpp->name);
        else
            snprintf(c, clen, "VPD page 0x%x", pn);
        if (dhex < 3)
            sgj_pr_hr(jsp, "%s in hex:\n", c);
        if (len > b_sz) {
            if (op->verbose)
                pr2serr("page length=%d too long, trim\n", len);
            len = b_sz;
        }
        if (dhex > 0) {
            if (dhex > 2)
                named_hhh_output(c, b, len, op);
            else
                hex2stdout(b, len, hex_format);
            goto fini;
        }
        if (as_json) {
            const char * ccp;

            snprintf(c, clen, "vpd_page_%02x", pn);
            jo2p = sg_vpd_js_hdr(jsp, jop, c, b);
            snprintf(c, clen, "%d bytes long when 4 byte header included",
                     len);
            sgj_js_nv_ihexstr(jsp, jo2p, "page_length", len, NULL, c);
            if (pn <= 0x80)
                ccp = "unimplemented";
            else if (pn <= 0x82)
                ccp = "obsolete";
            else if (pn <= 0x8f)
                ccp = "unimplemented";
            else if (pn <= 0xbf)
                ccp = "restricted";
            else
                ccp = "vendor_specific";
            sgj_js_nv_s(jsp, jo2p, "vpd_category", ccp);
            sgjv_js_hex_long(jsp, jo2p, b, len);
        } else
            hex2stdout(b, len, hex_format);
        break;
    }
    ret = 0;
    goto fini;

dumb_inq:
    pr2serr("malformed VPD response, VPD pages probably not supported\n");
    return SG_LIB_CAT_MALFORMED;

fini:
    if (free_b)
        free(free_b);
    return ret;
}
